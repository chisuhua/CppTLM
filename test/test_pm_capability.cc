// test/test_pm_capability.cc
// PM Capability (id=0x01) 寄存器 + PMCSR 写拦截 (Stage 1.4 §1.1)
// per openspec/changes/2026-09-10-cpptlm-stage-1-4-2-1/design.md §1.1+1.2
//
// 标签: [pcie] [pm] + 子标签 [cap] [pmcsr] [intercept]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/gpu/pcie_config_space_mvp.hh"

#include <cstdint>

using tlm::gpu::PcieConfigSpace;

namespace {

constexpr uint8_t  PM_CAP_ID          = 0x01;
constexpr uint16_t PM_CAP_OFFSET      = 0x40;
constexpr uint16_t PM_PMCSR_OFFSET    = PM_CAP_OFFSET + 0x04; // 0x44
constexpr uint16_t PM_CAP_DWORD       = PM_CAP_OFFSET;
constexpr uint16_t PM_CAP_NEXT_PTR    = PM_CAP_OFFSET + 0x01;
constexpr uint16_t PM_PMC_OFFSET      = PM_CAP_OFFSET + 0x02;
constexpr uint16_t PM_PMCSR_BSE_OFF   = PM_CAP_OFFSET + 0x06;

constexpr uint16_t PM_CAP_VERSION_MASK = 0x0007;
constexpr uint16_t PM_CAP_PME_CLOCK    = 0x0008;
constexpr uint16_t PM_CAP_D3HOT_SUPPORT = 0x0010;

constexpr uint16_t PMCSR_PME_EN       = 0x0100;
constexpr uint16_t PMCSR_DSEL_MASK    = 0x001E;
constexpr uint16_t PMCSR_PWS_MASK     = 0x0003;
constexpr uint16_t PMCSR_PWS_D0       = 0x0000;
constexpr uint16_t PMCSR_PWS_D3HOT    = 0x0003;

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: PM Cap ID readable
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieConfigSpace: PM Cap (id=0x01) registers standard layout",
          "[pcie][pm][cap]") {
    PcieConfigSpace cfg;
    cfg.init();

    SECTION("add_capability stores PM Cap header at offset") {
        REQUIRE(cfg.add_capability(PM_CAP_ID, PM_CAP_OFFSET, /*next=*/0x00,
                                   /*control=*/(0x03 | PM_CAP_D3HOT_SUPPORT)));
        REQUIRE(cfg.capability_count() == 1);

        const auto* cap = cfg.get_capability(0);
        REQUIRE(cap != nullptr);
        REQUIRE(cap->id == PM_CAP_ID);
        REQUIRE(cap->offset == PM_CAP_OFFSET);

        // Host CFG_READ[PM_CAP_OFFSET] → cap dword: bits[7:0]=id, bits[15:8]=next, bits[31:16]=control
        const uint32_t dword = cfg.read(PM_CAP_OFFSET);
        REQUIRE((dword & 0xFFu) == PM_CAP_ID);                       // id
        REQUIRE(((dword >> 8) & 0xFFu) == 0x00u);                    // next = 0 (end of chain)
        REQUIRE(((dword >> 16) & 0xFFFFu) == (0x03 | PM_CAP_D3HOT_SUPPORT));  // control
    }

    SECTION("PMCSR register at offset+4 starts at D0") {
        REQUIRE(cfg.add_capability(PM_CAP_ID, PM_CAP_OFFSET, 0, 0x03));
        const uint32_t pmcsr = cfg.read(PM_PMCSR_OFFSET);
        REQUIRE((pmcsr & PMCSR_PWS_MASK) == PMCSR_PWS_D0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: PMCSR write triggers power state transition callback (INV-A)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieConfigSpace: PMCSR write callback fires on power state change",
          "[pcie][pm][pmcsr][intercept]") {
    PcieConfigSpace cfg;
    cfg.init();
    REQUIRE(cfg.add_capability(PM_CAP_ID, PM_CAP_OFFSET, 0, 0x03));

    SECTION("writing D0 then D3hot fires callback on each transition") {
        uint32_t callback_count = 0;
        uint16_t last_state = 0xFFFFu;
        cfg.set_pmcsr_write_cb([&](uint16_t new_pws) {
            ++callback_count;
            last_state = new_pws;
        });

        cfg.write(PM_PMCSR_OFFSET, PMCSR_PWS_D0);
        REQUIRE(callback_count == 1);  // 首次写: sentinel -> D0, 必须触发
        REQUIRE(last_state == PMCSR_PWS_D0);

        cfg.write(PM_PMCSR_OFFSET, PMCSR_PWS_D3HOT);
        REQUIRE(callback_count == 2);
        REQUIRE(last_state == PMCSR_PWS_D3HOT);

        cfg.write(PM_PMCSR_OFFSET, PMCSR_PWS_D0);
        REQUIRE(callback_count == 3);
        REQUIRE(last_state == PMCSR_PWS_D0);
    }

    SECTION("callback not installed = write silently persists") {
        cfg.write(PM_PMCSR_OFFSET, PMCSR_PWS_D3HOT);
        REQUIRE((cfg.read(PM_PMCSR_OFFSET) & PMCSR_PWS_MASK) == PMCSR_PWS_D3HOT);
    }

    SECTION("DSEL field bits preserved across writes") {
        cfg.write(PM_PMCSR_OFFSET, PMCSR_DSEL_MASK | PMCSR_PWS_D3HOT);
        const uint32_t after = cfg.read(PM_PMCSR_OFFSET);
        REQUIRE((after & PMCSR_DSEL_MASK) == PMCSR_DSEL_MASK);
        REQUIRE((after & PMCSR_PWS_MASK) == PMCSR_PWS_D3HOT);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: PM Cap respects OOB / non-aligned writes (PCIe spec semantics)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieConfigSpace: PM Cap OOB / misaligned writes silently ignored",
          "[pcie][pm][cap]") {
    PcieConfigSpace cfg;
    cfg.init();
    REQUIRE(cfg.add_capability(PM_CAP_ID, PM_CAP_OFFSET, 0, 0x03));

    // 写越界不崩, 不变
    cfg.write(0xFFFC, 0xDEADBEEF);
    cfg.write(PM_PMCSR_OFFSET + 1, 0xDEADBEEF); // misaligned (within range)
    REQUIRE((cfg.read(PM_PMCSR_OFFSET) & PMCSR_PWS_MASK) == PMCSR_PWS_D0);
}
