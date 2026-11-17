// test/test_param_effect_landing.cc
// change `cpptlm-module-param-effect-landing` 的 **B1 批次**判据
//   AC-01/AC-02：逐键裁定（态① 真实消费 / 态② 显式拒绝）
//   AC-03：态① 的**行为判别**测试（两个不同合法值 ⇒ 可观测指标不同 —— ADR-X.17 D-6）
//   AC-04：态② 的**显式失败**测试（配置写了该键 ⇒ 装配期失败且错误含键名）
//
// B1 批次 = ADR-X.17 F-14 的 5 类 + 扩展键（design D-1）：
//   CacheTLM       6 键（size / level / associativity / assoc / line_size / protocol）
//                  ⛔ **不在本 change 范围** —— 该类是**冻结面**（`ADR-CEP-15` IC1:
//                  `cache_tlm.hh` / `cache_tlm.cc` 相对 CEP 基线**零 diff**，闸 =
//                  `test/test_cep_module_factory_seam.cc:133-134`）⇒ 需 architect 裁决
//                  解冻;本 change 只登记（proposal 已知残留 R-4）
//   MemoryTLM      6 键（capacity_gb / size / read_latency / write_latency ⇒ 态①;type / mem_type ⇒
//   态②） TrafficGenTLM  8 键（pattern / num_requests / start_addr / end_addr / hotspot_addrs /
//                        hotspot_weights / stride / seed）⇒ 全部态①
//   GmmuTLM        1 键（page_size_bytes）⇒ 态①
//   LwcBusHub      1 键（max_ip_ports）⇒ 态①
//   ---------------------------------------------------------------
//   合计 16 键（态① 13 / 态② 3）+ 6 键待裁决（CacheTLM 冻结面）
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
#include "tlm/cache_tlm.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/lwc/lwc_bus_hub.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/traffic_gen_tlm.hh"

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
