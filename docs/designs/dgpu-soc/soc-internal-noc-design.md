# SocInternalNoc 设计 — 指针化文档 (2026-10-09)

> **状态**: 🟡 **指针化定位** — 原 310 行设计稿 (`2026-10-09-cpptlm-dgpu-soc-internal-noc-minimal`) 已大部分被下游 OpenSpec change 实施真相替换
> **保留原因**: 文档原"5-IP 边界 + CppTLM vs ArchForge scope 决策"对后续 SoC 内部 NoC 集成仍有指引价值,但具体路由 / NoCFlitBundle 字段 / 装配流程等已与下述 OpenSpec change **严重重复**,**指针到对应 change 4 件套**作为权威源
> **OpenSpec change 实施真相如下 (本目录团队已起草/已实施)**:

## 1. 旧文档结构映射 (原 §1-§10 → 当前 OpenSpec change)

| 原文档结构 | 当前权威源 (OpenSpec change 4 件套) |
|----------|-------------------------------------|
| §1 背景与动机 (§1.1 问题陈述, §1.2 现状 4-IP, §1.3 CppTLM vs ArchForge) | 仍有效 (legacy concept) — 见下方 §2 保留 |
| §2.1 模块位置 + §2.2 5-IP 边界 | [`docs/designs/dgpu-soc/architecture.md` §2.4](../../designs/dgpu-soc/architecture.md) (实时维护) |
| §2.3 SocInternalNoc 接口 + §5 NoCFlitBundle 端口契约 | [`openspec/changes/cpptlm-gmmu-pte-bus-correction/design.md`](../../../../openspec/changes/cpptlm-gmmu-pte-bus-correction/design.md) (含 NoC 4 NODE_ID + 16 字段) |
| §3 路由语义 | [`openspec/changes/cpptlm-gmmu-pte-bus-correction/spec.md`](../../../../openspec/changes/cpptlm-gmmu-pte-bus-correction/spec.md) (e1 GMMU bypass + DataNoC) |
| §4 装配流程 (§4.1 JSON, §4.2 双注册, §4.3 DGpuBoard 桥接) | 同上 |
| §6 测试覆盖 | [`openspec/changes/cpptlm-gmmu-pte-bus-correction/tasks.md`](../../../../openspec/changes/cpptlm-gmmu-pte-bus-correction/tasks.md) (TDD 5-step + 16 commits) |
| §7 设计权衡 | 保留精简 (见 §3) |
| §8 演进路径 E.1/E.2/D3 | e1 v1.1 已 ✅ archived (PathSpec);D3/Phase 1 见 [`docs/designs/dgpu-soc/architecture.md` §0.2](../../designs/dgpu-soc/architecture.md) |
| §9 Frozen surface + §10 关联资源 | 23 ABI 零 diff 见 [`docs/designs/dgpu-soc/architecture.md` §0.1 §X](../../designs/dgpu-soc/architecture.md) |

## 2. 保留内容 (§1 背景与动机)

### 2.1 问题陈述 (legacy, 仍有效)

`dgpu-realip-pin-contract` Change A (`Fabric2VramCtrlBundle`) 只覆盖了 `GpuInternalFabric` ↔ `VramControllerTLM` 一个桥接。**SoC 内部 4 个核心模块 (GMMU/SDMA/PCIe/VRAM) 的端到端数据路径未落地** — 这是 `dgpu-realip-pin-contract` Change A 之外的下一阶段。

### 2.2 CppTLM vs ArchForge Scope 决策 (per AGENTS.md 季度复审产物)

| CppTLM scope (本仓) | ArchForge scope (defer) |
|---------------------------|--------------------------|
| "功能路由 + 接口兼容" + minimal_v1 driver 闭环 | 完整 SoC 架构建模 |
| DataNoC only (per e1 v1.1) | multi-VC 仲裁 + adaptive routing |
| `NoCFlitBundle` 16 字段编译期契约 + 4 NODE_ID | mesh XY + congestion + backpressure |
| Programmatic bridge via setters (`bind_memory_backings`) | latency / cycle-approximate timing |
| 0 latency / 0 arbitration | cycle-accurate |
| **仿真框架 + 基本 SoC 验证** | **SoC 架构建模 (跨仓镜像)** |

**接口对齐**已通过 `NoCFlitBundle` 16 字段 (mesh XY + VC + flit_type) 落地。**NoC 物理行为建模不是对齐目标**,是 ArchForge 范围。

### 2.3 与 minimal_v1 拓扑的关系 (4-IP 边界)

| 模块 | 真实 IP 对应 | CppTLM 当前实现 | 状态 |
|------|--------------|----------------|------|
| `PcieControllerIP` | DWC i5122 / Cadence PCIe Controller | 顶层 facade, `sim_module` 17 ports | ✅ Change B |
| `PcieAxiBridgeIP` | Synopsys AXI↔PCIe bridge | composite, frozen surface | ✅ Change B |
| `GpuInternalFabric` | Custom mesh / Arteris NoC IP | 4 ports default = CrossbarTLM | ✅ Change B v1.0 |
| `SocInternalNoc` | E0 路由骨架 | 4 NODE_ID + 16 字段 + vc_counts_[4] | ✅ archived 2026-10-09 |
| `VramControllerTLM` | HBM PHY / DFI-lite | `MemoryTLM` 子类 + 行缓冲 + bw counters | ✅ (mixed-mode per §1.4) |

## 3. 设计权衡 (核心保留)

### 3.1 为什么独立模块而非扩展 `GpuInternalFabric`?

- `GpuInternalFabric` 名字暗示"GPU 内部", 实际只承担 1 个桥接 (`vram_ctrl` via `Fabric2VramCtrlBundle`)
- chip-internal 4 模块 (GMMU/SDMA/PCIe/VRAM) 数据路径需要 **独立模块** 表达路由逻辑, 不被 GPU 内 fabric 命名混淆
- `SocInternalNoc` 是 chip-internal 路由骨架, 命名中性, 适用于全 SoC

### 3.2 为什么 `NoCFlitBundle` 而非 `Fabric2VramCtrlBundle`?

- `Fabric2VramCtrlBundle` 是 `GpuInternalFabric` ↔ `VramControllerTLM` 的 8 字段桥接口 (per `dgpu-realip-pin-contract` Change A)
- `NoCFlitBundle` 是 chip-internal SoC NoC 的 16 字段通用路由bundle (mesh XY + VC + flit_type)
- 用途不同: `Fabric2VramCtrl` 是窄义专用, `NoCFlit` 是广义路由; 二者不冲突, 由 `VramNocAdapter` 在中间做转换 (per e1 v1.1 design)

### 3.3 为什么本期不切 4 target 模块的 wire format?

- 当前 4 个 target 模块 (GMMU/SDMA/PCIe/VRAM) 各自有 **不同的 wire-format 协议**:
  - GMMU PTE walk: chip-internal AXI (AxiMemBundle, Gap I 修复方向)
  - SDMA bulk DMA: 经 chip-internal NOC (DataNoC, Gap G2 修复方向)
  - PCIe Controller → MMIO: PcieTlpBundle (frozen, real RTL = AXI4)
  - VRAM Controller: AXI4 (real RTL = HBM Controller AXI)
- 本期 (`dgpu-realip-pin-contract`) **接口契约 (programmatic bridge) 已对齐**, **4 target 物理行为建模** (cycle-accurate timing) 属 ArchForge 范围

## 4. 权威源 (Authoritative Sources)

- **架构视图**: [`docs/designs/dgpu-soc/architecture.md`](../../designs/dgpu-soc/architecture.md) (重点 §0.1 §0.1.1 §0.2)
- **Phase 1 OpenSpec change**: [`openspec/changes/cpptlm-gmmu-pte-bus-correction/`](../../../../openspec/changes/cpptlm-gmmu-pte-bus-correction/) (Phase 1, Change C, P2, 修复 Gap I + G2 + G1)
- **E0 路由骨架 (✅ archived)**: [`cpptlm-dgpu-soc-internal-noc-minimal`](../../../../openspec/changes/archive/2026-10-09-cpptlm-dgpu-soc-internal-noc-minimal/) (设计稿; 已落地为 `include/tlm/cluster/soc_internal_noc.hh`)
- **E.1 v1.1 (✅ archived)**: [`cpptlm-dgpu-soc-internal-noc-e1`](../../../../openspec/changes/archive/2026-10-09-cpptlm-dgpu-soc-internal-noc-e1/) (DataNoC 物理保真; 4 NODE_ID + NoCFlitBundle 16 字段 + vc_counts_[4] + 2 adapter)
- **Frozen surface 锁定**: `include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h` + `include/tlm/gpu/pcie_display_device.hh` + `include/tlm/pcie/pcie_bundles_tlm.hh` (per AGENTS.md H3)

## 5. 维护规则

1. **本文档不再扩展**: 新增内容直接写到 OpenSpec change 4 件套
2. **指针保持指向 4-IP 实时维护文档**: `architecture.md` §2.4 + `cpptlm-gmmu-pte-bus-correction` (Phase 1 实施)
3. **历史追溯**: 完整 310 行原版已归档在 git history (`git log -- docs/designs/dgpu-soc/soc-internal-noc-design.md`),需要可查阅
4. **如需重新打开**: 用户决策后, 由 Sisyphus 重新激活并按 archive 流程 `openspec sync` 同步到 main specs/
