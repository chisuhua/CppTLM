# 验收报告 · `cpptlm-cep-dcoh-nvlink-refkind-migration`

- **验收对象**:change `cpptlm-cep-dcoh-nvlink-refkind-migration`(第 3 项 + 第 5 项 deferral 登记)
- **角色**:verifier(2026-11-07)
- **判据来源**:change `proposal.md` AC-1 ~ AC-6 + delta spec(RKC-01~03)
- **结论**:**PASS**(0 失败项 · 1 条 NIT · 1 轮)

## 一、AC 逐条判定

| AC | 判定 | 证据 |
|---|---|---|
| AC-1 两族全部 basis 表逐格一致 | ✅ | `test/test_cep_refkind_coverage.cc` RKC-01/02 遍历 **13 张表**(cxl_dcoh 7 张 + nvlink_sharp 6 张)⇒ 全真;`[cep][refkind]` 5 cases / 32 assertions 全 PASS |
| AC-2 负控有牙 | ✅ | RKC-04:`PublicDirect` + `Derived`(cxl)/ `AdrContract`(nvlink)⇒ **必红**;同格改公开来源 ⇒ **必绿**(鉴别力自检) |
| AC-3 零追溯降级 | ✅ | RKC-05:`kSourceCount`(6 / 5)与 `kSourceLicenseConfirmed[]` 逐项断言(cxl S5=CC-BY⇒true、S4 许可 absent⇒false;nvlink S1/S2/S3⇒false)⇒ 许可表未被改动;**零字段变更** |
| AC-4 deferral 登记完整 | ✅ | `scripts/test/cep_b3b2_deferral_check.sh` ⇒ **2 PASS / 0 FAIL**:4 族齐全 + 7 字段齐备 + `ledger_entry`(Entry #249)在台账**可解析** |
| AC-5 0 回归 | ✅ | `ctest --test-dir build -j4` ⇒ **158/158**;`[cep]` 467 cases / 10964 assertions;`docs_sync` / `doc_ref` / `decl_alignment` / `freeze(41 零 diff)` 全绿 |
| AC-6 台账 + 归档 + KPI 回填 | ✅ | 归档为 ✅ 已完成归档语义(delta 合并进 `openspec/specs/cep-refkind-coverage/spec.md`,**+1 requirement**);KPI **1/3 → 0/3**(5 处活文档),`openspec list --json` = **0 active**;Entry #267 已落 |

## 二、关键判据的**诚实边界**(验收确认)

- 两族**恒不出现** `spec_clause` ⇒ **B3b-2 仍 0/4**;本 change 只把「来源类型可机器判」的覆盖面从 4 族扩到 **6 族**,⛔ 不改任何族的计数;
- deferral 登记面**只**校验「登记完整」⇒「登记 ≠ 达成」,计数口径由 `07 号` SU · 六 / Entry #264 锁;
- 跨载体同名来源的同一性**需人工确认**(本轮两族无同名冲突)⇒ 已在契约中声明为「类型标签统一 / 清单自治」。

## 三、NIT(登记,不阻塞)

- `nvlink_sharp` 载体的自足性探针(CP-05)与主测试需加「唯一白名单 `source_registry.hh`」⇒ 已加(纯 constexpr + 只依赖标准库;先例 `ualink_inc` UIS-10,per Entry #261)⇒ 建议在 `cep-ualink-inc-state-spec` 之外的**契约面**同步登记该白名单(与 `cep-instance-assembly` 的 required 机制同一治理模式)。

## 四、失败项与路由

**无失败项** ⇒ ⛔ 无需回 builder / planner。

## 五、已用轮次

第 **1** 轮(2026-11-07)⇒ **PASS**。
