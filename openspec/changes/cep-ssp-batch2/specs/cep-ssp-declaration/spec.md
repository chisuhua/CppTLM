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
- **AND** **掩码形态**的 `transition` SHALL 承载 `target_mask`（3 位掩码 · 对齐 `tilelink_state_spec.hh:131-139` 的既有实现）⇒ 其 `target_mask` SHALL **逐位**等于手写面的对应掩码（源不定、目标为集合的 `Cap` 形）
- **AND** 该对账 SHALL 为**双向**：JSON 侧或手写侧**任改一格** ⇒ 必红

#### Scenario: 声明面不表达受限维度

- **WHEN** 校验 schema
- **THEN** 它 SHALL NOT 出现 `belief` / `token` / `transient` / `atomic` 字段（`ADR-CEP-23` 决策 2 · `ADR-CEP-17` 决策 3/5 · `ADR-CEP-05` `INV-CEP-05-IC1`）
- **AND** `perm` SHALL 仅取 `none` / `read` / `unique` / `writeback`（4 值锁 · `CEN-Ib11`）
