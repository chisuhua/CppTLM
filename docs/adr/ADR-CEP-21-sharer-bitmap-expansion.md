# ADR-CEP-21: Sharer 位宽 —— responder bit 映射的**扩容机制**与触发判据

> **状态**: 🟢 **Proposed**（2026-11-14 · Draft 起草 → **双审通过**（oracle BLOCK→12 条全采纳 · critic APPROVE_WITH_CONDITIONS · **0 BLOCKER**→4 SHOULD_FIX + 2 NIT 全采纳）→ 转 Proposed）
> **日期**: 2026-11-14
> **作者**: CppTLM architect
> **Parent**: —（⛔ 不取代任何 ADR）
> **关联 change**: `backlog: cep-responder-bitmap-expansion`（⚠️ **尚未立项** —— 立项与 AC 编号由 planner 阶段补）
> **AC 映射**: ⚠️ **暂缺**（change 未立项；立项后回填）
> **审稿报告**: `.rddf/reviews/cep-adr-021-oracle-review.md`（第 1 审）· `.rddf/reviews/cep-adr-021-critic-review.md`（第 2 审）
> **关联真源**: `ADR-CEP-04`（Sharer Tracker 抽象 · ✅ 已签发 v1.0 · **不可变**）· `ADR-CEP-12`（协议引擎能力图 · ⚠️ **其 `INV-CEP-12-IC1` 锁定的 13 键 schema 形态是本 ADR 触发判据 1 的前置**）· `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_instance.schema.json`

---

## 背景与问题

### 三层机制事实（实测）

| 层 | 事实 | 位置 |
|---|---|---|
| **容量上限** | `responder_set_t = std::uint64_t` + `static_assert(sizeof(responder_set_t) == 8, …)` ⇒ **硬上限 = 64 个 sharer**；⚠️ `kNoBit = 64u` 与之**同值**（非巧合 ⇒ 见「问题 3」） | `sharer_tracker.hh:75-79` / `:91` |
| **当前映射宽度** | `kResponderMappedBits = 4`；`kResponderBitToNode` 是 **`std::array<node_id_t, 8>`**（尺寸已够 8，后 4 项为 `kInvalidNode`）—— bit0=`kNodeIdSdma` · bit1=`kNodeIdVram` · bit2=`kNodeIdGmmu` · bit3=`kNodeIdPcie` | `:92` / `:96-102` |
| **越界行为** | 未映射节点 ⇒ `responder_mask_of(node) == 0` ⇒ `set_sharer` **静默 no-op**；⚠️ 且 `sync_tracker` 在静默 no-op 时**仍** `++tracker_feeds` ⇒ 「**接线良好**」的假象 | `:112-115` + `mesif_protocol_engine.hh:340-343` |

### 当前防线（已落地，⛔ 本 ADR 不改）

`mesif_protocol_engine.hh:347` 的**装配期硬断言**：`responder_mask_of(node) == 0` ⇒ `mark_observation_invalid()` ⇒ 判「**不可判**」而非「零违例」（档 2 change 已交付并验收）。⚠️ 该防线**只覆盖 mesif 一族**；其余族的同类断言不在本 ADR 范围。

### 依赖点实测（扩容必须同步的 **6 处** —— v2 修正：原列 4 处，oracle 实测补 2 处）

| # | 位置 | 依赖形态 | 扩容后 |
|:--:|---|---|:--:|
| 1 | `responder_bit_of(node_id_t)` `:104` | `constexpr` · 上界 = `kResponderMappedBits` | ✅ 自动 |
| 2 | `responder_mask_of(node_id_t)` `:112` | `constexpr` · 经 `responder_bit_of` | ✅ 自动 |
| 3 | `responder_node_of(unsigned bit)` `:117` | `constexpr` · 上界 = `kResponderMappedBits` | ✅ 自动 |
| 4 | `kMappedResponderMask` `:125-127` | `inline constexpr` · `(1<<kResponderMappedBits)-1` | ✅ 自动 —— ⚠️ 但**语义漂移**（见问题 2） |
| 5 | `test/cep_directory_experiment.hh:180` | ⚠️ **硬编码 `bit < 4`**（snoop 遍历） | ❌ **必须改** |
| 6 | `test/cep_directory_experiment.hh:47` + `:165-166` | ⚠️ `broadcast_fanout = 4` + 注释「上限 4」;**`if (n > 4) n = 4;`**（`mapped_prefix_mask`） | ❌ **必须改** |
| **7** | **`openspec/specs/cep-sharer-tracker-base/spec.md:140`** | ⚠️ **契约面**：Requirement 正文写「含**未映射位**(**bits 4..63**)」⇒ 阶段 2 后 bits 4~(W−1) 变为**已映射** ⇒ **该 Requirement 语言直接失效** | ❌ **必须随阶段 2 change 走 spec delta**（`bits 4..63` ⇒ `bits W..63`） |

> ⚠️ **第 7 处为何单列**：它是清单中**唯一的契约面**（其余 6 处皆为执行面）⇒ 若阶段 2 只改代码不改 spec，会把「bits 4..63」旧契约与新代码并存 ⇒ **spec / 实现脱钩**。
> ✅ **经核查无需 delta 的同款措辞**：`openspec/specs/cep-tilelink-tree-manager/spec.md:103` 用的是**语义性**表述「含**未映射位**」⇒ 阶段 2 后仍成立（「未映射位」自动指 `W..63`）⇒ ⛔ 不需 delta（仅登记）。

⚠️ **另一实测约束**：依赖点 1~4 **全部 `constexpr`** ⇒ **运行时注册节点不可行**（除非把 `kMappedResponderMask` 从 `inline constexpr` 降级为运行时值、并让 `snoop_begin_core` / `snoop_join_core` 的拒绝检查运行时化）⇒ 扩容只能是**编译期表扩容**。

⚠️ **测试覆盖的实测缺口（v2 新增）**：`test/test_cep_sharer_tracker_base.cc:214-219` 的双向一致性判据是**显式 initializer list**（`{kNodeIdSdma, kNodeIdVram, kNodeIdGmmu, kNodeIdPcie}`）⇒ **不随 `kResponderMappedBits` 扩展** ⇒ 阶段 2 扩容后 bits 4~7 **零覆盖** ⇒ 必须新增**满射判据**（见 S2-3）。

⚠️ **闸的文本锚盲区（v2 新增）**：`scripts/test/cep_tilelink_adapter_reverse_verify.sh:49` 含 `join-no-unmapped-reject` 的**文本锚**（引用 `kMappedResponderMask` 的源码原文）⇒ 阶段 2 后 `kMappedResponderMask` 从 `0xF` 变 `0xFF` 而**源码文本不变** ⇒ **该闸照绿** ⇒ 需补**运行时**判据（见 V2-6）。

### ⚠️ 诚实登记：**仓内无「某族需要 > 4 sharer」的实测需求**

- `protocol_instances.json` 的 **8 个实例**（`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c` / `nvlink_4` / `ualink_4`）**均未声明节点数**；
- ⚠️ 且**结构上无处可声明** —— `protocol_instance.schema.json` 的 `sharer_tracker` 节是 `additionalProperties: false` + `required: [impl, caps]`，**只有 `impl` / `caps` 两个键** ⇒ **13 键中不存在节点数声明位**；
- 各族节点规模数据在 **ArchForge 仓**（`32 号 §6.x`）⇒ **本仓无权威来源**；
- ⇒ ⇒ 本 ADR **不能**以「某族现在就需要 N 个 sharer」为依据（那会是**编造**）；其正当性只能是「**扩容路径未机制化 + 触发判据未定义 + 两处硬编码残留 + 测试/闸的覆盖缺口**」这一**结构性缺口**。

### 问题陈述（四条 —— v2 由三条扩为四条）

1. **机制缺口**：`kResponderMappedBits` 的扩容**无成文流程** —— 何时该扩、扩到多少、随扩须同步哪些点，全无定义 ⇒ 首个真正需要 > 4 的族会**先撞静默 no-op**再被发现；
2. **语义漂移缺口**：`kMappedResponderMask` 随宽度自动变宽 ⇒ `snoop_begin_core`(`:370`) / `snoop_join_core`(`:425`) 的**拒绝面**随之收窄；而 `snoop_fold` 的扣减走 `responder_mask_of(node_id)`(`:452`) ⇒ 若某位映射到 `kInvalidNode`，该位的 `mask = 0` ⇒ **永不扣减 ⇒ in-flight 条目永不 retire ⇒ 该地址 `lookup` 永久 STALL**（这正是选项 B 的致命后果）；
3. **上限未文档化**：`responder_set_t` 的 **64 上限**与 `kNoBit = 64u` 的**同值**未在决策文档登记 ⇒ 未来有人扩到 64 之上会同时撞 `static_assert` 与 sentinel 语义；
4. **硬编码与覆盖缺口**：`cep_directory_experiment.hh` 两处（`:180` 循环 / `:47`+`:165` fanout 与前缀 clamp）+ 测试的 initializer list + 闸的文本锚 ⇒ 扩容后会留下**假绿面**。

---

## 决策

> **决策句**：采用**两阶段**方案 —— **阶段 1（本 ADR 成立即生效）机制化**; **阶段 2（触发式）按本 ADR 定义的流程扩容到宽度 `W`（**默认 8**，若实测需求 > 8 则按同一流程上调，依据入台账）**。

### 阶段 1 —— 机制化（本 ADR 成立即生效）

| # | 动作 | 落点 |
|:--:|---|---|
| **S1-1** | **三处硬编码全消**（v2 由一处扩为三处）：<br>① `:180` 循环上界 `bit < 4` ⇒ `bit < kResponderMappedBits`<br>② `:165` 的 `if (n > 4) n = 4;` ⇒ `n = std::min<std::uint64_t>(n, kResponderMappedBits);`<br>③ `:47` 的注释「映射位只有 4 个 ⇒ 上限 4」⇒「**默认**探前 4 个已映射节点；实际上限 = `kResponderMappedBits`」（⚠️ **默认值 4 作为实验参数可保留**，但其**语义**须改为「默认」而非「上限」） | `test/cep_directory_experiment.hh` |
| **S1-2** | 在 `sharer_tracker.hh` 的 `kResponderBitToNode` 上方**成文登记**四项：① 扩容 = **只改 `kResponderMappedBits` + 填表项**（数组尺寸**已是 8，不需改**）② **硬上限 = 64**（`responder_set_t` 宽度 + `kNoBit` 同值）③ ⛔ **不得**填无权威来源的 node id ④ **若某族需 > 64 sharer ⇒ 另立 ADR**（候选路径：沿用 `HierarchicalDirectory` 的层级聚合 / 或多字位图）—— **本 ADR 不裁** | 该文件注释 |
| **S1-3** | 建立**扩容触发判据**（下节）⇒ 由该判据决定是否进入阶段 2 | 本 ADR |
| **S1-4** | 在 `test/test_cep_sharer_tracker_base.cc` **新增满射判据**（v2 新增）：`for b in [0, kResponderMappedBits): responder_node_of(b) != kInvalidNode && responder_bit_of(responder_node_of(b)) == b` ⇒ **此判据随宽度自动覆盖新 bit**（补上 initializer list 的覆盖缺口） | 该测试文件 |

### 阶段 2 —— 扩容到 `W`（**触发后**执行，⛔ 不得预先执行）

| # | 动作 | 约束 |
|:--:|---|---|
| **S2-1** | `kResponderMappedBits` **4 → W**（默认 **8**） | ⚠️ bit 0~3 映射**逐位不变** ⇒ 向后兼容（`test_cep_sharer_tracker_base.cc:211-212` 的 `responder_node_of(0)==kNodeIdSdma` / `responder_bit_of(kNodeIdSdma)==0` 必须继续成立） |
| **S2-2** | `kResponderBitToNode` 填 bit 4~(W−1) 的表项 | ⚠️ **数组尺寸已是 `std::array<node_id_t, 8>` ⇒ 若 W ≤ 8 连数组尺寸都不改**，只填项；`W > 8` 时才需扩数组。⛔ 新 node id **必须**有**权威来源**（ArchForge 族声明 / 该实例的 13 字段声明）⇒ ⛔ **不得**由本仓编造 |
| **S2-3** | **跑满射判据**（S1-4 新增的那条）⇒ 自动覆盖新 bit；⚠️ **⛔ 不得**依赖原有的 initializer list 用例（它对 bits ≥ 4 零覆盖） | — |
| **S2-4** | 若新扩的族有 protocol engine ⇒ **必须**同时加装配期硬断言（`responder_mask_of(node)==0 ⇒ mark_observation_invalid`） | 沿用 mesif 先例；⚠️ 否则档 2 会把「不可判」误报为「零违例」 |
| **S2-5** | **补运行时判据**（v2 新增）：probe 断言 `kMappedResponderMask == (1<<W)-1` ⇒ 堵住「文本锚闸在 mask 语义漂移时照绿」的盲区 | 详见 V2-6 |

### 触发判据（进入阶段 2 的**唯一**条件 · v2 拆为「机判」与「人裁」两段）

> **2a（机器信号 · 可机判）**：某**非 `kInvalidNode`** 的 node 在装配期**由 `responder_mask_of(node) == 0` 这一分支**触发 `mark_observation_invalid()`（mesif 先例的 `:347` 断言），且该 node **在本次装配中确实被提交了 sharer 操作**（观测面可观测到该 node id）。
> ⚠️ **必须限定触发源**：`mark_observation_invalid()` 共 **4 个**触发源（`tracker_ == nullptr` / `responder_mask_of(node) == 0` / `level >= kLevelCount` / 防御断言 caps ≠ EXACT）⇒ 2a **只认第 2 个源**（前三个源中只有它指向「映射宽度不足」）⇒ 否则 2a 与「接线缺失 / 层级越界」不可区分;
> **2b（人裁 · ⛔ 不可机判，须留痕）**：由 architect 裁定该触发属「**映射宽度不足**」还是「**配置错误**」⇒ 裁定结论 + 原始数据（哪个 node、哪次装配、什么观测）**必须**入台账 Entry（载体见 V2-5）。
> **2c（替代入口 · 可机判）**：ArchForge 的族声明更新后，本仓镜像同步发现节点数 `> 4`（可用 `cep_decl_alignment_check.sh` 的镜像比对佐证）。
> ⚠️ **判据 2a 的结构限制**：`protocol_instances.json` 的 13 键**无节点数声明位**（schema `additionalProperties: false`）⇒ ⇒ 「实例声明给出 > 4 个节点」这条**当前结构上不可能成立** ⇒ 若要让其可用，须先演进 schema（属 **`ADR-CEP-12` 域**，其 `INV-CEP-12-IC1` 锁定 13 键形态）⇒ **该 schema 演进本身即触发本 ADR 判据 2c 的评估**。
> ⛔ **不构成**触发条件：「预计将来会需要」「某族理论上支持更多节点」「为了对称好看」—— 无实测/声明依据 ⇒ **不得**据此扩容。

### 为什么默认 `W = 8`

- **下界**：必须 > 4; **上界**：64（`responder_set_t` 硬上限）;
- **取 8 的理由**：是当前 4 的**最小整数倍**，配合「触发式再扩」的两段结构 ⇒ **避免一次跳到 16/32 造成 8~31 位长期空置**（死位）;
- ⚠️ **8 不是从需求数据反推的** ⇒ 它是**默认机制参数**（决策句已写明「若实测 > 8 按同一流程上调」）⇒ 触发后照抄 8 会形成**路径依赖**时，以台账的实测依据为准。

---

## 选项对比表

| 选项 | 内容 | 结论 | 理由 |
|---|---|---|---|
| **A（采纳）** | **两阶段**：阶段 1 机制化（三处硬编码全消 + 满射判据 + 成文扩容流程 + 触发判据）;阶段 2 触发后扩到 W（默认 8） | ✅ **采纳** | 唯一同时满足「不编造 node id」「不留死位」「消除硬编码/覆盖假绿」三项的方案;扩容路径就绪但**不预先消耗位宽** |
| **B** | **立即扩容到 8**，bit 4~7 填 `kInvalidNode` 占位 | ❌ 否决（v2 理由改写） | ① **sentinel 语义被破坏**：`kResponderMappedBits = 8` 后 `responder_bit_of(kInvalidNode)` 在 **bit 4 命中** ⇒ 返回 **`4` 而非 `kNoBit`**(`:104-110`) ⇒ `responder_mask_of(kInvalidNode)` = `1<<4` **≠ 0** ⇒ `kInvalidNode` 的「未映射」语义失效（`test_cep_sharer_tracker_base.cc:221` 即红）;② **制造永久 STALL**：`kMappedResponderMask` 随之覆盖 8 位 ⇒ `snoop_begin_core`(`:370`)/`snoop_join_core`(`:425`) 的**拒绝面收窄**（不再拒绝 bits 4~7）⇒ 而 bits 4~7 在表内映射的是 `kInvalidNode` ⇒ **真正的第 5 个真实 node id 在表内无匹配** ⇒ `responder_mask_of(该 node)` **恒 0** ⇒ `snoop_fold` 的扣减(`:452`)永不到位 ⇒ **条目永不 retire ⇒ 该地址 `lookup` 永久 STALL** |
| **C** | **立即扩容到 8**，bit 4~7 填本仓自拟的 node id | ❌ 否决 | ⛔ **编造 node id** —— 与 ArchForge 声明冲突时无仲裁依据;且这些 node 在 8 个实例里**无一**声明要跟踪 ⇒ **死代码** |
| **C′（v2 补 · oracle 指出的第三形态）** | 把 node id 与「SoC 硬件代理」**解耦**，改为「协议族内逻辑序号」 | ❌ 否决 | ① 破坏 `sharer_tracker.hh:84-85` 登记的「**复用传输层 `NODE_ID` 编码以保持可对账**」设计意图;② `soc_internal_noc.hh:61-64` 的 `NODE_ID` 空间**只有 4 个取值** ⇒ 逻辑序号 4+ **两仓均无定义** ⇒ 需另立编码真源 ⇒ **代价远超本 ADR 收益** |
| **D** | **改运行时注册**（`register_node(node_id_t)`） | ❌ 否决（v2 理由具体化） | 依赖点 1~3 全 `constexpr`(`:104/:112/:117`) **且** `kMappedResponderMask` 是 `inline constexpr`(`:125`) ⇒ 运行时注册须：① 把该 mask 降级为运行时值 ② 把 `snoop_begin_core` / `snoop_join_core` 的**拒绝检查运行时化** ⇒ 波及全部调用点（含 `cxl_dcoh_tracker.hh:199`）⇒ 成本远超收益;且节点集是**编译期已知**的协议族属性，运行时可变反而弱化契约 |
| **E** | **不扩容也不机制化**（维持现状 4） | ❌ 否决 | 硬编码（两处）+ initializer list 覆盖缺口 + 文本锚闸盲区**全部保留**;扩容流程继续无定义 ⇒ 首个需要 > 4 的族会先撞静默 no-op;⚠️ 作为 interim 与阶段 1 的差异仅 = 「是否消除缺口 + 是否成文」⇒ 阶段 1 **无理由不做** |
| **F** | 把 `responder_set_t` 扩到 128/256 bit | ❌ 否决 | 破坏 `static_assert(sizeof == 8)`(`:79`) + 改变 `ResponderSet` 的既成表示法 + `kNoBit` sentinel 语义 ⇒ 64 上限**当前零压力** ⇒ 成本/收益严重失衡;⚠️ 若某族真需 > 64 ⇒ **另立 ADR**（本 ADR 不裁，见 S1-2 ④） |

---

## 后果 / 代价

### 正面

- ✅ **三处硬编码 + 一处覆盖缺口 + 一处闸盲区全部消除**（S1-1 / S1-4 / S2-5）—— 扩容不再留下「实验仍只测 4 位」「mask 漂移而闸照绿」的死角;
- ✅ **扩容从「无人知道怎么做」变成「照流程做」** —— 触发判据 2a 可机判、2b 留痕、2c 有镜像比对佐证;
- ✅ **不编造、不预占** —— ⛔ 不填无权威来源的 node id，⛔ 不留长期空置死位;
- ✅ **零 ABI / 零接口变更** —— `SharerTrackerIf` 虚方法数保持 **5**，`responder_set_t` 宽度不变，`constexpr` 语义不变;
- ✅ **新增 invariant 数 = 0**;⛔ 不改 `ADR-CEP-04`（caps 抽象与 4 属性不受影响）。

### 负面 / 风险

| # | 代价 / 风险 | 缓解 |
|:--:|---|---|
| **N-1** | **阶段 1 不改变任何运行时行为** ⇒ 可能被质疑「空 ADR」 | 决策句明示：阶段 1 做**机制化**（三处硬编码 + 满射判据 + 成文流程 + 触发判据）;其价值在**首次触发时**兑现;⚠️ 但「消硬编码」本身在**当前**就消除假绿面（`cep_directory_experiment` 的 fallback 前缀与广播 fanout 的注释语义错误**今天就存在**） |
| **N-2** | **触发判据 2a 的可观测性依赖装配例** ⇒ 若某族从不跑装配例，2a 不触发 | 判据 2c（ArchForge 镜像比对）兜底;⚠️ 且该风险**不是本 ADR 引入**的 |
| **N-3** | **默认 `W = 8` 无需求依据** ⇒ 触发时可能照抄 | 决策句已改为「默认 8，实测 > 8 则上调，依据入台账」;V2-1 的验收判据要求「`W` 的取值依据在台账有 Entry」 |
| **N-4** | **阶段 2 若被误当「已执行」** ⇒ 文档与代码脱钩 | V1-4 要求 `kResponderMappedBits` 仍 = 4 且满射判据绿 ⇒ 「阶段 2 未执行」有可机判证据 |
| **N-5** | 新扩族漏加装配期硬断言（S2-4）⇒ 档 2 把「不可判」误报为「零违例」 | S2-4 为强制项;由档 2 的「断线三连」用例范式约束（新族须提供同型反 fixture） |
| **N-6** | **schema 演进是判据 1 的前置**，而 schema 属 `ADR-CEP-12` 域 ⇒ 存在**跨 ADR 协调成本** | 已在「触发判据」与「关联真源」两处显式登记;⛔ 本 ADR 不裁 schema |

### 不做什么（明确边界）

- ⛔ **不改** `responder_set_t` 宽度（仍 64 bit）; ⛔ **不改** `SharerTrackerIf` 的 5 个虚方法;
- ⛔ **不改** `ADR-CEP-04` / `ADR-CEP-12`（已签发，不可变）; ⛔ **不改** `kLevelCount`;
- ⛔ **不填**任何无权威来源的 node id; ⛔ **不做**运行时节点注册; ⛔ **不裁**「> 64 sharer」的表示法（**另立 ADR**）。

---

## Verification（可勾选 · **前瞻**判据）

### 阶段 1（本 ADR 成立即可验收）

- [ ] **V1-1** `test/cep_directory_experiment.hh` 三处硬编码全消：① `grep -c 'bit < 4'` = **0** 且含 `kResponderMappedBits`;② `grep -c 'n > 4'` = **0** 且含 `std::min…kResponderMappedBits`;③ `broadcast_fanout` 的注释不再含「上限 4」⚠️ **其中 ①/② 可机判;③ 的「语义是否为『默认』而非『上限』」由人裁**（与 V1-6 同类诚实登记）
- [ ] **V1-2** `sharer_tracker.hh` 的 `kResponderBitToNode` 上方注释含四项：**只改常量+填表项**（数组尺寸已 8）· **64 硬上限** · ⛔ 不得填无权威来源 id · **>64 ⇒ 另立 ADR**
- [ ] **V1-3** 全仓扫描**三种形态**（`grep -rn` 于 `include/ src/ test/ scripts/`）：`bit < 4` · `n > 4` · `broadcast_fanout = 4` ⇒ **除本 ADR 与其归档件外 0 命中**
- [ ] **V1-4** `kResponderMappedBits` 仍 = **4** ⇒ 阶段 2 **未执行**;且**新增满射判据**（S1-4）绿 ⇒ 且 `test_cep_sharer_tracker_base.cc` 原有用例全绿
- [ ] **V1-5** 0 回归：`ctest` 全绿 · `scripts/test/cep_*.sh` 逐个 rc=0 · `format --check` rc=0
- [ ] **V1-6** ⚠️ **已知真空登记**（v2 新增）：当前**无任何机器闸/测试钉死 `kResponderMappedBits == 4`** ⇒ 阶段 2 的「未执行」状态只能靠 V1-4 的 grep 判据 + 人肉复核 ⇒ 本项为**诚实登记**，⛔ 不假装有机器守
- [ ] **V1-7**（v2 新增）**观测面可定位触发节点**：`mark_observation_invalid()` 的 `responder_mask_of(node)==0` 分支**可反查**触发它的 node id（供判据 2a 使用）⇒ 落地形态 = 该分支记录 node id（或 `EventObservation` 侧可定位）;⚠️ 当前 mesif 观测面**未见**该反查能力 ⇒ 本项是**阶段 1 需补的前瞻判据**，若判定不补则 2a 降级为「人裁 + 装配日志」

### 阶段 2（**触发后**才验收；未触发时本组**不判**）

- [ ] **V2-1** `kResponderMappedBits` == **W**，且 **W 的取值依据在台账有 Entry**（⛔ 不得仅写「默认 8」）⚠️ **「依据是否充分」由人裁**（grep 只能验 Entry 存在）
- [ ] **V2-2** `kResponderBitToNode` 的 bit 4~(W−1) **全部**为有效 node id（⛔ 不得为 `kInvalidNode`）
- [ ] **V2-3** bit 0~3 映射**逐位未变** ⇒ `responder_node_of(0)==kNodeIdSdma` 且 `responder_bit_of(kNodeIdSdma)==0` 仍成立
- [ ] **V2-4** 每个新增 node id 的**出处**（ArchForge 声明 / 实例 13 字段）入台账 Entry
- [ ] **V2-5** **触发证据**入台账：2a 的机器信号原始数据 + 2b 的 architect 裁定 + （若适用）2c 的镜像比对输出
- [ ] **V2-6** **运行时 mask 判据**（v2 新增）：probe 断言 `kMappedResponderMask == (1<<W)-1` ⇒ 堵住「`cep_tilelink_adapter_reverse_verify.sh:49` 的文本锚在 mask 语义漂移时照绿」的盲区
- [ ] **V2-7** 新扩族（若有 engine）具备装配期硬断言 + 其「不可判」反 fixture（同 mesif「断线三连」范式）

### 永不可违（两阶段共通）

- [ ] **X-1** `SharerTrackerIf` 虚方法数 = **5** · **X-2** `sizeof(responder_set_t) == 8` 的 `static_assert` 仍在
- [ ] **X-3** 新增 `INV-CEP-` 标识数 = **0** · **X-4** `docs/adr/ADR-CEP-04-*.md` **零 diff**

---

## 关联

| 类型 | 载体 |
|---|---|
| 上游（不改） | `ADR-CEP-04`（Sharer Tracker 抽象 · ✅ 已签发 v1.0 · **不可变**）· **`ADR-CEP-12`**（⚠️ **其 `INV-CEP-12-IC1` 锁定的 13 键 schema 是触发判据 1 的前置**）· `ADR-CEP-09`（无冲突：VC 仲裁与 sharer 位宽正交） |
| 代码面 | `sharer_tracker.hh`（`:75-79` / `:84-85` / `:91-102` / `:104-127` / `:370` / `:425` / `:452`）· `mesif_protocol_engine.hh`（`:340-347`）· `cxl_dcoh_tracker.hh:199` · `soc_internal_noc.hh:61-64` |
| 测试面 | `test/test_cep_sharer_tracker_base.cc`（`:211-219` initializer list · `:221` sentinel）· `test/cep_directory_experiment.hh`（`:47` / `:165-166` / `:180`）· `test/test_cep_h2_h3_experiments.cc`（相对差判据，不钉绝对 fanout ⇒ 扩容不破坏） |
| 闸面 | `scripts/test/cep_tilelink_adapter_reverse_verify.sh:49`（**文本锚** ⇒ 阶段 2 需补运行时判据）· `scripts/test/cep_decl_alignment_check.sh`（判据 2c 的镜像比对佐证） |
| 声明面 | `protocol_instances.json`（8 实例 · 均未声明节点数）· `protocol_instance.schema.json` 的 `sharer_tracker` 节（`additionalProperties: false` · 仅 `impl`/`caps`） |
| 上游数据（仓外） | ArchForge `32-cep-based-coherence-architecture.md §6.1 / §6.2` |
| 关联 change | `backlog: cep-responder-bitmap-expansion`（**未立项**;立项后回填 AC 映射） |

---

## 审稿报告（指针 · ⛔ 过程日志不进本文档）

见元数据的「审稿报告」指针（`.rddf/reviews/cep-adr-021-oracle-review.md` / `cep-adr-021-critic-review.md`）—— 治理台账与审稿报告是过程日志的**唯一落点**。
