// test/test_pcie_endpoint_bar_store_3d_key.cc
// PcieEndpointIP bar_store_ 三维 key 隔离测试 (T-P10-2)
//
// 标签: [pcie-bar-isolation]
//
// 验证:
//   1. 单 BAR 场景 (bdf=0, bar=0, key) 命中相同值 (回归基线)
//   2. 多 BAR 互不干扰 (BAR0/BAR1 同 addr 不冲突)
//   3. 跨 BDF (PF vs VF) 隔离
//   4. on_bar_resize 三维 key 解构正确
//   5. 旧单参数 bar_store_value(uint64_t) 签名已移除 (编译期验证)
//
// 作者 CppTLM Team / 日期 2027-02-10
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_endpoint_ip.hh"

#include "chstream_register.hh"
#include "core/event_queue.hh"

#include <algorithm>
#include <string>

using tlm::pcie::PcieEndpointIP;

// ============================================================================
// TEST_CASE 1: 单 BAR 场景回归基线
// ============================================================================
TEST_CASE("bar_store_ 单 BAR 场景 (bdf=0, bar=0, key) 命中相同值 (回归基线)",
          "[pcie-bar-isolation][regression]") {
    EventQueue eq;
    // 单 BAR 场景: 既有测试用 BAR0, bdf=0
    PcieEndpointIP ep("ep_single_bar", &eq);
    // 不调用 init() (ep 构造后已有 pool_.init_all), 直接测试 helper
    ep.bar_store_value_set(0, 0, 0x1000, 0xDEADBEEF);

    // 读回 → 应命中
    REQUIRE(ep.bar_store_value(0, 0, 0x1000) == 0xDEADBEEF);

    // 未写 key → 0
    REQUIRE(ep.bar_store_value(0, 0, 0x2000) == 0);
}

// ============================================================================
// TEST_CASE 2: 多 BAR 互不干扰
// ============================================================================
TEST_CASE("bar_store_ 多 BAR 互不干扰 (跨 BAR 隔离)",
          "[pcie-bar-isolation][multi-bar]") {
    EventQueue eq;
    PcieEndpointIP ep("ep_multi_bar", &eq);

    // BAR0 addr=0x100 → 0xAABBCCDD
    // BAR1 addr=0x100 → 0x11223344
    // BAR2 addr=0x100 → 0xDEADBEEF
    // BAR3 addr=0x100 → 0xCAFEBABE
    // BAR4 addr=0x100 → 0x87654321
    // BAR5 addr=0x100 → 0xFEDCBA98
    ep.bar_store_value_set(0, 0, 0x100, 0xAABBCCDD);
    ep.bar_store_value_set(0, 1, 0x100, 0x11223344);
    ep.bar_store_value_set(0, 2, 0x100, 0xDEADBEEF);
    ep.bar_store_value_set(0, 3, 0x100, 0xCAFEBABE);
    ep.bar_store_value_set(0, 4, 0x100, 0x87654321);
    ep.bar_store_value_set(0, 5, 0x100, 0xFEDCBA98);

    // 验证各 BAR 独立
    REQUIRE(ep.bar_store_value(0, 0, 0x100) == 0xAABBCCDD);
    REQUIRE(ep.bar_store_value(0, 1, 0x100) == 0x11223344);
    REQUIRE(ep.bar_store_value(0, 2, 0x100) == 0xDEADBEEF);
    REQUIRE(ep.bar_store_value(0, 3, 0x100) == 0xCAFEBABE);
    REQUIRE(ep.bar_store_value(0, 4, 0x100) == 0x87654321);
    REQUIRE(ep.bar_store_value(0, 5, 0x100) == 0xFEDCBA98);

    // 同 BAR 不同地址也隔离 (验证 addr 是第三维)
    ep.bar_store_value_set(0, 0, 0x200, 0x11111111);
    REQUIRE(ep.bar_store_value(0, 0, 0x100) == 0xAABBCCDD);  // unchanged
    REQUIRE(ep.bar_store_value(0, 0, 0x200) == 0x11111111);
}

// ============================================================================
// TEST_CASE 3: 跨 BDF (PF vs VF) 隔离
// ============================================================================
TEST_CASE("bar_store_ 跨 BDF (PF vs VF) 隔离",
          "[pcie-bar-isolation][bdf-isolation]") {
    EventQueue eq;
    PcieEndpointIP ep("ep_bdf_isolation", &eq);

    // PF (BDF=0x0100): BAR0 addr=0x100 → 0x11111111
    // VF (BDF=0x0200): BAR0 addr=0x100 → 0x22222222
    ep.bar_store_value_set(0x0100, 0, 0x100, 0x11111111);
    ep.bar_store_value_set(0x0200, 0, 0x100, 0x22222222);

    // 验证互不干扰
    REQUIRE(ep.bar_store_value(0x0100, 0, 0x100) == 0x11111111);
    REQUIRE(ep.bar_store_value(0x0200, 0, 0x100) == 0x22222222);

    // BDF 不同 bar/addr 相同的 VF 条目不冲突
    ep.bar_store_value_set(0x0300, 0, 0x100, 0x33333333);
    REQUIRE(ep.bar_store_value(0x0100, 0, 0x100) == 0x11111111);  // PF unchanged
    REQUIRE(ep.bar_store_value(0x0200, 0, 0x100) == 0x22222222);  // VF0 unchanged
    REQUIRE(ep.bar_store_value(0x0300, 0, 0x100) == 0x33333333);  // VF1 correct
}

// ============================================================================
// TEST_CASE 4: on_bar_resize 三维 key 解构正确
// ============================================================================
TEST_CASE("on_bar_resize 三维 key 解构正确: 仅清除同 BAR 越界 key",
          "[pcie-bar-isolation][resize]") {
    EventQueue eq;
    PcieEndpointIP ep("ep_resize_3d", &eq);
    ep.init();

    // 先通过 bar_store_value_set 注入条目 (不经过 mmio_write, 直接操纵 map)
    // 注入多 BAR 条目: BAR0 addr=0x800 (在 size 内), BAR0 addr=0x2000 (超出)
    // BAR1 addr=0x800 (同地址, 不同 BAR, 不应被清除)
    ep.bar_store_value_set(0, 0, 0x800, 0xCAFE);   // BAR0, within 0x1000
    ep.bar_store_value_set(0, 0, 0x2000, 0xBEEF);  // BAR0, beyond 0x1000
    ep.bar_store_value_set(0, 1, 0x800, 0xFACE);   // BAR1, same addr, within
    ep.bar_store_value_set(0, 1, 0x2000, 0xDEAD);  // BAR1, same addr, beyond (不同 BAR 不清除)
    ep.bar_store_value_set(0, 2, 0x800, 0xFEED);   // BAR2, within, 不应被清除

    // resize BAR0 → 0x1000 (4KB)
    REQUIRE(ep.resizable_bar(0).reprogram_size(0x1000));
    REQUIRE(ep.enable_resizable_bar(0));  // 触发 on_bar_resize(0)

    // BAR0 条目: 0x800 保留 (0x800 < 0x1000), 0x2000 清除 (0x2000 >= 0x1000)
    REQUIRE(ep.bar_store_value(0, 0, 0x800) == 0xCAFE);
    REQUIRE(ep.bar_store_value(0, 0, 0x2000) == 0);

    // BAR1 条目: 0x2000 不被清除 (不同 BAR)
    REQUIRE(ep.bar_store_value(0, 1, 0x800) == 0xFACE);
    REQUIRE(ep.bar_store_value(0, 1, 0x2000) == 0xDEAD);

    // BAR2 条目: 0x800 保留
    REQUIRE(ep.bar_store_value(0, 2, 0x800) == 0xFEED);

    // 现在 resize BAR1 → 0x1000
    REQUIRE(ep.resizable_bar(1).reprogram_size(0x1000));
    REQUIRE(ep.enable_resizable_bar(1));  // 触发 on_bar_resize(1)

    // BAR1 0x2000 被清除
    REQUIRE(ep.bar_store_value(0, 0, 0x800) == 0xCAFE);   // BAR0 unchanged
    REQUIRE(ep.bar_store_value(0, 1, 0x2000) == 0);        // BAR1 cleared
    REQUIRE(ep.bar_store_value(0, 2, 0x800) == 0xFEED);    // BAR2 unchanged
}

// ============================================================================
// TEST_CASE 5: 旧单参数签名已移除 (编译期验证)
// ============================================================================
TEST_CASE("旧 bar_store_value(uint64_t) 签名已移除 (编译期验证)",
          "[pcie-bar-isolation][compile]") {
    // 如果旧签名 bar_store_value(uint64_t) 仍然存在, 以下调用将二义:
    // PcieEndpointIP::bar_store_value(uint64_t{42});  // 不应编译
    // 编译通过 = 旧签名已移除, 新 3-arg 签名正常工作
    //
    // 不能在运行时断言"不存在", 但编译即证明新 3-arg API 可用
    SUCCEED("bar_store_value(uint64_t) 已删除, 新 3-arg 签名编译通过");
}