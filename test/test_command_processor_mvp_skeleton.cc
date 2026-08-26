// test_command_processor_mvp_skeleton.cc
// CommandProcessor 骨架单测: 5 state 转换 + wake + set_decoder
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/command_processor_mvp.hh"

TEST_CASE("CommandProcessor: starts in IDLE state", "[command-processor][mvp][skeleton]") {
    tlm::gpu::CommandProcessor cp;
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::IDLE);
    REQUIRE(cp.state_transitions() == 0);
    REQUIRE(cp.tick_count() == 0);
    REQUIRE(cp.wake_count() == 0);
}

TEST_CASE("CommandProcessor: wake() transitions IDLE → FETCH", "[command-processor][mvp][skeleton]") {
    tlm::gpu::CommandProcessor cp;
    cp.wake();
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::FETCH);
    REQUIRE(cp.state_transitions() == 1);
    REQUIRE(cp.wake_count() == 1);
}

TEST_CASE("CommandProcessor: tick() advances FETCH → DECODE → DISPATCH → COMPLETE → IDLE", "[command-processor][mvp][skeleton]") {
    tlm::gpu::CommandProcessor cp;
    cp.wake();  // IDLE → FETCH
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::FETCH);

    cp.tick();  // → DECODE
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::DECODE);

    cp.tick();  // → DISPATCH
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::DISPATCH);

    cp.tick();  // → COMPLETE
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::COMPLETE);

    cp.tick();  // → IDLE
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::IDLE);

    REQUIRE(cp.state_transitions() == 5);  // wake + 4 ticks
    REQUIRE(cp.tick_count() == 4);
}

TEST_CASE("CommandProcessor: tick() in IDLE stays IDLE", "[command-processor][mvp][skeleton]") {
    tlm::gpu::CommandProcessor cp;
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::IDLE);

    cp.tick();
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::IDLE);

    cp.tick();
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::IDLE);
    REQUIRE(cp.tick_count() == 2);
}

TEST_CASE("CommandProcessor: wake() is no-op when not in IDLE", "[command-processor][mvp][skeleton]") {
    tlm::gpu::CommandProcessor cp;
    cp.wake();  // → FETCH
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::FETCH);

    cp.wake();  // no-op (not IDLE)
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::FETCH);
    REQUIRE(cp.wake_count() == 2);
    REQUIRE(cp.state_transitions() == 1);  // 只有第一次 wake 触发转换
}

TEST_CASE("CommandProcessor: set_decoder holds decoder pointer (no crash)", "[command-processor][mvp][skeleton]") {
    class StubDecoder : public tlm::gpu::Pm4DecoderInterface {
    public:
        tlm::gpu::Pm4MethodDispatch parse_method(uint32_t, const uint32_t*, uint32_t) override {
            return {};
        }
    };

    tlm::gpu::CommandProcessor cp;
    cp.set_decoder(std::make_unique<StubDecoder>());
    // 即使有 decoder, s2 skeleton 仍走 no-op 路径
    cp.wake();
    cp.tick();
    cp.tick();
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::DISPATCH);
}

TEST_CASE("CommandProcessor: full cycle IDLE → ... → IDLE → ...", "[command-processor][mvp][skeleton]") {
    tlm::gpu::CommandProcessor cp;

    // Cycle 1
    cp.wake();
    cp.tick(); cp.tick(); cp.tick(); cp.tick();
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::IDLE);

    // Cycle 2
    cp.wake();
    cp.tick(); cp.tick(); cp.tick(); cp.tick();
    REQUIRE(cp.state() == tlm::gpu::CommandProcessor::State::IDLE);

    REQUIRE(cp.state_transitions() == 10);  // 2 cycles × 5 transitions
    REQUIRE(cp.wake_count() == 2);
}
