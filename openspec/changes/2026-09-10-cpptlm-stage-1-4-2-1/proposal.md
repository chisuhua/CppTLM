# Proposal: cpptlm-stage-1-4-2-1 — 电源管理 + P2P/Resizable BAR

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 1.5 周
> **优先级**: P1（性能增强层，5.5.8 不阻塞）
> **前置依赖**: CppTLM `2026-09-10-cpptlm-stage-1-3-sdma` ship

---

## Why

阶段 1.4 电源管理 + 阶段 2.1 P2P + Resizable BAR 实施。电源管理确保 PCIe 设备 D0/D3 状态切换；P2P + Resizable BAR 启用 multi-root switch 性能优化。两者结合实现 dGPU 完整电源 + 拓扑能力。

**Why 单独 change（方案 B）**：1.4+2.1 都是 P1 性能增强层，独立于关键路径（5.5.8 阶段 3 不依赖它们），可独立 Wave 推进。

## What Changes

### 阶段 1.4 电源管理（0.5 周）
- `pcie_config_space_per_vf_tlm.cc`：PM Capabilities/Control/Status 寄存器
- `pcie_endpoint_ip.cc` + `dgpu_soc.cc`：D0/D3hot/D3cold 状态切换
- ASPM（L0s/L1）自动协商

### 阶段 2.1 P2P + Resizable BAR（1 周）
- `pcie_bypass_mux.cc` + `pcie_ari_router_tlm.cc`：P2P DMA 路由
- ACS Capability 寄存器 + 路由策略
- `pcie_config_space_per_vf_tlm.cc`：Resizable BAR Capability

## Impact

- **修改**：`pcie_config_space_per_vf_tlm.cc` + `pcie_endpoint_ip.cc` + `dgpu_soc.cc` + `pcie_bypass_mux.cc` + `pcie_ari_router_tlm.cc`
- **不改**：23 ABI / 5 ports wire-format
- **下游**：UE `ue-stage-1-4-2-1-extensions` 集成 + 5.5.9 真机验证（5.5.8 后）

## Oracle 复审

- 1 次轻量复审
- 验收标准：见 specs/cpptlm-stage-1-4-2-1/spec.md

## refs

- pcie-config-space spec
- ADR-069 BAR + ioremap
- 上游 `stage-1-3-sdma`
