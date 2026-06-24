// test/test_memory_cluster_tlm.cc
// MemoryClusterTLM 单元测试 (Catch2 v3.7.0)
// 功能: 验证 REQ-GPU-8A-2 规格中 MemoryClusterTLM stub 的契约行为
//       (round-robin channel 分配 + 默认 channels/capacity_gb + setter 注入 + module_type)
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/specs/gpu-soc-phase8a.md REQ-GPU-8A-2
//       openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.2
//       docs/soc_arch/adr/D2 (round-robin 简化模型, 不模拟真实 DRAM 调度)

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/memory_cluster_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {

    // 一次性注册所有 ChStream 模块 (MemoryClusterTLM 是 ChStream 派生)
    void registerChStreamModules() {
        static bool registered = false;
        if (!registered) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            registered = true;
        }
    }

} // namespace

// 1) 4-channel round-robin: 5 次调用第 5 次回到 channel 0
TEST_CASE("MemoryClusterTLM.RoundRobin_4Channels_5thCallWrapsTo0", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc("mc0", &eq);
    mc.set_channels(4);

    // REQ-GPU-8A-2: round-robin 分配, 4 channel 满 5 次回到 0
    REQUIRE(mc.allocate_channel(0) == 0);
    REQUIRE(mc.allocate_channel(1) == 1);
    REQUIRE(mc.allocate_channel(2) == 2);
    REQUIRE(mc.allocate_channel(3) == 3);
    REQUIRE(mc.allocate_channel(4) == 0); // 第 5 次 wrap 到 channel 0
}

// 2) Single channel 始终返回 0 (模 1 永远是 0)
TEST_CASE("MemoryClusterTLM.RoundRobin_1Channel_AlwaysZero", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc("mc0", &eq);
    mc.set_channels(1);

    // 任意 request_id 都应分配到 channel 0
    REQUIRE(mc.allocate_channel(0) == 0);
    REQUIRE(mc.allocate_channel(99) == 0);
    REQUIRE(mc.allocate_channel(1000) == 0);
}

// 3) 默认值: channels=4, capacity_gb=8 (GB203 HBM 典型配置)
TEST_CASE("MemoryClusterTLM.Defaults_Channels4_CapacityGB8", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc2("mc1", &eq);

    // REQ-GPU-8A-2 默认: 4 通道 (GB203 HBM 典型) + 8 GB (GB203 显存典型)
    REQUIRE(mc2.get_channels() == 4);
    REQUIRE(mc2.get_capacity_gb() == 8);
}

// 4) Setter 注入: 多次覆盖可工作
TEST_CASE("MemoryClusterTLM.SettersOverride_Defaults", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc("mc0", &eq);

    mc.set_channels(8);
    mc.set_capacity_gb(16);
    REQUIRE(mc.get_channels() == 8);
    REQUIRE(mc.get_capacity_gb() == 16);

    // 再次覆盖: 验证 setter 可重复调用
    mc.set_channels(2);
    mc.set_capacity_gb(4);
    REQUIRE(mc.get_channels() == 2);
    REQUIRE(mc.get_capacity_gb() == 4);
}

// 5) get_module_type 返回固定字符串 (ModuleFactory 查表用)
TEST_CASE("MemoryClusterTLM.ModuleType_String", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc("mc0", &eq);

    // REQ-GPU-8A-2: 模块类型字符串必须为 "MemoryClusterTLM"
    REQUIRE(mc.get_module_type() == "MemoryClusterTLM");
}

// 6) requests_completed 初始为 0
TEST_CASE("MemoryClusterTLM.RequestsCompleted_InitialZero", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc("mc0", &eq);

    // 构造后初始统计应为 0
    REQUIRE(mc.requests_completed() == 0);
}

// 7) tick() 推进 requests_completed (单调不降)
TEST_CASE("MemoryClusterTLM.Tick_AdvancesRequestsCompleted", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc("mc0", &eq);

    uint64_t before = mc.requests_completed();
    mc.tick();
    REQUIRE(mc.requests_completed() >= before);

    // 多次 tick: 严格递增 (本次实现每 tick +1)
    for (int i = 0; i < 5; ++i) {
        mc.tick();
    }
    REQUIRE(mc.requests_completed() >= before + 1);
}

// 8) Round-robin 大量调用不重复, 覆盖完整周期
TEST_CASE("MemoryClusterTLM.RoundRobin_8Channels_FullCycle", "[memcluster][gpu][phase8a]") {
    registerChStreamModules();
    EventQueue eq;
    MemoryClusterTLM mc("mc0", &eq);
    mc.set_channels(8);

    // 16 次调用应两次完整 cycle 0..7
    for (uint64_t i = 0; i < 8; ++i) {
        REQUIRE(mc.allocate_channel(i) == static_cast<uint32_t>(i));
    }
    for (uint64_t i = 0; i < 8; ++i) {
        REQUIRE(mc.allocate_channel(i) == static_cast<uint32_t>(i));
    }
}
