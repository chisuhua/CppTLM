# 审稿报告：ADR-CEP-23（不完整目录知识的表达边界）+ ADR-CEP-13 SU(2026-11-24)

> **审查对象**：architect 产出（G7 决策 + G4-c 登记 / G4-b 裁决）
> **日期**：2026-11-24 · **审查链**：oracle（1 轮）→ critic（1 轮，arch 档）
> **最终判定**：**oracle APPROVE** · **critic APPROVE_WITH_CONDITIONS**（mergeable: yes · scope_exceeded: false · **0 BLOCKER**）
> **收口**：ADR-CEP-23 状态 起草中 → **已签发 v1.0**；ADR-CEP-13 仅追加 SU（正文未改）

---

## 0. 判据来源

| 优先级 | 来源 | 取用 |
|:--:|---|---|
| 1 | 用户本次裁决 | 「按 1 走」= 开 architect 窗口处理 **G7 + R5 追认 + G4-b/G4-c** |
| 2 | gap 台账 | `.rddf/roadmap/cep-gen-family-gap-closure.md` §2.3（G4 / G7 行） |
| 3 | 上游真源 | `ADR-CEP-04`（决策 2/3 · V-04-7）· `ADR-CEP-00`（决策 1 判权域）· `ADR-CEP-18`（决策 1/6）· `ADR-CEP-12` |
| 4 | 一手证据 | 调研 `cep-coherence-protocol-space-survey.md` v0.4 §1.3 Q1 / §4.3.1 / §4.3.2 |

## 0.1 意图检查点（architect 路径）

| 项 | 结论 |
|---|---|
| 意图是否可判定 | ✅ 用户显式指定范围（G7 + R5 + G4-b/c）⇒ 无需提问 |
| 是否触发 metis | ❌ 未触发 —— 意图明确且不存在 ≥2 种合理解读；边界枚举（区间 / 租约 / 有限指针目录是否被静默排除）由 oracle 承担并已复核 |
| 审查域声明 | oracle 输入含「审查域: 其他审查」（项目内 ADR · 非 `~/.config/opencode/` 治理面） |

---

## 1. oracle 审查（第 1 轮）—— 判定 **APPROVE**

**根因诊断（采纳）**：

- G7：同一取值 `PARTIAL` **三重身份错位** —— schema 禁止（F2）/ J2 当「合法组合」保留（F3 · 合法声明上 vacuous）/ deferral 当「待实现」（F4），而平台无 belief 模型 ⇒ 声明者「是否真漏」**不可机判**。
- G4-c：原 R5 的精确诊断是「**基线选择不一致**」而非笼统「混用口径」—— 全表其余行取模型基线（`mesif` 亦如此），唯 `moesi_gpu` 取实例标签基线；**SC / TSO / PLO 在两基线下恰好重合 ⇒ RC 行是唯一分叉点**。
- 事实核查：oracle 独立复核 F1–F5 全部成立；两行 CONSERVATIVE 的 `broadcast_fallback` 均 true（决策 3 对现状零影响成立）。

**6 项处置（逐条已应用）**：

| # | oracle 意见 | 处置 |
|:--:|---|---|
| 1 | 决策 2「不建 / 显式排除」越权（与「判权域在 ArchForge」自相矛盾） | ✅ 标题改「**本仓声明面不承载**（范围限定：本仓）」+ 前置**范围声明**段（不构成对 ArchForge 的预否决 · 走取代流程）+ 理由 3 限定「本仓路线一致性」 |
| 2 | G4-b「无漂移面」表述失准（输入副本本身就是第二份数据） | ✅ 改「无**不可检出的**漂移面（输入副本受闸守）；否掉的只是**输出**存储」+ 补重开条件②（非 C++ 消费者互操作 = 存储唯一真实收益） |
| 3 | V-23-5 归因挂靠 IC1 属形式挂靠；grep 模式应固化 | ✅ 采纳目标，但**收敛方式与建议不同**（见 §1.1）：改为**复用 owner 编号**（`INV-CEP-04-IC1` / `V-04-7`）+ V-23-5 显式标为**守门判据（无归因）**；grep 模式固化为**字面清单** `BeliefModel` / `knowledge_belief` / `belief_` |
| 4 | R5 收尾三处 | ✅ 删悬空引用 `dmb_to_RC_map.txt`（改「fence 是三元接口独立操作」+ 调研 §4.3.1 行键是模型族）· 理由 1 改精确诊断 · **登记死枚举处置** |
| 5 | G7 决策 3/4 措辞精确化 | ✅ 决策 3 加「在**行为语义层面**」+ 补「区间 / 租约 / 有限指针均可被 CONSERVATIVE 建模」边界段 · 决策 4 例子具体化 |
| 6 | V-23-2 行数固化防漏检 | ✅ 改「**全行**（当前 6 行）」+ 双层断言（全行迭代 + static_assert(size==6)） |

### 1.1 审查后对**判据面**的收敛（不改变任何决策）

oracle 建议新增 `INV-CEP-23-IC1/IC2/IC3`。成稿时**改为复用 owner 编号**，理由两条：

1. **避免第二套判据标准**：「caps 值 ∈ enum」已是 `INV-CEP-04-IC1` 的语义；「`CONSERVATIVE` ⇒ 兜底」已是 `ADR-CEP-04` 的 **V-04-7** 判据。同一约束挂两套编号 = 同一件事两个维护点。
2. **零涟漪**：新造 3 条 invariant 需同步 `07 §4` 总数（66→69 / 编号位 67→70 / 15 组→16 组）+ objective DoD 7 + improvement A2 + phase-10 **四处**，其中两处在 planner 写集内。

⚠️ 该收敛发生在 critic 通过之后，**属判据面（归因 id）调整，未改任何决策内容**；critic 的 2 条 SHOULD_FIX / 2 条 NIT 已全部按其意见落地（见 §2）。附带效果：`cep_docs_sync_check.sh` 的「孤儿 invariant / OWNERS 覆盖」判据不再被新 id 触发 ⇒ 本 ADR 应登记进 `NO_INV` 而非 `OWNERS`（已写入 ADR §7 后续清单）。

## 2. critic 审查（第 1 轮 · arch 档）—— **APPROVE_WITH_CONDITIONS**

**mergeable: yes · scope_exceeded: false · BLOCKERS: 0**

5 个 focus 点逐条结论（原文摘要）：

1. **ADR-23 vs ADR-CEP-04 决策 2**：不构成冲突 —— 行为禁令（无兜底 PARTIAL = 失效）在决策 3 原样保留并由 V-23-4 机判；收窄的只是**可声明域**，§7 已声明「收窄其取值域」，语义无漂移。
2. **决策 2 范围限定**：不越界 —— 协议语义建模归 ArchForge，声明面/判据面属 CppTLM 治理域；现有措辞（范围声明 + R1 风险 + 决策 4 取代路径）已足够稳。
3. **SU 推翻 provisional**：在权限内 —— 被推翻文本自陈 provisional（`spec.md:17`）⇒ 本次推翻即追认履行；SU ② 已声明走五步授权链。
4. **V-04-7 共存**：可共存 —— 决策 5 正文（非仅 §7）已指明 `next_action` 须改判。
5. **死枚举**：不违零债务 —— 附启用判据 + 值序变更理由，属有据保留。

**SHOULD_FIX 处置**：

| # | 意见 | 处置 |
|:--:|---|---|
| SF-1 | SU ② 实施面清单漏了 `spec.md:17` 的 **Purpose 段 provisional 注记**（只改规则表会自相矛盾） | ✅ 已补为清单第 ④ 项，并写明「只改规则表不改注记 ⇒ 同一文本一边说待追认一边已取新口径」 |
| SF-2 | ADR-23 决策 1 缺一句「收窄但不取代 ADR-CEP-04 决策 2」（收窄关系只在 §7） | ✅ 决策 1 已加「与父决策的关系（收窄，不是取代）」条目 |

**NITS 处置**：

| # | 意见 | 处置 |
|:--:|---|---|
| N-1 | 死枚举启用条件与 G4-b 重开条件①措辞不同、易漂移 | ✅ 已加互指：两处**同源**（都指向「单方向 fence / 按 read·write 分列多值」），须同步演进 |
| N-2 | V-23-4 的归因链提示 | ✅ 维持现状（已在 §1 与 ADR §6 留痕，供 builder 改判 V-04-7 时核对） |

## 3. 结论与遗留

**签发**：ADR-CEP-23 → ✅ 已签发 v1.0（`docs/adr/README.md` 索引同步：CEP 26 份 · 已签发 22 · 起草中 3 · 合计 65）。

**留给后续执行者（非 architect 职责）**：

| # | 事项 | 执行者 | 依据 |
|:--:|---|---|---|
| 0 | `cep_docs_sync_check.sh` 的 `NO_INV` 白名单加 `ADR-CEP-23`（不进 `OWNERS`） | builder | ADR-CEP-23 §5 / §7 |
| 1 | 冻结五步链：`ADR-CEP-13` **hash 变更** + `ADR-CEP-23` **新增**（同一批） | builder（需先立台账 Entry） | `docs/guide/cep-usage.md` §12.6 |
| 2 | R5 实施面修正（4 项清单见 SU ②）：`fence_effects.hh` R5 分支 + 独立 oracle + main spec 规则表 + **Purpose 段 provisional 注记** | builder | ADR-CEP-13 SU ② |
| 3 | `test/cep_adr_structural_deferrals.json` 的 `V-04-7` `next_action` 改判 | builder | ADR-CEP-23 决策 5 |
| 4 | V-23-1 / V-23-5 的判据载体化（schema-enum 比对闸 · belief 字面清单 grep） | planner → builder | ADR-CEP-23 §6 |
| 5 | `docs/adr/CEP-DECISION-DIGEST.md` 补卡（**存量已旧**：仍标 24 份且缺 ADR-CEP-22 卡；本次未动，避免趁机改存量） | 待用户裁决是否单独立项 | — |

## 4. 复现命令

~~~bash
bash scripts/test/docs_adr_index_check.sh          # 65 份 · 逐份状态一致 · rc=0
bash scripts/test/markdown_link_check.sh           # rc=0
bash scripts/test/cep_docs_sync_check.sh --strict  # 预期红：ADR-CEP-23 未登记进 NO_INV（builder 待办 #0）
bash scripts/test/cep_freeze_check.sh              # 预期红：ADR-13 hash 变更 + ADR-23 新增（五步链，builder 待办 #1）
grep -n "R5" openspec/specs/cep-fence-consistency-effect-derivation/spec.md
~~~
