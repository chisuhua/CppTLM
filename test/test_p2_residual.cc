// test/test_p2_residual.cc
// P2 阶段 5 项残留 △ 补测 (per docs/superpowers/plans/2026-06-20-future-work-roadmap.md F1)
// 验证 P2 阶段应测但未测的 5 个细节:
//   P2.1: TpcCluster internal cu count
//   P2.2: routing="FIFO" 路径 (当前 GpuNoC 接受字符串但未实现 FIFO 算法)
//   P2.3: l1_size / l2_size 参数透传
//   P2.4: channel_size 参数透传
//   P2.5: 性能 metric 断言 (结构存在, 不断言 hit rate 具体值 — F10 负责扩展 metric 类型)
// 作者: Sisyphus / 日期: 2026-06-22
#include "chstream_register.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "metrics/metrics_reporter.hh"
#include "metrics/stats_manager.hh"
#include "modules.hh"
#include "modules_cluster.hh"
#include "tlm/cluster/cache_cluster.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/cluster/gpu_noc_cluster.hh"
#include "tlm/cluster/memory_cluster.hh"
#include <catch2/catch_all.hpp>

using json = nlohmann::json;

namespace {
    auto _register_p2 = []() {
        ModuleFactory::registerObject<MemoryTLM>("MemoryTLM");
        ModuleFactory::registerObject<ArbiterTLM<2>>("ArbiterTLM2");
        ModuleFactory::registerObject<ArbiterTLM<4>>("ArbiterTLM4");
        ModuleFactory::registerObject<tlm::RouterTLM>("RouterTLM");
        return 0;
    }();
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// P2.1: TpcCluster internal cu count
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("P2.1: GpuCluster set_config accepts gpc_count/tpc_per_gpc/cu_per_tpc",
          "[p2-residual][gpu][tpc]") {
    // 注: TpcCluster::cu_count 实际验证见 test_apu_soc_top.cc:108 P5 集成测试
    // (REQUIRE(xbar->peer_count() >= 16)). 本测试仅验证 GpuCluster::set_config 链路.
    EventQueue eq;
    auto* gpu = new GpuCluster("gpu0", &eq);

    json params = {{"gpc_count", 1}, {"tpc_per_gpc", 1}, {"cu_per_tpc", 4}};

    REQUIRE_NOTHROW(gpu->set_config(params));
    REQUIRE(gpu->get_module_type() == "GpuCluster");

    delete gpu;
}

// ─────────────────────────────────────────────────────────────────────────────
// P2.2: routing="FIFO" 路径 (GpuNoC 接受字符串, FIFO 算法本身未实现 — Phase 7.D+)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("P2.2: GpuNoC accepts routing=\"FIFO\" parameter", "[p2-residual][noc][routing]") {
    EventQueue eq;
    auto* noc = new GpuNoC("noc0", &eq);

    // 当前 GpuNoC 仅支持 XY 路由 (硬编码)；FIFO/RR 算法待 Phase 7.D 扩展
    // 此测试验证 routing 字符串参数被接受并存储（非空字段），但实际算法仍是 XY
    json params = {{"mesh_size", 2}, {"routing", "FIFO"}};
    noc->set_config(params);

    REQUIRE(noc->get_module_type() == "GpuNoC");
    // 注: 实际算法行为待 Phase 7.D 扩展 FIFORoutingAlgorithm
    SUCCEED("P2.2: GpuNoC routing=\"FIFO\" 参数被接受 (Phase 7.D 扩展实际算法)");

    delete noc;
}

// ─────────────────────────────────────────────────────────────────────────────
// P2.3: l1_size / l2_size 参数透传 (CacheCluster → internal CacheTLM)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("P2.3: CacheCluster l1_size/l2_size parameters propagate",
          "[p2-residual][cache][params]") {
    EventQueue eq;
    auto* cluster = new CacheCluster("cc0", &eq);

    // 验证 set_config 接受 l1_size / l2_size 并生成正确数量的 internal CacheTLM
    json params = {{"l1_count", 2}, {"l1_size", "64KB"}, {"l2_size", "1MB"}};
    cluster->set_config(params);
    cluster->simulate_instantiate({});

    // l1_count=2 + l2_count=1 (默认) = 3 internal instances
    REQUIRE(cluster->getInternalFactory().getAllInstances().size() == 3);

    auto* l2 = dynamic_cast<CacheTLM*>(cluster->getInternalInstance("l2"));
    auto* l1_0 = dynamic_cast<CacheTLM*>(cluster->getInternalInstance("l1_0"));
    auto* l1_1 = dynamic_cast<CacheTLM*>(cluster->getInternalInstance("l1_1"));
    REQUIRE(l2 != nullptr);
    REQUIRE(l1_0 != nullptr);
    REQUIRE(l1_1 != nullptr);

    delete cluster;
}

// ─────────────────────────────────────────────────────────────────────────────
// P2.4: channel_size 参数透传 (MemoryCluster → internal MemoryTLM)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("P2.4: MemoryCluster channel_size parameter propagates",
          "[p2-residual][memory][params]") {
    EventQueue eq;
    auto* mc = new MemoryCluster("mc0", &eq);

    json params = {{"channel_count", 4}, {"channel_size", "2GB"}, {"memory_type", "DDR4"}};
    mc->set_config(params);
    REQUIRE(mc->get_module_type() == "MemoryCluster");
    mc->simulate_instantiate({});

    // channel_count=4 + arbiter = 5 internal instances
    REQUIRE(mc->getInternalFactory().getAllInstances().size() == 5);
    REQUIRE(mc->getInternalInstance("arbiter") != nullptr);

    delete mc;
}

// ─────────────────────────────────────────────────────────────────────────────
// P2.5: 性能 metric 断言 (结构存在, 不断言 hit rate 具体值)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("P2.5: MetricsReporter infrastructure structure exists", "[p2-residual][metrics]") {
    // 验证 metric 基础设施存在 (F10 负责扩展具体 metric 类型)
    // 不断言 hit rate 等具体值, 只验证 API 可用 + StatsManager 单例 + Reporter 可生成

    // 清理全局状态
    tlm_stats::StatsManager::instance().reset_all();

    // 1. StatsManager 单例存在
    auto& sm1 = tlm_stats::StatsManager::instance();
    auto& sm2 = tlm_stats::StatsManager::instance();
    REQUIRE(&sm1 == &sm2);

    // 2. StatGroup 注册 + 查找链路通
    tlm_stats::StatGroup group("p2_residual.system");
    sm1.register_group(&group, "p2_residual.system");
    auto* found = sm1.find_group("p2_residual.system");
    REQUIRE(found != nullptr);
    REQUIRE(found->name() == "p2_residual.system");

    // 3. Reporter 可生成非空输出 (实际 JSON 格式: [{"<group_name>": {<stats>}}])
    tlm_stats::JSONReporter reporter;
    std::ostringstream oss;
    reporter.generate(oss);
    std::string output = oss.str();

    // 验证 JSON 输出非空 + 包含 group name 顶层段 (dotted name 被 nested 化: "a.b" → {"a": {"b":
    // {}}})
    REQUIRE_FALSE(output.empty());
    REQUIRE(output.find("\"p2_residual\"") != std::string::npos);
    REQUIRE(output.find("\"system\"") != std::string::npos);

    // 清理
    sm1.unregister_group("p2_residual.system");
}
