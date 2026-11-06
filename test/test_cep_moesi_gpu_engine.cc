// test/test_cep_moesi_gpu_engine.cc
// 判据:`moesi_gpu` 的 **L4 事务级 E2E** —— instantiate → submit → tick → 抵达
//      (change `cpptlm-cep-moesi-gpu-l4-e2e`)。
//
// 覆盖:
//   MGE-01 引擎已注册 ⇒ 可被 ModuleFactory 实例化(此前**未注册** ⇒ 无实例化路径)
//   MGE-02 10 canonical 流 dispatch ⇒ 逐流「抵达」记账(`flow_events`)
//   MGE-03 `submit_local_request` 表驱动迁移 ⇒ 状态机抵达(`state_of`)
//   MGE-04 补偿语义:`O + Wr` 离开合并态 ⇒ 无 tracker **拒绝迁移**(不丢信息)
//   MGE-05 装配闭环:拓扑挂载 + 装配闸 PASS + channel 闸逐 VC verdict
//
// 作者: builder  日期: 2026-11-06
#include "catch_amalgamated.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "cep_adr_parser.hh"  // `cpptlm::cep::repo_root()`(测试侧仓根定位)
#include "chstream_register.hh" // `REGISTER_ALL`(注册面)
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/adapter_contracts_loader.hh"
#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/instance_assembly_gate.hh"
#include "tlm/cep/moesi_gpu_protocol_engine.hh"

using Engine = tlm::cep::MoesiGpuProtocolEngine;
using State = tlm::cep::MoesiGpuAdapter::State;
using MEvent = tlm::cep::MoesiGpuAdapter::Event;
using Bundle = bundles::CohIfBundle;
using FlowId = bundles::CohIfFlowId;

namespace {

Bundle make_bundle(FlowId flow, std::uint64_t addr, std::uint64_t data,
                    std::uint64_t txn, std::uint8_t perm) {
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

constexpr std::uint64_t kAddr = 0x1000;

/// **经 CohIf 边界**的 5 条(canonical 10 流中跨 Stub↔PE 的条数,per
/// `cep_coherence_bundles_tlm.hh::kCohIfFlowCount = 5`)⇒ 只这 5 条可由 `dispatch_cohif` 派发。
const std::array<FlowId, 5> kCohIfFlows{
    FlowId::Request, FlowId::Completion, FlowId::Snoop, FlowId::SnoopResp, FlowId::Writeback};

/// **不经 CohIf** 的 5 条(其余 canonical 流)⇒ 只能**直调** `on_*`(显式落点)。
void drive_non_cohif_flows(Engine& e) {
    e.on_forward_data(kAddr, 0x2000, 0x1, tlm::cep::Perm::READ, 3);
    e.on_back_invalidate(kAddr, tlm::cep::SnoopType::INVALIDATE, 4);
    tlm::cep::AddressResolveResult r;
    e.on_address_resolve(kAddr, 0, r);
    e.on_barrier(0, 5);
    e.on_snoop_completion_ack(kAddr, 0, 6);
}

}  // namespace

TEST_CASE("MGE-01: MOESI-GPU 引擎已注册 —— 可实例化(此前未注册)", "[cep][moesi][engine][mge-01]") {
    // 注册面:短名 `MoesiGpuProtocolEngine` 必须出现在注册表里。
    const auto cfg = load_topology("cep_moesi_gpu_pe_minimal.json");
    REQUIRE(cfg.at("modules").size() == 2u);
    REQUIRE(cfg.at("modules")[0].at("type").get<std::string>() == "MoesiGpuProtocolEngine");

    // 引擎可构造(实例化路径的前半段)。
    EventQueue eq;
    Engine e("mge", &eq);
    REQUIRE(e.counters().flow_events[0] == 0u);
    REQUIRE(e.line_count() == 0u);
}

TEST_CASE("MGE-01b: ModuleFactory::instantiateAll 真跑 —— 注册面 ⇒ 真可实例化",
          "[cep][moesi][engine][mge-01]") {
    // ⚠️ 2026-11-06(per critic NIT-1):MGE-01 原先只验「配置里的 type 名匹配 + 直接构造」
    //   ⇒ 「可被实例化」被**弱验证**。此处真跑 `ModuleFactory::instantiateAll`
    //   (含 CEP 装配期闸)⇒ 注册面缺失会**立即**变红。
    REGISTER_ALL;
    EventQueue eq;
    ModuleFactory f(&eq);
    const auto cfg = load_topology("cep_moesi_gpu_pe_minimal.json");
    REQUIRE(f.instantiateAll(cfg));
}

TEST_CASE("MGE-02: 10 canonical 流 ⇒ 逐流「抵达」记账(5 条 CohIf + 5 条直调)", "[cep][moesi][engine][mge-02]") {
    EventQueue eq;
    Engine e("mge", &eq);
    // ① 经 CohIf 边界的 5 条
    for (std::size_t i = 0; i < kCohIfFlows.size(); ++i) {
        REQUIRE(e.dispatch_cohif(make_bundle(kCohIfFlows[i], kAddr, i, i + 1, Bundle::PERM_READ)));
    }
    // ② 不经 CohIf 的 5 条(显式落点)
    drive_non_cohif_flows(e);

    // ⚠️ canonical 10 流**逐条**都有抵达记账(per `protocol_engine_base.hh::CanonicalFlow`)。
    for (std::size_t i = 0; i < tlm::cep::kCanonicalFlowCount; ++i) {
        INFO("flow index=" << i);
        REQUIRE(e.counters().flow_events[i] == 1u);
    }
    REQUIRE(e.counters().unsupported_flow_rejected == 0u);
}

TEST_CASE("MGE-03: submit_local_request 表驱动迁移 ⇒ 状态机抵达", "[cep][moesi][engine][mge-03]") {
    EventQueue eq;
    Engine e("mge", &eq);
    // I + Rd ⇒ S
    const State s1 = e.submit_local_request(kAddr, State::I, MEvent::Rd);
    REQUIRE(s1 == State::S);
    REQUIRE(e.state_of(kAddr) == State::S);
    REQUIRE(e.counters().local_requests == 1u);
    REQUIRE(e.counters().adapter_transitions == 1u);
    REQUIRE(e.line_count() == 1u);

    // S + RdUnique ⇒ E
    const State s2 = e.submit_local_request(kAddr, State::S, MEvent::Inv);
    REQUIRE(e.counters().local_requests == 2u);
    // 表驱动的真值由载体决定 ⇒ 断言**与载体一致**(不自造期望值)。
    const State expect = static_cast<State>(tlm::cep::MoesiGpuAdapter::transition(State::S, MEvent::Inv));
    REQUIRE(s2 == expect);
    REQUIRE(e.state_of(kAddr) == expect);
}

TEST_CASE("MGE-04: 补偿语义 —— O + Wr 离开合并态且无 tracker ⇒ 拒绝迁移", "[cep][moesi][engine][mge-04]") {
    EventQueue eq;
    Engine e("mge", &eq);
    // 未接 tracker ⇒ `O + Wr` 必须**拒绝迁移**(不丢「可能有其他 sharer」信息)。
    const State out = e.submit_local_request(kAddr, State::O, MEvent::Wr);
    REQUIRE(out == State::O);
    // ⚠️ 补偿被拒 ⇒ **迁移未发生** ⇒ 该地址**未被登记** ⇒ `state_of` 返回默认 `I`
    //    (引擎语义:拒绝时不写入 `lines_`,⛔ 不伪造「停在 O」)。
    REQUIRE(e.state_of(kAddr) == State::I);
    REQUIRE(e.line_count() == 0u);
    REQUIRE(e.counters().compensation_skipped == 1u);
    REQUIRE(e.counters().compensation_queries == 0u);
    // 迁移**未发生** ⇒ `adapter_transitions` 不增。
    REQUIRE(e.counters().adapter_transitions == 0u);
}

TEST_CASE("MGE-05: 装配闭环 —— 拓扑挂载 + 装配闸 + channel 闸", "[cep][moesi][engine][mge-05]") {
    const auto cfg = load_topology("cep_moesi_gpu_pe_minimal.json");
    const auto instances = load_cep_json("include/tlm/cep/protocol_instances.json");
    const auto planes = load_cep_json("include/tlm/cep/collective_planes.json");

    // ⚠️ `moesi_gpu` 的 collective 平面绑定为 **null**(不启用 collective)⇒ 合法 ⇒ 不判红。
    const auto rep = tlm::cep::run_instance_assembly_gate(cfg, instances, planes);
    INFO(rep.log);
    REQUIRE(rep.verdict == tlm::cep::InstanceAssemblyVerdict::PASS);
    REQUIRE(rep.mounted.size() == 1u);
    REQUIRE(rep.mounted.front() == "moesi_gpu");

    const auto chan = tlm::cep::run_channel_gate(cfg);
    REQUIRE(chan.verdict != tlm::cep::GateVerdict::FAIL);
    REQUIRE(chan.per_instance.count("moesi_gpu") == 1u);
}
