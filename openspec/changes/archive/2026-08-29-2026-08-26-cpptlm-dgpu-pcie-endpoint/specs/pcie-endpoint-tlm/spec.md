# Spec: PcieEndpointTLM (PCIe slave 组件，归属 dGPU SOC)

> **Status**: Proposed — 2026-08-26
> **Scope**: cpptlm-dgpu-pcie-endpoint change
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D2

---

## ADDED Requirements

### Requirement: pcie-endpoint-component

The system MUST provide a `PcieEndpointTLM` class in `include/tlm/gpu/pcie_endpoint_tlm.hh` + `src/tlm/gpu/pcie_endpoint_tlm.cc` inheriting from `ChStreamModuleBase`, registered via `REGISTER_CHSTREAM`, modeling the PCIe Endpoint IP block that resides **inside the dGPU SOC die** (per real-hardware topology). The component MUST own: PCIe Config Space (parameterized 256B or 4KB + capability chain), BAR0 register routing table (data-driven, no hardcoded offset if-else in C++), BAR1 VRAM aperture mapping (forwarded to a memory component port, not owned), and the MSI-X table (vector count parameterized, default 16).

The component MUST expose exactly 4 ChStream ports (`num_ports() == 4`) and MUST be instantiable from JSON via `ModuleFactory::instantiateAll`:

| Port | Direction | Bundle |
|------|-----------|--------|
| `slave_in` | ingress | `PcieTlpBundle` |
| `mmio_out` | egress | `PcieTlpBundle` |
| `mem_out` | egress | `PcieTlpBundle` |
| `irq_out` | egress | `MsiXDeliveryBundle` |

#### Scenario: JSON instantiation with multi-port adapter injection
- **WHEN** a JSON config declares `{ "name": "pcie_ep", "type": "PcieEndpointTLM", "params": { ... } }`
- **THEN** `ModuleFactory::instantiateAll` MUST construct the component and inject 4 StreamAdapters via the multi-port `set_stream_adapter(adapters[])` path (PE-G1, PE-G5)

#### Scenario: Doorbell register write is table-driven
- **WHEN** a BAR0 MMIO_WRITE arrives at offset declared with `side_effect: "doorbell"` in `params.bar0_registers`
- **THEN** the component MUST emit a doorbell transaction on `mmio_out` preserving s2 strong-order write semantics (250-700ns window), and the register offset MUST come from configuration data, not a C++ if-else chain (PE-G3)

#### Scenario: All 4 ports receive non-null StreamAdapter
- **WHEN** `ModuleFactory::instantiateAll` finishes for a `PcieEndpointTLM` instance
- **THEN** the component MUST have received a non-null adapter for each of the 4 ports (`slave_in`, `mmio_out`, `mem_out`, `irq_out`); the component MUST override `set_stream_adapter(cpptlm::StreamAdapterBase* adapters[])` to bind each port (in port-order index 0..3) and MUST NOT rely on the default base-class implementation that only consumes `adapters[0]` (PE-G1, PE-G5)

### Requirement: pcie-config-space-and-msix

The Config Space sub-block MUST support capability chain walking declared via JSON (`capabilities: [{id, offset, next}]`), and MUST return `0xFFFFFFFF` for reads to unimplemented offsets. The MSI-X sub-block MUST support `update_pending(vector)` → `irq_out` delivery and `clear_pending(vector)`, and MUST reject out-of-range vectors.

The MSI-X sub-block MUST additionally expose per-vector mask bits (PBA-style): `set_mask(vector, masked)` / `clear_mask(vector)` / `is_masked(vector)` MUST be supported; `update_pending(vector)` MUST NOT emit an `irq_out` bundle while the vector's mask bit is set; mask bit semantics MUST match the PCI-SIG MSI-X spec table layout (bit set = masked).

#### Scenario: Capability chain walk
- **WHEN** the test walks the capability chain via config reads
- **THEN** all declared capabilities MUST be reachable in order and the chain MUST terminate cleanly (PE-G2)

#### Scenario: MSI-X delivery path
- **WHEN** `update_pending(3)` is invoked with vector 3 unmasked
- **THEN** an `MsiXDeliveryBundle` for vector 3 MUST be emitted on `irq_out` (PE-G4)

#### Scenario: Masked vector does not deliver IRQ
- **WHEN** `set_mask(3, true)` is called and then `update_pending(3)` is invoked
- **THEN** NO `MsiXDeliveryBundle` MUST be emitted on `irq_out` until `set_mask(3, false)` is called (mask semantics per PCI-SIG MSI-X) (PE-G4)

### Requirement: pcie-bar1-bulk-path

Per ADR-SOC-07 Status Update Q3, BAR1 MEM writes carrying `PcieTlpBundle.size > 8` bytes MUST NOT be carried inline in the bundle (which is POD with a `uint64_t data` field). The component MUST support the bulk-data path: when `size > 8`, the board shell handles actual data movement via the `cpptlm_emulator_backdoor_write` ABI, and the component MUST still emit a descriptor-only `PcieTlpBundle{MEM_WRITE, offset, size, data=0}` on `mem_out` to advance the bandwidth/timing model, with downstream `MemoryTLM` injecting `ceil(size/burst_width)`-cycle latency.

#### Scenario: BAR1 MEM write >8 bytes uses backdoor path
- **WHEN** a BAR1 MEM_WRITE arrives with `size > 8`
- **THEN** the component MUST emit a descriptor-only `PcieTlpBundle` on `mem_out` with `data == 0` and `size` reflecting the actual transfer byte count (PE-G3)
- **AND** the component MUST NOT attempt to carry the bulk data in the bundle (no `shared_ptr`/inline buffer, preserving POD convention)
