// ============================================================================
// stub_cache_endpoint.hh —— CEP 端口面 wrapper(`StubCacheEndpointTLM`)
//
// 功能:把**纯类** `StubCacheCore`(cpu_side 功能接口 + net_side 功能 5 流)包成
//       **可注册的 ChStream 端点**,使 `TrafficGenTLM` 的真实业务流量可经它抵达
//       协议引擎与 home 应答端。
// 作者:CppTLM build agent(builder)  日期:2026-11-08
//
// ── 形态(per ADR-CEP-19 SU · 八 ①「组合式 wrapper」)────────────────────────
//   `StubCacheEndpointTLM` **持有** `StubCacheCore` 实例 ⇒ `stub_cache_core.hh`
//   **纯类零 diff**(`INV-SC1`~`INV-SC6` 与既有单测零变化)。
//   ⛔ **不**继承 `StubCacheCore`;⛔ **不**给纯类加端口。
//
// ── 端口(异构 dual-port,per design D-1)──────────────────────────────────────
//   `pe_req_in()`   in  : `CacheReqBundle`      (业务侧请求;cpu_side 功能口径)
//   `pe_resp_out()` out : `CacheRespBundle`     (业务侧响应 —— **命中亦回**,一请求一响应)
//   `net_req_out()` out : `CohIfBundle`         (Request / SnoopResp / Writeback)
//   `net_resp_in()` in  : `CohIfBundle`         (Completion / Snoop)
//
// ── 语义分层红线(per design D-2 —— 逐字)──────────────────────────────────────
//   wrapper 只做「**功能方法 ↔ Bundle**」的**端点编码**;桥只做「Bundle ↔ flit」的
//   承载投影。`flow_id` → canonical 流的解释权在**类型层**(`CohIfFlowId` 强类型枚举)
//   ⇒ wrapper 只**产生**合法 `flow_id`,⛔ **不解释**其含义。
//   ⇒ 映射实现**仅许**存在于本模块;⛔ 不得另立 mapper / 翻译 / 桥接模块。
// ============================================================================
#ifndef TLM_CEP_STUB_CACHE_ENDPOINT_HH
#define TLM_CEP_STUB_CACHE_ENDPOINT_HH

#include "bundles/cache_bundles_tlm.hh"
#include "bundles/cep_coherence_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/protocol_bridge.hh"
#include "framework/stream_adapter.hh"
#include "tlm/cep/stub_cache_core.hh"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace tlm::cep {

class StubCacheEndpointTLM : public ChStreamModuleBase {
public:
    using CacheReq = bundles::CacheReqBundle;
    using CacheResp = bundles::CacheRespBundle;
    using Bundle = bundles::CohIfBundle;

    struct Counters {
        std::uint64_t cpu_requests = 0;       ///< 消费的业务侧请求
        std::uint64_t cpu_responses = 0;      ///< 回业务侧的响应(**含命中**)
        std::uint64_t net_requests_out = 0;   ///< 经 net_side 发出的 Request
        std::uint64_t net_completions_in = 0; ///< 收到的 Completion
        std::uint64_t net_snoops_in = 0;      ///< 收到的 Snoop
        std::uint64_t net_snoop_resps_out = 0;
        std::uint64_t net_writebacks_out = 0;
    };

    /// 原始观测事实(⛔ 不做聚合 —— latency 分位等归 `test/` 的 harness)
    struct TxnStamp {
        std::uint64_t txn_id = 0;
        std::uint64_t addr = 0;
        std::uint64_t issue_cycle = 0;
        std::uint64_t complete_cycle = 0;
        bool completed = false;
        bool hit = false;
    };

    StubCacheEndpointTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}
    ~StubCacheEndpointTLM() override = default;

    std::string get_module_type() const override { return "StubCacheEndpointTLM"; }
    unsigned num_ports() const override { return 4; }

    // ── 端口访问器(异构 dual-port;照 `NICTLM` 范式)─────────────────
    cpptlm::InputStreamAdapter<CacheReq>& pe_req_in() { return pe_req_in_; }
    cpptlm::OutputStreamAdapter<CacheResp>& pe_resp_out() { return pe_resp_out_; }
    cpptlm::OutputStreamAdapter<Bundle>& net_req_out() { return net_req_out_; }
    cpptlm::InputStreamAdapter<Bundle>& net_resp_in() { return net_resp_in_; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override { adapter_ = adapter; }

    // ── 装配期桥挂载 + 只读桥指针(⛔ 不动基类)───────────────────────
    bool attach_protocol_bridge(cpptlm::ProtocolBridgeEr* bridge) override {
        bridge_ = bridge;
        return true;
    }
    cpptlm::ProtocolBridgeEr* bridge() const { return bridge_; }

    // ── 观测面(原始事实)──────────────────────────────────────────────
    const Counters& counters() const { return ctr_; }
    const std::vector<TxnStamp>& txn_stamps() const { return stamps_; }
    std::size_t in_flight() const { return core_.outstanding_count(); }
    std::uint64_t cycle() const { return cycle_; }

    // ── 纯类访问(测试 / 装配夹具用;⛔ 不改变「纯类零 diff」)────────
    StubCacheCore& core() { return core_; }
    const StubCacheCore& core() const { return core_; }

    /// `evict_line` 的**功能原语**承载(缺口如实:⛔ 不伪造 `CacheReqBundle` 字段)
    /// ⇒ 由装配夹具显式调用,并把写回**作为 net_side 的 `Writeback` 流**发出。
    void evict(std::uint64_t addr) {
        const WritebackOut wb = core_.evict_line(addr);
        if (!wb.valid)
            return;
        Bundle b;
        b.flow_id = bundles::CohIfFlowId::Writeback;
        b.transaction_id.write(next_txn_++);
        b.address.write(wb.addr);
        b.data.write(wb.data);
        b.dirty.write(wb.dirty ? 1 : 0);
        pending_net_.push_back(b);
    }

    // ── 周期推进 ────────────────────────────────────────────────────
    void tick() override {
        ++cycle_;
        handle_cpu_ingress();
        handle_net_ingress();
        drain_net_egress();
        core_.tick();
        if (adapter_)
            adapter_->tick();
    }

private:
    void handle_cpu_ingress() {
        if (!pe_req_in_.valid())
            return;
        const CacheReq req = pe_req_in_.data();
        pe_req_in_.consume();
        ++ctr_.cpu_requests;

        const std::uint64_t addr = req.address.read();
        const bool is_write = req.is_write.read();
        const std::uint64_t data = req.data.read();

        const AccessResult r = is_write ? core_.cpu_write(addr, data) : core_.cpu_read(addr);

        if (r.hit) {
            // **命中亦回**(一请求一响应 ⇒ latency 样本完整;`INV-SC6` 零网络流量保持)
            CacheResp resp(req.transaction_id.read(), r.data, /*hit=*/true);
            emit_cpu_resp(resp);
            record_stamp(req.transaction_id.read(), addr, /*hit=*/true);
        } else {
            Bundle b;
            b.flow_id = bundles::CohIfFlowId::Request;
            b.transaction_id.write(r.txn_id);
            b.address.write(addr);
            b.data.write(data);
            b.size.write(req.size.read());
            b.perm.write(static_cast<std::uint8_t>(is_write ? Perm::WRITEBACK : Perm::READ));
            pending_net_.push_back(b);
            ++ctr_.net_requests_out;
            record_stamp(r.txn_id, addr, /*hit=*/false);
        }
    }

    void handle_net_ingress() {
        if (!net_resp_in_.valid())
            return;
        const Bundle b = net_resp_in_.data();
        net_resp_in_.consume();

        switch (b.flow_id) {
            case bundles::CohIfFlowId::Completion: {
                ++ctr_.net_completions_in;
                const std::uint64_t txn = b.transaction_id.read();
                const std::uint64_t addr = b.address.read();
                const std::uint64_t data = b.data.read();
                const auto perm = static_cast<Perm>(b.perm.read());
                const bool error = b.error.read() != 0;
                core_.completion(addr, txn, data, perm, error);
                CacheResp resp(txn, data, /*hit=*/false, error ? 1 : 0);
                emit_cpu_resp(resp);
                complete_stamp(txn);
                break;
            }
            case bundles::CohIfFlowId::Snoop: {
                ++ctr_.net_snoops_in;
                core_.snoop(b.address.read(), static_cast<SnoopType>(b.snoop_type.read()),
                            b.transaction_id.read());
                break;
            }
            default:
                break;  // home ☠ 不应发 Completion/Snoop 之外的东西给 RN;忽略
        }
    }

    void drain_net_egress() {
        // ① core 的 snoop 响应队列(到期才出)
        SnoopResp sr;
        while (core_.snoop_resp(sr)) {
            Bundle b;
            b.flow_id = bundles::CohIfFlowId::SnoopResp;
            b.transaction_id.write(sr.txn_id);
            b.address.write(sr.addr);
            b.data.write(sr.data);
            b.hit.write(sr.hit ? 1 : 0);
            b.dirty.write(sr.dirty ? 1 : 0);
            b.line_state.write(static_cast<std::uint8_t>(sr.state));
            pending_net_.push_back(b);
            ++ctr_.net_snoop_resps_out;
        }
        // ② net egress 队列(背压 ⇒ 下周期再发,⛔ 不丢)
        while (!pending_net_.empty() && !net_req_out_.valid()) {
            net_req_out_.write(pending_net_.front());
            if (pending_net_.front().flow_id == bundles::CohIfFlowId::Writeback)
                ++ctr_.net_writebacks_out;
            pending_net_.pop_front();
        }
    }

    void emit_cpu_resp(const CacheResp& resp) {
        if (!pe_resp_out_.valid()) {
            pe_resp_out_.write(resp);
            ++ctr_.cpu_responses;
        }
    }

    void record_stamp(std::uint64_t txn, std::uint64_t addr, bool hit) {
        TxnStamp s;
        s.txn_id = txn;
        s.addr = addr;
        s.issue_cycle = cycle_;
        s.hit = hit;
        s.completed = hit;  // 命中在 issue 当期完成
        if (hit)
            s.complete_cycle = cycle_;
        stamps_.push_back(s);
    }

    void complete_stamp(std::uint64_t txn) {
        for (auto it = stamps_.rbegin(); it != stamps_.rend(); ++it) {
            if (it->txn_id == txn && !it->completed) {
                it->completed = true;
                it->complete_cycle = cycle_;
                return;
            }
        }
    }

    Counters ctr_{};
    cpptlm::ProtocolBridgeEr* bridge_ = nullptr;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    cpptlm::InputStreamAdapter<CacheReq> pe_req_in_{};
    cpptlm::OutputStreamAdapter<CacheResp> pe_resp_out_{};
    cpptlm::OutputStreamAdapter<Bundle> net_req_out_{};
    cpptlm::InputStreamAdapter<Bundle> net_resp_in_{};
    StubCacheCore core_{};
    std::deque<Bundle> pending_net_{};
    std::vector<TxnStamp> stamps_{};
    std::uint64_t cycle_ = 0;
    std::uint64_t next_txn_ = 1;
};

}  // namespace tlm::cep

#endif  // TLM_CEP_STUB_CACHE_ENDPOINT_HH
