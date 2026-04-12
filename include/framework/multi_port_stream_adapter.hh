// include/framework/multi_port_stream_adapter.hh
// 多端口 StreamAdapter：为 CrossbarTLM 等 N 端口模块管理多个端口对
// 功能描述：在模块 req_in[0..N-1]/resp_out[0..N-1] 与框架 Port 之间建立桥接
// 作者 CppTLM Team
// 日期 2026-04-13
#ifndef FRAMEWORK_MULTI_PORT_STREAM_ADAPTER_HH
#define FRAMEWORK_MULTI_PORT_STREAM_ADAPTER_HH

#include "stream_adapter.hh"
#include "core/master_port.hh"
#include "core/slave_port.hh"
#include <cstddef>
#include <array>

namespace cpptlm {

template<typename ModuleT, typename ReqBundleT, typename RespBundleT, std::size_t N>
class MultiPortStreamAdapter : public StreamAdapterBase {
private:
    ModuleT* module_;
    MasterPort* req_out_port_[N] = {nullptr};     // → 下游
    SlavePort*  resp_in_port_[N] = {nullptr};     // ← 上游响应
    MasterPort* resp_out_port_[N] = {nullptr};    // → 上游响应
    SlavePort*  req_in_port_[N] = {nullptr};      // ← 下游请求

public:
    explicit MultiPortStreamAdapter(ModuleT* mod) : module_(mod) {}

    void bind_ports(
        MasterPort*, SlavePort*,
        MasterPort* = nullptr, SlavePort* = nullptr
    ) override {
        // MultiPortStreamAdapter 使用 bind_ports_array，此方法为空操作
    }

    void bind_ports_array(
        std::array<MasterPort*, N> req_out,
        std::array<SlavePort*,  N> resp_in,
        std::array<MasterPort*, N> resp_out = {},
        std::array<SlavePort*,  N> req_in = {}
    ) {
        for (std::size_t i = 0; i < N; i++) {
            req_out_port_[i] = req_out[i];
            resp_in_port_[i] = resp_in[i];
            resp_out_port_[i] = (i < resp_out.size()) ? resp_out[i] : nullptr;
            req_in_port_[i] = (i < req_in.size()) ? req_in[i] : nullptr;
        }
    }

    void bind_port_pair(
        unsigned port_idx,
        MasterPort* req_out,
        SlavePort*  resp_in,
        MasterPort* resp_out = nullptr,
        SlavePort*  req_in = nullptr
    ) {
        if (port_idx < N) {
            req_out_port_[port_idx] = req_out;
            resp_in_port_[port_idx] = resp_in;
            resp_out_port_[port_idx] = resp_out;
            req_in_port_[port_idx] = req_in;
        }
    }

    void tick() override {
        for (std::size_t i = 0; i < N; i++) {
            if (module_->resp_out[i].valid()) {
                if (req_out_port_[i]) {
                    module_->resp_out[i].send(req_out_port_[i]);
                    module_->resp_out[i].clear_valid();
                }
            }
        }
    }

    void process_request_input(Packet* pkt) override {
        process_request_input(pkt, 0);
    }

    void process_request_input(Packet* pkt, std::size_t port_idx) {
        if (!pkt || !pkt->payload || port_idx >= N) return;
        auto& req_adapter = module_->req_in[port_idx];
        if (!req_adapter.valid()) {
            req_adapter.process(pkt);
        }
    }

    Packet* process_response_output() override { return nullptr; }

    ModuleT*       module() const  { return module_; }
    static constexpr std::size_t num_ports() { return N; }
};

} // namespace cpptlm

#endif // FRAMEWORK_MULTI_PORT_STREAM_ADAPTER_HH
