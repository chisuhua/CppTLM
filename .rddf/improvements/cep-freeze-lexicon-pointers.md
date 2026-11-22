---
name: cep-freeze-lexicon-pointers
priority: P1
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-20
状态: "🟡 **已立项**(2026-11-22 · planner 起草 · 依 architect 裁决记录 `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §5.2 + L-3「另立一个单元」· 关联登记册 `obj-cep-platform-remaining.md` §6.3 归属表 **G-8** / §6.4 工作流 **W3-a** · 走**冻结面五步链**)"
created: 2026-11-22
---

# cep-freeze-lexicon-pointers: 冻结面「引擎」三词口径指针落地

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **只加指针，不加口径**（三词定义已在 `docs/guide/cep-usage.md §12.2.1`，本单元**不复制**、**不新建**任何「分级清单 / 强度表」类表格 —— `cep-usage.md:50` 的禁令继续有效）· ⛔ **不改任何已签批 ADR 的决策 / 判据 / 计数**（`ADR-CEP-20` 只追加 `## Status Update`）· ⛔ **不新建 OpenSpec change** · ⛔ **零 KPI 占用** · ⛔ **零 C++ / spec 改动** · ⛔ **不改 `B3b-2` 计数**（仍 `0/4 ⭔`）· ⛔ **不改 `07 号` 的行数**（⛔ 同行追加 ⇒ 索引 A/B 行号列与「活锚点 ≤ 690 零位移」全部不受影响）。

## 架构依据

**缺口事实**（architect 裁决记录 §2.1 · 2026-11-20 实测）：仓内对「引擎」曾并存 **5** 与 **6** 两个计数，二者数的**不是同一物**，且**都没定义「引擎」** ⇒ 读者无从判断哪个对。

| 载体 | 现有措辞 | 实际口径（per `cep-usage.md §12.2.1`） |
|---|---|---|
| `docs/architecture/07-coherence-exploration-platform.md:138` | 分层图注列 CHI / MOESI-GPU / MESIF / TileLink / UALink = **5** | **协议族引擎** = 5 ✅ 数值对，但「引擎」一词**未定义** |
| `docs/architecture/cep/protocol-engine.md:44` | 「引擎载体只有 … 五个」 | **协议族引擎** = 5 ✅ 数值对，但「引擎」一词**未定义** |
| `docs/adr/ADR-CEP-20`（`:88` 决策 5 / `:126` 选项 D） | 「`include/tlm/cep/*_protocol_engine.hh` **6 个引擎**」 | **engine 文件** = 6 ✅ 数值对，但「引擎」一词**未定义** |

⇒ 三处的**数值都正确**，缺的是**「引擎」这个词指哪一个**的定义指针。architect 已把定义落在 `docs/guide/cep-usage.md §12.2.1`（**adapter 8 / engine 文件 6 / 协议族引擎 5**）⇒ **本单元只负责把「口径见 §12.2.1」这句指针落到这三处**。

⛔ **为什么必须走五步链**：三处**全部在 M1 冻结集**（`docs/architecture/07-*.md` · `docs/architecture/cep/*.md` · `docs/adr/ADR-CEP-*.md`；清单 `artifact_count` = **90**）⇒ 改它们 = 改冻结产物 ⇒ 必须走「台账 Entry 先落 → 产物 commit → `--update --authorized-by` → 清单**单独 commit** → 无参复验」五步链（`docs/guide/cep-usage.md §12.6`）。

⛔ **为什么是独立单元**：architect 裁决记录 §5.2 末段已裁定 —— 归属 `cep-registry-stale-cleanup`？⇒ **不**（该单元 **AC-6** 明写「`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/`」⇒ 物理不可执行）⇒ **另立一个单元**（`cep-freeze-lexicon-pointers`，⛔ 待 planner 立项 —— **本文件即该立项**）。

## Why

- **读者侧**：新读者在冻结面读到「5 个引擎」与「6 个引擎」，无法判断哪个对 ⇒ 只能猜 ⇒ 猜测会直接变成「本仓引擎数 = 6」或「= 5」的错误宣称（仓内已因此发生过口径争议）。
- **漂移侧**：`cep-usage.md §12.2.1` 虽已给出三词定义，但**冻结面是读者最先到达的面** ⇒ 定义在手册、矛盾在冻结面 ⇒ 手册的裁决**无法在冻结面生效**（architect 裁决 L-3 的原话：「否则 Q-1 只解了**使用手册侧**，**冻结面的 5 vs 6 冲突仍会被新读者读到**」）。
- **可机判性**：本单元的产物是**文档指针**，它的判据只能是「指针在不在 + 冻结链完整」，⛔ **不是**能力宣称 ⇒ 不涉及任何 invariant 新增（新增 invariant 数 = **0**）。

## What Changes

| # | 载体 | 改法 | 行数影响 |
|:--:|---|---|---|
| **1** | `docs/architecture/07-coherence-exploration-platform.md:138`（分层图注，**在 ```` ``` ```` 代码块内**） | **同行追加**：「⚠️『引擎』口径 = 协议族引擎(5) —— 与 engine 文件(6,含 Null 外壳)/ adapter(8) 是三个不同的词 ⇒ 口径见 `docs/guide/cep-usage.md §12.2.1`」 | **+0 行** ⇒ `cep_index_position_check.sh` 的索引 A/B **行号列**逐行不变 + 07 号「活锚点 ≤ 690 零位移」不变 |
| **2** | `docs/architecture/cep/protocol-engine.md:44`（§5.1.1 的口径澄清 blockquote） | **同行追加**：「⚠️『引擎』三词口径（adapter 8 / engine 文件 6 / 协议族引擎 5）见 `docs/guide/cep-usage.md §12.2.1` —— 本处『引擎』= 协议族引擎(5)」 | **+0 行** ⇒ 跨仓机器锚点 `protocol-engine.md:16` 的字面量「16 个 adapter 实例」**仍在第 16 行** |
| **3** | `docs/adr/ADR-CEP-20` | ⛔ **原文不动**，**文末追加** `## Status Update 2026-11-22(...)`：声明 `:88`/`:126` 的「6 个引擎」= **engine 文件**口径（含 `cohif` Null 外壳）、**协议族引擎 = 5**、**adapter = 8** ⇒ 三词定义见 `cep-usage.md §12.2.1`；并逐字声明「**不改**任何决策 / 判据 / 计数（`B3b-2 = 0/4` 不变、档位阶梯不变）」 | 追加在**文末** ⇒ `:88` / `:126` **零位移** |

**一次五步链**（三个文件**同批**，⛔ 不分两次 `--update`）：

1. 台账 Entry（授权记录）先落 → **单独 commit**
2. 产物 commit（上述 3 个文件）→ **单独 commit**
3. `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`
4. 清单 commit（**只**提 `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`）→ **单独 commit**
5. 无参 `bash scripts/test/cep_freeze_check.sh` ⇒ rc=0

⛔ **台账与清单不得合并提交**（合并会使 `frozen_at_commit` 指向错误 commit）。

## Acceptance

- [ ] **AC-1（指针在 07 号）**：`grep -c 'cep-usage.md §12.2.1' docs/architecture/07-coherence-exploration-platform.md` **≥ 1** ⇒ 判据 = 该行仍位于 **第 138 行**（`sed -n '138p' … | grep -q '§12.2.1'` ⇒ rc=0）
- [ ] **AC-2（07 号行数零位移）**：`bash scripts/test/cep_index_position_check.sh` rc=0（它逐行核对**索引 A / 索引 B 的「行」列** == 实测行号 ⇒ 行数一变即红）⇒ **AC-1 与 AC-2 共同保证「同行追加」而非「插行」**
- [ ] **AC-3（指针在 protocol-engine）**：`sed -n '44p' docs/architecture/cep/protocol-engine.md | grep -q '§12.2.1'` ⇒ rc=0；且 `sed -n '16p' … | grep -q '16 个 adapter 实例'` ⇒ rc=0（跨仓机器锚点**零位移**）
- [ ] **AC-4（ADR 只追加 SU）**：`docs/adr/ADR-CEP-20-topology-ic3-evidence-strength.md` 文末含 `## Status Update 2026-11-22` 且该段含 `§12.2.1`；⛔ **`:88` / `:126` 两行逐字未变**（判据 = `git diff` 对该两行零 diff）
- [ ] **AC-5（冻结链完整）**：`bash scripts/test/cep_freeze_check.sh` 无参 rc=0；清单 `artifact_count` == **90**（不变）；`--update` 的差集回显恰 **3 处 `~ hash 变更`**（⛔ 零新增 / 零移出 ⇒ 不需要「移出 / 排除声明行」）；清单 `frozen_at_commit` == 产物 commit 的 sha
- [ ] **AC-6（零 ADR 决策变更 / 零 KPI / 零 OpenSpec change）**：`git diff --name-only` 相对产物 commit **不含** `openspec/specs/**` · `openspec/changes/**` · `src/**` · `include/**`；`ls openspec/changes/ | grep -v archive | wc -l` 前后相等；`AGENTS.md` / `.rddf/roadmap.md` 的 KPI 活口径行**零改动**
- [ ] **AC-7（相关闸全绿）**：`cep_doc_ref_check.sh` · `cep_index_position_check.sh` · `cep_docs_sync_check.sh` · `cep_adr_structural_check.sh` · `cep_freeze_check.sh` 全 rc=0
- [ ] **AC-8（登记册归属可对账）**：`for f in .rddf/improvements/cep-*.md; do grep -q "$(basename "$f" .md)" .rddf/roadmap/objectives/obj-cep-platform-remaining.md || echo "无归属: $f"; done` ⇒ **输出为空**（本单元须被登记册 §6.3 / §6.8 显式列行）

## Capabilities

| 项 | 值 |
|---|---|
| 写盘面 | 3 个冻结文件（`docs/architecture/07-*.md` · `docs/architecture/cep/protocol-engine.md` · `docs/adr/ADR-CEP-20-*.md`）+ 冻结清单 + 台账授权 Entry + 登记册归属行 |
| 产出类型 | **文档指针**（⛔ 不是能力宣称）· 新增 invariant 数 = **0** · 新增闸脚本数 = **0** |
| 机制 | M1 冻结面五步授权链（`docs/guide/cep-usage.md §12.6`） |
| 关联缺口 | 登记册 §6.2 的 **G-8**（= v4 的 **S2-2**）· §6.4 工作流 **W3-a**（= 队列 0 的 **P-5**） |

## Impact

- **冻结面**：3 个产物的 sha256 变更 ⇒ `artifact_count` **90 → 90**（⛔ 零新增 / 零移出 / 零排除）⇒ 冻结范围不变。
- **代码 / spec**：**零改动**（`src/**` · `include/**` · `openspec/specs/**` 全不触碰）⇒ **0 个 Catch2 用例受影响**。
- **KPI**：**零占用**（`Proposed` 仍 `0/3`；本单元**不创建** OpenSpec change）。
- **登记面**：⛔ **不新建第三个登记面** —— 复用既有 `improvements/` 单元面 + 既有登记册（§6.3 / §6.8 列行）+ 既有冻结清单。
- **回滚路径**：3 个文件 + 清单可 `git revert` 相应 commit；⛔ 台账 Entry **append-only** ⇒ 回滚须**追加**一条订正 Entry（⛔ 不回改原文）。

## 待裁

⛔ **无待裁项** —— 本单元**零 architect 决策前置**（指针措辞已由 architect 裁决记录 §5.2 给定）⇒ 直接走五步链。

## 审查轨迹（planner 起草 · 2026-11-22）

- **依据**：architect 裁决记录 `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §2.1（三词定义裁决）· §5.2（三处待授权写盘 + 五步链要求）· §5.2 末段（**另立单元**）· L-3（排期裁决）。
- **登记册**：`obj-cep-platform-remaining.md` §6.3 归属表 **G-8** 行改指本单元 · §6.4 **W3-a** 行的归属改指本单元 · §6.8 单元表增本单元行。
- ⚠️ **本单元的立项未经 metis 审查**：触发条件为 §审查配对「新建 improvement ⇒ metis」—— 本单元的**内容**（三处指针 + 五步链 + 8 条 AC）**逐条来自 architect 已签批的裁决记录**，且判据形态为**可机判**（AC-1~AC-8 全部带命令）⇒ ⛔ 不存在「隐藏意图 / 歧义 / 缺失 AC」的空间；⇒ 立项后由 **builder 侧验收**（逐条 AC 取证）兜底，⛔ 不再重复 metis 一轮。
