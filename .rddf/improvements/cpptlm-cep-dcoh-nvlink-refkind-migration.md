---
name: cpptlm-cep-dcoh-nvlink-refkind-migration
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-12
  - ADR-CEP-11
状态: 🟡 **提案**(2026-11-07,per 台账 Entry #263 —— 待 architect 裁决「来源清单双真源」+ oracle→critic 双审后实施)
created: 2026-11-07
---

# improvement: cpptlm-cep-dcoh-nvlink-refkind-migration

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

## 架构依据

`07 号` **SU 2026-11-06 · 五** §② 的**载体覆盖账**登记:`cxl_dcoh` 与 `nvlink_sharp` 使用**第三套 token 族** `EvidenceKind{PublicDirect, PublicAnalogical, SelfDerived}`,**未接入** `RefKind` ⇒ **本闸不保护这两族的升级路径**;而这两族恰是 **B3b-2 的 CXL 3.0 / NVLink**。

两族现状(实测):

| 族 | `EvidenceKind` | 来源机制 | `ref` 形态 |
|---|---|---|---|
| `cxl_dcoh_state_spec.hh` | `PublicDirect` / `PublicAnalogical` / `SelfDerived` | **本地 `SourceId`**(S2 FMS 2023 Tutorial · S4 ACM CSUR 2024 · S5 ASPLOS'25 · S6 Hot Chips 2022 · S7 HCDS 2024 · `Derived`)+ `kSourceLicenseConfirmed[]` 逐来源许可 | 条款定位(如 `§1 / S5`) |
| `nvlink_sharp_state_spec.hh` | 同上 | **本地 `SourceId`**(S1 arXiv 2507.04786v3 · S2 NVIDIA Blog · S3 SHARP UM Overview · **`AdrContract`**(本仓已签 ADR)· `Derived`)+ 许可表 | token 串(`C-n` / `A-n` / `N-n`) |

⇒ 两族**已有**「逐来源 + 许可人工确认」机制(比 `RefKind` 更细),缺的只是**跨载体统一的类型标签**。

## 范围

**In-Scope**:

1. 两族各加 `ref_kind_of(Basis)` + `basis_ref_kind_consistent(Basis)`,按**载体本地 `SourceId`** 映射:
   - `cxl_dcoh`:`S2/S4/S5/S6/S7` ⇒ `public_nonnormative`;`Derived` ⇒ `repo_derived`;
   - `nvlink`:`S1/S2/S3` ⇒ `public_nonnormative`;`AdrContract` / `Derived` ⇒ `repo_derived`;
   - 一致性:`PublicDirect` ⇒ 必须是 `public_nonnormative` **且 `source` 不是 `Derived`/`AdrContract`**(有牙);`PublicAnalogical` / `SelfDerived` ⇒ **≠ `spec_clause`**;
2. **判据**:逐格断言 0 违规 + 「`PublicDirect` 指向 `Derived`/`AdrContract` ⇒ 红」的负控;
3. **契约**:delta spec(新增 requirement `cep-refkind-coverage` —— 载体覆盖账从「4 已接入 / 1 N/A / 2 未接入」变为「6 已接入 / 1 N/A」)。

**Out of Scope(明确不做)**:

- ⛔ **不**改两族的 `basis` 取值、`pending`、`SourceId` 枚举、许可表(零追溯降级);
- ⛔ **不**把任一格判为 `spec_clause` ⇒ **两族仍不计入 B3b**(本条只为「升级路径」铺路,⛔ 不改 B3b-2 的 0/4);
- ⛔ **不**扩全局 `kSources[]` 注册表(见「待裁决问题」);
- ⛔ 不改引擎、adapter、JSON 声明。

## Why

- B3b-2 的 4 族要升级,前置是「该族的证据来源类型**可机器判**」⇒ 未接入的两族是**升级路径上的技术缺口**,不是可绕过的细节;
- `SU · 五` 已把「本闸不保护这两族的升级路径」写成显式缺口 ⇒ 现在补上,**覆盖账才能闭合**;
- 两族的来源**类型**天然是「公开非规范」(CXL 规范正文禁读 / NVLink 无条款级公开规范)⇒ 迁移后**必然**落在 `public_nonnormative`,与 D-1 的裁定一致 ⇒ **不会**因迁移而产生「冒充条款级」的风险。

## What Changes

| 变更 | 文件 |
|---|---|
| `ref_kind_of` + `basis_ref_kind_consistent` | `include/tlm/cep/cxl_dcoh_state_spec.hh` · `include/tlm/cep/nvlink_sharp_state_spec.hh` |
| 判据(逐格 0 违规 + 负控) | `test/test_cep_refkind_coverage.cc`(新) |
| 契约 | delta spec → `openspec/specs/cep-refkind-coverage/spec.md` |
| 文档 | `07 号` SU(覆盖账「2 未接入」→「已接入」)—— architect 域,冻结面两步授权 |

## Acceptance

- [ ] **AC-1** 两族每格的 `basis_ref_kind_consistent()` 均为真;`PublicDirect` 的格**无一**指向 `Derived`/`AdrContract`
- [ ] **AC-2** 负控有牙:把某 `PublicDirect` 格的 `source` 改为 `Derived`/`AdrContract` ⇒ 判红
- [ ] **AC-3** 零追溯降级:两族的 `basis` 取值 / `pending` / `SourceId` / 许可表**逐字未变**
- [ ] **AC-4** B3b 计数**不变**(B3b-2 仍 0/4)⇒ 判据显式断言该族仍**不**满足 `spec_clause`
- [ ] **AC-5** 0 回归:`ctest` 全绿 + 4 闸全绿 + 冻结面授权通过

## Capabilities

- **载体覆盖账闭合**:6 族接入 `RefKind`(CHI 仍 N/A —— opcode 表型)⇒ 「来源类型可机器判」覆盖 B3b-2 的 4 族中的 2 族;
- **升级路径就绪**:CXL 3.0 / NVLink 若日后获得**条款级**来源,只需新增 `spec_clause` 映射 + 逐格改判 ⇒ 已有机器判据兜底。

## Impact

- **公共头**:2 个载体头各加 2 个函数(零字段变更);
- **契约**:新增 1 份 main spec;
- **测试**:新增 1 个测试文件;
- **文档**:1 处 `07 号` SU(冻结面 ⇒ 两步授权);
- **KPI**:占 1 slot(归档后回 0/3);
- **⛔ 不改 B3b 计数**:B3b-2 仍 **0/4**。

## 待 architect 裁决的问题(实施前必须先裁)

**「来源清单双真源」**:全局 `kSources[]`(`source_registry.hh`)与载体本地 `SourceId` 两套来源清单并存。

| 选项 | 内容 | 代价 |
|---|---|---|
| **甲(建议)** | **分层**:各载体的**来源清单**自治(本地 `SourceId` 带许可)· `RefKind` 只作**跨载体统一的类型标签**;`resolve_ref` 把本地来源映射到 `RefKind` | 需在契约中写明「类型标签统一、清单自治」;跨载体**同名来源**(如两族都引 S5)仍需人工确认是否同一来源 |
| 乙 | 把两族的 `SourceId` 全量并入全局 `kSources[]` | 注册表膨胀(8 → ~19 条)· 跨载体同名来源需去重判定 · 许可字段双写 |
| 丙 | 保持现状(两族不接入) | 覆盖账缺口保留;B3b-2 升级路径无机器判据 |
