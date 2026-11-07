// test/test_cep_instance_assembly.cc
// 判据:`ualink_4` 的**实例级装配闭环**(change `cpptlm-cep-ualink-instance-assembly`)。
//
// 覆盖:
//   IAG-01 正控 —— `configs/cep_ualink_pe_minimal.json` 装配完整 ⇒ PASS
//   IAG-02 反控 —— bridge 指向未声明实例 ⇒ `[IAG-02]`
//   IAG-03 反控 —— family 不可解析 ⇒ `[IAG-03]`
//   IAG-04 反控 —— plane 绑定为空 ⇒ `[IAG-04]`
//   IAG-05 反控 —— plane 名未登记 ⇒ `[IAG-05]`
//   IAG-06 反控 —— bridge 引用的模块不在拓扑里 ⇒ `[IAG-06]`
//   IAG-07 反控 —— bridge_id 重复 ⇒ `[IAG-07]`
//   IAG-08 一致性 —— `ualink_4` 在 channel dependency 闸中也有逐 VC verdict
//
// 作者: builder  日期: 2026-11-06
#include "catch_amalgamated.hpp"

#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/instance_assembly_gate.hh"

using namespace tlm::cep;

namespace {

    nlohmann::json load_json(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        REQUIRE(in.good());
        nlohmann::json j;
        in >> j;
        return j;
    }

    constexpr const char* kConfigRel = "configs/cep_ualink_pe_minimal.json";
    constexpr const char* kInstancesRel = "include/tlm/cep/protocol_instances.json";
    constexpr const char* kPlanesRel = "include/tlm/cep/collective_planes.json";

} // namespace

TEST_CASE("IAG-01: ualink_4 实例级装配正控 —— 声明 + 拓扑 + 平面三层齐备", "[cep][assembly]") {
    const auto cfg = load_json(kConfigRel);
    const auto instances = load_json(kInstancesRel);
    const auto planes = load_json(kPlanesRel);

    const auto rep =
        run_instance_assembly_gate(cfg, instances, planes, std::vector<std::string>{"ualink_4"});
    INFO(rep.log);
    REQUIRE(rep.verdict == InstanceAssemblyVerdict::PASS);
    REQUIRE(rep.errors.empty());
    REQUIRE(rep.mounted.size() == 1u);
    REQUIRE(rep.mounted.front() == "ualink_4");

    // 平面层:`ualink_4` 必须绑**非空且已登记**的 plane。
    REQUIRE(planes.at("plane_binding").contains("ualink_4"));
    REQUIRE(planes.at("plane_binding").at("ualink_4").get<std::string>() ==
            "FabricCollectivePlane");
    REQUIRE(find_collective_plane("FabricCollectivePlane") != nullptr);

    // 拓扑层:bridge 必须真的把 `ualink_4` 挂上,且引用的模块在拓扑里。
    REQUIRE(cfg.at("cep").at("bridges").size() == 1u);
    REQUIRE(cfg.at("cep").at("bridges")[0].at("protocol_instance").get<std::string>() ==
            "ualink_4");
}

TEST_CASE("IAG-02~07: 装配闸的反向 fixture(逐条按归因 tag 变红)", "[cep][assembly]") {
    const auto instances = load_json(kInstancesRel);
    const auto planes = load_json(kPlanesRel);

    // ② bridge 指向**未声明**的实例
    {
        auto cfg = load_json(kConfigRel);
        cfg["cep"]["bridges"][0]["protocol_instance"] = "no_such_instance";
        const auto rep = run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-02]"));
    }
    // ③ family 不可解析
    {
        auto cfg = load_json(kConfigRel);
        auto inst = instances;
        for (auto& i : inst["instances"]) {
            if (i.value("id", "") == "ualink_4") {
                i["family"] = "no_such_family";
            }
        }
        const auto rep = run_instance_assembly_gate(cfg, inst, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-03]"));
    }
    // ④ plane 绑定为空(仅对**显式要求绑定**的实例判红)
    {
        auto pl = planes;
        pl["plane_binding"]["ualink_4"] = nullptr;
        const auto cfg = load_json(kConfigRel);
        const auto rep =
            run_instance_assembly_gate(cfg, instances, pl, std::vector<std::string>{"ualink_4"});
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-04]"));
    }
    // ④′ 未要求绑定的实例 + 空绑定 ⇒ **合法**(该实例不启用 collective)⇒ 不判红
    {
        auto pl = planes;
        pl["plane_binding"]["ualink_4"] = nullptr;
        const auto cfg = load_json(kConfigRel);
        const auto rep = run_instance_assembly_gate(cfg, instances, pl);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::PASS);
    }
    // ④″ 要求绑定但**键缺失** ⇒ 红
    {
        auto pl = planes;
        pl["plane_binding"].erase("ualink_4");
        const auto cfg = load_json(kConfigRel);
        const auto rep =
            run_instance_assembly_gate(cfg, instances, pl, std::vector<std::string>{"ualink_4"});
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-04]"));
    }
    // ⑤ plane 名未登记
    {
        auto pl = planes;
        pl["plane_binding"]["ualink_4"] = "NoSuchPlane";
        const auto cfg = load_json(kConfigRel);
        const auto rep = run_instance_assembly_gate(cfg, instances, pl);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-05]"));
    }
    // ⑥ bridge 引用的模块不在拓扑里
    {
        auto cfg = load_json(kConfigRel);
        cfg["cep"]["bridges"][0]["modules"] = nlohmann::json::array({"ghost_module"});
        const auto rep = run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-06]"));
    }
    // ⑦ bridge_id 重复
    {
        auto cfg = load_json(kConfigRel);
        cfg["cep"]["bridges"].push_back(cfg["cep"]["bridges"][0]);
        const auto rep = run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-07]"));
    }
    // ⑧ 缺 `family` ⇒ 红(声明层不完整)
    {
        auto cfg = load_json(kConfigRel);
        auto inst = instances;
        for (auto& i : inst["instances"]) {
            if (i.value("id", "") == "ualink_4") {
                i.erase("family");
            }
        }
        const auto rep = run_instance_assembly_gate(cfg, inst, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-03]"));
    }
    // ⑨ bridge 的 `modules` 为空 ⇒ 红(拓扑层不完整)
    {
        auto cfg = load_json(kConfigRel);
        cfg["cep"]["bridges"][0]["modules"] = nlohmann::json::array();
        const auto rep = run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-06]"));
    }
    // ⑩ 缺 `bridge_id` ⇒ 红
    {
        auto cfg = load_json(kConfigRel);
        cfg["cep"]["bridges"][0].erase("bridge_id");
        const auto rep = run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-07]"));
    }
    // ① 无 bridge ⇒ 未挂进拓扑
    {
        auto cfg = load_json(kConfigRel);
        cfg["cep"]["bridges"] = nlohmann::json::array();
        const auto rep = run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE(rep.verdict == InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-01]"));
    }
}

TEST_CASE("IAG-08: 装配闭环与既有闸一致 —— channel 闸对 ualink_4 也有逐 VC verdict",
          "[cep][assembly]") {
    const auto cfg = load_json(kConfigRel);
    const auto rep = run_channel_gate(cfg);
    REQUIRE(rep.verdict != GateVerdict::FAIL);
    REQUIRE(rep.per_instance.count("ualink_4") == 1u);
    bool saw_ualink_verdict = false;
    for (const auto& v : rep.verdicts) {
        if (v.instance == "ualink_4") {
            saw_ualink_verdict = true;
            REQUIRE(v.verdict != GateVerdict::FAIL);
        }
    }
    REQUIRE(saw_ualink_verdict);
}
