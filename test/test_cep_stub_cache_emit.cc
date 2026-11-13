// test/test_cep_stub_cache_emit.cc
// 判据:**CEP E2E 装置的实验产物产出**(change `cpptlm-cep-stub-cache-chstream-surface` · A8)
//
//   E2E-01 仅 `CEP_EXP_DIR` 设置时产出 `metrics.json` + `simulation_config.json`
//         (⛔ 不设 ⇒ **不产出任何文件**;产物落在 `$CEP_EXP_DIR`)
//   E2E-02 13 键**各有结论**:实测值 **或** `not-collected` + reason
//   E2E-03 `simulation_config.json` 含 **provenance**(种子 / git rev / 工具链)
//         + `topology_tag` + `verification_mode` / `verification_scope`
//
// ⚠️ **本 run 自 2026-11-13 起为「拓扑级实验 run」**(per change
//   `cpptlm-cep-full-green-run-evidence` · A3;台账 Entry #395 / #394):
//   · **绑定预登记行 `H3`**(`hypothesis_id = "h3"`,per `ADR-CEP-19` ASM-8 ① ——
//     拓扑级 run 须绑定 H3:唯一「双违例类均须实测」的已登记行)⇒ `run_class` /
//     `not_an_experiment_run` 两字段**已移除**(与 h2/h3/h4/h6 同形);
//     ⚠️ 与调用选择器 `CEP_EXP_ID`(仍为 `framework-e2e`)是**两回事** ——
//     后者只决定「本次测试调用是否产出」,前者决定「产物绑定哪条预登记行」;
//   · 违例类 `memory_violations` 由 `SwmrDataValueDetector` **逐条消费真实内存操作流**
//     ⇒ **实测为 0** ⇒ IC3 = **✓ 通过**(三态中该档位的实例);
//   · ⚠️ **口径限定(⛔ 必须随任何引用重复)**:本装置**无 sharer 语义** ⇒
//     **SWMR 违例结构上不可达**;`TrafficGenTLM` 的写数据**恒 0** + home 回程只回显
//     ⇒ **无版本语义** ⇒ data-value 检查**无判别力** ⇒ 该 0 是
//     「**本装置无该形态**」的 0,⛔ **不得**表述为「已证明协议层无违例」,
//     ⛔ **不得**称「检测力已证」;
//   · ✅ 用途 = **可对账**(台账 `metrics_sha256` ↔ 仓内 `metrics.json`)+
//     **拓扑级 IC3 通过态**的证据面。
//
// ⚠️ **延迟族自 2026-11-09 起为**实测值**(per change `cpptlm-cep-dual-port-inbound-dispatch`):
//   此前 dual-port 适配器的**入向路由恒落 PE 侧** ⇒ 回程不可达 ⇒ 延迟族只能记
//   `not-collected`(硬编码 reason)。⇒ 现双端口适配器的入向改为**按端口组**路由
//   (组 1 = Net ⇒ `net_resp_in()`)+ 拓扑补回程连接 ⇒ **已有完成往返**
//   ⇒ 延迟族由 wrapper 的 per-txn 周期戳(issue → complete)**实测**得出。
//
// 作者: builder  日期: 2026-11-09
#include "catch_amalgamated.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
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

    /// **生产喂料映射** —— ⛔ 单实现源:生产(E2E-01)与**喂料映射见证**用例**共用**本函数
    /// (per `ADR-CEP-20` 决策 6 / change `cep-evidence-tier-declaration` 的 AC-2)。
    ///
    /// 语义(`:is_write` / `:data` / `:addr` 三字段解释):
    ///   · 写 ⇒ `detector.write(addr, data)`(本装置无 sharer ⇒ invalidate 义务恒已满足)
    ///   · 读 ⇒ `detector.read(addr, data)`(观测值 vs 该地址最后一次写入值)
    ///   · `rejected` ⇒ 跳过(未真正访问存储阵列 ⇒ 无内存操作样本)
    ///
    /// ⚠️ **喂料通路对 SWMR 无通道(结构事实,⛔ 非缺陷)**:本函数只调**合法写** `write()`,
    ///    而 `SwmrDataValueDetector::write()` 无条件置 `written=true`、**不累加 `swmr`**
    ///    (`test/cep_experiment_harness.hh:157-162`)⇒ ⇒ **任何** `txn_stamps` 序列都只能产出
    ///    `swmr == 0` ⇒ ⛔ **不得**把该 0 读成「SWMR 检测力已见证」(per `ADR-CEP-20` 决策 3c)。
    void feed(cep_exp::SwmrDataValueDetector& detector,
              const std::vector<tlm::cep::StubCacheEndpointTLM::TxnStamp>& stamps,
              std::set<std::uint64_t>* addrs = nullptr) {
        for (const auto& s : stamps) {
            if (s.rejected)
                continue;
            if (addrs != nullptr)
                addrs->insert(s.addr);
            if (s.is_write)
                detector.write(s.addr, s.data);
            else
                detector.read(s.addr, s.data);
        }
    }

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

    // ── 内存不变量:把本 run 的**真实内存操作流**逐条送入检测器(per ADR-CEP-06 IC5)──
    //   ⚠️ 口径(⛔ 必须随任何引用重复):本装置**无 sharer 语义**(无 directory /
    //      sharer tracker)⇒ **SWMR 违例结构上不可达**;且 `TrafficGenTLM` 的
    //      `req.data` **恒 0** + home 回程只回显 ⇒ **无版本语义** ⇒ data-value 检查
    //      **无判别力** ⇒ 本 run 的 0 是「**本装置无该形态**」的 0,
    //      ⛔ 不得表述为「已证明协议层无违例」,⛔ 不得称「检测力已证」。
    //   ⚠️ 接上产出者才允许删 `not_collected` 的登记(per Entry #222 ④ 的反面要求)。
    cep_exp::SwmrDataValueDetector detector;
    std::set<std::uint64_t> memory_ops_addrs;
    feed(detector, w->txn_stamps(), &memory_ops_addrs); // ⛔ 单实现源(见文件内 feed())
    REQUIRE_FALSE(memory_ops_addrs.empty());            // 样本活性见证:⛔ 不得以零样本充数
    REQUIRE(detector.counts().swmr == 0u);
    REQUIRE(detector.counts().data_value == 0u);

    // ── 组装 13 键(实测 **或** `not-collected` + reason)──
    cep_exp::ExperimentMetrics m;
    m.total_cycles = kRunCycles;
    m.total_transactions = w->counters().cpu_requests;
    m.coherence_stalls = coherence_stall_cycles;
    m.deadlock_detected = watchdog.stalled() ? 1 : 0;
    m.violations = detector.counts();
    m.observation_keys["memory_ops_checked_addrs"] =
        std::vector<std::uint64_t>(memory_ops_addrs.begin(), memory_ops_addrs.end());
    m.verification_mode = "sampled";
    m.verification_scope = "CEP E2E 装置 · 拓扑 A(configs/cep_chi_rnf_e2e_a.json) · 300 cycles";

    // ── 延迟族:由 wrapper 的 **per-txn 周期戳**实测(issue → complete)──
    //   ⚠️ 只喂**网络往返**样本(`hit == false && !rejected && completed`):
    //      命中 / 拒绝路径在 issue **当期**本地完成(`record_stamp` 置 completed 且
    //      complete_cycle == issue_cycle)⇒ 它们的「延迟」恒 0 ⇒ 计入会污染分位。
    std::size_t completed_pairs = 0;
    for (const auto& s : w->txn_stamps()) {
        if (s.hit || s.rejected || !s.completed)
            continue;
        m.latency.note_issue();
        if (m.latency.note_complete(s.issue_cycle, s.complete_cycle))
            ++completed_pairs;
    }
    REQUIRE(completed_pairs > 0u); // 回程可达 ⇒ 必有完成往返 ⇒ 延迟族非空真
    REQUIRE(m.latency.sample_count() == completed_pairs);
    REQUIRE(m.latency.audit_balanced()); // 一发一收闭合
    REQUIRE(m.latency.underflow() == 0u);
    REQUIRE(m.latency.p50() > 0u); // ⛔ 非 0 值空真

    // 实测 5 键
    m.measured.insert("total_cycles");
    m.measured.insert("total_transactions");
    m.measured.insert("transactions_per_cycle");
    m.measured.insert("coherence_stalls");
    m.measured.insert("deadlock_violations");
    m.measured.insert("memory_violations"); // 违例类两键**均实测**(检测器已接上产出者)
    // 延迟族 5 键 = **实测**(回程可达 ⇒ 有完成往返)
    for (const char* k : {"avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
                          "p99_latency_cycles", "max_latency_cycles"})
        m.measured.insert(k);

    // 未采集 2 键(逐键 reason;⛔ 不填 0)
    m.not_collected["collective_stalls"] = "本装置无 collective plane 绑定 ⇒ 无产出者";
    m.not_collected["directory_misses"] = "本装置无 directory / sharer tracker ⇒ 无产出者";
    // ⚠️ `memory_violations` 已由上面的检测器**接上产出者** ⇒ 从 `not_collected` **移除**
    //    (⛔ 不得只删登记而不接产出者 —— 那会输出裸 0 假绿,per Entry #222 ④)

    // 13 键各有结论 + 装置自洽
    std::string missing;
    REQUIRE(cep_exp::metrics_13_covered(m, &missing));
    REQUIRE(missing.empty());
    REQUIRE(m.well_formed());
    // IC3 = ✓ 通过(两违例类**均实测**为 0)⇒ 三态中「通过」态该档位的实例
    REQUIRE(cep_exp::ic3_verdict(m) == cep_exp::Ic3Verdict::Pass);

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
        os << "  \"hypothesis_id\": \"h3\",\n";
        os << "  \"topology_tag\": \"A\",\n";
        // ⚠️ `run_class` / `not_an_experiment_run` 两字段**已移除** —— 本 run 现为
        //    **拓扑级实验 run**(与 h2/h3/h4/h6 的 simulation_config 同形:无该两字段)。
        //    `hypothesis_id = "h3"` 是**预登记绑定**(per `ADR-CEP-19` ASM-8 ①:
        //    拓扑级 run 须绑定 H3 —— 唯一双违例类均须实测的已登记行);
        //    ⚠️ 与调用选择器 `CEP_EXP_ID`(仍为 `framework-e2e`)是**两回事**:
        //    后者只决定「本次测试调用是否产出」,前者决定「产物绑定哪条预登记行」。
        os << "  \"evidence_scope\": "
           << "\"拓扑级 E2E run · 运行期 4 件套齐备 + 入库 2 件(retention 丙);"
              "⛔ 非协议级一致性断言(本装置无 sharer 语义 ⇒ SWMR 结构上不可达;"
              "写数据恒 0 ⇒ 无版本语义 ⇒ data-value 无判别力);"
              "⛔ 不得称检测力已证\",\n";
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

// ─────────────────────────────────────────────────────────────────────────────
// 喂料映射的 **data-value 可达性**(正负配对 · synthetic fixture)
//
// 谁需要它(per `ADR-CEP-20` 决策 6 / change `cep-evidence-tier-declaration`):
//   存量负控(`test/test_cep_experiment_harness.cc:87-130` 等 5 处)**不经过**本文件的
//   `feed()` 喂料映射 ⇒ 「映射能把 data-value 形态**送达**检测器」这一环**无判据**。
//   本用例补的正是这一环:⛔ 不是「再写一遍负控」(那是同义反复)。
//
// ⚠️ **本用例不设 `CEP_EXP_DIR`** ⇒ 无任何产物副作用(与 E2E-01 完全解耦)。
// ⚠️ 打 `[synthetic-fixture]` 显式标注:输入是**合成**的事务戳(⛔ 非真实 run)。
// ⛔ **不**证明「装置会产生违例形态」—— 真实流 `req.data` 恒 0(`traffic_gen_tlm.hh:233`)。
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("[cep][exp][evidence-tier] 喂料映射的 data-value 可达性(正负配对 · synthetic fixture)",
          "[cep][exp][evidence-tier][synthetic-fixture]") {
    using Stamp = tlm::cep::StubCacheEndpointTLM::TxnStamp;

    auto mk = [](std::uint64_t txn, bool is_write, std::uint64_t data) {
        Stamp s;
        s.txn_id = txn;
        s.addr = 0x1000;
        s.is_write = is_write;
        s.data = data;
        return s;
    };
    // 同一地址两次写(版本 1 → 2),随后一次读 —— 观测值由用例**唯一决定**
    auto base = [&] { return std::vector<Stamp>{mk(1, true, 1), mk(2, true, 2), mk(3, false, 0)}; };

    SECTION("负控:观测到**旧**版本 ⇒ data-value 违例 = 1") {
        auto stamps = base(); // 读的 data = 0 ≠ 最后写入的 2
        cep_exp::SwmrDataValueDetector det;
        feed(det, stamps);
        REQUIRE(det.counts().data_value == 1u);
    }
    SECTION("正控:**同一 vector**,仅把观测值改为最新 ⇒ 违例 = 0") {
        auto stamps = base();
        stamps.back().data = 2; // ⭐ 唯一差异
        cep_exp::SwmrDataValueDetector det;
        feed(det, stamps);
        REQUIRE(det.counts().data_value == 0u);
    }
    SECTION("结构事实锁:喂料通路对 SWMR **无通道** ⇒ 重复写恒 0(⛔ 非「已见证」)") {
        std::vector<Stamp> stamps;
        for (std::uint64_t i = 0; i < 10; ++i)
            stamps.push_back(mk(i + 1, true, i + 1)); // 同地址连续 10 次写
        cep_exp::SwmrDataValueDetector det;
        feed(det, stamps);
        // ⚠️ 本断言锁的是**事实**:`feed()` 只调合法写 `write()`,而 `write()` 不累加 `swmr`
        //    ⇒ 即便 10 次重复写也恒 0 ⇒ SWMR 违例在**喂料通路结构上不可达**
        //    (⛔ 故本 change **不**产出 SWMR 检测力见证,per ADR-CEP-20 决策 3c)。
        REQUIRE(det.counts().swmr == 0u);
    }
}
