// include/tlm/pcie/pcie_completion_tracker_tlm.hh
// CompletionTracker: SR-IOV completion 匹配 (per Q12)
// 功能描述：NP 请求 (trans_id) 与 CplD 关联匹配。
//           - register_np(vf_id, trans_id)：登记 outstanding NP 请求
//           - complete(vf_id, trans_id, cpl_data)：CplD 到达时匹配并返回数据
//           - 溢出策略（per Q12 + proposal.md T-P4-6）：outstanding 容量上限，
//             N+1 即拒绝新发出；完整超时/重传 out-of-scope（UsrLinuxEmu 容错吸收）。
// 作者 CppTLM Team / 日期 2026-10-13
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-6
//       decisions.md §Q12 (completion 匹配 + 溢出丢弃/上报)
#ifndef TLM_PCIE_PCIE_COMPLETION_TRACKER_TLM_HH
#define TLM_PCIE_PCIE_COMPLETION_TRACKER_TLM_HH

#include <cstdint>
#include <unordered_map>

namespace tlm::pcie {

/**
 * @brief Completion 跟踪器：per-VF outstanding NP 请求 ↔ CplD 匹配
 *
 * 简化模型（per Q12）：
 *   - 不实现完整 Completion Timeout 错误模型（out-of-scope）
 *   - 溢出策略：每 VF outstanding map 达到容量上限时，新 register_np 拒绝
 *   - flr_vf(vf_id) 清空该 VF 的 outstanding；flr_pf() 清空全部
 */
class CompletionTracker {
public:
    // 默认容量（per proposal T-P4-6 "N+1 即拒绝新发出"）
    static constexpr uint32_t DEFAULT_CAPACITY = 64;

    // CplD 载荷（简化：数据值 + 字节数）
    struct CplData {
        uint32_t data = 0;
        uint32_t byte_count = 0;
    };

    CompletionTracker() = default;

    // 初始化（清空所有 per-VF outstanding map）
    void init();

    // 登记 NP 请求（trans_id 关联）；返回 false 表示溢出（N+1 拒绝）或 vf_id 越界
    bool register_np(uint16_t vf_id, uint32_t trans_id);

    // CplD 到达：匹配并返回；false = 未匹配（无对应 outstanding 请求）
    bool complete(uint16_t vf_id, uint32_t trans_id, const CplData& cpl);

    // 查询：某 VF 的 outstanding 请求数 / 累计完成数
    std::size_t outstanding_count(uint16_t vf_id) const;
    std::size_t completed_count(uint16_t vf_id) const;

    // 容量上限（所有 VF 共享；测试/诊断）
    uint32_t capacity() const noexcept { return DEFAULT_CAPACITY; }

    // FLR 集成：清空指定 VF / 全部 VF 的 outstanding
    void flr_vf(uint16_t vf_id);
    void flr_pf();

private:
    struct PerVfState {
        std::unordered_map<uint32_t, CplData> outstanding;
        std::size_t completed = 0;
    };

    // vf_id 合法范围 (0=PF, 1..16=VF0..VF15)
    static constexpr uint16_t kNumSlots = 17;
    PerVfState per_vf_[kNumSlots];
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_COMPLETION_TRACKER_TLM_HH