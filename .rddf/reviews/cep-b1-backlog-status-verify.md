# 验收报告 · **G-18**（B-1 处置 —— `cep-memory-model-operational-trace` 状态行订正 + 登记册归属面同步）

- **角色**：builder（**实施侧自验**）
- **日期**：2026-11-23 · **基线 commit**：`1da58bee`（G-17 状态面刷新）
- **承载缺口**：**G-18** —— backlog 单元的**状态行陈旧**（「⏳ 待用户 ratify」+「I-6 的 R-2」双陈旧）⇒ 登记册归属面同步订正
- **授权来源**：用户 **「选 B」** —— 承接上一轮给出的「B · 保持 backlog」选项（推荐项）

## 判据来源

| 来源 | 位置 |
|---|---|
| **用户拍板** | 上一轮选项表「**B · 保持 backlog**」—— 「不开工；只修正 stale 状态行 + 写清触发条件」 |
| **裁决记录 §一 决策 2** | `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md:22`：「登记 backlog，独立立项解决」（**立项形态 2026-11-21 已拍板**） |
| **裁决记录 §三** | 同文件 `:231`：「**触发条件** = 需要「复合一致性公理**可判**」时（当前 `combination_contract.compound_consistency.adopted` 只能声明**采用**，⛔ 不能判定）」 |
| **§6.8 白名单口径** | 登记册 `:761` 的自查命令 + `:765` 的口径订正（W4-c 加「已落地」的**同型先例**） |

## G-18 逐条判定

| # | 判据 | 实测 | 结论 |
|:--:|---|---|:--:|
| **1** | **单元状态行的「⏳ 待用户 ratify」是陈旧的** | 单元 `:11` 原文含「⏳ **待用户 ratify**(本单元的「每域模型来源」ratify 项 + **I-6 的 R-2**)」⇒ 实测 **R-2a~R-2f 已于 2026-11-21 ratify**（裁决记录 §一 决策 1「接受 13→14」+ §五）⇒ **该半句已失效** | ✅ |
| **2** | **「待立项」也是陈旧的** | 立项**形态**早已拍板（§一 决策 2「登记 backlog，独立立项解决」）⇒ 单元处于「**已登记、待触发条件**」状态，⛔ 不是「待拍板」 | ✅ |
| **3** | **AC-16 开工门已满足** | 实测 `grep -c '"combination_contract"' include/tlm/cep/protocol_instance.schema.json` = **1**（AC-16 要求 == 1）⇒ 单元草稿里写的「当前 **0**」是 **I-6 落地前**的旧值 ⇒ **已过时** | ✅ |
| **4** | **I-6 确实已落地** | 归档 change `openspec/changes/archive/2026-11-21-cpptlm-cep-invariant-modeling-batch2` 在位 · schema `properties` = **14** · `required` = **13** · 含 `combination_contract` = **True** ⇒ 与 R-2 的「13→14 / required 仍 13」逐字相符 | ✅ |
| **5** | **真正待决点被正确登记** | 剩下的不是「等你拍板」，而是 **AC-1 的内容裁决**：「每域模型来源 + 域间边构造规则」= **架构决策** ⇒ 须 architect 裁出内容（新建 ADR）→ **然后**才轮到你 ratify ⇒ 单元状态行改为显式指向该链路 | ✅ |
| **6** | **白名单扩词不损失鉴别力** | 白名单新增 token = **`已登记·backlog`**（带 `backlog` 上下文，⛔ **非裸词** `已登记`）⇒ 误用「已登记」二字不会豁免 ⇒ 只豁免**显式标注为 backlog** 的单元 ⇒ 鉴别力损失受控 | ✅ |
| **7** | **§6.8 自查命令实测输出不变** | 改后 `grep -L -E "已归档\|已实施\|已落地\|已拆分\|已作废\|已登记·backlog" .rddf/improvements/cep-*.md` 实测输出 = **1 份** = `cep-freeze-lexicon-pointers`（真实活义务）⇒ 与 §6.8 现行口径**逐字一致** ⇒ 口径**未变**。⚠️ **实测发现（须记录）**：该单元**改前即未被报出** —— 全文匹配下其正文 **AC-16 行偶然含「已落地」** ⇒ 命中白名单 ⇒ **本次扩词对闸行为无变化**（只消除**偶然依赖**）| ✅ |
| **8** | **载体表 anchor 零破坏** | 载体表 `W-4d` 的 anchor = `` | `cep-memory-model-operational-trace` | ``（逐字）⇒ 本次改 §6.2 S3 表那行时**只动 needs 列、首列逐字保留** ⇒ 闸 P-3 rc=0 | ✅ |
| **9** | **不触五步链** | 改动文件 = 单元 improvement + 登记册 + 载体表 + 台账 + 验收报告 ⇒ **零冻结面成员**（`cep_freeze_check.sh` rc=0 · 90 产物 hash 不变）⇒ 登记册与单元均不在 `FROZEN_GLOBS` | ✅ |

## 实施内容

### 单元文件 `.rddf/improvements/cep-memory-model-operational-trace.md`

| # | 位置 | 改前 | 改后 |
|:--:|---|---|---|
| 1 | frontmatter `状态:` | `🟡 **待立项**` … `⏳ **待用户 ratify**(本单元的「每域模型来源」ratify 项 + I-6 的 R-2)· ⏳ 待立项` | `🟢 **已登记·backlog**` … ✅ **立项形态已拍板**（§一 决策 2）· ✅ **R-2 已 ratify** + I-6 已落地 ⇒ **AC-16 开工门实测 = 1** · ⏸ **待触发条件**「需要复合一致性公理可判时」⇒ 开工前置 = **architect 裁 AC-1** → 用户 ratify ⇒ planner 立 change ⇒ builder 实施（含**两处冻结面**五步链 ×2）· ⛔ **不占 KPI slot**（未立项） |
| 2 | 范围纪律行 | `⛔ 待用户 ratify` | `⛔ 已由 architect 裁决给定 + **用户已于 2026-11-21 ratify** R-2a~R-2f` |

### 登记册（10 处）

| # | 位置 | 改法 |
|:--:|---|---|
| 3 | §6.2 口径头 | 加 `G-18` · `G-18+` 续编 ⇒ `G-19+` · 「⏳ 待用户 ratify 单元」⇒ 「backlog 单元」 |
| 4 | §6.2 S3 说明行 | 「待用户拍板」⇒ 「**等的是 architect 裁 AC-1**，⛔ **不是**等你拍板」+ 待触发条件 |
| 5 | §6.2 S3 表那行 | needs 列：「⏳ **待用户 ratify** · 状态 token「🟡 待立项」」⇒ 「✅ **归属已登记** · 状态 token「🟢 **已登记·backlog**」⇒ **未立项 ⇒ 不占 KPI slot**」（⛔ **首列逐字保留**） |
| 6 | §6.2 已闭合项表 | 计数 `24 → 25` + `G-18` 行 |
| 7 | §6.3 兜底注 | `G-1~G-17` ⇒ `G-1~G-18` |
| 8 | §6.3 归属表 | `G-18` 归属行 |
| 9 | §6.4 编号空间表 | `G-1~G-17` ⇒ `G-1~G-18` |
| 10 | §6.8 单元表那行 | 「⏳ **待用户 ratify**」⇒ 「✅ **已登记·backlog**」+ 待触发条件 + 开工前置链 |
| 11 | §6.8 白名单命令 | 加 `已登记·backlog`（⛔ **非裸词**，见判据 6） |
| 12 | §6.8 口径订正 | 追加第 ④ 条（backlog 态 token 的加入理由） |

### 载体表 · +2 行

| AC | 载体 | anchor |
|---|---|---|
| `G-18` | `.rddf/improvements/cep-memory-model-operational-trace.md` | `状态: "🟢 **已登记·backlog**` |
| `G-18` | `.rddf/reviews/cep-b1-backlog-status-verify.md` | `**G-18 逐条判定**` |

## 失败项与路由建议

| # | 项 | 路由 |
|:--:|---|---|
| — | **无失败项** | — |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次改动 | 单元 improvement（2 处）· 登记册（10 处）· 载体表（+2 行）· §0.1 载体表行（连锁）· 台账 Entry #532 · 本报告 |
| 临时诊断代码 / 临时文件 | ✅ 无仓内残留 |
| TODO / 半截实现 / 被 skip 的测试 | ✅ 无新增 |
| 未替换的 mock / 临时 fallback | ✅ 无 |
| 范围纪律 | ✅ 零 ADR 正文 · 零**冻结面**成员 · 零 C++ · 零 `openspec/**` · 零 force-add · ⛔ 未改任何历史 Entry 正文 |
| 载体表既有 anchor | ✅ 零破坏（`W-4d` 的 anchor 逐字保留 ⇒ 闸 P-3 rc=0） |
| 台账 append-only | ✅ 订正**追加式**（Entry #532）⇒ 未回改历史 Entry |
| ⚠️ **范围外的历史遗留** | 台账 `:17675` 有 **2 个 U+FFFD**（替换字符）—— 实测**改动前即存在**（`git show HEAD` 版本同位置同字符）⇒ ⛔ **不改**（属**历史 Entry 正文** · append-only）⇒ 仅记录，不在本轮范围 |
| **登记册现状** | `G-*` 编号空间仍开 = **0 项**（`G-1`~`G-18` 全闭合）；`G-*` 之外的唯一真活义务 = 本 backlog 单元（**已登记 · 待触发条件**）⇒ 与 §6.2 口径头「本口径仅计 `G-*` 编号空间」自洽 |
| ⚠️ **附带残余** | §6.8 的白名单闸**域 = 文件全文**（W4-c 口径订正 ②）⇒ 「状态行漂移」**可被正文偶然词掩盖**（实测：该单元改前即因其正文 AC-16 行含「已落地」而被排除）⇒ 该类漂移**闸不可发现** ⇒ ⛔ **本轮不改口径域**（改闸的判据面属范围外 + 刚经 W4-c 裁定）⇒ **待单独立项** |

## 回归记录

| 检查 | 命令 | 结果 |
|---|---|---|
| 顺序机关 | `bash scripts/test/rddf_stage_gate.sh` | ✅ rc=0 |
| 活基线闸 | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0（5 项文档取值 == 实测） |
| 旧口径残留闸 | `bash scripts/test/cep_registry_stale_refs_check.sh` | ✅ rc=0（40 项全为 0） |
| 文档路径同步闸 | `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |
| 冻结面校验 | `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0（90 产物 hash 不变） |
| CEP 契约同步闸 | `bash scripts/test/cep_docs_sync_check.sh --strict` | ✅ rc=0（13 组） |
| **§6.8 自查命令** | `grep -L -E "已归档\|已实施\|已落地\|已拆分\|已作废\|已登记·backlog" .rddf/improvements/cep-*.md` | ✅ 输出 = **1 份** = `cep-freeze-lexicon-pointers`（真实活义务）⇒ **口径未变** |
| ctest（本批涉及） | `ctest --test-dir build -R "cep_docs_sync_check\|rddf_stage_gate\|cep_registry_baseline\|cep_registry_stale_refs\|cep_freeze"` | ✅ **7/7 Passed** |

## 已用轮次

**1/3**（G-18 实施侧自验第 1 轮 —— 实测确认 R-2 早已 ratify + 开工门 = 1 ⇒ 单元状态行双陈旧；白名单扩词选用**带上下文的精确形式**以控鉴别力损失；无返工轮）。
