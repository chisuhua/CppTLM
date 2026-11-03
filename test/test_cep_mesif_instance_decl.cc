// test/test_cep_mesif_instance_decl.cc
// 队列 #9 `cpptlm-cep-mesif-adapter` —— Phase 3 平台级声明 + 装配实测
// per openspec/changes/cpptlm-cep-mesif-adapter/specs/cep-mesif-adapter/spec.md
//     Requirement: cep-mesif-adapter(MA-04 / MA-05 / MA-08)
// 作者: CppTLM Team / 日期: 2026-10-30
//
// ── 判据口径 ────────────────────────────────────────────────────────────
// · MA-04 的「双向规则」判据在 `test_cep_adapter_schema_contracts.cc`(合成输入反控)——
//   本文件只钉**真实数据**侧的取值(3 条声明 / 派生量为空),不重复实现规则;
// · MA-05 的「12 个非 status 键与 ArchForge 逐字段全等」由跨仓镜像脚本
//   (`scripts/test/cep_archforge_alignment_check.sh`)承担;本文件钉 schema `required`
//   的**机器驱动**完备性 + 「豁免严格限于 `status` 一键」这个**结构**事实;
// · MA-08 的 `INV-CEP-12-IC2` 三条件由 `scripts/test/cep_decl_alignment_check.sh`
//   机检;本文件只做**非空转前置**(证明这份声明确实踩到了那三个条件)+ 通道依赖闸。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "tlm/cep/adapter_contracts_loader.hh"
#include "tlm/cep/channel_dependency_graph.hh"

namespace {

    using json = nlohmann::json;

    json load_cep_json(const char* name) {
        return json::parse(cpptlm::cep::read_file(
            (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / name).string()));
    }

    const json& instances_doc() {
        static const json d = load_cep_json("protocol_instances.json");
        return d;
    }

    const json& mesif() {
        static const json m = [&]() {
            for (const auto& i : instances_doc().at("instances")) {
                if (i.value("id", "") == "mesif")
                    return i;
            }
            throw std::runtime_error("protocol_instances.json 缺少 mesif");
        }();
        return m;
    }

    const json& upgrades_doc() {
        static const json d = load_cep_json("perm_upgrades.json");
        return d;
    }

    const json& fabric_doc() {
        static const json d = load_cep_json("fabric.json");
        return d;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// MA-04 —— 静默升级声明落地 + `current_state` 的事实性错误已修订
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 声明:静默升级行落地且 current_state 不再否认该能力", "[cep][mesif-decl]") {
    // ① `mesif` 行存在,且是 `UNIQUE → WRITEBACK`(对应 `E + Wr → M`)
    const auto& rows = upgrades_doc().at("perm_upgrade").at("silent_upgrades");
    std::size_t hits = 0;
    for (const auto& r : rows) {
        if (r.at("instance_id") != "mesif")
            continue;
        ++hits;
        CHECK(r.at("from") == "UNIQUE");
        CHECK(r.at("to") == "WRITEBACK");
    }
    CHECK(hits == 1u); // 恰好一条(不是零,也不是重复)

    // ② `current_state` 的**事实性错误**已修订:不得再声称「`mesif` 无该能力」
    const std::string cs = upgrades_doc().at("derived_from").at("current_state");
    CHECK(cs.find("mesif") != std::string::npos);
    CHECK(cs.find("无该能力") == std::string::npos); // 旧句的核心断言
    // ⚠️ 2026-11-01(队列 #10 实现层):新增 `cxl_2_0` ⇒ 计数 **+1**(per 台账 Entry #131)
    CHECK(cs.find("4 个实例全部") != std::string::npos); // 新句的核心断言
    CHECK(cs.find("双向规则") != std::string::npos);     // 负控空转陷阱已登记

    // ③ 真实数据侧:3 个实例全部有声明 ⇒ 派生量为空(正控)
    //    ⚠️ 「为空」本身**不构成**规则被验证(见 adapter-contracts 的合成输入用例)
    const auto loaded = tlm::cep::load_adapter_contract_inputs(cpptlm::cep::repo_root().string());
    REQUIRE(loaded.ok);
    // ⚠️ 2026-11-01(队列 #10 实现层):新增 `cxl_2_0` ⇒ 计数 **+1**(per 台账 Entry #131)
    CHECK(loaded.inputs.upgrades.silent_upgrades.size() == 4u);
    // ⚠️ 2026-11-02(队列 #11 实现层):`tilelink_c` **不声明**静默升级(无 `UNIQUE`)
    //    ⇒ 派生集**首次非空** ⇒ 断言由「为空」改为「**恰含** `tilelink_c`」
    //    (per 台账 Entry #146;⇒ 派生量从「恒空」转为「有判别力」)
    // ⚠️ 2026-11-02(队列 #12 实现层):新增 `cxl_3_0_symmetric` ⇒ 计数 **+1**
    //    (本族 E→M **未确证** ⇒ **不声明**静默升级 ⇒ 派生集**恰含 2 个**)
    // ⚠️ 2026-11-03(队列 #13 **实现层**):新增 `nvlink_4`(本仓自持 · BroadcastNull · 无静默升级行)⇒
    // 计数 **+1**(per 台账 Entry #187;设计 §9.3 计数同步清单)
    REQUIRE(loaded.inputs.no_upgrade_instances.size() == 3u);
    //    ⇒ 顺序 = `instances[]` 数组序(派生函数按数组序遍历)
    REQUIRE(loaded.inputs.no_upgrade_instances.at(0) == "cxl_3_0_symmetric");
    REQUIRE(loaded.inputs.no_upgrade_instances.at(1) == "tilelink_c");
    REQUIRE(loaded.inputs.no_upgrade_instances.at(2) == "nvlink_4");

    // ④ 反向 fixture:`mesif` 行的 `to` 一旦被改成 `READ` ⇒ 上面的 `== WRITEBACK` 必红
    //    (此处用可变异副本证明判据有鉴别力,不改真实文件)
    json mutated = rows;
    for (auto& r : mutated) {
        if (r.at("instance_id") == "mesif")
            r["to"] = "READ";
    }
    bool mutated_ok = true;
    for (const auto& r : mutated) {
        if (r.at("instance_id") == "mesif" && r.at("to") != "WRITEBACK")
            mutated_ok = false;
    }
    CHECK_FALSE(mutated_ok);
}

// ─────────────────────────────────────────────────────────────────────────
// MA-05 —— 13 个 required 顶层属性齐备 + 关键字段 + 「豁免严格限于 status」
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 声明:13 个 required 属性齐备(由 schema 机器驱动)", "[cep][mesif-decl]") {
    const json& schema = load_cep_json("protocol_instance.schema.json");
    const auto& required = schema.at("required");
    REQUIRE(required.is_array());
    REQUIRE(required.size() == 13u); // 冻结 schema 的 13 键(数量变了 ⇒ 本判据先红)

    // ① 逐键存在(键名来自 **schema**,不是手搓列表 ⇒ schema 增键时本判据自动跟上)
    for (const auto& k : required) {
        const std::string key = k.get<std::string>();
        INFO("required key: " << key);
        CHECK(mesif().count(key) == 1u);
    }

    // ② 关键字段取值(拼接错误会在装配期炸,这里提前钉住)
    CHECK(mesif().at("family") == "industrial");
    CHECK(mesif().at("adapter") == "MesifAdapter");
    CHECK(mesif().at("protocol_version") == "mesif-1.0");
    CHECK(mesif().at("perm").size() == 4u); // 4 值锁不破(CEN-Ib11)
    CHECK(mesif().at("sharer_tracker").at("impl") == "HierarchicalDirectory");
    CHECK(mesif().at("atomic").at("supported") == true);

    // ③ 「`status` 豁免**严格限于** `status` 一键」—— 结构事实:
    //    非 `status` 键**恰好 12 个**,少一个/多一个都会让「12 键逐字段镜像」失效
    std::size_t non_status = 0;
    for (const auto& kv : mesif().items()) {
        if (kv.key() != "status")
            ++non_status;
    }
    CHECK(non_status == 12u);

    // ④ `status` 取值域(Phase 5 会把它收紧为 `implemented`,本 Phase 只允许两值)
    const std::string st = mesif().at("status");
    INFO("status = " << st);
    CHECK((st == "planned" || st == "implemented"));

    // ⑤ 反向 fixture:schema `required` 里任一键被摘掉 ⇒ 上面的完备性判据必红
    json mutated = mesif();
    mutated.erase(required[3].get<std::string>());
    std::size_t missing = 0;
    for (const auto& k : required) {
        if (mutated.count(k.get<std::string>()) != 1u)
            ++missing;
    }
    CHECK(missing == 1u); // 有鉴别力(不是「怎么改都绿」)
}

// ─────────────────────────────────────────────────────────────────────────
// MA-08 ① —— `INV-CEP-12-IC2` 的**非空转前置**(证明声明真的踩到了三条件)
// ─────────────────────────────────────────────────────────────────────────
// ⚠️ 判定的**权威实现**是 `scripts/test/cep_decl_alignment_check.sh`(python,逐网络);
//    本用例只证明「这份声明**不是**空跑」—— 若某天声明被删成空对象,
//    那条脚本会因为「无可检查对象」而变得没有鉴别力,本用例会先红。
TEST_CASE("CEP MESIF 声明:INV-CEP-12-IC2 的输入非空转(两网络 + VC 上界 + 6 类词表)",
          "[cep][mesif-decl][channel-gate]") {
    // 网络 → vc_count(来自 frozen `fabric.json`,不是手搓)
    std::map<std::string, unsigned> vc_count;
    for (const auto& n : fabric_doc().at("networks").items()) {
        vc_count[n.key()] = n.value().at("vc_count").get<unsigned>();
    }
    REQUIRE(vc_count.count("compute_noc") == 1u);
    REQUIRE(vc_count.count("ualink_fabric") == 1u);

    const json& vb = mesif().at("vc_binding");
    const json& oa = mesif().at("ordering_assumption");

    // 条件 ② 的输入:`vc_binding[n]` 的每个 vc 都**有上界可判**(声明确实覆盖两个网络)
    CHECK(vb.size() == 2u);
    std::size_t checked_bindings = 0;
    for (const auto& net : vb.items()) {
        REQUIRE(vc_count.count(net.key()) == 1u);
        for (const auto& mc : net.value().items()) {
            const unsigned vc = mc.value().get<unsigned>();
            INFO("net=" << net.key() << " class=" << mc.key() << " vc=" << vc);
            CHECK(vc < vc_count.at(net.key())); // 槽位上界
            ++checked_bindings;
        }
    }
    CHECK(checked_bindings == 9u); // compute_noc 4 + ualink_fabric 5

    // 条件 ③ 的输入:消息类 ∈ 6 类规范词表
    const std::set<std::string> vocab = {"request",    "response", "snoop",
                                         "snoop_resp", "atomic",   "query"};
    for (const auto& net : vb.items()) {
        for (const auto& mc : net.value().items()) {
            INFO("class=" << mc.key());
            CHECK(vocab.count(mc.key()) == 1u);
        }
    }

    // 条件 ① 的输入:`ordering_assumption[n]` 的 vc 必须能在 `vc_binding[n].values()` 找到
    CHECK(oa.size() == 2u);
    for (const auto& net : oa.items()) {
        REQUIRE(vb.count(net.key()) == 1u);
        std::set<unsigned> bound;
        for (const auto& mc : vb.at(net.key()).items())
            bound.insert(mc.value().get<unsigned>());
        for (const auto& slot : net.value().items()) {
            // 键是 VC 号(字符串),值是序属性
            const unsigned vc = static_cast<unsigned>(std::stoul(slot.key()));
            INFO("net=" << net.key() << " ordering vc=" << vc);
            CHECK(bound.count(vc) == 1u); // 每个被排序的 VC 都被绑定过
            CHECK(slot.value().is_string());
        }
    }

    // 反向 fixture:把 `atomic` 的绑定 VC 改成越界值 ⇒ 条件 ② 的断言必红
    json poisoned = vb;
    poisoned["ualink_fabric"]["atomic"] = 9u;
    CHECK(poisoned.at("ualink_fabric").at("atomic").get<unsigned>() >=
          vc_count.at("ualink_fabric"));
}

// ─────────────────────────────────────────────────────────────────────────
// MA-08 ②③ —— 3 实例共存时通道依赖闸无环 + 逐 `(instance, network, vc)` 标注
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 装配:3 实例共存时通道依赖闸无环且逐 (net, vc) 标注",
          "[cep][mesif-decl][json-assembly][channel-gate]") {
    json cfg = json::parse(cpptlm::cep::read_file(
        (cpptlm::cep::repo_root() / "configs" / "cep_mesif_pe_minimal.json").string()));
    cfg["cep"]["protocol_instances"] =
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();

    const auto gate = tlm::cep::run_channel_gate(cfg);
    REQUIRE(gate.verdict == tlm::cep::GateVerdict::PASS); // 无环

    // 逐 `(instance, network, vc)` 标注:mesif 的每条 verdict 都要 PASS 且网络在声明集内
    std::set<std::string> networks;
    std::size_t mesif_verdicts = 0;
    for (const auto& v : gate.verdicts) {
        if (v.instance != "mesif")
            continue;
        ++mesif_verdicts;
        INFO("net=" << v.network << " vc=" << v.vc);
        CHECK(v.verdict == tlm::cep::GateVerdict::PASS);
        networks.insert(v.network);
    }
    // 闸按 `(network, vc)` **逐格**出 verdict:两个网络各 4 个 VC(0..3)⇒ 8 条
    // (不是「每条 vc_binding 一行」—— `response` 与 `snoop_resp` 在 ualink_fabric 同为 VC3)
    CHECK(mesif_verdicts == 8u);
    CHECK(networks == std::set<std::string>({"compute_noc", "ualink_fabric"}));

    // 3 实例**共存**(不是「只看见 mesif 就算过」)
    CHECK(gate.per_instance.count("moesi_gpu") == 1u);
    CHECK(gate.per_instance.count("mesif") == 1u);
    CHECK(gate.per_instance.count("chi_rnf") == 1u);

    // 日志可 grep(判据可复核)
    const std::string log = gate.to_log();
    CHECK(log.find("[verdict] net=ualink_fabric vc=1 PASS (instance=mesif") != std::string::npos);
    CHECK(log.find("[per-instance] instance=mesif ") != std::string::npos);

    // ── 反向 fixture:破坏 `mesif` 的 vc_binding(VC 越界)⇒ 闸必 FAIL ─────────
    json bad_decl = instances_doc();
    for (auto& i : bad_decl["instances"]) {
        if (i.value("id", "") == "mesif")
            i["vc_binding"]["compute_noc"]["request"] = 9; // 越界
    }
    const auto tmp = std::filesystem::temp_directory_path() / "cep_mesif_bad_instances.json";
    {
        std::ofstream out(tmp);
        out << bad_decl.dump(2);
    }
    json cfg_bad = cfg;
    cfg_bad["cep"]["protocol_instances"] = tmp.string();
    const auto gate_bad = tlm::cep::run_channel_gate(cfg_bad);
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    CHECK(gate_bad.verdict == tlm::cep::GateVerdict::FAIL);
}
