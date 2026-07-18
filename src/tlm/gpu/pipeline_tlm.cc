// src/tlm/gpu/pipeline_tlm.cc
// PipelineTLM: IPipelineLatencyProvider 实现 — P1 占位, 所有 lat=1.0
// 作者 CppTLM Team / 日期 2026-07-18
// 参考: include/tlm/gpu/pipeline_tlm.hh
#include "tlm/gpu/pipeline_tlm.hh"

namespace tlm {

PipelineTLM::PipelineTLM() = default;

double PipelineTLM::get_fractional_cycles(const std::string& /*instruction*/,
                                          PipelineId /*pipe_id*/) const {
    return 1.0;
}

double PipelineTLM::get_fractional_cycles_by_type(int /*statement_type*/,
                                                  PipelineId /*pipe_id*/) const {
    return 1.0;
}

}  // namespace tlm
