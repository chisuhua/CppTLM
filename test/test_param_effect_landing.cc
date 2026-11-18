// test/test_param_effect_landing.cc
// change `cpptlm-module-param-effect-landing` 的 **B1 批次**判据
//   AC-01/AC-02：逐键裁定（态① 真实消费 / 态② 显式拒绝）
//   AC-03：态① 的**行为判别**测试（两个不同合法值 ⇒ 可观测指标不同 —— ADR-X.17 D-6）
//   AC-04：态② 的**显式失败**测试（配置写了该键 ⇒ 装配期失败且错误含键名）
//
// B1 批次 = ADR-X.17 F-14 的 5 类 + 扩展键（design D-1）：
//   CacheTLM       6 键（size / level / associativity / assoc / line_size ⇒ 态①;
//                  protocol ⇒ 态② 显式拒绝）
//                  ✅ **已解冻并落地** —— per `ADR-CEP-15` Status Update 2026-11-18
//                  （决策 2 改判：文件 0 diff 冻结 ⇒ **协议语义边界**冻结）+ 台账 Entry #478
//   MemoryTLM      6 键（capacity_gb / size / read_latency / write_latency ⇒ 态①;type / mem_type ⇒
//   态②） TrafficGenTLM  8 键（pattern / num_requests / start_addr / end_addr / hotspot_addrs /
//                        hotspot_weights / stride / seed）⇒ 全部态①
//   GmmuTLM        1 键（page_size_bytes）⇒ 态①
//   LwcBusHub      1 键（max_ip_ports）⇒ 态①
//   ---------------------------------------------------------------
//   合计 22 键（态① 18 / 态② 4）
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/param_errors.hh"
#include "tlm/arbiter_tlm.hh"
#include "tlm/cache_tlm.hh"
#include "tlm/cluster/memory_cluster.hh"
#include "tlm/cluster/soc_internal_noc.hh"
#include "tlm/coherent_xbar_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"
#include "tlm/gpu/pcie_memory_device.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"
#include "tlm/lwc/lwc_bus_hub.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/pcie/pcie_controller_ip.hh"
#include "tlm/router_tlm.hh"
#include "tlm/traffic_gen_tlm.hh"
#include "tlm/vram_controller_tlm.hh"

using json = nlohmann::json;

namespace {

    /// 断言 `fn()` 抛 ParamValidationError 且 `param_name == key`
    template <class F> void expect_param_error(F&& fn, const char* key) {
        try {
            fn();
            FAIL("expected cpptlm::ParamValidationError for key '" << key << "'");
        } catch (const cpptlm::ParamValidationError& e) {
            CHECK(e.param_name == key);
            CHECK(!std::string(e.what()).empty());
        }
    }

    /// 驱动 TrafficGenTLM 采集最多 `max_samples` 个已发出请求的地址
    std::vector<uint64_t> collect_addrs(TrafficGenTLM& gen, int ticks, std::size_t max_samples) {
        std::vector<uint64_t> out;
        for (int i = 0; i < ticks && out.size() < max_samples; ++i) {
            gen.tick();
            if (gen.req_out().valid()) {
                out.push_back(gen.req_out().data().address.read());
                gen.req_out().clear_valid();
            }
        }
        return out;
    }

    /// 驱动 TrafficGenTLM 直到 `max_requests` 个请求发出（返回实际发出数）
    std::size_t count_issued(TrafficGenTLM& gen, int ticks) {
        std::size_t n = 0;
        for (int i = 0; i < ticks; ++i) {
            gen.tick();
            if (gen.req_out().valid()) {
                ++n;
                gen.req_out().clear_valid();
            }
        }
        return n;
    }

} // namespace

// ============================================================================
// AC-04 —— MemoryTLM 的 type / mem_type ⇒ 显式失败（态②）;size ↔ capacity_gb 互斥
// ============================================================================
TEST_CASE("AC-04: MemoryTLM 的 type / mem_type ⇒ 装配期显式失败", "[param][effect][memory]") {
    EventQueue eq;

    SECTION("type") {
        MemoryTLM m("m", &eq);
        expect_param_error([&] { m.set_config(json{{"type", "HBM2"}}); }, "type");
    }
    SECTION("mem_type") {
        MemoryTLM m("m", &eq);
        expect_param_error([&] { m.set_config(json{{"mem_type", "DDR4"}}); }, "mem_type");
    }
    SECTION("size 与 capacity_gb 同时出现 ⇒ 失败（同一量的两个真值）") {
        MemoryTLM m("m", &eq);
        expect_param_error([&] { m.set_config(json{{"size", "2GB"}, {"capacity_gb", 2}}); },
                           "size");
    }
    SECTION("空配置 ⇒ 不失败") {
        MemoryTLM m("m", &eq);
        CHECK_NOTHROW(m.set_config(json::object()));
    }
}

// ============================================================================
// AC-03 —— MemoryTLM：read_latency / write_latency 生效（timing 模式测真实延迟）
// ============================================================================
namespace {

    /// 注入一个请求并逐周期推进，返回「响应可见」所需的周期数（> 上限则返回 0）
    uint64_t cycles_until_resp(MemoryTLM& mem, uint64_t tid, uint64_t addr, bool wr) {
        bundles::CacheReqBundle req;
        req.transaction_id.write(tid);
        req.address.write(addr);
        req.is_write.write(wr ? 1 : 0);
        req.data.write(0);
        req.size.write(8);
        mem.req_in().consume();
        std::memcpy(&mem.req_in().data(), &req, sizeof(req));
        mem.req_in().set_valid(true);

        mem.tick(); // 消费请求 ⇒ 排入 pending_resps_
        if (mem.resp_out().valid()) {
            mem.resp_out().clear_valid();
            return 0;
        }
        for (uint64_t i = 1; i <= 500; ++i) {
            mem.advance_cycle();
            mem.tick();
            if (mem.resp_out().valid()) {
                mem.resp_out().clear_valid();
                return i;
            }
        }
        return 0;
    }

} // namespace

TEST_CASE("AC-03: MemoryTLM 的 read_latency / write_latency 生效", "[param][effect][memory]") {
    EventQueue eq;

    // 读延迟：7 ⇒ 7 周期；30 ⇒ 30 周期（两个不同合法值 ⇒ 可观测指标不同）
    MemoryTLM m1("m1", &eq);
    m1.set_use_zero_delay_for_test(false);
    m1.set_config(json{{"read_latency", 7}, {"write_latency", 3}});
    CHECK(cycles_until_resp(m1, 1, 0x10, /*wr=*/false) == 7u);
    CHECK(cycles_until_resp(m1, 2, 0x10, /*wr=*/true) == 3u);

    MemoryTLM m2("m2", &eq);
    m2.set_use_zero_delay_for_test(false);
    m2.set_config(json{{"read_latency", 30}, {"write_latency", 11}});
    CHECK(cycles_until_resp(m2, 3, 0x10, /*wr=*/false) == 30u);
    CHECK(cycles_until_resp(m2, 4, 0x10, /*wr=*/true) == 11u);
}

TEST_CASE("AC-03: MemoryTLM 的 size（带单位）生效 —— 收窄容量后越界报错",
          "[param][effect][memory]") {
    EventQueue eq;

    std::vector<uint8_t> backing(4096, 0);

    SECTION("未配置 size ⇒ 容量 = backing 大小 ⇒ 0x800 可访问") {
        MemoryTLM m("m", &eq);
        m.set_backing_view(backing.data(), backing.size());
        m.set_config(json::object());
        bundles::CacheReqBundle req;
        req.transaction_id.write(1);
        req.address.write(0x800);
        req.is_write.write(0);
        req.data.write(0);
        req.size.write(8);
        m.req_in().consume();
        std::memcpy(&m.req_in().data(), &req, sizeof(req));
        m.req_in().set_valid(true);
        m.tick();
        REQUIRE(m.resp_out().valid());
        CHECK(m.resp_out().data().error_code.read() == 0);
    }

    SECTION("size = \"2KB\" ⇒ 容量 2048 ⇒ 0x800 越界") {
        MemoryTLM m("m2", &eq);
        m.set_backing_view(backing.data(), backing.size());
        m.set_config(json{{"size", "2KB"}});
        bundles::CacheReqBundle req;
        req.transaction_id.write(2);
        req.address.write(0x800);
        req.is_write.write(0);
        req.data.write(0);
        req.size.write(8);
        m.req_in().consume();
        std::memcpy(&m.req_in().data(), &req, sizeof(req));
        m.req_in().set_valid(true);
        m.tick();
        REQUIRE(m.resp_out().valid());
        CHECK(m.resp_out().data().error_code.read() == 1); // OUT_OF_RANGE
    }
}

// ============================================================================
// AC-03 —— TrafficGenTLM：8 个键逐个生效
// ============================================================================
TEST_CASE("AC-03: TrafficGenTLM pattern 生效（SEQUENTIAL 步长4 / HOTSPOT 只在热点集）",
          "[param][effect][traffic]") {
    EventQueue eq;

    SECTION("SEQUENTIAL ⇒ 地址按 4 递增并回绕") {
        TrafficGenTLM g("g", &eq);
        g.set_config(json{{"pattern", "SEQUENTIAL"},
                          {"num_requests", 50},
                          {"start_addr", "0x1000"},
                          {"end_addr", "0x1010"}});
        const auto a = collect_addrs(g, 2000, 6);
        REQUIRE(a.size() == 6u);
        CHECK(a[0] == 0x1000u);
        CHECK(a[1] == 0x1004u);
        CHECK(a[2] == 0x1008u);
        CHECK(a[3] == 0x100cu);
        CHECK(a[4] == 0x1000u); // 回绕
    }

    SECTION("HOTSPOT ⇒ 地址只取自 hotspot 集合（与 SEQUENTIAL 可观测不同）") {
        TrafficGenTLM g("g2", &eq);
        g.set_config(json{{"pattern", "HOTSPOT"},
                          {"num_requests", 50},
                          {"hotspot_addrs", json::array({"0x2000", "0x4000"})},
                          {"hotspot_weights", json::array({50, 50})}});
        const auto a = collect_addrs(g, 2000, 10);
        REQUIRE(a.size() == 10u);
        for (uint64_t x : a) {
            CHECK((x == 0x2000u || x == 0x4000u));
        }
    }
}

TEST_CASE("AC-03: TrafficGenTLM num_requests 生效", "[param][effect][traffic]") {
    EventQueue eq;
    TrafficGenTLM g5("g5", &eq);
    g5.set_config(json{{"pattern", "SEQUENTIAL"}, {"num_requests", 5}});
    CHECK(count_issued(g5, 3000) == 5u);

    TrafficGenTLM g25("g25", &eq);
    g25.set_config(json{{"pattern", "SEQUENTIAL"}, {"num_requests", 25}});
    CHECK(count_issued(g25, 3000) == 25u);
}

TEST_CASE("AC-03: TrafficGenTLM start_addr / end_addr 生效", "[param][effect][traffic]") {
    EventQueue eq;

    TrafficGenTLM ga("ga", &eq);
    ga.set_config(json{{"pattern", "SEQUENTIAL"},
                       {"num_requests", 50},
                       {"start_addr", "0x1000"},
                       {"end_addr", "0x1010"}});
    const auto a = collect_addrs(ga, 2000, 4);
    REQUIRE(a.size() == 4u);
    for (uint64_t x : a)
        CHECK((x >= 0x1000u && x < 0x1010u));

    TrafficGenTLM gb("gb", &eq);
    gb.set_config(json{{"pattern", "SEQUENTIAL"},
                       {"num_requests", 50},
                       {"start_addr", "0x8000"},
                       {"end_addr", "0x8010"}});
    const auto b = collect_addrs(gb, 2000, 4);
    REQUIRE(b.size() == 4u);
    for (uint64_t x : b)
        CHECK((x >= 0x8000u && x < 0x8010u));
    CHECK(a != b);
}

TEST_CASE("AC-03: TrafficGenTLM hotspot_addrs / hotspot_weights 生效", "[param][effect][traffic]") {
    EventQueue eq;

    TrafficGenTLM g1("h1", &eq);
    g1.set_config(json{{"pattern", "HOTSPOT"},
                       {"num_requests", 50},
                       {"hotspot_addrs", json::array({"0x2000"})},
                       {"hotspot_weights", json::array({100})}});
    const auto s1 = collect_addrs(g1, 2000, 6);
    REQUIRE(s1.size() == 6u);
    for (uint64_t x : s1)
        CHECK(x == 0x2000u);

    TrafficGenTLM g2("h2", &eq);
    g2.set_config(json{{"pattern", "HOTSPOT"},
                       {"num_requests", 50},
                       {"hotspot_addrs", json::array({"0x6000"})},
                       {"hotspot_weights", json::array({100})}});
    const auto s2 = collect_addrs(g2, 2000, 6);
    REQUIRE(s2.size() == 6u);
    for (uint64_t x : s2)
        CHECK(x == 0x6000u);
    CHECK(s1 != s2);
}

TEST_CASE("AC-03: TrafficGenTLM stride 生效", "[param][effect][traffic]") {
    EventQueue eq;

    auto sample = [&eq](const char* name, uint64_t stride) {
        TrafficGenTLM g(name, &eq);
        g.set_config(json{{"pattern", "STRIDED"},
                          {"num_requests", 50},
                          {"start_addr", "0x1000"},
                          {"end_addr", "0x2000"},
                          {"stride", stride}});
        return collect_addrs(g, 2000, 6);
    };
    const auto s16 = sample("s16", 16);
    const auto s64 = sample("s64", 64);
    REQUIRE(s16.size() == 6u);
    REQUIRE(s64.size() == 6u);
    CHECK(s16 != s64);
    CHECK(s64[1] - s64[0] == 64u);
    CHECK(s16[1] - s16[0] == 16u);
}

TEST_CASE("AC-03: TrafficGenTLM seed 生效（可复现）", "[param][effect][traffic]") {
    EventQueue eq;
    auto sample = [&eq](const char* name, uint64_t seed) {
        TrafficGenTLM g(name, &eq);
        g.set_config(json{{"pattern", "RANDOM"},
                          {"num_requests", 50},
                          {"start_addr", "0x1000"},
                          {"end_addr", "0x100000"},
                          {"seed", seed}});
        return collect_addrs(g, 2000, 10);
    };
    const auto a1 = sample("r1", 1);
    const auto a2 = sample("r2", 1); // 同种子 ⇒ 同序列
    const auto b = sample("r3", 2);  // 不同种子 ⇒ 不同序列
    REQUIRE(a1.size() == 10u);
    CHECK(a1 == a2);
    CHECK(a1 != b);
}

TEST_CASE("AC-04: TrafficGenTLM 的键间一致性 ⇒ 不一致即显式失败", "[param][effect][traffic]") {
    EventQueue eq;

    SECTION("hotspot_addrs 但 pattern 不是 HOTSPOT") {
        TrafficGenTLM g("x1", &eq);
        expect_param_error(
            [&] {
                g.set_config(json{{"pattern", "SEQUENTIAL"},
                                  {"hotspot_addrs", json::array({"0x2000"})},
                                  {"hotspot_weights", json::array({100})}});
            },
            "hotspot_addrs");
    }
    SECTION("pattern = HOTSPOT 但缺 hotspot_addrs（否则策略内离散分布为空 ⇒ UB）") {
        TrafficGenTLM g("x2", &eq);
        expect_param_error([&] { g.set_config(json{{"pattern", "HOTSPOT"}}); }, "hotspot_addrs");
    }
    SECTION("stride 但 pattern 不是 STRIDED") {
        TrafficGenTLM g("x3", &eq);
        expect_param_error([&] { g.set_config(json{{"pattern", "SEQUENTIAL"}, {"stride", 16}}); },
                           "stride");
    }
    SECTION("未知 pattern 值") {
        TrafficGenTLM g("x4", &eq);
        expect_param_error([&] { g.set_config(json{{"pattern", "MESH"}}); }, "pattern");
    }
    SECTION("end_addr <= start_addr") {
        TrafficGenTLM g("x5", &eq);
        expect_param_error(
            [&] { g.set_config(json{{"start_addr", "0x2000"}, {"end_addr", "0x2000"}}); },
            "end_addr");
    }
    SECTION("num_requests 为负") {
        TrafficGenTLM g("x6", &eq);
        expect_param_error([&] { g.set_config(json{{"num_requests", -1}}); }, "num_requests");
    }
    SECTION("hotspot_weights 与 hotspot_addrs 数量不一致") {
        TrafficGenTLM g("x7", &eq);
        expect_param_error(
            [&] {
                g.set_config(json{{"pattern", "HOTSPOT"},
                                  {"hotspot_addrs", json::array({"0x2000", "0x4000"})},
                                  {"hotspot_weights", json::array({100})}});
            },
            "hotspot_weights");
    }
}

// ============================================================================
// AC-03 —— GmmuTLM：page_size_bytes 生效（同一 IOVA ⇒ 不同 paddr）
// ============================================================================
TEST_CASE("AC-03: GmmuTLM 的 page_size_bytes 生效", "[param][effect][gmmu]") {
    EventQueue eq;

    // 页表：PTE[0] → paddr page 0x2000;PTE[1] → paddr page 0x4000
    std::vector<uint8_t> mem(0x10000, 0);
    auto write_pte = [&mem](uint64_t pte_addr, uint64_t pte) {
        std::memcpy(mem.data() + pte_addr, &pte, sizeof(pte));
    };
    write_pte(0x1000 + 0 * 8, 0x2000ULL | 1ULL);
    write_pte(0x1000 + 1 * 8, 0x4000ULL | 1ULL);

    auto translate = [&eq, &mem](const char* name, uint64_t page_size) -> uint64_t {
        tlm::gpu::GmmuTLM g(name, &eq);
        g.set_enabled(true);
        g.set_pt_base_lo(0x1000);
        g.set_pt_base_hi(0);
        g.set_mem_view(mem.data(), mem.size());
        if (page_size)
            g.set_config(json{{"page_size_bytes", page_size}});
        uint64_t pa = 0;
        uint64_t lat = 0;
        const int rc = g.translate_timing(0x1800, 4, pa, lat);
        REQUIRE(rc == 0);
        return pa;
    };

    // 4KB：iova 0x1800 ⇒ page 1 ⇒ PTE[1] ⇒ paddr = 0x4000 | 0x800
    CHECK(translate("g4k", 4096) == 0x4800u);
    // 8KB：iova 0x1800 ⇒ page 0 ⇒ PTE[0] ⇒ paddr = 0x2000 | 0x1800
    CHECK(translate("g8k", 8192) == 0x3800u);
    // 默认（未配置）⇒ 4096 语义
    CHECK(translate("gdef", 0) == 0x4800u);
}

TEST_CASE("AC-04: GmmuTLM 的 page_size_bytes 非 2 的幂 ⇒ 显式失败", "[param][effect][gmmu]") {
    EventQueue eq;
    tlm::gpu::GmmuTLM g("g", &eq);
    expect_param_error([&] { g.set_config(json{{"page_size_bytes", 3000}}); }, "page_size_bytes");
    tlm::gpu::GmmuTLM g2("g2", &eq);
    expect_param_error([&] { g2.set_config(json{{"page_size_bytes", 1024}}); }, "page_size_bytes");
}

// ============================================================================
// AC-03 —— LwcBusHub：max_ip_ports 生效
// ============================================================================
TEST_CASE("AC-03: LwcBusHub 的 max_ip_ports 生效", "[param][effect][lwc]") {
    EventQueue eq;
    using CrAdapter = cpptlm::InputStreamAdapter<bundles::CrBundle>;
    using DbAdapter = cpptlm::InputStreamAdapter<bundles::DoorbellBundle>;

    SECTION("max_ip_ports = 2 ⇒ 第 3 个注册失败") {
        tlm::lwc::LwcBusHub hub("hub", &eq);
        hub.set_config(json{{"max_ip_ports", 2}});
        CHECK(hub.max_ip_ports() == 2u);
        CrAdapter a, b;
        DbAdapter c;
        CHECK(hub.register_cr_target("a", &a));
        CHECK(hub.register_cr_target("b", &b));
        CHECK_FALSE(hub.register_doorbell_target("c", &c));
        CHECK(hub.registered_ip_ports() == 2u);
    }

    SECTION("未配置 ⇒ 默认上限 32（零回归）") {
        tlm::lwc::LwcBusHub hub("hub2", &eq);
        CHECK(hub.max_ip_ports() == tlm::lwc::LwcBusHub::kMaxIpPorts);
        CrAdapter a, b;
        DbAdapter c;
        CHECK(hub.register_cr_target("a", &a));
        CHECK(hub.register_cr_target("b", &b));
        CHECK(hub.register_doorbell_target("c", &c));
    }

    SECTION("越界 max_ip_ports ⇒ 显式失败") {
        tlm::lwc::LwcBusHub hub("hub3", &eq);
        expect_param_error([&] { hub.set_config(json{{"max_ip_ports", 33}}); }, "max_ip_ports");
        tlm::lwc::LwcBusHub hub4("hub4", &eq);
        expect_param_error([&] { hub4.set_config(json{{"max_ip_ports", 0}}); }, "max_ip_ports");
    }
}

// ============================================================================
// AC-04 —— 「装配失败」的**端到端**取证（经 ModuleFactory，不是只测 set_config）
//
// ⚠️ 真实失败面：`ModuleFactory` 在装配阶段 catch 掉 `ParamValidationError`（`on_config_loaded`
//    返回 void，无其它失败通道）⇒ `instantiateAll` **返回 false**（`module_factory.cc`
//    的 `[CONFIG ERROR] on_config_loaded failed for ...` 分支）。异常文本经 `DPRINTF` 输出，
//    不经返回值 ⇒ 本 case 断言「返回 false」，「错误含键名」由各类型的直接 `set_config`
//    用例（本文件上半部分）取证。
// ============================================================================
TEST_CASE("AC-04: 配置里的无效果键 ⇒ 经 ModuleFactory 装配失败（返回 false）",
          "[param][effect][factory]") {
    auto make_cfg = [](const char* key, const json& value) {
        return json{
            {"modules", json::array({json{
                            {"name", "mem0"}, {"type", "MemoryTLM"}, {"params", {{key, value}}}}})},
            {"connections", json::array()}};
    };

    EventQueue eq;
    ModuleFactory factory(&eq);
    CHECK_FALSE(factory.instantiateAll(make_cfg("mem_type", "DDR4")));

    EventQueue eq2;
    ModuleFactory factory2(&eq2);
    CHECK_FALSE(factory2.instantiateAll(make_cfg("type", "HBM2")));

    // 反向：不带该键 ⇒ 可装配（确保上面不是「装配本来就失败」）
    EventQueue eq3;
    ModuleFactory factory3(&eq3);
    CHECK(factory3.instantiateAll(make_cfg("capacity_gb", 1)));
}

TEST_CASE("AC-03: 配置里的 size 经 ModuleFactory 生效（容量随之改变）",
          "[param][effect][factory]") {
    // 观测量 = 越界行为（`size_cap_` 无公开读访问器，且不为此新增观测面）：
    // 同一个 backing（4KB）+ 同一个地址 0x800 ⇒ "2KB" 越界、"4KB" 命中。
    auto cfg_with_size = [](const char* size) {
        return json{{"modules",
                     json::array({json{
                         {"name", "mem0"}, {"type", "MemoryTLM"}, {"params", {{"size", size}}}}})},
                    {"connections", json::array()}};
    };
    auto probe_error = [](MemoryTLM& mem) {
        bundles::CacheReqBundle req;
        req.transaction_id.write(1);
        req.address.write(0x800);
        req.is_write.write(0);
        req.data.write(0);
        req.size.write(8);
        mem.req_in().consume();
        std::memcpy(&mem.req_in().data(), &req, sizeof(req));
        mem.req_in().set_valid(true);
        mem.tick();
        REQUIRE(mem.resp_out().valid());
        const uint64_t ec = mem.resp_out().data().error_code.read();
        mem.resp_out().clear_valid();
        return ec;
    };

    std::vector<uint8_t> backing(4096, 0);

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg_with_size("2KB")));
    auto* mod = dynamic_cast<MemoryTLM*>(factory.getInstance("mem0"));
    REQUIRE(mod != nullptr);
    mod->set_backing_view(backing.data(), backing.size()); // size_cap_ 已由 size 定 ⇒ 不被覆盖
    CHECK(probe_error(*mod) == 1);                         // 越界

    EventQueue eq2;
    ModuleFactory factory2(&eq2);
    REQUIRE(factory2.instantiateAll(cfg_with_size("4KB")));
    auto* mod2 = dynamic_cast<MemoryTLM*>(factory2.getInstance("mem0"));
    REQUIRE(mod2 != nullptr);
    mod2->set_backing_view(backing.data(), backing.size());
    CHECK(probe_error(*mod2) == 0); // 命中
}

// ============================================================================
// AC-03 —— CacheTLM：容量模型 5 键逐个生效（态①）
//
// 观测量一律用**既有**运行时面（`stats` 的 hits / misses / latency 分布）——
// ⛔ 不为通过判别而新增「只为测试存在」的观测面（`ADR-X.17` D-6）。
// ============================================================================
namespace {

    /// 向 cache 注入一次访问（写 / 读）并 tick;返回响应
    bundles::CacheRespBundle drive_cache(CacheTLM& cache, uint64_t tid, uint64_t addr,
                                         bool is_write, uint64_t data = 0) {
        bundles::CacheReqBundle req;
        req.transaction_id.write(tid);
        req.address.write(addr);
        req.is_write.write(is_write ? 1 : 0);
        req.data.write(data);
        req.size.write(8);
        cache.req_in().consume();
        std::memcpy(&cache.req_in().data(), &req, sizeof(req));
        cache.req_in().set_valid(true);
        cache.tick();
        auto resp = cache.resp_out().data();
        cache.resp_out().clear_valid();
        return resp;
    }

    /// 命中 / 未命中计数（`stats` 内建面）
    std::pair<uint64_t, uint64_t> hit_miss(CacheTLM& cache) {
        auto* hits = static_cast<tlm_stats::Scalar*>(cache.stats().findStat("hits"));
        auto* misses = static_cast<tlm_stats::Scalar*>(cache.stats().findStat("misses"));
        return {hits ? hits->value() : 0, misses ? misses->value() : 0};
    }

    /// 延迟分布的最小值（命中延迟是分布里的最小样本 —— 未命中固定 50）
    uint64_t min_latency(CacheTLM& cache) {
        auto* lat = static_cast<tlm_stats::Distribution*>(cache.stats().findStat("latency"));
        return lat ? lat->min() : 0;
    }

} // namespace

TEST_CASE("AC-03: CacheTLM 的 size 生效（超容量 ⇒ 逐出 ⇒ 再访问变未命中）",
          "[param][effect][cache]") {
    EventQueue eq;

    // size = 1KB / line_size = 64 ⇒ 16 个槽 ⇒ 写入 17 个不同地址 ⇒ 首个被逐出
    CacheTLM c("c", &eq);
    c.set_config(json{{"size", "1KB"}, {"line_size", 64}});
    for (uint64_t i = 0; i < 17; ++i) {
        drive_cache(c, i + 1, i * 64, /*is_write=*/true, i + 1);
    }
    const auto before = hit_miss(c);
    // 地址 0（第 1 个写入）已被逐出 ⇒ 读它 ⇒ 未命中
    const auto resp = drive_cache(c, 100, 0, /*is_write=*/false);
    CHECK(resp.is_hit.read() == 0);
    const auto after = hit_miss(c);
    CHECK(after.second == before.second + 1); // misses +1
    CHECK(after.first == before.first);       // hits 不变

    // 对照：**未配 size** ⇒ 不限容量 ⇒ 第 1 个地址仍在 ⇒ 命中（零回归）
    CacheTLM u("u", &eq);
    u.set_config(json::object());
    for (uint64_t i = 0; i < 17; ++i) {
        drive_cache(u, i + 1, i * 64, /*is_write=*/true, i + 1);
    }
    const auto resp_u = drive_cache(u, 100, 0, /*is_write=*/false);
    CHECK(resp_u.is_hit.read() == 1);
}

TEST_CASE("AC-03: CacheTLM 的 associativity 生效（同组冲突 ⇒ 路数少者逐出）",
          "[param][effect][cache]") {
    EventQueue eq;

    // associativity = 1 ⇒ 每组 1 路 ⇒ 同组的第 2 个地址逐出第 1 个
    //   几何：size=1KB / line_size=64 ⇒ 16 槽;assoc=1 ⇒ 16 组(每组 1 路)
    //   地址 0x0 ⇒ 组 0;地址 0x400 ⇒ (0x400/64)=16 ⇒ 16%16=0 ⇒ **同组**
    CacheTLM a1("a1", &eq);
    a1.set_config(json{{"size", "1KB"}, {"line_size", 64}, {"associativity", 1}});
    drive_cache(a1, 1, 0x0, true, 0xAA);
    drive_cache(a1, 2, 0x400, true, 0xBB);
    CHECK(drive_cache(a1, 3, 0x0, false).is_hit.read() == 0); // 被逐出 ⇒ 未命中

    // associativity = 4 ⇒ 16 槽 / 4 组 ⇒ 每组 4 路 ⇒ 同组两个地址都留下 ⇒ 命中
    CacheTLM a4("a4", &eq);
    a4.set_config(json{{"size", "1KB"}, {"line_size", 64}, {"associativity", 4}});
    drive_cache(a4, 1, 0x0, true, 0xAA);
    drive_cache(a4, 2, 0x400, true, 0xBB);
    CHECK(drive_cache(a4, 3, 0x0, false).is_hit.read() == 1); // 仍在 ⇒ 命中

    // `assoc` 是 `associativity` 的**同义别名** ⇒ 行为等价
    CacheTLM alias("al", &eq);
    alias.set_config(json{{"size", "1KB"}, {"line_size", 64}, {"assoc", 4}});
    drive_cache(alias, 1, 0x0, true, 0xAA);
    drive_cache(alias, 2, 0x400, true, 0xBB);
    CHECK(drive_cache(alias, 3, 0x0, false).is_hit.read() == 1);
}

TEST_CASE("AC-03: CacheTLM 的 line_size 生效（改变分组 ⇒ 改变冲突行为）",
          "[param][effect][cache]") {
    EventQueue eq;

    // line_size = 64 ⇒ 槽 16 / 组 16 ⇒ 地址 0x0(组 0) 与 0x40(组 1) **不同组** ⇒ 不冲突
    CacheTLM l64("l64", &eq);
    l64.set_config(json{{"size", "1KB"}, {"line_size", 64}, {"associativity", 1}});
    drive_cache(l64, 1, 0x0, true, 0xAA);
    drive_cache(l64, 2, 0x40, true, 0xBB);
    CHECK(drive_cache(l64, 3, 0x0, false).is_hit.read() == 1);

    // line_size = 128 ⇒ 槽 8 / 组 8 ⇒ 地址 0x0(组 0) 与 0x40(组 0) **同组** ⇒ 冲突逐出
    CacheTLM l128("l128", &eq);
    l128.set_config(json{{"size", "1KB"}, {"line_size", 128}, {"associativity", 1}});
    drive_cache(l128, 1, 0x0, true, 0xAA);
    drive_cache(l128, 2, 0x40, true, 0xBB);
    CHECK(drive_cache(l128, 3, 0x0, false).is_hit.read() == 0);
}

TEST_CASE("AC-03: CacheTLM 的 level 生效（命中延迟随层级变）", "[param][effect][cache]") {
    EventQueue eq;

    // level 未指定 ⇒ 命中延迟 5（沿用原值,零回归）
    CacheTLM d("d", &eq);
    d.set_config(json::object());
    drive_cache(d, 1, 0x1000, true, 0xAA); // 未命中 ⇒ 50
    drive_cache(d, 2, 0x1000, false);      // 命中   ⇒ 5
    CHECK(min_latency(d) == 5u);

    CacheTLM l1("l1", &eq);
    l1.set_config(json{{"level", 1}});
    drive_cache(l1, 1, 0x1000, true, 0xAA);
    drive_cache(l1, 2, 0x1000, false);
    CHECK(min_latency(l1) == 5u);

    CacheTLM l2("l2", &eq);
    l2.set_config(json{{"level", 2}});
    drive_cache(l2, 1, 0x1000, true, 0xAA);
    drive_cache(l2, 2, 0x1000, false);
    CHECK(min_latency(l2) == 10u); // L2 比 L1 慢 ⇒ 可观测差异

    CacheTLM l3("l3", &eq);
    l3.set_config(json{{"level", 3}});
    drive_cache(l3, 1, 0x1000, true, 0xAA);
    drive_cache(l3, 2, 0x1000, false);
    CHECK(min_latency(l3) == 20u);
}

// ============================================================================
// AC-04 —— CacheTLM 的 protocol ⇒ 显式拒绝（协议语义不入本类）
// ============================================================================
TEST_CASE("AC-04: CacheTLM 的 protocol 键 ⇒ 显式失败（协议语义不入本类）",
          "[param][effect][cache]") {
    EventQueue eq;
    CacheTLM c("c", &eq);
    expect_param_error([&] { c.set_config(json{{"protocol", "MOESI"}}); }, "protocol");
}

// ============================================================================
// AC-04 —— CacheTLM 的几何键缺 `size` ⇒ 显式失败（否则该键零效果）
// ============================================================================
TEST_CASE("AC-04: CacheTLM 的几何键缺 size ⇒ 显式失败（不得静默零效果）",
          "[param][effect][cache]") {
    EventQueue eq;

    SECTION("line_size 单独出现") {
        CacheTLM c("c1", &eq);
        expect_param_error([&] { c.set_config(json{{"line_size", 64}}); }, "line_size");
    }
    SECTION("associativity 单独出现") {
        CacheTLM c("c2", &eq);
        expect_param_error([&] { c.set_config(json{{"associativity", 4}}); }, "associativity");
    }
    SECTION("assoc 单独出现") {
        CacheTLM c("c3", &eq);
        expect_param_error([&] { c.set_config(json{{"assoc", 4}}); }, "assoc");
    }
    SECTION("associativity 与 assoc 值不一致") {
        CacheTLM c("c4", &eq);
        expect_param_error(
            [&] { c.set_config(json{{"size", "1KB"}, {"associativity", 4}, {"assoc", 8}}); },
            "associativity");
    }
    SECTION("size 小于一个 line_size 单元") {
        CacheTLM c("c5", &eq);
        expect_param_error([&] { c.set_config(json{{"size", "32B"}, {"line_size", 64}}); }, "size");
    }
    SECTION("line_size 非 2 的幂") {
        CacheTLM c("c6", &eq);
        expect_param_error([&] { c.set_config(json{{"size", "1KB"}, {"line_size", 48}}); },
                           "line_size");
    }
    SECTION("size 非法文本") {
        CacheTLM c("c7", &eq);
        expect_param_error([&] { c.set_config(json{{"size", "1 gigabyte"}}); }, "size");
    }
    SECTION("空配置 ⇒ 不失败") {
        CacheTLM c("c8", &eq);
        CHECK_NOTHROW(c.set_config(json::object()));
    }
}

// ============================================================================
// AC-04 —— B2 批次（零读取面家族）：态② 显式拒绝的逐键判据
//
// 这些键的共性是「**该类不可能产生该语义**」：
//   · 端口数是**编译期常量**（`CrossbarTLM::NUM_PORTS` / `ArbiterTLM<N_PORTS>`）
//   · `SocInternalNoc` 已 archived（NoC 语义由 `GpuInternalFabric` 承担）
//   · `MemoryCluster` / `VramControllerTLM` 无对应模型（内存类型 / 运行期频率）
// ⇒ 按 ADR-X.17 D-2 落**态②**：配置写了即装配失败并报出键名（⛔ 不静默丢弃）。
// ============================================================================
TEST_CASE("AC-04: 端口数类键 ⇒ 显式失败（端口数是编译期常量）", "[param][effect][port-count]") {
    EventQueue eq;

    SECTION("CrossbarTLM.port_count") {
        CrossbarTLM c("c1", &eq);
        expect_param_error([&] { c.set_config(json{{"port_count", 4}}); }, "port_count");
    }
    SECTION("CrossbarTLM.ports") {
        CrossbarTLM c("c2", &eq);
        expect_param_error([&] { c.set_config(json{{"ports", 4}}); }, "ports");
    }
    SECTION("CoherentXBarTLM.ports（继承同行为）") {
        cpptlm::tlm::CoherentXBarTLM c("c3", &eq);
        expect_param_error([&] { c.set_config(json{{"ports", 8}}); }, "ports");
    }
    SECTION("ArbiterTLM4.n_ports") {
        ArbiterTLM<4> a("a1", &eq);
        expect_param_error([&] { a.set_config(json{{"n_ports", 4}}); }, "n_ports");
    }
    SECTION("ArbiterTLM4.ports（同族：端口数同样是编译期常量）") {
        ArbiterTLM<4> a("a2", &eq);
        expect_param_error([&] { a.set_config(json{{"ports", 4}}); }, "ports");
    }
    SECTION("空配置 ⇒ 不失败（正向对照）") {
        CrossbarTLM c("c4", &eq);
        CHECK_NOTHROW(c.set_config(json::object()));
    }
}

TEST_CASE("AC-04: SocInternalNoc 的两个键 ⇒ 显式失败（该类已 archived）", "[param][effect][noc]") {
    EventQueue eq;
    SECTION("data_flow") {
        tlm::cluster::SocInternalNoc n("n1", &eq);
        expect_param_error([&] { n.set_config(json{{"data_flow", true}}); }, "data_flow");
    }
    SECTION("vc_counts_enabled") {
        tlm::cluster::SocInternalNoc n("n2", &eq);
        expect_param_error([&] { n.set_config(json{{"vc_counts_enabled", true}}); },
                           "vc_counts_enabled");
    }
}

TEST_CASE("AC-04: MemoryCluster.memory_type ⇒ 显式失败（无内存类型模型）",
          "[param][effect][memory]") {
    EventQueue eq;
    cpptlm::tlm::MemoryCluster mc("mc", &eq);
    expect_param_error([&] { mc.set_config(json{{"memory_type", "DDR4"}}); }, "memory_type");

    // 正向对照：只给被消费的键 ⇒ 不失败
    cpptlm::tlm::MemoryCluster mc2("mc2", &eq);
    CHECK_NOTHROW(mc2.set_config(json{{"channel_count", 4}, {"channel_size", "2GB"}}));
}

TEST_CASE("AC-04: VramControllerTLM.operating_frequency_hz ⇒ 显式失败（编译期锚点）",
          "[param][effect][vram]") {
    EventQueue eq;
    VramControllerTLM v("v", &eq);
    expect_param_error([&] { v.set_config(json{{"operating_frequency_hz", 1000000000}}); },
                       "operating_frequency_hz");

    // 正向对照：板级 timing 键（由 `dgpu_board_shell.cc` 直读消费）不应被拒
    VramControllerTLM v2("v2", &eq);
    CHECK_NOTHROW(v2.set_config(json{{"bandwidth_gbps", 32}}));
}

TEST_CASE("AC-04: PcieMemoryDevice.capacity_gb ⇒ 显式失败（容量由注入 backing 决定）",
          "[param][effect][pcie-memory]") {
    EventQueue eq;
    // ⚠️ 该类的容量**不是配置说了算** —— backing 由 `DGpuBoard` 注入，
    //    bound = `backing_view_size_`（per ADR-DGPU-05 单一 VRAM 真源 + D-AXI v1.4 B8）
    //    ⇒ `capacity_gb` 不可能产生任何效果 ⇒ 态②（显式拒绝）。
    tlm::gpu::PcieMemoryDevice dev("mem0", &eq);
    expect_param_error([&] { dev.set_config(json{{"capacity_gb", 8}}); }, "capacity_gb");

    // 正向对照：无该键 ⇒ 不失败（防误伤）
    tlm::gpu::PcieMemoryDevice dev2("mem1", &eq);
    CHECK_NOTHROW(dev2.set_config(json::object()));
}

TEST_CASE("AC-04: PcieControllerIP 三键 ⇒ 显式失败（facade 从 EP 动态读 BAR，无本地消费面）",
          "[param][effect][pcie-controller]") {
    EventQueue eq;
    // ⚠️ 该 facade **有意**「Reads sizes dynamically from PcieEndpointIP (no static snapshot)」
    //    ⇒ 自己不存 BAR 表 ⇒ 三个键在本类零消费面 ⇒ 态②（显式拒绝）。
    //    ⚠️ **同名不同语义**：`pcie_ep` 节点的 `bar_sizes` 是 **PcieEndpointIP** 的键
    //    （**有**消费面：写 config space BAR 寄存器）⇒ 不得按键名一刀切。
    tlm::pcie::PcieControllerIP c("pc", &eq);
    expect_param_error([&] { c.set_config(json{{"bar_bases", json::array()}}); }, "bar_bases");
    expect_param_error([&] { c.set_config(json{{"bar_sizes", json::array()}}); }, "bar_sizes");
    expect_param_error([&] { c.set_config(json{{"link_speed_gts", 8}}); }, "link_speed_gts");

    // 正向对照：空配置 ⇒ 不失败（防误伤）
    tlm::pcie::PcieControllerIP c2("pc2", &eq);
    CHECK_NOTHROW(c2.set_config(json::object()));
}

TEST_CASE("AC-04: StreamingMultiprocessorTLM 两键 ⇒ 显式失败（无对应概念）",
          "[param][effect][sm]") {
    EventQueue eq;
    tlm::StreamingMultiprocessorTLM sm("sm", &eq);
    expect_param_error([&] { sm.set_config(json{{"execution_latency", 2}}); }, "execution_latency");
    expect_param_error([&] { sm.set_config(json{{"warp_size", 64}}); }, "warp_size");

    tlm::StreamingMultiprocessorTLM sm2("sm2", &eq);
    CHECK_NOTHROW(sm2.set_config(json::object()));
}

TEST_CASE("AC-04: CoherentXBarTLM 两键 ⇒ 显式失败（无协议状态机 / 无 snoop filter）",
          "[param][effect][coherent-xbar]") {
    EventQueue eq;
    cpptlm::tlm::CoherentXBarTLM x("x", &eq);
    expect_param_error([&] { x.set_config(json{{"domain_protocol", "MOESI_AMD"}}); },
                       "domain_protocol");
    expect_param_error([&] { x.set_config(json{{"snoop_filter", true}}); }, "snoop_filter");

    // ⚠️ 继承自 CrossbarTLM 的 `ports` 拒绝路径**不得**因覆写而失效
    expect_param_error([&] { x.set_config(json{{"ports", 8}}); }, "ports");

    cpptlm::tlm::CoherentXBarTLM x2("x2", &eq);
    CHECK_NOTHROW(x2.set_config(json::object()));
}

// ============================================================================
// A2b / A2c —— 态①（接真实消费）的行为判别测试 + 态③（伪参数）的非法值拒绝
//   per ADR-X.17 D-6「行为判别义务」：两个不同合法值 ⇒ 可观测指标必须不同。
//   ⚠️ 观测量用**已存在的**运行时面（D-2.1 禁造只为测试存在的观测面）。
// ============================================================================
TEST_CASE("AC-02: GpuMeshNoC.dim / hops_latency ⇒ 态①（D2D 延迟模型真消费）",
          "[param][effect][gpu-noc]") {
    EventQueue eq;
    std::vector<uint8_t> src(16, 0xAA), dst_a(16, 0), dst_b(16, 0);

    // hops_latency=1 vs 4 ⇒ 最坏路径延迟必须不同（route_latency = (|dx|+|dy|) × hops_latency）
    tlm::GpuMeshNoC a("a", &eq);
    a.set_config(json{{"hops_latency", 1}});
    a.d2d_forward_payload(dst_a.data(), src.data(), 16);
    const uint32_t lat1 = a.last_transfer_latency_cycles();

    tlm::GpuMeshNoC b("b", &eq);
    b.set_config(json{{"hops_latency", 4}});
    b.d2d_forward_payload(dst_b.data(), src.data(), 16);
    const uint32_t lat4 = b.last_transfer_latency_cycles();

    CHECK(lat4 > lat1);               // 判别力：值不同 ⇒ 行为不同
    CHECK(a.get_hops_latency() == 1); // 配置真被写入
    CHECK(b.get_hops_latency() == 4);

    // dim 同理（最坏路径 = {0,0}→{dim-1,dim-1}）
    tlm::GpuMeshNoC c("c", &eq);
    c.set_config(json{{"dim", 2}});
    tlm::GpuMeshNoC d("d", &eq);
    d.set_config(json{{"dim", 8}});
    CHECK(d.get_dim() == 8);
    CHECK(d.route_latency({0, 0}, {7, 7}) > c.route_latency({0, 0}, {1, 1}));
}

TEST_CASE("AC-05: CPUTLM.pattern ⇒ 态③（合法值唯一，非法值必须失败）",
          "[param][effect][cpu][state3]") {
    EventQueue eq;
    // 合法值：SEQUENTIAL ⇒ 通过
    CPUTLM ok("cpu_ok", &eq);
    CHECK_NOTHROW(ok.set_config(json{{"pattern", "SEQUENTIAL"}}));
    // 缺省（键缺失）⇒ 通过
    CPUTLM none("cpu_none", &eq);
    CHECK_NOTHROW(none.set_config(json::object()));
    // ⛔ 非法值 ⇒ 装配失败，且错误含键名 + 合法值集合
    CPUTLM bad("cpu_bad", &eq);
    expect_param_error([&] { bad.set_config(json{{"pattern", "RANDOM"}}); }, "pattern");
}

TEST_CASE("AC-02: CPUTLM.start_addr / request_interval ⇒ 态①（真被运行期读）",
          "[param][effect][cpu]") {
    EventQueue eq;

    // start_addr：配置改了 ⇒ 首个请求的地址必须随之改变（`cur_addr_` 须同步）
    CPUTLM c1("c1", &eq);
    c1.set_config(json{{"start_addr", 0x2000}});
    c1.tick(); // 发首个请求
    REQUIRE(c1.req_out().valid());
    CHECK(c1.req_out().data().address.read() == 0x2000);

    CPUTLM c2("c2", &eq);
    c2.set_config(json{{"start_addr", 0x3000}});
    c2.tick();
    REQUIRE(c2.req_out().valid());
    CHECK(c2.req_out().data().address.read() == 0x3000); // 判别力：不同值 ⇒ 不同地址

    // request_interval：间隔 1 与 10 ⇒ 同样 tick 数内发出的请求数必须不同
    // ⚠️ 观测量用**既有**的 `req_out().data().transaction_id`（每发一请求 +1），
    //    ⛔ 不为测试新增观测面（`ADR-X.17` D-2.1）。
    CPUTLM fast("fast", &eq);
    fast.set_config(json{{"request_interval", 1}});
    for (int i = 0; i < 20; ++i)
        fast.tick();

    CPUTLM slow("slow", &eq);
    slow.set_config(json{{"request_interval", 10}});
    for (int i = 0; i < 20; ++i)
        slow.tick();

    CHECK(fast.req_out().data().transaction_id.read() >
          slow.req_out().data().transaction_id.read());
}

TEST_CASE("AC-08: RouterTLM.routing ⇒ 态③（合法值唯一 XY，非法值必须失败）",
          "[param][effect][router][state3]") {
    EventQueue eq;
    // ⚠️ RouterTLM 的 `node_x` / `node_y` / `mesh_x` / `mesh_y` 是**必填** ⇒ 基准配置须带它们
    auto base = [] { return json{{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}}; };

    // 态③ 判据：合法值通过 / 缺省通过 / 非法值失败（错误含键名 + 合法值集合）
    tlm::RouterTLM ok("r_ok", &eq);
    {
        auto c = base();
        c["routing"] = "XY";
        CHECK_NOTHROW(ok.set_config(c));
    }

    tlm::RouterTLM none("r_none", &eq);
    CHECK_NOTHROW(none.set_config(base()));

    tlm::RouterTLM bad("r_bad", &eq);
    {
        auto c = base();
        c["routing"] = "YX";
        expect_param_error([&] { bad.set_config(c); }, "routing");
    }
}
