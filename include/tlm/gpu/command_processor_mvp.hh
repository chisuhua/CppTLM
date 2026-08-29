// command_processor_mvp.hh
// CommandProcessor MVP 5-state FSM + degraded latch(退避窗口门控 deferred to s4) (per design §3 + §4.4)
// Author: CppTLM Team
// Date: 2026-08-26 (s3 commit 2026-08-28 扩展 4 装配方法 + 常量 + getter)
//
// s3 W5 T-s3-2: 填充 GPU VA fetch + parse_method + DECODE 实际逻辑 + 退避策略
//
// 5-state FSM: IDLE → FETCH → DECODE → DISPATCH → COMPLETE → IDLE
#ifndef CPPTLM_COMMAND_PROCESSOR_MVP_H
#define CPPTLM_COMMAND_PROCESSOR_MVP_H

#include "tlm/gpu/pm4_decoder_mvp.hh"
#include "tlm/gpu/tmu_types_mvp.hh"  // per Metis C3: std::function DispatchFn 需完整类型
#include <cstdint>
#include <functional>
#include <memory>

namespace tlm::gpu {

    class CommandProcessor {
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

        CommandProcessor();
        ~CommandProcessor();

        CommandProcessor(const CommandProcessor&) = delete;
        CommandProcessor& operator=(const CommandProcessor&) = delete;

        // 状态查询
        State state() const { return state_; }

        // wake(): IDLE → FETCH (供 DGpuBoardTLM 在 Doorbell ring 后调)
        // 仅当 state == IDLE 时生效;其他状态 no-op
        void wake();

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

} // namespace tlm::gpu

#endif // CPPTLM_COMMAND_PROCESSOR_MVP_H
