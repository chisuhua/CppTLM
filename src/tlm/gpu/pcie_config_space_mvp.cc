// src/tlm/gpu/pcie_config_space_mvp.cc
// PcieConfigSpace 实现
// 作者 CppTLM Team / 日期 2026-08-26

#include "tlm/gpu/pcie_config_space_mvp.hh"
#include <cstring>
#include <stdexcept>

namespace tlm::gpu {

    PcieConfigSpace::PcieConfigSpace(std::size_t config_size) : config_size_(config_size) {
        if (config_size != CFG_SIZE_256B && config_size != CFG_SIZE_4KB) {
            throw std::invalid_argument("PcieConfigSpace: config_size must be 256 or 4096 bytes");
        }
        // dword count = config_size / 4
        regs_.resize(config_size / 4, 0);
    }

    void PcieConfigSpace::init() {
        std::memset(regs_.data(), 0, regs_.size() * sizeof(uint32_t));
        capabilities_.clear();

        // Standard PCI header (Type 0): offsets 0x00..0x3F
        regs_[0x00 / 4] = (static_cast<uint32_t>(DEFAULT_VENDOR_ID) << 0) |
                          (static_cast<uint32_t>(DEFAULT_DEVICE_ID) << 16);
        regs_[0x08 / 4] = (static_cast<uint32_t>(DEFAULT_REVISION) << 0) |
                          (static_cast<uint32_t>(DEFAULT_HEADER_TYPE) << 24);
        // Status register (offset 0x06): Capabilities Present bit
        regs_[0x06 / 4] = (1u << 4); // bit 4 = Capabilities List
    }

    bool PcieConfigSpace::add_capability(uint8_t id, uint8_t offset, uint8_t next,
                                         uint16_t control) {
        if (!is_aligned(offset))
            return false;
        if (offset >= config_size_)
            return false;
        if (next != 0x00 && next >= config_size_)
            return false;

        // 检查与已有 capability 的 offset 重叠
        for (const auto& c : capabilities_) {
            if (c.offset == offset)
                return false;
        }

        capabilities_.push_back(Capability{id, offset, next, control});

        // 更新 capabilities pointer (offset 0x34, byte 0x34)
        // 仅在 capabilities 列表为空时设置（指向首个 capability）
        if (capabilities_.size() == 1) {
            // Capabilities pointer 在 offset 0x34 (byte-level)
            uint32_t cp = regs_[0x34 / 4];
            cp = (cp & 0xFFFFFF00u) | static_cast<uint32_t>(offset);
            regs_[0x34 / 4] = cp;
        }

        // C2 修复 (Oracle HIGH-1): 把完整 cap dword (id | next<<8 | control<<16) 写入
        // regs_, 使 host CFG_READ 可见。此前 cap 仅存在于 capabilities_ 链表,
        // regs_ 未写 → host 读 0。write() 内建越界/RO 保护 (offset 已在上方校验)。
        write(offset, (static_cast<uint32_t>(id) << 0) | (static_cast<uint32_t>(next) << 8) |
                          (static_cast<uint32_t>(control) << 16));
        return true;
    }

    bool PcieConfigSpace::update_capability_control(std::size_t index, uint16_t control) {
        if (index >= capabilities_.size())
            return false;
        capabilities_[index].control = control;
        const Capability& c = capabilities_[index];
        // 重写 cap dword, 保持 host CFG_READ 视角与 descriptor 一致
        write(c.offset, (static_cast<uint32_t>(c.id) << 0) | (static_cast<uint32_t>(c.next) << 8) |
                            (static_cast<uint32_t>(c.control) << 16));
        return true;
    }

    uint32_t PcieConfigSpace::read(uint16_t offset) const {
        if (!is_aligned(offset) || offset >= config_size_) {
            return 0xFFFFFFFFu; // PCIe spec: ALL_ONES for unimplemented
        }
        return regs_[offset / 4];
    }

    void PcieConfigSpace::write(uint16_t offset, uint32_t value) {
        if (!is_aligned(offset) || offset >= config_size_) {
            return; // PCIe spec: silently ignore OOB writes (simplified)
        }
        // Read-Only 字段保护：vendor_id(0x00-0x01), device_id(0x02-0x03),
        // header_type(0x0E), capabilities pointer(0x34)
        if (offset == 0x00 || offset == 0x0E || offset == 0x34) {
            return;
        }
        regs_[offset / 4] = value;

        // Stage 1.4 §1.2: PMCSR 写拦截 (PM Cap id=0x01, PMCSR 在 cap offset+4)
        if (pmcsr_write_cb_) {
            for (const auto& c : capabilities_) {
                if (c.id == 0x01 && offset == static_cast<uint16_t>(c.offset + 0x04)) {
                    const uint16_t new_pws = static_cast<uint16_t>(value & 0x0003u);
                    if (new_pws != last_pmcsr_pws_) {
                        last_pmcsr_pws_ = new_pws;
                        pmcsr_write_cb_(new_pws);
                    }
                    break;
                }
            }
        }

        // A-4: LNKCTL 写拦截 (PCIe Cap id=0x10, LNKCTL 在 cap+0x10)
        // 仅在 ASPM bits[1:0] 实际变化时回调
        if (lnkctl_write_cb_) {
            for (const auto& c : capabilities_) {
                if (c.id == 0x10 && offset == static_cast<uint16_t>(c.offset + 0x10)) {
                    const uint16_t new_lnkctl = static_cast<uint16_t>(value & 0x0003u);
                    if (new_lnkctl != last_lnkctl_) {
                        last_lnkctl_ = new_lnkctl;
                        lnkctl_write_cb_(new_lnkctl);
                    }
                    break;
                }
            }
        }
    }

    bool PcieConfigSpace::add_extended_capability(uint16_t id, uint16_t offset, uint16_t next) {
        if (!is_aligned(offset))
            return false;
        if (offset >= config_size_)
            return false;
        if (next != 0x00 && next >= config_size_)
            return false;
        for (const auto& c : extended_caps_) {
            if (c.offset == offset)
                return false;
        }
        extended_caps_.push_back(ExtendedCapability{id, offset, next});

        // PCIe-SIG Ext Cap Header layout (4-byte aligned):
        //   bits[15:0]  = Cap ID
        //   bits[19:16] = Cap Version
        //   bits[31:20] = Next Cap Offset
        // 这里写 (next << 20) | (version=1 << 16) | id (Caller 应随后调 add_extended_register 覆盖更高位)
        write(offset, static_cast<uint32_t>(id) | (static_cast<uint32_t>(next) << 20));

        // 更新 Extended Capabilities pointer (offset 0x100, byte-level)
        // 当 first ext cap 位于 0x100 自身时, 写 pointer = 写 self, 跳过避免覆盖 header
        if (extended_caps_.size() == 1 && offset != 0x100) {
            uint32_t cp = regs_[0x100 / 4];
            cp = (cp & 0xFFFFFF00u) | static_cast<uint32_t>(offset & 0xFFu);
            regs_[0x100 / 4] = cp;
        }
        return true;
    }

    void PcieConfigSpace::add_extended_register(uint16_t offset, uint8_t width, uint32_t value) {
        (void)width;  // width 仅作语义标注, 当前统一按 dword 写
        write(offset, value);
    }

    const PcieConfigSpace::ExtendedCapability*
    PcieConfigSpace::get_extended_capability(std::size_t index) const {
        if (index >= extended_caps_.size())
            return nullptr;
        return &extended_caps_[index];
    }

    std::size_t PcieConfigSpace::pcie_cap_offset() const noexcept {
        for (const auto& c : capabilities_) {
            if (c.id == 0x10) return c.offset;
        }
        return 0;  // not installed
    }

    const PcieConfigSpace::Capability* PcieConfigSpace::get_capability(std::size_t index) const {
        if (index >= capabilities_.size())
            return nullptr;
        return &capabilities_[index];
    }

} // namespace tlm::gpu