// include/tlm/pcie/pcie_flow_control_token_bucket.hh
// FcTokenBucket + FcEngine: PCIe Flow Control Token Bucket 引擎
// 功能描述：P/NP/Cpl 各一个 token 桶，weight + capacity。
//           credit 单调非减，**仅由 UpdateFC DLLP 增加，无自动 refill** (per Q2)。
//           每 VF 一个桶（SR-IOV 时 per-VF bucket pool，per Q11 单 VC0）。
// 作者 CppTLM Team / 日期 2026-09-08
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/decisions.md §Q2
//       PCIe 5.0 Base Specification §3.4 "Flow Control"
//       specs/link-layer-and-fc/spec.md "Requirement: flow-control-token-bucket"
#ifndef TLM_PCIE_PCIE_FLOW_CONTROL_TOKEN_BUCKET_HH
#define TLM_PCIE_PCIE_FLOW_CONTROL_TOKEN_BUCKET_HH

#include "core/event_queue.hh"

#include <cstdint>
#include <unordered_map>

namespace tlm::pcie {

/**
 * @brief 单 VF 的 FC Token Bucket（P/NP/Cpl 各一个桶）
 *
 * ⚠️ 关键设计（per Q2 + spec.md）：
 *   - **无 refill_rate / 无自动 refill**：token 只能由 `update()`（UpdateFC DLLP
 *     到达时由 Link Layer 调用）增加。自动 refill 会让 credit 永远不耗尽 →
 *     反压永不触发 → backpressure 建模作废。
 *   - `consume()` 与 `can_send()`：发送前检查 + 发送后扣减。over-consume
 *     返回 false 且不扣减（spec Scenario "FC Token Bucket 基础行为"）。
 *   - UpdateFC 补充不超过 capacity（spec Scenario "UpdateFC 补充"）。
 *
 * 事件机制（per Oracle 修订）：不持有 sc_event_queue；事件通知由 Link Layer
 * 调度 EventQueue*（本类不直接调度事件，仅作为 Link Layer 的 FC 状态子组件，
 * 通过构造注入的 eq_ 保持与仿真时钟的关联）。
 */
class FcTokenBucket {
public:
    enum class Type { Posted, NonPosted, Completion };

    // 默认：capacity=256（per design §10.1 fc_token_bucket_capacity=256），weight=1
    FcTokenBucket()
        : capacity_(256), weight_p_(1), weight_np_(1), weight_cpl_(1),
          tokens_p_(256), tokens_np_(256), tokens_cpl_(256) {}

    // 参数化构造（测试/JSON 注入）
    explicit FcTokenBucket(uint32_t capacity)
        : capacity_(capacity), weight_p_(1), weight_np_(1), weight_cpl_(1),
          tokens_p_(capacity), tokens_np_(capacity), tokens_cpl_(capacity) {}

    FcTokenBucket(uint32_t capacity, uint32_t weight_p, uint32_t weight_np,
                  uint32_t weight_cpl)
        : capacity_(capacity), weight_p_(weight_p), weight_np_(weight_np),
          weight_cpl_(weight_cpl),
          tokens_p_(capacity), tokens_np_(capacity), tokens_cpl_(capacity) {}

    FcTokenBucket(uint32_t capacity, uint32_t weight)
        : capacity_(capacity), weight_p_(weight), weight_np_(weight),
          weight_cpl_(weight),
          tokens_p_(capacity), tokens_np_(capacity), tokens_cpl_(capacity) {}

    // 发送前检查：credit 不足返回 false（调用方必须等待 UpdateFC）
    [[nodiscard]] bool can_send(Type t) const noexcept;

    // 发送后扣减（只在 can_send() 为 true 后调用）；token 不足返回 false 不扣减
    bool consume(Type t) noexcept;

    // UpdateFC DLLP 到达时由 Link Layer 调用（唯一补充路径），不超 capacity
    void update(Type t, uint32_t credit) noexcept;

    // 当前 token 数（测试/诊断）
    uint32_t token_count(Type t) const noexcept;

    uint32_t capacity() const noexcept { return capacity_; }
    uint32_t weight(Type t) const noexcept;

    EventQueue* event_queue() const noexcept { return eq_; }
    void set_event_queue(EventQueue* eq) noexcept { eq_ = eq; }

private:
    uint32_t tokens(Type t) const noexcept {
        switch (t) {
        case Type::Posted:      return tokens_p_;
        case Type::NonPosted:   return tokens_np_;
        case Type::Completion:  return tokens_cpl_;
        }
        return 0;
    }

    void set_tokens(Type t, uint32_t v) noexcept {
        switch (t) {
        case Type::Posted:      tokens_p_ = v; break;
        case Type::NonPosted:   tokens_np_ = v; break;
        case Type::Completion:  tokens_cpl_ = v; break;
        }
    }

    EventQueue* eq_ = nullptr;   // 注入 CppTLM 事件队列（非 SystemC sc_event_queue）
    uint32_t capacity_;
    uint32_t weight_p_, weight_np_, weight_cpl_;
    uint32_t tokens_p_, tokens_np_, tokens_cpl_;

    friend class FcEngine;
};

/**
 * @brief FC 引擎：每 VF 一个 token bucket 的池（per Q2 + Q11 单 VC0）
 *
 * PF0 = bucket index 0，VF0 = 1，...，VF15 = 16。
 * `bucket(vf_id)` 按需创建默认 bucket；`install_bucket` 可注入自定义 bucket。
 */
class FcEngine {
public:
    explicit FcEngine(EventQueue* eq) : eq_(eq) {}

    // 访问/创建 per-VF bucket
    FcTokenBucket& bucket(uint32_t vf_id);

    // 注入自定义 bucket（测试/JSON 配置）
    void install_bucket(uint32_t vf_id, FcTokenBucket bucket);

    // 便捷委托：发送前检查
    [[nodiscard]] bool can_send(uint32_t vf_id, FcTokenBucket::Type t);
    // 发送后扣减
    bool consume(uint32_t vf_id, FcTokenBucket::Type t);
    // UpdateFC 补充（唯一补充路径）
    void update(uint32_t vf_id, FcTokenBucket::Type t, uint32_t credit);

    std::size_t bucket_count() const noexcept { return per_vf_buckets_.size(); }

private:
    EventQueue* eq_;
    std::unordered_map<uint32_t, FcTokenBucket> per_vf_buckets_;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_FLOW_CONTROL_TOKEN_BUCKET_HH