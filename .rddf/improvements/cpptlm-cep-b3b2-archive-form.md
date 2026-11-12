---
name: cpptlm-cep-b3b2-archive-form
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— B3b-2 归档形态(MUST-FIX-3)+ 回滚路径(MUST-FIX-4)
依赖:
  - ADR-CEP-00
状态: "🟡 待实施（2026-11-12 起草;per 台账 **Entry #383** 的 R-1 + architect 决策 `.rddf/reviews/cep-mustfix-3-4-archive-form-decision.md`(6 项定义 + 回滚路径 + V1~V7 判据);用户已拍板 2 个开放点(② 按终局分支自动选用 / ① 原地保留 + append-only 归档态段);⛔ 本 change **不执行归档**(终局未触发)—— 只定义形态)）"
created: 2026-11-12
---

# cpptlm-cep-b3b2-archive-form: B3b-2 的归档形态(MUST-FIX-3)与回滚路径(MUST-FIX-4)

> **范围纪律**:⛔ **不执行归档**(终局 Ⅰ/Ⅳ 均未触发)—— 本 change 只**定义形态**;⛔ 不改 `B3b-2` 计数(仍 **0/4**)· ⛔ 不改 deferrals JSON 的族名单与内容 · ⛔ 不回改任何既有 Entry · ⛔ 不动 `§4.0` 排期值 · ⛔ 不新建 ADR。
> **估时**:≈1-2 人日(闸升级 ≈0.5d · `07 号` 指针 + 冻结链 ≈0.5d · 归档态段 + 回滚形态 ≈0.3d · 新闸 + 回归 ≈0.5d)

## 架构依据

| 依据 | 内容 |
|---|---|
| 台账 **Entry #332 ⑧** | MUST-FIX-3(归档形态 **6 项**)+ MUST-FIX-4(回滚路径)原文 + 终局三分支 + ⑬「**不得先行指定归档语义**」 |
| 台账 **Entry #264** | 终态 DoD Go 挂起 + 季度复审节奏 + 「或用户另行裁决 DoD 结构」授权 |
| architect 决策 | `.rddf/reviews/cep-mustfix-3-4-archive-form-decision.md`(6 项逐项定义 + 回滚形态/触发/粒度/成对可判 + V1~V7) |
| **用户拍板**(2026-11-12) | ② **按终局分支自动选用**归档语义 · ① **原地保留 + append-only 归档态段** |

## 范围

| 面 | 内容 |
|---|---|
| ✅ 在范围内 | ① 闸 `cep_b3b2_deferral_check.sh` 的 `LEDGER` 可配置 + 重定向表多根 + **锚解析升级为行首标题匹配** ② `test/cep_b3b2_ledger_redirect.json`(新) ③ `07 号` §12 **单向指针**一句 ④ 台账 **追加** `## 归档态` 段(6 项定义 + 回滚路径 + 撤回标记形态) ⑤ 新闸 `cep_b3b2_archive_form_check.sh` + ctest 注册 + 守门表 |
| ⛔ 不在范围内 | ① **执行归档**(终局未触发)· ② 改 `B3b-2` 计数 · ③ 改 deferrals JSON 内容 · ④ 回改既有 Entry / 既有 SU · ⑤ 改 `07 号` 既有正文(只追加指针句)· ⑥ 动 `§4.0` 排期值 |

## Why

**终局 Ⅰ / Ⅳ 都依赖归档形态定义** ⇒ 未定义前终局 Ⅳ **不可执行**;而终局 Ⅱ 的复审是**每季度**周期性 ⇒ 再不定义就会在某次复审中被迫临场决定 ⇒ 正是本仓一直在治理的「临场决定」失败模式。

| 缺口 | 实测 | 后果 |
|---|---|---|
| **闸的台账路径硬编码** | `cep_b3b2_deferral_check.sh:20` `LEDGER=".rddf/roadmap/objectives/obj-cep-platform.md"` | 归档一旦移动/改名该文件 ⇒ 闸**立即失败** ⇒ 归档被一个路径字面量锁死 |
| **锚解析是全文子串** | `le not in ledger` ⇒ 任意位置出现即命中 | 与 C4 链确立的「**整行全等**」纪律**不一致** ⇒ 引述也会命中 ⇒ 假绿 |
| **回滚路径未定义** | Entry #332 ⑧ 已指出「回滚不得靠删除(违反 append-only)」但**未给形态** | 一旦要回滚 ⇒ 无处落笔 ⇒ 倾向删段落 ⇒ 破坏 append-only |
| **⑥ 的载体不存在** | — | — |
| ⚠️ **实测纠正**:⑥ 的载体**已存在** | `07 号` §12「CEP 使用手册(Stage 4+ 维护态交接)」**已是入口指针**形态 | ⇒ ⑥ **无需新建载体**,只追加一句单向指针 |

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **闸升级** | `scripts/test/cep_b3b2_deferral_check.sh` | `LEDGER="${CEP_LEDGER_PATH:-<现路径>}"` + 重定向表多根;锚匹配 ⇒ **行首标题匹配**(`### Entry <ref>` 开头) |
| 2 | **重定向表**(新) | `test/cep_b3b2_ledger_redirect.json` | `{"roots": ["<台账现路径>"]}` ⇒ 将来移动只改表不改代码 |
| 3 | **`07 号` 单向指针** | `docs/architecture/07-coherence-exploration-platform.md` §12 | 追加一句「维护态的 B3b-2 判定规则 = deferrals JSON + 闸;⛔ 本文件不复写规则」⇒ **冻结件 ⇒ 五步链** |
| 4 | **归档态段** | `.rddf/roadmap/objectives/obj-cep-platform.md` | **追加** `## 归档态` 段:6 项定义 + 回滚路径 + 撤回标记形态 + 触发条件 ⇒ ⛔ **不是「归档已发生」** |
| 5 | **新闸** | `scripts/test/cep_b3b2_archive_form_check.sh` | V1~V7:6 项齐备 + 锚规则未降级 + 重定向表 + 指针闭合 + 撤回标记成对 + 闸行为有牙;ctest **174 → 175**;守门表 **35 → 36** |

## Acceptance

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（6 项定义齐备）**:`obj` 的 `## 归档态` 段存在,且 **6 项**(路径形态 / 语义选用规则 / 义务转移载体 / LEDGER 重定向 / 逐族锚解析 / `07 号` 维护态段)**逐项可 grep** ⇒ 删任一项 ⇒ 新闸红
- [ ] **AC-2（闸锚规则已升级且未降级）**:闸的锚匹配 = **行首标题匹配** ⇒ 变异「改回全文子串」⇒ 新闸红(签名 `锚解析降级`)
- [ ] **AC-3（重定向表）**:`test/cep_b3b2_ledger_redirect.json` 存在且 `roots` 首项 = 台账现路径 ⇒ 改错 ⇒ 新闸红
- [ ] **AC-4（`07 号` 单向指针）**:`07 号` §12 含维护态判定规则的指针句 ⇒ 删句 ⇒ 新闸红;⛔ 该段**不复写**规则
- [ ] **AC-5（回滚形态成对可判）**:归档态段含「被撤回的 SU 标识」+「撤回 Entry 号」**成对** ⇒ 只留其一 ⇒ 新闸红;且**触发条件 3 条**齐备
- [ ] **AC-6（闸行为有牙）**:破坏 deferrals 的 `ledger_entry` ⇒ 闸红(签名含 `ledger_entry`)⇒ 还原 ⇒ 绿;且**既有 4 条仍绿**(行首匹配兼容 `Entry #249`)
- [ ] **AC-7（0 回归 + 冻结链）**:`ctest` **175/175** · `cep_*.sh` **41/41** rc=0 · format / docs_sync / doc_ref / lint / freeze 全 rc=0 · 冻结清单 `authorized_by` = 本 change 的 builder Entry · 无参复验 rc=0
- [ ] **AC-8（范围纪律）**:⛔ **未执行归档**(`## 归档态` 段**不含**「已归档」表述)· ⛔ `B3b-2` 仍 **0/4** · ⛔ deferrals JSON **零 diff** · ⛔ 无 ADR 变更 · ⛔ `§4.0` 未动 ⇒ `git diff --stat` 逐条可核

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-instance-assembly` | ⛔ **不涉及** | — |
| (新增)`cep-b3b2-archive-form` | **ADDED** | 「B3b-2 的归档形态与回滚路径」这一**契约面**(形态定义 + 指针闭合 + 撤回成对) |

## Impact

| 项 | 内容 |
|---|---|
| **新增** | `test/cep_b3b2_ledger_redirect.json` · `scripts/test/cep_b3b2_archive_form_check.sh` |
| **修改** | `scripts/test/cep_b3b2_deferral_check.sh` · `docs/architecture/07-coherence-exploration-platform.md`(§12 追加一句)· `.rddf/roadmap/objectives/obj-cep-platform.md`(追加 `## 归档态` 段)· `test/CMakeLists.txt` · `scripts/README.md` |
| **冻结面** | ✅ **1 处**(`07 号`)⇒ **走五步链** |
| **KPI slot** | 占 **1 个** Proposed(当前 0/3)⇒ 归档后回填 0/3 |
| **风险** | 闸的锚规则升级会改变既有 4 条 deferrals 的校验路径 ⇒ 已实测兼容(`Entry #249` 行首命中)· ⛔ `## 归档态` 段可能被误读为「已归档」⇒ 段首必须显式声明「形态定义,非归档已发生」 |
| **不可约残余** | ① 归档态段是**形态定义**,⛔ 不是归档事件 ② 终局 Ⅱ 的季度复审节奏**不变**(定义 ≠ 触发)③ 回滚触发条件含「用户 / 实测」⇒ ⛔ agent 不得自行宣告回滚已执行 |

## 已裁项（本草案不重新讨论）

| # | 裁决 | 落点 |
|:--:|---|---|
| 1 | **② 归档语义 = 按终局分支自动选用**(Ⅰ→✅ / Ⅱ→不归档 / Ⅲ→用户裁 / Ⅳ→⏸) | 用户拍板 2026-11-12 · architect 决策 §二 ② |
| 2 | **① 归档路径 = 原地保留 + append-only 归档态段** | 用户拍板 2026-11-12 · architect 决策 §二 ① |
| 3 | ④ `LEDGER` 可配置 + 重定向表多根;⑤ 锚 = 行首标题匹配 | architect 决策 §二 ④⑤ |
| 4 | MUST-FIX-4 粒度:默认只撤回「口径 ④ 登记形态」,⛔ 不撤销 C4 的 SU ②(除非触发条件 ③) | architect 决策 §三 3.3 |

## 关联

台账 **#383**(R-1 归因)· **#332 ⑧**(MUST-FIX 原文 + 终局三分支)· **#264** · architect 决策 `.rddf/reviews/cep-mustfix-3-4-archive-form-decision.md` · `scripts/test/cep_b3b2_deferral_check.sh` · `test/cep_b3b2_deferrals.json` · `docs/architecture/07-coherence-exploration-platform.md` §12
