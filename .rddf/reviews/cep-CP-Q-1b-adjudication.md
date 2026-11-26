# CP-Q-1b 裁决记录 —— 用户可写键子集 / `family` / `protocol_version`

> **性质**：**架构裁决记录**（`architect` 产出）—— 落实 `ADR-CEP-24` 决策 3.2 的三项实施前置（`ADR-CEP-24` §8 F5 的另一半）。
> **⛔ 不是 ADR**：本裁决**不落 contract**（`family` 加值 / `protocol_version` 前缀 / schema 变更均属后续 change 的 contract 面）。
> **⛔ 状态**：✅ **已 ratify**（2026-11-26 · 用户批准 **R1~R4**：先「批准R1~R3」，经 §7.3 澄清后选 **甲 = R4 一并批准**）
> **审查**：oracle 第 1 轮 **`REJECT`**（3 高影响 + 3 中 + 2 低 ⇒ **8/8 已落**，见 §0）→ 第 2 轮（聚焦复审·只验闭合）**`APPROVE`**（8/8 闭合 · 2/2 独立验证点未改坏）
> ⚠️ **critic 不在本件轮次**：本记录非 ADR、不反转既有决策（`ADR-CEP-24` 决策 3.2 显式委托）⇒ 其**落地的 3 份 ADR SU** 才是 ADR 修改，**届时各自走 critic**（见 §7 末）
> **前置**：`ADR-CEP-24`（已签发）· `CP-Q-1a` 裁决（已 ratify）· `cep-ssp-declaration` 批 1（**已归档** 2026-11-26）· 台账 Entry #552 / #553

---

## 0. oracle 首审（REJECT）的 8 条处置

| # | 严重度 | oracle finding | 处置 |
|:--:|:--:|---|---|
| **1** | 🔴 **架构性错误** | `combination_capabilities` 划为「框架固定 / 装配期由 CEP 判定」**不可实现**：① 它是**每实例声明输入**（`channel_dependency_graph.hh:241` 逐字消费 `combination_capabilities.hierarchical`）② 框架**无推导算法**（`ADR-CEP-12:146` 「**不提供**自动协议兼容性判定器」）③ `INV-CEP-12-IC1` 要求 13 属性**必含** ⇒ 用户不可写 + 框架无推导 = **无来源的必填键** | ✅ **改判「🟡 受限用户声明」**（§3）|
| **2** | 🔴 成本漏项 | Q3 漏 **`INV-CEP-11-IC1`**：其逐字写「每个协议实例的 `protocol_version` **必须**落在 §3 锁定表内；表外取值 = **装配期 fail**（不得静默降级）」⇒ `user:` 分支**直接违反 IC1 字面**。连带漏：决策 5 排除行（`:117`）· `test_cep_schema.cc:573` 的 IC1 通用闸（逐实例断言 ∈ 锁表 ⇒ `user:` 实例即红）| ✅ **已补**（§5.2 成本表 + §6 AC）|
| **3** | 🔴 必改非「若加」 | schema `pattern` = `^[a-z0-9]+(-[a-z0-9.]+)*$`（`protocol_instance.schema.json`）**拒绝 `:`** ⇒ `user:mesif_ssp` **在 schema 层即被拒**，到不了装配闸。且两分支谓词须落进**共享 helper**（`test_cep_schema.cc:552` 记 Entry #109 G-2「不得多处各抄一份」）| ✅ **已改为必改项 + 落点钉死**（§5.2）|
| **4** | 🟡 中 | Q1 的 「8 自由」会让批 2 漏掉三键的**校验面**：`vc_binding`（vc_id ∈ `fabric.json` 槽位上界 + 消息类词表 + 与 collective VC 不冲突 + 死锁自由闸消费）· `ordering_assumption`（`INV-CEP-12-IC2` 装配期一致性校验）· `flow_support`（14 锚封闭词表）| ✅ **已加「受限」注记**（§3）|
| **5** | 🟡 中 | Q2 连带面漏 **`V-24-13`**（`ADR-CEP-24:228` 含「`family` ⊆ **3 值域**（⛔ 不得出现第 4 个族值）」⇒ 加值后**必红**）；且 `protocol_family.hh:106-111` 的三个值断言应按同模式**新增** `static_assert(UserDefined == 4)`；「6 处 vs 6~7 处」口径须统一 | ✅ **已补 + 统一为 6 处**（§4.2）|
| **6** | 🟡 中 | `status` 的机判粒度须写明：「adapter 存在」= **presence**（注册表 / 符号存在性），⛔ **非**行为等价性证明 ⇒ 防批 2 读成 conformance 检查 | ✅ **已写明**（§3）|
| **7** | 🟢 低 | Q3② 版本维：现 `ssp.id` **无版本字段**（批 1 实测）⇒ `user:<ssp-id>` 单版本语义当前成立；须加 revisit trigger | ✅ **已加**（§5.4）|
| **8** | 🟢 低 | Q3 备选案**漏列 E**（固定字面量 `"user-defined"`）—— 应列出并**显式排除** | ✅ **已补**（§5.1 案 E）|

> **oracle 独立验证为成立的三点**（不再列问题）：① **两分支可证不相交**（官方版本全含 `-`/`.`；`ssp.id` 词表 `^[a-z][a-z0-9_]*$` **排除** `-` 与 `.` ⇒ `user:<ssp-id>` 不可能与官方版本字面相同）⇒ 「不变弱」成立 ② `protocol_id_` 推导（`module_factory.cc:1073` 的 `static_cast`）与 `kUnassignedProtocolFamilyId = 0` 语义不受加值影响 ③ Q2 的 A 案有 `ADR-CEP-24` 决策 3.2 的**显式授权**（「显式决策 `family` 是否加值」）⇒ 「须 SU」程序足够。

---

## 1. 裁决对象与**范围界定**

| # | 待定项 | `ADR-CEP-24` 决策 3.2 的原话 |
|:--:|---|---|
| **Q1** | **用户可写键子集** | 「13 必含键中**哪些是用户可写**、哪些是框架固定，本 ADR 未裁定」|
| **Q2** | **`family` 的用户侧取值** | 「`family` 枚举**仅 3 值** ⇒ 用户协议填哪个？加第 4 值 = 改冻结面 schema」|
| **Q3** | **`protocol_version` 与 `ADR-CEP-11` 的关系** | 「用户协议的版本语义与 `ADR-CEP-11` 的关系**未定义**」|

⚠️ **范围界定（重要）**：`cep-ssp-declaration` **批 1 = 声明面 + loader + V-24-2 逐格对账**（已归档），其 SSP **⛔ 不绑定 `protocol_instance`**（实测 `ssp.schema.json` 无 `family` / `protocol_version` / 绑定字段）⇒ **三项都只在「批 2」才成为阻碍**。⛔ **不要**把本裁决读成「批 1 有缺口」。

---

## 2. 证据（实测 · 逐 `file:line`）

### 2.1 实例面 13 必含 + 1 可选（值域实测）

| 键 | 值域 | 键 | 值域 |
|---|---|---|---|
| `id` | string | `atomic` | object |
| `family` | **enum 3 值** | `consistency_labels` | object |
| `adapter` | string | `ordering_assumption` | object |
| `status` | **enum 3 值** | `vc_binding` | object |
| `protocol_version` | string，**pattern `^[a-z0-9]+(-[a-z0-9.]+)*$`**（**拒绝 `:` / `_`**）| `sharer_tracker` | object |
| `flow_support` | object | `combination_capabilities` | object |
| `perm` | array | `combination_contract` | object（**可选**）|

### 2.2 ⭐ 三条装配期 / schema 层硬门（Q2/Q3 的真正动因）

| 门 | 原文 | 出处 |
|---|---|---|
| **[IAG-03]** | 「`family` **必须存在且可解析**」（`protocol_family_from_name` 只认 3 值）| `instance_assembly_gate.hh:141-148` |
| **[决策 3] + [IC1]** | 决策 3：「`protocol_version` **必须**落在锁定表内，否则装配期 fail」；**IC1**：「表外取值 = **装配期 fail**（不得静默降级）」| `ADR-CEP-11:84-92` · `:156` |
| **schema pattern** | `^[a-z0-9]+(-[a-z0-9.]+)*$` ⇒ ⛔ **`:` 与 `_` 均被拒** | `protocol_instance.schema.json` 的 `protocol_version` |

⚠️ **须分清**：`INV-CEP-11-IC2` 管「**唯一真源**」（防双真源）；**IC1** 管「**必须在表内**」。Q3 要动的是 **IC1 + 决策 3 + 决策 5**（三处），⛔ **不动 IC2**。

### 2.3 `combination_capabilities` 是**声明输入**（Q1 的关键证据）

`channel_dependency_graph.hh:241` 逐字：`bool hierarchical = false;  // combination_capabilities.hierarchical` ⇒ 它由**实例声明**解析而来，⛔ **不是**框架生成的结论。⇒ 框架「判定」它**无实现路径**（且 `ADR-CEP-12:146` 明确不提供自动兼容性判定器）。

---

## 3. Q1 —— 用户可写键子集（**裁定**）

**判据**：该键描述「**这个协议是什么**」（用户可知）还是「**它如何接入 CEP 的机制**」（框架决定 / 校验）。

| 键 | 归属 | 依据 |
|---|---|---|
| `id` | ✅ **用户可写** | 实例标识（须全局唯一 ⇒ 装配期唯一性检查）|
| `adapter` | ✅ **用户可写** | 指向用户 adapter 类名 |
| `perm` | ✅ **用户可写** | 用哪些 perm 值（子集；⛔ 不可造第 5 值，`CEN-Ib11`）|
| `atomic` | ✅ **用户可写** | 是否支持 atomic 意图 |
| `consistency_labels` | ✅ **用户可写** | 一致性模型标签（`ADR-CEP-12` 决策 3/4）|
| `flow_support` | 🟡 **受限用户声明** ⭐ | 用户可写，但**值域受 14 核心锚 / canonical 10 流约束**（`CP-Q-1a`）⇒ 声明须过词表校验 |
| `ordering_assumption` | 🟡 **受限用户声明** ⭐ | 用户可写，但 `INV-CEP-12-IC2` 做**装配期一致性校验**（逐 network × vc 解析到唯一 vc_id，否则 fail）|
| `vc_binding` | 🟡 **受限用户声明** ⭐ | 用户可写，但须过校验：vc_id ∈ `fabric.json` 槽位上界 + 6 类消息类词表 + 与 collective VC 预留不冲突 + 死锁自由闸消费 |
| `sharer_tracker` | 🟡 **受限用户选择** | 只能从 CEP 的**已登记实现**中选（`kTrackerTraits` 行集）；⛔ 不可自定义 tracker |
| `status` | 🟡 **受限用户声明** | 用户声明交付状态；⚠️ **机判粒度 = presence**（`implemented` 须有对应 adapter 的**存在性**）—— ⛔ **不是**行为等价 / conformance 证明（防批 2 读成一致性检查）|
| `combination_capabilities` | 🟡 **受限用户声明** ⭐ | **改判**（oracle finding 1）：用户声明**自己协议**的能力画像（concurrent / hierarchical / heterogeneous）；CEP **校验一致性**（如 `hierarchical=true` 须有 `vc_binding` / `ordering_assumption` 佐证），⛔ **不生成** |
| `family` | ❌ **框架固定** | 见 **Q2**（[IAG-03] 须可解析 ⇒ 须落在族域内）|
| `protocol_version` | ⚠️ **见 Q3** | 见 **Q3** |

⇒ **自由 5 + 受限 6 + 框架 1 + 待裁 1**（⚠️ oracle finding 4：不再是「8 自由」—— 三键有校验面）。

---

## 4. Q2 —— `family` 的用户侧取值（**裁定**：**加第 4 值 `user-defined`**）

### 4.1 案对比

| 案 | 形态 | 成本 | 判定 |
|---|---|---|---|
| **A. 加第 4 值 `user-defined`** | schema enum + 头文件 5 处 | **6 处**（1 处冻结面 ⇒ 五步链）| ✅ **选中** |
| B. 用户选既有 3 值之一（如 `academic`）| 零改动 | 零 | ❌ **假分类**（语义 + 统计双错，违诚实原则）|
| C. `family` 对用户协议变 optional | schema 去 required（13 → 12）| 破 `test_cep_schema.cc:443` + 改 [IAG-03] | ❌ 成本最高且破契约 |
| D. 独立字段 `source_kind` | 新字段（不动 family）| 新字段 + 两套分类 | ❌ 与 `family` 语义重叠 ⇒ 第二套标准 |
| E. `family` 改为自由字符串 | 去 enum | — | ❌ 使 [IAG-03] 的「可解析」机判**退化**（任意串均可解析）⇒ 比假分类更糟 |

### 4.2 成本 = **6 处**（统一口径 · oracle finding 5 已核）

| # | 位置 | 改动 |
|:--:|---|---|
| 1 | `protocol_instance.schema.json` 的 `family` enum | 加 `user-defined`（**M1 冻结面** ⇒ 五步链）|
| 2 | `protocol_family.hh:43-47` `enum class ProtocolFamily` | 加 `UserDefined = 4` |
| 3 | 同文件 `:53` `kProtocolFamilyCount = 3` | → **4** |
| 4 | 同文件 `:113` `static_assert(kProtocolFamilyCount == 3, ...)` | → 4 |
| 5 | 同文件 `:56` `kProtocolFamilies[]` | 加条目 |
| 6 | 同文件 `:79-89` `protocol_family_name` 的 **`switch`** | 加 `case UserDefined: return "user-defined";` |

⚠️ **须新增（同模式 · oracle finding 5）**：`protocol_family.hh:106-111` 现有三个 `static_assert(protocol_family_id(...) == 1/2/3)` ⇒ **新增** `static_assert(protocol_family_id(ProtocolFamily::UserDefined) == 4, ...)`（⛔ 不改既有的三个）。

⚠️ **「自动支持」的实测依据**：`from_name` / `from_id` 迭代 `kProtocolFamilies[]`（`:69-75 / :92-96`）；`protocol_id_` 推导 = `module_factory.cc:1073` 的 `protocol_family_id(fam)`（`static_cast`，`protocol_family.hh:63-65`）⇒ 编号 4 自动生效；`kUnassignedProtocolFamilyId = 0` 语义不受影响；`test_cep_protocol_bridge_contracts.cc` **无族数硬断言**（实测 0 命中）⇒ **无需改**。

### 4.3 ⭐ Q2 的连带面（**oracle finding 5 · 必须同批**）

| # | 连带项 | 处置 |
|:--:|---|---|
| **1** | **`ADR-CEP-24:228` 的 V-24-13** 含「`family` ⊆ **3 值域**（⛔ 不得出现第 4 个族值）」⇒ 加值后该判据**必红** | ⛔ **必须**随 `ADR-CEP-24` 的 SU **同批改写** V-24-13 为「⊆ **4 值域**（`industrial` / `ruby-slicc` / `academic` / `user-defined`）」|
| 2 | `ADR-CEP-12:49` 的 `family: <industrial \| ruby-slicc \| academic>` 注释 | `ADR-CEP-12` SU（**冻结面**）|
| 3 | `protocol_family.hh:53` 的注释「per `ADR-CEP-12` 的 `family` 域 —— 恰好 3 值」 | 改「恰好 4 值」 |
| 4 | `07 号 §5.1` 的「2 来源族 / 16 实例」口径 | ⛔ 需一句话澄清：「**来源族分布 ≠ 枚举值数**」；⛔ 不得读成「来源族 3」|

---

## 5. Q3 —— `protocol_version`（**裁定**：**`"user:<ssp-id>"` 前缀 + 装配期 / schema 两处两分支**）

### 5.1 案对比

| 案 | 形态 | 判定 |
|---|---|---|
| **A. `"user:<ssp-id>"` 前缀 + 两分支** | 校验改为「**在锁表内**」**或**「`^user:[a-z][a-z0-9_]*$` 且 `<ssp-id>` ∈ `ssps.json` 已登记 id」 | ✅ **选中** |
| B. 用户版本登记进锁表 | ❌ 污染唯一真源（`IC2`）—— 用户自创版本不是**规范**版本 |
| C. 用户协议豁免版本校验 | ❌ 削弱既有闸（`IC1` 的立法意图 = 防配置层错误静默降级）|
| D. `protocol_version` 对用户协议 optional | ❌ 破 13 必含 ⇒ 同 Q2 案 C 成本 |
| **E. 固定字面量 `"user-defined"`**（**oracle finding 8 · 补列**）| ❌ **显式排除**：不引用 `ssp-id` ⇒ **丢失「实例 → SSP 语义真源」的可追溯性**（用户改了哪个 SSP 无从对应）；且**同样**要动 `IC1` + pattern ⇒ 成本不低于 A 而信息量更低 |

### 5.2 裁定依据 + ⭐ **完整成本（oracle finding 2/3 · 三处 ADR + 两处代码）**

**依据**：
1. 锁表**不变**（官方版本仍只在该表 ⇒ **`IC2` 零触碰**）；
2. **两分支可证不相交**（oracle 独立验证）：官方版本全含 `-` / `.`（`chi-r2` / `tilelink-c-1.7.1` …）；`ssp.id` 词表 `^[a-z][a-z0-9_]*$`（`ssp_loader.hh:94` 的 E2）**排除** `-` 与 `.` ⇒ `user:<ssp-id>` **不可能**与任何官方版本字面相同 ⇒ **校验不变弱**；
3. `ssp.id` 唯一性由 loader **E2** 保证（`ids.insert(sid).second` 判重）⇒ `user:<ssp-id>` 的 `<ssp-id>` 可寻址。

**⚠️ 完整成本（原草案漏 IC1 与 pattern —— 已补）**：

| # | 面 | 改动 | 冻结面 |
|:--:|---|---|:--:|
| 1 | **`ADR-CEP-11` 决策 3** | 追加分支（SU）| ✅ 五步链 |
| 2 | ⭐ **`INV-CEP-11-IC1`**（`:156`）| 字面「表外取值 = 装配期 fail」⇒ 须 SU 扩展为「表外 **且非 `user:` 合法形态** ⇒ fail」| ✅ 五步链 |
| 3 | **`ADR-CEP-11` 决策 5 排除行**（`:117`「**不**允许 schema 出现锁定表之外的版本」）| 须 SU 收窄为「除 `user:` 形态外」| ✅ 五步链 |
| 4 | ⭐ **schema `pattern`** | `^[a-z0-9]+(-[a-z0-9.]+)*$` **必改**（否则 `:` 在 schema 层即拒）⇒ `^(?:[a-z0-9]+(-[a-z0-9.]+)*\|user:[a-z][a-z0-9_]*)$` 或等价 | ✅ 五步链 |
| 5 | ⭐ **共享 helper** | 两分支谓词落入**单一** helper（现 `test_cep_schema.cc:552` 记 Entry #109 G-2「不得多处各抄一份」；现存 IC1 通用闸在 `test_cep_schema.cc:573`，`instance_assembly_gate.hh` **不查** `protocol_version`）⇒ ⛔ **禁**在 IAG 另写一份 | — |
| 6 | **`test_cep_schema.cc:573` IC1 通用闸** | 逐实例断言 ∈ 锁表 ⇒ 须**两分支化** + 反向 fixture（`user:不存在的id` ⇒ 红；`user:` 但 id 未登记 ⇒ 红；`chi-r3` ⇒ 红（不匹 `^user:`））| — |
| 7 | **`test_cep_schema.cc:512` pattern 容纳性用例** | 须纳入 `user:<ssp-id>` 形态 | — |

⇒ ⚠️ **落点钉死**：**AC 的三处 ADR SU 必须在同一批**（`ADR-CEP-11` 的 决策 3 + IC1 + 决策 5），⛔ 不得只改决策 3。

### 5.3 SU 能否扩展决策语义（oracle 验证）

✅ **有先例**：`ADR-CEP-12:156-159`（IC1 口径修订）、`ADR-CEP-11:138-144`（原地更新块）⇒ **SU 路径足够**，⛔ 不必新建 ADR 取代决策 3。

### 5.4 ⭐ revisit trigger（oracle finding 7）

现 `ssp.id` **无版本字段**（批 1 实测）⇒ `user:<ssp-id>` 的单版本语义**当前成立**。⚠️ **若未来 SSP 引入版本字段**（如 `ssp.version`），则 `user:` 取值语义**须重裁**（届时可能需 `user:<ssp-id>@<ver>`）⇒ **登记为 revisit trigger**，⛔ 不得默认沿用。

---

## 6. 对「批 2」的约束（AC 草案）

| # | 约束 | 判据形态 |
|:--:|---|---|
| 1 | 用户可写的 **5 键**在 schema 上无框架硬编码 | 装配用例 + 反向 fixture |
| 2 | 三个**受限用户声明键** `flow_support` / `ordering_assumption` / `vc_binding` 各自的校验面生效（词表 / 逐 vc 解析 / vc 上界 + collective 不冲突）| 每键 1 正 1 负 |
| 3 | `sharer_tracker` 只接受 `kTrackerTraits` 的**已登记实现名** | 枚举 + 反向 fixture |
| 4 | `status: implemented` 须有对应 adapter 的**存在性**（⛔ 非 conformance）| presence 校验 + 反向 fixture |
| 5 | ⭐ `combination_capabilities` **由用户声明**（非框架生成）+ CEP **校验一致性**（`hierarchical=true` 须有 `vc_binding`/`ordering_assumption` 佐证）| 2 正 2 负（声明与佐证矛盾 ⇒ 红）|
| 6 | `family` 第 4 值 `user-defined` 在 `from_name` / `from_id` / `name` **三路径**可解析 | 三路径各 1 正 1 负 |
| 7 | ⭐ `protocol_version` **两分支**：锁表内 **或** `^user:[a-z][a-z0-9_]*$` 且 id ∈ `ssps.json` | 4 反向 fixture（见 §5.2 #6）|
| 8 | ⭐ **`V-24-13` 改写后**：`family` ⊆ **4 值域** | 反向 fixture（第 5 值 ⇒ 红）|
| 9 | 既有 8 实例**行为零变更** | 全量回归 |

---

## 7. 用户 ratify 记录

### 7.1 裁决记录（**2026-11-26 · 部分 ratify**）

| # | 待 ratify | **裁决** | 状态 |
|:--:|---|---|---|
| **R1** | Q1 键划分（**自由 5 / 受限 6 / 框架 1 / 待裁 1**）| ✅ **用户批准**（原话「批准R1~R3」）| ⚠️ **内容上已含 R4 的结论**（「受限 6」包含 `combination_capabilities`）—— 见 §7.2 |
| **R2** | Q2 取 **A**（加 `user-defined`）| ✅ **用户批准** | ✅ 可落盘 |
| **R3** | Q3 取 **A**（`user:<ssp-id>`）| ✅ **用户批准** | ✅ 可落盘 |
| **R4** | `combination_capabilities` 改判「**受限用户声明**」（用户声明 + CEP 校验，⛔ 不生成）| ✅ **用户批准（经 §7.3 澄清选「甲：R4 一并批准」）** | ✅ 可落盘 |

### 7.2 ✅ R1/R4 矛盾已澄清（**历史记录** · 用户选「甲」）

- **R1 的「受限 6」在内容上包含 `combination_capabilities`** ⇒ 批准 R1 = 批准 R4 的**实质结论**；但 R4 作为**独立编号**被排除 ⇒ **形式与内容不一致**。
- 若按 R1 字面落盘（`combination_capabilities` ∈ 受限）⇒ 事实上执行了 R4，与「未批准 R4」冲突。
- 若按「未批准 R4」落盘（`combination_capabilities` ∈ **框架固定**）⇒ **不可实现**（oracle 🔴 finding 1：它是声明输入 + 框架无推导算法 + `INV-CEP-12-IC1` 要求必含 ⇒ **无来源的必填键**），且「受限 6」要变「受限 5 / 框架 2」，**与 R1 的字面也不一致**。
- ⇒ ⛔ **本裁决暂不落盘**（3 份 ADR SU 均未启动）—— 待 §7.3 的澄清。

### 7.3 澄清结果（**已裁** · 候选路径留痕）

| 路径 | 内容 | 代价 / 风险 |
|---|---|---|
| **甲（✅ 用户选中）** | **R4 一并批准** —— 即确认 R1 的「受限 6」按内容执行（`combination_capabilities` = 受限用户声明）| 零额外代价；与 oracle 的 🔴 修正一致 |
| **乙** | **R4 不批准，且 `combination_capabilities` 保持「框架固定」** | ⛔ **不可实现**（无推导算法）⇒ 批 2 会卡在「必填键无来源」，且与 R1 字面不一致 |
| **丙** | **R4 不批准，改为「框架固定 + 用户声明为可选输入」**（框架不推导，但允许用户填；装配期只在用户填了才校验）| 需**重裁**该键（新形态，须再过 oracle）⇒ 追加一轮 |
| **丁** | **R4 暂缓** —— 本裁决**只落 R2 + R3**（`family` + `version`），`combination_capabilities` **留待后续裁决**；`ADR-CEP-24` 的 SU 只写「Q2 / Q3 已裁，Q1 的该键待裁」| 可立即推进 R2/R3；但 `ADR-CEP-24` 决策 3.2 的 Q1 **仍部分未定** |

### 7.4 落盘动作（✅ 已解除阻塞）

1. 本裁决 → 已 ratify（architect）
2. **三份 ADR SU 同批**：`ADR-CEP-24`（决策 3.2 + **V-24-13 改写为 4 值域**）· `ADR-CEP-11`（**决策 3 + IC1 + 决策 5**）· `ADR-CEP-12`（`family` 域加值）⇒ **一次五步链**（⛔ 不拆批）
3. 台账 Entry（planner）→ 产物 commit → `--update` → 清单单独 commit → 无参复验（builder）
4. ⚠️ **3 份 ADR SU 落地时各自走 critic**（它们是 ADR 修改，非本记录）

---

## 8. 证据来源

| 证据 | 落点 |
|---|---|
| 13 必含 + 值域 + **`pattern` 拒 `:`** | `protocol_instance.schema.json`（实测）|
| **[IAG-03]** | `instance_assembly_gate.hh:141-148` |
| **[决策 3] / [IC1] / [决策 5]** | `ADR-CEP-11:84-92` / `:156` / `:117` |
| **`combination_capabilities` 是声明输入** | `channel_dependency_graph.hh:241` |
| **无自动兼容性判定器** | `ADR-CEP-12:146` |
| **`family` 加值 6 处 + 三个值断言** | `protocol_instance.schema.json` · `protocol_family.hh:43-53,56,63-65,69-75,79-89,92-96,106-113` |
| **`protocol_id_` 推导用 `static_cast`** | `module_factory.cc:1073` + `ADR-CEP-12:618` |
| **IC1 通用闸 / pattern 用例** | `test_cep_schema.cc:512` / `:573` / `:552`（Entry #109 G-2）|
| **`V-24-13` 的 3 值锁** | `ADR-CEP-24:228` |
| **`ssp.id` 词表（排除 `-`/`.`）** | `ssp_loader.hh:94`（E2）|
| 批 1 / 批 2 分界 | `openspec/changes/archive/2026-11-26-cep-ssp-declaration/proposal.md:6` |
| 既有 8 实例版本取值 | `include/tlm/cep/protocol_instances.json` |

---

## 9. 关联

`ADR-CEP-24` 决策 3.2 / §8 F5 / **V-24-13** · `ADR-CEP-11` 决策 2/3/**IC1**/决策 5/**IC2** · `ADR-CEP-12` 决策 1 / `:132-134` / `:146` / `:618` · `CP-Q-1a` 裁决（已 ratify）· objective `obj-cep-custom-protocol` 的 `CP-Q-1b` / `CP-I-2`（批 2 前置）· Entry #109（G-2 单一 helper）
