// ============================================================================
// cohif_responder.hh —— CEP「完成者」= **真实 home 的简化模型**(home 简化模型)
//
// 功能:消费 CohIf `Request` ⇒ 经**参数化延迟**回 `Completion`;按读请求**可配发** `Snoop`;
//       `Writeback` **接收即消费、零响应**(fire-and-forget)。
// 作者:CppTLM build agent(builder)  日期:2026-11-08
//
// ── 定位(per ADR-CEP-19 SU 2026-11-08 · 三 ③ / 07 号 SU · 十二 ②)───────────
//   · 承载**第一族(CHI RN-F)的最小应答语义**;
//   · ⛔ **不**承载家族级 / 条款级语义(credit · retry · ordering point · 多拍聚合 ·
//     Perm 规则 · 容量与队列模型);
//   · ⇒ ⛔ **不**构成任何家族的 T1–T4 条款级证据 ⇒ **B3b-2 维持 0/4**;
//   · ⛔ **不进** `protocol_instances.json`(声明面已裁**甲**:它不是协议实例);
//   · ⛔ **不是** ProtocolEngineBase(独立 `ChStreamModuleBase` 派生)。
//
// ── 端口 ────────────────────────────────────────────────────────────────
//   N = 2(对称):`req_in[i]` = ingress(收 CohIfBundle)· `resp_out[i]` = egress(发)
//   ⇒ `Completion` **回到请求所在的 ingress 端口**(端口按 ingress 分桶,互不阻塞)。
//
// ── 本 change **不**建模的配对(显式登记,⛔ 不留半截含义)──────────────────
//   `Snoop` 的 `transaction_id` 使用**独立命名空间**(高位定位),⛔ 不与触发它的 `Request`
//   的 txn 关联 ⇒ 「snoop / snoop_resp 按 txn 配对」不在本模型语义内(得由协议引擎承载)。
// ============================================================================
#ifndef TLM_CEP_COHIF_RESPONDER_HH
#define TLM_CEP_COHIF_RESPONDER_HH

#include "bundles/cep_coherence_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/protocol_bridge.hh"
#include "framework/stream_adapter.hh"
#include "tlm/cep/stub_cache_core.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

namespace tlm::cep {

class CohIfResponderTLM : public ChStreamModuleBase {
public:
    using Bundle = bundles::CohIfBundle;
    using ReqAdapter = cpptlm::InputStreamAdapter<Bundle>;
    using RespAdapter = cpptlm::OutputStreamAdapter<Bundle>;

    static constexpr std::size_t kPortCount = 2;
    /// `Snoop` 的 txn 命名空间(高位定位;与 input `Request` 的 txn 空间不重叠)
    static constexpr std::uint64_t kSnoopTxnBase = 1ULL << 63;

    struct Config {
        /// `Completion` 的**参数化延迟**(周期)
        unsigned completion_latency_cycles = 4;
        /// 是否**可配发** `Snoop`(默认关 ⇒ 「可配发」的默认态)
        bool emit_snoops = false;
        /// 发 `Snoop` 的周期间隔(仅 `emit_snoops = true` 时生效)
        unsigned snoop_interval_cycles = 8;
        /// 是否**回发** `Completion`(默认开 = 模块级默认行为)
        /// ⚠️ 拓扑 A/B 把它**关掉**:dual-port 适配器的**入向路由恒落 PE 侧**
        ///    (`include/framework/bidirectional_port_adapter.hh` 的 `process_request_input`
        ///    只填 `pe_req_in()`)⇒ 回程 `Completion` 会被当成业务请求读进 PE 侧
        ///    ⇒ **产生垃圾请求**。⛔ 框架红线(`include/framework/` 零改动)⇒ 用本开关
        ///    把回程腿**留在模块级判据**(SCE-03 / SCE-04)闭合,拓扑级只闭合**出向腿**。
        bool emit_completions = true;
    };

    struct Counters {
        std::uint64_t requests_consumed = 0;    ///< 消费的 `Request`(⇒ 将回 `Completion`)
        std::uint64_t completions_sent = 0;     ///< 发出的 `Completion`
        std::uint64_t snoops_sent = 0;          ///< 发出的 `Snoop`
        std::uint64_t writebacks_consumed = 0;  ///< 消费的 `Writeback`(零响应)
        std::uint64_t snoop_resps_consumed = 0; ///< 消费的 `SnoopResp`(零响应)
        std::uint64_t foreign_consumed = 0;     ///< 其余流(零响应;如实记账)
        std::uint64_t completions_suppressed = 0; ///< 因 `emit_completions=false` 被抑制的回程
    };

    CohIfResponderTLM(const std::string& name, EventQueue* eq) : ChStreamModuleBase(name, eq) {}
    ~CohIfResponderTLM() override = default;

    std::string get_module_type() const override { return "CohIfResponderTLM"; }
    unsigned num_ports() const override { return static_cast<unsigned>(kPortCount); }

    // ── 端口访问器(照 `ProtocolEngineBase<N>` 范式)──────────────────
    std::array<ReqAdapter, kPortCount>& req_in() { return req_in_; }
    std::array<RespAdapter, kPortCount>& resp_out() { return resp_out_; }
    const std::array<ReqAdapter, kPortCount>& req_in() const { return req_in_; }
    const std::array<RespAdapter, kPortCount>& resp_out() const { return resp_out_; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override { adapter_ = adapter; }

    /// Credit 回收回调(`BidirectionalPortAdapter` 收到 credit 时会调 ⇒ 必须存在)
    /// ⚠️ CEP 的 home 模型**不建模链路 credit 回收** ⇒ 默认空实现(与 `ProtocolEngineBase` 同款)
    void receive_credit(unsigned /*port*/, unsigned /*vc*/) {}  // 鸭子类型调用点,非虚函数

    // ── 装配期桥挂载(per `ChStreamModuleBase::attach_protocol_bridge`)──
    bool attach_protocol_bridge(cpptlm::ProtocolBridgeEr* bridge) override {
        bridge_ = bridge;
        return true;
    }
    /// 只读桥指针(⛔ 不动基类 ⇒ 本类自存 + 暴露访问器)
    cpptlm::ProtocolBridgeEr* bridge() const { return bridge_; }

    // ── 配置 / 观测 ─────────────────────────────────────────────────
    void configure(const Config& cfg) { cfg_ = cfg; }

    /// 装配期配置(**只**认下列 4 键;⛔ 其余键忽略)
    ///   `emit_completions` · `completion_latency_cycles` · `emit_snoops` · `snoop_interval_cycles`
    void on_config_loaded() override {
        if (config_.contains("emit_completions") && config_["emit_completions"].is_boolean())
            cfg_.emit_completions = config_["emit_completions"].get<bool>();
        if (config_.contains("completion_latency_cycles") &&
            config_["completion_latency_cycles"].is_number_unsigned())
            cfg_.completion_latency_cycles = config_["completion_latency_cycles"].get<unsigned>();
        if (config_.contains("emit_snoops") && config_["emit_snoops"].is_boolean())
            cfg_.emit_snoops = config_["emit_snoops"].get<bool>();
        if (config_.contains("snoop_interval_cycles") &&
            config_["snoop_interval_cycles"].is_number_unsigned())
            cfg_.snoop_interval_cycles = config_["snoop_interval_cycles"].get<unsigned>();
    }
    const Config& config() const { return cfg_; }
    const Counters& counters() const { return ctr_; }
    std::size_t in_flight() const {
        std::size_t n = 0;
        for (const auto& q : pending_)
            n += q.size();
        return n;
    }
    std::uint64_t cycle() const { return cycle_; }
    /// 最近一次消费的 `Request` 的地址(只读观测面;供「事务抵达且地址一致」判定)
    std::uint64_t last_request_addr() const { return last_addr_; }

    // ── 周期推进 ────────────────────────────────────────────────────
    void tick() override {
        ++cycle_;

        // ① 消费 ingress(按 flow_id 分派;记录来源端口)
        for (std::size_t i = 0; i < kPortCount; ++i) {
            auto& in = req_in_[i];
            if (!in.valid())
                continue;
            consume(in.data(), i);
            in.consume();
        }

        // ② 到期 `Completion` 发出(**按 ingress 分桶** ⇒ 一个慢端口不阻塞其它端口)
        for (std::size_t port = 0; port < kPortCount; ++port) {
            auto& q = pending_[port];
            while (!q.empty() && q.front().ready_cycle <= cycle_) {
                if (resp_out_[port].valid())
                    break;  // 背压:本端口下周期再发(⛔ 不丢,也⛔ 不串端口)
                const Pending p = q.front();
                if (!cfg_.emit_completions) {
                    // 拓扑级:回程**不出网**(框架限制,见 `Config::emit_completions` 注释)
                    ++ctr_.completions_suppressed;
                    q.pop_front();
                    continue;
                }
                Bundle b;
                b.flow_id = bundles::CohIfFlowId::Completion;
                b.transaction_id.write(p.txn_id);
                b.address.write(p.addr);
                b.data.write(p.data);
                b.perm.write(p.perm);
                b.error.write(p.error ? 1 : 0);
                resp_out_[port].write(b);
                ++ctr_.completions_sent;
                q.pop_front();
            }
        }

        // ③ 可配发 `Snoop`(默认关;发回**最近请求的来源端口**)
        if (cfg_.emit_snoops && snoop_interval_cycles_hit() && last_addr_ != 0) {
            const std::size_t port = last_ingress_port_;
            if (!resp_out_[port].valid()) {
                Bundle b;
                b.flow_id = bundles::CohIfFlowId::Snoop;
                b.transaction_id.write(kSnoopTxnBase + next_snoop_txn_++);
                b.address.write(last_addr_);
                b.snoop_type.write(static_cast<std::uint8_t>(SnoopType::INVALIDATE));
                resp_out_[port].write(b);
                ++ctr_.snoops_sent;
            }
        }

        // ④ 驱动 StreamAdapter(发送 egress + credit 回收)
        if (adapter_)
            adapter_->tick();
    }

private:
    struct Pending {
        std::uint64_t txn_id = 0;
        std::uint64_t addr = 0;
        std::uint64_t data = 0;
        std::uint8_t perm = 0;
        bool error = false;
        std::uint64_t ready_cycle = 0;
    };

    bool snoop_interval_cycles_hit() const {
        return cfg_.snoop_interval_cycles != 0 && (cycle_ % cfg_.snoop_interval_cycles == 0);
    }

    void consume(const Bundle& b, std::size_t ingress_port) {
        const auto flow = b.flow_id;
        switch (flow) {
            case bundles::CohIfFlowId::Request: {
                ++ctr_.requests_consumed;
                Pending p;
                p.txn_id = b.transaction_id.read();
                p.addr = b.address.read();
                p.data = b.data.read();
                p.perm = static_cast<std::uint8_t>(b.perm.read());
                p.ready_cycle = cycle_ + cfg_.completion_latency_cycles;
                last_addr_ = p.addr;
                last_ingress_port_ = ingress_port;
                pending_[ingress_port].push_back(p);
                break;
            }
            case bundles::CohIfFlowId::Writeback:
                // **接收即消费、零响应**(fire-and-forget)
                ++ctr_.writebacks_consumed;
                break;
            case bundles::CohIfFlowId::SnoopResp:
                ++ctr_.snoop_resps_consumed;
                break;
            default:
                // home ☠ 不应收到 `Completion` / `Snoop`;如实记账,零响应
                ++ctr_.foreign_consumed;
                break;
        }
    }

    Config cfg_{};
    Counters ctr_{};
    cpptlm::ProtocolBridgeEr* bridge_ = nullptr;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    std::array<ReqAdapter, kPortCount> req_in_{};
    std::array<RespAdapter, kPortCount> resp_out_{};
    std::array<std::deque<Pending>, kPortCount> pending_{};
    std::uint64_t cycle_ = 0;
    std::uint64_t last_addr_ = 0;
    std::size_t last_ingress_port_ = 0;
    std::uint64_t next_snoop_txn_ = 1;
};

}  // namespace tlm::cep

#endif  // TLM_CEP_COHIF_RESPONDER_HH
