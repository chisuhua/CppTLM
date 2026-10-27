# 架构设计文档 (通用基础架构)

> **版本**: 1.2
> **最后更新**: 2026-10-27 (v1.4 — 14-sm-operand-supply-design-target.md 迁出至 ArchForge（编号 33-）, 18 文档 + 5 归档)
> **变更**: 与 `docs/designs/` 分工明确（dGPU 应用架构已迁出）+ 真源 1 (通用基础架构) 索引同步

> **⚠️ v0.4 X-11 cross-ref**: 本目录是 AGENTS.md §DOC HYGIENE 真源原则定义的"真源 1" (通用基础架构唯一真源). 文档状态与正文严格同步是治理成功的必要条件 (per critic 评审 B1+B2 BLOCKER). 任何修订必须先改这里 + 同步索引 (本 README) + 同步 ADR (docs/adr/) + 同步调研 (docs/research/).

---

## 目录职责分工

| 目录 | 内容 | 受众 |
|------|------|------|
| **`docs/architecture/`**（本目录） | **真源 1 通用基础架构** (事务、错误、复位、拓扑、metrics、可视化) | 跨模块开发者 |
| **[`../designs/`](../designs/)** | **真源 2 dGPU 应用架构** (Board 内部 + SoC 业务 + Driver 接口 三视角) | dGPU 模块开发者 + driver 编写者 + 评审者 |

**判断标准**：
- 涉及 dGPU 专用模块（DGpuBoard / PcieEndpointIP / GMMU / SDMA / PcieMemoryDevice / PcieConfigSpace）→ `docs/designs/` (真源 2)
- 涉及通用 TLM/框架/事务/错误/复位/拓扑/可视化 → `docs/architecture/` (真源 1)
- 不确定时优先放 `docs/architecture/`（基础架构优先稳定）

---

## 架构文档列表

### 核心架构（通用基础架构）

| 文档 | 状态 | 说明 |
|------|------|------|
| [01-hybrid-architecture-v2.1.md](./01-hybrid-architecture-v2.1.md) | ✅ v2.1.10 已批准 | 混合仿真架构 v2.1 (L1-L6 6 层框架真源, per [06 §2.1](./06-inter-module-network.md) 严格对齐) |
| [02-transaction-architecture.md](./02-transaction-architecture.md) | ✅ v2.1 已批准 | 交易处理架构 |
| [03-error-debug-architecture.md](./03-error-debug-architecture.md) | ✅ v1.0 已实施 | 错误与调试架构 (ErrorCategory + ErrorContextExt + DebugTracker, 2026-10-19 状态对齐) |
| [04-reset-checkpoint-architecture.md](./04-reset-checkpoint-architecture.md) | ✅ v1.0 已实施 | 复位与快照架构 (ResetConfig + save_snapshot, 2026-10-19 状态对齐) |
| [06-inter-module-network.md](./06-inter-module-network.md) | 🟡 **v0.4 修订中** (2026-10-19) | 模块互联网络协议中立基线 + Bundle 二分类 + Python 拓扑契约 (详化 01 §1.1 6 层 + §1.2 Bundle 命名 + §4.7 simulation_mode, 双向 cross-ref) |
| [06-bis-coherence-as-network-extension.md](./06-bis-coherence-as-network-extension.md) | 🟡 **v0.3 修订中** (2026-10-19) | Coherence 演进 seam 详化 (与 06 §15 + ArchForge coherence-protocol.md 跨仓引用, 不重发明) |
| [07-coherence-exploration-platform.md](./07-coherence-exploration-platform.md) | 🟡 **v0.1 起草中** (2026-10-20) | Coherence Exploration Platform (CEP) 一致性互联仿真平台 — 6 层架构 + canonical 10 流 + 66 条守门 invariant(编号位 67 / 15 组)+ 2 来源族 / 16 adapter 实例(+ 5 一致性模型语义索引)适配 + 实验输出协议 (per `ADR-CEP-00` 10 份 ADR v0.6 整合, **与 01 §1.1 L1-L6 正交**) |
| [08-metrics-collection-framework-v2.2.md](./08-metrics-collection-framework-v2.2.md) | ✅ v2.2.0 已实施 | 性能指标收集框架 (histogram + stats + streaming_reporter, 2026-10-19 状态对齐) |
| [09-topology-visualization-pipeline.md](./09-topology-visualization-pipeline.md) | ✅ v2.0 已批准 | 配置生成 + 可视化架构 (Python `cpptlm/visualization/` + dashboard, 2026-10-19 状态对齐) |
| [10-topology-generation-management-system.md](./10-topology-generation-management-system.md) | ✅ v3.0 已批准 | 拓扑生成管理系统 (mesh_cluster / ring_cluster + JSON Schema) |
| [11-coherence-domain-readiness.md](./11-coherence-domain-readiness.md) | 🟡 早期 readiness 评估 | 一致性域准备状态 (ARCH-011, 与 06-bis v0.3 实施蓝图范围正交, 2026-10-19 加 cross-ref) |
| [12-topology-gap-analysis.md](./12-topology-gap-analysis.md) | ✅ v1.1 已批准 | 拓扑差距分析 (审查修正版) |
| [13-dashboard-integration.md](./13-dashboard-integration.md) | ✅ v2.0 已批准 | Dashboard 集成 (FastAPI + RunContext + Monaco Editor) |
| ~~14-sm-operand-supply-design-target.md~~ | ➡️ **已迁 ArchForge** | **SM 操作数供给下一代架构设计目标（OSF：MCOS / ERW / WABR / SOP）** — 2026-10-27 迁至【ArchForge 仓】`docs/architecture/33-sm-operand-supply-design-target.md`（🟡 设计目标声明，非现状真相，**未经 Oracle/ADR 评审**）；配套证据【ArchForge 仓】`docs/research/SM/CudaCore/`（SYNTHESIS + 8 篇解析）。**本仓不再保留副本**（silicon 侧真源在 ArchForge；真源 1 只描述 CppTLM 现状） |
| [02-complex-topology-architecture.md](./02-complex-topology-architecture.md) | ✅ 已批准 | 复杂拓扑实现架构 (RT 桥接, per [06 §0.3](./06-inter-module-network.md) 边界标注) |

### 决策汇总

| 文档 | 说明 |
|------|------|
| [P0_P1_P2_DECISIONS.md](./P0_P1_P2_DECISIONS.md) | P0/P1/P2 核心决策汇总 (✅ 已全部确认) |
| [FRAGMENT_MAPPER_DECISIONS.md](./FRAGMENT_MAPPER_DECISIONS.md) | FragmentMapper 决议 (✅ 已确认) |
| [topology-editor-framework-decision.md](./topology-editor-framework-decision.md) | 拓扑编辑器前端框架决策 (✅ 已决策, Svelte 推荐, TD-004) |
| [rdd-workflow-integration-gap-gap-analysis.md](./rdd-workflow-integration-gap-gap-analysis.md) | 🟡 草案 (rdd-workflow 集成差距, 关联 ADR 待起草, 2026-10-19 加状态头) |

### 示例代码

| 目录 | 内容 |
|------|------|
| [examples/bundles/](./examples/bundles/) | Bundle 定义示例 |
| [examples/tlm/](./examples/tlm/) | TLM 模块示例 |
| [examples/rtl/](./examples/rtl/) | RTL 模块示例 |
| [examples/framework/](./examples/framework/) | 框架示例 |

---

## 已迁移文档（2026-09-26）

| 原路径 | 新路径 | 原因 |
|--------|--------|------|
| ~~`14-dgpu-board-ideal-arch.md`~~ | [`../designs/dgpu-board/architecture.md`](../designs/dgpu-board/architecture.md) | DGpuBoard 是 dGPU 专用模块，迁至 `docs/designs/dgpu-board/` |

## 已归档文档（2026-10-19 docs/architecture/ 重新规划 + 真源原则治理）

> **注**: 删除线内的路径仅作历史记录, **不构成 markdown 链接** (per `docs/architecture/README.md` v1.2 治理). 实际归档路径以"归档路径"列为准.

| 原文档名 | 归档路径 | 原因 |
|--------|---------|------|
| ~~`01-hybrid-architecture-v2.md`~~ (v2.0) | [../archive/architecture/01-hybrid-architecture-v2-archived-2026-10.md](../archive/architecture/01-hybrid-architecture-v2-archived-2026-10.md) | v2.0 已被 v2.1.10 完全替代 (新增 §1.1 通信框架层 + §1.2 Bundle 命名 + §4.7 ImplMode), 真源原则 v0.4 不允许"已 supersede"留在真源目录 |
| ~~`05-packet-semantic-network.md`~~ | [../archive/architecture/05-packet-semantic-network-archived-2026-10.md](../archive/architecture/05-packet-semantic-network-archived-2026-10.md) | Packet 语义早期方案, 立场与 01 §1.1 + [06 §2.5](./06-inter-module-network.md) 对立; 调研价值 (Packet 字段分类 + Gem5 MessageBuffer 锚点) 被 06 §2.5 + 01 §1.1 继承 |
| ~~`10-tgms-specifications.md`~~ | [../archive/architecture/10-tgms-specifications-archived-2026-10.md](../archive/architecture/10-tgms-specifications-archived-2026-10.md) | 与 [10-topology-generation-management-system.md](./10-topology-generation-management-system.md) v3.0 范围重复, 后者是统一真源; ARCH-010 §8.2 已修正引用至归档路径 |
| ~~`2026-05-12-unified-visualization-platform.md`~~ | [../archive/architecture/unified-visualization-platform-2026-05-12-archived-2026-10.md](../archive/architecture/unified-visualization-platform-2026-05-12-archived-2026-10.md) | 发布次日被 [13-dashboard-integration.md](./13-dashboard-integration.md) v2.0 已批准覆盖 |
| ~~`fastapi-migration-plan.md`~~ | [../archive/architecture/fastapi-migration-plan-archived-2026-10.md](../archive/architecture/fastapi-migration-plan-archived-2026-10.md) | FastAPI 迁移已实施完成, 当前 Python 工具链已迁至 `cpptlm/visualization/` (FastAPI) + `cpptlm/config/` (Pydantic) |

## 已迁出文档（2026-10-19 按 docs/architecture/README.md 分工迁出至 docs/designs/）

> **注**: 删除线内的文件名仅作历史记录, **不构成 markdown 链接**. 实际迁出路径以"迁出路径"列为准.

| 原文档名 | 迁出路径 | 原因 |
|--------|---------|------|
| ~~`14-pcie-tlp-wire-format.md`~~ | [../designs/dgpu-soc/14-pcie-tlp-wire-format.md](../designs/dgpu-soc/14-pcie-tlp-wire-format.md) | 主题是 PCIe Controller 板级接口 (dGPU 应用架构), 不在通用基础架构 (per README.md 分工) |
| ~~`多层次混合仿真.md`~~ | [../designs/dgpu-soc/multi-level-hybrid-simulation.md](../designs/dgpu-soc/multi-level-hybrid-simulation.md) | 主题是 GPGPU 多层 SimModule 容器 (dGPU 应用架构), 不在通用基础架构 |

---

## 推荐阅读顺序

1. 01-hybrid-architecture-v2.1 - 整体架构 (真源 1 基线)
2. 06-inter-module-network - 模块互联网络 (协议中立基线, 2026 修订主线)
3. 06-bis-coherence-as-network-extension - Coherence 演进 seam (06 §15 详化)
4. **07-coherence-exploration-platform - CEP 一致性互联仿真平台 (06-bis successor, 2026-10-20 新增)**
5. 02-transaction-architecture / 03-error-debug / 04-reset-checkpoint - 基础子架构

---

## 相关资源

- **dGPU 应用架构入口**: [`../designs/README.md`](../designs/README.md)
- **AGENTS.md §DOC HYGIENE 真源原则**: [4 层文档模型 + 真源 1/2/3](../../AGENTS.md#真源原则-single-source-of-truth-2026-10-19-确立)
- **ADR 索引**: [`../adr/README.md`](../adr/README.md)
- **调研附属**: [`../research/AGENTS.md`](../research/AGENTS.md)

---

**维护**: CppTLM 开发团队
