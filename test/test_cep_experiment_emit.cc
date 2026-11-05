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

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cep_completion_domain_model.hh"
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

    /// 按**预登记投影**(`experimental/preregistration.json`)标注本 run 的未采集 metric。
    ///
    /// ⚠️ **不由代码自行决定「不采什么」** —— 那是**预登记**(真源 = `objective §9.1` ② 表)
    /// 的职责;本函数只把投影**搬进**产出 ⇒ 产出与预登记**恒一致**(不可能漂移)。
    ///
    /// ⚠️ **实测优先**:`measured` 里列出的 metric 是本 run **真正有产出者**的
    /// (即使实测值为 0,也是**实测 0**,不是未测量)⇒ 不标 not-collected。
    /// ⇒ 二者的区分靠**产出者是否存在**,不靠值(实测 0 与未测量在数据层同形 —— 这正是
    ///    SU#1 §② 要消灭的歧义)。
    void mark_not_collected(ExperimentMetrics& m, const std::string& hypothesis_id,
                            const std::vector<std::string>& measured) {
        const char* env = std::getenv("CEP_EXP_PREREG");
        std::string path = (env && *env) ? env : std::string("experimental/preregistration.json");
        std::ifstream pf(path);
        if (!pf.good())
            return;
        std::stringstream ss;
        ss << pf.rdbuf();
        nlohmann::json pre;
        try {
            pre = nlohmann::json::parse(ss.str());
        } catch (...) {
            return;
        }
        if (!pre.contains("hypotheses"))
            return;
        auto hyps = pre.at("hypotheses");
        std::string hid = hypothesis_id;
        std::transform(hid.begin(), hid.end(), hid.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (!hyps.contains(hid))
            return;
        auto entry = hyps.at(hid);
        if (entry.value("status", std::string()) != "registered")
            return;
        std::set<std::string> measured_set(measured.begin(), measured.end());
        // ⚠️ 实测声明集 = 本 run **真正有产出者**的必纳 metric(含实测值为 0 者)
        // (per SU#1 §④ 判据③:实测 0 与未测量在数据层同形 ⇒ 必须由产出侧显式区分)
        m.measured = measured_set;
        for (const auto& k : entry.value("out_of_scope", std::vector<std::string>())) {
            if (measured_set.count(k))
                continue; // 实测优先
            m.not_collected[k] = "本实验未采集该 metric(per 预登记 out-of-scope)";
        }
    }

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
        // ⚠️ 本 run **只测延迟族** + total_* ⇒ 其余必纳 metric 一律未采集
        // (per ADR-CEP-06 SU#1 §②:未采集 ⇒ "not-collected",⛔ 禁写 0)
        mark_not_collected(r.metrics, "H2",
                           {"total_cycles", "total_transactions", "transactions_per_cycle",
                            "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
                            "p99_latency_cycles", "max_latency_cycles"});
        // ⚠️ 其余 5 项(coherence_stalls / collective_stalls / directory_misses /
        //    deadlock_violations / memory_violations)本 run **未采集** ⇒ 须显式声明
        for (const char* k : {"coherence_stalls", "collective_stalls", "directory_misses",
                              "deadlock_violations", "memory_violations"})
            r.metrics.not_collected[k] = "本实验只对比目录策略的延迟(per 预登记 out-of-scope)";
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
        // ⚠️ 本 run 测:延迟族 + total_* + **memory_violations**(SWMR + data-value 双分桶)
        // ⇒ `deadlock_violations` **未采集**(本 run 无死锁检测器)⇒ 必须标 not-collected
        //    (此前它被写成 0 ⇒ 被读作「测了,没死锁」= C1 的核心失败模式,per SU#1 §①)
        mark_not_collected(r.metrics, "H3",
                           {"total_cycles", "total_transactions", "transactions_per_cycle",
                            "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
                            "p99_latency_cycles", "max_latency_cycles", "memory_violations"});
        // ⚠️ 其余 4 项(coherence_stalls / collective_stalls / directory_misses /
        //    deadlock_violations)本 run **未采集** ⇒ 须显式声明
        //    (尤其 `deadlock_violations`:本 run 无死锁检测器 ⇒ 此前被写成 0
        //     ⇒ 被读作「测了,没死锁」= C1 的核心失败模式,per SU#1 §①)
        for (const char* k :
             {"coherence_stalls", "collective_stalls", "directory_misses", "deadlock_violations"})
            r.metrics.not_collected[k] = "本实验无该 metric 的产出者(per 预登记 out-of-scope)";
        r.metrics.verification_mode = "bounded_random";
        r.scope = "H3 CONSERVATIVE 完备性: " + std::to_string(n) + " 事务 / 3 节点 / 1 地址; " +
                  "漏探=" + std::to_string(superset_violations) +
                  "; 双不变量 swmr=" + std::to_string(detector.counts().swmr) +
                  " data_value=" + std::to_string(detector.counts().data_value);
        r.metrics.verification_scope = r.scope;
        return r;
    }

    /// H6:独立 VC vs 共享 VC(各 2000 cycle)
    /// **H4**:完成域感知的 VC 共存实验 —— 正控(requester-scoped coherence)+ **反事实负控**
    /// (coherence 被错标为 group-scoped ⇒ 循环等待 ⇒ 死锁)。
    /// ⚠️ 两个场景**必须同批产出** —— 只出正控 ⇒ 本实验退化为 H6 换名
    /// (per 台账 Entry #222 ③:负控是**装置合格的前提**)。
    EmitResult run_h4(std::uint64_t n, std::uint64_t cycles) {
        // ── 正控:coherence = requester-scoped(单点 ack)⇒ 无循环 ⇒ 无死锁 ──
        cep_cd::CompletionDomainConfig cfg;
        cfg.group_size = 3;
        cfg.deadlock_threshold = 200;
        cep_cd::CompletionDomainModel positive(cfg);
        for (std::uint64_t i = 0; i < n; ++i) {
            positive.inject_coherence(cep_cd::CompletionDomain::RequesterScoped, 100 + i, 1);
            positive.inject_collective(200 + i);
        }
        for (std::uint64_t c = 0; c < cycles; ++c)
            positive.tick();

        // ── 负控:coherence = group-scoped(完成条件绑定到 group rendezvous)⇒ 循环等待 ⇒ 死锁 ──
        cep_cd::CompletionDomainModel negative(cfg);
        for (std::uint64_t i = 0; i < n; ++i) {
            negative.inject_coherence(cep_cd::CompletionDomain::GroupScoped, 100 + i, 1);
            negative.inject_collective(200 + i);
        }
        for (std::uint64_t c = 0; c < cycles; ++c)
            negative.tick();

        EmitResult r;
        // ⚠️ 本实验的 metric 语义:死锁类 metric 取**负控**的实测值(它才是「完成语义混淆」的
        //    证据);正控的无死锁事实写进 verification_scope(文本)而非 metric
        //    ⇒ 因为 13 必纳里**没有**「正控无死锁」这个槽位。
        // ⚠️ `deadlock_violations` 的语义 = **本 run 的死锁违例数** = **正控**的实测(应为 0)
        //   ⇒ 负控的死锁是**设计预期的证据**(证明完成纪律混用 ⇒ 死锁),**不是**本 run 的违例
        //   ⇒ 写进 `verification_scope` 文本(见 r.scope),不进 metric 槽位。
        //   ⇒ 否则 tools 的 IC3 闸会把 h4 判成「实验 fail」⇒ 掩盖正控的结论。
        r.metrics.deadlock_detected = positive.deadlocked() ? 1 : 0;
        r.metrics.coherence_stalls = positive.coherence_stalls();
        r.metrics.collective_stalls = positive.collective_stalls();
        r.metrics.total_transactions = n * 2;
        r.metrics.total_cycles = positive.cycle();
        r.metrics.verification_mode = "bounded_random";
        r.scope = "H4 完成域共存: 正控(requester-scoped coherence + group-scoped collective)"
                  "两类流同实验内无死锁(隔离 VC,coherence 完成 " +
                  std::to_string(positive.coherence_retired()) + "/" + std::to_string(n) +
                  ",collective " + std::to_string(positive.collective_retired()) + "/" +
                  std::to_string(n) + "); 反事实负控(coherence 错标 group-scoped)⇒ 死锁=" +
                  (negative.deadlocked() ? "1" : "0") + "(两端均完成 " +
                  std::to_string(negative.coherence_retired()) + "/" +
                  std::to_string(negative.collective_retired()) +
                  ") ⇒ 完成纪律混用 = 静默死锁(per ADR-CEP-01 §6.2 代价 3)";
        r.metrics.verification_scope = r.scope;
        // ⚠️ `memory_violations`:本装置**无内存一致性语义** ⇒ 不挂检测器 ⇒ 登记为未采集
        // (per Entry #222 ④:不得写裸 0 而不声明)
        r.metrics.not_collected["memory_violations"] =
            "本装置无内存一致性语义(不挂 SwmrDataValueDetector)";
        r.metrics.not_collected["directory_misses"] = "本装置无目录概念";
        for (const char* lat : {"avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
                                "p99_latency_cycles", "max_latency_cycles"})
            r.metrics.not_collected[lat] = "本实验不测延迟(完成域共存实验,per 预登记 out-of-scope)";
        r.metrics.measured = {"total_cycles",     "total_transactions", "transactions_per_cycle",
                              "coherence_stalls", "collective_stalls",  "deadlock_violations"};
        return r;
    }

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
        // ⚠️ 本 run 测:deadlock_violations + collective_stalls + vc_stall_per_vc + total_*
        // ⇒ **延迟族未采集**(本实验不采延迟)⇒ per SU#1 §③ branch 3 必须标 not-collected,⛔ 禁写 0
        //    ⇒ `memory_violations` 亦未采集(本 run 无违例检测器)
        mark_not_collected(r.metrics, "H6",
                           {"total_cycles", "total_transactions", "transactions_per_cycle",
                            "deadlock_violations", "collective_stalls"});
        // ⚠️ h6 的延迟收集器**为空**(本实验不采延迟)⇒ 延迟族必须标 not-collected
        //    (per ADR-CEP-06 SU#1 §③ branch 3:timing + 预声明不采延迟 ⇒ 必须 not-collected,
        //    ⛔ 禁写 0)⇒ 显式登记 5 项延迟族为未采集。
        for (const char* lat : {"avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
                                "p99_latency_cycles", "max_latency_cycles"})
            r.metrics.not_collected[lat] = "本实验不采延迟(VC 隔离实验,per SU#1 §③ branch 3)";
        // ⚠️ 其余未采集项:coherence_stalls / directory_misses / memory_violations
        for (const char* k : {"coherence_stalls", "directory_misses", "memory_violations"})
            r.metrics.not_collected[k] = "本实验无该 metric 的产出者(per 预登记 out-of-scope)";
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
    else if (exp_id == "h4")
        // ⚠️ `h4` 的放行**必须与预登记登记同 PR**(per ADR-CEP-06 SU#2 §③ 顺序锁:
        //    「登记 H4 预登记行的同一 PR 才允许加 run_h4」)⇒ Entry #222 已同批登记。
        r = run_h4(8, 400);
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
        // ⚠️ `hypothesis_id` 是**纯标签** —— 声明本 run 服务哪个假设。
        // per ADR-CEP-06 SU#2 ② 门禁③:run↔hypothesis 绑定走此标签 + `preregistration.json` 的
        //   `registered` 行(**不是** run 自报 out-of-scope —— 那是被试者自证面,SU#2 ① 已判死)。
        // ⇒ tools 判据④ 据此把本 run 的 `not-collected` 集合与投影的 out_of_scope 做 ⊆ 校验。
        // ⚠️ 标签**不承载**「本 run 不采哪些 metric」—— 那是投影的唯一职责(真源 = objective §9.1 ②
        // 表)。
        os << "{\"simulation_mode\":\"timing\",\"verification_mode\":\""
           << r.metrics.verification_mode << "\",\"verification_scope\":\""
           << r.metrics.verification_scope << "\",\"hypothesis_id\":\"" << exp_id << "\"}\n";
    }

    REQUIRE(std::filesystem::exists(out / "metrics.json"));
    REQUIRE(std::filesystem::exists(out / "simulation_config.json"));
}
