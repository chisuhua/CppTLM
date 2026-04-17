/**
 * @file traffic_gen_tlm.hh
 * @brief TrafficGenTLM：流量生成模块（v2.1 TLM）
 * 
 * 支持 SEQUENTIAL / RANDOM / TRACE 3种基础模式，
 * 以及 HOTSPOT / STRIDED / NEIGHBOR / TORNADO 4种压力模式。
 * 通过 set_stress_pattern() 配置，压力模式地址生成覆盖基础模式。
 * 
 * @author CppTLM Development Team
 * @date 2026-04-16
 */

#ifndef TLM_TRAFFIC_GEN_TLM_HH
#define TLM_TRAFFIC_GEN_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include "tlm/stress_patterns.hh"
#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

// TrafficGenTLM uses GenMode_TLM to avoid ODR conflict with legacy GenMode
enum class GenMode_TLM { SEQUENTIAL, RANDOM, TRACE };

struct TraceRecord_TLM {
    uint64_t addr;
    bool is_write;
};

class TrafficGenTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>  resp_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>  req_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    GenMode_TLM mode_ = GenMode_TLM::SEQUENTIAL;
    uint64_t start_addr_ = 0x1000;
    uint64_t end_addr_ = 0x2000;
    uint64_t cur_addr_ = 0x1000;
    int num_requests_ = 20;
    int completed_ = 0;
    int issued_ = 0;
    std::mt19937 rng_;
    std::uniform_int_distribution<uint64_t> addr_dist_;
    std::vector<TraceRecord_TLM> trace_;
    size_t trace_pos_ = 0;

    // 压力模式支持
    StressPattern stress_pattern_ = StressPattern::SEQUENTIAL;
    std::unique_ptr<StressPatternStrategy> strategy_;

    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_requests_issued_;
    tlm_stats::Scalar& stats_requests_completed_;
    tlm_stats::Scalar& stats_reads_;
    tlm_stats::Scalar& stats_writes_;
    tlm_stats::Distribution& stats_latency_;
    tlm_stats::Distribution& stats_addr_distribution_;
    std::unordered_map<uint64_t, uint64_t> txn_issue_time_;
    uint64_t current_cycle_ = 0;

public:
    explicit TrafficGenTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          cur_addr_(start_addr_),
          completed_(0),
          issued_(0),
          rng_(42),
          addr_dist_(start_addr_, end_addr_),
          strategy_(create_strategy(StressPattern::SEQUENTIAL)),
          stats_("traffic_gen", nullptr),
          stats_requests_issued_(stats_.addScalar("requests_issued", "Total requests issued", "count")),
          stats_requests_completed_(stats_.addScalar("requests_completed", "Total requests completed", "count")),
          stats_reads_(stats_.addScalar("reads", "Read requests", "count")),
          stats_writes_(stats_.addScalar("writes", "Write requests", "count")),
          stats_latency_(stats_.addDistribution("latency", "Request-response latency", "cycle")),
          stats_addr_distribution_(stats_.addDistribution("addr_dist", "Address distribution", "addr")) {
        trace_ = {
            {0x1000, false},
            {0x1004, true},
            {0x1008, false},
            {0x100c, true}
        };
    }

    std::string get_module_type() const override { return "TrafficGenTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    void set_mode(GenMode_TLM m) { mode_ = m; }
    void set_num_requests(int n) { num_requests_ = n; }

    // 压力模式配置方法
    void set_stress_pattern(StressPattern p) {
        stress_pattern_ = p;
        strategy_ = create_strategy(p);
    }
    void set_hotspot_config(const std::vector<uint64_t>& addrs,
                             const std::vector<double>& weights) {
        if (auto* h = dynamic_cast<HotspotStrategy*>(strategy_.get())) {
            h->set_hotspot_config(addrs, weights);
        }
    }
    void set_stride(uint64_t s) {
        if (auto* st = dynamic_cast<StridedStrategy*>(strategy_.get())) {
            st->set_stride(s);
        }
    }
    void set_mesh_config(uint64_t w, uint64_t h) {
        if (auto* t = dynamic_cast<TornadoStrategy*>(strategy_.get())) {
            t->set_mesh_config(w, h);
        }
    }

    void tick() override {
        current_cycle_++;

        if (resp_in_.valid() && resp_in_.ready()) {
            auto& resp = resp_in_.data();
            uint64_t txn_id = resp.transaction_id.read();
            auto it = txn_issue_time_.find(txn_id);
            if (it != txn_issue_time_.end()) {
                uint64_t latency = current_cycle_ - it->second;
                stats_latency_.sample(latency);
                txn_issue_time_.erase(it);
            }
            completed_++;
            stats_requests_completed_++;
            resp_in_.consume();
        }

        if (completed_ < num_requests_ && issued_ < num_requests_) {
            if (std::uniform_int_distribution<int>{0, 9}(rng_) == 0) {
                issueRequest();
            }
        }

        if (adapter_) adapter_->tick();
    }

    void issueRequest() {
        bundles::CacheReqBundle req;
        bool is_write = false;
        uint64_t addr = 0;

        switch (mode_) {
            case GenMode_TLM::SEQUENTIAL:
                addr = cur_addr_;
                is_write = (cur_addr_ % 8 == 0);
                cur_addr_ += 4;
                if (cur_addr_ >= end_addr_) cur_addr_ = start_addr_;
                break;
            case GenMode_TLM::RANDOM:
                addr = addr_dist_(rng_);
                is_write = (rng_() % 2 == 0);
                break;
            case GenMode_TLM::TRACE:
                if (trace_pos_ >= trace_.size()) return;
                addr = trace_[trace_pos_].addr;
                is_write = trace_[trace_pos_].is_write;
                trace_pos_++;
                break;
        }

        // 如果使用了 stress pattern 策略（非默认 SEQUENTIAL），覆盖地址生成
        if (stress_pattern_ != StressPattern::SEQUENTIAL && strategy_) {
            addr = strategy_->next_address(start_addr_, end_addr_ - start_addr_);
        }

        req.transaction_id.write(issued_++);
        req.address.write(addr);
        req.is_write.write(is_write ? 1 : 0);
        req.data.write(0);
        req.size.write(4);
        req_out_.write(req);

        txn_issue_time_[req.transaction_id.read()] = current_cycle_;
        stats_requests_issued_++;
        if (is_write) {
            stats_writes_++;
        } else {
            stats_reads_++;
        }
        stats_addr_distribution_.sample(addr);
    }

    void do_reset(const ResetConfig& config) override {
        resp_in_.reset();
        req_out_.reset();
        cur_addr_ = start_addr_;
        completed_ = 0;
        issued_ = 0;
        trace_pos_ = 0;
        txn_issue_time_.clear();
        stats_.reset();
        current_cycle_ = 0;
        if (strategy_) strategy_->reset();
    }

    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in() { return resp_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out() { return req_out_; }
    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }

    // Initiator 不接收请求，这些是空适配器（满足 StreamAdapter 接口）
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        static cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        static cpptlm::InputStreamAdapter<bundles::CacheReqBundle> dummy;
        return dummy;
    }

    tlm_stats::StatGroup& stats() { return stats_; }

    void dumpStats(std::ostream& os) {
        stats_.dump(os, getName(), 50);
    }
};

#endif
