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

// ===========================================================================
// T5/T6 — LightweightInterconnect 路由 (RED: LwcBusHub 未定义 → 编译失败)
// ===========================================================================

TEST_CASE("LwcBusHub: 空注册状态 (routed=0, dropped=0)", "[lwc][unit]") {
    EventQueue eq;
    LwcBusHub hub("lwc", &eq);
    REQUIRE(hub.routed_count() == 0);
    REQUIRE(hub.dropped_count() == 0);
    REQUIRE(hub.cr_target_count() == 0);
    REQUIRE(hub.db_target_count() == 0);
}

TEST_CASE("LwcBusHub: cr_write 路由到正确 CR target (不串扰)", "[lwc][unit]") {
    EventQueue eq;
    LwcBusHub hub("lwc", &eq);
    cpptlm::InputStreamAdapter<CrBundle> gmmu_cr;
    cpptlm::InputStreamAdapter<CrBundle> msi_cr;
    cpptlm::InputStreamAdapter<DoorbellBundle> sdma_db;

    REQUIRE(hub.register_cr_target("gmmu", &gmmu_cr));
    REQUIRE(hub.register_cr_target("msi_x", &msi_cr));
    REQUIRE(hub.register_doorbell_target("sdma", &sdma_db));

    hub.cr_write(CrBundle::make_write(0x00, 0xDEADBEEF, 1));

    REQUIRE(hub.routed_count() == 1);
    REQUIRE(gmmu_cr.valid());
    REQUIRE(gmmu_cr.data().address.read() == 0x00);
    REQUIRE(gmmu_cr.data().wdata.read() == 0xDEADBEEF);
    REQUIRE_FALSE(msi_cr.valid());   // 不串扰
    REQUIRE_FALSE(sdma_db.valid());  // Doorbell target 不收 CR
}

TEST_CASE("LwcBusHub: doorbell_write 路由到正确 Doorbell target", "[lwc][unit]") {
    EventQueue eq;
    LwcBusHub hub("lwc", &eq);
    cpptlm::InputStreamAdapter<CrBundle> gmmu_cr;
    cpptlm::InputStreamAdapter<DoorbellBundle> sdma_db;
    REQUIRE(hub.register_cr_target("gmmu", &gmmu_cr));
    REQUIRE(hub.register_doorbell_target("sdma", &sdma_db));

    hub.doorbell_write(DoorbellBundle::make_doorbell(42, 0));

    REQUIRE(hub.routed_count() == 1);
    REQUIRE(sdma_db.valid());
    REQUIRE(sdma_db.data().wdata.read() == 42);
    REQUIRE(sdma_db.data().ring_id.read() == 0);
    REQUIRE_FALSE(gmmu_cr.valid());  // CR target 不收 Doorbell
}

TEST_CASE("LwcBusHub: unknown target → dropped_count 增, 0 crash", "[lwc][unit]") {
    EventQueue eq;
    LwcBusHub hub("lwc", &eq);
    cpptlm::InputStreamAdapter<CrBundle> gmmu_cr;
    hub.register_cr_target("gmmu", &gmmu_cr);

    hub.cr_write(CrBundle::make_write(0x00, 1, 1));
    REQUIRE(hub.routed_count() == 1);

    hub.cr_write(CrBundle::make_write(0x00, 2, 2));  // 无 target 名 → 仍默认路由 "gmmu"
    REQUIRE(hub.routed_count() == 2);

    hub.doorbell_write(DoorbellBundle::make_doorbell(1, 0));  // 无 Doorbell target → dropped
    REQUIRE(hub.dropped_count() == 1);
    REQUIRE(hub.routed_count() == 2);
}

TEST_CASE("LwcBusHub: 32-port 上限 + 拒绝 33 注册 (fail soft)", "[lwc][unit]") {
    EventQueue eq;
    LwcBusHub hub("lwc", &eq);
    cpptlm::InputStreamAdapter<CrBundle> targets[33];
    bool ok = true;
    for (unsigned i = 0; i < 32; ++i) {
        ok = hub.register_cr_target("ip" + std::to_string(i), &targets[i]);
        REQUIRE(ok);
    }
    // 第 33 个 → fail soft (返回 false, 不抛)
    REQUIRE_FALSE(hub.register_cr_target("ip32", &targets[32]));
    REQUIRE(hub.cr_target_count() == 32);
}

TEST_CASE("LwcBusHub: cr_read 同步 AR/R 返回 rdata (ready/valid handshake)", "[lwc][unit]") {
    EventQueue eq;
    LwcBusHub hub("lwc", &eq);
    cpptlm::InputStreamAdapter<CrBundle> gmmu_cr;
    hub.register_cr_target("gmmu", &gmmu_cr);

    // host 侧发起同步 CR 读
    auto fut = hub.cr_read(CrBundle::make_read(0x00, 5));
    // sim 侧: GMMU tick 处理 → 回填 rdata + ready
    REQUIRE(gmmu_cr.valid());
    CrBundle resp = gmmu_cr.data();
    resp.rdata.write(0xCAFEBABE);
    resp.ready.write(1u);
    gmmu_cr.set_valid(false);
    hub.complete_cr_read(resp);

    REQUIRE(fut.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready);
    CrBundle out = fut.get();
    REQUIRE(out.rdata.read() == 0xCAFEBABE);
    REQUIRE(out.ready.read() == 1u);
}

TEST_CASE("LwcBusHub: routed_count 计数器精度 (100 tx: 90 routed + 10 dropped)", "[lwc][unit]") {
    EventQueue eq;
    LwcBusHub hub("lwc", &eq);
    cpptlm::InputStreamAdapter<CrBundle> gmmu_cr;
    hub.register_cr_target("gmmu", &gmmu_cr);

    for (unsigned i = 0; i < 90; ++i) {
        hub.cr_write(CrBundle::make_write(0x00, i, i));
    }
    for (unsigned i = 0; i < 10; ++i) {
        hub.doorbell_write(DoorbellBundle::make_doorbell(i, 0));  // 无 doorbell target
    }
    REQUIRE(hub.routed_count() == 90);
    REQUIRE(hub.dropped_count() == 10);
}
