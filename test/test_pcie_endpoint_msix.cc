// test_pcie_endpoint_msix.cc
// PcieEndpointTLM: MSI-X pending + mask 测试 (PE-G4)
// Author: CppTLM Team
// Date: 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §6
//       spec.md Scenario "Masked vector does not deliver IRQ"

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/msix_table_mvp.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"

using namespace tlm::gpu;
using namespace bundles;

TEST_CASE("PcieEndpoint: MSI-X default num_vectors = 16", "[pcie][endpoint][msix]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();
    REQUIRE(ep.msix().num_vectors() == 16u);
}

TEST_CASE("PcieEndpoint: MSI-X update_pending emits irq_out", "[pcie][endpoint][msix]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 配置 vector 3 (addr + data)
    REQUIRE(ep.msix().configure_vector(3, 0xFEE00000ULL, 0xCAFEBABEu) == true);
    REQUIRE(ep.msix().update_pending(3, /*trans_id=*/42) == true);
    REQUIRE(ep.msix().pending_count() == 1u);
    REQUIRE(ep.msix().pba_count() == 1u);
    REQUIRE(ep.msix().is_pba_set(3) == true);

    const auto* evt = ep.msix().try_pop_irq_out();
    REQUIRE(evt != nullptr);
    REQUIRE(evt->vector == 3);
    REQUIRE(evt->msg_data == 0xCAFEBABEu);
    REQUIRE(evt->msg_addr == 0xFEE00000ULL);
    REQUIRE(evt->trans_id == 42u);
    ep.msix().consume_irq_out();
    REQUIRE(ep.msix().pending_count() == 0u);
    // PBA bit is preserved after consume_irq_out (front-end pop):
    // driver EOI path invokes clear_pending() to drop the PBA bit.
    REQUIRE(ep.msix().is_pba_set(3) == true);
}

// T-prereq-3 PBA semantics: masked → update_pending accepted (PBA bit set),
// but no IRQ queueing. Unmasking auto-delivers accumulated pending.
TEST_CASE("PcieEndpoint: MSI-X masked vector defers IRQ to PBA (no immediate delivery)",
          "[pcie][endpoint][msix][mask][pba]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    ep.msix().configure_vector(3, 0xFEE00000ULL, 0xCAFEBABEu);
    ep.msix().set_mask(3, true);
    REQUIRE(ep.msix().is_masked(3) == true);

    // Per PCI-SIG MSI-X ECN + T-prereq-3 spec: masked update_pending still accepted
    // (PBA bit tracked) but no IRQ event is queued.
    REQUIRE(ep.msix().update_pending(3) == true);
    REQUIRE(ep.msix().pending_count() == 0u);
    REQUIRE(ep.msix().pba_count() == 1u);
    REQUIRE(ep.msix().is_pba_set(3) == true);
    REQUIRE(ep.msix().try_pop_irq_out() == nullptr);

    // Unmasking auto-delivers accumulated PBA pending (per T-prereq-3 PBA semantics).
    REQUIRE(ep.msix().clear_mask(3) == true);
    REQUIRE(ep.msix().is_masked(3) == false);
    REQUIRE(ep.msix().pending_count() == 1u);
    REQUIRE(ep.msix().is_pba_set(3) == true);

    const auto* evt = ep.msix().try_pop_irq_out();
    REQUIRE(evt != nullptr);
    REQUIRE(evt->vector == 3);
    ep.msix().consume_irq_out();
    REQUIRE(ep.msix().pending_count() == 0u);

    REQUIRE(ep.msix().update_pending(3) == true);
    REQUIRE(ep.msix().pending_count() == 1u);
}

TEST_CASE("PcieEndpoint: MSI-X out-of-range vector rejected", "[pcie][endpoint][msix][bounds]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    REQUIRE(ep.msix().configure_vector(99, 0, 0) == false);
    REQUIRE(ep.msix().set_mask(99, true) == false);
    REQUIRE(ep.msix().update_pending(99) == false);
    REQUIRE(ep.msix().is_masked(99) == false);
    REQUIRE(ep.msix().is_pending(99) == false);
    REQUIRE(ep.msix().is_pba_set(99) == false);
}

TEST_CASE("PcieEndpoint: MSI-X clear_pending removes from PBA + queue",
          "[pcie][endpoint][msix][pba]") {
    // Per T-prereq-3 PBA semantics: clear_pending clears both PBA bit and queue entry.
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    ep.msix().configure_vector(5, 0xFEE00000ULL, 0x12345678u);
    REQUIRE(ep.msix().update_pending(5) == true);
    REQUIRE(ep.msix().is_pending(5) == true);
    REQUIRE(ep.msix().is_pba_set(5) == true);
    REQUIRE(ep.msix().pending_count() == 1u);

    REQUIRE(ep.msix().clear_pending(5) == true);
    REQUIRE(ep.msix().is_pending(5) == false);
    REQUIRE(ep.msix().is_pba_set(5) == false);
    REQUIRE(ep.msix().pending_count() == 0u);

    REQUIRE(ep.msix().clear_pending(5) == false);
}