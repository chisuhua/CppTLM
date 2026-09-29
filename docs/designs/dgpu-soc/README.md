# DGpu SoC 业务架构 (Minimal SoC Business Architecture)

> **状态**:
> - ✅ functional-mode (LT) v1.0 设计已通过 Oracle 二轮复审（[./architecture.md](./architecture.md)）
> - ✅ timing-mode (AT) v0.2 终稿 + **T1-T8 实施完成 (2027-02-11, 37 cases + 0 regressions)**（[./timing-mode.md](./timing-mode.md)，Oracle 八轮复评 PASS, [ADR-DGPU-11-timing-mode-soc-scope.md](../adr/ADR-DGPU-11-timing-mode-soc-scope.md) 已签发 + Status Update, 与 functional-mode **并列共存**）
> **对应架构文档**:
> - 主文档（functional-mode, LT, zero-delay）: [architecture.md](./architecture.md)（原 `docs/designs/2027-02-09-minimal-dgpu-soc.md`，2026-09-26 迁移）
> - 并列文档（timing-mode, AT, cycle-approximate）: [timing-mode.md](./timing-mode.md)（v0.2 终稿, 2027-02-09, Oracle 八轮复评 PASS, ADR-DGPU-11 签发）
> **配套 OpenSpec**:
> - functional-mode: [cpptlm-driver-visible-minimal-soc](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前)
> - timing-mode: [cpptlm-dgpu-soc-timing-mvp](../../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/) (✅ v0.2 终稿, Oracle 八轮 PASS, ADR-DGPU-11 签发)
> **基础架构**: [../dgpu-board/architecture.md](../dgpu-board/architecture.md) | **Driver 视角**: [../dgpu-driver/architecture.md](../dgpu-driver/architecture.md)

## 范围

本目录是 **DGpu SoC 业务架构** 文档，聚焦：

- 6 模块拓扑（DGpuBoard + PcieEndpointIP + PcieMemoryDevice + GMMU + SDMA + PcieConfigSpace）
- 存储子系统（镜像 gem5 `PhysicalMemory` + `AbstractMemory` 模式）
- BAR 路由与读写路径（BAR0 MMIO + BAR1 16MB 存储窗口 + BAR2 8GB vram aperture）
- GMMU 一级页表设计
- SDMA / Fence / MSI-X 链路
- 关键不变性（6 条 Invariants,functional-mode）
- ✅ timing-mode 并列视图（8 注册类型 / 7 实例化, VramControllerTLM 或 MemoryTLM 二选一, cycle accounting, per ADR-DGPU-11 H2 互斥）

## 仿真模式对照

| 模式 | 文档 | 仿真模式 | 用途 | 测试目标 |
|------|------|----------|------|----------|
| **functional-mode (LT)** | [architecture.md](./architecture.md) | 零时延 memcpy | driver 正确性验证 | `[minimal_dgpu_soc]` 41 assertions |
| **timing-mode (AT)** 🆕 | [timing-mode.md](./timing-mode.md) | 周期近似 | 性能回归 + 带宽分析 | `[dgpu_soc_timing]` 新增套件 |

**两模式并列共存**:
- 通过 `simulation_mode: "functional" | "timing"` JSON 字段切换
- 同一 DGpuBoard 框架,不同 DGpuSoc 实例化分支
- 23 ABI 入口 + vram_storage_ 共享 owner (per ADR-DGPU-05)
- **不替换**: functional-mode minimal_v1 v1.0 设计与 6 条 Invariants **完全保留**

## 与其他视角的关系

| 视角 | 文档 | 关注点 |
|------|------|--------|
| Board 内部 | [../dgpu-board/architecture.md](../dgpu-board/architecture.md) | DGpuBoard 5 层 + 5 组件 + 析构协议 |
| **SoC 业务 functional** | [architecture.md](./architecture.md) | 6 模块拓扑 + GMMU + SDMA + 存储子系统 (zero-delay) |
| **SoC 业务 timing** 🆕 | [timing-mode.md](./timing-mode.md) | 8 模块拓扑 + CrossbarTLM + VramControllerTLM + cycle accounting |
| Driver 接口 | [../dgpu-driver/architecture.md](../dgpu-driver/architecture.md) | 15 ABI + 4 BAR + driver 闭环数据流 |

## 阅读路径

### functional-mode (默认,driver 闭环)
1. **快速理解**: architecture.md §1（设计目标）+ §2（模块清单与拓扑）
2. **存储子系统**: architecture.md §3（镜像 gem5 模式 + DGpuBoard 新增）
3. **BAR 路由**: architecture.md §4（BAR 布局 + 路由表 + 读写路径）
4. **GMMU 设计**: architecture.md §5（API 骨架 + 翻译流程 + PTE 格式）
5. **关键不变性**: architecture.md §8（6 条 Invariants）
6. **演进路径**: 设计意图由 OpenSpec change [§X 演进路线图](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/design.md) 维护

### timing-mode (性能建模)
1. **动机与边界**: timing-mode.md §0（为什么需要独立 timing-mode SoC）+ §1（设计目标与边界）
2. **模块清单**: timing-mode.md §2（8 模块拓扑 + 2 新建 + 3 扩展）
3. **时序子系统**: timing-mode.md §3（cycle accounting 模型 + 5 模块时序表）
4. **BAR 路径**: timing-mode.md §4（与 minimal_v1 的关键差异表）
5. **GMMU 异步**: timing-mode.md §5（TLB 模型 + page walk 延迟）
6. **SDMA outstanding**: timing-mode.md §6（AXI master port + rid 关联）
7. **关键不变性**: timing-mode.md §8（6 条 Invariants,与 functional-mode 兼容）
8. **演进路径**: timing-mode.md §14（D3/D4 backlog:多通道 HBM + 多级页表）

## 维护记录

| 日期 | 事件 |
|------|------|
| 2026-09-26 | 从 `docs/designs/2027-02-09-minimal-dgpu-soc.md` 迁移 functional-mode 至 [architecture.md](./architecture.md) |
| 2027-02-09 | functional-mode v1.0 设计 + Oracle 二轮复审 |
| 2027-02-09 | 🆕 新增 timing-mode 提案 [timing-mode.md](./timing-mode.md) + OpenSpec [cpptlm-dgpu-soc-timing-mvp](../../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/) |

## 同步规则

本目录文档代表 SoC **业务层** 长期设计意图。具体修改应通过 OpenSpec change 提案；如涉及基础架构变更（新增模块、变更拓扑），需同步更新 dgpu-board 与 dgpu-driver 视角。

**两模式文档独立性**:
- 修改 functional-mode `architecture.md` **不影响** timing-mode `timing-mode.md`
- 修改 timing-mode `timing-mode.md` **不影响** functional-mode `architecture.md` 与 6 条 Invariants
- 共用基线更新（AGENTS.md / ADR 索引）应同步两文档顶部 `## 关联文档与变更` 段
