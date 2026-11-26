// test/test_cep_ssp_runtime.cc
// CEP SSP 运行时（批 2 `cep-ssp-batch2`）—— ⑦ 解释器 + ⑨ 掩码格扩展
//
// 覆盖:
//   ⑨ 掩码格扩展（contract 面）—— `transition` 单元支持 `{"target_mask": N}` 两形态
//      · 正控:掩码形态**合法** ⇒ loader 不红
//      · 反控:掩码格**非法**形态 ⇒ 红因码 `E14`（缺键 / 多键 / 非整数 / 负 / 越位宽）
//      · ★ V-24-2 两形态 killer:掩码形态的 SSP 与 `tilelink_state_spec.hh` 的
//        `kTransitions` **逐位**相等（转置对齐:手写面 [event][state] ↔ SSP [state][event]）
//      · 反向 fixture:JSON 侧改一位 ⇒ 与手写面不等（判别力见证）
//   ⑦ functional-only 解释器 —— 状态迁移 + perm 投影 + **洞命中违例计数**
//
// ⚠️ **零时序**:解释器输出结构**不承载**任何时长 / 延迟 / hop（属 S2 `ADR-CEP-25`）。
//
// 作者: CppTLM Team / 日期: 2026-11-27
#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/ssp_interpreter.hh"
#include "tlm/cep/ssp_loader.hh"
#include "tlm/cep/tilelink_state_spec.hh"

namespace {

using tlm::cep::TileLinkStateSpec;
namespace ssp = tlm::cep::ssp;

nlohmann::json load_ssps_doc() {
    const std::string path = std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/ssps.json";
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json j;
    in >> j;
    return j;
}

const nlohmann::json& find_ssp(const nlohmann::json& doc, const std::string& id) {
    for (const auto& s : doc["ssps"])
        if (s.value("id", "") == id) return s;
    FAIL("ssps.json 缺 " << id);
    return doc["ssps"][0];
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════════
// ⑨ 掩码格扩展（contract 面）
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("[cep][ssp][mask] transition 单元支持掩码形(target_mask)⇒ 合法不红",
          "[cep][ssp][mask]") {
    auto doc = load_ssps_doc();
    // 把 `mesif_ssp` 的 [0][0]（I + Rd → S）换成掩码对象
    // ⚠️ `mesif_ssp` 有 5 态 ⇒ 掩码位宽上限 = 0b11111 = 31
    doc["ssps"][0]["transition"][0][0] = {{"target_mask", 3}};
    const auto r = ssp::check_ssp_document(doc);
    INFO("red code=" << r.code << " detail=" << r.detail);
    REQUIRE(r.ok);
}

TEST_CASE("[cep][ssp][mask] 掩码格的非法形态 ⇒ 红因码 E14", "[cep][ssp][mask]") {
    auto doc = load_ssps_doc();

    SECTION("缺 target_mask") {
        doc["ssps"][0]["transition"][0][0] = {{"mask", 3}};
        const auto r = ssp::check_ssp_document(doc);
        INFO("red code=" << r.code << " detail=" << r.detail);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.code == "E14");
    }
    SECTION("带多余键") {
        doc["ssps"][0]["transition"][0][0] = {{"target_mask", 3}, {"extra", 1}};
        const auto r = ssp::check_ssp_document(doc);
        INFO("red code=" << r.code << " detail=" << r.detail);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.code == "E14");
    }
    SECTION("target_mask 非整数") {
        doc["ssps"][0]["transition"][0][0] = {{"target_mask", "3"}};
        const auto r = ssp::check_ssp_document(doc);
        INFO("red code=" << r.code << " detail=" << r.detail);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.code == "E14");
    }
    SECTION("target_mask 为负") {
        doc["ssps"][0]["transition"][0][0] = {{"target_mask", -1}};
        const auto r = ssp::check_ssp_document(doc);
        INFO("red code=" << r.code << " detail=" << r.detail);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.code == "E14");
    }
    SECTION("target_mask 越出 states 数的位宽(5 态 ⇒ 上限 31)") {
        doc["ssps"][0]["transition"][0][0] = {{"target_mask", 32}};
        const auto r = ssp::check_ssp_document(doc);
        INFO("red code=" << r.code << " detail=" << r.detail);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.code == "E14");
    }
}

// ══════════════════════════════════════════════════════════════════════════
// ★ V-24-2 两形态 killer —— 掩码形态与手写面逐位相等（双向）
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("[cep][ssp][mask] ★ V-24-2 两形态 —— 掩码形态与 kTransitions 逐位相等",
          "[cep][ssp][mask]") {
    const auto doc = load_ssps_doc();
    const auto& s = find_ssp(doc, "tilelink_cap_ssp");

    // ① 维度
    REQUIRE(s["states"].size() == TileLinkStateSpec::kStateCount);
    REQUIRE(s["events"].size() == TileLinkStateSpec::kEventCount);
    REQUIRE(s["transition"].size() == TileLinkStateSpec::kStateCount);

    // ② 态名 / 事件名逐字（顺序须与枚举值序一致 ⇒ 掩码 bit i ↔ states[i]）
    for (std::size_t i = 0; i < TileLinkStateSpec::kStateCount; ++i) {
        INFO("state[" << i << "]");
        REQUIRE(s["states"][i].get<std::string>() == TileLinkStateSpec::kStableStateNames[i]);
    }
    for (std::size_t j = 0; j < TileLinkStateSpec::kEventCount; ++j) {
        INFO("event[" << j << "]");
        REQUIRE(s["events"][j].value("name", "") == TileLinkStateSpec::kEventNames[j]);
    }

    // ③ 转移逐位（⚠️ **转置**:手写面 kTransitions[event][state] ↔ SSP transition[state][event]）
    for (std::size_t si = 0; si < TileLinkStateSpec::kStateCount; ++si) {
        for (std::size_t ei = 0; ei < TileLinkStateSpec::kEventCount; ++ei) {
            INFO("transition[" << si << "][" << ei << "]");
            const auto& cell = TileLinkStateSpec::kTransitions[ei][si];
            // ⚠️ 手写面此载体**全部**格为 `Known` ⇒ 掩码形可完整表达
            REQUIRE(cell.kind == TileLinkStateSpec::CellKind::Known);
            const auto& jcell = s["transition"][si][ei];
            REQUIRE(jcell.is_object());
            REQUIRE(jcell.contains("target_mask"));
            REQUIRE(jcell.at("target_mask").get<int>() == static_cast<int>(cell.target_mask));
        }
    }

    // ④ perm 投影逐格（4 值锁 · CEN-Ib11）
    for (std::size_t i = 0; i < TileLinkStateSpec::kStateCount; ++i) {
        INFO("perm[" << i << "]");
        const char* want = "none";
        switch (TileLinkStateSpec::kPermProjection[i]) {
            case TileLinkStateSpec::Perm::NONE: want = "none"; break;
            case TileLinkStateSpec::Perm::READ: want = "read"; break;
            case TileLinkStateSpec::Perm::UNIQUE: want = "unique"; break;
            case TileLinkStateSpec::Perm::WRITEBACK: want = "writeback"; break;
        }
        REQUIRE(s["perm"][i].get<std::string>() == want);
    }
}

TEST_CASE("[cep][ssp][mask] V-24-2 两形态反向:JSON 侧改一位 ⇒ 与手写面不等", "[cep][ssp][mask]") {
    auto doc = load_ssps_doc();
    // 把掩码形态的 [0][0]（Nothing + Cap）由 0b111 改成 0b110
    doc["ssps"][1]["transition"][0][0] = {{"target_mask", 6}};
    const auto& cell = TileLinkStateSpec::kTransitions[0][0];  // Cap × Nothing
    REQUIRE(doc["ssps"][1]["transition"][0][0].at("target_mask").get<int>() !=
            static_cast<int>(cell.target_mask));
}

// ══════════════════════════════════════════════════════════════════════════
// ⑦ functional-only 解释器
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("[cep][ssp][interp] 解释器做状态迁移 + perm 投影", "[cep][ssp][interp]") {
    const auto doc = load_ssps_doc();

    // `mesif_ssp`: 态 I →(Rd)→ S →(RdUnique)→ E
    const auto r = ssp::interpret_ssp(doc, "mesif_ssp", "I", std::vector<std::string>{"Rd", "RdUnique"});
    REQUIRE(r.ok);
    REQUIRE(r.steps.size() == 2u);

    // ① 每步:目标状态 + perm 投影（4 值锁）
    //    ⚠️ perm 投影取**目标状态**的投影 —— `mesif_ssp` 的 perm = [none, read, unique, writeback, writeback]
    REQUIRE(r.steps[0].source_state == "I");
    REQUIRE(r.steps[0].target_state == "S");
    REQUIRE(r.steps[0].perm == "read");      // perm[S]
    REQUIRE(r.steps[1].source_state == "S");
    REQUIRE(r.steps[1].target_state == "E");
    REQUIRE(r.steps[1].perm == "unique");    // perm[E]

    // ② 洞命中 = 0 ⇒ 该次实验**可判**
    REQUIRE(r.violation_count == 0u);
    REQUIRE(r.judgeable);
}

TEST_CASE("[cep][ssp][interp] 洞命中 ⇒ 显式违例计数 + 该次实验判「不可判」",
          "[cep][ssp][interp]") {
    auto doc = load_ssps_doc();
    // 制造一个洞：把 I + Rd 的目标改成 null（空洞）
    doc["ssps"][0]["transition"][0][0] = nullptr;

    const auto r = ssp::interpret_ssp(doc, "mesif_ssp", "I", std::vector<std::string>{"Rd"});
    // ⛔ 洞命中**不是**错误路径 ⇒ 不得 panic ⇒ `ok` 仍为 true
    REQUIRE(r.ok);
    REQUIRE(r.steps.size() == 1u);
    REQUIRE(r.steps[0].target_state.empty());   // 洞 ⇒ 无目标状态
    REQUIRE(r.violation_count == 1u);            // 显式计数
    REQUIRE_FALSE(r.judgeable);                 // 该次实验判「不可判」
}
