// =============================================================================
// test/test_cep_tilelink_state_spec.cc
//   —— TileLink-C(TileLink Tree Manager)规约载体的契约测试 + 反向 fixture
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/tilelink_state_spec.hh
//       docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md(basis 唯一人工审计面)
//       openspec/changes/cpptlm-cep-tilelink-state-spec/(delta spec TSS-01 ~ TSS-08)
//       台账 obj-cep-platform.md Entry #140(change 建档)· #141(二审批 2)
//
// ── 判据纪律(per 本仓 4 条铁律)──────────────────────────────────────────
// ① 纯负向断言 SHALL 有正控 ⇒ 每条负向断言都有**正向**对照;
// ② 每条判据 SHALL 有**可执行反向 fixture** 且**因正确原因红**;
// ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据 ⇒ 载体的结构校验
//    提供**运行时可判**入口(`*_for(...)` 接受任意矩阵 / 名字表);
// ④ fixture 状态变迁后 SHALL 重新基线化 —— 本文件为**新增**,基线 = 全绿。
//
// ── 覆盖映射(delta spec → TEST_CASE)──────────────────────────────────────
// TSS-01 稳定态集合(恰 3 + 逐字 + `Trunk` 缺席)      → ①
// TSS-02 事件词表(恰 4 + 逐字)                        → ②
// TSS-03 转移矩阵(12 格 + 5 个分桶分别断言)            → ③
// TSS-04 `perm` 投影(恰 3 行 + `UNIQUE` 不出现)        → ④
// TSS-05 `perm` 值域维持 4 值锁 + AC-05 族登记         → ⑤
// TSS-06 非对称断言(TileLink 形态)                    → ⑥
// TSS-07 逐格 `basis`(direct + pending=false + 条目号) → ⑦
// TSS-08 偏离清单(5 条)                              → ⑧
// =============================================================================

#include "tlm/cep/tilelink_state_spec.hh"

#include "catch_amalgamated.hpp"

#include "cep_anchor_ids.hh"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

    using Spec = tlm::cep::TileLinkStateSpec;
    using State = Spec::State;
    using Event = Spec::Event;
    using CellKind = Spec::CellKind;
    using Transition = Spec::Transition;
    using AsymEntry = Spec::AsymEntry;
    using AsymMsg = Spec::AsymMsg;

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

    /// 锚点摘要(basis 唯一人工审计面)。
    std::string anchor_summary_text() {
        return read_file_or_empty(
            repo_root_dir() +
            "/docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md");
    }

    /// 载体的 12 个 `ref`(按矩阵行序:event-major × state-minor)。
    std::vector<std::string> carrier_refs() {
        std::vector<std::string> out;
        for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
            for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
                out.emplace_back(Spec::kTransitions[e][s].ref);
            }
        }
        return out;
    }

    /// 构造一份**毒化**矩阵(改动一格),供反向 fixture 使用。
    /// ⚠️ 必须**深拷贝**后再改 —— `constexpr` 表不可就地修改。
    std::array<std::array<Transition, Spec::kStateCount>, Spec::kEventCount> poisoned_matrix() {
        std::array<std::array<Transition, Spec::kStateCount>, Spec::kEventCount> m{};
        for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
            for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
                m[e][s] = Spec::kTransitions[e][s];
            }
        }
        return m;
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ① TSS-01 —— 稳定态集合
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: 稳定态集合恰为 3 且逐字 Nothing/Branch/Tip",
              "[cep][tilelink][state-spec]") {
        // 正控
        CHECK(Spec::stable_states_are_exactly_nothing_branch_tip());
        CHECK(Spec::trunk_absent_from_stable_states());
        REQUIRE(Spec::kStableStateNames.size() == 3u);
        CHECK(std::string(Spec::kStableStateNames[0]) == "Nothing");
        CHECK(std::string(Spec::kStableStateNames[1]) == "Branch");
        CHECK(std::string(Spec::kStableStateNames[2]) == "Tip");

        // ── 反向 fixture ①:把 `Tip` 换成 `Trunk`(**仍是 3 个**)⇒ 必须红 ──
        // ⚠️ 这条是「逐字断言才有鉴别力」的**证明**:只断言「恰为 3 个」的话,这条 fixture
        // **不会红**。
        const std::array<const char*, 3> swapped{"Nothing", "Branch", "Trunk"};
        CHECK_FALSE(Spec::stable_states_are_exactly_nothing_branch_tip_for(swapped));
        // ⚠️ 而「恰为 3 个」的口径对它**无鉴别力** ⇒ 两者必须**分别**断言。
        CHECK(swapped.size() == 3u);

        // ── 反向 fixture ②:加回 `Trunk` ⇒ 必须红 ──
        const std::array<const char*, 4> with_trunk{"Nothing", "Branch", "Tip", "Trunk"};
        CHECK_FALSE(Spec::stable_states_are_exactly_nothing_branch_tip_for(with_trunk));
        CHECK_FALSE(Spec::trunk_absent_from_stable_states_for(with_trunk));

        // ── 正控(形状层):原表 `Trunk` 确实缺席 ──
        CHECK(Spec::trunk_absent_from_stable_states_for(Spec::kStableStateNames));
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ② TSS-02 —— 事件词表
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: 事件词表恰为 4 且逐字 Cap/Grow/Prune/Report",
              "[cep][tilelink][state-spec]") {
        CHECK(Spec::events_are_exactly_cap_grow_prune_report());
        REQUIRE(Spec::kEventNames.size() == 4u);
        CHECK(std::string(Spec::kEventNames[0]) == "Cap");
        CHECK(std::string(Spec::kEventNames[1]) == "Grow");
        CHECK(std::string(Spec::kEventNames[2]) == "Prune");
        CHECK(std::string(Spec::kEventNames[3]) == "Report");

        // ── 反向 fixture:把 `Report` 换成 `Query` ⇒ 必须红 ──
        const std::array<const char*, 4> swapped{"Cap", "Grow", "Prune", "Query"};
        CHECK_FALSE(Spec::events_are_exactly_cap_grow_prune_report_for(swapped));
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ③ TSS-03 —— 转移矩阵(12 格 + 5 个分桶**分别**断言)
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: 转移矩阵 12 格且分桶 5/5/2/0 分别可判",
              "[cep][tilelink][state-spec]") {
        CHECK(Spec::kTransitionCellCount == 12u);
        CHECK(Spec::transitions_are_valid());
        CHECK(Spec::counts_are_consistent());

        // ⚠️ 铁律 ③:编译期锁**同时**有运行时断言 ⇒ 这里逐个复核 5 个派生量。
        CHECK(Spec::count_kind(CellKind::Known) == 12u);
        CHECK(Spec::count_kind(CellKind::Unknown) == 0u);
        CHECK(Spec::count_popcount(0) == 2u); // empty
        CHECK(Spec::count_popcount(1) == 5u); // single
        CHECK(Spec::count_popcount(2) == 2u); // multi(2 位):Grow｜N + Prune｜T
        CHECK(Spec::count_popcount(3) == 3u); // multi(3 位):Cap × 3

        // 逐格核对:12 格的目标掩码与分桶
        // Cap 行:3 格都是 3 位(multi)
        for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
            const auto c = Spec::transition(static_cast<State>(s), Event::Cap);
            CHECK(c.kind == CellKind::Known);
            CHECK(Spec::is_multi_cell(c));
            CHECK(c.target_mask == Spec::kMaskAllStates);
        }
        // Grow 行:源 Nothing ⇒ 2 位;源 Branch ⇒ 1 位;源 Tip ⇒ **空集**
        CHECK(Spec::is_multi_cell(Spec::transition(State::Nothing, Event::Grow)));
        CHECK(Spec::is_single_cell(Spec::transition(State::Branch, Event::Grow)));
        CHECK(Spec::is_empty_cell(Spec::transition(State::Tip, Event::Grow)));
        // Prune 行:源 Nothing ⇒ **空集**;源 Branch ⇒ 1 位;源 Tip ⇒ 2 位
        CHECK(Spec::is_empty_cell(Spec::transition(State::Nothing, Event::Prune)));
        CHECK(Spec::is_single_cell(Spec::transition(State::Branch, Event::Prune)));
        CHECK(Spec::is_multi_cell(Spec::transition(State::Tip, Event::Prune)));
        // Report 行:3 格都是 1 位(single)
        for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
            CHECK(Spec::is_single_cell(Spec::transition(static_cast<State>(s), Event::Report)));
        }

        // ── 反向 fixture ①:某格改成 `Unknown` ⇒ 必须红 ──
        // ⚠️ 计数层红(`Unknown` 数 0 → 1);形状层**不**红(`Unknown` 格形状合法)⇒ 二者**分别**可判。
        {
            auto m = poisoned_matrix();
            m[static_cast<std::size_t>(Event::Grow)][static_cast<std::size_t>(State::Branch)] =
                Transition{CellKind::Unknown,    0, Spec::BasisKind::Direct, false, "",
                           "毒化:假装规范未覆盖"};
            CHECK(Spec::count_kind_for(m, CellKind::Unknown) == 1u);
            CHECK_FALSE(Spec::counts_are_consistent_for(m));
        }

        // ── 反向 fixture ②:空集格数改成 3 ⇒ 必须红 ──
        {
            auto m = poisoned_matrix();
            m[static_cast<std::size_t>(Event::Grow)][static_cast<std::size_t>(State::Tip)]
                .target_mask = Spec::kMaskTip; // 把「空集」变成单值 ⇒ empty 2 → 1
            CHECK(Spec::count_popcount_for(m, 0) == 1u);
            CHECK_FALSE(Spec::counts_are_consistent_for(m));
        }

        // ── 反向 fixture ③:`single` ↔ `multi` 互移一格 ⇒ 必须红 ──
        // ⚠️ 这条证明「5 / 5 分桶」有鉴别力 —— 只断言「12 格」**抓不到**它。
        {
            auto m = poisoned_matrix();
            m[static_cast<std::size_t>(Event::Grow)][static_cast<std::size_t>(State::Nothing)]
                .target_mask = Spec::kMaskTip; // 2 位 → 1 位 ⇒ single 5→6 / multi 5→4
            CHECK(Spec::count_popcount_for(m, 1) == 6u);
            CHECK(Spec::count_popcount_for(m, 2) == 1u); // 2 位格 2 → 1
            CHECK_FALSE(Spec::counts_are_consistent_for(m));
        }

        // ── 正控 ⑧:空集是**独立的第三类**格形(既非 single 也非 multi,也非 Unknown)──
        {
            const Transition empty_cell{CellKind::Known, 0,        Spec::BasisKind::Direct,
                                        false,           "TLC-06", "正控"};
            CHECK(Spec::is_empty_cell(empty_cell));
            CHECK_FALSE(Spec::is_single_cell(empty_cell));
            CHECK_FALSE(Spec::is_multi_cell(empty_cell));
            CHECK(empty_cell.kind == CellKind::Known); // 「规范明确」⇒ 不是 Unknown
        }

        // ── 反向 fixture ④:`Unknown` 格携带非零掩码 ⇒ 必须红(零非法组合)──
        {
            auto m = poisoned_matrix();
            m[static_cast<std::size_t>(Event::Report)][static_cast<std::size_t>(State::Tip)] =
                Transition{
                    CellKind::Unknown, Spec::kMaskTip, Spec::BasisKind::Direct, false, "", "毒化"};
            CHECK_FALSE(Spec::transitions_are_valid_for(m));
        }

        // ── 反向 fixture ⑤:`Known` 格缺 `ref` ⇒ 必须红 ──
        {
            auto m = poisoned_matrix();
            m[static_cast<std::size_t>(Event::Report)][static_cast<std::size_t>(State::Tip)].ref =
                "";
            CHECK_FALSE(Spec::transitions_are_valid_for(m));
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ④ TSS-04 —— `perm` 投影
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: perm 投影恰为 3 行且 UNIQUE 不出现",
              "[cep][tilelink][state-spec]") {
        REQUIRE(Spec::kPermProjection.size() == 3u);
        CHECK(Spec::perm_of(State::Nothing) == Spec::Perm::NONE);
        CHECK(Spec::perm_of(State::Branch) == Spec::Perm::READ);
        CHECK(Spec::perm_of(State::Tip) == Spec::Perm::WRITEBACK);

        // `UNIQUE` SHALL NOT 出现在投影结果中(逐行查)
        for (std::size_t i = 0; i < Spec::kPermProjection.size(); ++i) {
            CHECK(Spec::kPermProjection[i] != Spec::Perm::UNIQUE);
        }
        // 值域仍为 4 值(不扩)—— 只消费,不改
        CHECK(Spec::kPermCount == 4u);

        // ── 反向 fixture:投影里加回 `UNIQUE` ⇒ 必须红 ──
        {
            std::array<Spec::Perm, 3> poisoned{Spec::Perm::NONE, Spec::Perm::READ,
                                               Spec::Perm::UNIQUE};
            bool has_unique = false;
            for (const auto p : poisoned) {
                if (p == Spec::Perm::UNIQUE) {
                    has_unique = true;
                }
            }
            CHECK(has_unique);
            // 正控:原表无 UNIQUE
            bool orig_has_unique = false;
            for (const auto p : Spec::kPermProjection) {
                if (p == Spec::Perm::UNIQUE) {
                    orig_has_unique = true;
                }
            }
            CHECK_FALSE(orig_has_unique);
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ⑤ TSS-05 —— `perm` 值域 4 值锁 + AC-05 族不兼容**已登记**
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: perm 值域 4 值锁不变 + AC-05 族不兼容已登记",
              "[cep][tilelink][state-spec]") {
        // 值域维持 4 值(不新增第 5 值)
        CHECK(Spec::kPermCount == 4u);
        CHECK(std::string(Spec::perm_name(Spec::Perm::NONE)) == "NONE");
        CHECK(std::string(Spec::perm_name(Spec::Perm::READ)) == "READ");
        CHECK(std::string(Spec::perm_name(Spec::Perm::UNIQUE)) == "UNIQUE");
        CHECK(std::string(Spec::perm_name(Spec::Perm::WRITEBACK)) == "WRITEBACK");

        // `UNIQUE` 不可达 ⇒ 该实例**不声明**静默升级 ⇒ 不触发 `INV-CEP-08-IC2`
        // (规约层**只记录**该事实;实例声明属第 1 个 change)
        bool unique_reachable = false;
        for (std::size_t i = 0; i < Spec::kPermProjection.size(); ++i) {
            if (Spec::kPermProjection[i] == Spec::Perm::UNIQUE) {
                unique_reachable = true;
            }
        }
        CHECK_FALSE(unique_reachable);

        // ⚠️ AC-05 族不兼容**必须**在偏离清单里登记(否则实现层会填假值换绿灯)
        bool ac05_registered = false;
        for (const auto& d : Spec::kDeviations) {
            const std::string t(d.text);
            if (t.find("AC-05") != std::string::npos) {
                ac05_registered = true;
            }
        }
        CHECK(ac05_registered);
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ⑥ TSS-06 —— 非对称断言(TileLink 形态)
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: 非对称断言为 TileLink 形态(非 CXL 形态)",
              "[cep][tilelink][state-spec]") {
        CHECK(Spec::asymmetry_is_tilelink_form());
        // CXL 形态(「request 类消息只在一个方向」)对 TileLink **恒假**
        CHECK_FALSE(Spec::cxl_style_request_direction_would_hold_for(Spec::kAsymmetryMessages));

        // ── 反向 fixture:换成 CXL 形态(把 `Acquire` 挪到 B 通道)⇒ 必须红 ──
        {
            std::array<AsymEntry, 5> cxl_form = Spec::kAsymmetryMessages;
            for (auto& e : cxl_form) {
                if (e.msg == AsymMsg::Acquire) {
                    e.channel_mask = Spec::kChB;
                }
            }
            // CXL 形态此时**成立** ⇒ 证明该 fixture 命中的是「③ request 单向」那一条
            CHECK(Spec::cxl_style_request_direction_would_hold_for(cxl_form));
            // ⚠️ 而 TileLink 形态判据必须**红**
            CHECK_FALSE(Spec::asymmetry_is_tilelink_form_for(cxl_form));
        }

        // ── 反向 fixture:把 `Grant` 挪到 A 通道(违反「只由 slave 授予」)⇒ 必须红 ──
        {
            std::array<AsymEntry, 5> poisoned = Spec::kAsymmetryMessages;
            for (auto& e : poisoned) {
                if (e.msg == AsymMsg::Grant) {
                    e.channel_mask = Spec::kChA;
                }
            }
            CHECK_FALSE(Spec::asymmetry_is_tilelink_form_for(poisoned));
        }

        // ── 反向 fixture:把 `Probe` 挪到 D 通道(违反「只由 slave 强制撤销」)⇒ 必须红 ──
        {
            std::array<AsymEntry, 5> poisoned = Spec::kAsymmetryMessages;
            for (auto& e : poisoned) {
                if (e.msg == AsymMsg::Probe) {
                    e.channel_mask = Spec::kChD;
                }
            }
            CHECK_FALSE(Spec::asymmetry_is_tilelink_form_for(poisoned));
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ⑦ TSS-07 —— 逐格 `basis`(direct + pending=false + 指向**真实条目号**)
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: 逐格 basis 全 direct + pending=false + 条目号可解析",
              "[cep][tilelink][state-spec]") {
        // 正控:逐格断言
        for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
            for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
                const auto c = Spec::kTransitions[e][s];
                CHECK(c.kind == CellKind::Known);
                CHECK(c.basis == Spec::BasisKind::Direct);
                CHECK_FALSE(c.pending);
                REQUIRE(c.ref != nullptr);
                CHECK(std::string(c.ref).rfind("TLC-", 0) == 0u);
                REQUIRE(c.note != nullptr);
                CHECK(std::string(c.note).size() > 0u);
                // ⚠️ 合规:载体 SHALL **不**含规范正文的**整段复制** ⇒ note 只记**条款级定位**
                CHECK(std::string(c.note).find("D1 §8.1.4") != std::string::npos);
            }
        }

        // 逐格 `ref` ↔ 锚点摘要的**逐格条目号**一一对应(AC-6 的载体侧)
        const std::string anchor = anchor_summary_text();
        REQUIRE_FALSE(anchor.empty());
        const auto anchor_ids = cpptlm::cep::collect_ids(anchor, "TLC-");
        const auto refs = carrier_refs();
        CHECK(cpptlm::cep::refs_resolve_one_to_one(refs, anchor_ids));

        // ── ID 文法:容忍 Markdown 粗体 + 接受 `R-T<n>` 前缀(B-7 的 3 件事)──
        {
            const std::string synthetic =
                "| 条目号 | 值 |\n|---|---|\n| **TLC-01** | 粗体形态 |\n| TLC-02 | 无粗体 |\n"
                "| **R-T1** | 残余风险 |\n| **D1** | 来源 |\n";
            const auto tlc = cpptlm::cep::collect_ids(synthetic, "TLC-");
            CHECK(tlc.size() == 2u); // 粗体与无粗体**等价**
            CHECK(tlc.count("TLC-01") == 1u);
            CHECK(tlc.count("TLC-02") == 1u);
            const auto rt = cpptlm::cep::collect_ids(synthetic, "R-T");
            CHECK(rt.size() == 1u);
            CHECK(rt.count("R-T1") == 1u);
            const auto d = cpptlm::cep::collect_ids(synthetic, "D");
            CHECK(d.size() == 1u);
            CHECK(d.count("D1") == 1u);
        }

        // ── 反向 fixture ①:锚点少一条 ⇒ 必须红 ──
        {
            std::string poisoned = anchor;
            const std::string victim = "| **TLC-07** |";
            const std::size_t at = poisoned.find(victim);
            REQUIRE(at != std::string::npos);
            poisoned.replace(at, victim.size(), "| **TLC-99** |");
            const auto ids2 = cpptlm::cep::collect_ids(poisoned, "TLC-");
            CHECK_FALSE(cpptlm::cep::refs_resolve_one_to_one(refs, ids2));
        }

        // ── 反向 fixture ②:载体侧引用锚点里**不存在**的条目号 ⇒ 必须红 ──
        {
            std::vector<std::string> bad = refs;
            bad[0] = "TLC-99";
            CHECK_FALSE(cpptlm::cep::refs_resolve_one_to_one(bad, anchor_ids));
        }

        // ── 反向 fixture ③:载体侧重复引用同一条目号 ⇒ 必须红 ──
        {
            std::vector<std::string> dup = refs;
            dup[1] = dup[0];
            CHECK_FALSE(cpptlm::cep::refs_resolve_one_to_one(dup, anchor_ids));
        }

        // ── 反向 fixture ④:ID 后缀非全数字 ⇒ **不得**被收(文法收紧)──
        {
            const std::string synthetic = "| **TLC-0x** | 非数字后缀 |\n| **TLC-01** | 正常 |\n";
            const auto tlc = cpptlm::cep::collect_ids(synthetic, "TLC-");
            CHECK(tlc.size() == 1u);
            CHECK(tlc.count("TLC-01") == 1u);
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    // ⑧ TSS-08 —— 偏离清单
    // ══════════════════════════════════════════════════════════════════════════
    TEST_CASE("CEP TileLinkStateSpec: 偏离清单登记 5 条(矛盾显式保留)",
              "[cep][tilelink][state-spec]") {
        REQUIRE(Spec::kDeviations.size() == 5u);
        // 5 条的**关键 token** 各自在位
        bool has_six_subsets = false; // DEV-1「six subsets」矛盾
        bool has_legend = false;      // DEV-2 图例行笔误
        bool has_unique = false;      // DEV-3 UNIQUE 不可达
        bool has_trunk = false;       // DEV-4 Trunk 折叠的语义损失
        bool has_ac05 = false;        // DEV-5 AC-05 族恒红
        for (const auto& d : Spec::kDeviations) {
            const std::string t(d.text);
            REQUIRE(d.id != nullptr);
            REQUIRE(std::string(d.id).rfind("DEV-", 0) == 0u);
            if (t.find("six subsets") != std::string::npos) {
                has_six_subsets = true;
            }
            if (t.find("图例行") != std::string::npos) {
                has_legend = true;
            }
            if (t.find("UNIQUE") != std::string::npos) {
                has_unique = true;
            }
            if (t.find("Trunk") != std::string::npos) {
                has_trunk = true;
            }
            if (t.find("AC-05") != std::string::npos) {
                has_ac05 = true;
            }
        }
        CHECK(has_six_subsets);
        CHECK(has_legend);
        CHECK(has_unique);
        CHECK(has_trunk);
        CHECK(has_ac05);

        // 条目号 SHALL 唯一
        std::set<std::string> ids;
        for (const auto& d : Spec::kDeviations) {
            CHECK(ids.insert(std::string(d.id)).second);
        }
    }

} // namespace
    // ══════════════════════════════════════════════════════════════════════════
    // T3 —— 「CohIf 平台消息/意图面 → 载体事件轴」映射(per 台账 Entry #241)
    //   审计面:docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md
    //   载体  :include/tlm/cep/tilelink_state_spec.hh 的 kT3MappingCells
    //   判据源:07 号 Status Update(2026-11-06) §⑤ 的 T1~T4 判定式(T3 = 可消费)
    // ⚠️ 判据纪律:每条负向断言 SHALL 有正控;每条判据 SHALL 有**可执行反向 fixture**
    //   且**因正确原因红**;编译期锁 SHALL **同时**有运行时可判入口。
    // ══════════════════════════════════════════════════════════════════════════
    namespace {
        using PlatformIntent = Spec::PlatformIntent;
        using T3BasisKind = Spec::T3BasisKind;
        using T3MappingCell = Spec::T3MappingCell;

        std::string t3_anchor_summary_text() {
            return read_file_or_empty(
                repo_root_dir() +
                "/docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md");
        }

        /// 锚点摘要里的 `**TLC-M-NN**` 行 id 集合(X-2 元测试形态)
        std::vector<std::string> t3_anchor_row_ids(const std::string& text) {
            std::vector<std::string> out;
            const std::string key = "**TLC-M-";
            std::size_t pos = 0;
            while (true) {
                const auto at = text.find(key, pos);
                if (at == std::string::npos)
                    break;
                std::size_t j = at + key.size();
                std::string num;
                while (j < text.size() && text[j] >= '0' && text[j] <= '9')
                    num.push_back(text[j++]);
                if (!num.empty()) {
                    const std::string id = "TLC-M-" + num;
                    if (std::find(out.begin(), out.end(), id) == out.end())
                        out.push_back(id);
                }
                pos = j;
            }
            std::sort(out.begin(), out.end());
            return out;
        }

        /// 载体里的 `row_ref` 集合
        std::vector<std::string> t3_carrier_row_ids() {
            std::vector<std::string> out;
            for (const auto& c : Spec::kT3MappingCells)
                out.emplace_back(c.row_ref);
            std::sort(out.begin(), out.end());
            return out;
        }

        /// 构造一份**毒化**的 T3 行表(供反向 fixture;⚠️ 必须深拷贝后再改)
        std::array<T3MappingCell, Spec::kPlatformIntentCount> t3_poisoned_rows() {
            std::array<T3MappingCell, Spec::kPlatformIntentCount> rows{};
            for (std::size_t i = 0; i < Spec::kT3MappingCells.size(); ++i)
                rows[i] = Spec::kT3MappingCells[i];
            return rows;
        }
    } // namespace

    TEST_CASE("CEP TileLinkStateSpec: T3 行宇宙完整(11 行 · intent 唯一且全覆盖 · row_ref 唯一)",
              "[cep][tilelink-state-spec][t3]") {
        // ① 行数 = canonical 10 流 + atomic 意图
        REQUIRE(Spec::kPlatformIntentCount == 11u);
        REQUIRE(Spec::t3_row_count_is_exact());
        REQUIRE(Spec::kT3MappingCells.size() == 11u);

        // ② intent 唯一且覆盖 0 ~ 10(⛔ 不抽样)
        REQUIRE(Spec::t3_intents_are_unique_and_complete());
        for (std::size_t i = 0; i < Spec::kT3MappingCells.size(); ++i) {
            INFO("T3 行 " << i << " 的 intent 越界");
            REQUIRE(static_cast<std::size_t>(Spec::kT3MappingCells[i].intent) == i);
        }

        // ③ row_ref 唯一且非空(每行一个锚点摘要行 id)
        REQUIRE(Spec::t3_row_refs_are_unique());
        for (const auto& c : Spec::kT3MappingCells) {
            INFO("T3 行的 row_ref 为空: " << c.row_ref);
            REQUIRE(c.row_ref != nullptr);
            REQUIRE(std::string(c.row_ref).rfind("TLC-M-", 0) == 0u);
        }
    }

    TEST_CASE("CEP TileLinkStateSpec: T3 逐行 basis↔事件 / ref 非空 / 掩码范围自洽",
              "[cep][tilelink-state-spec][t3]") {
        REQUIRE(Spec::t3_rows_are_valid_for(Spec::kT3MappingCells));
        REQUIRE(Spec::t3_pending_rows_declare_no_event());
        REQUIRE(Spec::t3_non_pending_rows_have_ref());
        REQUIRE(Spec::t3_event_masks_are_in_range());

        // 本轮普查结论:**全行有条款级锚** ⇒ `Pending` 行数 = 0
        // (per 锚点摘要 §6;若后续普查出现无锚行 ⇒ 本断言须随裁决改写,⛔ 不得静默放宽)
        std::size_t n_pending = 0;
        for (const auto& c : Spec::kT3MappingCells)
            if (c.basis == T3BasisKind::Pending)
                ++n_pending;
        INFO("T3 的 Pending 行数 = " << n_pending << "(本轮普查结论:全行有锚 ⇒ 0)");
        REQUIRE(n_pending == 0u);
        for (const auto& c : Spec::kT3MappingCells) {
            INFO("T3 行 " << c.row_ref << " 的 basis 非 Direct");
            REQUIRE(c.basis == T3BasisKind::Direct);
        }
    }

    TEST_CASE("CEP TileLinkStateSpec: T3 锚点摘要 ↔ 载体 逐行相等(X-2 形态)",
              "[cep][tilelink-state-spec][t3]") {
        const std::string anchor = t3_anchor_summary_text();
        REQUIRE_FALSE(anchor.empty());

        const auto anchor_ids = t3_anchor_row_ids(anchor);
        const auto carrier_ids = t3_carrier_row_ids();
        REQUIRE_FALSE(anchor_ids.empty());
        REQUIRE(anchor_ids.size() == carrier_ids.size());
        for (std::size_t i = 0; i < carrier_ids.size(); ++i) {
            INFO("锚点摘要行 id 集合 ≠ 载体 row_ref 集合(下标 " << i << ")");
            REQUIRE(anchor_ids[i] == carrier_ids[i]);
        }
    }

    TEST_CASE("CEP TileLinkStateSpec: T3 事件取值逐行钉死(映射不得被悄悄改)",
              "[cep][tilelink-state-spec][t3]") {
        // 逐行期望(依据见锚点摘要 §2):
        //   Request ∅(权限随 Grant 回来)· Completion Grow · Snoop Prune
        //   SnoopResp Prune(potentially)· Writeback Prune · 其余 5 流 ∅ · atomic 意图 ∅
        const std::array<std::uint8_t, Spec::kPlatformIntentCount> kExpected{{
            0,                              // Request
            Spec::kT3EventGrow,             // Completion
            Spec::kT3EventPrune,            // Snoop
            Spec::kT3EventPrune,            // SnoopResp
            Spec::kT3EventPrune,            // Writeback
            0, 0, 0, 0, 0,                  // ForwardData..SnoopCompletionAck
            0,                              // AtomicIntent
        }};
        REQUIRE(kExpected.size() == Spec::kT3MappingCells.size());
        for (std::size_t i = 0; i < kExpected.size(); ++i) {
            INFO("T3 行 " << Spec::kT3MappingCells[i].row_ref << " 的事件掩码漂移");
            REQUIRE(Spec::kT3MappingCells[i].event_mask == kExpected[i]);
            REQUIRE(Spec::t3_consumable_event_mask(
                        static_cast<PlatformIntent>(i)) == kExpected[i]);
        }
    }

    TEST_CASE("CEP TileLinkStateSpec: T3 fail-closed —— Pending 行不得被消费者消费(反向 fixture)",
              "[cep][tilelink-state-spec][t3]") {
        // 正控:干净表 ⇒ 全部行可消费(且全为 Direct ⇒ 掩码即声明)
        REQUIRE(Spec::t3_rows_are_valid_for(Spec::kT3MappingCells));

        // 反向 fixture ①:把某行改成 `Pending` **却**声明事件 ⇒ 表不自洽
        {
            auto bad = t3_poisoned_rows();
            bad[1].basis = T3BasisKind::Pending;  // Completion 行
            INFO("Pending 行却声明了事件 ⇒ 判据必须判否");
            REQUIRE_FALSE(Spec::t3_rows_are_valid_for(bad));
        }
        // 反向 fixture ②:`Pending` 行 ⇒ **可消费掩码必须归零**(fail-closed)
        {
            auto bad = t3_poisoned_rows();
            bad[1].basis = T3BasisKind::Pending;
            bad[1].event_mask = Spec::kT3EventGrow;  // 违规:pending 却带事件
            INFO("Pending 行的可消费掩码必须归零(fail-closed)");
            REQUIRE(Spec::t3_consumable_event_mask_for(bad, PlatformIntent::Completion) == 0u);
        }
        // 反向 fixture ③:`row_ref` 重复 ⇒ 表不自洽
        {
            auto bad = t3_poisoned_rows();
            bad[2].row_ref = bad[1].row_ref;
            INFO("row_ref 重复 ⇒ 判据必须判否");
            REQUIRE_FALSE(Spec::t3_rows_are_valid_for(bad));
        }
        // 反向 fixture ④:掩码含 4 事件之外的位 ⇒ 表不自洽
        {
            auto bad = t3_poisoned_rows();
            bad[0].event_mask = 0b1'0000;  // 第 5 位
            INFO("掩码越界 ⇒ 判据必须判否");
            REQUIRE_FALSE(Spec::t3_rows_are_valid_for(bad));
        }
    }


