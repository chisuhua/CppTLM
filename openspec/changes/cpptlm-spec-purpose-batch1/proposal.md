# proposal: cpptlm-spec-purpose-batch1 —— R-1 批 1（16 份非 CEP 主 spec 的 Purpose 占位修复）

> **状态**: 🟡 Proposed（2026-11-15 立项 · planner 起草 · metis 预审 7 MUST + 8 SHOULD + 5 NIT 全处置 · 待 builder 实施）
> **依据**: improvement `.rddf/improvements/cpptlm-spec-purpose-batch1.md` · 台账 **Entry #441**
> **KPI**: 立项后 Proposed **0/3 → 1/3**（**归属已裁决**：改 `openspec/specs/` 契约面 ⇒ **不适用** doc-only 豁免 ⇒ 计入）

## Why

**全仓 129 份主 spec 里有 75 份的 `## Purpose` 是 `openspec archive` 自动写的占位**（`TBD - created by archiving change …`），归档后从未被人改写。

后果：① 人读规范不知道该能力管什么；② `openspec validate --strict` 把占位判为 WARNING ⇒ **非 `cep-*` 的 77 份 `--strict` 红里 75 份因此红**（R-1 主体）；③ 工具对新能力的 Purpose 有明确质量要求（≥50 字符、非占位），而这批能力归档时**没走 delta** ⇒ **工具自己留了坑、人没填**。

**⚠️ 关键澄清**：本 change **不新增任何能力** ⇒ 写 Purpose 属「授权契约内容」，但口径**收窄**为「**忠实记录既有实现**」：
- **事实基础** = 来源 change 的 proposal「为什么」段（实施期写下的意图陈述）+ 该 spec 的 Requirement 标题（能力面）⇒ 二者都是**实施时既有事实**。
- 逐份 canonical 草稿 + **写作规则 D-0** 见 `design.md`（**planner 起草**，builder 落地）。
- **superseded 一律移出**：本批 **16** 份（原 17 份中 `cpptlm-d1-p1-pipeline-scoreboard` 已 SUPERSEDED 且实现物理删除 ⇒ 写现状即**立假账** ⇒ 登记 R-10）。

**为什么只做 Purpose、不动正文**：Purpose 段与 Requirement 段是**两个独立段** ⇒ 改动**不触及任何可验证行为** ⇒ 不改变任何判据的判别力。而 **70 份正文 >500 字符的裁剪会动 Requirement 段** ⇒ **另一档**风险，**必须另立 change**。

## What Changes

| # | 内容 | 载体 |
|:--:|---|---|
| **W1** | **16 份**主 spec 的 `## Purpose` 段：占位 → 实质 Purpose（草稿见 design §D-1，规则见 §D-0） | **直接编辑主 spec**（⛔ **0 份 delta** —— 归档器实测原话 `delta Purpose ignored; … Edit … spec.md directly`） |
| **W2** | **1 个校验脚本** `scripts/test/cep_spec_purpose_batch1_check.py`（D-3 的 C-1~C-7 机判项：限段占位 / 长度与单行 / **LCS<12** 不复述 / 禁过程叙述与字面量 / `claim_kind` 互斥 / 标识符存在性 / 归因闭合） | 新文件 |
| **W3** | **1 份归因载体** `test/spec_purpose_batch1_attribution.json`（**由 W2 脚本生成**，人只填原始输入） | 新文件 |
| **W4** | 影子根预演 + 逐份复验 + 闸 0 回归（**归档前跑一次**） | — |

**16 份清单**（`pcie-*` 10 + `cpptlm-*` 6）：`pcie-axi-datapath-hardening` · `pcie-cfg-encoding-minor` · `pcie-flr` · `pcie-host-test-flr-rebar` · `pcie-host-test-infrastructure` · `pcie-ip-integration` · `pcie-ip-microarch` · `pcie-resizable-bar-runtime` · `pcie-slice-prerequisites` · `pcie-tlp-wire-datapath` · `cpptlm-pcie-endpoint-ip-json-config` · `cpptlm-pcie-ep-foundation` · `cpptlm-stage-1-1-fixes` · `cpptlm-stage-1-2-msix` · `cpptlm-stage-1-4-2-1` · `cpptlm-stage-1-4-2-1-ue-extensions-unblock`

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | 16 份 Purpose **限段**后 `TBD`/`TODO` 计数 = **0** |
| **AC-02** | **定向**：16/16 份的 `--strict --json` 输出中**无 Purpose 类 issue**（永远可达） |
| **AC-02b** | **全局**：`pre − post = k`（k = 修完整份转绿的份数），且 k 与剩余红数**逐份归因闭合**（数字不写死，但必须闭合） |
| **AC-03** | **Purpose 四条正交（逐份机判）**：① `--json` 判 valid ② `50 ≤ len ≤ 160` **且单物理行** ③ 规范化后与该 spec 全部 Requirement 正文的 **LCS < 12**（`waiver: single-requirement` ⇒ < 16 + 强制「为什么需要这条交付判据」起句）④ ⛔ 过程叙述 / 技术字面量 / change 引用 / 日期 |
| **AC-04** | **零语义改动 + diff 形状写死**：**16 文件 · 每文件恰 1 hunk · 删 1 行增 1 行 · 不新增空行**；`## Requirements` 段**逐字节不变** |
| **AC-05** | **不得声称不存在的能力**：`claim_kind` ∈ {`current`,`historical`,`superseded`} 且与 spec 内 `Superseded`/`物理删除` **互斥**；Purpose 标识符 ⊆ Requirement 标题 ∪ 仓内可 grep 符号；`source_evidence` 全部 `isfile` == True |
| **AC-06** | `cep-*` 不受影响：43 份逐份 `--strict` ⇒ rc≠0 = 0；`artifact_count` **86 不变**（零冻结产物 ⇒ 免五步链） |
| **AC-07** | 范围零 diff：`include/**` / `src/**` / `test/*.cc` / `docs/adr/**` / `openspec/specs/cep-*/spec.md` 全 0 |
| **AC-08** | 闸 0 回归（**归档前一次**）：45 闸全 rc=0（ctest 同参数）· `ctest -R cep` / `[cep]` == T0 |
| **AC-09** | KPI **0/3 → 1/3** → **0/3** + 归档 change 数 **149 → 150** |
| **AC-10** | **归档语义硬门 T3.0**：architect 裁决并落 **出口 A 或 B（二选一）** ⇒ 归档后目录**可判别** ⛔ 不得与 ⏸ 暂存归档同形无标记 |

## 归档语义（⚠️ 归档前硬门 T3.0 · 需 architect 裁决）

**实测（本轮影子根实验，非推断）**：
1. delta 的 `## Purpose` **被忽略** ⇒ 归档器要求**直接编辑 main spec** ⇒ **0 份 delta 是唯一路径**。
2. change 无 delta 时归档器要求 `.openspec.yaml` 设 `skip_specs: true`；实测该声明 ⇒ 归档**无警告** + main spec **零改动** + 正常归档。

⚠️ **可判别性缺口**：归档后目录形态「无 `specs/` + `skip_specs: true`」与 ⏸ 暂存归档**结构同形** ⇒ 半年后 grep `skip_specs` 无法区分。

⇒ **两条互斥出口**（⛔ 不许两条都做）：
- **出口 A**：`skip_specs: true` + **proposal 顶部**加「**✅ 已完成归档 · 主 spec 直改（无 delta）**」标记行 + 归因 JSON `archive_semantics: "completed-direct-main-spec"`。
- **出口 B**：**不走**归档器，按 doc-only change 手工 `mv` 进 `archive/`，台账显式记「**未走归档器**，理由：Purpose 无法走 delta（工具明文）」。

⚠️ **失败代价（必须认清）**：两条都不行 ⇒ 本 change 无法归档 ⇒ **KPI 卡在 1/3** ⇒ 后续提案全被 3/3 上限阻塞。

## 不做什么

① 不裁 Requirement 正文（批 2）· ② 不动 Requirement / Scenario 的任何字节 · ③ 不动 43 份 `cep-*` · ④ 不动实现层 · ⑤ 不改既有 45 闸（新增的校验脚本属本 change 自测工具）· ⑥ 不发明实现中不存在的能力 · ⑦ 不动其余 59 份 Purpose（批 3~5）· ⑧ 不处理 R-10~R-14（登记）· ⑨ 不裁决 R-2 / R-9 · ⑩ 不改 `AGENTS.md`。
