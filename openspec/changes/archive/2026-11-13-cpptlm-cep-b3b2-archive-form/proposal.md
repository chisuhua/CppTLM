# cpptlm-cep-b3b2-archive-form: B3b-2 的归档形态(MUST-FIX-3)+ 回滚路径(MUST-FIX-4)

> **状态**: 🟡 **Proposed**(2026-11-12 立项 —— per 台账 **Entry #384** 授权;KPI 并发 **0/3 → 1/3**)
> **立项依据**: `.rddf/improvements/cpptlm-cep-b3b2-archive-form.md`(5 段 + 8 条 AC)· architect 决策 `.rddf/reviews/cep-mustfix-3-4-archive-form-decision.md` · 用户拍板 2 点(2026-11-12)
> **⛔ 范围纪律**:只做**形态定义**;⛔ **不执行归档**(终局未触发)· ⛔ MUST-FIX-1/2 已由 C4 落地(不重做)
> **估时**: ≈1-2 人日

## Why

**终局 Ⅰ / Ⅳ 都依赖归档形态定义** ⇒ 未定义前终局 Ⅳ **不可执行**;而终局 Ⅱ 的复审是**每季度**周期性 ⇒ 再不定义就会在某次复审中被迫临场决定。

| 缺口 | 实测 | 后果 |
|---|---|---|
| 闸的台账路径硬编码 | `cep_b3b2_deferral_check.sh:20` `LEDGER="…obj-cep-platform.md"` | 归档一旦移动/改名 ⇒ 闸**立即失败** ⇒ 归档被一个路径字面量锁死 |
| 锚解析是全文子串 | `le not in ledger` ⇒ 任意位置出现即命中 | 引述也会命中 ⇒ 与 C4 链的「整行全等」纪律不一致 ⇒ 假绿面 |
| 回滚路径未定义 | Entry #332 ⑧ 已指出「不得靠删除」但未给形态 | 要回滚时无处落笔 ⇒ 倾向删段落 ⇒ 破坏 append-only |
| ⚠️ 实测纠正 | MUST-FIX-3 ⑥ 的载体**已存在**(`07 号` §12 已是入口指针) | ⇒ ⑥ 无需新建载体,只追加一句单向指针 |

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **闸升级** | `scripts/test/cep_b3b2_deferral_check.sh` | `LEDGER` 可配置 + 重定向表多根;锚匹配 ⇒ **行首标题匹配** |
| 2 | **重定向表**(新) | `test/cep_b3b2_ledger_redirect.json` | `{"roots": ["<台账现路径>"]}` ⇒ 将来移动只改表不改代码 |
| 3 | **`07 号` 单向指针** | `docs/architecture/07-coherence-exploration-platform.md` §12 | 追加一句;⛔ 不复写规则;**冻结件 ⇒ 五步链** |
| 4 | **归档态段** | `.rddf/roadmap/objectives/obj-cep-platform.md` | **追加** `## §14` 段(§14.1~§14.7)⇒ ⛔ **不是「归档已发生」** |
| 5 | **新闸** | `scripts/test/cep_b3b2_archive_form_check.sh` | V1~V6 + 2 条决定性变异;ctest **174 → 175**;守门表 **35 → 36** |

**Out of Scope(⛔ 硬边界)**:⛔ **执行归档**(终局未触发)· ⛔ 改 `B3b-2` 计数(仍 **0/4**)· ⛔ 改 deferrals JSON 内容 · ⛔ 回改任何既有 Entry / 既有 SU · ⛔ 动 `§4.0` 排期值 · ⛔ 新建 ADR

## 验收标准(Acceptance Criteria)

> **统一纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1**:台账 `## §14` 段存在且 **§14.1~§14.7 七项**齐备 ⇒ 删任一项 ⇒ 新闸红
- [ ] **AC-2**:闸的锚解析 = **行首标题匹配** ⇒ 变异「改回全文子串」⇒ 新闸红(签名 `锚解析降级`)
- [ ] **AC-3**:`test/cep_b3b2_ledger_redirect.json` 存在且 `roots[0]` = 台账现路径 ⇒ 改错 ⇒ 新闸红
- [ ] **AC-4**:`07 号` §12 含维护态判定规则的**单向指针** ⇒ 删句 ⇒ 新闸红;⛔ 不复写规则
- [ ] **AC-5**:回滚形态**成对可判**(被撤回 SU 标识 + 撤回 Entry 号)+ 触发条件 3 条齐备 ⇒ 只留其一 ⇒ 新闸红
- [ ] **AC-6**:破坏 deferrals 的 `ledger_entry` ⇒ 闸红(签名含 `ledger_entry`)⇒ 还原 ⇒ 绿;且**既有 4 条仍绿**
- [ ] **AC-7**:`ctest` **175/175** · `cep_*.sh` **41/41** rc=0 · format / docs_sync / doc_ref / lint / freeze 全 rc=0 · 冻结链 `authorized_by` = 本 change 的 builder Entry · 无参复验 rc=0
- [ ] **AC-8**:⛔ **未执行归档**(`## §14` 段不含「已归档」表述)· ⛔ `B3b-2` 仍 **0/4** · ⛔ deferrals JSON **零 diff** · ⛔ 无 ADR 变更 · ⛔ `§4.0` 未动

## 成本收益

**消「终局 Ⅳ 不可执行」+「闸被路径字面量锁死」+「引述也可命中锚」vs 1 张重定向表 + 1 处闸升级 + 1 句 `07 号` 指针 + 1 段落库 + 1 个闸**。

## 诚实边界(⛔ 不可约残余)

1. `## §14` 段是**形态定义**,⛔ **不是归档事件**;
2. 终局 Ⅱ 的季度复审节奏**不变**(定义 ≠ 触发);
3. 回滚触发条件含「用户 / 实测」⇒ ⛔ agent 不得自行宣告回滚已执行。

## 关联

台账 **#384**(立项)· **#332 ⑧**(MUST-FIX 原文 + 终局三分支)· **#264** · architect 决策 `.rddf/reviews/cep-mustfix-3-4-archive-form-decision.md` · `.rddf/improvements/cpptlm-cep-b3b2-archive-form.md`
