# DGpuBoard 内部架构 (Board Internals)

> **状态**: ✅ v2.0 设计已通过 Oracle 二轮复审
> **对应架构文档**: [architecture.md](./architecture.md)（原 `docs/architecture/14-dgpu-board-ideal-arch.md`，2026-09-26 迁移）
> **配套 ADR**: [ADR-DGPU-01](../ADR-DGPU-01-callback-worker-replaces-detached-threads.md) / [02](../ADR-DGPU-02-lifecycle-protocol-uniform-destruction.md) / [03](../ADR-DGPU-03-data-driven-pcie-path-dispatch-registry.md) / [04](../ADR-DGPU-04-namespace-unification-ep-cache-test-isolation.md)
> **上层文档**: [../dgpu-soc/architecture.md](../dgpu-soc/architecture.md) | [../dgpu-driver/architecture.md](../dgpu-driver/architecture.md)

## 范围

本目录是 **DGpuBoard 内部架构** 文档，聚焦：

- 5 层架构（Host ABI → DGpuBoard shell → DGpuSoc → PcieEndpointIP → PCIe Devices）
- 5 个关键内部组件：LifecycleProtocol / CallbackWorker / DispatchRegistry / EpCache / PendingReqGuard
- 与 v1.0 的对比矩阵
- ABI 兼容性硬约束（23 ABI 签名 0 diff）
- Oracle 二轮复审的 4 项关键修订

## 与其他视角的关系

| 视角 | 文档 | 关注点 |
|------|------|--------|
| **Board 内部**（本目录） | [architecture.md](./architecture.md) | DGpuBoard 5 层 + 5 组件 + 析构协议 |
| SoC 业务 | [../dgpu-soc/architecture.md](../dgpu-soc/architecture.md) | 6 模块拓扑 + GMMU + SDMA + 存储子系统 |
| Driver 接口 | [../dgpu-driver/architecture.md](../dgpu-driver/architecture.md) | 15 ABI + 4 BAR + driver 闭环数据流 |

## 阅读路径

1. **快速理解**: architecture.md §1（设计目标）+ §2（5 层架构）
2. **关键组件**: architecture.md §3（5 组件 API + Invariants）
3. **ABI 兼容性**: architecture.md §5.1（签名级 + 二进制级 0 diff 仲裁）
4. **Oracle 评审重点**: architecture.md §7（callback nullification race / EpCache resolve 协议 / DispatchRegistry 性能）

## 维护记录

| 日期 | 事件 |
|------|------|
| 2026-09-26 | 从 `docs/architecture/14-dgpu-board-ideal-arch.md` 迁移至此 |
| 2027-02-09 | v2.0 设计 + Oracle 二轮复审（v2.0.2 修订） |

## 同步规则

本目录文档代表 DGpuBoard **基础架构** 的长期设计意图。具体修改应通过 OpenSpec change 提案；如涉及基础架构变更（新增内部组件、变更 ABI 兼容性约束），需先签发 ADR-DGPU-* 决策。
