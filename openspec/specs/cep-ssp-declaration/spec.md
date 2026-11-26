# cep-ssp-declaration Specification

## Purpose

CEP 的 **用户自定义稳定态协议（SSP）声明面**（批 1）：把「用户自定义协议」从「写 C++ 插件」降为「填一份 JSON 声明 + 写适配器」，且该声明面能**逐格复现**既有手写 `*_state_spec.hh` 的稳定态语义。

**架构依据**：`ADR-CEP-24`（已签发 2026-11-26）· `CP-Q-1a` 裁决（已 ratify：事件字母表 = **新建** + 14 核心锚 + 扩权方 = architect + 先例库摊销）。

### ⭐ 表达力范围（**必须逐字随引用传播** —— `ADR-CEP-24` SU 第二则）

- **体裁（有 `State × Event` 转移表）= 3/6**：`mesif`（6 事件）· `tilelink_c`（4）· `cxl_2_0`（17）。
- **按本 spec 声明的格形态（`0xFF` 空洞 + 逐格 `Basis`）的干净覆盖 = 2/6**：`mesif` · `cxl_2_0`。
  ⚠️ **`tilelink_c` 的格是掩码格**（`Transition{kind, target_mask, …}`，`target_mask` 3 位）⇒ 需「掩码格」扩展 ⇒ **批 2**。
- 另 3 份 state_spec（DCoh 三视图 / SHARP 聚合 / UALink INC）**无 `Event` 枚举、无转移表**。
- ⛔ **不得**把上述读成「6 份全部可表达」（整文件零例外）。

### 诚实边界（⛔ 必须随引用重复）

1. **loader 只保「形态 + 引用闭环」**，⛔ **不承诺**语义（活性 / 无死锁 / 一致性 / 兼容性）—— 逐字继承 `ADR-CEP-13` 决策 5/6。一个**有死锁**的 SSP 在 loader 侧**不会被拒绝**（形态合法）⇒ 该缺口由实验侧违例闸承担。
2. **`flow` 映射是部分映射**：`Cap` / `Grow` / `Prune` / `Report` 是**本地权限转移**语义，⛔ 不对应任何 canonical 流 ⇒ 其 `flow = nullptr` ⇒ 「事件 flow ∈ 10 流」的正确表述 = 「**非空** flow ∈ 10 流」。
3. `ssps.json` 的 `mesif_ssp` 是 `mesif_state_spec.hh` 的 `kTable` 的**一次性手工转写**，由 `V-24-2` 测试**持续守护**；⛔ 无 repeatable code→data 管线 ⇒ 不构成 `ADR-CEP-24` 决策 1.3 的「烘焙」。
4. 本 spec 覆盖**声明面 + loader + 逐格对账**（批 1）+ **掩码格扩展**（批 2 `cep-ssp-batch2`）；**通用解释器 / 装配集成 / 实验 4 件套** ⇒ **批 2 另立 spec**（`openspec/specs/cep-ssp-runtime/spec.md`）。

### 范围边界（⛔ 零变更面）

- ⛔ **不进** `protocol_instance.schema.json`（实例面 13 必含 + 1 可选 = `properties` 14 不动）
- ⛔ 不改既有 6 份 `*_state_spec.hh`（**不迁**）
- ⛔ 不改既有 8 实例行为
- ⛔ 新增 invariant = 0（复用 `INV-CEP-07A-IC2` / `INV-CEP-12-IC1` / `CEN-Ib1` / `CEN-Ib11` / `INV-CEP-05-IC1`）

## Requirements

### Requirement: cep-ssp-event-alphabet

SSP 声明面的**事件** SHALL 引用**封闭的 14 核心语义锚集**（`include/tlm/cep/event_alphabet.hh`），或使用**受控私有事件**；⛔ **不得**发明未登记的锚。

#### Scenario: 锚集封闭且逐字

- **WHEN** 校验 `event_alphabet.hh`
- **THEN** `kCoreAnchorCount` SHALL == **14**，且 `kCoreAnchors` SHALL 恰 **14** 行
- **AND** 14 个锚名 SHALL **逐字**为 `ReadShared` `ReadUnique` `Write` `WriteNoAlloc` `Upgrade` `SnoopShared` `SnoopUnique` `SnoopInvalidate` `EvictDirty` `EvictClean` `Cap` `Grow` `Prune` `Report`
- **AND** ⛔ 新增锚 SHALL 改动 `kCoreAnchorCount` 常量（⇒ 触发判据复核 + architect 裁决）

#### Scenario: 私有事件须受控

- **WHEN** 一个事件**无**核心锚
- **THEN** 它 SHALL 声明 `flow` ∈ canonical 10 流 **且** `architect_approval` 非空
- **AND** ⛔ 私有事件 SHALL NOT 引用 / 组合其他私有事件（**原子性**）
- **AND** 「无等价锚」的占比 SHALL 可观测：> 半数 ⇒ 触发**核心锚集复审**

#### Scenario: flow 映射是部分的

- **WHEN** 校验锚的 `flow`
- **THEN** **非空** flow SHALL ∈ canonical 10 流（与 `kCanonicalFlowNames` 逐字一致）
- **AND** `Cap` / `Grow` / `Prune` / `Report` 的 `flow` SHALL 为 `nullptr`（本地权限转移 ⇒ 无流）
- **AND** ⛔ 该部分性 SHALL NOT 被表述为「flow 映射缺失」

### Requirement: cep-ssp-declaration

SSP 声明面 SHALL 以**独立数据源**（`ssp.schema.json` + `ssps.json`）存在，⛔ **不**内嵌进实例声明面；其格形状 SHALL 支持**两形态**——**单目标**（`0xFF` 表空洞）与**掩码**（`target_mask`）。

#### Scenario: 独立数据源与 schema 成对

- **WHEN** 校验声明面
- **THEN** `include/tlm/cep/ssp.schema.json` 与 `include/tlm/cep/ssps.json` SHALL **成对**存在（`AGENTS.md` 铁律 3）
- **AND** `protocol_instance.schema.json` 的 `properties` SHALL 仍 == **14**（`test/test_cep_schema.cc:443` 既有断言不变）
- **AND** SSP 数据文件 SHALL 是协议名的**唯一允许面**（第四类 · `ADR-CEP-07A` SU 2026-11-26）；通用层代码 SHALL 零协议名

#### Scenario: 逐格对账（V-24-2 killer）

- **WHEN** 校验 `ssps.json` 的 `mesif_ssp`（**单目标形态**）与**掩码形态**的 SSP（TileLink 形）
- **THEN** 其 `transition` / `perm` / `basis_kind` / `basis_pending` / `basis_projection_*` SHALL **逐格**等于 `mesif_state_spec.hh` 的 `kTable`（含 `0xFF` 空洞 ⇒ `null`）
- **AND** **掩码形态**的 `transition` SHALL 承载 `target_mask`（位掩码 · bit i ↔ `states[i]`（**声明顺序**）· 位宽上限 == `states` 数 ⇒ TileLink 形态下 == **3** 位 · 对齐 `tilelink_state_spec.hh:131-139` 的既有实现）⇒ 其 `target_mask` SHALL **逐位**等于手写面的对应掩码（源不定、目标为集合的 `Cap` 形）
- **AND** 该对账 SHALL 为**双向**：JSON 侧或手写侧**任改一格** ⇒ 必红

#### Scenario: 声明面不表达受限维度

- **WHEN** 校验 schema
- **THEN** 它 SHALL NOT 出现 `belief` / `token` / `transient` / `atomic` 字段（`ADR-CEP-23` 决策 2 · `ADR-CEP-17` 决策 3/5 · `ADR-CEP-05` `INV-CEP-05-IC1`）
- **AND** `perm` SHALL 仅取 `none` / `read` / `unique` / `writeback`（4 值锁 · `CEN-Ib11`）
### Requirement: cep-ssp-loader

SSP loader SHALL 为 **fail-closed**，且**只保「形态 + 引用闭环」**，⛔ **不承诺语义**。

#### Scenario: 五类错误按正确原因红

- **WHEN** 输入含 ① schema 字面量错 ② id 形态非法/重复 ③ state 空/重名 ④ event 空/重名 ⑤ transition 维度或目标 err ⑥ anchor 非核心锚 / 私有事件缺批准 ⑦ flow 非 10 流 ⑧ perm 越出 4 值 ⑨ basis_kind 非法 ⑩ basis 维度不符
- **THEN** loader SHALL 返回**非空红因码**（`E1`~`E14`）并带人读细节
- **AND** ⛔ 「`rc ≠ 0`」SHALL NOT 构成红因（须命中**具体红因码**）

#### Scenario: 措辞禁令

- **WHEN** 校验声明面 / loader / 本 spec
- **THEN** ⛔ SHALL NOT 出现任何「保证无…」型断言（死锁 / 违例 / 兼容 三类均禁；继承 `ADR-CEP-13` 决策 5/6）

#### Scenario: 掩码格的两形态

- **WHEN** 校验 `transition` 的单元形态
- **THEN** 单元 SHALL 接受**三形态**：**单目标**（state 名）· **空洞**（`null`）· **掩码**（`{target_mask}`）
- **AND** **掩码**单元 SHALL 只含 `target_mask` 一个键 · 其值 SHALL 为整数且 **位宽上限 == `states` 数**（bit i ↔ `states[i]`）
- **AND** 违反上述形态 ⇒ loader SHALL 判红且红因码为 **`E14`**
