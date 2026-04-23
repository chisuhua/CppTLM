// include/framework/bidirectional_port_adapter.hh
// 双向端口 StreamAdapter：RouterTLM 等需要每端口双向通信的模块使用
// 功能描述：RouterTLM 每个端口需要同时支持 req_in (接收) 和 resp_out (发送)，
//           BidirectionalPortAdapter 管理 N 个双向端口对，每周期执行六阶段流水线
// 作者 CppTLM Team / 日期 2026-04-23
#ifndef FRAMEWORK_BIDIRECTIONAL_PORT_ADAPTER_HH
#define FRAMEWORK_BIDIRECTIONAL_PORT_ADAPTER_HH

#include "stream_adapter.hh"
#include "core/master_port.hh"
#include "core/slave_port.hh"
#include <cstddef>
#include <array>

namespace cpptlm {

/**
 * @brief 双向端口 StreamAdapter
 *
 * 用于 RouterTLM 等需要每端口双向通信的模块：
 * - 每个端口有 req_in (接收 flit) 和 resp_out (发送 flit)
 * - 支持 N 个双向端口 (RouterTLM 使用 5 端口: N/E/S/W/Local)
 *
 * 模板参数:
 *   ModuleT       - 模块类型（如 RouterTLM）
 *   BundleT      - 统一 Flit Bundle 类型（如 bundles::NoCFlitBundle）
 *   N            - 端口数量
 *
 * 端口访问器约定（ModuleT 必须提供）:
 *   req_in[port_idx]()  -> InputStreamAdapter<BundleT>&   // 接收 flit
 *   resp_out[port_idx]() -> OutputStreamAdapter<BundleT>&  // 发送 flit
 */
template<typename ModuleT, typename BundleT, std::size_t N>
class BidirectionalPortAdapter : public StreamAdapterBase {
private:
    ModuleT* module_;

    // 端口数组：每个端口有 req_out (发往下游) 和 resp_in (从下游收)
    MasterPort* req_out_port_[N] = {nullptr};     // → 下游 req
    SlavePort*  resp_in_port_[N] = {nullptr};     // ← 下游 resp
    MasterPort* resp_out_port_[N] = {nullptr};    // → 上游 resp
    SlavePort*  req_in_port_[N] = {nullptr};     // ← 上游 req

public:
    explicit BidirectionalPortAdapter(ModuleT* mod) : module_(mod) {}

    /**
     * @brief 绑定所有端口数组
     */
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

    /**
     * @brief 绑定单个端口对
     */
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

    /**
     * @brief 兼容 StreamAdapterBase 接口（空操作）
     */
    void bind_ports(
        MasterPort*, SlavePort*,
        MasterPort* = nullptr, SlavePort* = nullptr
    ) override {
        // BidirectionalPortAdapter 使用 bind_ports_array / bind_port_pair
    }

    /**
     * @brief 每周期处理所有端口的双向数据流
     *
     * RouterTLM 的 tick() 逻辑：
     * 1. 从 req_in[port] 读取 flit，存入 input_buffer
     * 2. 执行六阶段流水线 (BW→RC→VA→SA→ST→LT)
     * 3. 将待发送的 flit 写入 resp_out[port]
     * 4. 由本方法将 resp_out 发送到 req_out_port_
     *
     * 本方法仅处理框架侧端口搬运，流水线逻辑在 RouterTLM::tick()
     */
    void tick() override {
        for (std::size_t i = 0; i < N; i++) {
            if (module_->resp_out()[i].valid()) {
                if (req_out_port_[i]) {
                    module_->resp_out()[i].send(req_out_port_[i], PKT_REQ);
                    module_->resp_out()[i].clear_valid();
                }
            }
        }
    }

    /**
     * @brief 处理请求输入（从上游 Router 来的 flit）
     */
    void process_request_input(Packet* pkt) override {
        process_request_input(pkt, 0);
    }

    void process_request_input(Packet* pkt, std::size_t port_idx) {
        if (!pkt || !pkt->payload || port_idx >= N) return;
        auto& req_adapter = module_->req_in()[port_idx];
        if (!req_adapter.valid()) {
            req_adapter.process(pkt);
        }
    }

    /**
     * @brief 处理响应输入（从下游 Router 来的 flit）
     */
    void process_response_input(Packet* pkt, std::size_t port_idx) {
        if (!pkt || !pkt->payload || port_idx >= N) return;
        auto& req_adapter = module_->req_in()[port_idx];
        if (!req_adapter.valid()) {
            req_adapter.process(pkt);
        }
    }

    Packet* process_response_output() override {
        // 响应输出由 tick() 直接处理
        return nullptr;
    }

    ModuleT* module() const { return module_; }
    static constexpr std::size_t num_ports() { return N; }
};

} // namespace cpptlm

#endif // FRAMEWORK_BIDIRECTIONAL_PORT_ADAPTER_HH
