// test/test_sm_reg_file_unit.cc
// Test: RegFileUnit + get_register_value + is_instruction_completed 真值端到端 (per plan Task 1.4)
//
// 验证 StreamingMultiprocessorTLM 经 IComputeDevice 接口:
//   - set_scalar_reg(7, 0xCAFE) + get_register_value(0, 0, 7, &val7) == 0xCAFE
//   - set_scalar_reg(8, 0xBABE) + get_register_value(0, 0, 8, &val8) == 0xBABE
//   - set_instr_descriptor_buf + exe_once + is_instruction_completed(instr_id) == true
//
// 关键路径:
//   - SM-owns-state: RegFileUnit (interim: scalar_regs_) 持寄存器真值 (per architecture/15 §15.5.6)
//   - get_register_value 从 scalar_regs_ 读 (Task 1.4 真值, Task 1.3 已有初步实现)
//   - is_instruction_completed 从 completed_instr_ids_ 查 (Task 1.4 新增)
//
// 作者 CppTLM Team / 2027-02-10 (Task 18 P1-4 实施)
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("SM-owns-state: get_register_value reads SM's RegFile (round-trip)",
          "[sm-regfile][sm-microarch][task18]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    REQUIRE(sm.initialize(cfg));

    // PTX-EMU 写入初始值 (Task 1.1 set_scalar_reg 真值)
    sm.set_scalar_reg(7, 0xCAFE);
    sm.set_scalar_reg(8, 0xBABE);

    // PTX-EMU 端 read back (per IComputeDevice::get_register_value 协议)
    uint64_t val7 = 0, val8 = 0;
    REQUIRE(sm.get_register_value(0, 0, 7, &val7));
    REQUIRE(sm.get_register_value(0, 0, 8, &val8));
    REQUIRE(val7 == 0xCAFE);
    REQUIRE(val8 == 0xBABE);
}

TEST_CASE("is_instruction_completed returns true after instr_id consumed",
          "[sm-regfile][sm-microarch][task18]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    sm.initialize(cfg);

    InstrDescriptor desc{};
    desc.instr_id = 42;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.dst_regs[0] = 5;
    desc.num_src = 2;
    desc.num_dst = 1;
    sm.set_scalar_reg(1, 100);
    sm.set_scalar_reg(2, 200);
    sm.set_instr_descriptor_buf(&desc, 1);

    for (int i = 0; i < 4; ++i) sm.exe_once();

    REQUIRE(sm.is_instruction_completed(42));
}
