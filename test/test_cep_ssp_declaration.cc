// test/test_cep_ssp_declaration.cc
// CEP SSP 声明面（批 1）—— 判据本体：
//   ★ V-24-2 killer：`ssps.json` 的 MESIF SSP **逐格**等于 `mesif_state_spec.hh` 的 `kTable`
//     （state/event 数 · transition 目标 · perm 投影 · 逐格 basis.kind / pending · 投影 basis）·
//     **双向**（任一侧改一格 ⇒ 必红）
//   + 14 核心锚封闭 / 逐字名 / flow 映射（部分映射的诚实边界）
//   + loader fail-closed 红因签名（E1..E12 抽样）
//
// 作者：CppTLM Team / 日期：2026-11-26
#include "catch_amalgamated.hpp"

#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "cep_adr_parser.hh"          // repo_root()
#include "tlm/cep/event_alphabet.hh"
#include "tlm/cep/mesif_state_spec.hh"
#include "tlm/cep/ssp_loader.hh"
#include "tlm/cep/protocol_engine_base.hh"  // kCanonicalFlowNames（交叉核对用）

namespace {

using tlm::cep::MesifStateSpec;
namespace ssp = tlm::cep::ssp;

nlohmann::json load_ssps_doc() {
    const auto p = cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "ssps.json";
    std::ifstream in(p);
    REQUIRE(in.good());
    nlohmann::json j;
    in >> j;
    return j;
}

const nlohmann::json& mesif_ssp(const nlohmann::json& doc) {
    for (const auto& s : doc["ssps"])
        if (s.value("id", "") == "mesif_ssp") return s;
    FAIL("ssps.json 缺 mesif_ssp");
    return doc["ssps"][0];
}

const char* state_name(std::uint8_t v) {
    switch (v) {
        case 0: return "I";
        case 1: return "S";
        case 2: return "E";
        case 3: return "M";
        case 4: return "F";
        default: return "<hole>";
    }
}

const char* perm_name(MesifStateSpec::Perm p) {
    switch (p) {
        case MesifStateSpec::Perm::NONE: return "none";
        case MesifStateSpec::Perm::READ: return "read";
        case MesifStateSpec::Perm::UNIQUE: return "unique";
        case MesifStateSpec::Perm::WRITEBACK: return "writeback";
    }
    return "<bad>";
}

const char* bk_name(MesifStateSpec::BasisKind k) {
    switch (k) {
        case MesifStateSpec::BasisKind::Primary: return "primary";
        case MesifStateSpec::BasisKind::Structural: return "structural";
        case MesifStateSpec::BasisKind::Analogical: return "analogical";
        case MesifStateSpec::BasisKind::Derived: return "derived";
    }
    return "<bad>";
}

} // namespace

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("[cep][ssp] ssps.json 通过 loader 校验（A-2 的基线正控）",
          "[cep][ssp]") {
    const auto doc = load_ssps_doc();
    const auto r = ssp::check_ssp_document(doc);
    INFO("red code=" << r.code << " detail=" << r.detail);
    REQUIRE(r.ok);
}

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("[cep][ssp] ★ V-24-2 killer —— MESIF SSP 与 kTable 逐格相等（双向）",
          "[cep][ssp]") {
    const auto doc = load_ssps_doc();
    const auto& s = mesif_ssp(doc);
    const auto& T = MesifStateSpec::kTable;

    constexpr std::size_t NS = MesifStateSpec::kStateCount;   // 5
    constexpr std::size_t NE = MesifStateSpec::kEventCount;   // 6

    // ① 维度
    REQUIRE(s["states"].size() == NS);
    REQUIRE(s["events"].size() == NE);
    REQUIRE(s["perm"].size() == NS);
    REQUIRE(s["transition"].size() == NS);
    REQUIRE(s["basis_kind"].size() == NS);
    REQUIRE(s["basis_pending"].size() == NS);
    REQUIRE(s["basis_projection_kind"].size() == NS);
    REQUIRE(s["basis_projection_pending"].size() == NS);

    // ② state 名逐字（顺序须与枚举值序一致：I=0 S=1 E=2 M=3 F=4）
    for (std::size_t i = 0; i < NS; ++i) {
        INFO("state[" << i << "]");
        REQUIRE(s["states"][i].get<std::string>() == state_name(static_cast<std::uint8_t>(i)));
    }

    // ③ 转移逐格（双向：JSON vs kTable）
    for (std::size_t i = 0; i < NS; ++i) {
        for (std::size_t j = 0; j < NE; ++j) {
            INFO("transition[" << i << "][" << j << "]");
            const auto want = std::string(state_name(T.transition[i][j]));
            const auto& cell = s["transition"][i][j];
            if (want == "<hole>") {
                REQUIRE(cell.is_null());
            } else {
                REQUIRE(cell.is_string());
                REQUIRE(cell.get<std::string>() == want);
            }
        }
    }

    // ④ perm 投影逐格
    for (std::size_t i = 0; i < NS; ++i) {
        INFO("perm[" << i << "]");
        REQUIRE(s["perm"][i].get<std::string>() == perm_name(T.projection[i]));
    }

    // ⑤ 逐格 basis.kind / pending（30 格）
    for (std::size_t i = 0; i < NS; ++i) {
        for (std::size_t j = 0; j < NE; ++j) {
            INFO("basis[" << i << "][" << j << "]");
            REQUIRE(s["basis_kind"][i][j].get<std::string>() == bk_name(T.transition_basis[i][j].kind));
            REQUIRE(s["basis_pending"][i][j].get<bool>() == T.transition_basis[i][j].pending);
        }
    }

    // ⑥ 投影 basis（5 格）
    for (std::size_t i = 0; i < NS; ++i) {
        INFO("basis_projection[" << i << "]");
        REQUIRE(s["basis_projection_kind"][i].get<std::string>() == bk_name(T.projection_basis[i].kind));
        REQUIRE(s["basis_projection_pending"][i].get<bool>() == T.projection_basis[i].pending);
    }
}

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("[cep][ssp] V-24-2 反向：JSON 侧改一格 ⇒ 与 kTable 不等（判别力见证）",
          "[cep][ssp]") {
    auto doc = load_ssps_doc();
    // 把 [0][0]（I + Rd → S）改成 I ⇒ 与 kTable 的 S 不等
    doc["ssps"][0]["transition"][0][0] = "I";
    const auto& T = MesifStateSpec::kTable;
    REQUIRE(doc["ssps"][0]["transition"][0][0].get<std::string>() !=
            std::string(state_name(T.transition[0][0])));
}

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("[cep][ssp] 14 核心锚封闭 + 逐字名 + flow 映射（部分映射的诚实边界）",
          "[cep][ssp]") {
    REQUIRE(ssp::kCoreAnchorCount == 14);
    REQUIRE(ssp::kCoreAnchors.size() == 14);

    std::set<std::string> names;
    for (const auto& a : ssp::kCoreAnchors) {
        INFO("anchor=" << a.name);
        REQUIRE(a.name != nullptr);
        REQUIRE(names.insert(a.name).second);   // 逐字名唯一
    }

    // 10 个有 flow 的锚，其 flow ∈ canonical 10 流（与 kCanonicalFlowNames 交叉核对）
    std::set<std::string> canonical(std::begin(tlm::cep::kCanonicalFlowNames),
                                    std::end(tlm::cep::kCanonicalFlowNames));
    int with_flow = 0, without_flow = 0;
    for (const auto& a : ssp::kCoreAnchors) {
        if (a.flow == nullptr) { ++without_flow; continue; }
        ++with_flow;
        INFO("anchor " << a.name << " flow=" << a.flow);
        REQUIRE(canonical.count(a.flow) == 1);
    }
    // 诚实边界：TileLink 的 4 个权限转移锚无 flow
    REQUIRE(with_flow == 10);
    REQUIRE(without_flow == 4);
    REQUIRE(ssp::kFlowMappingIsPartial);
}

// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("[cep][ssp] loader fail-closed —— 红因签名 E1..E12（抽样正向）",
          "[cep][ssp]") {
    auto base = load_ssps_doc();

    auto expect_code = [](nlohmann::json d, const char* code) {
        const auto r = ssp::check_ssp_document(d);
        INFO("want=" << code << " got=" << r.code << " detail=" << r.detail);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.code == code);
    };

    SECTION("E1 schema 字面量") {
        auto d = base; d["schema"] = "wrong/v0"; expect_code(d, "E1");
    }
    SECTION("E2 id 形态非法") {
        auto d = base; d["ssps"][0]["id"] = "Bad-Id"; expect_code(d, "E2");
    }
    SECTION("E2 空 ssps 数组（退化）") {
        auto d = base; d["ssps"] = nlohmann::json::array(); expect_code(d, "E2");
    }
    SECTION("E3 state 重名") {
        auto d = base; d["ssps"][0]["states"][1] = "I"; expect_code(d, "E3");
    }
    SECTION("E3 空 states 数组（退化）") {
        auto d = base; d["ssps"][0]["states"] = nlohmann::json::array(); expect_code(d, "E3");
    }
    SECTION("E4 event 重名") {
        auto d = base; d["ssps"][0]["events"][1]["name"] = "Rd"; expect_code(d, "E4");
    }
    SECTION("E5 transition 行数不符") {
        auto d = base; d["ssps"][0]["transition"].erase(d["ssps"][0]["transition"].begin());
        expect_code(d, "E5");
    }
    SECTION("E6 目标态未声明") {
        auto d = base; d["ssps"][0]["transition"][0][0] = "ZZZ"; expect_code(d, "E6");
    }
    SECTION("E7 anchor 非核心锚") {
        auto d = base; d["ssps"][0]["events"][0]["anchor"] = "NotAnAnchor"; expect_code(d, "E7");
    }
    SECTION("E8 flow 非 10 流") {
        auto d = base;
        d["ssps"][0]["events"][0] = {{"name", "Rd"}, {"flow", "not_a_flow"}, {"architect_approval", "x"}};
        expect_code(d, "E8");
    }
    SECTION("E9 第 5 个 perm 值") {
        auto d = base; d["ssps"][0]["perm"][0] = "exclusive"; expect_code(d, "E9");
    }
    SECTION("E10 basis_kind 非法") {
        auto d = base; d["ssps"][0]["basis_kind"][0][0] = "guessed"; expect_code(d, "E10");
    }
    SECTION("E11 basis_pending 非 bool") {
        auto d = base; d["ssps"][0]["basis_pending"][0][0] = 1; expect_code(d, "E11");
    }
    SECTION("E12 basis_projection 长度不符") {
        auto d = base; d["ssps"][0]["basis_projection_kind"].erase(
            d["ssps"][0]["basis_projection_kind"].begin());
        expect_code(d, "E12");
    }
    SECTION("E7 私有事件缺 architect_approval") {
        auto d = base;
        d["ssps"][0]["events"][0] = {{"name", "Rd"}, {"flow", "request"}};
        expect_code(d, "E7");
    }
    SECTION("E13 transition 全为空洞（退化 SSP）") {
        auto d = base;
        for (auto& row : d["ssps"][0]["transition"])
            for (auto& cell : row)
                cell = nullptr;
        expect_code(d, "E13");
    }
}
