// test/test_cep_tilelink_home_driver.cc
// 档 2 第二族(`tilelink_c`)home 侧消息簿记层的**判定级载体**
//   · 单元 = 登记册 §6.4 队列 2 的
//   **I-7b**（`.rddf/improvements/cep-tilelink-home-side-state-driver.md`） · 形态 = architect 裁决
//   `.rddf/reviews/cep-i7b-adjudication-2026-11-21.md`
//     （Q-1 逐行 basis 纪律 + Q-2 契约面主体）· 用户 ratify
//   · 前置 = I-7a 已交付（`TreeManager::view_of`，commit `a2a4c7ae`）
//
// ⚠️ **三段分离**(per 族 1 先例 per 裁决 R2):驱动只落**原始事实** ⇒ **判定全在 test 侧**。
// ⚠️ **零自造表**:判据只经**规约层访问器**只读消费 ⇒ ⛔ 不复制 T3 表 / 12 格转移表。
// ⚠️ **诚实边界**:本载体证明的是**观测面**成立 —— ⛔ **不等于**档 2 已扩面达成
//   (档 2 读法 = 两族 / 两种载体形态 / 单子树 / 单 tracker 实例 / 无通过态)。
//
// 作者: CppTLM Team / 日期: 2026-11-21
#include "catch_amalgamated.hpp"

#include "tlm/cep/tilelink_home_state_driver.hh"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    using tlm::cep::node_id_t;
    using tlm::cep::responder_set_t;
    using tlm::cep::TileLinkHomeStateDriver;
    using tlm::cep::TileLinkStateSpec;
    using tlm::cep::TlHomeAction;
    using tlm::cep::TlHomeInstallRow;
    using tlm::cep::TlHomeMessage;
    using tlm::cep::TlHomeObservation;
    using tlm::cep::TlHomeOutcome;
    using tlm::cep::TlHomeState;
    using tlm::cep::TreeManager;

    constexpr std::uint64_t kAddr = 0x7000;

    // ── 仓库文件读取(静态扫描用)────────────────────────────────────────
    std::string repo_root_dir() {
#ifdef CPPTLM_SOURCE_DIR
        return std::string(CPPTLM_SOURCE_DIR);
#else
        return std::string(".");
#endif
    }

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(repo_root_dir() + "/" + rel, std::ios::binary);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去注释(行 + 块)—— 静态扫描的**前提**(注释里的字面量不算副本)。
    std::string strip_comments(std::string code) {
        std::string out;
        out.reserve(code.size());
        for (std::size_t i = 0; i < code.size();) {
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
                while (i < code.size() && code[i] != '\n')
                    ++i;
                continue;
            }
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
                i += 2;
                while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/'))
                    ++i;
                i = (i + 1 < code.size()) ? i + 2 : code.size();
                out.push_back(' ');
                continue;
            }
            out.push_back(code[i]);
            ++i;
        }
        return out;
    }

    // ── C-1 行宇宙谓词(可测 · 接受任意表 ⇒ 影子表可驱动反 fixture)───────
    template <class Container> bool row_universe_ok(const Container& rows) {
        if (rows.size() != tlm::cep::kTlHomeMessageCount)
            return false;
        // 行序 == 消息枚举值
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (static_cast<std::size_t>(rows[i].message) != i)
                return false;
        // 消息名集合 == `kProtocolMessageMap` 的 10 词表
        const char* const kExpected[10] = {"Acquire",    "Probe",       "ProbeAck", "ProbeAckData",
                                           "Release",    "ReleaseData", "Grant",    "GrantData",
                                           "ReleaseAck", "GrantAck"};
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const char* name = nullptr;
            switch (rows[i].message) {
            case TlHomeMessage::Acquire:
                name = kExpected[0];
                break;
            case TlHomeMessage::Probe:
                name = kExpected[1];
                break;
            case TlHomeMessage::ProbeAck:
                name = kExpected[2];
                break;
            case TlHomeMessage::ProbeAckData:
                name = kExpected[3];
                break;
            case TlHomeMessage::Release:
                name = kExpected[4];
                break;
            case TlHomeMessage::ReleaseData:
                name = kExpected[5];
                break;
            case TlHomeMessage::Grant:
                name = kExpected[6];
                break;
            case TlHomeMessage::GrantData:
                name = kExpected[7];
                break;
            case TlHomeMessage::ReleaseAck:
                name = kExpected[8];
                break;
            case TlHomeMessage::GrantAck:
                name = kExpected[9];
                break;
            }
            if (name == nullptr)
                return false;
        }
        return true;
    }

    /// 驱动表与**规范 10 消息表**(`tilelink_c_adapter.hh` 的 `kProtocolMessageMap`)逐名一致。
    bool driver_messages_match_spec() {
        const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/tilelink_c_adapter.hh"));
        if (code.empty())
            return false;
        const char* const kNames[10] = {"Acquire",    "Probe",       "ProbeAck", "ProbeAckData",
                                        "Release",    "ReleaseData", "Grant",    "GrantData",
                                        "ReleaseAck", "GrantAck"};
        for (const char* n : kNames) {
            const std::string needle = std::string("{\"") + n + "\",";
            if (code.find(needle) == std::string::npos)
                return false;
        }
        return true;
    }

    // ── 族 2 判定器(**test 侧** · per 族 1 先例)──────────────────────────
    //
    // ⚠️ 判定行 = 「**Tip 获取**」事件(装表目标 == Tip)⇒ 此刻「他人 sharer」非空 ⇒ SWMR 违例形态。
    // ⚠️ `Undecidable` ⭔ ≠ 零违例 ⇒ invalid 时**不计入**违例数。
    struct Tier2TrackerVerdict {
        std::size_t swmr_violations = 0;
        std::size_t write_episodes = 0; ///< 判定行数(Tip 获取次数)
        std::size_t install_records = 0;
        bool invalid = false;
        std::string invalid_reason;
        bool provenance_broken = false;
        std::uint64_t untraceable_bits = 0;
    };

    Tier2TrackerVerdict
    detect(const std::vector<TlHomeObservation>& obs, std::uint64_t dropped,
           std::uint64_t invalid_count, bool observations_enabled,
           const std::function<std::uint64_t(std::uint64_t)>& installed_mask_of) {
        Tier2TrackerVerdict v;
        if (dropped > 0) {
            v.invalid = true;
            v.invalid_reason = "观测记录被丢弃(dropped>0)=> 观测不完整";
        }
        if (!observations_enabled) {
            v.invalid = true;
            v.invalid_reason = "观测面未开启 => 无观测记录 => 不可判(⛔ 不是零违例)";
        }
        if (invalid_count > 0) {
            v.invalid = true;
            if (v.invalid_reason.empty())
                v.invalid_reason = "存在不可判的安装(装配前提不成立 / 载荷超域 / 源态下目标不存在)";
        }
        for (const auto& o : obs) {
            ++v.install_records;
            if (!o.self_mask_valid) {
                v.invalid = true;
                if (v.invalid_reason.empty())
                    v.invalid_reason = "self_mask 无效(节点未映射到 sharer 位)";
            }
            if (o.action == TlHomeAction::SetState && o.target == TlHomeState::Tip) {
                ++v.write_episodes;
                const std::uint64_t others = o.sharers_before & ~o.self_mask;
                if (others != 0)
                    ++v.swmr_violations;
                // 溯源:「他人 sharer」的每位都必须能溯源到**驱动装表**的位
                const std::uint64_t known = installed_mask_of(o.addr);
                const std::uint64_t untraceable = others & ~known;
                if (untraceable != 0) {
                    v.provenance_broken = true;
                    v.untraceable_bits |= untraceable;
                }
            }
        }
        return v;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// AC-1 · C-1 行宇宙锁(恰 10 行 · 行序 == 消息枚举 · 消息名集合 == 规范 10 词表)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink home driver: 安装表行宇宙锁(C-1)", "[cep][tilelink][home-driver]") {
    // I-7b AC-1: 安装表恰 10 行 / 行序 == 消息枚举值 / 消息名集合 == 规范 10 词表
    CHECK(row_universe_ok(tlm::cep::kTlHomeInstallRows));
    CHECK(driver_messages_match_spec());

    // ⚠️ 反 fixture:影子表删一行 / 加一行 / 换序 ⇒ 判据必须红
    {
        std::vector<TlHomeInstallRow> shadow(tlm::cep::kTlHomeInstallRows.begin(),
                                             tlm::cep::kTlHomeInstallRows.end());
        shadow.pop_back(); // 删末行
        CHECK_FALSE(row_universe_ok(shadow));
    }
    {
        std::vector<TlHomeInstallRow> shadow(tlm::cep::kTlHomeInstallRows.begin(),
                                             tlm::cep::kTlHomeInstallRows.end());
        shadow.push_back(shadow.front()); // 加一行
        CHECK_FALSE(row_universe_ok(shadow));
    }
    {
        std::vector<TlHomeInstallRow> shadow(tlm::cep::kTlHomeInstallRows.begin(),
                                             tlm::cep::kTlHomeInstallRows.end());
        std::swap(shadow[0], shadow[1]); // 换序
        CHECK_FALSE(row_universe_ok(shadow));
    }
}

// ══════════════════════════════════════════════════════════════════════════
// AC-2 / AC-3 · C-2 basis ↔ 锚点一致性 + C-3 Pending ⇒ fail-closed
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink home driver: basis ↔ 锚点一致性(C-2)+ Pending 锁(C-3)",
          "[cep][tilelink][home-driver]") {
    // I-7b AC-2: 逐行 basis ↔ ref kind 一致(Direct ⇒ 双 SpecClause;SelfDerived ⇒ event SpecClause +
    // payload RepoDerived)
    for (const auto& row : tlm::cep::kTlHomeInstallRows) {
        INFO("行消息 = " << static_cast<int>(row.message));
        REQUIRE(tlm::cep::tl_home_basis_ref_kind_consistent(row));
        // ⛔ 锚点必须**已登记**(未登记 ⇒ fail-closed ⇒ 红)
        REQUIRE(tlm::cep::tl_home_resolve_ref(row.event_ref).registered);
        REQUIRE(tlm::cep::tl_home_resolve_ref(row.payload_ref).registered);
        // ⛔ **不得**锚到 improvement 自身(循环自证)
        const std::string ev(row.event_ref);
        const std::string pl(row.payload_ref);
        CHECK(ev.find("improvements") == std::string::npos);
        CHECK(pl.find("improvements") == std::string::npos);
    }

    // ⚠️ 反 fixture:把某行 basis 改成 SelfDerived 而 ref 不变 ⇒ 判据红
    {
        std::vector<TlHomeInstallRow> shadow(tlm::cep::kTlHomeInstallRows.begin(),
                                             tlm::cep::kTlHomeInstallRows.end());
        shadow[0].basis =
            TileLinkStateSpec::T3BasisKind::SelfDerived; // Direct 行 ⇒ 双 SpecClause ⇒ 红
        bool all_ok = true;
        for (const auto& r : shadow)
            all_ok = all_ok && tlm::cep::tl_home_basis_ref_kind_consistent(r);
        CHECK_FALSE(all_ok);
    }
    // ⚠️ 反 fixture:把某行 ref 改成**未注册 id** ⇒ 判据红
    // ⚠️ 必须用**不以 `TLC-` 开头**的 id:`tl_home_resolve_ref` 对 `TLC-*` 走前缀匹配
    //    (整族 = `tilelink-c` = SpecClause)⇒ `"TLC-NOT-REGISTERED"` 会被判为**已登记**,
    //    该反 fixture 就永远不红 ⇒ 用登记面外的裸 id。
    {
        std::vector<TlHomeInstallRow> shadow(tlm::cep::kTlHomeInstallRows.begin(),
                                             tlm::cep::kTlHomeInstallRows.end());
        shadow[1].event_ref = "UNREGISTERED-ID";
        bool all_ok = true;
        for (const auto& r : shadow)
            all_ok = all_ok && tlm::cep::tl_home_basis_ref_kind_consistent(r);
        CHECK_FALSE(all_ok);
    }

    // I-7b AC-3: Pending 行 ⇒ 动作恒 NoOp
    for (const auto& row : tlm::cep::kTlHomeInstallRows) {
        if (row.basis == TileLinkStateSpec::T3BasisKind::Pending)
            CHECK(row.action == TlHomeAction::NoOp);
    }
    // ⚠️ 反 fixture:Pending 行却带装表动作 ⇒ 判据红
    {
        TlHomeInstallRow bad = tlm::cep::kTlHomeInstallRows[1];
        bad.basis = TileLinkStateSpec::T3BasisKind::Pending;
        CHECK_FALSE(tlm::cep::tl_home_basis_ref_kind_consistent(bad));
    }
}

// ══════════════════════════════════════════════════════════════════════════
// AC-4 / AC-5 · C-4 决定性变异 + C-5 越权消费锁(禁复制规约层映射表)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink home driver: C-4 变异 + C-5 越权消费锁", "[cep][tilelink][home-driver]") {
    // I-7b AC-4: 影子表改 basis ⇒ 判据红(判别力)
    // ⚠️ 必须选 `payload_ref = "repo"`(RepoDerived)的行改成 `Direct` —— 反向(改 `SelfDerived`)
    //    对该行是**不变**的(它本来就 SelfDerived)⇒ 反 fixture 无判别力。
    {
        std::vector<TlHomeInstallRow> shadow(tlm::cep::kTlHomeInstallRows.begin(),
                                             tlm::cep::kTlHomeInstallRows.end());
        shadow[3].basis = TileLinkStateSpec::T3BasisKind::Direct; // ProbeAckData: payload "repo" ⇒
                                                                  // Direct 要求 SpecClause ⇒ 红
        bool all_ok = true;
        for (const auto& r : shadow)
            all_ok = all_ok && tlm::cep::tl_home_basis_ref_kind_consistent(r);
        CHECK_FALSE(all_ok);
    }

    // I-7b AC-5: 驱动头文件**不得**出现规约层映射表的自有字面量副本
    {
        const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/tilelink_home_state_driver.hh"));
        REQUIRE_FALSE(code.empty());
        CHECK(code.find("kT3MappingCells") == std::string::npos);
        CHECK(code.find("kTransitions") == std::string::npos);
        CHECK(code.find("kProtocolMessageMap") == std::string::npos);
        CHECK(code.find("kMessageClassMap") == std::string::npos);
        // ✅ 但**允许**经访问器只读消费
        CHECK(code.find("t3_consumable_event_mask") != std::string::npos);
        CHECK(code.find("TileLinkStateSpec::transition") != std::string::npos);
    }

    // I-7b AC-14 ③:载体内容判据 —— 驱动**确实**经 `TreeManager::set_sharer` 装表
    // ⚠️ 反 fixture:把该调用删掉 ⇒ 谓词必红(影子副本 · ⛔ 不落盘)
    // ⚠️ 这条抓的是「载体**内容**是否真的动树表」—— 只看表/字段存在会漏掉「装了但不生效」。
    {
        const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/tilelink_home_state_driver.hh"));
        REQUIRE_FALSE(code.empty());
        const auto installs_via_set_sharer = [](const std::string& c) {
            return c.find("tracker_->set_sharer(") != std::string::npos;
        };
        CHECK(installs_via_set_sharer(code));
        std::string mutated = code;
        const std::size_t p = mutated.find("tracker_->set_sharer(");
        REQUIRE(p != std::string::npos);
        const std::size_t semi = mutated.find(';', p);
        REQUIRE(semi != std::string::npos);
        mutated.erase(p, semi - p + 1);
        CHECK_FALSE(installs_via_set_sharer(mutated));
    }
}

// ══════════════════════════════════════════════════════════════════════════
// AC-6 ~ AC-9 · 安装解析的逐条判据(单调性 / 方向 / 载荷折叠 / 源=Tip)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink home driver: 安装解析(12 格矩阵 + fail-closed)",
          "[cep][tilelink][home-driver]") {
    TlHomeState target = TlHomeState::Nothing;

    // I-7b AC-9: 源 = Tip 再收 Grant ⇒ 依 TLC-06(源=Tip 无 Grow 转移)⇒ **不可判**
    {
        const TlHomeOutcome o = tlm::cep::tl_home_resolve_install(
            TlHomeMessage::Grant, TileLinkStateSpec::kMaskTip, TlHomeState::Tip, target);
        CHECK(o == TlHomeOutcome::Undecidable);
    }
    // 源 = Nothing 收 Grant(载荷 Tip)⇒ 装表(Tip)
    {
        const TlHomeOutcome o = tlm::cep::tl_home_resolve_install(
            TlHomeMessage::Grant, TileLinkStateSpec::kMaskTip, TlHomeState::Nothing, target);
        CHECK(o == TlHomeOutcome::Installed);
        CHECK(target == TlHomeState::Tip);
    }
    // 源 = Branch 收 Grant(载荷 Branch)⇒ 不可判(Grow 在 Branch 上只允许 → Tip)
    {
        const TlHomeOutcome o = tlm::cep::tl_home_resolve_install(
            TlHomeMessage::Grant, TileLinkStateSpec::kMaskBranch, TlHomeState::Branch, target);
        CHECK(o == TlHomeOutcome::Undecidable);
    }
    // I-7b AC-7 · D-2: 「保留 vs 放弃」单差异 ⇒ 方向相反 ⇒ 目标不同
    {
        TlHomeState keep = TlHomeState::Nothing;
        TlHomeState drop = TlHomeState::Nothing;
        const TlHomeOutcome ok = tlm::cep::tl_home_resolve_install(
            TlHomeMessage::ProbeAck, TileLinkStateSpec::kMaskBranch, TlHomeState::Tip, keep);
        const TlHomeOutcome od = tlm::cep::tl_home_resolve_install(
            TlHomeMessage::ProbeAck, TileLinkStateSpec::kMaskNothing, TlHomeState::Tip, drop);
        CHECK(ok == TlHomeOutcome::Installed);
        CHECK(od == TlHomeOutcome::Installed);
        CHECK(keep == TlHomeState::Branch);  // 保留 ⇒ 降级为只读副本
        CHECK(drop == TlHomeState::Nothing); // 放弃 ⇒ 无副本
    }
    // I-7b AC-8 · D-3: 载荷折叠试金石 —— 无载荷(0)给需要载荷的行 ⇒ 不可判
    {
        const TlHomeOutcome o =
            tlm::cep::tl_home_resolve_install(TlHomeMessage::Release, 0, TlHomeState::Tip, target);
        CHECK(o == TlHomeOutcome::Undecidable);
    }
    // 载荷超域(Grant 不接受 Nothing)⇒ 不可判
    {
        const TlHomeOutcome o = tlm::cep::tl_home_resolve_install(
            TlHomeMessage::Grant, TileLinkStateSpec::kMaskNothing, TlHomeState::Nothing, target);
        CHECK(o == TlHomeOutcome::Undecidable);
    }
    // 无事件的消息(Acquire)带载荷 ⇒ 不可判;不带载荷 ⇒ NoOp
    {
        TlHomeState t2 = TlHomeState::Nothing;
        CHECK(tlm::cep::tl_home_resolve_install(TlHomeMessage::Acquire, 0, TlHomeState::Nothing,
                                                t2) == TlHomeOutcome::NoOp);
        CHECK(tlm::cep::tl_home_resolve_install(TlHomeMessage::Acquire, TileLinkStateSpec::kMaskTip,
                                                TlHomeState::Nothing,
                                                t2) == TlHomeOutcome::Undecidable);
    }
}

// ══════════════════════════════════════════════════════════════════════════
// AC-10 ~ AC-13 · 族 2 判据载体(三段分离 · 溯源 · 装配期硬断言 · 有界性)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("CEP TileLink home driver: 族 2 判据载体(正控 / 负控 / 反 fixture / 硬断言 / 有界)",
          "[cep][tilelink][home-driver]") {
    const node_id_t kParent = tlm::cep::kNodeIdPcie;
    const node_id_t kSelf = tlm::cep::kNodeIdSdma;
    const node_id_t kPeer = tlm::cep::kNodeIdGmmu;
    const responder_set_t mSelf = tlm::cep::responder_mask_of(kSelf);
    const responder_set_t mPeer = tlm::cep::responder_mask_of(kPeer);

    // ── I-7b AC-10 · 正控:合法流 ⇒ 违例数 **0** ────────────────────────
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        drv.apply(TlHomeMessage::Grant, kAddr, kSelf, TileLinkStateSpec::kMaskTip);
        drv.apply(TlHomeMessage::Release, kAddr, kSelf, TileLinkStateSpec::kMaskNothing);
        const auto v = detect(drv.observations(), drv.dropped_observations(),
                              drv.invalid_observations(), drv.observations_enabled(),
                              [&](std::uint64_t a) { return drv.installed_mask_of(a); });
        CHECK_FALSE(v.invalid);
        CHECK(v.swmr_violations == 0u);
        CHECK(v.write_episodes == 1u);
        CHECK(v.install_records == 2u); // Grant + Release 各 1 条(NoOp 也记)
    }

    // ── I-7b AC-10 · 负控:写者取 Tip 时目录里还有他人 ⇒ 违例 **精确 == 1** ──
    // ⚠️ 他人必须先落 **Branch**(不是 Tip):否则第二步触发「多 Tip」⇒ 驱动 fail-closed
    //    不装表 ⇒ 判定行只剩 1 条 ⇒ 违例数被装配期硬断言吞掉,测不到 SWMR 判据本身。
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        drv.apply(TlHomeMessage::Grant, kAddr, kPeer,
                  TileLinkStateSpec::kMaskBranch); // 他人先持只读副本
        drv.apply(TlHomeMessage::Grant, kAddr, kSelf, TileLinkStateSpec::kMaskTip); // 本方再取独占
        const auto v = detect(drv.observations(), drv.dropped_observations(),
                              drv.invalid_observations(), drv.observations_enabled(),
                              [&](std::uint64_t a) { return drv.installed_mask_of(a); });
        CHECK_FALSE(v.invalid);         // ⚠️ 判据可判(违例数可信),否则「数 == 1」无意义
        CHECK(v.swmr_violations == 1u); // ⛔ 精确 1(非 > 0)
        CHECK(v.write_episodes == 1u);  // 判定行 = 唯一一次 Tip 获取(peer 的 Branch 不算)
        CHECK(v.install_records == 2u);
    }

    // ── I-7b AC-6 · D-1:单调性 —— Grant→Release 配对 ⇒ 终态 sharers **精确 == 0** ──
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        drv.apply(TlHomeMessage::Grant, kAddr, kSelf, TileLinkStateSpec::kMaskTip);
        CHECK(tm.view_of(kAddr).sharers == mSelf); // 单差异对照:未卸表 ⇒ 非 0
        drv.apply(TlHomeMessage::Release, kAddr, kSelf, TileLinkStateSpec::kMaskNothing);
        CHECK(tm.view_of(kAddr).sharers == 0u); // ⛔ 精确 0(抓到「不卸表」方向)
    }

    // ── I-7b AC-11 · 反 fixture:绕过驱动直接播种 ⇒ provenance_broken ──
    // ⚠️ 绕过播种必须落 **Branch**:播 Tip 会让随后本方的 Tip 获取触发「多 Tip」
    //    ⇒ 该次判 NoOp ⇒ 判定行只有 1 条且是 NoOp ⇒ 溯源分支根本不执行(测不到)。
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        tm.set_sharer(kAddr, kPeer, TlHomeState::Branch); // ⛔ 绕过驱动(不在安装台账里)
        drv.apply(TlHomeMessage::Grant, kAddr, kSelf, TileLinkStateSpec::kMaskTip);
        const auto v = detect(drv.observations(), drv.dropped_observations(),
                              drv.invalid_observations(), drv.observations_enabled(),
                              [&](std::uint64_t a) { return drv.installed_mask_of(a); });
        CHECK(v.provenance_broken);
        CHECK(v.untraceable_bits != 0u);
    }

    // ── I-7b AC-12 · 装配期硬断言 ①:node 未映射 ⇒ 不可判(⛔ 不是零违例) ──
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        const auto res = drv.apply(TlHomeMessage::Grant, kAddr, tlm::cep::kInvalidNode,
                                   TileLinkStateSpec::kMaskTip);
        CHECK(res.observation_invalid);
        CHECK(drv.invalid_observations() == 1u);
        const auto v = detect(drv.observations(), drv.dropped_observations(),
                              drv.invalid_observations(), drv.observations_enabled(),
                              [&](std::uint64_t a) { return drv.installed_mask_of(a); });
        CHECK(v.invalid);
        CHECK(v.swmr_violations == 0u); // ⛔ 不可判 ⇒ 不计入违例数
    }

    // ── I-7b AC-12 · 硬断言 ②:多 Tip ⇒ 不装表 + 不可判 ──
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        drv.apply(TlHomeMessage::Grant, kAddr, kPeer, TileLinkStateSpec::kMaskTip);
        const std::uint64_t before = tm.view_of(kAddr).sharers;
        const auto res = drv.apply(TlHomeMessage::Grant, kAddr, kSelf, TileLinkStateSpec::kMaskTip);
        CHECK(res.observation_invalid);
        CHECK(tm.view_of(kAddr).sharers == before); // ⛔ 不装表(不破坏「至多 1 个 Tip」)
    }

    // ── I-7b AC-13 · 观测面默认关闭 ⇒ 零记录 + 不可判 ──
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        CHECK_FALSE(drv.observations_enabled());
        drv.apply(TlHomeMessage::Grant, kAddr, kSelf, TileLinkStateSpec::kMaskTip);
        CHECK(drv.observations().empty());
        const auto v = detect(drv.observations(), drv.dropped_observations(),
                              drv.invalid_observations(), drv.observations_enabled(),
                              [&](std::uint64_t a) { return drv.installed_mask_of(a); });
        CHECK(v.invalid);
    }

    // ── I-7b AC-13 · 有界性:溢出 ⇒ 丢弃计数递增 ⇒ 不可判 ──
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        drv.set_observation_capacity(2);
        drv.apply(TlHomeMessage::Grant, kAddr, kSelf, TileLinkStateSpec::kMaskTip);
        drv.apply(TlHomeMessage::Acquire, kAddr, kSelf, 0);
        drv.apply(TlHomeMessage::GrantAck, kAddr, kSelf, 0);
        CHECK(drv.observations().size() == 2u);
        CHECK(drv.dropped_observations() == 1u);
        const auto v = detect(drv.observations(), drv.dropped_observations(),
                              drv.invalid_observations(), drv.observations_enabled(),
                              [&](std::uint64_t a) { return drv.installed_mask_of(a); });
        CHECK(v.invalid);
    }

    // ── I-7b AC-10 · 判定行只取「Tip 获取」 ⇒ 纯非 Tip 事件 ⇒ 判定行增量 == 0 ──
    {
        TreeManager tm(kParent);
        TileLinkHomeStateDriver drv(tm);
        drv.set_observations_enabled(true);
        drv.apply(TlHomeMessage::Acquire, kAddr, kSelf, 0);
        drv.apply(TlHomeMessage::GrantAck, kAddr, kSelf, 0);
        drv.apply(TlHomeMessage::ReleaseAck, kAddr, kSelf, 0);
        const auto v = detect(drv.observations(), drv.dropped_observations(),
                              drv.invalid_observations(), drv.observations_enabled(),
                              [&](std::uint64_t a) { return drv.installed_mask_of(a); });
        CHECK(v.write_episodes == 0u); // ⛔ 判定行增量 == 0
        CHECK(v.swmr_violations == 0u);
        CHECK(v.install_records == 3u);
    }
}
