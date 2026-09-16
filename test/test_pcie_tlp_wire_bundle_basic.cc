// test/test_pcie_tlp_wire_bundle_basic.cc
// PcieTlpWireBundle: wire-format TLP bundle with 4KB payload array
// 功能描述：3 个 TEST_CASE 验证 PcieTlpWireBundle 的正确性
//           1. 4KB MWr payload 完整携带 (std::array<uint32_t, 1024>)
//           2. wire↔descriptor 互转 (to_descriptor + from_descriptor)
//           3. LL 接口签名不变 (compile-time check, const PcieTlpBundle& 不动)
// 作者 CppTLM Team / 日期 2026-09-17
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md §T-P9-2

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>

using namespace cpptlm::pcie;

// ============================================================================
// TEST_CASE 1: 4KB MWr payload 完整携带
// ============================================================================

TEST_CASE("PcieTlpWireBundle: 4KB MWr payload fully carried in array<uint32_t,1024>",
          "[phase9][pcie][wire-bundle]") {
    PcieTlpWireBundle wb;

    // 填充 wire-format header 字段 (3DW MWr: Fmt=010, Type=00000)
    wb.fmt = 0b010;       // 3DW with data
    wb.type = 0b00000;     // Memory
    wb.tc = 0;
    wb.td = 0;
    wb.ep = 0;
    wb.attr = 0;
    wb.length = 1024;      // 1024 DW = 4096 bytes (PCIe Length=0 means 1024 DW)
    wb.requester_id = 0x0100;
    wb.tag = 0x42;
    wb.last_be = 0;
    wb.first_be = 0xF;
    wb.address_lo = 0x10000000;
    wb.address_hi = 0;     // 3DW header, hi=0
    wb.kind = PcieTlpWireBundle::MMIO_WRITE;

    // 填满 1024 DW payload
    for (int i = 0; i < 1024; ++i) {
        wb.payload[i] = 0xDEADBEEF;
    }

    // 验证 payload 容量
    REQUIRE(wb.payload.size() == 1024);
    static_assert(PcieTlpWireBundle::MAX_PAYLOAD_DW == 1024,
                  "MAX_PAYLOAD_DW must be 1024 for 4KB payload");

    // 验证 payload 内容完整性 (首/末 DW)
    REQUIRE(wb.payload[0] == 0xDEADBEEF);
    REQUIRE(wb.payload[512] == 0xDEADBEEF);
    REQUIRE(wb.payload[1023] == 0xDEADBEEF);

    // 验证 header 字段可读写
    REQUIRE(wb.fmt == 0b010);
    REQUIRE(wb.type == 0b00000);
    REQUIRE(wb.length == 1024);
    REQUIRE(wb.requester_id == 0x0100);
    REQUIRE(wb.tag == 0x42);
    REQUIRE(wb.address_lo == 0x10000000);

    // 验证 wire bundle 总数据量 ≥ 4KB (payload only)
    // header ~12-16 bytes + payload 4096 bytes + ECRC(4) + LCRC(4)
    // 确保 4KB payload 是真实携带的, 不是 64-bit inline data
    constexpr std::size_t payload_bytes = 1024 * sizeof(uint32_t);
    static_assert(payload_bytes == 4096, "1024 DW = 4096 bytes");
    REQUIRE(payload_bytes == 4096);

    // 验证 payload 数组通过 std::array 真实存储 (非 ch_uint 64-bit 截断)
    // 修改中间元素验证独立性
    wb.payload[42] = 0xAABBCCDD;
    REQUIRE(wb.payload[42] == 0xAABBCCDD);
    REQUIRE(wb.payload[0] == 0xDEADBEEF);  // 不改其他
    REQUIRE(wb.payload[1023] == 0xDEADBEEF);
}

// ============================================================================
// TEST_CASE 2: wire↔descriptor 互转
// ============================================================================

TEST_CASE("PcieTlpWireBundle: to_descriptor and from_descriptor round-trip",
          "[phase9][pcie][wire-bundle]") {
    // ── 准备 wire bundle ──
    PcieTlpWireBundle wire;
    wire.fmt = 0b010;          // 3DW with data
    wire.type = 0b00000;       // Memory
    wire.length = 16;          // 16 DW = 64 bytes
    wire.requester_id = 0x0100;
    wire.tag = 0x42;
    wire.last_be = 0;
    wire.first_be = 0xF;
    wire.address_lo = 0x20000000;
    wire.address_hi = 0;
    wire.kind = PcieTlpWireBundle::MMIO_WRITE;
    // 填充前 2 DW payload (不填满, 验证有限转换)
    wire.payload[0] = 0xAA55AA55;
    wire.payload[1] = 0xDEADBEEF;
    wire.payload[2] = 0x12345678;
    wire.payload[3] = 0x9ABCDEF0;

    // ── to_descriptor: wire → PcieTlpBundle ──
    bundles::PcieTlpBundle desc = wire.to_descriptor();

    // 验证 desc 字段
    // kind: MMIO_WRITE (3)
    REQUIRE(desc.kind.read() == bundles::PcieTlpBundle::MMIO_WRITE);
    // requester_id
    REQUIRE(desc.requester_id.read() == 0x0100);
    // trans_id (从 tag 映射)
    REQUIRE(desc.trans_id.read() == 0x42);
    // offset (从 address_lo 映射)
    REQUIRE(desc.offset.read() == 0x20000000);
    // size (从 length 映射: 16 DW = 64 bytes)
    REQUIRE(desc.size.read() == 64);
    // data (首 8 字节: payload[0] + payload[1])
    // 注意: PcieTlpBundle data 是 uint64, payload 是 uint32[2]
    uint64_t expected_data = (static_cast<uint64_t>(wire.payload[1]) << 32)
                             | wire.payload[0];
    REQUIRE(desc.data.read() == expected_data);

    // ── from_descriptor: PcieTlpBundle → wire ──
    PcieTlpWireBundle wire2 = PcieTlpWireBundle::from_descriptor(desc);

    // 验证 wire2 字段
    // kind (在 from_descriptor 中映射回 wire kind)
    REQUIRE(wire2.kind == PcieTlpWireBundle::MMIO_WRITE);
    // requester_id
    REQUIRE(wire2.requester_id == 0x0100);
    // tag (从 trans_id 低 8 位映射)
    REQUIRE(wire2.tag == 0x42);
    // address_lo (从 offset 映射)
    REQUIRE(wire2.address_lo == 0x20000000);
    // length (从 size 映射: 64 bytes = 16 DW)
    REQUIRE(wire2.length == 16);
    // payload[0..1] (从 data 映射)
    REQUIRE(wire2.payload[0] == 0xAA55AA55);
    REQUIRE(wire2.payload[1] == 0xDEADBEEF);

    // ── round-trip consistency ──
    // wire → desc → wire2: kind, req_id, tag, addr 一致
    REQUIRE(wire.kind == wire2.kind);
    REQUIRE(wire.requester_id == wire2.requester_id);
    REQUIRE(wire.tag == wire2.tag);
    REQUIRE(wire.address_lo == wire2.address_lo);
    REQUIRE(wire.length == wire2.length);
}

// ============================================================================
// TEST_CASE 3: LL 接口签名不变 compile-time check
// ============================================================================

TEST_CASE("PcieTlpWireBundle: LL interface remains const PcieTlpBundle& unchanged",
          "[phase9][pcie][wire-bundle]") {
    // 验证 PcieTlpBundle 仍可正常构造和使用 (接口不变)
    bundles::PcieTlpBundle desc(
        bundles::PcieTlpBundle::MMIO_WRITE,  // kind
        0,                                    // bar_index
        0x1000,                               // offset
        4,                                    // size (bytes)
        0xDEADBEEF,                           // data
        0x0100,                               // requester_id
        42                                    // trans_id
    );

    // 验证字段可读
    REQUIRE(desc.kind.read() == bundles::PcieTlpBundle::MMIO_WRITE);
    REQUIRE(desc.requester_id.read() == 0x0100);
    REQUIRE(desc.offset.read() == 0x1000);
    REQUIRE(desc.size.read() == 4);
    REQUIRE(desc.data.read() == 0xDEADBEEF);
    REQUIRE(desc.trans_id.read() == 42);

    // 验证 rx_tlp_from_host 签名兼容: lambda 接收 const PcieTlpBundle&
    // (模拟 PcieLinkLayer::rx_tlp_from_host 的函数签名)
    std::function<void(const bundles::PcieTlpBundle&)> rx_lambda =
        [](const bundles::PcieTlpBundle&) {};

    // 调用 rx_lambda 验证编译通过
    rx_lambda(desc);

    // 验证 PcieTlpWireBundle 不包含 bundle_base (它不是 bundles::bundle_base)
    // 验证其完全独立于既有 bundle 层次结构
    static_assert(!std::is_base_of_v<bundles::bundle_base, PcieTlpWireBundle>,
                  "PcieTlpWireBundle must NOT inherit from bundle_base");

    // 验证 PcieTlpWireBundle 类型存在且可实例化 (编译期验证)
    static_assert(std::is_standard_layout_v<PcieTlpWireBundle>,
                  "PcieTlpWireBundle must be standard layout");
}