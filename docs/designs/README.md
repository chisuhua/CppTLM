# docs/designs/ — dGPU 应用架构目录

> **职责**: dGPU 专用应用架构文档（Board 内部 + SoC 业务 + Driver 接口 三视角）
> **与 docs/architecture/ 的关系**: 分工明确（见下文）
> **维护记录**: 2026-09-26 重组完成（三视角拆分 + 子目录化）

---

## 目录职责分工

| 目录 | 内容 | 受众 | 维护频率 |
|------|------|------|----------|
| **`docs/architecture/`** | **通用基础架构**（事务、错误、复位、拓扑、metrics） | 跨模块开发者 | 较低（基础架构稳定） |
| **`docs/designs/`** | **dGPU 应用架构**（Board + SoC + Driver 三视角） | dGPU 模块开发者 + driver 编写者 + 评审者 | 较高（业务演进） |

**判断标准**：
- 文档涉及 dGPU 专用模块（DGpuBoard / PcieEndpointIP / GMMU / SDMA / PcieMemoryDevice / PcieConfigSpace）→ `docs/designs/`
- 文档涉及通用 TLM/框架/事务/错误/复位/拓扑 → `docs/architecture/`
- 不确定时优先放 `docs/architecture/`（基础架构优先稳定）

---

## 三视角子目录索引

| 子目录 | 视角 | 核心问题 | 入口文档 |
|--------|------|---------|---------|
| **[dgpu-board/](./dgpu-board/)** | Board 内部架构 | DGpuBoard 5 层 + 5 关键组件（LifecycleProtocol / CallbackWorker / DispatchRegistry / EpCache / PendingReqGuard）如何协作？ | [dgpu-board/architecture.md](./dgpu-board/architecture.md) |
| **[dgpu-soc/](./dgpu-soc/)** | SoC 业务架构 | 6 模块（DGpuBoard + PcieEndpointIP + PcieMemoryDevice + GMMU + SDMA + PcieConfigSpace）如何组合成最小 SoC？两种仿真模式并列：functional-mode (LT, zero-delay) + timing-mode (AT, cycle-approximate)？ | [dgpu-soc/architecture.md](./dgpu-soc/architecture.md) (functional-mode)<br/>[dgpu-soc/timing-mode.md](./dgpu-soc/timing-mode.md) (timing-mode 🆕) |
| **[dgpu-driver/](./dgpu-driver/)** | Driver 接口架构 | Linux driver 通过 15 ABI + 4 BAR 看到什么？怎么闭环（H2D / D2H / D2D）？ | [dgpu-driver/architecture.md](./dgpu-driver/architecture.md) |

---

## 4 层文档模型

本目录是第 1 层（长期维护架构文档）。完整文档分层：

```
┌─ 第 1 层 ─ 长期维护架构 ─ docs/architecture/ + docs/designs/        ← 本目录
├─ 第 2 层 ─ 架构决策 ───── docs/adr/ADR-DGPU-XX-*.md
├─ 第 3 层 ─ OpenSpec ───── openspec/changes/<name>/{proposal,design,spec,tasks}.md
└─ 第 4 层 ─ 实施跟踪 ───── docs/superpowers/plans/<name>.md           (归档至 archive/)
```

详细说明见 [AGENTS.md §DOC HYGIENE](../../AGENTS.md)。

---

## 与 ADR / OpenSpec / 实施跟踪 的关系

| 层级 | 职责 | 维护规则 | 变更门槛 |
|------|------|---------|---------|
| **架构（本文档）** | 长期设计意图 | 修改需评审 | Oracle 评审 or ADR 触发 |
| **ADR** | 不可变决策 | 签发后不改 | 仅追加 Status Update |
| **OpenSpec change** | 实施变更提案 | archive 后归 `specs/` | 标准 OpenSpec 工作流 |
| **实施跟踪** | TDD 5 步 + 进度 | 完成后归档 `archive/` | 无（实施期内部用） |

**同步规则（单向，OpenSpec → 架构）**：
- 修改 OpenSpec change 的 design.md 时，若涉及设计意图变化，应同步更新 `docs/designs/<topic>/architecture.md`
- 架构文档顶部加 `## 关联 OpenSpec changes` 段指向当前 change
- 反向同步不强制（架构文档可以领先于实施）

---

## 阅读路径（按角色）

### 5 分钟快速理解
1. [dgpu-driver/architecture.md §1](./dgpu-driver/architecture.md) — 15 ABI + 4 BAR 一图概览
2. [dgpu-soc/architecture.md §1-§2](./dgpu-soc/architecture.md) — 6 模块拓扑（functional-mode 默认）

### 1 小时深度理解（推荐起点）
1. [dgpu-driver/README.md](./dgpu-driver/README.md) → [architecture.md](./dgpu-driver/architecture.md)（driver 接口视角）
2. [dgpu-soc/README.md](./dgpu-soc/README.md) → [architecture.md](./dgpu-soc/architecture.md)（SoC functional-mode 业务视角）→ [timing-mode.md](./dgpu-soc/timing-mode.md)（SoC timing-mode 业务视角 🆕）
3. [dgpu-board/README.md](./dgpu-board/README.md) → [architecture.md](./dgpu-board/architecture.md)（Board 内部视角）

### 2 小时全链路理解
1. 上述 1 小时路径
2. 对应 [OpenSpec change](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) 的 design.md（functional-mode 实施指导）
3. 对应 [OpenSpec change](../../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/) 的 design.md（timing-mode 实施指导 🆕）
4. 对应 4 篇基础架构 ADR（ADR-DGPU-01 ~ 04）
5. [../pcie/driver-visible-minimal-soc.md](../../pcie/driver-visible-minimal-soc.md) — 实施笔记（图文并茂入口）

---

## 迁移历史

| 日期 | 事件 |
|------|------|
| 2026-09-26 | 从 `docs/architecture/14-dgpu-board-ideal-arch.md` 迁移 DGpuBoard 顶层架构 → `dgpu-board/architecture.md` |
| 2026-09-26 | 从 `docs/designs/2027-02-09-minimal-dgpu-soc.md` 迁移 Minimal SoC 业务设计 → `dgpu-soc/architecture.md` |
| 2026-09-26 | 新建 `dgpu-driver/architecture.md`（Driver 接口视角，原 driver-visible-minimal-soc.md 实施笔记的长期化） |
| 2026-09-26 | 新建本 README.md + 3 个子目录 README.md（三视角索引） |
| 2027-02-09 | 🆕 新增 timing-mode 提案 [dgpu-soc/timing-mode.md](./dgpu-soc/timing-mode.md)（与 functional-mode 并列共存）+ OpenSpec change `cpptlm-dgpu-soc-timing-mvp` |
| 2027-02-09 | ✅ timing-mode v0.2 终稿 (Oracle 八轮复评 PASS, 21/21 硬伤落盘) + [ADR-DGPU-11-timing-mode-soc-scope.md](../adr/ADR-DGPU-11-timing-mode-soc-scope.md) 签发 (T0 阶段) + 18 ADDED Requirements + 52 scenarios |

---

## 维护规则

1. **新视角添加**：需更新本 README.md 三视角索引
2. **跨视角修改**：涉及多个视角的变更应同步更新对应子目录，并在各自的 `## 关联文档` 段加链接
3. **路径稳定性**：`docs/designs/<topic>/architecture.md` 是固定入口；不要因临时变更改名
4. **路径漂移防护**：本目录变更需同步更新 `scripts/test/docs_sync_check.sh` VIRTUAL_PATHS 数组

---

## 相关资源

- [AGENTS.md §DOC HYGIENE](../../AGENTS.md) — 4 层文档模型详细说明
- [../architecture/README.md](../architecture/README.md) — 通用基础架构目录入口
- [../../openspec/changes/cpptlm-driver-visible-minimal-soc/](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) — 当前 active 的 OpenSpec change
- [../../pcie/driver-visible-minimal-soc.md](../../pcie/driver-visible-minimal-soc.md) — D-AXI 实施笔记（快速入口）
