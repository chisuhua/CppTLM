// test/test_cep_cohif_json_assembly.cc
// 队列 #7 `cpptlm-cep-cohif-carrier` Phase 5 —— JSON 装配连通性(判据 CC-04 ①②③)
//
// per openspec/changes/cpptlm-cep-cohif-carrier/tasks.md task 5.1 ~ 5.4
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ⚠️ **判据的核心是「非仅绑定成功」** —— 端到端断言必须证明
//    「一次 CohIf 事务**穿过** adapter 抵达对端」,而不是「端口绑上了」。
//    ⇒ 判据形态:pe0 产出 `CohIfBundle` → adapter 搬运 → pe1 的 `req_in` 收到
//    → pe1 的 canonical 流**计数器增加**且**流标识正确**。
//
// ⚠️ **共享实现**:端到端辅助函数 + 结局分类枚举在
//    `test/cep_cohif_assembly_helper.hh`(与 `test_cep_cohif_carrier.cc` 共用)
//    ⇒ 避免两份判据漂移。
#include "catch_amalgamated.hpp"

#include "cep_cohif_assembly_helper.hh"

#include "chstream_register.hh"

#include <nlohmann/json.hpp>

#include <string>

using json = nlohmann::json;
using cpptlm::cep::testing::ReachOutcome;

namespace {

using PE = tlm::cep::ProtocolEngineBase<2>;

}  // namespace

TEST_CASE("CC-04 ① 端到端:一次 CohIf 事务穿过 adapter 抵达对端", "[cep][json-assembly]") {
    const json cfg = cpptlm::cep::testing::load_pe_topology();

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    // 协议桥被消费(注册与消费成对,per CC-05 ②)
    REQUIRE(factory.consumed_bridges().count("cohif_noc") == 1);

    auto* pe0 = dynamic_cast<PE*>(factory.getInstanceForTesting("pe0"));
    auto* pe1 = dynamic_cast<PE*>(factory.getInstanceForTesting("pe1"));
    REQUIRE(pe0 != nullptr);
    REQUIRE(pe1 != nullptr);

    // 桥已挂到两个 PE 上(per CC-03 ③ 的共享)
    REQUIRE(pe0->bridge() != nullptr);
    REQUIRE(pe0->bridge() == pe1->bridge());

    // ── 端到端事务 ──
    pe0->resp_out()[0].write(cpptlm::cep::testing::make_cohif_request());

    pe0->tick();  // 产出 → adapter 搬运
    pe1->tick();  // 接收 → 分派

    auto* engine = dynamic_cast<tlm::cep::NullProtocolEngine<2>*>(pe1);
    REQUIRE(engine != nullptr);
    const auto& ctr = engine->counters();
    REQUIRE(ctr.per_flow[static_cast<std::size_t>(tlm::cep::CanonicalFlow::Request)] == 1u);
    // 其它流**未**被误触发
    REQUIRE(ctr.per_flow[static_cast<std::size_t>(tlm::cep::CanonicalFlow::Snoop)] == 0u);
    // pe0(发起侧)**未**收到自己的事务
    auto* engine0 = dynamic_cast<tlm::cep::NullProtocolEngine<2>*>(pe0);
    REQUIRE(engine0 != nullptr);
    REQUIRE(engine0->counters().per_flow[static_cast<std::size_t>(tlm::cep::CanonicalFlow::Request)] ==
            0u);
}

TEST_CASE("CC-04 ① 事务按 flow_id 被正确分派(非乱序)", "[cep][json-assembly]") {
    const json cfg = cpptlm::cep::testing::load_pe_topology();

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* pe0 = dynamic_cast<PE*>(factory.getInstanceForTesting("pe0"));
    auto* pe1 = dynamic_cast<PE*>(factory.getInstanceForTesting("pe1"));
    REQUIRE(pe0 != nullptr);
    REQUIRE(pe1 != nullptr);

    pe0->resp_out()[0].write(cpptlm::cep::testing::make_cohif_request());

    pe0->tick();
    pe1->tick();

    // ⚠️ `NullProtocolEngine` 只暴露「每流计数」这一可观测点 ⇒ 字段级无损由
    //    Phase 3 的桥契约测试覆盖(`test_cep_protocol_bridge_contracts.cc` 的
    //    pack/unpack 恒等断言)。这里断言「**按 flow_id** 分派」⇒ 其它流计数为 0。
    auto* engine = dynamic_cast<tlm::cep::NullProtocolEngine<2>*>(pe1);
    REQUIRE(engine != nullptr);
    const auto& ctr = engine->counters();
    REQUIRE(ctr.per_flow[static_cast<std::size_t>(tlm::cep::CanonicalFlow::Request)] == 1u);
    REQUIRE(ctr.per_flow[static_cast<std::size_t>(tlm::cep::CanonicalFlow::Completion)] == 0u);
    REQUIRE(ctr.per_flow[static_cast<std::size_t>(tlm::cep::CanonicalFlow::SnoopResp)] == 0u);
    REQUIRE(ctr.per_flow[static_cast<std::size_t>(tlm::cep::CanonicalFlow::Writeback)] == 0u);
}

TEST_CASE("CC-04 ② N ≥ 2(端口数不为 1)", "[cep][json-assembly]") {
    const json cfg = cpptlm::cep::testing::load_pe_topology();
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* pe0 = dynamic_cast<PE*>(factory.getInstanceForTesting("pe0"));
    REQUIRE(pe0 != nullptr);
    REQUIRE(pe0->num_ports() == 2u);
    REQUIRE(ChStreamAdapterFactory::get().getPortCount("CohIfProtocolEngine") == 2u);
}

TEST_CASE("CC-04 ③ CohIf 的 egress 走 `req_out`(本方向的 request 通道)", "[cep][json-assembly]") {
    // ⚠️ **这不是形式断言** —— 该值决定「断开我→对方的连接」**是否**切得断 egress。
    //    实测(Phase 5 对照实验):把 `CohIfBundle` 的 `egress_channel` 改成 `Resp`
    //    ⇒ egress 借用「对方→我」那条连接的响应通道 ⇒ 断链**切不断** ⇒
    //    下方「反向 fixture ②」**假通过**(实测 `3 != 3` 失败)。
    //    ⇒ 该值必须为 `Req`,此处把它钉成具名判据(而非只靠反向 fixture 隐式覆盖)。
    REQUIRE(cpptlm::BundleDirectionTraits<bundles::CohIfBundle>::egress_channel() ==
            cpptlm::EgressChannel::Req);
    // 默认特化(`RouterTLM` 语义)仍为 `Resp` ⇒ 既有行为零变化
    REQUIRE(cpptlm::BundleDirectionTraits<bundles::NoCFlitBundle>::egress_channel() ==
            cpptlm::EgressChannel::Resp);
}

TEST_CASE("CC-04 ③ 反向 fixture:断开端口绑定 ⇒ 端到端断言必红", "[cep][json-assembly]") {
    // 正控:完整连接 ⇒ 事务抵达
    {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(cpptlm::cep::testing::cohif_transaction_reaches_peer(
                    cpptlm::cep::testing::load_pe_topology()) == ReachOutcome::Arrived);
    }
    // 反向 ①:完全无连接 ⇒ 装配**成功**(端口在位)但事务**不**抵达
    //
    // ⚠️ 必须断言 `NotArrived` 而**不是** `!= Arrived` —— 后者会把
    //    「装配被拒」也算作「红」⇒ fixture「因错误的原因红」。
    {
        json broken = cpptlm::cep::testing::load_pe_topology();
        broken["connections"] = json::array();
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(cpptlm::cep::testing::cohif_transaction_reaches_peer(broken) ==
                ReachOutcome::NotArrived);
    }
    // 反向 ②:dst 指向**不存在的端口索引** ⇒ 该绑定断开
    //
    // ⚠️ 端口索引非法 ⇒ ModuleFactory 会**拒绝该连接**(`connection_failed`)
    //    ⇒ 装配层就断了。这条断言记录的是**装配层的断链语义**,
    //    与反向 ①(装配成功但数据通不到)**互补** —— 两条合起来覆盖
    //    「断链」的两种形态:配置层拒绝 / 传输层不通。
    {
        json broken = cpptlm::cep::testing::load_pe_topology();
        broken["connections"][0]["dst"] = "pe1.9";
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(cpptlm::cep::testing::cohif_transaction_reaches_peer(broken) !=
                ReachOutcome::Arrived);
    }
}
