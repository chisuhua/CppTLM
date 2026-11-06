# 验收报告 · `cpptlm-cep-source-kind-contract-alignment`(A 第四变异态 + B 契约登记)

- **验收对象**:change `cpptlm-cep-source-kind-contract-alignment`(A+B 合并小 change)
- **验收角色**:verifier(2026-11-06,用户口令「按你建议的顺序实施」链内)
- **判据来源**:change `proposal.md` 的 **AC-1 ~ AC-5** + delta spec 的 UIS-12 场景
- **分流判定**:`.rddf/state/iteration.json` **不存在** ⇒ P3 archive gate 未触发 ⇒ ⛔ 不走 `rdd-verifier` skill,按本地 1~5 步验收
- **结论**:**PASS**(0 失败项 · 0 SHOULD_FIX · 已用 **1** 轮)

---

## 一、AC 逐条判定

| AC | 判定 | 证据 |
|---|---|---|
| **AC-1** 第四态 `misclassified` 按签名红 + 真源 sha256 未变 | ✅ | `bash scripts/test/cep_source_effect_check.sh` ⇒ `✅ misclassified 按签名变红: FREE-KIND mismatch s=0 e=3`;收尾 `header / json / cxl adapter sha256 未变`(3 文件) |
| **AC-2** 既有三态不回归 | ✅ | 同一次运行内 ② `missing in json: cxl-d1` · ③ `SRK-02 inconsistency: TLC-M-01` · ① `consistent: 8 sources` + `SRK clean` ⇒ 四态合计 **8 PASS / 0 FAIL** |
| **AC-3** delta spec 含 requirement + UIS-12 + validate 通过 | ✅ | `grep` 命中 `Requirement: cep-ualink-inc-state-spec-self-containment` + `Scenario: UIS-12`(计数 = 2);`openspec validate …` ⇒ **valid** |
| **AC-4** 0 回归 + 闸族 + 冻结面零 diff | ✅ | `ctest --test-dir build -j4` ⇒ **156/156**;`cep_docs_sync_check` / `cep_doc_ref_check` / `cep_decl_alignment_check` 全绿;`cep_freeze_check` ⇒ **41 产物零 diff** |
| **AC-5** 归档后 main spec 含新 requirement + KPI 回填 0/3 | ⏳ **待归档后复验** | 归档属 builder 动作 ⇒ 本报告在**归档后**补验(见 §四) |

## 二、变更面核对(与 proposal 的 What Changes 对齐)

| 变更 | 落点 | 核对 |
|---|---|---|
| 第四态 | `scripts/test/cep_source_effect_check.sh` + probe | ✅ 影子树改为复制整个 `include/tlm/cep/`;probe 增「`Free` ⇔ `transition_is_free` 逐格一致」;真源校验扩到 3 文件 |
| 契约登记 | delta spec → 合并进 `openspec/specs/cep-ualink-inc-state-spec/spec.md` | ✅ ADDED requirement(非 MODIFIED)—— 与 design D-1 一致 |

**零改动面核对**:载体实现 **0 改动**(`git show --stat` 仅脚本/probe/change/台账)· 主测试与反向验证探针的**已生效**白名单逻辑 **0 改动** · M1 冻结面 **0 diff**。

## 三、遗留清扫(本 change 范围)

| 检查项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK | **0** |
| 临时调试日志 | **0**(probe 的 `printf` 属**签名取证机制**) |
| 被 skip / disabled 的测试 | **0** |
| 注释掉的代码块 | **0** |
| 未替换的 mock / 临时 fallback | **0** |

## 四、失败项与路由

**无失败项** ⇒ ⛔ 无需回 builder / planner。

## 五、已用轮次

第 **1** 轮(2026-11-06)⇒ **PASS**;未触发重试上限(3)。

## 六、归档后复验(2026-11-06 归档后补验 —— 全部 ✅)

- [x] `openspec/specs/cep-ualink-inc-state-spec/spec.md:143` 含 `### Requirement: cep-ualink-inc-state-spec-self-containment`,`:149` 含 `#### Scenario: UIS-12`(归档合并 **+1 requirement**)
- [x] KPI 回填 **1/3 → 0/3**(5 处活文档:`obj §5.2` · `phase-10` · `umbrella` · `nvlink-sharp`(2 处)· `roadmap.md`);`openspec list --json` = **0 active**
- [x] `cep_doc_ref_check` 的 VIRTUAL_PATHS 已登记 `openspec/changes/cpptlm-cep-source-kind-contract-alignment`(旧 change 路径引用不断链)

⇒ **AC-5 PASS** ⇒ 本 change 验收 **PASS**(0 失败项 · 0 SHOULD_FIX · 1 轮)。归档回执见台账 **Entry #256**。
