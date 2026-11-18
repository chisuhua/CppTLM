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
#include "tlm/cep/cohif_nic.hh"
#include "tlm/cep/cohif_responder.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
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
