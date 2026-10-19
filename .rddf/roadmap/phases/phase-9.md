---
id: phase-9
kind: phase
status: active
phase_refs: []
主题: dGPU Phase 2/3: 06 v0.4 + 06-bis + 3 active changes (ControlHub / physical-ip-port-boundary 已 archived)
---

## phase-9: dGPU Phase 2/3 (06 v0.4 + 06-bis + 3 active changes)

> **真源 1 (基础)**: [`docs/architecture/06-inter-module-network.md`](../../docs/architecture/06-inter-module-network.md) v0.4 (主文档)
> **真源 1 (基础)**: [`docs/architecture/06-bis-coherence-as-network-extension.md`](../../docs/architecture/06-bis-coherence-as-network-extension.md) v0.3
> **真源 2 (dGPU 应用)**: [`docs/designs/dgpu-soc/architecture.md`](../../docs/designs/dgpu-soc/architecture.md) §0.1 原则 3 (SoC 内部模块接口贴近真实物理 IP 边界)
> **关键 ADR**: [`docs/adr/ADR-DGPU-10~14-*.md`](../../docs/adr/)
> **OpenSpec KPI**: per AGENTS.md §DOC HYGIENE line 420, **2026-10-19 baseline 3/3 合规** (per archive A+B 后, 控制 hub + physical-ip-port-boundary 已 archived with sync)

### 当前活跃 (Active) — 3 open changes (per 2026-10-19 KPI 释放)

| OpenSpec change | 状态 | 范围 | tasks | 估时 | 关联 phase-9 子任务 |
|-----------------|------|------|------|------|---------------------|
| **cpptlm-msix-generator-ip** | 🟡 Proposed Draft | MSI-X Generator IP + `trigger_msix` API (修复 Gap L) | 44 | — | **9.C MSI-X Generator** |
| **cpptlm-pcie-tlp-entry-layer** | 🟡 Proposed | PcieTlpSerializer + TLP 解码层 (修复 Gap M) | 56 | — | **9.D TLP 解码层** |
| **cpptlm-p2-integration-unblock** | 📋 Proposed (2027-09-17 修订) | P2 (CP 接入) 集成阻塞修复 (Metis + Oracle 2027-09-17 修订版) | 102 | — | **9.E P2 EP↔SoC 4 方向 AXI 桥接** |

**总 tasks 待 execute**: 202 个 (0% 完成)
**KPI**: 3/3 合规 (per AGENTS.md §DOC HYGIENE line 420, 2026-10-19 archive A+B 后)

### 历史归档 (Archived) — 关键 4 个 change (Tranche 0/1 实施完成)

| OpenSpec change | 状态 | 完成日期 |
|-----------------|------|---------|
| **cpptlm-vram-controller-functional-integration** (Change A) | ✅ archived (Tranche 2) | 2026-10-11 |
| **cpptlm-lwc-bus-hub-mvp** (Change B) | ✅ archived (Tranche 3) | 2026-10-11 |
| **cpptlm-gmmu-pte-bus-correction** (Change C) | 🟡 P2 (待 Activate) | — |
| **cpptlm-pcie-host-test-flr-rebar** v1.1 | ✅ archived | 2026-10-09 |
| **cpptlm-dgpu-soc-internal-noc-e1** v1.1 | ✅ archived | 2026-10-09 |

### 2026-10-19 KPI 释放 (per user 决策)

| Archive 路径 | 状态 | 释放 slot |
|--------------|------|----------|
| [`openspec/changes/archive/2026-10-19-cpptlm-dgpu-soc-control-hub/`](../../openspec/changes/archive/2026-10-19-cpptlm-dgpu-soc-control-hub/) | ✅ archived with sync (5 ADDED 主 spec 创建 `openspec/specs/dgpu-soc-control-hub/spec.md`) | 5/3 → 4/3 |
| [`openspec/changes/archive/2026-10-19-cpptlm-dgpu-soc-physical-ip-port-boundary/`](../../openspec/changes/archive/2026-10-19-cpptlm-dgpu-soc-physical-ip-port-boundary/) | ✅ archived with sync (5 ADDED 主 spec 创建 `openspec/specs/dgpu-soc-physical-ip-port-boundary/spec.md`) | 4/3 → 3/3 ✅ 合规 |

### 历史归档 (Archived) — 关键 5 个 change (Tranche 0/1 实施完成)

| OpenSpec change | 状态 | 完成日期 |
|-----------------|------|---------|
| **cpptlm-vram-controller-functional-integration** (Change A) | ✅ archived (Tranche 2) | 2026-10-11 |
| **cpptlm-lwc-bus-hub-mvp** (Change B) | ✅ archived (Tranche 3) | 2026-10-11 |
| **cpptlm-gmmu-pte-bus-correction** (Change C) | 🟡 P2 (待 Activate) | — |
| **cpptlm-pcie-host-test-flr-rebar** v1.1 | ✅ archived | 2026-10-09 |
| **cpptlm-dgpu-soc-internal-noc-e1** v1.1 | ✅ archived | 2026-10-09 |

### 当前 Draft (待完成变更)

- **cpptlm-dgpu-soc-arch-truth-cleanup** v1.3 (2026-10-10) — ✅ doc-only 修复 19 项 finding (Oracle + Metis 双轮复评), **不计入 Proposed KPI 上限** (per Proposal §6 显式声明, doc-only 类)
- **e1 v1.1 双向 Oracle/Metis + RTL 复审** — ✅ 实施完成, 待办 v0.2.0 收尾

### 关键变更背景 (per `docs/designs/dgpu-soc/architecture.md` §2.4)

- **MVP 临时方案**: `SdmaEngineTLM` 5 端口 + `VramControllerTLM` 端口 + `GpuInternalFabric` 内部 wire **全 PcieTlpBundle**, 违背真源 1 §L4-L5 分层
- **目标**: SoC 内部 NoC 接口走 `NoCFlitBundle` (16 字段, per `include/bundles/noc_bundles_tlm.hh:26-50` v2.1 决策), AXI 接口走 AxiBundle/AxiStreamBundle 系列, PCIe 板级接口保持 PcieTlpBundle
- **真实 IP 对应 (per Oracle RTL 调研)**: NVIDIA H100 GMMU 专用 memory client bypass mesh NoC, ARM CMN-700 + SMMUv3 DVM 独立通道, ARM CHI 4 通道 + 4 VC, HBM3 Controller AXI4 5 通道, PCIe EP 硬件 MSI-X Generator

### KPI slot 节流路径 (per AGENTS.md §DOC HYGIENE line 682) — 2026-10-19 已执行 ✅

- 状态: **3/3 合规** (per 2026-10-19 archive A+B 后)
- **路径 A ✅ 已执行**: archive `cpptlm-dgpu-soc-control-hub` (170 tasks) → 4/3
- **路径 B ✅ 已执行**: archive `cpptlm-dgpu-soc-physical-ip-port-boundary` (127 tasks) → 3/3 合规
- **路径 C** (后续): archive Change C (GMMU PTE bus) → activate 后续 Change E (MSI-X Generator, **按 user 决策暂不activate**)
- **路径 D** (后续): archive Change D (TLP Entry Layer) → activate 后续 Change F (Error/CDC)

### 子任务依赖图

```
phase-7 D-AXI Driver-Visible ✅ ──→ phase-8 timing-mode SoC ✅
                                            ↓
                                   phase-9 dGPU Phase 2/3 (current)
                                            ↓
                    ┌──────────────────────┼──────────────────────┐
                    ↓                      ↓                      ↓
            9.A ControlHub         9.C MSI-X Generator    9.E P2 桥接
                    ↓                      ↓                      ↓
            9.B 物理 IP 边界       9.D TLP 解码层
                    ↓
            (后续 Change E/F)
```

### 配套测试标签

- `[realip]` 22 cases / 193 assertions (Step 0 后 +9: cycle 3× 加强)
- `[pcie]` 430 / 36672 assertions
- `[pcie-memory]` 17 / 48
- `[minimal_dgpu_soc]` 12 / 66
- `[dgpu_soc_timing]` 37 / 12024
- `[driver_visible]` 2 / 29

**总 baseline**: 1241 cases / 47634 assertions / 2 known fail (Phase 8 e2e `_config` + `_bar` Minor, per AGENTS.md)