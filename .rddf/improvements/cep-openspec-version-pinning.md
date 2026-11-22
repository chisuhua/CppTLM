---
name: cep-openspec-version-pinning
priority: P3
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖: []
状态: "✅ **已落地**（I-8 · 2026-11-21 · 版本锁文件 + 闸 + CI 从锁文件读（**已落地** · 登记册该行**未记**归档 change 名） · per 登记册 `obj-cep-platform-remaining.md` §6.4 队列 2）"
created: 2026-11-20
---

# cep-openspec-version-pinning: `openspec` 版本 pinning（CI 侧锁版本）

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文** · ⛔ **不动 `07 号` 正文与既有 SU** · ⛔ **不改闸脚本的判据逻辑**（只加版本断言）· ⛔ **不升 openspec 版本**（本单元是「锁住当前版本」，⛔ 不是「追版本」）· ⛔ **零槽位批路径**。

## 架构依据

登记册 §6.2 的 **S3-4（R-6）**：**`openspec` 版本未 pinning** —— 闸脚本打印版本、CI **不锁** ⇒ 版本漂移**不可察**（0.2 d 量级，登记册标 `BACKLOG`）。

**为什么在 CEP 域内**：CEP 的 **45** 个契约闸 + `openspec validate --specs --strict` 的 **55 红 / 137** 口径判定**全部依赖 openspec 的判据行为** ⇒ 尤其「Requirement text is very long (>500 characters)」这类**文案级判据**在跨版本时可能变化 ⇒ 版本漂移会**静默改变 CEP 契约面的红绿分布** ⇒ 属真实风险，不是洁癖。

**与既有判据的关系**：`docs_sync_check.sh` 等闸已打印版本号（供人看）⇒ 本单元把「人看」升级为「**机器拦**」。

## 范围

- **In-Scope**：
  - CI 配置 —— 记录并断言 `openspec` 的期望版本（安装版本 + 断言）
  - 若干闸脚本 —— 把「打印版本」升级为「版本不符 ⇒ 明确告警/判红」（**判据逻辑不变** ⇒ 既有 rc 语义不变）
- **Out-Scope**：
  - ⛔ 升级 openspec 本身 / 迁移 spec 写法
  - ⛔ 改任何闸的**判据内容**（只加版本前置断言）
  - ⛔ 冻结面成员

## Why

**不做的代价**：`--strict` 的 55 红（45 Purpose + 26 正文 >500）这套口径是**按 openspec 自己的判据**算的 ⇒ 换版本后红/绿分布可能变，而**没有任何机器面会告诉你** ⇒ §6.2 S2-6 的「口径」类债会**再生产**。

**为什么是 P3 / BACKLOG**：风险真实但**不阻塞任何当前工作**（当前版本行为已知且已记录）⇒ 排在 I-0~I-7 之后。

**为什么放在本轮一起立项**：成本极低（0.2 d）、判据形态唯一（版本断言）、且它是后续所有 `openspec validate` 类判据的**前提条件**（版本漂移会让 §6.2 的所有计数类判据失效）。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **期望版本登记** | 在仓内某处（CI 配置或既有闸的共用常量）记录 `openspec` 的期望版本 + 实测版本（**当前仓实测值**，⛔ 不写死「最新」） |
| 2 | **CI 版本断言** | CI 跑闸前断言 `openspec --version` == 期望版本 ⇒ 不符 ⇒ **明确判红并打印两侧版本**；⚠️ 判红理由须**有签名**（⛔ 不得是泛化 rc≠0） |
| 3 | **闸内版本前置** | 既有打印版本的位置升级为「打印 + 与期望值比对」；⚠️ **既有判据逻辑零改动** ⇒ 既有 rc 语义与红因签名**逐字不变** |

## Acceptance

- [ ] **AC-1（版本被机器锁住）**：CI 配置含期望版本；`openspec --version` 与期望值一致 ⇒ 该断言在 CI 中**可执行**（判据 = 断言载体存在 + 被 CI 调用）
- [ ] **AC-2（版本漂移必红且红因有签名）**：反向 fixture —— 把期望版本改成一个**不存在的版本** ⇒ 版本断言**按固定签名红**（签名含「期望版本」与「实测版本」两个值）；还原 ⇒ 绿；⛔ 「rc≠0」不构成红因
- [ ] **AC-3（既有闸判据零改动）**：`git diff` 对 `scripts/test/cep_*.sh` 的改动**只含版本断言行**（既有判据行 diff 行数 == **0**）⇒ 证明未改任何既有红因语义
- [ ] **AC-4（全部闸 rc 不变 · 数量实测取）**：全部 `scripts/test/cep_*.sh`（**45** 个）逐个 rc 与改动前**逐字节相等**（含本单元新增的版本断言在版本一致时**不改变** rc）
- [ ] **AC-5（范围纪律）**：`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/` · `openspec/specs/**` · `include/**` · `src/**` · `test/**`（⛔ 除非新增断言载体落在 `test/`；若落 `test/` 须在 design 中列白名单）
- [ ] **AC-6（零 OpenSpec change · 零 KPI 占用）**：不创建 `openspec/changes/<name>/`；`ls openspec/changes/ | grep -v archive | wc -l` 前后相等
- [ ] **AC-7（0 回归）**：`ctest` 全绿（⛔ **不写死数字**）· `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无） | — | ⛔ **零能力增量** —— CI 版本锁定 ⇒ **不产 delta spec**、**不合并 main spec** ⇒ 归档时**零契约面变化** |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | CI 配置（版本期望 + 断言）· `scripts/test/cep_*.sh`（**只含版本断言行**）· 可能 1 个共用常量文件 |
| **零改动（硬查）** | `docs/adr/**` · `docs/architecture/**` · `openspec/specs/**` · `include/**` · `src/**` |
| **冻结面** | **零**（`scripts/test/cep_*.sh` 不在 `FROZEN_GLOBS`；⛔ 但 `tools/*.py` **在**冻结集 ⇒ 若版本断言落 `tools/` ⇒ 触五步链 ⇒ **优先落 `scripts/`**） |
| **闸** | 全部 `cep_*.sh` 闸（数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准）+1 版本断言（版本一致时 rc 不变）· ctest **0 新增** |
| **KPI** | **0/3 → 0/3**（零槽位批路径） |
| **风险** | ① 期望版本写「最新」⇒ 形同没锁 ⇒ AC-1 要求写具体值 ② 版本断言误放在 `tools/` ⇒ 触冻结面 ⇒ AC-5 + 落点约束 ③ 版本断言改变既有 rc 语义 ⇒ AC-3 / AC-4 兜底 |

## 待裁

✅ **无** —— planner / builder 域可闭环。
⚠️ 唯一待定 = **期望版本取哪个值** ⇒ 建议取**当前仓实测值**（与 §0.1 基线同批记录），⛔ 不追最新。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis | 登记为 `BACKLOG` | ⇒ 优先级 P3 |
| 2 | oracle | **REJECT** · 修法 3 | ⇒ 非 CEP 域项已移出；本单元保留 |
| 3 | critic（v2） | **REJECT** · 3 BLOCKER | ⇒ 不涉及本单元 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ 不涉及本单元 |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | ⇒ 不涉及本单元 |

⇒ **本单元可立即开工**（⛔ 不占 KPI slot）。
