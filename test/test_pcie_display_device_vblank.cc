// test/test_pcie_display_device_vblank.cc
// D1 v1.1.1 Step 5 (T2.1+T2.3): VBLANK timer 推进单元测试
//
// 覆盖 spec.md §"VBLANK timer 推进" 全场景:
//   - tick 1024 次 → STATUS.VBLANK_PENDING=1
//   - tick 10240 次 → vblank_count()==10
//   - INT_MASK=0 → STATUS=1 但 msix.update_pending(0) 不被调
//   - INT_MASK=1 → msix.update_pending(0) 被调 1 次
//   - Pending cleared via W1C
//   - INT_MASK enable 后 tick → update_pending 被调
//
// 标签: [pcie][display][vblank]
// 配套: openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/tasks.md Task 5

#include <cstdint>
#include <vector>

#include <catch_amalgamated.hpp>
#include "tlm/gpu/msix_table_mvp.hh"
#include "tlm/gpu/pcie_display_device.hh"

using namespace tlm::gpu;

// ============================================================================
// 1. VBLANK triggers every 1024 cycles (default INT_MASK=0)
// ============================================================================
TEST_CASE("PcieDisplayDevice VBLANK: triggers every 1024 cycles",
          "[pcie][display][vblank]") {
    PcieDisplayDevice dev;
    MsiXTable msix(1);

    // First 1023 ticks: no VBLANK
    for (int i = 0; i < 1023; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 0);
    REQUIRE(dev.cycle_counter() == 1023);

    uint8_t status = 0;
    REQUIRE(dev.mmio_read(PcieDisplayDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieDisplayDevice::kStatusVblankPending) == 0);

    // 1024th tick: VBLANK triggers, STATUS bit set
    dev.tick(msix);
    REQUIRE(dev.vblank_count() == 1);
    REQUIRE(dev.cycle_counter() == 0);  // counter reset

    REQUIRE(dev.mmio_read(PcieDisplayDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieDisplayDevice::kStatusVblankPending) ==
            PcieDisplayDevice::kStatusVblankPending);
}

// ============================================================================
// 2. VBLANK counter increments (10240 ticks → 10)
// ============================================================================
TEST_CASE("PcieDisplayDevice VBLANK: counter increments on each trigger",
          "[pcie][display][vblank]") {
    PcieDisplayDevice dev;
    MsiXTable msix(1);

    for (int i = 0; i < 10240; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 10);
}

// ============================================================================
// 3. INT_MASK=0 suppresses MSI-X (STATUS bit set, but no IRQ delivered)
// ============================================================================
TEST_CASE("PcieDisplayDevice VBLANK: INT_MASK=0 suppresses MSI-X",
          "[pcie][display][vblank]") {
    PcieDisplayDevice dev;
    MsiXTable msix(1);

    uint32_t mask = 0;
    REQUIRE(dev.mmio_write(PcieDisplayDevice::kRegIntMask, &mask, sizeof(mask)) == 0);
    REQUIRE_FALSE(msix.is_pba_set(0));

    for (int i = 0; i < 1024; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 1);

    uint8_t status = 0;
    REQUIRE(dev.mmio_read(PcieDisplayDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieDisplayDevice::kStatusVblankPending) ==
            PcieDisplayDevice::kStatusVblankPending);

    REQUIRE_FALSE(msix.is_pba_set(0));
}

// ============================================================================
// 4. INT_MASK=1 enables MSI-X delivery (device calls update_pending)
// ============================================================================
TEST_CASE("PcieDisplayDevice VBLANK: INT_MASK=1 enables MSI-X delivery",
          "[pcie][display][vblank]") {
    PcieDisplayDevice dev;
    MsiXTable msix(1);

    uint32_t mask = PcieDisplayDevice::kIntMaskVblankEnable;
    REQUIRE(dev.mmio_write(PcieDisplayDevice::kRegIntMask, &mask, sizeof(mask)) == 0);
    REQUIRE_FALSE(msix.is_pba_set(0));

    for (int i = 0; i < 1024; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 1);

    uint8_t status = 0;
    REQUIRE(dev.mmio_read(PcieDisplayDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieDisplayDevice::kStatusVblankPending) ==
            PcieDisplayDevice::kStatusVblankPending);

    REQUIRE(msix.is_pba_set(0));
}

// ============================================================================
// 5. Pending cleared via W1C (write STATUS_CLEAR with pending bit)
// ============================================================================
TEST_CASE("PcieDisplayDevice VBLANK: pending cleared via STATUS_CLEAR W1C",
          "[pcie][display][vblank]") {
    PcieDisplayDevice dev;
    MsiXTable msix(1);

    // Trigger VBLANK via tick
    for (int i = 0; i < 1024; ++i) {
        dev.tick(msix);
    }

    // Verify pending is set
    uint8_t status = 0xFF;
    REQUIRE(dev.mmio_read(PcieDisplayDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieDisplayDevice::kStatusVblankPending) ==
            PcieDisplayDevice::kStatusVblankPending);

    // Write STATUS_CLEAR with pending bit to clear
    uint8_t clear_byte = PcieDisplayDevice::kStatusVblankPending;
    REQUIRE(dev.mmio_write(PcieDisplayDevice::kRegStatusClear,
                            &clear_byte, sizeof(clear_byte)) == 0);

    // Verify pending is cleared
    REQUIRE(dev.mmio_read(PcieDisplayDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieDisplayDevice::kStatusVblankPending) == 0);
}

// ============================================================================
// 6. Multiple VBLANK triggers: each increments vblank_count and resets cycle_counter
// ============================================================================
TEST_CASE("PcieDisplayDevice VBLANK: cycle_counter resets after each trigger",
          "[pcie][display][vblank]") {
    PcieDisplayDevice dev;
    MsiXTable msix(1);

    // First trigger
    for (int i = 0; i < 1024; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 1);
    REQUIRE(dev.cycle_counter() == 0);

    // Partial progress
    for (int i = 0; i < 500; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 1);
    REQUIRE(dev.cycle_counter() == 500);

    // Complete second trigger
    for (int i = 0; i < 524; ++i) {  // 500 + 524 = 1024
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 2);
    REQUIRE(dev.cycle_counter() == 0);
}

// ============================================================================
// 7. INT_MASK transition: 0 → 1; next VBLANK delivers MSI-X
// ============================================================================
TEST_CASE("PcieDisplayDevice VBLANK: INT_MASK transition (0 → 1)",
          "[pcie][display][vblank]") {
    PcieDisplayDevice dev;
    MsiXTable msix(1);

    // INT_MASK=0: first VBLANK does NOT call update_pending
    for (int i = 0; i < 1024; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 1);
    REQUIRE_FALSE(msix.is_pba_set(0));

    // Clear pending via STATUS_CLEAR
    uint8_t clear_byte = PcieDisplayDevice::kStatusVblankPending;
    REQUIRE(dev.mmio_write(PcieDisplayDevice::kRegStatusClear,
                            &clear_byte, sizeof(clear_byte)) == 0);

    // INT_MASK=1: next VBLANK should call update_pending
    uint32_t mask = PcieDisplayDevice::kIntMaskVblankEnable;
    REQUIRE(dev.mmio_write(PcieDisplayDevice::kRegIntMask, &mask, sizeof(mask)) == 0);

    for (int i = 0; i < 1024; ++i) {
        dev.tick(msix);
    }
    REQUIRE(dev.vblank_count() == 2);
    REQUIRE(msix.is_pba_set(0));
}