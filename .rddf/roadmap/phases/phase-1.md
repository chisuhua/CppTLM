---
id: phase-1
kind: phase
status: completed
phase_refs: []
主题: 通用基础架构 (事务/错误/复位/拓扑/metrics) v1.2 治理完成
---

## phase-1: 通用基础架构 v1.2 治理完成

> **真源**: [`docs/architecture/`](../../docs/architecture/) (真源 1, 18 文档 + 5 归档, 2026-10-19 v1.2 治理完成 per critic 评审 6 BLOCKER 修复)
> **关键 ADR**: [`docs/architecture/P0_P1_P2_DECISIONS.md`](../../docs/architecture/P0_P1_P2_DECISIONS.md) ✅ 已全部确认 2026-04-07

### 18 文档清单 (per 真源 1 v1.2 索引, 2026-10-19)

| 文档 | 状态 | 说明 |
|------|------|------|
| [01-hybrid-architecture-v2.1.md](../../docs/architecture/01-hybrid-architecture-v2.1.md) | ✅ v2.1.10 已批准 | 混合仿真架构 v2.1 (L1-L6 6 层框架真源) |
| [02-transaction-architecture.md](../../docs/architecture/02-transaction-architecture.md) | ✅ v2.1 已批准 | 交易处理架构 |
| [02-complex-topology-architecture.md](../../docs/architecture/02-complex-topology-architecture.md) | ✅ 已批准 | 复杂拓扑实现架构 (RT 桥接) |
| [03-error-debug-architecture.md](../../docs/architecture/03-error-debug-architecture.md) | ✅ v1.0 已实施 | 错误与调试架构 |
| [04-reset-checkpoint-architecture.md](../../docs/architecture/04-reset-checkpoint-architecture.md) | ✅ v1.0 已实施 | 复位与快照架构 |
| [06-inter-module-network.md](../../docs/architecture/06-inter-module-network.md) | 🟡 v0.4 修订中 | 模块互联网络协议中立基线 |
| [06-bis-coherence-as-network-extension.md](../../docs/architecture/06-bis-coherence-as-network-extension.md) | 🟡 v0.3 修订中 | Coherence 演进 seam 详化 |
| [08-metrics-collection-framework-v2.2.md](../../docs/architecture/08-metrics-collection-framework-v2.2.md) | ✅ v2.2.0 已实施 | 性能指标收集框架 |
| [09-topology-visualization-pipeline.md](../../docs/architecture/09-topology-visualization-pipeline.md) | 🟡 草案 | 配置生成 + 可视化架构 |
| [10-topology-generation-management-system.md](../../docs/architecture/10-topology-generation-management-system.md) | ✅ v3.0 已批准 | 拓扑生成管理系统 |
| [11-coherence-domain-readiness.md](../../docs/architecture/11-coherence-domain-readiness.md) | 🟡 早期 readiness 评估 | 一致性域准备状态 |
| [12-topology-gap-analysis.md](../../docs/architecture/12-topology-gap-analysis.md) | ✅ v1.1 已批准 | 拓扑差距分析 |
| [13-dashboard-integration.md](../../docs/architecture/13-dashboard-integration.md) | ✅ v2.0 已批准 | Dashboard 集成 |
| [P0_P1_P2_DECISIONS.md](../../docs/architecture/P0_P1_P2_DECISIONS.md) | ✅ 已全部确认 | P0/P1/P2 决策汇总 |
| [FRAGMENT_MAPPER_DECISIONS.md](../../docs/architecture/FRAGMENT_MAPPER_DECISIONS.md) | ✅ 已确认 | FragmentMapper 决议 |
| [topology-editor-framework-decision.md](../../docs/architecture/topology-editor-framework-decision.md) | ✅ 已决策 | 拓扑编辑器前端框架决策 |
| [rdd-workflow-integration-gap-gap-analysis.md](../../docs/architecture/rdd-workflow-integration-gap-gap-analysis.md) | 🟡 草案 | rdd-workflow 集成差距分析 |
| [README.md](../../docs/architecture/README.md) | ✅ v1.2 治理完成 | 通用基础架构索引 (18 文档 + 5 归档) |

### 5 归档清单 (per 2026-10-19 docs/architecture/ 重新规划)

| 原文档 | 归档路径 | 原因 |
|--------|---------|------|
| `01-hybrid-architecture-v2.md` (v2.0) | [archive](../../docs/archive/architecture/01-hybrid-architecture-v2-archived-2026-10.md) | v2.0 已被 v2.1.10 完全替代 |
| `05-packet-semantic-network.md` | [archive](../../docs/archive/architecture/05-packet-semantic-network-archived-2026-10.md) | Packet 语义早期方案 |
| `10-tgms-specifications.md` | [archive](../../docs/archive/architecture/10-tgms-specifications-archived-2026-10.md) | 范围重复 |
| `2026-05-12-unified-visualization-platform.md` | [archive](../../docs/archive/architecture/unified-visualization-platform-2026-05-12-archived-2026-10.md) | 被 13-dashboard-integration v2.0 覆盖 |
| `fastapi-migration-plan.md` | [archive](../../docs/archive/architecture/fastapi-migration-plan-archived-2026-10.md) | FastAPI 迁移已实施完成 |

### 2 迁出清单 (per 2026-10-19 真源原则)

| 原文档 | 迁出路径 | 原因 |
|--------|---------|------|
| `14-pcie-tlp-wire-format.md` | [docs/designs/dgpu-soc/](../../docs/designs/dgpu-soc/14-pcie-tlp-wire-format.md) | PCIe Controller 板级接口 (dGPU 应用架构) |
| `多层次混合仿真.md` | [docs/designs/dgpu-soc/](../../docs/designs/dgpu-soc/multi-level-hybrid-simulation.md) | GPGPU 多层 SimModule 容器 (dGPU 应用架构) |

### 关键修复 (2026-10-19 critic 评审 commit `95ba88af`)

- **B1**: 03/04 文档头尾状态一致 (顶部 ✅ / 尾部 📋 自相矛盾修复)
- **B2**: README.md 索引同步 (18 文档 + 5 归档 + 2 迁出 全部登记)
- **B3**: 06 v0.4 状态头补全 (标题/顶部/页脚 v0.3 → v0.4)
- **B4**: 09-topology-visualization 状态头回退 ✅ → 🟡 草案 (关联 ADR-X.12 未签发)
- **B5**: 01-hybrid-v2 归档后断链修复 (6 处引用)
- **B6**: 新增 `scripts/test/markdown_link_check.sh` (~150 行, 扫 `.md 链接`, 互补 docs_sync_check.sh)