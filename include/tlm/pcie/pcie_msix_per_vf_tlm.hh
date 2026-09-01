// include/tlm/pcie/pcie_msix_per_vf_tlm.hh
// PcieMsixTablePerVf: SR-IOV per-VF MSI-X Table 池 (PF + 16 VF)
// 功能描述：持有 17 份独立 MsiXTable（slot 0 = PF，slot 1..16 = VF0..VF15）。
//           per-VF vector 数独立、pending bit 独立、mask bit 独立。
// 作者 CppTLM Team / 日期 2026-10-13
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-2
//       design.md §8 (per-VF MSI-X Table 独立 vector 表)
#ifndef TLM_PCIE_PCIE_MSIX_PER_VF_TLM_HH
#define TLM_PCIE_PCIE_MSIX_PER_VF_TLM_HH

#include "tlm/gpu/msix_table_mvp.hh"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace tlm::pcie {

/**
 * @brief SR-IOV per-VF MSI-X Table 池
 *
 * 布局（per design.md §8 / proposal.md "17 端口"）：
 *   - slot 0  = PF0
 *   - slot 1  = VF0
 *   - ...
 *   - slot 16 = VF15
 *
 * 每份 MsiXTable 独立（vector table / PBA / mask 全部分离），
 * PF 的 pending 不影响任何 VF。
 */
class PcieMsixTablePerVf {
public:
    static constexpr uint16_t PF_SLOT = 0;
    static constexpr uint16_t VF0_SLOT = 1;
    static constexpr uint16_t NUM_SLOTS = 17;   // 1 PF + 16 VF

    PcieMsixTablePerVf() {
        init_all();
    }

    // 访问指定 vf_id 的 MSI-X table（0=PF, 1..16=VF0..VF15），越界抛异常
    tlm::gpu::MsiXTable& table_of(uint16_t vf_id) {
        if (vf_id >= NUM_SLOTS) {
            throw std::out_of_range("PcieMsixTablePerVf: vf_id out of range");
        }
        return tables_[vf_id];
    }
    const tlm::gpu::MsiXTable& table_of(uint16_t vf_id) const {
        if (vf_id >= NUM_SLOTS) {
            throw std::out_of_range("PcieMsixTablePerVf: vf_id out of range");
        }
        return tables_[vf_id];
    }

    // 重新配置某 slot 的 vector 数
    // MsiXTable 不可拷贝/移动：使用 placement-new 原地重建
    bool configure_vectors(uint16_t vf_id, uint16_t num_vectors) {
        if (vf_id >= NUM_SLOTS) {
            return false;
        }
        tables_[vf_id].~MsiXTable();
        new (&tables_[vf_id]) tlm::gpu::MsiXTable(num_vectors);
        tables_[vf_id].init();
        return true;
    }

    // 池析构：placement-new 构造过的对象必须显式析构（MsiXTable 不可移动
    // 但默认构造可调用析构 — std::array 默认在析构时调用成员析构，
    // 这里为了在 configure_vectors 后析构栈上对象不出问题，让 array 默认析构）
    ~PcieMsixTablePerVf() = default;

    // ========== per-VF 便捷委托（update/clear/pending）==========
    bool update_pending(uint16_t vf_id, uint16_t vector, uint32_t trans_id = 0) {
        if (vf_id >= NUM_SLOTS) {
            return false;
        }
        return tables_[vf_id].update_pending(vector, trans_id);
    }

    bool clear_pending(uint16_t vf_id, uint16_t vector) {
        if (vf_id >= NUM_SLOTS) {
            return false;
        }
        return tables_[vf_id].clear_pending(vector);
    }

    bool pending(uint16_t vf_id, uint16_t vector) const {
        if (vf_id >= NUM_SLOTS) {
            return false;
        }
        return tables_[vf_id].is_pending(vector);
    }

    bool is_pba_set(uint16_t vf_id, uint16_t vector) const {
        if (vf_id >= NUM_SLOTS) {
            return false;
        }
        return tables_[vf_id].is_pba_set(vector);
    }

    [[nodiscard]] uint16_t num_slots() const noexcept { return NUM_SLOTS; }
    [[nodiscard]] bool is_valid_vf_id(uint16_t vf_id) const noexcept {
        return vf_id < NUM_SLOTS;
    }

    // 初始化所有 slot（默认 16 vectors + vector table 清零）
    void init_all() {
        for (auto& t : tables_) {
            t.init();
        }
    }

private:
    // 固定 17 份（MsiXTable 不可拷贝/移动，不能用 vector）
    std::array<tlm::gpu::MsiXTable, NUM_SLOTS> tables_;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_MSIX_PER_VF_TLM_HH