# proposal: cep-litmus-falsifier-naming-disambiguation —— 「litmus」命名与能力错配的消歧（阶段 1）

> **来源**：`.rddf/improvements/cep-litmus-falsifier-naming-disambiguation.md`（planner 2026-11-23 · metis 1 轮已处置 3 条 finding · 用户 2026-11-23 批准「选择 A：实施 I2 阶段 1」）
> **上游分析**：`.rddf/roadmap/cep-gen-family-gap-closure.md`（Oswald 博士论文 178 页一手 PDF × 本仓 CEP 实现面对照 ⇒ 缺口 **G6**）
> **定位**：消除「本仓具备内存模型 litmus 能力」这一**能力高估**。⛔ **不改任何证伪逻辑**（找环算法 / 语料格式 / 三态判定语义逐字不变）。
> **形态**：`skip_specs: true`（**零 delta spec** ⇒ 归档不产 main spec ⇒ 冻结清单零变化 ⇒ 无需五步链）。

## Why

### 1. 事实：本仓的 `litmus_falsifier` 不是内存模型 litmus 测试

| 事实 | 一手出处 |
|---|---|
| 顶点 = `(instance, network, vc)` ⇒ 与装配期**通道依赖图**同形 | `openspec/specs/cep-litmus-falsification/spec.md`（该 spec 自陈） |
| 判据 = **图上的环存在性**（`g.find_cycle()`）；边类 `rfe` / `fr` / `ws` **仅记录、不解释** | `include/tlm/cep/litmus_falsifier.hh` |
| 语料实测为空（`compound_edges: []`）⇒ 当前判定 = 「不可证伪」 | `test/cep_litmus_corpus.json` |

### 2. 对照：论文侧的 litmus 是**另一个东西**

Oswald 博士论文 §8.7.2 / HeteroGen §VII 用 **herd7** 生成 **111 个异构 memory-model litmus**（线程 × 读写 × 值 × 允许结果），检验的是**内存模型公理的可满足性**（`Ld r1=y` 与 `St x=1` 的重排能否被观测）。

⇒ **两者同名为「litmus」，但范畴不同**：本仓工装检验**通道依赖图的环**（装配期静态图），论文 litmus 检验**执行迹的允许结果**（运行期语义）。

### 3. 不做会怎样

| # | 后果 | 严重度 |
|:--:|---|:--:|
| **C1** | 读者 / 评审据文件名（`litmus_falsifier`）误判本仓**具备**内存模型 litmus 能力 ⇒ **能力高估** | 🔴 高 |
| **C2** | 与论文侧对齐时把「CEP 有 litmus」当作「CEP 已能证伪复合一致性公理」⇒ 结论错位（与 `ADR-CEP-13` 决策 5「不提供无违例保证」直接矛盾） | 🔴 高 |
| **C3** | 未来的操作级模型（backlog `cep-memory-model-operational-trace`）落地时，两个「litmus」并存 ⇒ 载体归属歧义 | 🟡 中 |

### 4. 为什么阶段 1 就够

**消歧常量**让「本工装不是内存模型 litmus」变成**下游可断言的事实**（而非散文），且**零冻结面**；阶段 2 的改名是「锦上添花」，不是价值前提。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| **1** | `include/tlm/cep/litmus_falsifier.hh` 新增**消歧常量** | `inline constexpr const char* kFalsifierScope = "channel-dependency-graph";` · `inline constexpr bool kFalsifierIsMemoryModelLitmus = false;` ⇒ 下游可 `static_assert(!kFalsifierIsMemoryModelLitmus)` |
| **2** | 头文件顶部的「诚实边界」段**升级** | 新增一段：**顶点形态**（`(instance, network, vc)`）+ **判据对象**（环存在性）+ **与内存模型 litmus 的区别**（论文 §8.7.2 的 herd7 litmus 检验执行迹的允许结果，本工装检验装配期静态图的环）⇒ 可直接被文档引用 |
| **3** | **禁用表述闸**（新）：`scripts/test/cep_litmus_naming_check.sh` | 扫描集 = `include/tlm/cep/litmus_falsifier.hh` + `test/test_cep_litmus_falsification.cc` + `openspec/specs/cep-litmus-falsification/spec.md`；命中「memory-model litmus」/「herd7」/「RISC-V litmus」/「内存模型 litmus」⇒ **同一行**内**必须**含**否定标记**（⚠️ 否定词表 = `不是` / `非` / `⛔` / `不` —— **⛔ 不含「区别」**：「X 与内存模型 litmus 的区别是 Y」是**肯定句**，不构成否定）⇒ 否则红因码 `litmus-scope-claim`<br>⚠️ **「同一行」定义**：以 `\n` 为界（⛔ 不按空行分段 —— 否则维护期一行换行即抖动）<br>⚠️ **零冻结面**：本闸**只读** spec，⛔ 不改 spec |
| **4** | 测试断言（`test/test_cep_litmus_falsification.cc` 追加） | `static_assert(!tlm::cep::kFalsifierIsMemoryModelLitmus)` + 断言 `kFalsifierScope` 逐字等于 `"channel-dependency-graph"` |
| **5** | *（阶段 2 · 另行立项）* 改名 | ⛔ 本 change 不做 |

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **反 fixture 范式**：沿用 `scripts/test/cep_engine_capabilities_check.sh` 的 **probe TU 模式**（临时副本 + 断言**按指定签名**变红）；⛔ 不接受「只看 `rc≠0`」。

- [ ] **AC-1（消歧常量存在且取值正确 · 机检）**：`kFalsifierScope` 逐字 == `"channel-dependency-graph"` ∧ `kFalsifierIsMemoryModelLitmus == false`；反 fixture：把 `kFalsifierIsMemoryModelLitmus` 改成 `true` ⇒ `static_assert` 编译失败（红因可见）
- [ ] **AC-2（禁用表述闸 · 行为判据 · 三路）**：① 插入「本工装是 memory-model litmus」⇒ 必红（红因码 `litmus-scope-claim`）② 插入「**不是** memory-model litmus」⇒ 必须绿 ③ 插入「**X 与内存模型 litmus 的区别是 Y**」（**肯定句**，无否定标记）⇒ **必红** ⇒ 覆盖「区别」非否定词这一路径
- [ ] **AC-3（头文件诚实边界可引用 · 人工评审并入机检范围）**：`litmus_falsifier.hh` 的边界段须同时含**顶点形态**、**判据对象**、**与内存模型 litmus 的区别**三要素 ⇒ 以**关键词共现**机检（`(instance, network, vc)` ∧ `环` ∧ 否定标记），⚠️ 语义是否准确属**人工评审**（⛔ 不写成 checkbox 的判定）
- [ ] **AC-4（逻辑等价 · 零行为变更）**：既有断言段**逐字未变**且全绿（⚠️ **允许文件尾部追加新断言**，追加区与既有区以注释行 `// == disambiguation additions ==` 分隔）；`scripts/test/cep_litmus_falsification_check.sh` rc=0 ⇒ 证明本单元是**纯命名/表述**变更
- [ ] **AC-5（零冻结面 · 机检）**：`git diff --name-only` **不含** `openspec/specs/cep-litmus-falsification/spec.md` · `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` ⇒ 违反即阶段 1 失败（应改走阶段 2）
- [ ] **AC-6（零回归）**：`ctest` 全绿（⛔ 不写死断言数）· 全部 `scripts/test/cep_*.sh` rc=0 · `bash scripts/test/cep_docs_sync_check.sh --strict` rc=0

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-litmus-falsification` | **未改**（阶段 1 零冻结面 · 无 spec delta） | 阶段 1 只加消歧常量与表述闸 |
| —（无新 capability） | — | 阶段 1 的能力由 `include/tlm/cep/litmus_falsifier.hh` 的常量 + `scripts/test/cep_litmus_naming_check.sh` 承载 |

## 影响面

| 面 | 影响 |
|---|---|
| **新增文件** | `scripts/test/cep_litmus_naming_check.sh` ·（阶段 2 才有）`openspec/specs/cep-litmus-falsification/spec.md` 的 REMOVED requirement |
| **修改文件** | `include/tlm/cep/litmus_falsifier.hh`（**追加**常量 + 边界段升级）· `test/test_cep_litmus_falsification.cc`（追加 2 条断言） |
| **零改动（硬查 · AC-5）** | `openspec/specs/cep-litmus-falsification/spec.md` · `scripts/test/cep_litmus_falsification_check.sh` · `test/cep_litmus_corpus.json` · `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` |
| **冻结面** | **无**（阶段 1）⇒ ⛔ 不需要五步链；**阶段 2 有**（spec REMOVED requirement + 改名） |
| **闸** | `scripts/test/cep_litmus_naming_check.sh` 新增 1 个 · ctest 断言 +2（⛔ 数量不是鉴别力指标） |
| **KPI** | **实施期占 1 slot**（`skip_specs: true` 的 change 仍计入 `openspec list --json` active 数）· 归档后释放回 0/3 · **能力 spec 面零变化** |
| **风险** | ① 禁用表述闸**误报**（正常讨论「litmus」被误判）⇒ 缓解：只匹配**肯定式**表述，且要求同段无否定标记 ② 消歧常量被误读为「能力增强」⇒ 缓解：常量名带 `IsMemoryModelLitmus = false`（**否定式命名**，语义自明）③ 阶段 2 被遗忘 ⇒ 缓解：AC-7 强制登记触发条件 |
| **阶段 2 触发条件** | **触发** = ① 有人主张改名，或 ② 操作级模型（`cep-memory-model-operational-trace`）落地需「litmus」名让位。**成本** = 冻结面五步链 ×1（`openspec/specs/cep-litmus-falsification/spec.md` 的 REMOVED requirement）+ 全仓引用改名 |
