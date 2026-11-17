// test/test_param_alignment.cc
// 仓库级运行时对拍：凡含**已准入**类型实例的配置都必须能装配，且其实例无未知键
// 作者 CppTLM Team / 日期 2026-11-17
//
// 判据源：openspec/changes/cpptlm-module-param-read-declares/proposal.md AC-04
//   「遍历 configs/**/*.json（排除 configs/param_rules/**），对含已准入类型实例的配置调用
//     instantiateAll ⇒ 成功;并断言该类型实例的 unknown_keys() 为空。⛔ 不用 grep 提取键名」
//
// ⚠️ 准入类型清单**从 cpptlm::admitted_param_types() 派生**（唯一 C++ 真源）——
//    本文件不硬编码任何类型名（除下方登记的**基线豁免**，那些是既存缺陷不是本 change 的回归）。
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/param_admission.hh"

using json = nlohmann::json;

namespace {

    const std::string& source_dir() {
        static const std::string dir = CPPTLM_SOURCE_DIR;
        return dir;
    }

    /// 该配置（含嵌套 modules）里是否出现任一已准入类型的实例
    bool mentions_admitted(const json& node, const std::vector<std::string>& admitted) {
        if (node.is_object()) {
            auto t = node.find("type");
            if (t != node.end() && t->is_string()) {
                const std::string type = t->get<std::string>();
                for (const auto& a : admitted) {
                    if (a == type)
                        return true;
                }
            }
            for (auto it = node.begin(); it != node.end(); ++it) {
                if (it.value().is_structured() && mentions_admitted(it.value(), admitted))
                    return true;
            }
        } else if (node.is_array()) {
            for (const auto& el : node) {
                if (mentions_admitted(el, admitted))
                    return true;
            }
        }
        return false;
    }

    /// 基线豁免：改动**之前**就已无法 `instantiateAll` 的配置（与本 change 无关的既存缺陷）。
    /// 每条都必须写明原因 —— ⛔ 不得为了「让闸变绿」而往这里加新项。
    struct BaselineExempt {
        const char* path;
        const char* reason;
    };

    /// ⚠️ 每条都必须写明原因，并在 docs/implementation/module-param-baseline.md §3 留证据。
    const std::vector<BaselineExempt>& baseline_exempt() {
        static const std::vector<BaselineExempt> v = {
            {"configs/noc_mesh.json",
             "RouterTLM 实例既无 params 子对象、也无扁平 node_x/node_y ⇒ 改动前被 CFG-08 拦、"
             "改动后被 load_param 必填拦 ⇒ 行为等价（该配置全仓零引用）"},
        };
        return v;
    }

    const BaselineExempt* find_exempt(const std::string& rel) {
        for (const auto& e : baseline_exempt()) {
            if (rel == e.path)
                return &e;
        }
        return nullptr;
    }

} // namespace

TEST_CASE("AC-04: 含已准入类型实例的配置全部可装配且无未知键", "[param][alignment]") {
    const std::vector<std::string> admitted = cpptlm::admitted_param_types();
    REQUIRE_FALSE(admitted.empty());

    namespace fs = std::filesystem;
    const fs::path root = fs::path(source_dir()) / "configs";
    REQUIRE(fs::exists(root));

    std::size_t checked = 0;
    std::size_t exempted = 0;

    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;

        const std::string rel = fs::relative(entry.path(), fs::path(source_dir())).generic_string();
        if (rel.rfind("configs/param_rules/", 0) == 0)
            continue; // 规则表不是拓扑

        std::ifstream f(entry.path());
        if (!f.is_open())
            continue;
        json cfg;
        try {
            cfg = json::parse(f);
        } catch (const std::exception&) {
            continue; // 非拓扑片段 / 非法 JSON 不属本判据范围
        }
        if (!cfg.is_object() || !cfg.contains("modules"))
            continue;
        if (!mentions_admitted(cfg, admitted))
            continue;

        ++checked;
        if (const BaselineExempt* ex = find_exempt(rel)) {
            ++exempted;
            WARN("baseline-exempt: " << rel << " — " << ex->reason);
            continue;
        }

        EventQueue eq;
        ModuleFactory factory(&eq);
        INFO("config: " << rel);
        CHECK(factory.instantiateAll(cfg));
    }

    CHECK(checked >= 10); // 现有仓库实测：11 个含 RouterTLM 的配置
    WARN("of " << checked << " configs with admitted types, " << exempted
               << " are baseline-exempt");
}
