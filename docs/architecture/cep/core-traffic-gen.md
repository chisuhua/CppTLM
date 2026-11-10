# Core 与 Traffic Generator(主干驱动)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> 业务流量驱动面 —— Core 模型与 TrafficGen 的接入形态。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 本层当前状态

⚠️ **本层的架构决策尚未落 ADR**(属**批 4** 的 A 类落点)⇒ 本文件**暂为骨架**。

现行决策登记见 [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) 的
`Status Update(2026-11-08 · 十一)`(10 条决策登记)与 `Status Update(2026-11-08 · 十二)`(D-0 更正 / J-9)。

**已确立的边界**(摘自上述两则 SU,⛔ 未经批 4 不得改写):
- 可观测面在 wrapper(发起端),wrapper **只暴露原始观测事实**,聚合由 harness 消费;
- 双向映射的响应侧归 wrapper;⛔ 不得写成「承载层回响应 = 协议引擎发射」;
- 拓扑形态:**A(直连,零延迟,全绿载体)/ B(经网络,不承载全绿)** 并存 ⇒ 判据见 `fabric-transport.md` 的 J-9;
- ⛔ 本层**不**改变 B3b 的计数(仍 2/6)。
