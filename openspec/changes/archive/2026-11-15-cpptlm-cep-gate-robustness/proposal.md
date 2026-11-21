# proposal: cpptlm-cep-gate-robustness —— CEP 闸族健壮性 + 决策块覆盖义务 + 覆盖集补齐

> **状态**: 🟡 Proposed（2026-11-15 立项 · planner 起草 · 待 builder 补 design/tasks 并实施）
> **依据**: improvement `.rddf/improvements/cpptlm-cep-gate-robustness.md`（v2 · metis 第 1 轮 **7 MUST + 5 SHOULD + 5 NIT 全处置**，其中 **1 条推翻初稿事实**（helper/自建 = 5/4 非 6/3）· **1 条指出上一 change 自己留下的契约自陈互斥**）· 台账 **Entry #436**
> **KPI**: 立项后 Proposed **0/3 → 1/3**

## 为什么

三件事，同一个病：**闸族的「可信度」欠账** —— 会假红、会漏判、会自相矛盾。

### 缺口 1：9 个闸的默认影子路径是**固定字面量** ⇒ 同闸并发必假红（已兑现）

上一 change（Entry #432 ② / #435）实测：`ctest -R cep -j4` 下 `cep_decision_block_check` **假红**、单独跑绿。当时只在**调用方**隔离，**闸本身的默认值没动** ⇒ 下次任何「同一闸并发两次」照样假红。而假红的代价是**信任**。

⚠️ **不用 PID 后缀**：容器场景下各容器共享 hostPath 的 `/tmp`，而 PID namespace 各自从 1 起 ⇒ `$$` 会撞。⇒ 用 `mktemp -d`（仓内已有 20+ 处先例；`cep_gate_harness.py:34` 明文反对硬编码 `/tmp/opencode`）。

### 缺口 2：决策块闸的覆盖集合**无自愈**，且上一 change 自己写下了互斥自陈

`cep_decision_block_check.py:237-240` 明文把「新增 ADR」划出 R-d 段职责。实测该分工留洞：

| 场景 | 谁会红 |
|---|---|
| 新 ADR 进 `docs/adr/`，**未进** structural 分类表 | `cep_adr_structural_check` ⇒ **红** ✅ |
| 新 ADR 进分类表，**忘了**登记决策块快照 | **无人红** ❌ |

⚠️ 且 `openspec/specs/cep-invariant-gate/spec.md:30` 现写着「本 Requirement **不**提供「覆盖完整性」判据（该缺口另行立项）」——**本 change 就是那个「另行立项」** ⇒ 不改该句则 spec 内两句互斥。

### 缺口 3（新发现的活缺陷）：`docs_sync` 的 `ADR_LIST` **原 22 条**（缺 `-20`/`-21`）

实测 **22 条**（末位是 `ADR-CEP-NUMBER-MAP`）⇒ **`-20` / `-21` 两份从未被「存在 + 含不变量/决策节」两项检查**（`-19` 已于 Entry #74 登记）。而该文件 `:72` 自己写着「漏登记 = 新 ADR **静默脱检**」。⚠️ **本段为订正后表述** —— 初稿写「20 条 / 末位 `-18` / 缺 3 份」，实施时实测不符，已按实测改（见 tasks T4）。

另：`scripts/README.md:95` 写「**31** = 磁盘实际」，实测 `ls scripts/test/cep_*.sh | wc -l` = **45**。

### ⛔ 认识论边界

- W1 是**可靠性修复**，不改变任何判据的判别力。
- W2 **新增一条有牙判据**（签名 + 双向 fixture），但**不自动维护快照**（仍须人工 + 台账 Entry）。
- **不完全解决「清单增殖」**：同一事实仍 5 处手工维护 ⇒ 登记 R-9，本 change **不**主张已解决。

## 变更内容

| # | 内容 | 落点 |
|:--:|---|---|
| **W1** | 9 闸影子路径 mktemp 化（5 helper + 4 自建）+ 不删调用方目录 + `echo "[shadow] $SHADOW"`；另 **3 处**固定/越界临时路径（`SHADOW_LOG`、2 × `mktemp -t`） | 11 个闸脚本 |
| **W2** | 决策块闸**覆盖义务**正向判据（谓词 = `section2(strip_fences)`）+ docstring + `--list` + 双向 fixture（开火 `ADR-CEP-30` / **不误报** `ADR-CEP-31`） | `cep_decision_block_check.{py,sh}` |
| **W3** | stale「22 份」按**指代表**订正（实测 **8 处 / 3 种指代**：快照键 23 · 磁盘文件 24 · `decision_adrs` 分母 21/24） | 8 处 |
| **W4** | `docs_sync` 的 `ADR_LIST` 补 `-20`/`-21` ⇒ **22 → 24**（== 磁盘全量） | `cep_docs_sync_check.sh:50` |
| **W5** | **1 份 delta**：`cep-invariant-gate` 整条 Requirement 重发（改 `:30` 自陈 + 追加覆盖义务 bullet + 残余第 ④ 条；其余 Scenario 逐字保留） | `specs/cep-invariant-gate/spec.md` |
| **W6** | 文档连带：`scripts/README.md:73`（22→23）/ `:95`（31→45）· `test/CMakeLists.txt:286`（22→23）· 台账 Entry #436 | 3 文件 + 台账 |

**⛔ 边界**：`include/**` / `src/**` / `test/*.cc` / `docs/adr/**` 零 diff · **闸数 45 不变** · 不新增 `INV-CEP-*` · 不动非 `cep-*` 的红 spec · 不把 4 个自建工作区迁 helper（它们复制的是 `include/tlm/cep`，非 ADR 语料，不在 spec `:13` 射程内）。

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | 9 闸默认分支为 `mktemp -d`，无固定字面量残留 |
| **AC-02** | 每闸输出 `[shadow] <path>`；同闸**并行 2 实例** ⇒ 两 path **不同**且两 rc 都为 0 |
| **AC-03** | 显式 `SHADOW_ROOT` 被使用，且跑完**仍存在**（不删调用方目录） |
| **AC-04** | 默认路径跑完**无残留**（trap 生效） |
| **AC-05** | 覆盖判据**开火**且归因唯一：影子根加 `ADR-CEP-30-<probe>.md`（`## 2` + `### 决策 1`）⇒ `python3 cep_decision_block_check.py --root` **rc=1** 且含 `决策块覆盖缺失` + 文件名 |
| **AC-06** | 覆盖判据**不误报**：影子根加 `ADR-CEP-31-<probe>.md`（`## 决策` 无 `## 2`）⇒ 该判据**不红** |
| **AC-07** | 真实仓一致且**可复算**：打印 `{含 §2 章的 ADR-CEP-*.md}` 与 `{快照 keys}` 两集合，`diff` 为空；`check.py` rc=0 |
| **AC-08** | `--list` 输出含新判据 |
| **AC-09** | 契约自陈**不互斥**：spec 中无「本 Requirement **不**提供「覆盖完整性」判据」；含覆盖义务 bullet + 残余第 ④ 条；spec `--strict` rc=0 |
| **AC-10** | 「22 份」清零且**指代正确**（8 处逐一核对，`--include` 排除 `__pycache__`） |
| **AC-11** | `ADR_LIST` 条数 == **24**（== 磁盘 `ADR-CEP-*.md` 全量）且含 `-20`/`-21`；`docs_sync --strict` rc=0 |
| **AC-12** | 0 回归：**45 闸**全 rc=0（按 ctest 同参数）· `ctest -R cep` 全绿且条目数 == **T0 记录值** · `[cep]` 用例数 == **T0 记录值**（⛔ 不写死 180/529） |
| **AC-13** | 范围零 diff（含闸数 45 不变、新增 `INV-CEP-*` = 0） |
| **AC-14** | 五步链（**1 份**冻结产物）：Entry → 产物 commit → `--update --authorized-by`（diff = 1 份 hash + 元数据；`artifact_count` 86 不变）→ 清单单独 commit → 无参复验 rc=0 |
| **AC-15** | `manifest.frozen_at_commit` == 产物 commit sha |

## 不做什么

① 不新增闸（45 不变）· ② 不改计数口径（只加覆盖义务，期望值仍人工登记 + 台账授权）· ③ 不把 4 个自建工作区迁 helper · ④ 不动非 `cep-*` 的 77 份红 spec（含新发现 R-7 的 2 份 `ERROR` 级）· ⑤ 不改 `docs/adr/**` · ⑥ 不裁决 R-2 / R-3（architect）· ⑦ 不做 R-6（版本 pinning）/ R-9（清单增殖 canonical 化）· ⑧ 不以 `$$` 作唯一性来源（容器 PID namespace 会撞）· ⑨ 不新增 ADR。
