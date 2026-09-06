// test/test_sm_scalar_alu_e2e.cc
// Test: ScalarALU ADD 真值端到端 (per plan Task 1.3)
//
// 验证 StreamingMultiprocessorTLM 经 IComputeDevice 接口 (set_instr_descriptor_buf
// + exe_once + get_register_value) 真实计算 ADD 指令: reg 1 + reg 2 → reg 5 = 300
//
// 关键路径:
//   - sm.initialize(cfg)
//   - sm.set_scalar_reg(1, 100), sm.set_scalar_reg(2, 200)  (Task 1.1 interim 真值源)
//   - InstrDescriptor{kScalarALU, kFixed1Cycle, dst=[5], src=[1,2], num_src=2, num_dst=1}
//   - sm.set_instr_descriptor_buf(&desc, 1)  (浅拷贝 internal_buf_)
//   - 推进 4 cycle: sm.exe_once() × 4
//   - sm.get_register_value(0, 0, 5, &val) == 300  (验证寄存器真值)
//
// 作者 CppTLM Team / 2027-02-10 (Task 18 P1-3 实施)
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("ScalarALU ADD: reg 1 + reg 2 → reg 5 (round-trip via IComputeDevice)",
          "[sm-alu][sm-microarch][task18]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    REQUIRE(sm.initialize(cfg)); // Task 1.3 真值
    sm.set_scalar_reg(1, 100);
    sm.set_scalar_reg(2, 200);

    InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1; // 寄存器号 (语义统一 per Oracle P0-4)
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;
    sm.set_instr_descriptor_buf(&desc, 1); // 浅拷贝 internal_buf_

    // 推进 4 cycle (ADD 1 cycle, 留 buffer 写入 + Result 收集)
    for (int i = 0; i < 4; ++i) {
        sm.exe_once();
    }

    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 5, &val));
    REQUIRE(val == 300); // 100 + 200
}
