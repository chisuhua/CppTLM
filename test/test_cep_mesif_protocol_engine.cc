// test/test_cep_mesif_protocol_engine.cc
// 队列 #9 `cpptlm-cep-mesif-adapter` —— Phase 2 真插件引擎的契约测试
// per openspec/changes/cpptlm-cep-mesif-adapter/specs/cep-mesif-adapter/spec.md
//     Requirement: cep-mesif-adapter(MA-06 / MA-07)
// 作者: CppTLM Team / 日期: 2026-10-30
//
// ── 判据纪律(本文件的写法约束)──────────────────────────────────────
// ① 每条判据都有可执行反向 fixture,且断言**具体判据**而不是「整体不全」;
// ② 纯负向断言必带正控(同款驱动下的反向取值可分辨);
// ③ 判别谓词必须能区分「走了 adapter 表」与「空转」—— 只记调用次数的计数器不算。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/cohif_protocol_engine.hh"
#include "tlm/cep/mesif_adapter.hh"
#include "tlm/cep/mesif_protocol_engine.hh"
#include "tlm/cep/sharer_tracker.hh"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

    namespace cep = tlm::cep;
    using json = nlohmann::json;

    using cep::LineState;
    using cep::MesifAdapter;
    using cep::MesifProtocolEngine;
    using cep::Perm;
    using cep::SnoopType;
    /// ⚠️ 别名用 `Mev` 而非 `Event`:仓库里另有同名的 `Event`(全局/框架侧)⇒ 裸 `Event` 歧义
    using Mev = cep::MesifAdapter::Event;
    using State = cep::MesifAdapter::State;

    constexpr std::uint64_t kAddr = 0x4000;

    const json& decl_doc() {
        static const json doc = json::parse(cpptlm::cep::read_file(
            (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json")
                .string()));
        return doc;
    }

    const json& mesif_decl() {
        static const json inst = [&]() {
            for (const auto& i : decl_doc().at("instances")) {
                if (i.at("id") == "mesif")
                    return i;
            }
            throw std::runtime_error("protocol_instances.json 里没有 mesif");
        }();
        return inst;
    }

    /// 把 `mesif` 的 `vc_binding` **原样**装进引擎(声明是唯一真源;引擎不解析 JSON)
    void load_routes_from_declaration(MesifProtocolEngine& e, const json& decl) {
        for (const auto& net : decl.at("vc_binding").items()) {
            for (const auto& mc : net.value().items())
                e.set_route(net.key(), mc.key(), mc.value().get<unsigned>());
        }
    }

    /// 驱动一次本地请求(经 egress)
    template <typename PE> std::size_t pending_bundles(PE& pe) {
        std::size_t n = 0;
        for (std::size_t p = 0; p < PE::kPortCount; ++p) {
            if (pe.resp_out()[p].valid())
                ++n;
        }
        return n;
    }

    /// 引擎是否有 `submit_local_request`(Null 引擎没有 ⇒ 结构上不可满足该判据)
    template <typename PE> bool has_submit_local_request(PE& pe) {
        (void)pe;
        return requires(PE & p) {
            p.submit_local_request(std::uint64_t{0}, Mev::Rd, std::uint64_t{0});
        };
    }

    template <typename PE> void drain_egress(PE& pe) {
        for (std::size_t p = 0; p < PE::kPortCount; ++p) {
            if (pe.resp_out()[p].valid())
                pe.resp_out()[p].clear_valid();
        }
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// MA-06 ①⑥ —— 引擎**驱动 adapter 表**(不是空转),且状态可观测
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 引擎:驱动 adapter 表(快照与规约逐格一致,空转引擎不可满足)",
          "[cep][mesif-engine]") {
    EventQueue eq;
    MesifProtocolEngine e("mesif", &eq);

    // ── 反向 fixture 的**起点**:零迁移 ⇒ 快照无效(这就是「空转」的取值)────
    CHECK_FALSE(e.last_transition().valid);
    CHECK(e.adapter_transitions() == 0u);

    struct Case {
        State from;
        Mev ev;
        State to;
    };
    const Case cases[] = {
        {State::I, Mev::Rd, State::S},       {State::S, Mev::Upgrade, State::E},
        {State::E, Mev::Wr, State::M},       {State::M, Mev::SnpShared, State::F},
        {State::F, Mev::RdUnique, State::E},
    };

    std::uint64_t n = 0;
    for (const auto& c : cases) {
        e.set_line(kAddr, c.from, 0xC0FFEE);
        INFO("from=" << cep::MesifStateSpec::state_name(c.from)
                     << " event=" << cep::MesifStateSpec::event_name(c.ev));
        REQUIRE(e.apply_event(kAddr, c.ev));

        // ① 快照必须**等于规约载体**的迁移结果(不是「非空就算过」)
        const auto& snap = e.last_transition();
        CHECK(snap.valid);
        CHECK(static_cast<int>(snap.from) == static_cast<int>(c.from));
        CHECK(static_cast<int>(snap.event) == static_cast<int>(c.ev));
        CHECK(static_cast<int>(snap.to) ==
              static_cast<int>(MesifAdapter::transition(c.from, c.ev)));
        // ② 迁态结果落到行表
        CHECK(static_cast<int>(e.state_of(kAddr)) == static_cast<int>(snap.to));
        ++n;
    }
    CHECK(e.adapter_transitions() == n);

    // ── 正控:远端/本地事件**分别**记账(不是混在一个计数器里)───────────
    CHECK(e.counters().remote_events == 1u); // 只有 M+SnpShared 是远端事件
    CHECK(e.counters().local_events == n - 1u);
}

// ─────────────────────────────────────────────────────────────────────────
// MA-06 ②③④⑥ —— atomic 最小可用路径:接受 + VC 意图 + 意图位
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 引擎:atomic 接受 + 按 vc_binding 路由 ualink_fabric VC1",
          "[cep][mesif-engine]") {
    EventQueue eq;
    MesifProtocolEngine e("mesif", &eq);
    load_routes_from_declaration(e, mesif_decl());

    // ── 声明装载的直接可观测结果(逐类逐网络)─────────────────────────────
    CHECK(e.vc_of("ualink_fabric", "atomic") == 1);
    CHECK(e.vc_of("ualink_fabric", "query") == 2);
    CHECK(e.vc_of("compute_noc", "snoop") == 2);
    // `atomic` 只在 `ualink_fabric` 声明 ⇒ 唯一网络可判定(歧义 ⇒ nullptr)
    REQUIRE(e.network_of("atomic") != nullptr);
    CHECK(std::string(e.network_of("atomic")) == "ualink_fabric");
    // 未声明的组合 ⇒ -1(**不**默认到 0)
    CHECK(e.vc_of("ualink_fabric", "snoop") == -1);

    // ── 正控:合法 atomic 被接受并发射(意图位 + perm 都在 Bundle 里)─────
    REQUIRE(e.submit_atomic(kAddr, 0xDEADBEEF, Perm::WRITEBACK, 77));
    CHECK(e.counters().atomic_accepted == 1u);
    CHECK(e.counters().atomic_rejected == 0u);
    CHECK(e.last_atomic_network() == "ualink_fabric");
    CHECK(e.last_atomic_vc() == 1u);
    CHECK(pending_bundles(e) == 1u);

    // Bundle 侧:atomic 意图位与 perm **正交**承载(CEN-Ib11)
    ::bundles::CohIfBundle sent{};
    bool captured = false;
    for (std::size_t p = 0; p < MesifProtocolEngine::kPortCount; ++p) {
        if (e.resp_out()[p].valid()) {
            sent = e.resp_out()[p].data();
            captured = true;
        }
    }
    REQUIRE(captured);
    CHECK(sent.atomic.read() == 1u);
    CHECK(sent.perm.read() == static_cast<std::uint8_t>(Perm::WRITEBACK));
    CHECK(sent.flow_id == ::bundles::CohIfFlowId::Request);
    drain_egress(e);

    // ── 反向 fixture ①:声明 `atomic.supported = false` ⇒ 必被拒绝闸拦下 ────
    MesifProtocolEngine no_atomic("mesif_no_atomic", &eq);
    load_routes_from_declaration(no_atomic, mesif_decl());
    MesifProtocolEngine::Support s{};
    s.atomic = false;
    no_atomic.set_support(s);
    CHECK_FALSE(no_atomic.submit_atomic(kAddr, 1, Perm::WRITEBACK, 78));
    CHECK(no_atomic.counters().atomic_rejected == 1u);
    CHECK(no_atomic.counters().atomic_accepted == 0u);
    CHECK(pending_bundles(no_atomic) == 0u); // 拒绝 ⇒ 零发射(不是「拒了还发」)

    // ── 反向 fixture ②:VC 被毒化成 3 ⇒ 判据(钉死 1)必红 ─────────────────
    MesifProtocolEngine poisoned("mesif_poisoned", &eq);
    load_routes_from_declaration(poisoned, mesif_decl());
    poisoned.set_route("ualink_fabric", "atomic", 3); // 毒化:走错 VC
    REQUIRE(poisoned.submit_atomic(kAddr, 1, Perm::WRITEBACK, 79));
    CHECK(poisoned.last_atomic_vc() == 3u);
    CHECK(poisoned.last_atomic_vc() != 1u); // ⇒ 与上面的 `== 1` 正控构成判别
}

// ─────────────────────────────────────────────────────────────────────────
// MA-07 —— R-3 的补偿**可观测**:perm 不可分辨(正控)而 tracker dirty 可分辨
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 引擎:R-3 补偿 —— M+SnpShared→F 时 dirty 位由置转清", "[cep][mesif-engine]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    MesifProtocolEngine e("mesif", &eq);
    e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);

    e.set_line(kAddr, State::M, 0xABCD);
    dir.set_sharer(kAddr, cep::kNodeIdSdma, 0, cep::SnoopState::M); // 目录侧:脏副本

    // 前置:两个面都「脏」
    REQUIRE(e.state_of(kAddr) == State::M);
    REQUIRE(dir.view_of(kAddr).dirty != 0);

    e.on_snoop(kAddr, SnoopType::FORWARD, /*txn=*/5); // FORWARD ⇒ SnpShared(决策 E-2)

    // ① 引擎侧迁移到 F
    CHECK(e.state_of(kAddr) == State::F);
    CHECK(e.counters().writebacks_cleared == 1u);

    // ② **不可分辨的正控**:`perm` 在转换前后**同为 WRITEBACK**
    cep::MesifAdapter::Perm before = cep::MesifAdapter::Perm::NONE;
    cep::MesifAdapter::Perm after = cep::MesifAdapter::Perm::NONE;
    REQUIRE(MesifAdapter::perm_of(State::M, before));
    REQUIRE(MesifAdapter::perm_of(State::F, after));
    CHECK(static_cast<int>(before) == static_cast<int>(after));

    // ③ **可分辨的判据**:目录 dirty 位由置转清,sharer 位**仍在**(不是整体删除)
    const auto view = dir.view_of(kAddr);
    CHECK(view.dirty == 0u);
    CHECK(view.sharers == cep::responder_mask_of(cep::kNodeIdSdma));
    CHECK(e.counters().tracker_missing == 0u);

    // ④ 线上照实回「原来是脏的」(写回事实不能凭空消失)
    ::bundles::CohIfBundle sent{};
    bool captured = false;
    for (std::size_t p = 0; p < MesifProtocolEngine::kPortCount; ++p) {
        if (e.resp_out()[p].valid()) {
            sent = e.resp_out()[p].data();
            captured = true;
        }
    }
    REQUIRE(captured);
    CHECK(sent.flow_id == ::bundles::CohIfFlowId::SnoopResp);
    CHECK(sent.dirty.read() == 1u);
    CHECK(sent.hit.read() == 1u);
    // F 在 wire 上无对应值 ⇒ 投影为 S(决策 E-1)
    CHECK(sent.line_state.read() == static_cast<std::uint8_t>(LineState::S));

    // ── 反向 fixture:断掉引擎→tracker 的接线 ⇒ dirty **不转** ⇒ 判据必红 ──
    cep::HierarchicalDirectory dir2;
    MesifProtocolEngine unwired("mesif_unwired", &eq);
    unwired.set_line(kAddr, State::M, 0xABCD);
    dir2.set_sharer(kAddr, cep::kNodeIdSdma, 0, cep::SnoopState::M);
    unwired.on_snoop(kAddr, SnoopType::FORWARD, 6);
    CHECK(unwired.state_of(kAddr) == State::F);      // 引擎自身仍迁移
    CHECK(dir2.view_of(kAddr).dirty != 0);           // 但目录**未**被清 ⇒ 补偿缺失
    CHECK(unwired.counters().tracker_missing == 1u); // 缺口可观测(不是静默)
    CHECK(unwired.counters().tracker_feeds == 0u);
}

// ─────────────────────────────────────────────────────────────────────────
// MA-06 ② —— JSON 链装配 + 一次 CohIf 事务**穿过**引擎抵达对端
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 引擎:JSON 装配 + 事务穿过引擎抵达对端", "[cep][mesif-engine][json-assembly]") {
    const auto path = cpptlm::cep::repo_root() / "configs" / "cep_mesif_pe_minimal.json";
    std::ifstream in(path);
    REQUIRE(in.good());
    json cfg = json::parse(in);
    // 相对路径以**仓库根**为基准(测试 CWD 不固定 ⇒ 绝对化)
    cfg["cep"]["protocol_instances"] =
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* m0 = dynamic_cast<MesifProtocolEngine*>(factory.getInstanceForTesting("mesif0"));
    auto* m1 = dynamic_cast<MesifProtocolEngine*>(factory.getInstanceForTesting("mesif1"));
    REQUIRE(m0 != nullptr);
    REQUIRE(m1 != nullptr);

    // 桥被**消费**(不是「注册表里有但没人用」)
    REQUIRE(m0->bridge() != nullptr);
    REQUIRE(m1->bridge() != nullptr);
    CHECK(factory.unconsumed_bridges().empty());

    // ── 抵达:mesif0 发起 ⇒ 穿过 adapter ⇒ mesif1 收到 ────────────────────
    CHECK(m1->counters().flow_events[static_cast<std::size_t>(cep::CanonicalFlow::Request)] == 0u);
    REQUIRE(m0->submit_local_request(kAddr, Mev::RdUnique, /*txn=*/11));
    m0->tick(); // 消费 ingress(空)+ 驱动 adapter ⇒ 发出
    m1->tick(); // 消费 ingress ⇒ 分派到 canonical 流
    CHECK(m1->counters().flow_events[static_cast<std::size_t>(cep::CanonicalFlow::Request)] == 1u);
    CHECK(m0->counters().request_emitted == 1u);

    // ── 反向 fixture:同拓扑换成 Null 引擎 ⇒ 类型判据与抵达判据**双红** ────
    json cfg_null = cfg;
    cfg_null["modules"][0]["type"] = "CohIfProtocolEngine";
    cfg_null["modules"][1]["type"] = "CohIfProtocolEngine";

    EventQueue eq2;
    ModuleFactory f2(&eq2);
    REQUIRE(f2.instantiateAll(cfg_null)); // 装配**成功**(不是靠装配失败蒙对判据)
    CHECK(dynamic_cast<MesifProtocolEngine*>(f2.getInstanceForTesting("mesif0")) == nullptr);
    auto* n0 = dynamic_cast<cep::CohIfNullProtocolEngine*>(f2.getInstanceForTesting("mesif0"));
    REQUIRE(n0 != nullptr);
    CHECK_FALSE(has_submit_local_request(*n0));
}

// ─────────────────────────────────────────────────────────────────────────
// MA-06 ⑤ —— `consistency_intent` 无独立字段的裁决**显式登记**(不沉默)
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF 引擎:consistency_intent 无独立字段的裁决已登记", "[cep][mesif-engine]") {
    const std::string design =
        cpptlm::cep::read_file((cpptlm::cep::repo_root() / "openspec" / "changes" /
                                "cpptlm-cep-mesif-adapter" / "design.md")
                                   .string());
    const std::string spec_file = cpptlm::cep::read_file(
        (cpptlm::cep::repo_root() / "openspec" / "changes" / "cpptlm-cep-mesif-adapter" / "specs" /
         "cep-mesif-adapter" / "spec.md")
            .string());
    const std::string decl = cpptlm::cep::read_file(
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json")
            .string());

    // 声明侧确实写了 `consistency_intent`(否则这条裁决没有对象)
    REQUIRE(decl.find("consistency_intent") != std::string::npos);

    // 载体侧**没有**该字段(`CohIfBundle` 零扩展的机器可见证据)
    CHECK(decl.find("\"consistency_intent\"") != std::string::npos);
    const std::string bundle = cpptlm::cep::read_file(
        (cpptlm::cep::repo_root() / "include" / "bundles" / "cep_coherence_bundles_tlm.hh")
            .string());
    CHECK(bundle.find("consistency_intent") == std::string::npos);

    // 裁决**显式登记**在 change 的 design / spec 里(两处都可寻址)
    CHECK(design.find("consistency_intent") != std::string::npos);
    CHECK(design.find("无独立字段") != std::string::npos);
    CHECK(spec_file.find("consistency_intent") != std::string::npos);
}
