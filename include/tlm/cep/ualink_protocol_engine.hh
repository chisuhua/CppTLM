// include/tlm/cep/ualink_protocol_engine.hh
// UalinkProtocolEngine —— `ualink_4` 实例的**记账级专用承载引擎**
//
// 功能:交付 `cpptlm-cep-ualink-coherence-engine` —— 为 `ualink_4` 的
//       `request` / `completion` 两条 canonical 流提供**专用承载者**,
//       取代此前由 `NullProtocolEngine` 承载的拓扑层。
// 作者: CppTLM Team / 日期: 2026-11-07
//
// ── ⚠️ 强度裁决(per `ADR-CEP-12` **SU 2026-11-07**(Entry #269)②)────────
// 本引擎 = **记账级专用承载**(bookkeeping-only dedicated carrier)。
// 对 canonical 10 流的全部行为 = **计数器自增**;⛔ **零发射**(zero egress)、
// ⛔ **零协议状态机**、⛔ **零 UPLI 通道级语义**(`ReqCmd` 编码 / 4 通道时序 /
// `transaction tag` 配对 **均不实现**)。
// ⇒ 本引擎 **不构成**「UALink 协议语义已实现」的任何证据;
// ⇒ **B3b-2 维持 0/4**(本引擎**不改**任何计数,per `07 号` SU · 四 D-1)。
//
// ── 观测面:**三分法**(per `ADR-CEP-12` SU 2026-11-07 ④)────────────────
// | 状态 | 流 | 计数器 |
// |---|---|---|
// | **记账**(声明 `true`) | `request` · `completion` | `flow_events[10]` |
// | **拒绝**(声明 `false`) | `snoop` · `writeback` · `forward_data` · `back_invalidate` · `address_resolve` · `barrier` · `snoop_completion_ack`(7 条) | `unsupported_rejected[10]`(per-flow) |
// | **pending 不消费**(语义待定) | `snoop_resp` | `snoop_resp_pending_not_consumed` |
// ⚠️ **不复用** `MoesiGpuProtocolEngine` 的 M-6「10 流一律只记账」形态 ——
// 那会让观测面与 `flow_support` 声明**矛盾**(8 条声明 `false` 的流被正常记账)
// ⇒ 复犯 `07 号` SU · 四 D-1 点名的「静默 … 且看起来权威」失败模式。
// ✅ 既有范式:`MesifProtocolEngine::gate()`(「声明 false ⇒ 拒绝计数,不静默降级」)。
//
// ── 「零 TxnID 配对」的边界(⚠️ **第一道文本防线**)─────────────────────
// 「零 TxnID 配对」= **零 UPLI 协议层配对语义**(零 tag 配对规则 / 零
// request↔response 配对协议 / 零超时 / 零重试 / 零排序 / 零资源门控);
// **平台自有 `transaction_id` 字段的恒等记账**属**平台层** ⇒ **允许**,
// 且是「completion 记账不依赖有序性」判据**具备鉴别力**的必要条件。
// ⛔ 头文件与文档**禁用**「TxnID 管理」一词 —— 那是 CHI 真插件的词汇
// (per `openspec/specs/cep-chi-rnf-subset` **CR-13**)⇒ 会构成越界宣称。
//
// ── 在途集合(唯一的平台层状态)────────────────────────────────────────
// `on_request` 按 `txn_id` **插入**;`on_completion` 按 `txn_id` **恒等擦除**。
// **依据 = 平台自有相关性字段**:`dispatch_cohif` 把同一个 `b.transaction_id.read()`
// 传给两条流 ⇒ 按恒等匹配是**协议中立**的**记账决定**,**零 UALink 宣称**。
// ⚠️ **键型约束**:在途集合 SHALL 以 **`txn_id` 为唯一键**(⛔ 不得以地址 /
// 到达序 / 任何派生量作键)⇒ 「零 TxnID 配对」的边界见上条。
// ⚠️ **本集合不含任何协议层语义**:⛔ 无超时 / 无重试 / 无排序 / 无资源门控。
//
// ── `snoop_resp` 的**不对称**(per `ADR-CEP-12` SU 2026-11-07 ⑥)────────
// 声明给了它 `vc_binding`(VC3)却给 `flow_support = false`(per
// `protocol_instances.json`),而其余 7 条流**无任何正向陈述** ⇒ per
// `ADR-CEP-12` 决策 6,`mapping ≠ support` ⇒ 它处在「有邻近误导性陈述」的
// **不同认识论位置**。
// ⚠️ `pending` 是**终态处置,不是暂态** —— 语义待定,**恒不消费**;
// ⛔ **不得**读作「已支持、待完成 / 待处理」。
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only** · 端口数 **N = 2**(与 `MoesiGpuProtocolEngine` /
//     `MesifProtocolEngine` 同款)
//   · ⛔ **不**解析 JSON(声明是唯一真源)· ⛔ **不**改 `protocol_instances.json`
//   · ⛔ **不**改 `ualink_4` 的 13 键取值(声明逐字不动)
//   · ⛔ **不得**为 `response` 消息类 / collective 结果回传路径**新增**任何
//     handler(= 锚点摘要 I-6 路径)⇒ 承载者仍是 `FabricCollectivePlane`
#ifndef TLM_CEP_UALINK_PROTOCOL_ENGINE_HH
#define TLM_CEP_UALINK_PROTOCOL_ENGINE_HH

#include "tlm/cep/protocol_engine_base.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace tlm::cep {

    class UalinkProtocolEngine : public ProtocolEngineBase<2> {
    public:
        using Base = ProtocolEngineBase<2>;
        using PlatformPerm = tlm::cep::Perm;

        /// 引擎的**可观测状态**(逐流 + 标量)。
        struct Counters {
            /// 逐 canonical 流**抵达**计数(**全部 10 流**都计,不限记账/拒绝/pending)
            std::array<std::uint64_t, kCanonicalFlowCount> flow_events{};
            /// 逐 canonical 流**拒绝**计数(声明 `false` 且**不是** `snoop_resp` 的 7 条)
            std::array<std::uint64_t, kCanonicalFlowCount> unsupported_rejected{};
            /// `snoop_resp` 的 pending 登记次数(语义待定,**恒不消费**)
            std::uint64_t snoop_resp_pending_not_consumed = 0;
            /// completion 的 `txn_id` **未匹配**到在途请求的次数(fail-closed 的牙)
            std::uint64_t completion_unmatched = 0;
            /// 引擎**发射**(egress)的 Bundle 总数 —— ⛔ 恒为 0(零发射)
            std::uint64_t egress_emitted = 0;
        };

        UalinkProtocolEngine(const std::string& name, EventQueue* eq) : Base(name, eq) {}
        ~UalinkProtocolEngine() override = default;

        const Counters& counters() const { return ctr_; }

        /// 在途 `txn_id` 集合(平台层记账状态;⛔ 无协议层语义)
        /// ⚠️ 返回**只读视图** ⇒ 外部无法改写引擎状态(判据可依赖其不可变性)
        const std::unordered_set<std::uint64_t>& outstanding_request_ids() const {
            return outstanding_;
        }

        // ══════════════════════════════════════════════════════════════════
        //  canonical 10 流 —— **三分法**(per `ADR-CEP-12` SU 2026-11-07 ④)
        // ══════════════════════════════════════════════════════════════════

        /// 流 1 `request` —— **记账**(声明 `true`)
        void on_request(std::uint64_t, PlatformPerm, std::uint64_t txn_id) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Request)];
            // 在途登记:**恒等插入**(⛔ 不依赖到达序的语义)
            outstanding_.insert(txn_id);
        }

        /// 流 2 `completion` —— **记账**(声明 `true`)
        void on_completion(std::uint64_t txn_id, std::uint64_t, PlatformPerm, bool) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Completion)];
            // ⚠️ **恒等匹配**擦除(⛔ 非 FIFO-pop / 非 LIFO / 非按到达序)
            const auto it = outstanding_.find(txn_id);
            if (it == outstanding_.end()) {
                // 未匹配的 completion ⇒ **计数 + 不插入 + 不崩**(fail-closed)
                ++ctr_.completion_unmatched;
                return;
            }
            outstanding_.erase(it);
        }

        /// 流 3 `snoop` —— **拒绝**(声明 `false`)
        void on_snoop(std::uint64_t, SnoopType, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Snoop)];
            ++ctr_.unsupported_rejected[static_cast<std::size_t>(CanonicalFlow::Snoop)];
        }

        /// 流 4 `snoop_resp` —— **pending 不消费**(语义待定,**恒不消费**)
        void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t,
                           std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::SnoopResp)];
            ++ctr_.snoop_resp_pending_not_consumed;
        }

        /// 流 5 `writeback` —— **拒绝**(声明 `false`)
        void on_writeback(std::uint64_t, std::uint64_t, bool) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Writeback)];
            ++ctr_.unsupported_rejected[static_cast<std::size_t>(CanonicalFlow::Writeback)];
        }

        /// 流 6 `forward_data` —— **拒绝**(声明 `false`)
        void on_forward_data(std::uint64_t, std::uint64_t, std::uint64_t, PlatformPerm,
                             std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::ForwardData)];
            ++ctr_.unsupported_rejected[static_cast<std::size_t>(CanonicalFlow::ForwardData)];
        }

        /// 流 7 `back_invalidate` —— **拒绝**(声明 `false`)
        void on_back_invalidate(std::uint64_t, SnoopType, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::BackInvalidate)];
            ++ctr_.unsupported_rejected[static_cast<std::size_t>(CanonicalFlow::BackInvalidate)];
        }

        /// 流 8 `address_resolve` —— **拒绝**(声明 `false`)
        void on_address_resolve(std::uint64_t, std::uint64_t, AddressResolveResult& out) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::AddressResolve)];
            ++ctr_.unsupported_rejected[static_cast<std::size_t>(CanonicalFlow::AddressResolve)];
            // ⚠️ **不解析地址**(声明 `address_resolve = false`)⇒ 输出保持默认
            out.home_node = 0;
        }

        /// 流 9 `barrier` —— **拒绝**(声明 `false`)
        void on_barrier(std::uint64_t, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Barrier)];
            ++ctr_.unsupported_rejected[static_cast<std::size_t>(CanonicalFlow::Barrier)];
        }

        /// 流 10 `snoop_completion_ack` —— **拒绝**(声明 `false`)
        void on_snoop_completion_ack(std::uint64_t, std::uint64_t, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::SnoopCompletionAck)];
            ++ctr_.unsupported_rejected[static_cast<std::size_t>(CanonicalFlow::SnoopCompletionAck)];
        }

    private:
        /// 在途 `txn_id` 集合 —— **唯一键 = `txn_id`**(⛔ 非地址 / 非到达序)
        std::unordered_set<std::uint64_t> outstanding_;
        Counters ctr_{};
    };

}  // namespace tlm::cep

#endif  // TLM_CEP_UALINK_PROTOCOL_ENGINE_HH
