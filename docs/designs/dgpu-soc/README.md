# DGpu SoC 业务架构 (Minimal SoC Business Architecture)

> **状态 v0.5.1 (2026-10-09 修订, per Metis/Oracle 第 2 轮复审 + Phase E 修复)**:
> - ✅ §0.1 业务定位已 lock (4 原则 + 16 行 Gap 矩阵 + 同步规则), **禁止 silent 偏离**
> - ✅ §0.2 总体实施计划已 lock (4 Phases / 6 Changes / 修复 11 Gap)
> - ✅ §1.4 仿真模式声明经用户评审重写: **VramControllerTLM 是 minimal_v1 唯一 functional-mode 模块**, 不是 SoC 整体 zero-delay
> - ✅ 当前进行中: **Phase 0 + Phase 1 全部 4 change (A/B/C/D) 起草完毕 + Oracle 复审修订完成, 4 changes openspec validate PASS**
> - ✅ KPI 现状: 5/3 (临时超限, 因 4 change 全 Proposed), 用户决策 archive A/B → 释放 slot
> - ⏳ 后续 Phase 2-3 Change E (MSI-X Generator, Gap L) + Change F (Error/CDC, Gap N+O) 起草需等 A/B/C/D archive 释放 slot

> **业务架构视图分层 (per AGENTS.md §DOC HYGIENE 4 层文档模型)**:
> | 层 | 文档 | 作用 |
> |----|------|------|
> | 1 (架构视图) | `[./architecture.md](./architecture.md) — mixed-mode SoC per §1.4` | 业务架构 + 仿真模式 + Gap 修复路线 |
> | 2 (ADR) | [`../adr/ADR-DGPU-{05,06,07,08,09,10,11}-*.md`](../adr/) | 不可变决策 |
> | 3 (OpenSpec change) | [`../../../openspec/changes/`](../../../openspec/changes/) | 实施变更提案 (A-F 6 个, 其中 4 个已 Proposed Draft) |
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
- **Phase 0 + Phase 1 已起草完毕 (v0.5)**: `cpptlm-vram-controller-functional-integration` (P0) + `cpptlm-lwc-bus-hub-mvp` (P1) + `cpptlm-gmmu-pte-bus-correction` (P2, Gap I+G2+G1) + `cpptlm-pcie-tlp-entry-layer` (P2, Gap M)
- **Phase 0-1 实施状态**: 待用户决策 archive A → activate C/D; 当前 4 change 已 validate PASS, 4 件套正文已修订 (per Oracle/Metis v0.5 复审)
- **Phase 2 起草需等 C archive**: `cpptlm-msix-generator-ip` (P2, Gap L) — 用户决策 archive C 后激活
- **Phase 3 起草需等 D archive**: `cpptlm-soc-error-cdc-bus` (P3, Gap N+O) — 用户决策 archive D 后激活

## 维护规则

1. **修改前必读 §0.1 业务定位**: 任何修改文档/新增 OpenSpec change 必须先对照 §0.1 4 原则 + Gap 矩阵 16 行
2. **新增 OpenSpec change**: §0.2 同步规则要求显式引用本文档 + Gap 矩阵
3. **文档同步**: 业务架构代表 why, OpenSpec change design.md 代表 how; 反向同步不强制
4. **仿真模式定位锁 v0.4**: 后续 change 不再标 "minimal_v1 是 zero-delay functional-mode SoC", 改用 "mixed-mode SoC (VramControllerTLM 单独 functional-mode)"
