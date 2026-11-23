// test/test_nested_failure_propagation.cc
// change `cpptlm-module-param-migrate-batch2`（C2）的 T0.2 / AC-05 判据
//
//   AC-05 嵌套失败传播 —— 嵌套容器内某模块缺必填参数 ⇒ 外层 `instantiateAll`
//         返回 **false**;且该失败**不再**被静默吞掉
//         （`SimModule::simulate_instantiate` 的成败**不得**被丢弃）
//
// 改动前的缺陷（design D-4）：
//   `SimModule::simulate_instantiate` 调 `internal_factory->instantiateAll(config)`
//   后**丢弃返回值**（只打一行日志）⇒ 嵌套实例的必填声明即使执行了，
//   失败也不会让外层返回 false ⇒ **嵌套路径上的参数校验形同虚设**。
//
// 失败通道用**成员**而非返回值（design D-4 的第二个方案）：
//   `simulate_instantiate` 的签名被 CEP 冻结面锁定
//   （`include/tlm/pcie/pcie_endpoint_ip.hh` 属 `INV-CEP-07-IC5` 的 4 个 frozen
//   headers，零 diff 硬约束）⇒ 返回值从 void 改 bool 会让该 override 失配。
//   ⇒ `instantiate_ok_` 成员 + `instantiate_succeeded()` 读取口，语义等价。
//
// ⚠️ 本文件的判据**不含**「嵌套未知类型」—— 那属于 R-5 的 AC-08
//    （`test_config_type_registration.cc` 的邻近语义），此处只钉**参数缺失**这条路径。

#include <string>

#include <nlohmann/json.hpp>

#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"

using json = nlohmann::json;

namespace {

    /// 注册所有内置类型（与 `cpptlm_sim` 的正常路径一致 —— 见 `src/main.cpp`）
    void register_all() {
        REGISTER_ALL
        REGISTER_MODULE(CpuCluster);
    }

    /// 嵌套容器 + 内层一个**缺必填**的 RouterTLM（`node_x` 无默认值 ⇒ 必填）
    /// @param child_params 内层 RouterTLM 的 params（缺 `node_x` 即「必填缺失」）
    json nested_cfg(json child_params) {
        return json{
            {"modules", json::array({json{
                            {"name", "box"},
                            {"type", "CpuCluster"},
                            {"params", json::object()},
                            {"modules", json::array({json{{"name", "inner_r"},
                                                          {"type", "RouterTLM"},
                                                          {"params", std::move(child_params)}}})},
                            {"connections", json::array()}}})},
            {"connections", json::array()},
        };
    }

} // namespace

TEST_CASE("AC-05: 嵌套容器内缺必填参数 ⇒ 外层 instantiateAll 返回 false",
          "[nested][failure-propagation][param]") {
    EventQueue eq;
    register_all();
    ModuleFactory factory(&eq);

    // 内层 RouterTLM 缺 node_x（必填，无默认值）⇒ 内层装配必失败
    // mesh_x / mesh_y 给齐，确保**只有** node_x 是缺失项
    const json cfg = nested_cfg(json{{"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}});

    // ★ 核心判据：失败必须传播到外层
    REQUIRE_FALSE(factory.instantiateAll(cfg));

    // 失败时**实例表不提交**（`instances = std::move(object_instances)` 在函数末尾，
    // Step 4.5 的 early-return 到不了那里）⇒ 零半构造残留。
    CHECK(factory.getInstance("box") == nullptr);
    CHECK(factory.getInstance("inner_r") == nullptr);
}

TEST_CASE("AC-05 反向: 嵌套容器内参数齐备 ⇒ 外层 instantiateAll 返回 true（不误伤）",
          "[nested][failure-propagation][param]") {
    EventQueue eq;
    register_all();
    ModuleFactory factory(&eq);

    // 同一个嵌套结构，只是把 node_x 补齐 ⇒ 必须装配成功
    const json cfg = nested_cfg(json{{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}});

    REQUIRE(factory.instantiateAll(cfg));

    auto* box = factory.getInstance<SimModule>("box");
    REQUIRE(box != nullptr);
    // 内层实例确实建出来了（不是「返回 true 但什么都没建」）
    CHECK(box->getInternalFactory().getInstance("inner_r") != nullptr);
    // 且内层**上报了成功**（失败通道的读数与整体一致）
    CHECK(box->instantiate_succeeded());
}

TEST_CASE("AC-05: 失败后 instantiate_succeeded() 为 false（失败通道可读）",
          "[nested][failure-propagation][param]") {
    EventQueue eq;
    register_all();
    ModuleFactory factory(&eq);

    const json cfg = nested_cfg(json{{"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}});
    REQUIRE_FALSE(factory.instantiateAll(cfg));

    // 外层实例未提交 ⇒ 无法从外层读；改从**独立构造**的容器验证失败通道本身。
    // ⚠️ 这条断言钉的是「失败**不是**返回 void 后无迹可寻」——
    //    即使将来改回 void 签名，`instantiate_succeeded()` 也必须反映真实结果。
    CpuCluster standalone("standalone", &eq);
    standalone.simulate_instantiate(
        json{{"name", "standalone"},
             {"type", "CpuCluster"},
             {"params", json::object()},
             {"modules",
              json::array({json{{"name", "bad_r"},
                                {"type", "RouterTLM"},
                                {"params", json{{"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}}}}})},
             {"connections", json::array()}});
    CHECK_FALSE(standalone.instantiate_succeeded());
}
