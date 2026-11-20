# 裁决请求 · **I-6**（invariant 建模批 2）—— 请 architect 出形态 + 请用户 ratify

> **请求方**：builder（登记册 §6.9 阶段 ③ 的决策门）· **日期**：2026-11-21
> **对象**：`INV-CEP-13-IC1` / `INV-CEP-13-IC3` 两条（批 2 最后 2 条 `⭭-unmodelled`）
> **⛔ 本请求不含任何裁决** —— 全部决策点留空待裁；本文只给**已复验的事实** + **必须回答的问题** + **建议（供参考，非结论）**
> **裁出处建议**：`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md`（独立文件，⛔ 不 append 进 CEP 裁决记录 —— 对象是规格面修订）

## 一、已复验的事实（2026-11-21 实测 · 每条可由一条命令复现）

```
F-1 剩 2 条 ⭕-unmodelled：test/cep_invariant_deferrals.json = [INV-CEP-13-IC1, INV-CEP-13-IC3]
F-2 边类 rfe / fr / ws **两仓均无定义**
     grep -rn "rfe\b|\"fr\"|\"ws\"" include/ src/ test/ docs/  → 0 命中（除公理公式本身）
     ArchForge 同 grep → 仅命中 ADR-SOC-25 D6 引用的公式
F-3 复合公理当前**无机器面**：装配期闸 channel_dependency_graph.hh **逐实例建图**且边**不跨网络**
     ⇒ 复合图 ≡ 各网络子图之并 ⇒ 「复合」与「逐网络」当前**等价** ⇒ 公理形式未被真正检查
F-4 verification_scope **不是已声明字段**：grep 全仓 → 仅命中 openspec/specs/cep-core-contracts/spec.md 的散文
F-5 schema 版本 = 13（include/tlm/cep/protocol_instance.schema.json），且该文件**在冻结集**（FROZEN_GLOBS）
F-6 protocol_instances.json **不在**冻结集 ⇒ 实例声明项新增**不触**五步链
F-7 ADR-CEP-12 存在（追加式修订合规：签发后只追加 Status Update）
F-8 litmus 侧已就位（**I-6a，2026-11-21 落地**）：归档 change
     openspec/changes/archive/2026-11-21-cpptlm-cep-litmus-falsification-harness/
     工装 include/tlm/cep/litmus_falsifier.hh + 语料 test/cep_litmus_corpus.json（**当前空**）
     ⇒ 边类一旦有定义 ⇒ 语料可**派生**填充 ⇒ litmus 判定自动从「不可证伪」翻转
```

## 二、必须回答的问题（**每条给出「可断言对象 + 载体形态 + 判据写法」三要素** —— per Q-4 的裁决格式）

### D-1 · schema 13→14 的**新增声明项**是什么？

| 候选 | 形态 | 代价 | 风险 |
|:--:|---|---|---|
| **A** | `compound_consistency`: {`edge_classes`: [...], `domains`: [...]} —— 声明**边类取值域** + **域划分** | 字段名/取值域需定死；与 `INV-CEP-12-IC1` 的联动边界需明写 | 取值域一旦发布即成契约 ⇒ 后续增类需版本 15 |
| **B** | 只加 `verification_scope`（域内可验范围），边类与域划分**留给 ADR** | 字段面最小 | ⛔ 仍无「机器可检声明项」⇒ `INV-CEP-13-IC1` 的缺口未闭 |
| **C** | A + `verification_scope` 两个字段 | 面更大 | 与 A 同 |

⇒ **建议 A**（缺口原文要的是「组合判据成为**结构化声明项**」⇒ B 不满足）。⚠️ 建议仅供参考。

### D-2 · 边类 `rfe` / `fr` / `ws` 的**语义**从哪来？

| 候选 | 说明 |
|:--:|---|
| **A** | 由 `ADR-CEP-13` 决策 2 的**追加式**修订给出定义（⛔ 原文不改）|
| **B** | 由 `ADR-CEP-12` 的追加式修订给出（per 单元 What Changes #4）|
| **C** | 沿用 ArchForge `32 号 §6.0.3` 的 VC-R1/VC-R2 支撑规则**推导**（⚠️ 两仓**实测无** rfe/fr/ws 定义 ⇒ 推导 = **发明语义** ⇒ 风险最高）|

⇒ **建议 A 或 B**（⛔ **不建议 C** —— 与 F-2 冲突 ⇒ 属发明语义）。

### D-3 · `INV-CEP-13-IC1` 与 `INV-CEP-13-IC3` 的**载体**分别落在哪？

- IC1（复合公理）⇒ 候选：`litmus_falsifier.hh` 的载体 + schema 14 声明项的**联动**判据
- IC3（adapter 提前 ack 行为）⇒ 候选：`protocol_instances.json` 的新声明项 + 装配期闸适配

⚠️ 需明确：IC1 的载体是否**依赖** IC3 的 schema 项（同一份 schema 14）⇒ 决定二者的实施顺序。

### D-4 · `INV-CEP-12-IC1` 的**联动边界**

单元要求「**只联动，不改语义**」⇒ 需 architect 明写：schema 14 之后，`INV-CEP-12-IC1` 的载体**如何改**（载体文件位置 / 判据写法 / 是否需重生成映射表），且其 `reason` / `trigger_condition` / `ledger_entry` **逐字不变**。

### D-5 · 五步链的**授权顺序**

schema 13→14 属**冻结面成员**改动 ⇒ 需 `cep_freeze_check.sh --update --authorized-by "Entry #N"`；而 `Entry #N` 须先落台账（append-only）⇒ 请明确：Entry 编号的**分配点**（是否由 architect 的裁决文件预留编号）。

## 三、⛔ 用户侧 ratify 项（**请在 architect 形态落定后单独 ratify**）

| # | ratify 对象 | 为何需用户 |
|:--:|---|---|
| **R-1** | **schema 版本 13 → 14**（契约面版本递增） | 契约面变更 |
| **R-2** | 新增声明项的**取值域**（若 D-1 选 A/C） | 取值域一经发布即成对外承诺 |

⇒ 建议**先出形态（architect）→ 再 ratify（用户）**，⛔ 不要并行（形态未定则 ratify 无对象）。

## 四、⛔ 循环依赖提示（**必须先裁，否则两边都动不了**）

```
I-6 的前置 ① = 「litmus change 立项 + 归档」  ⇒ 已由 I-6a 落定 ✅（F-8）
I-6a 的「设计」若需要 schema 14 的声明项     ⇒ 反向依赖 I-6  ⚠️
```
⇒ 本轮 I-6a **刻意不依赖** schema 14：语料保持**空**、判定给出「**不可证伪**」的真结果（⛔ 不发明语义）⇒ 循环已断开。
⇒ **仍需 architect 明确**：D-1 若选 A/C，schema 14 的声明项**是否**同时定义边类语义（若是 ⇒ litmus 语料**由声明项派生**，仍无需手写）。

## 五、建议的下一步（供参考）

1. architect 出裁决（形态 + 三要素 + D-1~D-5 的答案）⇒ 落 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md`
2. 用户 ratify R-1 / R-2
3. builder 立项 OpenSpec change（占 1 slot）⇒ 实施 I-6 ⇒ 移除最后 2 条 `⭕-unmodelled` ⇒ `⭭-unmodelled` 归零
