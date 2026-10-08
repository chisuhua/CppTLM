// include/bundles/dgpu_fabric_bundles_tlm.hh
// Fabric2VramCtrlBundle: chip-internal wire-protocol between GpuInternalFabric and VramControllerTLM
// Per design AD-5 (Field alignment with cache_bundles_tlm.hh CacheReqBundle + chip_select extension)
//
// ## BUNDLE IDENTITY
// Name: Fabric2VramCtrlBundle
// Width: 8 fields, max width 64-bit (avoids ch_uint<W>64 wide-path truncation shim per Oracle B.6)
// Direction: chip-internal (NOT board-level TLP / NOT chip-internal AXI)
// Consumers: GpuInternalFabric (Initiator) ↔ VramControllerTLM (Target)
// Registered: compile-time include via chstream_register.hh
//
// ## FIELD TABLE
// ┌────────────────┬───────┬─────────────────────────────────────────────────────┐
// │ field          │ width │ semantic                                            │
// ├────────────────┼───────┼─────────────────────────────────────────────────────┤
// │ address        │ 64    │ DRAM byte address (chip-level)                       │
// │ size           │  8    │ transaction size in bytes (1-128)                    │
// │ is_write       │  1    │ 1=write, 0=read                                      │
// │ data           │ 64    │ write data (≤64-bit, avoids ch_uint wide-path shim)  │
// │ transaction_id │ 32    │ request id (changed from CacheReqBundle 64→32)      │
// │ resp_data      │ 64    │ read response data                                  │
// │ error_code     │  8    │ 0=OK, 1=DECERR, 2=SLVERR, 3=unaligned, etc.         │
// │ chip_select    │  8    │ chip-select (multi-chip select, HCS topology)       │
// └────────────────┴───────┴─────────────────────────────────────────────────────┘
//
// Author: CppTLM Team / Date: 2026-10-08
// See: openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md
//
// Momus 修订: 原 cpphdl::ch_uint 命名空间错,实际是 bundles::ch_uint (per include/bundles/cpphdl_types.hh)。

#pragma once

#include <type_traits>

#include "bundles/cpphdl_types.hh"

namespace bundles {

struct Fabric2VramCtrlBundle {
    bundles::ch_uint<64> address{};
    bundles::ch_uint<8>  size{};
    bundles::ch_uint<1>  is_write{};
    bundles::ch_uint<64> data{};
    bundles::ch_uint<32> transaction_id{};
    bundles::ch_uint<64> resp_data{};
    bundles::ch_uint<8>  error_code{};
    bundles::ch_uint<8>  chip_select{};
};

}  // namespace bundles

// Compile-time invariants (auto-checked at every TU including this header)
static_assert(sizeof(bundles::Fabric2VramCtrlBundle) > 0,
              "Fabric2VramCtrlBundle must be a complete aggregate type");
static_assert(std::is_standard_layout_v<bundles::Fabric2VramCtrlBundle>,
              "Fabric2VramCtrlBundle must be standard-layout for zero-copy TLM payload");