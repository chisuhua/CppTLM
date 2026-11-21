## MODIFIED Requirements

### Requirement: pcie-slice-wire-format-in-repo-guarded

The system MUST enforce byte-level layout stability for `PcieTlpBundle` / `DmaDescriptorBundle` / `CompletionBundle` / `MsiXDeliveryBundle` (all `bundle_base`-derived types) via compile-time `static_assert`. Any field reordering or size change MUST fail the build.

> **Scope 重锚定注记 (per Oracle 审查 2026-08-28 + Metis 审查 2026-08-28)**:
> 原需求描述 "防止跨仓 silent drift" 不完全成立:
> - `bundle_serialization.hh:23-27` 自带注释"仅在单一仿真进程内使用"
> - UsrLinuxEmu 经 23 ABI C 符号消费,从不 memcpy 这些 TLM Bundle
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

