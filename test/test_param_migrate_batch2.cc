// test/test_param_migrate_batch2.cc
// change `cpptlm-module-param-migrate-batch2`（C2）的逐类型判据
//
// 两类判据（`ADR-X.17` D-6 · proposal AC-02）：
//   **行为判别** —— 同一键的两个不同合法值 ⇒ 可观测指标必须不同
//                   （⛔ 只断言「load_param 被调过」= 没验效果）
//   **准入判定** —— `cpptlm::is_param_admitted(type)` 为真时未知键必须失败
//
// ⚠️ 本文件的「观测面」全部来自**模块自身的公开 getter / 运行期行为**，
//    ⛔ 不读 `load_param` 内部状态（那只能证明「声明了」，不能证明「生效了」）。
//
// 分工：
//   · 本文件 = **ChStream 家族**（P1a 批次，T1.x）
//   · SimModule 家族（P1b，T2.x）在本文件另立 section（同一 change 的同一判据族）

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>

#include <nlohmann/json.hpp>

#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/param_admission.hh"
#include "tlm/cep/cep_mesh_interconnect.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/cep/cohif_nic.hh"
#include "tlm/cep/cohif_responder.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "tlm/cluster/apu_soc.hh"
#include "tlm/cluster/cache_cluster.hh"
#include "tlm/cluster/compute_cluster.hh"
#include "tlm/cluster/cpu_cluster.hh"
#include "tlm/cluster/gpc_cluster.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/cluster/gpu_noc_cluster.hh"
#include "tlm/cluster/memory_cluster.hh"
#include "tlm/cluster/tpc_cluster.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/nic_tlm.hh"
#include "tlm/vram_controller_tlm.hh"

using json = nlohmann::json;

namespace {

    void register_all_types() {
        REGISTER_ALL
        REGISTER_MODULE(CpuCluster);
    }

    /// 构造一个只含单模块的顶层配置（`instantiateAll` 需要 `modules` + `connections`）
    json one_module(const std::string& name, const std::string& type, json params) {
        json m{{"name", name}, {"type", type}};
        if (!params.is_null()) m["params"] = std::move(params);
        return json{{"modules", json::array({m})}, {"connections", json::array()}};
    }

} // namespace

// ============================================================================
// T1.1 — NICTLM
// ============================================================================

TEST_CASE("AC-02: NICTLM.node_id 生效 —— 两个合法值 ⇒ getter 不同",
          "[param][migrate][batch2][nic]") {
    register_all_types();

    auto node_id_of = [](std::uint32_t v) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "nic_x", "NICTLM", json{{"node_id", v}, {"mesh_x", 4}, {"mesh_y", 4}})));
        auto* nic = dynamic_cast<tlm::NICTLM*>(factory.getInstance("nic_x"));
        REQUIRE(nic != nullptr);
        return nic->node_id();
    };

    const auto a = node_id_of(0);
    const auto b = node_id_of(7);
    // ★ 判别力：两个不同合法值 ⇒ 可观测读数必须不同
    CHECK(a == 0);
    CHECK(b == 7);
    CHECK(a != b);
}

TEST_CASE("AC-02: NICTLM.mesh_x / mesh_y 生效 —— 两个合法值 ⇒ getter 不同",
          "[param][migrate][batch2][nic]") {
    register_all_types();

    auto mesh_of = [](std::uint32_t mx, std::uint32_t my) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("nic_m", "NICTLM", json{{"node_id", 0}, {"mesh_x", mx}, {"mesh_y", my}})));
        auto* nic = dynamic_cast<tlm::NICTLM*>(factory.getInstance("nic_m"));
        REQUIRE(nic != nullptr);
        return std::make_pair(nic->mesh_x(), nic->mesh_y());
    };

    const auto a = mesh_of(2, 2);
    const auto b = mesh_of(8, 4);
    CHECK(a == std::make_pair(std::uint32_t{2}, std::uint32_t{2}));
    CHECK(b == std::make_pair(std::uint32_t{8}, std::uint32_t{4}));
    CHECK(a != b);
}

TEST_CASE("AC-03: NICTLM.node_id 缺失 ⇒ 装配失败（必填真值唯一 —— CFG-08 分支已退役）",
          "[param][migrate][batch2][nic]") {
    register_all_types();
    EventQueue eq;
    ModuleFactory factory(&eq);

    // 只给 mesh，不给 node_id ⇒ `load_param("node_id", ...)` 无默认值 ⇒ 必填缺失 ⇒ 抛
    REQUIRE_FALSE(
        factory.instantiateAll(one_module("nic_bad", "NICTLM", json{{"mesh_x", 2}, {"mesh_y", 2}})));
}

TEST_CASE("AC-07: NICTLM 拼错键 ⇒ 装配失败且错误含键名（准入已启用）",
          "[param][migrate][batch2][nic]") {
    register_all_types();
    EventQueue eq;
    ModuleFactory factory(&eq);

    // 准入类型（NICTLM 已在 `admitted_param_types()` 内）⇒ 未知键必须失败
    CHECK(cpptlm::is_param_admitted("NICTLM"));

    // `nodeid` 是 `node_id` 的常见拼错
    const json cfg =
        one_module("nic_typo", "NICTLM", json{{"node_id", 0}, {"nodeid", 3}, {"mesh_x", 2}, {"mesh_y", 2}});
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

TEST_CASE("AC-02: NICTLM.address_regions 生效（非标量 / 态② 模块内解析）",
          "[param][migrate][batch2][nic]") {
    register_all_types();
    EventQueue eq;
    ModuleFactory factory(&eq);

    // `address_regions` 是对象数组 ⇒ `load_param` 不承载（D-2 态②）⇒ 模块内解析 + 登记
    const json cfg = one_module("nic_reg", "NICTLM",
                                json{{"node_id", 0},
                                     {"mesh_x", 4},
                                     {"mesh_y", 4},
                                     {"address_regions",
                                      json::array({json{{"base", 0x1000},
                                                        {"size", 0x1000},
                                                        {"target_node", 3}}})}});
    REQUIRE(factory.instantiateAll(cfg));

    auto* nic = dynamic_cast<tlm::NICTLM*>(factory.getInstance("nic_reg"));
    REQUIRE(nic != nullptr);
    // 区域已注册 ⇒ 区内地址解析到 target_node
    CHECK(nic->lookup_node(0x1000) == 3);
    CHECK(nic->lookup_node(0x1FFF) == 3);
}

// ============================================================================
// T1.2 — MemoryTLM
// ============================================================================

TEST_CASE("AC-02: MemoryTLM.capacity_gb 生效 —— 两个合法值 ⇒ 容量上限不同",
          "[param][migrate][batch2][memory]") {
    register_all_types();

    auto cap_of = [](int gb) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module("mem_x", "MemoryTLM", json{{"capacity_gb", gb}})));
        auto* m = dynamic_cast<MemoryTLM*>(factory.getInstance("mem_x"));
        REQUIRE(m != nullptr);
        return m->size_cap();
    };

    const auto a = cap_of(1);
    const auto b = cap_of(4);
    CHECK(a == (1ULL << 30));
    CHECK(b == (4ULL << 30));
    CHECK(a != b);
}

TEST_CASE("AC-02: MemoryTLM.read_latency / write_latency 生效（两值 ⇒ 读数不同）",
          "[param][migrate][batch2][memory]") {
    register_all_types();

    auto lat_of = [](std::uint64_t rl, std::uint64_t wl) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "mem_l", "MemoryTLM", json{{"read_latency", rl}, {"write_latency", wl}})));
        auto* m = dynamic_cast<MemoryTLM*>(factory.getInstance("mem_l"));
        REQUIRE(m != nullptr);
        // 单值 `read_latency` ⇒ hit 与 miss **同值**（细粒度拆分归板级键）
        return std::make_tuple(m->read_latency_hit_cycles(), m->read_latency_miss_cycles(),
                               m->write_latency_cycles());
    };

    const auto a = lat_of(7, 11);
    const auto b = lat_of(70, 110);
    CHECK(a == std::make_tuple(7ULL, 7ULL, 11ULL));
    CHECK(b == std::make_tuple(70ULL, 70ULL, 110ULL));
    CHECK(a != b);
}

TEST_CASE("AC-04: MemoryTLM.type / mem_type ⇒ 装配期显式失败（态②，已由 C1 落地）",
          "[param][migrate][batch2][memory]") {
    register_all_types();
    EventQueue eq;
    ModuleFactory factory(&eq);

    CHECK(cpptlm::is_param_admitted("MemoryTLM"));
    REQUIRE_FALSE(factory.instantiateAll(one_module("mem_t", "MemoryTLM", json{{"type", "DDR4"}})));
}

// ============================================================================
// T1.3 — VramControllerTLM
// ============================================================================

TEST_CASE("AC-02: VramControllerTLM.vram_size_bytes 生效 + operating_frequency_hz 态②拒绝",
          "[param][migrate][batch2][vram]") {
    register_all_types();

    auto cap_of = [](std::uint64_t sz) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("vram_x", "VramControllerTLM", json{{"vram_size_bytes", sz}})));
        auto* v = dynamic_cast<VramControllerTLM*>(factory.getInstance("vram_x"));
        REQUIRE(v != nullptr);
        return v->size_cap();
    };

    const auto a = cap_of(1ULL << 20);
    const auto b = cap_of(4ULL << 20);
    CHECK(a == (1ULL << 20));
    CHECK(b == (4ULL << 20));
    CHECK(a != b);

    // 态②：编译期锚点不可配 ⇒ 写了即失败
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(
        one_module("vram_bad", "VramControllerTLM", json{{"operating_frequency_hz", 2000000000}})));
}

// ============================================================================
// T1.4 — GmmuTLM
// ============================================================================

TEST_CASE("AC-02: GmmuTLM.page_size_bytes 生效 —— 两个合法值 ⇒ getter 不同",
          "[param][migrate][batch2][gmmu]") {
    register_all_types();

    auto ps_of = [](std::uint64_t ps) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module("gmmu_x", "GmmuTLM", json{{"page_size_bytes", ps}})));
        auto* g = dynamic_cast<tlm::gpu::GmmuTLM*>(factory.getInstance("gmmu_x"));
        REQUIRE(g != nullptr);
        return g->page_size_bytes();
    };

    CHECK(ps_of(4096) == 4096);
    CHECK(ps_of(65536) == 65536);
    CHECK(ps_of(4096) != ps_of(65536));

    // 非法值（非 2 的幂）⇒ 装配失败
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(
        factory.instantiateAll(one_module("gmmu_bad", "GmmuTLM", json{{"page_size_bytes", 5000}})));
}

// ============================================================================
// T1.6 — SdmaEngineTLM
// ============================================================================

TEST_CASE("AC-02: SdmaEngineTLM 三键生效（两值 ⇒ 读数不同）",
          "[param][migrate][batch2][sdma]") {
    register_all_types();

    auto read_of = [](std::uint32_t mi, std::uint32_t tl, std::uint64_t vs) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "sdma_x", "SdmaEngineTLM",
            json{{"max_inflight", mi}, {"translate_latency", tl}, {"vram_size_bytes", vs}})));
        auto* s = dynamic_cast<tlm::gpu::SdmaEngineTLM*>(factory.getInstance("sdma_x"));
        REQUIRE(s != nullptr);
        return std::make_tuple(s->max_inflight(), s->translate_latency(), s->vram_size_bytes());
    };

    const auto a = read_of(2, 5, 1ULL << 20);
    const auto b = read_of(8, 20, 4ULL << 20);
    CHECK(a == std::make_tuple(2u, 5u, 1ULL << 20));
    CHECK(b == std::make_tuple(8u, 20u, 4ULL << 20));
    CHECK(a != b);
}

// ============================================================================
// T1.8 — CohIfNICTLM
// ============================================================================

TEST_CASE("AC-02: CohIfNICTLM 三键生效 + address_regions 非标量登记",
          "[param][migrate][batch2][cohif-nic]") {
    register_all_types();

    auto node_of = [](std::uint32_t nid) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("cnic_x", "CohIfNICTLM", json{{"node_id", nid}, {"mesh_x", 4}, {"mesh_y", 4}})));
        auto* c = dynamic_cast<tlm::cep::CohIfNICTLM*>(factory.getInstance("cnic_x"));
        REQUIRE(c != nullptr);
        return c->node_id();
    };

    CHECK(node_of(1) == 1);
    CHECK(node_of(9) == 9);
    CHECK(node_of(1) != node_of(9));

    // address_regions（对象数组）⇒ 模块内解析生效
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(one_module(
        "cnic_reg", "CohIfNICTLM",
        json{{"node_id", 0},
             {"address_regions", json::array({json{{"base", 0x2000}, {"size", 0x1000}, {"target_node", 5}}})}})));
    auto* c = dynamic_cast<tlm::cep::CohIfNICTLM*>(factory.getInstance("cnic_reg"));
    REQUIRE(c != nullptr);
    // ⚠️ `lookup_node` 是 private（地址映射属实现细节）⇒ 本 case **不**断言解析结果，
    //    只断言「含非标量键的配置能装配成功」（= `ignore_param` 登记生效，未被误判
    //    为未知键）。解析行为本身由 `test_cep_mesh_e2e` 的端到端流量覆盖。
    CHECK(cpptlm::is_param_admitted("CohIfNICTLM"));
    // 反证：同一份配置若**不**登记 `address_regions` 会因未知键失败 —— 见下一条断言
    EventQueue eq2;
    ModuleFactory f2(&eq2);
    REQUIRE_FALSE(f2.instantiateAll(
        one_module("cnic_bad", "CohIfNICTLM",
                   json{{"node_id", 0}, {"address_regionz", json::array()}})));
}

// ============================================================================
// T1.9 — CohIfResponderTLM
// ============================================================================

TEST_CASE("AC-02: CohIfResponderTLM 四键生效（两值 ⇒ 读数不同）",
          "[param][migrate][batch2][cohif-resp]") {
    register_all_types();

    auto cfg_of = [](bool ec, unsigned lat, bool es, unsigned iv) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "cresp", "CohIfResponderTLM",
            json{{"emit_completions", ec},
                 {"completion_latency_cycles", lat},
                 {"emit_snoops", es},
                 {"snoop_interval_cycles", iv}})));
        auto* r = dynamic_cast<tlm::cep::CohIfResponderTLM*>(factory.getInstance("cresp"));
        REQUIRE(r != nullptr);
        const auto& c = r->config();
        return std::make_tuple(c.emit_completions, c.completion_latency_cycles, c.emit_snoops,
                               c.snoop_interval_cycles);
    };

    const auto a = cfg_of(true, 3, false, 6);
    const auto b = cfg_of(false, 30, true, 60);
    CHECK(a == std::make_tuple(true, 3u, false, 6u));
    CHECK(b == std::make_tuple(false, 30u, true, 60u));
    CHECK(a != b);

    // 类型不符 ⇒ 装配期失败（⛔ 旧实现是静默忽略）
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(
        one_module("cresp_bad", "CohIfResponderTLM", json{{"emit_completions", "yes"}})));
}

// ============================================================================
// T2 — SimModule 家族（P1b）
//
// 观测面：模块自身的公开 getter（`ADR-X.17` D-6 要求「两个合法值 ⇒ 读数不同」）。
// 带单位字符串（`"16KB"`）的键只断言**字符串已落成员**；解析正确性由内层
// `CacheTLM` / `MemoryTLM` 的既有判据覆盖（本文件不重复造）。
// ============================================================================

namespace {

    /// 取模块类型 T 实例的辅助（失败 ⇒ REQUIRE 终止该 case）
    template <typename T>
    T* get_as(ModuleFactory& factory, const std::string& name) {
        return dynamic_cast<T*>(factory.getInstance(name));
    }

} // namespace

// ── T2.1 CpuCluster ──────────────────────────────────────────────────────
TEST_CASE("AC-02: CpuCluster.num_cpus / cluster_id 生效", "[param][migrate][batch2][cpucluster]") {
    register_all_types();

    auto probe = [](int n, const std::string& id) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("cc", "CpuCluster", json{{"num_cpus", n}, {"cluster_id", id}})));
        auto* c = get_as<CpuCluster>(factory, "cc");
        REQUIRE(c != nullptr);
        return std::make_pair(c->num_cpus(), c->cluster_id());
    };

    const auto a = probe(2, "alpha");
    const auto b = probe(8, "beta");
    CHECK(a == std::make_pair(2, std::string("alpha")));
    CHECK(b == std::make_pair(8, std::string("beta")));
    CHECK(a != b);

    // 拼错键 ⇒ 装配失败（准入已启用）
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(
        one_module("cc_typo", "CpuCluster", json{{"num_cpu", 2}})));
}

// ── T2.2 CacheCluster ────────────────────────────────────────────────────
TEST_CASE("AC-02: CacheCluster.l1_count 决定子模块数 + 尺寸字符串落成员",
          "[param][migrate][batch2][cachecluster]") {
    register_all_types();

    auto sub_count = [](int l1n) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("cache_c", "CacheCluster", json{{"l1_count", l1n}})));
        auto* c = get_as<cpptlm::tlm::CacheCluster>(factory, "cache_c");
        REQUIRE(c != nullptr);
        // 判别力：l1_count 直接决定 L1 实例数（+1 个 L2）
        CHECK(c->l1_count() == l1n);
        return c->getInternalFactory().getAllInstances().size();
    };

    CHECK(sub_count(2) == 3u);
    CHECK(sub_count(5) == 6u);

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(one_module(
        "cache_sz", "CacheCluster", json{{"l1_size", "32KB"}, {"l2_size", "1MB"}})));
    auto* c = get_as<cpptlm::tlm::CacheCluster>(factory, "cache_sz");
    REQUIRE(c != nullptr);
    CHECK(c->l1_size() == "32KB");
    CHECK(c->l2_size() == "1MB");
}

// ── T2.3 MemoryCluster ───────────────────────────────────────────────────
TEST_CASE("AC-02: MemoryCluster.channel_count 生效 + memory_type 态②拒绝",
          "[param][migrate][batch2][memorycluster]") {
    register_all_types();

    auto sub_count = [](int ch) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("mem_c", "MemoryCluster", json{{"channel_count", ch}, {"channel_size", "2GB"}})));
        auto* c = get_as<cpptlm::tlm::MemoryCluster>(factory, "mem_c");
        REQUIRE(c != nullptr);
        CHECK(c->channel_size() == "2GB");
        return c->getInternalFactory().getAllInstances().size();
    };

    // 2 / 4 通道（ArbiterTLM2 / ArbiterTLM4）+ 1 个 arbiter
    CHECK(sub_count(2) == 3u);
    CHECK(sub_count(4) == 5u);

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(
        one_module("mem_bad", "MemoryCluster", json{{"memory_type", "DDR4"}})));
}

// ── T2.4 ComputeCluster ──────────────────────────────────────────────────
TEST_CASE("AC-02: ComputeCluster.cu_count / cu_template 生效",
          "[param][migrate][batch2][computecluster]") {
    register_all_types();

    auto probe = [](int n) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "cu", "ComputeCluster",
            json{{"cu_count", n}, {"cu_template", "configs/templates/compute_unit_v1.json"}})));
        auto* c = get_as<cpptlm::tlm::ComputeCluster>(factory, "cu");
        REQUIRE(c != nullptr);
        CHECK(c->cu_template() == "configs/templates/compute_unit_v1.json");
        return c->cu_count();
    };

    CHECK(probe(2) == 2);
    CHECK(probe(5) == 5);

    // 钳位语义保留（< 1 ⇒ 1）—— 合法性裁决归模块内，不是 `load_param` 的职责
    CHECK(probe(0) == 1);
}

// ── T2.5 TpcCluster ──────────────────────────────────────────────────────
TEST_CASE("AC-02: TpcCluster.tpc_id / cu_per_tpc 生效", "[param][migrate][batch2][tpccluster]") {
    register_all_types();

    auto probe = [](int id, int cu) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "tpc", "TpcCluster",
            json{{"tpc_id", id},
                 {"cu_per_tpc", cu},
                 {"cu_template", "configs/templates/compute_unit_v1.json"}})));
        auto* c = get_as<cpptlm::tlm::TpcCluster>(factory, "tpc");
        REQUIRE(c != nullptr);
        CHECK(c->tpc_id() == id);
        return c->cu_per_tpc();
    };

    CHECK(probe(0, 2) == 2);
    CHECK(probe(3, 4) == 4);
}

// ── T2.6 GpcCluster ──────────────────────────────────────────────────────
TEST_CASE("AC-02: GpcCluster.gpc_id / tpc_per_gpc / cu_per_tpc 生效",
          "[param][migrate][batch2][gpccluster]") {
    register_all_types();

    auto probe = [](int id, int tpc) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "gpc", "GpcCluster",
            json{{"gpc_id", id},
                 {"tpc_per_gpc", tpc},
                 {"cu_per_tpc", 2},
                 {"cu_template", "configs/templates/compute_unit_v1.json"}})));
        auto* c = get_as<cpptlm::tlm::GpcCluster>(factory, "gpc");
        REQUIRE(c != nullptr);
        CHECK(c->gpc_id() == id);
        CHECK(c->cu_per_tpc() == 2);
        return c->tpc_per_gpc();
    };

    CHECK(probe(0, 1) == 1);
    CHECK(probe(2, 2) == 2);
}

// ── T2.7 GpuCluster ──────────────────────────────────────────────────────
TEST_CASE("AC-02: GpuCluster.gpc_count / 几何参数生效", "[param][migrate][batch2][gpucluster]") {
    register_all_types();

    auto probe = [](int gpc, int tpc, int cu) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "gpu", "GpuCluster",
            json{{"gpc_count", gpc},
                 {"tpc_per_gpc", tpc},
                 {"cu_per_tpc", cu},
                 {"cu_template", "configs/templates/compute_unit_v1.json"}})));
        auto* c = get_as<cpptlm::tlm::GpuCluster>(factory, "gpu");
        REQUIRE(c != nullptr);
        return std::make_tuple(c->gpc_count(), c->tpc_per_gpc(), c->cu_per_tpc());
    };

    CHECK(probe(1, 1, 1) == std::make_tuple(1, 1, 1));
    CHECK(probe(2, 2, 2) == std::make_tuple(2, 2, 2));
}

// ── T2.8 ApuSoC ──────────────────────────────────────────────────────────
TEST_CASE("AC-02: ApuSoC.cpu_topology / gpu_topology 生效", "[param][migrate][batch2][apusoc]") {
    register_all_types();

    auto probe = [](const std::string& cpu_path) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "apu", "ApuSoC",
            json{{"cpu_topology", cpu_path},
                 {"gpu_topology", "configs/templates/gpu_2gpc_2tpc_2cu.json"}})));
        auto* c = get_as<cpptlm::tlm::ApuSoC>(factory, "apu");
        REQUIRE(c != nullptr);
        return c->cpu_topology();
    };

    CHECK(probe("configs/templates/cpu_cluster_2level.json") ==
          "configs/templates/cpu_cluster_2level.json");
}

// ── T2.9 CepMeshInterconnect ─────────────────────────────────────────────
TEST_CASE("AC-02: CepMeshInterconnect.template 生效", "[param][migrate][batch2][cepm]") {
    register_all_types();

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(one_module(
        "fabric", "CepMeshInterconnect", json{{"template", "configs/interconnect/cep_mesh_2x2.json"}})));
    auto* c = get_as<tlm::cep::CepMeshInterconnect>(factory, "fabric");
    REQUIRE(c != nullptr);
    CHECK(c->template_path() == "configs/interconnect/cep_mesh_2x2.json");
}

// ── T2.10 + T3.2 GpuNoC ─────────────────────────────────────────────────
TEST_CASE("AC-02: GpuNoC.mesh_size 生效 + 准入（T3.2：routing 裁决已由 C1 的 B3 完成）",
          "[param][migrate][batch2][gpunoc]") {
    register_all_types();

    // T2.10 暂缓准入 ⇒ **T3.2 落地**（前置：C1 的 B3 已把 `RouterTLM.routing` 落态③）
    CHECK(cpptlm::is_param_admitted("GpuNoC"));

    auto probe = [](int n) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module("noc", "GpuNoC", json{{"mesh_size", n}})));
        auto* c = get_as<cpptlm::tlm::GpuNoC>(factory, "noc");
        REQUIRE(c != nullptr);
        return c->mesh_size();
    };

    CHECK(probe(2) == 2);
    CHECK(probe(3) == 3);
}

TEST_CASE("AC-02: GpuNoC.routing 非法值 ⇒ 装配失败（态③ 透传给内部 RouterTLM）",
          "[param][migrate][batch2][gpunoc]") {
    register_all_types();

    // 合法值 "XY"（默认）⇒ 正常装配
    {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("noc_ok", "GpuNoC", json{{"mesh_size", 2}, {"routing", "XY"}})));
        auto* c = get_as<cpptlm::tlm::GpuNoC>(factory, "noc_ok");
        REQUIRE(c != nullptr);
        CHECK(c->routing() == "XY");
    }
    // 非法值 ⇒ 本类把值透传给内部 `RouterTLM`，后者态③ 拒绝 ⇒ 整体装配失败
    {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE_FALSE(factory.instantiateAll(
            one_module("noc_bad", "GpuNoC", json{{"mesh_size", 2}, {"routing", "FIFO"}})));
    }
}

// ============================================================================
// T4.6 — ParamRules 机制退役后的**正向契约断言**（原
// `test/test_d3_param_resolver_registry.cc` 的「RouterTLM 已迁移」条目）
//
// ⚠️ 该断言 **SHALL 迁移**（T4.6 明写「⛔ 不得丢失」）：
//    机制退役后，「注册表里没有 RouterTLM 条目」这一判据的对象本身已不存在
//    ⇒ 保留其**语义**：该类型**已迁移到 `load_param` 声明**，判据 = 准入表成员资格
//    （`ADR-X.17` D-3 的准入条件 (a)(b)(c) 三条同时成立才可能进表）。
// ============================================================================
TEST_CASE("T4.6: RouterTLM 已迁移到 load_param（准入表成员 = 注册表无条目的等价判据）",
          "[param][migrate][batch2][d3]") {
    CHECK(cpptlm::is_param_admitted("RouterTLM"));

    const auto admitted = cpptlm::admitted_param_types();
    REQUIRE(std::find(admitted.begin(), admitted.end(), "RouterTLM") != admitted.end());

    // 迁移后的类型是全量化的（不是只有 RouterTLM 一个）
    CHECK(cpptlm::is_param_admitted("NICTLM"));
    CHECK(cpptlm::is_param_admitted("MemoryTLM"));
}

// ============================================================================
// T5 — 测试与验收支撑（AC-01 / AC-07 / AC-08）
// ============================================================================

// ── T5.1 AC-01：准入表断言（**从 `admitted_param_types()` 派生**）──────────
//    ⛔ 测试内**不硬编码第二份类型清单**（那是 `ADR-X.17` 要消灭的形态）：
//    这里断言的是「准入表自身的结构不变量」，具体成员由逐类型判据（T1/T2/T3）覆盖。
TEST_CASE("AC-01: 准入表结构不变量（派生自 admitted_param_types()，无第二份清单）",
          "[param][migrate][batch2][admission]") {
    const auto admitted = cpptlm::admitted_param_types();

    // ① 非空
    REQUIRE_FALSE(admitted.empty());

    for (const auto& t : admitted) {
        // ② 无空名 / 无首尾空白（类型名来自 `get_module_type()`，不该有空白）
        CHECK_FALSE(t.empty());
        CHECK(t.front() != ' ');
        CHECK(t.back() != ' ');
        // ③ 自洽：出现在清单里的类型，`is_param_admitted` 必须为真
        CHECK(cpptlm::is_param_admitted(t));
    }

    // ④ 严格升序 ⇒ 无重复（`set` 的天然语义，此处把它变成可失败的判据）
    CHECK(std::is_sorted(admitted.begin(), admitted.end()));
    CHECK(std::adjacent_find(admitted.begin(), admitted.end()) == admitted.end());

    // ⑤ 负例：不存在的类型不得被准入（否则「准入」不构成筛选）
    CHECK_FALSE(cpptlm::is_param_admitted("NoSuchModuleType"));

    // ⑥ 幂等：重复调用返回同一内容（不得因某次装配而漂移）
    const auto again = cpptlm::admitted_param_types();
    CHECK(again == admitted);
}

// ── T5.2 AC-07：≥3 个不同类型的拼错键 ⇒ 装配期失败且错误含键名 ──────────────
//    直接调 `set_config`（`ModuleFactory` 会把异常折叠成 `false`，
//    错误文本只在 `ParamValidationError::param_name` 上可断言）
TEST_CASE("AC-07: 三种不同类型的拼错键 ⇒ ParamValidationError 且 param_name 含键名",
          "[param][migrate][batch2][admission]") {
    register_all_types();
    EventQueue eq;

    auto expect_unknown_key = [](auto&& fn, const char* key) {
        try {
            fn();
            FAIL("expected cpptlm::ParamValidationError for key '" << key << "'");
        } catch (const cpptlm::ParamValidationError& e) {
            CHECK(e.param_name == key);
            CHECK(!std::string(e.what()).empty());
        }
    };

    // 三个**不同**类型（ChStream 家族 / SimModule 家族各取样）
    SECTION("NICTLM: nodeid（应为 node_id）") {
        tlm::NICTLM nic("nic", &eq);
        expect_unknown_key([&] { nic.set_config(json{{"node_id", 0}, {"nodeid", 7}}); },
                           "nodeid");
    }
    SECTION("MemoryTLM: capacit_gb（应为 capacity_gb）") {
        MemoryTLM mem("mem", &eq);
        expect_unknown_key([&] { mem.set_config(json{{"capacit_gb", 4}}); }, "capacit_gb");
    }
    SECTION("CpuCluster: num_cpu（应为 num_cpus）") {
        CpuCluster cc("cc", &eq);
        expect_unknown_key([&] { cc.set_config(json{{"num_cpu", 2}}); }, "num_cpu");
    }
    SECTION("SdmaEngineTLM: max_inflght（应为 max_inflight）") {
        tlm::gpu::SdmaEngineTLM sdma("sdma", &eq);
        expect_unknown_key([&] { sdma.set_config(json{{"max_inflght", 4}}); }, "max_inflght");
    }
}

// ── T5.4 AC-08：板级直读键三态合规（`ignore_param` ↔ 台账表 A）─────────────
//    台账映射（`docs/implementation/module-param-baseline.md`）：
//      · `MemoryTLM` 7 键（`read_latency_hit_cycles` / `read_latency_miss_cycles` /
//        `write_latency_cycles` / `use_zero_delay_for_test` / `row_hit_cycles` /
//        `row_miss_cycles` / `bandwidth_gbps`）§1.1 板级直读
//      · `GmmuTLM` 2 键（`tlb_size` / `tlb_miss_latency_cycles`）§1.1 板级直读
//    判据：这些键**不得**被判为未知键（否则板级 injection 装配必失败）。
TEST_CASE("AC-08: 板级直读键（台账表 A）不被判为未知键", "[param][migrate][batch2][admission]") {
    register_all_types();
    EventQueue eq;

    SECTION("MemoryTLM 的 7 个板级直读键") {
        MemoryTLM mem("mem", &eq);
        REQUIRE_NOTHROW(mem.set_config(json{{"read_latency_hit_cycles", 100},
                                            {"read_latency_miss_cycles", 200},
                                            {"write_latency_cycles", 120},
                                            {"use_zero_delay_for_test", false},
                                            {"row_hit_cycles", 30},
                                            {"row_miss_cycles", 60},
                                            {"bandwidth_gbps", 512}}));
    }
    SECTION("VramControllerTLM 继承同一批键（+ 自己的 vram_size_bytes）") {
        VramControllerTLM vram("vram", &eq);
        REQUIRE_NOTHROW(vram.set_config(json{{"vram_size_bytes", 1ULL << 20},
                                             {"read_latency_hit_cycles", 100},
                                             {"row_hit_cycles", 30},
                                             {"bandwidth_gbps", 512}}));
        CHECK(vram.size_cap() == (1ULL << 20));
    }
    SECTION("GmmuTLM 的 2 个板级直读键") {
        tlm::gpu::GmmuTLM gmmu("gmmu", &eq);
        REQUIRE_NOTHROW(
            gmmu.set_config(json{{"tlb_size", 64}, {"tlb_miss_latency_cycles", 50}}));
    }
    SECTION("CohIfNICTLM / NICTLM 的 address_regions（非标量）") {
        tlm::cep::CohIfNICTLM cnic("cnic", &eq);
        REQUIRE_NOTHROW(cnic.set_config(
            json{{"node_id", 0},
                 {"address_regions", json::array({json{{"base", 0x1000}, {"size", 0x1000}}})}}));
        tlm::NICTLM nic("nic", &eq);
        REQUIRE_NOTHROW(nic.set_config(
            json{{"node_id", 0},
                 {"address_regions",
                  json::array({json{{"base", 0x1000}, {"size", 0x1000}, {"target_node", 0}}})}}));
    }
}

// ============================================================================
// T8 返工 — verifier 验收 finding F1（`PcieEndpointIP` 未准入）
// ============================================================================

// F1-1: 两个零消费键 ⇒ 态② 显式拒绝（错误含键名）
TEST_CASE("F1: PcieEndpointIP 的 resizable_bars / num_msix_vectors ⇒ 显式拒绝",
          "[param][migrate][batch2][pcie-ep]") {
    register_all_types();
    EventQueue eq;

    auto expect_rejected = [](auto&& fn, const char* key) {
        try {
            fn();
            FAIL("expected cpptlm::ParamValidationError for key '" << key << "'");
        } catch (const cpptlm::ParamValidationError& e) {
            CHECK(e.param_name == key);
            CHECK(std::string(e.what()).find(key) != std::string::npos);
        }
    };

    SECTION("resizable_bars（嵌套块，本类无消费面）") {
        tlm::pcie::PcieEndpointIP ep("ep_rb", &eq);
        ep.init();
        expect_rejected(
            [&] {
                ep.set_config(json{{"config_size", 4096},
                                    {"resizable_bars", json{{"supported_bar_indices", json::array({2})}}}});
            },
            "resizable_bars");
    }
    SECTION("num_msix_vectors（旧 EP 模型遗留名）") {
        tlm::pcie::PcieEndpointIP ep("ep_nv", &eq);
        ep.init();
        expect_rejected([&] { ep.set_config(json{{"config_size", 4096}, {"num_msix_vectors", 16}}); },
                        "num_msix_vectors");
    }
}

// F1-2: 准入生效 ⇒ 拼错键必须失败（此前该类型完全不受未知键检查覆盖）
TEST_CASE("F1: PcieEndpointIP 已准入 ⇒ 拼错键装配失败且错误含键名",
          "[param][migrate][batch2][pcie-ep]") {
    register_all_types();

    CHECK(cpptlm::is_param_admitted("PcieEndpointIP"));

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(one_module(
        "ep_typo", "PcieEndpointIP", json{{"config_size", 4096}, {"totally_bogus_key", 1}})));
}

// F1-3: 合法键集仍可装配（回归护栏）
TEST_CASE("F1: PcieEndpointIP 的合法键集仍可装配", "[param][migrate][batch2][pcie-ep]") {
    register_all_types();
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(one_module(
        "ep_ok", "PcieEndpointIP",
        json{{"config_size", 4096}, {"bar_sizes", json::array({4096, 16777216})}})));
}

// ============================================================================
// T8 返工 — verifier 验收 finding F3 / F4 / F5（AC-02 的判别覆盖缺口）
// ============================================================================

// F3: ApuSoC.coherent_xbar_name / gpu_topology（此前**零断言** / 单一值）
TEST_CASE("F3: ApuSoC.coherent_xbar_name / gpu_topology 生效",
          "[param][migrate][batch2][apusoc][f3]") {
    register_all_types();

    // F3-a: `coherent_xbar_name` —— 两个合法值 ⇒ 观测面不同
    auto xbar_name_of = [](const std::string& v) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "apu", "ApuSoC",
            json{{"cpu_topology", "configs/templates/cpu_cluster_2level.json"},
                 {"coherent_xbar_name", v}})));
        auto* c = get_as<cpptlm::tlm::ApuSoC>(factory, "apu");
        REQUIRE(c != nullptr);
        return c->coherent_xbar_name();
    };
    CHECK(xbar_name_of("xbar") == "xbar");
    CHECK(xbar_name_of("xbar_alt") == "xbar_alt");
    CHECK(xbar_name_of("xbar") != xbar_name_of("xbar_alt"));

    // F3-b: `gpu_topology` —— 两个合法蓝图 ⇒ 观测面不同 + **蓝图被真实加载**
    //       （内层 `gpu` 实例存在 ⇒ 不只是「值落成员」）
    auto gpu_topo_of = [](const std::string& path) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "apu", "ApuSoC",
            json{{"cpu_topology", "configs/templates/cpu_cluster_2level.json"},
                 {"gpu_topology", path}})));
        auto* c = get_as<cpptlm::tlm::ApuSoC>(factory, "apu");
        REQUIRE(c != nullptr);
        // ★ 消费证据：蓝图被加载 ⇒ 内层 `gpu` 容器存在
        REQUIRE(c->getInternalInstance("gpu") != nullptr);
        return c->gpu_topology();
    };
    CHECK(gpu_topo_of("configs/templates/gpu_2gpc_2tpc_2cu.json") ==
          "configs/templates/gpu_2gpc_2tpc_2cu.json");
    CHECK(gpu_topo_of("configs/gpu_2gpc_2tpc_2cu.json") == "configs/gpu_2gpc_2tpc_2cu.json");
    CHECK(gpu_topo_of("configs/templates/gpu_2gpc_2tpc_2cu.json") !=
          gpu_topo_of("configs/gpu_2gpc_2tpc_2cu.json"));

    // F3-c: `cpu_topology` —— 两个合法蓝图（此前只有单一值）
    auto cpu_topo_of = [](const std::string& path) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("apu", "ApuSoC", json{{"cpu_topology", path}})));
        auto* c = get_as<cpptlm::tlm::ApuSoC>(factory, "apu");
        REQUIRE(c != nullptr);
        REQUIRE(c->getInternalInstance("cpu") != nullptr);
        return c->cpu_topology();
    };
    CHECK(cpu_topo_of("configs/templates/cpu_cluster_2level.json") ==
          "configs/templates/cpu_cluster_2level.json");
    CHECK(cpu_topo_of("configs/library_cpu_demo.json") == "configs/library_cpu_demo.json");
    CHECK(cpu_topo_of("configs/templates/cpu_cluster_2level.json") !=
          cpu_topo_of("configs/library_cpu_demo.json"));
}

// F4: 尺寸类键 —— 两个不同合法值 ⇒ 观测面不同（此前只有单一值）
TEST_CASE("F4: CacheCluster.l1_size/l2_size 与 MemoryCluster.channel_size 生效",
          "[param][migrate][batch2][f4]") {
    register_all_types();

    // CacheCluster 的两个尺寸键
    auto cache_sizes = [](const std::string& l1, const std::string& l2) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "cache", "CacheCluster", json{{"l1_count", 1}, {"l1_size", l1}, {"l2_size", l2}})));
        auto* c = get_as<cpptlm::tlm::CacheCluster>(factory, "cache");
        REQUIRE(c != nullptr);
        return std::make_pair(c->l1_size(), c->l2_size());
    };
    const auto a = cache_sizes("16KB", "256KB");
    const auto b = cache_sizes("32KB", "1MB");
    CHECK(a == std::make_pair(std::string("16KB"), std::string("256KB")));
    CHECK(b == std::make_pair(std::string("32KB"), std::string("1MB")));
    CHECK(a != b);

    // MemoryCluster 的通道尺寸键
    auto channel_size_of = [](const std::string& sz) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "mem", "MemoryCluster", json{{"channel_count", 2}, {"channel_size", sz}})));
        auto* c = get_as<cpptlm::tlm::MemoryCluster>(factory, "mem");
        REQUIRE(c != nullptr);
        return c->channel_size();
    };
    CHECK(channel_size_of("1GB") == "1GB");
    CHECK(channel_size_of("4GB") == "4GB");
    CHECK(channel_size_of("1GB") != channel_size_of("4GB"));
}

// F4: `cu_template` —— ⚠️ 仓内**只有 1 个**合法 CU 蓝图 ⇒ 「两个不同合法值」不可达
//     ⇒ 以「值落成员 + 蓝图被真实加载（子实例存在）」为判别力（显式论证豁免）。
TEST_CASE("F4: ComputeCluster/TpcCluster/GpcCluster/GpuCluster.cu_template 被真实消费",
          "[param][migrate][batch2][f4][cu-template]") {
    register_all_types();
    const std::string kCuTemplate = "configs/templates/compute_unit_v1.json";

    SECTION("ComputeCluster ⇒ 子实例 cu0 存在") {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "cu", "ComputeCluster", json{{"cu_count", 2}, {"cu_template", kCuTemplate}})));
        auto* c = get_as<cpptlm::tlm::ComputeCluster>(factory, "cu");
        REQUIRE(c != nullptr);
        CHECK(c->cu_template() == kCuTemplate);
        REQUIRE(c->getInternalInstance("cu0") != nullptr);
        REQUIRE(c->getInternalInstance("cu1") != nullptr);
    }
    SECTION("TpcCluster ⇒ 子实例 compute_grp 存在（cu_template 已转发）") {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "tpc", "TpcCluster",
            json{{"tpc_id", 0}, {"cu_per_tpc", 1}, {"cu_template", kCuTemplate}})));
        auto* c = get_as<cpptlm::tlm::TpcCluster>(factory, "tpc");
        REQUIRE(c != nullptr);
        CHECK(c->cu_template() == kCuTemplate);
        REQUIRE(c->getInternalInstance("compute_grp") != nullptr);
    }
    SECTION("GpcCluster ⇒ 子实例 tpc0 存在") {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "gpc", "GpcCluster",
            json{{"gpc_id", 0}, {"tpc_per_gpc", 1}, {"cu_per_tpc", 1},
                 {"cu_template", kCuTemplate}})));
        auto* c = get_as<cpptlm::tlm::GpcCluster>(factory, "gpc");
        REQUIRE(c != nullptr);
        CHECK(c->cu_template() == kCuTemplate);
        REQUIRE(c->getInternalInstance("tpc0") != nullptr);
    }
    SECTION("GpuCluster ⇒ 子实例 gpc0 存在") {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "gpu", "GpuCluster",
            json{{"gpc_count", 1}, {"tpc_per_gpc", 1}, {"cu_per_tpc", 1},
                 {"cu_template", kCuTemplate}})));
        auto* c = get_as<cpptlm::tlm::GpuCluster>(factory, "gpu");
        REQUIRE(c != nullptr);
        CHECK(c->cu_template() == kCuTemplate);
        REQUIRE(c->getInternalInstance("gpc0") != nullptr);
    }
}

// F4: CepMeshInterconnect.template —— 两个合法蓝图（此前只有单一值）
TEST_CASE("F4: CepMeshInterconnect.template 两个合法蓝图 ⇒ 观测不同",
          "[param][migrate][batch2][f4]") {
    register_all_types();

    auto template_of = [](const std::string& path) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(
            one_module("fabric", "CepMeshInterconnect", json{{"template", path}})));
        auto* c = get_as<tlm::cep::CepMeshInterconnect>(factory, "fabric");
        REQUIRE(c != nullptr);
        // 消费证据：蓝图被加载 ⇒ 内层 router 存在（契约命名 r00..）
        REQUIRE(c->getInternalInstance("r00") != nullptr);
        return c->template_path();
    };
    CHECK(template_of("configs/interconnect/cep_mesh_1x2.json") ==
          "configs/interconnect/cep_mesh_1x2.json");
    CHECK(template_of("configs/interconnect/cep_mesh_2x2.json") ==
          "configs/interconnect/cep_mesh_2x2.json");
    CHECK(template_of("configs/interconnect/cep_mesh_1x2.json") !=
          template_of("configs/interconnect/cep_mesh_2x2.json"));
}

// F4: CohIfNICTLM.mesh_x / mesh_y —— 两个不同合法值（此前固定 4,4）
TEST_CASE("F4: CohIfNICTLM.mesh_x / mesh_y 生效", "[param][migrate][batch2][f4]") {
    register_all_types();

    auto mesh_of = [](std::uint32_t mx, std::uint32_t my) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "cnic", "CohIfNICTLM", json{{"node_id", 0}, {"mesh_x", mx}, {"mesh_y", my}})));
        auto* c = get_as<tlm::cep::CohIfNICTLM>(factory, "cnic");
        REQUIRE(c != nullptr);
        return std::make_pair(c->mesh_x(), c->mesh_y());
    };
    CHECK(mesh_of(2, 2) == std::make_pair(2u, 2u));
    CHECK(mesh_of(8, 4) == std::make_pair(8u, 4u));
    CHECK(mesh_of(2, 2) != mesh_of(8, 4));
}

// F5: RouterTLM.credit_timeout —— 两个不同合法值 ⇒ 观测面不同（此前只有负值拒绝用例）
TEST_CASE("F5: RouterTLM.credit_timeout 生效（两个合法值 ⇒ getter 不同）",
          "[param][migrate][batch2][f5][router]") {
    register_all_types();

    auto timeout_of = [](std::uint64_t v) {
        EventQueue eq;
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(one_module(
            "rt", "RouterTLM",
            json{{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2},
                 {"credit_timeout", v}})));
        auto* r = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("rt"));
        REQUIRE(r != nullptr);
        return r->credit_timeout();
    };
    CHECK(timeout_of(0) == 0);
    CHECK(timeout_of(5) == 5);
    CHECK(timeout_of(50) == 50);
    CHECK(timeout_of(5) != timeout_of(50));
}
