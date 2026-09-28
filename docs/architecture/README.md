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
| [01-hybrid-architecture-v2.1.md](./01-hybrid-architecture-v2.1.md) | ✅ 已批准 | 混合仿真架构 v2.1 |
| [02-transaction-architecture.md](./02-transaction-architecture.md) | ✅ 已批准 | 交易处理架构 |
| [03-error-debug-architecture.md](./03-error-debug-architecture.md) | ✅ 已批准 | 错误与调试架构 |
| [04-reset-checkpoint-architecture.md](./04-reset-checkpoint-architecture.md) | 📋 待确认 | 复位与快照架构 |
| [2026-05-12-unified-visualization-platform.md](./2026-05-12-unified-visualization-platform.md) | ✅ 已批准 | 统一可视化平台 |

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
