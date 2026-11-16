// test/test_cep_mesh_e2e.cc
// CEP coherence 流量**穿过真实 RouterTLM XY mesh**(2×2)抵达对端 —— 端到端判据
//
// 关联 change:`cpptlm-cep-mesh-e2e-carrier`(design D-3 的 J-1 ~ J-5)
// 拓扑配置:`configs/cep_mesh_e2e.json`(17 条 connection,连接条数由探针
//          `test/test_zz_mesh_probe.cc` 实测定稿)
//
// 路径:业务流量(TrafficGen)→ `StubCacheEndpointTLM` → `CohIfNICTLM` →
//      RouterTLM 2×2 mesh → 对端 `CohIfNICTLM` → `CohIfResponderTLM` → Completion 原路返回
//
// 作者 CppTLM Team / 日期 2026-11-16
#include <catch2/catch_all.hpp>

#include "cep_adr_parser.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "metrics/stats.hh"
#include "tlm/cep/cohif_nic.hh"
#include "tlm/cep/stub_cache_endpoint.hh"
#include "tlm/router_tlm.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace cpptlm;

namespace {

/// 端到端判据的**结果分类**（照 `test/cep_cohif_assembly_helper.hh:35-40` 的既有范式）
/// ⇒ 反向 fixture 若只断言「没抵达」,则「装配被拒」也会被判红 ⇒ 必须分类
enum class MeshOutcome {
    AssemblyFailed,   ///< 装配本身失败(配置被拒)—— **不是**端到端判据的目标
    NotInstantiated,  ///< 装配成功但拿不到模块实例
    NotArrived,       ///< 装配成功、模块在位,但事务**没有**抵达对端 ← 反向 fixture 期望
    Arrived,          ///< 事务**穿过** mesh 抵达对端 ← 正控期望
};

/// 加载本 change 的联合拓扑配置
/// ⚠️ `cep.protocol_instances` 的相对路径以「仓库根」为基准 ⇒ 须绝对化
///    (测试的 CWD 不固定)—— 照 `test/cep_cohif_assembly_helper.hh:56-66` 的既有做法
json load_mesh_topology() {
    const auto path = cpptlm::cep::repo_root() / "configs" / "cep_mesh_e2e.json";
    std::ifstream in(path);
    if (!in.good()) {
        throw std::runtime_error("无法打开 mesh 拓扑配置: " + path.string());
    }
    json cfg = json::parse(in);
    cfg["cep"]["protocol_instances"] =
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    return cfg;
}

int64_t scalar_of(tlm_stats::StatGroup& g, const std::string& name) {
    auto* s = dynamic_cast<tlm_stats::Scalar*>(g.findStat(name));
    return s ? static_cast<int64_t>(s->value()) : -1;
}

/// 判据的原始观测(一次 run 的结果)
struct MeshRun {
    MeshOutcome outcome = MeshOutcome::AssemblyFailed;
    int64_t nic_b_recv_dsts = -1;   ///< nic_b 收到的 flit 的 dst_node
    int64_t nic_b_recv_hops = -1;   ///< nic_b 收到的 flit 的 hops
    int64_t nic_a_dropped = -1;     ///< nic_a 地址未命中丢弃数
    int64_t nic_a_recv = -1;        ///< nic_a 收到的 flit 数(**回程**穿过 mesh 回到源端的证据)
    int64_t nic_b_dropped = -1;
    int64_t r_flits = 0;            ///< 4 个 router 的 flits_forwarded 之和
    int64_t r_packets = 0;          ///< 4 个 router 的 packets_forwarded 之和
    int64_t r_hops = 0;             ///< 4 个 router 的 total_hops 之和
    int64_t stub_completions_in = -1;  ///< stub 收到的 Completion 数
    int64_t stub_in_flight = -1;    ///< stub 的 in-flight 计数
};

/// 跑一次并收集观测
MeshRun run_mesh(const json& cfg, int cycles) {
    MeshRun out;
    EventQueue eq;
    ModuleFactory factory(&eq);
    if (!factory.instantiateAll(cfg)) {
        out.outcome = MeshOutcome::AssemblyFailed;
        return out;
    }
    factory.startAllTicks();
    eq.run(static_cast<uint64_t>(cycles));

    auto* nic_a = dynamic_cast<tlm::cep::CohIfNICTLM*>(factory.getInstance("nic_a"));
    auto* nic_b = dynamic_cast<tlm::cep::CohIfNICTLM*>(factory.getInstance("nic_b"));
    auto* stub = dynamic_cast<tlm::cep::StubCacheEndpointTLM*>(factory.getInstance("w"));
    if (!nic_a || !nic_b || !stub) {
        out.outcome = MeshOutcome::NotInstantiated;
        return out;
    }

    out.nic_b_recv_dsts = static_cast<int64_t>(nic_b->last_recv_dst_node());
    out.nic_b_recv_hops = static_cast<int64_t>(nic_b->last_recv_hops());
    out.nic_a_dropped = static_cast<int64_t>(nic_a->dropped_unmapped());
    out.nic_a_recv = static_cast<int64_t>(nic_a->flits_received());
    out.nic_b_dropped = static_cast<int64_t>(nic_b->dropped_unmapped());
    out.stub_completions_in = static_cast<int64_t>(stub->counters().net_completions_in);
    out.stub_in_flight = static_cast<int64_t>(stub->in_flight());

    for (const char* rn : {"r00", "r01", "r10", "r11"}) {
        auto* r = dynamic_cast<tlm::RouterTLM*>(factory.getInstance(rn));
        if (!r) {
            continue;
        }
        out.r_flits += scalar_of(r->stats(), "flits_forwarded");
        out.r_packets += scalar_of(r->stats(), "packets_forwarded");
        out.r_hops += scalar_of(r->stats(), "total_hops");
    }

    // 「抵达对端」= 对端 NIC 收到过 dst_node == 3 的 flit
    out.outcome = (out.nic_b_recv_dsts == 3) ? MeshOutcome::Arrived : MeshOutcome::NotArrived;
    return out;
}

}  // namespace

TEST_CASE("MESH-1 (J-1): 联合拓扑可装配 —— 9 个实例全部在位", "[cep][e2e][mesh]") {
    const json cfg = load_mesh_topology();
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    for (const char* name : {"tg", "w", "home", "nic_a", "nic_b", "r00", "r01", "r10", "r11"}) {
        REQUIRE(factory.getInstance(name) != nullptr);
    }
}

TEST_CASE("MESH-2 (J-2/J-3/J-4): coherence 流量穿过 mesh 抵达对端并带回响应",
          "[cep][e2e][mesh]") {
    const MeshRun r = run_mesh(load_mesh_topology(), 300);

    REQUIRE(r.outcome == MeshOutcome::Arrived);
    // J-2 —— 抵达的是**对端**(dst_node == 3),⛔ 不是自身
    REQUIRE(r.nic_b_recv_dsts == 3);
    // J-4 —— 确实**穿过**了 mesh:router 有转发 ∧ flit 的 hops ≥ 1
    //        (router 转发时 hops += 1 ⇒ 直连的 flit hops == 0)
    REQUIRE(r.r_flits > 0);
    REQUIRE(r.r_packets > 0);
    REQUIRE(r.nic_b_recv_hops >= 1);
    // 地址映射未命中 ⇒ 0 丢弃(⛔ 显式失败路径不该被触发)
    REQUIRE(r.nic_a_dropped == 0);
    REQUIRE(r.nic_b_dropped == 0);
}

TEST_CASE("MESH-3 (J-3): Completion 经 mesh 回程抵达 stub", "[cep][e2e][mesh]") {
    const MeshRun r = run_mesh(load_mesh_topology(), 300);
    // ⚠️ **两条**证据(⛔ 不得只要第一条):`home` 会**自发** Completion
    //    (`emit_completions: true`)⇒ 「stub 收到 Completion」**单独**不足以证明
    //    「Completion 穿过 mesh 回来」⇒ 追加「回程 flit 抵达**源端** NIC」。
    REQUIRE(r.stub_completions_in > 0);
    REQUIRE(r.nic_a_recv > 0);
}

TEST_CASE("MESH-4 (J-5): 决定性变异 —— 断开环边必红,且**按正确原因**红", "[cep][e2e][mesh]") {
    json cfg = load_mesh_topology();
    // 去掉去程的一环(r01 ↔ r11)⇒ flit 到不了 r11 ⇒ 对端收不到
    //
    // ⚠️ **必须同时删两条**(探针实测的连带事实):router↔router 的「2 条对称
    //    connection」互为对方的 req 路径与 resp 路径 ——
    //    `r01.0 → r11.2` 提供 req 对(r01.req_out[0] → r11.req_in[2]),
    //    `r11.2 → r01.0` 提供 resp 对(r01.resp_out[0] → r11.req_in[2])
    //    ⇒ 只删一条时,r01 仍可经 `resp_out[0]` 把 flit 送到 r11 ⇒ **变异无效**
    //    ⇒ 断开一条链路必须**成对**删除。
    json kept = json::array();
    for (const auto& c : cfg["connections"]) {
        const bool is_r01_r11 = (c["src"] == "r01.0" && c["dst"] == "r11.2") ||
                                 (c["src"] == "r11.2" && c["dst"] == "r01.0");
        if (is_r01_r11) {
            continue;
        }
        kept.push_back(c);
    }
    cfg["connections"] = kept;

    const MeshRun r = run_mesh(cfg, 300);
    // ⚠️ 必须是「**未抵达**」这个**具体**结局
    //    ⛔ 不得写成 `REQUIRE(r.outcome != Arrived)` —— 那样「装配失败」也会被判红
    //       ⇒ 变异就「因错误的原因红」⇒ 判据无牙
    REQUIRE(r.outcome == MeshOutcome::NotArrived);
}
