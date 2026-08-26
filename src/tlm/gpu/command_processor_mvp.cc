// command_processor_mvp.cc
// CommandProcessor MVP 骨架实现 - 5-state FSM no-op 模式
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/command_processor_mvp.hh"

namespace tlm::gpu {

    CommandProcessor::CommandProcessor() = default;
    CommandProcessor::~CommandProcessor() = default;

    void CommandProcessor::wake() {
        ++wake_count_;
        // 仅在 IDLE 时触发 IDLE → FETCH 转换
        if (state_ == State::IDLE) {
            transition_to(State::FETCH);
        }
    }

    void CommandProcessor::tick() {
        ++tick_count_;
        // s2 skeleton: no-op 状态机 (cycle through 5 states)
        // s3 will replace with actual fetch/decode/dispatch logic
        switch (state_) {
            case State::IDLE:
                // stay IDLE until wake() called
                break;
            case State::FETCH:
                transition_to(State::DECODE);
                break;
            case State::DECODE:
                // s2 skeleton: no-op (no decoder injected yet)
                // s3 will call decoder_->parse_method() here
                transition_to(State::DISPATCH);
                break;
            case State::DISPATCH:
                // s2 skeleton: no-op dispatch (no TMU/SQ injected)
                // s3 will call tmu_.submit(Pm4MethodDispatch) here
                transition_to(State::COMPLETE);
                break;
            case State::COMPLETE:
                transition_to(State::IDLE);
                break;
        }
    }

    void CommandProcessor::set_decoder(std::unique_ptr<Pm4DecoderInterface> decoder) {
        decoder_ = std::move(decoder);
        // s3 will use decoder_ in tick() DECODE transition
    }

    void CommandProcessor::transition_to(State new_state) {
        state_ = new_state;
        ++state_transitions_;
    }

} // namespace tlm::gpu
