// test/test_pin_contract_short_name_registration.cc
// T23B.0 短名注册验证测试 (Phase A.2 P0 #1)。
//
// Per P3 Amendment (commit 8919afa3) + Metis bg_06a27d8b pre-Phase-C 评审:
//   PcieControllerIP + GpuInternalFabric 之前只注册 FQ 键
//   (`tlm::pcie::PcieControllerIP` / `tlm::cluster::GpuInternalFabric`),
//   但 JSON 配用短名。ModuleFactory 精确匹配 → 静默丢模块。
//   Fix (modules_cluster.hh): 照 _reg_pcieendpointip_short precedent 加 2 行短名注册。
//
// 本测试断言:getRegisteredModuleTypes() 同时含短名 + FQ (双注册保留)。

#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"
// Fallback for some include path styles:
#include "module_factory.hh"

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE("PcieControllerIP: 短名 + FQ 双注册已生效", "[controller_ip][p2]") {
    auto mods = ModuleFactory::getRegisteredModuleTypes();
    const bool has_short = std::find(mods.begin(), mods.end(), "PcieControllerIP") != mods.end();
    const bool has_fq =
        std::find(mods.begin(), mods.end(), "tlm::pcie::PcieControllerIP") != mods.end();
    REQUIRE(has_short);
    REQUIRE(has_fq);
}

TEST_CASE("GpuInternalFabric: 短名 + FQ 双注册已生效", "[fabric]") {
    auto mods = ModuleFactory::getRegisteredModuleTypes();
    const bool has_short = std::find(mods.begin(), mods.end(), "GpuInternalFabric") != mods.end();
    const bool has_fq =
        std::find(mods.begin(), mods.end(), "tlm::cluster::GpuInternalFabric") != mods.end();
    REQUIRE(has_short);
    REQUIRE(has_fq);
}

TEST_CASE("ModuleFactory: 未知短名 absent (double-registration 反向断言)", "[controller_ip][p2]") {
    auto mods = ModuleFactory::getRegisteredModuleTypes();
    const bool has_unknown =
        std::find(mods.begin(), mods.end(), "NonExistentModule_xyz") != mods.end();
    REQUIRE_FALSE(has_unknown);
}