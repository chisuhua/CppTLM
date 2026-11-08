// test/test_cep_stub_cache_emit.cc
// 判据:**CEP E2E 装置的实验产物产出**(change `cpptlm-cep-stub-cache-chstream-surface` · A8)
//
//   E2E-01 仅 `CEP_EXP_DIR` 设置时产出 `metrics.json` + `simulation_config.json`
//         (⛔ 不设 ⇒ **不产出任何文件**;产物落在 `$CEP_EXP_DIR`)
//   E2E-02 13 键**各有结论**:实测值 **或** `not-collected` + reason
//   E2E-03 `simulation_config.json` 含 **provenance**(种子 / git rev / 工具链)
//         + `topology_tag` + `verification_mode` / `verification_scope`
//
// ⚠️ **本 run 不是实验证据 run**(诚实边界,per ADR-CEP-06 SU#1 §④ + SU#2 ①):
//   · ⛔ **不绑定**任何**预登记假设**(`hypothesis_id = "framework-e2e"` ⇒ 不在
//     `experimental/preregistration.json` 的 registered 行内)⇒ ⛔ **不得**作 B4 证据、
//     ⛔ **不得**改任何 hypothesis 结论;
//   · ⛔ 违例类 `memory_violations` 无样本 ⇒ IC3 = **⚪ 不适用** ⇒ 该 run
//     **不得**作任何一致性安全断言的证据;
//   · ✅ 它的用途 = **可对账**(台账 `metrics_sha256` ↔ 仓内 `metrics.json`)。
//
// ⚠️ **延迟族未采集的原因**(框架限制,非装置缺陷):dual-port 适配器的**入向路由恒落 PE 侧**
//   (`include/framework/bidirectional_port_adapter.hh` 的 `process_request_input` 只填
//   `pe_req_in()`)⇒ **回程不可达** ⇒ 本装置**没有已完成往返** ⇒ 延迟族无可测样本。
//   回程腿在**模块级**判据(`test_cep_stub_cache_e2e.cc` SCE-03/SCE-14)闭合。
//
// 作者: builder  日期: 2026-11-09
#include "catch_amalgamated.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "chstream_register.hh"
#include "cep_adr_parser.hh"
#include "cep_experiment_harness.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/cohif_responder.hh"
#include "tlm/cep/stub_cache_endpoint.hh"

namespace {

    constexpr int kRunCycles = 300;
    constexpr std::uint64_t kRunSeed = 20261108;

    /// 装配配置加载(路径按**仓根**解析 ⇒ 与 CWD 无关)
    nlohmann::json load_topology(const char* file) {
        const auto path = cpptlm::cep::repo_root() / "configs" / file;
        std::ifstream in(path);
        REQUIRE(in.good());
        nlohmann::json cfg = nlohmann::json::parse(in);
        cfg["cep"]["protocol_instances"] =
            (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json")
                .string();
        return cfg;
    }

} // namespace

TEST_CASE("E2E-01: E2E 装置产出 `metrics.json` + `simulation_config.json`(仅 CEP_EXP_DIR 设置时)",
          "[cep][e2e][framework-emit]") {
    const char* dir = std::getenv("CEP_EXP_DIR");
    if (dir == nullptr || *dir == '\0') {
        SUCCEED("未设置 CEP_EXP_DIR ⇒ 不产出文件");
        return;
    }
    // ⚠️ 与预登记假设的 run(**h2/h3/h4/h6**)共用 `CEP_EXP_DIR` ⇒ 必须按 `CEP_EXP_ID` 隔离,
    //    否则本用例会**覆盖**那些 run 的产物(实测:曾把 h3 的 metrics.json 覆写成框架 run)。
    const char* exp_id_env = std::getenv("CEP_EXP_ID");
    if (exp_id_env != nullptr && *exp_id_env != '\0' &&
        std::string(exp_id_env) != "framework-e2e") {
        SUCCEED("CEP_EXP_ID 非 framework-e2e ⇒ 本用例不产出(避免覆盖预登记 run)");
        return;
    }

    REGISTER_ALL;
    const auto cfg = load_topology("cep_chi_rnf_e2e_a.json");
    EventQueue eq;
    ModuleFactory f(&eq);
    REQUIRE(f.instantiateAll(cfg));
    f.startAllTicks();

    auto* w = dynamic_cast<tlm::cep::StubCacheEndpointTLM*>(f.getInstance("w"));
    auto* home = dynamic_cast<tlm::cep::CohIfResponderTLM*>(f.getInstance("home"));
    REQUIRE(w != nullptr);
    REQUIRE(home != nullptr);

    // ── 跑固定周期预算;逐周期喂 `ProgressWatchdog`(停滞判定)──
    cep_exp::ProgressWatchdog watchdog(1000);
    std::uint64_t coherence_stall_cycles = 0;
    for (int i = 0; i < kRunCycles; ++i) {
        eq.run(1);
        const std::uint64_t in_flight = w->in_flight();
        if (in_flight > 0)
            ++coherence_stall_cycles;
        watchdog.tick(in_flight, /*retired=*/0);
    }
    REQUIRE_FALSE(watchdog.stalled()); // 跑满预算仍无停滞 ⇒ 死锁判据成立

    // ── 组装 13 键(实测 **或** `not-collected` + reason)──
    cep_exp::ExperimentMetrics m;
    m.total_cycles = kRunCycles;
    m.total_transactions = w->counters().cpu_requests;
    m.coherence_stalls = coherence_stall_cycles;
    m.deadlock_detected = watchdog.stalled() ? 1 : 0;
    m.verification_mode = "sampled";
    m.verification_scope = "CEP E2E 装置 · 拓扑 A(configs/cep_chi_rnf_e2e_a.json) · 300 cycles";

    // 实测 5 键
    m.measured.insert("total_cycles");
    m.measured.insert("total_transactions");
    m.measured.insert("transactions_per_cycle");
    m.measured.insert("coherence_stalls");
    m.measured.insert("deadlock_violations");

    // 未采集 8 键(逐键 reason;⛔ 不填 0)
    const char* kLatencyReason =
        "框架层回程不可达(dual-port 适配器入向路由恒落 PE 侧)⇒ 本装置无已完成往返 ⇒ 延迟族无样本";
    for (const char* k : {"avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
                          "p99_latency_cycles", "max_latency_cycles"})
        m.not_collected[k] = kLatencyReason;
    m.not_collected["collective_stalls"] = "本装置无 collective plane 绑定 ⇒ 无产出者";
    m.not_collected["directory_misses"] = "本装置无 directory / sharer tracker ⇒ 无产出者";
    m.not_collected["memory_violations"] = "本 run 无内存操作样本 ⇒ 无可检样本";

    // 13 键各有结论 + 装置自洽
    std::string missing;
    REQUIRE(cep_exp::metrics_13_covered(m, &missing));
    REQUIRE(missing.empty());
    REQUIRE(m.well_formed());
    // IC3 = ⚪ 不适用(违例类 `memory_violations` 未采集)⇒ ⛔ 不得记「绿」
    REQUIRE(cep_exp::ic3_verdict(m) == cep_exp::Ic3Verdict::NotApplicable);

    // ── 落盘 ──
    const std::filesystem::path out(dir);
    std::filesystem::create_directories(out);
    {
        std::ofstream os(out / "metrics.json");
        REQUIRE(os.good());
        os << m.to_json();
    }
    {
        cep_exp::ExperimentProvenance prov;
        prov.seed = kRunSeed;
        prov.git_rev =
            cep_exp::ExperimentProvenance::current_git_rev(cpptlm::cep::repo_root().string());
        prov.toolchain = cep_exp::ExperimentProvenance::current_toolchain();

        std::ofstream os(out / "simulation_config.json");
        REQUIRE(os.good());
        os << "{\n";
        os << "  \"simulation_mode\": \"functional\",\n";
        os << "  \"verification_mode\": \"" << m.verification_mode << "\",\n";
        os << "  \"verification_scope\": \"" << m.verification_scope << "\",\n";
        os << "  \"hypothesis_id\": \"framework-e2e\",\n";
        os << "  \"topology_tag\": \"A\",\n";
        os << "  \"run_class\": \"framework-verification\",\n";
        os << "  \"not_an_experiment_run\": true,\n";
        os << "  \"evidence_scope\": "
           << "\"可对账(台账 metrics_sha256 ↔ 仓内 metrics.json);⛔ 非 B4 证据;"
              "⛔ 非一致性安全断言(IC3=⚪ 不适用)\",\n";
        os << "  \"provenance\": " << prov.to_json() << "\n";
        os << "}\n";
    }

    REQUIRE(std::filesystem::exists(out / "metrics.json"));
    REQUIRE(std::filesystem::exists(out / "simulation_config.json"));
    // ⛔ retention 丙:run 目录**只**含 2 件(trace.h5 / report.html 不入库)
    std::size_t file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(out)) {
        (void)entry;
        ++file_count;
    }
    REQUIRE(file_count == 2u);
}
