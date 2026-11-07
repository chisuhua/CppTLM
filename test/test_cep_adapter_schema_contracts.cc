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
#include "tlm/cep/tilelink_c_adapter.hh"

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
// AC-03 负控见证 —— `no_upgrade_instances` 的**双向规则**(合成输入反控)
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ 为什么单独成用例:队列 #9 之后**所有**实例都声明了静默升级 ⇒ 真实数据下该派生量
//    **恒为空**。若判据只断言「为空」,派生逻辑坏掉(永远返回空)也照样绿 —— 这就是
//    判据纪律第 4 条的「fixture 空转」。⇒ 用**合成输入**把规则的两个方向都钉住:
//      · 无声明 ⇒ **在**集合里
//      · 有声明 ⇒ **不在**集合里
//    并对判据本体(`check_decl_source_sanity`)补一条「越权实例 ⇒ FAIL」的反控。
TEST_CASE("CEP adapter 契约: no_upgrade_instances 双向规则(合成输入反控)",
          "[cep][adapter-contracts][single-source]") {
    using tlm::cep::PermUpgradesDecl;
    using tlm::cep::ProtocolInstanceDecl;
    using tlm::cep::SilentUpgradeDecl;

    auto make_instances = [](std::initializer_list<const char*> ids) {
        std::vector<ProtocolInstanceDecl> v;
        for (const char* id : ids) {
            ProtocolInstanceDecl d;
            d.id = id;
            v.push_back(d);
        }
        return v;
    };
    auto make_upgrades = [](std::initializer_list<const char*> ids) {
        PermUpgradesDecl u;
        for (const char* id : ids) {
            SilentUpgradeDecl d;
            d.instance_id = id;
            d.from = "UNIQUE";
            d.to = "WRITEBACK";
            u.silent_upgrades.push_back(d);
        }
        return u;
    };

    const auto instances = make_instances({"alpha", "beta", "gamma"});

    // 方向 ①:一个都没声明 ⇒ 3 个全**在**集合里(若派生逻辑坏成恒空 ⇒ 本段必红)
    {
        const auto derived = tlm::cep::derive_no_upgrade_instances(instances, make_upgrades({}));
        REQUIRE(derived.size() == 3);
        CHECK(derived[0] == "alpha");
        CHECK(derived[1] == "beta");
        CHECK(derived[2] == "gamma");
    }
    // 方向 ②:部分声明 ⇒ 只有未声明的**在**集合里
    {
        const auto derived =
            tlm::cep::derive_no_upgrade_instances(instances, make_upgrades({"beta"}));
        REQUIRE(derived.size() == 2);
        CHECK(derived[0] == "alpha");
        CHECK(derived[1] == "gamma");
    }
    // 方向 ③:全部声明 ⇒ 空(**这正是当前真实数据的取值**)
    {
        const auto derived = tlm::cep::derive_no_upgrade_instances(
            instances, make_upgrades({"alpha", "beta", "gamma"}));
        CHECK(derived.empty());
    }

    // 判据本体反控:被登记为「无该能力」的实例出现在声明源里 ⇒ **FAIL**
    {
        const auto line =
            AdapterSchemaContracts::check_decl_source_sanity(make_upgrades({"beta"}), {"beta"});
        CHECK(line.rule_id == "AC-03");
        REQUIRE(line.verdict == AdapterContractVerdict::FAIL);
        CHECK(line.detail.find("beta") != std::string::npos); // 归因到具体实例
    }
    // 正控:不冲突时 ⇒ PASS(不是「永远 FAIL」)
    {
        const auto line =
            AdapterSchemaContracts::check_decl_source_sanity(make_upgrades({"beta"}), {"alpha"});
        CHECK(line.verdict == AdapterContractVerdict::PASS);
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
    //    ⚠️ 队列 #9 后:`mesif` 也进入(3 条)—— 它的 `E + Wr → M` 静默升级在规约层落地后
    //    是**真实能力**,此前 `perm_upgrades.json` 的 `current_state` 自述「`mesif` 无该能力」
    //    是**事实性错误**(已随本 change 修订)。
    // ⚠️ 2026-11-01(队列 #10 实现层):`cxl_2_0` 的 `E + 写事件 → M` 静默升级登记 ⇒ 4 条
    //    (⚠️ 该格 basis = `public-analogical` + `pending` ⇒ 登记**不等于**「已确证」)
    REQUIRE(loaded.inputs.upgrades.silent_upgrades.size() == 4);
    REQUIRE(loaded.inputs.upgrades.silent_upgrades[0].instance_id == "moesi_gpu");
    REQUIRE(loaded.inputs.upgrades.silent_upgrades[1].instance_id == "chi_rnf");
    REQUIRE(loaded.inputs.upgrades.silent_upgrades[2].instance_id == "mesif");
    REQUIRE(loaded.inputs.upgrades.silent_upgrades[3].instance_id == "cxl_2_0");
    // ⚠️ 2026-11-01(队列 #10 实现层):新增 `cxl_2_0` ⇒ 计数 **+1**(per 台账 Entry #131)
    // ⚠️ 2026-11-02(队列 #11 实现层):新增 `tilelink_c` ⇒ 计数 **+1**(per 台账 Entry #146)
    // ⚠️ 2026-11-03(队列 #13 **实现层**):新增 `nvlink_4`(本仓自持 · BroadcastNull · 无静默升级行)⇒
    // 计数 **+1**(per 台账 Entry #187;设计 §9.3 计数同步清单)
    // ⚠️ 2026-11-02 队列 #12:+1 · ⚠️ 2026-11-03 队列 #13:+1 · ⚠️ 2026-11-06(Entry #239):+1(`ualink_4`)
    REQUIRE(loaded.inputs.instances.instances.size() == 8);
    REQUIRE(loaded.inputs.perm_domain.values.size() == 4);

    // ⚠️ **负控见证已改造为双向规则**(队列 #9 AC-5):
    //    若只断言「集合为空」,则「派生逻辑正确」与「派生逻辑坏成永远返回空」
    //    **不可分辨**(恒真空转)⇒ 规格规则改由下面的合成输入用例覆盖
    //    (见 `TEST_CASE("... no_upgrade_instances 的双向规则 ...")`)。
    // ⚠️ 2026-11-02(队列 #11 实现层):**该「恒为空」前提已失效** ——
    //    `tilelink_c` 的投影是 3→3、`UNIQUE` 不可达 ⇒ **不声明**静默升级
    //    ⇒ 派生量**首次非空** = **{`tilelink_c`}** ⇒ **首次具备判别力**
    //    (判据变强,非变弱)⇒ 断言由「为空」改为「**恰含** `tilelink_c`」。
    // ⚠️ 2026-11-02(队列 #12 实现层):新增 `cxl_3_0_symmetric` ⇒ 计数 **+1**
    //    (本族**不声明**静默升级 —— E→M 在本族**未确证** ⇒ 声明即**假声明**)
    //    ⇒ 派生集**恰含 2 个**(`tilelink_c` + `cxl_3_0_symmetric`)
    // ⚠️ 2026-11-03(队列 #13 **实现层**):新增 `nvlink_4`(本仓自持 · BroadcastNull ·
    //    无静默升级行)⇒ 计数 **+1**(per 台账 Entry #187;设计 §9.3 计数同步清单)
    // ⚠️ 2026-11-06(per 台账 Entry #239 §⑤-1):4 —— 新增 `ualink_4`(无静默升级 ⇒ 入派生集)
    REQUIRE(loaded.inputs.no_upgrade_instances.size() == 4u);
    //    ⇒ 顺序 = `instances[]` 数组序(派生函数按数组序遍历)
    REQUIRE(loaded.inputs.no_upgrade_instances.at(0) == "cxl_3_0_symmetric");
    REQUIRE(loaded.inputs.no_upgrade_instances.at(1) == "tilelink_c");
    REQUIRE(loaded.inputs.no_upgrade_instances.at(3) == "ualink_4"); // ⚠️ 2026-11-06:追加在末尾
    REQUIRE(loaded.inputs.no_upgrade_instances.at(2) == "nvlink_4");

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
    // ⚠️ 2026-11-02(队列 #11 实现层):CLI **新增** `TileLinkCAdapter` 段 ⇒ 报告数 2 → 3
    //    ⇒ 下方与 CLI 输出的**逐行**比对随之多一段 ⇒ TileLink 的 AC-05 **SKIP** 也被锁住
    {
        auto inputs = loaded.inputs;
        tlm::cep::fill_adapter_inputs<tlm::cep::TileLinkCAdapter>(inputs);
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
    // ⚠️ **CLI 现在逐 adapter 各跑一次** ⇒ 测试侧同样按 **moesi → chi → tilelink** 的顺序
    //    拼接三个 report 的 verdict 行,再与 CLI 输出**逐行**比对。
    //    ⚠️ 2026-11-02(队列 #11 实现层):CLI **新增** `TileLinkCAdapter` 段 ⇒ 报告数 2 → 3
    //    ⇒ **逐行**比对(不是只比行数)⇒ 新段的 verdict 内容也被逐行锁住。
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
