// test/test_cep_hop_timing_capabilities.cc
// 协议语义层的「hop / 时序模型」承载能力声明面 —— 判据测试
// per openspec/changes/cep-hop-timing-capability-declaration/spec.md
// 作者: CppTLM Team / 日期: 2026-11-24
//
// 判据纪律：每条 = 基线正控绿 ⇒ 破坏 ⇒ 按正确原因红 ⇒ 还原 ⇒ 必须绿。
// 本文件承担：
//   · J-5 的**编译期否定断言**（引擎新增 hop / 时序 API ⇒ 编译失败；失败签名含实例 id）
//   · J-1 / J-2 / J-3 的**运行期**断言
//   · 诚实边界常量（J-8）的断言 + **载体 / 器件层既有事实**的复述（J-6）
//   · 谓词变异副本负控（basis 置空 / id 重复 / 协议语义层 hop ≠ None）
// 反向 fixture（引擎头注入 API ⇒ 闸级 J-4 变红）在
// `scripts/test/cep_hop_timing_capabilities_check.sh`。

#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh" // repo_root()

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "tlm/cep/hop_timing_capabilities.hh"

// ── 有引擎的 5 个实例（协议头；用于编译期否定断言）──
#include "tlm/cep/chi_rnf_protocol_engine.hh"
#include "tlm/cep/mesif_protocol_engine.hh"
#include "tlm/cep/moesi_gpu_protocol_engine.hh"
#include "tlm/cep/tilelink_protocol_engine.hh"
#include "tlm/cep/ualink_protocol_engine.hh"

using cpptlm::cep::repo_root;

namespace {

using tlm::cep::HopModel;
using tlm::cep::HopTimingCapability;
using tlm::cep::TimingModel;
using tlm::cep::has_hop_api_v;
using tlm::cep::has_timing_api_v;
using tlm::cep::hop_timing_capabilities_are_consistent;
using tlm::cep::kHopTimingAbsenceIsProtocolSemanticsOnly;
using tlm::cep::kHopTimingCapabilities;
using tlm::cep::kHopTimingCapabilityCount;
using tlm::cep::kProtocolSemanticsHasNoHopOrTimingModel;
using tlm::cep::kTickIsSteppingHookNotTimingModel;

// ══════════════════════════════════════════════════════════════════════════
//  J-5（编译期）：协议语义层**零** hop / 时序 API
//  ⚠️ 失败签名必须含**实例 id 字面量** ⇒ 每条单独写、消息各带 id。
// ══════════════════════════════════════════════════════════════════════════
static_assert(!has_hop_api_v<tlm::cep::ChiRnfProtocolEngine>,
              "chi_rnf: 引擎暴露了 hop 计数 API ⇒ 协议语义层已有 hop 模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_timing_api_v<tlm::cep::ChiRnfProtocolEngine>,
              "chi_rnf: 引擎暴露了消息级时序 API ⇒ 协议语义层已有时序模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_hop_api_v<tlm::cep::MesifProtocolEngine>,
              "mesif: 引擎暴露了 hop 计数 API ⇒ 协议语义层已有 hop 模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_timing_api_v<tlm::cep::MesifProtocolEngine>,
              "mesif: 引擎暴露了消息级时序 API ⇒ 协议语义层已有时序模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_hop_api_v<tlm::cep::MoesiGpuProtocolEngine>,
              "moesi_gpu: 引擎暴露了 hop 计数 API ⇒ 协议语义层已有 hop 模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_timing_api_v<tlm::cep::MoesiGpuProtocolEngine>,
              "moesi_gpu: 引擎暴露了消息级时序 API ⇒ 协议语义层已有时序模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_hop_api_v<tlm::cep::TileLinkProtocolEngine>,
              "tilelink_c: 引擎暴露了 hop 计数 API ⇒ 协议语义层已有 hop 模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_timing_api_v<tlm::cep::TileLinkProtocolEngine>,
              "tilelink_c: 引擎暴露了消息级时序 API ⇒ 协议语义层已有时序模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_hop_api_v<tlm::cep::UalinkProtocolEngine>,
              "ualink_4: 引擎暴露了 hop 计数 API ⇒ 协议语义层已有 hop 模型 ⇒ 须走决策 + 更新声明");
static_assert(!has_timing_api_v<tlm::cep::UalinkProtocolEngine>,
              "ualink_4: 引擎暴露了消息级时序 API ⇒ 协议语义层已有时序模型 ⇒ 须走决策 + 更新声明");

// 正控补强：探针本身**有牙**（对**确实**暴露该 API 的类型必须为 true）
// ⇒ 用一个**合成**类型做正控（⛔ 不污染任何协议头）
namespace probe_fixture {
    struct WithHop {
        int last_hops() const { return 0; }
    };
    struct WithTiming {
        int latency_cycles() const { return 0; }
    };
    struct WithNothing {
        int unrelated() const { return 0; }
    };
}  // namespace probe_fixture

static_assert(has_hop_api_v<probe_fixture::WithHop>,
              "probe 正控:含 last_hops() 的类型必须被探针命中（探针无牙 ⇒ 判据假绿）");
static_assert(!has_hop_api_v<probe_fixture::WithNothing>,
              "probe 负控:无关类型不得被误判为有 hop API");
static_assert(has_timing_api_v<probe_fixture::WithTiming>,
              "probe 正控:含 latency_cycles() 的类型必须被探针命中（探针无牙 ⇒ 判据假绿）");
static_assert(!has_timing_api_v<probe_fixture::WithNothing>,
              "probe 负控:无关类型不得被误判为有时序 API");

// ══════════════════════════════════════════════════════════════════════════
//  J-8（编译期）：诚实边界的**机器可读**常量
// ══════════════════════════════════════════════════════════════════════════
static_assert(kTickIsSteppingHookNotTimingModel,
              "tick() 是全局步进钩子 ⛔ 不是消息级时序模型 ⇒ 该诚实边界常量必须为 true");
static_assert(kHopTimingAbsenceIsProtocolSemanticsOnly,
              "载体 / 器件层已有 hop 与时延 ⇒「无 hop / 时序」仅对协议语义层成立 ⇒ 常量必须为 true");
static_assert(kProtocolSemanticsHasNoHopOrTimingModel,
              "协议语义层现状恒无 hop / 时序模型 ⇒ 该现状锁常量必须为 true");

/// 读文件全部内容（失败 ⇒ 空串）。
std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// 文件是否含某 token。
bool file_contains(const std::filesystem::path& p, const std::string& token) {
    const std::string content = read_file(p);
    return content.find(token) != std::string::npos;
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP hop_timing: 声明自洽（basis 非空 / id 唯一 / 协议语义层恒 None）",
          "[cep][hop_timing]") {
    REQUIRE(kHopTimingCapabilityCount == 8);
    CHECK(hop_timing_capabilities_are_consistent(kHopTimingCapabilities.data(),
                                                  kHopTimingCapabilityCount));

    for (std::size_t i = 0; i < kHopTimingCapabilityCount; ++i) {
        INFO("row " << i << " id=" << kHopTimingCapabilities[i].id);
        REQUIRE(kHopTimingCapabilities[i].id != nullptr);
        REQUIRE(kHopTimingCapabilities[i].id[0] != '\0');
        REQUIRE(kHopTimingCapabilities[i].basis != nullptr);
        REQUIRE(kHopTimingCapabilities[i].basis[0] != '\0');
        // J-3 现状锁：协议语义层恒 None
        CHECK(kHopTimingCapabilities[i].protocol_hop == HopModel::None);
        CHECK(kHopTimingCapabilities[i].protocol_timing == TimingModel::None);
    }
}

TEST_CASE("CEP hop_timing: 谓词变异副本负控", "[cep][hop_timing]") {
    REQUIRE(hop_timing_capabilities_are_consistent(kHopTimingCapabilities.data(),
                                                    kHopTimingCapabilityCount));

    SECTION("basis 置空 ⇒ 必假") {
        auto copy = kHopTimingCapabilities;
        copy[3].basis = "";
        CHECK_FALSE(hop_timing_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("basis = nullptr ⇒ 必假") {
        auto copy = kHopTimingCapabilities;
        copy[0].basis = nullptr;
        CHECK_FALSE(hop_timing_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("id 重复 ⇒ 必假") {
        auto copy = kHopTimingCapabilities;
        copy[5].id = copy[2].id;
        CHECK_FALSE(hop_timing_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("id 置空 ⇒ 必假") {
        auto copy = kHopTimingCapabilities;
        copy[1].id = "";
        CHECK_FALSE(hop_timing_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("协议语义层 hop ≠ None ⇒ 必假（J-3 现状锁）") {
        auto copy = kHopTimingCapabilities;
        copy[2].protocol_hop = HopModel::CounterOnly;
        CHECK_FALSE(hop_timing_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("协议语义层 timing ≠ None ⇒ 必假（J-3 现状锁）") {
        auto copy = kHopTimingCapabilities;
        copy[4].protocol_timing = TimingModel::ParameterizedDelay;
        CHECK_FALSE(hop_timing_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("空表 ⇒ 必假") {
        CHECK_FALSE(hop_timing_capabilities_are_consistent(nullptr, 0));
    }
}

TEST_CASE("CEP hop_timing: 实例集双向零 orphan（J-1）", "[cep][hop_timing]") {
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
    for (std::size_t i = 0; i < kHopTimingCapabilityCount; ++i)
        decl_ids.insert(kHopTimingCapabilities[i].id);

    for (const auto& id : json_ids) {
        INFO("missing in kHopTimingCapabilities: " << id);
        CHECK(decl_ids.count(id) == 1);  // hop-timing-orphan-missing
    }
    for (const auto& id : decl_ids) {
        INFO("extra in kHopTimingCapabilities: " << id);
        CHECK(json_ids.count(id) == 1);  // hop-timing-orphan-extra
    }
    CHECK(json_ids.size() == decl_ids.size());
    CHECK(json_ids.size() == kHopTimingCapabilityCount);
}

TEST_CASE("CEP hop_timing: 枚举名映射（hop / timing）", "[cep][hop_timing]") {
    using tlm::cep::hop_model_name;
    using tlm::cep::timing_model_name;
    CHECK(std::string(hop_model_name(HopModel::None)) == "none");
    CHECK(std::string(hop_model_name(HopModel::CounterOnly)) == "counter_only");
    CHECK(std::string(timing_model_name(TimingModel::None)) == "none");
    CHECK(std::string(timing_model_name(TimingModel::ParameterizedDelay)) == "parameterized_delay");
}

TEST_CASE("CEP hop_timing: 载体 / 器件层既有 hop / 时延事实仍在（J-6 · 防声明失真）",
          "[cep][hop_timing]") {
    // ⚠️ 本用例守的是**「载体 / 器件层已有 hop / 时延」**这一事实 ⇒ 被删 ⇒ 必红
    //    （⇒ 声明面必须同步更新，而不是默默失真）。
    const auto cep_dir = repo_root() / "include" / "tlm" / "cep";

    // ① 载体层：flit 的 hop 计数字段 + NIC 侧回读
    CHECK(file_contains(cep_dir / "cohif_nic.hh", "hops"));
    CHECK(file_contains(cep_dir / "cohif_nic.hh", "last_recv_hops"));
    // ② 器件层：responder 的固定完成延迟
    CHECK(file_contains(cep_dir / "cohif_responder.hh", "completion_latency_cycles"));
    // ③ 器件层：桩缓存核的固定 snoop 延迟
    CHECK(file_contains(cep_dir / "stub_cache_core.hh", "snoop_latency_cycles"));
}
