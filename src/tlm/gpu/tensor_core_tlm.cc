// src/tlm/gpu/tensor_core_tlm.cc
// TensorCoreTLM: ITensorCoreTiming 实现 — P1 占位, 所有 lat/throughput = 1
// 作者 CppTLM Team / 日期 2026-07-18
// 参考: include/tlm/gpu/tensor_core_tlm.hh
#include "tlm/gpu/tensor_core_tlm.hh"

namespace tlm {

TensorCoreTLM::TensorCoreTLM() = default;

uint32_t TensorCoreTLM::get_latency(TcPrecision /*prec*/) const {
    return 1;
}

uint32_t TensorCoreTLM::get_throughput_cycles(TcPrecision /*prec*/) const {
    return 1;
}

}  // namespace tlm
