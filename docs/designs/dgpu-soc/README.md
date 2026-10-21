# DGpu SoC 业务架构 (Minimal SoC Business Architecture)

> **状态 v0.5.1 (2026-10-09 修订, per Metis/Oracle 第 2 轮复审 + Phase E 修复)**:
> - ✅ §0.1 业务定位已 lock (4 原则 + 16 行 Gap 矩阵 + 同步规则), **禁止 silent 偏离**
> - ✅ §0.2 总体实施计划已 lock (4 Phases / 6 Changes / 修复 11 Gap)
> - ✅ §1.4 仿真模式声明经用户评审重写: **VramControllerTLM 是 minimal_v1 唯一 functional-mode 模块**, 不是 SoC 整体 zero-delay
> - ✅ 当前进行中: **Phase 0 + Phase 1 全部 4 change (A/B/C/D) 起草完毕 + Oracle 复审修订完成, 4 changes openspec validate PASS**
> - ✅ KPI 现状: 5/3 (临时超限, 因 4 change 全 Proposed), 用户决策 archive A/B → 释放 slot
> - ⏳ 后续 Phase 2-3 Change E (MSI-X Generator, Gap L) + Change F (Error/CDC, Gap N+O) 起草需等 A/B/C/D archive 释放 slot

## ⚠️ v0.6 Lock Note (2026-10-13) — Chub change 推动 6 changes 重新洗牌

> 本节不是新内容，是对当前文档与未来 OpenSpec 变化关系的指针声明。

[cpptlm-dgpu-soc-control-hub (Proposed Draft 2026-10-13)](../../../openspec/changes/cpptlm-dgpu-soc-control-hub/) 提议"新建 ControlHub (Chub) + 移除 PcieMemoryDevice + SDMA 独立 translate + 完全 JSON 拓扑化", 范围**超集**于 `cpptlm-dgpu-soc-physical-ip-port-boundary` (Proposed Draft 2026-10-12, [commits `12cc50de`](../../../) + [`b3334d36`](../../../))。

**本 README 后续状态预期** (Chub change 实施后):
- 4 changes 命名 (A/B/C/D) 部分被 superset 关系合并: physical-ip-port-boundary + control-hub 一起 supersedes 当前 A/B/C/D 中的部分内容
- 6 业务模块 (Gmmu + Sdma + VramController + PcieController + PcieEndpoint + GpuInternalFabric) 改写为 7 业务模块 (+Chub, -PcieMemoryDevice, 净 +1)
- 顶层文档 (architecture.md / data-flow-and-topology-comparison.md / d3-evolution-roadmap.md / soc-internal-tlm-design-target.md) 全部 supersede 重写

**4 份文档已加 Lock Note 显式标记** (per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 修订流程):
- [architecture.md v0.6 Lock Note](./architecture.md#%E2%9A%A0%EF%B8%8F-v06-lock-note-2026-10-13--chub-change-supersedes-%E9%83%A8%E5%88%86%E6%8F%8F%E8%BF%B0)
- [data-flow-and-topology-comparison.md v0.5 Lock Note](./data-flow-and-topology-comparison.md#%E2%9A%A0%EF%B8%8F-v05-lock-note-2026-10-13--chub-change-supersedes-minimal_v1-%E6%8B%93%E6%89%91%E5%9B%BE)
- [d3-evolution-roadmap.md v0.6 Lock Note](./d3-evolution-roadmap.md#%E2%9A%A0%EF%B8%8F-v06-lock-note-2026-10-13--chub-change-%E5%BD%B1%E5%93%8D-d3b-%E8%B7%AF%E5%BE%84)
- [soc-internal-tlm-design-target.md v0.2 Lock Note](./soc-internal-tlm-design-target.md#%E2%9A%A0%EF%B8%8F-v02-lock-note-2026-10-13--chub-change-%E4%B8%8E-d-axi-v20-%E7%9B%AE%E6%A0%87%E4%B8%80%E8%87%B4)

**预期完整重写时点**: 任意 active change archive 后, KPI 释放 1 slot, 激活 Chub change (估时 21-30d), 实施完成后 4 份 Lock Note 段会被替换为完整 v0.6 重写。

> **业务架构视图分层 (per AGENTS.md §DOC HYGIENE 4 层文档模型)**:
> | 层 | 文档 | 作用 |
> |----|------|------|
> | 1 (架构视图) | `[./architecture.md](./architecture.md) — mixed-mode SoC per §1.4` | 业务架构 + 仿真模式 + Gap 修复路线 |
> | 2 (ADR) | [`../adr/ADR-DGPU-{05,06,07,08,09,10,11}-*.md`](../adr/) | 不可变决策 |
> | 3 (OpenSpec change) | [`../../../openspec/changes/`](../../../openspec/changes/) | 实施变更提案 (A-F 6 个 + Chub change 1 个) |
> | 4 (实施跟踪) | [`../../../docs/superpowers/plans/`](../../../docs/superpowers/plans/) | 短周期实施跟踪 |

> **配套文档**:
> - 主文档 (mixed-mode SoC, v0.5.1 修订): [./architecture.md](./architecture.md)
> - timing-mode (并列视图, v0.2 终稿): [./timing-mode.md](./timing-mode.md)
> - Driver 视角 (15 ABI + 4 BAR): [../dgpu-driver/architecture.md](../dgpu-driver/architecture.md)
> - Board 视角 (DGpuBoard v2.0.2): [../dgpu-board/architecture.md](../dgpu-board/architecture.md)

## 范围

本目录是 **DGpu SoC 业务架构** 文档, 聚焦 **(per §0.1 + Gap 矩阵)**:

- **SoC 拓扑** (`GmmuTLM` + `SdmaEngineTLM` + `VramControllerTLM` + `PcieControllerIP` + `PcieEndpointIP` + `GpuInternalFabric` per `dgpu-realip-pin-contract` 6-IP 边界;**双重计数说明**: §0.1 列 6 个含 GMMU/SDMA 引擎; §2.4 表格列 5 个 IP 边界模块不含 GMMU/SDMA)
- **仿真模式**: mixed-mode (VramControllerTLM 单独 functional-mode, 其他模块 cycle-approximate)
- **数据流**: SDMA → ChStream + SocInternalNoc mesh → VramController functional-mode → backing
- **关键不变式**: 单一 VRAM 真源 + chip-internal vs board-level 边界 + 双重无注入约束 + 23 ABI 零 diff
- **Gap 修复**: §0.1 矩阵 16 行 + §0.2 总体计划 (Phase 0-3 / Change A-F)

## 阅读路径

### functional-mode (v0.4)
1. **[./architecture.md §0.1 业务定位](./architecture.md)** (4 原则 + Gap 矩阵) — 必读
2. **[./architecture.md §1.4 仿真模式声明](./architecture.md)** (修订版, 7 子段) — 必读
3. **[./architecture.md §2 模块清单与拓扑](./architecture.md)** — IP 边界 + 接线

### timing-mode (并列)
- **[./timing-mode.md §0 关联与背景](./timing-mode.md)** + ADR-DGPU-11

### 实施跟踪
- **Phase 0 + Phase 1 已起草完毕 (v0.5)**: `cpptlm-vram-controller-functional-integration` (P0) + `cpptlm-lwc-bus-hub-mvp` (P1) + `cpptlm-gmmu-pte-bus-correction` (P2, Gap I+G2+G1) + `cpptlm-pcie-tlp-entry-layer` (P2, Gap M) — 前三者 ✅ **已完成归档**;Change D **⏸ 2026-10-20 暂存归档**(未实施 0/56)
- **Phase 0-1 实施状态 (2026-10-20 更新)**: A/B/C ✅ 已完成归档(2026-10-11~12);D/E + `cpptlm-p2-integration-unblock` **⏸ 暂存归档**(未实施,0/56 / 0/44 / 0/45)—— 四件套完整保留于 `openspec/changes/archive/2026-10-20-*/`,以后重建时拷回即可。**当前 OpenSpec active = 0,KPI 0/3**
- **Phase 2 起草需等 C archive**: `cpptlm-msix-generator-ip` (P2, Gap L) — C 已于 2026-10-12 归档;E 本身 **⏸ 2026-10-20 暂存归档**(未实施),重建时拷回 `openspec/changes/cpptlm-msix-generator-ip/` 即可激活
- **Phase 3 起草需等 D archive**: `cpptlm-soc-error-cdc-bus` (P3, Gap N+O) — D 已于 2026-10-20 暂存归档(**未实施**);D 未真正落地前,F 起草需先重建 D

## 维护规则

1. **修改前必读 §0.1 业务定位**: 任何修改文档/新增 OpenSpec change 必须先对照 §0.1 4 原则 + Gap 矩阵 16 行
2. **新增 OpenSpec change**: §0.2 同步规则要求显式引用本文档 + Gap 矩阵
3. **文档同步**: 业务架构代表 why, OpenSpec change design.md 代表 how; 反向同步不强制
4. **仿真模式定位锁 v0.4**: 后续 change 不再标 "minimal_v1 是 zero-delay functional-mode SoC", 改用 "mixed-mode SoC (VramControllerTLM 单独 functional-mode)"
5. **v0.6 Lock Note 同步规则**: 任何新提议 (含 Chub change) 涉及业务定位/拓扑/数据流/Gap 矩阵变化, 必须**先**在本文档加 Lock Note 段, **后**写 OpenSpec change; Lock Note 段会被后续完整修订替换 (per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 流程)
