// src/tlm/gpu/tensor_core_tlm.cc
// TensorCoreTLM: ITensorCoreTiming 实现 — S4 Phase 2a 延迟表
// 作者 CppTLM Team / 日期 2026-07-18 (P1 占位) + 2026-07-21 (S4 Phase 2a)
// 参考:
//   - include/tlm/gpu/tensor_core_tlm.hh
//   - PTX-EMU docs/dev-process/cpptlm-co-simulation-plan.md §2a.2
//   - A100 MMA 指令间距: GPGPU-Sim tensor_core_config.h
//
// 延迟来源:
//   FP16=8, BF16=8, TF32=4, INT8=8, FP8=16, FP6=16, FP4=32
//   throughput_cycles: 与 latency 相同值 (A100 TC 每周期可发射 1 指令)
#include "tlm/gpu/tensor_core_tlm.hh"

namespace tlm {

TensorCoreTLM::TensorCoreTLM() : is_placeholder_(false) {}

uint32_t TensorCoreTLM::get_latency(TcPrecision prec) const {
    switch (prec) {
        case TcPrecision::FP4:  return 32;
        case TcPrecision::FP6:  return 16;
        case TcPrecision::FP8:  return 16;
        case TcPrecision::FP16: return 8;
        case TcPrecision::BF16: return 8;
        case TcPrecision::TF32: return 4;
    }
    return 8;  // default
}

uint32_t TensorCoreTLM::get_throughput_cycles(TcPrecision prec) const {
    // A100: 每 SM 每周期可发射 1 TC 指令, throughput = latency
    return get_latency(prec);
}

}  // namespace tlm
