// include/tlm/cpu_tlm.hh
// CPUTLM：发起请求的 CPU 模块（v2.1 TLM）
#ifndef TLM_CPU_TLM_HH
#define TLM_CPU_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
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

public:
    explicit CPUTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          cur_addr_(start_addr_),
          timer_(0),
          next_txn_id_(1) {}

    std::string get_module_type() const override { return "CPUTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    void tick() override {
        if (resp_in_.valid() && resp_in_.ready()) {
            auto& resp = resp_in_.data();
            uint64_t txn_id = resp.transaction_id.read();
            inflight_txns_.erase(txn_id);
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
            inflight_txns_[req.transaction_id.read()] = cur_addr_;
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
