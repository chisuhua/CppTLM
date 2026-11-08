// test/test_cep_stub_cache_e2e.cc
// 判据:CEP 端到端可测框架的**端口面**(change `cpptlm-cep-stub-cache-chstream-surface`)
//   SCE-01 `StubCacheEndpointTLM` 可构造 · **两侧 Bundle 异构**(编译期)· 纯类零改动的用法不变
//   SCE-02 **命中路径**:回 CPU 侧响应 ∧ `net_side` **零请求**(`INV-SC6` 的 wrapper 等价断言)
//   SCE-03 **miss 路径**:发 CohIf `Request` ⇒ 喂 `Completion` ⇒ 回 CPU 响应 + 周期戳闭合
//   SCE-04 `CohIfResponderTLM`(**home 简化模型**):`Request` ⇒ **参数化延迟**后 `Completion`;
//          `Writeback` **接收即消费、零响应**;⛔ 不承载条款级语义(负向断言)
//   SCE-05 注册面:两类型均 `registerObject` 过(`ModuleFactory` 可构造)
//
// ⚠️ 强度标签:本 change **只**提升**可实验性**;⛔ 不构成 `B3b-2` 的解除证据(仍 **0/4**)。
//
// 作者: builder  日期: 2026-11-08
#include "catch_amalgamated.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

#include <nlohmann/json.hpp>

#include <fstream>

#include "chstream_register.hh"
#include "cep_adr_parser.hh" // `cpptlm::cep::repo_root()`
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/chi_rnf_protocol_engine.hh"
#include "tlm/cep/cohif_responder.hh"
#include "tlm/cep/instance_assembly_gate.hh"
#include "tlm/cep/stub_cache_endpoint.hh"

using Wrapper = tlm::cep::StubCacheEndpointTLM;
using Responder = tlm::cep::CohIfResponderTLM;
using Req = bundles::CacheReqBundle;
using Resp = bundles::CacheRespBundle;
using Bundle = bundles::CohIfBundle;

namespace {

    /// 测试注入:把 bundle 推进**输入**适配器(`InputStreamAdapter` 无 `write`,用 data()+set_valid)
    template <typename InputAdapterT, typename BundleT>
    void inject(InputAdapterT& in, const BundleT& b) {
        in.data() = b;
        in.set_valid(true);
    }

    Bundle make_request(std::uint64_t txn, std::uint64_t addr, std::uint64_t data) {
        Bundle b;
        b.flow_id = bundles::CohIfFlowId::Request;
        b.transaction_id.write(txn);
        b.address.write(addr);
        b.data.write(data);
        b.perm.write(static_cast<std::uint8_t>(tlm::cep::Perm::READ));
        return b;
    }

    Bundle make_completion(std::uint64_t txn, std::uint64_t addr, std::uint64_t data) {
        Bundle b;
        b.flow_id = bundles::CohIfFlowId::Completion;
        b.transaction_id.write(txn);
        b.address.write(addr);
        b.data.write(data);
        b.perm.write(static_cast<std::uint8_t>(tlm::cep::Perm::READ));
        return b;
    }

    Bundle make_writeback(std::uint64_t txn, std::uint64_t addr) {
        Bundle b;
        b.flow_id = bundles::CohIfFlowId::Writeback;
        b.transaction_id.write(txn);
        b.address.write(addr);
        b.dirty.write(1);
        return b;
    }

    /// 装配配置加载(路径按**仓根**解析 ⇒ 与 CWD 无关)
    nlohmann::json load_topology(const char* file) {
        const auto path = cpptlm::cep::repo_root() / "configs" / file;
        std::ifstream in(path);
        REQUIRE(in.good());
        nlohmann::json cfg = nlohmann::json::parse(in);
        // ⚠️ `cep.protocol_instances` 是**相对路径** ⇒ 装配期按 CWD 解析 ⇒ 改为绝对路径
        //    (与 `test_cep_ualink_engine.cc` 的既有做法一致)。
        cfg["cep"]["protocol_instances"] =
            (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json")
                .string();
        return cfg;
    }

    nlohmann::json load_cep_json(const char* rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        REQUIRE(in.good());
        return nlohmann::json::parse(in);
    }

} // namespace

TEST_CASE("SCE-01: 端口面 wrapper 可构造且两侧 Bundle **异构**", "[cep][e2e][sce-01]") {
    // ── 编译期:两侧类型确实不同(⛔ 不得退化为对称单 BundleT)──
    static_assert(!std::is_same_v<decltype(std::declval<Wrapper&>().pe_req_in().data()), Bundle&>,
                  "cpu 侧请求 Bundle 必须与 CohIf 侧不同类型");
    static_assert(std::is_same_v<decltype(std::declval<Wrapper&>().net_resp_in().data()), Bundle&>,
                  "net 侧 ingress 必须是 CohIfBundle");

    EventQueue eq;
    Wrapper w("sce_wrapper", &eq);
    REQUIRE(w.get_module_type() == "StubCacheEndpointTLM");
    REQUIRE(w.num_ports() == 4u);
    REQUIRE(w.bridge() == nullptr);
    REQUIRE(w.in_flight() == 0u);

    // 纯类用法**不变**:`StubCacheCore` 仍是可独立构造的功能对象
    tlm::cep::StubCacheCore core;
    const auto r = core.cpu_read(0x1000);
    REQUIRE(r.hit == false);
    REQUIRE(core.outstanding_count() == 1u);
}

TEST_CASE("SCE-02: 命中路径 ⇒ 回 CPU 响应 ∧ net_side **零请求**", "[cep][e2e][sce-02]") {
    EventQueue eq;
    Wrapper w("sce_hit", &eq);

    // 预置一行(S/W:命中;`cpu_read` 命中 ⇒ 不再发 request)
    w.core().cpu_read(0x2000);
    // 用 completion 把该行填成 S(命中态)
    const std::uint64_t txn = w.core().outstanding_txn(0x2000);
    REQUIRE(txn != 0u);
    REQUIRE(w.core().completion(0x2000, txn, 0xDEAD, tlm::cep::Perm::READ, false));
    REQUIRE(w.core().has_line(0x2000));

    const std::uint64_t net_before = w.counters().net_requests_out;

    // 业务侧发一个读请求 → 命中
    Req req(77, 0x2000, 8, /*is_write=*/false, 0);
    inject(w.pe_req_in(), req);
    w.tick();

    REQUIRE(w.counters().cpu_requests == 1u);
    REQUIRE(w.counters().net_requests_out == net_before); // `INV-SC6`:hit ⇒ 零网络流量
    REQUIRE(w.pe_resp_out().valid());
    const Resp resp = w.pe_resp_out().data();
    REQUIRE(resp.is_hit.read() == true);
    REQUIRE(resp.transaction_id.read() == 77u);

    // 原始观测事实:命中 ⇒ issue 当期完成
    REQUIRE(w.txn_stamps().size() == 1u);
    REQUIRE(w.txn_stamps().back().hit == true);
    REQUIRE(w.txn_stamps().back().completed == true);
    REQUIRE(w.txn_stamps().back().issue_cycle == w.txn_stamps().back().complete_cycle);
}

TEST_CASE("SCE-03: miss 路径 ⇒ 发 CohIf `Request`;喂 `Completion` ⇒ 回 CPU 响应 + 周期戳闭合",
          "[cep][e2e][sce-03]") {
    EventQueue eq;
    Wrapper w("sce_miss", &eq);

    Req req(101, 0x3000, 8, false, 0);
    inject(w.pe_req_in(), req);
    w.tick();

    // miss ⇒ net_side 发 Request(⛔ 不发 cpu 响应)
    REQUIRE(w.counters().net_requests_out == 1u);
    REQUIRE(w.net_req_out().valid());
    const Bundle out = w.net_req_out().data();
    REQUIRE(out.flow_id == bundles::CohIfFlowId::Request);
    REQUIRE(out.address.read() == 0x3000u);
    const std::uint64_t txn = out.transaction_id.read();
    REQUIRE(txn != 0u);
    REQUIRE_FALSE(w.pe_resp_out().valid());
    REQUIRE(w.in_flight() == 1u);

    // 喂 Completion ⇒ 回 CPU 响应 + 在途归零 + 周期戳闭合
    inject(w.net_resp_in(), make_completion(txn, 0x3000, 0xBEEF));
    w.tick();
    REQUIRE(w.counters().net_completions_in == 1u);
    REQUIRE(w.pe_resp_out().valid());
    REQUIRE(w.pe_resp_out().data().data.read() == 0xBEEFu);
    REQUIRE(w.in_flight() == 0u);
    REQUIRE(w.txn_stamps().back().completed == true);
    REQUIRE(w.txn_stamps().back().complete_cycle >= w.txn_stamps().back().issue_cycle);
}

TEST_CASE("SCE-04: home 简化模型 ⇒ 参数化延迟回 Completion;Writeback 零响应",
          "[cep][e2e][sce-04]") {
    EventQueue eq;
    Responder r("sce_home", &eq);
    Responder::Config cfg;
    cfg.completion_latency_cycles = 3;
    r.configure(cfg);

    inject(r.req_in()[0], make_request(7, 0x4000, 0));
    r.tick(); // 第 1 拍:消费 Request ⇒ 排入延迟队列
    REQUIRE(r.counters().requests_consumed == 1u);
    REQUIRE(r.in_flight() == 1u);
    r.tick(); // 第 2 拍:未到延迟
    REQUIRE_FALSE(r.resp_out()[0].valid());
    r.tick(); // 第 3 拍:仍未到(延迟 3 ⇒ 第 4 拍发)
    r.tick(); // 第 4 拍:应发出
    REQUIRE(r.resp_out().size() > 0);
    REQUIRE(r.counters().completions_sent == 1u);
    // `Completion` **回到请求的 ingress 端口**(⛔ 不按 txn 哈希选端口)
    REQUIRE(r.resp_out()[0].valid());
    REQUIRE_FALSE(r.resp_out()[1].valid());
    const Bundle comp = r.resp_out()[0].data();
    REQUIRE(comp.flow_id == bundles::CohIfFlowId::Completion);
    REQUIRE(comp.transaction_id.read() == 7u);

    // `Writeback` ⇒ **接收即消费、零响应**
    const std::uint64_t sent_before = r.counters().completions_sent;
    inject(r.req_in()[0], make_writeback(8, 0x4000));
    r.tick();
    REQUIRE(r.counters().writebacks_consumed == 1u);
    REQUIRE(r.counters().completions_sent == sent_before); // ⛔ 不为 writeback 回响应

    // ⛔ 不承载条款级语义(负向):`Snoop` 默认**不**发出
    REQUIRE(r.config().emit_snoops == false);
    REQUIRE(r.counters().snoops_sent == 0u);
}

TEST_CASE("SCE-05: 两类型均 `registerObject` 过(ModuleFactory 可实例化)", "[cep][e2e][sce-05]") {
    REGISTER_ALL;
    nlohmann::json cfg = {
        {"modules",
         {{{"type", "StubCacheEndpointTLM"}, {"name", "w1"}},
          {{"type", "CohIfResponderTLM"}, {"name", "r1"}}}},
        {"connections", nlohmann::json::array()},
    };
    EventQueue eq;
    ModuleFactory f(&eq);
    REQUIRE(f.instantiateAll(cfg)); // 注册面缺失 ⇒ 立即变红
}

TEST_CASE("SCE-06: 多端点 ⇒ Completion 各回各的 ingress 端口(⛔ 串端口)", "[cep][e2e][sce-06]") {
    EventQueue eq;
    Responder r("sce_multi", &eq);
    Responder::Config cfg;
    cfg.completion_latency_cycles = 1;
    r.configure(cfg);

    inject(r.req_in()[0], make_request(1, 0x6000, 0));
    inject(r.req_in()[1], make_request(2, 0x7000, 0));
    r.tick(); // 消费两条;ready = 1 + 1 = 2
    REQUIRE(r.counters().requests_consumed == 2u);
    REQUIRE(r.in_flight() == 2u);
    r.tick(); // 第 2 拍 ⇒ 两个端口各发一条

    REQUIRE(r.resp_out()[0].valid());
    REQUIRE(r.resp_out()[1].valid());
    REQUIRE(r.resp_out()[0].data().transaction_id.read() == 1u);
    REQUIRE(r.resp_out()[1].data().transaction_id.read() == 2u);
    REQUIRE(r.counters().completions_sent == 2u);
}

TEST_CASE("SCE-07: CPU 侧背压 ⇒ 响应**排队不丢**,enqueued 与 delivered 分账",
          "[cep][e2e][sce-07]") {
    EventQueue eq;
    Wrapper w("sce_bp", &eq);

    // 预置命中行
    w.core().cpu_read(0x2100);
    const std::uint64_t t = w.core().outstanding_txn(0x2100);
    REQUIRE(t != 0u);
    REQUIRE(w.core().completion(0x2100, t, 0xAA, tlm::cep::Perm::READ, false));

    inject(w.pe_req_in(), Req(201, 0x2100, 8, false, 0));
    w.tick();
    REQUIRE(w.counters().cpu_responses_enqueued == 1u);
    REQUIRE(w.counters().cpu_responses == 1u);
    REQUIRE(w.cpu_resp_backlog() == 0u);

    // 第二条:下游未消费 ⇒ 只能入队(⛔ 静默丢弃)
    inject(w.pe_req_in(), Req(202, 0x2100, 8, false, 0));
    w.tick();
    REQUIRE(w.counters().cpu_responses_enqueued == 2u);
    REQUIRE(w.counters().cpu_responses == 1u); // 仍是第 1 条在出口
    REQUIRE(w.cpu_resp_backlog() == 1u);
    REQUIRE(w.pe_resp_out().data().transaction_id.read() == 201u);

    // 下游消费 ⇒ 下一拍交付第 2 条
    w.pe_resp_out().clear_valid(); // 模拟下游消费
    w.tick();
    REQUIRE(w.counters().cpu_responses == 2u);
    REQUIRE(w.cpu_resp_backlog() == 0u);
    REQUIRE(w.pe_resp_out().valid());
    REQUIRE(w.pe_resp_out().data().transaction_id.read() == 202u);
}

TEST_CASE("SCE-08: core 拒绝 ⇒ ⛔ 不伪造 net Request,回显式 reject 响应", "[cep][e2e][sce-08]") {
    EventQueue eq;
    Wrapper w("sce_reject", &eq);

    inject(w.pe_req_in(), Req(301, 0x8000, 8, false, 0));
    w.tick();
    REQUIRE(w.counters().net_requests_out == 1u);
    REQUIRE(w.in_flight() == 1u);

    // 同 addr 再发 ⇒ core 拒绝(`INV-SC1`);⛔ 不得发 `transaction_id=0` 的 Request
    inject(w.pe_req_in(), Req(302, 0x8000, 8, false, 0));
    w.tick();
    REQUIRE(w.counters().cpu_rejected == 1u);
    REQUIRE(w.counters().net_requests_out == 1u); // 未新增
    REQUIRE(w.pe_resp_out().valid());
    const Resp resp = w.pe_resp_out().data();
    REQUIRE(resp.transaction_id.read() == 302u);
    REQUIRE(resp.is_hit.read() == false);
    REQUIRE(resp.error_code.read() == 1u);
    REQUIRE(w.txn_stamps().back().rejected == true);
}

TEST_CASE("SCE-09: 拓扑 A 装配 + **出向事务腿**(⛔ 判据不止 instantiateAll)",
          "[cep][e2e][sce-09]") {
    REGISTER_ALL;
    const auto cfg = load_topology("cep_chi_rnf_e2e_a.json");
    EventQueue eq;
    ModuleFactory f(&eq);
    REQUIRE(f.instantiateAll(cfg)); // 装配面
    f.startAllTicks();
    eq.run(300);

    auto* w = dynamic_cast<tlm::cep::StubCacheEndpointTLM*>(f.getInstance("w"));
    auto* home = dynamic_cast<tlm::cep::CohIfResponderTLM*>(f.getInstance("home"));
    REQUIRE(w != nullptr);
    REQUIRE(home != nullptr);

    // ── 行为判据(per A3.2:`instantiateAll==true` **不**算装配判据)──
    REQUIRE(w->counters().cpu_requests > 0u);         // 真实业务流量抵达 wrapper
    REQUIRE(w->counters().net_requests_out > 0u);     // wrapper 发出 net Request
    REQUIRE(home->counters().requests_consumed > 0u); // 事务抵达对端 home

    // ⚠️ 拓扑内**回程被抑制**(dual-port 适配器入向路由恒落 PE 侧 ⇒ 框架限制;
    //    `include/framework/` 红线 ⇒ 零改动;回程腿在**模块级** SCE-03 / SCE-04 闭合)。
    REQUIRE(home->counters().completions_sent == 0u);
    REQUIRE(home->counters().completions_suppressed > 0u);
}

TEST_CASE("SCE-10: 拓扑 B 装配 + 桥挂载(三模块同 `bridge_id`)+ 带延迟事务仍抵达对端",
          "[cep][e2e][sce-10]") {
    REGISTER_ALL;
    const auto cfg = load_topology("cep_chi_rnf_e2e_b.json");
    EventQueue eq;
    ModuleFactory f(&eq);
    REQUIRE(f.instantiateAll(cfg));
    f.startAllTicks();

    auto* w = dynamic_cast<tlm::cep::StubCacheEndpointTLM*>(f.getInstance("w"));
    auto* chi = dynamic_cast<tlm::cep::ChiRnfProtocolEngine*>(f.getInstance("chi"));
    auto* home = dynamic_cast<tlm::cep::CohIfResponderTLM*>(f.getInstance("home"));
    REQUIRE(w != nullptr);
    REQUIRE(chi != nullptr);
    REQUIRE(home != nullptr);

    // ── 桥挂载面:三个端点**共享同一** bridge(同一 `bridge_id`)──
    REQUIRE(w->bridge() != nullptr);
    REQUIRE(chi->bridge() != nullptr);
    REQUIRE(home->bridge() != nullptr);
    REQUIRE(w->bridge()->bridge_id() == chi->bridge()->bridge_id());
    REQUIRE(w->bridge()->bridge_id() == home->bridge()->bridge_id());
    REQUIRE(w->bridge()->bridge_id() == 7u);

    // ── 带连接延迟时,事务**仍抵达对端** ──
    bundles::CacheReqBundle req(1, 0x3000, 8, false, 0);
    w->pe_req_in().data() = req;
    w->pe_req_in().set_valid(true);
    eq.run(200);
    REQUIRE(w->counters().net_requests_out >= 1u);
    REQUIRE(home->counters().requests_consumed >= 1u);
}

TEST_CASE("SCE-11: J-9 拓扑分档(A ⇒ 闸 SKIP ∧ 无 `cep.bridges` ∧ 无 latency>0;"
          "B ⇒ 闸 PASS ∧ 1 entry / 3 modules / ≥1 latency>0)",
          "[cep][e2e][sce-11]") {
    const auto cfg_a = load_topology("cep_chi_rnf_e2e_a.json");
    const auto cfg_b = load_topology("cep_chi_rnf_e2e_b.json");
    const auto instances = load_cep_json("include/tlm/cep/protocol_instances.json");
    const auto planes = load_cep_json("include/tlm/cep/collective_planes.json");

    // ── A 档 ──
    REQUIRE_FALSE(cfg_a["cep"].contains("bridges"));
    const auto rep_a = tlm::cep::run_instance_assembly_gate(cfg_a, instances, planes);
    REQUIRE(rep_a.verdict == tlm::cep::InstanceAssemblyVerdict::SKIP);
    for (const auto& c : cfg_a["connections"]) {
        REQUIRE(c.value("latency", 0) == 0);
    }

    // ── B 档 ──
    REQUIRE(cfg_b["cep"]["bridges"].size() == 1u);
    REQUIRE(cfg_b["modules"].size() == 3u);
    int positive_latency = 0;
    for (const auto& c : cfg_b["connections"]) {
        if (c.value("latency", 0) > 0)
            ++positive_latency;
    }
    REQUIRE(positive_latency >= 1);
    const auto rep_b = tlm::cep::run_instance_assembly_gate(cfg_b, instances, planes);
    INFO(rep_b.log);
    REQUIRE(rep_b.verdict == tlm::cep::InstanceAssemblyVerdict::PASS);
    REQUIRE(rep_b.errors.empty());
}
