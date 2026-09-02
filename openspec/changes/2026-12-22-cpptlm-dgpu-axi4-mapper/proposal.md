# cpptlm-dgpu-axi4-mapper: AXI4Mapper 独立模块

> **状态**: Proposed — 2026-12-22
> **父 change**: `2026-09-01-cpptlm-dgpu-pcie-ip-microarch`
> **前置**: Phase 5 (AXI Stream Adapter)

## Why

AXI4 有 Outstanding / Out-of-order / W-channel 等复杂特性，**不应内嵌在 PcieEndpointIP**:
- 内嵌 → 复杂度爆炸 + 与 SoC 其他 AXI 用户耦合
- 独立模块 → 可被 CrossbarTLM / CacheTLM 复用 + 按 JSON 可选注入
- OOO 核心机制: AXI4 通过 `rid` 把乱序 `rdata` 关联回原事务（design.md §6.4 Oracle 修订）

## What Changes

| 文件 | 用途 |
|---|---|
| `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc` | **新**: AXI4 ↔ Bundle Mapper（独立模块，outstanding 跟踪 + OOO 调度） |
| `include/framework/axi4_signal_to_bundle.hh` + `src/framework/axi4_signal_to_bundle.cc` | **新**: AXI 信号 ↔ Axi4Bundle 转换 |
| `include/framework/axi4_bundle_to_signal.hh` + `src/framework/axi4_bundle_to_signal.cc` | **新**: Axi4Bundle ↔ AXI 信号 转换 |
| `test/test_axi4_mapper_basic.cc` | **新**: AXI4 ↔ Bundle 基础测试 |
| `test/test_axi4_mapper_outstanding.cc` | **新**: Outstanding transaction 测试 |
| `test/test_axi4_mapper_out_of_order.cc` | **新**: Out-of-order completion 测试（rid 关联） |
| `test/test_axi4_mapper_integration_with_pcie.cc` | **新**: 与 PcieEndpointIP 集成测试 |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 显式注册源与测试 |

## Scope

- AXI4Mapper 独立于 PcieEndpointIP，消费 Phase 5 的 `Axi4Bundle`/`Axi4LiteBundle`
- Outstanding 跟踪 + OOO completion（rid 关联回原事务）
- 可选按 JSON 注入（`axi4_mapper_inject: true`，design.md §8）
- 不在本 Phase 实现 Host Bypass / RC（Phase 7）

## Acceptance Gate

- `openspec validate --strict` PASS
- 4 个 Phase 6 测试 PASS（basic / outstanding / out_of_order / integration_with_pcie）
- Phase 1-5 PCIe/AXI 测试无回归
- `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改
