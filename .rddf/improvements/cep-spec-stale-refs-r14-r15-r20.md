---
name: cep-spec-stale-refs-r14-r15-r20
priority: P2
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-SOC-20
状态: "✅ **已归档**（I-11 · 2026-11-21 · 归档 `2026-11-21-cpptlm-spec-stale-refs-b-class` · per 登记册 `obj-cep-platform-remaining.md` §6.4 队列 2）"
created: 2026-11-20
---

# cep-spec-stale-refs-r14-r15-r20: spec 契约面 stale 文本修正（**5 项 B 类**）

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> ⚠️ **单元名说明**：文件名保留立项时的 `r14-r15-r20`（登记册多处引用此名，改名会制造悬空引用），**实际覆盖 5 项** —— R-12 · R-13 · R-14 · R-15 · R-20。
>
> **范围纪律**: ⛔ **不直接改 `openspec/specs/**` 文本**（契约面 ⇒ 必须走 delta spec + 归档合并）· ⛔ **不改任何已签批 ADR 正文** · ⛔ **不复活已删 ABI** · ⛔ **不新建 spec** · ⛔ **不碰 CEP 的 46 份 spec**。
> ⚠️ **占用 1 个 KPI slot**（新建 1 个 openspec change）⇒ 立项前确认 `openspec/changes/` 的 active ≤ 2。

## 架构依据

这 5 项来自登记册 §1.3 的 OWNER 批（该分类已按用户 2026-11-20 裁决**废除**）。**分类边界 = 落点是否在 `openspec/specs/`** —— 5 项全部落在 spec 内 ⇒ 属**契约面**，改它就是改契约。

| # | spec（落点） | 失效内容（2026-11-20 实测） | 类别 |
|:--:|---|---|---|
| **R-12** | `openspec/specs/pcie-slice-prerequisites/spec.md:14` | 引用 `include/cudart/abi_guards.h` —— **该文件不存在**（`ls` 实测）| 悬空引用 |
| **R-13** | `openspec/specs/pcie-ip-microarch/spec.md:296` + `openspec/specs/pcie-ip-integration/spec.md:53`（末行）| 旧状态行：「等待评审 + Phase 1 启动实施」/「Phase 8 最终整合交付中」—— 与新 Purpose 打架 | 状态行 stale |
| **R-14** | `openspec/specs/pcie-ip-integration/spec.md:13,19` | 要求交付 `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`（**已迁 ArchForge**，仓内仅占位 README）| 迁移后引用 |
| **R-15** | `openspec/specs/v1-architecture/spec.md:8,20` | Requirement 写「SHALL 交付 `docs/soc_arch/architecture/00-overview.md`」—— **该文件不存在** | 迁移后引用（且是 `SHALL`）|
| **R-20** | `openspec/specs/dgpu-board-adapter-info/spec.md:10,21` | 引用**已删除**的 ABI `cpptlm_emulator_get_adapter_info`（per `ADR-SOC-20` §2.1；`include/abi/cpptlm_emulator.h:108` 处已删）| 悬空 ABI |

**共同点**：5 条都是「spec 承诺了一个不存在 / 已迁走 / 已删除的东西」⇒ 读 spec 的人会按它去找，找不到。这与「文档 stale」不同 —— **spec 是要被实现的契约**，而 `openspec validate --specs` 对它们是**绿的**（只查格式与长度，不查引用是否可达）⇒ 契约自称有效而实际不可实施。

**为什么不能直接改 main spec 文本**：仓内契约面变更有既定流程（delta spec → `openspec validate` → 归档合并）。直接改会绕过该流程，且 `openspec validate --specs` 看不到变更来源。

## 范围

- **In-Scope**：
  - 新建 `openspec/changes/<name>/`（`proposal.md` + `design.md` + `tasks.md` + `specs/` 下的 delta）
  - 5 处 Requirement / 状态行的修正（形态见 What Changes）
  - 归档时 5 份 main spec 的 MODIFIED 合并
- **Out-Scope**：
  - ⛔ R-17 / R-19（A 类，归 `cep-doc-hygiene-kpi-and-impl-state`）
  - ⛔ R-18（C 类，归 architect Q-12）
  - ⛔ 任何 CEP spec · 任何 ADR 正文 · 已删 ABI 的复活动作
  - ⛔ 45 份 Purpose 占位 / 26 份正文 >500（仓级另有批次）

## Why

**不做的代价**：5 条 spec **都在说「你要交付/调用某个不存在的东西」**。任何人按 spec 实施都会撞墙；更糟的是 `openspec validate --specs` **是绿的** ⇒ 这是「假绿」在契约层的形态。

**为什么 5 条一起做**：修法同型（改引用/改状态行），拆成 5 个 change 要占 5 个 KPI slot 与 5 次归档流程。

**为什么 R-14 / R-15 的修法需要 design.md 讨论**：两条都指向**已迁 ArchForge** 的文档 ⇒ 有两种合法形态：
- (a) **改指跨仓路径**（写 `ArchForge:docs/architecture/19-...`）⇒ 保留交付意图，引入跨仓依赖表述
- (b) **降级为本仓不承诺**（改为「该文档由 ArchForge 维护，本仓不重复交付」）⇒ 消除跨仓假设

⇒ 取舍须在 `design.md` 里对比后定，⛔ 不在本草稿预判。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **R-12 修正** | `pcie-slice-prerequisites` delta（MODIFIED）| 移除或改写对 `include/cudart/abi_guards.h` 的引用；⚠️ 若该 ABI 面**确实需要**守卫断言 ⇒ 须在 design.md 说明「当前由何处承担」（⛔ 不得只删引用而不交代等价物）|
| 2 | **R-13 修正** | `pcie-ip-microarch` + `pcie-ip-integration` delta（MODIFIED）| 两处**状态行**更新为现行状态；⚠️ **只改状态行**，⛔ 不动 Requirement 正文与 `## Purpose` 语义 |
| 3 | **R-14 修正** | `pcie-ip-integration` delta（MODIFIED）| 二选一：(a) 跨仓指针 / (b) 本仓不承诺；⚠️ design.md 须给对比表 |
| 4 | **R-15 修正** | `v1-architecture` delta（MODIFIED）| 同上；⚠️ 原文是 `SHALL` ⇒ 若选 (b) **必须同时改写 `SHALL` 语义**（⛔ 不得留「SHALL 交付一个不存在且不承诺交付的文件」）|
| 5 | **R-20 修正** | `dgpu-board-adapter-info` delta（MODIFIED）| 移除对 `cpptlm_emulator_get_adapter_info` 的引用；⚠️ ⛔ 不得改指其它已删 ABI；须对照 `ADR-SOC-20` §2.1 现行 ABI 清单 |
| 6 | **change 骨架** | `openspec/changes/<name>/` | `proposal.md`（含 `## 验收标准`）+ `design.md` + `tasks.md` + `specs/` |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因签名）** ⇒ 还原 ⇒ **必须绿**」。

- [ ] **AC-0（openspec 可行性 spike · **开工前置** · per metis S6 + 歧义 5）**：**先做影子 change 验证两件事**，任一为否 ⇒ **本单元改走「architect 裁 + 主管契约面直接修订」路径，⛔ 不得硬套 delta**：① `MODIFIED` delta 能否改动 `openspec/specs/pcie-ip-microarch/spec.md:296` 这类**位于 Requirement 块尾部**（`---` 之后、文件末）的状态行 —— ⚠️ 经 delta `MODIFIED` 改它**必然要重述整个 Requirement** ⇒ 与「⛔ 只改状态行，不动 Requirement 正文」**可能物理冲突**；② 同样地，R-12 的引用处 `pcie-slice-prerequisites/spec.md:14` 也**在 Requirement 块内**（`:10-16`）⇒ 「只删一行引用而不重述 Requirement」是否可行。判据 = 影子 change 跑通 `openspec validate` + 试归档 ⇒ ①② 各给「可行 / 不可行 + 原因」的**书面结论**落 `.rddf/reviews/`
- [ ] **AC-1（change 骨架合规）**：`openspec validate <name>` rc=0；`proposal.md` 含 `## 验收标准` 段；`design.md` 对 **R-14 / R-15** 各给出 (a) 跨仓指针 vs (b) 本仓不承诺 的对比表（含取舍理由）；⚠️ **须先过 AC-0** ⇒ AC-0 判「不可行」时本单元**改路径**而非「硬做」
- [ ] **AC-2（R-12 旧引用归零且有交代）**：归档合并后 `grep -c "include/cudart/abi_guards.h" openspec/specs/pcie-slice-prerequisites/spec.md` == **0**；⚠️ 且 design.md 说明「该守卫面现由何处承担」（或明确「本仓不承担」）
- [ ] **AC-3（R-13 状态行更新 · 只动状态行）**：`pcie-ip-microarch/spec.md:296` 与 `pcie-ip-integration/spec.md:53` 的旧状态串（「等待评审 + Phase 1 启动实施」/「Phase 8 最终整合交付中」）`grep -c` == **0**；⚠️ 反 fixture：改动两文件的 `## Purpose` 或任一 `### Requirement` 正文 ⇒ 判据必须能抓到（diff 范围断言）
- [ ] **AC-4（R-14 旧引用归零）**：`grep -c "docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md" openspec/specs/pcie-ip-integration/spec.md` == **0**（或只剩与 design.md 所选方案一致的指针形态）
- [ ] **AC-5（R-15 旧引用归零且 SHALL 自洽）**：`00-overview.md` 命中数符合所选方案；⚠️ **若选 (b)** ⇒ 该 Requirement 的 `SHALL` 必须同时改写（⛔ 不得自相矛盾）
- [ ] **AC-6（R-20 旧 ABI 归零）**：`grep -c "cpptlm_emulator_get_adapter_info" openspec/specs/dgpu-board-adapter-info/spec.md` == **0**；⚠️ 反 fixture：改指另一个**已删除** ABI ⇒ 必须能被 `ADR-SOC-20` §2.1 清单比对抓出
- [ ] **AC-7（未复活血缘）**：`git diff --name-only` **不含** `include/abi/**` · `include/tlm/**` · `src/**` ⇒ 证明只是「改契约去描述现状」，⛔ 不是「改代码去满足旧契约」
- [ ] **AC-8（CEP 零 diff）**：`git diff --name-only` **不含** `openspec/specs/cep-*` · `docs/adr/**` · `docs/architecture/**`
- [ ] **AC-9（KPI 与归档形态）**：立项时 active ≤ **0**（⛔ 与登记册 §6.4 **I-11** 的前置一致；⚠️ 旧写「≤ 2」与仓内 `cep-usage.md` 「Proposed 并发 ≤ 1」冲突 —— per critic SHOULD_FIX 6）；归档用**默认**（⛔ 不用 `--skip-specs` —— 5 条都是**实现层已成立、只是契约措辞错**，delta 必须合并）
- [ ] **AC-10（0 回归）**：`openspec validate --specs` 的红数**不增加**（⚠️ 基数以 `openspec validate --specs --strict` **实测**为准 —— ⛔ 不写死「55 红 / 137」；当前实测为 55 红 / 137 且全为非 `cep-` 前缀 ⇒ **红数不增加**即可）· `ctest` 全绿（⛔ 不写死数字）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `pcie-slice-prerequisites` | **MODIFIED** | R-12 移除悬空 `abi_guards.h` 引用（并交代等价物）|
| `pcie-ip-microarch` | **MODIFIED** | R-13 状态行更新 |
| `pcie-ip-integration` | **MODIFIED** | R-13 状态行 + R-14 交付要求 |
| `v1-architecture` | **MODIFIED** | R-15 的 `SHALL 交付 00-overview.md` 改为现行形态 |
| `dgpu-board-adapter-info` | **MODIFIED** | R-20 移除已删 ABI 引用 |

> ⚠️ 五者均**非 CEP** ⇒ 本单元不改变 CEP 平台任何能力。

## Impact

| 面 | 影响 |
|---|---|
| **新增** | `openspec/changes/<name>/`（4 类文件 + delta specs）|
| **修改（归档时合并）** | `openspec/specs/pcie-slice-prerequisites/spec.md` · `pcie-ip-microarch/spec.md` · `pcie-ip-integration/spec.md` · `v1-architecture/spec.md` · `dgpu-board-adapter-info/spec.md` |
| **零改动（硬查）** | `include/**` · `src/**` · `docs/adr/**` · `docs/architecture/**` · `openspec/specs/cep-*` |
| **冻结面** | **零** —— 五份 spec 都**非** `cep-*`；`FROZEN_GLOBS` 的 spec 侧是 `openspec/specs/cep-*/spec.md` **glob** ⇒ 不触五步链 |
| **KPI** | **0/3 → 1/3** ⇒ 归档 ⇒ **0/3** |
| **风险** | ① R-15 选 (b) 但忘改 `SHALL` ⇒ 自相矛盾契约 ⇒ AC-5 ② R-20 改指另一个已删 ABI ⇒ AC-6 ③ 误用 `--skip-specs` ⇒ main spec 不合并、债原地不动 ⇒ AC-9 ④ 改 R-13 时顺手动 Requirement 正文 ⇒ AC-3 的 diff 范围断言 ⑤ 改动溢出到 CEP spec ⇒ AC-8 |

## 待裁

✅ **无 architect 前置** —— 具体修法（跨仓指针 vs 本仓不承诺）属**该 change 自己的 design 取舍**，在其 `design.md` 里对比后由 builder 定稿。

## 审查轨迹（planner 起草 · 2026-11-20）

| 阶段 | 结论 | 影响 |
|---|---|---|
| 缺口梳理 | OWNER 批被收窄为 4 项（R-12/13/17/19）| ⇒ 复验后实测 **8 项全 live** |
| architect 裁决 | 「4 项改归 builder」| ⇒ 8 项须分类 |
| **user 拍板 2026-11-20（Q-9）** | 废除 OWNER + 8 项逐项终态；**B 类 → openspec change** | ⇒ 本单元成立（初版 3 项）|
| **oracle 对 §10 判 REJECT（BLOCKER-3）** | R-12 / R-13 落点**也在 `openspec/specs/`**，归 A 类物理不可执行（A 类单元 AC-6 禁止 specs diff）| ⇒ 本单元扩到 **5 项**（本版）|
