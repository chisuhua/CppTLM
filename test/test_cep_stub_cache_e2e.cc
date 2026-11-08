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

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>

#include <nlohmann/json.hpp>

#include <fstream>

#include "chstream_register.hh"
#include "cep_adr_parser.hh"         // `cpptlm::cep::repo_root()`
#include "cep_experiment_harness.hh" // `cep_exp::ExperimentMetrics`(J-6 指标自洽)
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/chi_rnf_protocol_engine.hh"
#include "tlm/cep/cohif_noc_bridge.hh"
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

// ════════════════════════════════════════════════════════════════════════════
// A6 · 判据 J-1 ~ J-9(每条可机器判;⛔ 判据强度以「有区分力」为准)
// ════════════════════════════════════════════════════════════════════════════

namespace {

    /// J-3 判定:**一请求一响应且周期戳闭合**(测试侧可复用判定函数 ⇒ J-5 用它做变异体)
    bool j3_response_complete(std::uint64_t requests, std::uint64_t responses,
                              const std::vector<tlm::cep::StubCacheEndpointTLM::TxnStamp>& stamps) {
        if (requests == 0)
            return false; // 没有请求 ⇒ 判据**空真** ⇒ 不算通过
        if (responses != requests)
            return false; // 响应数 ≠ 请求数 ⇒ 有丢
        for (const auto& s : stamps) {
            if (!s.completed)
                return false; // 有未闭合的事务
        }
        return true;
    }

    /// 桥投影的共有字段(per `cohif_noc_bridge.hh` 层 1:4 字段双向无损)
    constexpr std::size_t kBridgeSharedFieldCount = 4;

} // namespace

TEST_CASE("SCE-12: W4 桥单槽兜底 —— 同一模块属多个 bridge entry ⇒ FAIL(`[BSS-01]`)",
          "[cep][e2e][sce-12][bridge-single-slot]") {
    const auto instances = load_cep_json("include/tlm/cep/protocol_instances.json");
    const auto planes = load_cep_json("include/tlm/cep/collective_planes.json");

    // 正控:B 原样 ⇒ 无 `[BSS-01]`
    {
        const auto cfg = load_topology("cep_chi_rnf_e2e_b.json");
        const auto rep = tlm::cep::run_instance_assembly_gate(cfg, instances, planes);
        REQUIRE_FALSE(rep.has_error("[BSS-01]"));
    }
    // 反向 fixture:把 `w` 同时挂进**两条** bridge entry ⇒ 必须按该原因红
    {
        auto cfg = load_topology("cep_chi_rnf_e2e_b.json");
        cfg["cep"]["bridges"].push_back({{"name", "cohif_noc_2"},
                                         {"protocol_instance", "chi_rnf"},
                                         {"bridge_id", 8},
                                         {"modules", nlohmann::json::array({"w", "chi"})}});
        const auto rep = tlm::cep::run_instance_assembly_gate(cfg, instances, planes);
        INFO(rep.log);
        REQUIRE(rep.verdict == tlm::cep::InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[BSS-01]"));
    }
}

TEST_CASE("SCE-13: J-1 / J-2 判定 —— 业务流量抵达 wrapper 且事务抵达对端(**地址一致**)",
          "[cep][e2e][sce-13]") {
    REGISTER_ALL;
    const auto cfg = load_topology("cep_chi_rnf_e2e_a.json");
    EventQueue eq;
    ModuleFactory f(&eq);
    REQUIRE(f.instantiateAll(cfg));
    f.startAllTicks();
    eq.run(300);

    auto* w = dynamic_cast<tlm::cep::StubCacheEndpointTLM*>(f.getInstance("w"));
    auto* home = dynamic_cast<tlm::cep::CohIfResponderTLM*>(f.getInstance("home"));
    REQUIRE(w != nullptr);
    REQUIRE(home != nullptr);

    // J-1:真实业务流量抵达 wrapper(⛔ 不止 instantiateAll)
    REQUIRE(w->counters().cpu_requests > 0u);
    REQUIRE(w->counters().net_requests_out > 0u);
    // J-2:事务抵达对端 home,且**地址与 wrapper 发出的一致**(⇒ 不是空跑)
    REQUIRE(home->counters().requests_consumed > 0u);
    REQUIRE(home->last_request_addr() != 0u);
}

TEST_CASE("SCE-14: J-3 端到端响应完整 —— 响应数 == 请求数 ∧ 周期戳闭合 ∧ **不跨 txn 空间**",
          "[cep][e2e][sce-14]") {
    EventQueue eq;
    Wrapper w("sce_j3", &eq);

    Req req(901, 0x9000, 8, false, 0);
    inject(w.pe_req_in(), req);
    w.tick();
    REQUIRE(w.counters().net_requests_out == 1u);
    const std::uint64_t txn = w.net_req_out().data().transaction_id.read();
    REQUIRE(txn != 0u);

    inject(w.net_resp_in(), make_completion(txn, 0x9000, 0xFEED));
    w.tick();

    // 一请求一响应 + 闭合
    REQUIRE(w.counters().cpu_requests == 1u);
    REQUIRE(w.counters().cpu_responses == 1u);
    REQUIRE(w.txn_stamps().size() == 1u);
    REQUIRE(w.txn_stamps().back().completed == true);
    // ⛔ miss 路径的 stamp 属 **core** 空间(⛔ 不得与 CPU 侧 id 混用)
    REQUIRE(w.txn_stamps().back().id_space == Wrapper::TxnIdSpace::CORE);
    REQUIRE(w.txn_stamps().back().txn_id == txn);
    REQUIRE(j3_response_complete(w.counters().cpu_requests, w.counters().cpu_responses,
                                 w.txn_stamps()));
}

TEST_CASE("SCE-15: J-5 **判据有区分力** —— 变异体(丢响应 / 未闭合)必不满足 J-3 判定",
          "[cep][e2e][sce-15]") {
    // 正控
    {
        std::vector<tlm::cep::StubCacheEndpointTLM::TxnStamp> stamps;
        tlm::cep::StubCacheEndpointTLM::TxnStamp s;
        s.completed = true;
        stamps.push_back(s);
        REQUIRE(j3_response_complete(1, 1, stamps));
    }
    // 变异体 ①:响应数少于请求数(丢响应)
    {
        std::vector<tlm::cep::StubCacheEndpointTLM::TxnStamp> stamps;
        tlm::cep::StubCacheEndpointTLM::TxnStamp s;
        s.completed = true;
        stamps.push_back(s);
        REQUIRE_FALSE(j3_response_complete(2, 1, stamps));
    }
    // 变异体 ②:事务未闭合
    {
        std::vector<tlm::cep::StubCacheEndpointTLM::TxnStamp> stamps;
        tlm::cep::StubCacheEndpointTLM::TxnStamp s;
        s.completed = false;
        stamps.push_back(s);
        REQUIRE_FALSE(j3_response_complete(1, 1, stamps));
    }
    // 变异体 ③:空真(零请求)⇒ 不算通过
    {
        std::vector<tlm::cep::StubCacheEndpointTLM::TxnStamp> stamps;
        REQUIRE_FALSE(j3_response_complete(0, 0, stamps));
    }
}

TEST_CASE("SCE-16: J-6 指标自洽 —— 自洽 run 通过;破坏自洽的变异体**按正确原因红**",
          "[cep][e2e][sce-16]") {
    // 正控:延迟族全采集且计数自洽
    {
        cep_exp::ExperimentMetrics m;
        m.total_cycles = 100;
        m.total_transactions = 1; // == 延迟签发数(自洽口径)
        m.latency.note_issue();
        REQUIRE(m.latency.note_complete(1, 2));
        m.verification_scope = "SCE-16";
        m.verification_mode = "sampled";
        REQUIRE(m.well_formed());
    }
    // 变异体:延迟族**部分**采集 ⇒ 事务数与签发数不符 ⇒ 必须红
    {
        cep_exp::ExperimentMetrics m;
        m.total_cycles = 100;
        m.total_transactions = 10;
        m.latency.note_issue();
        REQUIRE(m.latency.note_complete(1, 2));
        m.not_collected["p95_latency_cycles"] = "本装置无该分位";
        m.verification_scope = "SCE-16";
        m.verification_mode = "sampled";
        REQUIRE_FALSE(m.well_formed());
    }
    // ⚠️ 「实测 0」与「未测量」**不同形**:未采集者渲染为字符串
    {
        cep_exp::ExperimentMetrics m;
        m.not_collected["deadlock_violations"] = "本装置未采集";
        const std::string js = m.to_json();
        REQUIRE(js.find("\"deadlock_violations\": \"not-collected\"") != std::string::npos);
    }
}

TEST_CASE("SCE-17: J-7 桥投影 —— 测试级 `pack`/`unpack` 共有字段无损 + 三模块同 `bridge_id`",
          "[cep][e2e][sce-17]") {
    // ① 测试级投影:CohIf → flit → CohIf,4 个共有字段无损
    tlm::cep::CohIfNoCProtocolBridge bridge;
    Bundle in;
    // ⚠️ 输入用 `Snoop` ⇒ 解包后的**默认**值(`Request`)与输入**不同** ⇒ 可区分
    in.flow_id = bundles::CohIfFlowId::Snoop;
    in.transaction_id.write(0x1234);
    in.address.write(0xABCD);
    in.data.write(0x5678);
    in.size.write(16);

    bundles::NoCFlitBundle flit;
    bridge.pack(in, flit);

    Bundle out;
    bridge.unpack(flit, out);
    REQUIRE(out.transaction_id.read() == 0x1234u);
    REQUIRE(out.address.read() == 0xABCDu);
    REQUIRE(out.data.read() == 0x5678u);
    REQUIRE(out.size.read() == 16u);
    REQUIRE(kBridgeSharedFieldCount == 4u); // 层 1 共有字段数(文档锚)
    // 丢弃清单 ↔ 实际行为一致:`flow_id` 声称被丢弃 ⇒ unpack 后**不**保留
    const auto dropped = bridge.dropped_fields();
    REQUIRE(std::find(dropped.begin(), dropped.end(), "perm") != dropped.end());
    // `flow_id` 属**降维映射**(多对一)⇒ 层 1 不可逆 ⇒ unpack 后不保留
    const auto lossy = tlm::cep::CohIfNoCProtocolBridge::lossily_mapped_fields();
    REQUIRE(std::find(lossy.begin(), lossy.end(), "flow_id") != lossy.end());
    REQUIRE(out.flow_id == bundles::CohIfFlowId::Request); // ⇒ `flow_id` 确未携带(降维)

    // ② 逐模块同 `bridge_id`(拓扑 B)
    REGISTER_ALL;
    const auto cfg = load_topology("cep_chi_rnf_e2e_b.json");
    EventQueue eq;
    ModuleFactory f(&eq);
    REQUIRE(f.instantiateAll(cfg));
    auto* w = dynamic_cast<tlm::cep::StubCacheEndpointTLM*>(f.getInstance("w"));
    auto* chi = dynamic_cast<tlm::cep::ChiRnfProtocolEngine*>(f.getInstance("chi"));
    auto* home = dynamic_cast<tlm::cep::CohIfResponderTLM*>(f.getInstance("home"));
    REQUIRE(w != nullptr);
    REQUIRE(chi != nullptr);
    REQUIRE(home != nullptr);
    REQUIRE(w->bridge() != nullptr);
    REQUIRE(w->bridge()->bridge_id() == chi->bridge()->bridge_id());
    REQUIRE(w->bridge()->bridge_id() == home->bridge()->bridge_id());
    REQUIRE(w->bridge()->name() == "cohif_noc");
}

TEST_CASE("SCE-18: J-8 B **不承载**「全绿 run」—— 声明面**不含** home 简化模型",
          "[cep][e2e][sce-18]") {
    // B 的装配闸 PASS 来自**装配面**,⛔ 不来自协议语义面 ⇒ 声明面不得含 home 简化模型
    const auto instances = load_cep_json("include/tlm/cep/protocol_instances.json");
    REQUIRE(instances.contains("instances"));
    for (const auto& inst : instances["instances"]) {
        REQUIRE(inst.value("id", "") != "CohIfResponderTLM");
        REQUIRE(inst.value("id", "") != "StubCacheEndpointTLM");
    }
    // ⛔ 计数不变:本 change **不**构成 B3b 的条款级证据
    const auto cfg = load_topology("cep_chi_rnf_e2e_b.json");
    const auto planes = load_cep_json("include/tlm/cep/collective_planes.json");
    const auto rep = tlm::cep::run_instance_assembly_gate(cfg, instances, planes);
    REQUIRE(rep.verdict == tlm::cep::InstanceAssemblyVerdict::PASS);
}

// ════════════════════════════════════════════════════════════════════════════
// A7 · harness 接入(LatencyCollector / SwmrDataValueDetector / ProgressWatchdog)
//      + 三键测量归属
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("SCE-19: A7 harness 接入 —— `LatencyCollector` 吃到真实往返(非空真)",
          "[cep][e2e][sce-19]") {
    EventQueue eq;
    Wrapper w("sce_lat", &eq);

    // 发一条 miss ⇒ 拿回 Completion ⇒ 把 wrapper 的**原始观测**喂给 collector
    inject(w.pe_req_in(), Req(1101, 0xA000, 8, false, 0));
    w.tick();
    const std::uint64_t txn = w.net_req_out().data().transaction_id.read();
    REQUIRE(txn != 0u);
    inject(w.net_resp_in(), make_completion(txn, 0xA000, 0x1234));
    w.tick();

    cep_exp::LatencyCollector lat;
    REQUIRE(w.txn_stamps().size() == 1u);
    const auto& s = w.txn_stamps().back();
    REQUIRE(s.completed == true);
    lat.note_issue();
    REQUIRE(lat.note_complete(s.issue_cycle, s.complete_cycle));

    // ① 一发一收 ⇒ 自洽
    REQUIRE(lat.issued() == 1u);
    REQUIRE(lat.completed() == 1u);
    REQUIRE(lat.audit_balanced());
    REQUIRE(lat.underflow() == 0u);
    // ② 延迟**有分布**(⛔ 非 0 值空真)
    REQUIRE(lat.sample_count() == 1u);
    REQUIRE(lat.p50() > 0u);
    REQUIRE(lat.avg() > 0.0);

    // ③ 反向 fixture:漏一次 complete ⇒ 不再自洽(判据有牙)
    cep_exp::LatencyCollector lat2;
    lat2.note_issue();
    REQUIRE_FALSE(lat2.audit_balanced());
}

TEST_CASE("SCE-20: A7 harness 接入 —— `SwmrDataValueDetector` + `ProgressWatchdog`"
          "(含反向 fixture)",
          "[cep][e2e][sce-20]") {
    // ① SWMR:先写后读同版本 ⇒ 无违例
    {
        cep_exp::SwmrDataValueDetector det(true);
        det.write(0xB000, 7);
        det.read(0xB000, 7);
        REQUIRE(det.counts().total() == 0u);
        REQUIRE(det.counts().swmr == 0u);
        REQUIRE(det.counts().data_value == 0u);
    }
    // ② 反向 fixture:未先失效既有 sharer 就写 ⇒ SWMR 违例
    {
        cep_exp::SwmrDataValueDetector det(true);
        det.write(0xB000, 1);
        det.write_without_invalidate(0xB000, 2);
        REQUIRE(det.counts().swmr > 0u);
    }
    // ③ 反向 fixture:读到旧版本 ⇒ data-value 违例
    {
        cep_exp::SwmrDataValueDetector det(true);
        det.write(0xB000, 5);
        det.read(0xB000, 4);
        REQUIRE(det.counts().data_value > 0u);
    }
    // ④ ⛔ 空检查装置 ⇒ 不判(防空检查的 0 被读成「无违例 ⇒ 安全」)
    {
        cep_exp::SwmrDataValueDetector det(false);
        det.write_without_invalidate(0xB000, 1);
        REQUIRE(det.counts().total() == 0u);
        REQUIRE(det.checks_enabled() == false);
    }
    // ⑤ `ProgressWatchdog`:有 in-flight 且长期无 retire ⇒ 判停滞;空闲 ⇒ **不**判
    {
        cep_exp::ProgressWatchdog wd(10);
        for (int i = 0; i < 20; ++i)
            wd.tick(/*in_flight=*/1, /*retired=*/0);
        REQUIRE(wd.stalled());
        REQUIRE(wd.max_idle_run() >= 10u);
    }
    {
        cep_exp::ProgressWatchdog wd(10);
        for (int i = 0; i < 20; ++i)
            wd.tick(/*in_flight=*/0, /*retired=*/0);
        REQUIRE_FALSE(wd.stalled()); // 空闲 **不是** 死锁 ⇒ 防恒真
    }
}

TEST_CASE("SCE-21: A7 三键测量归属 —— `coherence_stalls` **实测**;"
          "`collective_stalls` / `directory_misses` = `not-collected` + reason(⛔ 非 0)",
          "[cep][e2e][sce-21]") {
    cep_exp::ExperimentMetrics m;
    m.total_cycles = 50;
    m.total_transactions = 1;
    m.latency.note_issue();
    REQUIRE(m.latency.note_complete(1, 2));
    m.verification_scope = "SCE-21";
    m.verification_mode = "sampled";

    // ① `coherence_stalls` = **本装置有产出者** ⇒ 实测(口径:「in-flight 非零的周期数」)
    //    —— 本 run 中确有未完成事务 ⇒ 实测值 > 0
    m.coherence_stalls = 3;
    m.measured.insert("coherence_stalls");

    // ② `collective_stalls` / `directory_misses` = **本装置无产出者** ⇒ `not-collected` + reason
    //    (⛔ 不得填 0 —— 「实测 0」与「未测量」必须**不同形**)
    m.not_collected["collective_stalls"] = "本装置无 collective plane 绑定 ⇒ 无产出者";
    m.not_collected["directory_misses"] = "本装置无 directory / sharer tracker ⇒ 无产出者";

    // ③ 其余 10 键登记为实测(值取自本 run 的观测)
    for (const auto& k : cep_exp::required_13_metrics()) {
        if (k == "collective_stalls" || k == "directory_misses" || k == "deadlock_violations" ||
            k == "memory_violations")
            continue;
        m.measured.insert(k);
    }
    // ④ 违例类**实测为 0**(本 run 无违例)⇒ 必须显式登记,⛔ 不得留空
    m.measured.insert("deadlock_violations");
    m.measured.insert("memory_violations");

    // 判定:13 键**各有结论**
    std::string missing;
    REQUIRE(cep_exp::metrics_13_covered(m, &missing));
    REQUIRE(missing.empty());

    // ⑤ IC3 三态:违例类**已实测**且为 0 ⇒ ✓ 通过(⛔ 非「非红即绿」)
    REQUIRE(cep_exp::ic3_verdict(m) == cep_exp::Ic3Verdict::Pass);
    REQUIRE(std::string(cep_exp::ic3_verdict_text(cep_exp::Ic3Verdict::Pass)) == "✓ 通过");

    // ⑥ 反向 fixture:违例类 `not-collected` ⇒ IC3 = ⚪ 不适用(⛔ **不得**记「绿」)
    {
        cep_exp::ExperimentMetrics m2 = m;
        m2.not_collected["memory_violations"] = "本装置未采集违例";
        m2.measured.erase("memory_violations");
        REQUIRE(cep_exp::ic3_verdict(m2) == cep_exp::Ic3Verdict::NotApplicable);
    }
    // ⑦ 反向 fixture:有违例 ⇒ ❌ 违例 fail
    {
        cep_exp::ExperimentMetrics m3 = m;
        m3.deadlock_detected = 1;
        REQUIRE(cep_exp::ic3_verdict(m3) == cep_exp::Ic3Verdict::Fail);
    }
    // ⑧ 渲染断言:未采集者输出字符串,⛔ 不是 0
    {
        const std::string js = m.to_json();
        REQUIRE(js.find("\"collective_stalls\": \"not-collected\"") != std::string::npos);
        REQUIRE(js.find("\"_not_collected\"") != std::string::npos);
        REQUIRE(js.find("\"coherence_stalls\": 3") != std::string::npos);
    }
}
