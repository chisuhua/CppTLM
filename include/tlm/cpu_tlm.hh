// include/tlm/cpu_tlm.hh
// CPUTLM：发起请求的 CPU 模块（v2.1 TLM）
#ifndef TLM_CPU_TLM_HH
#define TLM_CPU_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include <cstdint>
#include <unordered_map>

class CPUTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>  resp_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>  req_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    uint64_t start_addr_ = 0x1000;
    uint64_t cur_addr_ = 0x1000;
    uint64_t request_interval_ = 10;
    uint64_t timer_ = 0;
    uint64_t next_txn_id_ = 1;
    static constexpr unsigned MAX_INFLIGHT = 4;
    std::unordered_map<uint64_t, uint64_t> inflight_txns_;
    std::unordered_map<uint64_t, uint64_t> inflight_issue_cycles_;
    // 测试可观察性：最近收到的响应 transaction_id（端到端响应路径健康指标）
    // P0-5 bug (module_factory.cc 多端口分支) 未修时此字段保持 0
    uint64_t last_response_transaction_id_ = 0;

    // F10 telemetry: transactions_issued + latency distribution
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_requests_issued_;
    tlm_stats::Scalar& stats_requests_completed_;
    tlm_stats::Distribution& stats_latency_;

public:
    explicit CPUTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          cur_addr_(start_addr_),
          timer_(0),
          next_txn_id_(1),
          stats_("cpu", nullptr),
          stats_requests_issued_(stats_.addScalar("requests_issued", "Total CPU requests issued", "count")),
          stats_requests_completed_(stats_.addScalar("requests_completed", "Total CPU responses received", "count")),
          stats_latency_(stats_.addDistribution("latency", "CPU request-to-response latency", "cycle")) {}

    std::string get_module_type() const override { return "CPUTLM"; }

    std::string get_stats_path() const override { return "system.cpu"; }

    tlm_stats::StatGroup* get_stats_group() override { return &stats_; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    void tick() override {
        if (resp_in_.valid() && resp_in_.ready()) {
            auto& resp = resp_in_.data();
            uint64_t txn_id = resp.transaction_id.read();
            inflight_txns_.erase(txn_id);
            auto it_cycle = inflight_issue_cycles_.find(txn_id);
            if (it_cycle != inflight_issue_cycles_.end()) {
                uint64_t latency = getEventQueue()->getCurrentCycle() - it_cycle->second;
                stats_latency_.sample(latency);
                inflight_issue_cycles_.erase(it_cycle);
            }
            ++stats_requests_completed_;
            last_response_transaction_id_ = txn_id;  // 观察：端到端响应回路可达性
            resp_in_.consume();
        }

        if (inflight_txns_.size() < MAX_INFLIGHT && timer_ == 0) {
            bundles::CacheReqBundle req;
            req.transaction_id.write(next_txn_id_++);
            req.address.write(cur_addr_);
            req.is_write.write(0);
            req.data.write(0);
            req.size.write(4);
            req_out_.write(req);
            uint64_t issued_txn = req.transaction_id.read();
            inflight_txns_[issued_txn] = cur_addr_;
            inflight_issue_cycles_[issued_txn] = getEventQueue()->getCurrentCycle();
            ++stats_requests_issued_;
            cur_addr_ += 4;
            if (cur_addr_ >= start_addr_ + 0x100) cur_addr_ = start_addr_;
        }

        timer_ = (timer_ + 1) % request_interval_;

        if (adapter_) adapter_->tick();
    }

    void do_reset(const ResetConfig& config) override {
        resp_in_.reset();
        req_out_.reset();
        cur_addr_ = start_addr_;
        timer_ = 0;
        next_txn_id_ = 1;
        inflight_txns_.clear();
        inflight_issue_cycles_.clear();
        last_response_transaction_id_ = 0;
    }

    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in() { return resp_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out() { return req_out_; }
    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }

    // 端到端可观察性：返回最近收到的响应 transaction_id
    // 0 表示尚未收到响应（P0-5 bug 未修时维持 0）
    uint64_t last_response_transaction_id() const { return last_response_transaction_id_; }

    // Initiator 不接收请求，这些是空适配器（满足 StreamAdapter 接口）
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        static cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        static cpptlm::InputStreamAdapter<bundles::CacheReqBundle> dummy;
        return dummy;
    }
};

#endif
