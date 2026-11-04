// test/test_cep_experiment_emit.cc
// 4 件套产出器(H2/H3/H6 的真实 metrics)
//
// 作者: CppTLM Team / 日期: 2026-11-04
//
// 契约(per `tools/run_sweep.py` 文件头):
//   · 输出目录经 `CEP_EXP_DIR` 环境变量告知
//   · 组合 id 经 `CEP_EXP_ID` 告知(`h2` / `h3` / `h6`)
//   · 本文件只产 **输入 3 件套**中的 2 件(`simulation_config.json` + `metrics.json`);
//     `trace.h5` 由 `scripts/test/cep_experiment_run.sh` 用 h5py 补,
//     `report.html` 由 `tools/render_report.py` 产 —— 链路不自相矛盾。
//
// ⚠️ 未设置 `CEP_EXP_DIR` ⇒ 本用例**不产出任何文件**(普通 ctest 无副作用)

#include "catch_amalgamated.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cep_directory_experiment.hh"
#include "cep_experiment_harness.hh"
#include "cep_vc_fabric_model.hh"
#include "tlm/cep/broadcast_null_tracker.hh"
#include "tlm/cep/cxl_dcoh_tracker.hh"
#include "tlm/cep/sharer_tracker.hh"

using namespace cep_exp;
using namespace tlm::cep;

namespace {

    constexpr std::uint64_t kAddr = 0x1000;
    constexpr std::uint64_t kRegionBase = 0x1000;
    constexpr std::uint64_t kRegionSize = 0x1000;

    struct EmitResult {
        ExperimentMetrics metrics;
        std::string scope;
    };

    /// H2:三策略各跑 N 次读,取**集中策略之外**的最差延迟作为本 run 的代表值,
    /// 同时把三策略的 avg 写进 directory_misses 之外的旁路字段? —— 不行,metrics 是固定 schema。
    /// ⇒ 本 run 的代表值 = **分布策略**(H2 的假设主体),三策略对比值写 scope 描述。
    EmitResult run_h2(int n) {
        FullMapDirectory full_map;
        full_map.set_sharer(kAddr, kNodeIdSdma, SnoopState::S);
        full_map.set_sharer(kAddr, kNodeIdVram, SnoopState::S);

        DcohSymmetric dcoh(DcohSymmetric::LocalRegion{kRegionBase, kRegionSize});
        dcoh.set_sharer(kAddr, kNodeIdSdma, SnoopState::S);
        dcoh.set_sharer(kAddr, kNodeIdVram, SnoopState::S);

        BroadcastNullTracker bcast;

        const auto avg_of = [n](SharerTrackerIf& t) {
            HomeNodeModel home(t);
            LatencyCollector lat;
            for (int i = 0; i < n; ++i) {
                lat.note_issue();
                lat.note_complete(0, home.service_read(kAddr).latency_cycles);
            }
            return lat;
        };

        LatencyCollector lat_full = avg_of(full_map);
        LatencyCollector lat_dcoh = avg_of(dcoh);
        LatencyCollector lat_bcast = avg_of(bcast);

        EmitResult r;
        // 代表值取**分布策略**(H2 的假设主体是 DCOH 的 CONSERVATIVE 行为)
        r.metrics.latency = lat_dcoh;
        r.metrics.total_transactions = static_cast<std::uint64_t>(n);
        r.metrics.total_cycles = static_cast<std::uint64_t>(n) * 20; // 每次访问成本上界
        r.metrics.verification_mode = "bounded_random";
        r.scope = "H2 目录策略对比: 集中(FullMap) avg=" + std::to_string(lat_full.avg()) +
                  " / 分布(Dcoh) avg=" + std::to_string(lat_dcoh.avg()) +
                  " / 广播(BroadcastNull) avg=" + std::to_string(lat_bcast.avg()) + " cycles; 各 " +
                  std::to_string(n) + " 次读; 固定 RN 侧行为";
        r.metrics.verification_scope = r.scope;
        return r;
    }

    /// H3:CONSERVATIVE 完备性 + 双不变量(1200 事务)
    EmitResult run_h3(int n) {
        DcohSymmetric dcoh(DcohSymmetric::LocalRegion{kRegionBase, kRegionSize});
        HomeNodeModel home(dcoh);
        SwmrDataValueDetector detector;
        LatencyCollector lat;

        const std::vector<node_id_t> candidates{kNodeIdSdma, kNodeIdVram, kNodeIdGmmu};
        std::uint64_t superset_violations = 0;

        for (int i = 0; i < n; ++i) {
            const std::size_t want = static_cast<std::size_t>(i % 3) + 1;
            responder_set_t truth = 0;
            for (std::size_t k = 0; k < want; ++k) {
                const node_id_t node =
                    candidates[(static_cast<std::size_t>(i) + k) % candidates.size()];
                dcoh.set_sharer(kAddr, node, SnoopState::S);
                truth |= responder_mask_of(node);
            }

            lat.note_issue();
            lat.note_complete(0, home.service_read(kAddr).latency_cycles);

            if ((dcoh.view_of(kAddr).sharers & truth) != truth)
                ++superset_violations;

            home.service_write(kAddr);
            detector.write(kAddr, static_cast<std::uint64_t>(i));
            detector.read(kAddr, static_cast<std::uint64_t>(i));

            for (const auto node : candidates)
                dcoh.clear_sharer(kAddr, node);
        }

        EmitResult r;
        r.metrics.latency = lat;
        r.metrics.total_transactions = static_cast<std::uint64_t>(n);
        r.metrics.total_cycles = static_cast<std::uint64_t>(n) * 20;
        r.metrics.violations = detector.counts();
        r.metrics.verification_mode = "bounded_random";
        r.scope = "H3 CONSERVATIVE 完备性: " + std::to_string(n) + " 事务 / 3 节点 / 1 地址; " +
                  "漏探=" + std::to_string(superset_violations) +
                  "; 双不变量 swmr=" + std::to_string(detector.counts().swmr) +
                  " data_value=" + std::to_string(detector.counts().data_value);
        r.metrics.verification_scope = r.scope;
        return r;
    }

    /// H6:独立 VC vs 共享 VC(各 2000 cycle)
    EmitResult run_h6(int cycles) {
        VcFabricConfig iso;
        iso.isolate_collective = true;
        VcFabricModel m_iso(iso);
        for (int i = 0; i < cycles; ++i) {
            m_iso.inject(TrafficClass::Coherence, 2);
            m_iso.inject(TrafficClass::Collective, 1);
            m_iso.tick();
        }

        VcFabricConfig shared;
        shared.isolate_collective = false;
        VcFabricModel m_shared(shared);
        for (int i = 0; i < cycles; ++i) {
            m_shared.inject(TrafficClass::Coherence, 2);
            m_shared.inject(TrafficClass::Collective, 1);
            m_shared.tick();
        }

        EmitResult r;
        LatencyCollector lat;
        const std::uint64_t txns = m_iso.served_per_vc()[0] + m_iso.served_per_vc()[3];
        for (std::uint64_t i = 0; i < txns; ++i) {
            lat.note_issue();
            lat.note_complete(0, 0);
        }
        r.metrics.latency = lat;
        r.metrics.total_transactions = txns;
        r.metrics.total_cycles = static_cast<std::uint64_t>(cycles);
        r.metrics.deadlock_detected = m_iso.deadlock_violations();
        r.metrics.collective_stalls = m_iso.collective_wait_on_coherence_vc();
        for (std::size_t vc = 0; vc < m_iso.stall_per_vc().size(); ++vc)
            r.metrics.vc_stall_per_vc[static_cast<int>(vc)] = m_iso.stall_per_vc()[vc];
        r.metrics.verification_mode = "bounded_random";
        r.scope = "H6 VC 隔离: " + std::to_string(cycles) +
                  " cycle; 独立VC 死锁=" + std::to_string(m_iso.deadlock_violations()) +
                  " 且 collective 占 coherence VC 等待=" +
                  std::to_string(m_iso.collective_wait_on_coherence_vc()) +
                  "; 共享VC(负控) 死锁=" + std::to_string(m_shared.deadlock_violations()) +
                  " collective backlog=" + std::to_string(m_shared.collective_backlog()) +
                  "; 本实验不采延迟(avg/p50/p95 恒 0)";
        r.metrics.verification_scope = r.scope;
        return r;
    }

} // namespace

TEST_CASE("实验产出 · 4 件套输入(仅 CEP_EXP_DIR 设置时)", "[cep][exp][emit]") {
    const char* dir = std::getenv("CEP_EXP_DIR");
    if (dir == nullptr || *dir == '\0') {
        SUCCEED("未设置 CEP_EXP_DIR ⇒ 不产出文件");
        return;
    }
    const char* id = std::getenv("CEP_EXP_ID");
    const std::string exp_id = (id == nullptr || *id == '\0') ? std::string("h2") : std::string(id);

    EmitResult r;
    if (exp_id == "h2")
        r = run_h2(200);
    else if (exp_id == "h3")
        r = run_h3(1200);
    else if (exp_id == "h6")
        r = run_h6(2000);
    else {
        FAIL("未知 CEP_EXP_ID: " << exp_id);
    }

    REQUIRE(r.metrics.well_formed());

    const std::filesystem::path out(dir);
    std::filesystem::create_directories(out);

    {
        std::ofstream os(out / "metrics.json");
        REQUIRE(os.good());
        os << r.metrics.to_json();
    }
    {
        std::ofstream os(out / "simulation_config.json");
        REQUIRE(os.good());
        os << "{\"simulation_mode\":\"timing\",\"verification_mode\":\""
           << r.metrics.verification_mode << "\",\"verification_scope\":\""
           << r.metrics.verification_scope << "\"}\n";
    }

    REQUIRE(std::filesystem::exists(out / "metrics.json"));
    REQUIRE(std::filesystem::exists(out / "simulation_config.json"));
}
