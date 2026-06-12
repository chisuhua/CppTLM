/**
 * @file arbiter_tlm.hh
 * @brief ArbiterTLM - 多端口仲裁模块（v2.1 TLM）
 * 
 * ArbiterTLM 是一个模板化的多端口仲裁器，用于在 N_PORTS 个输入端口间
 * 进行请求仲裁并路由到单一输出端口。支持 Round-Robin 仲裁策略，
 * 自动追踪事务 ID 以确保响应正确路由回源端口。
 * 
 * ## 功能特性
 * - **多端口仲裁**：N_PORTS 个输入端口，单一输出端口
 * - **Round-Robin 策略**：公平轮转，last_served_ 记录上次服务端口
 * - **事务追踪**：txn_to_port_ 映射 transaction_id → src_port
 * - **响应路由**：自动将响应路由回正确的源端口
 * 
 * ## 端口结构
 * - **输入**：req_in[N_PORTS] (InputStreamAdapter<CacheReqBundle>)
 * - **输出**：req_out (OutputStreamAdapter<CacheReqBundle>) — 仲裁后统一输出
 * - **响应输入**：resp_in (InputStreamAdapter<CacheRespBundle>) — 从下游返回
 * - **响应输出**：resp_out[N_PORTS] (OutputStreamAdapter<CacheRespBundle>) — 路由回源端口
 * 
 * ## 使用示例
 * ```cpp
 * // 4 端口仲裁器实例化
 * ArbiterTLM<4> arbiter("arbiter", event_queue);
 * 
 * // JSON 配置连接
 * // "connections": [
 *   {"src": "cache0.req_out", "dst": "arbiter.req_in.0"},
 *   {"src": "cache1.req_out", "dst": "arbiter.req_in.1"},
 *   {"src": "arbiter.req_out", "dst": "memory.req_in"}
 * ]
 * ```
 * 
 * ## 注册方式
 * 使用 REGISTER_CHSTREAM 宏注册：
 * ```cpp
 * REGISTER_CHSTREAM(ArbiterTLM<4>, "ArbiterTLM4")
 * ```
 * 
 * ## 注意事项
 * - 模板参数 N_PORTS 必须在编译期确定
 * - tick() 每周期执行：接收请求 → 仲裁 → 发送 → 处理响应
 * - txn_to_port_ 自动清理已完成事务
 * - 通过 set_stream_adapter() 注入 MultiPortStreamAdapter
 * 
 * @tparam N_PORTS 输入端口数量
 * @author CppTLM Team
 * @date 2024-05
 * @see ChStreamModuleBase
 * @see bundles/cache_bundles_tlm.hh
 */

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

    std::string get_module_type() const override {
        if constexpr (N_PORTS == 2) return "ArbiterTLM2";
        else if constexpr (N_PORTS == 4) return "ArbiterTLM4";
        else return "ArbiterTLM";
    }

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
