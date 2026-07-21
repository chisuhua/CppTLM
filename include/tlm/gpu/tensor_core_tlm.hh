// include/tlm/gpu/tensor_core_tlm.hh
// TensorCoreTLM: ITensorCoreTiming 实现 — S4 Phase 2a 延迟表
// 功能: TensorCore latency/throughput 查询, 基于 A100 MMA 指令间距
// 作者 CppTLM Team / 日期 2026-07-18 (P1 占位) + 2026-07-21 (S4 Phase 2a)
// 参考:
//   - PTX-EMU docs/dev-process/cpptlm-co-simulation-plan.md §2a.2
//   - A100 MMA 指令间距: GPGPU-Sim tensor_core_config.h
//   - include/cudart/tensor_core_interface.h (vendor from PTX-EMU 463038e0)
#ifndef TLM_GPU_TENSOR_CORE_TLM_HH
#define TLM_GPU_TENSOR_CORE_TLM_HH

#include "cudart/tensor_core_interface.h"  // ITensorCoreTiming + TcPrecision enum (vendor)

#include <cstdint>

namespace tlm {

/// TensorCoreTLM: ITensorCoreTiming 的 CppTLM 端实现（直接继承, 无 Internal 层）
///
/// S4 Phase 2a: 实现基于 A100 的真实 TC 延迟表
///   - FP16=8, BF16=8, TF32=4, INT8=8, FP8=16, FP6=16, FP4=32
///   - get_latency_mnk 不 override — 用 vendor 头 default impl 退化到 get_latency
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
