# cep-basis-source-kind Specification

## Purpose
TBD - created by archiving change cpptlm-cep-basis-source-kind-and-cell-free. Update Purpose after archive.

## Requirements

### Requirement: cep-basis-source-kind

CEP 的 4 个状态载体(`tilelink` / `cxl` / `mesif` / `ualink_inc`)SHALL 使每一格的证据来源类型**可机器判定**,且 `basis` 与来源类型的一致性 SHALL 由载体自身提供的 `basis_ref_kind_consistent()` 断言;来源类型 SHALL 由**单一注册表**(`sources.json` + `kSources[]`,逐条一致)解析,⛔ 不得按枚举 token 名跨载体套用。

`CellKind::Free` SHALL 表示「规范**明确**实现定义 / 自由选择」,与 `Known`(明确转移)、`Unknown`(未覆盖)、`Illegal`(明确不合法)互斥;消费端 SHALL 提供显式分支,⛔ 不得把 `Free` 归入 `Known` 或 `Unknown`。

#### Scenario: SRK-01 —— 逐格来源可解析

- **WHEN** 遍历 4 载体的每一格
- **THEN** 非空 `ref` SHALL 解析到 `kSources[]` 中已登记条目(未登记计数 = 0);空 `ref` SHALL 解析为 `repo_derived`

#### Scenario: SRK-02 —— 按载体映射零违规

- **WHEN** 对每格调用 `basis_ref_kind_consistent()`
- **THEN** 全部为真 —— tilelink `Direct` ⇒ `spec_clause`;cxl `PublicDirect` ⇒ `public_nonnormative`(**不是** `spec_clause`);mesif `Primary` ⇒ `repo_derived` 且 `ref` 非空;ualink `SelfDerived` ⇒ `repo_derived` / `Pending` ⇒ `public_nonnormative`

#### Scenario: SRK-03 —— 注册表双载体一致

- **WHEN** 比对 `sources.json` 与 `kSources[]`
- **THEN** id / kind / title 逐条一致(条数与顺序一致);漂移 ⇒ 测试红

#### Scenario: SRK-04 —— `CellKind::Free` 四态互斥

- **WHEN** 统计 tilelink / cxl 各格 `CellKind`
- **THEN** 各态计数自洽(和 = 总格数);`Free` 当前 = 0;`transition_is_free()` 对全表返回 false,且 `Free` **不**使 `transition_is_known()` / `transition_is_unknown()` 为真(合成枚举值测试)

#### Scenario: SRK-05 —— 决定性变异

- **WHEN** 影子树执行三条变异(删注册表条目 / 弱来源配 `Direct` / `Free` 误当 `Unknown`)
- **THEN** 各自按**预期签名**红(缺 id / 映射不一致 / 四态断言失败),真源 sha256 未变
