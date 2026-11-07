// test/test_cep_ualink_engine.cc
// 判据:`ualink_4` 的**记账级专用承载引擎**(`UalinkProtocolEngine`)——
//      三分法观测面 + completion 不依赖有序性 + 装配闭环 + B3b-2 计数不变
//      (change `cpptlm-cep-ualink-coherence-engine`)。
//
// 覆盖:
//   UGE-01 引擎已注册 ⇒ ModuleFactory::instantiateAll **真跑** ⇒ 可实例化
//   UGE-02 **三分法**观测面:记账 2 / 拒绝 7 / pending 1 + **4 条机器可判不变量**
//   UGE-03 completion 记账**不依赖有序性**:中途探针 + 端态 + **决定性变异**(判据有牙)
//   UGE-04 `snoop_resp` 的 pending **终态**语义(四句的机器绑定)
//   UGE-05 装配闭环(拓扑挂载 + 装配闸 PASS + `ualink_4` **required** plane 绑定)+ **零发射**
//   UGE-06 **B3b-2 仍 0/4** 的断言显式存在(本 change **不改**计数)
//
// ⚠️ 强度标签(per `ADR-CEP-12` SU 2026-11-07 Entry #269):本引擎 = **记账级**
//    (bookkeeping-only)—— 零发射 / 零协议状态机 / 零 UPLI 通道级语义;
//    ⛔ 本测试**不**证明「UALink 协议语义已实现」;⇒ **不计入 B3b**。
//
// 作者: builder  日期: 2026-11-07
#include "catch_amalgamated.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "chstream_register.hh" // `REGISTER_ALL`(注册面)
#include "cep_adr_parser.hh"    // `cpptlm::cep::repo_root()`(测试侧仓根定位)
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/instance_assembly_gate.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/ualink_protocol_engine.hh"

using Engine = tlm::cep::UalinkProtocolEngine;
using Bundle = bundles::CohIfBundle;
using FlowId = bundles::CohIfFlowId;
using Flow = tlm::cep::CanonicalFlow;

namespace {

    constexpr std::uint64_t kAddr = 0x1000;

    Bundle make_bundle(FlowId flow, std::uint64_t addr, std::uint64_t data, std::uint64_t txn,
                       std::uint8_t perm) {
        Bundle b;
        b.flow_id = flow;
        b.address.write(addr);
        b.data.write(data);
        b.transaction_id.write(txn);
        b.perm.write(perm);
        b.hit.write(0);
        b.dirty.write(0);
        b.error.write(0);
        b.atomic.write(0);
        b.snoop_type.write(Bundle::SNOOP_INVALIDATE);
        b.line_state.write(Bundle::STATE_NONE);
        return b;
    }

    nlohmann::json load_topology(const char* file) {
        const auto path = cpptlm::cep::repo_root() / "configs" / file;
        std::ifstream in(path);
        if (!in.good())
            throw std::runtime_error(std::string("无法打开装配配置: ") + file);
        nlohmann::json cfg = nlohmann::json::parse(in);
        cfg["cep"]["protocol_instances"] =
            (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json")
                .string();
        return cfg;
    }

    nlohmann::json load_cep_json(const char* rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        REQUIRE(in.good());
        nlohmann::json j;
        in >> j;
        return j;
    }

    /// **经 CohIf 边界**的 5 条(canonical 10 流中跨 Stub↔PE 的条数)⇒ 只这 5 条可由
    /// `dispatch_cohif` 派发;其余 5 条只能**直调** `on_*`(显式落点)。
    const std::array<FlowId, 5> kCohIfFlows{FlowId::Request, FlowId::Completion, FlowId::Snoop,
                                            FlowId::SnoopResp, FlowId::Writeback};

    /// **不经 CohIf** 的 5 条(其余 canonical 流)—— 直调落点。
    void drive_non_cohif_flows(Engine& e) {
        e.on_forward_data(kAddr, 0x2000, 0x1, tlm::cep::Perm::READ, 3);
        e.on_back_invalidate(kAddr, tlm::cep::SnoopType::INVALIDATE, 4);
        tlm::cep::AddressResolveResult r;
        e.on_address_resolve(kAddr, 0, r);
        e.on_barrier(0, 5);
        e.on_snoop_completion_ack(kAddr, 0, 6);
    }

    /// canonical 10 流的**逐流索引**(便于断言逐流计数)。
    constexpr std::size_t flow_idx(Flow f) {
        return static_cast<std::size_t>(f);
    }

    /// 7 条**拒绝**流(声明 `false` 且**不是** `snoop_resp`)。
    const std::array<Flow, 7> kRejectedFlows{
        Flow::Snoop,          Flow::Writeback, Flow::ForwardData,       Flow::BackInvalidate,
        Flow::AddressResolve, Flow::Barrier,   Flow::SnoopCompletionAck};

} // namespace

TEST_CASE("UGE-01: UALink 引擎已注册 —— instantiateAll 真跑 ⇒ 可实例化",
          "[cep][ualink][engine][uge-01]") {
    const auto cfg = load_topology("cep_ualink_engine_pe_minimal.json");
    REQUIRE(cfg.at("modules").size() == 2u);
    REQUIRE(cfg.at("modules")[0].at("type").get<std::string>() == "UalinkProtocolEngine");

    // ⚠️ 真跑 `ModuleFactory::instantiateAll`(含 CEP 装配期闸)⇒ 注册面缺失会**立即**变红。
    //   (先例:critic NIT-1 —— MGE-01 原先只验「配置里的 type 名匹配」⇒ 弱验证。)
    REGISTER_ALL;
    EventQueue eq;
    ModuleFactory f(&eq);
    REQUIRE(f.instantiateAll(cfg));
}

TEST_CASE("UGE-02: 三分法观测面 —— 记账 2 / 拒绝 7 / pending 1 + 4 条机器可判不变量",
          "[cep][ualink][engine][uge-02]") {
    EventQueue eq;
    Engine e("uge", &eq);

    // ① 经 CohIf 边界的 5 条(request / completion / snoop / snoop_resp / writeback)
    for (std::size_t i = 0; i < kCohIfFlows.size(); ++i) {
        REQUIRE(e.dispatch_cohif(make_bundle(kCohIfFlows[i], kAddr, i, i + 1, Bundle::PERM_READ)));
    }
    // ② 不经 CohIf 的 5 条(直调)
    drive_non_cohif_flows(e);

    const auto& c = e.counters();

    // ── 不变量 1:被声明支持的流**永不被拒** ──
    REQUIRE(c.unsupported_rejected[flow_idx(Flow::Request)] == 0u);
    REQUIRE(c.unsupported_rejected[flow_idx(Flow::Completion)] == 0u);

    // ── 不变量 2:声明不支持的 7 条 ⇒ **每次抵达都被拒绝**(抵达 == 拒绝) ──
    for (const Flow f : kRejectedFlows) {
        INFO("flow index=" << flow_idx(f));
        REQUIRE(c.flow_events[flow_idx(f)] == 1u);
        REQUIRE(c.unsupported_rejected[flow_idx(f)] == 1u);
    }

    // ── 不变量 3:`snoop_resp` ⇒ 恰好一次 pending 登记,零「拒绝」计数 ──
    REQUIRE(c.flow_events[flow_idx(Flow::SnoopResp)] == 1u);
    REQUIRE(c.snoop_resp_pending_not_consumed == 1u);
    REQUIRE(c.unsupported_rejected[flow_idx(Flow::SnoopResp)] == 0u);

    // ── 不变量 4:completion 的 `txn_id` **恒等匹配** ⇒ 正常运行路径 unmatched == 0 ──
    // (本用例的 completion 用 txn=2,而 request 用 txn=1 ⇒ 故意构造 unmatched)
    REQUIRE(c.completion_unmatched == 1u);

    // ⚠️ `flow_events` 对**全部 10 流**计抵达(per ADR-CEP-12 SU 2026-11-07 ④ 的表格括注)
    for (std::size_t i = 0; i < tlm::cep::kCanonicalFlowCount; ++i) {
        INFO("flow index=" << i);
        REQUIRE(c.flow_events[i] == 1u);
    }

    // ⚠️ **零发射**(per SU ②:⛔ 零 egress)—— 引擎从不写 egress。
    REQUIRE(c.egress_emitted == 0u);
    for (std::size_t p = 0; p < 2; ++p) {
        REQUIRE_FALSE(e.resp_out()[p].valid());
    }
}

TEST_CASE("UGE-03: completion 记账不依赖有序性 —— 中途探针 + 端态 + 决定性变异(判据有牙)",
          "[cep][ualink][engine][uge-03]") {
    // ── ① 中途探针:**中间 id** 的 completion ⇒ 在途集合逐格相等 ──
    //    ⚠️ 用「中间 id」而非「最小 id」:恒等 erase(t2) ⇒ {t1,t3};
    //    FIFO-pop(t1) ⇒ {t2,t3};LIFO-pop(t3) ⇒ {t1,t2} ⇒ **三者互不相同**
    //    ⇒ 集合**逐格相等**断言有鉴别力(若只断言「仅剩 1 个」则三者都过 ⇒ 无牙)。
    {
        EventQueue eq;
        Engine e("uge-mid", &eq);
        e.on_request(kAddr, tlm::cep::Perm::NONE, 1);
        e.on_request(kAddr, tlm::cep::Perm::NONE, 2);
        e.on_request(kAddr, tlm::cep::Perm::NONE, 3);
        REQUIRE(e.outstanding_request_ids().size() == 3u);

        e.on_completion(2, 0, tlm::cep::Perm::NONE, false); // 只完成**中间**那个

        // 恒等匹配 ⇒ 擦除的**是** txn=2 ⇒ 剩 {1, 3}
        const auto& outstanding = e.outstanding_request_ids();
        REQUIRE(outstanding.size() == 2u);
        REQUIRE(outstanding.count(1) == 1u);
        REQUIRE(outstanding.count(2) == 0u); // ← LIFO mutant 会红(它剩 {1,2})
        REQUIRE(outstanding.count(3) == 1u);
        REQUIRE(e.counters().completion_unmatched == 0u);
    }

    // ── ② 端态:反序完成 ⇒ 集合空 / unmatched==0 / 计数与正序**逐字段相等** ──
    Engine in_order("uge-in", nullptr);
    Engine rev_order("uge-rev", nullptr);
    for (std::uint64_t t : {1u, 2u, 3u}) {
        in_order.on_request(kAddr, tlm::cep::Perm::NONE, t);
        rev_order.on_request(kAddr, tlm::cep::Perm::NONE, t);
    }
    for (std::uint64_t t : {1u, 2u, 3u}) {
        in_order.on_completion(t, 0, tlm::cep::Perm::NONE, false);
    }
    for (std::uint64_t t : {3u, 1u, 2u}) { // ← **反序**
        rev_order.on_completion(t, 0, tlm::cep::Perm::NONE, false);
    }
    REQUIRE(in_order.outstanding_request_ids().empty());
    REQUIRE(rev_order.outstanding_request_ids().empty());
    REQUIRE(rev_order.counters().completion_unmatched == 0u);
    // 逐字段相等(⛔ 不仅计数相等,连拒绝/pending 计数也相等)
    for (std::size_t i = 0; i < tlm::cep::kCanonicalFlowCount; ++i) {
        REQUIRE(rev_order.counters().flow_events[i] == in_order.counters().flow_events[i]);
        REQUIRE(rev_order.counters().unsupported_rejected[i] ==
                in_order.counters().unsupported_rejected[i]);
    }
    REQUIRE(rev_order.counters().snoop_resp_pending_not_consumed ==
            in_order.counters().snoop_resp_pending_not_consumed);
    REQUIRE(rev_order.counters().egress_emitted == in_order.counters().egress_emitted);

    // ── ③ **决定性变异**:证明本判据**有牙**(不是恒真) ──
    //    构造一个「**插入序回退且不比对 txn_id**」的 mutant(等价于 FIFO-pop 无比对):
    //    completion 到达时**弹最早插入的那个**,⛔ 不比对 txn_id。
    //    ⇒ 端态与「恒等」实现**不可区分**(集合都空)⇒ **单独端态断言无鉴别力**;
    //    ⇒ 但**中途探针**下它剩 {t2,t3} ≢ {t1,t3} ⇒ **必红**。
    {
        std::vector<std::uint64_t> fifo; // 插入序
        std::unordered_set<std::uint64_t> live;
        for (std::uint64_t t : {1u, 2u, 3u}) {
            fifo.push_back(t);
            live.insert(t);
        }
        // mutant:pop 最早插入的,⛔ 不比对 txn_id
        if (!fifo.empty()) {
            live.erase(fifo.front());
            fifo.erase(fifo.begin());
        }
        // ⇒ mutant 的中途集合是 {2,3},而恒等实现是 {1,3} ⇒ **不等**
        REQUIRE(live.count(2) == 1u);
        REQUIRE(live.count(3) == 1u);
        REQUIRE(live.count(1) == 0u); // ← mutant 擦错了 txn(=1 而非 =2)
        // ⇒ 断言:若引擎用了「插入序回退」,中途探针的 `count(1)==1` 必红
        //    (本测试的引擎实测 count(1)==1 ⇒ 恒等匹配 ⇒ 非 mutant)
    }
}

TEST_CASE("UGE-04: `snoop_resp` 的 pending **终态**语义 —— 恒不消费 + 零其他效果",
          "[cep][ualink][engine][uge-04]") {
    EventQueue eq;
    Engine e("uge-sr", &eq);

    // 断言前提:引擎**不消费** snoop_resp ⇒ 在途集合**不受影响**
    e.on_request(kAddr, tlm::cep::Perm::NONE, 42);
    const auto before = e.outstanding_request_ids();

    e.on_snoop_resp(kAddr, /*hit=*/true, tlm::cep::LineState::S, /*dirty=*/false, 0, 42);

    // ① 恰好一次 pending 登记
    REQUIRE(e.counters().snoop_resp_pending_not_consumed == 1u);
    REQUIRE(e.counters().flow_events[flow_idx(Flow::SnoopResp)] == 1u);
    // ② 零「拒绝」计数(它是 pending,**不是** rejected)
    REQUIRE(e.counters().unsupported_rejected[flow_idx(Flow::SnoopResp)] == 0u);
    // ③ **恒不消费** ⇒ 对在途集合**零影响**
    REQUIRE(e.outstanding_request_ids() == before);
    REQUIRE(e.outstanding_request_ids().count(42) == 1u);
    // ④ 对 request/completion 记账**零影响**
    REQUIRE(e.counters().flow_events[flow_idx(Flow::Request)] == 1u);
    REQUIRE(e.counters().flow_events[flow_idx(Flow::Completion)] == 0u);
    // ⑤ **零发射**
    REQUIRE(e.counters().egress_emitted == 0u);
}

TEST_CASE("UGE-05: 装配闭环 —— 拓扑挂载 + 装配闸 PASS + `ualink_4` **required** plane 绑定",
          "[cep][ualink][engine][uge-05]") {
    const auto cfg = load_topology("cep_ualink_engine_pe_minimal.json");
    const auto instances = load_cep_json("include/tlm/cep/protocol_instances.json");
    const auto planes = load_cep_json("include/tlm/cep/collective_planes.json");

    // `ualink_4` **required**(per ADR-CEP-12 SU 2026-11-07 Entry #261 的 required 机制)
    const std::vector<std::string> required{"ualink_4"};
    const auto rep = tlm::cep::run_instance_assembly_gate(cfg, instances, planes, required);
    INFO(rep.log);
    REQUIRE(rep.verdict == tlm::cep::InstanceAssemblyVerdict::PASS);
    REQUIRE(rep.mounted.size() == 1u);
    REQUIRE(rep.mounted.front() == "ualink_4");

    // ⚠️ 负控 ①:把**已挂载**的 `ualink_4` 的 plane 绑定**置空** + `required` ⇒ 必红 `[IAG-04]`
    //    (证明 required 机制**未被稀释** —— 空绑定对「非 required」实例是合法状态,
    //     对「required」实例必须判红)
    {
        auto planes_mut = planes;
        planes_mut["plane_binding"]["ualink_4"] = nullptr;
        const std::vector<std::string> required2{"ualink_4"};
        const auto neg =
            tlm::cep::run_instance_assembly_gate(cfg, instances, planes_mut, required2);
        INFO(neg.log);
        REQUIRE(neg.verdict == tlm::cep::InstanceAssemblyVerdict::FAIL);
        // 归因命中:必须是 `[IAG-04]`(空绑定),而非别的归因
        bool hit = false;
        for (const auto& e : neg.errors) {
            if (e.find("[IAG-04]") != std::string::npos)
                hit = true;
        }
        REQUIRE(hit);
    }

    // ⚠️ 负控 ②:把 plane 名改成**未登记**的值 ⇒ 必红 `[IAG-05]`
    {
        auto planes_mut = planes;
        planes_mut["plane_binding"]["ualink_4"] = "NoSuchPlane";
        const std::vector<std::string> required3{"ualink_4"};
        const auto neg =
            tlm::cep::run_instance_assembly_gate(cfg, instances, planes_mut, required3);
        INFO(neg.log);
        REQUIRE(neg.verdict == tlm::cep::InstanceAssemblyVerdict::FAIL);
        bool hit = false;
        for (const auto& e : neg.errors) {
            if (e.find("[IAG-05]") != std::string::npos)
                hit = true;
        }
        REQUIRE(hit);
    }
}

TEST_CASE("UGE-06: **B3b-2 仍 0/4** —— 本 change 不改计数(deferral 登记完整 + 依据档位不升格)",
          "[cep][ualink][engine][uge-06]") {
    // ① 4 族 deferral 登记**仍在**(登记 ≠ 达成;⛔ 不得因引擎落地而移除)
    const auto deferrals = load_cep_json("test/cep_b3b2_deferrals.json");
    REQUIRE(deferrals.at("deferrals").size() == 4u);
    bool ualink_deferred = false;
    for (const auto& row : deferrals.at("deferrals")) {
        if (row.value("family", std::string()) == "ualink") {
            ualink_deferred = true;
            // ⛔ 计数**不因**本 change 改变 ⇒ disposition 仍为 deferred
            REQUIRE(row.value("disposition", std::string()) == "deferred");
        }
    }
    REQUIRE(ualink_deferred);

    // ② `ualink_4` 的声明**逐字未变**:`snoop=false` / `perm=[NONE]` / 8 条 flow false
    const auto instances = load_cep_json("include/tlm/cep/protocol_instances.json");
    for (const auto& inst : instances.at("instances")) {
        if (inst.value("id", std::string()) == "ualink_4") {
            const auto& fs = inst.at("flow_support");
            REQUIRE(fs.at("snoop").get<bool>() == false);
            REQUIRE(fs.at("snoop_resp").get<bool>() == false);
            REQUIRE(fs.at("writeback").get<bool>() == false);
            REQUIRE(fs.at("forward_data").get<bool>() == false);
            REQUIRE(fs.at("back_invalidate").get<bool>() == false);
            REQUIRE(fs.at("address_resolve").get<bool>() == false);
            REQUIRE(fs.at("barrier").get<bool>() == false);
            REQUIRE(fs.at("snoop_completion_ack").get<bool>() == false);
            REQUIRE(fs.at("request").get<bool>() == true);
            REQUIRE(fs.at("completion").get<bool>() == true);
            // `perm` 恒 NONE(软件一致性 ⇒ 无硬件权限授予)
            REQUIRE(inst.at("perm").size() == 1u);
            REQUIRE(inst.at("perm")[0].get<std::string>() == "NONE");
        }
    }

    // ③ 依据档位**不升格**:`ualink` 的来源仍为 `public_nonnormative`(⛔ 非 `spec_clause`)
    //    ⇒ ⛔ 不构成 `Direct` ⇒ ⛔ 不计入 B3b(per `07 号` SU · 四 D-1)。
    const auto sources = load_cep_json("include/tlm/cep/sources.json");
    bool found_nonnormative = false;
    for (const auto& src : sources.at("sources")) {
        if (src.value("id", std::string()) == "ualink-pending") {
            found_nonnormative = true;
            REQUIRE(src.value("kind", std::string()) == "public_nonnormative");
        }
    }
    REQUIRE(found_nonnormative);
}
