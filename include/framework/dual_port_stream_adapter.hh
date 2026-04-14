// include/framework/dual_port_stream_adapter.hh
// 双端口非对称 StreamAdapter：NIC 模块使用两组独立的端口对
// 功能描述：NICTLM 等模块同时需要 PE 侧（CacheReqBundle/CacheRespBundle）
//           和 Network 侧（NoCReqBundle/NoCRespBundle）的独立通信通道
// 作者 CppTLM Team
// 日期 2026-04-14
#ifndef FRAMEWORK_DUAL_PORT_STREAM_ADAPTER_HH
#define FRAMEWORK_DUAL_PORT_STREAM_ADAPTER_HH

#include "stream_adapter.hh"
#include "core/master_port.hh"
#include "core/slave_port.hh"

namespace cpptlm {

/**
 * @brief 双端口非对称 StreamAdapter
 *
 * 用于 NICTLM 等需要两组独立端口的模块：
 * - PE 侧：连接 Core/Cache，使用 PE_ReqBundle / PE_RespBundle
 * - Network 侧：连接 Router/NoC，使用 Net_ReqBundle / Net_RespBundle
 *
 * 两组端口各自独立，Bundle 类型可不同（非对称）。
 *
 * 模板参数:
 *   ModuleT       - 模块类型（如 NICTLM）
 *   PE_ReqBundleT  - PE 侧请求 Bundle（如 CacheReqBundle）
 *   PE_RespBundleT - PE 侧响应 Bundle（如 CacheRespBundle）
 *   Net_ReqBundleT - Network 侧请求 Bundle（如 NoCReqBundle）
 *   Net_RespBundleT - Network 侧响应 Bundle（如 NoCRespBundle）
 *
 * 端口访问器约定（ModuleT 必须提供）:
 *   pe_req_in()   -> InputStreamAdapter<PE_ReqBundleT>&
 *   pe_resp_out() -> OutputStreamAdapter<PE_RespBundleT>&
 *   net_req_in()  -> InputStreamAdapter<Net_ReqBundleT>&
 *   net_resp_out()-> OutputStreamAdapter<Net_RespBundleT>&
 *   net_req_out() -> OutputStreamAdapter<Net_ReqBundleT>&  (发往 NoC)
 *   pe_resp_in()  -> InputStreamAdapter<PE_RespBundleT>&   (从 NoC 回来)
 */
template<typename ModuleT,
         typename PE_ReqBundleT, typename PE_RespBundleT,
         typename Net_ReqBundleT, typename Net_RespBundleT>
class DualPortStreamAdapter : public StreamAdapterBase {
private:
    ModuleT* module_;

    // PE 侧端口（面向 Core/Cache）
    MasterPort* pe_req_out_port_ = nullptr;     // PE 请求 → NoC 方向
    SlavePort*  pe_resp_in_port_ = nullptr;     // PE 响应 ← NoC 方向
    MasterPort* pe_resp_out_port_ = nullptr;    // PE 响应输出（可选）
    SlavePort*  pe_req_in_port_ = nullptr;      // PE 请求输入（可选）

    // Network 侧端口（面向 Router/NoC）
    MasterPort* net_req_out_port_ = nullptr;    // 网络请求 → Router
    SlavePort*  net_resp_in_port_ = nullptr;    // 网络响应 ← Router
    MasterPort* net_resp_out_port_ = nullptr;   // 网络响应输出
    SlavePort*  net_req_in_port_ = nullptr;     // 网络请求输入

public:
    explicit DualPortStreamAdapter(ModuleT* mod) : module_(mod) {}

    /**
     * @brief 绑定 PE 侧端口对
     */
    void bind_pe_ports(
        MasterPort* pe_req_out,
        SlavePort*  pe_resp_in,
        MasterPort* pe_resp_out = nullptr,
        SlavePort*  pe_req_in = nullptr
    ) {
        pe_req_out_port_ = pe_req_out;
        pe_resp_in_port_ = pe_resp_in;
        pe_resp_out_port_ = pe_resp_out;
        pe_req_in_port_ = pe_req_in;
    }

    /**
     * @brief 绑定 Network 侧端口对
     */
    void bind_net_ports(
        MasterPort* net_req_out,
        SlavePort*  net_resp_in,
        MasterPort* net_resp_out = nullptr,
        SlavePort*  net_req_in = nullptr
    ) {
        net_req_out_port_ = net_req_out;
        net_resp_in_port_ = net_resp_in;
        net_resp_out_port_ = net_resp_out;
        net_req_in_port_ = net_req_in;
    }

    /**
     * @brief 兼容 StreamAdapterBase 接口（空操作，使用 bind_pe_ports/bind_net_ports）
     */
    void bind_ports(
        MasterPort*, SlavePort*,
        MasterPort* = nullptr, SlavePort* = nullptr
    ) override {
        // DualPortStreamAdapter 使用 bind_pe_ports / bind_net_ports，此方法为空操作
    }

    /**
     * @brief 每周期数据搬运：PE 侧 + Network 侧双向转发
     *
     * 方向语义：
     *   PE 请求: pe_req_in() (模块内部) → pe_req_out_port_ (框架外发)
     *   PE 响应: pe_resp_in_port_ (框架接收) → pe_resp_out() (模块内部)
     *   Net 请求: net_req_out() (模块内部) → net_req_out_port_ (发往 Router)
     *   Net 响应: net_resp_in_port_ (从 Router 来) → net_req_in() (模块内部接收)
     */
    void tick() override {
        // === PE 侧响应输出：模块产生响应 → 发回 Core/Cache ===
        if (module_->pe_resp_out().valid()) {
            MasterPort* out = pe_resp_out_port_ ? pe_resp_out_port_ : pe_req_out_port_;
            if (out) {
                module_->pe_resp_out().send(out, PKT_RESP);
            }
        }

        // === Network 侧请求输出：NIC 产生 Flits → 发往 Router ===
        if (module_->net_req_out().valid()) {
            if (net_req_out_port_) {
                module_->net_req_out().send(net_req_out_port_, PKT_REQ);
            }
        }

        // === Network 侧响应输入：从 Router 回来的 Flits → 模块内部 ===
        // 由 process_request_input 处理，非 tick 驱动
    }

    /**
     * @brief 处理 PE 侧请求输入（从 Core 来的 CacheReq）
     */
    void process_request_input(Packet* pkt) override {
        process_pe_request_input(pkt);
    }

    void process_pe_request_input(Packet* pkt) {
        if (!pkt || !pkt->payload) return;
        auto& req_in = module_->pe_req_in();
        if (!req_in.valid()) {
            req_in.process(pkt);
        }
    }

    /**
     * @brief 处理 Network 侧响应输入（从 Router 回来的 NoCResp）
     */
    void process_net_response_input(Packet* pkt) {
        if (!pkt || !pkt->payload) return;
        auto& resp_in = module_->net_resp_in();
        if (!resp_in.valid()) {
            resp_in.process(pkt);
        }
    }

    /**
     * @brief 兼容接口（路由到 PE 处理）
     */
    Packet* process_response_output() override {
        // 响应输出由 tick() 直接处理
        return nullptr;
    }

    /**
     * @brief 端口数量标识（2 个逻辑端口组）
     */
    static constexpr unsigned logical_port_groups() { return 2; }

    ModuleT* module() const { return module_; }
};

} // namespace cpptlm

#endif // FRAMEWORK_DUAL_PORT_STREAM_ADAPTER_HH
