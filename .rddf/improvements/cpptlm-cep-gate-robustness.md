---
name: cpptlm-cep-gate-robustness
priority: P2
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-00
状态: 🟡 **已立项**(2026-11-15 · metis 第 1 轮 **7 MUST + 5 SHOULD + 5 NIT 全处置**;其中 **1 条推翻初稿事实**（helper/自建计数 6/3 → **5/4**）+ **1 条指出上轮自身留下契约自陈冲突** · change 已建 · KPI **0/3 → 1/3**)
created: 2026-11-15
---

# improvement: cpptlm-cep-gate-robustness

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**:⛔ 不改 `include/**` / `src/**` / `test/*.cc`（零实现层）· ⛔ `docs/adr/**` 零 diff · ⛔ 不新增闸（**闸数 45 不变**）· ⛔ 不动非 `cep-*` 的红 spec。
> **估时**:**实施 1.5 人日 + 归档链 0.3 人日**（metis NIT:原 1.0 低估了 9 闸逐闸回归 + 双向 fixture）
> **KPI**:占 OpenSpec slot **1/3**

---

## 架构依据

| 依据 | 来源（一手） | 要点 |
|---|---|---|
| 闸脚本**不冻结** | `cep_freeze_check.sh` 的 `FROZEN_GLOBS` | 冻结面 = `docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / `openspec/specs/cep-*/spec.md` 等；**闸脚本不在其中**（「闸脚本不冻结，但须能修 ⇒ 自测保证『修了没坏』」）⇒ W1/W3/W4 免五步链，仅 W5 需（1 份产物） |
| 影子树 = 共享 helper | `openspec/specs/cep-invariant-gate/spec.md:13` / `:115` | 「影子树**机制**由共享 helper 承载 ⇒ ⛔ 不存在第二份实现」 |
| **R-d 职责边界（作者明文）** | `cep_decision_block_check.py:237-240` | 「本段遍历**快照已登记的文件**（22 份）⇒ 语料里**新增**的 ADR **不在本段职责内**」⇒ 本 change 加正向覆盖判据，**必须同改 docstring** |
| **上轮自己留下的契约自陈** | `openspec/specs/cep-invariant-gate/spec.md:30` | 原文：「覆盖集合不自愈…本 Requirement **不**提供「覆盖完整性」判据(该缺口另行立项)」⇒ ⚠️ 本 change 就是那个「另行立项」⇒ **必须改写该句**，否则 spec 内两句互斥 |
| 快照授权纪律 | 同上闸 `:207-218` | `authorized_by` 必须指向**已存在**的台账 Entry ⇒ 顺序：**Entry 先落 → ADR → 快照** |
| 仓内 `/tmp` 约定 | `scripts/test/cep_gate_harness.py:34-35` | 明文：「**不**用 `/tmp/opencode` —— 那是 agent 会话目录约定，本脚本已在 CI 的 ctest 里跑」；仓内已有 20+ 处 `${TMPDIR:-/tmp/opencode}` + `mktemp -d` 先例 |
| 并发假红史实 | 台账 Entry #432 ② / #435 | `cep_decision_block_check` 在 `-j4` 下假红、单独跑绿；当时只在**调用方**隔离，闸默认值未动 |

## 范围

**In Scope**:

### W1 · 9 个闸的影子路径 mktemp 化（+ 3 处其它固定共享路径）

实测 9 个闸的默认值全是固定字面量：

| 类 | 闸 | 行 |
|---|---|---|
| **走 helper（5 个）** | `cep_adr_structural_check` | `:30` |
| | `cep_b3b2_archive_form_check` | `:41` |
| | `cep_decision_block_check` | `:36` |
| | `cep_dod_reading_check` | `:30` |
| | `cep_iag_gate_reconcile_check` | `:31` |
| **自建临时工作区（4 个）** | `cep_b3b2_d4_probe_check` | `:22` |
| | `cep_b3b2_d4_quarterly_review` | `:32` |
| | `cep_source_effect_check` | `:23`（变量名 `SHADOW_ROOT`） |
| | `cep_tilelink_engine_failclosed` | `:28`（变量名 `SHADOW_ROOT`，`make_shadow()` 在 `:48`） |

**改法**（metis MUST-1：⛔ **不用 `$$`** —— 容器共享 `/tmp` + PID namespace 各自从 1 起 ⇒ PID 会撞；且 `trap rm -rf` 会删掉调用方给的目录）：

```bash
_base="${TMPDIR:-/tmp/opencode}"; mkdir -p "$_base" 2>/dev/null || true
if [ -n "${SHADOW_ROOT:-}" ]; then SHADOW="$SHADOW_ROOT"; _own_shadow=0
else SHADOW="$(mktemp -d "$_base/<gate>.XXXXXX")"; _own_shadow=1; fi
_cleanup_shadow() { [ "$_own_shadow" = 1 ] && rm -rf -- "$SHADOW"; }
trap _cleanup_shadow EXIT
```

- 每闸加一行 `echo "[shadow] $SHADOW"`（供人复现 + 供 AC 判「两实例路径不同」）。
- **另 3 处固定/越界临时路径**（同型缺陷，一并处理）：
  - `cep_dod_reading_check.sh:31` `LOG="${SHADOW_LOG:-/tmp/opencode/cep-dod-reading-last.log}"` ⇒ 默认改 `"$SHADOW/last.log"`（`SHADOW_LOG` 覆盖保留）
  - `cep_iag_gate_reconcile_check.sh:142` `mktemp -t`（写到系统 `/tmp`）⇒ `mktemp -d "${TMPDIR:-/tmp/opencode}/iag_broken.XXXXXX"`
  - `cep_freeze_reverse_verify.sh:30` `mktemp -t` ⇒ 同上
- ⛔ **不改 helper**（`cep_test_shadow.sh`）：`cep_shadow_reset` 的职责是「复制 ADR 语料」，本 change 只在 9 个调用点管理**路径与清理**，不改其语义 ⇒ 与 spec `:115`（`source` + `reset`）不冲突。

### W2 · 决策块闸「覆盖义务」正向判据

- **谓词钉死**（metis MUST-2）：`section2(strip_fences(text)) is not None` —— **复用现函数，⛔ 不新写正则**（围栏内 `## 2` 不得算章）。
- **判据**：`{语料中含 §2 章的 ADR-CEP-*.md}` ⊆ `{快照 keys}`；否则 **FAIL**，签名 **`决策块覆盖缺失`** + 文件名 + 提示「登记义务：新 ADR 须同批登记快照（`authorized_by` 复用同一台账 Entry）」。
- **双向说明**：R-d 既有段负责「快照 → 语料」（文件缺失 / 计数偏离）；本判据负责「语料 → 快照」。计数仍是**闸外期望值**，只有 **key 集**被机器推导。
- ⚠️ **同批改 3 处**：① 该函数 docstring 的「职责边界」段 ② `--list` 输出的判据清单（加新判据，否则文档与实现不一致）③ `openspec/specs/cep-invariant-gate/spec.md:30` 的自陈（见 W5）。
- ⚠️ **覆盖射程的诚实登记**：只覆盖 `ADR-CEP-*.md`；`ADR-CEP-NUMBER-MAP.md` / `00A` 有 `## 2` 且在快照内（不影响）；**`ADR-CEP-21` 形态（`## 决策` 无编号）不在射程内** ⇒ 进 spec 的「不可约残余」。

### W3 · stale「22 份」按**指代表**订正（⛔ 不盲替换）

实测 **8 处 / 3 种指代**：

| 文件:行 | 所指 | 现值 | 应改为 |
|---|---|:--:|:--:|
| `cep_decision_block_check.py:11` / `:237` / `:504` · `.sh:10` · `scripts/README.md:73` · `test/CMakeLists.txt:286` | **快照键数** | 22 | **23** |
| `cep_freeze_check.sh:10` | **磁盘 `ADR-CEP-*.md` 文件数** | 22 | **24** |
| `cep_adr_structural_check.py:8` | **`decision_adrs` 分母**（22 = 磁盘 − NUMBER-MAP/00A） | 19 / 22 | **21 / 24** |
| `cep_docs_sync_check.sh:4` | **`ADR_LIST` 条数** | 22 | 以数组实际条数为准（W4 后 **23**） |

⚠️ `grep` 必须 `--include=*.py --include=*.sh --include=*.md --include=*.txt`（`scripts/test/__pycache__/*.pyc` 内含「22 份」⇒ 全量 `grep -r` 会误红）。

### W4 · `docs_sync` 的 `ADR_LIST` 补齐（**新发现的活缺陷**）

实测 `ADR_LIST`（`cep_docs_sync_check.sh:50`）**仅 20 条**，末位 `ADR-CEP-18` ⇒ **`ADR-CEP-19` / `-20` / `-21` 三份从未被「存在 + 含不变量/决策节」两项检查**（该文件 `:72` 自己写着「漏登记 = 新 ADR **静默脱检**」）。

**改法**：补齐 3 条 ⇒ **20 → 23**；并验证 `docs_sync --strict` 仍 rc=0（三份都含「决策」节 ⇒ 应绿）。

### W5 · 1 份 delta（`cep-invariant-gate`，**整条 Requirement 重发**）

- 改写 `:30` 的自陈为「**提供**覆盖义务判据（key 集），但 ⛔ 不自动维护**计数**」；
- 在「R-d 段 SHALL 按四步口径计数，并与快照逐份相等」Scenario 追加 AND bullet（覆盖义务 + 签名）；
- 在「判据 SHALL 声明其不可约残余」Scenario 追加第 ④ 条残余（`## 决策` 无编号形态 = `ADR-CEP-21` 形态，不在射程）；
- 其余 Scenario **逐字保留**（MODIFIED 重发时不得静默丢段）。

### W6 · 文档连带 + 台账

- `scripts/README.md:95`「31 = 磁盘实际」→ **45**（**新发现的活缺陷**）；`:73` 22→23（W3）。
- `test/CMakeLists.txt:286` 注释 22→23（W3）。
- 台账 **Entry #436**：含 **R-5 已闭合**、**R-7 新发现（2 份 ERROR 级 spec）**、**R-9 清单台账五处同源事实**、以及 §Why 第 3 条的「不构成循环」论证。

**Out of Scope**:

- ⛔ 不动非 `cep-*` 的 **77 份**红 spec（含 R-7 的 2 份 `ERROR`）⇒ 另立。
- ⛔ 不把 4 个自建临时工作区的闸迁到 helper（它们复制的是 `include/tlm/cep` 副本 / 探针目录，**不是 ADR 语料**，不在 spec `:13` 的射程内）⇒ 只统一**并发安全**这一属性。
- ⛔ 不改快照口径（仍须台账 Entry 授权；本 change 只加**覆盖义务**，key 集由机器推导，**计数仍是人工期望值**）。
- ⛔ 不裁决 R-2（镜像 20 值）/ R-3（ArchForge 缺席 `exit 0`）—— architect 判权。
- ⛔ 不做 R-6（`openspec` 版本 pinning）· ⛔ 不新增闸（45 不变）。

## Why

**第 1 条 —— 固定影子路径是**已兑现**的真实故障，不是理论风险。**

上一 change（Entry #432 ② / #435）实测：`ctest -R cep -j4` 下 `cep_decision_block_check` **假红**、单独跑绿。根因 = 默认影子路径固定 ⇒ 两实例共用目录互踩。当时**只在调用方隔离**，闸默认值没动 ⇒ 下次任何「同一闸并发两次」照样假红。而**假红的代价是信任**。

⚠️ **为什么不用 PID 后缀**（上一轮台账 L-1 的建议）：容器场景下各容器共享 hostPath 的 `/tmp` 而 PID namespace 各自从 1 起 ⇒ **PID 会撞**，`$$` 修的正是这个场景时可能失效。`mktemp` 无此问题，且仓内已有 20+ 处先例、`cep_gate_harness.py:34` 明文反对硬编码 `/tmp/opencode`。⇒ **以 mktemp 取代 PID**，并在台账登记该取代理由（L-1 的处置更新，非静默忽略）。

**第 2 条 —— 「覆盖自愈」是 R-d 段**明文划走**的职责；补它必须同时改契约自陈。**

`cep_decision_block_check.py:237-240` 承认 R-d 只遍历快照里的文件。实测该分工留了一个洞：

| 场景 | 谁会红 |
|---|---|
| 新 ADR 进 `docs/adr/`，**未进** structural 分类表 | `cep_adr_structural_check` ⇒ 未归类 ⇒ **红** ✅ |
| 新 ADR 进分类表，**忘了**登记决策块快照 | **无人红** ❌ |

⚠️ 更关键：上一 change 我在 `cep-invariant-gate/spec.md:30` 写下了「本 Requirement **不**提供「覆盖完整性」判据（该缺口另行立项）」——**本 change 就是那个「另行立项」**。若不改该句，spec 内会同时存在「不提供」与「提供」两句互斥正文 ⇒ **这是本 change 必须闭环的自指债**。

**第 3 条 —— 「加 ADR 现在必须同时改快照」会不会与「快照须台账授权」形成循环？不会。**

顺序纪律（写进 tasks）：**Entry 先落 → ADR 落盘 → 快照同批登记**，`authorized_by` **复用同一个 Entry**。且 `docs/adr/ADR-CEP-*.md` 在 `FROZEN_GLOBS` 内 ⇒ 加 ADR 本就**必须先有台账 Entry** 才能 `--update` ⇒ 新判据只是要求在**同一次登记**里顺手把 key 补上，**不引入新的授权环节**。

**第 4 条 —— 认识论边界。**

- W1 是**可靠性修复**，不改变任何判据的判别力（不新增、不放宽）。
- W2 **新增一条有牙判据**（签名 + 双向 fixture：开火 + 不误报），但它**不自动维护快照**（仍须人工登记，本 change 只让「忘记登记」从静默变红）。
- W3/W4 是**指代订正与覆盖补齐**，无判据语义变化 —— 但 W4 **恢复了 3 份 ADR 的既有检查**（此前静默脱检）⇒ 属**功能性修复**，非纯文档。
- ⚠️ **不完全解决「清单增殖」**：同一事实仍被 5 处手工维护（structural `decision_adrs` / 快照 keys / `NUMBER-MAP` / `docs_sync ADR_LIST` / `docs_sync OWNERS+NO_INV`）⇒ 登记 R-9，本 change **不**主张已解决。

## What Changes

1. 9 个闸（W1）：mktemp + `trap`（不删调用方目录）+ `echo "[shadow] $SHADOW"`。
2. `cep_dod_reading_check.sh`（W1）：`LOG` 默认 → `"$SHADOW/last.log"`。
3. `cep_iag_gate_reconcile_check.sh` / `cep_freeze_reverse_verify.sh`（W1）：`mktemp -t` → `mktemp -d "$base/..."`。
4. `scripts/test/cep_decision_block_check.py`（W2 + W3）：正向覆盖判据 + docstring + `--list` + 3 处注释。
5. `scripts/test/cep_decision_block_check.sh`（W1 + W3）：影子路径 + 2 条 fixture（开火 / 不误报）+ 1 处注释。
6. `scripts/test/cep_docs_sync_check.sh`（W4 + W3）：`ADR_LIST` +3 条；`:4` 注释。
7. `scripts/test/cep_freeze_check.sh:10` / `cep_adr_structural_check.py:8`（W3）：指代订正 22→24 / 19→21。
8. `scripts/README.md:73/:95` / `test/CMakeLists.txt:286`（W3/W6）：23 / 31→45。
9. 1 份 delta（W5）。
10. 台账 Entry #436（W6）。

## Acceptance

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | 9 闸默认路径**不再是固定字面量** | 逐闸 `grep` 其默认分支为 `mktemp -d`；且**无** `/tmp/opencode/<固定名>}` 形式残留 |
| **AC-02** | 并发**路径确实不同** | 每闸输出含 `[shadow] <path>`；同一闸**并行 2 实例** ⇒ 两行 path **不同**且两 rc 都为 0 |
| **AC-03** | 显式 `SHADOW_ROOT` **被使用且不被删** | 传 `SHADOW_ROOT=<tmp>/x` ⇒ 闸内路径 == 该值；跑完后该目录**仍存在** |
| **AC-04** | 默认路径跑完**无残留** | 跑任一闸后 `ls -d "${TMPDIR:-/tmp/opencode}/<gate>."*` 为空（trap 生效） |
| **AC-05** | 覆盖判据**开火**且归因唯一 | 影子根加 `ADR-CEP-30-<probe>.md`（`## 2` + `### 决策 1`）⇒ `python3 cep_decision_block_check.py --root <影子根>` **rc=1** 且输出含 `决策块覆盖缺失` **与**文件名 |
| **AC-06** | 覆盖判据**不误报** | 影子根加 `ADR-CEP-31-<probe>.md`（`## 决策` 无 `## 2`）⇒ 该判据**不红**（复刻 `ADR-CEP-21` 形态） |
| **AC-07** | 真实仓当前一致（可复算） | 打印 `{含 §2 章的 ADR-CEP-*.md}` 与 `{快照 keys}` 两集合，`comm`/`diff` 为空；且 `python3 …check.py` rc=0 |
| **AC-08** | `--list` 含新判据 | `python3 cep_decision_block_check.py --list` 输出含覆盖义务判据 |
| **AC-09** | 契约自陈**不再互斥** | `cep-invariant-gate/spec.md` 中**无**「本 Requirement **不**提供「覆盖完整性」判据」；含覆盖义务 bullet + 残余第 ④ 条；该 spec `--strict` rc=0 |
| **AC-10** | 「22 份」清零且**指代正确** | 8 处逐一核对（⛔ 不盲替换）；`grep -rn '22 份' --include=*.py --include=*.sh --include=*.md --include=*.txt scripts/ test/ CMakeLists.txt` == **0**；`:pyc` 不计 |
| **AC-11** | `ADR_LIST` 覆盖 23 份 | 数组条数 == **23** 且含 `-19` / `-20` / `-21`；`docs_sync --strict` rc=0 |
| **AC-12** | 0 回归 | **45 个** `cep_*.sh`（按 ctest 同参数）全 rc=0；`ctest -R cep` 全绿且条目数 == **T0 记录值**；`[cep]` 用例数 == **T0 记录值**（⛔ AC 里不写死 180/529，按 T0 实测记录对比） |
| **AC-13** | 范围零 diff | `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` 零 diff；闸数 **45 不变**；新增 `INV-CEP-*` = 0 |
| **AC-14** | 五步链（**1 份**冻结产物） | Entry 先落 → 产物 commit → `--update --authorized-by`（清单 diff = **1 份** `cep-invariant-gate/spec.md` + 元数据；`artifact_count` 86 不变）→ 清单单独 commit → 无参复验 rc=0 |
| **AC-15** | provenance | `manifest.frozen_at_commit` == 产物 commit 的 `git rev-parse` |

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|:--:|
| **C-1** | 9 闸**并发安全**且不删调用方目录 | AC-01~04 | ❌ 0 |
| **C-2** | 「已分类的新 ADR 必须登记决策块快照」**可机判**（双向 fixture） | AC-05~07 | ❌ 0 |
| **C-3** | 闸注释与实测规模**一致**（按指代，不盲替换） | AC-10 | ❌ 0 |
| **C-4** | `docs_sync` 的 ADR 覆盖集**不再静默脱检** | AC-11 | ❌ 0 |

## Impact

| 面 | 影响 |
|---|---|
| **实现层** | ⛔ 零改动 |
| **闸** | 9 闸可靠性 + 1 条新判据（有牙）+ ADR 覆盖集 +3 ⇒ **闸数 45 不变**；ctest 条目数**不变** |
| **契约** | 1 份 spec 的 1 条 Scenario + 1 句自陈 + 1 条残余 ⇒ **1 份冻结产物** ⇒ 五步链 |
| **文档** | `scripts/README.md` 2 处（22→23 / 31→45）· `test/CMakeLists.txt` 1 处注释 |
| **口径澄清** | 「影子树单一实现」射程 = **ADR 语料复制**；4 个自建临时工作区不受其约束 |

**⚠️ 残余（9 项 · 全未关闭）**:

| # | 残余 | 实测 | 路由 |
|:--:|---|---|---|
| **R-1** | 非 `cep-*` 的 **77 份** spec rc=1（75 Purpose 占位 + 27 超长，有重叠） | cep 0 + 非 cep 77 | 各 owner |
| **R-2** | 镜像 20 值 13 键无仓内 golden | — | architect |
| **R-3** | ArchForge 缺席 ⇒ 整闸 `exit 0` | 本机在位 | architect |
| **R-5** | 轴 2 另两轴 | ✅ **已闭合**（`Scenario ≥1` 43/43；锚点 6 个悬空 0） | **关闭**（台账订正） |
| **R-6** | `openspec` 版本未 pinning | 闸打印版本，CI 不锁 | backlog |
| **R-7** | ⚠️ **新**：2 份 spec **非 strict 也 rc=1**（`ERROR`：Requirement 无 Scenario）—— `pcie-slice-prerequisites`（`req.5`）/ `vram-controller-functional-mode`（`req.1/2`），**均不在 CEP 冻结面** | 需撰写 Scenario ⇒ 授权契约内容 | **另立** |
| **R-8** | **4 个**闸自建临时工作区（非 ADR 语料） | 见 §Why 末条口径澄清 | **不追** |
| **R-9** | **清单增殖**：同一事实 5 处手工维护（structural `decision_adrs` / 快照 keys / `NUMBER-MAP` / `docs_sync ADR_LIST` / `docs_sync OWNERS+NO_INV`），失效模式同为「漏登记 = 静默脱检」 | 本 change 只堵 2 处（快照 key 集 + ADR_LIST） | backlog（需定 canonical） |
| **R-10** | `cep_iag_gate_reconcile_check.sh:142` 与 `cep_freeze_reverse_verify.sh:30` 的 `mktemp -t`（写系统 `/tmp`） | ⚠️ 本 change **已修**（W1） | — |

## Non-Goals

- ⛔ 不新增闸（45 不变）· ⛔ 不改计数口径（只加覆盖义务）· ⛔ 不把 4 个自建工作区迁 helper。
- ⛔ 不动非 `cep-*` 的 spec（含 R-7 的 2 份 `ERROR`）· ⛔ 不改 `docs/adr/**`。
- ⛔ 不裁决 R-2 / R-3 · 不做 R-6 / R-9。
- ⛔ 不以 `$$` 作唯一性来源（容器 PID namespace 会撞）。

---

## metis 处置对照表（第 1 轮 · 7 MUST + 5 SHOULD + 5 NIT）

| # | metis 条目 | 处置 |
|:--:|---|---|
| **M-1** | `$$` 不足（容器共享 `/tmp` + PID namespace 撞）；`trap` 会删调用方目录 | ✅ **采纳（推翻初稿方案）** —— 改 `mktemp -d` + `_own_shadow` 标志（仅自己建的才删）+ CI 无 `/tmp/opencode` 用 `${TMPDIR:-}` 兜底 |
| **M-2** | 谓词须钉死为 `section2(strip_fences)`，且关系为**双向** | ✅ **采纳** —— 谓词复用现函数（⛔ 新写正则会把围栏内 `## 2` 当章）；W2 说明 R-d 管「快照→语料」、新判据管「语料→快照」，**计数仍是人工期望值** |
| **M-3** | **helper/自建计数算错**（实为 5/4，我写 6/3；漏 `cep_tilelink_engine_failclosed`） | ✅ **采纳（推翻初稿事实）** —— 已实测订正：helper **5** / 自建 **4**（含 tilelink `make_shadow()`），R-8 同步改 4 |
| **M-4** | W4 必须改 `spec.md:30` 的自陈 + 残余第 ④ 条；MODIFIED 是**整条重发** | ✅ **采纳** —— W5 明列三处改动；并引「上一 change 自己写下该句」⇒ 属**自指债**，不是外部要求 |
| **M-5** | AC-04 需点名子检查（`--root` 的 py，不是 `.sh`） | ✅ **采纳** —— AC-05 明确 `python3 …check.py --root` + 签名 + 文件名 |
| **M-6** | 缺「**不误报**」fixture | ✅ **采纳** —— AC-06 新增 `ADR-CEP-31`（`## 决策` 无 `## 2`）必须**绿**（复刻 `ADR-CEP-21` 形态） |
| **M-7** | AC-02 弱判据（只看 rc 可能假绿）；须复核「9 个之外还有没有同型固定路径」 | ✅ **采纳** —— AC-02 断言**两行 `[shadow]` 不同**；复核后**新增 3 处**进 W1（`SHADOW_LOG` / 2 × `mktemp -t`） |
| **S-8** | stale 注释按**指代表**订正，别只改 4 处 | ✅ **采纳** —— 实测 **8 处 / 3 种指代**，W3 列出逐条表 |
| **S-9** | 可把路径管理收敛进 helper | ⛔ **不采纳**（记录理由）—— helper 的 `cep_shadow_reset` 职责是「复制 ADR 语料」；把路径分配塞进去会扩其语义面并波及 5 个既有调用点。改为在各闸**统一同一段 4 行**（并在 §Why 登记取舍） |
| **S-10** | `SHADOW_LOG` 默认改 `$SHADOW/last.log` | ✅ **采纳** —— W1 第三项 |
| **S-11** | AC-08 去硬编码（180/529） | ✅ **采纳** —— AC-12 改「== T0 记录值」 |
| **S-12** | 登记 R-9（清单增殖）+ 两个活缺陷（`ADR_LIST` 漏 19/20/21；README「31 = 磁盘」实为 45） | ✅ **采纳** —— 两者均**实测成立**；`ADR_LIST` 升为 **W4**（功能性修复），README 31→45 进 W6，R-9 登记 |
| **N-15** | AC-07 的 grep 须排除 `__pycache__` | ✅ **采纳** —— 实测 `.pyc` 含「22 份」⇒ 指定 `--include` |
| **N-16** | 顺手改 `iag:142` 的 `mktemp -t` | ✅ **采纳** —— W1（并**多发现** `freeze_reverse_verify:30` 同型） |
| **N-17** | `trap` 显式含 INT/TERM | ✅ **采纳** —— `trap _cleanup_shadow EXIT INT TERM` |
| **N-18** | fixture 命名（`ADR-CEP-30`）写理由 | ✅ **采纳** —— R-c 的 `SPLIT_RE` 只匹配 `^ADR-CEP-0[123]` ⇒ 30/31 不撞；写进注释 |
| **N-19** | AC-05 别把「实测 = 0」当证据 | ✅ **采纳** —— AC-07 改为**打印两集合并 diff 为空** |
