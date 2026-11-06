# cpptlm-cep-dcoh-nvlink-refkind-migration: `cxl_dcoh` / `nvlink_sharp` 接入 `RefKind` + B3b-2 deferral 机器可查登记

> **状态**: 🟡 **Proposed**(2026-11-07 立项 —— 用户裁决「双真源 = **甲(类型标签统一 / 清单自治)**」,per 台账 Entry #266;KPI 并发 **0/3 → 1/3**)
> **立项依据**:`07 号` SU 2026-11-06 · 五 §② 的**载体覆盖账**缺口(2 族未接入 ⇒ 本闸**不保护** B3b-2 的 CXL 3.0 / NVLink 升级路径)+ Entry #264 §⑤ 的强度缺口(4 族 ⏭ 仅台账文字登记)
> **关联**:`.rddf/improvements/cpptlm-cep-dcoh-nvlink-refkind-migration.md`
> **估时**: ~1d

## Why

- **覆盖账**:SU · 五 明确登记「`cxl_dcoh` / `nvlink_sharp` 未接入 `RefKind` ⇒ 本闸不保护这两族的升级路径」——而它们恰是 **B3b-2 的 CXL 3.0 / NVLink**;
- **强度缺口**:`07 号` SU · 四 §⑤ 已登记「4 族 ⏭ 只有台账文字登记,弱于 B2 的机器校验」⇒ 本条补机器可查登记面(与 SU · 六 的「挂起」裁决配套)。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | 两族接入 `RefKind`(类型标签):`ref_kind_of` + `basis_ref_kind_consistent`(含负控语义) | `include/tlm/cep/cxl_dcoh_state_spec.hh` · `include/tlm/cep/nvlink_sharp_state_spec.hh` |
| 2 | **零追溯降级**:`basis` / `pending` / `SourceId` / 许可表逐字未动 | 同上(无字段变更) |
| 3 | 判据 | `test/test_cep_refkind_coverage.cc`(新,5 cases / 32 assertions) |
| 4 | **deferral 机器可查登记** | `test/cep_b3b2_deferrals.json`(新,4 族)+ `scripts/test/cep_b3b2_deferral_check.sh`(新闸) |
| 5 | 契约 | delta spec → `openspec/specs/cep-refkind-coverage/spec.md` |

**Out of Scope**: ⛔ 不改 B3b-2 计数(仍 0/4);⛔ 不把任一格判为 `spec_clause`;⛔ 不扩全局 `kSources[]`(清单自治);⛔ 不改引擎/adapter/JSON 声明。

## Acceptance

- [ ] **AC-1** 两族全部 `basis` 表逐格一致(RKC-01)
- [ ] **AC-2** 负控有牙:`PublicDirect` + `Derived`/`AdrContract` ⇒ 红(RKC-02)
- [ ] **AC-3** 零追溯降级:许可表 / 来源枚举 / `basis` 取值未变(RKC-05)
- [ ] **AC-4** deferral 登记 4 族齐全 + 字段齐备 + 台账凭据可解析(RKC-03)
- [ ] **AC-5** 0 回归:`ctest` 全绿 + 4 闸全绿 + 冻结面零 diff
- [ ] **AC-6** 台账 Entry #266 + 归档 + KPI 回填 0/3
