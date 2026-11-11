# Core 与 Traffic Generator(主干驱动)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> 业务流量驱动面 —— Core 模型与 TrafficGen 的接入形态。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 本层当前状态

✅ **本层的架构决策已落 ADR**(per 批 4,台账 Entry #338)⇒ 本文件**不再是骨架**;下列边界现由 ADR 承载。

**决策载体**:
- **ASM-1 / 3 / 4 / 7 / 8 / 9**(六项装配决策)→ [`ADR-CEP-19`](../adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md) 的 `Status Update 2026-11-11`;
- **D-0 / D-2 / D-5 / D-6 / D-10**(另 5 条)→ `ADR-CEP-19` 的既有 Status Update;
- **「经网络」的定义 + J-9 拓扑分档判据** → [`fabric-transport.md`](fabric-transport.md) §6.3.1。

**已确立的边界**(摘自上述 ADR 与 §6.3.1;⛔ 改写须走 ADR 的「末尾追加 SU」形态):
- 可观测面在 wrapper(发起端),wrapper **只暴露原始观测事实**,聚合由 harness 消费;
- 双向映射的响应侧归 wrapper;⛔ 不得写成「承载层回响应 = 协议引擎发射」;
- 拓扑形态:**A(直连,零延迟,全绿载体)/ B(经网络,不承载全绿)** 并存 ⇒ 判据见 `fabric-transport.md` 的 J-9;
- ⛔ 本层**不**改变 B3b 的计数(仍 2/6)。
