# cep-refkind-coverage Specification

## Purpose
TBD - created by archiving change cpptlm-cep-dcoh-nvlink-refkind-migration. Update Purpose after archive.

## Requirements

### Requirement: cep-refkind-coverage

CEP 的**来源类型标签**(`RefKind`)SHALL 覆盖全部**有逐格 `basis`** 的状态载体。`RefKind` SHALL 只作**跨载体统一的类型标签**;各载体的**来源清单 SHALL 自治**(本地 `SourceId` + 逐来源许可表)⇒ 全局注册表与本地清单**不是同一真源**。

载体 SHALL 提供 `ref_kind_of(basis)` 与 `basis_ref_kind_consistent(basis)`,且后者 SHALL 具备**鉴别力**:把 `PublicDirect` 的格指向**非公开来源**(`Derived` / 仓内 ADR 锚点)SHALL 判红。

⚠️ 接入 `RefKind` **不**改变任何载体的 `basis` 取值 / `pending` / 来源枚举 / 许可表(⛔ 零追溯降级);⚠️ 由**公开非规范来源**支撑的族 SHALL **恒不**出现 `spec_clause` ⇒ **不计入 B3b**(per `07 号` SU · 四 D-1)。

#### 载体自足性的**唯一白名单**(准入条件与登记)

载体接入 `RefKind` 时**需要**引用来源类型注册表 ⇒ 与「载体自足性」判据(禁引号形式 include 项目内头)存在**结构性张力**。本 Requirement SHALL 登记该张力的**唯一豁免面**:

- **唯一白名单成员** = `"tlm/cep/source_registry.hh"`(纯 `constexpr` / `inline constexpr` 的 `RefKind` 三档类型标签 + 全局来源注册表);
- **准入条件**(SHALL **全部**满足才可进入白名单):① **纯 `constexpr` / `inline constexpr`**(无运行期状态、无构造副作用);② **只依赖标准库**(⛔ 不 include 任何项目内头);③ **不引入**平台标识 / 区域表 / 转移判定 / 协议语义(⇒ 不破坏「载体自足」的实质);
- ⚠️ 白名单集合 SHALL **单一真源** = 本 Requirement ⇒ 各载体的自足性探针 SHALL **不自行扩张**该集合;探针的**实现形态** MAY 不同(如「只允许这一个」/「跳过这一个」),但**成员集合** SHALL 与本 Requirement **一致**;
- ⚠️ 载体自足性探针与**主测试** SHALL **同步**修改(否则二者漂移);
- ⚠️ 契约面 SHALL **不**以研究文档 / 台账的推断编号作为该机制的唯一锚(表述 SHALL 自解释)。

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

#### Scenario: RKC-04 —— 白名单**唯一**且**三处一致**

- **WHEN** 扫描 `ualink_inc` 的自足性探针(UIS-10)与 `nvlink_sharp` 的自足性探针(CP-05)源码中形如 `"tlm/cep/*.hh"` 的引号路径
- **THEN** 每处探针的该路径集合 SHALL **恰好** == `{ "tlm/cep/source_registry.hh" }`(多一个 ⇒ 必红)
- **AND** 契约面 `cep-refkind-coverage` SHALL 登记**同一**白名单路径与**准入条件**(三处一致)
