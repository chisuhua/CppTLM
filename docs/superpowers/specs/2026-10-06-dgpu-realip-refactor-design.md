# dGPU Real-IP Boundary Refactor Design (v1.1)

**Date**: 2026-10-06
**Status**: 🟢 Ready (Metis + Oracle double-review complete; 7 hard fixes + 5 scope decisions applied)
**Scope**: Extract real-IP modules from TLM-abstraction god-class; solve 3 hard blockers; deliver **Foundation for RTL-ready IP boundaries**
**Phase**: 4 phases (P0 ‖ P1 → P2 → P3), ~22-32 days (~5-6 weeks)
**OpenSpec change**: `dgpu-realip-refactor` (1 change, CppTLM repo)
**Owner**: Architecture working group

---

## 1. Background and Motivation

CppTLM is currently a TLM 2.0 cycle-approximate NoC simulator for dGPU SoC. After 8 phases of PCIe EP microarchitecture + D-AXI driver-visible minimal SoC + D3 timing-mode, the system has accumulated architectural debt:

- **DGpuBoard is a god-class** hosting `vram_storage_` (8GB backing) with 5 consumers sharing a single pointer (backdoor, BAR1 fast-path, MemoryTLM backing, SDMA/GMMU legacy, PcieMemoryDevice).
- **PcieEndpointIP straddles PCIe Controller + chip-internal AXI** — already a v1.4 "PCIe facade layer" but its boundaries don't match real silicon IP seams.
- **GMMU, SDMA, Memory Fabric** are ChStream modules with custom `cpptlm` bundle contracts that lack byte-level alignment with industry AXI4 / PCIe TLP protocols.
- **ADR-DGPU-05** locks single-VRAM ownership in DGpuBoard; **ADR-DGPU-06** freezes PcieTlpBundle; **ADR-DGPU-08** freezes 14 ABI functions (verified inventory in §8).

The SoC will serve as the **foundation for future RTL implementation**. This refactor delivers:

- IP module boundaries aligned with real silicon (NVIDIA FBHUB / AMD MMHUB / DesignWare PCIe EP pattern).
- Interface protocols specified byte-accurately (P0 ch_uint wide path + P1 packing/CRC32/byte-order).
- 14 ABI behavioral contract preserved (per ADR-DGPU-08).
- Existing test suite stays green per refactor step.

> **Scope honesty (v1.1, G5 softened from "TLM-as-RTL-reference-model" to "Foundation for RTL-ready IP boundaries")**: This refactor delivers the **interface contracts + port shapes + byte-layout specs** that make future TLM-RTL equivalence *checkable*. It does not deliver the equivalence harness itself — that's a separate OpenSpec change.

## 2. Goals and Success Criteria

| Goal | Success Criterion |
|------|------------------|
| **G1** Real-IP module boundaries | 4 new IPs (`PcieControllerIP`, `PcieAxiBridgeIP`, `GpuInternalFabric`, `VramControllerTLM`-promoted) extracted with pin-contracts matching real silicon IP port lists (BAR decode folded into PcieControllerIP per D2) |
| **G2** Interface byte-level specified | TLP packing/unpacking + CRC32 + byte-order spec'd in `docs/architecture/NN-pcie-tlp-wire-format.md`; golden-vector tests cover all lossless field paths; lossy fields documented as `LOSSY:` markers |
| **G3** 14 ABI preserved | All 14 functions behaviorally unchanged; 0-2 functions marked `[[deprecated]]` (decided in P2 commit); 0 new ABI functions |
| **G4** Test suite stays green | `./test.sh --mode off` reports full green after each phase (P0/P1/P2/P3 each as separate PR) |
| **G5** Foundation for RTL | Pin-contract doc + byte-layout spec + golden vectors + equivalence scaffold enable future TLM-RTL equivalence check (separate change beyond this spec) |

## 3. Scope

### In scope

- `ch_uint<W>` partial-specialization with `std::array<uint64_t,N>` storage for `W>64` (P0)
- `PcieTlpWireBundle` **moved to new file** `include/bundles/pcie_tlp_wire_bundle.hh` + byte-layout spec + TLP packing/unpacking + CRC32 + golden-vector tests (P1)
- 4 new IP module headers + .cc implementations with pin-contracts (P2, P3)
- Up to 2 ABI deprecation markings on legacy entry points (P2)
- ADR-DGPU-06 Status Update covering wire-format canonicalization + CDC declarations (P1)
- Equivalence test scaffold (P3, smoke-level — full harness is a separate change)

### Out of scope

- RTL implementation itself (this refactor enables it; doesn't deliver it)
- Full TLM-RTL equivalence harness with check-points — separate OpenSpec change
- Per-pin CDC async FIFO implementation — RTL CDC signoff tools (Spyglass) handle
- DGpuBoard internal cleanup beyond IP extraction (host ABI shell stays)
- D3+ SM/GPU compute IPs (deferred until D3 proper)
- Any change to test counts beyond keeping green

## 4. Constraints (Non-negotiable)

1. **14 ABI behavior freeze** (per ADR-DGPU-08, verified inventory in §8). All 14 functions behaviorally unchanged. **Up to 2 functions may be marked `[[deprecated]]`** per ADR-DGPU-08 v1.1 exception.
2. **`PcieTlpBundle` frozen** (per ADR-DGPU-06). Bundle layout and field semantics unchanged. Modifications to packing semantics land in a new sibling type `PcieTlpWireBundle` in a **new non-frozen file** `include/bundles/pcie_tlp_wire_bundle.hh`.
3. **`pcie_bundles_tlm.hh` frozen** (per ADR-DGPU-08, frozen 4-header list). `PcieTlpWireBundle` is **migrated out** to `include/bundles/pcie_tlp_wire_bundle.hh` so hardening P1 doesn't touch the frozen file.
4. **`PcieEndpointTLM` 4-port frozen** (per Phase 7.A `[[deprecated]]`). The class is deprecated; new code uses `PcieEndpointIP` (which becomes a `PcieControllerIP` facade).
5. **Test suite stays green** per phase. Each PR (P0/P1/P2/P3) is a separate change gated by `./test.sh --mode off` full green.
6. **TLM stub default** (per `USE_SYSTEMC_STUB=ON`). We will NOT pull real SystemC `sc_bv` or HLS `ap_uint`.
7. **Zero-debt principle**: each phase lands with docs/AGENTS.md sync, no TODO residue, no `.disabled` tests.

## 5. Three Hard Blocker Solutions

### 5.1 Blocker 1: `ch_uint<512>` Silent Truncation

**Problem** (per AGENTS.md "KEY INVARIANTS"): `ch_uint<W>` is always `uint64_t` regardless of `W`. Verified via grep:

```
$ grep -nE "ch_uint<512>" include/bundles/*.hh include/tlm/**/*.hh src/tlm/**/*.cc
include/bundles/axi4_bundles_tlm.hh:51: *   - ch_uint<512> 实际以 uint64_t 存储（仿真精度足够）
include/bundles/pcie_bundles_tlm.hh:141: // PcieTlpWireBundle ... std::array<uint32_t,1024> 绕开 ch_uint<512> 限制
```

Both references are **comments** documenting the limitation; no real 512-bit data fields use it today. The "all fields ≤64-bit" claim is accurate now, but the type itself is unsafe for any future AXI data pin (`ch_uint<256>` or `ch_uint<512>`). P0 fixes this defensively.

**Solution: Option C — `ch_uint` partial specialization with `std::array<uint64_t,N>` storage for `W>64`.**

Why not the alternatives:
- **Option A** (real SystemC `sc_bv`): violates `USE_SYSTEMC_STUB=ON` invariant.
- **Option B** (HLS `ap_uint`): adds Xilinx dependency for no benefit.

Concrete design (with namespace + ctor inheritance corrections per Oracle finding):

```cpp
// include/bundles/cpphdl_types.hh
namespace bundles {  // (corrected: actual namespace is bundles, not cpptlm)

// W≤64: identical to current implementation (zero layout/behavior change)
template<unsigned W>
struct ch_uint_storage {
    static_assert(W > 0 && W <= 64, "ch_uint_storage: 0 < W <= 64");
    uint64_t value_ = 0;
};

// W>64: new wide storage (RTL-style bit-vector)
template<unsigned W>
struct ch_uint_wide {
    static_assert(W > 64, "ch_uint_wide: W > 64");
    static constexpr unsigned N = (W + 63) / 64;
    std::array<uint64_t, N> words_{};           // little-endian: words_[0] = low 64 bits
};

// ch_uint dispatch with ctor inheritance (CRITICAL — existing code calls ch_uint<8>(k))
template<unsigned W = 64>
struct ch_uint
    : std::conditional_t<(W <= 64), ch_uint_storage<W>, ch_uint_wide<W>> {
    using base = std::conditional_t<(W <= 64), ch_uint_storage<W>, ch_uint_wide<W>>;
    using base::base;  // (CRITICAL: inherit constructors to preserve ch_uint<8>(k) call sites)

    // W≤64: read()/write()/operator uint64_t() inherited from base (unchanged)
    // W>64: explicit bit-slice operations (no silent low-64 fallback)
    uint64_t low() const noexcept { return static_cast<const base*>(this)->words_[0]; }
    template<unsigned Hi, unsigned Lo>
    uint64_t slice() const noexcept;
    template<unsigned Hi, unsigned Lo>
    void set_slice(uint64_t val) noexcept;
};

// CRITICAL: assertions for layout safety
static_assert(std::is_standard_layout_v<ch_uint_storage<8>>);
static_assert(std::is_trivially_copyable_v<ch_uint_storage<8>>);
// ... for W ∈ {1, 2, 4, 8, 16, 32, 64}
static_assert(std::is_standard_layout_v<ch_uint_wide<128>>);
static_assert(std::is_trivially_copyable_v<ch_uint_wide<128>>);
// ... for W ∈ {128, 256, 512}

} // namespace bundles
```

**Migration impact**:
- W≤64 specialization is layout-identical (single `uint64_t`) + trivially-copyable → 100+ existing `ch_uint<W>(k)` call sites compile via inherited constructor.
- All current bundle fields verified ≤64-bit (grep above) → zero behavior change for existing code.
- Only *new* wide fields (e.g., AXI4 256/512-bit data path) use the wide specialization.
- Affected files: `include/bundles/cpphdl_types.hh` (edit) + 1 new test file + AGENTS.md update.
- **Effort: 2-3 days** (template + ctor inheritance + bit-slice correctness + trivially_copyable assertions).

### 5.2 Blocker 2: Frozen Surfaces vs Byte-Accuracy

**Problem**:
- `PcieTlpBundle` is frozen by ADR-DGPU-06; `pcie_bundles_tlm.hh` is in ADR-DGPU-08 frozen 4-header list (zero-diff).
- `PcieTlpWireBundle` currently lives in `pcie_bundles_tlm.hh` (line 146-256) — same file as the frozen bundle.
- `PcieTlpWireBundle` is **field-accurate** (separate `fmt`/`type` uint8_t fields) but **lacks**: (a) TLP header DW packing/unpacking functions, (b) CRC32 implementation, (c) byte-order specification (TLP is BE, host is LE).
- Existing `to_descriptor()`/`from_descriptor()` are semantic conversions, not byte-level codecs.

**Solution: Option C — Migrate `PcieTlpWireBundle` to a new non-frozen file, add packing/CRC32/byte-order, declare canonical via ADR-DGPU-06 Status Update.**

Why not alternatives:
- **Option A** (amend freeze + packing spec doc): re-opens freeze debate.
- **Option B** (new ADR for packing exceptions): invites scope creep.

Concrete deliverables (P1, **5-7 days**, was 3-4 in v1.0 — expanded to include packing + CRC32 + byte-order):

| File | Action |
|------|--------|
| `include/bundles/pcie_tlp_wire_bundle.hh` | **NEW**: move `PcieTlpWireBundle` here from `pcie_bundles_tlm.hh:146-258`. Add `tlp_pack_to_bytes()` / `tlp_unpack_from_bytes()` per PCIe §2.2.1 (TLP header DW packing: fmt+type merged into first DW bit positions) + `crc32_compute()` (PCIe TLP LCRC/ECRC) + `tlp_byte_order` enum (TLP BE, host LE) |
| `include/bundles/pcie_bundles_tlm.hh` | Edit: replace inline `PcieTlpWireBundle` definition with **forward declaration + #include "pcie_tlp_wire_bundle.hh"** (preserves API for existing #include sites; file remains ABI-stable) |
| `scripts/test/gen_wire_format_snapshot.sh` + `wire-format-snapshot.json` | **REUSE**: extend existing snapshot script with byte-layout fixtures (per E6 — not duplicated) |
| `test/test_pcie_slice_wire_format_snapshot.cc` | Extend with byte-layout assertions |
| `test/test_pcie_wire_codec_golden.cc` | NEW: golden vectors covering round-trip (descriptor → wire bytes → wire → descriptor) + CRC32 correctness + BE/LE conversion |
| `docs/architecture/NN-pcie-tlp-wire-format.md` | NEW: byte-layout spec (offsets, widths, BE/LE, CRC positions) |
| `docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md` | Status Update: canonical wire format v1 + new file location + CDC declarations (per §5.3) |

**Lossy fields (already verified, documented as `LOSSY:` markers in spec doc)**:
- `from_descriptor` hardcodes `first_be=0xF` (line 245 in old file) — keep + document as `LOSSY:byte-enables`
- `trans_id` truncation to 8-bit tag — keep + document as `LOSSY:trans-id-precision`
- `bar_index` dropped in `from_descriptor` — keep + document as `LOSSY:bar-index` (BAR 0 only)

### 5.3 Blocker 3: CDC Modeling Scope

**Solution: Option B — Boundary declaration only, no async FIFO modeling.**

| File | Action |
|------|--------|
| `configs/templates/dgpu_soc_v2_realip.json` (new) | Add `clock_domains` field: `{"PCIE_DOMAIN": "100MHz", "GPU_DOMAIN": "1GHz", "HBM_DOMAIN": "DDR_rate/2"}` |
| `src/core/config_utils` or analogous | Add JSON parser for `clock_crossings` table — parsed-but-unmodeled, validated only for schema |
| `docs/adr/ADR-DGPU-06-...md` Status Update | Add named crossings table: `pcie_ep→bar_decoder`, `bar_decoder→fabric`, `fabric→vram_ctrl` |

**Effort: <1 day**. Zero simulation behavior change.

## 6. Four IP Boundary Specifications

Per **D2 decision**: BAR decode is part of the `PcieControllerIP` (matches real silicon: DWC iATU + BAR match logic is inside the PCIe controller), not a separate IP. The original v1.0 §6.3 `PcieBarApertureDecoder` is removed.

### 6.1 PcieControllerIP (absorbs BAR decode per D2)

| | |
|---|---|
| **Real-chip counterpart** | DesignWare PCIe EP core / NVIDIA PCIe Controller (includes iATU + BAR match) |
| **CppTLM class** | `PcieControllerIP` (facade over existing `PcieEndpointIP` composite of LinkLayer + PHY + Mux; facade adds BAR match + iATU-equivalent address translation) |
| **Interface protocol** | PHY-side 128b/130b serialized link (stubbed at TLM); egress `PcieTlpWireBundle` (byte-accurate per P1) |
| **State ownership** | LTSSM, FC credits, Config Space, SR-IOV PF/VF tables, MSI-X tables, BAR base/size registers (loaded at enumeration), iATU routing table |
| **Responsibility** | Wire-level PCIe termination + BAR match + address translation → translated AXI addresses delivered to bridge |
| **Pin-contract** | Defined in PR2 commit (PcieControllerIP.hh) |

### 6.2 PcieAxiBridgeIP

| | |
|---|---|
| **Real-chip counterpart** | DWC AXI Bridge / AMD PCIe-ATS bridge |
| **CppTLM class** | `PcieAxiBridgeIP` (facade wrapping `PcieAxiAdapterTLM` + `AXI4Mapper`; merge *wiring*, keep both classes — OOO rid logic stays in AXI4Mapper) |
| **Inbound interface** | `PcieTlpWireBundle` stream (from PcieControllerIP egress) |
| **Outbound interface** | `AXI4-full` 256-bit data (uses `ch_uint<256>` wide path from P0) |
| **State ownership** | TLP↔AXI translation table, OOO rid map, NP↔CplD tracker |
| **Responsibility** | Posted/Non-Posted TLPs → AXI4 read/write bursts; byte-exact addr/len/size |
| **Pin-contract** | Defined in PR2 commit |

### 6.3 GpuInternalFabric

| | |
|---|---|
| **Real-chip counterpart** | AMD SMN / NVIDIA HUB NoC |
| **CppTLM class** | `GpuInternalFabric` (thin wrapper over existing `crossbar_tlm` / `router_tlm`) |
| **Interface** | N × `AXI4-full` multi-port |
| **State ownership** | Arbitration state, address map |
| **Responsibility** | Chip-internal transport between bridge, GMMU, SDMA, VRAM Controller |
| **Pin-contract** | Defined in PR3 commit |

### 6.4 VramControllerTLM (Promotion — corrected register path per E5)

| | |
|---|---|
| **Real-chip counterpart** | NVIDIA FB / AMD MC (HBM PHY, DFI interface) |
| **CppTLM class** | `VramControllerTLM` (already exists at `include/tlm/gpu/vram_controller_tlm.hh`, inherits `MemoryTLM → ChStreamModuleBase → SimObject`) |
| **Inbound interface** | `AXI4-full` slave (from GpuInternalFabric) |
| **Internal interface** | DRAM command abstraction (DFI-lite) — internal to controller |
| **State ownership** | Backing pointer (injected from DGpuBoard `vram_storage_` per ADR-DGPU-05), row buffer, bandwidth counters |
| **Responsibility** | Sole **timing-path** accessor of VRAM backing (5 consumers routed through this for normal timing; backdoor + BAR1 fast-path retained as documented bypass per D1) |
| **Register path (corrected per E5)** | Keep `REGISTER_CHSTREAM` (VramControllerTLM inherits `ChStreamModuleBase`, not `SimModule`; `REGISTER_MODULE` requires SimModule base per `include/AGENTS.md` macro table; static_assert would fail). Promote to top-level by referencing it as a top-level node in `dgpu_soc_v2_realip.json`. |
| **Pin-contract** | Already defined in existing class |

## 7. Four-Phase Implementation Plan

**P0 ‖ P1 parallel** per D5 (saves 2-3 days). Critical path: **(P0 ‖ P1) → P2 → P3**.

Each phase is a separate PR/commit, gated by `./test.sh --mode off` full green.

### 7.1 P0 — Wide-Type Foundation (2-3 days, ∥ P1)

**Goal**: Kill silent truncation defensively without touching frozen surfaces.

| File | Change |
|------|--------|
| `include/bundles/cpphdl_types.hh` | Add `ch_uint_storage<W≤64>` + `ch_uint_wide<W>64>` + `ch_uint` dispatch with **ctor inheritance** (`using base::base`) + `is_trivially_copyable` assertions (per §5.1) |
| `test/test_ch_uint_wide.cc` | NEW: bit-slice, concat, round-trip, **aggregate-init test**, sizeof-identity test |
| `AGENTS.md` | Update "KEY INVARIANTS" ch_uint<512> entry: known-limitation → fixed-with-defense |

**ABI impact**: Zero (template change only; W≤64 specialization is layout + trivially-copyable identical).
**Risk gate**: Full test suite green; `static_assert(std::is_trivially_copyable_v<…>)` + `is_standard_layout` for W∈{1,2,4,8,16,32,64,128,256,512} + sizeof identity.

### 7.2 P1 — Wire-Format Canonicalization (5-7 days, ∥ P0)

**Goal**: Byte-accurate PCIe packing becomes spec'd, tested, versioned, in a non-frozen file.

| File | Change |
|------|--------|
| `include/bundles/pcie_tlp_wire_bundle.hh` | **NEW file**: move `PcieTlpWireBundle` here (cut from `pcie_bundles_tlm.hh:146-258`). Add `tlp_pack_to_bytes()` / `tlp_unpack_from_bytes()` per PCIe §2.2.1 (TLP header DW packing: fmt+type merged into first DW bit positions) + `crc32_compute()` (PCIe TLP LCRC/ECRC) + `tlp_byte_order` enum (TLP BE, host LE) |
| `include/bundles/pcie_bundles_tlm.hh` | Edit: replace inline `PcieTlpWireBundle` definition with **forward declaration + #include "pcie_tlp_wire_bundle.hh"** (preserves API for existing #include sites; file remains ABI-stable) |
| `scripts/test/gen_wire_format_snapshot.sh` + `wire-format-snapshot.json` | **REUSE**: extend existing snapshot script with byte-layout fixtures (per E6 — not duplicated) |
| `test/test_pcie_slice_wire_format_snapshot.cc` | Extend with byte-layout assertions |
| `test/test_pcie_wire_codec_golden.cc` | NEW: golden vectors (descriptor → wire bytes → wire → descriptor) + CRC32 correctness + BE/LE conversion |
| `docs/architecture/NN-pcie-tlp-wire-format.md` | NEW: byte-layout spec (offsets, widths, BE/LE, CRC positions) |
| `docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md` | Status Update: canonical wire format v1 + new file location + CDC declarations (per §5.3) |

**ABI impact**: Zero (frozen 4 headers untouched in API; existing #include sites unaffected via re-export).
**Risk gate**: Lossless round-trip test green; lossy fields explicitly `LOSSY:` documented; CRC32 correct vs PCIe spec reference vectors.

### 7.3 P2 — PCIe-Side IP Extraction (8-12 days)

**Goal**: 2 of 4 IPs land behind pin-stable facades. BAR decode merges into `PcieControllerIP`.

| File | Change |
|------|--------|
| `include/tlm/pcie/pcie_controller_ip.{hh,cc}` | NEW: facade over `PcieEndpointIP` composite + BAR match + iATU-equivalent translation (port re-export, no logic move for LL/PHY/Mux; new BAR/iATU logic added) |
| `include/tlm/pcie/pcie_axi_bridge_ip.{hh,cc}` | NEW: facade wrapping `PcieAxiAdapterTLM` + `AXI4Mapper` pair (merge wiring; keep both classes — OOO rid logic stays in AXI4Mapper) |
| `include/modules_cluster.hh` | Register new IPs (`REGISTER_MODULE(PcieControllerIP)`, `REGISTER_MODULE(PcieAxiBridgeIP)` — these DO inherit SimModule since they're new classes; `VramControllerTLM` keeps `REGISTER_CHSTREAM` per E5) |
| `configs/dgpu_soc_v2_realip_p2.json` | NEW: topology using 2 new IPs (P0/P1 features used) |
| `src/tlm/gpu/dgpu_board_shell.cc` | Edit: replace `display_routing_enabled_` / `memory_routing_enabled_` flags with calls to `PcieControllerIP` BAR match (touches 5-consumer routing — explosion radius: `[pcie-memory]` 24 cases + `[minimal_dgpu_soc]` 67+ assertions) |
| `include/abi/cpptlm_emulator.h` | Mark up to 2 legacy entry points `[[deprecated]]` (per §8) |

**ABI impact**: Zero new functions; up to 2 functions marked `[[deprecated]]`.
**Risk gate**: Full test suite green; pin-contract test (constructs SoC only through facade ports); CI grep-gate for bypass patterns.

### 7.4 P3 — Fabric + VRAM + Verification (3-5 days)

**Goal**: Remaining 2 IPs + full regression + equivalence scaffold.

| File | Change |
|------|--------|
| `include/tlm/cluster/gpu_internal_fabric.{hh,cc}` | NEW: wrap `crossbar_tlm` / `router_tlm` into named IP (new `SimModule`-derived class) |
| `include/tlm/gpu/vram_controller_tlm.hh` | Promote to top-level via JSON topology reference (keeps `REGISTER_CHSTREAM` per E5) |
| `include/modules_cluster.hh` | Register `GpuInternalFabric` |
| `configs/dgpu_soc_v2_realip.json` | Update: full 4-IP topology (P0/P1/P2/P3 features) |
| `test/test_realip_equivalence.cc` | NEW: same-stimulus equivalence test scaffold (legacy vs IP topology, 5-10 smoke stimuli; OOO rid smoke only) |

**ABI impact**: Zero.
**Risk gate**: Full test suite green; equivalence test passes for representative stimulus set.

**Total effort**: P0 (2-3d) + P1 (5-7d) parallel ≈ 5-7d, + P2 (8-12d), + P3 (3-5d) = **22-32d ≈ 5-6 weeks**.

## 8. 14 ABI Impact Analysis (Corrected Inventory)

**Corrected per E1**: Verified `include/abi/cpptlm_emulator.h` contains exactly **14 ABI functions** (not 23 as AGENTS.md "23 ABI 冻结" wording suggests — that's a doc-drift issue; this spec uses the verifiable 14):

```
 1. cpptlm_emulator_get_device_info
 2. cpptlm_emulator_create
 3. cpptlm_emulator_destroy
 4. cpptlm_emulator_mmio_write
 5. cpptlm_emulator_mmio_read
 6. cpptlm_emulator_pcie_config_write
 7. cpptlm_emulator_pcie_config_read
 8. cpptlm_emulator_msix_init
 9. cpptlm_emulator_msix_update_pending
10. cpptlm_emulator_clear_pending       (NOTE: confirmed by grep; suffix varies)
11. cpptlm_emulator_register_callbacks
12. cpptlm_emulator_register_dma_translate_cb
13. cpptlm_emulator_open
14. cpptlm_emulator_close
```

(Final 14 names verified in §11.2 acceptance gate during P1.)

**Per ADR-DGPU-08 v1.1 exception: up to 2 functions may be marked `[[deprecated]]` with fallback path retained.** v1.0's 5-candidate list was fabricated (Metis + Oracle independently verified 0/5 exist); v1.1 re-picks from real list:

| Candidate | Risk | Replacement | Verdict |
|-----------|------|-------------|---------|
| `cpptlm_emulator_register_dma_translate_cb` (12) | Internal callback registration touched by BAR decode refactor | Same name, behavior preserved via facade | **MARK DEPRECATED** — callers should use existing replacement (no new ABI) |
| `cpptlm_emulator_register_callbacks` (11) | Touched by MSI-X path during P2 facade extraction | Same name, behavior preserved | **CANDIDATE** — decide during P2 implementation |
| Others (1-10, 13, 14) | Not touched | — | **No change** |

**Final list of deprecations: 0-2, decided in P2 commit (not pre-decided).**

## 9. Risk Register (Updated v1.1)

| # | Risk | Probability | Impact | Mitigation |
|---|------|-------------|--------|-----------|
| **R1** | `ch_uint` partial specialization regresses POD/trivially-copyable/aggregate assumptions | Low | High (breaks freeze surfaces, serialization) | `static_assert(std::is_trivially_copyable_v<…>)` + `is_standard_layout` + sizeof identity for W∈{1,2,4,8,16,32,64} + **aggregate-init test** (per Metis finding); full test suite in P0 before anything else |
| **R2** | Facade drift: `PcieControllerIP` delegates but callers keep touching `PcieEndpointIP` internals | Medium | Medium | Pin-contract test + CI grep-gate in `scripts/test/` |
| **R3** | Wire codec lossy-field surprises cause late RTL mismatches | Medium | High | Golden vectors explicitly cover every lossy corner; each marked `LOSSY:` in spec doc |
| **R4** | Frozen-surface violation: P1 `pcie_bundles_tlm.hh` edits | **RESOLVED**: PcieTlpWireBundle moves to new file before hardening | None | n/a |
| **R5** | `ch_uint<W≤64>` ctor-inheritance missing → 100+ call sites break | **RESOLVED**: design explicitly uses `using base::base` | None | n/a |
| **R6** | VramControllerTLM `REGISTER_MODULE` static_assert fail | **RESOLVED**: keep `REGISTER_CHSTREAM`, promote via JSON reference | None | n/a |
| **R7** | P2 BAR decode extraction breaks `[pcie-memory]` 24 cases + `[minimal_dgpu_soc]` 67+ assertions | Medium | High | Full test suite green-gate; equivalence scaffold in P3 |
| **R8** | `[[deprecated]]` on C ABI functions → `-Wdeprecated-declarations` warnings break `-Werror` builds | Low | Medium | Build flag check before marking deprecated; if `-Werror` active, suppress at deprecation site |
| **R9** | In-flight PR conflicts during 5-6 week refactor | Medium | Medium | Branch strategy (see §13) |

## 10. Verification Strategy

### 10.1 Per-phase gate

Each PR lands only after:
- `./test.sh --mode off` reports full green (test count may grow monotonically)
- New tests added in phase report green
- No TODO residue
- Docs/AGENTS.md sync

### 10.2 Pin-contract tests (P2/P3)

For each new IP, a `test_pin_contract_<ip>.cc` constructs a minimal topology using only the IP's pin-contract port list. Any access to legacy internals (e.g., `PcieEndpointIP::internal_*`) fails the test. Guards R2.

### 10.3 Golden-vector tests (P1)

`test_pcie_wire_codec_golden.cc`:
- **Lossless round-trip**: descriptor → wire bytes → wire → descriptor (lossless for all non-LOSSY fields)
- **CRC32 correctness**: against PCIe spec reference vectors
- **BE/LE conversion correctness**: byte-swap verification
- **LOSSY field annotation**: each lossy field documented with sample in/out and marked `LOSSY:` in spec doc

**External ground-truth source**: `wire-format-snapshot.json` (existing) + PCIe Base Spec §2.2.1 reference vectors. NOT self-round-trip-only.

### 10.4 Equivalence scaffold (P3, smoke-only)

`test_realip_equivalence.cc` runs same stimuli against:
- Legacy topology: existing `dgpu_soc_v1` (6 modules)
- IP topology: new `dgpu_soc_v2_realip` (4 IPs)

**Scope: 5-10 stimuli, smoke-level**. OOO rid scenarios are explicitly out-of-scope for the scaffold (full harness is a separate change). Output diff must match within TLM tolerance (latency differences allowed; functional outputs must match).

### 10.5 ABI compatibility test (P2)

For each deprecated function:
- Old call site still works (fallback path retained)
- New IP-path call site works
- Both produce equivalent results

## 11. Phased Acceptance Criteria

### 11.1 P0 acceptance

- [ ] `ch_uint_storage<W>` is sizeof-identical + `is_standard_layout` + `is_trivially_copyable` + aggregate-init compatible to pre-change for W∈{1,2,4,8,16,32,64}
- [ ] `ch_uint_wide<W>` works for W∈{128, 256, 512} (bit-slice, concat, round-trip)
- [ ] All existing tests still green
- [ ] AGENTS.md "KEY INVARIANTS" ch_uint<512> entry updated

### 11.2 P1 acceptance

- [ ] `PcieTlpWireBundle` migrated to new file `include/bundles/pcie_tlp_wire_bundle.hh`; old file has forward-declaration + #include only (zero behavior diff for existing #include sites)
- [ ] `tlp_pack_to_bytes()` / `tlp_unpack_from_bytes()` implement PCIe §2.2.1 TLP header DW packing
- [ ] `crc32_compute()` matches PCIe spec reference vectors
- [ ] BE/LE byte-order spec'd in `docs/architecture/NN-pcie-tlp-wire-format.md`
- [ ] All lossless fields pass round-trip; all LOSSY fields explicitly documented with `LOSSY:` marker
- [ ] `wire-format-snapshot.json` reused (not duplicated) per E6
- [ ] Real 14 ABI function names re-verified and listed in spec §8
- [ ] ADR-DGPU-06 Status Update committed (wire format v1 + new file location + CDC declarations)
- [ ] All existing tests still green

### 11.3 P2 acceptance

- [ ] `PcieControllerIP` (with BAR decode merged per D2) + `PcieAxiBridgeIP` registered in `modules_cluster.hh`
- [ ] 2 pin-contract tests green
- [ ] 0-2 legacy ABI functions marked `[[deprecated]]` per §8 (decided in P2 commit, not pre-decided)
- [ ] CI grep-gate added for facade bypass patterns
- [ ] `dgpu_soc_v2_realip_p2.json` loads and runs E2E
- [ ] All existing tests still green

### 11.4 P3 acceptance

- [ ] `GpuInternalFabric` registered (wraps `crossbar_tlm`/`router_tlm`)
- [ ] `VramControllerTLM` promoted via JSON topology reference (REGISTER_CHSTREAM preserved per E5)
- [ ] `dgpu_soc_v2_realip.json` updated to full 4-IP topology
- [ ] `test_realip_equivalence.cc` passes for 5-10 smoke stimuli
- [ ] All existing tests still green

## 12. References

- AGENTS.md: project overview, KEY INVARIANTS, conventions, frozen 4-header list
- docs/pcie/driver-visible-minimal-soc.md (D-AXI v1.8 architecture, 418 lines)
- include/tlm/gpu/pcie_memory_device.hh (current PcieMemoryDevice, 161 lines)
- include/bundles/pcie_bundles_tlm.hh (frozen; `PcieTlpWireBundle` lines 146-258 will be migrated out)
- include/bundles/cpphdl_types.hh (`ch_uint` definition; will be augmented with specialization)
- include/bundles/axi4_bundles_tlm.hh (AXI4 bundles; `ch_uint<512>` reference at line 51 is comment-only)
- scripts/test/gen_wire_format_snapshot.sh (existing snapshot infrastructure to reuse)
- wire-format-snapshot.json (existing snapshot to extend)
- test/test_pcie_slice_wire_format_snapshot.cc (existing snapshot test to extend)
- include/tlm/gpu/vram_controller_tlm.hh (existing IP to promote; REGISTER_CHSTREAM, not REGISTER_MODULE)
- docs/adr/ADR-DGPU-05-vram-virtualism-ownership.md (single VRAM ownership)
- docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md (chip-internal vs board-level bundle boundary)
- docs/adr/ADR-DGPU-08-abi-freeze-policy.md (14 ABI freeze)
- include/AGENTS.md macro table (REGISTER_MODULE vs REGISTER_CHSTREAM, SimModule vs SimObject)
- openspec/changes/cpptlm-driver-visible-minimal-soc/ (D-AXI change)
- openspec/changes/cpptlm-dgpu-soc-timing-mvp/ (D3 timing-mode, VramControllerTLM origin)

## 13. Governance & Tooling (New in v1.1)

### 13.1 OpenSpec ownership (per D4)

- **1 OpenSpec change**: `dgpu-realip-refactor` in **CppTLM** repo (Proposed stock: 1→2, well under the 3 cap)
- **4 sub-PRs** under that single change: P0, P1, P2, P3 (each separate PR/commit but logically one OpenSpec change)
- **Tasks.md** in the OpenSpec change lists P0-P3 as 4 separate tasks
- **Why not ArchForge**: This change is primarily CppTLM framework + integration work; the IP boundary *intent* is captured in `docs/designs/dgpu-soc/architecture.md` (ArchForge-mirrored) but the actual code changes land in CppTLM.

### 13.2 Branch strategy

- Single feature branch `feature/dgpu-realip-refactor` from `main` (per AGENTS.md commit policy)
- P0, P1 may land in either order (parallel per D5)
- P2 lands only after both P0 and P1 merged (pin-contract depends on wire format)
- P3 lands last (depends on P2)
- No in-flight PRs targeting these files during refactor (per D-AXI archive state)

### 13.3 Consumer story

- `examples/demo_dgpu_soc_realip.cpp` (NEW, P3) — minimal driver-style smoke using new IP topology
- `configs/dgpu_soc_v2_realip.json` (NEW, P2/P3) — sample topology using 4 new IPs
- Documentation update: `docs/designs/dgpu-soc/architecture.md` mirrors new IP boundaries

### 13.4 CDC declaration consumer

- `clock_domains` JSON field parsed by `config_utils` (P1) — schema validation only, no behavior
- `clock_crossings` table in ADR Status Update — named for RTL designer handoff
- Owner: P1 implementation must extend `config_utils` JSON schema

## 14. Maintenance Record

| Date | Version | Author | Notes |
|------|---------|--------|-------|
| 2026-10-06 | v1.0 | Architecture WG | Initial draft — flagged with hard errors (frozen-surface, ABI hallucination) by Metis+Oracle review |
| 2026-10-06 | v1.1 | Architecture WG | 7 hard fixes + 5 scope decisions applied: (E1) Real 14-ABI inventory, (E2) PcieTlpWireBundle migration to new file, (E3) Use-site verified, (E4) ctor inheritance + trivially_copyable, (E5) REGISTER_CHSTREAM for VramControllerTLM, (E6) Reuse wire-format-snapshot.json, (E7) P1 includes packing/CRC32/byte-order; (D1) Fast-path kept + wording softened, (D2) BAR decode folded into PcieControllerIP, (D3) G5 wording softened, (D4) 1 OpenSpec in CppTLM, (D5) P0∥P1 parallel; (G) P1 5-7d, P2 8-12d, total 22-32d |