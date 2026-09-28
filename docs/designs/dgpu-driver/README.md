# DGpu Driver 接口架构 (Driver Interface View)

> **状态**: ✅ v1.8 设计已通过 Oracle 五轮评审
> **对应架构文档**: [architecture.md](./architecture.md)（新建, 2026-09-26）
> **配套 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前)
> **基础架构**: [../dgpu-board/architecture.md](../dgpu-board/architecture.md) | **业务视角**: [../dgpu-soc/architecture.md](../dgpu-soc/architecture.md)

## 范围

本目录是 **DGpu Driver 接口架构** 文档，从 Linux kernel driver 视角描述：

- 15 ABI 函数入口（0 新增约束）
- 4 BAR 视角（BAR0 MMIO + BAR1 16MB + BAR2 8GB + BAR3+ 保留）
- 冻结面（4 个头文件零 diff）
- Driver 闭环数据流（H2D / D2H / D2D）
- 演进路径承诺（driver 源码零迁移）

## 与其他视角的关系

| 视角 | 文档 | 关注点 |
|------|------|--------|
| Board 内部 | [../dgpu-board/architecture.md](../dgpu-board/architecture.md) | DGpuBoard 5 层 + 5 组件 + 析构协议 |
| SoC 业务 | [../dgpu-soc/architecture.md](../dgpu-soc/architecture.md) | 6 模块拓扑 + GMMU + SDMA + 存储子系统 |
| **Driver 接口**（本目录） | [architecture.md](./architecture.md) | 15 ABI + 4 BAR + driver 闭环数据流 |

## 阅读路径

1. **快速理解**: architecture.md §1（15 ABI 函数表）+ §2（BAR 视角）
2. **冻结面约束**: architecture.md §3（4 个头文件零 diff）
3. **driver 闭环**: architecture.md §4（H2D / D2H / D2D 三类典型操作）
4. **演进承诺**: architecture.md §5（minimal → D5 零迁移）

## 适用读者

- UsrLinuxEmu 端 Linux driver 编写者
- 跨仓 driver-visible 测试用例设计者
- 架构评审者验证 driver 闭环完整性

## 维护记录

| 日期 | 事件 |
|------|------|
| 2026-09-26 | 新建（基于 cpptlm-driver-visible-minimal-soc v1.8 OpenSpec change） |

## 同步规则

本目录文档代表 driver 接口长期契约。修改需通过 OpenSpec change 提案；如涉及 ABI 行为变更或冻结面松绑，需先签发 ADR-DGPU-* 决策。
