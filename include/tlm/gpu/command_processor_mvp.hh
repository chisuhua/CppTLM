// command_processor_mvp.hh
// CommandProcessor MVP 骨架 - 5-state FSM (no-op 模式)
// Author: CppTLM Team
// Date: 2026-08-26
//
// s2 阶段: 骨架编译通过即可 (tick() 默认 no-op 状态机)
// s3 阶段: 填充 GPU VA fetch + parse_method + DECODE 实际逻辑
//
// 5-state FSM: IDLE → FETCH → DECODE → DISPATCH → COMPLETE → IDLE
#ifndef CPPTLM_COMMAND_PROCESSOR_MVP_H
#define CPPTLM_COMMAND_PROCESSOR_MVP_H

#include "tlm/gpu/pm4_decoder_mvp.hh"
#include <cstdint>
#include <memory>

namespace tlm::gpu {

    class CommandProcessor {
    public:
        // 5-state FSM (per Phase F-H.3)
        enum class State {
            IDLE,
            FETCH,
            DECODE,
            DISPATCH,
            COMPLETE
        };

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
        // s2 skeleton: 默认 no-op 状态机 (state cycles through 5 then back to IDLE)
        // s3 fill: 实际 fetch + decode + dispatch 逻辑
        void tick();

        // set_decoder(): s3 注入 Pm4DecoderInterface 实现
        // s2 阶段: 即使没注入 decoder, tick() 也不崩溃 (no-op)
        void set_decoder(std::unique_ptr<Pm4DecoderInterface> decoder);

        // 计数器 (诊断/测试)
        uint64_t state_transitions() const { return state_transitions_; }
        uint64_t wake_count() const { return wake_count_; }
        uint64_t tick_count() const { return tick_count_; }

    private:
        State state_ = State::IDLE;
        std::unique_ptr<Pm4DecoderInterface> decoder_;

        uint64_t state_transitions_ = 0;
        uint64_t wake_count_ = 0;
        uint64_t tick_count_ = 0;

        void transition_to(State new_state);
    };

} // namespace tlm::gpu

#endif // CPPTLM_COMMAND_PROCESSOR_MVP_H
