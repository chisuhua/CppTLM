// test/test_kernel_launch_tlm.cc
// KernelLaunchTLM AQL dispatcher 单元测试 (Catch2 v3.7.0)
// 功能: 验证 REQ-GPU-8A-4 规格中 KernelLaunchTLM stub 的契约行为
//       (setter/getter + 按 interval 周期计数 + 默认值 + module_type)
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/specs/gpu-soc-phase8a.md REQ-GPU-8A-4
//       openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.4

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {

    // 一次性注册所有 ChStream 模块 (KernelLaunchTLM 是 ChStream 派生)
    void registerChStreamModules() {
        static bool registered = false;
        if (!registered) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            registered = true;
        }
    }

} // namespace

TEST_CASE("KernelLaunchTLM.Defaults", "[kernel_launch][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    KernelLaunchTLM kl("kl0", &eq);

    // REQ-GPU-8A-4: 默认参数 (header 声明)
    REQUIRE(kl.get_kernel_id() == 0);
    REQUIRE(kl.get_workgroup_size() == 64);
    REQUIRE(kl.get_grid_size() == 1);
    REQUIRE(kl.get_kernel_launch_interval() == 1000);

    // 计数器初始为 0
    REQUIRE(kl.kernels_launched() == 0);
}

TEST_CASE("KernelLaunchTLM.SettersAndGetters", "[kernel_launch][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    KernelLaunchTLM kl("kl0", &eq);

    kl.set_kernel_id(42);
    kl.set_workgroup_size(128);
    kl.set_grid_size(4);
    kl.set_kernel_launch_interval(500);

    REQUIRE(kl.get_kernel_id() == 42);
    REQUIRE(kl.get_workgroup_size() == 128);
    REQUIRE(kl.get_grid_size() == 4);
    REQUIRE(kl.get_kernel_launch_interval() == 500);
}

TEST_CASE("KernelLaunchTLM.Interval100_Launches10", "[kernel_launch][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    KernelLaunchTLM kl("kl0", &eq);
    kl.set_kernel_launch_interval(100);
    REQUIRE(kl.kernels_launched() == 0);

    for (int i = 0; i < 1000; ++i) {
        kl.tick();
    }
    REQUIRE(kl.kernels_launched() == 10); // 1000/100 = 10
}

TEST_CASE("KernelLaunchTLM.Interval0_NoLaunch", "[kernel_launch][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    KernelLaunchTLM kl("kl1", &eq);
    kl.set_kernel_launch_interval(0); // 禁用 dispatch

    for (int i = 0; i < 1000; ++i) {
        kl.tick();
    }
    REQUIRE(kl.kernels_launched() == 0);
}

TEST_CASE("KernelLaunchTLM.PartialTick_NoLaunch", "[kernel_launch][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    KernelLaunchTLM kl("kl2", &eq);
    kl.set_kernel_launch_interval(100);

    // 50 个 tick: cycle_counter_ 走到 50, 没有命中 100 倍数 → 0 launches
    for (int i = 0; i < 50; ++i) {
        kl.tick();
    }
    REQUIRE(kl.kernels_launched() == 0);

    // 再补 50 个 tick: cycle_counter_ 走到 100, 触发第一次 launch
    for (int i = 0; i < 50; ++i) {
        kl.tick();
    }
    REQUIRE(kl.kernels_launched() == 1);
}

TEST_CASE("KernelLaunchTLM.ModuleType", "[kernel_launch][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    KernelLaunchTLM kl("kl0", &eq);
    REQUIRE(kl.get_module_type() == "KernelLaunchTLM");
}

TEST_CASE("KernelLaunchTLM.VariousIntervals", "[kernel_launch][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;

    SECTION("interval=10") {
        KernelLaunchTLM kl("kl10", &eq);
        kl.set_kernel_launch_interval(10);
        for (int i = 0; i < 1000; ++i) {
            kl.tick();
        }
        REQUIRE(kl.kernels_launched() == 100); // 1000/10 = 100
    }

    SECTION("interval=250") {
        KernelLaunchTLM kl("kl250", &eq);
        kl.set_kernel_launch_interval(250);
        for (int i = 0; i < 1000; ++i) {
            kl.tick();
        }
        REQUIRE(kl.kernels_launched() == 4); // 1000/250 = 4
    }

    SECTION("interval=1 (every cycle)") {
        KernelLaunchTLM kl("kl1", &eq);
        kl.set_kernel_launch_interval(1);
        for (int i = 0; i < 100; ++i) {
            kl.tick();
        }
        REQUIRE(kl.kernels_launched() == 100); // 100/1 = 100
    }
}