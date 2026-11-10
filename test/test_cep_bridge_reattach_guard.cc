// test/test_cep_bridge_reattach_guard.cc
// 装配路径的**桥重挂检测**(`[BSS-01]`)—— 判据**执行臂**的判据
//
// 功能描述:验证 `ModuleFactory::instantiateAll()` 在**挂载循环之前**拦住
//          「同一模块属多个 `cep.bridges[]` entry」的拓扑 ⇒ 装配失败
//          ⇒ 避免模块的 `bridge_` 单槽被**后挂的桥静默覆盖**。
// 作者: CppTLM Team
// 日期: 2026-11-09
//
// ── 为什么有这组判据(per 台账 Entry #319 / critic B1 + B2)────────────────
// 契约侧判据 `[BSS-01]` 早已存在(`include/tlm/cep/instance_assembly_gate.hh`),
// 但它只被**测试侧**调用 ⇒ 「只测不拦」⇒ 装配路径**不拦** ⇒ R-3 残余。
// 本 change 补上**执行臂**:判据实现在 `include/tlm/cep/bridge_module_membership.hh`
// (**唯一实现**,装配闸与装配路径共用 ⇒ 禁双实现,per `src/core/module_factory.cc:173`)。
//
// ⚠️ **口径(与 `[BSS-01]` 现文本逐字一致)**:**无 entry 内去重** ⇒ 两个子情形都命中:
//    ① **跨 entry 重叠**(模块出现在 ≥2 条 entry 的 `modules` 中)
//    ② **单 entry 内重复**(同一 entry 的 `modules` 列同一模块两次,如 `["w","w"]`)
//    ⇒ 本文件对**两个子情形**各有一条用例。
//
// ⚠️ **载体形态**:走**真 `instantiateAll`**(与 `test_cep_channel_dependency_gate.cc`
//    Scenario 4 的正/负控对同型)⇒ ⛔ 不用「直接调 helper」冒充装配路径判据。
// ⚠️ **与既有 SCE-12 的关系**:`test/test_cep_stub_cache_e2e.cc` 的 SCE-12 判的是
//    **装配闸**(`run_instance_assembly_gate`)⇒ 本文件判的是**装配路径**
//    (`instantiateAll`)⇒ 两者互补,⛔ 不重复。
#include <filesystem>
#include <fstream>
#include <string>

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/instance_assembly_gate.hh"

#include <catch2/catch_all.hpp>

namespace {

/// 读一份拓扑 config(并把 `cep.protocol_instances` 改成绝对路径 —— 该字段是
/// **相对路径** ⇒ 装配期按 CWD 解析 ⇒ 须绝对化,per `test_cep_stub_cache_e2e.cc`
/// 的既有做法)
nlohmann::json load_topology(const char* file) {
    const auto path = std::filesystem::path(CPPTLM_SOURCE_DIR) / "configs" / file;
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json cfg = nlohmann::json::parse(in);
    cfg["cep"]["protocol_instances"] =
        (std::filesystem::path(CPPTLM_SOURCE_DIR) / "include" / "tlm" / "cep" /
         "protocol_instances.json")
            .string();
    return cfg;
}

/// 负向变换(与 SCE-12 **同一变换形态**):把 `w` 同时挂进**第二条** bridge entry
nlohmann::json with_second_entry_repeating_w(nlohmann::json cfg) {
    cfg["cep"]["bridges"].push_back({{"name", "cohif_noc_2"},
                                     {"protocol_instance", "chi_rnf"},
                                     {"bridge_id", 8},
                                     {"modules", nlohmann::json::array({"w", "chi"})}});
    return cfg;
}

/// 负向变换(子情形 ②):**同一条** entry 的 `modules` 里重复列 `w`
nlohmann::json with_single_entry_repeating_w(nlohmann::json cfg) {
    cfg["cep"]["bridges"][0]["modules"] = nlohmann::json::array({"w", "chi", "w"});
    return cfg;
}

} // namespace

// ── 子情形 ① · 跨 entry 重叠 ⇒ 装配失败(零桥挂载)──────────────────────
TEST_CASE("CEP bridge reattach guard: 跨 entry 重叠 ⇒ instantiateAll 返回 false",
          "[cep][assembly][bridge-reattach][bss-01]") {
    REGISTER_CHSTREAM;

    // 正控:原拓扑 B ⇒ 装配成功
    {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(load_topology("cep_chi_rnf_e2e_b.json")));
    }

    // 负向:同一基底 + 同一变换(第二条 entry 重复挂 `w`)⇒ 装配失败
    {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
                    with_second_entry_repeating_w(load_topology("cep_chi_rnf_e2e_b.json"))) ==
                false);
    }
}

// ── 子情形 ② · 单 entry 内重复 ⇒ 装配失败(口径不得收窄)────────────────
// ⚠️ 这条是**新增红条件**(今天可装配)⇒ 与 `[BSS-01]` 现文本对齐 ⇒ 口径不得收窄。
TEST_CASE("CEP bridge reattach guard: 单 entry 内重复 ⇒ instantiateAll 返回 false",
          "[cep][assembly][bridge-reattach][bss-01]") {
    REGISTER_CHSTREAM;

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(with_single_entry_repeating_w(
                load_topology("cep_chi_rnf_e2e_b.json"))) == false);
}

// ── 一致性:装配路径与装配闸对同一 config 结论**一致** ──────────────────
// ⚠️ 这是「**禁双实现**」的落地证明:两侧共用同一 helper ⇒ 同一 config 必须
//    同时「装配失败」且「闸判红」⇒ 杜绝「一边红一边不红」。
TEST_CASE("CEP bridge reattach guard: 装配路径与装配闸结论一致",
          "[cep][assembly][bridge-reattach][bss-01]") {
    const auto instances = [] {
        std::ifstream in(std::filesystem::path(CPPTLM_SOURCE_DIR) / "include" / "tlm" / "cep" /
                         "protocol_instances.json");
        REQUIRE(in.good());
        return nlohmann::json::parse(in);
    }();
    const auto planes = [] {
        std::ifstream in(std::filesystem::path(CPPTLM_SOURCE_DIR) / "include" / "tlm" / "cep" /
                         "collective_planes.json");
        REQUIRE(in.good());
        return nlohmann::json::parse(in);
    }();

    // 负向 config ⇒ 闸判红 **且** 装配失败
    {
        const auto cfg = with_second_entry_repeating_w(load_topology("cep_chi_rnf_e2e_b.json"));
        const auto rep = tlm::cep::run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE(rep.verdict == tlm::cep::InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[BSS-01]"));

        REGISTER_CHSTREAM;
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(cfg) == false);
    }

    // 正控 config ⇒ 闸判绿 **且** 装配成功
    {
        const auto cfg = load_topology("cep_chi_rnf_e2e_b.json");
        const auto rep = tlm::cep::run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE_FALSE(rep.has_error("[BSS-01]"));

        REGISTER_CHSTREAM;
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(cfg));
    }
}
