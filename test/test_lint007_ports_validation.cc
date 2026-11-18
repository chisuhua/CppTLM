/**
 * @file test_lint007_ports_validation.cc
 * @brief `params.ports` 的处置判据 —— **态② 显式拒绝**（原 LINT007「告警不失败」已退役）
 *
 * ## 背景（2026-11-18 改判，per change `cpptlm-module-param-effect-landing` 的 B2 批次）
 *
 * 原判据（docs/superpowers/plans/2026-06-20-future-work-roadmap.md F7 / LINT007）：
 * `params.ports` 与类实际端口数不一致时 **emit warning 但非阻塞**（`validateConfig` 返 true、
 * 装配成功）。
 *
 * ⛔ **该判据已过时**：「告警但不失败」正是 `ADR-X.17` 要消灭的形态（静默容忍 ⇒ 用户以为
 * 配置生效、实际被忽略）。且端口数是**编译期常量**（`CrossbarTLM::NUM_PORTS` /
 * `ArbiterTLM<N_PORTS>`）⇒ 该键**不可能产生任何效果** ⇒ 按 `ADR-X.17` D-2 落**态②**：
 * **配置写了即装配失败并报出键名**。
 *
 * ⇒ 本文件由「告警不阻塞」改为「**显式拒绝**」判据；`module_factory_validate.cc` 的
 * LINT007 告警段已随之退役（否则是死代码）。
 *
 * ## Test Cases
 * 1. LINT007a: `params.ports > max_ports` ⇒ **装配失败**
 * 2. LINT007b: `params.ports < max_ports` ⇒ **装配失败**
 * 3. LINT007c: `params.ports == max_ports` ⇒ **仍失败**（**键存在即无效**，与值无关）
 * 4. LINT007d: 无 `params.ports` ⇒ 装配成功（正向对照,防误伤）
 * 5. LINT007e: `configs/apu_soc_v1.json` 加载成功（该配置的 `ports` 已按态②清理）
 *
 * @see docs/adr/ADR-X.17-module-param-read-declares.md（D-2 态② / D-6 行为判别义务）
 * @see openspec/changes/cpptlm-module-param-effect-landing/design.md（D-1.6 逐键裁决）
 */

#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/coherent_xbar_tlm.hh"
#include "tlm/crossbar_tlm.hh"

#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// CPPTLM_SOURCE_DIR 由 test/CMakeLists.txt 注入 (项目根目录)
#ifndef CPPTLM_SOURCE_DIR
#define CPPTLM_SOURCE_DIR "."
#endif

TEST_CASE("LINT007a: params.ports > max_ports ⇒ 装配失败（态② 显式拒绝）",
          "[config-lint][lint007]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    // CoherentXBarTLM NUM_PORTS=4, 用户配置 ports=8（原 forward-looking 占位）
    json config = R"({
        "modules": [
            {
                "name": "xbar_forward",
                "type": "CoherentXBarTLM",
                "params": { "ports": 8 }
            }
        ],
        "connections": []
    })"_json;

    // 态②：键存在 ⇒ 装配失败（⛔ 不再是「告警 + 成功」）
    CHECK_FALSE(factory.instantiateAll(config));
    CHECK(factory.getInstance("xbar_forward") == nullptr);
}

TEST_CASE("LINT007b: params.ports < max_ports ⇒ 装配失败（态② 显式拒绝）",
          "[config-lint][lint007]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {
                "name": "xbar_under",
                "type": "CrossbarTLM",
                "params": { "ports": 2 }
            }
        ],
        "connections": []
    })"_json;

    CHECK_FALSE(factory.instantiateAll(config));
    CHECK(factory.getInstance("xbar_under") == nullptr);
}

TEST_CASE("LINT007c: params.ports == max_ports ⇒ 仍失败（键存在即无效，与值无关）",
          "[config-lint][lint007]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    // ⚠️ 值等于类的实际端口数也**不放过** —— 态② 判据是「键存在即失败」，
    //    否则「配了个恰好等于默认值的无效键」仍会静默通过（正是要消灭的形态）。
    json config = R"({
        "modules": [
            {
                "name": "xbar_exact",
                "type": "CrossbarTLM",
                "params": { "ports": 4 }
            }
        ],
        "connections": []
    })"_json;

    CHECK_FALSE(factory.instantiateAll(config));
    CHECK(factory.getInstance("xbar_exact") == nullptr);
}

TEST_CASE("LINT007d: 无 params.ports ⇒ 装配成功（正向对照,防误伤）", "[config-lint][lint007]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    json config = R"({
        "modules": [
            {
                "name": "mem_no_ports",
                "type": "MemoryTLM",
                "params": { "capacity_gb": 1 }
            }
        ],
        "connections": []
    })"_json;

    CHECK(factory.instantiateAll(config));
    CHECK(factory.getInstance("mem_no_ports") != nullptr);
}

TEST_CASE("LINT007e: apu_soc_v1.json 可装配（其 ports 键已按态②清理）", "[config-lint][lint007]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    std::string path = std::string(CPPTLM_SOURCE_DIR) + "/configs/apu_soc_v1.json";
    std::ifstream ifs(path);
    REQUIRE(ifs.is_open());
    json config;
    ifs >> config;
    ifs.close();

    // ⚠️ 该配置原先的 `CoherentXBarTLM.params.ports = 8` 已按态②删除
    //    （否则本 case 会因「键存在」而失败）⇒ 这里也顺带守住「配置不回漂」。
    CHECK(factory.instantiateAll(config));
    CHECK(factory.getInstance("apu_top") != nullptr);
}
