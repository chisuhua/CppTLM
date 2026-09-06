// src/tlm/gpu/sm/scalar_alu.cc
// ScalarALU 真值实现 (per plan Task 1.3)
//
// 真值路径:
//   - kFixed1Cycle (ADD):  dst[0] = src[0] + src[1],  返回 1
//   - kFixed4Cycle (IMAD): dst[0] = src[0] * src[1],  返回 4 (简化, 无第三操作数 + 0)
//   - 其他: 返回 0 (不支持)
//
// 写回路径:
//   - parent_->set_scalar_reg(dst[0], r)  (Task 1.1 interim 真值源 scalar_regs_)
//   - desc.result_value[0] = r  (写入 SM 内部 desc 副本, 供 PTX-EMU 后续 pull)
//
// 作者 CppTLM Team / 日期 2027-02-10 (Task 18 P1-3 实施)
#include "tlm/gpu/sm/scalar_alu.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

namespace cpptlm {
    namespace gpu {

        uint32_t ScalarALU::execute(InstrDescriptor& desc) {
            switch (desc.latency_class) {
            case LatencyClass::kFixed1Cycle:
                // ADD: dst = src[0] + src[1]
                if (desc.num_dst >= 1 && desc.num_src >= 2) {
                    uint64_t a = parent_->get_scalar_reg(desc.src_regs[0]);
                    uint64_t b = parent_->get_scalar_reg(desc.src_regs[1]);
                    uint64_t r = a + b;
                    parent_->set_scalar_reg(desc.dst_regs[0], r);
                    desc.result_value[0] = r;
                    return 1;
                }
                break;
            case LatencyClass::kFixed4Cycle:
                // IMAD: dst = src[0] * src[1] (简化版)
                if (desc.num_dst >= 1 && desc.num_src >= 2) {
                    uint64_t a = parent_->get_scalar_reg(desc.src_regs[0]);
                    uint64_t b = parent_->get_scalar_reg(desc.src_regs[1]);
                    uint64_t r = a * b;
                    parent_->set_scalar_reg(desc.dst_regs[0], r);
                    desc.result_value[0] = r;
                    return 4;
                }
                break;
            default:
                return 0;
            }
            return 0;
        }

    } // namespace gpu
} // namespace cpptlm
