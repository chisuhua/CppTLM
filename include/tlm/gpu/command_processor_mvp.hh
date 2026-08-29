// command_processor_mvp.hh
// CommandProcessorTLM - ChStream 组件(CP 5-state FSM + 4 端口)
// Per board-soc-split design §3.5 ports table + ADR-SOC-07 D1
// Author: CppTLM Team · Date: 2026-08-31 (updated 2026-08-31: promote to ChStreamModuleBase)
//
// s3 W5 T-s3-2: 填充 GPU VA fetch + parse_method + DECODE 实际逻辑 + 退避策略
//
// 5-state FSM: IDLE → FETCH → DECODE → DISPATCH → COMPLETE → IDLE
// 4 端口化: cmd_in[0] / fetch_out[1] ⭐新增 / dma_req[2] / dispatch[3]
#ifndef CPPTLM_COMMAND_PROCESSOR_MVP_H
#define CPPTLM_COMMAND_PROCESSOR_MVP_H

#include "tlm/gpu/pm4_decoder_mvp.hh"
#include "tlm/gpu/tmu_types_mvp.hh"  // per Metis C3: std::function DispatchFn 需完整类型
#include "core/chstream_module.hh"     // ChStreamModuleBase
#include "framework/stream_adapter.hh"  // cpptlm::StreamAdapterBase
#include "bundles/pcie_bundles_tlm.hh"   // PcieTlpBundle (cmd_in)
#include "bundles/dma_bundles_tlm.hh"    // DmaDescriptorBundle (dma_req) - 复用 sdma-engine
#include "bundles/cache_bundles_tlm.hh"  // CacheReqBundle (fetch_out)
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace tlm::gpu {

// Pm4DispatchBundle stub(deferred T-bs-2e 创建 dgpu_bundles_tlm 时定义)
// POD 化 Pm4MethodDispatch(per design Q3 裁决)
struct Pm4DispatchBundle {
    uint32_t method_addr = 0;
    uint32_t subchannel_id = 0;
    uint32_t data_count = 0;
    // 完整字段 deferred T-bs-2e
};

    class CommandProcessorTLM : public ChStreamModuleBase {
    public:
        // 5-state FSM + degraded latch(退避窗口门控 deferred to s4)
        enum class State {
            IDLE,
            FETCH,
            DECODE,
            DISPATCH,
            COMPLETE
        };

        // 退避常量 (per Oracle P2-2 修复 2026-08-28)
        static constexpr uint64_t MIN_BACKOFF_CYCLES = 8;
        static constexpr uint64_t CP_BACKOFF_DEGRADED_THRESHOLD = 3;

        // ── 装配回调签名 (per design §3.2) ──
        // FETCH: CP 调 reader(gpu_va, out_buf, sizeof(gpu_gpfifo_entry))
        // 返回 0 = 成功,负值 = errno (与 DGpuBoardTLM::read_vram 对齐)
        using VramReadFn = std::function<int32_t(uint64_t gpu_va, void* out, size_t size)>;

        // DISPATCH: CP 把 Pm4MethodDispatch 适配为 TmuDispatchRecord 后调 fn(record)
        // 返回 TmuSubmitResult,CP 据此推进或退避(per §4.4)
        using DispatchFn = std::function<TmuSubmitResult(const TmuDispatchRecord&)>;

        // ChStream 4 端口(per design §3.5):
        // cmd_in[0]      - PcieTlpBundle ingress    (doorbell → wake())
        // fetch_out[1]   - CacheReqBundle egress    (FETCH 态 mem_read_vram)
        // dma_req[2]     - DmaDescriptorBundle egress (FSM 中大块搬运)
        // dispatch[3]    - Pm4DispatchBundle egress  (→ TMU)

        explicit CommandProcessorTLM(const std::string& n = "", EventQueue* eq = nullptr);
        ~CommandProcessorTLM() = default;

        CommandProcessorTLM(const CommandProcessorTLM&) = delete;
        CommandProcessorTLM& operator=(const CommandProcessorTLM&) = delete;

        std::string get_object_type() const { return "CommandProcessorTLM"; }

        // ChStream 适配器注入(no-op for deferred T-bs-5)
        void set_stream_adapter(cpptlm::StreamAdapterBase*) override {}
        void set_stream_adapter(cpptlm::StreamAdapterBase*[]) override {}

        // 状态查询
        State state() const { return state_; }

        // wake(): IDLE → FETCH (供 DGpuBoardTLM 在 Doorbell ring 后调)
        // 仅当 state == IDLE 时生效;其他状态 no-op
        void wake();

        // ── ChStream 4 端口声明(per design §3.5) ──
        // 端口索引与方向:
        //   cmd_in[0]   - PcieTlpBundle ingress    (doorbell → wake())
        //   fetch_out[1]- CacheReqBundle egress    (FETCH 态 mem_read_vram)
        //   dma_req[2]  - DmaDescriptorBundle egress (FSM 中大块搬运)
        //   dispatch[3] - Pm4DispatchBundle egress  (→ TMU)
        // 注: MultiPortStreamAdapter 4 端口注册 deferred T-bs-5(per W4/W5 同等妥协)
        //     当前仅声明端口, 适配器注册 deferred

        // tick(): 推进 FSM
        // s3: 实际 fetch + decode + dispatch 逻辑(per §3.1)
        void tick();

        // set_decoder(): 注入 Pm4DecoderInterface 实现
        void set_decoder(std::unique_ptr<Pm4DecoderInterface> decoder);

        // s3 新增装配方法(per design §3.2):
        void set_vram_reader(VramReadFn reader);
        void set_dispatch_target(DispatchFn fn);

        // on_backpressure / on_submit_queue_rejected: 可选外部通知接口(per Oracle P1-a
        // 修复 2026-08-28)。CP 内部已自动从 dispatch_target 返回值退避,默认空实现;
        // DGpuBoardTLM §3.3 装配 4 行不调用它们。测试断言通过 getter 直接读取状态。
        void on_backpressure(uint64_t cycles);
        void on_submit_queue_rejected(uint64_t cycles);

        // 计数器 (诊断/测试断言, per design §3.2.1)
        uint64_t state_transitions() const { return state_transitions_; }
        uint64_t wake_count() const { return wake_count_; }
        uint64_t tick_count() const { return tick_count_; }
        uint64_t cp_backoff_count() const { return cp_backoff_count_; }
        uint64_t backoff_cycles_remaining() const { return backoff_cycles_remaining_; }
        bool degraded() const { return degraded_; }

    private:
        State state_ = State::IDLE;
        std::unique_ptr<Pm4DecoderInterface> decoder_;
        VramReadFn vram_read_cb_;
        DispatchFn dispatch_target_;

        uint64_t state_transitions_ = 0;
        uint64_t wake_count_ = 0;
        uint64_t tick_count_ = 0;
        uint64_t cp_backoff_count_ = 0;
        uint64_t backoff_cycles_remaining_ = 0;
        bool degraded_ = false;
        uint64_t next_task_id_ = 1;

        void transition_to(State new_state);
        void enter_backoff(TmuSubmitResult dispatch_result);
    };

// 向后兼容别名(s3 测试桩用旧名)
using CommandProcessor = CommandProcessorTLM;

} // namespace tlm::gpu

#endif // CPPTLM_COMMAND_PROCESSOR_MVP_H
