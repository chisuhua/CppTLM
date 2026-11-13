# 档 2 的两个前置裁决(U-3 权限判据充分性 · 版本语义口径)

> **性质**:architect **裁决**(⛔ 不是 ADR —— 未改变任何已签批决策;两条裁决均在 `ADR-CEP-20` 决策 5 的「登记路由」范围内)
> **授权**:台账 **Entry #403** ⑥ / A→B→C 的 **C′**;前置调查 = `.rddf/reviews/cep-evidence-tier2-scale-assessment.md` 的 **U-1 / U-2 / U-3**
> **结论先行**:
> - **U-3 = 「部分可判」**:权限违例 ✅ 可判(`perm` 两侧都有);**标签违例 ⛔ 不可判**(流上无运行时标签)⇒ **档 2 最小切片只做 `sharer` + `data-value` 两维**(均已在 `ADR-CEP-06` IC5 语义内),**⛔ 不含权限/标签维**;
> - **版本口径 = 引擎内部写序**(观测维度)⇒ ⛔ **不引入协议级版本字段** ⇒ ⛔ **不需 ArchForge 设计权威**(不改协议语义设计)⇒ **档 2 可在本仓立项**。

---

## 0 · 为什么这两条必须先裁

`ADR-CEP-20` 决策 4 把「让拓扑级承载一致性语义」判为**不采纳**,其主理由 ② 是「协议语义的**设计**权威归 ArchForge 单向门」⇒ ⇒ 档 2 若**碰到新协议语义设计**,必须先取那份权威;若只是**观测既有实现**,本仓即可做。**这两条裁决决定档 2 落在哪一侧。**

---

## 1 · U-3:权限判据的充分性 —— 裁决 = **部分可判**

### 1.1 实测(逐 `file:line`)

| # | 事实 | 落点 |
|:--:|---|---|
| 1 | **期望侧已齐**:`perm` 4 值锁(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`,每实例声明**子集**)+ `consistency_labels`(read ∈ {SC, TSO, RC-acquire, RC-data, PLO} / write ∈ {SC, TSO, RC-release, RC-data, PLO}) | `include/tlm/cep/protocol_instance.schema.json` 的 `perm` / `consistency_labels`;`ADR-CEP-12` 决策 2 / 3 / 4 |
| 2 | ✅ **实际侧已齐(perm)**:`CohIfBundle` **携带 `perm`**(`ch_uint<8> perm;`)与 `atomic`(与 `perm` 正交) | `include/bundles/cep_coherence_bundles_tlm.hh:102` / `:114`(字段表见 `:26-27`) |
| 3 | ✅ **三处在流上确实写 `perm`** | `chi_rnf_protocol_engine.hh:373` · `mesif_protocol_engine.hh:362/394/632` · `cohif_responder.hh:165` |
| 4 | ⚠️ **实际侧缺(标签)**:`consistency_labels` **只有实例声明**(静态能力面);**事务流上无运行时标签字段**(`bundle` 无该字段) | 同 2 的字段表 |
| 5 | 引擎已有 `perm` **值域锁**断言(非「期望集 vs 实际」判定) | `mesif_protocol_engine.hh:134/136`(「perm 值域漂移」) |

### 1.2 裁决

| 维度 | 可判? | 依据 | 处置 |
|---|:--:|---|---|
| **权限违例**(实际 `perm` ∉ 该实例声明的 `perm` 子集) | ✅ **可判** | 事实 1(期望)+ 事实 2/3(实际) | ⚠️ **但不在档 2 最小切片内** —— 它会**新增一个违例类** ⇒ **需新 invariant** ⇒ 与本仓「新增 invariant 数 = 0」纪律冲突 ⇒ **另立 ADR** |
| **标签违例**(声明 SC 却发 RC-data 序) | ⛔ **不可判** | 事实 4(流上无运行时标签) | 要判须**在流上新增标签字段** ⇒ 属**新协议语义设计** ⇒ `ADR-CEP-20` 决策 4 主理由 ② ⇒ **须 ArchForge 设计权威** ⇒ ⛔ 不在本仓单方面做 |
| **sharer 违例**(SWMR) | ✅ **可判** | `sharer_tracker.hh` + 5 实现;**mesif / moesi_gpu 已注入** | ✅ **档 2 最小切片的主判别力** |
| **data-value 违例** | ✅ 可判(**但需版本**) | 见 §2 | ✅ 档 2 的第二维(版本取内部写序) |

⇒ ⇒ **裁决:档 2 最小切片 = `sharer`(SWMR)+ `data-value` 两维**;⛔ **不含权限维**(需新 invariant / 另立 ADR)· ⛔ **不含标签维**(需 ArchForge 权威)。

---

## 2 · 版本语义口径 —— 裁决 = **引擎内部写序(观测维度)**

### 2.1 三选项

| 选项 | 内容 | 结论 | 理由 |
|---|---|---|---|
| **A 引擎内部写序** | 引擎对每个地址维护**单调写计数**(仅 run 内),读时带回该计数 | ✅ **采纳** | ⛔ 不改 bundle / ⛔ 不改协议语义设计 ⇒ **不需 ArchForge 授权**;版本作为**观测维度**存在,足以判 data-value 的**内部一致性**;⚠️ 须逐字登记「**观测侧代理版本,⛔ 非协议语义版本**」 |
| B 协议级版本字段 | `CacheReq/RespBundle` 或 `CohIfBundle` 增 version 字段 | ⛔ **不采纳** | ① 改 **bundle 语义** ⇒ 波及全部消费方(实测 `CohIfBundle` 有 3 处写入点 + 23 处 flit 类别消费文件);② 属**新协议语义设计** ⇒ `ADR-CEP-20` 决策 4 主理由 ② ⇒ **须 ArchForge 单向门**;③ 收益有限(见 2.2) |
| C 测试侧代理版本 | 在 `test/` 里自造 | ⛔ **不采纳** | `ADR-CEP-20` 决策 3a 已判:**测试侧代理版本不产出装置/引擎级语义**(它在**拓扑级**是概念错误;在**协议级**同样不构成协议语义) |

### 2.2 为什么 A 的「代理版本」在这里**不是**决策 3a 所否定的那种

| 维度 | 决策 3a 否定的情形(拓扑级) | 本裁决的情形(协议级) |
|---|---|---|
| 版本从哪来 | **测试侧**自造 | **引擎内部**状态(实现侧) |
| 判别力来自哪 | ⛔ 仅版本 ⇒ 无其他真实维度 | ✅ **`sharer`(真实可见性)为主**,版本为辅 |
| 结论 | **概念错误**(该档位没有真实语义维度) | ✅ **可接受** —— 档 2 的判别力**不依赖**版本:即便版本退化,sharer 侧仍能判 SWMR |

⇒ ⇒ **裁决:采纳 A**;⚠️ 版本维须逐字标注「观测侧代理」,且**⛔ 不得**把「data-value 零违例」单独当作协议语义结论(须与 sharer 侧合并陈述)。

---

## 3 · 对建档 2 的影响(供 planner 立项)

| 项 | 结论 |
|---|---|
| **档 2 是否可立项** | ✅ **可**(最小切片 = `mesif` 一族 · `sharer` + `data-value` 两维) |
| **是否需 ArchForge 授权** | ⛔ **不需要**(不引入新协议语义设计) |
| **是否新增 invariant** | ⛔ **不新增**(SWMR 与 data-value 均属 `ADR-CEP-06` **IC5** 既有语义) |
| **估时修正** | 评估报告 §3 的 S3 取 **A ⇒ 1 人日**(非 3-5)⇒ 最小切片 **≈ 7-9 人日** |
| **⛔ 明确排除** | 权限维(需新 invariant ⇒ 另立 ADR)· 标签维(需运行时标签 ⇒ ArchForge)· 全族覆盖(逐族裁) |
| **前置** | U-1(`mesif` 事务边界是否足以承载 per-txn 观测)· U-2(`SharerTrackerIf` 是否**可查询集合**而非只接受/拒绝)⇒ ⇒ **仍须在读码后确认**(本裁决**不能**替代它们) |

---

## 4 · 残余(⛔ 不可约)

1. **U-1 / U-2 仍是未知** —— 本裁决只裁「做哪两维 / 版本从哪来」,**不**证明实现可行 ⇒ ⛔ 不得据本裁决宣称「档 2 已可实施」;
2. **版本是观测侧代理** ⇒ data-value 的结论强度**低于** sharer 侧;
3. **权限/标签两维被明确排除** ⇒ 档 2 的判别力**不等于**「协议语义全覆盖」;
4. 本裁决**未改变**任何已签批 ADR —— 若后续要把权限维纳入(需新 invariant)⇒ **必须**走 ADR(取代/新增)。

**关联**:台账 **Entry #403**(授权)· `.rddf/reviews/cep-evidence-tier2-scale-assessment.md`(U-1~U-3 的提出处)· `ADR-CEP-20` 决策 3a / 4 / 5 · `ADR-CEP-12` 决策 2/3/4 · `ADR-CEP-06` IC5 · `include/bundles/cep_coherence_bundles_tlm.hh:102/114` · `include/tlm/cep/sharer_tracker.hh`

---

## C′-bis · 追加裁决(2026-11-13 · per 台账 Entry #408)—— metis 第 2 轮(正式审查)引出的 **3 个结构性死锁**与 **R1~R8**

> **性质**:architect **追加裁决**(⛔ **不改**任何已签批 ADR · ⛔ **未新增** invariant · ⛔ 本段**细化/修正** C′ 段的两条裁决(版本口径 · 检测器归属)⇒ 按 `ADR-CEP-20` 决策 5 的登记路由处理).
> **触发**:planner 阶段的 **metis 第 2 轮(正式审查)** 在 improvement 草稿 `.rddf/improvements/cep-evidence-tier2-minimal-slice.md` 上挖出 3 个**结构��死锁** —— 即「按草稿原样实施 ⇒ 必然在实施期返工或产出空洞」.
> **本段裁决对象**:C′ 段的 **U-3(不变)** 与 **版本口径(被 R1 修正)** + 草稿的 **Q1/Q2/Q3/Q4/Q5**.

### 0 · 三个死锁(先摆事实,再裁决)

| # | 死锁 | 一手事实链 |
|:--:|---|---|
| **DL-1** | **预登记 ↔ 4 件套 ↔ 冻结面** 三方互斥 | `tools/render_report.py:178-184` **无条件**要求 run 的 `hypothesis_id` 在 `experimental/preregistration.json` 有登记行且非 `pending`;`scripts/test/cep_experiment_run.sh:156,177` 用 `render_report.py --check` 做 4 件套校验 ⇒ **无登记行 ⇒ 4 件套产不出来**;而 `experimental/preregistration.json` **在冻结面**(manifest `:38`)⇒ 改它须 `--update` 授权 + 改真源 `objective §9.1 ②`(`cep_prereg_drift_check.sh:172-174` 要求两侧同 PR 同改)⇒ **草稿的 AC-7「`preregistration.json` 零 diff」与 AC-5「产 4 件套」直接互斥** |
| **DL-2** | **`deadlock_violations` ↔ `ic3_verdict` ↔ H3 预登记** 三方互斥 | `test/cep_experiment_harness.hh:551-555`:`ic3_verdict()` 在**任一违例类 `not-collected`** ⇒ 直接返 `⚪ 不适用`(⛔ 不记「绿」);引擎**明确不建模在飞 snoop 集合**(`mesif_protocol_engine.hh:469` 注释)⇒ 引擎面**无死锁形态** ⇒ 只能 `not-collected` ⇒ **verdict 必 ⚪** ⇒ 「档 2 通过态」**不可得**;若改写实测 0 ⇒ **人造 0**(违反 H3 预登记「`deadlock_violations` 移出 out-of-scope ⇒ 必须实测」的收紧方向 + `ADR-CEP-06` 的「⛔ 不得写裸 0 而不声明」) |
| **DL-3** | **观测点 ↔ 行数据写入点** | `mesif_protocol_engine.hh:325-346` 的 `apply_event` 只改 `lines_[addr].state`、**不改 data**;data 的**唯一**写入点是 `on_completion` 的 `:442` ⇒ 在 `apply_event` 入口采到的 `data_after` **恒为旧值** ⇒ 草稿 What Changes #1/#2 的 `data_after` **结构性错误**;且 WRITEBACK 完成时 `apply_event` 被调 **2 次**(`:439` + `:441`)**都早于** `:442` |

### 1 · R1 —— data-value 判据路径 + **版本口径**(修正 C′ 段)

- **C′ 段的裁决**(版本 = 引擎内部写序)**被本段修正**:metis 实测指出 —— per-engine 私有写序在「**自读自写**」下是**恒等式** ⇒ data-value **恒 0** ⇒ **不构成判据**;且「最新写值」的持有者问题**并未**因「用 data 值代替版本号」而消失(引擎侧各记各的 ⇒ 跨引擎不可比)。
- ⇒ **R1 裁决**:档 2 最小切片的 data-value 维 **采纳「注入陈旧副本」形态** ⇒ **仅在负控下可达** ⇒ ⛔ **生产流恒 0 登记为「本装置无该形态」**(与档 0 的诚实边界**同型**,⛔ 不得称「检测力已证」);**版本维整体不进本切片** ⇒ C′ 段的「引擎内部写序」裁决 **降级为「不实施」**(⛔ 不是被推翻,是**证伪其判别力** ⇒ 不再需要 architect 追加「共享写序」口径 ⇒ 草稿的 Q2 随之**消解**)。
- ⇒ 观测面**仍记** `data_before` / `data_after`(观测事实,供负控断言)⛔ 但**不作判据**。
- ⇒ **归因声明(逐字)**:data-value 负控的归因对象 = **注入的陈旧副本构造**,⛔ **不是**「引擎未保持一致性」。

### 2 · R2 —— 检测器归属(修正 C′ 段的实现硬约束)

- **R2 裁决**:采纳**仓内既有先例**(`include/tlm/cep/stub_cache_endpoint.hh:68-83`「原始观测事实(⛔ 不做聚合 —— 判定归 `test/` 的 harness)」)⇒ **实现侧只出原始事实,判定留在 `test/` 侧 harness**。
- ⇒ ⛔ **撤回** 草稿的硬约束「检测器不在 test 侧」;⛔ **不动** `test/cep_experiment_harness.hh`(⇒ 消除 metis 指出的「薄委托不可机械实现」+「归因问题下沉进实现」+「改存量语义却无差异化判据」三项风险);⇒ AC-10 由「档 1 零削弱」简化为「**档 1 既有负控逐字绿(零改动)**」。

### 3 · R3 —— **档 2 的载体形态**(本轮最重裁决;同时消解 DL-1 与 DL-2)

**四条「产 4 件套 run」的路逐条走死**:

| 路 | 断点 |
|:--:|---|
| **a. 复用 `hypothesis_id = h3`** | H3 的 `out_of_scope` = `['coherence_stalls','collective_stalls','directory_misses']` ⇒ `deadlock_violations` **必须实测** ⇒ 引擎面无死锁形态 ⇒ 只��写**人造 0** ⇒ 违反 H3 的收紧方向 + `ADR-CEP-06`「⛔ 不得写裸 0 而不声明」 ⇒ **断** |
| **b. 新增假设行(H9)** | 改 `experimental/preregistration.json`(冻结面)+ 真源 `objective §9.1 ②` ⇒ **B7 分母 8 → 9**,而 H9 的 verdict 因 DL-2 **必为 ⚪**(⚪ ≠ 「有实验结论」)⇒ ⇒ **DoD 从 8/8 退到 8/9** ⇒ **断**(代价:终态 DoD 退步) |
| **c. 产 run 且 `deadlock_violations = not-collected`** | DL-2 ⇒ verdict 必 ⚪ ⇒ 「档 2 通过态」不可得 ⇒ **产了 run 也没有「通过态」** ⇒ **断** |
| **d. 产 run 且写实测 0** | = 人造 0 ⇒ 同 a ⇒ **断** |

⇒ ⇒ **R3 裁决:档 2 的载体 = 「协议级观测面 + 契约级断言」,⛔ 不产 4 件套 run。**

**四条随之落定的后果(逐条)**:

| # | 后果 |
|:--:|---|
| **C-1** | ⛔ **不改** `experimental/preregistration.json` · `experimental/metrics.schema.json` · `tools/render_report.py`(三者皆冻结面)⇒ **零冻结面变更** ⇒ **B7 分母不变** ⇒ **DoD 不退步** |
| **C-2** | ⛔ **不产 run** ⇒ `hypothesis_id` / `ic3_verdict` / `deadlock_violations` / `not-collected` **四个议题一并消解** ⇒ DL-1 / DL-2 **闭合** |
| **C-3** | ⚠️ **档 2 与档 0 / 档 1 的载体形态不同**(前两者是 run,档 2 是「观测面 + 契约级断言」)⇒ **档位阶梯表须显式登记「档 2 = 非 run 载体」** ⇒ ⛔ 不得让读者以为档 2 有 run / 有「通过态」;⇒ 档 2 的登记面 = **新表**(如 `test/cep_evidence_tiers.json`)⇒ ⛔ **不进** `test/cep_e2e_run_tiers.json`(该表**只登记 run**,且**不在**冻结面) |
| **C-4** | ⚠️ **阶梯的「强度」列须按实际收敛**:档 2 当前 = 「协议语义(天花板档 · **仅 sharer 可见性维**)」⇒ ⛔ **不得**宣称「档 2 强度 > 档 1」(档 1 = 模型级三负控 + 耦合探针)⇒ ⛔ 不得称「档 2 通过态」;⇒ 阶梯表的排序语义 = 「**档位 = 装置归属层级**,⛔ **不是** 强度全序」 |

### 4 · R4 —— 观测点必须覆盖**两个点**(消解 DL-3)

- **R4 裁决**:观测面 = **两处**落点,记录带 `kind` 枚举 —— ① `kind = transition`:`apply_event` 入口(状态迁移;`sharers_snapshot` = **迁移前**视图,`view_of` 口径)② `kind = fill`:`on_completion` 的 `lines_[addr].data = data` **之后**(`mesif_protocol_engine.hh:442`)。
- ⇒ ⛔ 不改协议语义 · ⛔ 不改 bundle · ⛔ 不动 `sharer_tracker.hh` ⇒ 「`data_after` 恒旧」的结构性错误**消解**。

### 5 · R5 —— 观测面的字段表(定死,供 implementation 直接引用)

`{ kind, addr, event, from, to, origin, data_before, data_after, sharers_snapshot, self_mask, self_mask_valid, level, node }` —— ⛔ **无 version 字段**(R1)· `sharers_snapshot` / `self_mask` 用 `std::uint64_t`(**⛔ 不用 `responder_set_t`** ⇒ 基类**不新增**对 `sharer_tracker.hh` 的依赖,保 M2 的 seam 面窄)· 结构名 **`EventObservation`**(⛔ 不用 `TxnObservation` —— 它是 per-event 不是 per-txn)。

### 6 · R6 —— `observation_invalid` 的载体(修正 metis 的 P1-15)

- **R6 裁决**:因不产 run ⇒ `metrics.json` 顶层键**不适用** ⇒ 改为**观测面自身的状态位**(`EventObservation` 流的一个计数 + 断言)⇒ ⛔ **不进** `memory_violation_kinds`(`metrics.schema.json:196` `additionalProperties: false` + 冻结)。

### 7 · R7 —— 跨档聚合隔离(作废 metis 的 P1-11 T7 提案)

- 因 R3(不产 run)⇒ 无跨档 `hypothesis_id` 复用 ⇒ **T7 提案作废** ⇒ C8 已落地的 `evidence_scope` + tiers 表**不受影响** ⇒ ⛔ 不改 `test/cep_e2e_run_tiers.json`(C8 产物,本切片零改动)。

### 8 · R8 —— 范围与估时(收敛)

| 项 | 草稿 v1 | **裁决后** | 说明 |
|---|---|---|---|
| 判据维 | sharer + data-value + **版本** | **sharer(可判)+ data-value(仅负控可达)** | R1 去版本维 |
| 载体形态 | 4 件套 run | **观测面 + 契约级断言** | R3 |
| 检测器 | 搬进 `include/` | **留 `test/` 侧** | R2 |
| 冻结面变更 | 84 → 85(两次 `--update`) | **仅 `07 号` 新 SU + 新 main spec**(⇒ 84 → 85,**一次** `--update`) | C-1 |
| 新增闸判据 | T4/T5/T6/T7 | **T4/T5(档 2 登记面)+ 断线三连判据** | R3 / R7 |
| 估时 | 7-9 人日 | **≈ 5-7 人日** | 消除 4 件套 / runner / 预登记 / 检测器搬迁四项 |

### 9 · 本段的边界声明

1. ⛔ 本段**不改**任何已签批 ADR(全部落在 `.rddf/reviews/` + 台账);
2. ⛔ **档 2 不产 run** ⇒ ⛔ 不得宣称「档 2 有通过态」· ⛔ 不得宣称「档 2 强度 > 档 1」· ⛔ 不得把档 2 的 0 与档 0/档 1 的 0 并列引用;
3. ⚠️ **C′ 段的「版本口径 = 引擎内部写序」被 R1 降级为「不实施」** —— 理由是**证伪其判别力**(恒等式 ⇒ 恒 0),⛔ **不是**推翻该裁决的方向(方向仍是「不引入协议级版本字段」);
4. ⚠️ U-3(权限维可判但需另立 ADR / 标签维须 ArchForge)**维持不变**;
5. ⚠️ **本段不证明**档 2 可实施 —— 实施期的非空洞判据(AC-1)若不成立 ⇒ ⛔ 停手(不立项优于立项)。

**关联**:台账 **Entry #408**(本段)· **#407**(architect 梳理)· **#406**(C′ 段)· **#403**(档 2 评估)· `.rddf/improvements/cep-evidence-tier2-minimal-slice.md`(草稿 v1 · 待按本段改 v2)· `.rddf/reviews/cep-evidence-tier2-scale-assessment.md` · `ADR-CEP-20` 决策 3a/4/5/6 · `ADR-CEP-06` SU#4 ② · `test/cep_experiment_harness.hh:551-555` · `tools/render_report.py:161-185` · `include/tlm/cep/stub_cache_endpoint.hh:68-83` · `include/tlm/cep/mesif_protocol_engine.hh:325-346,442,469` · `experimental/preregistration.json`(H3 行)

---

## C′-bis-ter · 勘误段(2026-11-13 · per oracle 复核 · 台账 Entry #408)—— R3 的**理由订正**与 7 项补正

> **性质**:architect **勘误**(⛔ 不改 C′-bis 的任何**结论** · ⛔ 不改任何已签批 ADR · ⛔ 不新增 invariant).
> **触发**:oracle 对 C′-bis 段的复核 = **APPROVE(附 1 项强制修正 + 4 项澄清)**.

### 1 · 强制修正 —— R3 的**理由**订正(结论不变)

**勘误**:C′-bis 的 **DL-2** 把「**引擎级**死锁能力」与「**run 级**死锁测量」混为一谈 ⇒ **事实有误**:

| 事实 | 出处 |
|---|---|
| **ProgressWatchdog 在 harness / 模型层做死锁判定**（「连续 N cycle 无 retire 且在途非空」),且有**合格的非空洞负控**(`deadlock_detector_live` —— 错标负控真的检出死锁) | `test/cep_consistency_completion_model.hh:111-116,175-177,328,356` · 负控 `:181-186` |
| **H3 的 2026-11-05 收紧登记明写**:「装置升级为『一致性 × 完成域』复合装置(v2, device=v2)后**实测**该 metric」 | `experimental/preregistration.json:63-71` |

⇒ ⇒ **「第五条路」存在**:把 `mesif` 观测面 + 检测器挂进既有 `ConsistencyCompletionModel` 复合装置 ⇒ 复用 `hypothesis_id = h3`(H3 的 `out_of_scope` 恰为 `coherence_stalls`/`collective_stalls`/`directory_misses` ⇒ 三项 `not-collected` ⊆ 预登记,`tools/render_report.py:187-193` 越界检查通过;两项违例类**均实测**)⇒ `ic3_verdict` **可得 ✓** ⇒ run 在档位表登记为档 2 ⇒ **零冻结面变更 + B7 分母不变 + 无造假 0**。

⇒ **R3 的结论(档 2 ⛔ 不产 4 件套 run)维持不变**,但**理由必须改述**为 ——

> **第五条路不是不存在,而是「定价后排除」**:① 复活 4 件套机器面(`scripts/test/cep_experiment_run.sh:155-177` 全链)≈ 草稿原估 **7-9 人日**(vs 裁决后 5-7);② 复用 `hypothesis_id = h3` 须**显式回驳**草稿自设的 AC-6 / metis P0-8;③ **watchdog 死锁是「装置活性」判据**,对档 2 声称的「**协议语义**」几乎**零增量** ⇒ **花 run 的价钱买不到 run 级判别力**。

⚠️ **改述的理由**(防止将来重开本裁决):若不写,任何读者看到 `mesif_protocol_engine.hh:469` 的「不建模在飞 snoop 集合」注释与 `consistency_completion_model.hh:175` 的 watchdog 都会**重开**「档 2 能否产 run」⇒ 正是 `ADR-CEP-20` 决策 6 想杜绝的「反复重开」.

### 2 · M1(引擎本地非法写)处置 —— **不进本切片**

metis P0-1 曾把 M1 降为「检测力见证」,但草稿 AC-3 ⛔ 禁「直接调 `apply_event` 注入冒充生产流可达」,而 M1 在**生产路径结构不可达**(引擎永不做非法本地写)⇒ M1 在「负控 3 条」里**无合法实现形态** ⇒ R1~R8 亦从未处置 ⇒ 实施期必然撞「AC-3 × M1」矛盾.

⇒ **裁决:M1 不进本切片**(⛔ 不实现 · ⛔ 不进负控 · ⛔ 登记为「生产路径结构不可达」). 负控 3 条因此固定为:① **观测面自身失效** ② **写时他人 sharer 注入** ③ **陈旧副本注入**.

### 3 · SWMR 判据的**判定行**钉死

R4 的两处观测点下 `sharers_snapshot` 语义**随 kind 不同**:`kind=transition`(`apply_event` 入口,`:325-344`)= **迁移前**视图(`sync_tracker` 在 `:344` **末尾**才写目录);`kind=fill`(`:442` 之后)= **两次 apply_event + 两次 sync 之后**的视图(E-4 静默升级:一次 completion 产生 **2 条 transition + 1 条 fill**)⇒ 「写时 `view_of.sharers & ~self_mask != 0`」**未指明判定行** ⇒ 两行的 sharer 视图可差 **2 次目录同步**.

⇒ **裁决:判定行 = `kind = transition ∧ event = Wr ∧ to = M`(态迁移写)**;`kind=fill` 行**不参与** SWMR 判定(⛔ 显式豁免,理由 = 它是**同一写 episode 的第 3 条记录**)⇒ ⇒ 记入 design.

### 4 · 防「用测试台架喂饱判据」(M3 动机的可执行形态)

AC-1 ①「真看到过他人 sharer」**可被 fixture 直接播种目录满足** —— 观测流的 `sharers_snapshot` 取 `view_of`,**无法分辨** bit 来自对端引擎的 `sync_tracker` 自同步(`mesif_protocol_engine.hh:599-620`)还是测试 `set_sharer` 播种.

⇒ **design 硬约束**:**判定窗口内禁止 fixture 播种目录** —— 正控的**每一个** sharer bit 必须**可溯源到 peer 引擎自身的 `apply_event → sync_tracker`**;⇒ 溯源判据须**可机判**(见 v2 的 AC-1).

### 5 · R7 补洞 —— 新档 2 登记表须自带跨档聚合检查

C8 的跨档聚合禁令机器面(`cep_e2e_reconcile_check.sh` T1~T3/T2b + `evidence_scope` 档位词检查)**只覆盖 run** ⇒ 新表(档 2 登记面)必须**自带**:① 「不得与档 0/1 的 0 并列」② 「不含『通过态』表述」③ 上述两项的**反向 fixture**(⛔ AC-9 的 grep 闸是**文档层**,不足以独守).

### 6 · 计数纪律 —— 按「**写 episode**」归并

WRITEBACK 完成 = **2 条 transition + 1 条 fill** ⇒ 叠加缓冲上限与 `dropped_observations` ⇒ 若检测器按**原始行**计数,AC-3 的「违例数精确 == 1」会因**行倍增 / 丢行**以**错误原因**红.

⇒ **裁决:违例计数按 `(addr, write-episode)` 归并后计**(write-episode = 一次 `on_completion` 引发的连续记录组)⇒ 该纪律进 v2 的 AC-3 条文.

### 7 · 其余澄清(逐条)

| # | 澄清 |
|:--:|---|
| **a** | ⚠️ **R3-C4 的「降级」不是降级** —— `07 号:690` 阶梯表标题**已签批**为「强度**不可跨档比较**」;档 2 行的机器载体 cell(`:696`)本就写「⭕ 观测面未建(另立 change 评估)」⇒ **已签批阶梯的排序语义本就是「装置归属层级」** ⇒ R3 的交付恰好落在该 cell 内 ⇒ ⛔ 无需再作「降级」表述(原措辞作废,以本条为准) |
| **b** | 档 2 与档 1 是**正交**,⛔ **不是**强弱序 —— 档 2 判**真引擎状态机 + 真目录可见性**;档 1 判**完成域模型** ⇒ ⛔ 不得宣称「档 2 强于档 1」;宣传措辞须守住「**单族 / 单拓扑 / 无通过态**」 |
| **c** | `origin` 字段须钉死取值域 —— 取值 = `local_request`(本地发起)· `completion_fill`(完成装行)· `remote_snoop`(远端 snoop)· `writeback`(写回)· `back_invalidate`(反向失效)· `direct_inject`(⛔ 测试直调,**不计入**任何生产流判据) |
| **d** | 冻结引用**行号更正**:C′-bis 引「manifest `:38`」⇒ 实际判据落 `scripts/test/cep_freeze_check.sh:67`(manifest 只是被它读);行号以脚本为准 |
| **e** | 新 main spec **不得**绕过 change 直接落 `openspec/specs/` ⇒ 走本 change 的 delta spec 合并(⇒ 冻结面 84 → 85,**一次** `--update`) |
| **f** | oracle 建议「v2 后再过 metis 第 3 轮」—— ⛔ **不采纳**(planner 的 metis 预算 = 该 improvement **总计 1 轮**,已用于正式审查)⇒ v2 改由 **architect 裁决 + oracle 复核 + planner 自查**三方承担,⚠️ 该偏离在此登记 |

### 8 · 勘误后的裁决净态

| 项 | 净态 |
|---|---|
| R1 ~ R8 的**结论** | ✅ **全部维持** |
| R3 的**理由** | ⚠️ **改述**为「第五条路 = 定价后排除」(§1) |
| 新增硬约束 | M1 不进本切片(§2)· SWMR 判定行 = `transition ∧ Wr ∧ to=M`(§3)· 判定窗口禁目录播种 + provenance 可机判(§4)· 新表自带跨档聚合检查 + 反向 fixture(§5)· 按写 episode 归并计数(§6) |
| 估时 | **5-7 人日**(不变;勘误未增工程量) |

**关联**:台账 **Entry #408** · **#407**(architect 梳理)· **#406**(C′ 段)· C′-bis 段(R1~R8)· `.rddf/improvements/cep-evidence-tier2-minimal-slice.md`(v1 · 待改 v2)· `test/cep_consistency_completion_model.hh:111-116,175-186` · `experimental/preregistration.json:63-71` · `tools/render_report.py:161-193` · `scripts/test/cep_experiment_run.sh:155-177` · `docs/architecture/07-coherence-exploration-platform.md:690,696`
