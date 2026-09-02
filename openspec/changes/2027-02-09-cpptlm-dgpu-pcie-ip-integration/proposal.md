# cpptlm-dgpu-pcie-ip-integration: Phase 8 最终整合交付

> **状态**: Proposed — 2027-02-09
> **父 change**: `2026-09-01-cpptlm-dgpu-pcie-ip-microarch`
> **前置**: Phase 1-7 全部完成（含 Phase 7 Oracle PASS 含 M1/M2 条件）

## Why

7 阶段 PCIe IP 微架构（Link Layer / Encoding / PHY Digital / SR-IOV / AXI Stream Adapter / AXI4Mapper / Host Bypass + RC）已分阶段实施完成，最终整合交付：
- **架构文档迁移**：从 umbrella `design.md`（931 行）迁移到 `docs/architecture/14-pcie-ip-microarchitecture.md`
- **完整 dGPU SOC + PCIe EP 示例配置**（顶层交付）
- **全链路 E2E 测试**（解决 Phase 7 Oracle M1 缺口：EP 真实消费 AXI 请求并返回真实响应）
- **旧代码迁移**：移除 `PcieEndpointTLM` 从 `chstream_register.hh` 注册，加 `[[deprecated]]` 提示迁移到 `PcieEndpointIP`

## What Changes

| 文件 | 用途 |
|---|---|
| `docs/architecture/14-pcie-ip-microarchitecture.md` | **新**: 完整架构文档（由 `openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md` 迁移，含 Phase 7 Oracle M2 标注：RC 枚举 PF0-only 简化模型） |
| `examples/dgpu_soc_with_pcie_ip.json` | **新**: 完整 dGPU SOC + PCIe EP 示例配置（PcieEndpointIP + HostBypassTLM + PcieRootComplexTLM + AXI4Mapper 可选注入） |
| `test/test_pcie_endpoint_ip_full_e2e.cc` | **新**: 全链路 E2E 测试（解决 Phase 7 M1：EP 真实消费 AXI 请求并返回真实响应） |
| `include/chstream_register.hh` | **改**: 移除 `PcieEndpointTLM` 注册，保留 `PcieEndpointIP` |
| `include/tlm/gpu/pcie_endpoint_tlm.h` | **改**: 加 `[[deprecated]]` 提示迁移到 `PcieEndpointIP` |
| `cpptlm_config/` 或 `scripts/migrate_pcie_endpoint_tlm.py` | **改**: JSON config 迁移工具（旧 PcieEndpointTLM → PcieEndpointIP） |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 注册新 E2E 测试 + 迁移脚本（如有） |

## Scope

- 整合模块 `PcieEndpointIP` 已包含 §1-§7 全部组件（Phase 4 实现），不再修改
- 文档迁移：以 `umbrella/design.md` 为源，按 `docs/architecture/14-pcie-ip-microarchitecture.md` 结构组织
- 全链路 E2E 测试：PcieEndpointIP ↔ HostBypassTLM ↔ AXI 真实数据路径闭环
- 旧代码迁移：仅 `chstream_register.hh` + `.h` 标记，不删除（23 ABI 保留原则）
- Phase 7 Oracle 条件：
  - **M1（必做）**：HostBypass/RC ↔ PcieEndpointIP AXI 数据路径真实接线（EP 消费请求 + 产生响应）
  - **M2（必标注）**：架构文档明确 RC 枚举为 PF0-only 简化模型

## Acceptance Gate

- `openspec validate --strict` PASS
- 全链路 E2E 测试 PASS（EP 真实消费 AXI 请求）
- Phase 1-7 测试无回归
- `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改（23 ABI 冻结）
- `examples/dgpu_soc_with_pcie_ip.json` 通过 validates_topology
- 架构文档 `docs/architecture/14-pcie-ip-microarchitecture.md` 包含 Phase 7 Oracle M2 标注
