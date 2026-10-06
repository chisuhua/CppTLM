# PCIe TLP Wire-Format Byte Layout (v1.0)

**Date**: 2026-10-06
**Status**: 🟢 Canonical (per ADR-DGPUDG-06 Status Update)
**Scope**: Byte-accurate specification of PcieTlpWireBundle ↔ PCIe wire bytes
**Reference**: PCIe Base Specification §2.2 (TLP Format), §2.7 (CRC)
**C++ Implementation**: `include/tlm/pcie/pcie_tlp_codec.hh` (canonical byte packing)

## Byte Order

PCIe wire is **Big-Endian (BE)**. Host (x86) is Little-Endian (LE). All byte
transitions in `PcieTlpWireBundle` are handled by `cpptlm::pcie::PcieTlpCodec`
(per Oracle bg_ae8d4f14: do NOT duplicate the byte-packing logic).

For `payload[]` field (`std::array<uint32_t, 1024>`), the host stores as LE;
the bridge converts to BE on `tlp_pack_to_bytes` and LE on `tlp_unpack_from_bytes`.

## TLP Header Layout (3DW, 32-bit addressing)

| Byte Offset | Field | Width (bits) | Per-PCIe §2.2.1 Notes |
|-------------|-------|--------------|------------------------|
| 0 | fmt[2:0] + type[4:0] + R + TC[2:0] + Attr[1:0] + TH + TD + EP + Attr[2] + AT[1:0] | 32 | First DW, encoded BE |
| 4 | Length[9:0] + Requester ID[15:0] | 32 | Second DW, BE |
| 8 | Tag[7:0] + Last DW BE[3:0] + First DW BE[3:0] + Address low[31:2] | 32 | Third DW, BE |
| 12+ | Payload (Length DW × 4 bytes), aligned | varies | LE within each DW |
| 12 + payload×4 | LCRC[31:0] | 32 | Last 4 bytes, computed over [0, 12+payload×4) |

### 4DW Header (64-bit addressing)

Not yet supported by `PcieTlpCodec` (per spec §2.2.1 4DW header layout). Follow-up
change `cpptlm-tlp-descriptor-fmt-fix` will extend the codec.

## ECRC Position (TD=1 only)

If `dec.td == 1` (TLP Digest bit in header byte 0), the ECRC (4 bytes) is
inserted **between** payload and LCRC:

| Offset | Field |
|--------|-------|
| 12 + payload_bytes | ECRC[31:0] (optional, TD=1) |
| 12 + payload_bytes + 4 (or 12+payload) | LCRC[31:0] (always) |

ECRC is over header+payload (excludes ECRC and LCRC). LCRC is over
header+payload+ECRC (excludes LCRC). See `PcieTlpCodec::compute_ecrc`
and `compute_lcrc` for the exact byte ranges.

## LOSSY fields (documented, intentional)

These are pre-existing losses preserved for backward compatibility with snapshot
tests. Marked `LOSSY:` per spec §11.2.

| Field | Loss | Reason | Affected Functions |
|-------|------|--------|--------------------|
| `from_descriptor fmt` | CFG gets 0b100 (invalid; CfgRd should be 0b000) | Pre-existing bug | `from_descriptor` only — `[deprecated]` |
| `from_descriptor fmt` | MRd gets 0b010 (should be 0b000 for no-data) | Pre-existing bug | `from_descriptor` only — `[deprecated]` |
| `from_descriptor type` | Always 0 (should differ MRd=00000/CfgRd=00100/etc.) | Pre-existing bug | `from_descriptor` only — `[deprecated]` |
| `from_descriptor first_be` | Always 0xF (assumes full DW) | Hardcoded | `from_descriptor` only — `[deprecated]` |
| `from_descriptor tag` | Truncated to 8 bits (trans_id precision) | Original field width | `from_descriptor` only — `[deprecated]` |
| `from_descriptor bar_index` | Dropped (BAR 0 only) | Original codec limitation | `from_descriptor` only — `[deprecated]` |
| `to_descriptor CPLD` | Collapsed to MMIO_READ (PcieTlpBundle lacks CPLD kind) | Pre-existing bug | `to_descriptor` only — `[deprecated]` |
| `to_descriptor bar_index` | Always 0 | | `to_descriptor` only — `[deprecated]` |
| `from_descriptor length` | Bytes→DW ceiling (length=0 means 1024 DW special case) | | `from_descriptor` only — `[deprecated]` |

**CRITICAL**: Byte-level packing via `tlp_pack_to_bytes` / `tlp_unpack_from_bytes`
does NOT have these losses — they use `PcieTlpCodec` directly. The `LOSSY:`
functions are only the deprecated `to_descriptor`/`from_descriptor` (kept for
snapshot test backward compat per ADR-DGPU-06 Status Update).

## External Ground-Truth

Golden vectors derive from:
- PCIe Base Specification §2.2.1 reference tables (header layout)
- PCIe Base Specification §2.7 reference vectors (CRC-32 known-answer 0x2144DF1C, see
  `PcieTlpCodec::crc_self_test()` validation)
- `wire-format-snapshot.json` (existing project snapshot, extended per Task 10)

NOT self-round-trip-only (would be tautological).

## Pin-Contract Implications

A consumer reading wire bytes from a real PCIe tap (or RTL simulation) and
producing `PcieTlpWireBundle` MUST:
- Honor BE byte order (codec handles)
- Honor LOSSY field expectations only for the deprecated functions; byte-level
  methods are lossless for MEM_READ/MEM_WRITE/CPLD
- Match CRC-32 polynomial 0x04C11DB7 (codec handles)

A consumer producing wire bytes from `PcieTlpWireBundle` MUST produce identical
byte sequences to a real PCIe transmitter for non-LOSSY fields.

## Maintenance

This doc is updated when wire format changes. Per `docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md`
Status Update, `PcieTlpWireBundle` (in `cpptlm::pcie` namespace) is the canonical
byte-accurate wire format. The `PcieTlpBundle` descriptor (in `bundles::` namespace)
is the canonical descriptor surface; conversions between them go through the
deprecated `to_descriptor` / `from_descriptor` (LOSSY) or the byte-canonical
`tlp_pack_to_bytes` / `tlp_unpack_from_bytes` (lossless for covered kinds).