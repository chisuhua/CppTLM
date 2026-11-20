---
name: cep-stage-gate-p5-hardening
priority: P1
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-20
  - ADR-CEP-06
状态: "🟡 **已立项**(2026-11-20 · planner 起草 · 经 metis 1 轮 + oracle 1 轮 + critic 3 轮审查(critic 第 3 轮 APPROVE_WITH_CONDITIONS)· 关联登记册 `obj-cep-platform-remaining.md` §6.3 归属表 I-0 / I-4 · 前置 = Q-6 architect 裁决)"
created: 2026-11-20
---

# cep-stage-gate-p5-hardening: 顺序机关载体扩展 + 闸 P-5「KPI 同步」判据去平凡化

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不新建第三个登记面**（复用既有 `scripts/test/rddf_ac_carrier.tsv` + `scripts/test/rddf_stage_gate.sh`）· ⛔ **不新建闸脚本类型**（既有 shadow 机制）· ⛔ **不改任何已签批 ADR 正文** · ⛔ **不动 `07 号` 正文与既有 SU** · ⛔ **不改 `obj §6.2` B2 行** · ⛔ **不改 KPI 计数**（仍 `0/3`）· ⛔ **不改 `B3b-2` 计数** · ⛔ **判据形态由 Q-6 architect 裁决给定，builder 只实现**（⛔ 不得自行发明 invariant / 判据语义）。

## 架构依据

本单元处理登记册 §6.2 的 **S3-11**（闸判据可被平凡满足）与 **S3-3**（KPI 活口径同步面），并为 §6.4 队列 2 提供**顺序机关**。

**既有机关的实况**（2026-11-20 实测）：

| 项 | 实测 |
|---|---|
| 顺序门禁脚本 | `scripts/test/rddf_stage_gate.sh`（**13618 字节**，2026-11-19）—— **已存在**，文件头自述「阶段顺序门禁 —— per 册子 `obj-cep-platform-remaining.md` §1.5.6」，实现 **P-1~P-6** |
| ctest 注册 | `test/CMakeLists.txt:286-287` `add_test(NAME rddf_stage_gate COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test/rddf_stage_gate.sh)` ⇒ **CI 强制** |
| 数据源 | `scripts/test/rddf_ac_carrier.tsv`（4 列 TSV：`AC 编号 / 载体类型 / 载体路径 / 锚点`；⚠️ **行数有两个口径** —— 文件 **35 行**（含 17 行注释/空行）· **数据行 18 行**（per `grep -vcE "^\s*#|^\s*$"`）⇒ 引用时须写明口径，per metis 歧义 9）—— 「AC ↔ 判据载体 ↔ 锚点」的**唯一数据源** |
| P-2a 现状 | `rddf_stage_gate.sh:158` `ok "P-2a 三个阶段均已归档 ⇒ 无下一阶段可提前开工"` ⇒ 旧框架治理规划（C1/R-5/C2）已无「下一阶段」⇒ 闸处于**无目标**状态 ⇒ **可承载本计划队列** |
| P-5 现状 | `rddf_stage_gate.sh:243-250` 硬编码 `LIVE_FILES` **6 文件**；判据 `:255` = `grep -qE "$_active/3" "$f"` —— **子串存在性** |
| 实跑 | `bash scripts/test/rddf_stage_gate.sh` ⇒ **rc=0**（当前绿） |

**S3-11 的核心事实（影子副本决定性变异实测）**：把 6 个 `LIVE_FILES` 的**活口径行**全部改成 `3/3`（模拟「槽位变了但活口径没同步」的真实漂移）⇒ P-5 **仅判红 2/6**：

| 文件 | 变异后 P-5 判定 | 原因 |
|---|---|---|
| `.rddf/roadmap.md` | ✗ **判红** | `0/3` 只出现在活口径行 ⇒ 有鉴别力 |
| `.rddf/roadmap/phases/phase-10.md` | ✗ **判红** | 同上 |
| `AGENTS.md` | ✓ 仍判「已同步」 | `:316`「历史快照」行残留 `0/3` |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | ✓ 仍判「已同步」 | **382** 处 `0/3` 残留（台账流水） |
| `.rddf/improvements/cpptlm-cep-umbrella.md` | ✓ 仍判「已同步」 | 3 处 `0/3` 残留 |
| `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` | ✓ 仍判「已同步」 | 3 处 `0/3` 残留（`:31` + `:211`） |

⇒ **判据可被平凡满足**（历史流水里的 `0/3` 即可满足）⇒ **未来漂移 4/6 抓不到**。
⚠️ **真实仓当前 6 处活口径取值全部 == `0/3`** ⇒ **当前无漂移**（⛔ 不是假绿事故）⇒ 本单元修的是**判据的鉴别力**，不是修一次事故。

**同步面真源**（S3-3）：`LIVE_FILES` **6 文件 / 7 处**活口径行 = `AGENTS.md:313` · `.rddf/roadmap.md:5` · `.rddf/roadmap/phases/phase-10.md:209` · `obj-cep-platform.md:474` · `cpptlm-cep-umbrella.md:306` · `cpptlm-cep-nvlink-sharp.md:31` + `:211`（**2 处**）⇒ 取值全部 == `0/3`。

## 范围

- **In-Scope**：
  - `scripts/test/rddf_stage_gate.sh` —— ① P-5 判据改「**锚定活口径行 + 取值 == 实测 `N/3`**」；② **队列绑定扩展**（`--phase` 语义从「A/B/C 框架治理」扩到本计划的队列）；③ 闸头引用 `obj-cep-platform-remaining.md §1.5.6` ⇒ 改指 **§6.5**
  - `scripts/test/rddf_ac_carrier.tsv` —— 新增本计划各 I-* 的 **AC ↔ 载体 ↔ 锚点**行
- **Out-Scope**：
  - ⛔ **新建**任何登记面 / 闸脚本 / checklist 文件（本单元是**扩展**既有载体）
  - ⛔ KPI 活口径**取值**的同步动作本身（⇒ 归 `I-4`，每次立项时由闸强制）
  - ⛔ `obj-cep-platform.md` 的任何改动（含 `:474` 散文 stale ⇒ 归 `cep-registry-stale-cleanup`）
  - ⛔ `obj §4.0-a` / `obj §6.2` B2 行 / 冻结面成员 / `B3b-2` 计数

## Why

**不做的代价**：本仓唯一的顺序机关（`rddf_stage_gate.sh`）目前**只守旧规划**（P-2a 已自解）⇒ §6.4 队列 2 的「前置命中判据」若不落到这个闸上，就退化为**文风**（登记册 `:193` 自己的原话：「⛔ P-1~P-6 落盘前，规划只是『文风』」）⇒ 下一个 session 可以直接从 I-5 开工而不违反任何机制。同时 P-5 的 4/6 无鉴别力 ⇒ **KPI 活口径漂移不会被 CI 抓住** ⇒ 槽位计数会悄悄失真。

**为什么是「扩展既有闸」而不是新建**：仓内已有先例与禁令 —— 登记册 `:18622`（Entry #334 ④）明确否决「新建登记面」；`scripts/README.md:17` 已把 `rddf_stage_gate.sh` 登记进守门表并写明其「把实施顺序与『AC 可寻址』从**文风**变成**机关**」的定位 ⇒ **新建第二个顺序机关 = 制造第三个登记面**，与本轮计划「⛔ 不新建第三个登记面」直接冲突。

**为什么 P-5 必须改而不是只登记**：P-5 是**唯一**机器化的 KPI 同步守门；它对 4/6 文件无鉴别力 ⇒ 若只把它登记为「已知局限」，则**槽位计数的唯一机检面永久半盲**。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **P-5 判据去平凡化** | `scripts/test/rddf_stage_gate.sh:255` 附近 | 判据从「文件内**存在** `N/3` 子串」改为「**锚定的活口径行**（行号或唯一措辞，由 Q-6 裁决）**该行取值** == 实测 `N/3`」；⛔ **不新增闸脚本类型**；⛔ 不改 P-1/P-3/P-4/P-6 语义 |
| 2 | **`LIVE_FILES` 活口径锚定表** | 闸内新增一张「文件 → 活口径行锚点」表 | **6 文件 / 7 处**（`nvlink-sharp.md` **2 处**）⇒ ⚠️ **7 处全锚**（critic SHOULD_FIX-1：只锚 `:31` 漏 `:211` ⇒ 未来 `:211` 漂移抓不到）；⚠️ 是否增补文件由 Q-6 裁决 |
| 3 | **队列绑定扩展** | 闸的 `--phase` 参数与 P-2a 段 | 从「A/B/C 框架治理」扩为「**本计划的队列**（队列 2 的 I-* 序列）」⇒ P-2a 判据 = 「下一 I-* 的 AC 行**未被声明完成**」；⚠️ **三阶段已归档的自解分支保留**（旧规划的判据不回退） |
| 4 | **载体表扩展** | `scripts/test/rddf_ac_carrier.tsv` | 新增本计划各 I-*（**I-0~I-11**，含新增的 I-10 / I-11）的 **AC ↔ 载体 ↔ 锚点**行；⛔ **既有数据行 18 行逐字保留**（⚠️ 口径见架构依据表：18 数据行 ≠ 35 文件行）；⚠️ `TODO` 行的升级时限 / 顺序规则按 Q-6 裁决落地（闸 `:189-192` 对 `TODO` 行是**跳过不计失败** ⇒ 长期 `TODO` = 机关无牙） |
| 5 | **闸头引用转指** | `scripts/test/rddf_stage_gate.sh:3` | 「per 册子 `obj-cep-platform-remaining.md` §1.5.6」⇒ 改指 **§6.5**（顺序机关节）⇒ 因 §1.5 整节已作废（登记册 §1.5 顶部标注）⇒ 旧引用悬空 |
| 6 | **反 fixture（决定性变异）** | 闸内自测段（**不注册 ctest**，随闸的 `--self-test` 路径） | 把 6 个 `LIVE_FILES` 的活口径行在**影子副本**里全改 `3/3` ⇒ 修正后的 P-5 **必须判红 ≥ 5/6**（若采纳 7 处全锚 ⇒ 要求 **6/6**）⇒ 证明判据有鉴别力；⛔ **真实仓字节指纹前后必须相等** |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因；⛔ 影子副本模式 ⇒ **真实仓字节指纹前后必须相等**。

- [ ] **AC-1（顺序机关生效 · 本计划队列被守）**：`bash scripts/test/rddf_stage_gate.sh` rc=0；`ctest --test-dir build -R rddf_stage_gate` 绿；且 `rddf_ac_carrier.tsv` 内本计划各 I-* 的 AC 行可 `grep -F` 逐条命中 ⇒ 证明「AC 可寻址」面覆盖到本计划
- [ ] **AC-2（顺序由机关保证 · 非文风 · per metis 歧义 10）**：影子副本模式：把「下一 I-*」标为已完成 ⇒ **P-2a 必须判红**（红因签名 = 「下一阶段提前开工」）；还原 ⇒ 绿；⛔ 判据是**闸红**而非人读。<br>⚠️ **「完成」的机器形态待定**（初稿写「把 AC 行标为已完成」，但载体表**无完成态列** ⇒ 该 AC **无载体**）：现行 P-2a 只认 `openspec/changes/<NEXT_C>/tasks.md` 的 `^- \[x\]`（`rddf_stage_gate.sh:170-173`）⇒ **必须先定形态再写判据**，三个候选：① 载体表**增第 5 列** `state ∈ {pending, done}`（⛔ 既有行该列全填 `pending` ⇒ 逐字保留断言须同步放宽）② 另立 `rddf_queue_state.tsv`（⚠️ = **新建第三个登记面** ⇒ 违反 §6.7 #9 ⇒ 除非 Q-6 明确批准）③ 复用 `openspec/changes/<I-*>/tasks.md` 的勾选（⛔ 但 I-* 不是 openspec change ⇒ **无 tasks.md** ⇒ 不可行）。<br>⇒ **本 AC 的完成判据 = 「形态已由 Q-6 裁决并在载体表落地」**，⛔ **形态未定前不得开工**（否则 AC-2 不可执行）。
- [ ] **AC-3（P-5 判据有鉴别力 · 决定性变异）**：影子副本模式：6 个 `LIVE_FILES` 的**全部 7 处**活口径行改 `3/3` ⇒ 修正后的 P-5 **必须判红 ≥ 5/6**（目标 **6/6**）⇒ ⛔ 修正前实测仅 2/6 ⇒ **变异在修正前必绿（假绿）⇒ 修正后必红**；还原 ⇒ 绿；⚠️ **真实仓字节指纹前后相等**
- [ ] **AC-4（当前无漂移 · 活口径取值 == 实测）**：真实仓 6 个 `LIVE_FILES` 的 **7 处**活口径行取值 == `ls openspec/changes/ | grep -v '^archive$' | wc -l` / 3 ⇒ 当前实测 `0/3` ⇒ 全绿；⚠️ 判据是**逐行取值**，⛔ **不是**「文件内存在 `0/3` 子串」（后者对 4/6 文件平凡成立 ⇒ AC-3 的变异正是用它证伪）
- [ ] **AC-5（零新建登记面）**：`ls scripts/test/rddf_*` 仍只 **2** 个文件（`rddf_stage_gate.sh` + `rddf_ac_carrier.tsv`）；`git diff --name-only` **不含**任何新建的 `.rddf/roadmap/**` 登记面文件；⇒ 守住 Entry #334 ④「不新建登记面」先例
- [ ] **AC-6（既有载体表**判据语义**零变化 · 口径已钉死）**：`git diff` 对 `rddf_ac_carrier.tsv` 的改动 = **既有 18 数据行逐行末尾追加第 5 列 `pending`**（⛔ **前 4 列逐字不变**）+ **新增 13 行**（本计划 I-*）。<br>⚠️ **原判据「既有数据行 18 行 diff 行数 == 0」与「载体表增第 5 列 state」直接矛盾**（加列必然改行尾）⇒ **判据已订正为「前 4 列逐字不变 + 第 5 列全 pending」**（这才是「判据语义零变化」的真正含义）。<br>判据 = ① 逐行前 4 列比对：HEAD 与当前的 `(AC, kind, target, anchor)` **0 处不匹配** ② 既有 18 行 + 新增 13 行的第 5 列取值**全部 == `pending`** ③ 数据行数 18 → **31**（新增 == 本计划 I-* 行数 == 13）
- [ ] **AC-7（闸头引用不悬空）**：`grep -c "§1.5.6" scripts/test/rddf_stage_gate.sh` == **0** ∧ `grep -c "§6.5" scripts/test/rddf_stage_gate.sh` ≥ **1** ⇒ 闸头指向**未作废**的节
- [ ] **AC-8（`TODO` 行不长期停留）**：载体表内**零** `TODO` 行（若 Q-6 裁决允许分阶段 ⇒ 每个 `TODO` 行须带「升级时限 + 升级顺序」字面量且被本单元的 AC 逐条列出）；⚠️ 依据：闸 `:189-192` 对 `TODO` 行是**跳过不计失败** ⇒ 长期 `TODO` = 机关无牙
- [ ] **AC-9（范围纪律 · 零越权）**：`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/` · `openspec/specs/` · `include/tlm/cep/` · `experimental/` · `tools/` · `.rddf/roadmap/objectives/` · `.rddf/improvements/` ⇒ **零冻结面 diff**、**零登记册/台账 diff**（台账侧归 `cep-registry-stale-cleanup`）；⚠️ `git diff` 对 `obj-cep-platform.md` **零改动**
- [ ] **AC-10（0 回归）**：`ctest` 全绿（⛔ **不写死数字**，以 `ctest -N` 实测为准）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**
- [ ] **AC-11（裁决吸收 · M-2 + §9.1 · 锚点表按实测措辞逐文件映射 · 联合命中语义）**：闸内锚点表**逐文件**给出（⛔ **不用全局词表** —— 原全局词表与磁盘实测不符）：`AGENTS.md` → `实测基线`（行内含 `→ **N/3**`，**1** 处）· `.rddf/roadmap.md` → `当前真实 baseline = `（**1**）· `.rddf/roadmap/phases/phase-10.md` → `当前真实 baseline = `（**1**）· `.rddf/roadmap/objectives/obj-cep-platform.md` → **`**当前 Proposed 并发 = `**（**粗体精确锚**，**1** 处 —— ⚠️ 宽子串 `当前 Proposed 并发 = ` 实测命中 **3** 处：`:474` 真锚点 + `:25815`/`:25967` 台账 §11 历史流水 ⇒ **宽子串不可用作锚点**）· `.rddf/improvements/cpptlm-cep-umbrella.md` → `当前真实 baseline = `（**1**）· `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` → `当前实测 **`（**2**）⇒ **合计 7**；**判据语义 = 联合命中**（⛔ **不是**「先按子串计数、再逐命中行取值」两步）：「对每个文件 —— **存在至少一处**命中该文件的**精确锚** ∧ **该锚所在行内**的 `N/3` == 实测 `active/3` ∧ **该精确锚的命中处数 == 期望处数**」⇒ 「台账流水里的同形措辞」**既不计入处数、也不参与取值判定** ⇒ **无歧义**（builder 无需自行发明读法）；⛔ **不用行号**（行号随活口径行被编辑位移）· ⛔ **零豁免**（允许在锚点表里显式登记「某文件不承载 baseline 活口径」）· **`LIVE_FILES` 不增补**（6 文件即全集，新增活口径载体须同步进 `LIVE_FILES`）；⚠️ **通用纪律** = **任何锚点子串在落盘前必须跑 `grep -c` 复验**；若同一措辞在文件内**多处**出现 ⇒ **必须选能唯一定位活口径行的精确形式**（本例 = 粗体标记）
- [ ] **AC-12（裁决吸收 · Q-6e · R 编号空间冻结 + 语义前缀）**：旧 `R-*` **冻结为历史快照**（⛔ **不重编号** —— 重编号会断掉台账与各 improvement 的既有引用）；**新登记一律用语义前缀**：`SPEC-*`（契约面）· `ADR-*`（ADR 归因）· `PROC-*`（流程/治理）· `INV-*`（invariant）· `E2E-*`（证据/实验）；判据 = 登记册新增登记的 id 全部落在上述 5 个前缀空间内（可 `grep -E` 机判）∧ 旧 `R-*` 条目带「历史快照」标记
- [ ] **AC-13（裁决吸收 · Q-6c · `TODO` 行时限可被 user 修订）**：载体表内 **⛔ 零 `TODO` 行为常态**；`TODO` 行**必须**带「升级时限 + 升级顺序」且**最长存活 1 个工作日**（超过判红）；⚠️ **该时限是流程松紧取舍（严格说属 user 域）** ⇒ architect 裁为**默认值**，**可被 user 修订** ⇒ ⛔ 不得表述为不可协商的技术约束（per 裁决 §8.3 L-2）

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无） | — | ⛔ **本单元零能力增量** —— 顺序机关扩展 + 判据修正（治理面）⇒ **不产 delta spec**、**不合并 main spec** ⇒ 归档时**零契约面变化** |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | `scripts/test/rddf_stage_gate.sh`（P-5 段 / 锚定表 / `--phase` 与 P-2a / 闸头引用 / 自测段）· `scripts/test/rddf_ac_carrier.tsv`（**只增行**） |
| **零改动（硬查）** | `docs/**` · `openspec/specs/**` · `include/**` · `experimental/**` · `tools/**` · `.rddf/roadmap/**` · `.rddf/improvements/**` · `test/**`（本单元**不新增测试文件** ⇒ 变异验证走闸的自测段） |
| **冻结面** | **零**（`scripts/test/rddf_*` **不在** `FROZEN_GLOBS` 内 ⇒ `FROZEN_GLOBS` 只含 `docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / `docs/architecture/cep/*.md` / `include/tlm/cep/{protocol_instance,fabric}.schema.json` / `include/tlm/cep/fabric.json` / `openspec/specs/cep-*/spec.md` / `openspec/changes/archive/*cpptlm-cep-umbrella/...` / `experimental/{metrics,preregistration}.json` / `tools/{render_report,run_sweep,ab_compare}.py`）⇒ **不触五步链**、**零 `--update`** |
| **闸** | ctest **0 新增**（`rddf_stage_gate` 已注册）· `cep_freeze_check.sh` rc 不变 · **P-5 判据强度提升**（2/6 → ≥5/6）· P-2a 覆盖面从「三阶段已归档（自解）」扩到「本计划队列」 |
| **KPI** | **0/3 → 0/3**（不创建 OpenSpec change ⇒ 不占 slot） |
| **风险** | ① 锚定形态选错（行号 vs 措辞）⇒ 文档一改就失效 ⇒ **归 Q-6 裁决**（⛔ 不得 builder 自行发明）② `LIVE_FILES` 增补 ⇒ 同步面扩大 ⇒ 归 Q-6 ③ `TODO` 行长期停留 ⇒ 机关无牙 ⇒ AC-8 ④ 影子副本污染真实仓 ⇒ AC-3 强制字节指纹前后相等 ⑤ 与 `cep-registry-stale-cleanup` 同改 `rddf_stage_gate.sh` ⇒ **文件冲突** ⇒ **闸头转指归本单元**、台账侧归前者（§6.3 归属表已划死）⑥ P-2a 语义扩展可能破坏旧规划的自解分支 ⇒ AC-10 的 0 回归 + 保留自解分支 |

## 待裁

⛔ **本单元有 architect 前置** —— 判据形态不得由 builder 自行发明（登记册 §1.1 B-1a 同款纪律：「判据须 architect 先定义，否则 builder 自行发明 invariant 语义 = 越权」）。

| 问 | 内容 | 建议 |
|:--:|---|---|
| **Q-A** | **P-5 的锚定形态**：用**行号**还是**唯一措辞**（如 `当前真实 baseline = N/3` / `当前实测 **N/3**` / `实测基线 … → **N/3**`）？ | **措辞锚定** —— 行号会随文档编辑位移（`roadmap.md:5` 这类活口径行本身就在被反复改），⇒ 措辞 + 计数（命中数 == 期望处数）双保险 |
| **Q-B** | **`LIVE_FILES` 是否增补**？ | ⛔ **不增补** —— 6 文件已覆盖 `AGENTS.md` / `roadmap.md` / `phase-10.md` / `obj` / `umbrella` / `nvlink-sharp`；增补需先答「凭什么这 6 个是全集」⇒ 建议 Q-6 裁「以闸的 6 文件为唯一全集」并登记「新增活口径载体须同步进 `LIVE_FILES`」 |
| **Q-C** | **载体表 `TODO` 行的升级规则**？ | 每行必带「升级时限 + 升级顺序」；⛔ 长期 `TODO` ⇒ 机关无牙 ⇒ 建议「Q-6 裁决后 1 个工作日内把本计划 I-* 行全部落为实锚」 |
| **Q-D** | **闸头引用转指的目标**？ | `obj-cep-platform-remaining.md` **§6.5**（顺序机关节）—— §1.5 已作废 ⇒ 旧引用悬空 |

⇒ **Q-A ~ Q-D 合并为登记册 §6.4 的 Q-6 一次裁决**（architect 域）⇒ 裁决落 `.rddf/reviews/` ⇒ 本单元在该裁决落盘后方可开工（前置命中判据 = 裁决文件存在且 `grep -F` 命中「登记面收敛」）。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis | 指出「规划无机关 ⇒ 顺序只由文风保证」 | ⇒ 促成「复用既有闸而非新建 checklist」的路线 |
| 2 | oracle | **REJECT** · 修法 9「队列无 enforcement 载体」 | ⇒ v3 引入启动机关段（**假事实**，见下） |
| 3 | critic（v2） | **REJECT** · B-1「假事实」 | ⇒ **v3 声称 `rddf_stage_gate.sh`「待建」** —— 实测**已存在且已注册 ctest** ⇒ 论证前提是假的 ⇒ 本单元改为「扩展既有闸」 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ KPI 同步面从「3+2」更正为「**6 文件 / 7 处**」；新增 **S3-11** 记录 P-5 鉴别力缺陷（附影子副本决定性变异） |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | 2 SHOULD_FIX：① 锚定粒度须写明「**7 处全锚**」（`nvlink-sharp.md` 有 2 处）② 闸头转指须指派执行归属 ⇒ 两者均落进本单元（AC-2 判据 + What Changes #5） |

⇒ **本单元须待 Q-6 architect 裁决后开工**（⛔ 判据形态不自行发明）。
