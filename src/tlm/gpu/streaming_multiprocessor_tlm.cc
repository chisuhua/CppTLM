// src/tlm/gpu/streaming_multiprocessor_tlm.cc
// StreamingMultiprocessorTLM 实现 (Task 4 stub; Task 18 完整实现 15 方法)
//
// 作者 CppTLM Team / 日期 2027-02-09
// 参考: docs/soc_arch/architecture/15-sm-microarchitecture-design.md §15.2 + §15.5
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

namespace tlm {

StreamingMultiprocessorTLM::StreamingMultiprocessorTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {
    // Task 1.3 P1-3: 初始化 ScalarALU 真值 (独立 cpptlm::gpu::ScalarALU)
    // parent_ = this, ScalarALU 通过 parent_->get_scalar_reg / set_scalar_reg
    // 访问 SM 顶层 scalar_regs_ 真值源 (Task 1.1 interim, Task 2.11 迁移到 RegFileUnit)
    scalar_alu_ = std::make_unique<cpptlm::gpu::ScalarALU>(this);
}

void StreamingMultiprocessorTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    adapter_ = adapter;
}

void StreamingMultiprocessorTLM::tick() {
    // Stub: Task 18 将协调 12 子模块:
    //   fu_->tick(); du_->tick(); iu_->tick(); ... 等
    // 当前为空 (Task 4 占位, 编译通过即可)
}

}  // namespace tlm