## ADDED Requirements

### Requirement: cep-flow-support-value-lock

CEP SHALL 对**本仓权威**协议实例（`include/tlm/cep/protocol_instances.json` 的 `repo_owned_instance_ids`）的 `flow_support` 声明值建立**值层的变更检测义务**：声明值发生变更时 SHALL 同步更新仓内期望表并附台账 Entry；期望表与声明源不一致时 SHALL 判红。

> **边界（⛔ 不得过度声称）**：本 Requirement 规定的是**变更检测义务**，**不**赋予本仓对声明值的**独立权威** —— 值的权威仍是声明源 `include/tlm/cep/protocol_instances.json`；**镜像实例**（`moesi_gpu` / `mesif`）的权威在 ArchForge，**不在本 Requirement 范围内**。
> **依据**：`ADR-CEP-02 §2`（canonical 10 流清单）· `ADR-CEP-12 §13`（权威分区）· `ADR-CEP-04 §13`（第 6 种 SharerTracker 的追加式修订）。

#### Scenario: 期望表逐键登记 repo_owned 实例的 10 流声明值

- **WHEN** 契约测试加载期望表与声明源 `include/tlm/cep/protocol_instances.json`
- **THEN** 期望表 SHALL 覆盖 `repo_owned_instance_ids` 的**全部**实例，且两者的实例 id 集合 SHALL **双向相等** ⇒ 新增实例不得「无覆盖且绿」
- **AND** 每个实例的 `flow_support` SHALL 恰含 **10** 键，且键名 SHALL 与 canonical 10 流（`tlm::cep::kCanonicalFlowNames`）**逐名对齐**
- **AND** 每个期望值 SHALL 以**字面量常量**表达（⛔ 不得从声明源推导）⇒ 声明源与期望表不一致时 SHALL 判红
- **AND** 失败消息 SHALL 含 **实例 id + 键名 + 期望值 + 实际值**
- **AND** 域 SHALL 为 `repo_owned_instance_ids` 的**全集**（⛔ 不按 `status` 字段过滤）

#### Scenario: 期望表的出处逐条可核

- **WHEN** 契约测试校验期望表逐实例登记的出处
- **THEN** 每个实例的 `source` SHALL 指向**仓内存在**的文件，且该文件 SHALL 含所引锚点串
- **AND** 仓内**无逐值出处**的实例 SHALL 显式标注「无逐值出处（冻结口径）」
- **AND** ⛔ **不得**以「`ADR-CEP-12` 能力映射」作笼统背书 —— 该 ADR 的 `## Verification` 六条判据中**无**「逐实例 `flow_support` 值」对应项

#### Scenario: 反向控制证明判据有牙

- **WHEN** 对声明源的**内存副本**逐实例轮转翻转 1 个 `flow_support` 布尔值
- **THEN** 翻转后的值 SHALL **不等于**原值（⛔ 防止「true → true」空转）
- **AND** 期望表与副本的比对 SHALL 判否
- **AND** 失败消息 SHALL 含**实例 id + 键名**
- **AND** 被变异的实例数 SHALL 等于 `repo_owned_instance_ids` 的**全集**大小
- **AND** ⛔ **不得**采用「找到目标才断言」形态 —— 目标缺失时 SHALL 判红

#### Scenario: 持久反向 fixture 在脚本侧独立存活

- **WHEN** 仓内执行本 Requirement 的**持久**反向 fixture
- **THEN** 该 fixture SHALL 位于**脚本侧**（⛔ 不在 C++ 测试文件内）⇒ 删除 C++ 用例后 fixture 仍能令判据变红
- **AND** fixture SHALL 仅**写自己的临时目录**（⛔ 不改写仓内任何文件）⇒ 可安全进 ctest 并行运行
- **AND** 毒化态 SHALL 判红、未毒化态 SHALL 判绿

## MODIFIED Requirements

### Requirement: cep-sharer-tracker-contract

CEP SHALL 定义 Sharer Tracker 的 **6 种实现** + **caps descriptor** + **6 条不变量**（`INV-CEP-04-IC1/2/3/5/6/7`；`IC4` 永久跳空）。

> **订正（2026-11-15）**：原文写「**5 种实现**」，与代码真源 `include/tlm/cep/sharer_tracker.hh` 的 `kTrackerTraits`（**6** 行）不一致，亦与 `ADR-CEP-04 §13` 的追加式修订（「5 种实现 ⇒ **6 种实现**」，第 6 种 = `HierarchicalDirectory`）及兄弟 spec `openspec/specs/cep-hierarchical-directory/spec.md`（以 `kTrackerTraits[5]` 为三方真源）不一致 ⇒ 本次订正为 **6 种**。**决策正文不改**（`ADR-CEP-04` 零 diff）。
> **⚠️ Scenario 标题沿用历史命名（工具约束，非疏漏）**：openspec 的 `MODIFIED` 块按**场景名精确匹配**，且 archive **拒绝丢弃**既有场景名 ⇒ **含数量的 Scenario 标题一经落盘即无法经 delta 改名**。故本块**保留**标题「5 实现 + caps 四属性齐备」，**实现数以本条正文为准**（= 6）。⚠️ 该标题债的根因是「标题内嵌数量」这一写法本身 ⇒ 后续新增 Scenario **SHALL NOT** 在标题内嵌数量。

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
