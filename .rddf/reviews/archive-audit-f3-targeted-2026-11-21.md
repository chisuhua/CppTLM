# 定向补验报告 · 甲层抽检命中的 3 份（I-10 F-1 的定向补验）

> **角色**：builder（验收 = 角色内建环节）· **已用轮次**：1/3
> **性质**：⚠️ **定向补验 3 份**（⛔ **不是**全量审计；⛔ **不是** `-verify.md` 形态的归档验收报告 —— 甲层基线不可复现，见 I-10 AC-12 口径）
> **判据来源**：① `.rddf/reviews/archive-audit-2026-11-21.md` F-1 / F-3（抽检命中 3 份需定向补）· ② `.rddf/improvements/cep-d1-archive-verify-backfill.md` §范围纪律「抽检命中问题 ⇒ **定向补那一份**，不扩成全量」
> **基线（base）**：`27bc06c6`（I-7b 收口 commit）

## 逐份结论

| # | change | 抽检原结论 | 定向补验结论 | 差额性质 |
|:--:|---|---|:--:|---|
| 1 | `2026-10-28-cpptlm-cep-chi-rnf-subset` | 实质可信 + 疑点（7.1 / 7.2 / 7.3 未勾） | **实质可信** | **历史 bookkeeping 未回勾**（非实现债） |
| 2 | `2026-11-06-cpptlm-cep-tilelink-t3-mapping-survey` | 发现疑点（**真债**：无 tasks / 无 delta spec / 4 条 AC 无证据面） | **实现与证据齐备** | **形态债**（无 `tasks.md` 勾选面）· ⚠️ 原「真债」判定**订正** |
| 3 | `2026-11-10-cpptlm-cep-bridge-reattach-guard` | 实质可信 + 疑点（T4.8 核对未落） | **核对完成 ⇒ 不需独立注册** | **无债**（仅补一项核对结论） |

## ① `cpptlm-cep-chi-rnf-subset` —— 3 个未勾项逐项取证

| 未勾项 | 判据 | 实测证据 | 结论 |
|---|---|---|---|
| **7.1** 台账 Entry + §4.0 行 #8 + `phase-10.md` + `.rddf/roadmap.md` + `AGENTS.md` KPI 基线 + improvement front-matter | 每处是否有**收口记录** | `obj-cep-platform.md:8028` = `### Entry #95 — 2026-10-28 [build]`（收尾 Entry）· `obj-cep-platform.md:192` §4.0 行 8 = `✅ 已归档(2026-10-28, Entry #95)` · `phase-10.md:163` = `✅ 2026-10-28 已归档` · `improvements/cpptlm-cep-chi-rnf-subset.md` front-matter = `状态: ✅ 已归档(2026-10-28, Entry #95)` | **已收口** |
| （同上）`.rddf/roadmap.md` 与 `AGENTS.md` KPI 基线 | 是否是「应有而无」 | `roadmap.md` 的 `Phase Skeleton` 是**阶段级**真源（文件头自述「阶段状态真源 … CEP 载体分工见 `obj-cep-platform §4.7`」）⇒ **change 级状态不落该文件**；KPI 基线行是**滚动值**（现为 2026-11-19 口径），7.1 当时的目标值早被后续 12 次更新覆盖 | **N/A by 结构演进**（⛔ 不是缺口） |
| **7.2** 归档动作本身 | 归档是否真的发生 | `openspec/changes/archive/2026-10-28-cpptlm-cep-chi-rnf-subset/` 在位（含 `specs/` delta）· 注册表登记「已完成归档」语义 | **已完成**（勾选面未回填） |
| **7.3** 守门连锁修 4 类 | 4 类是否各自成立 | ① KPI baseline —— 同上（滚动值，已被后续覆盖）② 移交链断言 ⑦ —— `docs_sync_check.sh --strict` rc=0（输出「移交链断言 ⑦ 满足」）③ 变异 fixture 锚点 —— `grep -rn "chi-rnf\|chi_rnf" test/ scripts/test/` 仅命中其**自身** ctest 注册（`test/CMakeLists.txt:57` `cep_chi_rnf_subset`），无其他 fixture 指向它 ⇒ 无锚点需跟随 ④ 对外引用可达 —— `cep_doc_ref_check.sh --strict` rc=0 | **已满足** |

## ② `cpptlm-cep-tilelink-t3-mapping-survey` —— 9 条 Acceptance 逐条取证（⚠️ 订正原判定）

**先判性质（audit F-3 要求的步骤）**：该 change **不是「未实施」** —— `proposal.md:117` 明文 `**不改**任何 main spec(⇒ **不触发** M1 冻结面授权);openspec/specs/ 的 delta **归后续的引擎 change**` ⇒ **无 delta spec 是设计**，⛔ 不是漏做。⇒ **已实施，形态债 = 缺 `tasks.md` 勾选面**。

| AC（proposal 原文顺序） | 载体 | 实测 |
|---|---|---|
| 行宇宙完整（全部行已列出） | `kT3MappingCells`（11 行 = `kPlatformIntentCount`） | `static_assert(TileLinkStateSpec::t3_row_count_is_exact())` · `t3_intents_are_unique_and_complete()`（`tilelink_state_spec.hh:932/934`）· `test_cep_tilelink_state_spec.cc:551` |
| 逐行 `basis` + 条款级 `ref` | 同上 | `static_assert(t3_non_pending_rows_have_ref())`（`:940`）· `:574` |
| 禁冒充（无「公开来源」充当规范依据） | `t3_basis_ref_kind_consistent` | `test/test_cep_source_registry.cc:57-58`（逐行 `REQUIRE`）· `:574` |
| 载体编译期锁（行数 / 唯一 / basis↔ref / Pending 不可消费） | 6 条 `static_assert` | `tilelink_state_spec.hh:932-942`（行数 · intent 唯一全覆盖 · row_ref 唯一 · Pending 无事件 · 非 Pending 有 ref · 掩码范围） |
| 决定性变异（删任一行 ⇒ 按原因红） | `:610`「事件取值逐行钉死」 | `:610` TEST_CASE（映射被悄悄改即红） |
| 反向 fixture（basis 冒充 ⇒ 必红） | `:636` fail-closed | `:636` TEST_CASE（`Pending` 行不得被消费） |
| 锚点摘要 ↔ 载体逐项相等 | `:595` X-2 元测试 | 读 `docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md` 解析 `TLC-M-NN` 行 id 集合 ⇔ 载体行集合 |
| 二分支结论已落 | 台账 | 预登记 **Entry #241** · 归档 **Entry #244**（`obj-cep-platform.md:15782` 关联段） |
| `[cep]` 全绿 + 映射表闸 `--for-achievement` rc=0 | 闸 + 套件 | `python3 scripts/test/cep_invariant_map_check.py --for-achievement` → **rc=0**（`66/66 = 66 项有牙载体 + 0 ⏭-unmodelled + 0 ⏭-pending`）· `[cep]` 12740 assertions / 554 cases 全绿 |

⇒ **9/9 有载体**；差额仅「无 `tasks.md` 可勾清单」—— 归档件不可回改（⛔ 补写 tasks 等于伪造勾选历史）⇒ 记为**形态债**，不需实现性返工。

## ③ `cpptlm-cep-bridge-reattach-guard` —— T4.8 核对

| 判据 | 实测 | 结论 |
|---|---|---|
| 是否需独立注册 ctest 项 | `./build/bin/cpptlm_tests "[cep][assembly][bridge-reattach]"` → **15 assertions / 3 test cases 全绿**；`test/CMakeLists.txt:264` = `add_test(NAME cep_all COMMAND cpptlm_tests "[cep]" -r compact)` ⇒ 该标签集 ⊂ `[cep]` | **不需独立注册**（已被 `cep_all` 覆盖）· T4.8 的「核对」动作**已完成** |

## 对 `archive-audit-2026-11-21.md` 的两处订正（⛔ 不回改原文件，就地登记）

| # | 原表述 | 订正 | 依据 |
|:--:|---|---|---|
| K-1 | `t3-mapping-survey` = 「发现疑点（**真债**：… 无 delta spec …）」 | 「**已实施**；无 delta spec 是 proposal 明文的**设计决定**（delta 归后续引擎 change）⇒ 差额 = 形态债（缺 tasks.md）」 | `proposal.md:117` |
| K-2 | `chi-rnf-subset` 的 7.1/7.2/7.3 未勾被列为「疑点」 | 属**历史 bookkeeping 未回勾**（收口已由 Entry #95 + 四处状态登记承担）⇒ 非「实质可疑」 | 上文 ① 的四项证据 |

## 失败项与路由建议

| # | 项 | 分类 | 路由 |
|:--:|---|---|---|
| F-1 | `t3-mapping-survey` 缺 `tasks.md` | **形态债**（不可回补：归档件不可改） | 记录即止；⛔ 不新建「补写 tasks」动作 |
| F-2 | 三份的未勾项均**无实现缺口** | — | **无需返工**；⛔ 不转 implementation gap |

**结论：0 项实现债 · 0 项需返工。** 3 个「定向补验单元」的**技术内容在本报告内闭合**；若要把它们立成 planner 侧的独立 improvement（纯流程形式），归 planner 阶段，⛔ 本报告不代其立项。

## 遗留清扫

| 项 | 结论 |
|---|---|
| 仓库改动 | **零改动**（全程只读：`read` / `grep` / `ls` / `ctest -N` / 运行既有闸与既有测试） |
| 临时文件 | 无（仅 `/tmp/opencode/bgate.log` 一份闸输出，在工作区外） |
| 半截状态 | 无：3 份各有结论 + 2 处订正 + 路由 |

## 已用轮次

**1/3**（第 1 轮即闭合；无返工轮）。
