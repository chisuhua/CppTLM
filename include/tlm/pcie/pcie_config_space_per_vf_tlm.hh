// include/tlm/pcie/pcie_config_space_per_vf_tlm.hh
// PcieConfigSpacePerVf: SR-IOV per-VF Config Space 池 (PF + 16 VF)
// 功能描述：持有 17 份独立 PcieConfigSpace（slot 0 = PF，slot 1..16 = VF0..VF15）。
//           per-VF 4KB Config Space 独立（BAR0/1/2 大小独立，Command register 独立）。
// 作者 CppTLM Team / 日期 2026-10-13
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-1
//       design.md §8 (per-VF Config Space 4KB 独立基址)
#ifndef TLM_PCIE_PCIE_CONFIG_SPACE_PER_VF_TLM_HH
#define TLM_PCIE_PCIE_CONFIG_SPACE_PER_VF_TLM_HH

#include "tlm/gpu/pcie_config_space_mvp.hh"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace tlm::pcie {

/**
 * @brief SR-IOV per-VF Config Space 池
 *
 * 布局（per design.md §8 + proposal.md "17 端口"）：
 *   - slot 0   = PF0
 *   - slot 1   = VF0
 *   - slot 2   = VF1
 *   - ...
 *   - slot 16  = VF15
 *
 * 每份 PcieConfigSpace 都是独立对象（独立 BAR/Command/MSI 配置），
 * VFx 的读写不影响 PF0 或其他 VF。
 */
class PcieConfigSpacePerVf {
public:
    static constexpr uint16_t PF_SLOT = 0;
    static constexpr uint16_t VF0_SLOT = 1;
    static constexpr uint16_t NUM_SLOTS = 17;   // 1 PF + 16 VF

    PcieConfigSpacePerVf() = default;

    // 访问指定 vf_id 的 Config Space（vf_id 0=PF, 1..16=VF0..VF15）
    // 越界（>16）抛 std::out_of_range
    tlm::gpu::PcieConfigSpace& config_of(uint16_t vf_id) {
        if (vf_id >= NUM_SLOTS) {
            throw std::out_of_range("PcieConfigSpacePerVf: vf_id out of range");
        }
        return slots_[vf_id];
    }
    const tlm::gpu::PcieConfigSpace& config_of(uint16_t vf_id) const {
        if (vf_id >= NUM_SLOTS) {
            throw std::out_of_range("PcieConfigSpacePerVf: vf_id out of range");
        }
        return slots_[vf_id];
    }

    // 便捷：VF 逻辑号（0..15）→ slot 号（1..16）
    static uint16_t vf_index_to_slot(uint16_t vf_index) {
        return VF0_SLOT + vf_index;
    }

    [[nodiscard]] uint16_t num_slots() const noexcept { return NUM_SLOTS; }
    [[nodiscard]] bool is_valid_vf_id(uint16_t vf_id) const noexcept {
        return vf_id < NUM_SLOTS;
    }

    // 初始化所有 slot（填充默认 vendor/device/header）
    void init_all() {
        for (auto& s : slots_) {
            s.init();
        }
    }

private:
    // 固定 17 份（避免 PcieConfigSpace 不可拷贝导致 vector 无法 realloc）
    std::array<tlm::gpu::PcieConfigSpace, NUM_SLOTS> slots_;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_CONFIG_SPACE_PER_VF_TLM_HH
