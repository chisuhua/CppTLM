// include/bundles/pcie_tlp_wire_bundle.hh (NEW)
// PcieTlpWireBundle migrated from include/bundles/pcie_bundles_tlm.hh:130-258
// Phase P1: dgpu-realip-refactor §5.2 (wire-format canonicalization)
// Author: Architecture WG / Date: 2026-10-06
//
// This file is NON-FROZEN — PcieTlpWireBundle + its byte-level codec bridge
// to PcieTlpCodec land here per P1 spec §5.2 + Task 7 (REVISED per Oracle
// bg_ae8d4f14: reuse PcieTlpCodec, no parallel implementation).
// pcie_bundles_tlm.hh re-exports via #include "pcie_tlp_wire_bundle.hh"
// (frozen header surface preserved).

#ifndef CPPTLM_PCIE_TLP_WIRE_BUNDLE_H
#define CPPTLM_PCIE_TLP_WIRE_BUNDLE_H

#include <array>
#include <cstdint>

namespace bundles { struct PcieTlpBundle; }

namespace cpptlm::pcie {

// ===== PcieTlpWireBundle (Phase 9+ T-P9-2) =====
// Wire-format TLP bundle with 4KB payload array (std::array<uint32_t, 1024>)
//
// Used for: (a) golden snapshot tests, (b) CRC verification, (c) error injection
// NOT used to replace existing PcieTlpBundle descriptor; LL interface sig const
// PcieTlpBundle& unchanged.
//
// Design decisions:
// - std::array<uint32_t, 1024> works around ch_uint<512> = 64-bit limit
//   (per AGENTS.md KEY INVARIANTS; P0 fix preserves backward-compat shim)
// - CPLD=7 kind constant reserved for T-P10-1 fc_type_for_kind fix
// - Located in cpptlm::pcie namespace alongside PcieTlpCodec (wire-format codec)
struct PcieTlpWireBundle {
    // Kind 常量 (PCIe 事务类型, 与 bundles::PcieTlpBundle 0-6 对齐)
    static constexpr uint8_t CFG_READ     = 0;
    static constexpr uint8_t CFG_WRITE    = 1;
    static constexpr uint8_t MMIO_READ    = 2;
    static constexpr uint8_t MMIO_WRITE   = 3;
    static constexpr uint8_t MEM_READ     = 4;
    static constexpr uint8_t MEM_WRITE    = 5;
    static constexpr uint8_t IRQ_DELIVERY = 6;
    static constexpr uint8_t CPLD         = 7;

    // Wire-format header 字段 (PCIe Base Spec §2.2)
    uint8_t  fmt;              // 3 bits, 0=3DW/no-data 1=4DW/no-data 2=3DW/data 3=4DW/data
    uint8_t  type;             // 5 bits
    uint8_t  tc;               // 3 bits, Traffic Class
    uint8_t  td;               // TLP Digest (ECRC present)
    uint8_t  ep;               // Poisoned
    uint8_t  attr;             // 3 bits, Attr[2:0]
    uint16_t length;           // 10 bits, in DW (0 = 1024 DW)
    uint16_t requester_id;     // BDF of requester
    uint8_t  tag;              // 8 bits
    uint8_t  last_be;          // 4 bits, Last DW Byte Enables
    uint8_t  first_be;         // 4 bits, First DW Byte Enables
    uint32_t address_lo;       // 32-bit address low (3DW header)
    uint32_t address_hi;       // 32-bit address high (4DW header, 0 for 3DW)
    // Completion-only fields (CplD)
    uint16_t completer_id;     // CplD only
    uint8_t  status;           // CplD only, 3 bits
    uint16_t byte_count;       // CplD only, 12 bits
    uint8_t  lower_address;    // CplD only, 8 bits

    static constexpr std::size_t MAX_PAYLOAD_DW = 1024;
    std::array<uint32_t, MAX_PAYLOAD_DW> payload{};

    uint32_t ecrc = 0;
    uint32_t lcrc = 0;

    uint8_t kind = MMIO_READ;

    PcieTlpWireBundle() = default;

    // ── LOSSY conversions — DEPRECATED per Oracle bg_ae8d4f14 ──
    // Bugs: from_descriptor fmt always 0b100 for CFG (invalid; CfgRd is 0b000)
    //       MRd also gets 0b010 (should be 0b000 no-data); type always 0.
    //       to_descriptor collapses CPLD → MMIO_READ.
    // Byte-level packing via PcieTlpCodec (P1 Task 7 bridge) is canonical.
    // Follow-up change 'cpptlm-tlp-descriptor-fmt-fix' to repair these.
    [[deprecated("Lossy fmt/type semantics; use PcieTlpCodec for byte-level packing")]]
    bundles::PcieTlpBundle to_descriptor() const;

    [[deprecated("Lossy fmt/type semantics; use PcieTlpCodec for byte-level packing")]]
    static PcieTlpWireBundle from_descriptor(const bundles::PcieTlpBundle& desc);

    // ── P1 NEW: byte-level packing + CRC32 + byte-order ──
    // Per PCIe Base Spec §2.2.1 TLP header DW packing.
    // Thin bridge to PcieTLPcodec (per Oracle bg_ae8d4f14: reuse, no duplicate).
    // Wire format is Big-Endian (host LE).
    void tlp_pack_to_bytes(uint8_t out[4096], unsigned& out_len) const noexcept;
    void tlp_unpack_from_bytes(const uint8_t in[4096], unsigned in_len) noexcept;
    uint32_t crc32_compute() const noexcept;  // LCRC over header+payload
};

} // namespace cpptlm::pcie

#endif // CPPTLM_PCIE_TLP_WIRE_BUNDLE_H