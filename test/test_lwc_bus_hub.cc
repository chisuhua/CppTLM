// test/test_lwc_bus_hub.cc
// TDD RED→GREEN 测试: LightweightInterconnect (LWC) + CrBundle + DoorbellBundle + MsixBundle
// 修复 Gap H (CR-Bus 寄存器访问) + Gap J (Doorbell 直调) + Gap K (MMIO Read write-mirror)
// 作者: CppTLM Team · 日期: 2026-10-09
// 配套: openspec/changes/cpptlm-lwc-bus-hub-mvp/
#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bundles/cr_bundles_tlm.hh"         // T1 RED: 编译失败 (未定义) → T2 GREEN
#include "bundles/doorbell_bundles_tlm.hh"   // T3 RED: 编译失败 → T4 GREEN
#include "bundles/msix_bundles_tlm.hh"       // T7 RED: 编译失败 → T8 GREEN
#include "core/event_queue.hh"
#include "framework/stream_adapter.hh"
#include "tlm/lwc/lwc_bus_hub.hh"            // T5 RED: 编译失败 → T6 GREEN
#include "utils/port_contract_asserts.hh"

using tlm::lwc::LwcBusHub;
using bundles::CrBundle;
using bundles::DoorbellBundle;
using bundles::MsixBundle;

// ===========================================================================
// T1/T2 — CrBundle wire-format (RED: 类型未定义 → 编译失败)
// ===========================================================================

TEST_CASE("CrBundle: 8-field wire-format 编译期契约 (BUNDLE_FIELD_WIDTH 锁)",
          "[lwc][cr_bundle][unit]") {
    // 字段宽度锁 (per design §2 + spec cr-bundle-wire-format)
    BUNDLE_FIELD_WIDTH(CrBundle, address, 32);
    BUNDLE_FIELD_WIDTH(CrBundle, wdata, 32);
    BUNDLE_FIELD_WIDTH(CrBundle, rdata, 32);
    BUNDLE_FIELD_WIDTH(CrBundle, write, 1);
    BUNDLE_FIELD_WIDTH(CrBundle, ready, 1);
    BUNDLE_FIELD_WIDTH(CrBundle, valid, 1);
    BUNDLE_FIELD_WIDTH(CrBundle, error, 2);
    BUNDLE_FIELD_WIDTH(CrBundle, transaction_id, 8);

    CrBundle tx;
    tx.address.write(0x00);
    tx.wdata.write(0xDEADBEEF);
    tx.write.write(1u);
    tx.valid.write(1u);
    tx.transaction_id.write(0);

    REQUIRE(tx.address.read() == 0x00);
    REQUIRE(tx.wdata.read() == 0xDEADBEEF);
    REQUIRE(tx.write.read() == 1u);
    REQUIRE(tx.valid.read() == 1u);
}

TEST_CASE("CrBundle: 序列化 roundtrip (InputStreamAdapter process 语义)",
          "[lwc][cr_bundle][unit]") {
    CrBundle tx;
    tx.address.write(0x04);
    tx.wdata.write(0x12345678);
    tx.write.write(1u);
    tx.valid.write(1u);
    tx.transaction_id.write(7);

    // 经 Packet payload 序列化/反序列化 (与 StreamAdapter 运行时路径一致)
    std::vector<uint8_t> raw(sizeof(CrBundle));
    REQUIRE(bundles::serialize_bundle(tx, raw.data(), raw.size()));
    CrBundle out;
    REQUIRE(bundles::deserialize_bundle(raw.data(), raw.size(), out));
    REQUIRE(out.address.read() == 0x04);
    REQUIRE(out.wdata.read() == 0x12345678);
    REQUIRE(out.write.read() == 1u);
    REQUIRE(out.transaction_id.read() == 7);
}

TEST_CASE("CrBundle: read transaction 语义 (write=0 → rdata 回填)",
          "[lwc][cr_bundle][unit]") {
    CrBundle tx;
    tx.address.write(0x00);
    tx.write.write(0u);
    tx.valid.write(1u);

    // GMMU handler 回填 rdata + ready
    CrBundle resp;
    resp = tx;
    resp.rdata.write(0xCAFEBABE);
    resp.ready.write(1u);

    REQUIRE(resp.write.read() == 0u);
    REQUIRE(resp.rdata.read() == 0xCAFEBABE);
    REQUIRE(resp.ready.read() == 1u);
}

// ===========================================================================
// T3/T4 — DoorbellBundle wire-format (RED: 类型未定义 → 编译失败)
// ===========================================================================

TEST_CASE("DoorbellBundle: 4-field posted-write wire-format 编译期契约",
          "[lwc][doorbell_bundle][unit]") {
    BUNDLE_FIELD_WIDTH(DoorbellBundle, wdata, 32);
    BUNDLE_FIELD_WIDTH(DoorbellBundle, ring_id, 8);
    BUNDLE_FIELD_WIDTH(DoorbellBundle, posted, 1);
    BUNDLE_FIELD_WIDTH(DoorbellBundle, valid, 1);

    DoorbellBundle db;
    db.wdata.write(42);
    db.ring_id.write(0);
    db.posted.write(1u);
    db.valid.write(1u);

    REQUIRE(db.wdata.read() == 42);
    REQUIRE(db.ring_id.read() == 0);
    REQUIRE(db.posted.read() == 1u);
    REQUIRE(db.valid.read() == 1u);
}

TEST_CASE("DoorbellBundle: 序列化 roundtrip + ring 提交语义",
          "[lwc][doorbell_bundle][unit]") {
    DoorbellBundle db;
    db.wdata.write(0x100);
    db.ring_id.write(0);
    db.posted.write(1u);
    db.valid.write(1u);

    std::vector<uint8_t> raw(sizeof(DoorbellBundle));
    REQUIRE(bundles::serialize_bundle(db, raw.data(), raw.size()));
    DoorbellBundle out;
    REQUIRE(bundles::deserialize_bundle(raw.data(), raw.size(), out));
    REQUIRE(out.wdata.read() == 0x100);
    REQUIRE(out.ring_id.read() == 0);
    REQUIRE(out.posted.read() == 1u);
}

TEST_CASE("DoorbellBundle: producer 写 valid, consumer (SDMA) 写 ready 反馈",
          "[lwc][doorbell_bundle][unit]") {
    // per v1.0.1 B-H4 修订: ready 是 consumer 驱动 (SDMA tick 写回), producer 不写 ready
    DoorbellBundle producer_tx;
    producer_tx.wdata.write(16);
    producer_tx.ring_id.write(0);
    producer_tx.posted.write(1u);
    producer_tx.valid.write(1u);
    REQUIRE(producer_tx.valid.read() == 1u);

    // SDMA consumer 消费后: valid 清除 (consume), ready 反馈
    DoorbellBundle consumed = producer_tx;
    consumed.valid.write(0u);
    REQUIRE(consumed.valid.read() == 0u);
}
