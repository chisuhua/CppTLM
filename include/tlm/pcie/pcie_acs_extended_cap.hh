// include/tlm/pcie/pcie_acs_extended_cap.hh
// ACS Extended Capability (id=0x000D, version=1) 安装 + Control Reg dword RMW
// per openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/design.md §5
// 勘误: header=0x0001000D (PCIe Ext Cap dword: bits[15:0]=ID, [19:16]=version,
// [31:20]=next); Control Reg 在 offset+4 dword 高 16-bit (16-bit offset+6 不可读写)
//
// 作者 CppTLM Team / 日期 2027-02-09
#ifndef CPPTLM_PCIE_ACS_EXTENDED_CAP_HH
#define CPPTLM_PCIE_ACS_EXTENDED_CAP_HH

#include "tlm/gpu/pcie_config_space_mvp.hh"

#include <cstdint>

namespace tlm::pcie {

    // ACS Extended Cap header dword (id=0x000D, version=1, next=0)
    constexpr uint32_t ACS_EXT_CAP_HEADER = 0x0001000Du;

    // 安装 ACS Extended Cap 到 cfg 扩展空间 (默认 0x100, 需 config_size=4096)
    inline bool install_acs_extended_cap(tlm::gpu::PcieConfigSpace& cfg,
                                         uint16_t offset = 0x100) noexcept {
        if (cfg.config_size() < static_cast<std::size_t>(offset) + 8) {
            return false;  // 扩展空间不足
        }
        cfg.write(offset, ACS_EXT_CAP_HEADER);
        cfg.write(static_cast<uint16_t>(offset + 4), 0x00000000u);  // Cap+Control 全 0
        return true;
    }

    // ACS Control Reg bit enable/disable (dword RMW, 高 16-bit 内的 bit_idx)
    // bit_idx 相对 offset+4 dword 的绝对 bit (16..23 = Control Reg bits 0..7)
    inline bool set_acs_bit_enabled(tlm::gpu::PcieConfigSpace& cfg,
                                    uint16_t offset, unsigned bit_idx,
                                    bool enable) noexcept {
        const uint16_t reg = static_cast<uint16_t>(offset + 4);
        const uint32_t val = cfg.read(reg);
        const uint32_t mask = 1u << bit_idx;
        const uint32_t new_val = enable ? (val | mask) : (val & ~mask);
        cfg.write(reg, new_val);
        return true;
    }

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_ACS_EXTENDED_CAP_HH
