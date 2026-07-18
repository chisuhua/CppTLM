// include/tlm/gpu/pipeline_tlm.hh
// PipelineTLM: IPipelineLatencyProvider 实现 — D1-Full P1 Phase 1 占位
// 功能: Fractional cycle latency 查询, P1 返回 1.0 (占位), Phase 4 对齐 gpgpu-sim G-D5
// 作者 CppTLM Team / 日期 2026-07-18
// 参考:
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/design.md §2.2
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §1.2
//   - include/cudart/pipeline_interface.h (vendor from PTX-EMU 9e7361b9)
#ifndef TLM_GPU_PIPELINE_TLM_HH
#define TLM_GPU_PIPELINE_TLM_HH

#include "cudart/pipeline_interface.h"  // IPipelineLatencyProvider + PipelineId enum (vendor)

#include <string>

namespace tlm {

/// PipelineTLM: IPipelineLatencyProvider 的 CppTLM 端实现（直接继承, 无 Internal 层）
///
/// ⚠️ PHASE 1 PLACEHOLDER — DO NOT USE AS CANONICAL VALUE
/// P1 占位返回 1.0, Phase 4 替换为 gpgpu-sim 精确值 (G-D5 ±15%)
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
