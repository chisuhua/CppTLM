# cep-refkind-coverage Specification (delta)

## ADDED Requirements

### Requirement: cep-refkind-coverage

CEP 的**来源类型标签**(`RefKind`)SHALL 覆盖全部**有逐格 `basis`** 的状态载体。`RefKind` SHALL 只作**跨载体统一的类型标签**;各载体的**来源清单 SHALL 自治**(本地 `SourceId` + 逐来源许可表)⇒ 全局注册表与本地清单**不是同一真源**。

载体 SHALL 提供 `ref_kind_of(basis)` 与 `basis_ref_kind_consistent(basis)`,且后者 SHALL 具备**鉴别力**:把 `PublicDirect` 的格指向**非公开来源**(`Derived` / 仓内 ADR 锚点)SHALL 判红。

⚠️ 接入 `RefKind` **不**改变任何载体的 `basis` 取值 / `pending` / 来源枚举 / 许可表(⛔ 零追溯降级);⚠️ 由**公开非规范来源**支撑的族 SHALL **恒不**出现 `spec_clause` ⇒ **不计入 B3b**(per `07 号` SU · 四 D-1)。

#### Scenario: RKC-01 —— 逐格一致性

- **WHEN** 遍历 `cxl_dcoh` 与 `nvlink_sharp` 的**全部** `basis` 表
- **THEN** 每格 `basis_ref_kind_consistent()` SHALL 为真;公开来源 ⇒ `public_nonnormative`;`Derived` / 仓内 ADR 锚点 ⇒ `repo_derived`

#### Scenario: RKC-02 —— 负控:非公开来源冒充 `PublicDirect` ⇒ 必红

- **WHEN** 构造 `PublicDirect` 且 `source` 为 `Derived`(cxl_dcoh)/ `AdrContract`(nvlink_sharp)的格
- **THEN** `basis_ref_kind_consistent()` SHALL 为**假**;同一格改为公开来源 ⇒ SHALL 为**真**(鉴别力)

#### Scenario: RKC-03 —— deferral 的机器可查登记

- **WHEN** 跑 deferral 登记闸
- **THEN** 4 族(`cxl_2_0` / `cxl_3_0_symmetric` / `nvlink` / `ualink`)SHALL **各有一条**登记,每条 SHALL 含 `disposition` / `blocking_type` / `reason` / `trigger_condition` / `ledger_entry`
- **AND** 每条 `ledger_entry` SHALL 在台账中**可解析**
- **AND** ⚠️ 本登记 SHALL **不**被读作「已达成」⇒ B3b-2 计数仍为 **0/4**
