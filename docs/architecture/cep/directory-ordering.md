# Directory & Ordering Plane(第 4 层)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> 目录 / 嗅探过滤的可插拔策略(Sharer Tracker)与排序点。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 5.2 目录策略可插拔(Sharer Tracker 5 实现,per `ADR-CEP-04 §2`)

| 实现 | scope | completeness | authority | aggregation_at | 适用 |
|------|-------|--------------|-----------|----------------|------|
| `FullMapDirectory` | SYSTEM | EXACT | GLOBAL | HOME | CHI HN-F 精确目录 |
| `SparseSnoopFilter` | SYSTEM | CONSERVATIVE | GLOBAL | HOME | CHI SF / CXL 2.0 Host SF(宁多探不漏探)|
| `DcohSymmetric` | LOCAL_MEMORY_REGION | **必须 CONSERVATIVE** | LOCAL_HINT | **HOME** | CXL 3.0 对称(per-device DCOH)|
| `TreeManager` | SUBTREE | EXACT | LOCAL | TREE_PARENT | TileLink ClientMetadata |
| `BroadcastNull` | NONE | EXACT(degenerate) | NONE | NONE | 无目录基线(target = all)|

**caps descriptor 替代 bool**(per `ADR-CEP-04 §2 Decision 3`):`scope` / `completeness` / `authority` / `aggregation_at` 四属性 struct,禁止任何 `is_distributed` / `is_central` bool flag。

**DCOH 完备性契约**(正确性命门):`CONSERVATIVE` = **绝不漏 sharer**(可超集);`PARTIAL` 必须声明 broadcast fallback;禁止无 fallback 的 PARTIAL。
