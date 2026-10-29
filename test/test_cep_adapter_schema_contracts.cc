// test/test_cep_adapter_schema_contracts.cc
// 队列 #6 `cpptlm-cep-moesi-gpu-adapter` —— adapter 侧跨实例复用契约的判据测试
// per openspec/changes/cpptlm-cep-moesi-gpu-adapter/specs/cep-adapter-schema-contracts/spec.md
//     Requirement: cep-adapter-schema-contracts(AC-01 ~ AC-05 + ⑥ 收窄 + 单一实现)
// 作者: CppTLM Team / 日期: 2026-10-27
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "tlm/cep/adapter_contracts_loader.hh"
#include "tlm/cep/adapter_schema_contracts.hh"

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    using tlm::cep::AdapterContractVerdict;
    using tlm::cep::AdapterContractVerdictLine;
    using tlm::cep::AdapterSchemaContracts;
    using tlm::cep::MessageClassMapDecl;
    using tlm::cep::PermUpgradesDecl;
    using tlm::cep::PermValueDomain;
    using tlm::cep::ProtocolInstanceDecl;
    using tlm::cep::ProtocolInstancesDecl;
    using tlm::cep::SilentUpgradeDecl;

    PermValueDomain good_domain() {
        PermValueDomain d;
        d.values = {"NONE", "READ", "UNIQUE", "WRITEBACK"};
        return d;
    }

    PermUpgradesDecl good_upgrades() {
        PermUpgradesDecl u;
        u.silent_upgrades.push_back(SilentUpgradeDecl{"moesi_gpu", "UNIQUE", "WRITEBACK"});
        return u;
    }

    ProtocolInstancesDecl good_instances() {
        ProtocolInstancesDecl i;
        ProtocolInstanceDecl a;
        a.id = "moesi_gpu";
        a.perm = {"NONE", "READ", "UNIQUE", "WRITEBACK"};
        i.instances.push_back(a);
        ProtocolInstanceDecl b;
        b.id = "mesif";
        b.perm = {"NONE", "READ", "UNIQUE", "WRITEBACK"};
        i.instances.push_back(b);
        return i;
    }

    MessageClassMapDecl good_map() {
        MessageClassMapDecl m;
        m.rows = {{"request", 0},    {"response", 1}, {"snoop", 0},
                  {"snoop_resp", 1}, {"atomic", 0},   {"query", 0}};
        m.used_message_classes = {"request", "response", "snoop", "snoop_resp", "atomic", "query"};
        m.is_identity = true;
        m.identity_reason = "测试用恒等理由";
        return m;
    }

    std::vector<std::pair<int, bool>> good_stable_states() {
        return {{0, true}, {1, true}, {2, true}, {3, true}, {4, true}};
    }

    /// 离线 CLI 的路径(`build/bin/cpptlm_tests` → `build/bin/examples/cep_adapter_contracts`)
    std::filesystem::path cli_path() {
        std::error_code ec;
        const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
        const auto dir = self.parent_path();
        return dir / "examples" / "cep_adapter_contracts";
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════
// AC-01:`perm` 4 值锁不破(含跨 schema 值域绑定)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP adapter 契约: AC-01 perm 4 值锁不破 + 2 条反向 fixture",
          "[cep][adapter-contracts][ac-01]") {
    // 正控
    const auto line =
        AdapterSchemaContracts::check_perm_four_value_lock(good_domain(), good_upgrades());
    REQUIRE(line.rule_id == std::string("AC-01"));
    REQUIRE(line.verdict == AdapterContractVerdict::PASS);

    // 反向 fixture ①:值域里塞入 ATOMIC(4 值锁被击穿)⇒ 红
    {
        auto d = good_domain();
        d.values = {"NONE", "READ", "UNIQUE", "WRITEBACK", "ATOMIC"};
        const auto l = AdapterSchemaContracts::check_perm_four_value_lock(d, good_upgrades());
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ②:声明的 from 取域外值 ⇒ 红
    {
        auto u = good_upgrades();
        u.silent_upgrades[0].from = "ATOMIC";
        const auto l = AdapterSchemaContracts::check_perm_four_value_lock(good_domain(), u);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ③:契约侧秩序与 schema 枚举顺序不一致 ⇒ 红
    {
        auto d = good_domain();
        d.values = {"NONE", "UNIQUE", "READ", "WRITEBACK"};
        const auto l = AdapterSchemaContracts::check_perm_four_value_lock(d, good_upgrades());
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ④:值域被扩成 3 值 ⇒ 红(4 值锁**不**因本 change 而松动)
    {
        auto d = good_domain();
        d.values = {"NONE", "READ", "UNIQUE"};
        const auto l = AdapterSchemaContracts::check_perm_four_value_lock(d, good_upgrades());
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
}

// ══════════════════════════════════════════════════════════════════════
// AC-02:`perm_upgrade` 引用完整性 + 权限单调
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP adapter 契约: AC-02 引用完整性 + 权限单调 + 3 条反向 fixture",
          "[cep][adapter-contracts][ac-02]") {
    const auto line =
        AdapterSchemaContracts::check_perm_upgrade_integrity(good_instances(), good_upgrades());
    REQUIRE(line.rule_id == std::string("AC-02"));
    REQUIRE(line.verdict == AdapterContractVerdict::PASS);

    // 反向 fixture ①:引用不存在的实例 id ⇒ 红
    {
        auto u = good_upgrades();
        u.silent_upgrades[0].instance_id = "no_such_instance";
        const auto l = AdapterSchemaContracts::check_perm_upgrade_integrity(good_instances(), u);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ②:实例的 perm 数组缺 `to` ⇒ 红
    {
        auto i = good_instances();
        i.instances[0].perm = {"NONE", "READ", "UNIQUE"}; // 缺 WRITEBACK
        const auto l = AdapterSchemaContracts::check_perm_upgrade_integrity(i, good_upgrades());
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ③:降权限(UNIQUE → READ)⇒ 红
    {
        auto u = good_upgrades();
        u.silent_upgrades[0].to = "READ";
        const auto l = AdapterSchemaContracts::check_perm_upgrade_integrity(good_instances(), u);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
}

// ══════════════════════════════════════════════════════════════════════
// AC-03:三态显式化 + 反向 fixture(声明源畸形 ⇒ FAIL,而非 SKIP)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP adapter 契约: AC-03 三态可区分 + 声明源畸形 ⇒ FAIL + mesif 注入 ⇒ 红",
          "[cep][adapter-contracts][ac-03]") {
    // 正控:有可检查对象 ⇒ PASS
    {
        const auto l = AdapterSchemaContracts::check_decl_source_sanity(good_upgrades(), {"mesif"});
        REQUIRE(l.verdict == AdapterContractVerdict::PASS);
    }
    // SKIP 与 PASS **可区分**:合法但为空 ⇒ SKIP(**不得**记为 PASS)
    {
        PermUpgradesDecl empty;
        const auto l = AdapterSchemaContracts::check_decl_source_sanity(empty, {"mesif"});
        REQUIRE(l.verdict == AdapterContractVerdict::SKIP);
    }
    // 声明源畸形 ⇒ FAIL(配置错误,**不是** SKIP)
    {
        PermUpgradesDecl broken;
        broken.source_error = true;
        broken.source_error_detail = "JSON 解析失败";
        const auto l = AdapterSchemaContracts::check_decl_source_sanity(broken, {"mesif"});
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture:注入 `mesif`(无静默升级能力)⇒ 红
    {
        auto u = good_upgrades();
        u.silent_upgrades.push_back(SilentUpgradeDecl{"mesif", "UNIQUE", "WRITEBACK"});
        const auto l = AdapterSchemaContracts::check_decl_source_sanity(u, {"mesif"});
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
}

// ══════════════════════════════════════════════════════════════════════
// AC-04:6→3 映射表完备、值域受限且非恒等
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP adapter 契约: AC-04 映射完备 + 值域受限 + 恒等须登记 + 3 条反向 fixture",
          "[cep][adapter-contracts][ac-04]") {
    const auto line = AdapterSchemaContracts::check_message_class_mapping(good_map());
    REQUIRE(line.rule_id == std::string("AC-04"));
    REQUIRE(line.verdict == AdapterContractVerdict::PASS);

    // 反向 fixture ①:删一行映射 ⇒ 红
    //   ⚠️ 只删**映射行**;该实例**仍在用**这个消息类(⇒ 「用到的类」不减)——
    //   若连「用到的类」一起删,计数仍自洽 ⇒ 判据合法地给绿(那不是缺陷)。
    {
        auto m = good_map();
        m.rows.pop_back();
        const auto l = AdapterSchemaContracts::check_message_class_mapping(m);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ②:目标写成第 4 类 ⇒ 红
    {
        auto m = good_map();
        m.rows[0].second = 3;
        m.is_identity = false;
        const auto l = AdapterSchemaContracts::check_message_class_mapping(m);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ③:全表恒等且**无** identity_reason ⇒ 红
    {
        auto m = good_map();
        m.is_identity = true;
        m.identity_reason.clear();
        const auto l = AdapterSchemaContracts::check_message_class_mapping(m);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ④:多一行(无多、无少)⇒ 红
    {
        auto m = good_map();
        m.rows.emplace_back("extra", 0);
        const auto l = AdapterSchemaContracts::check_message_class_mapping(m);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
}

// ══════════════════════════════════════════════════════════════════════
// AC-05:adapter 的投影与状态机自洽
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP adapter 契约: AC-05 投影与状态机自洽 + 2 条反向 fixture",
          "[cep][adapter-contracts][ac-05]") {
    const auto line =
        AdapterSchemaContracts::check_adapter_self_consistency(good_stable_states(), 3, 2, 3);
    REQUIRE(line.rule_id == std::string("AC-05"));
    REQUIRE(line.verdict == AdapterContractVerdict::PASS);

    // 反向 fixture ①:稳定态缺投影 ⇒ 红
    {
        auto states = good_stable_states();
        states[1].second = false;
        const auto l = AdapterSchemaContracts::check_adapter_self_consistency(states, 3, 2, 3);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ②:`E+Write` 的目标被改掉 ⇒ 红
    {
        const auto l =
            AdapterSchemaContracts::check_adapter_self_consistency(good_stable_states(), 2, 2, 3);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
    // 反向 fixture ③:转换后 perm 秩未升高 ⇒ 红
    {
        const auto l =
            AdapterSchemaContracts::check_adapter_self_consistency(good_stable_states(), 3, 3, 3);
        REQUIRE(l.verdict == AdapterContractVerdict::FAIL);
    }
}

// ══════════════════════════════════════════════════════════════════════
// 判据实现单一 + 声明源被消费 + CLI 与测试 verdict 逐条相同
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP adapter 契约: 判据实现单一 + 声明源被消费 + CLI 与测试 verdict 逐条相同",
          "[cep][adapter-contracts][single-source]") {
    // ① 装载真实声明源(判据输入来自**真实文件**,不是手搓常量)
    const auto loaded = tlm::cep::load_adapter_contract_inputs(repo_root().string());
    REQUIRE(loaded.ok);
    REQUIRE(loaded.error.empty());

    // ② 声明源被**至少一个**测试消费(⇒ 不产生死文件)
    //    ⚠️ 队列 #8 后:`chi_rnf` 进入静默升级声明源(2 条)且进入实例声明集(3 个)——
    //    `chi_rnf` 是**本仓域实例**(权威 = 本仓,per `ADR-CEP-12 §11`)。
    REQUIRE(loaded.inputs.upgrades.silent_upgrades.size() == 2);
    REQUIRE(loaded.inputs.upgrades.silent_upgrades[0].instance_id == "moesi_gpu");
    REQUIRE(loaded.inputs.upgrades.silent_upgrades[1].instance_id == "chi_rnf");
    REQUIRE(loaded.inputs.instances.instances.size() == 3);
    REQUIRE(loaded.inputs.perm_domain.values.size() == 4);
    //    `mesif` 无静默升级能力 ⇒ 不出现在声明源里(AC-03 的负控见证)
    REQUIRE(loaded.inputs.no_upgrade_instances.size() == 1);
    REQUIRE(loaded.inputs.no_upgrade_instances[0] == "mesif");

    // ③ 真实数据下 5 条判据全绿 —— **逐 adapter 各跑一次**
    //    (AC-04 / AC-05 是**逐 adapter**的判据,per `adapter_contracts_loader.hh` 的两层结构)
    {
        auto inputs = loaded.inputs;
        tlm::cep::fill_adapter_inputs<tlm::cep::MoesiGpuAdapter>(inputs);
        const auto report = AdapterSchemaContracts::check_all(inputs);
        REQUIRE(report.verdicts.size() == 5);
        REQUIRE_FALSE(report.failed());
        REQUIRE(report.verdict == AdapterContractVerdict::PASS);
    }
    {
        auto inputs = loaded.inputs;
        tlm::cep::fill_adapter_inputs<tlm::cep::ChiRnfAdapter>(inputs);
        const auto report = AdapterSchemaContracts::check_all(inputs);
        REQUIRE(report.verdicts.size() == 5);
        REQUIRE_FALSE(report.failed());
        REQUIRE(report.verdict == AdapterContractVerdict::PASS);
    }

    // ④ verdict 逐条可 grep(格式 `[verdict] <rule-id> PASS|FAIL|SKIP (detail)`)
    //    —— **两个 adapter 的 verdict 都要满足**(逐 adapter 各一份)
    std::vector<tlm::cep::AdapterContractReport> reports;
    {
        auto inputs = loaded.inputs;
        tlm::cep::fill_adapter_inputs<tlm::cep::MoesiGpuAdapter>(inputs);
        reports.push_back(AdapterSchemaContracts::check_all(inputs));
    }
    {
        auto inputs = loaded.inputs;
        tlm::cep::fill_adapter_inputs<tlm::cep::ChiRnfAdapter>(inputs);
        reports.push_back(AdapterSchemaContracts::check_all(inputs));
    }
    for (const auto& rep : reports) {
        for (const auto& v : rep.verdicts) {
            const std::string line = v.render();
            REQUIRE(line.rfind("[verdict] ", 0) == 0);
            REQUIRE(line.find(v.rule_id) != std::string::npos);
            REQUIRE(line.find("(") != std::string::npos);
            REQUIRE(line.back() == ')');
        }
    }

    // ⑤ **CLI 与测试的 verdict 输出逐条相同**(同一实现 ⇒ 同一结论)
    //    —— 直接跑离线 CLI 并逐行比对。
    const auto cli = cli_path();
    REQUIRE(std::filesystem::exists(cli));
    const std::string cmd = cli.string() + " " + repo_root().string() + " 2>/dev/null";
    std::string cli_out;
    {
        std::FILE* pipe = popen(cmd.c_str(), "r");
        REQUIRE(pipe != nullptr);
        char buf[512];
        while (std::fgets(buf, sizeof(buf), pipe) != nullptr)
            cli_out += buf;
        const int rc = pclose(pipe);
        REQUIRE(rc == 0); // 0 = 全部 PASS/SKIP
    }
    const auto lines = [&]() {
        std::vector<std::string> out;
        std::string cur;
        for (char ch : cli_out) {
            if (ch == '\n') {
                out.push_back(cur);
                cur.clear();
            } else
                cur += ch;
        }
        if (!cur.empty())
            out.push_back(cur);
        return out;
    }();
    // ⚠️ **CLI 现在逐 adapter 各跑一次** ⇒ 测试侧同样按 **moesi → chi** 的顺序
    //    拼接两个 report 的 verdict 行,再与 CLI 输出**逐行**比对。
    const auto mine = [&]() {
        std::vector<std::string> out;
        for (const auto& rep : reports) {
            for (const auto& v : rep.verdicts)
                out.push_back(v.render());
            out.push_back(std::string("[verdict] SUMMARY ") +
                          tlm::cep::adapter_contract_verdict_name(rep.verdict) + " (5 rule(s))");
        }
        return out;
    }();
    REQUIRE(lines.size() == mine.size() + 1); // 末行是 CLI 自己的汇总行
    for (std::size_t i = 0; i < mine.size(); ++i) {
        INFO("CLI 第 " << i << " 行: " << lines[i]);
        REQUIRE(lines[i] == mine[i]);
    }
}
