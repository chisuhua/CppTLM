// test/test_cep_engine_capabilities.cc
// 协议引擎承载能力声明面 —— 判据测试（J-1 / J-2 / J-2b / J-3 + AC-5 / AC-9）
// per openspec/changes/cep-protocol-engine-capability-declaration/spec.md
// 作者: CppTLM Team / 日期: 2026-11-23
//
// 判据纪律：每条 = 基线正控绿 ⇒ 破坏 ⇒ 按正确原因红 ⇒ 还原 ⇒ 必须绿。
// 本文件承担：
//   · J-1 / J-3 的**编译期**断言（失败签名含实例 id 字面量 ⇒ 满足 AC-3/AC-4）
//   · J-2 / J-2b 的**运行期**断言
//   · `engine_capabilities_are_consistent` 的**变异副本**负控
// 反向 fixture（probe TU 编译失败）在 `scripts/test/cep_engine_capabilities_check.sh`。

#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh" // repo_root()

#include <array>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/engine_capabilities.hh"

// ── 有引擎的 5 个实例（协议头；用于类型级能力特化 + SFINAE 反射）──
#include "tlm/cep/chi_rnf_protocol_engine.hh"
#include "tlm/cep/mesif_protocol_engine.hh"
#include "tlm/cep/moesi_gpu_protocol_engine.hh"
#include "tlm/cep/tilelink_protocol_engine.hh"
#include "tlm/cep/ualink_protocol_engine.hh"

// ── 持 `kTransientStateCount` 的 5 个 adapter（J-3 的范围；⚠️ ualink/nvlink 无该字段）──
#include "tlm/cep/chi_rnf_adapter.hh"
#include "tlm/cep/cxl_cache_adapter.hh"
#include "tlm/cep/mesif_adapter.hh"
#include "tlm/cep/moesi_gpu_adapter.hh"
#include "tlm/cep/tilelink_c_adapter.hh"

#include <fstream>

using cpptlm::cep::repo_root;

namespace {

using tlm::cep::EngineCapability;
using tlm::cep::InFlightModeling;
using tlm::cep::engine_capabilities_are_consistent;
using tlm::cep::in_flight_declaration_matches;
using tlm::cep::kEngineCapabilities;
using tlm::cep::kEngineCapabilityCount;
using tlm::cep::kEgressHasMachineCheck;
using tlm::cep::kMaxTransientStatesPerInstance;

// ══════════════════════════════════════════════════════════════════════════
//  J-1（编译期）：类型级声明的 in_flight ⇔ HasInFlightApi（真实公开 API）
//  ⚠️ 失败签名必须含**实例 id 字面量**（AC-3）⇒ 每条单独写、消息各带 id。
// ══════════════════════════════════════════════════════════════════════════
static_assert(in_flight_declaration_matches<tlm::cep::ChiRnfProtocolEngine>(),
              "chi_rnf: EngineTypeTraits::in_flight 与 HasInFlightApi 不一致");
static_assert(in_flight_declaration_matches<tlm::cep::MesifProtocolEngine>(),
              "mesif: EngineTypeTraits::in_flight 与 HasInFlightApi 不一致");
static_assert(in_flight_declaration_matches<tlm::cep::MoesiGpuProtocolEngine>(),
              "moesi_gpu: EngineTypeTraits::in_flight 与 HasInFlightApi 不一致");
static_assert(in_flight_declaration_matches<tlm::cep::TileLinkProtocolEngine>(),
              "tilelink_c: EngineTypeTraits::in_flight 与 HasInFlightApi 不一致");
static_assert(in_flight_declaration_matches<tlm::cep::UalinkProtocolEngine>(),
              "ualink_4: EngineTypeTraits::in_flight 与 HasInFlightApi 不一致");

// 正控补强：只有 chi_rnf 暴露在飞 API（其余四个必须为 false）
static_assert(tlm::cep::HasInFlightApi<tlm::cep::ChiRnfProtocolEngine>::value,
              "chi_rnf 应暴露 txn_in_flight");
static_assert(!tlm::cep::HasInFlightApi<tlm::cep::MesifProtocolEngine>::value,
              "mesif 不应暴露 txn_in_flight");
static_assert(!tlm::cep::HasInFlightApi<tlm::cep::MoesiGpuProtocolEngine>::value,
              "moesi_gpu 不应暴露 txn_in_flight");
static_assert(!tlm::cep::HasInFlightApi<tlm::cep::TileLinkProtocolEngine>::value,
              "tilelink_c 不应暴露 txn_in_flight");
static_assert(!tlm::cep::HasInFlightApi<tlm::cep::UalinkProtocolEngine>::value,
              "ualink_4 不应暴露 txn_in_flight");

// ══════════════════════════════════════════════════════════════════════════
//  J-3（编译期）：瞬态态策略上限（仅对持 kTransientStateCount 的 5 个 adapter）
//  ⚠️ ualink_adapter / nvlink_adapter **无该字段** ⇒ 不在本判据范围。
// ══════════════════════════════════════════════════════════════════════════
static_assert(tlm::cep::ChiRnfAdapter::kTransientStateCount <= kMaxTransientStatesPerInstance,
              "chi_rnf: kTransientStateCount 超出策略上限");
static_assert(tlm::cep::MesifAdapter::kTransientStateCount <= kMaxTransientStatesPerInstance,
              "mesif: kTransientStateCount 超出策略上限");
static_assert(tlm::cep::MoesiGpuAdapter::kTransientStateCount <= kMaxTransientStatesPerInstance,
              "moesi_gpu: kTransientStateCount 超出策略上限");
static_assert(tlm::cep::TileLinkCAdapter::kTransientStateCount <= kMaxTransientStatesPerInstance,
              "tilelink_c: kTransientStateCount 超出策略上限");
static_assert(tlm::cep::CxlCacheAdapter::kTransientStateCount <= kMaxTransientStatesPerInstance,
              "cxl_2_0: kTransientStateCount 超出策略上限");

// ══════════════════════════════════════════════════════════════════════════
//  AC-5 ②：egress 无机器判据的诚实登记必须为 false
// ══════════════════════════════════════════════════════════════════════════
static_assert(!kEgressHasMachineCheck,
              "kEgressHasMachineCheck 为 true 但仓内无 egress 机检载体 "
              "⇒ engine-cap-egress-claim-without-check");

} // namespace

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP engine_capabilities: 声明自洽（basis 非空 / id 唯一）", "[cep][engine_capabilities]") {
    REQUIRE(kEngineCapabilityCount == 8);
    CHECK(engine_capabilities_are_consistent(kEngineCapabilities.data(), kEngineCapabilityCount));

    for (std::size_t i = 0; i < kEngineCapabilityCount; ++i) {
        INFO("row " << i << " id=" << kEngineCapabilities[i].id);
        REQUIRE(kEngineCapabilities[i].id != nullptr);
        REQUIRE(kEngineCapabilities[i].id[0] != '\0');
        REQUIRE(kEngineCapabilities[i].basis != nullptr);
        REQUIRE(kEngineCapabilities[i].basis[0] != '\0');
    }
}

TEST_CASE("CEP engine_capabilities: 谓词变异副本负控（AC-6）", "[cep][engine_capabilities]") {
    // 基线：真实表必须一致
    REQUIRE(engine_capabilities_are_consistent(kEngineCapabilities.data(), kEngineCapabilityCount));

    SECTION("basis 置空 ⇒ 必假") {
        auto copy = kEngineCapabilities;
        copy[3].basis = "";
        CHECK_FALSE(engine_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("basis = nullptr ⇒ 必假") {
        auto copy = kEngineCapabilities;
        copy[0].basis = nullptr;
        CHECK_FALSE(engine_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("id 重复 ⇒ 必假") {
        auto copy = kEngineCapabilities;
        copy[5].id = copy[2].id;
        CHECK_FALSE(engine_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("id 置空 ⇒ 必假") {
        auto copy = kEngineCapabilities;
        copy[1].id = "";
        CHECK_FALSE(engine_capabilities_are_consistent(copy.data(), copy.size()));
    }
}

TEST_CASE("CEP engine_capabilities: 枚举名映射（egress / in_flight）", "[cep][engine_capabilities]") {
    using tlm::cep::egress_mode_name;
    using tlm::cep::in_flight_modeling_name;
    CHECK(std::string(egress_mode_name(tlm::cep::EgressMode::ProtocolMessages)) ==
          "protocol_messages");
    CHECK(std::string(egress_mode_name(tlm::cep::EgressMode::None)) == "none");
    CHECK(std::string(in_flight_modeling_name(tlm::cep::InFlightModeling::Modeled)) == "modeled");
    CHECK(std::string(in_flight_modeling_name(tlm::cep::InFlightModeling::NotModeled)) ==
          "not_modeled");
}

TEST_CASE("CEP engine_capabilities: 实例集双向零 orphan（J-2 / AC-2）", "[cep][engine_capabilities]") {
    const auto path = (repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json doc;
    in >> doc;
    REQUIRE(doc.contains("instances"));

    std::set<std::string> json_ids;
    for (const auto& inst : doc.at("instances"))
        json_ids.insert(inst.at("id").get<std::string>());

    std::set<std::string> decl_ids;
    for (std::size_t i = 0; i < kEngineCapabilityCount; ++i)
        decl_ids.insert(kEngineCapabilities[i].id);

    // 正向：每个 JSON 实例都被声明
    for (const auto& id : json_ids) {
        INFO("missing in kEngineCapabilities: " << id);
        CHECK(decl_ids.count(id) == 1); // engine-cap-orphan-missing
    }
    // 反向：没有多余声明
    for (const auto& id : decl_ids) {
        INFO("extra in kEngineCapabilities: " << id);
        CHECK(json_ids.count(id) == 1); // engine-cap-orphan-extra
    }
    CHECK(json_ids.size() == decl_ids.size());
    CHECK(json_ids.size() == kEngineCapabilityCount);
}

TEST_CASE("CEP engine_capabilities: egress 取值与现状一致（声明面 · ⛔ 无机器判据）",
          "[cep][engine_capabilities]") {
    // ⚠️ 本用例只核对**声明值**，⛔ 不核对引擎行为（egress 无机检载体 ⇒
    //    kEgressHasMachineCheck == false 已由编译期断言登记）。
    using tlm::cep::EgressMode;
    auto find = [](const char* id) -> const EngineCapability* {
        for (std::size_t i = 0; i < kEngineCapabilityCount; ++i)
            if (std::string(kEngineCapabilities[i].id) == id)
                return &kEngineCapabilities[i];
        return nullptr;
    };

    const EngineCapability* chi = find("chi_rnf");
    REQUIRE(chi != nullptr);
    CHECK(chi->egress == EgressMode::ProtocolMessages);

    const EngineCapability* mesif = find("mesif");
    REQUIRE(mesif != nullptr);
    CHECK(mesif->egress == EgressMode::ProtocolMessages);

    for (const char* id : {"moesi_gpu", "tilelink_c", "ualink_4", "cxl_2_0", "cxl_3_0_symmetric",
                           "nvlink_4"}) {
        const EngineCapability* row = find(id);
        REQUIRE(row != nullptr);
        INFO("id=" << id);
        CHECK(row->egress == EgressMode::None);
    }
}

TEST_CASE("CEP engine_capabilities: 3 个无引擎实例与 5 个有引擎实例的划分",
          "[cep][engine_capabilities]") {
    // 有引擎的 5 个实例必须能取到类型级特化（编译期已由 J-1 断言覆盖）⇒
    // 本用例只做「集合基数」的运行期复述。
    const std::set<std::string> kWithEngine{"chi_rnf", "mesif", "moesi_gpu", "tilelink_c",
                                            "ualink_4"};
    const std::set<std::string> kNoEngine{"cxl_2_0", "cxl_3_0_symmetric", "nvlink_4"};
    CHECK(kWithEngine.size() + kNoEngine.size() == kEngineCapabilityCount);

    for (std::size_t i = 0; i < kEngineCapabilityCount; ++i) {
        const std::string id = kEngineCapabilities[i].id;
        INFO("id=" << id);
        CHECK((kWithEngine.count(id) + kNoEngine.count(id)) == 1);
    }
}
