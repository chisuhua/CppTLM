// scripts/test/cep_chstream_bidirectional_macro_probe.cc
// `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏的**编译期锁**探针
//
// 用途(per `cpptlm-cep-cohif-carrier` task 4.4 的反向 fixture):
//   用 `PROBE_PORT_COUNT = 1` 编译本 TU ⇒ **必须编译失败**,且失败原因必须是
//   宏内的 `static_assert(n > 1)`(判据 CC-04 ③)—— N=1 会落
//   `BidirectionalPortAdapter::bind_ports` 的**空操作 shim** ⇒ 静默不绑定。
//
// ⚠️ **编译期锁不得只以「编译红」充当 ctest 判据** —— 本探针只提供「编译会红」这
//    一事实;真正的 ctest 判据是**调用方断言「编译确实失败了」且失败原因命中
//    预期的 static_assert**(见 `scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh`)。
//    ⇒ 若有人把宏里的 `static_assert` 删掉,本探针会**编译成功** ⇒ 调用方必须判红
//      ⇒ 回归被钉住。
//
// 编译:`g++ -std=c++17 -fsyntax-only -DPROBE_PORT_COUNT=<N> -I <include 路径> <probe.cc>`
#include "chstream_register.hh"

#include "bundles/cep_coherence_bundles_tlm.hh"
#include "cep_test_protocol_engine.hh"

#ifndef PROBE_PORT_COUNT
#define PROBE_PORT_COUNT 2
#endif

/// 宏必须在**函数体内**使用(与 `REGISTER_CHSTREAM` 同风格)
void cep_probe_register() {
    REGISTER_CHSTREAM_BIDIRECTIONAL(tlm::cep::testing::TestProtocolEngine,
                                     bundles::CohIfBundle, PROBE_PORT_COUNT);
}

int main() {
    return 0;
}
