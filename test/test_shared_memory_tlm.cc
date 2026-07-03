// test/test_shared_memory_tlm.cc
// SharedMemoryTLM 单元测试 (Catch2 v3.7.0)
// 功能: 验证 REQ-GPU-8A-1 规格中 SharedMemoryTLM stub 的契约行为
//       (bank conflict 周期模型 + 默认 size_kb/banks + setter 注入 + module_type)
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/specs/gpu-soc-phase8a.md REQ-GPU-8A-1
//       openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.1
//       docs/soc_arch/adr/D2 (bank conflict 简化模型: base 1 + 每个 conflict way +1)

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/shared_memory_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {

    // 一次性注册所有 ChStream 模块 (SharedMemoryTLM 是 ChStream 派生)
    void registerChStreamModules() {
        static bool registered = false;
        if (!registered) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            registered = true;
        }
    }

} // namespace

TEST_CASE("SharedMemoryTLM.BankConflict_FourWay_ReturnsFourCycles", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // REQ-GPU-8A-1: 4-way conflict → 1 base + 3 conflict = 4 cycles
    REQUIRE(sm.bank_conflict_cycles(4, 4) == 4);
}

TEST_CASE("SharedMemoryTLM.BankConflict_SingleThread_ReturnsOneCycle", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // 单线程不构成 conflict → base 1 cycle
    REQUIRE(sm.bank_conflict_cycles(1, 4) == 1);
}

TEST_CASE("SharedMemoryTLM.BankConflict_ZeroThreads_BoundaryOneCycle", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // 边界: 0 thread 不应 crash, 退化到 base 1 cycle
    REQUIRE(sm.bank_conflict_cycles(0, 0) == 1);
}

TEST_CASE("SharedMemoryTLM.BankConflict_WarpSize32_Returns32Cycles", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // warp 大小 32 (NVIDIA 标准) → 1 + 31 = 32 cycles
    REQUIRE(sm.bank_conflict_cycles(32, 4) == 32);
}

TEST_CASE("SharedMemoryTLM.BankConflict_StrideIgnored_InPhase8A", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // Phase 8.A 简化模型: stride_bytes 参数当前未参与计算
    // (per design.md §3.1 + ADR-NV-01 D2 决策)
    // 同 num_threads 不同 stride → 结果应一致
    REQUIRE(sm.bank_conflict_cycles(8, 4) == 8);
    REQUIRE(sm.bank_conflict_cycles(8, 128) == 8);
    REQUIRE(sm.bank_conflict_cycles(8, 1024) == 8);
}

TEST_CASE("SharedMemoryTLM.Defaults_Size64KB_Banks32", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // REQ-GPU-8A-1 默认: 64 KB (与 GB203 SM L1 一致) + 32 bank (NVIDIA 标准)
    REQUIRE(sm.get_size_kb() == 64);
    REQUIRE(sm.get_banks() == 32);
}

TEST_CASE("SharedMemoryTLM.SettersOverride_Defaults", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    sm.set_size_kb(128);
    sm.set_banks(64);

    REQUIRE(sm.get_size_kb() == 128);
    REQUIRE(sm.get_banks() == 64);

    // 再次覆盖: 验证 setter 可重复调用
    sm.set_size_kb(32);
    sm.set_banks(16);
    REQUIRE(sm.get_size_kb() == 32);
    REQUIRE(sm.get_banks() == 16);
}

TEST_CASE("SharedMemoryTLM.ModuleType_String", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // REQ-GPU-8A-1: 模块类型字符串必须为 "SharedMemoryTLM"
    // (ModuleFactory 通过此字符串查表创建实例)
    REQUIRE(sm.get_module_type() == "SharedMemoryTLM");
}

TEST_CASE("SharedMemoryTLM.Tick_NoCrash", "[smem][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    SharedMemoryTLM sm("sm0", &eq);

    // Phase 8.A stub: tick() 当前为 no-op, 仅验证不 crash
    // 真实 shared memory 访问模型在 Phase 9+ (per ADR-NV-01 §10)
    for (int i = 0; i < 100; ++i) {
        sm.tick();
    }
    // 断言仅做"未崩溃"语义验证
    REQUIRE(sm.get_size_kb() == 64);
    REQUIRE(sm.get_banks() == 32);
}
