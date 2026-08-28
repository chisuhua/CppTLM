# pcie-slice-prerequisites Specification

## Purpose
TBD - created by archiving change 2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites. Update Purpose after archive.
## Requirements
### Requirement: pcie-slice-wire-format-in-repo-guarded

The system MUST enforce byte-level layout stability for `PcieTlpBundle` / `DmaDescriptorBundle` / `CompletionBundle` / `MsiXDeliveryBundle` (all `bundle_base`-derived types) via compile-time `static_assert`. Any field reordering or size change MUST fail the build.

> **Scope 重锚定注记 (per Oracle 审查 2026-08-28 + Metis 审查 2026-08-28)**:
> 原需求描述 "防止跨仓 silent drift" 不完全成立:
> - `bundle_serialization.hh:23-27` 自带注释"仅在单一仿真进程内使用"
> - UsrLinuxEmu 经 23 ABI C 符号消费,从不 memcpy 这些 TLM Bundle
> - 真正跨仓 ABI 由 `include/cudart/abi_guards.h` G-D4 17 条静态断言承担 (per AGENTS.md CROSS-PROJECT)
>
> 本需求**实际作用是仓内布局守卫**: 防止 `cpptlm-dgpu-board-soc-split` (change C) 集成时本仓 bundle 布局被意外修改。

#### Scenario: PcieTlpBundle layout snapshot
- **WHEN** a developer reorders or resizes `PcieTlpBundle` fields
- **THEN** `static_assert` in `test_pcie_slice_wire_format_snapshot.cc` fails to compile (PR-G1, 仓内布局守卫)

#### Scenario: DmaDescriptorBundle layout snapshot
- **WHEN** a developer changes `DmaDescriptorBundle` fields
- **THEN** build fails (PR-G1, 仓内布局守卫)

#### Scenario: CompletionBundle layout snapshot
- **WHEN** a developer changes `CompletionBundle` fields
- **THEN** build fails (PR-G1, 仓内布局守卫)

#### Scenario: bundle_base standard-layout invariant
- **WHEN** a developer derives from `bundle_base` (per `cpphdl_types.hh:36`)
- **THEN** `std::is_standard_layout_v<T>` MUST be true, ensuring `memcpy` serialization correctness (per `bundle_serialization.hh:23-37`)

#### Scenario: snapshot file consistency (per Metis)
- **WHEN** a developer changes a bundle field
- **THEN** the generated `wire-format-snapshot.json` MUST also be regenerated (`scripts/gen_wire_format_snapshot.sh`) — 否则 1 1 1 1 1 测试失败

### Requirement: sdma-translate-callback-contract

The `SdmaEngineTLM::DmaTranslateCb` callback (per ADR-088 §D3.8) MUST handle the following edge cases without crashing the simulation:

| 场景 | 期望行为 |
|------|---------|
| translate 返回非 0 (IOMMU fault) | error completion + board error channel |
| translate 返回 phys 但 phys > host_backdoor_size | 请求拒绝(MVP 容忍,不 panic) |
| translate callback 抛 C++ 异常 | 组件不崩溃(per spec R3 abort semantics) |
| translate callback 重入调用 | 线程安全(单线程 EventQueue 保证顺序) |

#### Scenario: phys 越界拒绝
- **WHEN** translate returns `phys > host_backdoor_size()`
- **THEN** request is rejected without memcpy; VRAM is unmodified; done_out error completion (PR-G2)

#### Scenario: callback 异常处理
- **WHEN** translate callback throws `std::exception`
- **THEN** simulation continues; error completion emitted; no UB / segfault (PR-G2)

#### Scenario: callback 重入安全
- **WHEN** multiple `desc_in` are processed in same tick, each calling translate
- **THEN** all callbacks complete in order; no race; no state corruption (PR-G2)

### Requirement: msix-state-machine-fine-grained

`MsiXTable` MUST correctly handle vector mask/unmask + PBA bit synchronization per VFIO `SET_IRQS` semantics.

#### Scenario: mask 期间 pending 不丢失
- **WHEN** vector is masked, then pending is set, then vector is unmasked
- **THEN** pending state is preserved across mask period; trigger fires after unmask (PR-G3, VFIO SET_IRQS 契约)

#### Scenario: PBA bit 与 vector pending 同步
- **WHEN** vector N has pending=true
- **THEN** PBA bit N is set; reading PBA returns bitmask consistent with vector pending (PR-G3)

### Requirement: doorbell-queue-stability

`PcieBarRouter::Doorbell` MUST handle burst MMIO_WRITE writes without losing events.

#### Scenario: 100 次连续 MMIO_WRITE
- **WHEN** 100 `MMIO_WRITE` to doorbell offset in single tick burst
- **THEN** all 100 events are queued; SQ tail advances by 100; no event lost (PR-G4)

#### Scenario: latency 区间稳定性(per design §3)
- **WHEN** multiple doorbell writes are issued
- **THEN** each doorbell side effect latency falls within `[250, 700]` cycles range (PR-G4)

### Requirement: sdma-engine-backdoor-isolation (scope 降级 per Oracle 审查 2026-08-28)

`SdmaEngineTLM::set_host_backdoor(ptr, size)` / `set_vram_backdoor(ptr, size)` MUST reject out-of-range accesses without writing the backdoor memory.

> **Scope 降级注记 (per Oracle 审查 2026-08-28)**:
> 原需求描述 "backdoor_read/write 不影响 BAR0 寄存器" 需要 `DGpuBoardTLM::read_vram/write_vram` (change C, 未实施) 或 `PcieEndpointTLM::backdoor_*` (无此 API) 或 `cpptlm_backdoor_read/write` (change D, 未实施)。本 change 测不到。
>
> 本需求**实际覆盖**: SdmaEngineTLM 已交付的 `set_host_backdoor()` / `set_vram_backdoor()` API 的隔离行为。

#### Scenario: host_backdoor 越界拒绝
- **WHEN** host_backdoor ptr is set with `size_size=N`, and translate callback returns `phys > N`
- **THEN** SdmaEngineTLM does NOT write VRAM; done_out emits error completion; no memcpy (PR-G5, per change B §R6)

#### Scenario: vram_backdoor 越界拒绝
- **WHEN** vram_backdoor ptr is set with `size_size=N`, and descriptor has `vram_offset + size > N`
- **THEN** SdmaEngineTLM does NOT write VRAM; done_out emits error completion; no memcpy (PR-G5, per change B §R6)

#### Scenario: host + vram backdoor 互不污染
- **WHEN** both host_backdoor and vram_backdoor are set
- **THEN** writing host_backdoor does NOT affect vram_backdoor region (per change B set_host_backdoor/set_vram_backdoor 独立性)

### 【延至 change C/D】 Backdoor-vs-BAR0 隔离 (本 change 无法测试)

> **⚠️ 跨 change 依赖注记 (per Metis 审查 2026-08-28)**: 以下 Scenario 属 change C (Board shell) + change D (ABI export) 范围,本 change 无法直接验证。建议在 C 的 T-bs-2 (Doorbell/DGpuBar 迁移) + D 的 AE-G3 (23 ABI 端到端) 中实现并验证。本 spec 不创建对应的 Requirement (避免 openspec validate 误判)。

**待实现场景 (本 change Out of Scope, 留待 C/D)**:

- `cpptlm_backdoor_read(bar0_offset)` 应拒绝并保持 BAR0 寄存器不变 (per ADR-088 §D3.7, change D 交付)
- backdoor 越界访问应返回 `-ERANGE` 且无内存损坏

---

**Maintenance**: CppTLM Team (Sisyphus)
**Status**: 📝 Proposed — 待评审后启动 T-prereq-1~5 实施

