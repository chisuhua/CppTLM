# design: cpptlm-cep-spec-strict-compliance

> **角色**: builder · **上游**: `openspec/changes/cpptlm-cep-spec-strict-compliance/proposal.md` + improvement v2（metis 10 MUST + 7 SHOULD + 6 NIT 全处置）· 台账 **Entry #434**
> **范围**: 12 份 spec 修到 `--strict` 0 红 + 防退闸 · **零实现层改动 · 零 ADR 改动 · 零归档件改动**

## D-1 · 修法 = **正文只放规范核心，细节下沉 Scenario，过程记录回台账**

`--strict` 的判据原文是「Move examples and edge cases into scenarios, or split it into separate requirements that **each state one behavior**」。据此：

| 手法 | 用在哪 | 为什么不用别的 |
|---|---|---|
| **删过程引用块**（`> **本次修订…**` / `> **订正(2026-11-15)**`） | `cep-sharer-tracker-base` · `cep-invariant-gate` · `cep-core-contracts` · `cep-collective-plane-contract` | 这类内容按本仓文档卫生纪律本就**不该进契约正文**；它们正是本轮腐化的成因 |
| **去重**（正文重复 Scenario 已载的条款） | `cep-ualink-coherence-engine` 的三分法表 + 4 条不变量（785 字符） | 已由既有 Scenario **byte-identical** 承载：不变量 1/2/3 → `UGE-A` + `UGE-C`，不变量 4（`completion_unmatched == 0`）→ `UGE-B` ⇒ 正文重复无增量判别力 |
| **下沉**（正文规范句 → 新 Scenario） | `cep-ualink-coherence-engine` 的 3 条**无场景覆盖**的规范句 | ⛔ 不能删 —— metis Q2 实测 `UGE-A~E` 全无覆盖，删掉就是**削弱契约**；另补 `UGE-I` 承载「零 UPLI 通道级语义」的操作定义 |
| **保留**（规范句留在正文） | `cep-core-contracts` 的 `SHALL NOT 在标题内嵌数量` | 它是该约束的**唯一载体**，删整块会丢 |
| **⛔ 不拆分 Requirement** | 全部 5 份 | 拆分改变契约面的 Requirement 计数与语义边界；下沉到 Scenario 是**纯结构搬移** |

**⚠️ 一处不可回避的改写**：`cep-ualink-coherence-engine` 从 1931 压到 ≤500，**纯删除达不到**（实测：真正重复的只有 785，去重后仍剩 ≈1156）⇒ 必须压缩措辞。这类句子在 `removals.json` 标 `kind: reworded`，`covered_by: text:<新句 sha256>`（可机判：该新句必须存在于终态）。

## D-2 · Purpose = **直接改 main spec**（⛔ 不走 delta）

openspec 的 message 原文：`Replace it with what this capability is for, **editing the main spec directly**: a '## Purpose' in a delta is read only when the capability is created, so it cannot replace this one.`

⇒ 8 份直接改；写「该 capability **为什么存在**」，⛔ 不复述 Requirement 首句，⛔ 不含过程叙述（`本 change` / `2026-11-15` / `Entry #43*`）。

## D-3 · 闸 = **零容忍 + 覆盖对账 + 环境 exit 2 + 版本固化 + 反向 fixture**

`scripts/test/cep_spec_strict_check.sh`：

| 段 | 判据 | 为什么需要 |
|---|---|---|
| ① 零红 | `openspec validate --specs --strict --json` 过滤 `cep-` ⇒ `invalid == ∅` | ⛔ **不登记基线红** —— 登记红值等于把判据抹掉 |
| ② 覆盖对账 | `items` == `openspec/specs` 目录数（129）∧ `cep-` 项 == `cep-*` 目录数（43） | 防「工具静默跳过某些 spec」⇒ 漏检可查（⛔ 不是「自愈」） |
| ③ 环境 exit 2 | `command -v openspec` 失败 ⇒ rc=2 | 先例：`cep-sharer-tracker-base/spec.md:249`「编译失败 SHALL 归类为环境错误并以 `exit 2` 退出」 |
| ④ 版本固化 | 打印 `openspec --version` + 阈值（500/50）；**版本仅记录**，漂移走独立裁决（R-6），⛔ 脚本不自作判断 | 判据 owner 在仓外 ⇒ 本闸是变更检测器，⛔ 不自行改判据 |
| ⑤ 反向 fixture（**两条毒化**） | 影子根毒化一份**基线已绿**的 spec：**A** Purpose 占位 ⇒ **B** Requirement 正文 >500 字符；各自断言 `invalid_ids == 基线(∅) ∪ {毒化 id}` 且**恰好 +1** | A 验 Purpose 判据；**B 验 >500 判据 —— 正是本轮腐化的真实模式**（只验 A 会让「工具静默移除长度检查」的复发逃逸） |

**⚠️ fixture 的时序依赖**（metis Q4）：fixture **只能在 12 份全绿之后**才有意义 ⇒ 实施顺序 = 先修 spec、后跑 `--fixture`，最后才注册 ctest。

## D-4 · 影子树 **必须**走共享 helper（冻结契约硬约束）

`openspec/specs/cep-invariant-gate/spec.md:13` + `:115` 明文：「影子树机制由共享 helper（`scripts/test/cep_test_shadow.sh`）承载 ⇒ ⛔ 不存在第二份实现」+「任一闸 SHALL `source` 并调用 `reset <shadow> [extra_cp...]`」。

⇒ 本 change **不内联 `cp -r`**，而是把 helper 的 `extra_cp` 扩展为支持目录（`cp` → `cp -r`；对**文件**行为完全相同）。⚠️ 该 helper 被 `cep_adr_structural_check.sh` 与 `cep_decision_block_check.sh` 共用 ⇒ 改动后**必须回归这两闸**（AC-14）。

调用形态：`cep_shadow_reset "$TMP/shadow" openspec` ⇒ 影子根有 `openspec/` 整树 ⇒ `cd $TMP/shadow && openspec validate ...`（`root.source = nearest`）。

## D-5 · `removals.json` = 规范句移除清单（**逐句归因**）

```json
{ "schema": "cep-spec-removals-v1",
  "authorized_by": "Entry #434",
  "entries": [ { "spec": "<capability>", "sentence_sha256": "<sha256 of baseline sentence>",
                 "kind": "process|interpretive|moved|reworded",
                 "covered_by": "scenario:<标题> | ledger:Entry #N | text:<sha256>" } ] }
```

- `process` = 过程日志（回台账 ⇒ `covered_by: ledger:Entry #434`）；
- `interpretive` = 解释性句，语义已由场景断言覆盖（`covered_by: scenario:<标题>`）；
- `moved` = 原样移到新 Scenario（`covered_by: scenario:<标题>`）；
- `reworded` = 措辞压缩（`covered_by: text:<新句 sha256>`）。

判据（AC-04）：`sentence_sha256` 集合 **==** 「基线 − 终态」的 SHALL/MUST 句差集（**集合相等**，⛔ 非 ⊆）。

## D-6 · 归档链 = **7 步**（顺序固定，含影子根预演）

| 步 | 动作 | 为什么非要这个顺序 |
|:--:|---|---|
| ① | 台账 Entry #434 | 快照/清单的 `authorized_by` 必须指向**已存在**的 Entry |
| ② | Purpose 8 份 + 5 份 delta 落盘 | — |
| ③ | **影子根预演 `openspec archive`** | 真仓 archive 会**重写整份 spec**；预演能提前暴露「场景被丢 / 场景被改 / diff 溢出目标段」 |
| ④ | 真仓 `openspec archive` + 复验 0 红 | delta 合并**必须**在产物 commit 之前（否则 commit 的产物不是终态） |
| ⑤ | 产物 commit | `--update` 取 `git log -1` 作 `frozen_at_commit` ⇒ 必须在 commit **之后**才跑 |
| ⑥ | `--update --authorized-by "Entry #434"` | 全量重算（不分步）；清单 diff 只应有 12 份 hash + 3 个元数据键 |
| ⑦ | 清单单独 commit + 无参复验 | 五步链既有形态 |

⚠️ **`artifact_count` 不变（86）**：本 change 不新增/删除冻结文件，只改内容。
⚠️ **`frozen_at_commit ≠ HEAD` 是常态告警** ⇒ ⛔ 不得重跑 `--update`。

## D-7 · 边界（与 proposal 一致，零放宽）

⛔ `include/**` / `src/**` / `test/*.cc` 零 diff · ⛔ `docs/adr/**` 零 diff · ⛔ **归档件零 diff**（含上一 change 的 AC-09 原文）· ⛔ 新增 `INV-CEP-*` = 0 · ⛔ 新增 spec Requirement = 0 · ⛔ 不改任何**既有** Scenario · ⛔ 不用闸外快照登记基线债 · ⛔ 不动非 `cep-*` 的 spec · ⛔ 不发明 `rddf feedback`。

## 下沉清单（`UGE-F/G/H/I` —— AC-07 的登记载体）

| 新场景 | 承载的被删正文 | 共享核心 token |
|---|---|---|
| `UGE-F` | `:29` 「`txn_id` SHALL 为在途集合的**唯一键**（⛔ 不得以地址 / 到达序 / 任何派生量作键）」+ 「J1 的 `p2p_ordered` SHALL **NOT** 作为引擎行为前提」 | `txn_id` · `p2p_ordered` |
| `UGE-G` | `:33` 「引擎 SHALL **NOT** 为 `response` 消息类 / collective 结果回传路径新增任何 handler（承载者仍是 `FabricCollectivePlane`）」 | `response` · `FabricCollectivePlane` |
| `UGE-H` | `:35` 「**不**改变 `ualink_4` 的 13 键声明」+「B3b-2 维持 0/4」+「`test/cep_b3b2_deferrals.json` 的 4 族登记完整」 | `13 键` · `cep_b3b2_deferrals.json` |
| `UGE-I` | 首段括号「`ReqCmd` 编码 / 4 通道时序 / `transaction tag` 配对 **均不实现**」 | `ReqCmd` · `transaction tag` |
