// test/test_cep_fence_effects.cc
// 「fence 一致性效果」派生声明面 —— 判据测试
// per openspec/changes/cep-fence-consistency-effect-derivation/spec.md
// 作者: CppTLM Team / 日期: 2026-11-24
//
// 判据纪律：每条 = 基线正控绿 ⇒ 破坏 ⇒ 按正确原因红 ⇒ 还原 ⇒ 必须绿。
// 本文件承担：
//   · AC-1 推导函数存在性（编译期：逐行 effect == derive(行输入)）
//   · AC-2 派生正确性（**独立 oracle**：硬编码期望表 ⇄ 编译出的派生值 · ⛔ 不自证）
//   · AC-3 输入副本漂移（头文件副本 ⇄ protocol_instances.json 逐实例逐字段）
//   · AC-5 JSON 缺键必红（运行期：缺键 ⇒ 判定失败 · ⛔ 不静默跳过）
//   · AC-6 实例集双向零 orphan
//   · AC-7 basis 非空 + id 唯一 + 自洽谓词变异副本负控
//   · AC-8 死分支正控（TSO / PLO / RC-acquire-only / 跨族 / 未知标签）
//   · AC-9 诚实边界常量
// 反向 fixture（域外标签组合 / JSON 副本漂移 ⇒ 闸级变红）在
// `scripts/test/cep_fence_effects_check.sh`。

#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh" // repo_root()

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "tlm/cep/fence_effects.hh"

using cpptlm::cep::repo_root;

namespace {

using tlm::cep::ConsistencyFamily;
using tlm::cep::FenceEffect;
using tlm::cep::FenceEffectCapability;
using tlm::cep::consistency_family_of;
using tlm::cep::derive_fence_effect;
using tlm::cep::fence_effect_capabilities_are_consistent;
using tlm::cep::fence_effect_name;
using tlm::cep::kBarrierAbsenceMeansNoFenceEffect;
using tlm::cep::kFenceEffectCapabilities;
using tlm::cep::kFenceEffectCapabilityCount;
using tlm::cep::kFenceEffectGranularityIsPerInstance;
using tlm::cep::kFenceEffectIsDeclarationOnly;
using tlm::cep::kFenceEffectIsNotCrossDomainGuarantee;
using tlm::cep::kRcEffectIsPaperUpperBoundSubset;
using tlm::cep::kScModelMeansNoExtraFenceEffect;

// ══════════════════════════════════════════════════════════════════════════
//  独立 oracle（**硬编码期望表** · ⛔ **不由** derive_fence_effect 生成）
//  ⇒ 规则实现写错 ⇒ 本表与派生值不等 ⇒ 编译/测试红（⛔ 非自证）
//  依据：台账 Entry #544 ② 的推导规则表 R1–R7 + 论文四行表
// ══════════════════════════════════════════════════════════════════════════
struct ExpectedEffect {
    const char* id;
    FenceEffect effect;
};

constexpr ExpectedEffect kExpectedEffects[8] = {
    {"chi_rnf", FenceEffect::None},              // R1 · 无 barrier 流
    {"cxl_2_0", FenceEffect::None},              // R1 · 无 barrier 流
    {"cxl_3_0_symmetric", FenceEffect::None},    // R1 · 无 barrier 流
    {"mesif", FenceEffect::ReleaseAcquire},      // R5 · RC 族 ⇒ release+acquire（模型基线）
    {"moesi_gpu", FenceEffect::ReleaseAcquire},  // R5 · 同上（标签 RC-data / RC-release 不影响结论）
    {"nvlink_4", FenceEffect::None},             // R3 · SC 已全序（有通道但空操作）
    {"tilelink_c", FenceEffect::None},           // R1 · 无 barrier 流
    {"ualink_4", FenceEffect::None},             // R1 · 无 barrier 流（遮蔽 R6）
};

// ══════════════════════════════════════════════════════════════════════════
//  AC-1（编译期）：逐行 effect == derive(行输入)
//  ⚠️ 失败签名须含**行 id** ⇒ 每行单独写。
// ══════════════════════════════════════════════════════════════════════════
static_assert(kFenceEffectCapabilities[0].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[0].read_label,
                                      kFenceEffectCapabilities[0].write_label,
                                      kFenceEffectCapabilities[0].has_barrier),
              "chi_rnf: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");
static_assert(kFenceEffectCapabilities[1].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[1].read_label,
                                      kFenceEffectCapabilities[1].write_label,
                                      kFenceEffectCapabilities[1].has_barrier),
              "cxl_2_0: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");
static_assert(kFenceEffectCapabilities[2].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[2].read_label,
                                      kFenceEffectCapabilities[2].write_label,
                                      kFenceEffectCapabilities[2].has_barrier),
              "cxl_3_0_symmetric: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");
static_assert(kFenceEffectCapabilities[3].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[3].read_label,
                                      kFenceEffectCapabilities[3].write_label,
                                      kFenceEffectCapabilities[3].has_barrier),
              "mesif: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");
static_assert(kFenceEffectCapabilities[4].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[4].read_label,
                                      kFenceEffectCapabilities[4].write_label,
                                      kFenceEffectCapabilities[4].has_barrier),
              "moesi_gpu: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");
static_assert(kFenceEffectCapabilities[5].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[5].read_label,
                                      kFenceEffectCapabilities[5].write_label,
                                      kFenceEffectCapabilities[5].has_barrier),
              "nvlink_4: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");
static_assert(kFenceEffectCapabilities[6].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[6].read_label,
                                      kFenceEffectCapabilities[6].write_label,
                                      kFenceEffectCapabilities[6].has_barrier),
              "tilelink_c: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");
static_assert(kFenceEffectCapabilities[7].effect ==
                  derive_fence_effect(kFenceEffectCapabilities[7].read_label,
                                      kFenceEffectCapabilities[7].write_label,
                                      kFenceEffectCapabilities[7].has_barrier),
              "ualink_4: 行的 effect 与推导函数结果不一致 ⇒ 表行不是由函数生成");

// ══════════════════════════════════════════════════════════════════════════
//  AC-8（编译期）：**死分支正控** —— TSO / PLO 当前**无实例**
//  ⇒ 用**合成行**覆盖（不进实例表 ⇒ 不破坏 AC-6 的双向零 orphan）
// ══════════════════════════════════════════════════════════════════════════
static_assert(derive_fence_effect("TSO", "TSO", true) == FenceEffect::FullBarrier,
              "TSO 分支正控: TSO 标签 + barrier ⇒ 必须是全屏障（当前无实例 ⇒ 靠合成行覆盖）");
static_assert(derive_fence_effect("PLO", "PLO", true) == FenceEffect::Partial,
              "PLO 分支正控: PLO 标签 + barrier ⇒ 必须是部分效果（当前无实例 ⇒ 靠合成行覆盖）");
static_assert(derive_fence_effect("RC-acquire", "RC-data", true) == FenceEffect::ReleaseAcquire,
              "RC 模型基线正控: RC 族 ⇒ release+acquire（⛔ 不由访问标签门控）");
static_assert(derive_fence_effect("RC-data", "RC-data", true) == FenceEffect::ReleaseAcquire,
              "RC 标签无关性正控: 读写皆 RC-data ⇒ 仍为 release+acquire（模型基线口径的判别力所在）");
static_assert(derive_fence_effect("SC", "SC", true) == FenceEffect::None,
              "SC 分支正控: SC 已全序 ⇒ fence 无额外效果（与 barrier 存在与否无关）");
static_assert(derive_fence_effect("PLO", "PLO", false) == FenceEffect::None,
              "R1 优先级正控: 无 barrier 通道 ⇒ 遮蔽模型规则（PLO ⇒ None 而非 Partial）");

/// 域外组合（⛔ 必须返回 Invalid ⇒ 闸级域外必红）
static_assert(derive_fence_effect("SC", "TSO", true) == FenceEffect::Invalid,
              "跨族组合正控: SC 读 + TSO 写 ⇒ 必须域外（Invalid）");
static_assert(derive_fence_effect("XYZ", "XYZ", true) == FenceEffect::Invalid,
              "未知标签正控: 不在枚举内的标签 ⇒ 必须域外（Invalid）");

/// 标签 → 族（唯一映射点）
static_assert(consistency_family_of("SC") == ConsistencyFamily::Sc, "SC 族");
static_assert(consistency_family_of("TSO") == ConsistencyFamily::Tso, "TSO 族");
static_assert(consistency_family_of("RC-acquire") == ConsistencyFamily::Rc, "RC 族（acquire）");
static_assert(consistency_family_of("RC-data") == ConsistencyFamily::Rc, "RC 族（data）");
static_assert(consistency_family_of("RC-release") == ConsistencyFamily::Rc, "RC 族（release）");
static_assert(consistency_family_of("PLO") == ConsistencyFamily::Plo, "PLO 族");
static_assert(consistency_family_of("XYZ") == ConsistencyFamily::Unknown, "未知标签 ⇒ Unknown 族");

}  // namespace

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP fence_effects: 派生值 ⇄ 独立 oracle（硬编码期望表）",
          "[cep][fence_effects]") {
    REQUIRE(kFenceEffectCapabilityCount == 8);
    REQUIRE(std::size(kExpectedEffects) == kFenceEffectCapabilityCount);

    for (std::size_t i = 0; i < kFenceEffectCapabilityCount; ++i) {
        const FenceEffectCapability& row = kFenceEffectCapabilities[i];
        INFO("row " << i << " id=" << row.id);
        REQUIRE(std::string_view(row.id) == std::string_view(kExpectedEffects[i].id));
        // ⛔ 期望值来自**硬编码 oracle** ⇒ 规则实现写错 ⇒ 此处红（非自证）
        CHECK(row.effect == kExpectedEffects[i].effect);
    }
}

TEST_CASE("CEP fence_effects: 声明自洽（basis 非空 / id 唯一 / 无域外行）",
          "[cep][fence_effects]") {
    REQUIRE(fence_effect_capabilities_are_consistent(kFenceEffectCapabilities.data(),
                                                      kFenceEffectCapabilityCount));
    for (std::size_t i = 0; i < kFenceEffectCapabilityCount; ++i) {
        INFO("row " << i << " id=" << kFenceEffectCapabilities[i].id);
        REQUIRE(kFenceEffectCapabilities[i].id != nullptr);
        REQUIRE(kFenceEffectCapabilities[i].id[0] != '\0');
        REQUIRE(kFenceEffectCapabilities[i].basis != nullptr);
        REQUIRE(kFenceEffectCapabilities[i].basis[0] != '\0');
        REQUIRE(kFenceEffectCapabilities[i].read_label != nullptr);
        REQUIRE(kFenceEffectCapabilities[i].read_label[0] != '\0');
        REQUIRE(kFenceEffectCapabilities[i].write_label != nullptr);
        REQUIRE(kFenceEffectCapabilities[i].write_label[0] != '\0');
        CHECK(kFenceEffectCapabilities[i].effect != FenceEffect::Invalid);
    }
}

TEST_CASE("CEP fence_effects: 自洽谓词变异副本负控", "[cep][fence_effects]") {
    SECTION("basis 置空 ⇒ 必假") {
        auto copy = kFenceEffectCapabilities;
        copy[3].basis = "";
        CHECK_FALSE(fence_effect_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("id 置空 ⇒ 必假") {
        auto copy = kFenceEffectCapabilities;
        copy[0].id = "";
        CHECK_FALSE(fence_effect_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("read_label 置空 ⇒ 必假") {
        auto copy = kFenceEffectCapabilities;
        copy[1].read_label = "";
        CHECK_FALSE(fence_effect_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("id 重复 ⇒ 必假") {
        auto copy = kFenceEffectCapabilities;
        copy[5].id = copy[2].id;
        CHECK_FALSE(fence_effect_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("域外行（Invalid）⇒ 必假") {
        auto copy = kFenceEffectCapabilities;
        copy[4].effect = FenceEffect::Invalid;
        CHECK_FALSE(fence_effect_capabilities_are_consistent(copy.data(), copy.size()));
    }
    SECTION("空表 ⇒ 必假") {
        CHECK_FALSE(fence_effect_capabilities_are_consistent(nullptr, 0));
    }
}

TEST_CASE("CEP fence_effects: 输入副本漂移 ⇄ protocol_instances.json（AC-3 / AC-5）",
          "[cep][fence_effects]") {
    const auto path = (repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json doc;
    in >> doc;
    REQUIRE(doc.contains("instances"));

    for (const auto& inst : doc.at("instances")) {
        const std::string id = inst.at("id").get<std::string>();
        INFO("instance id=" << id);

        // AC-5 · 缺键 ⇒ **必红**（⛔ 不静默跳过）
        REQUIRE(inst.contains("consistency_labels"));
        REQUIRE(inst.contains("flow_support"));
        const auto& cl = inst.at("consistency_labels");
        const auto& fs = inst.at("flow_support");
        REQUIRE(cl.contains("read"));
        REQUIRE(cl.contains("write"));
        REQUIRE(fs.contains("barrier"));

        // 找对应行
        const FenceEffectCapability* row = nullptr;
        for (std::size_t i = 0; i < kFenceEffectCapabilityCount; ++i) {
            if (std::string_view(kFenceEffectCapabilities[i].id) == std::string_view(id)) {
                row = &kFenceEffectCapabilities[i];
                break;
            }
        }
        REQUIRE(row != nullptr);   // 缺声明 ⇒ 必红

        // AC-3 · 副本漂移 ⇒ 必红（逐字段）
        CHECK(std::string_view(row->read_label) == std::string_view(cl.at("read").get<std::string>()));
        CHECK(std::string_view(row->write_label) == std::string_view(cl.at("write").get<std::string>()));
        CHECK(row->has_barrier == fs.at("barrier").get<bool>());
    }
}

TEST_CASE("CEP fence_effects: 实例集双向零 orphan（AC-6）", "[cep][fence_effects]") {
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
    for (std::size_t i = 0; i < kFenceEffectCapabilityCount; ++i)
        decl_ids.insert(kFenceEffectCapabilities[i].id);

    for (const auto& id : json_ids) {
        INFO("missing in kFenceEffectCapabilities: " << id);
        CHECK(decl_ids.count(id) == 1);
    }
    for (const auto& id : decl_ids) {
        INFO("extra in kFenceEffectCapabilities: " << id);
        CHECK(json_ids.count(id) == 1);
    }
    CHECK(json_ids.size() == decl_ids.size());
    CHECK(json_ids.size() == kFenceEffectCapabilityCount);
}

TEST_CASE("CEP fence_effects: 诚实边界常量（AC-9）", "[cep][fence_effects]") {
    static_assert(kFenceEffectIsDeclarationOnly,
                  "派生值 = 声明效果 ⛔ 不是引擎行为语义 ⇒ 该边界常量必须为 true");
    static_assert(kFenceEffectIsNotCrossDomainGuarantee,
                  "派生值 ⛔ 不是跨一致性域的 barrier 保证 ⇒ 该边界常量必须为 true");
    static_assert(kBarrierAbsenceMeansNoFenceEffect,
                  "无 barrier 通道 ⇒ fence 无效果（R1）⇒ 该边界常量必须为 true");
    static_assert(kScModelMeansNoExtraFenceEffect,
                  "SC 已全序 ⇒ fence 无额外效果（R3）⇒ 该边界常量必须为 true");
    static_assert(kRcEffectIsPaperUpperBoundSubset,
                  "RC 族取论文并集上界的子集 ⇒ 该边界常量必须为 true");
    static_assert(kFenceEffectGranularityIsPerInstance,
                  "派生粒度 = 实例 ⇒ 该边界常量必须为 true");

    CHECK(std::string(fence_effect_name(FenceEffect::None)) == "none");
    CHECK(std::string(fence_effect_name(FenceEffect::ReleaseOnly)) == "release_only");
    CHECK(std::string(fence_effect_name(FenceEffect::AcquireOnly)) == "acquire_only");
    CHECK(std::string(fence_effect_name(FenceEffect::ReleaseAcquire)) == "release_acquire");
    CHECK(std::string(fence_effect_name(FenceEffect::FullBarrier)) == "full_barrier");
    CHECK(std::string(fence_effect_name(FenceEffect::Partial)) == "partial");
    CHECK(std::string(fence_effect_name(FenceEffect::Invalid)) == "invalid");
}
