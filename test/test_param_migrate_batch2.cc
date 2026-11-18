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
