// test/test_apu_soc_from_config.cc
// 端到端测试：从 configs/ 加载 APU SoC 拓扑（Phase 7.B）并运行仿真
//
// ⚠️ 2026-11-18：Phase 7.A 配置（`apu_soc_phase7a.json`）已随 `GPUTLM` 归档而删除
//    （per change `cpptlm-config-type-registration-align` T7.1：设计归 ArchForge，
//    CppTLM 只留框架验证示例）⇒ 本文件不再覆盖 7.A。
// 对应 docs/soc_arch/specs/apu-soc-design.md §2.2 (最终态) 和 §2.3 (阶段递进)
// 标签：[e2e][apu][from-config]
#include <filesystem>
#include <fstream>
#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/topology_parser.hh"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using cpptlm::parse_hierarchy_tree;

static json loadConfig(const std::string& path) {
    std::string full_path = std::string(CPPTLM_SOURCE_DIR) + "/" + path;
    std::ifstream f(full_path);
    REQUIRE(f.is_open());
    return json::parse(f);
}

// ────────────────────────────────────────────────────────────────────
// Phase 7.B: 2 CPU + Cache hierarchy + Crossbar + Memory
// ────────────────────────────────────────────────────────────────────

TEST_CASE("E2E: Load apu_soc_phase7b.json — Phase 7.B with CPU+Cache+CU",
          "[e2e][apu][from-config][phase7b]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    auto config = loadConfig("configs/apu_soc_phase7b.json");
    REQUIRE(factory.instantiateAll(config));
    factory.startAllTicks();

    // 2 CPU + 2 L1 + 1 L2 + 1 Crossbar + 1 Memory = 7 modules
    // （`compute_unit_0`(GPUTLM) 已于 2026-11-18 随类型归档移除）
    REQUIRE(factory.getInstance("cpu_0") != nullptr);
    REQUIRE(factory.getInstance("cpu_1") != nullptr);
    REQUIRE(factory.getInstance("l1_cpu_0") != nullptr);
    REQUIRE(factory.getInstance("l1_cpu_1") != nullptr);
    REQUIRE(factory.getInstance("l2_cpu_shared") != nullptr);
    REQUIRE(factory.getInstance("xbar") != nullptr);
    REQUIRE(factory.getInstance("hbm_memory") != nullptr);

    // Verify types
    REQUIRE(factory.getInstance("cpu_0")->get_module_type() == "TrafficGenTLM");
    REQUIRE(factory.getInstance("l1_cpu_0")->get_module_type() == "CacheTLM");
    REQUIRE(factory.getInstance("xbar")->get_module_type() == "CrossbarTLM");
    REQUIRE(factory.getInstance("hbm_memory")->get_module_type() == "MemoryTLM");

    // Verify 4-port CrossbarTLM
    auto* xbar = dynamic_cast<CrossbarTLM*>(factory.getInstance("xbar"));
    REQUIRE(xbar != nullptr);
    REQUIRE(xbar->num_ports() == 4);

    // Verify hierarchy tree (root → cpu_cluster/interconnect/memory_cluster)
    REQUIRE(config.contains("hierarchy"));
    auto root = parse_hierarchy_tree(config["hierarchy"]);
    REQUIRE(root != nullptr);
    REQUIRE(root->get_name() == "apu_phase7b");
    REQUIRE(root->get_children().size() == 3);

    // Verify coherence_domains
    REQUIRE(config.contains("coherence_domains"));
    REQUIRE(config["coherence_domains"].size() == 1);
    REQUIRE(config["coherence_domains"][0]["protocol"] == "WRITE_THROUGH_SIMPLIFIED");

    uint64_t before = eq.getCurrentCycle();
    eq.run(50);
    REQUIRE(eq.getCurrentCycle() == before + 50);
}

// ────────────────────────────────────────────────────────────────────
// Phase 7.F: Full APU —— **可装配**（2026-11-18 起）
//
// ⚠️ 本 case 原先叫「aspirational」并**有意不调** `instantiateAll` —— 因为
//    `ComputeUnitTLM`(×4) / `TCCTLM`(×1) 在运行时注册表里**不存在**，装配会静默丢弃它们。
//    change `cpptlm-config-type-registration-align`（T1.4）移除了这两个**无实现**的类型条目
//    ⇒ 配置现在可装配。本 case 相应改为**断言可装配**，并**反向断言**两类不再回归
//    （⛔ 不删 case 换绿灯 —— 它的判别价值从「记录愿景」变成「守住诚实」）。
// ────────────────────────────────────────────────────────────────────

TEST_CASE("E2E: apu_soc_full.json 可装配（ComputeUnitTLM/TCCTLM 条目已移除）",
          "[e2e][apu][from-config][phase7f][loadable]") {
    auto config = loadConfig("configs/apu_soc_full.json");

    REQUIRE(config["name"].get<std::string>().find("Phase 7.F") != std::string::npos);

    // 2 CPU + 3 Cache(L1×2+L2) + 4 GPU router + 1 CoherentXBar + 1 HBM = 11
    REQUIRE(config["modules"].size() == 11);
    REQUIRE(config["connections"].size() == 10);

    // ⛔ 反向断言：无实现的两个类型**不得**回到配置里（它们会让装配静默丢模块）
    int cu_count = 0;
    int tcc_count = 0;
    for (const auto& m : config["modules"]) {
        if (m["type"] == "ComputeUnitTLM")
            ++cu_count;
        if (m["type"] == "TCCTLM")
            ++tcc_count;
    }
    REQUIRE(cu_count == 0);
    REQUIRE(tcc_count == 0);

    // Verify 2x2 GPU mesh (4 RouterTLM)
    int router_count = 0;
    for (const auto& m : config["modules"]) {
        if (m["type"] == "RouterTLM")
            ++router_count;
    }
    REQUIRE(router_count == 4);

    // Verify hierarchy tree (4-level)
    REQUIRE(config.contains("hierarchy"));
    auto root = parse_hierarchy_tree(config["hierarchy"]);
    REQUIRE(root != nullptr);
    REQUIRE(root->get_name() == "apu_phase7f");

    // Verify MOESI coherence domain
    REQUIRE(config.contains("coherence_domains"));
    bool has_moesi = false;
    for (const auto& d : config["coherence_domains"]) {
        if (d.value("protocol", "") == "MOESI_AMD_6_STATE") {
            has_moesi = true;
            break;
        }
    }
    REQUIRE(has_moesi);

    // ★ 本 change 的核心成果：该配置**可装配**（原先因未注册类型只能「记录愿景」）
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(config));
}

// ────────────────────────────────────────────────────────────────────
// Bulk: verify Phase 7.B config is valid + loadable
// ────────────────────────────────────────────────────────────────────

TEST_CASE("E2E: Bulk load APU Phase 7.B config — v2.1.0 loadable subset",
          "[e2e][apu][from-config][bulk]") {
    const std::vector<std::string> configs_to_test = {
        "configs/apu_soc_phase7b.json",
    };

    for (const auto& cfg_path : configs_to_test) {
        EventQueue eq;
        REGISTER_CHSTREAM;
        ModuleFactory factory(&eq);

        auto config = loadConfig(cfg_path);
        INFO("APU Config: " << cfg_path);
        REQUIRE(factory.instantiateAll(config));
    }
}
