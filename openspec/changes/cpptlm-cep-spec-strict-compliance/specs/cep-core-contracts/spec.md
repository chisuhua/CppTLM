## MODIFIED Requirements

### Requirement: cep-sharer-tracker-contract
CEP SHALL 定义 Sharer Tracker 的 **6 种实现** + **caps descriptor** + **6 条不变量**（`INV-CEP-04-IC1/2/3/5/6/7`；`IC4` 永久跳空）。


> **⚠️ Scenario 标题沿用历史命名（工具约束，非疏漏）**：openspec 的 `MODIFIED` 块按**场景名精确匹配**，且 archive **拒绝丢弃**既有场景名 ⇒ 含数量的 Scenario 标题一经落盘即无法经 delta 改名。故本块**保留**标题「5 实现 + caps 四属性齐备」，**实现数以本条正文为准**（= 6）⇒ 后续新增 Scenario **SHALL NOT** 在标题内嵌数量。

#### Scenario: 5 实现 + caps 四属性齐备

> ⚠️ **标题沿用历史命名**：本 Scenario 标题中的「5 实现」为**过时遗留**，**实现数 = 6**（见下方列表）。⛔ **不得**按标题数字理解本条。

- **WHEN** 契约测试检查 `ADR-CEP-04 §2` 的实现表
- **THEN** 恰好 **6** 个实现：`FullMapDirectory` / `SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull` / `HierarchicalDirectory`（第 6 种 = `HierarchicalDirectory`，per `ADR-CEP-04 §13` R-1 裁决 / 台账 Entry #96）
- **AND** 每个实现的 caps **恰好 4 属性**：`scope` / `completeness` / `authority` / `aggregation_at`
- **AND** 实现表中**不含** `is_distributed` / `is_central` 之类的 bool flag

#### Scenario: caps 值域收敛(IC1 / IC6)

- **WHEN** 契约测试枚举 caps 的全部枚举值
- **THEN** 每个属性字段的取值**全部落在已声明的 enum 字面值集合内**（机器可检查）
- **AND** `Hazard_*` 枚举（`Hazard_NONE` / `Hazard_STALL` / `Hazard_CHAIN`）在 **6** 种实现间**字面值收敛**

#### Scenario: token 类协议不跟踪 sharer set(IC7)

- **WHEN** 契约测试检查 token 类协议实例的 Sharer Tracker 选型
- **THEN** 该实例的 `sharer_tracker.impl` 为 `BroadcastNull` 或标记为**不参与**
- **AND** **不得**为 `FullMapDirectory`（token 由数据副本携带权限，不依赖中心 sharer set）
