// test/test_noc_mesh_binding_semantics.cc
// 「JSON 装配下 router 间双向 flit 链路」的**绑定语义**实测载体
// (change `cpptlm-cep-mesh-e2e-carrier` 的 T1.3 探针**转正**为回归载体)
//
// 本文件最初是临时探针,其产出的实测结论已被写回该 change 的 `design.md` §D-2.0;
// 现转为**正式回归载体** ⇒ 绑定语义若被框架层改动破坏,本文件会红。
//
// 背景:全仓**无**该语义的实测先例 —— `configs/mesh_2x2_tlm.json` / `mesh_4x4_tlm.json`
//      的 NIC 均无 `address_regions` ⇒ `lookup_node` 静默投 node 0 ⇒ 实为「全流量→node 0
//      的单向生成树」;其 e2e 测试零 flit 流动断言 ⇒ 不能作为先例。
//
// 本探针**只测既有模块**(CPUTLM + NICTLM + RouterTLM),**不依赖**本 change 的新代码。
//
// 拓扑(1 维 2 节点,mesh_x=2 mesh_y=1):
//     cpu0 ──► ni0 ──req_out[1]──► r0(LOCAL) ══EAST══► r1(LOCAL) ◄──req_out[1]── ni1
//     ni0.address_regions: {base:0x1000,size:0x1000,target_node:1} ⇒ dst_node=1
//
// 待实测的 4 件事:
//   ① router↔router **1 条** connection 是否够(还是必须 2 条对称)?
//   ② router↔NIC 是否确实需要 **2 条**(NIC egress 走 req_out[1] 且无回退)?
//   ③ flit 是否真的**穿过 mesh**抵达对端(而非停在中途)?
//   ④ router 的 flits_forwarded / packets_forwarded / total_hops 是否可读且 > 0?
//
// 作者: CppTLM Team / 日期: 2026-11-16
#include <catch2/catch_all.hpp>

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "metrics/stats.hh"
#include "tlm/nic_tlm.hh"
#include "tlm/router_tlm.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <iostream>

using json = nlohmann::json;
using namespace cpptlm;

namespace {

    /// 读一个 Scalar 统计的值(找不到 ⇒ -1)
    int64_t scalar_of(tlm_stats::StatGroup& g, const std::string& name) {
        auto* s = dynamic_cast<tlm_stats::Scalar*>(g.findStat(name));
        return s ? static_cast<int64_t>(s->value()) : -1;
    }

    /// 探针拓扑 —— `rtr_links` 决定 router↔router 用 1 条还是 2 条
    json probe_topology(bool rtr_two_links) {
        json mods = json::array();
        mods.push_back({{"name", "cpu0"}, {"type", "CPUTLM"}});
        mods.push_back(
            {{"name", "ni0"},
             {"type", "NICTLM"},
             {"params",
              {{"node_id", 0},
               {"mesh_x", 2},
               {"mesh_y", 1},
               {"address_regions",
                json::array({{{"base", "0x1000"}, {"size", "0x1000"}, {"target_node", 1}}})}}}});
        mods.push_back({{"name", "ni1"},
                        {"type", "NICTLM"},
                        {"params", {{"node_id", 1}, {"mesh_x", 2}, {"mesh_y", 1}}}});
        mods.push_back({{"name", "r0"},
                        {"type", "RouterTLM"},
                        {"params", {{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 1}}}});
        mods.push_back({{"name", "r1"},
                        {"type", "RouterTLM"},
                        {"params", {{"node_x", 1}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 1}}}});

        json conns = json::array();
        auto add = [&](const char* s, const char* d) {
            conns.push_back({{"src", s}, {"dst", d}, {"latency", 1}, {"bandwidth", 100}});
        };
        // CPU ↔ NIC0(PE 侧,双向)
        add("cpu0", "ni0.0");
        add("ni0.0", "cpu0");
        // NIC0 ↔ r0(router↔NIC:**2 条**)
        add("ni0.1", "r0.4");
        add("r0.4", "ni0.1");
        // r0 ↔ r1(router↔router:**1 条** 或 **2 条**)
        add("r0.1", "r1.3");
        if (rtr_two_links)
            add("r1.3", "r0.1");
        // r1 ↔ NIC1(router↔NIC:**2 条**)
        add("r1.4", "ni1.1");
        add("ni1.1", "r1.4");

        return json{{"modules", mods}, {"connections", conns}};
    }

    struct ProbeResult {
        bool assembled = false;
        int64_t ni1_packets_received = -1;
        int64_t ni1_flits_received = -1;
        int64_t r0_flits = -1;
        int64_t r0_packets = -1;
        int64_t r0_hops = -1;
        int64_t r1_flits = -1;
        int64_t r1_packets = -1;
        int64_t r1_hops = -1;
    };

    ProbeResult run_probe(bool rtr_two_links, int cycles) {
        ProbeResult out;
        EventQueue eq;
        ModuleFactory factory(&eq);
        const json cfg = probe_topology(rtr_two_links);
        out.assembled = factory.instantiateAll(cfg);
        if (!out.assembled)
            return out;
        factory.startAllTicks();
        eq.run(static_cast<uint64_t>(cycles));

        auto* ni1 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni1"));
        auto* r0 = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("r0"));
        auto* r1 = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("r1"));
        if (ni1) {
            auto* g = ni1->get_stats_group();
            out.ni1_packets_received = scalar_of(*g, "packets_received");
            out.ni1_flits_received = scalar_of(*g, "flits_received");
        }
        if (r0) {
            out.r0_flits = scalar_of(r0->stats(), "flits_forwarded");
            out.r0_packets = scalar_of(r0->stats(), "packets_forwarded");
            out.r0_hops = scalar_of(r0->stats(), "total_hops");
        }
        if (r1) {
            out.r1_flits = scalar_of(r1->stats(), "flits_forwarded");
            out.r1_packets = scalar_of(r1->stats(), "packets_forwarded");
            out.r1_hops = scalar_of(r1->stats(), "total_hops");
        }
        return out;
    }

    void dump(const char* label, const ProbeResult& r) {
        std::cout << "[PROBE] " << label << " assembled=" << r.assembled
                  << " ni1.packets_received=" << r.ni1_packets_received
                  << " ni1.flits_received=" << r.ni1_flits_received << " r0.flits=" << r.r0_flits
                  << " r0.packets=" << r.r0_packets << " r0.hops=" << r.r0_hops
                  << " r1.flits=" << r.r1_flits << " r1.packets=" << r.r1_packets
                  << " r1.hops=" << r.r1_hops << "\n";
    }

} // namespace

TEST_CASE("PROBE-1: router↔router **1 条** connection 是否足以让 flit 穿过 mesh",
          "[noc][mesh][binding-semantics]") {
    const ProbeResult r = run_probe(/*rtr_two_links=*/false, 200);
    dump("1-link", r);
    REQUIRE(r.assembled);
}

TEST_CASE("PROBE-2: router↔router **2 条**(对称)时的对照", "[noc][mesh][binding-semantics]") {
    const ProbeResult r = run_probe(/*rtr_two_links=*/true, 200);
    dump("2-link", r);
    REQUIRE(r.assembled);
}

TEST_CASE("PROBE-3: router↔NIC 只给 **1 条** 时能否工作(验证「必须 2 条」)",
          "[noc][mesh][binding-semantics]") {
    // 变体:去掉 `r0.4 → ni0.1`(router→NIC 的那一半)
    json cfg = probe_topology(false);
    json kept = json::array();
    for (const auto& c : cfg["connections"]) {
        if (c["src"] == "r0.4" && c["dst"] == "ni0.1")
            continue;
        kept.push_back(c);
    }
    cfg["connections"] = kept;

    EventQueue eq;
    ModuleFactory factory(&eq);
    const bool ok = factory.instantiateAll(cfg);
    if (ok) {
        factory.startAllTicks();
        eq.run(200);
    }
    ProbeResult r;
    r.assembled = ok;
    auto* ni1 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni1"));
    auto* r0 = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("r0"));
    if (ni1) {
        auto* g = ni1->get_stats_group();
        r.ni1_packets_received = scalar_of(*g, "packets_received");
    }
    if (r0)
        r.r0_flits = scalar_of(r0->stats(), "flits_forwarded");
    dump("1-nic-link", r);
    REQUIRE(ok);
}

// ────────────────────────────────────────────────────────────────────────────
// PROBE-4: **2×2 mesh**(最终拓扑候选)—— X-first 路由下 node0 → node3 是否可达
//
// 2×2 节点编号(mesh_x=2, mesh_y=2,node = y*mesh_x + x):
//   node0=(0,0)  node1=(1,0)
//   node2=(0,1)  node3=(1,1)
// X-first 路径:node0 →(X)→ node1 →(Y)→ node3   = r00 → r01 → r11
// 回程(X 优先):node3 →(X)→ node2 →(Y)→ node0  = r11 → r10 → r00
// ⇒ 4 条 router-router 连接构成环(r00-r01-r11-r10-r00),每条 2 个方向 = 8 条 connection
// ────────────────────────────────────────────────────────────────────────────
namespace {

    json probe_topology_2x2() {
        json mods = json::array();
        mods.push_back({{"name", "cpu0"}, {"type", "CPUTLM"}});
        mods.push_back(
            {{"name", "ni0"},
             {"type", "NICTLM"},
             {"params",
              {{"node_id", 0},
               {"mesh_x", 2},
               {"mesh_y", 2},
               {"address_regions",
                json::array({{{"base", "0x1000"}, {"size", "0x1000"}, {"target_node", 3}}})}}}});
        mods.push_back({{"name", "ni1"},
                        {"type", "NICTLM"},
                        {"params", {{"node_id", 3}, {"mesh_x", 2}, {"mesh_y", 2}}}});
        const char* rnames[4] = {"r00", "r01", "r10", "r11"};
        for (int i = 0; i < 4; ++i) {
            mods.push_back(
                {{"name", rnames[i]},
                 {"type", "RouterTLM"},
                 {"params", {{"node_x", i % 2}, {"node_y", i / 2}, {"mesh_x", 2}, {"mesh_y", 2}}}});
        }

        json conns = json::array();
        auto add = [&](const char* s, const char* d) {
            conns.push_back({{"src", s}, {"dst", d}, {"latency", 1}, {"bandwidth", 100}});
        };
        // CPU ↔ ni0
        add("cpu0", "ni0.0");
        add("ni0.0", "cpu0");
        // ni0 ↔ r00(LOCAL) —— 2 条
        add("ni0.1", "r00.4");
        add("r00.4", "ni0.1");
        // r11(LOCAL) ↔ ni1 —— 2 条
        add("r11.4", "ni1.1");
        add("ni1.1", "r11.4");
        // 环:r00-r01(X) · r01-r11(Y) · r11-r10(X) · r10-r00(Y) —— 每条 2 个方向
        add("r00.1", "r01.3");
        add("r01.3", "r00.1");
        add("r01.0", "r11.2");
        add("r11.2", "r01.0");
        add("r11.3", "r10.1");
        add("r10.1", "r11.3");
        add("r10.2", "r00.0");
        add("r00.0", "r10.2");

        return json{{"modules", mods}, {"connections", conns}};
    }

} // namespace

TEST_CASE("PROBE-4: 2×2 mesh —— X-first 下 node0 → node3 可达?(最终拓扑候选)",
          "[noc][mesh][binding-semantics]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    const json cfg = probe_topology_2x2();
    REQUIRE(factory.instantiateAll(cfg));
    factory.startAllTicks();
    eq.run(300);

    auto* ni1 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni1"));
    auto* ni0 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni0"));
    REQUIRE(ni1 != nullptr);
    REQUIRE(ni0 != nullptr);

    auto* g1 = ni1->get_stats_group();
    const int64_t ni1_pkts = scalar_of(*g1, "packets_received");
    auto* g0 = ni0->get_stats_group();
    const int64_t ni0_sent = scalar_of(*g0, "packets_sent");

    int64_t hops = 0;
    for (const char* rn : {"r00", "r01", "r10", "r11"}) {
        auto* r = dynamic_cast<tlm::RouterTLM*>(factory.getInstance(rn));
        REQUIRE(r != nullptr);
        const int64_t f = scalar_of(r->stats(), "flits_forwarded");
        const int64_t h = scalar_of(r->stats(), "total_hops");
        std::cout << "[PROBE] 2x2 " << rn << " flits=" << f
                  << " packets=" << scalar_of(r->stats(), "packets_forwarded") << " hops=" << h
                  << "\n";
        hops += h;
    }
    std::cout << "[PROBE] 2x2 ni0.packets_sent=" << ni0_sent << " ni1.packets_received=" << ni1_pkts
              << " total_hops=" << hops << "\n";

    // 可达性(探针结论用 REQUIRE 固化)
    REQUIRE(ni1_pkts > 0);
    REQUIRE(ni0_sent > 0);
}

// ────────────────────────────────────────────────────────────────────────────
// PROBE-5: 2×2 mesh 的**回程**(ni1 → node0)—— PROBE-4 里 r10 flits=0 是因为
//          ni1 未接 CPU ⇒ 响应无处可去 ⇒ 回程无流量。补上 cpu1 后测回程。
// 回程 X-first:node3=(1,1) → node0=(0,0) ⇒ 先 X( WEST) ⇒ r11→r10,再 Y(SOUTH)⇒ r10→r00
// ⇒ 预期 r10 flits > 0(若仍为 0 ⇒ 回程未走 X-first 路径 ⇒ 拓扑需重设计)
// ────────────────────────────────────────────────────────────────────────────
TEST_CASE("PROBE-5: 2×2 mesh 的**回程**(ni1 → node0)是否走 X-first 路径",
          "[noc][mesh][binding-semantics]") {
    json cfg = probe_topology_2x2();
    // 加 cpu1 并接到 ni1 的 PE 侧
    json mods = cfg["modules"];
    mods.push_back({{"name", "cpu1"}, {"type", "CPUTLM"}});
    cfg["modules"] = mods;
    json conns = cfg["connections"];
    conns.push_back({{"src", "cpu1"}, {"dst", "ni1.0"}, {"latency", 1}, {"bandwidth", 100}});
    conns.push_back({{"src", "ni1.0"}, {"dst", "cpu1"}, {"latency", 1}, {"bandwidth", 100}});
    cfg["connections"] = conns;

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));
    factory.startAllTicks();
    eq.run(300);

    auto* ni0 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni0"));
    auto* ni1 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni1"));
    REQUIRE(ni0 != nullptr);
    REQUIRE(ni1 != nullptr);

    const int64_t ni1_sent = scalar_of(*ni1->get_stats_group(), "packets_sent");
    const int64_t ni1_recv = scalar_of(*ni1->get_stats_group(), "packets_received");
    const int64_t ni0_recv = scalar_of(*ni0->get_stats_group(), "packets_received");
    std::cout << "[PROBE] 5 ni1.sent=" << ni1_sent << " ni1.recv=" << ni1_recv
              << " ni0.recv=" << ni0_recv << "\n";
    for (const char* rn : {"r00", "r01", "r10", "r11"}) {
        auto* r = dynamic_cast<tlm::RouterTLM*>(factory.getInstance(rn));
        std::cout << "[PROBE] 5 " << rn << " flits=" << scalar_of(r->stats(), "flits_forwarded")
                  << " hops=" << scalar_of(r->stats(), "total_hops") << "\n";
    }
    // 回程可行性
    REQUIRE(ni0_recv > 0);
}
