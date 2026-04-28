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
#include "core/chstream_port.hh"
#include "core/packet_pool.hh"
#include "bundles/noc_bundles_tlm.hh"
#include <cstddef>
#include <array>
#include <queue>

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

    // 待发送的 Credit 队列（支持链路延迟建模）
    struct PendingCredit {
        unsigned port;
        unsigned vc;
        unsigned remaining_cycles;
    };
    std::queue<PendingCredit> pending_credits_;

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
            // ========== 处理 flit 发送 (forward direction) ==========
            if (module_->resp_out()[i].valid()) {
                if (req_out_port_[i]) {
                    auto& flit = module_->resp_out()[i].data();
                    PacketType pkt_type = (flit.flit_category.read() == bundles::NoCFlitBundle::CATEGORY_REQUEST)
                        ? PKT_REQ : PKT_RESP;
                    module_->resp_out()[i].send(req_out_port_[i], pkt_type);
                }
            }

            // ========== 处理 Credit 接收 (reverse direction) ==========
            // 检查 resp_in_port_[i] 是否有 Credit 信号
            if (resp_in_port_[i]) {
                // 使用 ChStreamInitiatorPort 的 drainResponse() 获取 Packet
                auto* initiator = dynamic_cast<cpptlm::ChStreamInitiatorPort*>(resp_in_port_[i]);
                if (initiator) {
                    while (initiator->hasResponse()) {
                        Packet* pkt = initiator->drainResponse();
                        if (pkt && pkt->isCredit()) {
                            // 解析 Credit 信号
                            unsigned vc = pkt->vc_id;
                            module_->receive_credit(i, vc);
                            DPRINTF(MODULE, "[BidirectionalPortAdapter] tick() port=%zu credit vc=%u\n", i, vc);
                        }
                        PacketPool::get().release(pkt);
                    }
                }
            }
        }

        // ========== 处理 Credit 发送延迟队列 ==========
        std::queue<PendingCredit> next_queue;
        while (!pending_credits_.empty()) {
            auto cr = pending_credits_.front();
            pending_credits_.pop();
            cr.remaining_cycles--;
            if (cr.remaining_cycles == 0) {
                // 链路延迟结束，实际发送 Credit
                send_credit_impl(cr.port, cr.vc);
            } else {
                next_queue.push(cr);
            }
        }
        pending_credits_ = std::move(next_queue);
    }

    /**
     * @brief 发送 Credit 信号到指定端口（带 1 周期链路延迟）
     * @param port 目标端口索引
     * @param vc 虚拟通道 ID
     */
    void send_credit(unsigned port, unsigned vc) {
        PendingCredit cr;
        cr.port = port;
        cr.vc = vc;
        cr.remaining_cycles = 1;  // 1 周期链路延迟
        pending_credits_.push(cr);
    }

private:
    /**
     * @brief 实际发送 Credit 信号（延迟到期后调用）
     */
    void send_credit_impl(unsigned port, unsigned vc) {
        if (port >= N || !resp_out_port_[port]) return;

        // 创建 Credit 信号 Bundle
        bundles::NoCFlitBundle credit_bundle;
        credit_bundle.vc_id.write(vc);
        credit_bundle.flit_category.write(bundles::NoCFlitBundle::CATEGORY_CREDIT);
        credit_bundle.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);  // 单 flit

        // 通过 resp_out_port_ 发送（连接到上游路由器的 resp_in）
        Packet* pkt = PacketPool::get().acquire();
        pkt->type = PKT_CREDIT_RETURN;
        pkt->vc_id = vc;

        bundles::serialize_bundle(
            credit_bundle,
            pkt->payload->get_data_ptr(),
            pkt->payload->get_data_length()
        );

        resp_out_port_[port]->send(pkt);
        DPRINTF(MODULE, "[BidirectionalPortAdapter] send_credit_impl port=%u vc=%u\n", port, vc);
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
