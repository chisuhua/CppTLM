## MODIFIED Requirements

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
