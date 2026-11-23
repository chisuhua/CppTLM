// test/test_router_buffer_size_param.cc
// 功能描述：`RouterTLM` 的 `buffer_size` param 真正生效（per change
//           `cpptlm-router-buffer-size-param` · D-1 默认值裁决取 8）。
//           覆盖 AC-01（容量生效）/ AC-02（默认深度 = 8）/ AC-03（credit 预算同源）/
//           AC-04（非法值装配期 fail）。
//
// ⚠️ **观测面边界**（per design D-3）：`input_buffer_` 是 private，且「容量判定」
//    在**稳态**下与深度无关（egress 1 flit/cycle 会持续消化积压 ⇒ 转发总数由时长决定）。
//    ⇒ 采用两条互补判据：
//      ① **直接**：`buffer_depth()` 只读访问器（param 已生效的直接证据）
//      ② **行为**：每 in_port 注入 K 个、K > C 时**必丢**（转发数 < 4K）——
//         4 个 in_port 汇聚到同一 LOCAL 出口 ⇒ 消费 1/cycle 远慢于注入 4/cycle
//         ⇒ 首轮即填满缓冲 ⇒ 超出部分被静默丢弃。
// 作者 CppTLM Team / 日期 2026-11-17
#include <cstdint>
#include "bundles/noc_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/router_tlm.hh"
#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

using namespace tlm;
using FlitBundle = bundles::NoCFlitBundle;
using json = nlohmann::json;

namespace {

    constexpr unsigned kLocal = static_cast<unsigned>(RouterPort::LOCAL);
    constexpr unsigned kEast = static_cast<unsigned>(RouterPort::EAST);
    /// 4 个 mesh 向 in_port（NORTH/EAST/SOUTH/WEST）—— 它们的 flit 汇聚到同一 out_port(LOCAL)
    constexpr unsigned kInjectPorts = 4;

    FlitBundle make_flit(std::uint64_t txid, std::uint32_t dst, unsigned vc) {
        FlitBundle f;
        f.transaction_id.write(txid);
        f.src_node.write(0);
        f.dst_node.write(dst);
        f.vc_id.write(vc);
        f.flit_type.write(FlitBundle::FLIT_HEAD_TAIL);
        f.flit_index.write(0);
        f.flit_count.write(1);
        f.flit_category.write(FlitBundle::CATEGORY_REQUEST);
        return f;
    }

    /// 从 4 个 in_port **每周期各注入 1 个** flit（`dst` = 本节点 ⇒ 全部路由到 LOCAL）：
    /// 注入 4 flit/cycle vs egress 1 flit/cycle ⇒ 净积压 ⇒ 每 buffer 超过 `buffer_depth_` 即丢弃。
    /// @param inject_per_port 每个 port 注入多少个（总量 = 4 × 该值）
    /// @return `resp_out[LOCAL]` 实际读到的 flit 数
    std::size_t inject_to_local(RouterTLM& r, std::size_t inject_per_port, unsigned vc = 0) {
        std::size_t per_port_injected[kInjectPorts] = {0, 0, 0, 0};
        std::size_t drained = 0;
        std::uint64_t txid = 100000;

        // 周期预算：注入阶段 4×K 周期 + 排空阶段（最多 4×K 个 flit，egress 1/cycle）
        const unsigned budget = static_cast<unsigned>(8 * inject_per_port + 128);
        for (unsigned cyc = 0; cyc < budget; ++cyc) {
            for (unsigned p = 0; p < kInjectPorts; ++p) {
                if (per_port_injected[p] < inject_per_port) {
                    r.req_in()[p].data() = make_flit(txid++, 0, vc);
                    r.req_in()[p].set_valid(true);
                    ++per_port_injected[p];
                }
            }
            r.tick();
            if (r.resp_out()[kLocal].valid()) {
                ++drained;
                r.resp_out()[kLocal].clear_valid();
            }
        }
        return drained;
    }

    /// 向 **mesh 向**（EAST 口）注入 —— 裸 router（`adapter_ == nullptr`）⇒ credit 永不归还
    /// ⇒ 转发数封顶于该口的 **credit 预算**（= `buffer_depth_`）
    std::size_t inject_to_east(RouterTLM& r, std::size_t inject_count) {
        std::size_t injected = 0;
        std::size_t drained = 0;
        std::uint64_t txid = 200000;

        for (unsigned cyc = 0; cyc < 4000 && drained < inject_count + 64; ++cyc) {
            if (injected < inject_count) {
                r.req_in()[0].data() = make_flit(txid++, 1, 0); // dst = node(1,0) ⇒ EAST
                r.req_in()[0].set_valid(true);
                ++injected;
            }
            r.tick();
            if (r.resp_out()[kEast].valid()) {
                ++drained;
                r.resp_out()[kEast].clear_valid();
            }
        }
        return drained;
    }

    /// 构造单 router 的最小 config（`params` 可注入 `buffer_size`）
    json make_router_config(bool with_buffer_size, int buffer_size = 0) {
        json cfg;
        json params{{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}};
        if (with_buffer_size)
            params["buffer_size"] = buffer_size;
        cfg["modules"] =
            json::array({json{{"name", "r0"}, {"type", "RouterTLM"}, {"params", params}}});
        cfg["connections"] = json::array();
        return cfg;
    }

} // namespace

TEST_CASE("BSP-1 (AC-02): 默认（未声明 buffer_size）⇒ 深度 == 8", "[router][buffer_size]") {
    EventQueue eq;
    RouterTLM router("r_def", &eq, 0, 0, 2, 2);

    // ① 直接证据：默认深度 == 8（= 参数声明默认值 = 实现默认值）
    REQUIRE(router.buffer_depth() == 8);

    // ② 行为：每 port 注入 8 个（恰好不超容量 8）⇒ 32 个**全部**转发（零丢弃）
    REQUIRE(inject_to_local(router, 8) == 32);
}

TEST_CASE("BSP-2 (AC-01): buffer_size 生效 ⇒ 容量随之改变", "[router][buffer_size]") {
    EventQueue eq;

    SECTION("buffer_size=2 ⇒ buffer_depth()==2 且 每 port 注入 8 个必有丢弃") {
        RouterTLM router("r_2", &eq, 0, 0, 2, 2);
        router.set_config(make_router_config(true, 2)["modules"][0]["params"]);

        // ① 直接证据
        REQUIRE(router.buffer_depth() == 2);

        // ② 行为：每 port 注入 8 个（8 > 容量 2）⇒ 必丢 ⇒ 转发数 < 32
        //    (改前 buffer_size 不生效时恒 8 ⇒ 该断言必红 ⇒ 有判别力)
        REQUIRE(inject_to_local(router, 8) < 32);
    }

    SECTION("buffer_size=16 ⇒ buffer_depth()==16 且 每 port 注入 8 个零丢弃") {
        RouterTLM router("r_16", &eq, 0, 0, 2, 2);
        router.set_config(make_router_config(true, 16)["modules"][0]["params"]);

        REQUIRE(router.buffer_depth() == 16);
        // 8 ≤ 16 ⇒ 不超容量 ⇒ 32 个全部转发
        REQUIRE(inject_to_local(router, 8) == 32);
    }

    SECTION("边界: buffer_size=1（合法下界）⇒ 深度 == 1") {
        RouterTLM router("r_1", &eq, 0, 0, 2, 2);
        router.set_config(make_router_config(true, 1)["modules"][0]["params"]);

        REQUIRE(router.buffer_depth() == 1);
    }
}

TEST_CASE("BSP-3 (AC-03): credit 预算与缓冲深度同源", "[router][buffer_size]") {
    EventQueue eq;

    SECTION("默认: mesh 向 credit 预算 == 8") {
        RouterTLM router("r_def", &eq, 0, 0, 2, 2);
        REQUIRE(inject_to_east(router, 64) == 8);
    }

    SECTION("buffer_size=4: mesh 向 credit 预算 == 4") {
        RouterTLM router("r_4", &eq, 0, 0, 2, 2);
        router.set_config(make_router_config(true, 4)["modules"][0]["params"]);

        REQUIRE(inject_to_east(router, 64) == 4);
    }
}

TEST_CASE("BSP-4 (AC-04): 非法 buffer_size 装配期 fail", "[router][buffer_size]") {
    EventQueue eq;

    SECTION("buffer_size = 0（下界外）⇒ 装配失败") {
        ModuleFactory factory(&eq);
        REQUIRE_FALSE(factory.instantiateAll(make_router_config(true, 0)));
    }

    SECTION("buffer_size = 65（上界外）⇒ 装配失败") {
        ModuleFactory factory(&eq);
        REQUIRE_FALSE(factory.instantiateAll(make_router_config(true, 65)));
    }

    SECTION("buffer_size = 非数字字符串 ⇒ 装配失败") {
        ModuleFactory factory(&eq);
        json cfg = make_router_config(false);
        cfg["modules"][0]["params"]["buffer_size"] = "abc";
        REQUIRE_FALSE(factory.instantiateAll(cfg));
    }

    SECTION("buffer_size = 合法值 ⇒ 装配成功") {
        ModuleFactory factory(&eq);
        REQUIRE(factory.instantiateAll(make_router_config(true, 32)));
        auto* r = dynamic_cast<RouterTLM*>(factory.getInstance("r0"));
        REQUIRE(r != nullptr);
        REQUIRE(r->buffer_depth() == 32);
    }
}
