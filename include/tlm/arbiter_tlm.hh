// include/tlm/arbiter_tlm.hh
// ArbiterTLM：多端口仲裁模块（v2.1 TLM）
#ifndef TLM_ARBITER_TLM_HH
#define TLM_ARBITER_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <queue>
#include <unordered_map>

template<unsigned N_PORTS>
class ArbiterTLM : public ChStreamModuleBase {
private:
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>  req_out_;
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>  resp_in_;
    cpptlm::StreamAdapterBase* adapters_[N_PORTS] = {nullptr};
    cpptlm::StreamAdapterBase* single_adapter_ = nullptr;

    struct QueuedReq {
        bundles::CacheReqBundle bundle;
        unsigned src_port;
    };
    std::queue<QueuedReq> req_queue_;
    unsigned last_served_ = 0;
    std::unordered_map<uint64_t, unsigned> txn_to_port_;

public:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>  req_in[N_PORTS];
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out[N_PORTS];

public:
    explicit ArbiterTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}

    std::string get_module_type() const override { return "ArbiterTLM"; }

    unsigned num_ports() const override { return N_PORTS; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        single_adapter_ = adapter;
    }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override {
        for (unsigned i = 0; i < N_PORTS; i++) {
            adapters_[i] = adapters[i];
        }
    }

    void tick() override {
        for (unsigned i = 0; i < N_PORTS; i++) {
            if (req_in[i].valid() && req_in[i].ready()) {
                QueuedReq qr;
                qr.bundle = req_in[i].data();
                qr.src_port = i;
                req_queue_.push(qr);
                txn_to_port_[qr.bundle.transaction_id.read()] = i;
                req_in[i].consume();
            }
        }

        if (!req_queue_.empty() && req_out_.valid() == false) {
            auto& req = req_queue_.front().bundle;
            req_out_.write(req);
            req_queue_.pop();
            last_served_ = (last_served_ + 1) % N_PORTS;
        }

        if (resp_in_.valid() && resp_in_.ready()) {
            auto& resp = resp_in_.data();
            uint64_t txn_id = resp.transaction_id.read();
            auto it = txn_to_port_.find(txn_id);
            if (it != txn_to_port_.end()) {
                resp_out[it->second].write(resp);
                txn_to_port_.erase(it);
            }
            resp_in_.consume();
        }

        for (unsigned i = 0; i < N_PORTS; i++) {
            if (adapters_[i]) adapters_[i]->tick();
        }
        if (single_adapter_) single_adapter_->tick();
    }

    void do_reset(const ResetConfig& config) override {
        for (unsigned i = 0; i < N_PORTS; i++) {
            req_in[i].reset();
            resp_out[i].reset();
        }
        req_out_.reset();
        resp_in_.reset();
        last_served_ = 0;
        while (!req_queue_.empty()) req_queue_.pop();
        txn_to_port_.clear();
    }

    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out() { return req_out_; }
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in() { return resp_in_; }
};

#endif
