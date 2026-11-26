// test/test_cep_ssp_experiment_emit.cc
// SSP **档 0** 实验产出（批 2 `cep-ssp-batch2` · ⑩）—— 4 件套的 **C++ 侧 2 件**
//   （`metrics.json` + `simulation_config.json`；`trace.h5` + `report.html` 由脚本侧补）
//
// ⚠️ **档 0 诚实边界**（per `ADR-CEP-24` 决策 2.3 / `ADR-CEP-20` 决策 6）:
//   · 本 run 只覆盖「**形态 + 引用闭环**」⇒ 全部 **13 项必纳 metric 均不采**
//     ⇒ 一律写 `"not-collected"` + `_not_collected` per-metric 原因
//     （⛔ 禁写裸 0 冒充实测 · per `ADR-CEP-06` SU#1 §②）；
//   · ⛔ **零时序**（不含 hop / 消息级延迟 · 属 S2 `ADR-CEP-25`）
//     ⇒ `simulation_mode` = `functional`（⛔ **不是** `timing`）；
//   · ⛔ 违例类（`deadlock_violations` / `memory_violations`）未采集
//     ⇒ IC3 判定「**不适用**」⇒ 该 run **不作**一致性安全断言的证据。
//
// ⚠️ **洞命中**（per `ADR-CEP-24` 决策 6.1）⇒ 拒绝产出（⛔ 不留半套 4 件套）
//   且该次实验判「**不可判**」。
//
// 作者: CppTLM Team / 日期: 2026-11-27
#include "catch_amalgamated.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/ssp_interpreter.hh"

namespace {

namespace ssp = tlm::cep::ssp;

nlohmann::json load_ssps_doc() {
    const std::string path = std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/ssps.json";
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json j;
    in >> j;
    return j;
}

/// 13 必纳 metric（per `experimental/metrics.schema.json` 的 required 集）
const char* const kRequired13[13] = {
    "total_cycles", "total_transactions", "transactions_per_cycle",
    "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
    "collective_stalls", "directory_misses", "deadlock_violations",
    "memory_violations",
};

/// 档 0 的 scope 文本（⛔ 必须逐字携带「零时序 + 不承诺语义」的限定语）
std::string ssp_scope(const std::string& ssp_id, std::size_t steps) {
    return "SSP 档 0(形态 + 引用闭环)· 声明 " + ssp_id + " · " + std::to_string(steps) +
           " 步迁移 · ⛔ 零时序(不含 hop / 消息级延迟)· ⛔ 不承诺语义(活性 / 无死锁 / 一致性 / 兼容性)";
}

}  // namespace

TEST_CASE("SSP 档 0 实验产出 · 4 件套输入(仅 CEP_EXP_DIR 设置时)", "[cep][ssp][emit]") {
    const char* dir = std::getenv("CEP_EXP_DIR");
    if (dir == nullptr || *dir == '\0') {
        SUCCEED("未设置 CEP_EXP_DIR ⇒ 不产出文件");
        return;
    }
    const char* id_env = std::getenv("CEP_EXP_ID");
    const std::string exp_id =
        (id_env == nullptr || *id_env == '\0') ? std::string("ssp") : std::string(id_env);
    REQUIRE(exp_id == "ssp");

    // ── 跑 SSP 解释器（档 0：形态 + 引用闭环 · 零时序）────────────────────
    const auto doc = load_ssps_doc();
    // ⚠️ **反向 fixture 钩子**（per 既有 `CEP_EXP_VARIANT` 先例）:置 `CEP_SSP_HOLE=1`
    //   ⇒ 在声明面制造一个**洞** ⇒ 该次实验判「不可判」⇒ **拒绝产出**。
    const char* hole_env = std::getenv("CEP_SSP_HOLE");
    const bool inject_hole = (hole_env != nullptr && *hole_env == '1');
    if (inject_hole) {
        auto hole_doc = doc;
        hole_doc["ssps"][0]["transition"][0][0] = nullptr;  // I + Rd ⇒ 空洞
        const auto hr = ssp::interpret_ssp(hole_doc, "mesif_ssp", "I",
                                            std::vector<std::string>{"Rd"});
        REQUIRE(hr.ok);
        REQUIRE(hr.violation_count == 1u);
        REQUIRE_FALSE(hr.judgeable);
        // ⛔ 拒绝产出(⛔ 不留半套 4 件套)
        REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(dir) / "metrics.json"));
        REQUIRE_FALSE(
            std::filesystem::exists(std::filesystem::path(dir) / "simulation_config.json"));
        return;
    }

    const auto r = ssp::interpret_ssp(doc, "mesif_ssp", "I",
                                       std::vector<std::string>{"Rd", "RdUnique", "Wr", "Upgrade"});
    REQUIRE(r.ok);

    // ⚠️ **洞命中** ⇒ 该次实验判「不可判」⇒ **拒绝产出**（⛔ 不留半套 4 件套）
    if (!r.judgeable) {
        REQUIRE(r.violation_count > 0u);
        REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(dir) / "metrics.json"));
        REQUIRE_FALSE(
            std::filesystem::exists(std::filesystem::path(dir) / "simulation_config.json"));
        return;
    }
    REQUIRE(r.violation_count == 0u);
    REQUIRE(!r.steps.empty());

    const std::filesystem::path out(dir);
    std::filesystem::create_directories(out);
    const std::string scope = ssp_scope("mesif_ssp", r.steps.size());

    // ── metrics.json（13 必纳**全不采** + per-metric 原因 + 档位声明）────────
    {
        nlohmann::json m;
        nlohmann::json reasons = nlohmann::json::object();
        for (const char* k : kRequired13) {
            m[k] = "not-collected";
            reasons[k] = "SSP 档 0 只覆盖「形态 + 引用闭环」(per ADR-CEP-24 决策 2.3);"
                         "⛔ 零时序 ⇒ 本装置无该 metric 的采集链路(禁写裸 0 冒充实测)";
        }
        m["_measured"] = nlohmann::json::array();       // 本 run **零**实测项
        m["_not_collected"] = reasons;
        m["observation_keys"] = nlohmann::json::object();
        m["verification_mode"] = "sampled";
        m["verification_scope"] = scope;
        // ── SSP 特有观测（附加键 · `additionalProperties: true` ⇒ 不改 13 必纳语义）──
        m["ssp_id"] = "mesif_ssp";
        m["ssp_steps"] = r.steps.size();
        m["ssp_violations"] = r.violation_count;
        m["ssp_judgeable"] = r.judgeable;
        m["ssp_target_mask"] = r.masks;

        std::ofstream os(out / "metrics.json");
        REQUIRE(os.good());
        os << m.dump(2) << "\n";
    }

    // ── simulation_config.json（档位声明 + 预登记绑定）────────────────────
    {
        nlohmann::json cfg;
        // ⚠️ `functional`(**不是** `timing`)—— SSP 档 0 **零时序** ⇒ 无时序语义
        cfg["simulation_mode"] = "functional";
        cfg["verification_mode"] = "sampled";
        cfg["verification_scope"] = scope;
        // ⚠️ 预登记绑定:H9 = 「用户自定义协议(SSP)档 0 实验」(out_of_scope = 全部 13 项)
        cfg["hypothesis_id"] = "H9";
        std::ofstream os(out / "simulation_config.json");
        REQUIRE(os.good());
        os << cfg.dump(2) << "\n";
    }

    REQUIRE(std::filesystem::exists(out / "metrics.json"));
    REQUIRE(std::filesystem::exists(out / "simulation_config.json"));
}
