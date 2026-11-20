---
name: cep-archforge-ci-failclosed
priority: P2
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖: []
状态: "🟡 **已立项 · Q-10 已拍板（方案甲）**(2026-11-20 · planner 起草 · 关联登记册 §6.3 归属表 I-9 · **前置已满足 · 可开工**)"
created: 2026-11-20
---

# cep-archforge-ci-failclosed: ArchForge 缺席时契约闸的 fail-closed 处置

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文** · ⛔ **不动 `07 号` 正文与既有 SU** · ⛔ **不改该闸的对齐判据逻辑**（只改「缺席时的行为」）· ⛔ **不新建闸脚本类型** · ⛔ **不引入 ArchForge 依赖进 CI**（⛔ 本单元是让「缺席」变得**可察**，不是把跨仓校验搬进本仓 CI）· ⛔ **零槽位批路径**。

## 架构依据

登记册 §6.2 的 **S2-8（R-3）**：`scripts/test/cep_archforge_alignment_check.sh` 在 **ArchForge 缺席时 `exit 0`** ⇒ **CI 假绿**。

**本机不成立**：`/workspace/main/ArchForge` **在位** ⇒ 闸真跑 **7 项**全绿 ⇒ 该缺口**仅在 CI（无 ArchForge checkout）时成立**。

**为什么仍是真缺口**：该闸是 **CEP ↔ ArchForge 跨仓契约对齐**的唯一机器面（`fabric.json` / `collective_planes.json` 等声明源与 ArchForge 权威的派生副本一致性）⇒ 它静默 `exit 0` ⇒ **CI 上跨仓漂移完全不可察** ⇒ 而 `fabric.json` 的 `authority_note` 明写「改动本文件前必须先确认 ArchForge 侧已更新」⇒ 这条纪律**没有机器保障**。

**与既有先例的一致性**：仓内已有「缺席即显式 SKIP 标记」的判据范式（`rddf_stage_gate.sh` 的归档感知回退：**skip + 告警**，⛔ 不判 fail）⇒ 本单元应复用该范式，而不是发明新形态。

## 范围

- **In-Scope**：
  - `scripts/test/cep_archforge_alignment_check.sh` —— 缺席时的行为：改为「**显式 SKIP 标记 + 明确告警**」；⚠️ **Q-10 已拍板为方案甲 ⇒ 本分支不成立**（⛔ 不得再实现「非 0 退出」双形态 —— per critic SHOULD_FIX 7：Q-10 已定，保留死分支会诱导执行者照做）
  - CI 配置 —— 明确标注该闸在「无 ArchForge」环境下的期望状态
- **Out-Scope**：
  - ⛔ 跨仓校验的**内容**（7 项判据本身）
  - ⛔ 在 CI 中引入 ArchForge checkout（⛔ 那是 CI 基础设施变更，另议）
  - ⛔ 冻结面成员（`include/tlm/cep/fabric.json` **在**冻结集 ⇒ ⛔ 本单元不改它）

## Why

**不做的代价**：CI 绿 ≠ 跨仓契约一致 ⇒ 未来某次改 `fabric.json` 而 ArchForge 侧未更新 ⇒ CI 全绿 ⇒ 漂移**只在人工发现时**才暴露 ⇒ 而 `fabric.json` 的双向比对闸（`cep_decl_alignment_check.sh` 第 ⑤ 项）需要 ArchForge 才跑得起来 ⇒ **静默跳过**。

**为什么不能直接「判红」**：本机与 CI 环境不同 ⇒ 若一律判红 ⇒ 本机无 ArchForge 的场景下所有 CI 会**永久红** ⇒ CI 噪声 ⇒ 教会团队无视红灯（与 `cep_b3b2_d4_quarterly_review.sh` 刻意 NOT 注册 ctest 的理由同型：cadence 断言不能进常规 CI）。⇒ **正确处置是「显式可察」而非「一律判红」**。

**为什么需要 user 决策**：两种处置都合理（① 显式 SKIP + 告警 + CI 标注 ② 直接判红并要求 CI 具备 ArchForge）⇒ 取决于 CI 是否愿意引入跨仓 checkout ⇒ **属用户/基础设施取舍**，不由 builder 自行拍板。

## What Changes

| # | 变更 | 形态 / 约束（**Q-10 已裁 = 方案甲**）|
|:--:|---|---|
| 1 | **缺席时行为** | 方案甲：打印**可 grep 的 `[SKIP]` token**（如 `  · [SKIP] ArchForge 仓不在本环境($ARCHFORGE)—— 跨仓引用完整性无法在本仓校验`）+ **显式告警**（⛔ **不是**「非零的标记位」—— 用词已按裁决 §9.3 N-3 统一为 `[SKIP]`，与 `scripts/test/cep_decl_alignment_check.sh:41` 的既有 token 同形）；⚠️ **当前实测**（2026-11-20）：`cep_archforge_alignment_check.sh:30-34` **有**中文跳过提示（「· 跳过:ArchForge 仓不在本环境…」+「(设置 ARCHFORGE_DIR=<路径> 可启用)」+ `exit 0`）但**无 `[SKIP]` token** ⇒ **真实缺陷 = 与「跑过」机器侧不可区分**；方案乙：**判红**（rc≠0 + 固定红因签名）|
| 2 | **CI 标注** | CI 配置显式声明该闸在无 ArchForge 环境下的期望状态（⇒ 方案甲下 CI 必须能区分「SKIP」与「PASS」）|
| 3 | **反向 fixture** | 影子副本：把 ArchForge 路径指向一个不存在的目录 ⇒ 闸**按选定方案**给出可区分的输出（SKIP 标记 or 红因签名）⇒ 证明「缺席」**可察**（⛔ 当前 `exit 0` 且无任何标记 ⇒ 不可察）|

## Acceptance

- [ ] **AC-1（缺席可察 · ⚠️ 缺陷表述已按裁决 §8.1 M-3 订正）**：⚠️ **原表述「当前实测：无任何输出 + exit 0」是错的** —— 实测 `scripts/test/cep_archforge_alignment_check.sh:30-34` **有输出**（「· 跳过:ArchForge 仓不在本环境($ARCHFORGE)—— 跨仓引用完整性无法在本仓校验」+「(设置 ARCHFORGE_DIR=<路径> 可启用)」+ `exit 0`）⇒ **真实缺陷 = 无统一的 `[SKIP]` token** ⇒ 与「跑过」**机器侧不可区分**。⇒ 判据 = 影子副本下 ArchForge 路径失效 ⇒ 输出**含可 grep 的 `[SKIP]`**（⛔ **不是**「有任何输出」）⇒ 与 `scripts/test/cep_decl_alignment_check.sh:41` 的「· [SKIP] ArchForge 仓不在本环境…」**token 形态统一**（同仓已有 `[SKIP]` / `[RUN]` 分 token 范式，见其文件头 `:26-28`）⇒ ⇒ 机器侧可分流；⚠️ 反 fixture 须命中 **`[SKIP]` 字面量**，⛔ 不得以「有中文提示」判绿
- [ ] **AC-1b（ctest 层须显示 Skipped 而非 Passed · **Q-10 方案甲的真正落点**）**：`test/CMakeLists.txt` 的 `cep_archforge_alignment_check` 含 **`SKIP_REGULAR_EXPRESSION "\\[SKIP\\]"`**；在 ArchForge 缺席环境（影子副本 / `ARCHFORGE_DIR=/nonexistent`）跑 `ctest -R cep_archforge_alignment_check` ⇒ 该测试状态为 **Skipped**（⛔ **不是 Passed**）；⚠️ 反 fixture：把 `SKIP_REGULAR_EXPRESSION` 删掉 ⇒ 该测试回到 **Passed** ⇒ 判据必须能抓到这一差异；⛔ **不得**用 `SKIP_RETURN_CODE 0`（`test/CMakeLists.txt:660` 已固化警告）
- [ ] **AC-2（真实 ArchForge 在位时判据不变）**：`/workspace/main/ArchForge` 在位 ⇒ 闸跑满 **7 项**且 rc 与改动前**逐字节相等**（⇒ 证明只改缺席行为，未改对齐判据）
- [ ] **AC-3（CI 可区分 SKIP 与 PASS）**：CI 配置能区分「SKIP」与「PASS」两种结果（判据 = CI 侧有对应分流逻辑 + 断言载体）；⚠️ 方案甲下若 CI 不区分 ⇒ 本单元**不得**合并
- [ ] **AC-4（范围纪律）**：`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/` · `openspec/specs/**` · `include/tlm/cep/fabric.json`（冻结面）· `include/**` · `src/**`；⇒ **零冻结面 diff**、**零代码 diff**
- [ ] **AC-5（零 OpenSpec change · 零 KPI 占用）**：不创建 `openspec/changes/<name>/`；`ls openspec/changes/ | grep -v archive | wc -l` 前后相等
- [ ] **AC-6（0 回归）**：`ctest` 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0（⛔ 本机 ArchForge 在位 ⇒ 本单元的缺席分支**不在** ctest 主路径上 ⇒ 其判据靠 AC-1 的影子副本验证）· `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无） | — | ⛔ **零能力增量** —— CI 可察性处置 ⇒ **不产 delta spec**、**不合并 main spec** ⇒ 归档时**零契约面变化** |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | `scripts/test/cep_archforge_alignment_check.sh`（缺席分支）· CI 配置（期望状态标注 / 分流） |
| **零改动（硬查）** | `docs/adr/**` · `docs/architecture/**` · `openspec/specs/**` · `include/tlm/cep/fabric.json` · `include/**` · `src/**` · `test/**` |
| **冻结面** | **零**（`scripts/test/cep_*.sh` 与 CI 配置不在 `FROZEN_GLOBS`）⇒ 不触五步链 |
| **闸** | `cep_archforge_alignment_check.sh` 缺席分支行为变更（**7 项判据本身零改动**）· ctest **0 新增** |
| **KPI** | **0/3 → 0/3**（零槽位批路径） |
| **风险** | ① 方案乙 ⇒ CI 无 ArchForge 时**永久红** ⇒ CI 噪声 ⇒ 故方案甲为默认建议（AC-2 兜底）② 方案甲若 CI 不区分 SKIP/PASS ⇒ 等于换一种假绿 ⇒ AC-3 兜底 ③ 改闸时误动 7 项判据 ⇒ AC-2 兜底 ④ 本机 ArchForge 在位 ⇒ 缺席分支**很少被跑到** ⇒ AC-1 的影子副本是唯一判据 |

## 待裁

✅ **Q-10 已由用户 2026-11-20 拍板：选方案甲** —— 前置已满足，本单元可开工。

**落地形态（已钉死，⛔ builder 不得自行改选）**：

| 项 | 内容 |
|---|---|
| **闸侧** | `scripts/test/cep_archforge_alignment_check.sh` 缺席分支：`· 跳过:...` → **`· [SKIP] ...`**（与 `cep_decl_alignment_check.sh:41` 的 token 同形）；⚠️ 可选在 RUN 路径补 `[RUN]`（该闸当前**无** `[RUN]` token，与 decl 闸不对称）|
| **CMake 侧** | `test/CMakeLists.txt` 的 `set_tests_properties(cep_archforge_alignment_check ...)` 增 **`SKIP_REGULAR_EXPRESSION "\\[SKIP\\]"`** ⇒ ctest 显示 **Skipped 而非 Passed** |
| **⛔ 禁止** | ⛔ **不得**设 `SKIP_RETURN_CODE 0`（`test/CMakeLists.txt:660` 已固化警告：会把**正常通过**也标成 Skipped）· ⛔ **不 checkout ArchForge**（那是方案乙）|
| **先例依据** | `cep_decl_alignment_check` 的 `[SKIP]` + `SKIP_REGULAR_EXPRESSION` 三件套，**2026-10-22 代码复审 S2** 的产物（`test/CMakeLists.txt:655-666`）|
| **成本** | ≈3 行（1 行 echo + 1 行 CMake property [+ 可选 1 行 `[RUN]`]）|

⇒ **关键区分**：甲修掉的是**假绿**（`Passed` → `Skipped`），不是「换个字」；「本仓 CI 要不要真验跨仓一致性」是**另一个问题**（方案乙），本轮不承担。

**⚠️ 环境前提（per oracle SF-3）**：`SKIP_REGULAR_EXPRESSION` **恰为 CMake 3.16 引入**，而 `CMakeLists.txt:6` 的地板正是 `3.16` ⇒ **零余量**。⇒ 本单元须在判据里写明：CI / 本地 CMake **必须 ≥ 3.16**；若将来降地板版本 ⇒ 该 property 会被静默忽略 ⇒ 闸退回 `Passed`（假绿复活）。⚠️ 反 fixture（AC-1b 已含）：删掉该 property ⇒ 测试回退 `Passed` ⇒ 判据必须能抓到。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis | 指出「R-3 CI 假绿」为漏项 | ⇒ 本单元因此设立 |
| 2 | oracle | **REJECT** · 修法 3 | ⇒ 非 CEP 域项移出；本单元保留为 S2 |
| 3 | critic（v2） | **REJECT** · 3 BLOCKER | ⇒ 不涉及本单元 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ 不涉及本单元 |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | ⇒ NITS 明确「不追罚 S2-8 的 `exit 0` 语义」（plan 档下本机 N/A + 已有 Q-10 用户门）⇒ 与本单元一致 |

⇒ **Q-10 已由用户 2026-11-20 拍板为方案甲** ⇒ **本单元前置已满足，可开工**（per metis B9：初稿 front-matter 写「待 user 决策」而 §待裁 写「已拍板」⇒ **同一文件两处状态矛盾** ⇒ 已统一）。
