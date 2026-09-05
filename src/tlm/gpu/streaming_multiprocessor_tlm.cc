// src/tlm/gpu/streaming_multiprocessor_tlm.cc
// StreamingMultiprocessorTLM 实现 (Task 4 stub; Task 18 完整实现 15 方法)
//
// 作者 CppTLM Team / 日期 2027-02-09
// 参考: docs/soc_arch/architecture/15-sm-microarchitecture-design.md §15.2 + §15.5
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

namespace tlm {

StreamingMultiprocessorTLM::StreamingMultiprocessorTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {
    // Task 5 添加 12 子模块 .hh 后, 在构造函数中 make_unique 初始化:
    //   fu_ = std::make_unique<sm::FetchUnitTLM>(name + ".fu", eq);
    //   ... 等 12 个
    // Task 18 完整实现 15 方法 (IComputeDevice) + tick() 协调 12 子模块
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