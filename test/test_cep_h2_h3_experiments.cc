// test/test_cep_h2_h3_experiments.cc
// H2 / H3 实验：目录策略延迟对比 + CONSERVATIVE 完备性
//
// 作者: CppTLM Team / 日期: 2026-11-04
//
// 判据来源(`obj-cep-platform.md §9.1`):
//   H2 —— 集中(CHI HN-F) vs 分布(DCOH) vs 广播 的 latency 差异可量化;成功判据 **> 20%**
//   H3 —— CONSERVATIVE 完备性在 CXL 对称下无漏 snoop;成功判据 **0 违例(1000+ 事务)**
//
// ⚠️ 口径限定(与 `cep_directory_experiment.hh` 文件头一致):
//    本实验固定 RN 侧行为,只变化 home 侧目录策略 ⇒ 结论 = 「**目录策略**对延迟的影响」,
//    **不是**「CHI 协议 vs CXL 协议的延迟差」。
//
// 纪律:
//   · H2 的「>20%」必须配**反向 fixture**:三策略全用同一 tracker ⇒ 差异 0 ⇒ 判据不成立
//   · H3 的「0 违例」必须配**反向 fixture**:跳过 invalidate ⇒ 违例 > 0
//   · 每条实验都做**可复现**检查(同配置重跑,结果一致)

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "cep_directory_experiment.hh"
#include "cep_experiment_harness.hh"
#include "tlm/cep/broadcast_null_tracker.hh"
#include "tlm/cep/cxl_dcoh_tracker.hh"
#include "tlm/cep/sharer_tracker.hh"

using namespace cep_exp;
using namespace tlm::cep;

namespace {

    constexpr std::uint64_t kAddr = 0x1000;
    constexpr std::uint64_t kRegionBase = 0x1000;
    constexpr std::uint64_t kRegionSize = 0x1000;

    /// 相对差(以较小值为基准;0 基准时退化为「绝对差」语义避免除零)
    double pct_diff(double a, double b) {
        const double lo = std::min(a, b);
        if (lo == 0.0)
            return a == b ? 0.0 : 100.0;
        return std::fabs(a - b) / lo * 100.0;
    }

    /// 同一 workload 跑 N 次读,返回平均延迟。
    /// @param tracker 具体目录策略(调用方负责安装初始 sharer)
    double run_reads(SharerTrackerIf& tracker, int n) {
        HomeNodeModel home(tracker);
        LatencyCollector lat;
        for (int i = 0; i < n; ++i) {
            lat.note_issue();
            const auto r = home.service_read(kAddr);
            lat.note_complete(0, r.latency_cycles);
        }
        REQUIRE(lat.audit_balanced());
        return lat.avg();
    }

    /// 安装 2 个 sharer(bit0 = Sdma / bit1 = Vram) —— 语义说明用,实际由各 SECTION 直接调用
    /// ⚠️ 三个策略的安装面签名不同 ⇒ 不在此抽象(避免造一个假的统一接口)

} // namespace

// ══════════════════════════════════════════════════════════════════
// H2 —— 目录策略延迟对比
// ══════════════════════════════════════════════════════════════════

TEST_CASE("H2 · 三种目录策略的延迟差异 > 20%", "[cep][exp][h2]") {
    // ── 集中(FullMapDirectory,EXACT)──
    FullMapDirectory full_map;
    full_map.set_sharer(kAddr, kNodeIdSdma, SnoopState::S);
    full_map.set_sharer(kAddr, kNodeIdVram, SnoopState::S);

    // ── 分布(DcohSymmetric,CONSERVATIVE)──
    DcohSymmetric dcoh(DcohSymmetric::LocalRegion{kRegionBase, kRegionSize});
    dcoh.set_sharer(kAddr, kNodeIdSdma, SnoopState::S);
    dcoh.set_sharer(kAddr, kNodeIdVram, SnoopState::S);

    // ── 广播(BroadcastNull,无目录)──
    BroadcastNullTracker broadcast;

    const double avg_full = run_reads(full_map, 200);
    const double avg_dcoh = run_reads(dcoh, 200);
    const double avg_bcast = run_reads(broadcast, 200);

    INFO("full_map avg = " << avg_full);
    INFO("dcoh avg     = " << avg_dcoh);
    INFO("broadcast avg= " << avg_bcast);

    SECTION("集中 vs 分布") {
        // FullMap 的 EXACT 视图能直接回答(0 snoop);Dcoh 在「多 sharer」时保守广播
        REQUIRE(pct_diff(avg_full, avg_dcoh) > 20.0);
    }
    SECTION("分布 vs 广播") {
        REQUIRE(pct_diff(avg_dcoh, avg_bcast) > 20.0);
    }
    SECTION("集中 vs 广播") {
        REQUIRE(pct_diff(avg_full, avg_bcast) > 20.0);
    }
}

TEST_CASE("H2 · 反向 fixture:三策略全用同一 tracker ⇒ 差异 0 ⇒ 判据不成立",
          "[cep][exp][h2][reverse]") {
    // 三个「策略」都指向 FullMap ⇒ 任何 >20% 的判定都必须是假象
    FullMapDirectory a;
    FullMapDirectory b;
    FullMapDirectory c;
    for (auto* t : {&a, &b, &c}) {
        t->set_sharer(kAddr, kNodeIdSdma, SnoopState::S);
        t->set_sharer(kAddr, kNodeIdVram, SnoopState::S);
    }

    const double avg_a = run_reads(a, 200);
    const double avg_b = run_reads(b, 200);
    const double avg_c = run_reads(c, 200);

    REQUIRE(pct_diff(avg_a, avg_b) == Catch::Approx(0.0));
    REQUIRE(pct_diff(avg_b, avg_c) == Catch::Approx(0.0));
    // ⇒ 判据「至少一对 >20%」在此配置下**不成立**(证明上面那条不是恒真)
    REQUIRE_FALSE(pct_diff(avg_a, avg_c) > 20.0);
}

TEST_CASE("H2 · 同一配置重跑结果可复现", "[cep][exp][h2]") {
    const auto once = []() {
        DcohSymmetric d(DcohSymmetric::LocalRegion{kRegionBase, kRegionSize});
        d.set_sharer(kAddr, kNodeIdSdma, SnoopState::S);
        d.set_sharer(kAddr, kNodeIdVram, SnoopState::S);
        return run_reads(d, 150);
    };
    REQUIRE(once() == Catch::Approx(once()));
}

// ══════════════════════════════════════════════════════════════════
// H3 —— CONSERVATIVE 完备性 + 运行时双不变量
// ══════════════════════════════════════════════════════════════════

TEST_CASE("H3 · 1200 事务下 CONSERVATIVE 不漏探且 0 违例", "[cep][exp][h3]") {
    DcohSymmetric dcoh(DcohSymmetric::LocalRegion{kRegionBase, kRegionSize});
    HomeNodeModel home(dcoh);
    SwmrDataValueDetector detector;
    LatencyCollector lat;

    const std::vector<node_id_t> candidates{kNodeIdSdma, kNodeIdVram, kNodeIdGmmu};
    std::uint64_t superset_violations = 0;

    constexpr int kTransactions = 1200;
    for (int i = 0; i < kTransactions; ++i) {
        // 本轮真实 sharer 子集:1~3 个(轮换,保证覆盖不同规模)
        const std::size_t want = static_cast<std::size_t>(i % 3) + 1;
        responder_set_t truth = 0;
        for (std::size_t k = 0; k < want; ++k) {
            const node_id_t node =
                candidates[(static_cast<std::size_t>(i) + k) % candidates.size()];
            dcoh.set_sharer(kAddr, node, SnoopState::S);
            truth |= responder_mask_of(node);
        }

        // ── 读:CONSERVATIVE 的上界必须覆盖真实 sharer 集 ──
        lat.note_issue();
        const auto rd = home.service_read(kAddr);
        lat.note_complete(0, rd.latency_cycles);

        const LocalView view = dcoh.view_of(kAddr);
        if ((view.sharers & truth) != truth)
            ++superset_violations; // 漏探 ⇒ 违例

        // ── 写:先经 home 失效全部 sharer(合法序列)──
        const auto wr = home.service_write(kAddr);
        INFO("want = " << want << ", snoop = " << wr.snoop_count);
        REQUIRE(wr.snoop_count == want); // 写 SHALL 探到本轮的**全部**真实 sharer(不漏探)

        // 合法序列:失效完成后再写数据
        detector.write(kAddr, static_cast<std::uint64_t>(i));

        // 读回同版本(合法)
        detector.read(kAddr, static_cast<std::uint64_t>(i));

        // 本轮收尾:清空该地址的目录条目,下一轮重新安装
        for (const auto node : candidates)
            dcoh.clear_sharer(kAddr, node);
    }

    INFO("superset violations = " << superset_violations);
    INFO("swmr = " << detector.counts().swmr);
    INFO("data_value = " << detector.counts().data_value);

    REQUIRE(lat.audit_balanced());
    REQUIRE(lat.sample_count() == kTransactions);
    REQUIRE(superset_violations == 0);    // 无漏探
    REQUIRE(detector.counts().swmr == 0); // 双不变量 0 违例
    REQUIRE(detector.counts().data_value == 0);
}

TEST_CASE("H3 · 反向 fixture:跳过 invalidate ⇒ SWMR 违例必须 > 0", "[cep][exp][h3][reverse]") {
    DcohSymmetric dcoh(DcohSymmetric::LocalRegion{kRegionBase, kRegionSize});
    SwmrDataValueDetector detector;

    // 第一次写(该地址尚无写者)
    detector.write(kAddr, 1);

    // 跳过 invalidate 直接再写 —— 这正是 CONSERVATIVE 契约要防的序列
    for (int i = 0; i < 10; ++i)
        detector.write_without_invalidate(kAddr, static_cast<std::uint64_t>(i + 2));

    INFO("swmr = " << detector.counts().swmr);
    REQUIRE(detector.counts().swmr == 10);
    REQUIRE(detector.counts().total() > 0);
}

TEST_CASE("H3 · 反向 fixture:读到过期版本 ⇒ data-value 违例必须 > 0", "[cep][exp][h3][reverse]") {
    SwmrDataValueDetector detector;
    detector.write(kAddr, /*version=*/100);
    detector.read(kAddr, /*observed=*/99); // 读到旧版本
    REQUIRE(detector.counts().data_value == 1);
}
