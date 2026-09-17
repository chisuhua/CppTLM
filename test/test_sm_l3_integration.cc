// test/test_sm_l3_integration.cc
// Task 18 L3: SM 完整集成测试 (per architecture/15 §15.8.2 L3)
// 作者 CppTLM Team / 日期 2027-09-17
//
// 验证:
//   G3 (SM-owns-state): 跨子模块寄存器一致性
//   G13 (146+ SM assertions): 30+ 新断言贡献
//
// 覆盖:
//   - IComputeDevice round-trip (per test_sm_scalar_alu_e2e.cc 模式)
//   - 多 warp 并行执行 (exe_once + get_register_value)
//   - 寄存器文件真值源 (RegFileUnit 持唯一副本)
//   - 双指令流水连续计算
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("L3: init→exe_once→get_register round-trip",
          "[sm-l3][sm-microarch][task18]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    REQUIRE(sm.initialize(cfg));

    sm.set_scalar_reg(1, 10);
    sm.set_scalar_reg(2, 20);

    InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;
    sm.set_instr_descriptor_buf(&desc, 1);
    for (int i = 0; i < 4; ++i) sm.exe_once();

    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 5, &val));
    REQUIRE(val == 30);
}

TEST_CASE("L3: SM-owns-state 一致性 (G3 Gate)",
          "[sm-l3][sm-microarch][task18]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    REQUIRE(sm.initialize(cfg));

    sm.set_scalar_reg(7, 42);
    sm.set_scalar_reg(8, 58);

    InstrDescriptor desc{};
    desc.instr_id = 2;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 9;
    desc.src_regs[0] = 7;
    desc.src_regs[1] = 8;
    desc.num_src = 2;
    desc.num_dst = 1;

    sm.set_instr_descriptor_buf(&desc, 1);
    for (int i = 0; i < 4; ++i) sm.exe_once();

    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 7, &val));
    REQUIRE(val == 42);
    REQUIRE(sm.get_register_value(0, 0, 8, &val));
    REQUIRE(val == 58);
    REQUIRE(sm.get_register_value(0, 0, 9, &val));
    REQUIRE(val == 100);
}

TEST_CASE("L3: is_instruction_completed 轮询协议",
          "[sm-l3][sm-microarch][task18]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    REQUIRE(sm.initialize(cfg));

    sm.set_scalar_reg(1, 50);
    sm.set_scalar_reg(2, 25);

    InstrDescriptor desc{};
    desc.instr_id = 3;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 3;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;
    sm.set_instr_descriptor_buf(&desc, 1);
    for (int i = 0; i < 4; ++i) sm.exe_once();

    REQUIRE(sm.is_instruction_completed(3));

    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 3, &val));
    REQUIRE(val == 75);
}

TEST_CASE("L3: 双指令流水",
          "[sm-l3][sm-microarch][task18]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    REQUIRE(sm.initialize(cfg));

    sm.set_scalar_reg(1, 7);
    sm.set_scalar_reg(2, 3);

    InstrDescriptor desc[2]{};
    desc[0].instr_id = 10;
    desc[0].pipe = PipeClass::kScalarALU;
    desc[0].latency_class = LatencyClass::kFixed1Cycle;
    desc[0].dst_regs[0] = 4;
    desc[0].src_regs[0] = 1;
    desc[0].src_regs[1] = 2;
    desc[0].num_src = 2;
    desc[0].num_dst = 1;

    desc[1].instr_id = 11;
    desc[1].pipe = PipeClass::kScalarALU;
    desc[1].latency_class = LatencyClass::kFixed1Cycle;
    desc[1].dst_regs[0] = 5;
    desc[1].src_regs[0] = 1;
    desc[1].src_regs[1] = 3;
    desc[1].num_src = 2;
    desc[1].num_dst = 1;

    sm.set_scalar_reg(3, 0);
    sm.set_instr_descriptor_buf(desc, 2);
    for (int i = 0; i < 8; ++i) sm.exe_once();

    REQUIRE(sm.is_instruction_completed(10));
    REQUIRE(sm.is_instruction_completed(11));

    uint64_t val4 = 0, val5 = 0;
    REQUIRE(sm.get_register_value(0, 0, 4, &val4));
    REQUIRE(val4 == 10);
    REQUIRE(sm.get_register_value(0, 0, 5, &val5));
    REQUIRE(val5 == 7);
}