// include/tlm/link_tlm.hh
// LinkTLM: 物理链路模块 - 建模两个 Router 之间的链路延迟和 Credit 返回
// 功能描述：物理链路延迟建模和 Credit-based Flow Control
// 作者 CppTLM Team / 日期 2026-04-27
#ifndef TLM_LINK_TLM_HH
#define TLM_LINK_TLM_HH

#include "bundles/noc_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <queue>

namespace tlm {

    /**
     * @brief LinkTLM - 物理链路模块
     *
     * 建模两个 Router 之间的物理链路延迟和 Credit 返回：
     * - 链路延迟（可配置，默认 1 周期）
     * - Credit 返回传递（延迟后返回上游）
     *
     * 继承关系：
     *   SimObject
     *   └── ChStreamModuleBase
     *       └── LinkTLM
     *
     * 端口访问器（单端口模块）：
     *   req_in()  -> InputStreamAdapter<NoCFlitBundle>&   // 接收 flit
     *   resp_out() -> OutputStreamAdapter<NoCFlitBundle>&  // 发送 flit
     */
    class LinkTLM : public ChStreamModuleBase {
    public:
        static constexpr unsigned DEFAULT_LATENCY = 1;

        /**
         * @brief 构造函数
         * @param name 模块名称
         * @param eq 事件队列
         * @param latency 链路延迟周期数
         */
        LinkTLM(const std::string& name, EventQueue* eq, unsigned latency = DEFAULT_LATENCY);

        ~LinkTLM() override = default;

        std::string get_module_type() const override {
            return "LinkTLM";
        }

        // ========== 配置 ==========
        void set_latency(unsigned latency) {
            latency_ = latency;
        }
        unsigned latency() const {
            return latency_;
        }

        // ========== ChStreamModuleBase 接口 ==========
        void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
            adapter_ = adapter;
        }

        // ========== 周期精确仿真 ==========
        void tick() override;

        // ========== 端口访问器 ==========
        cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>& req_in() {
            return req_in_;
        }
        cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>& resp_out() {
            return resp_out_;
        }
        // P0-5b: 被动模块,无 req 输出/resp 输入。返回静态 dummy 供 StreamAdapter 统一调用。
        cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>& req_out() {
            static cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle> dummy;
            return dummy;
        }
        cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>& resp_in() {
            static cpptlm::InputStreamAdapter<bundles::NoCFlitBundle> dummy;
            return dummy;
        }

        // ========== Credit 返回接口（供上游 Router 调用） ==========
        void receive_credit(unsigned port, unsigned vc);

        // ========== 统计 ==========
        struct LinkStats {
            uint64_t flits_forwarded = 0;
            uint64_t credits_returned = 0;
            uint64_t cycles_active = 0;
        };
        LinkStats& stats() {
            return stats_;
        }
        const LinkStats& stats() const {
            return stats_;
        }

    private:
        // ========== 延迟队列元素 ==========
        struct DelayedFlit {
            bundles::NoCFlitBundle flit;
            unsigned remaining_cycles;
            unsigned src_port; // 源端口（用于 credit 返回）
        };

        // ========== Credit 返回队列元素 ==========
        struct CreditReturn {
            unsigned port;
            unsigned vc;
            unsigned remaining_cycles;
        };

        // ========== 端口适配器 ==========
        cpptlm::InputStreamAdapter<bundles::NoCFlitBundle> req_in_;
        cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle> resp_out_;
        cpptlm::StreamAdapterBase* adapter_ = nullptr;

        // ========== 配置 ==========
        unsigned latency_ = DEFAULT_LATENCY;

        // ========== 延迟队列 ==========
        std::queue<DelayedFlit> delay_queue_;

        // ========== Credit 返回队列 ==========
        std::queue<CreditReturn> credit_queue_;

        // ========== 统计 ==========
        LinkStats stats_;
    };

} // namespace tlm

#endif // TLM_LINK_TLM_HH