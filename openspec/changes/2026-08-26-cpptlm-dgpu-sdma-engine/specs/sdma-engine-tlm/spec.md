# Spec: SdmaEngineTLM（PCIe master / DMA 引擎，归属 dGPU SOC）

> **Status**: Proposed — 2026-08-26
> **Scope**: cpptlm-dgpu-sdma-engine change
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D3 · UsrLinuxEmu ADR-088 §D3.8

---

## ADDED Requirements

### Requirement: sdma-engine-component

The system MUST provide an `SdmaEngineTLM` class in `include/tlm/gpu/sdma_engine_tlm.hh` + `src/tlm/gpu/sdma_engine_tlm.cc` inheriting from `ChStreamModuleBase`, registered via `REGISTER_CHSTREAM`, modeling the SOC-internal PCIe **master** engine (SDMA/copy engine). The component MUST support both H2D (host→device) and D2H (device→host) transfers described by `DmaDescriptorBundle` values (POD bundle delivered over `desc_in`; the C++ `DmaDescriptor` type in `include/tlm/gpu/dma_descriptor_mvp.hh` is the component-API type, internally converted to/from the bundle), and MUST be instantiable from JSON with 5 ports. The `CompletionBundle` type (carrying `task_id`, `status`, `tag`) is owned exclusively by this change and MUST be reused by any consumer (notably board-soc-split) — no other change defines a same-named bundle type in the `bundles` namespace.

Port order (matches `set_stream_adapter(adapters[])` index per `design.md §2.5` Port index ordering lock — index 0 ingress first by declaration order, then egress — per `chstream_module.hh:37-47` multi-port adapter convention):

| Port | Direction | Bundle |
|------|-----------|--------|
| `desc_in` | ingress | `DmaDescriptorBundle` |
| `mem_in` | ingress | `PcieTlpBundle` |
| `mem_out` | egress | `PcieTlpBundle` |
| `host_out` | egress | `PcieTlpBundle` (descriptor-only; bulk data via board backdoor ABI) |
| `done_out` | egress | `CompletionBundle` |

Upstream (host-bound) transactions MUST be emitted on `host_out` and routed by the DGpuBoard shell through the registered DMA-translate callback (`cpptlm_dma_translate_cb` per ADR-088 §D3.8) before reaching host memory. The component MUST NOT implement any system IOMMU behavior itself.

#### Scenario: H2D transfer completes end to end
- **WHEN** a descriptor `{dir=H2D, host_iova, vram_offset, size, tag}` arrives on `desc_in` and the translate callback succeeds
- **THEN** data MUST be read from host (via `host_out`), written to VRAM (via `mem_out`), and a completion with the matching `tag` MUST be emitted on `done_out` (SD-G2)

#### Scenario: D2H transfer completes end to end
- **WHEN** a descriptor `{dir=D2H, ...}` arrives on `desc_in`
- **THEN** data MUST be read from VRAM (via `mem_in`), written upstream (via `host_out`), and completion emitted on `done_out` (SD-G3)

### Requirement: sdma-engine-error-paths

When the DMA-translate callback returns non-zero, the component MUST simulate a PCIe RequesterCompleterAbort: emit an error completion on `done_out` AND surface the fault through the board error channel (`cpptlm_error_cb_t` path). Descriptors with out-of-range VRAM windows or `size == 0` MUST be rejected with an error completion without issuing any memory transaction.

#### Scenario: IOMMU translation fault
- **WHEN** the translate callback returns non-zero for an H2D descriptor
- **THEN** no VRAM write MUST occur, an error completion MUST be emitted on `done_out`, and the board error channel MUST be signaled (SD-G4)

#### Scenario: Invalid descriptor rejected
- **WHEN** a descriptor with `size == 0` or an out-of-range `vram_offset` arrives
- **THEN** the descriptor MUST be rejected with an error completion and no `host_out`/`mem_*` transaction MUST be emitted (SD-G4)

### Requirement: sdma-completion-ordering

For H2D: `done_out` completion with `status==0` MUST imply that all VRAM writes issued by this descriptor are visible to subsequent reads on the same board (data-visibility fence semantics). For D2H: `done_out` completion with `status==0` MUST imply that all upstream host writes issued by this descriptor are visible at the host side. When `max_inflight > 1` and multiple descriptors are in flight, completions MUST be emitted in descriptor-`tag` order (in-order completion per ADR-088 v1.0 conservative semantics; out-of-order completion is a future v1.x extension).

#### Scenario: Completion implies VRAM write visibility (H2D)
- **WHEN** an H2D descriptor completes with `status==0` on `done_out`
- **THEN** a subsequent VRAM read on the same board at the same offset MUST return the data written by this descriptor (SD-G2)

#### Scenario: In-order completion with max_inflight > 1
- **WHEN** 2+ descriptors are submitted to `desc_in` with `max_inflight=4` and tags {1, 2, 3}
- **THEN** `done_out` MUST emit completions in tag order {1, 2, 3} (in-order completion) (SD-G2)
