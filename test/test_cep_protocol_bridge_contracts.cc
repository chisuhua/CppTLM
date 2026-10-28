// test/test_cep_protocol_bridge_contracts.cc
// 队列 #7 `cpptlm-cep-cohif-carrier` Phase 3 —— ProtocolBridge 契约测试
// per openspec/changes/cpptlm-cep-cohif-carrier/specs/cep-protocol-bridge-contracts/spec.md
//     Requirement: cep-protocol-bridge-contracts(CC-03 / CC-05 / CC-06)
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 判据分层(per B′ 方案的架构论证,2026-10-28)────────────────────────
//   【层 1 · 内容层】共有字段 round-trip 恒等 + 丢弃字段复位 + 网络字段不被越权改写
//   【层 2 · 端到端层】`Packet` payload 承载完整 12 字段 ⇒ 逐字段恒等
//   ⇒ 「CohIf 语义跨网络不透损」由**端到端层**保证(Bundle 级不可达 ——
//     `flow_id` 5 值 → `flit_category` 3 值是**多对一**有损映射)
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"
#include "cep_protocol_bridge_contract.hh"
#include "cep_test_protocol_engine.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "framework/protocol_bridge.hh"
#include "tlm/cep/adapter_schema_contracts.hh"
#include "tlm/cep/cohif_noc_bridge.hh"
#include "tlm/cep/moesi_gpu_adapter.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/protocol_family.hh"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

using tlm::cep::CohIfNoCProtocolBridge;
using bundles::CohIfBundle;
// 平台级枚举在 `tlm::cep` 命名空间(per `ADR-CEP-02` / `ADR-CEP-05`)
using tlm::cep::Perm;
using tlm::cep::SnoopType;
using tlm::cep::LineState;

/// 每个用例用**独立**桥名 —— 注册表是进程级全局,且 `register_protocol_bridge`
/// 对同名做 early-return(ODR-safe)⇒ 同名注册会被**静默跳过** ⇒ 后一个用例
/// 拿到的就不是自己注册的那个(factory 不被调用)。
std::string bridge_name(const char* tag) { return std::string("cep_test_cohif_noc_") + tag; }

/// RAII 守卫:析构时**恢复**桥注册表到进入前的状态 —— 避免某个用例的 `REQUIRE`
/// 失败时跳过清理、污染后续用例(实测踩过:CC-05 ② 失败 ⇒ 注册表残留 ⇒
/// CC-05 ③ 的 factory 从未被调用)。
///
/// ⚠️ **恢复**而非**清空** —— 因为 `chstream_register.hh` 的 `REGISTER_CHSTREAM`
///    会**全局**注册 `cohif_noc` 桥(队列 #7 Phase 5)⇒ 清空会**破坏**其它用例
///    依赖的全局注册 ⇒ 改为「保存 + 恢复」。
struct BridgeRegistryGuard {
    BridgeRegistryGuard() : saved_(ModuleFactory::get_bridges()) {}
    ~BridgeRegistryGuard() { ModuleFactory::get_bridges() = saved_; }
    BridgeRegistryGuard(const BridgeRegistryGuard&) = delete;
    BridgeRegistryGuard& operator=(const BridgeRegistryGuard&) = delete;

private:
    decltype(ModuleFactory::get_bridges()) saved_;
};

// ── 测试用 ProtocolEngine 派生类(实现 Phase 2 定义的 10 个纯虚 `on_*`)──
// ⚠️ 定义在**共享头**(Phase 3 测试 + Phase 4 编译探针共用)⇒ 避免两份重复
using TestProtocolEngine = tlm::cep::testing::TestProtocolEngine;

// ── **篡改**桥(reverse fixture 用)──────────────────────────────────
// ① `TamperedPermBridge`:把 `perm` 映射到 `flit_category`(task 3.9 的例子)
//    ⇒ 声称丢弃的字段**泄漏**进了网络 flit ⇒ 字段映射探针必须报红
class TamperedPermBridge : public CohIfNoCProtocolBridge {
public:
    void pack(const InBundle& in, OutBundle& out) override {
        CohIfNoCProtocolBridge::pack(in, out);
        out.flit_category.write(in.perm.read() & 0x3);  // ← 篡改点
    }
};

// ② `TamperedAddrBridge`:把 `address` 与 `data` 对调
//    ⇒ 共有字段 round-trip 必须报红
class TamperedAddrBridge : public CohIfNoCProtocolBridge {
public:
    void pack(const InBundle& in, OutBundle& out) override {
        CohIfNoCProtocolBridge::pack(in, out);
        out.address.write(in.data.read());  // ← 篡改点
        out.data.write(in.address.read());
    }
};

/// 一个**全字段非默认**的 `CohIfBundle` —— 端到端判据必须能区分每个字段
CohIfBundle rich_bundle() {
    CohIfBundle b;
    b.flow_id = bundles::CohIfFlowId::SnoopResp;
    b.transaction_id.write(0x0123456789ABCDEFULL);
    b.address.write(0xFEDCBA9876543210ULL);
    b.data.write(0x0F1E2D3C4B5A6978ULL);
    b.size.write(64);
    b.perm.write(CohIfBundle::PERM_WRITEBACK);
    b.snoop_type.write(CohIfBundle::SNOOP_FORWARD);
    b.line_state.write(CohIfBundle::STATE_M);
    b.hit.write(1);
    b.dirty.write(1);
    b.error.write(1);
    b.atomic.write(1);
    return b;
}

/// 协议实例声明的 `family`(从 `protocol_instances.json` 读 —— 单一真源在 JSON)
std::string declared_family(const std::string& instance_id) {
    static const json doc = json::parse(
        cpptlm::cep::read_file((cpptlm::cep::repo_root() / "include" / "tlm" / "cep" /
                                "protocol_instances.json")
                                   .string()));
    for (const auto& inst : doc.at("instances")) {
        if (inst.at("id") == instance_id)
            return inst.at("family").get<std::string>();
    }
    return "";
}

/// 最小可装配拓扑 + `cep.bridges` 段
json minimal_cfg_with_bridge(const std::string& bridge_name_str,
                             const std::string& protocol_instance) {
    json cfg;
    cfg["modules"] = json::array();
    cfg["connections"] = json::array();
    cfg["modules"].push_back({{"name", "pe0"}, {"type", "TestProtocolEngine"}});
    cfg["modules"].push_back({{"name", "pe1"}, {"type", "TestProtocolEngine"}});

    cfg["cep"] = json::object();
    cfg["cep"]["protocol_instances"] =
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    cfg["cep"]["bridges"] = json::array();
    cfg["cep"]["bridges"].push_back({{"name", bridge_name_str},
                                     {"protocol_instance", protocol_instance},
                                     {"bridge_id", 7},
                                     {"modules", json::array({"pe0", "pe1"})}});
    return cfg;
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════
//  CC-03 —— `pack` / `unpack` 双向可逆
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("CC-03 ① 内容层:共有字段 round-trip 恒等 + 丢弃字段复位", "[cep][protocol-bridge]") {
    CohIfNoCProtocolBridge bridge;
    const CohIfBundle in = rich_bundle();

    const auto r = cpptlm::cep::check_content_round_trip(bridge, in);
    INFO("共有字段不相等: " << [&] {
             std::string s;
             for (const auto& f : r.mismatched) s += f + " ";
             return s;
         }());
    INFO("丢弃字段未复位: " << [&] {
             std::string s;
             for (const auto& f : r.not_reset) s += f + " ";
             return s;
         }());
    REQUIRE(r.mismatched.empty());
    REQUIRE(r.not_reset.empty());
    REQUIRE(r.ok);
}

TEST_CASE("CC-03 ① 端到端层:完整 12 字段逐字段恒等(端到端不透损)", "[cep][protocol-bridge]") {
    CohIfNoCProtocolBridge bridge;
    const CohIfBundle in = rich_bundle();

    const auto r = cpptlm::cep::check_end_to_end_round_trip(bridge, in);
    INFO("payload 可用: " << r.payload_usable);
    INFO("不相等字段: " << [&] {
             std::string s;
             for (const auto& f : r.mismatched) s += f + " ";
             return s;
         }());
    REQUIRE(r.payload_usable);
    REQUIRE(r.mismatched.empty());
    REQUIRE(r.ok);
}

TEST_CASE("CC-03 ② 职责边界:网络层语义只在 NoCFlitBundle 侧", "[cep][protocol-bridge]") {
    CohIfNoCProtocolBridge bridge;
    const CohIfBundle in = rich_bundle();

    const auto r = cpptlm::cep::check_content_round_trip(bridge, in);
    INFO("被桥越权改写的网络字段: " << [&] {
             std::string s;
             for (const auto& f : r.touched_net) s += f + " ";
             return s;
         }());
    REQUIRE(r.touched_net.empty());

    // 字段清单自洽:共有(4) + 降维(1) + 丢弃(7) = 12;三类两两不交
    const auto& shared = CohIfNoCProtocolBridge::shared_fields();
    const auto& dropped = CohIfNoCProtocolBridge::dropped_field_names();
    const auto& lossy = CohIfNoCProtocolBridge::lossily_mapped_fields();
    REQUIRE(shared.size() == 4);
    REQUIRE(dropped.size() == 7);
    REQUIRE(lossy.size() == 1);
    REQUIRE(shared.size() + dropped.size() + lossy.size() == 12);
    for (const auto& s : shared) {
        INFO("共有字段与丢弃/降维字段重叠: " << s);
        REQUIRE(std::find(dropped.begin(), dropped.end(), s) == dropped.end());
        REQUIRE(std::find(lossy.begin(), lossy.end(), s) == lossy.end());
    }
    // `flow_id` 是**降维**映射(5→3 多对一),**不是**丢弃 —— 混淆二者会让
    // 「丢弃清单 ↔ 实际行为」判据假红(实测踩过)
    REQUIRE(std::find(dropped.begin(), dropped.end(), "flow_id") == dropped.end());
    REQUIRE(lossy[0] == "flow_id");
    // 层 1 不携带的字段 = 丢弃 7 + 降维 1 = 8
    REQUIRE(CohIfNoCProtocolBridge::layer1_uncarried_fields().size() == 8);
    // 丢弃清单不得是网络层字段
    for (const auto& d : dropped) {
        INFO("丢弃清单含网络层字段: " << d);
        REQUIRE(d != "src_node");
        REQUIRE(d != "dst_node");
        REQUIRE(d != "vc_id");
        REQUIRE(d != "hops");
    }
}

TEST_CASE("CC-03 ③ 同一 ProtocolBridgeEr* 可被多个 PE 共享", "[cep][protocol-bridge]") {
    EventQueue eq;
    TestProtocolEngine pe0("pe0", &eq);
    TestProtocolEngine pe1("pe1", &eq);

    CohIfNoCProtocolBridge bridge;
    cpptlm::ProtocolBridgeEr* erased = &bridge;

    REQUIRE(pe0.attach_protocol_bridge(erased));
    REQUIRE(pe1.attach_protocol_bridge(erased));
    REQUIRE(pe0.bridge() == erased);
    REQUIRE(pe1.bridge() == erased);
    REQUIRE(pe0.bridge() == pe1.bridge());
}

// ══════════════════════════════════════════════════════════════════════
//  CC-05 —— `register_protocol_bridge` 从死钩子变活钩子
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("CC-05 ① 注册表可读(不再是 write-only 死钩子)", "[cep][protocol-bridge]") {
    BridgeRegistryGuard guard;
    const std::string name = bridge_name("readable");
    ModuleFactory::register_protocol_bridge(
        name, []() -> cpptlm::ProtocolBridgeEr* { return new CohIfNoCProtocolBridge(); });

    REQUIRE(ModuleFactory::get_bridges().count(name) == 1);
    auto it = ModuleFactory::get_bridges().find(name);
    REQUIRE(it != ModuleFactory::get_bridges().end());
    REQUIRE(static_cast<bool>(it->second));

    // 工厂可调用 ⇒ 注册的是**可实例化**的桥,不是只有名字
    std::unique_ptr<cpptlm::ProtocolBridgeEr> made(it->second());
    REQUIRE(made != nullptr);
    REQUIRE(made->name() == "cohif_noc");

}

TEST_CASE("CC-05 ② 桥被 ModuleFactory 消费(注册与消费成对)", "[cep][protocol-bridge]") {
    BridgeRegistryGuard guard;
    ModuleFactory::registerObject<TestProtocolEngine>("TestProtocolEngine");
    const std::string name = bridge_name("consumed");
    ModuleFactory::register_protocol_bridge(
        name, []() -> cpptlm::ProtocolBridgeEr* { return new CohIfNoCProtocolBridge(); });

    EventQueue eq;
    ModuleFactory factory(&eq);
    json cfg = minimal_cfg_with_bridge(name, "moesi_gpu");

    REQUIRE(factory.instantiateAll(cfg));
    REQUIRE(factory.consumed_bridges().count(name) == 1);
    // 目标桥**已消费** ⇒ 不在「死桥」清单里
    //
    // ⚠️ 清单**不必**为空 —— `chstream_register.hh` 的 `REGISTER_CHSTREAM` 会**全局**
    //    注册 `cohif_noc`(队列 #7 Phase 5)⇒ 本配置不引用它是**正常**的
    //    ⇒ per Step 7.5 的「死桥**不判红**,以清单暴露」口径。
    const auto dead = factory.unconsumed_bridges();
    REQUIRE(std::find(dead.begin(), dead.end(), name) == dead.end());
}

TEST_CASE("CC-05 ③ 钩子形态为 (bridge_name, factory_lambda)", "[cep][protocol-bridge]") {
    BridgeRegistryGuard guard;
    const std::string name = bridge_name("lambda");
    int calls = 0;
    ModuleFactory::register_protocol_bridge(name, [&calls]() -> cpptlm::ProtocolBridgeEr* {
        ++calls;
        return new CohIfNoCProtocolBridge();
    });
    REQUIRE(ModuleFactory::get_bridges().count(name) == 1);
    auto it = ModuleFactory::get_bridges().find(name);
    REQUIRE(it != ModuleFactory::get_bridges().end());
    std::unique_ptr<cpptlm::ProtocolBridgeEr> made(it->second());
    REQUIRE(made != nullptr);
    REQUIRE(calls == 1);
}

TEST_CASE("CC-05 ④ per-bridge 元数据:protocol_id 非零 / bridge_id 唯一 / 与 family 一致",
          "[cep][protocol-bridge]") {
    // `protocol_id_` = family 的数字编号(per 2026-10-28 DRI 裁决)
    const std::string fam_str = declared_family("moesi_gpu");
    REQUIRE_FALSE(fam_str.empty());
    tlm::cep::ProtocolFamily fam{};
    REQUIRE(tlm::cep::protocol_family_from_name(fam_str, fam));
    const std::uint8_t expect = tlm::cep::protocol_family_id(fam);
    REQUIRE(expect != 0);

    CohIfNoCProtocolBridge bridge;
    bridge.set_bridge_id(3);
    bridge.set_protocol_id(expect);

    std::string why;
    REQUIRE(ModuleFactory::validate_bridge_metadata(&bridge, expect, &why));
    REQUIRE(why.empty());

    // `bridge_id_` 唯一
    std::vector<std::pair<std::uint16_t, std::string>> assigned;
    assigned.emplace_back(3, "a");
    REQUIRE(ModuleFactory::check_bridge_id_collision(assigned, 4, "b", &why));
    ModuleFactory::get_bridges().clear();
}

// ══════════════════════════════════════════════════════════════════════
//  CC-06 —— 6→3 映射表产出的 CATEGORY_* 全部落在网络 Bundle 的 3 个常量内
//  (引用队列 #6 的 AC-04 实现,**不重立第二份**)
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("CC-06 桥的 5→3 映射产出 ∈ 队列 #6 的 3 类值域", "[cep][protocol-bridge]") {
    const auto& domain = tlm::cep::category_value_domain();
    REQUIRE(domain.size() == 3);

    const std::vector<bundles::CohIfFlowId> all = {
        bundles::CohIfFlowId::Request, bundles::CohIfFlowId::Completion,
        bundles::CohIfFlowId::Snoop,   bundles::CohIfFlowId::SnoopResp,
        bundles::CohIfFlowId::Writeback};
    for (auto f : all) {
        const std::uint8_t c = CohIfNoCProtocolBridge::category_of(f);
        INFO("flow 产出类别不在 3 类值域内");
        REQUIRE(std::find(domain.begin(), domain.end(), c) != domain.end());
    }

    // 队列 #6 的 6→3 映射表:类别 ∈ 值域(引用 AC-04 的实现,不重立)
    for (std::size_t i = 0; i < tlm::cep::MoesiGpuAdapter::kMessageClassCount; ++i) {
        const auto& row = tlm::cep::MoesiGpuAdapter::kMessageClassMap[i];
        INFO("队列 #6 的类别不在 3 类值域内: " << row.message_class);
        REQUIRE(std::find(domain.begin(), domain.end(), row.category) != domain.end());
    }

    // 桥的 5→3 映射**非恒等**(否则特化字典无意义)
    bool non_identity = false;
    for (auto f : all) {
        const std::uint8_t c = CohIfNoCProtocolBridge::category_of(f);
        const char* canonical = (c == bundles::NoCFlitBundle::CATEGORY_REQUEST)  ? "request"
                               : (c == bundles::NoCFlitBundle::CATEGORY_RESPONSE) ? "response"
                                                                                  : "credit";
        const char* flow_name = (f == bundles::CohIfFlowId::Request)    ? "request"
                                : (f == bundles::CohIfFlowId::Completion) ? "completion"
                                : (f == bundles::CohIfFlowId::Snoop)      ? "snoop"
                                : (f == bundles::CohIfFlowId::SnoopResp)  ? "snoop_resp"
                                                                          : "writeback";
        if (std::string(canonical) != std::string(flow_name))
            non_identity = true;
    }
    REQUIRE(non_identity);
}

// ══════════════════════════════════════════════════════════════════════
//  反向 fixture —— 每条判据都必须在**因正确原因**红
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("反向 fixture ① 只注册不消费 ⇒ 死桥清单非空", "[cep][protocol-bridge]") {
    BridgeRegistryGuard guard;
    const std::string name = bridge_name("dead");
    ModuleFactory::register_protocol_bridge(
        name, []() -> cpptlm::ProtocolBridgeEr* { return new CohIfNoCProtocolBridge(); });

    EventQueue eq;
    ModuleFactory factory(&eq);
    json cfg;
    cfg["modules"] = json::array();
    cfg["connections"] = json::array();
    REQUIRE(factory.instantiateAll(cfg));

    const auto dead = factory.unconsumed_bridges();
    // ⚠️ **含**目标桥即可(不要求 size == 1)—— `chstream_register.hh` 的
    //    `REGISTER_CHSTREAM` 会**全局**注册 `cohif_noc`(队列 #7 Phase 5)
    //    ⇒ 本次装配的未消费清单里可能有其它全局注册的桥。
    REQUIRE(std::find(dead.begin(), dead.end(), name) != dead.end());
    REQUIRE(factory.consumed_bridges().empty());
}

TEST_CASE("反向 fixture ② 重复 bridge_id ⇒ 判红", "[cep][protocol-bridge]") {
    std::vector<std::pair<std::uint16_t, std::string>> assigned;
    assigned.emplace_back(5, "bridge_a");
    std::string why;
    REQUIRE(ModuleFactory::check_bridge_id_collision(assigned, 6, "bridge_b", &why));
    // 重复 ⇒ 红
    REQUIRE_FALSE(ModuleFactory::check_bridge_id_collision(assigned, 5, "bridge_c", &why));
    REQUIRE(why.find("重复") != std::string::npos);
    REQUIRE(why.find("bridge_a") != std::string::npos);
    REQUIRE(why.find("bridge_c") != std::string::npos);
}

TEST_CASE("反向 fixture ③ protocol_id 与 family 不符 ⇒ 判红", "[cep][protocol-bridge]") {
    const std::string fam_str = declared_family("moesi_gpu");
    tlm::cep::ProtocolFamily fam{};
    REQUIRE(tlm::cep::protocol_family_from_name(fam_str, fam));
    const std::uint8_t expect = tlm::cep::protocol_family_id(fam);

    CohIfNoCProtocolBridge bridge;
    bridge.set_bridge_id(1);
    bridge.set_protocol_id(static_cast<std::uint8_t>(expect + 1));  // ← 篡改:与 family 不符

    std::string why;
    REQUIRE_FALSE(ModuleFactory::validate_bridge_metadata(&bridge, expect, &why));
    REQUIRE(why.find("protocol_id_") != std::string::npos);
}

TEST_CASE("反向 fixture ④ 篡改字段映射 ⇒ round-trip 必红", "[cep][protocol-bridge]") {
    // ④-a 把 `perm` 映射到 `flit_category` ⇒ 声称丢弃的字段泄漏 ⇒ 字段映射探针红
    TamperedPermBridge tampered_perm;
    const CohIfBundle base = rich_bundle();
    const auto probe = cpptlm::cep::check_field_mapping(tampered_perm, base);
    INFO("泄漏的字段: " << [&] {
             std::string s;
             for (const auto& f : probe.leaked) s += f + " ";
             return s;
         }());
    REQUIRE_FALSE(probe.ok);
    REQUIRE(std::find(probe.leaked.begin(), probe.leaked.end(), "perm") != probe.leaked.end());

    // ④-b `address` 与 `data` 对调 ⇒ 内容层共有字段 round-trip 红
    TamperedAddrBridge tampered_addr;
    const auto rt = cpptlm::cep::check_content_round_trip(tampered_addr, base);
    INFO("不相等的共有字段: " << [&] {
             std::string s;
             for (const auto& f : rt.mismatched) s += f + " ";
             return s;
         }());
    REQUIRE_FALSE(rt.ok);
    REQUIRE_FALSE(rt.mismatched.empty());
}

TEST_CASE("正控:正确实现的桥在三条判据上全绿", "[cep][protocol-bridge]") {
    CohIfNoCProtocolBridge bridge;
    const CohIfBundle base = rich_bundle();
    REQUIRE(cpptlm::cep::check_content_round_trip(bridge, base).ok);
    REQUIRE(cpptlm::cep::check_end_to_end_round_trip(bridge, base).ok);
    REQUIRE(cpptlm::cep::check_field_mapping(bridge, base).ok);
}
