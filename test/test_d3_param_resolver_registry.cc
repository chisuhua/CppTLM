// test/test_d3_param_resolver_registry.cc
// ParamRules 注册表的**通用机制**测试 + 「RouterTLM 已迁移」的契约断言
// 作者 CppTLM Team / 日期 2026-10-08（2026-11-17 改写）
//
// 历史：本文件原为 `cpptlm-dgpu-realip-pin-contract` Change D 的 RouterTLM 自注册测试。
// 2026-11-17 起 RouterTLM 迁移到「读取即声明」机制（change cpptlm-module-param-read-declares
// / ADR-X.17 D-3），其 `get_param_rules()` 与静态注册已删除 ⇒ 原 RouterTLM 用例按
// design D-6 删除，保留注册表的通用形态断言，并新增迁移完成的**正向**契约。
#include "catch_amalgamated.hpp"

#include "core/module_factory.hh"
#include "core/param_admission.hh"
#include "core/param_rules.hh"

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE("D.3: unknown type lookup returns end (graceful skip)", "[d3][param-resolver]") {
    auto& registry = ModuleFactory::getParamRulesRegistry();
    auto it = registry.find("NonExistentType_XYZ");
    REQUIRE(it == registry.end());
}

TEST_CASE("D.3: registerParamRules is idempotent (ODR-safe)", "[d3][param-resolver]") {
    // 通用机制：同一 type 重复注册被 early-return 忽略，不覆盖原 provider
    auto make_rules = []() -> cpptlm::ParamRules {
        return {{"a",
                 cpptlm::ParamRule{"a", cpptlm::ParamType::INTEGER, true, std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                                   std::nullopt}}};
    };
    ModuleFactory::registerParamRules("__D3_IdempotentProbe__", make_rules);
    const size_t before = ModuleFactory::getParamRulesRegistry().size();

    ModuleFactory::registerParamRules("__D3_IdempotentProbe__", []() -> cpptlm::ParamRules {
        return {{"sentinel",
                 cpptlm::ParamRule{"sentinel", cpptlm::ParamType::INTEGER, true, std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                                   std::nullopt, std::nullopt}}};
    });
    const size_t after = ModuleFactory::getParamRulesRegistry().size();
    REQUIRE(before == after); // early-return

    // 原 provider 未被覆盖（不含 sentinel）—— 使用 find 避免 operator[] 隐式插入默认项
    auto& registry = ModuleFactory::getParamRulesRegistry();
    auto it = registry.find("__D3_IdempotentProbe__");
    REQUIRE(it != registry.end());
    const cpptlm::ParamRules rules = it->second();
    REQUIRE(rules.size() == 1);
    REQUIRE(rules.count("a") == 1);
    REQUIRE(rules.count("sentinel") == 0);
}

TEST_CASE("D.3: RouterTLM 已迁移到 load_param（不在注册表 + 已准入）", "[d3][param-resolver]") {
    // ADR-X.17 D-3 的准入条件 (b)：registry 中无 T 的条目
    auto& registry = ModuleFactory::getParamRulesRegistry();
    CHECK(registry.find("RouterTLM") == registry.end());

    // 准入表是唯一 C++ 真源 ⇒ 从它判定，不在测试里硬编码第二份事实
    CHECK(cpptlm::is_param_admitted("RouterTLM"));

    // 迁移后的类型清单包含它（供仓库级对拍派生）
    const auto admitted = cpptlm::admitted_param_types();
    REQUIRE(std::find(admitted.begin(), admitted.end(), "RouterTLM") != admitted.end());

    // 反向：NICTLM 仍未迁移（其 CFG-08 分支保留 ⇒ 不满足准入条件 (c)）
    CHECK_FALSE(cpptlm::is_param_admitted("NICTLM"));
}
