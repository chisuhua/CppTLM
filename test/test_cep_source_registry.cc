// test/test_cep_source_registry.cc
// 判据:来源类型可机器判(change cpptlm-cep-basis-source-kind-and-cell-free 的 AC-1~AC-4)。
//
// 覆盖:
//   SRK-01 4 载体逐格 ref 可解析(未登记 = 0)
//   SRK-02 按载体 basis ↔ ref_kind 映射 0 违规
//   SRK-03 sources.json ↔ kSources[] 逐条一致
//   SRK-04 CellKind::Free 四态互斥 + 当前 0 格 + adapter 显式分支
//
// 作者: builder  日期: 2026-11-06
#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "tlm/cep/cxl_cache_adapter.hh"
#include "tlm/cep/cxl_state_spec.hh"
#include "tlm/cep/mesif_state_spec.hh"
#include "tlm/cep/source_registry.hh"
#include "tlm/cep/tilelink_c_adapter.hh"
#include "tlm/cep/tilelink_state_spec.hh"
#include "tlm/cep/ualink_inc_state_spec.hh"

using namespace tlm::cep;

namespace {

template <typename Enum>
constexpr Enum as_enum(std::uint8_t v) noexcept {
    return static_cast<Enum>(v);
}

}  // namespace

TEST_CASE("SRK-01a: tilelink 主矩阵 12 格 ref 全部可解析", "[cep][source-registry]") {
    std::size_t checked = 0;
    for (std::uint8_t s = 0; s < TileLinkStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < TileLinkStateSpec::kEventCount; ++e) {
            const auto t = TileLinkStateSpec::transition(as_enum<TileLinkStateSpec::State>(s),
                                                        as_enum<TileLinkStateSpec::Event>(e));
            const auto r = TileLinkStateSpec::resolve_ref(t);
            INFO("cell state=" << int(s) << " event=" << int(e) << " ref=" << (t.ref ? t.ref : ""));
            REQUIRE(r.registered);
            ++checked;
        }
    }
    REQUIRE(checked == TileLinkStateSpec::kTransitionCellCount);
}

TEST_CASE("SRK-01b: tilelink T3 映射 11 行 ref 全部可解析", "[cep][source-registry]") {
    for (const auto& cell : TileLinkStateSpec::kT3MappingCells) {
        const auto r = TileLinkStateSpec::resolve_t3_ref(cell);
        INFO("row=" << (cell.row_ref ? cell.row_ref : ""));
        REQUIRE(r.registered);
        REQUIRE(TileLinkStateSpec::t3_basis_ref_kind_consistent(cell));
    }
    REQUIRE(TileLinkStateSpec::kT3MappingCells.size() == TileLinkStateSpec::kPlatformIntentCount);
}

TEST_CASE("SRK-01c: cxl 68 格 ref 全部可解析", "[cep][source-registry]") {
    std::size_t checked = 0;
    for (std::uint8_t s = 0; s < CxlStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < CxlStateSpec::kEventCount; ++e) {
            const auto t = CxlStateSpec::transition(as_enum<CxlStateSpec::State>(s),
                                                   as_enum<CxlStateSpec::Event>(e));
            const auto r = CxlStateSpec::resolve_ref(t);
            INFO("cell state=" << int(s) << " event=" << int(e) << " ref=" << (t.ref ? t.ref : ""));
            REQUIRE(r.registered);
            ++checked;
        }
    }
    REQUIRE(checked == CxlStateSpec::kTransitionCellCount);
}

TEST_CASE("SRK-01d: mesif 35 转换格 + 5 投影格 ref 全部可解析", "[cep][source-registry]") {
    std::size_t checked = 0;
    for (std::uint8_t s = 0; s < MesifStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < MesifStateSpec::kEventCount; ++e) {
            const auto& b = MesifStateSpec::kTable.transition_basis[s][e];
            REQUIRE(MesifStateSpec::resolve_ref(b).registered);
            ++checked;
        }
        const auto& p = MesifStateSpec::kTable.projection_basis[s];
        REQUIRE(MesifStateSpec::resolve_ref(p).registered);
        ++checked;
    }
    REQUIRE(checked == MesifStateSpec::kBasisCount);
}

TEST_CASE("SRK-01e: ualink 11 义务格 + 4 原语格 ref 全部可解析", "[cep][source-registry]") {
    for (const auto& c : ualink_inc::kBasisObligations) {
        INFO("ref=" << std::string(c.ref));
        REQUIRE(ualink_inc::resolve_ref(c.ref).registered);
    }
    for (const auto& c : ualink_inc::kBasisIncOps) {
        INFO("ref=" << std::string(c.ref));
        REQUIRE(ualink_inc::resolve_ref(c.ref).registered);
    }
    REQUIRE(ualink_inc::kBasisObligations.size() == ualink_inc::kObligationCount);
    REQUIRE(ualink_inc::kBasisIncOps.size() == ualink_inc::kIncOpBasisCount);
}

TEST_CASE("SRK-02: 4 载体 basis ↔ ref_kind 映射 0 违规", "[cep][source-registry]") {
    for (std::uint8_t s = 0; s < TileLinkStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < TileLinkStateSpec::kEventCount; ++e) {
            const auto t = TileLinkStateSpec::transition(as_enum<TileLinkStateSpec::State>(s),
                                                        as_enum<TileLinkStateSpec::Event>(e));
            INFO("tilelink state=" << int(s) << " event=" << int(e));
            REQUIRE(TileLinkStateSpec::basis_ref_kind_consistent(t));
        }
    }
    for (const auto& cell : TileLinkStateSpec::kT3MappingCells) {
        INFO("tilelink T3 " << (cell.row_ref ? cell.row_ref : ""));
        REQUIRE(TileLinkStateSpec::t3_basis_ref_kind_consistent(cell));
    }
    for (std::uint8_t s = 0; s < CxlStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < CxlStateSpec::kEventCount; ++e) {
            const auto t = CxlStateSpec::transition(as_enum<CxlStateSpec::State>(s),
                                                   as_enum<CxlStateSpec::Event>(e));
            INFO("cxl state=" << int(s) << " event=" << int(e) << " basis="
                              << int(static_cast<std::uint8_t>(t.basis)));
            REQUIRE(CxlStateSpec::basis_ref_kind_consistent(t));
        }
    }
    for (std::uint8_t s = 0; s < MesifStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < MesifStateSpec::kEventCount; ++e) {
            INFO("mesif state=" << int(s) << " event=" << int(e));
            REQUIRE(MesifStateSpec::basis_ref_kind_consistent(
                MesifStateSpec::kTable.transition_basis[s][e]));
        }
        REQUIRE(MesifStateSpec::basis_ref_kind_consistent(
            MesifStateSpec::kTable.projection_basis[s]));
    }
    for (const auto& c : ualink_inc::kBasisObligations) {
        INFO("ualink ob "
             << int(static_cast<std::uint8_t>(c.obligation)) << " ref=" << std::string(c.ref));
        REQUIRE(ualink_inc::basis_ref_kind_consistent(c.ref, c.kind));
    }
    for (const auto& c : ualink_inc::kBasisIncOps) {
        INFO("ualink op ref=" << std::string(c.ref));
        REQUIRE(ualink_inc::basis_ref_kind_consistent(c.ref, c.kind));
    }
}

TEST_CASE("SRK-03: sources.json ↔ kSources[] 逐条一致", "[cep][source-registry]") {
    const std::string path = std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/sources.json";
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json doc;
    in >> doc;
    const auto& arr = doc.at("sources");
    REQUIRE(arr.size() == kSources.size());

    auto kind_name = [](RefKind k) -> std::string {
        switch (k) {
            case RefKind::SpecClause:
                return "spec_clause";
            case RefKind::PublicNonnormative:
                return "public_nonnormative";
            case RefKind::RepoDerived:
                return "repo_derived";
        }
        return "";
    };

    for (std::size_t i = 0; i < kSources.size(); ++i) {
        INFO("index=" << i);
        REQUIRE(arr.at(i).at("id").get<std::string>() == std::string(kSources[i].id));
        REQUIRE(arr.at(i).at("kind").get<std::string>() == kind_name(kSources[i].kind));
        REQUIRE(arr.at(i).at("title").get<std::string>() == std::string(kSources[i].title));
    }
}

TEST_CASE("SRK-04: CellKind::Free 四态互斥 + 当前 0 格 + adapter 分支", "[cep][source-registry]") {
    // tilelink: 三态互斥,Free 当前 0。
    std::size_t tl_known = 0, tl_free = 0, tl_unknown = 0;
    for (std::uint8_t s = 0; s < TileLinkStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < TileLinkStateSpec::kEventCount; ++e) {
            const auto st = as_enum<TileLinkStateSpec::State>(s);
            const auto ev = as_enum<TileLinkStateSpec::Event>(e);
            const auto t = TileLinkStateSpec::transition(st, ev);
            tl_known += (t.kind == TileLinkStateSpec::CellKind::Known) ? 1 : 0;
            tl_free += (t.kind == TileLinkStateSpec::CellKind::Free) ? 1 : 0;
            tl_unknown += (t.kind == TileLinkStateSpec::CellKind::Unknown) ? 1 : 0;
            REQUIRE(TileLinkCAdapter::transition_is_free(st, ev) ==
                    (t.kind == TileLinkStateSpec::CellKind::Free));
            // Free 不得被 Known/Unknown 吞掉。
            if (t.kind == TileLinkStateSpec::CellKind::Free) {
                REQUIRE_FALSE(TileLinkCAdapter::transition_is_known(st, ev));
                REQUIRE_FALSE(TileLinkCAdapter::transition_is_unknown(st, ev));
            }
        }
    }
    REQUIRE(tl_known + tl_free + tl_unknown == TileLinkStateSpec::kTransitionCellCount);
    REQUIRE(tl_free == 0);

    // cxl: 四态互斥,Free 当前 0。
    REQUIRE(CxlStateSpec::count_cells(CxlStateSpec::CellKind::Free) == 0);
    REQUIRE(CxlStateSpec::count_cells(CxlStateSpec::CellKind::Known) +
                CxlStateSpec::count_cells(CxlStateSpec::CellKind::Illegal) +
                CxlStateSpec::count_cells(CxlStateSpec::CellKind::Unknown) +
                CxlStateSpec::count_cells(CxlStateSpec::CellKind::Free) ==
            CxlStateSpec::kTransitionCellCount);
    for (std::uint8_t s = 0; s < CxlStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < CxlStateSpec::kEventCount; ++e) {
            const auto st = as_enum<CxlStateSpec::State>(s);
            const auto ev = as_enum<CxlStateSpec::Event>(e);
            REQUIRE_FALSE(CxlCacheAdapter::transition_is_free(st, ev));
        }
    }

    // 枚举值互异(防有人把 Free 定义成 Known 的别名)。
    REQUIRE(static_cast<std::uint8_t>(TileLinkStateSpec::CellKind::Free) !=
            static_cast<std::uint8_t>(TileLinkStateSpec::CellKind::Known));
    REQUIRE(static_cast<std::uint8_t>(TileLinkStateSpec::CellKind::Free) !=
            static_cast<std::uint8_t>(TileLinkStateSpec::CellKind::Unknown));
    REQUIRE(static_cast<std::uint8_t>(CxlStateSpec::CellKind::Free) !=
            static_cast<std::uint8_t>(CxlStateSpec::CellKind::Known));
    REQUIRE(static_cast<std::uint8_t>(CxlStateSpec::CellKind::Free) !=
            static_cast<std::uint8_t>(CxlStateSpec::CellKind::Illegal));
    REQUIRE(static_cast<std::uint8_t>(CxlStateSpec::CellKind::Free) !=
            static_cast<std::uint8_t>(CxlStateSpec::CellKind::Unknown));
}

TEST_CASE("SRK-04b: 决定性变异输入 —— 弱来源配 Direct 必然不一致", "[cep][source-registry]") {
    // 合成格:ref 指向公开非规范来源,cxl `PublicDirect` 语义上**应当**一致;
    // 但若把它错当 `spec_clause` 则不一致 —— 这里验证判据**有鉴别力**。
    using T = CxlStateSpec::Transition;
    T weak{};
    weak.kind = CxlStateSpec::CellKind::Known;
    weak.target = CxlStateSpec::State::S;
    weak.basis = CxlStateSpec::BasisKind::PublicDirect;
    weak.pending = false;
    weak.ref = "D1 p.5 Fig.4 InvalidLoad1";
    weak.note = "synthetic";
    REQUIRE(CxlStateSpec::resolve_ref(weak).kind == RefKind::PublicNonnormative);
    REQUIRE(CxlStateSpec::basis_ref_kind_consistent(weak));

    // 同来源却标 `ArchforgeAnchor` ⇒ 必须不一致(否则说明映射无鉴别力)。
    weak.basis = CxlStateSpec::BasisKind::ArchforgeAnchor;
    REQUIRE_FALSE(CxlStateSpec::basis_ref_kind_consistent(weak));
}
