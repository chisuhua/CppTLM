# DGpu SoC 业务架构 (Minimal SoC Business Architecture)

> **状态**: ✅ v1.0 设计已通过 Oracle 二轮复审
> **对应架构文档**: [architecture.md](./architecture.md)（原 `docs/designs/2027-02-09-minimal-dgpu-soc.md`，2026-09-26 迁移）
> **配套 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前)
> **基础架构**: [../dgpu-board/architecture.md](../dgpu-board/architecture.md) | **Driver 视角**: [../dgpu-driver/architecture.md](../dgpu-driver/architecture.md)

## 范围

本目录是 **DGpu SoC 业务架构** 文档，聚焦：

- 6 模块拓扑（DGpuBoard + PcieEndpointIP + PcieMemoryDevice + GMMU + SDMA + PcieConfigSpace）
- 存储子系统（镜像 gem5 `PhysicalMemory` + `AbstractMemory` 模式）
- BAR 路由与读写路径（BAR0 MMIO + BAR1 16MB 存储窗口 + BAR2 8GB vram aperture）
- GMMU 一级页表设计
- SDMA / Fence / MSI-X 链路
- 关键不变性（6 条 Invariants）

## 与其他视角的关系

| 视角 | 文档 | 关注点 |
|------|------|--------|
| Board 内部 | [../dgpu-board/architecture.md](../dgpu-board/architecture.md) | DGpuBoard 5 层 + 5 组件 + 析构协议 |
| **SoC 业务**（本目录） | [architecture.md](./architecture.md) | 6 模块拓扑 + GMMU + SDMA + 存储子系统 |
| Driver 接口 | [../dgpu-driver/architecture.md](../dgpu-driver/architecture.md) | 15 ABI + 4 BAR + driver 闭环数据流 |

## 阅读路径

1. **快速理解**: architecture.md §1（设计目标）+ §2（模块清单与拓扑）
2. **存储子系统**: architecture.md §3（镜像 gem5 模式 + DGpuBoard 新增）
3. **BAR 路由**: architecture.md §4（BAR 布局 + 路由表 + 读写路径）
4. **GMMU 设计**: architecture.md §5（API 骨架 + 翻译流程 + PTE 格式）
5. **关键不变性**: architecture.md §8（6 条 Invariants）
6. **演进路径**: 设计意图由 OpenSpec change [§X 演进路线图](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/design.md) 维护

## 维护记录

| 日期 | 事件 |
|------|------|
| 2026-09-26 | 从 `docs/designs/2027-02-09-minimal-dgpu-soc.md` 迁移至此 |
| 2027-02-09 | v1.0 设计 + Oracle 二轮复审 |

## 同步规则

本目录文档代表 SoC **业务层** 长期设计意图。具体修改应通过 OpenSpec change 提案；如涉及基础架构变更（新增模块、变更拓扑），需同步更新 dgpu-board 与 dgpu-driver 视角。
