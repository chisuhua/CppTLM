// test/test_cep_mesif_state_spec.cc
// 队列 #9 `cpptlm-cep-mesif-adapter` 的**规约层**判据 —— MESIF 状态机规约 + 5→4 `perm` 投影表
//
// per openspec/changes/cpptlm-cep-mesif-state-spec delta capability `cep-mesif-state-spec`
// (MS-01 ~ MS-05) + `ADR-CEP-04 §13` + `ADR-CEP-12 §13`
// 作者: CppTLM Team / 日期: 2026-10-29
//
// ── 判据纪律 ──────────────────────────────────────────────────────────
// ① 每条判据都有**可执行反向 fixture**:对**载体数据的变异副本**跑**同一谓词**
//    ⇒ 变异只发生在副本上 ⇒ 真实表与编译期锁**恒绿** ⇒ fixture 必是**运行时红**。
// ② **纯负向断言必带正控**:「变异副本 ⇒ 谓词为假」之后必跟「真实表 ⇒ 同一谓词为真」,
//    否则「谓词恒假」与「谓词正确」不可分辨。
// ③ 判据**不锁**推导值的**语义正确性**(per change 的 design `D-ζ`)—— 锁的是
//    **完备性 / 可审计性 / 待追认标记 / 内部自洽**;语义正确性由 ArchForge 逐格追认闭合。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include "tlm/cep/chi_rnf_adapter.hh"
#include "tlm/cep/mesif_state_spec.hh"
#include "tlm/cep/moesi_gpu_adapter.hh"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

    using Spec = tlm::cep::MesifStateSpec;
    using Table = Spec::Table;

    /// 载体的**变异副本**(反向 fixture 用;真实表恒不变)
    Table copy_table() {
        return Spec::kTable;
    }

    /// 语义损失 ① 的**登记文本**谓词(正控 = 载体常量;反向 fixture = 变异文本)
    bool merge_note_satisfies(const char* note) {
        return Spec::contains(note, "Sharer Tracker") &&
               Spec::contains(note, "可由 Sharer Tracker 补偿") &&
               // ⚠️ 主体登记**不得**出现「不可补偿」—— 那是 `chi_rnf` 侧的口径
               //    (对比说明在 `kPermMergeNoteChiRnfContrast` 里单独登记)
               !Spec::contains(note, "不可补偿");
    }

    /// 语义损失 ②(秩回落)的**登记文本**谓词
    bool rank_note_satisfies(const char* note) {
        return Spec::contains(note, "F → E") && Spec::contains(note, "秩回落") &&
               Spec::contains(note, "第 5 个 perm 值");
    }

    /// 清单里的一行(前 5 列有效)
    struct ChecklistRow {
        std::string c1;      ///< 态(转换表) / 态(投影表)
        std::string c2;      ///< 事件(转换表) / perm(投影表)
        std::string c3;      ///< 目标态(转换表)
        std::string kind;    ///< `kind`
        std::string pending; ///< `yes` / `no`
    };


    /// 去空白 + 去反引号
    std::string trim_cell(std::string s) {
        const auto b = s.find_first_not_of(" \t");
        if (b == std::string::npos)
            return std::string();
        const auto e = s.find_last_not_of(" \t");
        s = s.substr(b, e - b + 1);
        if (s.size() >= 2 && s.front() == '`' && s.back() == '`')
            s = s.substr(1, s.size() - 2);
        return s;
    }

    /// 按**表头行**定位并解析一张 markdown 表(只取数据行)
    std::vector<ChecklistRow> parse_checklist_table(const std::string& doc,
                                                    const std::string& header, std::size_t kind_col,
                                                    std::size_t pending_col) {
        std::vector<ChecklistRow> rows;
        std::size_t pos = doc.find(header);
        if (pos == std::string::npos)
            return rows;
        pos += header.size();
        // 跳过表头行尾的换行 ⇒ 从**分隔行**开始读
        const std::size_t nl = doc.find('\n', pos);
        if (nl == std::string::npos)
            return rows;
        pos = nl + 1;
        while (pos < doc.size()) {
            std::size_t eol = doc.find('\n', pos);
            if (eol == std::string::npos)
                eol = doc.size();
            const std::string line = doc.substr(pos, eol - pos);
            pos = eol + 1;
            if (line.empty() || line[0] != '|')
                break; // 表结束
            std::vector<std::string> cells;
            std::string cur;
            for (const char ch : line) {
                if (ch == '|') {
                    cells.push_back(cur);
                    cur.clear();
                } else {
                    cur.push_back(ch);
                }
            }
            cells.push_back(cur);
            if (cells.size() < kind_col + 2)
                continue;
            const std::string first = trim_cell(cells[1]);
            if (first.empty() || first.find_first_not_of("0123456789") != std::string::npos)
                continue; // 表头 / 分隔行
            ChecklistRow r;
            r.c1 = trim_cell(cells[2]);
            r.c2 = trim_cell(cells[3]);
            r.c3 = trim_cell(cells[4]);
            r.kind = trim_cell(cells[kind_col]);
            r.pending = trim_cell(cells[pending_col]);
            rows.push_back(r);
        }
        return rows;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════
//  MS-01 —— 5 态 SHALL 全部有 4 值 `perm` 投影;瞬态 SHALL 无投影
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("MESIF 规约:5 态各有 4 值 perm 投影 + 瞬态无投影(MS-01)", "[cep][mesif-state-spec]") {
    // ── ① 5 态**各有**投影(缺一即红)──────────────────────────────────
    const std::pair<Spec::State, Spec::Perm> expected[] = {
        {Spec::State::I, Spec::Perm::NONE},      // I → NONE
        {Spec::State::S, Spec::Perm::READ},      // S → READ
        {Spec::State::E, Spec::Perm::UNIQUE},    // E → UNIQUE
        {Spec::State::M, Spec::Perm::WRITEBACK}, // M → WRITEBACK
        {Spec::State::F, Spec::Perm::WRITEBACK}, // F → WRITEBACK
    };
    for (const auto& [st, perm] : expected) {
        INFO("态: " << Spec::state_name(st));
        Spec::Perm out = Spec::Perm::NONE;
        REQUIRE(Spec::perm_of(st, out));
        CHECK(out == perm);
    }
    // 投影表完备(谓词口径:5 态**各有**合法投影)
    CHECK(Spec::projection_is_complete(Spec::kTable));

    // ── ② `I` → `NONE`;`F` → `WRITEBACK`(后者是 ArchForge **一手原文**)──
    {
        const auto& b = Spec::kTable.projection_basis[4]; // F
        INFO("F 态投影的依据: " << b.ref);
        CHECK(b.kind == Spec::BasisKind::Primary);
        CHECK(Spec::contains(b.ref, "32 号:529"));
        CHECK_FALSE(b.pending); // 一手原文 ⇒ 不计入待追认
    }

    // ── ③ 瞬态**无投影**(本规约不声明瞬态 ⇒ 平凡成立)─────────────────
    CHECK(Spec::kTransientStateCount == 0u);
    CHECK(Spec::transients_have_no_projection(Spec::kTable, Spec::kTransientStates,
                                              Spec::kTransientStateCount));

    // ── 反向 fixture ①:让某个态**无投影** ⇒ 必红 ──────────────────────
    {
        Table t = copy_table();
        t.projection[4] = Spec::kNoProjection;
        CHECK_FALSE(Spec::projection_is_complete(t));
        // 正控:同一谓词在真实表上为真
        CHECK(Spec::projection_is_complete(Spec::kTable));
    }

    // ── 反向 fixture ②:给「瞬态」加投影 ⇒ 必红 ───────────────────────
    {
        // 伪造一个瞬态列表(把 `F` 当瞬态)⇒ `F` 有投影 ⇒ 违反「瞬态无投影」
        const std::uint8_t fake_transient[1] = {static_cast<std::uint8_t>(Spec::State::F)};
        CHECK_FALSE(Spec::transients_have_no_projection(Spec::kTable, fake_transient, 1));
        // 正控:真实瞬态列表(空)下同一谓词为真
        CHECK(Spec::transients_have_no_projection(Spec::kTable, Spec::kTransientStates,
                                                  Spec::kTransientStateCount));
    }

    // ── 正控:`chi_rnf` / `moesi_gpu` 的既有投影**零回归** ──────────────
    {
        tlm::cep::ChiRnfAdapter::Perm p = tlm::cep::ChiRnfAdapter::Perm::NONE;
        REQUIRE(tlm::cep::ChiRnfAdapter::perm_of(tlm::cep::ChiRnfAdapter::State::UC, p));
        CHECK(p == tlm::cep::ChiRnfAdapter::Perm::UNIQUE);
        REQUIRE(tlm::cep::ChiRnfAdapter::perm_of(tlm::cep::ChiRnfAdapter::State::SC, p));
        CHECK(p == tlm::cep::ChiRnfAdapter::Perm::READ);
        // `chi_rnf` 的 `SD`/`UD` 同投影 `WRITEBACK`(对照:与 `mesif` 的 `F`/`M` 合并**同型**)
        REQUIRE(tlm::cep::ChiRnfAdapter::perm_of(tlm::cep::ChiRnfAdapter::State::SD, p));
        CHECK(p == tlm::cep::ChiRnfAdapter::Perm::WRITEBACK);

        tlm::cep::MoesiGpuAdapter::Perm p2 = tlm::cep::MoesiGpuAdapter::Perm::NONE;
        REQUIRE(tlm::cep::MoesiGpuAdapter::perm_of(tlm::cep::MoesiGpuAdapter::State::O, p2));
        CHECK(p2 == tlm::cep::MoesiGpuAdapter::Perm::WRITEBACK);
        // `moesi_gpu` 的瞬态 `T` **无投影**(与 `mesif` 的判据同口径)
        CHECK_FALSE(tlm::cep::MoesiGpuAdapter::perm_of(tlm::cep::MoesiGpuAdapter::State::T, p2));
    }
}

// ══════════════════════════════════════════════════════════════════════
//  MS-02 —— 转换表 SHALL 完备;写只在独占态合法;秩口径
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("MESIF 规约:转换表完备 + 写只在独占态 + 秩口径(MS-02)", "[cep][mesif-state-spec]") {
    // ── ① 转换表**完备**(5 态 × 6 事件 = 30 格,无空洞无越界)─────────
    CHECK(Spec::table_is_complete(Spec::kTable));
    CHECK(Spec::kStateCount == 5u);
    CHECK(Spec::kEventCount == 6u);
    // 每格的目标态都落在合法范围内(逐格再确认一次,防谓词实现走样)
    for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
        for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
            const std::uint8_t tgt = Spec::kTable.transition[s][e];
            INFO("格: " << Spec::state_name(static_cast<Spec::State>(s)) << " + "
                        << Spec::event_name(static_cast<Spec::Event>(e)));
            CHECK(tgt < Spec::kStateCount);
        }
    }

    // ── ② `E + Wr → M` 存在(唯一干净独占态 + 写 ⇒ 脏独占)──────────────
    CHECK(Spec::transition(Spec::State::E, Spec::Event::Wr) ==
          static_cast<std::uint8_t>(Spec::State::M));
    CHECK(Spec::is_silent_upgrade(Spec::State::E, Spec::Event::Wr));
    CHECK(Spec::kUniqueCleanState == Spec::State::E);
    CHECK(Spec::kUniqueDirtyState == Spec::State::M);
    CHECK(Spec::kWriteEvent == Spec::Event::Wr);

    // ── ③ 写**只在独占态**合法;共享态上的写**状态不变** ───────────────
    CHECK(Spec::write_is_legal(Spec::State::E));
    CHECK(Spec::write_is_legal(Spec::State::M));
    CHECK_FALSE(Spec::write_is_legal(Spec::State::I));
    CHECK_FALSE(Spec::write_is_legal(Spec::State::S));
    CHECK_FALSE(Spec::write_is_legal(Spec::State::F));
    CHECK(Spec::transition(Spec::State::S, Spec::Event::Wr) ==
          static_cast<std::uint8_t>(Spec::State::S)); // 共享态写 ⇒ 不变
    CHECK(Spec::transition(Spec::State::F, Spec::Event::Wr) ==
          static_cast<std::uint8_t>(Spec::State::F)); // `F` 非独占 ⇒ 不变
    CHECK(Spec::write_rule_holds(Spec::kTable));

    // ── ④ 秩口径:本地事件单调不降;`E → M` 严格升高;唯一例外 `F → E` ──
    CHECK(Spec::rank_rule_holds(Spec::kTable));
    CHECK(Spec::perm_rank(Spec::Perm::WRITEBACK) > Spec::perm_rank(Spec::Perm::UNIQUE));
    CHECK(Spec::perm_rank(Spec::Perm::UNIQUE) > Spec::perm_rank(Spec::Perm::READ));
    CHECK(Spec::perm_rank(Spec::Perm::READ) > Spec::perm_rank(Spec::Perm::NONE));
    CHECK(Spec::is_rank_regression_exception(Spec::State::F, Spec::Event::RdUnique));
    CHECK(Spec::is_rank_regression_exception(Spec::State::F, Spec::Event::Upgrade));
    CHECK_FALSE(Spec::is_rank_regression_exception(Spec::State::E, Spec::Event::RdUnique));
    // 远端 snoop **不**纳入单调口径(必然降秩)
    CHECK_FALSE(Spec::is_local_event(Spec::Event::SnpShared));
    CHECK_FALSE(Spec::is_local_event(Spec::Event::SnpUnique));

    // ── 反向 fixture ③:挖掉转换表的一格 ⇒ 必红 ────────────────────────
    {
        Table t = copy_table();
        t.transition[0][4] = 0xFF; // I + SnpShared ⇒ 未定义
        CHECK_FALSE(Spec::table_is_complete(t));
        CHECK(Spec::table_is_complete(Spec::kTable)); // 正控
    }

    // ── 反向 fixture ④:让 `S` 上的写改变状态 ⇒ 必红 ───────────────────
    {
        Table t = copy_table();
        t.transition[1][static_cast<std::size_t>(Spec::Event::Wr)] =
            static_cast<std::uint8_t>(Spec::State::E);
        CHECK_FALSE(Spec::write_rule_holds(t));
        CHECK(Spec::write_rule_holds(Spec::kTable)); // 正控
    }

    // ── 反向 fixture ⑤:本地事件上的秩下降(**非** `F → E` 例外)⇒ 必红 ──
    {
        Table t = copy_table();
        t.transition[2][static_cast<std::size_t>(Spec::Event::Rd)] =
            static_cast<std::uint8_t>(Spec::State::I); // E(UNIQUE) → I(NONE) ⇒ 秩 2→0
        CHECK_FALSE(Spec::rank_rule_holds(t));
        CHECK(Spec::rank_rule_holds(Spec::kTable)); // 正控
    }

    // ── 反向 fixture ⑥:让 `E + Wr` 不再严格升高 ⇒ 必红 ────────────────
    {
        Table t = copy_table();
        t.transition[2][static_cast<std::size_t>(Spec::Event::Wr)] =
            static_cast<std::uint8_t>(Spec::State::E); // E + Wr ⇒ 停在 E
        CHECK_FALSE(Spec::rank_rule_holds(t));
        CHECK(Spec::rank_rule_holds(Spec::kTable)); // 正控
    }
}

// ══════════════════════════════════════════════════════════════════════
//  MS-03 —— 每一格 SHALL 带推导依据与待追认状态
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("MESIF 规约:逐格依据齐备 + kind/pending 双向一致 + 待追认可查(MS-03)",
          "[cep][mesif-state-spec]") {
    // ── ① 35 格**每一格**都带非空 `basis`(三字段)────────────────────
    CHECK(Spec::basis_is_consistent(Spec::kTable));
    for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
        for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
            const auto& b = Spec::kTable.transition_basis[s][e];
            INFO("格: " << Spec::state_name(static_cast<Spec::State>(s)) << " + "
                        << Spec::event_name(static_cast<Spec::Event>(e)));
            REQUIRE(b.ref != nullptr);
            REQUIRE(b.note != nullptr);
            CHECK(b.ref[0] != '\0');
            CHECK(b.note[0] != '\0');
        }
    }
    for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
        const auto& b = Spec::kTable.projection_basis[s];
        INFO("投影态: " << Spec::state_name(static_cast<Spec::State>(s)));
        REQUIRE(b.ref != nullptr);
        REQUIRE(b.note != nullptr);
        CHECK(b.ref[0] != '\0');
        CHECK(b.note[0] != '\0');
    }

    // ── ② `kind` 与 `pending` **双向一致** ────────────────────────────
    for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
        for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
            const auto& b = Spec::kTable.transition_basis[s][e];
            if (b.kind == Spec::BasisKind::Primary) {
                INFO("Primary 格被标为待追认: " << Spec::state_name(static_cast<Spec::State>(s)));
                CHECK_FALSE(b.pending);
                CHECK(Spec::has_archforge_anchor(b.ref));
            } else {
                INFO("推导格未被标为待追认: " << Spec::state_name(static_cast<Spec::State>(s)));
                CHECK(b.pending);
            }
        }
    }

    // ── ③ 待追认格数**可查** + 与 design `D-ε.11` 逐数一致 ──────────
    CHECK(Spec::pending_count(Spec::kTable) == Spec::kExpectedPendingCount);
    CHECK(Spec::pending_count(Spec::kTable) == 31u);
    CHECK(Spec::primary_count(Spec::kTable) == Spec::kExpectedPrimaryCount);
    CHECK(Spec::primary_count(Spec::kTable) == 4u);
    CHECK(Spec::kind_count(Spec::kTable, Spec::BasisKind::Structural) ==
          Spec::kExpectedStructuralCount);
    CHECK(Spec::kind_count(Spec::kTable, Spec::BasisKind::Derived) == Spec::kExpectedDerivedCount);
    CHECK(Spec::kind_count(Spec::kTable, Spec::BasisKind::Analogical) ==
          Spec::kExpectedAnalogicalCount);
    // `Primary` 的分布:3 转换格 + 1 投影格
    CHECK(Spec::transition_primary_count(Spec::kTable) == 3u);
    CHECK(Spec::projection_primary_count(Spec::kTable) == 1u);

    // ── ④ 正控:`F` 态投影是 `Primary` 且指向 `32 号:529` ⇒ 不计入待追认 ──
    CHECK(Spec::projection_primary_count(Spec::kTable) == 1u);
    CHECK_FALSE(Spec::kTable.projection_basis[4].pending);

    // ── 反向 fixture ⑦:清空某格的 `basis` ⇒ 必红 ─────────────────────
    {
        Table t = copy_table();
        t.transition_basis[0][0].ref = nullptr;
        CHECK_FALSE(Spec::basis_entry_is_consistent(t.transition_basis[0][0]));
        CHECK(Spec::basis_entry_is_consistent(Spec::kTable.transition_basis[0][0])); // 正控
    }

    // ── 反向 fixture ⑧:一手原文格标为待追认 ⇒ 必红 ───────────────────
    {
        Table t = copy_table();
        t.transition_basis[2][4].pending = true; // E + SnpShared(Primary)
        CHECK_FALSE(Spec::basis_entry_is_consistent(t.transition_basis[2][4]));
        CHECK(Spec::basis_entry_is_consistent(Spec::kTable.transition_basis[2][4])); // 正控
    }

    // ── 反向 fixture ⑨:推导格标为已追认 ⇒ 必红 ───────────────────────
    {
        Table t = copy_table();
        t.transition_basis[0][0].pending = false; // I + Rd(Structural)
        CHECK_FALSE(Spec::basis_entry_is_consistent(t.transition_basis[0][0]));
        CHECK(Spec::basis_entry_is_consistent(Spec::kTable.transition_basis[0][0])); // 正控
    }

    // ── 反向 fixture ⑩:一手原文格的 `ref` 改成无定位表述 ⇒ 必红 ──────
    {
        Table t = copy_table();
        t.transition_basis[2][4].ref = "参见设计文档";
        CHECK_FALSE(Spec::basis_entry_is_consistent(t.transition_basis[2][4]));
        CHECK(Spec::basis_entry_is_consistent(Spec::kTable.transition_basis[2][4])); // 正控
    }

    // ── 反向 fixture ⑪:改动载体 ⇒ 统计与推导记录不一致 ⇒ 必红 ────────
    {
        Table t = copy_table();
        t.transition_basis[0][0].pending = false; // 抹掉一个待追认标记
        CHECK(Spec::pending_count(t) != Spec::kExpectedPendingCount);
        CHECK(Spec::pending_count(Spec::kTable) == Spec::kExpectedPendingCount); // 正控
    }
}

// ══════════════════════════════════════════════════════════════════════
//  MS-04 —— 2 项语义损失 SHALL 被显式登记
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("MESIF 规约:2 项语义损失显式登记 + 措辞可机器区分(MS-04)", "[cep][mesif-state-spec]") {
    // ── 损失 ①:`F` 与 `M` 同投影 `WRITEBACK` ─────────────────────────
    CHECK(merge_note_satisfies(Spec::kPermMergeNote));
    // 对照说明**必须**出现 `chi_rnf` 的「不可补偿」口径(与主体登记分离)
    CHECK(Spec::contains(Spec::kPermMergeNoteChiRnfContrast, "chi_rnf"));
    CHECK(Spec::contains(Spec::kPermMergeNoteChiRnfContrast, "不可补偿"));
    // 主体登记**不得**混入对照口径(否则会被读成「本实例不可补偿」)
    CHECK_FALSE(Spec::contains(Spec::kPermMergeNote, "不可补偿"));
    // `F` 与 `M` **确实**同投影(登记的前提)
    CHECK(Spec::kTable.projection[3] == Spec::kTable.projection[4]);
    CHECK(Spec::kTable.projection[4] == Spec::Perm::WRITEBACK);

    // ── 损失 ②:`F → E` 的秩回落 ─────────────────────────────────────
    CHECK(rank_note_satisfies(Spec::kRankRegressionNote));
    // 秩回落**确实**存在(前提):`F`(WRITEBACK) → `E`(UNIQUE)
    const std::uint8_t e_idx = static_cast<std::uint8_t>(Spec::State::E);
    const std::uint8_t f_idx = static_cast<std::uint8_t>(Spec::State::F);
    CHECK(Spec::perm_rank(Spec::kTable.projection[f_idx]) >
          Spec::perm_rank(Spec::kTable.projection[e_idx]));
    CHECK(Spec::kTable.transition[f_idx][static_cast<std::size_t>(Spec::Event::RdUnique)] == e_idx);

    // ── 反向 fixture ⑫:登记缺「可由 Sharer Tracker 补偿」的限定 ⇒ 必红 ──
    {
        const char* broken = "F 与 M 合并投影为 WRITEBACK:信息丢失。";
        CHECK_FALSE(merge_note_satisfies(broken));
        CHECK(merge_note_satisfies(Spec::kPermMergeNote)); // 正控
    }

    // ── 反向 fixture ⑬:登记混入 `chi_rnf` 的「不可补偿」口径 ⇒ 必红 ──
    {
        const char* broken = "F 与 M 合并投影为 WRITEBACK:该损失不可补偿(RN 侧不维护 sharer set)。";
        CHECK_FALSE(merge_note_satisfies(broken));
        CHECK(merge_note_satisfies(Spec::kPermMergeNote)); // 正控
    }

    // ── 反向 fixture ⑭:登记缺次生损失(秩回落)⇒ 必红 ─────────────────
    {
        const char* broken = "F 与 M 合并投影为 WRITEBACK。";
        CHECK_FALSE(rank_note_satisfies(broken));
        CHECK(rank_note_satisfies(Spec::kRankRegressionNote)); // 正控
    }
}

// ══════════════════════════════════════════════════════════════════════
//  MS-05 —— 与协议模板的偏离 SHALL 被显式登记
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("MESIF 规约:偏离清单与载体逐格一致(MS-05)", "[cep][mesif-state-spec]") {
    CHECK(Spec::deviations_are_consistent(Spec::kTable));
    CHECK(Spec::kDeviationCount == 8u);

    // 每条登记都**真偏离**:实际目标态 ≠ 模板取值
    for (std::size_t i = 0; i < Spec::kDeviationCount; ++i) {
        const auto& d = Spec::kDeviations[i];
        INFO("偏离: " << Spec::state_name(d.state) << " + " << Spec::event_name(d.event));
        const std::size_t si = static_cast<std::size_t>(d.state);
        const std::size_t ei = static_cast<std::size_t>(d.event);
        const char* actual =
            Spec::state_name(static_cast<Spec::State>(Spec::kTable.transition[si][ei]));
        CHECK_FALSE(Spec::streq(actual, d.template_target));
        REQUIRE(d.reason != nullptr);
        CHECK(d.reason[0] != '\0');
    }

    // `F` 行的 5 格(除 `F + Rd`)必须**全部**登记
    std::size_t f_listed = 0;
    for (std::size_t i = 0; i < Spec::kDeviationCount; ++i) {
        if (Spec::kDeviations[i].state == Spec::State::F)
            ++f_listed;
    }
    CHECK(f_listed == 5u);

    // ── 反向 fixture ⑮:删除清单中的一条偏离 ⇒ 必红 ───────────────────
    {
        Spec::Deviation devs[Spec::kDeviationCount - 1];
        for (std::size_t i = 0; i + 1 < Spec::kDeviationCount; ++i)
            devs[i] = Spec::kDeviations[i];
        CHECK_FALSE(Spec::deviations_are_consistent(Spec::kTable, devs));
        CHECK(Spec::deviations_are_consistent(Spec::kTable)); // 正控
    }

    // ── 反向 fixture ⑯:清单登记的取值与载体不符 ⇒ 必红 ───────────────
    {
        Spec::Deviation devs[Spec::kDeviationCount];
        for (std::size_t i = 0; i < Spec::kDeviationCount; ++i)
            devs[i] = Spec::kDeviations[i];
        // 把 D1 的模板取值改成与实际目标态同名 ⇒ 不再是「真偏离」
        devs[0].template_target = Spec::state_name(static_cast<Spec::State>(
            Spec::kTable.transition[0][static_cast<std::size_t>(Spec::Event::SnpShared)]));
        CHECK_FALSE(Spec::deviations_are_consistent(Spec::kTable, devs));
        CHECK(Spec::deviations_are_consistent(Spec::kTable)); // 正控
    }
}

// ══════════════════════════════════════════════════════════════════════
//  MS-06 —— 逐格追认清单 SHALL 与**载体**逐格一致
// ══════════════════════════════════════════════════════════════════════
namespace {

    const char* perm_name(Spec::Perm p) {
        switch (p) {
        case Spec::Perm::NONE:
            return "NONE";
        case Spec::Perm::READ:
            return "READ";
        case Spec::Perm::UNIQUE:
            return "UNIQUE";
        case Spec::Perm::WRITEBACK:
            return "WRITEBACK";
        }
        return "?";
    }

    /// 清单与载器的**比对谓词**(返回违规描述;空 = 逐格一致)
    std::vector<std::string> checklist_mismatches(const std::vector<ChecklistRow>& tr,
                                                  const std::vector<ChecklistRow>& pr) {
        std::vector<std::string> bad;
        if (tr.size() != Spec::kStateCount * Spec::kEventCount)
            bad.push_back("转换表行数不是 30");
        for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
            for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
                const std::size_t idx = s * Spec::kEventCount + e;
                if (idx >= tr.size()) {
                    bad.push_back("转换表缺行");
                    continue;
                }
                const auto& row = tr[idx];
                const auto& b = Spec::kTable.transition_basis[s][e];
                const char* to =
                    Spec::state_name(static_cast<Spec::State>(Spec::kTable.transition[s][e]));
                if (row.c1 != Spec::state_name(static_cast<Spec::State>(s)))
                    bad.push_back("态不符");
                if (row.c2 != Spec::event_name(static_cast<Spec::Event>(e)))
                    bad.push_back("事件不符");
                if (row.c3 != to)
                    bad.push_back("目标态不符");
                if (row.kind != Spec::basis_kind_name(b.kind))
                    bad.push_back("kind 不符");
                if (row.pending != (b.pending ? "yes" : "no"))
                    bad.push_back("pending 不符");
            }
        }
        if (pr.size() != Spec::kStateCount)
            bad.push_back("投影表行数不是 5");
        for (std::size_t s = 0; s < Spec::kStateCount && s < pr.size(); ++s) {
            const auto& row = pr[s];
            const auto& b = Spec::kTable.projection_basis[s];
            if (row.c1 != Spec::state_name(static_cast<Spec::State>(s)))
                bad.push_back("投影态不符");
            if (row.c2 != perm_name(Spec::kTable.projection[s]))
                bad.push_back("perm 不符");
            if (row.kind != Spec::basis_kind_name(b.kind))
                bad.push_back("投影 kind 不符");
            if (row.pending != (b.pending ? "yes" : "no"))
                bad.push_back("投影 pending 不符");
        }
        return bad;
    }

} // namespace

TEST_CASE("MESIF 规约:逐格追认清单与载体逐格一致(MS-06)", "[cep][mesif-state-spec]") {
    const std::string doc =
        cpptlm::cep::read_file((cpptlm::cep::repo_root() / "docs" / "implementation" /
                                "2026-10-29-cep-mesif-state-spec-recognition-checklist.md")
                                   .string());
    REQUIRE_FALSE(doc.empty());

    const auto tr = parse_checklist_table(
        doc, "| # | 态 | 事件 | 目标态 | kind | pending | ArchForge 需确认什么 |", 5, 6);
    const auto pr = parse_checklist_table(
        doc, "| # | 态 | perm | kind | pending | ArchForge 需确认什么 |", 4, 5);

    // ── ① ~ ④ 正控:清单与载体**零差异** + 待追认刻度唯一 ──────────────
    const auto mismatches = checklist_mismatches(tr, pr);
    for (const auto& m : mismatches)
        INFO("清单与载体不一致: " << m);
    CHECK(mismatches.empty());

    std::size_t pending_rows = 0;
    for (const auto& r : tr) {
        if (r.pending == "yes")
            ++pending_rows;
    }
    for (const auto& r : pr) {
        if (r.pending == "yes")
            ++pending_rows;
    }
    CHECK(pending_rows == Spec::pending_count(Spec::kTable));
    CHECK(pending_rows == 31u);

    // ── 反向 fixture ⑰:清单某行的 `kind` 被改 ⇒ 必红 ──────────────────
    {
        auto broken = tr;
        REQUIRE(!broken.empty());
        broken[0].kind = "Primary";
        CHECK_FALSE(checklist_mismatches(broken, pr).empty());
        CHECK(checklist_mismatches(tr, pr).empty()); // 正控
    }

    // ── 反向 fixture ⑱:清单少一行 ⇒ 必红 ─────────────────────────────
    {
        auto broken = tr;
        broken.pop_back();
        CHECK_FALSE(checklist_mismatches(broken, pr).empty());
        CHECK(checklist_mismatches(tr, pr).empty()); // 正控
    }
}
