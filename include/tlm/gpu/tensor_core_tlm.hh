// include/tlm/gpu/tensor_core_tlm.hh
// TensorCoreTLM: ITensorCoreTiming 实现 — D1-Full P1 Phase 1 占位
// 功能: TensorCore latency/throughput 查询, P1 返回 1 (占位), Phase 4 对齐 gpgpu-sim G-D5
// 作者 CppTLM Team / 日期 2026-07-18
// 参考:
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/design.md §2.3
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §1.3
//   - include/cudart/tensor_core_interface.h (vendor from PTX-EMU 463038e0)
#ifndef TLM_GPU_TENSOR_CORE_TLM_HH
#define TLM_GPU_TENSOR_CORE_TLM_HH

#include "cudart/tensor_core_interface.h"  // ITensorCoreTiming + TcPrecision enum (vendor)

#include <cstdint>

namespace tlm {

/// TensorCoreTLM: ITensorCoreTiming 的 CppTLM 端实现（直接继承, 无 Internal 层）
///
/// ⚠️ PHASE 1 PLACEHOLDER — DO NOT USE AS CANONICAL VALUE
/// P1 占位返回 1, Phase 4 替换为 gpgpu-sim 精确值 (G-D5 ±15%)
/// get_latency_mnk 不 override — 用 vendor 头 default impl 退化到 get_latency
class TensorCoreTLM : public ITensorCoreTiming {
public:
    TensorCoreTLM();
    ~TensorCoreTLM() override = default;

    uint32_t get_latency(TcPrecision prec) const override;
    uint32_t get_throughput_cycles(TcPrecision prec) const override;

    /// 占位标记（Phase 4 替换真实值后返回 false）
    bool is_placeholder() const { return is_placeholder_; }

private:
    bool is_placeholder_ = true;
};

}  // namespace tlm

#endif  // TLM_GPU_TENSOR_CORE_TLM_HH
