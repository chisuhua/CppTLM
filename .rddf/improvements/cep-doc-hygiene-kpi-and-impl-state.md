---
name: cep-doc-hygiene-kpi-and-impl-state
priority: P2
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-00
状态: "🟡 **已立项**(2026-11-20 · planner 起草 · 关联登记册 §6.3 归属表 I-2 / I-3 · 前置 = 无)"
created: 2026-11-20
---

# cep-doc-hygiene-kpi-and-impl-state: L3 KPI 行堆积清理 + L4 improvement 状态漂移回填

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文** · ⛔ **不动 `07 号` 正文与既有 SU** · ⛔ **不改 KPI 活口径的取值**（取值同步归 `cep-stage-gate-p5-hardening` 的 I-4；本单元只**压缩体积**）· ⛔ **不删任何历史流水语义**（只搬走，不改判据强度）· ⛔ **不新建登记面** · ⛔ **零槽位批路径**（不创建 OpenSpec change）。

## 架构依据

登记册 §6.2 的 **S3-6（L3）** 与 **S3-7（L4）**：

| 缺口 | 实测 | 后果 |
|---|---|---|
| **S3-6** | `phase-10.md` / `roadmap.md` / `umbrella.md` / `nvlink-sharp.md` 的 KPI 行**单行 6.3~6.8K 字符**（堆 10+ 次归档流水）+ `obj §5.2` 的订正链 **8.5K** | ① **token 成本**：AGENTS.md 注入每个 agent 上下文，`.rddf/roadmap.md` 是高频读取面；② **可读性**：一行里塞 40+ 条 Entry 号 ⇒ 人读与 grep 都失效 |
| **S3-7** | 3 处 improvement 状态漂移：`cpptlm-cep-dcoh-nvlink-refkind-migration` / `cpptlm-cep-ualink-coherence-engine` / `cpptlm-cep-basis-source-kind-and-cell-free` 仍写「提案 / 已批准实施」但**已归档** | 「读成未做」⇒ 与 §6.2 S0 的判定口径直接相关（虽不构成假绿，但**制造误读面**）|

**口径纪律**（`obj §5.1` KPI 单一真源）：`docs_sync_check` 的 KPI 断言只抓「当前 / 现行 / 真实」限定的**当前态** ⇒ **历史叙述不影响校验** ⇒ ⇒ **压缩历史流水不削弱判据**（这正是 L3 可安全做的原因）。

**单行阈的来源**：仓内既有可机测阈 = `scripts/test/agents_md_lint.py` 的「单行 ≤ 1000 字符」⇒ 本单元沿用同阈（⛔ 不自造新阈）。

## 范围

- **In-Scope**：
  - 4 份文档的 KPI 行：把长归档流水**搬到一个「历史快照」小节**（或指向台账 §11 的既有指针），保留「当前真实 baseline = N/3」一句在原位
  - `obj §5.2` 的订正链（8.5K）压缩
  - 3 份 `.rddf/improvements/*.md` 的 `状态:` 行回填「✅ 已完成并归档（日期 · Entry 锚）」
  - **【Q-9 · 用户 2026-11-20 已拍板】A 类 2 项**（OWNER 维度已废除；§1.3 实为 **8 项 live**，按「落点是否在 `openspec/specs/`」三分后**只有 A 类归本单元**）：**R-17** **2 份来源 change 归档目录缺失**（`dgpu-soc-timing` / `driver-visible-minimal-soc`；曾存在于 commit `0345d283` / `689752ef`）· **R-19** `AGENTS.md:423` 记载的 LWC handler 名 **stale**（写 `handle_cr_transaction`，实现实为 `register_doorbell_target` / `doorbell_write`，见 `lwc_bus_hub.hh:84,107`）
    - ⛔ **B 类 5 项不归本单元**（R-12 · R-13 · R-14 · R-15 · R-20 —— 落点**全在 `openspec/specs/`** ⇒ 走 `cep-spec-stale-refs-r14-r15-r20` 的 openspec change）
    - ⛔ **C 类 R-18 不归本单元**（doorbell 配置 vs hardcoded = 架构取舍 ⇒ architect **Q-12**）
    - ⚠️ **R-12 / R-13 原列在本单元是错的**（per 裁决记录 §10.9.3）—— 二者落点在 spec 内，而本单元 AC-6 明文禁止 `openspec/specs/` diff ⇒ 按原归属**物理不可执行**
    - ⚠️ **scope 变更已显式**（按裁决 §2.7 同款纪律）；登记册 §1.3 的重写已由 planner 在落盘时完成 ⇒ 本单元**只做这 4 项的具体修正**
- **Out-Scope**：
  - ⛔ KPI 活口径**取值**（归 `cep-stage-gate-p5-hardening`）
  - ⛔ 登记册 §0.1 / §1.5 / §6（归 `cep-registry-stale-cleanup`）
  - ⛔ `docs/architecture/**` · `docs/adr/**`（冻结面）
  - ⛔ 任何台账历史 Entry 的**原文**改动

## Why

**不做的代价**：`.rddf/roadmap.md` 是**每个 agent session 都会读**的高频面（AGENTS.md 指向它）；一行 6.8K 字符 × N 份 ⇒ 每次读都吃 token，且**真基线那句被埋在流水里** ⇒ 人与 grep 都难定位「当前是 0/3 还是 3/3」⇒ 这是 §6.2 S3-3（KPI 活口径）**难以被机械判据锚定**的次生原因。

**为什么状态漂移必须回填**：improvement 的 `状态:` 行是「这个单元处于什么阶段」的**唯一**速查面；3 份已归档的仍写「提案」⇒ 下一个 session 读 improvement 列表会以为它们待做 ⇒ **可能重复立项**。

**为什么放同一单元**：两件事都是**纯文档压缩/回填**，零设计、零契约面、零闸改动 ⇒ 拆开只会多花 2 次审批与 2 次 slot。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **4 份 KPI 行压缩** | 原位保留「**当前真实 baseline = N/3**」一句 + 一句指针（如「归档流水见 `obj §5.2` / 台账 §11」）；归档流水搬进同文档的「历史快照」小节或直接删除（⛔ **不删语义** ⇒ 若有唯一信息则保留在小节内）⇒ **单行 ≤ 1000 字符** |
| 2 | **`obj §5.2` 订正链压缩** | 同法：保留当前态 + 指针；⚠️ 台账是 **append-only** ⇒ ⛔ **不改历史 Entry 原文** ⇒ 本项若需改动历史段落 ⇒ ⛔ **改为追加一条订正 Entry**（归 `cep-registry-stale-cleanup` 的台账侧）⇒ ⇒ **本单元实际只改 4 份文档**，`obj §5.2` 列入**待裁**（需追加式授权） |
| 3 | **3 份 improvement 状态回填** | `状态:` 行改为「✅ **已完成并归档**（YYYY-MM-DD · 台账 Entry #N）」，格式与同目录既有已归档条目一致 |
| 4 | **单行阈断言** | 新增/扩展一个可机测断言（并入既有 `docs_sync_check` 或 `agents_md_lint`）⇒ 4 份文档的 KPI 行**单行 ≤ 1000 字符** ⇒ ⛔ 不新建闸脚本类型 |

## Acceptance

- [ ] **AC-1（4 份 KPI 行达阈）**：对 `phase-10.md` / `roadmap.md` / `umbrella.md` / `nvlink-sharp.md` 的 KPI 行逐行测长 ⇒ **全部 ≤ 1000 字符**；⚠️ 断言载体须可机测（并入既有闸），⛔ 不新建闸脚本类型
- [ ] **AC-2（当前态仍在原位且可 grep · ⚠️ 锚点措辞逐字保留）**：4 份文档的 KPI 行仍含「当前真实 baseline = N/3」或等价措辞，且该值 == 实测 `ls openspec/changes/ | grep -v archive | wc -l` / 3；⇒ 压缩**未削弱**当前态可读性。⚠️ **交叉引用约束（per 裁决 §9.2 SF-3）** = 压缩时 **6 文件 / 7 处活口径行的锚点措辞必须逐字保留**（以 `cep-stage-gate-p5-hardening` AC-11 的**锚点表**为准：`AGENTS.md` → `实测基线` · `roadmap.md` / `phase-10.md` / `umbrella.md` → `当前真实 baseline = ` · `nvlink-sharp.md` → `当前实测 **`（2 处）· `obj-cep-platform.md` → `**当前 Proposed 并发 = `（**粗体**））⇒ ⛔ **只搬历史流水，不动锚点句本身**；⚠️ 尤其 `nvlink-sharp.md` 的**两处**（`:31` / `:211`）⇒ ⛔ 压缩其一即打断 I-0 刚钉的锚；⚠️ **范围对账（per metis S7）**：AC-1/AC-2/AC-3 的**受测对象 = 4 份文档**（`phase-10.md` / `roadmap.md` / `umbrella.md` / `nvlink-sharp.md`）；而 6 文件锚点表里的 **`AGENTS.md` 与 `obj-cep-platform.md` 均不在本单元 In-Scope** ⇒ 它们的锚点**天然免疫**（本单元不改它们 ⇒ 其措辞不变 ⇒ ⛔ **不构成范围矛盾**）；⚠️ 反之：若将来有人把 `AGENTS.md` 纳入本单元 ⇒ ⛔ 必须同步加一条「压缩前后 6 文件 7 处锚点 `grep -F` **逐字相等**」的判据（⛔ 本轮不纳入范围）
- [ ] **AC-3（历史语义未丢）**：压缩前后，4 份文档中出现的**台账 Entry 号集合**逐个相等（判据 = 压缩前集合 ⊆ 压缩后集合，且被搬入「历史快照」小节的条目数 == 移出数）；⚠️ 若某 Entry 号在全仓无其它出处 ⇒ **必须保留**在该小节内
- [ ] **AC-4（3 份状态回填）**：3 份 `.rddf/improvements/*.md` 的 `状态:` 行含「已归档」+ 日期 + Entry 锚；⚠️ **状态行之外的内容零改动**（判据 = `git diff` 中每份文件仅 1 行改动）
- [ ] **AC-5（台账 append-only）**：`git diff` 对 `.rddf/roadmap/objectives/obj-cep-platform.md` **零改动**（本单元的 `obj §5.2` 诉求若落地 ⇒ 走 `cep-registry-stale-cleanup` 的追加式订正 Entry）
- [ ] **AC-6（范围纪律）**：`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/` · `openspec/specs/` · `include/**` · `src/**` · `test/**` · `scripts/**`；⇒ **零冻结面 diff**、**零代码 diff**
- [ ] **AC-7（零 OpenSpec change · 零 KPI 占用）**：不创建 `openspec/changes/<name>/`；`ls openspec/changes/ | grep -v archive | wc -l` 前后相等
- [ ] **AC-8（0 回归）**：`ctest` 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无） | — | ⛔ **零能力增量** —— 纯文档压缩 + 状态回填 ⇒ **不产 delta spec**、**不合并 main spec** ⇒ 归档时**零契约面变化** |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | `.rddf/roadmap/phases/phase-10.md` · `.rddf/roadmap.md` · `.rddf/improvements/cpptlm-cep-umbrella.md` · `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` · `.rddf/improvements/cpptlm-cep-dcoh-nvlink-refkind-migration.md` · `.rddf/improvements/cpptlm-cep-ualink-coherence-engine.md` · `.rddf/improvements/cpptlm-cep-basis-source-kind-and-cell-free.md` · （可能）既有 docs_sync / agents_md_lint 断言 |
| **零改动（硬查）** | `docs/adr/**` · `docs/architecture/**` · `openspec/**` · `include/**` · `src/**` · `test/**` · `.rddf/roadmap/objectives/**` |
| **冻结面** | **零**（`.rddf/**` 不在 `FROZEN_GLOBS`）⇒ 不触五步链 |
| **闸** | ctest **0 新增**（断言并入既有闸）· `docs_sync_check` / `agents_md_lint` 可能 +1 断言 |
| **KPI** | **0/3 → 0/3**（零槽位批路径） |
| **风险** | ① 压缩时误删唯一语义 ⇒ AC-3 的 Entry 号集合比对兜底 ② 压缩时误删「当前态限定词」⇒ `docs_sync_check` 的 KPI 断言必红（它只抓当前态限定词）⇒ 天然兜底 ③ 与 `cep-stage-gate-p5-hardening` 同改 KPI 相关文件 ⇒ **文件冲突面** ⇒ 本单元只改**体积**，不改**判据**（判据在后者）⇒ 冲突面最小 ④ `obj §5.2` 诉求受 append-only 约束 ⇒ 走追加式（AC-5 划界） |

## 待裁

✅ **无 architect 裁决项** —— 本单元是 planner 域的纯文档治理。
⚠️ 唯一待定 = **`obj §5.2` 的 8.5K 订正链**（台账 append-only ⇒ 原地压缩会改历史段落）⇒ 本单元**不改**它；落地路径 = 追加一条订正 Entry（与 `cep-registry-stale-cleanup` 同批）。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis | 指出「d 数是纸面并行」+「清单增殖」类债 | ⇒ 本单元属 S3 治理债 |
| 2 | oracle | **REJECT** · 修法 3（Q-8 越域） | ⇒ 非 CEP 域项已移出 |
| 3 | critic（v2） | **REJECT** · 3 BLOCKER | ⇒ 本单元不碰 B2 行 / KPI 判据 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ **本单元的 KPI 相关部分被收窄**：取值同步归 `cep-stage-gate-p5-hardening`，本单元只压体积 |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | ⇒ 不涉及本单元 |

⇒ **本单元可立即开工**（⛔ 不占 KPI slot）。
