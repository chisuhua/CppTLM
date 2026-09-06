// include/tlm/gpu/sm/scalar_alu.hh
// ScalarALU 独立真值类 (per plan Task 1.3)
//
// 功能: SM 顶层调用链调用 ScalarALU 真值计算指令 (per architecture/15 §15.5.6 + ADR-SOC-16 §2.3)
//   - ADD (kFixed1Cycle): dst = src[0] + src[1]
//   - IMAD (kFixed4Cycle): dst = src[0] * src[1] (简化, 无第三操作数)
//
// 架构定位:
//   - cpptlm::gpu::ScalarALU (独立, 不继承 ChStreamModuleBase)
//   - 构造函数 (tlm::StreamingMultiprocessorTLM* parent) - 父指针访问 scalar_regs_
//   - execute(InstrDescriptor&) - 真值入口, 返回 cycles
//   - 不同于 SM .hh 内 tlm::sm::ScalarALU stub (12 子模块 inline stub 保留, 不冲突)
//
// 跨仓契约 (per HSK-9 §3):
//   - PTX-EMU 端通过 set_instr_descriptor_buf() 注入 InstrDescriptor
//   - SM 顶层浅拷贝到 internal_buf_
//   - exe_once() 调用 ScalarALU::execute() 处理 internal_buf_ 中 kScalarALU 指令
//
// 作者 CppTLM Team / 日期 2027-02-10 (Task 18 P1-3 实施)
#ifndef TLM_GPU_SM_SCALAR_ALU_HH
#define TLM_GPU_SM_SCALAR_ALU_HH

#include "tlm/gpu/instruction_descriptor.hh"
#include <cstdint>

namespace tlm {
    class StreamingMultiprocessorTLM; // 前向声明 (SM 实际 namespace)
}

namespace cpptlm {
    namespace gpu {

        class ScalarALU {
        public:
            explicit ScalarALU(::tlm::StreamingMultiprocessorTLM* parent) : parent_(parent) {
            }

            // execute: 真值计算, 返回 cycles 占用 (1 / 4 / 0=不支持)
            uint32_t execute(InstrDescriptor& desc);

        private:
            ::tlm::StreamingMultiprocessorTLM* parent_;
        };

    } // namespace gpu
} // namespace cpptlm

#endif // TLM_GPU_SM_SCALAR_ALU_HH
