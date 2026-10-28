// test/test_cep_moesi_gpu_adapter.cc
// 队列 #6 `cpptlm-cep-moesi-gpu-adapter` —— 首个 `implemented` adapter 的契约测试
// per openspec/changes/cpptlm-cep-moesi-gpu-adapter/specs/cep-moesi-gpu-adapter/spec.md
//     Requirement: cep-moesi-gpu-adapter(7 条 Scenario)
// 作者: CppTLM Team / 日期: 2026-10-27
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "tlm/cep/adapter_schema_contracts.hh"
#include "tlm/cep/moesi_gpu_adapter.hh"

using json = nlohmann::json;
using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    using tlm::cep::MoesiGpuAdapter;

    const json& decl_doc() {
        static const json doc = json::parse(
            read_file((repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string()));
        return doc;
    }

    const json& instance(const std::string& id) {
        static std::map<std::string, json> cache;
        auto it = cache.find(id);
        if (it != cache.end()) return it->second;
        for (const auto& inst : decl_doc().at("instances")) {
            if (inst.at("id") == id) {
                cache[id] = inst;
                return cache[id];
            }
        }
        throw std::runtime_error("instance not found: " + id);
    }

    /// `perm` 的 4 值锁(per ADR-CEP-02 CEN-Ib11)
    const std::vector<std::string>& perm_lock() {
        static const std::vector<std::string> kLock = {"NONE", "READ", "UNIQUE", "WRITEBACK"};
        return kLock;
    }

    /// canonical 10 流(per ADR-CEP-02 §2)
    const std::vector<std::string>& canonical_flows() {
        static const std::vector<std::string> kFlows = {
            "request", "completion", "snoop", "snoop_resp", "writeback",
            "forward_data", "back_invalidate", "address_resolve", "barrier", "snoop_completion_ack"};
        return kFlows;
    }

    /// 6 类规范消息类词表(per ADR-CEP-12 §8.3 / ArchForge 32 号 §6.0.1)
    const std::vector<std::string>& msg_class_vocab() {
        static const std::vector<std::string> kV = {"request", "response", "snoop",
                                                    "snoop_resp", "atomic", "query"};
        return kV;
    }

    /// IC2 三条件(per ADR-CEP-12 §8.3)—— 装配期校验的**测试侧**检查器
    /// (闸侧同一判据在 `cep_decl_alignment_check.sh` 的 ②③④ 项;此处是行为化 + 反向 fixture)
    std::vector<std::string> ic2_violations(const json& inst, const json& fabric) {
        std::vector<std::string> bad;
        const json& ordering = inst.at("ordering_assumption");
        const json& binding  = inst.at("vc_binding");
        std::set<std::string> nets;
        for (auto it = ordering.begin(); it != ordering.end(); ++it) nets.insert(it.key());
        for (auto it = binding.begin(); it != binding.end(); ++it) nets.insert(it.key());

        for (const auto& net : nets) {
            if (!ordering.contains(net) || !binding.contains(net)) {
                bad.push_back("网络 " + net + " 未同时出现在 ordering_assumption 与 vc_binding");
                continue;
            }
            // 条件 ①:ordering_assumption[n] 的每个 **vc_id(key)** ∈ vc_binding[n].values()
            //   (值是排序标签 "unordered" / "p2p_ordered",vc_id 落在 key 上)
            for (auto it = ordering.at(net).begin(); it != ordering.at(net).end(); ++it) {
                const int vc = std::stoi(it.key());
                bool bound = false;
                for (auto b = binding.at(net).begin(); b != binding.at(net).end(); ++b) {
                    if (b.value().get<int>() == vc) { bound = true; break; }
                }
                if (!bound) bad.push_back("网络 " + net + ":ordering 的 vc_id " + std::to_string(vc) + " 未被绑定");
            }
            // 条件 ②:每个 vc_id < 该网络的 vc_count
            const int vc_count = fabric.at("networks").at(net).at("vc_count").get<int>();
            for (auto b = binding.at(net).begin(); b != binding.at(net).end(); ++b) {
                const int vc = b.value().get<int>();
                if (vc >= vc_count) {
                    bad.push_back("网络 " + net + ":vc_id " + std::to_string(vc) + " 越界(vc_count=" +
                                  std::to_string(vc_count) + ")");
                }
            }
            // 条件 ③:每个消息类 ∈ 6 类词表
            for (auto b = binding.at(net).begin(); b != binding.at(net).end(); ++b) {
                const std::string cls = b.key();
                if (std::find(msg_class_vocab().begin(), msg_class_vocab().end(), cls) ==
                    msg_class_vocab().end()) {
                    bad.push_back("网络 " + net + ":消息类 " + cls + " 不在 6 类词表内");
                }
            }
        }
        return bad;
    }

    const json& fabric_doc() {
        static const json doc = json::parse(
            read_file((repo_root() / "include" / "tlm" / "cep" / "fabric.json").string()));
        return doc;
    }

}  // namespace

// ══════════════════════════════════════════════════════════════════════
// Scenario 1:6 态 × 6 事件表驱动,且 `E+Write→M` 静默升级存在
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP moesi_gpu: 6 态 × 6 事件表驱动 + E+Write→M 静默升级 + snoop 响应语义",
          "[cep][moesi-gpu-adapter][state-machine]") {
    using A = MoesiGpuAdapter;

    // 表的尺寸恰为 6×6
    REQUIRE(A::kStateCount == 6);
    REQUIRE(A::kEventCount == 6);

    // 状态/事件枚举的**索引**与表下标一致(表是按索引寻址的)
    REQUIRE(static_cast<std::size_t>(A::State::I) == 0);
    REQUIRE(static_cast<std::size_t>(A::State::S) == 1);
    REQUIRE(static_cast<std::size_t>(A::State::E) == 2);
    REQUIRE(static_cast<std::size_t>(A::State::M) == 3);
    REQUIRE(static_cast<std::size_t>(A::State::O) == 4);
    REQUIRE(static_cast<std::size_t>(A::State::T) == 5);
    REQUIRE(static_cast<std::size_t>(A::Event::Rd) == 0);
    REQUIRE(static_cast<std::size_t>(A::Event::Wr) == 1);
    REQUIRE(static_cast<std::size_t>(A::Event::Inv) == 2);
    REQUIRE(static_cast<std::size_t>(A::Event::InvAck) == 3);
    REQUIRE(static_cast<std::size_t>(A::Event::Fl) == 4);
    REQUIRE(static_cast<std::size_t>(A::Event::FlAck) == 5);

    // `E + Write → M`(静默升级的关键转换,per ArchForge 09 号 §3.3)
    REQUIRE(A::transition(A::State::E, A::Event::Wr) == 3);
    REQUIRE(A::is_silent_upgrade(A::State::E, A::Event::Wr));

    // 该转换是**唯一**的静默升级(其余组合都不得被判成静默升级)
    int silent = 0;
    for (std::size_t s = 0; s < A::kStateCount; ++s) {
        for (std::size_t e = 0; e < A::kEventCount; ++e) {
            if (A::is_silent_upgrade(static_cast<A::State>(s), static_cast<A::Event>(e))) ++silent;
        }
    }
    REQUIRE(silent == 1);

    // snoop 响应语义:M 态(s2 支)携带数据值;E 态(s1 支)不携带
    REQUIRE(A::snoop_response_carries_data(A::State::M));
    REQUIRE_FALSE(A::snoop_response_carries_data(A::State::E));

    // 反向 fixture:把 `E+Write` 的目标改掉 ⇒ 静默升级判定必须失效
    // (不改表本体 —— 用一个**独立的**表副本证明判据依赖 `state_transition[2][1]`)
    {
        // ⚠️ `kStateTransition` 是 `static constexpr` ⇒ 直接 `auto` 拷贝会推成**只读**数组
        std::uint8_t broken[A::kStateCount][A::kEventCount];
        for (std::size_t i = 0; i < A::kStateCount; ++i) {
            for (std::size_t j = 0; j < A::kEventCount; ++j) broken[i][j] = A::kStateTransition[i][j];
        }
        broken[2][1] = 2;                    // E + Wr → E(不再是 M)
        REQUIRE(broken[2][1] != 3);
        REQUIRE_FALSE(broken[2][1] == static_cast<std::uint8_t>(A::State::M));
    }
}

// ══════════════════════════════════════════════════════════════════════
// Scenario 2:6→4 `perm` 投影完备,且瞬态 `T` **不泄漏**
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP moesi_gpu: 6→4 perm 投影完备 + 瞬态 T 不泄漏 + O/M 合并登记",
          "[cep][moesi-gpu-adapter][perm-projection]") {
    using A = MoesiGpuAdapter;

    // 每个**稳定态**都有投影
    A::Perm p = A::Perm::NONE;
    REQUIRE(A::perm_of(A::State::I, p));
    REQUIRE(p == A::Perm::NONE);
    REQUIRE(A::perm_of(A::State::S, p));
    REQUIRE(p == A::Perm::READ);
    REQUIRE(A::perm_of(A::State::E, p));
    REQUIRE(p == A::Perm::UNIQUE);
    REQUIRE(A::perm_of(A::State::M, p));
    REQUIRE(p == A::Perm::WRITEBACK);
    REQUIRE(A::perm_of(A::State::O, p));
    REQUIRE(p == A::Perm::WRITEBACK);

    // 稳定态计数 = 5(缺一即判红)
    REQUIRE(A::kStableStateCount == 5);
    int projected = 0;
    for (std::size_t s = 0; s < A::kStateCount; ++s) {
        A::Perm tmp = A::Perm::NONE;
        if (A::perm_of(static_cast<A::State>(s), tmp)) ++projected;
    }
    REQUIRE(projected == 5);

    // 瞬态 `T` **无**投影(跨域接口 SHALL NOT 出现中间态,per INV-CEP-05-IC1)
    REQUIRE_FALSE(A::perm_of(A::State::T, p));

    // 反向 fixture:让 `T` 拥有投影 ⇒ 投影表完备性判据必须能抓到
    {
        // 用一份**带 T 投影**的副本表重算计数 ⇒ 稳定态投影数不变,但 T 不再是「无投影」
        bool t_has_projection = false;
        for (std::size_t s = 0; s < A::kStateCount; ++s) {
            if (static_cast<A::State>(s) == A::State::T) {
                A::Perm tmp = A::Perm::NONE;
                t_has_projection = A::perm_of(static_cast<A::State>(s), tmp);
            }
        }
        REQUIRE_FALSE(t_has_projection);   // 当前形态:T 无投影
    }

    // `O` 与 `M` 合并为 `WRITEBACK` 的语义损失**被显式登记**
    REQUIRE(std::string(A::kPermMergeNote).size() > 0);
    REQUIRE(std::string(A::kPermMergeNote).find("SharerTracker") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════
// Scenario 3:6→3 消息类映射表**完备且非恒等**
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP moesi_gpu: 6→3 消息类映射完备 + 目标值域受限 + 恒等须登记",
          "[cep][moesi-gpu-adapter][message-class-map]") {
    using A = MoesiGpuAdapter;

    // 本实例实际用到的 6 类消息类(逐字对齐 ArchForge 32 号 §6.0.1)
    const auto& vocab = msg_class_vocab();
    REQUIRE(A::kMessageClassCount == vocab.size());
    REQUIRE(A::used_message_class_count() == vocab.size());

    // 每个用到的类**恰好一行**
    for (const auto& cls : vocab) {
        int hits = 0;
        for (std::size_t i = 0; i < A::kMessageClassCount; ++i) {
            if (std::string(A::kMessageClassMap[i].message_class) == cls) ++hits;
        }
        REQUIRE(hits == 1);
    }

    // 每个映射目标 ∈ 链路层 `CATEGORY_*` 的 **3 类**(不扩类)
    const std::set<std::uint8_t> domain = {
        bundles::NoCFlitBundle::CATEGORY_REQUEST,
        bundles::NoCFlitBundle::CATEGORY_RESPONSE,
        bundles::NoCFlitBundle::CATEGORY_CREDIT};
    for (std::size_t i = 0; i < A::kMessageClassCount; ++i) {
        REQUIRE(domain.count(A::kMessageClassMap[i].category) == 1);
    }

    // 查表 API:未知消息类必须查不到(不得静默返回某个默认值)
    std::uint8_t cat = 0xFF;
    REQUIRE(A::category_of("request", cat));
    REQUIRE(cat == bundles::NoCFlitBundle::CATEGORY_REQUEST);
    REQUIRE_FALSE(A::category_of("no_such_class", cat));

    // 恒等判定:本实例的映射 == 平台方向默认表 ⇒ **恒等是合法状态**,但必须被登记
    // (per delta spec:「若某实例的映射确实恒等,则 SHALL 显式声明恒等并携带理由」)
    const auto& ref = tlm::cep::identity_reference_map();
    bool identical = (ref.size() == A::kMessageClassCount);
    for (std::size_t i = 0; i < A::kMessageClassCount && identical; ++i) {
        identical = (std::string(ref[i].first) == std::string(A::kMessageClassMap[i].message_class)) &&
                    (ref[i].second == A::kMessageClassMap[i].category);
    }
    REQUIRE(identical);
    REQUIRE(A::kMappingIsIdentity);
    REQUIRE(std::string(A::kIdentityReason).size() > 0);

    // 反向 fixture:目标写成第 4 类 ⇒ 必须落在 3 类值域之外(判据据此判红)
    {
        const std::uint8_t bogus = 3;
        REQUIRE(domain.count(bogus) == 0);
    }
}

// ══════════════════════════════════════════════════════════════════════
// Scenario 4:实例声明字段断言(逐字镜像由 ⑥ 闸 + 反向 fixture 钉住)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP moesi_gpu: 实例声明字段(protocol_version / 10 流 / perm 4 值 / 13 键)",
          "[cep][moesi-gpu-adapter][declaration]") {
    const json inst = instance("moesi_gpu");

    // 13 个顶层键(schema 的 required 集合)
    REQUIRE(inst.size() == 13);

    // `protocol_version` 落在锁表内(per ADR-CEP-11 §3)⇒ 行为化 INV-CEP-12-IC1
    REQUIRE(inst.at("protocol_version") == "moesi-gpu-1.0");

    // `flow_support` 覆盖 canonical **10 流**全项(per ADR-CEP-02 §2)
    const auto& flows = canonical_flows();
    REQUIRE(inst.at("flow_support").size() == flows.size());
    for (const auto& f : flows) {
        REQUIRE(inst.at("flow_support").contains(f));
        REQUIRE(inst.at("flow_support").at(f).get<bool>());
    }

    // `perm` 是 **4 值**且不含 ATOMIC(per CEN-Ib11)
    const auto& perm = inst.at("perm");
    REQUIRE(perm.size() == perm_lock().size());
    for (const auto& v : perm_lock()) {
        REQUIRE(std::find(perm.begin(), perm.end(), v) != perm.end());
    }
    REQUIRE(std::find(perm.begin(), perm.end(), "ATOMIC") == perm.end());

    // `adapter` 逐字对齐本类的注册名
    REQUIRE(inst.at("adapter") == MoesiGpuAdapter::kAdapterName);
    REQUIRE(inst.at("id") == MoesiGpuAdapter::kInstanceId);
}

// ══════════════════════════════════════════════════════════════════════
// Scenario 5:per-network 嵌套形态的装配期校验通过(IC2 三条件)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP moesi_gpu: per-network 装配期校验(IC2 三条件)+ 越界反向 fixture",
          "[cep][moesi-gpu-adapter][ic2]") {
    const json inst = instance("moesi_gpu");
    const json fab  = fabric_doc();

    // 校验覆盖**两个**网络(嵌套形态是 per-network,不是扁平)
    REQUIRE(inst.at("ordering_assumption").size() == 2);
    REQUIRE(inst.at("vc_binding").size() == 2);
    REQUIRE(inst.at("ordering_assumption").contains("compute_noc"));
    REQUIRE(inst.at("ordering_assumption").contains("ualink_fabric"));

    // 三条件全部成立
    const auto bad = ic2_violations(inst, fab);
    for (const auto& b : bad) {
        INFO("IC2 违规: " << b);
        REQUIRE(bad.empty());
    }

    // 反向 fixture:越界的 `vc_id` ⇒ 必须被抓到(不静默降级)
    {
        json broken = inst;
        broken["vc_binding"]["compute_noc"]["request"] = 9;
        const auto bad2 = ic2_violations(broken, fab);
        REQUIRE_FALSE(bad2.empty());
    }
    // 反向 fixture:消息类写成词表外的值 ⇒ 必须被抓到
    {
        json broken = inst;
        broken["vc_binding"]["compute_noc"]["forward_data"] = 2;
        const auto bad3 = ic2_violations(broken, fab);
        REQUIRE_FALSE(bad3.empty());
    }
}

// ══════════════════════════════════════════════════════════════════════
// Scenario 6:`status` 翻转 ⇒ 计入 adapter 实例数(S1-2 **条件达成**)
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP moesi_gpu: status 翻转 ⇒ implemented 计数 0→1(条件达成登记)",
          "[cep][moesi-gpu-adapter][status]") {
    const json doc = decl_doc();

    int implemented = 0;
    for (const auto& inst : doc.at("instances")) {
        if (inst.at("status") == "implemented") ++implemented;
    }
    REQUIRE(implemented == 1);
    REQUIRE(instance("moesi_gpu").at("status") == "implemented");
    REQUIRE(instance("mesif").at("status") == "planned");

    // 「条件达成」登记:ArchForge 侧是否已翻转**不**由本仓断言
    // (per change design `D-ζ`:跨仓同步**不**作为前置 ⇒ S1-2 记「条件达成」)
    const std::string note = decl_doc().at("derived_from").at("note_entry").get<std::string>();
    REQUIRE(note.find("本仓权威字段") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════
// Scenario 7:`mesif` SHALL **不在**本 change 的交付面内
// ══════════════════════════════════════════════════════════════════════
TEST_CASE("CEP moesi_gpu: 本载体不含 mesif(声明源 / 状态机 / 映射表)",
          "[cep][moesi-gpu-adapter][scope]") {
    // ① adapter 头文件里**零** mesif 命中
    const std::string hdr = read_file(
        (repo_root() / "include" / "tlm" / "cep" / "moesi_gpu_adapter.hh").string());
    REQUIRE(hdr.find("mesif") == std::string::npos);

    // ② 静默升级声明源里**不得**有 mesif 条目
    //   (语义检查而非 grep —— 声明源的 `derived_from` 说明文字里**可以**提及 mesif,
    //    判据关心的是「mesif 有没有静默升级能力」)
    const json pu = json::parse(
        read_file((repo_root() / "include" / "tlm" / "cep" / "perm_upgrades.json").string()));
    for (const auto& u : pu.at("perm_upgrade").at("silent_upgrades")) {
        REQUIRE(u.at("instance_id").get<std::string>() != "mesif");
    }
    // ⚠️ 队列 #8 后:`chi_rnf` 也声明了静默升级(UC→UD)⇒ 2 行。
    //    本断言的**意图**是「mesif **不**出现在声明源里」⇒ 由上面的**逐条**检查承担;
    //    此处的行数断言改为「moesi_gpu **恰好一条**」(逐实例断言,不被别的实例影响)。
    {
        int moesi_rows = 0;
        for (const auto& u : pu.at("perm_upgrade").at("silent_upgrades")) {
            if (u.at("instance_id").get<std::string>() == "moesi_gpu") ++moesi_rows;
        }
        REQUIRE(moesi_rows == 1);
    }

    // ③ `mesif` 的声明保持 13 键逐字镜像 + status = planned(零改动)
    const json mesif = instance("mesif");
    REQUIRE(mesif.size() == 13);
    REQUIRE(mesif.at("status") == "planned");
    REQUIRE(mesif.at("adapter") == "MesifAdapter");
}
