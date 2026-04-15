// include/tlm/traffic_gen_tlm.hh
// TrafficGenTLM：流量生成模块（v2.1 TLM）
#ifndef TLM_TRAFFIC_GEN_TLM_HH
#define TLM_TRAFFIC_GEN_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <vector>
#include <random>

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

public:
    explicit TrafficGenTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          cur_addr_(start_addr_),
          completed_(0),
          issued_(0),
          rng_(42),
          addr_dist_(start_addr_, end_addr_) {
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

    void tick() override {
        if (resp_in_.valid() && resp_in_.ready()) {
            completed_++;
            resp_in_.consume();
        }

        if (completed_ < num_requests_ && issued_ < num_requests_) {
            if (rand() % 10 == 0) {
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

        req.transaction_id.write(issued_++);
        req.address.write(addr);
        req.is_write.write(is_write ? 1 : 0);
        req.data.write(0);
        req.size.write(4);
        req_out_.write(req);
    }

    void do_reset(const ResetConfig& config) override {
        resp_in_.reset();
        req_out_.reset();
        cur_addr_ = start_addr_;
        completed_ = 0;
        issued_ = 0;
        trace_pos_ = 0;
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
};

#endif
