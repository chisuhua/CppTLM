# design —— 接入 RefKind + deferral 机器可查登记

## D-1 「类型标签统一 / 清单自治」(用户裁决:甲)

- `RefKind` = **跨载体统一的类型标签**(3 档):`spec_clause` / `public_nonnormative` / `repo_derived`;
- 各载体的**来源清单自治**:本地 `SourceId` + 逐来源许可表(`kSourceLicenseConfirmed[]`)⇒ **许可信息仍在本地**;
- ⇒ 全局 `kSources[]` 与本地 `SourceId` **不是同一真源**;跨载体**同名来源**是否同一来源 **需人工确认**(本轮两族无同名冲突)。

## D-2 映射规则(两族恒不出现 `spec_clause`)

| 族 | 公开来源 | 其它 |
|---|---|---|
| `cxl_dcoh` | `S2`(FMS 2023 Tutorial)· `S4`(ACM CSUR 2024)· `S5`(ASPLOS'25)· `S6`(Hot Chips 2022)· `S7`(HCDS 2024)⇒ `public_nonnormative` | `Derived` ⇒ `repo_derived` |
| `nvlink_sharp` | `S1`(arXiv 2507.04786v3)· `S2`(NVIDIA Blog)· `S3`(SHARP UM)⇒ `public_nonnormative` | `AdrContract`(仓内已签 ADR)· `Derived` ⇒ `repo_derived` |

⇒ 两族**恒无** `spec_clause`(CXL 规范正文禁读 / NVLink 无条款级公开规范)⇒ **不计入 B3b**,本条**不**改 B3b-2 的 0/4。

## D-3 deferral 登记面

`test/cep_b3b2_deferrals.json`(4 族 × `{family, disposition, blocking_type, reason, trigger_condition, ledger_entry, internal_predecessor}`)+ 闸脚本逐条校验:
① 族名单一致 ② 字段齐备 ③ `ledger_entry` 在台账中可解析 ④ 与 `07 号` SU · 六 的族名单一致。
⚠️ 登记面**只**校验「登记完整」,⛔ **不**改计数 ⇒ 「登记 ≠ 达成」(per SU · 六 / Entry #264)。
