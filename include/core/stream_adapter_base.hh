// include/core/stream_adapter_base.hh
// StreamAdapter 抽象基类 — 移至 core/ 以打破 core/framework 循环依赖
// 功能描述：定义框架侧 Port ↔ 模块侧 ch_stream 适配器的类型擦除基类
// 作者：CppTLM Team
// 日期：2026-06-02
#ifndef CORE_STREAM_ADAPTER_BASE_HH
#define CORE_STREAM_ADAPTER_BASE_HH

#include "core/master_port.hh"
#include "core/slave_port.hh"
#include "core/packet.hh"

namespace cpptlm {

/**
 * @brief StreamAdapter 基类（类型擦除）
 *
 * 用于在 ModuleFactory 中统一管理不同类型的 StreamAdapter
 * （StandaloneStreamAdapter, MultiPortStreamAdapter 等）。
 *
 * 移至 core/ 的原因：chstream_port.hh 持有 StreamAdapterBase* 指针，
 * 若该基类留在 framework/，则 core→framework 的反向 include 形成循环。
 */
class StreamAdapterBase {
public:
    virtual ~StreamAdapterBase() = default;

    /**
     * @brief 每 tick 调用，处理双向数据流
     */
    virtual void tick() = 0;

    /**
     * @brief 绑定框架侧端口（由 ModuleFactory 调用）
     * @param req_out_port 请求输出 MasterPort
     * @param resp_in_port 响应输入 SlavePort
     * @param resp_out_port 响应输出 MasterPort（可选）
     * @param req_in_port  请求输入 SlavePort（可选）
     */
    virtual void bind_ports(
        MasterPort* req_out_port,
        SlavePort*  resp_in_port,
        MasterPort* resp_out_port = nullptr,
        SlavePort*  req_in_port = nullptr
    ) = 0;

    /**
     * @brief 绑定单个端口对（多端口模块使用，ModuleFactory 按 port_idx 调用）
     *
     * 默认空实现：单端口 StreamAdapter 与 DualPortStreamAdapter 不使用此模式，
     * 仅 MultiPortStreamAdapter / BidirectionalPortAdapter override。
     *
     * @param port_idx 端口索引（0..N-1）
     * @param req_out   请求输出 MasterPort
     * @param resp_in   响应输入 SlavePort
     * @param resp_out  响应输出 MasterPort（可选）
     * @param req_in    请求输入 SlavePort（可选）
     */
    virtual void bind_port_pair(
        unsigned    port_idx,
        MasterPort* req_out,
        SlavePort*  resp_in,
        MasterPort* resp_out = nullptr,
        SlavePort*  req_in = nullptr
    ) {}

    /**
     * @brief 处理请求方向输入（Packet → ch_stream）
     */
    virtual void process_request_input(Packet* pkt) = 0;

    /**
     * @brief 处理请求方向输入（多端口版本,P0-5b fix）
     *
     * 多端口 StreamAdapter 需要知道请求来自哪个端口才能正确路由到
     * 内部 req_in[port_idx] / resp_in[port_idx]。单端口 adapter 使用
     * 1 参数重载（默认 port_idx=0）。
     *
     * @param pkt      Packet
     * @param port_idx 接收端口索引（0..N-1）
     */
    virtual void process_request_input(Packet* pkt, unsigned /*port_idx*/) {
        process_request_input(pkt);  // 默认回退到单端口版本
    }

    /**
     * @brief 处理响应方向输出（ch_stream → Packet）
     * @return 待发送的 Packet（若无返回 nullptr）
     */
    virtual Packet* process_response_output() = 0;
};

} // namespace cpptlm

#endif // CORE_STREAM_ADAPTER_BASE_HH
