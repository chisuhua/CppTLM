# proposal: cpptlm-spec-purpose-batch3 —— R-1 批 3（17 份 dGPU 板级 / SoC 域主 spec 的 Purpose 占位修复）

> **状态**: 🟡 Proposed（2026-11-16 立项 · planner 起草 · 待 metis 预审 + builder 实施）
> **依据**: improvement `.rddf/improvements/cpptlm-spec-purpose-batch3.md` · 台账 **Entry #444**
> **KPI**: 立项后 Proposed **0/3 → 1/3**（改 `openspec/specs/` **契约面** ⇒ **不适用** doc-only 豁免 ⇒ **计入**）
> **归档语义标记**: ✅ **已完成归档 · 主 spec 直改（无 delta）** —— 本 change 的契约效果 = **主 spec 直改**（`## Purpose` 段）；⛔ **0 份 delta**（归档器实测：delta 的 `## Purpose` 被忽略，要求「Edit … spec.md directly」）；`.openspec.yaml` 的 `skip_specs: true` 表示「**不做 delta 合并**」⇔ **⛔ 不是** AGENTS.md 的 ⏸ 暂存归档语义（后者 = 能力未落地、设计保留）—— **本 change 的能力已落地**。判别载体：本标记行 + 归因 JSON `archive_semantics` 字段 + 台账 Entry #445。

## Why

**全仓 129 份主 spec 里有 59 份的 `## Purpose` 是 `openspec archive` 自动写的占位**（`TBD - created by archiving change …`），归档后从未被人改写。

后果：① 人读规范不知道该能力管什么；② `openspec validate --strict` 把占位判为 WARNING ⇒ **非 `cep-*` 的 65 份 `--strict` 红里 59 份因此红**（R-1 主体）；③ 工具对新能力的 Purpose 有明确质量要求（≥50 字符、非占位），而这批能力归档时**没走 delta** ⇒ **工具自己留了坑、人没填**。

**⚠️ 关键澄清**：本 change **不新增任何能力** ⇒ 写 Purpose 属「授权契约内容」，但口径**收窄**为「**忠实记录既有实现**」：
- **事实基础** = 来源 change 的 proposal「为什么」段（**实施期**写下的意图陈述）+ 该 spec 的 Requirement 标题（能力面）⇒ 二者都是**实施时既有事实**。
- canonical 草稿 + **写作规则 D-0（复用批 1 的 7 条）**见 `design.md`；逐条**可追性核对**（每个能力断言对应的仓内证据文件）见 `design.md` D-1 末。
- **superseded 一律移出**（本批实测 **0 份** spec 级 superseded）。

**为什么只做 Purpose、不动正文**：Purpose 段与 Requirement 段是**两个独立段** ⇒ 改动**不触及任何可验证行为** ⇒ 不改变任何判据的判别力。而 **26 份**有「正文 >500」issue 的裁剪**会动 Requirement 段** ⇒ **另一档**风险，**必须另立 change**（批 2）。

**⛔ 范围边界**：零实现层 · **只改 `## Purpose` 段**（Requirements / Scenario 正文**逐字节不变**）· 不动 43 份 `cep-*`（已 `--strict` 全绿）· 不裁正文（批 2）· 不动其余 42 份（批 4~6）· 不改既有 45 闸。

## What Changes

| # | 内容 | 载体 |
|:--:|---|---|
| **W1** | **17 份**主 spec 的 `## Purpose` 段：占位 → 实质 Purpose（canonical 草稿见 design §D-1；规则见 §D-0） | **直接编辑主 spec**（⛔ **0 份 delta**） |
| **W2** | **1 个校验脚本** `scripts/test/cep_spec_purpose_batch3_check.py`（C-1~C-7） | 新文件 |
| **W3** | **1 份归因载体** `test/spec_purpose_batch3_attribution.json`（**由 W2 脚本生成**；17 条） | 新文件 |
| **W4** | 影子根预演 + 逐份复验 + 闸 0 回归（**归档前跑一次**） | — |

**17 份清单**（D3 域 · dGPU 板级 / SoC）：`dgpu-4ip-port-contract` · `dgpu-board-abi-hygiene` · `dgpu-board-adapter-info` · `dgpu-board-mvp` · `dgpu-soc-control-hub` · `dgpu-soc-physical-ip-port-boundary` · `dgpu-soc-timing` · `driver-visible-minimal-soc` · `gmmu-pte-wf` · `lwc-bus-hub-mvp` · `minimal-dgpu-soc-abi-landing` · `minimal-dgpu-soc-architecture` · `minimal-dgpu-soc-test-coverage` · `sdma-engine-tlm` · `soc-internal-noc` · `soc-internal-noc-data-flow` · `vram-controller-functional-mode`

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | 17 份 Purpose **限段**后 `TBD`/`TODO` 计数 = **0** |
| **AC-02** | **定向**：17/17 份的 `--strict --json` 输出中**无 Purpose 类 issue**（永远可达） |
| **AC-02b** | **全局**：`pre − post = k`（k = 修完整份转绿的份数），且 k 与剩余红数**逐份归因闭合**（数字不写死，但必须闭合） |
| **AC-03** | **Purpose 四条正交（逐份机判）**：① `--json` 判 valid ② `50 ≤ len ≤ 160` **且单物理行** ③ **CJK-only LCS < 12** + **英文词序列公共 < 5 词** ④ ⛔ 过程叙述 / 技术字面量 / change 引用 / 日期 |
| **AC-04** | **零语义改动 + diff 形状写死**：**17 文件 · 每文件恰 1 hunk · 删 1 行增 1 行 · 不新增空行**；`## Requirements` 段**逐字节不变** |
| **AC-05** | **不得声称不存在的能力**：`claim_kind` 与 **spec 级** `Superseded` 标记**互斥**；Purpose 标识符 ⊆ Requirement 标题 ∪ 仓内可 grep 符号 ∪ 领域缩写白名单；`source_evidence` 全部 `isfile` **且**按 `evidence_kind` 命中 |
| **AC-06** | `cep-*` 不受影响：43 份逐份 `--strict` ⇒ rc≠0 = 0；`artifact_count` **86 不变**（零冻结产物 ⇒ 免五步链） |
| **AC-07** | 范围零 diff：`include/**` / `src/**` / `test/*.cc` / `docs/adr/**` / `openspec/specs/cep-*/spec.md` 全 0 |
| **AC-08** | 闸 0 回归（**归档前一次**）：45 闸全 rc=0（ctest 同参数）· `ctest -R cep` / `[cep]` == T0 |
| **AC-09** | KPI **0/3 → 1/3** → **0/3** + 归档 change 数 **150 → 151** |
| **AC-10** | **归档语义可判别**：出口 **A**（实测收敛为唯一路径）⇒ 须落 **proposal 顶部标记行** + 归因 JSON `archive_semantics` 字段 ⇒ 归档后目录**可判别**，⛔ 不得与 ⏸ 暂存归档同形无标记 |

## 归档语义（⚠️ 归档前**备案**，不阻塞）

**实测（批 1 影子根实验 + 仓内校验，非推断）**：① delta 的 `## Purpose` **被忽略** ⇒ 工具要求**直接编辑 main spec** ⇒ **0 份 delta 是唯一路径**；② `validate --changes --strict` 对 0-delta change **会红** ⇒ `.openspec.yaml` 的 `skip_specs: true` 是**仓内 strict 校验保持绿的必要条件** ⇒ **出口 B（不走归档器）会破坏校验基线** ⇒ **出口 A 唯一可行**；③ 工具对该声明的 INFO 措辞 = 「change declares **no spec-level behavior changes**」⇒ 与本 change（改**表述**、⛔ 未改任何**可验证行为**）**精确匹配**。

⚠️ **可判别性缺口**：归档后目录形态「无 `specs/` + `skip_specs: true`」与 ⏸ 暂存归档**结构同形** ⇒ 须靠 **标记行 + 归因 JSON 字段 + 台账 Entry** 区分。

## 不做什么

① 不裁 Requirement 正文（批 2）· ② 不动 Requirement / Scenario 的任何字节 · ③ 不动 43 份 `cep-*` · ④ 不动实现层 · ⑤ 不改既有 45 闸 · ⑥ 不发明实现中不存在的能力 · ⑦ 不动其余 42 份 Purpose（批 4~6）· ⑧ 不处理 R-9~R-15（登记）· ⑨ 不裁决 R-2 / R-9 / B3b-2 / DoD 7 口径 · ⑩ 不改 `AGENTS.md`。
