// =============================================================================
// test/test_cep_cxl_state_spec.cc
//   —— CXL 2.0 非对称规约载体(Phase 2a 结构层)的契约测试 + 反向 fixture
//
// 作者: CppTLM Team / 日期: 2026-11-01
// 配套: include/tlm/cep/cxl_state_spec.hh · docs/architecture/14-coherence-parity-baseline.md(无关)
//       openspec/changes/cpptlm-cep-cxl-nonsymmetric-state-spec/(delta spec AC-1 / AC-2 / AC-4 /
//       AC-5) 台账 obj-cep-platform.md Entry #122(口径裁决)· #123(change 转正)
//
// ── 判据纪律(per 本仓 4 条铁律)────────────────────────────────────────────
// ① 纯负向断言 SHALL 有正控 ⇒ 每条负向断言都有**正向**对照;
// ② 每条判据 SHALL 有**可执行反向 fixture** 且**因正确原因红**;
// ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据 ⇒ 结构校验提供
//    **运行时可判**入口(`structure_is_valid_for` 接受任意集合);
// ④ fixture 状态变迁后 SHALL 重新基线化 —— 本文件为**新增**,基线 = 全绿。
// =============================================================================

#include "tlm/cep/cxl_state_spec.hh"

#include "catch_amalgamated.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    using tlm::cep::CxlStateSpec;

    using State = CxlStateSpec::State;
    using Event = CxlStateSpec::Event;
    using Perm = CxlStateSpec::Perm;
    using BasisKind = CxlStateSpec::BasisKind;

    constexpr std::array<State, 4> kStates{State::I, State::S, State::E, State::M};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// 正控:基线必须全绿
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL state spec — 态集合恰 4 态(无 F)", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::kStateCount == 4);
    STATIC_REQUIRE(CxlStateSpec::kPermCount == 4);

    // ⚠️ 「无 `F` 态」的运行时判据:枚举值域恰为 0..3 ⇒ 取不到第 5 个态。
    //    (MESIF 载体的 `State::F` == 4 ⇒ 本载体**形态不同构**)
    STATIC_REQUIRE(static_cast<std::uint8_t>(State::M) == 3);
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::I)) == "I");
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::S)) == "S");
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::E)) == "E");
    STATIC_REQUIRE(std::string(CxlStateSpec::state_name(State::M)) == "M");
}

TEST_CASE("CEP CXL state spec — perm 投影恰 4 行且逐格正确", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::kPermProjection.size() == 4);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[0] == Perm::NONE);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[1] == Perm::READ);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[2] == Perm::UNIQUE);
    STATIC_REQUIRE(CxlStateSpec::kPermProjection[3] == Perm::WRITEBACK);
    // ⚠️ **无 `F→WRITEBACK` 行**(MESIF 的第 5 行在 CXL 无对应物)
    STATIC_REQUIRE(CxlStateSpec::kPermProjection.size() != 5);
}

TEST_CASE("CEP CXL state spec — 秩只增不减,无例外", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::NONE) == 0);
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::READ) == 1);
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::UNIQUE) == 2);
    STATIC_REQUIRE(CxlStateSpec::perm_rank(Perm::WRITEBACK) == 3);
    // ⚠️ 静默升级 = `E + 写事件 → M`(**类级判定**,MESIF 的 `F→E` 秩回落例外**不可移植**)
    // ⚠️ 类级口径:摘要 §4 的 `local write` 类 = {`WrInv`, `WOWrInv`, `ItoMWr`}
    //    ⇒ **不得**收窄到单 opcode(`e == ItoMWr` 会把类级证据越证成单 opcode 事实)
    STATIC_REQUIRE(CxlStateSpec::is_write_event(Event::WrInv));
    STATIC_REQUIRE(CxlStateSpec::is_write_event(Event::WOWrInv));
    STATIC_REQUIRE(CxlStateSpec::is_write_event(Event::ItoMWr));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_write_event(Event::RdShared));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_write_event(Event::SnpInv));
    // ⚠️ `WrCur` **存疑**:摘要 §4 的 write 类未收它 ⇒ 本载体不收(登记于 R-2)
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_write_event(Event::WrCur));

    STATIC_REQUIRE(CxlStateSpec::is_silent_upgrade(State::E, Event::WrInv));
    STATIC_REQUIRE(CxlStateSpec::is_silent_upgrade(State::E, Event::WOWrInv));
    STATIC_REQUIRE(CxlStateSpec::is_silent_upgrade(State::E, Event::ItoMWr));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_silent_upgrade(State::M, Event::ItoMWr));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_silent_upgrade(State::S, Event::ItoMWr));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_silent_upgrade(State::E, Event::RdShared));
}

TEST_CASE("CEP CXL state spec — 非对称结构:local / remote 划分", "[cep][cxl][state-spec]") {
    // remote = **仅** H2D snoop(发起方恒为 Home)
    STATIC_REQUIRE(CxlStateSpec::kRemoteEvents.size() == 3);
    STATIC_REQUIRE(CxlStateSpec::is_remote_event(Event::SnpInv));
    STATIC_REQUIRE(CxlStateSpec::is_remote_event(Event::SnpData));
    STATIC_REQUIRE(CxlStateSpec::is_remote_event(Event::SnpCurr));
    // ⚠️ **设备侧无 snoop 发起型事件** ⇒ 其余全部是 local
    STATIC_REQUIRE(CxlStateSpec::is_local_event(Event::RdOwn));
    STATIC_REQUIRE(CxlStateSpec::is_local_event(Event::WrInv));
    STATIC_REQUIRE(CxlStateSpec::is_local_event(Event::DirtyEvict));
    STATIC_REQUIRE_FALSE(CxlStateSpec::is_local_event(Event::SnpInv));
}

TEST_CASE("CEP CXL state spec — basis kind / pending 双向一致", "[cep][cxl][state-spec]") {
    // 强度不得升格:analogical / self-derived ⇒ 必须 pending
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::PublicAnalogical, true));
    STATIC_REQUIRE_FALSE(
        CxlStateSpec::basis_pending_consistent(BasisKind::PublicAnalogical, false));
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::SelfDerived, true));
    STATIC_REQUIRE_FALSE(CxlStateSpec::basis_pending_consistent(BasisKind::SelfDerived, false));
    // direct / archforge-anchor:⚠️ **两轴独立**(2026-11-01 修正,per 裁决 A)
    //    ⇒ `public-direct` + `pending=true` 是**合法组合**(pin=weak 的证据仍是 direct,
    //      只是字节不可本地复核)⇒ 原「双向一致」判据已废(会把合法组合判成非法)
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::PublicDirect, false));
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::PublicDirect, true));
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::ArchforgeAnchor, false));
    STATIC_REQUIRE(CxlStateSpec::basis_pending_consistent(BasisKind::ArchforgeAnchor, true));
}

TEST_CASE("CEP CXL state spec — 头注释消歧声明可机器校", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::has_disambiguation(CxlStateSpec::kDisambiguation));
    // ⚠️ 反向:不含「dGPU SoC」⇒ 红
    STATIC_REQUIRE_FALSE(CxlStateSpec::has_disambiguation("CXL 实例 = CEP 探索实例"));
    // ⚠️ 反向:不含「!=」⇒ 红
    STATIC_REQUIRE_FALSE(CxlStateSpec::has_disambiguation("dGPU SoC 实现路径"));
}

TEST_CASE("CEP CXL state spec — 残余风险登记 8 项", "[cep][cxl][state-spec]") {
    STATIC_REQUIRE(CxlStateSpec::kResidualRiskCount == 8);
    STATIC_REQUIRE(CxlStateSpec::kResidualRisks.size() == 8);
    STATIC_REQUIRE(CxlStateSpec::kTransientStateCount == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 反向 fixture:每条 SHALL **因正确原因**红(判据铁律 ②)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL state spec — 反向 fixture:结构校验对错误集合必红",
          "[cep][cxl][state-spec][reverse]") {
    // ① 态数 **≠ 4** ⇒ 红(正控:恰 4 ⇒ 绿)
    {
        constexpr std::array<State, 3> three{State::I, State::S, State::E};
        constexpr std::array<Perm, 3> proj3{Perm::NONE, Perm::READ, Perm::UNIQUE};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            three, CxlStateSpec::kRemoteEvents, CxlStateSpec::kLocalEvents, proj3));
    }
    // ② 投影逐格错(第 4 行给了 `UNIQUE` 而非 `WRITEBACK`)⇒ 红
    {
        constexpr std::array<Perm, 4> bad_proj{Perm::NONE, Perm::READ, Perm::UNIQUE, Perm::UNIQUE};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, CxlStateSpec::kRemoteEvents, CxlStateSpec::kLocalEvents, bad_proj));
    }
    // ③ remote 集里混入一个**非** snoop 事件 ⇒ 红(remote SHALL 全是 snoop)
    {
        constexpr std::array<Event, 3> bad_remote{Event::SnpInv, Event::SnpData, Event::RdOwn};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, bad_remote, CxlStateSpec::kLocalEvents, CxlStateSpec::kPermProjection));
    }
    // ④ local 集里混入一个 snoop ⇒ 红(local ∩ remote = ∅ 被破)
    {
        constexpr std::array<Event, 4> bad_local{Event::RdOwn, Event::WrInv, Event::SnpInv,
                                                 Event::DirtyEvict};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, CxlStateSpec::kRemoteEvents, bad_local, CxlStateSpec::kPermProjection));
    }
    // ⑤ remote 集有重复 ⇒ 红
    {
        constexpr std::array<Event, 3> dup{Event::SnpInv, Event::SnpInv, Event::SnpCurr};
        STATIC_REQUIRE_FALSE(CxlStateSpec::structure_is_valid_for(
            kStates, dup, CxlStateSpec::kLocalEvents, CxlStateSpec::kPermProjection));
    }
    // 正控:全部正确 ⇒ 绿(否则上面的负向断言就没有鉴别力)
    STATIC_REQUIRE(CxlStateSpec::structure_is_valid_for(kStates, CxlStateSpec::kRemoteEvents,
                                                        CxlStateSpec::kLocalEvents,
                                                        CxlStateSpec::kPermProjection));
    STATIC_REQUIRE(CxlStateSpec::structure_is_valid());
}

// ─────────────────────────────────────────────────────────────────────────────
// 转换矩阵(Phase 2b)—— 三态格语义 + 反秃头覆盖率
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL state spec — 转换矩阵 68 格且三态计数自洽", "[cep][cxl][state-spec][matrix]") {
    STATIC_REQUIRE(CxlStateSpec::kEventCount == 17);
    STATIC_REQUIRE(CxlStateSpec::kTransitionCellCount == 68);

    // ⚠️ 硬编码常量 SHALL 与**逐格实际统计**相等(防虚报覆盖率,判据 AC-9)
    STATIC_REQUIRE(CxlStateSpec::kKnownCellCount ==
                   CxlStateSpec::count_cells(CxlStateSpec::CellKind::Known));
    STATIC_REQUIRE(CxlStateSpec::kIllegalCellCount ==
                   CxlStateSpec::count_cells(CxlStateSpec::CellKind::Illegal));
    STATIC_REQUIRE(CxlStateSpec::kUnknownCellCount ==
                   CxlStateSpec::count_cells(CxlStateSpec::CellKind::Unknown));

    STATIC_REQUIRE(CxlStateSpec::kKnownCellCount == 26);
    // ⚠️ `Illegal` 恒 0 = **来源不给**(D1 p.7 证实存在但不说哪些)⇒ 填任何 Illegal 即杜撰
    STATIC_REQUIRE(CxlStateSpec::kIllegalCellCount == 0);
    STATIC_REQUIRE(CxlStateSpec::kUnknownCellCount == 42);
    STATIC_REQUIRE(CxlStateSpec::kLegalTransitionCoverageNum == 26);
    STATIC_REQUIRE(CxlStateSpec::kLegalTransitionCoverageDen == 68);
    // ⚠️ **运行时断言**(非 `STATIC_REQUIRE`):矩阵**数据内容**若被毒化,须表现为
    //    **ctest 失败**而非「编译失败」—— 判据铁律 ③(编译期锁不得只以「编译红」充当 ctest 判据)
    REQUIRE(CxlStateSpec::transitions_are_valid());
}

TEST_CASE("CEP CXL state spec — Unknown 显式携带缺失项(R 编号),不得退化为 Illegal",
          "[cep][cxl][state-spec][matrix][reverse]") {
    // ⚠️ 关键语义:「我不知道」≠「它非法」—— 消费者不得把 Unknown 读成 Illegal
    STATIC_REQUIRE(CxlStateSpec::transition(State::E, Event::ItoMWr).kind ==
                   CxlStateSpec::CellKind::Unknown);
    STATIC_REQUIRE(CxlStateSpec::transition(State::E, Event::ItoMWr).kind !=
                   CxlStateSpec::CellKind::Illegal);
    STATIC_REQUIRE(CxlStateSpec::transition(State::I, Event::SnpInv).kind ==
                   CxlStateSpec::CellKind::Unknown);

    // 正控:有证据的格 SHALL 是 Known
    STATIC_REQUIRE(CxlStateSpec::transition(State::M, Event::SnpInv).kind ==
                   CxlStateSpec::CellKind::Known);
    STATIC_REQUIRE(CxlStateSpec::transition(State::M, Event::SnpInv).target == State::I);
    STATIC_REQUIRE(CxlStateSpec::transition(State::S, Event::SnpData).target == State::S);
    STATIC_REQUIRE(CxlStateSpec::transition(State::I, Event::RdOwn).target == State::M);
}

TEST_CASE("CEP CXL state spec — 反向 fixture:非法矩阵必因正确原因红",
          "[cep][cxl][state-spec][matrix][reverse]") {
    using Cell = CxlStateSpec::Transition;
    using CK = CxlStateSpec::CellKind;

    constexpr Cell good_k{CK::Known, State::I, BasisKind::PublicDirect, false, "D1 p.4", ""};
    constexpr Cell good_u{CK::Unknown, State::I, BasisKind::SelfDerived, true, "", "R-2: 缺"};

    // 正要:全合法 ⇒ 绿(否则下面的负向断言没有鉴别力)
    {
        constexpr std::array<std::array<Cell, 4>, 1> ok{{{{good_k, good_u, good_k, good_u}}}};
        REQUIRE(CxlStateSpec::transitions_are_valid_for(ok));
    }
    // ① `Unknown` 的 note 不带 `R-` 编号 ⇒ 红(「缺什么」必须可读,不能只是一个布尔)
    {
        constexpr Cell bad{CK::Unknown, State::I, BasisKind::SelfDerived, true, "", "不知道"};
        constexpr std::array<std::array<Cell, 4>, 1> m{{{{bad, good_k, good_k, good_k}}}};
        REQUIRE_FALSE(CxlStateSpec::transitions_are_valid_for(m));
    }
    // ② `Illegal` 无证据(`ref` 空)⇒ 红(断言「非法」也必须举证)
    {
        constexpr Cell bad{CK::Illegal, State::I, BasisKind::PublicDirect, false, "", "R-2"};
        constexpr std::array<std::array<Cell, 4>, 1> m{{{{bad, good_k, good_k, good_k}}}};
        REQUIRE_FALSE(CxlStateSpec::transitions_are_valid_for(m));
    }
    // ③ `Unknown` 被编码成 `Illegal`(二态化)⇒ 红 —— 这正是 mesif 的 `0xFF` 空洞语义之害
    {
        constexpr Cell bad{CK::Illegal, State::I, BasisKind::PublicDirect, false, "D1 p.4", "R-2"};
        constexpr std::array<std::array<Cell, 4>, 1> m{{{{bad, good_k, good_k, good_k}}}};
        // 该格本身合法(有 ref)⇒ 用「三态之和」不守恒的矩阵触发红更强:换 3 格
        constexpr std::array<std::array<Cell, 3>, 1> m3{{{{bad, good_k, good_k}}}};
        REQUIRE_FALSE(CxlStateSpec::transitions_are_valid_for(m3));
        (void)m;
    }
    // ④ `pending = false` 却无 `ref` ⇒ 红(不得声称已裁决而不给依据)
    {
        constexpr Cell bad{CK::Known, State::I, BasisKind::PublicDirect, false, "", ""};
        constexpr std::array<std::array<Cell, 4>, 1> m{{{{bad, good_k, good_k, good_k}}}};
        REQUIRE_FALSE(CxlStateSpec::transitions_are_valid_for(m));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CSS-06 —— 载体 ↔ 核对表 ↔ 锚点摘要 三方逐格一致(机器判据)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

    /// 仓库根(CPPTLM_SOURCE_DIR 由 test/CMakeLists.txt 注入)。
    std::string repo_root_dir() {
#ifdef CPPTLM_SOURCE_DIR
        return std::string(CPPTLM_SOURCE_DIR);
#else
        return std::string(".");
#endif
    }

    std::string read_file_or_empty(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return {};
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::vector<std::string> split_lines(const std::string& doc) {
        std::vector<std::string> out;
        std::istringstream in(doc);
        std::string line;
        while (std::getline(in, line)) {
            out.push_back(line);
        }
        return out;
    }

    /// 取标记块内的表行(`<!-- CXL-TABLE:<name>:START -->` … `END -->`)。
    std::vector<std::string> extract_block_rows(const std::string& doc, const std::string& name) {
        std::vector<std::string> rows;
        const std::string start = "<!-- CXL-TABLE:" + name + ":START -->";
        const std::string end = "<!-- CXL-TABLE:" + name + ":END -->";
        const std::size_t i = doc.find(start);
        const std::size_t j = doc.find(end);
        if (i == std::string::npos || j == std::string::npos || j < i) {
            return rows;
        }
        for (const auto& line : split_lines(doc.substr(i + start.size(), j - i - start.size()))) {
            if (line.rfind("|", 0) == 0) {
                rows.push_back(line);
            }
        }
        return rows;
    }

    /// 拆分 markdown 表行(去首尾 `|`,按 `|` 切分并 trim)。
    std::vector<std::string> split_row(const std::string& line) {
        std::vector<std::string> cells;
        std::string cur;
        for (const char c : line) {
            if (c == '|') {
                cells.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        cells.push_back(cur);
        // 去掉首尾两个空单元(行首 `|` 与行尾 `|`)
        if (cells.size() >= 2 && cells.front().empty()) {
            cells.erase(cells.begin());
        }
        if (!cells.empty() && cells.back().empty()) {
            cells.pop_back();
        }
        for (auto& c : cells) {
            const std::size_t b = c.find_first_not_of(" \t");
            const std::size_t e = c.find_last_not_of(" \t");
            c = (b == std::string::npos) ? std::string() : c.substr(b, e - b + 1);
        }
        return cells;
    }

    /// 抽取引文段(直引号 / 直角引号)的最长长度 —— CSS-06 ③「不整段复制原文」。
    std::size_t longest_quote_span(const std::string& doc) {
        std::size_t best = 0;
        // 直角引号
        std::size_t pos = 0;
        while ((pos = doc.find("\u300c", pos)) != std::string::npos) {
            const std::size_t e = doc.find("\u300d", pos);
            if (e == std::string::npos) {
                break;
            }
            best = std::max(best, e - pos - 1);
            pos = e;
        }
        // 直引号(成对)
        pos = 0;
        while ((pos = doc.find('"', pos)) != std::string::npos) {
            const std::size_t e = doc.find('"', pos + 1);
            if (e == std::string::npos) {
                break;
            }
            best = std::max(best, e - pos - 1);
            pos = e + 1;
        }
        return best;
    }

} // namespace

TEST_CASE("CEP CXL state spec — CSS-06:载体 ↔ 核对表 ↔ 摘要 三方逐格一致",
          "[cep][cxl][state-spec][css06]") {
    const std::string doc = read_file_or_empty(
        repo_root_dir() + "/docs/implementation/2026-11-01-cep-cxl-state-spec-anchor-checklist.md");
    const std::string summary = read_file_or_empty(
        repo_root_dir() + "/docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md");
    REQUIRE_FALSE(doc.empty());
    REQUIRE_FALSE(summary.empty());

    // ── ① 核对表 SHALL 恰有 68 行(表头 2 行 + 数据 68 行)──────────────────
    const auto rows = extract_block_rows(doc, "MATRIX");
    REQUIRE(rows.size() == CxlStateSpec::kTransitionCellCount + 2);

    // ── ② 逐格比对:核对表 ⟷ 载体(任一格不一致 ⇒ 红)────────────────────────
    std::size_t mismatches = 0;
    std::size_t idx = 0;
    for (std::size_t e = 0; e < CxlStateSpec::kEventCount; ++e) {
        for (std::size_t st = 0; st < CxlStateSpec::kStateCount; ++st) {
            const auto& cell = CxlStateSpec::kTransitions[e][st];
            const auto c = split_row(rows[2 + idx]);
            ++idx;
            REQUIRE(c.size() == 9);
            const std::string want_kind =
                cell.kind == CxlStateSpec::CellKind::Known
                    ? "Known"
                    : (cell.kind == CxlStateSpec::CellKind::Illegal ? "Illegal" : "Unknown");
            const std::string want_target = cell.kind == CxlStateSpec::CellKind::Known
                                                ? std::string(CxlStateSpec::state_name(cell.target))
                                                : std::string("-");
            if (c[1] != CxlStateSpec::event_name(static_cast<Event>(e)) ||
                c[2] != CxlStateSpec::state_name(static_cast<State>(st)) || c[3] != want_kind ||
                c[4] != want_target || c[5] != CxlStateSpec::basis_kind_name(cell.basis) ||
                c[6] != (cell.pending ? "true" : "false") || c[7] != cell.ref ||
                c[8] != cell.note) {
                ++mismatches;
            }
        }
    }
    REQUIRE(mismatches == 0);

    // ── ③ 摘要统计块 ⟷ 载体(聚合层;防「虚报覆盖率」)───────────────────────
    const auto stats = extract_block_rows(summary, "STATS");
    // STATS 块是 `key=value` 行(非表),故按行解析
    std::string stats_text;
    {
        const std::string start = "<!-- CXL-TABLE:STATS:START -->";
        const std::string end = "<!-- CXL-TABLE:STATS:END -->";
        const std::size_t i = summary.find(start);
        const std::size_t j = summary.find(end);
        REQUIRE(i != std::string::npos);
        REQUIRE(j != std::string::npos);
        stats_text = summary.substr(i + start.size(), j - i - start.size());
    }
    REQUIRE(stats_text.find("cells=68") != std::string::npos);
    REQUIRE(stats_text.find("known=26") != std::string::npos);
    REQUIRE(stats_text.find("illegal=0") != std::string::npos);
    REQUIRE(stats_text.find("unknown=42") != std::string::npos);
    REQUIRE(stats_text.find("pending_false=22") != std::string::npos);
    REQUIRE(stats_text.find("coverage=26/68") != std::string::npos);
    // ⚠️ **单调下界**(AC-9「单调不减」条款)—— 整体降档 SHALL 必红
    REQUIRE(stats_text.find("coverage_floor=26") != std::string::npos);
    REQUIRE(stats_text.find("pending_false_floor=22") != std::string::npos);
    REQUIRE(CxlStateSpec::coverage_is_not_regressed());
    REQUIRE(CxlStateSpec::kCoverageFloor == 26);
    REQUIRE(CxlStateSpec::kPendingFalseFloor == 22);
    // ⚠️ 硬编码值 SHALL 与载体实际统计相等(防虚报)
    REQUIRE(CxlStateSpec::count_cells(CxlStateSpec::CellKind::Known) == 26);
    REQUIRE(CxlStateSpec::count_cells(CxlStateSpec::CellKind::Illegal) == 0);
    REQUIRE(CxlStateSpec::count_cells(CxlStateSpec::CellKind::Unknown) == 42);

    // ── ④ CSS-06 ③:摘要与核对表 SHALL **不**整段复制来源原文 ────────────────
    //     (阈值 200 字符 —— 实测当前最长引文 122 ⇒ 不误伤;整段复制必超)
    REQUIRE(longest_quote_span(summary) <= 200);
    REQUIRE(longest_quote_span(doc) <= 200);
}
