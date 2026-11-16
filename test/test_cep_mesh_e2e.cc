// test/test_cep_mesh_e2e.cc
// CEP coherence 流量**穿过真实 RouterTLM XY mesh**(2×2)抵达对端 —— 端到端判据
//
// 关联 change:
//   · `cpptlm-cep-mesh-e2e-carrier`(J-1 ~ J-5 的原始判据)
//   · `cpptlm-cep-mesh-interconnect-submodule`(层次化:互联件抽到蓝图)
//   · `cpptlm-simmodule-exposed-port-wiring`(Step 7c:暴露端口真实数据通路)
//
// 拓扑配置:`configs/cep_mesh_e2e.json`(业务 5 模块 + `fabric` + 13 条 connection)
// 互联蓝图:`configs/interconnect/cep_mesh_2x2.json`(4 router + 8 条环边)
//
// ⚠️ **行为等价判据采用「同一次运行内比对平铺基线」**（而非硬编码数字）：
//    平铺基线(下方 FLAT_BASELINE 字面量)与层次化拓扑**共享同一份互联拓扑**，
//    但前者把 router 平铺在顶层、后者收进 `fabric` 容器。
//    ⇒ 判据 = 两者的 **mesh 级观测逐项相等**（router 转发计数 / hop 计数），
//    该写法对无关改动免疫。
// ⚠️ **NIC 侧计数（flits_sent/flits_received）是时序敏感的**：层次化把容器子模块的
//    tick 收成**连续一组**，与平铺时各模块按哈希序交织不同 ⇒ 同周期内的到达顺序
//    与回声命中数会变（详见 ADR-X.15 关联段的发现记录）。故 NIC 侧只断言
//    **不依赖顺序**的量（收到的总 flit 数 / 发出的总 flit 数），不断言
//    `last_recv_*` 这类「最后到达者」计数器。
//
// 作者 CppTLM Team / 日期 2026-11-16(2026-11-16 层次化改造)
#include <catch2/catch_all.hpp>

#include "cep_adr_parser.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "metrics/stats.hh"
#include "tlm/cep/cep_mesh_interconnect.hh"
#include "tlm/cep/cohif_nic.hh"
#include "tlm/cep/stub_cache_endpoint.hh"
#include "tlm/router_tlm.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace cpptlm;

namespace {

/// 端到端判据的**结果分类**（照 `test/cep_cohif_assembly_helper.hh:35-40` 的既有范式）
enum class MeshOutcome {
    AssemblyFailed,   ///< 装配本身失败(配置被拒)—— **不是**端到端判据的目标
    NotInstantiated,  ///< 装配成功但拿不到模块实例
    NotArrived,       ///< 装配成功、模块在位,但事务**没有**抵达对端 ← 反向 fixture 期望
    Arrived,          ///< 事务**穿过** mesh 抵达对端 ← 正控期望
};

/// ⚠️ 平铺基线（**冻结副本**）：把 4 个 router 平铺在顶层 + 17 条 connection。
///    它是 `cpptlm-cep-mesh-e2e-carrier` 归档时的绿基线，用于「层次化前后行为等价」比对。
const char* FLAT_BASELINE = R"JSON({
  "modules": [
    { "name": "tg",   "type": "TrafficGenTLM",   "params": { "seed": 20261116 } },
    { "name": "w",    "type": "StubCacheEndpointTLM" },
    { "name": "home", "type": "CohIfResponderTLM", "params": { "emit_completions": true } },
    { "name": "nic_a", "type": "CohIfNICTLM", "params": { "node_id": 0, "mesh_x": 2, "mesh_y": 2,
        "address_regions": [ { "base": "0x1000", "size": "0x1000", "target_node": 3 } ] } },
    { "name": "nic_b", "type": "CohIfNICTLM", "params": { "node_id": 3, "mesh_x": 2, "mesh_y": 2,
        "address_regions": [ { "base": "0x1000", "size": "0x1000", "target_node": 0 } ] } },
    { "name": "r00", "type": "RouterTLM", "params": { "node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2 } },
    { "name": "r01", "type": "RouterTLM", "params": { "node_x": 1, "node_y": 0, "mesh_x": 2, "mesh_y": 2 } },
    { "name": "r10", "type": "RouterTLM", "params": { "node_x": 0, "node_y": 1, "mesh_x": 2, "mesh_y": 2 } },
    { "name": "r11", "type": "RouterTLM", "params": { "node_x": 1, "node_y": 1, "mesh_x": 2, "mesh_y": 2 } }
  ],
  "connections": [
    { "src": "tg",      "dst": "w.0",    "latency": 0 },
    { "src": "w.1",     "dst": "nic_a.0", "latency": 0 },
    { "src": "nic_a.0", "dst": "w.1",     "latency": 0 },
    { "src": "nic_a.1", "dst": "r00.4",  "latency": 1 },
    { "src": "r00.4",   "dst": "nic_a.1", "latency": 1 },
    { "src": "r00.1",   "dst": "r01.3",  "latency": 1 },
    { "src": "r01.3",   "dst": "r00.1",  "latency": 1 },
    { "src": "r01.0",   "dst": "r11.2",  "latency": 1 },
    { "src": "r11.2",   "dst": "r01.0",  "latency": 1 },
    { "src": "r11.3",   "dst": "r10.1",  "latency": 1 },
    { "src": "r10.1",   "dst": "r11.3",  "latency": 1 },
    { "src": "r10.2",   "dst": "r00.0",  "latency": 1 },
    { "src": "r00.0",   "dst": "r10.2",  "latency": 1 },
    { "src": "r11.4",   "dst": "nic_b.1", "latency": 1 },
    { "src": "nic_b.1", "dst": "r11.4",   "latency": 1 },
    { "src": "nic_b.0", "dst": "home.0",  "latency": 0 },
    { "src": "home.0",  "dst": "nic_b.0", "latency": 0 }
  ]
})JSON";

std::filesystem::path repo_root() {
    std::filesystem::path p = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (std::filesystem::exists(p / "configs") && std::filesystem::exists(p / "test")) {
            return p;
        }
        p = p.parent_path();
    }
    return std::filesystem::current_path();
}

/// 加载层次化主拓扑并把**所有仓库根相对路径**绝对化
/// ⚠️ `cep.protocol_instances` 与 `fabric.params.template` 都以「仓库根」为
///    基准书写,而测试的 CWD 不固定 ⇒ 必须绝对化
json load_hier_topology(const std::string& blueprint_override = "") {
    const auto root = repo_root();
    std::ifstream in(root / "configs" / "cep_mesh_e2e.json");
    if (!in.good()) {
        throw std::runtime_error("无法打开 mesh 拓扑配置");
    }
    json cfg = json::parse(in);
    cfg["cep"]["protocol_instances"] =
        (root / "include" / "tlm" / "cep" / "protocol_instances.json").string();

    const std::string bp = blueprint_override.empty()
                               ? (root / "configs" / "interconnect" / "cep_mesh_2x2.json").string()
                               : blueprint_override;
    for (auto& m : cfg["modules"]) {
        if (m.value("type", "") == "CepMeshInterconnect") {
            m["params"]["template"] = bp;
        }
    }
    return cfg;
}

/// 平铺基线拓扑
json load_flat_topology() {
    json cfg = json::parse(FLAT_BASELINE);
    cfg["cep"]["protocol_instances"] =
        (repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    return cfg;
}

int64_t scalar_of(tlm_stats::StatGroup& g, const std::string& name) {
    auto* s = dynamic_cast<tlm_stats::Scalar*>(g.findStat(name));
    return s ? static_cast<int64_t>(s->value()) : -1;
}

/// 单个 router 的观测（flit 数 / hop 数）
struct RouterObs {
    int64_t flits = 0;
    int64_t packets = 0;
    int64_t hops = 0;
};

struct MeshRun {
    MeshOutcome outcome = MeshOutcome::AssemblyFailed;
    int64_t nic_a_sent = -1;
    int64_t nic_a_recv = -1;
    int64_t nic_b_sent = -1;
    int64_t nic_b_recv = -1;
    int64_t nic_a_dropped = -1;
    int64_t nic_b_dropped = -1;
    int64_t stub_completions_in = -1;
    int64_t internal_router_count = -1;
    std::vector<RouterObs> routers;  ///< r00, r01, r10, r11
};

/// router 观测：层次化从 `fabric` 内部取，平铺从顶层取（同一取值口径）
RouterObs observe_router(tlm::RouterTLM* r) {
    RouterObs o;
    if (!r) return o;
    o.flits = scalar_of(r->stats(), "flits_forwarded");
    o.packets = scalar_of(r->stats(), "packets_forwarded");
    o.hops = scalar_of(r->stats(), "total_hops");
    return o;
}

MeshRun run_mesh(const json& cfg, int cycles) {
    MeshRun out;
    EventQueue eq;
    ModuleFactory factory(&eq);
    if (!factory.instantiateAll(cfg)) {
        return out;
    }
    factory.startAllTicks();
    eq.run(static_cast<uint64_t>(cycles));

    auto* fabric = dynamic_cast<tlm::cep::CepMeshInterconnect*>(factory.getInstance("fabric"));
    auto* nic_a = dynamic_cast<tlm::cep::CohIfNICTLM*>(factory.getInstance("nic_a"));
    auto* nic_b = dynamic_cast<tlm::cep::CohIfNICTLM*>(factory.getInstance("nic_b"));
    auto* stub = dynamic_cast<tlm::cep::StubCacheEndpointTLM*>(factory.getInstance("w"));
    if (!nic_a || !nic_b || !stub) {
        out.outcome = MeshOutcome::NotInstantiated;
        return out;
    }

    out.nic_a_sent = static_cast<int64_t>(nic_a->flits_sent());
    out.nic_a_recv = static_cast<int64_t>(nic_a->flits_received());
    out.nic_b_sent = static_cast<int64_t>(nic_b->flits_sent());
    out.nic_b_recv = static_cast<int64_t>(nic_b->flits_received());
    out.nic_a_dropped = static_cast<int64_t>(nic_a->dropped_unmapped());
    out.nic_b_dropped = static_cast<int64_t>(nic_b->dropped_unmapped());
    out.stub_completions_in = static_cast<int64_t>(stub->counters().net_completions_in);

    for (const char* rn : {"r00", "r01", "r10", "r11"}) {
        tlm::RouterTLM* r = nullptr;
        if (fabric) {
            r = dynamic_cast<tlm::RouterTLM*>(fabric->getInternalInstance(rn));
        } else {
            r = dynamic_cast<tlm::RouterTLM*>(factory.getInstance(rn));
        }
        out.routers.push_back(observe_router(r));
    }
    if (fabric) {
        out.internal_router_count =
            static_cast<int64_t>(fabric->getInternalFactory().getAllInstances().size());
    }

    // 「抵达对端」= 对端 NIC 收到了 flit（不依赖到达顺序）
    out.outcome = (out.nic_b_recv > 0) ? MeshOutcome::Arrived : MeshOutcome::NotArrived;
    return out;
}

}  // namespace

TEST_CASE("MESH-1 (J-1): 层次化拓扑可装配 —— 6 顶层实例 + fabric 内部 4 router",
          "[cep][e2e][mesh]") {
    const json cfg = load_hier_topology();
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    // 顶层:业务 5 + fabric 1
    for (const char* name : {"tg", "w", "home", "nic_a", "nic_b", "fabric"}) {
        REQUIRE(factory.getInstance(name) != nullptr);
    }
    // ⛔ 顶层**不再**有 router(它们已下沉到 fabric)
    for (const char* rn : {"r00", "r01", "r10", "r11"}) {
        REQUIRE(factory.getInstance(rn) == nullptr);
    }
}

TEST_CASE("MESH-2 (J-2/J-3): 层次命名空间与端口暴露成立", "[cep][e2e][mesh]") {
    const json cfg = load_hier_topology();
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* fabric = dynamic_cast<tlm::cep::CepMeshInterconnect*>(factory.getInstance("fabric"));
    REQUIRE(fabric != nullptr);

    // J-2 层次命名空间:内部实例可经 `<容器>.<内部名>` 访问
    REQUIRE(fabric->getInternalInstance("r00") != nullptr);
    REQUIRE(fabric->getInternalInstance("r11") != nullptr);
    REQUIRE(fabric->getInternalFactory().getAllInstances().size() == 4);

    // J-3 端口暴露:router LOCAL 口的 4 个通道全部暴露且可解析
    struct Exposed {
        const char* label;
        const char* internal;
    };
    const Exposed exposed[] = {
        {"node0_req_in", "r00.req_in[4]"},  {"node0_req_out", "r00.req_out[4]"},
        {"node0_resp_in", "r00.resp_in[4]"}, {"node0_resp_out", "r00.resp_out[4]"},
        {"node3_req_in", "r11.req_in[4]"},  {"node3_req_out", "r11.req_out[4]"},
        {"node3_resp_in", "r11.resp_in[4]"}, {"node3_resp_out", "r11.resp_out[4]"},
    };
    for (const auto& e : exposed) {
        REQUIRE(fabric->isExposedPort(e.label));
        REQUIRE(fabric->findInternalPath(e.label) == e.internal);
    }
}

TEST_CASE("MESH-3 (J-4): 层次化后**行为等价** —— mesh 级观测与平铺基线逐项相等",
          "[cep][e2e][mesh]") {
    const MeshRun hier = run_mesh(load_hier_topology(), 300);
    const MeshRun flat = run_mesh(load_flat_topology(), 300);

    REQUIRE(hier.outcome == MeshOutcome::Arrived);
    REQUIRE(flat.outcome == MeshOutcome::Arrived);

    // ⚠️ **mesh 级观测逐项相等** —— 这是「行为等价」的核心判据：
    //    层次化只改「互联件放在哪一层」，**不改**互联拓扑与转发语义。
    REQUIRE(hier.routers.size() == 4);
    REQUIRE(flat.routers.size() == 4);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE(hier.routers[i].flits == flat.routers[i].flits);
        REQUIRE(hier.routers[i].packets == flat.routers[i].packets);
        REQUIRE(hier.routers[i].hops == flat.routers[i].hops);
    }
    // 路由确实穿过 mesh(转发计数 > 0 且 hop 累积 > flit 数 ⇒ 多跳)
    REQUIRE(hier.routers[0].flits > 0);
    REQUIRE(hier.routers[3].flits > 0);
    REQUIRE(hier.routers[3].hops > hier.routers[3].flits);

    // 对端 NIC 收到了请求（不依赖到达顺序）
    REQUIRE(hier.nic_b_recv == flat.nic_b_recv);
    REQUIRE(hier.nic_b_recv > 0);
    // 无静默丢弃
    REQUIRE(hier.nic_a_dropped == 0);
    REQUIRE(hier.nic_b_dropped == 0);
    // 蓝图复用:内部实例数 == 蓝图 router 数
    REQUIRE(hier.internal_router_count == 4);
}

TEST_CASE("MESH-4 (J-4b): Completion 经 mesh 回程抵达 stub", "[cep][e2e][mesh]") {
    const MeshRun r = run_mesh(load_hier_topology(), 300);
    // ⚠️ 两条证据:`home` 会**自发** Completion ⇒ 「stub 收到 Completion」单独不足以
    //    证明「穿过 mesh 回来」⇒ 追加「回程 flit 抵达源端 NIC」。
    REQUIRE(r.stub_completions_in > 0);
    REQUIRE(r.nic_a_recv > 0);
}

TEST_CASE("MESH-5 (J-5): 决定性变异 —— 蓝图断开环边必红,且**按正确原因**红",
          "[cep][e2e][mesh]") {
    // ⚠️ 层次化后,变异作用于**蓝图**而非主拓扑 —— 这正是「换形态只改蓝图」的能力体现。
    const auto broken = std::filesystem::path("/tmp/opencode/cep_mesh_2x2_broken.json");
    std::filesystem::create_directories(broken.parent_path());
    {
        std::ifstream in(repo_root() / "configs" / "interconnect" / "cep_mesh_2x2.json");
        json bp = json::parse(in);
        json kept = json::array();
        for (const auto& c : bp["connections"]) {
            const bool is_r01_r11 = (c["src"] == "r01.0" && c["dst"] == "r11.2") ||
                                    (c["src"] == "r11.2" && c["dst"] == "r01.0");
            if (is_r01_r11) {
                continue;
            }
            kept.push_back(c);
        }
        bp["connections"] = kept;
        std::ofstream out(broken);
        out << bp.dump(2);
    }

    const MeshRun r = run_mesh(load_hier_topology(broken.string()), 300);
    // ⚠️ 必须是「**未抵达**」这个**具体**结局(⛔ 不得写成 `!= Arrived`
    //    —— 那样「装配失败」也会被判红 ⇒ 变异「因错误的原因红」)
    REQUIRE(r.outcome == MeshOutcome::NotArrived);
}
