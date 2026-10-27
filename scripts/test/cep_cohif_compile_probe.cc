// scripts/test/cep_cohif_compile_probe.cc
// CohIf 载体**编译期锁**探针 —— 被编译的 TU 必须因毒化副本而编译失败
//
// 用途(per `cpptlm-cep-cohif-carrier` task 1.7 / CC-02 ②③ 的**反向 fixture**):
//   · 「字段宽度 +1」的毒化副本 ⇒ `BUNDLE_FIELD_WIDTH` 静态断言触发 ⇒ 编译失败
//   · 「加虚函数」的毒化副本     ⇒ `is_trivially_copyable` 静态断言触发 ⇒ 编译失败
//
// ⚠️ **编译期锁不得只以「编译红」充当 ctest 判据** —— 本探针只提供「编译会红」这
//    一事实;真正的 ctest 判据是**调用方断言「编译确实失败了」**
//    (见 `scripts/test/cep_cohif_reverse_verify.sh` 与 Phase 6 的 Catch2 用例)。
//    ⇒ 若有人把宽度锁删掉,本探针会**编译成功** ⇒ 调用方必须判红 ⇒ 回归被钉住。
//
// 编译:`g++ -std=c++17 -fsyntax-only -I <include 路径> <probe.cc>`
#include "bundles/cep_coherence_bundles_tlm.hh"

#include <type_traits>

// CC-02 ② —— `is_trivially_copyable` 锁(加虚函数 ⇒ 此处触发)
static_assert(std::is_trivially_copyable_v<bundles::CohIfBundle>,
              "CC-02 ②: CohIfBundle 必须 trivially copyable");

// CC-02 ③ —— 宽度锁(头文件内已对**全部**字段施加 `BUNDLE_FIELD_WIDTH`;
//                 字段宽度 +1 ⇒ 头文件内的静态断言触发 ⇒ 本 TU 编译失败)
//
// CC-02 ① —— `sizeof` 锁(布局契约;加虚函数 / 改宽度都会改变布局 ⇒ 此处触发)
static_assert(sizeof(bundles::CohIfBundle) == 96,
              "CC-02 ①: CohIfBundle 布局尺寸漂移(期望 96 字节)");

int main() { return 0; }
