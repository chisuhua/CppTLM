# 架构设计文档 (通用基础架构)

> **版本**: 1.1
> **最后更新**: 2026-09-26
> **变更**: 与 `docs/designs/` 分工明确（dGPU 应用架构已迁出）

---

## 目录职责分工

| 目录 | 内容 | 受众 |
|------|------|------|
| **`docs/architecture/`**（本目录） | **通用基础架构**（事务、错误、复位、拓扑、metrics、可视化） | 跨模块开发者 |
| **[`../designs/`](../designs/)** | **dGPU 应用架构**（Board 内部 + SoC 业务 + Driver 接口 三视角） | dGPU 模块开发者 + driver 编写者 + 评审者 |

**判断标准**：
- 涉及 dGPU 专用模块（DGpuBoard / PcieEndpointIP / GMMU / SDMA / PcieMemoryDevice / PcieConfigSpace）→ `docs/designs/`
- 涉及通用 TLM/框架/事务/错误/复位/拓扑/可视化 → `docs/architecture/`（本目录）
- 不确定时优先放 `docs/architecture/`（基础架构优先稳定）

---

## 架构文档列表

### 核心架构（通用基础架构）

| 文档 | 状态 | 说明 |
|------|------|------|
| [01-hybrid-architecture-v2.1.md](./01-hybrid-architecture-v2.1.md) | ✅ v2.1.10 已批准 | 混合仿真架构 v2.1 (L1-L6 6 层框架真源, per [06 §2.1](./06-inter-module-network.md) 严格对齐) |
| [02-transaction-architecture.md](./02-transaction-architecture.md) | ✅ 已批准 | 交易处理架构 |
| [03-error-debug-architecture.md](./03-error-debug-architecture.md) | ✅ 已批准 | 错误与调试架构 |
| [04-reset-checkpoint-architecture.md](./04-reset-checkpoint-architecture.md) | 📋 待确认 | 复位与快照架构 |
| [06-inter-module-network.md](./06-inter-module-network.md) | 🟡 **v0.4 修订中** (2026-10-18) | 模块互联网络协议中立基线 + Bundle 二分类 + Python 拓扑契约 (详化 01 §1.1 6 层 + §1.2 Bundle 命名 + §4.7 simulation_mode, 双向 cross-ref) |
| [06-bis-coherence-as-network-extension.md](./06-bis-coherence-as-network-extension.md) | 🟡 **v0.3 修订中** (2026-10-18) | Coherence 演进 seam 详化 (与 06 §15 + ArchForge coherence-protocol.md 跨仓引用, 不重发明) |

### 决策汇总

| 文档 | 说明 |
|------|------|
| [P0_P1_P2_DECISIONS.md](./P0_P1_P2_DECISIONS.md) | P0/P1/P2 核心决策汇总 |
| [FRAGMENT_MAPPER_DECISIONS.md](./FRAGMENT_MAPPER_DECISIONS.md) | FragmentMapper 决议 |

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

## 已归档文档（2026-10-18 docs/architecture/ 重新规划）

| 原路径 | 归档路径 | 原因 |
|--------|---------|------|
| ~~[./05-packet-semantic-network.md](./05-packet-semantic-network.md)~~ | [../archive/architecture/05-packet-semantic-network-archived-2026-10.md](../archive/architecture/05-packet-semantic-network-archived-2026-10.md) | Packet 语义早期方案, 立场与 01 §1.1 + [06 §2.5](./06-inter-module-network.md) 对立; 调研价值 (Packet 字段分类 + Gem5 MessageBuffer 锚点) 被 06 §2.5 + 01 §1.1 继承 |
| ~~[./10-tgms-specifications.md](./10-tgms-specifications.md)~~ | [../archive/architecture/10-tgms-specifications-archived-2026-10.md](../archive/architecture/10-tgms-specifications-archived-2026-10.md) | 与 [10-topology-generation-management-system.md](./10-topology-generation-management-system.md) v3.0 范围重复, 后者是统一真源; ARCH-010 §8.2 已修正引用至归档路径 |
| ~~[./2026-05-12-unified-visualization-platform.md](./2026-05-12-unified-visualization-platform.md)~~ | [../archive/architecture/unified-visualization-platform-2026-05-12-archived-2026-10.md](../archive/architecture/unified-visualization-platform-2026-05-12-archived-2026-10.md) | 发布次日被 [13-dashboard-integration.md](./13-dashboard-integration.md) v2.0 已批准覆盖 |
| ~~[./fastapi-migration-plan.md](./fastapi-migration-plan.md)~~ | [../archive/architecture/fastapi-migration-plan-archived-2026-10.md](../archive/architecture/fastapi-migration-plan-archived-2026-10.md) | FastAPI 迁移已实施完成, 当前 Python 工具链已迁至 `cpptlm/visualization/` (FastAPI) + `cpptlm/config/` (Pydantic) |

## 已迁出文档（2026-10-18 按 docs/architecture/README.md 分工迁出至 docs/designs/）

| 原路径 | 迁出路径 | 原因 |
|--------|---------|------|
| ~~[./14-pcie-tlp-wire-format.md](./14-pcie-tlp-wire-format.md)~~ | [../designs/dgpu-soc/14-pcie-tlp-wire-format.md](../designs/dgpu-soc/14-pcie-tlp-wire-format.md) | 主题是 PCIe Controller 板级接口 (dGPU 应用架构), 不在通用基础架构 (per README.md 分工) |
| ~~[./多层次混合仿真.md](./多层次混合仿真.md)~~ | [../designs/dgpu-soc/multi-level-hybrid-simulation.md](../designs/dgpu-soc/multi-level-hybrid-simulation.md) | 主题是 GPGPU 多层 SimModule 容器 (dGPU 应用架构), 不在通用基础架构 |

---

## 推荐阅读顺序

1. 01-hybrid-architecture-v2.1 - 整体架构
2. P0_P1_P2_DECISIONS - 核心决策
3. 02-transaction-architecture - 交易处理
4. 03-error-debug-architecture - 错误处理

---

## 相关资源

- **dGPU 应用架构入口**: [`../designs/README.md`](../designs/README.md)
- **AGENTS.md §DOC HYGIENE**: [4 层文档模型](../../AGENTS.md)
- **ADR 索引**: [`../adr/README.md`](../adr/README.md)

---

**维护**: CppTLM 开发团队
