# Directory & Ordering Plane(第 4 层)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> 目录 / 嗅探过滤的可插拔策略(Sharer Tracker)与排序点。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 5.2 目录策略可插拔(Sharer Tracker 6 实现,per `ADR-CEP-04 §2`)

| 实现 | scope | completeness | authority | aggregation_at | 适用 |
|------|-------|--------------|-----------|----------------|------|
| `FullMapDirectory` | SYSTEM | EXACT | GLOBAL | HOME | CHI HN-F 精确目录 |
| `SparseSnoopFilter` | SYSTEM | CONSERVATIVE | GLOBAL | HOME | CHI SF / CXL 2.0 Host SF(宁多探不漏探)|
| `DcohSymmetric` | LOCAL_MEMORY_REGION | **必须 CONSERVATIVE** | LOCAL_HINT | **HOME** | CXL 3.0 对称(per-device DCOH)|
| `TreeManager` | SUBTREE | EXACT | LOCAL | TREE_PARENT | TileLink ClientMetadata |
| `BroadcastNull` | NONE | EXACT(degenerate) | NONE | NONE | 无目录基线(target = all)|
| `HierarchicalDirectory` | SYSTEM | EXACT | GLOBAL | **HIERARCHICAL** | MESIF 分级目录(多级聚合;per `ADR-CEP-04 §13` R-1 裁决)|

> ⚠️ **枚举名 vs 实现类名**:上表第 1 列是 `TrackerImpl` **枚举值**(代码真源 = `include/tlm/cep/sharer_tracker.hh` 的 `kTrackerTraits`,6 行)⇒ 其中 `BroadcastNull` 的**实现类名**是 `BroadcastNullTracker`(`include/tlm/cep/broadcast_null_tracker.hh`),其余 5 种实现类名与枚举值同名。
> ⚠️ **本表 caps 四属性与代码真源逐键一致**(2026-11-14 实测:`scope` / `completeness` / `authority` / `aggregation_at` 6 行全符)⇒ 文档与代码的漂移面只在**实现计数**(曾写 5)与**类名写法**两处。

**caps descriptor 替代 bool**(per `ADR-CEP-04 §2 Decision 3`):`scope` / `completeness` / `authority` / `aggregation_at` 四属性 struct,禁止任何 `is_distributed` / `is_central` bool flag。

**DCOH 完备性契约**(正确性命门):`CONSERVATIVE` = **绝不漏 sharer**(可超集);`PARTIAL` 必须声明 broadcast fallback;禁止无 fallback 的 PARTIAL。
