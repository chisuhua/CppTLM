# cpptlm-cep-source-kind-contract-alignment: 来源判据的**契约对齐** + 第四变异态

> **状态**: 🟡 **Proposed**(2026-11-06 立项 —— 用户裁决「按建议顺序实施」,per 台账 Entry #255;KPI 并发 **0/3 → 1/3**)
> **立项依据**: P1 change(`cpptlm-cep-basis-source-kind-and-cell-free`)验收留下的 **2 条 SHOULD_FIX**(报告 `.rddf/reviews/cep-source-kind-verify.md` §三):
> ① **AC-4③ 未进脚本** —— `Free` 误当 `Unknown` 的决定性变异只在验收时影子树补做,CI 无自动化有牙载体;
> ② **`UIS-10` 白名单未进契约** —— 「载体只 include 标准库」的白名单已同步主测试与反向验证探针,但 **main spec 根本没写这条约束** ⇒ 属**契约缺项**(非漂移)
> **关联 change**: `cpptlm-cep-basis-source-kind-and-cell-free`(**已归档**,✅ 已完成归档语义)
> **冻结面**: **零 diff**(只改 `scripts/test/` + 新建 change + `openspec/specs/` 合并面)
> **估时**: ~1.5d(实际实现 < 2h)

## Why

P1 把「来源效力」从人工抽样变成可机器判,但**判据的自动化覆盖与契约登记**留了两处尾巴:

- 判据 ③(`Free` 误当 `Unknown`)只在**验收时**用影子树补做 ⇒ CI 里**没有**这条有牙载体 ⇒ 后续若有人把 `transition_is_free` 改成 `kind == Unknown`,**常规回归不会变红**;
- `ualink_inc` 载体的**自足约束**只活在**测试**里,main spec 无对应条款 ⇒ 契约与实现面脱节 ⇒ 未来读 spec 的人不知道「白名单 1 条」是**设计**还是**漏洞**。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | 闸脚本**第四态** `misclassified`(影子头把 `transition_is_free` 改判 `Unknown` ⇒ 签名 `FREE-KIND mismatch`) | `scripts/test/cep_source_effect_check.sh` + probe(扩展 `Free` ⇔ `kind` 一致性检查) |
| 2 | **契约登记**:新增 requirement `cep-ualink-inc-state-spec-self-containment`(自足 + 唯一白名单 + 鉴别力 + 反向 fixture) | delta spec → 归档时合并进 `openspec/specs/cep-ualink-inc-state-spec/spec.md` |

**Out of Scope**: ⛔ 不改任何载体实现;⛔ 不改主测试与反向验证探针的**已生效**白名单逻辑(仅本 change 的脚本/probe 扩展);⛔ 不改 M1 冻结面。

## Impact

- **闸**: `cep_source_effect_check` 由 3 态扩为 **4 态**(8 PASS / 0 FAIL);
- **契约**: main spec 新增 1 条 requirement(归档时合并)⇒ 消除「测试放宽、契约缺项」;
- **测试**: 0 回归(既有 `[cep]` / `ctest` 不受影响);
- **KPI**: 占 1 slot(归档后回 0/3)。

## Acceptance

- [ ] **AC-1** 闸脚本第四态落地:`misclassified` 按签名 `FREE-KIND mismatch` 红,真源 sha256 未变(3 个文件)
- [ ] **AC-2** 既有三态**不回归**(clean / dropped / downgraded 仍按原签名红)
- [ ] **AC-3** delta spec 含 `cep-ualink-inc-state-spec-self-containment` requirement + UIS-12 场景;`openspec validate` 通过
- [ ] **AC-4** 0 回归:`ctest --test-dir build -j4` 全绿 + 4 个 CEP 闸全绿 + M1 冻结面 41 产物零 diff
- [ ] **AC-5** 归档后 `openspec/specs/cep-ualink-inc-state-spec/spec.md` 含新 requirement;KPI 回填 0/3
