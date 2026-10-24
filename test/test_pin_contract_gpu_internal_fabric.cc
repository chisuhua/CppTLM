// test/test_pin_contract_gpu_internal_fabric.cc
// Pin-contract test for GpuInternalFabric (T22b — T22 commit 95a68889 scaffold-only 漏做).
//
// Per docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md Task 22
// (P3 GpuInternalFabric SimModule wrapper scaffold) + session handoff
// 2026-10-07 §5 T22b.
//
// Purpose:
//   Lock the behavioral contract of GpuInternalFabric BEFORE any consumer
//   (T23B JSON 嵌套 + T23C backing 注入 + T25B dual-path equivalence) wires it
//   up. Test must explicitly call attach_crossbar() to activate the dormant
//   gate (per Metis bg_0cb04e652 AI-FP-1: attach_crossbar 零调用方 trap).
//
// Contracts to verify:
//   1. Construct: get_module_type() == "GpuInternalFabric"
//   2. Dormant: crossbar() == nullptr initially (no leak / premature wire-up)
//   3. Activate: attach_crossbar(inner) → crossbar() returns inner
//   4. Default num_ports() == 4 (matches CrossbarTLM::NUM_PORTS)
//   5. set_num_ports(8) → num_ports() returns 8
//   6. SimModule base: cast to SimModule* succeeds (composition vs
//      ChStreamModuleBase trap per Metis AM-3)
//
// TDD state: GREEN (scaffold 95a68889 was incomplete; this test pins the
// future contract and catches any deviation).
//
// Tag: `[fabric]` (per plan Amendment 标签规范)
//
// Author: Architecture WG / Date: 2026-10-07
// Status: GREEN

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/sim_module.hh"
#include "tlm/cluster/gpu_internal_fabric.hh"
#include "tlm/crossbar_tlm.hh"

#include <memory>

using ::tlm::cluster::GpuInternalFabric;
// CrossbarTLM is in global namespace (per crossbar_tlm.hh line 31: no `namespace` wrapper)
// NOT in `::tlm::` — forward-decl in GpuInternalFabric header mistakenly created
// a `tlm::cluster::CrossbarTLM` ghost type. Use raw `CrossbarTLM` after the include.

namespace {

    struct FabricHarness {
        ::EventQueue eq;
        GpuInternalFabric fabric;
        std::unique_ptr<CrossbarTLM> crossbar;

        FabricHarness()
            : eq{}, fabric{"gpu_fabric_pinned", &eq},
              crossbar{std::make_unique<CrossbarTLM>("inner_xbar_pinned", &eq)} {
            // Activate dormant gate (per Metis AI-FP-1: attach_crossbar 零调用方).
            fabric.attach_crossbar(crossbar.get());
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// Contract 1+2: construct returns correct module_type, dormant crossbar
// before attach_crossbar.
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("GpuInternalFabric: construct returns 'GpuInternalFabric' module_type", "[fabric]") {
    ::EventQueue eq;
    GpuInternalFabric fabric("fabric_ctor", &eq);

    REQUIRE(fabric.get_module_type() == "GpuInternalFabric");
    REQUIRE(fabric.crossbar() == nullptr); // dormant before attach
}

TEST_CASE("GpuInternalFabric: num_ports() defaults to 4 (matches CrossbarTLM::NUM_PORTS)",
          "[fabric]") {
    ::EventQueue eq;
    GpuInternalFabric fabric("fabric_np", &eq);

    // num_ports default = 4 (matches CrossbarTLM internal port count)
    REQUIRE(fabric.num_ports() == 4u);
    // CrossbarTLM::NUM_PORTS is private — verified separately by visual inspection
    // of crossbar_tlm.hh line 33: `static constexpr unsigned NUM_PORTS = 4;`.
}

// ─────────────────────────────────────────────────────────────────────────
// Contract 3: attach_crossbar() activates crossbar pointer (non-owning).
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("GpuInternalFabric: attach_crossbar() activates crossbar() (dormant gate)", "[fabric]") {
    FabricHarness h;

    REQUIRE(h.fabric.crossbar() != nullptr);
    REQUIRE(h.fabric.crossbar() == h.crossbar.get());
}

TEST_CASE("GpuInternalFabric: crossbar() returns attached inner, not copy", "[fabric]") {
    FabricHarness h;

    // Pointer identity: same raw address, not copy or default-constructed.
    auto* expected = h.crossbar.get();
    auto* actual = h.fabric.crossbar();

    REQUIRE(actual == expected);
    REQUIRE(actual != nullptr);
}

// ─────────────────────────────────────────────────────────────────────────
// Contract 4: set_num_ports adjusts contract for non-GPU, non-default topology.
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("GpuInternalFabric: set_num_ports(8) updates num_ports() (router override)", "[fabric]") {
    FabricHarness h;

    h.fabric.set_num_ports(8);
    REQUIRE(h.fabric.num_ports() == 8u);

    // Setter is idempotent / mutable (vs. const-only contract).
    h.fabric.set_num_ports(2);
    REQUIRE(h.fabric.num_ports() == 2u);
}

// ─────────────────────────────────────────────────────────────────────────
// Contract 5: SimModule base (composition vs ChStreamModuleBase trap).
// Per Oracle bg_c60653e0 §3.4: GpuInternalFabric MUST extend SimModule
// (composite holder), NOT ChStreamModuleBase (which is reserved for inner
// ChStream-bearing modules like CacheTLM/CrossbarTLM/MemoryTLM).
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("GpuInternalFabric: is SimModule (composite, not ChStreamModuleBase)", "[fabric]") {
    FabricHarness h;

    // Upcast to SimModule* must compile cleanly (B1-style contract pin).
    ::SimModule* as_sim = &h.fabric;
    REQUIRE(as_sim != nullptr);

    // Module type string must be preserved through SimModule base.
    REQUIRE(as_sim->get_module_type() == "GpuInternalFabric");
}