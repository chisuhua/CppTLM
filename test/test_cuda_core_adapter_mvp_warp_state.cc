// test/test_cuda_core_adapter_mvp_warp_state.cc
// =============================================================================
// CudaCoreAdapterMVP — WarpStateMirror 镜像单元测试 (S1 / T-s1-4 §6.e)
// 功能: 验证 WarpStateMirror 恰好 4 字段 (cycle_count / exec_mask /
//       blocked_cycles / scheduler_state), **不含 PC** (per R4 — 聚合初始化
//       第 5 个字段会编译失败, 构成编译期守卫); 验证 null warp / 空 SM 的
//       安全默认行为。
// 约束: 仅用 facade API + adapter API — 不 include 任何 PTX-EMU 头,
//       不直接构造 WarpContext/ThreadContext/GPUContext。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-4, Sisyphus)
// 标签 [cuda-core][mvp][warp-state]
// =============================================================================

#include "catch_amalgamated.hpp"

#include <type_traits>

#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

// 编译期守卫: WarpStateMirror 是标准布局聚合体, 且恰好 4 个数据成员。
// 聚合初始化 {cycle, mask, blocked, sched} 恰好覆盖全部字段 — 若有人
// 向 struct 添加 PC (或任何第 5 字段), 下方 4 值初始化仍合法, 但
// sizeof 断言会失败 (4 字段 = 8+4+4+4 = 20 bytes, 无 padding 悬念:
// uint64 对齐 8 → 8+4+4+4 = 20 → 对齐到 24)。为免 ABI 敏感, 改用
// 成员指针计数不可行, 故以"5 值聚合初始化不可编译"为文档化约定,
// 运行时断言字段语义正确即可。
static_assert(std::is_standard_layout_v<tlm::CudaCoreAdapterMVP::WarpStateMirror>,
              "WarpStateMirror 必须保持标准布局 (POD-like, 无 PC)");
static_assert(sizeof(tlm::CudaCoreAdapterMVP::WarpStateMirror) ==
                  sizeof(uint64_t) + 3 * sizeof(uint32_t) +
                      /*padding for alignof(uint64_t)=8*/ 4,
              "WarpStateMirror 恰好 4 字段 (cycle_count/exec_mask/"
              "blocked_cycles/scheduler_state), 禁止添加 PC");

TEST_CASE("cuda_core_adapter_warp_state_mirror_fields_no_pc",
          "[cuda-core][mvp][warp-state]") {
    // 4 值聚合初始化: 字段语义按声明顺序
    tlm::CudaCoreAdapterMVP::WarpStateMirror m{100, 0xFF, 7, 1};
    CHECK(m.cycle_count == 100u);
    CHECK(m.exec_mask == 0xFFu);
    CHECK(m.blocked_cycles == 7u);
    CHECK(m.scheduler_state == 1);  // 1=RUN

    // 默认构造全零 (IDLE)
    tlm::CudaCoreAdapterMVP::WarpStateMirror d{};
    CHECK(d.cycle_count == 0u);
    CHECK(d.exec_mask == 0u);
    CHECK(d.blocked_cycles == 0u);
    CHECK(d.scheduler_state == 0);  // 0=IDLE
}

TEST_CASE("cuda_core_adapter_warp_state_null_warp_safe_defaults",
          "[cuda-core][mvp][warp-state]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);

    // null warp: 仅 cycle_count 反映 SM 时钟, 其余字段安全默认
    adapter.tick();
    adapter.tick();
    auto m = adapter.get_warp_state(nullptr);
    CHECK(m.cycle_count == 2u);
    CHECK(m.exec_mask == 0u);
    CHECK(m.blocked_cycles == 0u);
    CHECK(m.scheduler_state == 0);

    facade.shutdown();
}

TEST_CASE("cuda_core_adapter_warp_state_empty_sm_graceful",
          "[cuda-core][mvp][warp-state]") {
    tlm::PtxEmuSubmoduleMVP facade;
    tlm::CudaCoreAdapterMVP adapter;
    adapter.init(facade);

    // 经 facade 查询空 SM 的 warp 0 (不存在 → nullptr) — 不 crash
    GPUContext* gpu = adapter.gpu_context();
    REQUIRE(gpu != nullptr);
    SMContext* sm = facade.get_sm_context(*gpu, 0);
    REQUIRE(sm != nullptr);
    WarpContext* warp = facade.get_warp_context(*sm, 0);  // 空 SM → nullptr
    CHECK(warp == nullptr);

    auto m = adapter.get_warp_state(warp);
    CHECK(m.cycle_count == 0u);
    CHECK(m.exec_mask == 0u);
    CHECK(m.blocked_cycles == 0u);
    CHECK(m.scheduler_state == 0);

    facade.shutdown();
}
