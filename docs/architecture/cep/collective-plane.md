# Fabric Collective Plane(第 5 层)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> 集合通信旁路平面 —— 与主干一致性流正交的第二平面。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 5.3 集合通信旁路平面(per `ADR-CEP-03`)

**原语最小集**:`{REDUCE, BROADCAST, MULTICAST, BARRIER}`(AllReduce = REDUCE + BROADCAST 复合,不列原语)。

**SHARP atomic vs cache-coherent atomic 区分**(5 条防误分类规则,per `ADR-CEP-03 §3.1`):

| 规则 | 内容 |
|------|------|
| **R1** | opcode 永远不作分类器(源端插件只发一种 ATOMIC)|
| **R2** | 唯一分类点在 fabric ingress:`(dest_addr ∩ collective_region_table) && group_epoch_valid` |
| **R3** | 区域互斥强制(一个地址不得同时 SHARP 已注册 + coherence-cacheable;组注册时 drain)|
| **R4** | epoch 纪律(group_destroy 后区域回归 coherence 域;epoch 未 commit 前 coherence atomic = 正常服务)|
| **R5** | 可观测性(divert 计数器 + fail-stop 断言)|

**跨平面 ordering 契约**(5 场景,per `ADR-CEP-03 §3.3`):Coherent atomic ↔ SHARP reduce 同地址时,前者须先 completion、后者须先 commit;复合流由 Core 层编排,**禁止**通过拓宽 coherence 事务集表达。
