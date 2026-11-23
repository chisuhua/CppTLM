---
name: cep-litmus-falsifier-naming-disambiguation
priority: P2
phase: phase-10
category: core-impl
type: refactor
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
- ADR-CEP-13
状态: ✅ 已实施·已归档(planner 2026-11-23 · metis 1 轮已处置 · builder 2026-11-23 阶段 1 实施 + 验收 PASS + 归档)
created: 2026-11-23
roadmap_ref:
  project_id: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
  phase: phase-10
---

# cep-litmus-falsifier-naming-disambiguation: 「litmus」命名与能力错配的消歧

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改**任何证伪逻辑（不改找环算法 / 不改语料格式 / 不改三态判定语义）——
> 本单元**只做命名与表述的消歧**。⛔ **不扩**语料（⊆ 既有 backlog `cep-memory-model-operational-trace`）·
> ⛔ **不定义** `rfe` / `fr` / `ws` 语义 · ⛔ **新增 invariant 数 = 0**。
> **分阶段**：**阶段 1（零冻结面 · 可立即实施）** / **阶段 2（触碰冻结面 · 另立）**。

---

## 架构依据

### 1. 事实：`litmus_falsifier` 不是内存模型 litmus 测试

| 事实 | 一手出处 |
|---|---|
| 顶点 = `(instance, network, vc)` ⇒ 与装配期**通道依赖图**同形 | `openspec/specs/cep-litmus-falsification/spec.md`（该 spec 自陈） |
| 判据 = **图上的环存在性**（`g.find_cycle()`）；边类 `rfe` / `fr` / `ws` **仅记录、不解释** | `include/tlm/cep/litmus_falsifier.hh` |
| 语料实测为空（`compound_edges: []`）⇒ 当前判定 = 「不可证伪」 | `test/cep_litmus_corpus.json` |

### 2. 对照：论文侧的 litmus 是**另一个东西**

Oswald 博士论文 §8.7.2 / HeteroGen §VII 用 herd7 生成 **111 个异构 memory-model litmus**（线程 × 读写 × 值 × 允许结果），
检验的是**内存模型公理的可满足性**（`Ld r1=y` 与 `St x=1` 的重排能否被观测）。

⇒ **两者同名为「litmus」，但范畴不同**：
本仓的工装检验**通道依赖图的环**（装配期静态图），论文的 litmus 检验**执行迹的允许结果**（运行期语义）。

### 3. 不做会怎样

| # | 后果 | 严重度 |
|:--:|---|:--:|
| **C1** | 读者 / 评审据文件名（`litmus_falsifier`）误判本仓**具备**内存模型 litmus 能力 ⇒ **能力高估** | 🔴 高 |
| **C2** | 与论文侧对齐时把「CEP 有 litmus」当作「CEP 已能证伪复合一致性公理」⇒ 结论错位（与 `ADR-CEP-13` 决策 5「不提供无违例保证」直接矛盾） | 🔴 高 |
| **C3** | 未来的操作级模型（backlog）落地时，两个「litmus」并存 ⇒ 载体归属歧义 | 🟡 中 |

---

## 范围

### In-Scope

**阶段 1（零冻结面 · 本单元的交付主体）**

1. `include/tlm/cep/litmus_falsifier.hh` 内新增**消歧常量**（`kFalsifierScope` / `kFalsifierIsMemoryModelLitmus = false`），使其可被下游断言消费
2. 头文件顶部的「诚实边界」段**升级为可引用的一句话声明**（覆盖「顶点形态 / 判据对象 / 与内存模型 litmus 的区别」）
3. **禁用表述闸**（新）：`scripts/test/cep_litmus_naming_check.sh` —— 在 `include/tlm/cep/litmus_falsifier.hh` 与相关测试/文档中，出现「memory-model litmus」「herd7」「RISC-V litmus」等字样时**必须同段带否定语境**（否则红因码 `litmus-scope-claim`）

**阶段 2（触碰冻结面 · 另行立项）**

4. 实际改名：`litmus_falsifier.hh` ⇒ `compound_graph_falsifier.hh`；`evaluate_litmus_falsification` ⇒ `evaluate_compound_graph_falsification`；`LitmusVerdict` ⇒ `CompoundGraphVerdict`
   ⚠️ 因 `openspec/specs/cep-litmus-falsification/spec.md` 命中冻结 glob（M1 清单）⇒ 主 spec 的 requirement 需走**五步授权链**（`--update --authorized-by` + 清单单独 commit + 无参复验）⇒ **不在阶段 1 做**

### Out-of-Scope（⛔ 逐条列明）

| 排除项 | 理由 |
|---|---|
| **改找环算法 / 语料格式 / 三态判定语义** | ⛔ 本单元只做命名消歧；逻辑等价是硬约束 |
| **扩语料 / 定义 `rfe`/`fr`/`ws`** | 既有 backlog `cep-memory-model-operational-trace` |
| **阶段 2 的改名** | 触碰冻结面 spec ⇒ 五步链 ⇒ 须单独排期（本单元只登记） |
| **改「被取代」的既有闸臂** | `scripts/test/cep_litmus_falsification_check.sh` 的逻辑不动（只可能在阶段 2 随改名同步） |

---

## Why

- **命名是判据的一部分**：本仓已多次因「名字与能力不符」产生治理成本（如 `cep-invariant-gate` 实际是 ADR 治理面机器化，而非一致性 invariant 闸 —— 见 `openspec/specs/cep-invariant-gate/spec.md` 的定位澄清）。
- **为什么阶段 1 就能产生价值**：**消歧常量**让「本工装不是内存模型 litmus」变成**下游可断言的事实**（而非散文），且**零冻结面**；阶段 2 的改名是「锦上添花」，不是价值前提。
- **为什么不做成「直接改名」**：`openspec/specs/cep-litmus-falsification/spec.md` 在 M1 冻结清单 ⇒ 改名必须走五步授权链；把一个「命名微调」升级为「冻结面变更」不划算。

---

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| **1** | `include/tlm/cep/litmus_falsifier.hh` 新增**消歧常量** | `inline constexpr const char* kFalsifierScope = "channel-dependency-graph";` · `inline constexpr bool kFalsifierIsMemoryModelLitmus = false;` ⇒ 下游可 `static_assert(!kFalsifierIsMemoryModelLitmus)` |
| **2** | 头文件顶部的「诚实边界」段**升级** | 新增一段：**顶点形态**（`(instance, network, vc)`）+ **判据对象**（环存在性）+ **与内存模型 litmus 的区别**（论文 §8.7.2 的 herd7 litmus 检验执行迹的允许结果，本工装检验装配期静态图的环）⇒ 可直接被文档引用 |
| **3** | **禁用表述闸** `scripts/test/cep_litmus_naming_check.sh`（新建） | 扫描集 = `include/tlm/cep/litmus_falsifier.hh` + `test/test_cep_litmus_falsification.cc` + `openspec/specs/cep-litmus-falsification/spec.md`；命中「memory-model litmus」/「herd7」/「RISC-V litmus」/「内存模型 litmus」⇒ **同一行**内**必须**含**否定标记**（⚠️ 否定词表 = `不是` / `非` / `⛔` / `不` —— **⛔ 不含「区别」**：「X 与内存模型 litmus 的区别是 Y」是**肯定句**，不构成否定）⇒ 否则红因码 `litmus-scope-claim`<br>⚠️ **「同一行」定义**：以 `\n` 为界（⛔ 不按空行分段 —— 否则维护期一行换行即抖动）<br>⚠️ **零冻结面**：本闸**只读** spec，⛔ 不改 spec |
| **4** | 测试断言（`test/test_cep_litmus_falsification.cc` 追加） | `static_assert(!tlm::cep::kFalsifierIsMemoryModelLitmus)` + 断言 `kFalsifierScope` 逐字等于 `"channel-dependency-graph"` |
| **5** | *（阶段 2 · 另行立项）* 改名 | ⛔ 本单元不做 |

---

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（消歧常量存在且取值正确 · 机检）**：`kFalsifierScope` 逐字 == `"channel-dependency-graph"` ∧ `kFalsifierIsMemoryModelLitmus == false`；反 fixture：把 `kFalsifierIsMemoryModelLitmus` 改成 `true` ⇒ `static_assert` 编译失败（红因可见）
- [ ] **AC-2（禁用表述闸 · 行为判据 · 三路）**：① 插入「本工装是 memory-model litmus」⇒ 必红（红因码 `litmus-scope-claim`）② 插入「**不是** memory-model litmus」⇒ 必须绿 ③ 插入「**X 与内存模型 litmus 的区别是 Y**」（**肯定句**，无否定标记）⇒ **必红** ⇒ 覆盖「区别」非否定词这一路径
- [ ] **AC-3（头文件诚实边界可引用 · 人工评审并入机检范围）**：`litmus_falsifier.hh` 的边界段须同时含**顶点形态**、**判据对象**、**与内存模型 litmus 的区别**三要素 ⇒ 以**关键词共现**机检（`(instance, network, vc)` ∧ `环` ∧ 否定标记），⚠️ 语义是否准确属**人工评审**（⛔ 不写成 checkbox 的判定）
- [ ] **AC-4（逻辑等价 · 零行为变更）**：既有断言段**逐字未变**且全绿（⚠️ **允许文件尾部追加新断言**，追加区与既有区以注释行 `// == disambiguation additions ==` 分隔）；`scripts/test/cep_litmus_falsification_check.sh` rc=0 ⇒ 证明本单元是**纯命名/表述**变更
- [ ] **AC-5（零冻结面 · 机检）**：`git diff --name-only` **不含** `openspec/specs/cep-litmus-falsification/spec.md` · `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` ⇒ 违反即阶段 1 失败（应改走阶段 2）
- [ ] **AC-6（零回归）**：`ctest` 全绿（⛔ 不写死断言数）· 全部 `scripts/test/cep_*.sh` rc=0 · `bash scripts/test/cep_docs_sync_check.sh --strict` rc=0
- ~~**AC-7（阶段 2 显式登记）**~~ ⇒ **已在 planner 阶段闭合**（本条原为「Impact 段写明阶段 2 触发条件」的自检项 ⇒ 内容已落地，且 planner 自检已过 ⇒ ⛔ **不留给 verifier 勾**：恒真项零判别力）。阶段 2 触发条件见 Impact 表末行。

---

## Capabilities

> **形态裁决（planner 现定 · ⛔ 不留给 rdd-builder P0）**：**阶段 1 不独立成 spec** ——
> 能力由「头文件常量 + 闸」承载 ⇒ **无 spec delta** ⇒ 属 refactor 类 change ⇒ 归档时 `skip_specs: true`
> ⇒ **零 main spec 产出**（先例 = `2026-11-09-cpptlm-cep-bidirectional-multiport-inbound`：代码改动 + 零 delta）。
> ⚠️ 若 rdd-builder 或 verifier 认为必须有 spec 面 ⇒ 属**阶段 2** 的范畴（含改名 + 冻结面五步链），⛔ 不得在阶段 1 补 delta。
>
> **KPI 口径修正（2026-11-23 · 实施前实测修正）**：⛔ 本单元**不适用** AGENTS.md 的「doc-only cleanup 不占 slot」豁免
> —— 该豁免的判据是「**0 函数签名/逻辑/构建改动**」，而本单元含**新闸脚本 + ctest 注册**（构建面改动）
> ⇒ **立项期间占 1 slot**（实测口径 = `openspec list --json` 的 active 数；探针实测 `skip_specs: true` 的 change **仍被计入** active）
> ⇒ 活文档 KPI 活口径：**实施期 1/3** · 归档后回 **0/3**（与 I1 `cep-protocol-engine-capability-declaration` 同构）。
> ⚠️ 「不占 slot」在**能力 spec 面**上成立（`openspec/specs/cep-*` 计数 **零变化**），在 **change 存量面**上**不成立** —— 两个面不可混谈。

| capability | 关系 | 说明 |
|---|---|---|
| `cep-litmus-falsification` | **未改**（阶段 1 零冻结面 · 无 spec delta） | 阶段 1 只加消歧常量与表述闸 |
| —（无新 capability） | — | 阶段 1 的能力由 `include/tlm/cep/litmus_falsifier.hh` 的常量 + `scripts/test/cep_litmus_naming_check.sh` 承载 |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `scripts/test/cep_litmus_naming_check.sh` ·（可选）`openspec/specs/cep-litmus-naming-disambiguation/spec.md` |
| **修改文件** | `include/tlm/cep/litmus_falsifier.hh`（**追加**常量 + 边界段升级）· `test/test_cep_litmus_falsification.cc`（追加 2 条断言） |
| **零改动（硬查 · AC-5）** | `openspec/specs/cep-litmus-falsification/spec.md` · `scripts/test/cep_litmus_falsification_check.sh` · `test/cep_litmus_corpus.json` · `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` |
| **冻结面** | **无**（阶段 1）⇒ ⛔ 不需要五步链；**阶段 2 有**（spec REMOVED requirement + 改名） |
| **闸** | `scripts/test/cep_litmus_naming_check.sh` 新增 1 个 · ctest 断言 +2（⛔ 数量不是鉴别力指标） |
| **KPI** | **实施期占 1 slot**（`skip_specs: true` 的 change 仍计入 `openspec list --json` active 数）· 归档后释放回 0/3 · **能力 spec 面零变化**（`openspec/specs/cep-*` 计数不变 ⇒ 冻结清单不变 ⇒ 无需五步链） |
| **风险** | ① 禁用表述闸**误报**（正常讨论「litmus」被误判）⇒ 缓解：只匹配**肯定式**表述，且要求同段无否定标记 ② 消歧常量被误读为「能力增强」⇒ 缓解：常量名带 `IsMemoryModelLitmus = false`（**否定式命名**，语义自明）③ 阶段 2 被遗忘 ⇒ 缓解：AC-7 强制登记触发条件 |
| **阶段 2 触发条件（AC-7）** | **触发** = ① 有人主张改名，或 ② 操作级模型（`cep-memory-model-operational-trace`）落地需「litmus」名让位。**成本** = 冻结面五步链 ×1（`openspec/specs/cep-litmus-falsification/spec.md` 的 REMOVED requirement）+ 全仓引用改名 |

---

## 审查轨迹

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|:--:|---|
| 1 | **metis** | **提出 P1 × 2 + P2 × 1（未发现 P0）** | ✅ **全部处置**：P1-8「既有测试逐字未变」与「追加 2 条断言」措辞冲突 ⇒ AC-4 改为「既有断言段逐字未变，允许尾部追加（注释行分隔）」· P1-11「区别」不是否定词 ⇒ 从否定词表删除 + AC-2 增第三路反 fixture（肯定句「与…的区别是」⇒ 必红）+ 「同一行」边界定为 `\n` · P1-12 AC-7 恒真 ⇒ **planner 阶段闭合**，⛔ 不留给 verifier · P2-17 capability 形态 ⇒ **planner 现定：阶段 1 不独立成 spec、不占 KPI slot** |
