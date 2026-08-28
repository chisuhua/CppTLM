// include/tlm/gpu/msix_table_mvp.hh
// MsiXTable: MSI-X vector table + PBA + per-vector mask bit
// 功能描述：参数化 vector 数（默认 16）；update_pending → irq_out 投递；
//           mask bit 阻止 pending 投递（per PCI-SIG MSI-X spec）
// 作者 CppTLM Team / 日期 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §3
//       ADR-SOC-07 D2
//       spec.md Scenario "Masked vector does not deliver IRQ"
#ifndef CPPTLM_MSIX_TABLE_MVP_H
#define CPPTLM_MSIX_TABLE_MVP_H

#include "bundles/pcie_bundles_tlm.hh"
#include <cstdint>
#include <deque>
#include <vector>

namespace tlm::gpu {

    /**
     * @brief MSI-X Table（MVP）：vector table + PBA + per-vector mask
     *
     * 设计原则（per design.md §3 + spec.md Scenarios + T-prereq-3 PBA 语义 2026-08-28）：
     *   - vector 数参数化（默认 16，对齐 UsrLinuxEmu MSIX_DEFAULT_VECTORS）
     *   - update_pending(vector) → 总设置 PBA bit; 若 vector 未 mask, 同步入队 IRQ
     *   - clear_pending(vector) 由 driver EOI 路径触发（清 PBA + 队首出队）
     *   - mask bit：set_mask(vector, true) → 后续 update_pending 仅置 PBA, 不投递 IRQ
     *     set_mask(vector, false) → 若 PBA 位置位, 自动投递累积的 IRQ
     *     （per PCI-SIG MSI-X ECN: mask 期间 pending 保留在 PBA, unmask 触发）
     *   - vector 越界拒绝（>= num_vectors）
     *
     * IRQ 出向事件：
     *   - update_pending 时: pba_[v] = 1; 若未 mask → 入队 pending_irq_out_
     *   - set_mask(v, false) (unmask) 时: 若 pba_[v] = 1 → 投递累积 IRQ (PBA 语义)
     *   - tick() 中 irq_out 事件被 PcieEndpointTLM 拉取
     */
    class MsiXTable {
    public:
        // 默认 vector 数（对齐 UsrLinuxEmu MSIX_DEFAULT_VECTORS）
        static constexpr uint16_t DEFAULT_NUM_VECTORS = 16;

        // Vector table entry（每 vector 4 dword: addr_lo, addr_hi, data, control）
        struct VectorEntry {
            uint64_t msg_addr = 0;
            uint32_t msg_data = 0;
            uint32_t control = 0;  // bit 0 = mask bit
        };

        // IRQ 出向事件
        struct IrqOutEvent {
            uint16_t vector;
            uint32_t msg_data;
            uint64_t msg_addr;
            uint32_t trans_id;
        };

        explicit MsiXTable(uint16_t num_vectors = DEFAULT_NUM_VECTORS);
        ~MsiXTable() = default;

        MsiXTable(const MsiXTable&) = delete;
        MsiXTable& operator=(const MsiXTable&) = delete;
        MsiXTable(MsiXTable&&) = delete;
        MsiXTable& operator=(MsiXTable&&) = delete;

        // 初始化（vector table + PBA 清零）
        void init();

        // 配置 vector table entry（addr + data + control）
        bool configure_vector(uint16_t vector, uint64_t msg_addr, uint32_t msg_data,
                              uint32_t control = 0);

        // Per-vector mask bit (per PCI-SIG MSI-X spec + T-prereq-3 PBA semantics)
        // set_mask(vector, true) 后 update_pending 仅置 PBA, 不入队 IRQ;
        // set_mask(vector, false) → 若 PBA bit 已置位, 自动投递累积 IRQ。
        bool set_mask(uint16_t vector, bool masked);
        bool clear_mask(uint16_t vector);
        bool is_masked(uint16_t vector) const;

        // Pending bitmap 操作（per design.md §3, PBA semantics per T-prereq-3）
        // 返回 false 仅当 vector 越界
        // (per PCI-SIG spec + T-prereq-3: 即使 masked 也置 PBA, 不丢失)
        bool update_pending(uint16_t vector, uint32_t trans_id = 0);
        bool clear_pending(uint16_t vector);

        // 查询 vector 是否 pending（兼容旧 API：检查 irq_out 队列 + PBA）
        bool is_pending(uint16_t vector) const;

        // 查询 vector 的 PBA bit（per T-prereq-3 spec Scenario "PBA bit 与 vector pending 同步"）
        bool is_pba_set(uint16_t vector) const;

        // IRQ 出向队列（tick() 时 PcieEndpointTLM 拉取）
        const IrqOutEvent* try_pop_irq_out();
        void consume_irq_out();

        // 统计
        std::size_t num_vectors() const { return num_vectors_; }
        // 仅 irq_out 队列长度（向后兼容）
        std::size_t pending_count() const;
        // PBA bit 计数（per T-prereq-3 spec）
        std::size_t pba_count() const;

    private:
        uint16_t num_vectors_;
        std::vector<VectorEntry> entries_;
        std::deque<IrqOutEvent> pending_irq_out_;
        // PBA bits（per T-prereq-3 PBA semantics）: 1 bit per vector
        // pba_[v]=1 表示 vector v 有 pending write 请求待处理
        // 累积语义: 多次 update_pending 不增加计数, clear_pending 清除
        std::vector<uint8_t> pba_;

        // 内部 helper: 当 unmask 后, 若 pba_[v] 置位, 投递累积 IRQ 到队列
        void try_deliver_pending_on_unmask(uint16_t vector);
    };

} // namespace tlm::gpu

#endif // CPPTLM_MSIX_TABLE_MVP_H