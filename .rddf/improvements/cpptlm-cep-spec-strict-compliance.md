---
name: cpptlm-cep-spec-strict-compliance
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-00
  - ADR-CEP-01
  - ADR-CEP-06
状态: 🟡 **已立项**(2026-11-15 · metis 第 1 轮 **10 MUST + 7 SHOULD + 6 NIT 全处置**;其中 **2 条推翻了我的初稿事实判断** —— ① ualink「纯去重」算术不成立 ② 影子树内联 `cp -r` 违反冻结契约 · change `cpptlm-cep-spec-strict-compliance` 已建 · KPI **0/3 → 1/3** · **待 builder 补 design/tasks 并实施**)
created: 2026-11-15
---

# improvement: cpptlm-cep-spec-strict-compliance

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**:⛔ 不改 `include/**` / `src/**` / `test/*.cc`(本 change 零实现层改动)· ⛔ `docs/adr/**` 零 diff · ⛔ 不新增 `INV-CEP-*` 标识 · ⛔ 不新增 spec Requirement(**只 MODIFIED + 直接改 Purpose**)· ⛔ 不动非 `cep-*` 的 spec · ⛔ **12 份 spec 的终态文本 SHALL NOT 含本 change 的过程叙述**(禁 `本 change` / `2026-11-15` / `Entry #434` —— 这**正是**本轮腐化的成因)。
> **估时**:**实施 2.5 人日 + 归档链 0.5 人日**(metis SHOULD-17:12 份契约改写 + 1 闸 + 反向 fixture + 12 份冻结产物;原估 1.5 低估了 `cep-ualink-coherence-engine` 的下沉工作量)
> **KPI**:占 OpenSpec slot **1/3**
> **审查**:metis 第 1 轮(10 MUST + 7 SHOULD + 6 NIT **全处置**,见文末对照表)

---

## 架构依据

| 依据 | 来源（一手） | 本 change 用到的决策要点 |
|---|---|---|
| **`--strict` 三条判据** | `@fission-ai/openspec dist/core/validation/validator.js:716-745` + `constants.js:6,9` | ① Purpose 占位 ② Purpose < **50** 字符 ③ Requirement `text` > **500** 字符 |
| **`text` 的口径** | `dist/core/parsers/requirement-text.js` | 取 `### Requirement:` 后到**第一个 `^#{1,6}\s`**（非围栏内）之间的行；**跳空行** / **跳围栏内行** / 跳 `**xxx**:` 元数据行(仅当其余正文非空) ⇒ **`> 引用块` 与表格行都计入**。planner 已 Python 复刻并**实测逐条一致**（5 份 / 同名 Requirement） |
| **⚠️ 工具的不对称（回归根因）** | `validator.js:266-276` 注释原文：`MODIFIED is left alone: its text is the existing requirement, which the specs instruction says to keep whole.` | delta 的 `ADDED` 被查 500，**`MODIFIED` 不查**；main spec 被查 ⇒ **超长只能在归档合并后暴露，delta 阶段零告警** |
| **Purpose 的修正渠道** | `validator.js:722` message 原文：`Replace it with what this capability is for, **editing the main spec directly**: a '## Purpose' in a delta is read only when the capability is created, so it cannot replace this one.` | **Purpose 必须直接改 main spec，⛔ 不能走 delta** |
| **Requirement 的修正渠道** | `openspec archive` 的 MODIFIED 合并语义 | 按**场景名精确匹配**，archive **拒绝丢弃**既有场景名 ⇒ delta 块 SHALL 含**整段**（正文 + 全部既有 Scenario 逐字）+ 与既有场景名集合相等 |
| **影子树 = 共享 helper（硬约束）** | `openspec/specs/cep-invariant-gate/spec.md:13` + `:115`（**冻结契约**） | 「影子树机制由共享 helper（`scripts/test/cep_test_shadow.sh`）承载 ⇒ ⛔ 不存在第二份实现」+「任一闸 SHALL `source` 并调用 `reset <shadow> [extra_cp...]`」⇒ **新闸不得内联 `cp -r`** |
| **轴 2 的治理来源** | `obj-cep-platform.md:18150`（architect，2026-11-10，Entry **#324**） | 「**轴 2 达标度**（Purpose 非占位符 / Scenario ≥1 / 无悬空锚点）= **独立** remediation 批次 · ⛔ 不入冻结判据 · ⛔ 不阻断轴 1」⇒ 本 change = 该批次的**子集**（只做 `--strict` 可检的部分） |
| **归档件不可变** | 仓内纪律（Entry #148 的 S1 教训） | 上一 change 的 AC-09 原文「max 298/348」**已失实但不得回改** ⇒ 只能由台账 Entry **认领** |
| **先例：反向 fixture 只写临时目录** | `scripts/test/cep_adr_verification_carriers_reverse_verify.sh`（上一 change） | 「`mktemp -d` + `sha256sum` 前后比对」形态 ⇒ 本 change 照抄 |

## 范围

**In Scope**:

**W0 · 事实基线**：`cep-*` = **43** 份，其中 `--strict` **12 份 rc=1**。**12 = {5 超长} ∪ {8 Purpose 占位}**（`cep-ualink-coherence-engine` 计入两边，主键 = 12 个目录名）。

**W1 · 12 份修到 0 红**（**内部定序固定，⛔ 不得换序**）:

| 序 | spec | 类 | 现状 | 修法 | 语义风险 |
|:--:|---|---|:--:|---|:--:|
| **W1a** | `cep-sharer-tracker-base` | 超长 | 604 | **删**「本次修订」过程引用块（345 字符，纯过程日志）⇒ 目标 ≈ 260 | **零**（纯删日志） |
| **W1a** | `cep-invariant-gate` | 超长 | 811 | **删**「本次修订 / 边界 / 不自愈」三条过程引用块（≈460 字符）⇒ 目标 ≈ 350 | **零** |
| **W1b** | `cep-collective-plane-contract` | 超长 | 517 | **删** `:29-30` 的「本次修订」引用块（116 字符）⇒ **401** | **零** |
| **W1c** | `cep-core-contracts`（Req `cep-sharer-tracker-contract`） | 超长 | 697 | **删**「订正(2026-11-15)」+「Scenario 标题沿用历史命名」过程块，但 **SHALL 保留** 其尾部的规范句 `SHALL NOT 在标题内嵌数量` 于正文 | **低**（须保留 1 条规范句） |
| **W1d** | `cep-ualink-coherence-engine` | 超长 + Purpose | 1931 | **去重 785**（三分法表 387 + 4 条不变量 398 —— 已由 `UGE-A` 承载）+ **下沉 3 条无场景覆盖的规范句为新 Scenario**（见下） | **高** ⇒ 单独审查轮 |
| **W4** | 8 份 Purpose 占位 | Purpose | `TBD - created by archiving …` | 直接改 main spec，写「该 capability 为什么存在」（≥50 字符） | **零** |

**⚠️ W1d 的 3 条规范句（metis Q2 实测：**UGE-A~E 全无覆盖**，删掉 = 削弱契约）**:

| 原正文位置 | 规范内容 | 归宿 |
|---|---|---|
| `:29` | `txn_id` SHALL 为在途集合的**唯一键**（⛔ 不得以地址 / 到达序 / 派生量作键）。⚠️ `UGE-B` 明写「本 scenario SHALL NOT 被读作覆盖…**键型 SHALL 由代码评审 + 头文件注释保证**」⇒ 确实无覆盖 | **新 Scenario `UGE-F`** |
| `:33` | 引擎 SHALL NOT 为 `response` 消息类 / collective 结果回传路径**新增任何 handler**（承载者仍是 `FabricCollectivePlane`） | **新 Scenario `UGE-G`** |
| `:35` | 本 requirement **不**改 `ualink_4` 的 **13 键**声明（逐字不动）∧ B3b-2 维持 0/4 | **新 Scenario `UGE-H`**（`UGE-E` 只查 `flow_support` 8+2 与 `perm`，**不含**其余键） |

**W2 · 防退闸**（新增 `scripts/test/cep_spec_strict_check.sh`）：
- **主判据**：`openspec validate --specs --strict --json`（实测 **1.3s** 全 129 份）⇒ 过滤 `id.startswith('cep-')` ⇒ **断言 invalid 集合 == ∅**；
- **覆盖对账**：`items` 数 == `ls openspec/specs | wc -l`（129）∧ `cep-` 项数 == **43**（防静默漏检）；
- **归因**：失败时逐份打印 `id` + `issues[].path` + `message`（⛔ 不只给总数）；
- **环境缺失 ⇒ `exit 2`**（`command -v openspec` 失败；⛔ 不得当 pass，先例 = `cep-sharer-tracker-base/spec.md:249`）；
- **判据版本固化**：输出含 `openspec --version` + 阈值（500 / 50）；major 漂移打 `!` 警告（⛔ 不自行改判据）；
- **反向 fixture**：`source scripts/test/cep_test_shadow.sh` ⇒ `cep_shadow_reset <tmp> openspec` ⇒ 在影子根毒化一份**基线已绿**的 spec 的 `## Purpose` 为 `TBD` ⇒ 闸**必红**且 `invalid_ids(毒化后) == invalid_ids(基线) ∪ {毒化 id}`（集合等式，⛔ 非「输出含该 id」）；未毒化态必绿；**⛔ 仓内零改动**（`openspec/` + `docs/adr/` 的全量 sha256 manifest 前后相等，⛔ 不用 `git diff --quiet`）。

**W3 · 5 份 delta**（5 个 `MODIFIED Requirements`）：正文裁到 ≤500 + **既有 Scenario 逐块 byte-equal**（ualink 另加 `UGE-F/G/H`）。
**W4 · 8 份 Purpose**（直接改 main spec；⛔ 不进 delta）。
**W5 · 文档连带**：`scripts/README.md` 守门表 +1 行 · `test/CMakeLists.txt` 注册 ctest · `AGENTS.md` 契约闸数 **44 → 45**。
**W6 · 台账 Entry #434**（planner 域，append-only）：含**回归认领**（上一 change 的 AC-09 失实，原文不回改）。
**W7 · `scripts/test/cep_test_shadow.sh`**（改）：让 `extra_cp` 支持目录 ⇒ `cp` → `cp -r`（对文件行为完全相同）⇒ 供 W2 复用。⚠️ 该 helper 被 2 个冻结契约闸共用 ⇒ **改动后须回归那 2 闸**。

**Out of Scope**:

- ⛔ **不动非 `cep-*` 的 75 份红 spec**（实测 2026-11-15：全仓 129 份中 **87** rc=1 = 非 cep **75**（全为 Purpose 占位）+ cep **12**）⇒ 登记 R-1。
- ⛔ **不裁决 G-4 / G-5**（镜像 20 值跨仓权威 / ArchForge 缺席整闸 `exit 0`）⇒ architect 判权。
- ⛔ **不改** 9 个闸的固定影子路径默认值 / 决策块闸注释 stale / 覆盖集合无自愈 ⇒ backlog。
- ⛔ **不用闸外快照登记基线债**（见 §Why 第 3 条）。
- ⛔ **不新增 Requirement**（超长的 5 份走「正文下沉 Scenario」，⛔ 不拆分 Requirement）。
- ⛔ **不改任何既有 Scenario 的语义或标题**（新增场景见 W1d，须登记）。
- ⛔ **不改归档件**（`openspec/changes/archive/**` 零 diff）—— 含上一 change 的 AC-09 原文。
- ⛔ **不发明 `rddf feedback` 通道**（本仓无此机制；唯一渠道 = 台账 append-only Entry）。

## Why

**第 1 条 —— 12 份红里 2 份是本轮上一 change 引入的回归，且工具**结构上**不会在 delta 阶段告警。**

| spec | Requirement | 归档前 | 现在 |
|---|---|:--:|:--:|
| `cep-sharer-tracker-base` | `responder 位宽扩容的机制与触发判据` | **89** | **604** |
| `cep-invariant-gate` | `cep-invariant-gate-decision-block-and-residual-check` | **348** | **811** |

**为什么当时没被发现**：`openspec validate <change> --strict` 对 **`MODIFIED` 块不作长度检查**（工具注释原文 `MODIFIED is left alone`）⇒ delta 阶段**零告警**；main spec 的长度检查**只在归档合并后才成立**，而**仓内没有任何闸跑逐份 `--strict`** ⇒ **落地即静默腐化**。上一 change 的 AC-09 写「归档后逐份 rc=0（max 298/348）」—— 那是**归档前**的估值，归档后未复跑。

⇒ 本 change 的**第一价值不是修这 12 份，而是补上让第 13 份无法静默出现的闸**。⚠️ **诚实边界**：闸的收益在**未来**；实施期它必须是**最后**变绿的产物（中间 commit ⛔ 不得带红用例 ⇒ ctest 注册放最后一个产物 commit）。

**第 2 条 —— 超长本身就是契约质量缺陷：过程日志混进了规范正文。**

实测 5 份的构成：

| spec | 长度 | 其中「过程日志」占比 | 结论 |
|---|:--:|---|---|
| `cep-invariant-gate` | 811 | **≈460**（三条「本次修订 / 边界 / 不自愈」引用块） | 删日志即达标 |
| `cep-sharer-tracker-base` | 604 | **≈345**（「本次修订」引用块） | 同上 |
| `cep-core-contracts` | 697 | ≈580（「订正(2026-11-15)」+「Scenario 标题沿用历史命名」），但**尾部含规范句** | 删日志 + 保留规范句 |
| `cep-collective-plane-contract` | 517 | **116**（`:29-30`） | 删日志即达标（**401**） |
| `cep-ualink-coherence-engine` | 1931 | **785**（重复 `UGE-A` 的表 + 不变量）+ **3 条无覆盖的规范句** | **去重 + 下沉**（⛔ 不是纯删） |

⇒ 按本仓文档卫生纪律（「过程日志禁止进文档」），修法 = **正文只放规范核心（≤500），细节下沉 Scenario，过程记录回台账**，语义零变更。

**第 3 条 —— 为什么不用「闸外快照登记基线债」。**

既有先例（`test/cep_decision_block_snapshot.json` + `authorized_by`）是「把当前值登记成期望值」。用在**计数**上合理（计数会变，需显式授权），但用在**质量判据**上**方向相反**：登记 12 份红 = 把「不应该红」写成「允许红」⇒ **等于抹掉判据**。⇒ 本闸**不接受任何基线豁免**，判据 = **绝对 0 红**；代价 = 必须把 12 份全修（这也是单 change 而非碎片的原因）。

**第 4 条 —— 认识论边界（⛔ 不得过度声称）。**

- **判据的 owner 在仓外**：500 / 50 两个阈值由第三方工具持有 ⇒ 本闸是**变更检测器**，**不是**判据权威。一次工具升级即可改变闸的语义 ⇒ 闸输出必须打印工具版本（W2），major 漂移走独立裁决（R-6）。
- 本 change 让 `cep-*` 的 `--strict` **0 红**，**不**论证「场景划分最优」，只保证「规范核心在正文 / 细节在场景」这条结构约束成立。
- 本闸覆盖 = **目录名以 `cep-` 开头**；一个叫 `coherence-xyz` 的新 spec 会逃逸 ⇒ 登记 R-1，⛔ 不自称「全仓防腐化」。
- 「语义零变更」**不靠机器自证**：机器只防「偷偷改」（byte-equal + 移除清单 + token 表），**删掉的到底是什么**须由 **critic（arch 档）逐条比对**（W1d 单独一轮）。

## What Changes

1. **5 份 delta**（`openspec/changes/cpptlm-cep-spec-strict-compliance/specs/<capability>/spec.md`）—— 各 1 个 `MODIFIED Requirements` 块：**整块复制既有 Requirement 原文**（含全部 Scenario 逐字、含顺序），只做**定点删除/下沉**；ualink 另加 `UGE-F/G/H`。
2. **8 份 main spec 的 `## Purpose`**（直接改）：写「该 capability 为什么存在」，≥50 字符，⛔ 不得复制 Requirement 首句，⛔ 不得含过程叙述。
3. **`scripts/test/cep_spec_strict_check.sh`**（新）：主判据 + 覆盖对账 + 环境 exit 2 + 版本固化 + 反向 fixture（经 helper）。
4. **`scripts/test/cep_test_shadow.sh`**（改）：`extra_cp` 支持目录（`cp` → `cp -r`）。
5. **`test/CMakeLists.txt`**（改）：注册 `cep_spec_strict_check`（⚠️ **放最后一个产物 commit**）。
6. **`scripts/README.md`**（改）：守门表 +1 行 · **`AGENTS.md`**（改）：闸数 44 → 45。
7. **`openspec/changes/<name>/removals.json`**（新）：规范句移除清单，每条 `{spec, sentence_sha256, kind: process|interpretive|moved, covered_by}`。
8. **台账 Entry #434**（planner 域）。

## Acceptance

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | `cep-*` 逐份 `--strict` **0 红** | `openspec validate --specs --strict --json` 过滤 `cep-` ⇒ `invalid == 0`（⛔ 无双真源：**不用** `for` 循环逐份跑） |
| **AC-02** | 12 份**逐名**核对 | 12 个目录名逐个在 `items` 里 `valid == true` |
| **AC-03** | 覆盖对账（防漏检） | `items` 数 == `ls openspec/specs \| wc -l`（**129**）∧ `cep-` 项数 == **43** |
| **AC-04** | 规范句移除**逐句归因** | `removals.json` 的 `sentence_sha256` 集合 **== 基线−终态的 SHALL/MUST 句差集**（逐条相等，⛔ 非 ⊆ 的弱判据）；非 `process` 类的 `covered_by` 必须可解析（场景标题存在于同 spec，或台账 Entry 命中） |
| **AC-05** | 术语表**不降** | 每份 spec 的受控 token（`txn_id` / `flow_events` / `unsupported_rejected` / `snoop_resp_pending_not_consumed` / `completion_unmatched` / `egress_emitted` / `kResponderMappedBits` / `kMappedResponderMask` / `0xF` / `kResponderBitToNode`）在终态的**出现次数 ≥ 基线** |
| **AC-06** | 既有 Scenario **逐块 byte-equal** | 5 份 target Requirement 段内每个 `#### Scenario:` 块的 sha256 集合**逐块相等**（基线取 `git show <base>:<path>`） |
| **AC-07** | 新增场景**已登记**且可归因 | 若 delta 含新场景，其标题 ∈ `design.md` 下沉清单，且正文**至少 1 个 bullet** 与被删正文句共享核心 token |
| **AC-08** | Purpose 四条正交 | 8 份：①`openspec --json` 判 valid（唯一真源）②≥50 字符 ③**不复制** Requirement 首句 ④**不含** `本 change` / `2026-11-15` / `Entry #43*` |
| **AC-09** | 防退闸主判据 + 反向 | 闸 rc=0；**反向**：fixture 首步断言 `invalid_ids(基线) == ∅`（否则 `exit 2`，⛔ 不是 pass/red），毒化一份**本已绿**的 spec ⇒ `invalid_ids == 基线 ∪ {毒化 id}` 且**恰好 +1** |
| **AC-10** | 反向 fixture 仓内零改动 | `openspec/` + `docs/adr/` 的**全量** sha256 manifest（排序后）前后相等（⛔ 只测目标文件） |
| **AC-11** | 环境缺失 ⇒ exit 2 | `PATH` 去掉 `openspec` 后跑闸 ⇒ rc=**2** 且 stderr 含 `openspec` |
| **AC-12** | 范围零改动 | `git diff --name-only` **不含** `include/` · `src/` · `test/*.cc` · `docs/adr/` · `openspec/changes/archive/`；新增 `INV-CEP-*` = **0**；新增 spec Requirement = **0** |
| **AC-13** | 0 回归（**无魔数**） | `[cep]` 用例数 == 基线记录值（`--list-tests "[cep]" \| wc -l`，**前后对比**）· `ctest -R cep` 全绿且**按 ctest 名单**（⛔ 不用文件 glob —— 含 helper 与实施期必红的 freeze 闸）· `openspec validate --changes --strict` rc=0 · `agents_md_lint` rc=0 |
| **AC-14** | helper 改动的回归 | `cep_adr_structural_check.sh` 与 `cep_decision_block_check.sh`（共用 helper 的 2 闸）改动后全绿 |
| **AC-15** | 五步链 **7 步**（顺序固定） | ① Entry #434 落 ② Purpose 8 份 + delta 落盘 ③ **影子根预演 `openspec archive`**（0 红 + 场景名集合相等 + diff 只落目标段）④ 真仓 `openspec archive` + 复验 0 红 ⑤ 产物 commit ⑥ `--update --authorized-by "Entry #434"`（清单 diff = **12 份** main spec hash + `authorized_by` / `frozen_at` / `frozen_at_commit`，⛔ 无其他 artifact hash 变化，`docs/adr/**` 与 `07-*.md` 零变化，`artifact_count` **86 不变**）⑦ 清单单独 commit + 无参复验 rc=0 |
| **AC-16** | provenance 成立 | `manifest.frozen_at_commit` == 产物 commit 的 `git rev-parse` 值 |
| **AC-17** | 归档后 diff 只落目标段 | 5 份的 `git diff <base> -- openspec/specs/X/spec.md` 的 hunk 行号**全部**落在「目标 Requirement 起 → 下一个 `### Requirement:` 或 EOF」区间 |
| **AC-18** | KPI 回填 | 归档后 **1/3 → 0/3**（6 处活口径 + obj 的「当前 Proposed 并发」句）+ 体量数字（归档 146 → **147**） |

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|:--:|
| **C-1** | `cep-*` 契约面 `--strict` **长期 0 红且漏检可查** | `cep_spec_strict_check.sh`（AC-01/03/09） | ❌ 0 |
| **C-2** | 「Requirement 正文只放规范核心」**可回归**（移除清单 + token 表 + byte-equal） | AC-04/05/06 | ❌ 0 |
| **C-3** | 8 个 capability 的 Purpose **说明为什么存在** | 8 份 main spec（AC-08） | ❌ 0 |

## Impact

| 面 | 影响 |
|---|---|
| **代码** | ⛔ 零改动 |
| **契约** | 12 份 main spec 的**文本结构**改变（语义零变更，由 AC-04/05/06 + critic 逐条保证） |
| **闸** | 新增 1（44 → **45**）· ctest +1 · 共享 helper 小改（2 闸需回归） |
| **冻结面** | **12 份** main spec hash 变化 ⇒ 7 步链；`artifact_count` 不变（86） |
| **既有债** | **认领并登记**上一 change 的 AC-09 失实（⛔ **不**改归档原文 —— 归档件不可变；删除依据 = Entry #431 的记录仍在台账） |

**⚠️ 残余登记（6 项 · 全部**未关闭** · ⛔ 本 change 不做）**:

| # | 残余 | 实测状态 | 路由 |
|:--:|---|---|---|
| **R-1** | **非 `cep-*` 的 75 份** spec 仍 rc=1（全为 Purpose 占位） | 全仓 129 份中 87 红 = 非 cep **75** + cep **12** | 各 owner；本闸不覆盖（逃逸面含「不叫 `cep-*` 的新 spec」） |
| **R-2** | 镜像 20 值（`moesi_gpu`/`mesif`）13 键无仓内 golden | 期望表只覆盖 6 个 `repo_owned` | architect 判权 |
| **R-3** | `cep_decl_alignment_check.sh:40-44` ArchForge 缺席 ⇒ 整闸 `exit 0` | 本机 ArchForge 在位 ⇒ `[RUN]` 7/7 | architect 判权 |
| **R-4** | 9 个闸的固定影子路径默认值 + 决策块闸注释 stale + 覆盖集合无自愈 | 上一 change 已实测出并发假红 | backlog（闸健壮性，另一域） |
| **R-5** | **轴 2 的另两轴**（`Scenario ≥1` / **无悬空锚点**）未闭合 | Entry **#324** 的批次含三轴；本 change 只做 `--strict` 可检的部分 | 另立（须独立判据） |
| **R-6** | `openspec` 工具**版本未 pinning** | 闸打印版本但 CI 不锁版本；major 漂移会改语义 | backlog |

## Non-Goals

- ⛔ 不新增 spec Requirement（5 份**不拆分**，只做正文下沉）。
- ⛔ 不改任何**既有** Scenario 的语义 / 标题（新增场景 = W1d 的 `UGE-F/G/H`，须登记）。
- ⛔ 不改任何 Requirement 的 SHALL **内容**（只搬位置，且逐句进 `removals.json`）。
- ⛔ 不新增 `INV-CEP-*`（编号位 67 / 实际 66 不变）。
- ⛔ 不改已签批 ADR / 不改归档件。
- ⛔ 不用闸外快照登记基线债。
- ⛔ 不动非 `cep-*` 的 spec。
- ⛔ 不裁决 R-2 / R-3（architect）· 不做 R-1 / R-4 / R-5 / R-6。

---

## metis 处置对照表（第 1 轮 · 10 MUST + 7 SHOULD + 6 NIT · 全处置）

| # | metis 条目 | 处置 |
|:--:|---|---|
| **M-1** | ualink「纯去重」事实不成立（去重后仍剩 ≈1156） | ✅ **采纳（推翻初稿）** —— 实测算术成立：去重 785 后仍剩 1156；`UGE-B` 明写「键型 SHALL 由代码评审保证」⇒ 3 条规范句**无场景覆盖** ⇒ 改为「去重 + 下沉为 `UGE-F/G/H`」；Non-Goals 改「不改任何**既有** Scenario」 |
| **M-2** | 影子树内联 `cp -r` 违反冻结契约；须走共享 helper | ✅ **采纳（推翻初稿）** —— 冻结核查：`cep-invariant-gate/spec.md:13` + `:115` 明文要求 SHALL `source` helper。⇒ 新增 **W7** 扩展 helper（`cp` → `cp -r`）+ **AC-14** 回归共用它的 2 闸 |
| **M-3** | AC-03 的 `\b(SHALL\|MUST)\b` 是恒真判据 | ✅ **采纳** —— 删除该条，改为 **AC-04 移除清单逐句归因**（句级集合等式）+ **AC-05 术语表不降** |
| **M-4** | AC-04 只比标题 ⇒ 场景内容被改照样绿 | ✅ **采纳** —— 升级为 **AC-06 逐块 byte-equal**（基线 `git show <base>:`) + **AC-07 新增场景登记** |
| **M-5** | AC-10 漏 `openspec archive` 步 + 无 provenance 断言 | ✅ **采纳** —— 改为 **AC-15 七步** + **AC-16 `frozen_at_commit == 产物 commit sha`** + **AC-17 diff 只落目标段** |
| **M-6** | AC-06 反向 fixture 缺基线前置与集合等式 | ✅ **采纳** —— **AC-09** 加「首步断言 `invalid_ids(基线) == ∅`，否则 `exit 2`」+「`invalid_ids == 基线 ∪ {毒化 id}` 且恰好 +1」；毒化目标固定为**本已绿**的 spec |
| **M-7** | 缺「工具缺失 ⇒ exit 2」与「覆盖规模对账」 | ✅ **采纳** —— 新增 **AC-11**（环境 exit 2）+ **AC-03**（129 / 43 对账） |
| **M-8** | 数字「80 份非 cep」错，应为 **75**，且 80+12≠87 自相矛盾 | ✅ **采纳** —— 实测 `grep -rln 'TBD - created by archiving'` = **83** 总，cep 占 **8** ⇒ 非 cep **75**；全文订正 |
| **M-9** | 「闭环 AC-09 失实」措辞过度 + 归档件不可变 | ✅ **采纳** —— 改为「**认领并登记**（原文 ⛔ 不改）」+ Out of Scope 加「不改归档件」+「不发明 `rddf feedback`」 |
| **M-10** | 缺「自身禁写过程日志」硬约束 | ✅ **采纳** —— 范围纪律首段加「12 份终态 SHALL NOT 含 `本 change` / `2026-11-15` / `Entry #434`」+ **AC-08 ④** 可机检 |
| **S-11** | 补 Entry #324 授权链 + 「判据 owner 在仓外」 | ✅ **采纳** —— §架构依据 加该行（实测 `:18150` 命中）；§Why 第 4 条加「变更检测器 ≠ 判据权威」+ R-6 |
| **S-12** | 「44/45 个 `cep_*.sh` 全 rc=0」口径错（含 helper + 实施期必红的 freeze 闸） | ✅ **采纳** —— **AC-13** 改为「**按 ctest 名单**」（⛔ 不用文件 glob） |
| **S-13** | AC-09 魔数 529 | ✅ **采纳** —— 改「基线记录值 == 终态值」（前后对比） |
| **S-14** | 缺 W1 内部定序 | ✅ **采纳** —— W1a → W1b → W1c → W1d（W1d 单独审查轮） |
| **S-15** | AC-05 自建长度测量 | ✅ **采纳** —— Purpose 以 `openspec --json` 为**唯一真源**（AC-08 ①） |
| **S-16** | 5 份改写须人工/审查逐句比对 | ✅ **采纳** —— §Why 第 4 条明确「由 **critic（arch 档）** 逐条比对」 |
| **S-17** | 估时 1.5 → 3 人日 | ✅ **采纳** —— 2.5 实施 + 0.5 归档链 |
| **N-18** | 补「12 = 5 ∪ 8」 | ✅ **采纳** —— W0 |
| **N-19** | AC-01 / AC-02 合并 | ✅ **部分采纳** —— AC-01 定单一真源（`--specs --json`），AC-02 保留为**逐名核对**（不同判据：数量 vs 具名） |
| **N-20** | 闸可先落盘但不注册 ctest | ✅ **采纳** —— What Changes 5 写明「注册放最后一个产物 commit」+ AC-15 ⑤ |
| **N-21** | R-4 与 AC-03 关系写清 | ✅ **采纳** —— AC-03 = 「漏检防线」（规模对账），⛔ 不是「自愈」 |
| **N-22** | 残余表加 R-5 / R-6 | ✅ **采纳** |
| **N-23** | 闸的收益在未来 | ✅ **采纳** —— §Why 第 1 条末段显式声明 |
