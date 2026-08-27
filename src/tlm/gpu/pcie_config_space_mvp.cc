// src/tlm/gpu/pcie_config_space_mvp.cc
// PcieConfigSpace 实现
// 作者 CppTLM Team / 日期 2026-08-26

#include "tlm/gpu/pcie_config_space_mvp.hh"
#include <cstring>
#include <stdexcept>

namespace tlm::gpu {

    PcieConfigSpace::PcieConfigSpace(std::size_t config_size)
        : config_size_(config_size) {
        if (config_size != CFG_SIZE_256B && config_size != CFG_SIZE_4KB) {
            throw std::invalid_argument(
                "PcieConfigSpace: config_size must be 256 or 4096 bytes");
        }
        // dword count = config_size / 4
        regs_.resize(config_size / 4, 0);
    }

    void PcieConfigSpace::init() {
        std::memset(regs_.data(), 0, regs_.size() * sizeof(uint32_t));

        // Standard PCI header (Type 0): offsets 0x00..0x3F
        regs_[0x00 / 4] = (static_cast<uint32_t>(DEFAULT_VENDOR_ID) << 0)
                        | (static_cast<uint32_t>(DEFAULT_DEVICE_ID) << 16);
        regs_[0x08 / 4] = (static_cast<uint32_t>(DEFAULT_REVISION) << 0)
                        | (static_cast<uint32_t>(DEFAULT_HEADER_TYPE) << 24);
        // Status register (offset 0x06): Capabilities Present bit
        regs_[0x06 / 4] = (1u << 4);  // bit 4 = Capabilities List
        // Capabilities pointer (offset 0x34): start of chain
        // (set by first add_capability call)
    }

    bool PcieConfigSpace::add_capability(uint8_t id, uint8_t offset, uint8_t next,
                                          uint16_t control) {
        if (!is_aligned(offset)) return false;
        if (offset >= config_size_) return false;
        if (next != 0x00 && next >= config_size_) return false;

        // 检查与已有 capability 的 offset 重叠
        for (const auto& c : capabilities_) {
            if (c.offset == offset) return false;
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
        return true;
    }

    uint32_t PcieConfigSpace::read(uint16_t offset) const {
        if (!is_aligned(offset) || offset >= config_size_) {
            return 0xFFFFFFFFu;  // PCIe spec: ALL_ONES for unimplemented
        }
        return regs_[offset / 4];
    }

    void PcieConfigSpace::write(uint16_t offset, uint32_t value) {
        if (!is_aligned(offset) || offset >= config_size_) {
            return;  // PCIe spec: silently ignore OOB writes (simplified)
        }
        // Read-Only 字段保护：vendor_id(0x00-0x01), device_id(0x02-0x03),
        // header_type(0x0E), capabilities pointer(0x34)
        if (offset == 0x00 || offset == 0x0E || offset == 0x34) {
            return;
        }
        regs_[offset / 4] = value;
    }

    const PcieConfigSpace::Capability*
    PcieConfigSpace::get_capability(std::size_t index) const {
        if (index >= capabilities_.size()) return nullptr;
        return &capabilities_[index];
    }

} // namespace tlm::gpu