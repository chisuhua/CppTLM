# proposal: cpptlm-cep-spec-strict-compliance —— `cep-*` 契约面 `--strict` 合规 + 防退闸

> **状态**: 🟡 Proposed（2026-11-15 立项 · planner 起草 · 待 builder 补 design/tasks 并实施）
> **依据**: improvement `.rddf/improvements/cpptlm-cep-spec-strict-compliance.md`（v2 · metis 第 1 轮 **10 MUST + 7 SHOULD + 6 NIT 全处置**，其中 **2 条推翻了初稿的事实判断**）· `obj-cep-platform.md:18150`（Entry **#324** 的「轴 2 达标度 = 独立 remediation 批次」）· 台账 **Entry #434**
> **KPI**: 立项后 Proposed **0/3 → 1/3**

## 为什么

### 缺口 1：`cep-*` 43 份 spec 里 **12 份** `--strict` rc=1，**2 份是上一 change 引入的回归**

| spec | 判据 | 归档前 | 现在 |
|---|---|:--:|:--:|
| `cep-sharer-tracker-base` | Requirement 正文长度 | **89** | **604** ❌ |
| `cep-invariant-gate` | 同上 | **348** | **811** ❌ |

**为什么当时没被发现**：`openspec validate <change> --strict` 对 **`MODIFIED` 块不作长度检查**（工具源码注释原文 `MODIFIED is left alone: its text is the existing requirement, which the specs instruction says to keep whole.`）⇒ delta 阶段**零告警**；main spec 的长度检查**只在归档合并后**才成立，而**仓内没有任何闸跑逐份 `--strict`** ⇒ 落地即静默腐化。上一 change 的 AC-09 写「归档后逐份 rc=0（max 298/348）」是**归档前的估值**，归档后未复跑。

### 缺口 2：12 份的构成 —— 多数是「过程日志混进了规范正文」

| 类 | 份数 | spec（长度） |
|---|:--:|---|
| **Requirement 正文 >500** | **5** | `cep-invariant-gate`（811，其中 ≈460 是「本次修订/边界/不自愈」三条过程引用块）· `cep-sharer-tracker-base`（604，≈345 是「本次修订」引用块）· `cep-core-contracts`（697，「订正(2026-11-15)」+「Scenario 标题沿用历史命名」）· `cep-collective-plane-contract`（517，`:29-30` 116 字符）· `cep-ualink-coherence-engine`（1931，**重复** `UGE-A` 已载的 785 + 3 条**无场景覆盖**的规范句） |
| **Purpose 占位** | **8** | `cep-basis-source-kind` · `cep-instance-assembly` · `cep-moesi-gpu-l4-e2e` · `cep-poc-spike-verification` · `cep-refkind-coverage` · `cep-stub-cache-core` · `cep-ualink-coherence-engine` · `cep-vc-mapping-contract` |
| **并集** | **12** | （`cep-ualink-coherence-engine` 两类都占） |

⇒ **代价**：一份**契约正文**里混着「谁在哪天改了哪一句」的过程记录，读者无法区分「规范要求」与「修订日志」；而 `--strict` 的判据原文正是「Move examples and edge cases into scenarios, or split it into separate requirements that **each state one behavior**」。

### 缺口 3：没有闸 ⇒ 修完照样会再腐化（本轮就是活证据）

⇒ 本 change 的**第一价值不是修这 12 份，而是补上让第 13 份无法静默出现的闸**。

### ⛔ 认识论边界（不得过度声称）

- **判据的 owner 在仓外**：500 / 50 两个阈值由第三方工具持有（`validator.js` / `constants.js`）⇒ 本闸是**变更检测器**，**不是**判据权威。工具 major 升级即改变闸的语义 ⇒ 闸必须打印工具版本，版本漂移走独立裁决（残余 R-6）。
- 闸的覆盖 = **目录名以 `cep-` 开头**；不叫 `cep-*` 的新 spec 会逃逸 ⇒ 残余 R-1。
- **「语义零变更」不靠机器自证**：机器只防「偷偷改」（既有场景 byte-equal + 规范句移除清单 + 术语表不降），**删掉的到底是什么**须由 **critic（arch 档）逐条比对**。
- 闸的收益在**未来**；实施期它是**最后**变绿的产物（ctest 注册放最后一个产物 commit，⛔ 中间 commit 不带红用例）。

## 变更内容

**范围 = 12 份 spec + 1 个新闸 + 1 个共享 helper 小改 + 文档连带 + 台账**，详见 improvement 的 §What Changes。

| # | 内容 | 落点 |
|:--:|---|---|
| **W1** | **12 份修到 0 红**，内部定序固定：**W1a** 2 份回归（纯删过程引用块，目标 206 / 386）→ **W1b** `cep-collective-plane-contract`（删 `:29-30` → 401）→ **W1c** `cep-core-contracts`（删过程块但**保留**规范句 `SHALL NOT 在标题内嵌数量` → 336）→ **W1d** `cep-ualink-coherence-engine`（去重 785 + **下沉 3 条无覆盖的规范句**为新场景 `UGE-F/G/H` + `UGE-I` → **481**） | 5 份 delta + 8 份 Purpose 直接改 |
| **W2** | **防退闸** `scripts/test/cep_spec_strict_check.sh`：主判据 `openspec validate --specs --strict --json` 过滤 `cep-` ⇒ `invalid == ∅`；**覆盖对账**（items == 129 ∧ cep- == 43）；归因逐份打印；**环境缺失 ⇒ exit 2**；**判据版本固化**；**反向 fixture**（经共享 helper 的影子根，毒化一份**本已绿**的 spec ⇒ `invalid_ids == 基线 ∪ {毒化 id}` 且恰好 +1；仓内零改动用**全量 sha256 manifest** 前后比对） | 新脚本 |
| **W3** | **5 份 delta**：`MODIFIED Requirements`，**整块复制既有 Requirement 原文**（全部既有 Scenario 逐字、含顺序），只做定点删除/下沉 | `specs/<capability>/spec.md` × 5 |
| **W4** | **8 份 Purpose**（直接改 main spec —— openspec 明文授权） | 8 份 main spec |
| **W5** | 文档连带：`scripts/README.md` 守门表 +1 · `test/CMakeLists.txt` 注册（⚠️ 放最后一个产物 commit）· `AGENTS.md` 闸数 **44 → 45** | 3 文件 |
| **W6** | `openspec/changes/<name>/removals.json`：**规范句移除清单**（`{spec, sentence_sha256, kind: process\|interpretive\|moved\|reworded, covered_by}`） | change 目录 |
| **W7** | `scripts/test/cep_test_shadow.sh`（改）：`extra_cp` 支持目录（`cp` → `cp -r`）—— ⚠️ **冻结契约硬约束**：`cep-invariant-gate/spec.md:13`/`:115` 要求「影子树 SHALL 由共享 helper 承载，⛔ 任一闸 SHALL NOT 自带复制实现」 | 共享 helper |
| **W8** | 台账 **Entry #434**（含**回归认领**） | 台账 |

**⛔ 边界**：`include/**` / `src/**` / `test/*.cc` 零 diff · `docs/adr/**` 零 diff · **归档件零 diff**（含上一 change 的 AC-09 原文 —— 归档件不可变，只由台账认领）· 不新增 `INV-CEP-*` · **不新增 spec Requirement** · 不改任何**既有** Scenario · 不用闸外快照登记基线债 · 不动非 `cep-*` 的 spec · 不发明 `rddf feedback` 通道。

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | `cep-*` 逐份 `--strict` **0 红**（`openspec validate --specs --strict --json` 过滤 `cep-` ⇒ `invalid == 0`；⛔ 无双真源） |
| **AC-02** | 12 个目录名**逐个** `valid == true` |
| **AC-03** | 覆盖对账：`items` == `ls openspec/specs \| wc -l`（**129**）∧ `cep-` 项 == **43**（防漏检；⛔ 不是「自愈」） |
| **AC-04** | 规范句移除**逐句归因**：`removals.json` 的 `sentence_sha256` 集合 == 基线−终态的 SHALL/MUST 句差集（**集合相等**）；非 `process` 类的 `covered_by` 必须可解析（`scenario:<标题>` 存在于同 spec / `ledger:Entry #N` 命中 / `text:<sha256>` 存在于终态） |
| **AC-05** | 术语表**不整体消失**：对每份 spec，**基线存在**的受控 token（`txn_id` / `flow_events` / `unsupported_rejected` / `snoop_resp_pending_not_consumed` / `completion_unmatched` / `egress_emitted` / `kResponderMappedBits` / `kResponderMappedMask` / `0xF` / `kResponderBitToNode`）在终态仍 **≥1 次**（⛔ 不得整体消失）；**次数下降**不单独判红，但必须由 AC-04 的句级归因解释（⚠️ 原措辞「次数 ≥ 基线」**已订正** —— 它禁止了本 change 必须做的去重） |
| **AC-06** | 既有 Scenario **逐块 byte-equal**（基线取 `git show <base>:<path>`，5 份 target Requirement 段内每个 `#### Scenario:` 块的 sha256 集合相等） |
| **AC-07** | 新增场景**已登记**：标题 ∈ `design.md` 的下沉清单，且正文 ≥1 bullet 与被删正文句共享核心 token |
| **AC-08** | Purpose 四条正交：① `openspec --json` 判 valid（唯一真源）② ≥50 字符 ③ **不复制** Requirement 首句 ④ 不含 `本 change` / `2026-11-15` / `Entry #43*` |
| **AC-09** | 防退闸 + 反向：闸 rc=0；fixture 首步断言 `invalid_ids(基线) == ∅`（否则 **exit 2**，⛔ 不是 pass/red），毒化后 `invalid_ids == 基线 ∪ {毒化 id}` 且**恰好 +1** |
| **AC-10** | 反向 fixture **仓内零改动**：`openspec/` + `docs/adr/` 的**全量** sha256 manifest（排序后）前后相等（⛔ 只测目标文件） |
| **AC-11** | 环境缺失 ⇒ **exit 2**（`PATH` 去掉 `openspec` 后跑闸，stderr 含 `openspec`） |
| **AC-12** | 范围零改动：`git diff --name-only` 不含 `include/` · `src/` · `test/*.cc` · `docs/adr/` · `openspec/changes/archive/`；新增 `INV-CEP-*` = 0；新增 spec Requirement = 0 |
| **AC-13** | 0 回归（**无魔数**）：`[cep]` 用例数 == 基线记录值（前后对比）· `ctest -R cep` 全绿且**按 ctest 名单**（⛔ 不用文件 glob —— 含 helper 与实施期必红的 freeze 闸）· `validate --changes --strict` rc=0 · `agents_md_lint` rc=0 |
| **AC-14** | helper 改动的回归：共用该 helper 的 `cep_adr_structural_check.sh` 与 `cep_decision_block_check.sh` 全绿 |
| **AC-15** | 五步链 **7 步**（顺序固定）：① Entry #434 落 → ② Purpose 8 份 + delta 落盘 → ③ **影子根预演 `openspec archive`**（0 红 + 场景名集合相等 + diff 只落目标段）→ ④ 真仓 `openspec archive` + 复验 0 红 → ⑤ 产物 commit → ⑥ `--update --authorized-by "Entry #434"`（清单 diff = **12 份** main spec hash + 3 个元数据键；⛔ 无其他 artifact hash 变化；`docs/adr/**` 与 `07-*.md` 零变化；`artifact_count` **86 不变**）→ ⑦ 清单单独 commit + 无参复验 rc=0 |
| **AC-16** | provenance：`manifest.frozen_at_commit` == 产物 commit 的 `git rev-parse` 值 |
| **AC-17** | 归档后 diff 只落目标段：5 份的 hunk 行号全部落在「**目标 Requirement 段 ∪ Purpose 段**」（后者 = D-2 的合法直改；⛔ 不得落在其它 Requirement / 无关段） |
| **AC-18** | KPI 回填：归档后 **1/3 → 0/3**（6 处活口径 + obj 的「当前 Proposed 并发」句）+ 体量数字（归档 146 → **147**） |

## 不做什么

① **不动非 `cep-*` 的 75 份**红 spec（全仓 129 中 87 红 = 非 cep **75** + cep **12**）⇒ 残余 R-1；
② **不新增 spec Requirement**（5 份**不拆分**，只做正文下沉）；
③ **不改任何既有 Scenario**（新增 = `UGE-F/G/H/I`，须登记）；**不改任何 Requirement 的 SHALL 内容**（只搬位置，逐句进 `removals.json`）；
④ **不用闸外快照登记基线债**（登记 12 份红 = 把判据抹掉）；
⑤ **不改归档件**（含上一 change 的 AC-09 原文 —— 归档件不可变，由台账认领）；
⑥ **不裁决** 镜像 20 值跨仓权威 / ArchForge 缺席整闸 `exit 0`（architect 判权）；
⑦ **不做** 9 个闸的固定影子路径 / 决策块闸注释 stale / 覆盖集合无自愈（backlog）；
⑧ **不发明** `rddf feedback` 通道（本仓无此机制；唯一渠道 = 台账 append-only Entry）；
⑨ **不新增 ADR**（`docs/adr/**` 零 diff）。
