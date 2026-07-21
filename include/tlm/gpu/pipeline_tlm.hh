// include/tlm/gpu/pipeline_tlm.hh
// PipelineTLM: IPipelineLatencyProvider 实现 — S4 Phase 2a 延迟表
// 功能: Fractional cycle latency 查询, 基于 A100 指令延迟模型
// 作者 CppTLM Team / 日期 2026-07-18 (P1 占位) + 2026-07-21 (S4 Phase 2a)
// 参考:
//   - PTX-EMU docs/dev-process/cpptlm-co-simulation-plan.md §2a.1/2b.1
//   - A100 whitepaper §5.4.1 + GPGPU-Sim gpu_config.h
//   - include/cudart/pipeline_interface.h (vendor from PTX-EMU 9e7361b9)
#ifndef TLM_GPU_PIPELINE_TLM_HH
#define TLM_GPU_PIPELINE_TLM_HH

#include "cudart/pipeline_interface.h"  // IPipelineLatencyProvider + PipelineId enum (vendor)

#include <string>

namespace tlm {

/// PipelineTLM: IPipelineLatencyProvider 的 CppTLM 端实现（直接继承, 无 Internal 层）
///
/// S4 Phase 2a: 实现基于 A100 的真实指令延迟查表
///   - P0_INT_FP32: IADD/MOV=1.0, FFMA=4.22, IMAD=2.0
///   - P3_LSU: GLOBAL_LD=200, SHARED=1.0, LOCAL=5.0
///   - P4_TC: 返回 0 (委托给 TensorCoreTLM)
///   - get_fractional_cycles: 指令名匹配 (case-insensitive)
///   - get_fractional_cycles_by_type: 管线平均值 (无 PTX statement_type 枚举依赖)
class PipelineTLM : public IPipelineLatencyProvider {
public:
    PipelineTLM();
    ~PipelineTLM() override = default;

    double get_fractional_cycles(const std::string& instruction,
                                 PipelineId pipe_id) const override;
    double get_fractional_cycles_by_type(int statement_type,
                                         PipelineId pipe_id) const override;

    /// 占位标记（Phase 4 替换真实值后返回 false）
    bool is_placeholder() const { return is_placeholder_; }

private:
    bool is_placeholder_ = true;
};

}  // namespace tlm

#endif  // TLM_GPU_PIPELINE_TLM_HH
