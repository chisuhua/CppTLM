// test/test_cep_tier2_observation.cc
// 档 2(协议级)**观测面 + 契约级断言** —— change `cep-evidence-tier2-minimal-slice`
// per
// openspec/changes/cep-evidence-tier2-minimal-slice/specs/cep-evidence-tier2-observation/spec.md
//     Requirement: cep-evidence-tier2-observation
// 作者: CppTLM Team / 日期: 2026-11-13
//
// ── 判据纪律(本文件的写法约束)──────────────────────────────────────
// ① 每条判据都有**可执行反向 fixture**,且断言**具体判据**而不是「整体不全」;
// ② 纯负向断言必带正控(**同款驱动下的反向取值可分辨**);
// ③ 判别谓词必须能区分「真的观测到了」与「什么都没发生」—— 只记调用次数不算;
// ④ ⛔ **判定在 test 侧**(per architect 裁决 R2:沿用 `stub_cache_endpoint.hh:68-83`
//    的既有先例「实现侧只出原始事实」)⇒ 本文件**不**改 `test/cep_experiment_harness.hh`
//    ⇒ 档 1 的判据与负控**零改动**。
//
// ── ⛔ 诚实边界(随引用重复)────────────────────────────────────────
// · 本档位**不产 4 件套 run** ⇒ **无 `ic3_verdict` / 无「通过态」**(per 裁决 R3);
// · data-value 维在**生产流恒 0** = 「本装置无该形态」(本模型无本地缓存数据版本)
//   ⇒ ⛔ **不得**称「检测力已证」;其可达形态**仅**「注入陈旧副本」的负控;
// · 档 2 与档 1 **正交**(⛔ 非强弱序);**单族(mesif)/ 单拓扑**。
#include "catch_amalgamated.hpp"

#include "core/event_queue.hh"
#include "tlm/cep/mesif_adapter.hh"
#include "tlm/cep/mesif_protocol_engine.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/sharer_tracker.hh"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

    namespace cep = tlm::cep;
    using cep::EventObservation;
    using cep::MesifProtocolEngine;

    constexpr std::uint64_t kAddr = 0x1000;

    // ── 判定器(**test 侧** · per 裁决 R2)──────────────────────────────
    //
    // ⚠️ **归因分层**(per 裁决 R1):
    // · `swmr_violations` 的归因 = **目录可见性**(home 侧)—— 判据是
    //   「写时该地址仍被**他人**持有」⇒ 归因对象 = run 内的 home 行为;
    // · `data_value_violations` 的归因 = **run 内 home 行为**(home 回给的值);
    // · ⛔ 二者**都不是**「引擎实现错误」的证据。
    struct Tier2Verdict {
        std::size_t swmr_violations = 0;
        std::size_t data_value_violations = 0;
        std::size_t write_episodes = 0; ///< 判定行数(写 episode 数)
        std::size_t fill_records = 0;   ///< 装行记录数
        bool invalid = false;           ///< 观测面**不可判**(⛔ 不是「0 违例」)
        std::string invalid_reason;
        /// ⚠️ **provenance 破口**(per 验收 AC-1 ③):观测到的「他人 sharer」里存在
        ///    **无法溯源到任何引擎**的位 ⇒ 该观测**不可信**(⇒ 判不可判)
        bool provenance_broken = false;
        std::uint64_t untraceable_bits = 0; ///< 不可溯源的位掩码
    };

    /// 判定(**SWMR 判定行 = `transition ∧ Wr ∧ to == M`**,per 裁决 §3)
    ///
    /// ⚠️ `kind = fill` 行**显式豁免** —— 它是同一写 episode 的第 3 条记录
    ///    (WRITEBACK 一次 completion = 2 条 transition + 1 条 fill)
    ///    ⇒ 若不豁免,违例数会因**行倍增**以错误原因变红。
    ///
    /// ⚠️ **参数一律无默认值**(per 验收 A-11「死判据免税」):
    ///   · `dropped` —— 缓冲丢弃计数(非零 ⇒ 观测不完整 ⇒ 不可判)
    ///   · `known_engine_masks` —— **已知引擎的 `self_mask` 并集**;「他人 sharer」
    ///     里不在此集合内的位 ⇒ **不可溯源**(fixture 播种 / 未接线引擎)⇒ 破口。
    ///     ⛔ **禁传 `~0`** —— 那会让 `untraceable` 恒 0 ⇒ 判据成为死代码。
    ///   · `observations_enabled` —— 观测面是否**开启**;关闭 ⇒ **无观测记录**
    ///     ⇒ 判「不可判」(⛔ 不是「零违例」—— 两者是不同的事)。
    Tier2Verdict detect(const std::vector<EventObservation>& obs,
                        const std::unordered_map<std::uint64_t, std::uint64_t>& home_last_write,
                        std::uint64_t dropped, std::uint64_t known_engine_masks,
                        bool observations_enabled) {
        Tier2Verdict v;
        // ⚠️ 观测被丢弃 ⇒ 该次观测**不完整** ⇒ 判「不可判」(⛔ 不是零违例)
        if (dropped > 0) {
            v.invalid = true;
            v.invalid_reason = "观测记录被丢弃(dropped>0)=> 观测不完整";
        }
        // ⚠️ 观测面**关闭** ⇒ 零记录 ⇒ 「没观测到」⭔ ≠「无违例」⇒ 判「不可判」
        if (!observations_enabled) {
            v.invalid = true;
            v.invalid_reason = "观测面未开启 => 无观测记录 => 不可判(⛔ 不是零违例)";
        }
        const std::int32_t kWr = static_cast<std::int32_t>(cep::MesifAdapter::Event::Wr);
        const std::int32_t kM = static_cast<std::int32_t>(cep::MesifAdapter::State::M);
        for (const auto& o : obs) {
            if (!o.self_mask_valid) {
                v.invalid = true;
                v.invalid_reason = "self_mask 无效(节点未映射到 sharer 位)";
            }
            if (o.kind == EventObservation::Kind::Transition && o.event == kWr && o.to == kM) {
                ++v.write_episodes;
                const std::uint64_t others = o.sharers_snapshot & ~o.self_mask;
                // 写时目录里仍存在**他人** sharer ⇒ SWMR 违例形态
                if (others != 0)
                    ++v.swmr_violations;
                // ⚠️ **provenance**(AC-1 ③):「他人 sharer」的每一位都必须可溯源到
                //    **某个引擎自身的 `apply_event → sync_tracker`** ⇒ 不在已知引擎
                //    self_mask 并集里的位 ⇒ **fixture 播种**(或未接线的引擎)⇒ 破口。
                const std::uint64_t untraceable = others & ~known_engine_masks;
                if (untraceable != 0) {
                    v.provenance_broken = true;
                    v.untraceable_bits |= untraceable;
                }
            }
            if (o.kind == EventObservation::Kind::Fill) {
                ++v.fill_records;
                const auto it = home_last_write.find(o.addr);
                if (it != home_last_write.end() && it->second != o.data_after)
                    ++v.data_value_violations;
            }
        }
        return v;
    }

    /// 打开 Request / Completion / Snoop 三条流
    ///
    /// ⚠️ **必须**:默认 `Support::flow` 全 false ⇒ `gate()` 会拒 ⇒ 观测面
    ///    **不会有任何记录**(「什么都没发生」≠「观测到 0 违例」)。
    void open_flows(MesifProtocolEngine& e) {
        MesifProtocolEngine::Support s{};
        s.flow[static_cast<std::size_t>(cep::CanonicalFlow::Request)] = true;
        s.flow[static_cast<std::size_t>(cep::CanonicalFlow::Completion)] = true;
        s.flow[static_cast<std::size_t>(cep::CanonicalFlow::Snoop)] = true;
        e.set_support(s);
    }

    /// 一次「本地发起写 + home 回完成」的最小驱动
    ///
    /// ⚠️ `submit_local_request` 会先登记 `txn_addr_` ⇒ `on_completion` 才能定位地址;
    ///    egress 是否写成功**不影响**观测(本档位⭔ 不走拓扑级装配)。
    void drive_write(MesifProtocolEngine& e, std::uint64_t addr, std::uint64_t txn,
                     std::uint64_t data, cep::Perm perm) {
        e.submit_local_request(addr, cep::MesifAdapter::Event::Wr, txn);
        e.on_completion(txn, data, perm, false);
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════
//  AC-1 · 非空洞判据 —— 正控:独占写 ⇒ 写时无他人 sharer ⇒ 违例 0
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:正控 —— 单引擎独占写 ⇒ 写时无他人 sharer ⇒ 违例 0",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine e("mesif_solo", &eq);
    e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    e.enable_observations(true);
    open_flows(e);

    drive_write(e, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);

    const auto v = detect(e.observations(), {}, /*dropped=*/0u,
                          /*known_engine_masks=*/cep::responder_mask_of(cep::kNodeIdSdma),
                          /*observations_enabled=*/true);
    // ① 观测面**真的**产出了记录(不是「什么都没发生」)
    REQUIRE(e.observation_count() > 0u);
    REQUIRE(e.dropped_observations() == 0u);
    REQUIRE(e.observation_invalid() == 0u);
    // ② 写 episode 被观测到(判定行存在)
    CHECK(v.write_episodes == 1u);
    // ③ 无他人 sharer ⇒ 违例 0
    CHECK(v.swmr_violations == 0u);
    CHECK(v.invalid == false);
}

// ══════════════════════════════════════════════════════════════════════
//  AC-3 · 负控 ① —— 写时目录存在**他人** sharer ⇒ SWMR 违例**精确 == 1**
//  (与正控**同流 + 单差异**:唯一差异 = 「A 是否先持有该地址」)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:负控 —— 写时存在他人 sharer ⇒ SWMR 违例精确 1",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine a("mesif_a", &eq);
    MesifProtocolEngine b("mesif_b", &eq);
    a.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    b.set_tracker(&dir, cep::kNodeIdVram, /*level=*/0);
    a.enable_observations(true);
    b.enable_observations(true);
    open_flows(a);
    open_flows(b);

    // A 先持有该地址(⇒ 目录 own_[0] 记下 A 的 sharer 位)
    drive_write(a, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);
    // B 随后写同一地址 ⇒ B 的**写前视图**里仍含 A ⇒ SWMR 违例形态
    drive_write(b, kAddr, /*txn=*/2, /*data=*/0x22, cep::Perm::WRITEBACK);

    const auto v = detect(b.observations(), {}, /*dropped=*/0u,
                          /*known_engine_masks=*/cep::responder_mask_of(cep::kNodeIdSdma) |
                              cep::responder_mask_of(cep::kNodeIdVram),
                          /*observations_enabled=*/true);
    REQUIRE(b.observation_invalid() == 0u);
    REQUIRE(b.dropped_observations() == 0u);
    // 违例数**精确** == 1(⛔ 不用「> 0」)
    CHECK(v.write_episodes == 1u);
    CHECK(v.swmr_violations == 1u);
    // ⚠️ 归因说明:该违例的成因是「home 侧在 B 写时未失效 A 的副本」
    //    ⇒ 归因对象 = **run 内 home 行为**,⛔ 不是「引擎实现错误」。
    CHECK(b.observation_count() > 0u);
}

// ══════════════════════════════════════════════════════════════════════
//  AC-6 · 判定行钉死 —— WRITEBACK 一次 = 2 transition + 1 fill
//  ⇒ 写 episode 计 **1**(不是 3);fill 行**不**计入 SWMR 判定
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:判定行 —— 一次写 = 2 transition + 1 fill ⇒ episode 计 1",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine e("mesif_episode", &eq);
    e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    e.enable_observations(true);
    open_flows(e);

    drive_write(e, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);

    std::size_t transitions = 0;
    std::size_t fills = 0;
    for (const auto& o : e.observations()) {
        if (o.kind == EventObservation::Kind::Transition)
            ++transitions;
        else
            ++fills;
    }
    // ① 记录形态:RdUnique + Wr 两条 transition + 一条 fill
    CHECK(transitions == 2u);
    CHECK(fills == 1u);
    // ② 但**判定行**只取 `Wr → M` ⇒ 写 episode == 1(⛔ 不是 3)
    const auto v = detect(e.observations(), {}, /*dropped=*/0u,
                          /*known_engine_masks=*/cep::responder_mask_of(cep::kNodeIdSdma),
                          /*observations_enabled=*/true);
    CHECK(v.write_episodes == 1u);
    CHECK(v.fill_records == 1u);
}

// ══════════════════════════════════════════════════════════════════════
//  AC-3 · 负控 ② —— data-value:home 回**陈旧**值 ⇒ 违例**精确 == 1**
//  (⛔ 归因 = **run 内 home 行为**;生产流恒 0 = 「本装置无该形态」)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:负控 —— home 回陈旧值 ⇒ data-value 违例精确 1",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine e("mesif_stale", &eq);
    e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    e.enable_observations(true);
    open_flows(e);

    // home 侧最近一次写的值 = 新值;但回给本节点的是**陈旧**副本
    std::unordered_map<std::uint64_t, std::uint64_t> home_last_write{{kAddr, 0x22}};
    drive_write(e, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);

    const auto v = detect(e.observations(), home_last_write, /*dropped=*/0u,
                          /*known_engine_masks=*/cep::responder_mask_of(cep::kNodeIdSdma),
                          /*observations_enabled=*/true);
    REQUIRE(v.fill_records == 1u);
    CHECK(v.data_value_violations == 1u);
    // ⚠️ **正控配对**:同一驱动,home 回**最新**值 ⇒ 违例 0
    const auto v_ok = detect(e.observations(), {{kAddr, 0x11}}, /*dropped=*/0u,
                             /*known_engine_masks=*/cep::responder_mask_of(cep::kNodeIdSdma),
                             /*observations_enabled=*/true);
    CHECK(v_ok.data_value_violations == 0u);
    // ⚠️ 本模型的**生产流**恒 0(home 总是回最新值)⇒ 该 0 = 「本装置无该形态」
    //    ⇒ ⛔ **不得**表述为「已证明协议层无违例」。
}

// ══════════════════════════════════════════════════════════════════════
//  AC-4 · 装配期硬断言 —— 断线三连 ⇒ 判「**不可判**」(⛔ 不是 0)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:断线三连 ⇒ 观测面判不可判(⛔ 不是 0)", "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;

    // ① tracker 未接线
    MesifProtocolEngine no_tracker("mesif_no_tracker", &eq);
    no_tracker.set_tracker(nullptr, cep::kNodeIdSdma, /*level=*/0);
    CHECK(no_tracker.observation_invalid() == 1u);

    // ② 节点未映射到 sharer 位(`responder_mask_of` 只有 4 个 bit)
    MesifProtocolEngine bad_node("mesif_bad_node", &eq);
    bad_node.set_tracker(&dir, cep::kInvalidNode, /*level=*/0);
    CHECK(bad_node.observation_invalid() == 1u);

    // ③ 层级越界(`set_sharer` / `clear_sharer` 会**静默 no-op**)
    MesifProtocolEngine bad_level("mesif_bad_level", &eq);
    bad_level.set_tracker(&dir, cep::kNodeIdSdma,
                          /*level=*/cep::HierarchicalDirectory::kLevelCount);
    CHECK(bad_level.observation_invalid() == 1u);

    // ④ **正控**:合法接线 ⇒ 零 invalid(⛔ 否则上面的断言无判别力)
    MesifProtocolEngine good("mesif_good", &eq);
    good.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    CHECK(good.observation_invalid() == 0u);
}

// ══════════════════════════════════════════════════════════════════════
//  AC-8 · 确定性 —— 同一流跑两遍 ⇒ 观测记录**逐字相等**
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:确定性 —— 同流两遍 ⇒ 观测记录逐字相等", "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir1;
    cep::HierarchicalDirectory dir2;

    MesifProtocolEngine a("mesif_a", &eq);
    MesifProtocolEngine b("mesif_b", &eq);
    a.set_tracker(&dir1, cep::kNodeIdSdma, /*level=*/0);
    b.set_tracker(&dir2, cep::kNodeIdSdma, /*level=*/0);
    a.enable_observations(true);
    b.enable_observations(true);
    open_flows(a);
    open_flows(b);

    drive_write(a, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);
    drive_write(b, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);

    REQUIRE(a.observation_count() == b.observation_count());
    REQUIRE(a.observation_count() > 0u);
    for (std::size_t i = 0; i < a.observation_count(); ++i) {
        INFO("观测记录 #" << i);
        CHECK(a.observations()[i] == b.observations()[i]);
    }
    CHECK(a.dropped_observations() == 0u);
    CHECK(b.dropped_observations() == 0u);
}

// ══════════════════════════════════════════════════════════════════════
//  观测面**默认关闭** ⇒ 既有行为与性能零影响(零回归的机械证据)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:默认关闭 ⇒ 零记录(既有路径零影响)", "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine e("mesif_off", &eq);
    e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    open_flows(e);

    CHECK(e.observations_enabled() == false);
    drive_write(e, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);
    CHECK(e.observation_count() == 0u);
    CHECK(e.dropped_observations() == 0u);
    // ⚠️ 但**状态迁移本身**照常发生(观测面关闭 ≠ 引擎不工作)
    CHECK(e.state_of(kAddr) == cep::MesifAdapter::State::M);
}

// ══════════════════════════════════════════════════════════════════════
//  AC-1 ③ · provenance —— 「他人 sharer」位**可溯源到 peer 引擎**
//  (per 验收 G-1:判定窗口内禁止 fixture 播种目录 ⇒ 每个 sharer bit 须
//   可溯源到 peer 引擎自身的 `apply_event → sync_tracker`)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:provenance 正控 —— 他人 sharer 位可溯源到 peer 引擎 ⇒ 无破口",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine a("mesif_prov_a", &eq);
    MesifProtocolEngine b("mesif_prov_b", &eq);
    a.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    b.set_tracker(&dir, cep::kNodeIdVram, /*level=*/0);
    a.enable_observations(true);
    b.enable_observations(true);
    open_flows(a);
    open_flows(b);

    // A 先持有该地址 ⇒ 目录 own_[0] 记下 A 的 sharer 位(**经 A 引擎的 sync_tracker**)
    drive_write(a, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);
    // B 随后写 ⇒ B 的**写前视图**含 A ⇒ 他人位 = A 的 self_mask
    drive_write(b, kAddr, /*txn=*/2, /*data=*/0x22, cep::Perm::WRITEBACK);

    // 已知引擎的 self_mask 并集 = A ∪ B(⛔ **禁传 `~0`** —— 那会让判据成死代码)
    const std::uint64_t known =
        cep::responder_mask_of(cep::kNodeIdSdma) | cep::responder_mask_of(cep::kNodeIdVram);
    const auto v = detect(b.observations(), {}, /*dropped=*/0u, known,
                          /*observations_enabled=*/true);

    REQUIRE(v.write_episodes == 1u);
    REQUIRE(v.swmr_violations == 1u);
    // ① 他人位**恰等于** A 的 self_mask ⇒ 完全可溯源 ⇒ 无破口
    CHECK(v.provenance_broken == false);
    CHECK(v.untraceable_bits == 0u);
    // ② ⚠️ **正控的判别力**:若把 A 的位排除出已知集合 ⇒ 立刻破口
    //    (证明该断言**不是恒真** —— 否则它只是装饰)
    const auto v_excluded = detect(b.observations(), {}, /*dropped=*/0u,
                                   /*known_engine_masks=*/cep::responder_mask_of(cep::kNodeIdVram),
                                   /*observations_enabled=*/true);
    CHECK(v_excluded.provenance_broken == true);
    CHECK((v_excluded.untraceable_bits & cep::responder_mask_of(cep::kNodeIdSdma)) != 0u);
}

TEST_CASE("CEP 档 2 观测面:provenance 反 fixture —— 目录被 fixture 播种 ⇒ 不可溯源位 ⇒ 破口",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine b("mesif_prov_fixture", &eq);
    b.set_tracker(&dir, cep::kNodeIdVram, /*level=*/0);
    b.enable_observations(true);
    open_flows(b);

    // ⚠️ **fixture 播种**:把 GMMU(bit 2)直接写进目录 —— **不经任何引擎的 apply_event**
    //    ⇒ 该位在观测里**不可溯源**到任何引擎。
    dir.set_sharer(kAddr, cep::kNodeIdGmmu, /*level=*/0, cep::SnoopState::S);
    // ⚠️ **播种前置断言**(per 验收 R-6):`set_sharer` 对未映射节点 / 越界 level
    //    会**静默 no-op** ⇒ 若播种失败,反 fixture 会以「判据不灵」的方式假红
    //    ⇒ 必须先确认目录位**确实**置上。
    const auto seeded = dir.lookup_scoped(kAddr, /*level=*/0);
    REQUIRE((seeded.sharers & cep::responder_mask_of(cep::kNodeIdGmmu)) != 0u);

    drive_write(b, kAddr, /*txn=*/2, /*data=*/0x22, cep::Perm::WRITEBACK);

    // 已知引擎**只有 B 自己**(GMMU 位不在其中 ⇒ 不可溯源)
    const std::uint64_t known = cep::responder_mask_of(cep::kNodeIdVram);
    const auto v = detect(b.observations(), {}, /*dropped=*/0u, known,
                          /*observations_enabled=*/true);

    REQUIRE(v.write_episodes == 1u);
    REQUIRE(v.swmr_violations == 1u); // 他人位非空 ⇒ 形态上是违例
    // ① **破口被检出**:不可溯源位 ≠ 0 ⇒ 观测不可信
    CHECK(v.provenance_broken == true);
    CHECK((v.untraceable_bits & cep::responder_mask_of(cep::kNodeIdGmmu)) != 0u);
    // ② ⚠️ 该位**确实**来自 fixture(不在 B 自己的 self_mask 里)
    CHECK((v.untraceable_bits & cep::responder_mask_of(cep::kNodeIdVram)) == 0u);
}

// ══════════════════════════════════════════════════════════════════════
//  AC-8 · 有界性 —— 缓冲溢出 ⇒ `dropped_observations` 非零 ⇒ 判「不可判」
//  (per 验收 G-3:该路径此前**从未被执行过**)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:有界性 —— 缓冲溢出 ⇒ 丢弃计数非零 ⇒ 判不可判(⛔ 不是零违例)",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;

    // ── ① 上限 = 2 ⇒ 一次写(2 transition + 1 fill)⇒ fill 被丢弃 ──
    {
        MesifProtocolEngine e("mesif_bounded2", &eq);
        e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
        e.enable_observations(true);
        open_flows(e);
        e.set_observation_capacity(2);
        REQUIRE(e.observation_capacity() == 2u);

        drive_write(e, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);

        // cap = n ⇒ 恰好存 n 条(第 n+1 条起被丢弃并计数)
        CHECK(e.observation_count() == 2u);
        REQUIRE(e.dropped_observations() == 1u);

        const auto v = detect(e.observations(), {}, e.dropped_observations(),
                              cep::responder_mask_of(cep::kNodeIdSdma),
                              /*observations_enabled=*/true);
        // ⚠️ 观测不完整 ⇒ 判「不可判」;该用例**只**触发丢弃(合法接线 ⇒
        //    `self_mask` 有效 ⇒ 不会覆盖 `invalid_reason`)
        CHECK(v.invalid == true);
        REQUIRE(v.invalid_reason.find("丢弃") != std::string::npos);
    }

    // ── ② 上限 = 1 ⇒ **判定行本身**被丢弃 ⇒ 违例计数为 0 ──
    //    ⚠️ 这是「零违例」假象的**最小复现**:若不判「不可判」,就会把
    //    「没观测到」误报成「没违例」⇒ 正是 AC-14 边界 ⑥ 的禁区。
    {
        MesifProtocolEngine e("mesif_bounded1", &eq);
        e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
        e.enable_observations(true);
        open_flows(e);
        e.set_observation_capacity(1);

        drive_write(e, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);

        REQUIRE(e.observation_count() == 1u);
        REQUIRE(e.dropped_observations() == 2u);

        const auto v = detect(e.observations(), {}, e.dropped_observations(),
                              cep::responder_mask_of(cep::kNodeIdSdma),
                              /*observations_enabled=*/true);
        // ① 判定行丢失 ⇒ 计数为 0
        CHECK(v.write_episodes == 0u);
        CHECK(v.swmr_violations == 0u);
        // ② ⚠️ 但**必须**判「不可判」⇒ ⛔ 不得把这组 0 记成「零违例」
        CHECK(v.invalid == true);
        REQUIRE(v.invalid_reason.find("丢弃") != std::string::npos);
    }
}

// ══════════════════════════════════════════════════════════════════════
//  AC-2 · 三段连通 —— **行为级**负控(per 验收 G-2)
//  ⚠️ **诚实边界**:本组是「行为级」见证,⭔ **非** AC-2 原文要求的
//    「拔掉任一段 ⇒ 按签名红」(影子树 / 变异机制)⇒ **部分达成**。
//    三段中「断言不落」一段**仍未可机判** ⇒ 见验收报告的 ⚠️ 判定。
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP 档 2 观测面:三段连通(行为级) —— 拔掉「观测面」段 ⇒ 判不可判(⛔ 不是零违例)",
          "[cep][tier2][observation]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine a("mesif_conn_a", &eq);
    MesifProtocolEngine b("mesif_conn_b", &eq);
    a.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    b.set_tracker(&dir, cep::kNodeIdVram, /*level=*/0);
    a.enable_observations(true);
    b.enable_observations(true);
    open_flows(a);
    open_flows(b);

    // A 先持有 ⇒ B 写时他人位非空
    drive_write(a, kAddr, /*txn=*/1, /*data=*/0x11, cep::Perm::WRITEBACK);
    drive_write(b, kAddr, /*txn=*/2, /*data=*/0x22, cep::Perm::WRITEBACK);

    const std::uint64_t known =
        cep::responder_mask_of(cep::kNodeIdSdma) | cep::responder_mask_of(cep::kNodeIdVram);

    // ── 段 ①:观测面 → 判定 → 断言,**正向**贯通 ──
    const auto v_on = detect(b.observations(), {}, /*dropped=*/0u, known,
                             /*observations_enabled=*/true);
    REQUIRE(v_on.write_episodes == 1u);
    REQUIRE(v_on.swmr_violations == 1u);

    // ── 段 ②:「判定不收」—— 只喂**非判定行**(fill 行)⇒ 判定行数 0 ──
    //    ⚠️ 这证明判定行的**选择**是决定性的(装行行**显式豁免**)
    std::vector<EventObservation> fills_only;
    for (const auto& o : b.observations())
        if (o.kind == EventObservation::Kind::Fill)
            fills_only.push_back(o);
    REQUIRE(!fills_only.empty());
    const auto v_fills = detect(fills_only, {}, /*dropped=*/0u, known,
                                /*observations_enabled=*/true);
    CHECK(v_fills.write_episodes == 0u);
    CHECK(v_fills.swmr_violations == 0u);
    CHECK(v_fills.fill_records == fills_only.size());

    // ── 段 ③:「观测面不产记录」—— 同款驱动,仅「观测面关闭」一个差异 ──
    MesifProtocolEngine c("mesif_conn_c", &eq);
    c.set_tracker(&dir, cep::kNodeIdVram, /*level=*/0);
    // ⚠️ **刻意不开** `enable_observations`
    open_flows(c);
    drive_write(a, kAddr, /*txn=*/3, /*data=*/0x33, cep::Perm::WRITEBACK);
    drive_write(c, kAddr, /*txn=*/4, /*data=*/0x44, cep::Perm::WRITEBACK);

    REQUIRE(c.observation_count() == 0u); // 零记录
    const auto v_off = detect(c.observations(), {}, /*dropped=*/0u, known,
                              /*observations_enabled=*/false);
    CHECK(v_off.write_episodes == 0u);
    CHECK(v_off.swmr_violations == 0u);
    // ⚠️ **关键**:那组 0 **必须**被标成「不可判」⇒ ⛔ 不得记成「零违例」
    CHECK(v_off.invalid == true);
    REQUIRE(v_off.invalid_reason.find("不可判") != std::string::npos);
    // ⚠️ **诚实边界**:「断言不落」一段(断言被删除 ⇒ 闸应变红)**仍未可机判**
    //    —— 那需要影子树 / 变异机制,⭔ 本轮不提供 ⇒ **部分达成**。
}
