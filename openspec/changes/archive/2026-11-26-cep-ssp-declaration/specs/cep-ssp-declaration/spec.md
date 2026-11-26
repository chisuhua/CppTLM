## ADDED Requirements

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

SSP 声明面 SHALL 以**独立数据源**（`ssp.schema.json` + `ssps.json`）存在，⛔ **不**内嵌进实例声明面。

#### Scenario: 独立数据源与 schema 成对

- **WHEN** 校验声明面
- **THEN** `include/tlm/cep/ssp.schema.json` 与 `include/tlm/cep/ssps.json` SHALL **成对**存在（`AGENTS.md` 铁律 3）
- **AND** `protocol_instance.schema.json` 的 `properties` SHALL 仍 == **14**（`test/test_cep_schema.cc:443` 既有断言不变）
- **AND** SSP 数据文件 SHALL 是协议名的**唯一允许面**（第四类 · `ADR-CEP-07A` SU 2026-11-26）；通用层代码 SHALL 零协议名

#### Scenario: 逐格对账（V-24-2 killer）

- **WHEN** 校验 `ssps.json` 的 `mesif_ssp`
- **THEN** 其 `transition` / `perm` / `basis_kind` / `basis_pending` / `basis_projection_*` SHALL **逐格**等于 `mesif_state_spec.hh` 的 `kTable`（含 `0xFF` 空洞 ⇒ `null`）
- **AND** 该对账 SHALL 为**双向**：JSON 侧或手写侧**任改一格** ⇒ 必红

#### Scenario: 声明面不表达受限维度

- **WHEN** 校验 schema
- **THEN** 它 SHALL NOT 出现 `belief` / `token` / `transient` / `atomic` 字段（`ADR-CEP-23` 决策 2 · `ADR-CEP-17` 决策 3/5 · `ADR-CEP-05` `INV-CEP-05-IC1`）
- **AND** `perm` SHALL 仅取 `none` / `read` / `unique` / `writeback`（4 值锁 · `CEN-Ib11`）

### Requirement: cep-ssp-loader

SSP loader SHALL 为 **fail-closed**，且**只保「形态 + 引用闭环」**，⛔ **不承诺语义**。

#### Scenario: 五类错误按正确原因红

- **WHEN** 输入含 ① schema 字面量错 ② id 形态非法/重复 ③ state 空/重名 ④ event 空/重名 ⑤ transition 维度或目标 err ⑥ anchor 非核心锚 / 私有事件缺批准 ⑦ flow 非 10 流 ⑧ perm 越出 4 值 ⑨ basis_kind 非法 ⑩ basis 维度不符
- **THEN** loader SHALL 返回**非空红因码**（`E1`~`E12`）并带人读细节
- **AND** ⛔ 「`rc ≠ 0`」SHALL NOT 构成红因（须命中**具体红因码**）

#### Scenario: 措辞禁令

- **WHEN** 校验声明面 / loader / 本 spec
- **THEN** ⛔ SHALL NOT 出现任何「保证无…」型断言（死锁 / 违例 / 兼容 三类均禁；继承 `ADR-CEP-13` 决策 5/6）
