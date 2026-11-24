# ADR-CEP-23: 不完整目录知识的表达边界（Completeness 取值域）

> **状态**: ✅ **已签发**（v1.0 · 2026-11-24 —— oracle → critic 双审完成 · 0 BLOCKER · 审稿报告 `.rddf/reviews/adr-cep-23-and-adr13-su-arch-review.md`）
> **日期**: 2026-11-24
> **作者**: CppTLM build agent（architect mode）
> **作用域**: `Completeness` 的**可声明取值域** + 平台对「不完整目录知识」**能表达什么 / 明确不表达什么**
> **父决策**: `ADR-CEP-04` 决策 2（DCOH 完备性契约）+ 决策 3（caps descriptor）
> **关联 change**: `backlog: cep-incomplete-directory-knowledge-boundary`（尚未立项；立项后回填 `openspec/changes/<name>/` 与本 ADR 判据的 AC 编号映射）
> **一手锚点**:
> - HieraGen §VII-D / §VII-E：把「更深层次」「**不完整目录知识**」列为**放宽项**（而「非包容共享缓存」「跨层通信」实为**不支持**）· 调研 **v0.4**（文档版本）§1.3 Q1（该表自身为 **v0.3 修订**口径）
> - 调研 §6 五源矩阵「目录机制」行：`FullMap / 不完整知识 / null dir(Dir1B)`

---

## 1. 背景与问题

**缺口（G7）**：「不完整目录知识」在本平台**无表达** —— 既无 belief 模型，也无一个可声明、可机判的形态。

**现状事实（可逐条复现）**：

| # | 事实 | 出处 |
|:--:|---|---|
| F1 | `Completeness` 枚举有 **3** 值：`EXACT` / `CONSERVATIVE` / `PARTIAL` | `include/tlm/cep/sharer_tracker.hh` |
| F2 | 但实例 schema 的 `completeness` 枚举只有 **2** 值（`EXACT` / `CONSERVATIVE`）⇒ **`PARTIAL` 在合法声明上不可出现** | `include/tlm/cep/protocol_instance.schema.json` |
| F3 | `ordering_point_checker.hh` 的 **J2** 判据有一条 `PARTIAL` + 广播兜底的**合法分支**（其 detail 自陈「决策 2 的最小合法组合」），但该分支在合法声明上**不可达**（判据自陈 vacuous PASS） | `include/tlm/cep/ordering_point_checker.hh` |
| F4 | 结构化 deferral `V-04-7` 登记为 `condition_unmet`，`next_action` = 「`PARTIAL` tracker 实现落地时改判并补断言」 ⇒ 契约面把该取值当作**将来会实现的声明值**保留 | `test/cep_adr_structural_deferrals.json` |
| F5 | **无 belief 模型**：平台不表示「目录**认为**谁持有」与「**实际**谁持有」的差集 ⇒ 即便声明 `PARTIAL`，也表达不出**漏了谁**、信念如何收敛 | 通用层无对应类型（`include/tlm/cep/` 全量） |

**问题陈述**：同一个取值，在契约面同时以「**合法组合**」（J2）与「**待实现**」（F4）两种身份被保留，而声明面（F2）禁止它、语义面（F5）又无法定义它 ⇒ 声明语义**无定义**，且未来任何人「把它打开」都不会被任何判据拦住。这正是「不完整目录知识无表达」的实质。

---

## 2. 决策

### 决策 1：可声明取值域 = {`EXACT`, `CONSERVATIVE`}；`PARTIAL` 降为**不可声明的合成值**

- **可声明**（进 schema enum、可出现在 `protocol_instances.json`）：`EXACT` / `CONSERVATIVE`。
- **不可声明**（⛔ 不得进 schema enum）：`PARTIAL` —— 保留为**合成声明专用值**，仅作 J2 判据与反向 fixture 的输入。
- **`Scope::NONE` + `EXACT` 不是矛盾**：广播型实现（`BroadcastNull` / 声明为 `不参与` 的实例）无目录但**必然触达全体** ⇒ 观察意义下**无遗漏** ⇒ `EXACT` 成立。它表达的是「无知识但无遗漏」，与「精确知识」不同物，但**同属 `INV-CEP-04-IC1` 的合法域。
- 依据：一个**不可机判**的声明值不得留在契约面 —— 平台无 belief 模型（F5）⇒ 无法验证「声明 PARTIAL 者是否真的漏了 sharer」⇒ 该值不承载可验证语义。
- **与父决策的关系（收窄，⛔ 不是取代）**：本决策收窄 `ADR-CEP-04` 决策 2 的**声明面**（该决策允许「`PARTIAL` + 兜底」的组合）；其**行为禁令**（无兜底的 `PARTIAL` = 失效实例）**原样保留**，由决策 3 与 V-23-4 继续机判 ⇒ 本条**不取代** `ADR-CEP-04` 决策 2。

### 决策 2：本仓**声明面不承载** belief 模型（范围限定：本仓）

**范围声明（先于理由）**：本决策的排除范围 = **本仓的声明面与判据面**，⛔ **不构成**对 ArchForge 域内决策的预否决 —— 若 ArchForge 提出 belief 判据并要求本仓承载，走**取代流程**修正本 ADR（见 §4 风险 R1）。

理由三条，缺一不可：

1. **判据不可得（限本仓）**：belief 模型的可验证性要求「信念 → 实际」的收敛判据（谁在何时必然发现漏了谁），本仓无此判据、亦无计划提供 ⇒ 在本仓建了也只能是装饰。
2. **判权域不在本仓**：协议 FSM / 瞬态态 / 目录知识演进属**协议语义设计**，权威归 ArchForge（`ADR-CEP-00` 决策 1）；本仓只承载**声明面与判据** ⇒ 本仓**无立法权**定义信念语义。
3. **本仓路线一致性（限本仓）**：`ADR-CEP-18` 决策 1/6 已排除生成器路线，而 belief 模型正是**生成器**的前置件（需要状态空间里的信念维）⇒ 在本仓建它等于绕道复活本仓已排除的路线。**本条只证明「本仓不建」的路线一致性，⛔ 不证明「belief 被全域否决」。**

### 决策 3：「不完整目录知识」在**行为语义层面**的**唯一合法可实现形态** = `CONSERVATIVE` + `broadcast_fallback`

- `CONSERVATIVE` = **过近似**（**绝不漏** sharer，可能多报）；配合广播兜底后，最坏情况退化为「多 snoop 几个节点」而不是「漏掉一个 sharer」。
- ⛔ **禁止**以「漏 sharer 且不广播」表达不完整知识 —— 那是**失效实例**（`ADR-CEP-04` 决策 2 已定），不是「另一种知识不完全」。
- 现状两行 `CONSERVATIVE` 实现（`SparseSnoopFilter` / `DcohSymmetric`）均满足该形态 ⇒ **本决策对现有实例零影响**。
- **已复核的边界（防误读为「静默排除合法硬件」）**：带**区间** / **租约** / **有限指针**的稀疏目录在本平台**均可**被 `CONSERVATIVE` 建模 —— 区间目录（sharer ∈ [i,j]）的区间即**保守超集**（绝不漏、可能多报）；租约目录（过期必重登记）的当前集是保守集；有限指针目录的 overflow 广播（DirNB）= 保守集 + 兜底；否决新缓存类（DirNF）以**服务拒绝**换精确性，**不属**「知识不完整」。⇒ 本报废的只是**自述维度**（声明者自称漏了谁），⛔ 不是行为建模能力。

### 决策 4：偷渡禁止 —— 未来若要真 belief，必须新 ADR + 新 caps 维度

若将来确需表达「**有界漏 + 补集不可枚举 + 需声明该界**」（⚠️ 注意：「有界漏 + 补集**可**枚举」合并后仍是 `CONSERVATIVE`，见决策 3 的边界段 ⇒ **不在**本决策适用范围），必须：

- 新开 ADR 定义**新 caps 维度**（如 `KnowledgeModel`），语义与判据一并定义；
- ⛔ **不得**复用 `PARTIAL`，⛔ **不得**把 `PARTIAL` 塞回 schema enum 来「先跑起来」。

### 决策 5：J2 的 `PARTIAL` 分支**定位变更**（保留判别力，撤销「待实现」身份）

- J2 的 `PARTIAL` + 兜底分支**保留**：它仍是合成声明上的**反向判据**（`PARTIAL` 且无兜底 ⇒ 必红，毒化可验）。
- 但它**不再是**「等待某实例实现的合法组合」⇒ `V-04-7` 的 `next_action` 须由「等待实现」改为「按本 ADR 收敛」（见 §6 与 §7）。

---

## 3. 选项对比

| 选项 | 结论 | 理由 |
|---|:--:|---|
| **A. 现状钉死**（本 ADR） | ✅ **选中** | 零 schema 变更；消除「合法但不可达」的分支身份冲突；把「不完整知识」收敛到 `CONSERVATIVE` + 兜底**一个**可机判形态；与 ADR-CEP-04 决策 2 同源 |
| B. 扩 schema 允许 `PARTIAL`（+ 强制兜底） | ❌ | 引入的声明值在**行为上等价于** `CONSERVATIVE` + 兜底（都靠广播兜住），区别仅在「声明者自述是否真的漏了」；而平台无 belief 模型 ⇒ 该区别**不可机判** ⇒ 契约面净增一个不可验证状态，且给「假精确」开口 |
| C. 删除 `PARTIAL` 枚举值 | ❌ | 是**公共契约面的破坏性变更**（枚举值序 / `channel_dependency_graph.hh` 的合成分支 / 测试映射表均引用）；收益与 A 完全相同 ⇒ 纯风险 |
| D. 建 belief 模型 | ❌ | 见决策 2 三条理由（判据不可得 / 越判权域 / 绕道被排除路线）；需要时走决策 4 |

---

## 4. 后果 / 代价

**正面**

- 声明面与判据面**口径一致**：schema 的 2 值即全部可声明语义，不再有「枚举里有、schema 里没有、判据里等着」的三方错位。
- 「不完整目录知识」有了**唯一可判定形态**（`CONSERVATIVE` + 兜底），后续任何「补齐目录知识」的改动都能被 J2 + caps 断言检出，而不是以注释漂移的形式存在。
- 关掉一个**未来静默打开**的缺口：即便有人把 `PARTIAL` 塞回 schema，本 ADR 的判据（§6 V-23-1 / V-23-2）也会红。

**负面 / 代价**

- **表达力**：平台从此**无法**表达「我有意漏掉 sharer 但保证有界」的实现 —— 这类实现在本平台只能被建模成 `CONSERVATIVE`（多报）或被排除。这是**主动放弃**的表达力，换来的是可判据性。
- `PARTIAL` 成为**半死值**（枚举保留、声明禁止）：阅读者需要 ADR 才知道它为何存在 ⇒ 靠本文与 F3/F4 指针解释，无法靠代码自明。

**风险**

- **R1 · 判权越界风险**：若将来 ArchForge 给出「belief 模型的判据」并判定本仓必须承载，则本 ADR 决策 2 需被取代（走决策 4 的新 ADR + 取代流程）。
- **R2 · 反向 fixture 退化风险**：J2 的 `PARTIAL` 分支若被后人当作「死代码」删除，判别力会静默消失 ⇒ 由 §6 V-23-4 的合成声明断言兜住（该断言存在即证明分支在用）。

---

## 5. 与上游不变量 / 判据的关系（本 ADR **不新造** invariant 编号）

> **口径**：本 ADR 收窄的是 **`ADR-CEP-04` 决策 2** 的**声明面**，其约束**已有** owner 编号 ⇒ ⛔ 本 ADR **不**新造 `INV-CEP-23-*`（同一约束挂两套编号 = 第二套判据标准）。

| 本 ADR 的约束 | 复用的上游编号 | 关系 |
|---|---|---|
| 合法声明（`protocol_instances.json` 内实例）的 `caps.completeness` **∈** {`EXACT`, `CONSERVATIVE`}；`PARTIAL` 只可由**合成声明**产生 | `INV-CEP-04-IC1`（caps 字段值必须 ∈ enum 字面值） | 本 ADR 把「enum 字面值」的**实例 schema 侧**钉死为 2 值 ⇒ 该 enum 的取值域由本 ADR 定义 |
| `completeness == CONSERVATIVE` ⇒ `broadcast_fallback == true`（「过近似」必须伴随兜底；反对偶不成立 —— `EXACT` 也可有兜底，如 `BroadcastNull`） | `V-04-7`（`ADR-CEP-04` 的 DCOH 完备性契约判据） | ⚠️ 该契约上游只有 **V 级判据**、无独立 INV 编号 ⇒ 复用 `V-04-7`，⛔ 不新造 |
| 通用层不含 belief / knowledge 维度的类型或字段（决策 2 的机器可读形态） | **无上游编号** | ✅ 显式登记为**守门判据**（⛔ 非 invariant，见 §6 V-23-5 的诚实边界） |

---

## 6. Verification（前瞻判据 · 可勾选 —— **不是**已跑完的实测日志）

> **口径**：每条给**载体** + **反向 fixture（毒化 ⇒ 必红）** + **归因 id**；归因 id 一律**复用 owner 编号**（`INV-CEP-04-IC1` / `V-04-7`）—— **本 ADR 零新造 id**（per §5）。

| # | 判据 | 载体 | 反向 fixture（毒化 ⇒ 必红） |
|---|---|---|---|
| **V-23-1** | 实例 schema 的 `completeness` 枚举 **==** {`EXACT`, `CONSERVATIVE`}（`INV-CEP-04-IC1` 的声明面） | `include/tlm/cep/protocol_instance.schema.json` + 装配闸（实例类型/值级校验） | 把 `PARTIAL` 加入枚举 ⇒ 装配闸必红 · 归因 `INV-CEP-04-IC1` |
| **V-23-2** | `kTrackerTraits` **全行**（当前 6 行）均**不**声明 `PARTIAL`（`INV-CEP-04-IC1` 的实现面） | `test/test_cep_sharer_tracker_base.cc`（**全行迭代**断言 `caps.completeness != PARTIAL` + `static_assert(size == 6)` **双层** ⇒ 未来新增第 7 实现时判据不漏检）· `test/test_cep_authority_migration.cc` | 把任一行改成 `PARTIAL` ⇒ 必红 · 归因 `INV-CEP-04-IC1` |
| **V-23-3** | `CONSERVATIVE` 的实现**必须**带 `broadcast_fallback`（`V-04-7`） | `sharer_tracker.hh` 的 `kTrackerTraits` + `test/test_cep_sharer_tracker_base.cc` | 构造 `CONSERVATIVE` + `broadcast_fallback=false` ⇒ 必红 · 归因 `V-04-7`（owner `ADR-CEP-04` · V 级判据） |
| **V-23-4** | J2 在**合法声明**上 vacuous PASS（并显式登记该事实），在**合成声明** `PARTIAL` 无兜底时必红 | `test/test_cep_ordering_point_checker.cc`（两条断言并存）· `include/tlm/cep/ordering_point_checker.hh` | 删掉合成声明的红断言 ⇒ 判别力静默消失 ⇒ 必红 · 归因 `V-04-7`（owner `ADR-CEP-04` · ⚠️ **V 级判据**，非 invariant：红分支的语义依据就是 DCOH 完备性契约「无兜底 PARTIAL = 失效实例」；J2 是本平台对该判据的**机器载体**）|
| **V-23-5** | 通用层**未引入** belief 模型符号（决策 2 的机器可读形态 · **守门判据，无 invariant 归因**）| `include/tlm/cep/*.hh` —— grep 模式为**字面清单** `BeliefModel` / `knowledge_belief` / `belief_`；载体落地时须**逐字**写入闸脚本（⛔ 不得以「一类类型」等开放集表述）| 注入任一字面模式 ⇒ 必红 · 归因 **无**（守门判据；⛔ 不挂 invariant 编号） |

> **诚实边界**：V-23-5（**守门判据**，⛔ 非 invariant）只能证明「**未**引入**字面清单内**的符号」，⛔ 不能证明「未来不会以别名引入」—— 它是**守门**而不是**证明**。同理，V-23-1 ~ V-23-3 证明的是「声明面与实现面口径一致」，⛔ 不证明「本平台足以表达真实硬件的全部目录知识形态」。

---

## 7. 关联 ADR / 后续

| 关系 | 对象 |
|---|---|
| **父**（本 ADR 收窄其取值域） | `ADR-CEP-04` 决策 2（DCOH 完备性契约）/ 决策 3（caps descriptor） |
| 同日关联（同一批次语义裁决） | `ADR-CEP-13` —— 其 Status Update 登记一致性标签 → fence 效果的派生规则（G4-c）与「不增设 `fence` 子字段」（G4-b） |
| 排除路线依据 | `ADR-CEP-18` 决策 1/6（排除生成器） |
| 同域相邻 | `ADR-CEP-21`（sharer 位宽扩容 · G9 触发式）· `ADR-CEP-12`（能力映射表） |

**后续（非本 ADR，需独立执行者）**：

1. **builder**：把 `test/cep_adr_structural_deferrals.json` 的 `V-04-7` `next_action` 由「等待实现」改为「按 ADR-CEP-23 决策 5 收敛」（改判登记，非删除条目）。
2. **builder**：① `scripts/test/cep_docs_sync_check.sh` 的 `NO_INV` 白名单加本 ADR（⛔ **不进** `OWNERS` —— 本 ADR **零新造** invariant，见 §5）② 冻结清单五步链（本 ADR 是**新增冻结产物**，`ADR-CEP-13` 同时 hash 变更 ⇒ 同一批）。
3. **planner**：若要把本 ADR 落地为可执行 change（主要是 ⑤ 的判据载体化），新建 `openspec/changes/` 并回填本 ADR 的 AC 映射（当前暂为 `backlog:` 占位）。

---

## 8. 修订记录

> **变更史**：见 `.rddf/roadmap/objectives/obj-cep-platform.md` 台账（授权记录单一真源）。
