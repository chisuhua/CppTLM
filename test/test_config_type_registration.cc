// test/test_config_type_registration.cc
// change `cpptlm-config-type-registration-align`（R-5）的判据测试
//
//   AC-01 正向不误伤 —— 全已注册类型的配置 ⇒ `instantiateAll` 返回 true
//   AC-02 负向 fail-fast —— 含未注册类型 ⇒ 返回 false，且**实例表零污染**
//                          （⛔ 不断言日志 —— 日志不是判据接口，且 DPRINTF 仅 Debug 生效）
//   AC-03 失败后可重入 —— 同进程内先失败一次，再跑合法配置 ⇒ 仍 true 且实例完好
//                          （动机：`ModuleGroup` 的静态表持裸指针，半构造会留下悬空）
//   AC-04 差集为空 —— 三份曾受影响的配置**可装配**
//
// ⚠️ 判据的「未注册类型集合」**不硬编码** —— AC-02 用的伪类型名只在测试内构造，
//    实现侧（`ModuleFactory::instantiateAll` 的 Step 0.3）只查运行时注册表。
// ⚠️ AC-08（嵌套失败传播）**不在本文件** —— 它 `blocked-by` change
//    `cpptlm-module-param-migrate-batch2` 的 T0.2（`simulate_instantiate` 丢弃返回值）。

#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"

using json = nlohmann::json;

namespace {

    /// 只含已注册类型的最小合法配置（`CPUTLM` + `MemoryTLM`，均为 `REGISTER_CHSTREAM` 路径）
    json valid_config() {
        return json{
            {"name", "valid_min"},
            {"modules",
             json::array({json{{"name", "cpu"}, {"type", "CPUTLM"}},
                          json{{"name", "mem"}, {"type", "MemoryTLM"}, {"params", {{"capacity_gb", 1}}}}})},
            {"connections", json::array()},
        };
    }

    /// 注册所有内置类型（与 `cpptlm_sim` 的正常路径一致 —— 见 `src/main.cpp::registerAllTypes`）
    void register_all() {
        REGISTER_ALL
        REGISTER_MODULE(CpuCluster);
    }

} // namespace

TEST_CASE("AC-01: 全已注册类型的配置 ⇒ 装配成功（不误伤）", "[config][type-registry][registration]") {
    EventQueue eq;
    register_all();
    ModuleFactory factory(&eq);
    factory.set_config_source("<test:valid>");

    const json cfg = valid_config();
    REQUIRE(factory.instantiateAll(cfg));

    // 实例真的建出来了（不是「返回 true 但什么都没建」）
    REQUIRE(factory.getInstance("cpu") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
}

TEST_CASE("AC-02: 含未注册类型 ⇒ 装配失败 + 实例表零污染", "[config][type-registry][registration]") {
    EventQueue eq;
    register_all();
    ModuleFactory factory(&eq);
    factory.set_config_source("<test:unregistered>");

    // 第一项合法、第二项未注册、第三项未注册 ⇒ 判据只看返回值与实例表，不看日志
    json cfg = valid_config();
    cfg["modules"].push_back(json{{"name", "bogus1"}, {"type", "NoSuchTypeXYZ"}});
    cfg["modules"].push_back(json{{"name", "bogus2"}, {"type", "AlsoNotRegistered"}});

    // ⚠️ 前提校验：这两个名字确实不在运行时注册表里（否则本 case 什么都没验）
    const auto types = ModuleFactory::getRegisteredTypes();
    for (const auto& t : types) {
        REQUIRE(t != "NoSuchTypeXYZ");
        REQUIRE(t != "AlsoNotRegistered");
    }

    REQUIRE_FALSE(factory.instantiateAll(cfg));

    // ★ 零实例残留：预扫描在实例创建**之前** ⇒ 连**合法**项也不该被建出来
    //   （这是「由位置保证」的直接可观测后果，而不只是「没崩」）
    CHECK(factory.getInstance("cpu") == nullptr);
    CHECK(factory.getInstance("mem") == nullptr);
    CHECK(factory.getInstance("bogus1") == nullptr);
    CHECK(factory.getInstance("bogus2") == nullptr);
}

TEST_CASE("AC-03: 失败后可重入 —— 同一 factory 再跑合法配置仍成功", "[config][type-registry][registration]") {
    EventQueue eq;
    register_all();
    ModuleFactory factory(&eq);

    // 第一次：失败
    {
        json bad = valid_config();
        bad["modules"].push_back(json{{"name", "bogus"}, {"type", "NoSuchTypeXYZ"}});
        REQUIRE_FALSE(factory.instantiateAll(bad));
    }

    // 第二次：合法配置 ⇒ 仍成功，且实例完好
    // （动机：`ModuleGroup` 的静态注册表持裸指针 —— 半构造对象会留下悬空指针）
    const json good = valid_config();
    REQUIRE(factory.instantiateAll(good));
    REQUIRE(factory.getInstance("cpu") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
}

TEST_CASE("AC-04: 三份曾含未注册类型的配置均可装配", "[config][type-registry][registration][from-config]") {
    EventQueue eq;
    register_all();

    // 这三份就是 §2bis 登记过的漂移现场（dgpu_board_v1 / library_memory_demo / apu_soc_full）
    const char* configs[] = {
        "configs/dgpu_board_v1.json",
        "configs/library_memory_demo.json",
        "configs/apu_soc_full.json",
    };
    for (const char* path : configs) {
        INFO("config: " << path);
        json cfg;
        {
            std::ifstream in(path);
            REQUIRE(in.good());
            in >> cfg;
        }
        ModuleFactory f(&eq);
        f.set_config_source(path);
        CHECK(f.instantiateAll(cfg));
    }
}
