# Design — cpptlm-cep-mesif-adapter v1.0

> **状态**: 🟡 **Proposed**(2026-10-30)· **队列**: `obj-cep-platform §4.0` **队列 #9**(Stage 1 / 2028-Q4)
> **配套**: [`proposal.md`](proposal.md) · [`specs/cep-mesif-adapter/spec.md`](specs/cep-mesif-adapter/spec.md) · [`specs/cep-hierarchical-directory/spec.md`](specs/cep-hierarchical-directory/spec.md) · [`tasks.md`](tasks.md)
> **上游**: 规约层 `cpptlm-cep-mesif-state-spec`(**已归档**)—— 本设计**只消费**其载体,不重新推导
> **实施计划**: `.rddf/plans/2026-10-30-cpptlm-cep-mesif-adapter.md`(v0.2,oracle + metis 双审)

---

## D-α `MesifAdapter` **零自有表副本**,委托消费规约载体

**问题**:`moesi_gpu` / `chi_rnf` 的 adapter 都**自带表**(`moesi_gpu_adapter.hh:73` 内嵌 `kStateTransition`)⇒ 规约层与实现层各一份 ⇒ 双真源风险(规约改一格,adapter 不红;或反之)。

**决策**:`MesifAdapter` **SHALL NOT** 携带自有 5×6 转换表 / 投影表副本;SHALL 以 `constexpr` 委托消费 `MesifStateSpec::kTable` + `perm_of()` + 三元组(`kUniqueCleanState` / `kWriteEvent` / `kUniqueDirtyState`,`mesif_state_spec.hh:440-443` 明写「供未来实现层消费」)。

**理由**:① 规约层已把推导做完(35 格逐格 + 追认闭合),实现层重推 = 引入第二个可漂移的语义源;② 规约层的 14 条 `static_assert`(静态锁 ⑪~⑫)自动覆盖 adapter —— 规约一改,adapter 编译期即红;③ proposal 的风险表第 1 行「实现层自己拍一套转换表」在此形态下**结构性不可能**,而非「拍了会被抓到」。

**代价**:adapter 不再是**自包含**载体(换协议要换规约层)。接受 —— 规约层本就是该协议的**唯一真源**。

**落点**:`include/tlm/cep/mesif_adapter.hh`;判据 AC-1。

---

## D-β `HierarchicalDirectory` 的行为决策**逐条 basis 登记** + **挂跨仓追认债**

**问题**:该实现的**行为语义**在 ArchForge 与本仓**都无规约** —— 只有 caps 声明(`sharer_tracker.hh:205-207`)+ 一句注释级描述(`:138-141`「L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray」)。实现者必然**自推**多层聚合语义。

**决策**:
1. 凡 ArchForge 无规约的行为决策 —— **层级数** / **`ESCALATE_UP` 是否使用** / **`snoop_fold` 合并规则是否与 `FullMapDirectory::join_state` 同源** / **handle 满时的拒绝语义** / **F 态(转发点)在 tracker 中的表示** —— **逐条**在类注释 + 本 design 中以 **`basis` 式**登记(**推导依据** + **是否待追认**)。
2. 每条决策 SHALL 标 `pending = true` ⇒ **挂跨仓追认债**(类比规约层 D-α 的 35 格 `pending` + ArchForge `42 号` 逐格追认)⇒ **不得**写成「已裁决」。
3. **F 态表示**必须**显式决策**:`SnoopState` 只有 `I/S/E/M`(`sharer_tracker.hh:128`),**无 F** ⇒ 「谁是转发点」对 tracker **不可见**。候选:`LocalView::forward_to`(`:251`)承载 F 节点 / 登记「转发点保真度缺口」。

**理由**:规约层的 G1/G2 之所以能爬出来,靠的正是「逐格 basis + `pending` 标记 + 跨仓追认」三件套。若本 change 不配同款机制,等于**刚闭合的债又开一个新口**,而且是**无标记的隐性债**。

**落点**:`include/tlm/cep/sharer_tracker.hh` 的 `HierarchicalDirectory` 类注释 + 本 design §行为决策登记表;判据 AC-3 + HD-04。

### 行为决策登记表(实施时逐条填「依据」与「待追认」)

| # | 决策点 | 依据 | 待追认 |
|---|--------|------|:-----:|
| B-1 | 层级数与层级语义(L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray) | `sharer_tracker.hh:138-141` 注释级描述 + `ADR-CEP-04 §13` 的「层级深度是实现参数,不是能力属性」 | ✅ |
| B-2 | `LookupHazard::ESCALATE_UP` 是否使用 | `ADR-CEP-04 §2` 的 hazard 语义 | ✅ |
| B-3 | `snoop_fold` 的合并规则(是否复用 `FullMapDirectory::join_state`) | `sharer_tracker.hh` 的 `MergedState` 同型约束 | ✅ |
| B-4 | handle 空间满时的拒绝语义 | `ADR-CEP-04 §2` 的 caps 语义(`EXACT` ⇒ 不可降级) | ✅ |
| B-5 | **F 态(转发点)在 tracker 中的表示** | MESIF 规约的 `F` 态 = 单 Owner + 多 Sharer + 转发点 | ✅ |

---

## D-γ R-3 的补偿维度 = `LocalView.dirty` 掩码 + 写回接线(**不是**「谁持有」)

**问题**:语义损失 ③(纠正 `M+SnpShared`:`S`→`F` 后)—— 「已写回(干净)」与「仍脏」在 4 值 `perm` 上**同值不可分辨** ⇒ 写回事实在 `perm` 接口不可见。

**决策**:补偿路径 SHALL 是:
1. **维度**:`LocalView::dirty`(`sharer_tracker.hh:247-252`,64-bit bitmask,per-node 脏位)—— `FullMapDirectory::set_sharer` 已按 `SnoopState == M` 置/清 dirty 位(`:301-313`)⇒ home 侧目录**能**分辨「dirty 位清 + sharer 位仍在」(已写回)与「dirty 位置」(仍脏)。
2. **接线**:`MesifProtocolEngine` 在 `M + SnpShared → F` 路径上 SHALL 把「写回发生」喂给 tracker(fold / observe 时 dirty → clean)。

**⚠️ 与损失 ① 的维度区分**(措辞纪律,判据须能机器区分):

| | 损失 ①(`kPermMergeNote`) | 损失 ③(`kPermMergeWritebackPathNote`) |
|---|---|---|
| 不可分辨的 | `F` 与 `M` 同投影 `WRITEBACK` ⇒ 「是否还有其他 sharer」 | 「已写回(干净)」与「仍脏」 |
| 补偿维度 | **sharer 集合**(`LocalView::sharers`) | **dirty 掩码**(`LocalView::dirty`) |
| 可补偿性 | `mesif` = `EXACT` + `GLOBAL` ⇒ **可补偿** | **可补偿**(同上) |
| 对照 | `chi_rnf` = 「不参与」⇒ **协议信息拓扑的固有损失**,RN 侧**不可补偿** | — |

**理由**:补偿**不会自动发生** —— 只声明 caps 不够;若不接线,「可补偿」就是**口号**,而 `mesif_state_spec.hh:333-337` 的登记措辞已承诺「可由 Sharer Tracker 补偿」⇒ 载体必须兑现这句话。

**判据**:AC-7 —— 驱动 `M + SnpShared → F` 后,① `perm` 前后**同为 `WRITEBACK`**(不可分辨的**正控**)② tracker `dirty` 位**由置转清**(可分辨的**判据**);反向 fixture = 断掉写回接线 ⇒ dirty 不转 ⇒ 必红。

---

## D-δ atomic 的**最小可用路径** + `consistency_intent` **无独立字段**

**问题**:`protocol_instances.json:108-111` 声明 `mesif.atomic.supported = true` + `intent_field = "consistency_intent"`,但全仓 **grep 零消费点**。载体侧:`CohIfBundle` 已有 `ch_uint<1> atomic`(`cep_coherence_bundles_tlm.hh:114`,「与 `perm` **正交**的独立属性, per CEN-Ib11」)⇒ **意图位可承载**;但 `consistency_intent` **无独立字段**。

**决策**:
1. 引擎 SHALL **接受** atomic 事务(**不**触发拒绝闸)⇒ 按 `vc_binding` 路由至 `ualink_fabric` 的 **VC 1**(`protocol_instances.json:139`)。
2. 意图由 `CohIfBundle::atomic` 承载。
3. `consistency_intent` SHALL **不新增独立字段** —— 由 **`perm` + `consistency_labels`** 承载(`read: RC-acquire` / `write: RC-release`,`:113-116`)⇒ 该裁决 SHALL 在本 design **显式登记**,**不得沉默**。

**理由**:① `atomic.supported` 属**镜像域 12 键**之一 ⇒ 改它会破 ⑥ 镜像闸(除非 ArchForge 同步改)⇒ 「本期不支持」这条路**代价更高**;② `chi_rnf` 的先例恰是 `supported = false` + 拒绝闸(`chi_rnf_protocol_engine.hh:82,90`),两者语义不一致会污染先例;③ 扩 `CohIfBundle` 违反队列 #7 的「只覆盖 5 条 RN-side 流」定位 + 触碰冻结面外的载体契约。

**代价**:`consistency_intent` 的**协议侧**定义(acquire/release 的精确语义)**不在本仓** ⇒ 挂跨仓追认债(第 2 项)。

---

## D-ε 6→3 消息类映射表:**恒等与否必须显式登记并携带理由**

**决策**:`MesifAdapter` SHALL 提供 6→3 映射表(6 类一致性消息词表 → 链路层 `CATEGORY_*`),SHALL 完备覆盖本实例用到的消息类,SHALL 与 `adapter_schema_contracts.hh` 的 `identity_reference_map()` 对拍,SHALL **显式登记**恒等与否并**携带理由**(per 队列 #6 delta spec 先例:`moesi_gpu_adapter.hh:173-178` 的 `kMappingIsIdentity` + `kIdentityReason`)。

**理由**:`mesif` 的 `forward_data` 按 `ADR-CEP-12 §8.3` 补充约定是 **`snoop` 槽的承载实例**(`forward_data` 不在 6 类词表里)⇒ 映射**是否恒等本身是设计决策**,不能默认。

**落点**:`mesif_adapter.hh` 的 `kMessageClassMap` + `kMappingIsIdentity` + `kIdentityReason`;判据 AC-8。

---

## D-ζ 判据口径边界:**不**锁语义正确性

**决策**:本 change 的判据 SHALL 锁**完备性 / 可审计性 / 待追认标记 / 内部自洽**,SHALL **不**锁各格**目标态**的**语义正确性**。

**理由**:与规约层 `D-ζ` 同口径 —— 追认闭合后 `pending` 归零,「未确认」的表达责任**移交**给残余风险登记(R-1 / R-3 / A)。判据**不**能声称它没验证过的东西。

**例外**:3 个**锚点格**(`I → NONE` / `E + Wr → M` / `S + Wr → S`)是**自洽性不变量**的支点 ⇒ 判据**锁其自洽性**(非语义正确性)。

---

## D-η 「因正确原因红」的**机器判定**机制

**问题**:既有脚本 `cep_mesif_state_spec_reverse_verify.sh:94` 把「编译失败」与「运行期谓词失败」**合并计数**（「两者都算被抓到」）⇒ 「正确原因」**没有机器区分**;而判据纪律第 3 条要求「编译期锁不得只以『编译红』充当 ctest 判据」。

**决策**:新脚本 `cep_mesif_adapter_reverse_verify.sh` SHALL:
1. 每条 fixture **声明期望失败模式**(`compile-lock` / `runtime-predicate`);
2. probe SHALL 输出**失败判据 id** ⇒ 脚本 SHALL **grep 断言「红在预期的判据上」**,而非仅断言「红了」;
3. **SHALL NOT** 沿用既有脚本的「编译 / 运行合并计数」口径 ⇒ **差异须在本 design 显式说明**(即本条);
4. 若 fixture 毒化的是**规约载体**,规约层判据与实现层判据会**同时**红 ⇒ probe SHALL 把**两层判据 id 分开打印** ⇒ 归因不模糊;
5. 既有套件 SHALL **零回归**(仍 11/11)⇒ 防 fixture 空转(per Entry #103 ③)。

**落点**:`scripts/test/cep_mesif_adapter_reverse_verify.sh`;判据 AC-13。

---

## D-θ `no_upgrade_instances` 的负控见证 ⇒ **双向规则化**

**问题**:`no_upgrade_instances` 是**派生**量(`adapter_contracts_loader.hh:213-224` = instances − 已声明)。加 `mesif` 后**无任何实例**落在其中 ⇒ 原断言 `size() == 1` 且 `[0] == "mesif"` 变成**恒真空转**(无鉴别力)⇒ 违判据纪律第 1 条。

**决策**:负控见证 SHALL 从「事实断言」改造为**规则级双向判据**:
- **规则**:**有声明 ⇒ 不在 `no_upgrade_instances`;无声明 ⇒ 在**;
- **正控**:真实数据(3 实例全有声明 ⇒ 集合为空);
- **反控**(可执行反向 fixture):传入**合成输入**(把 `mesif` 从 `silent_upgrades` 移除)⇒ 断言 `mesif` **必然**出现在集合里 ⇒ 派生逻辑坏了即红。

**形态先例**:与规约层 `deviations_are_consistent(t, list)` 的**可变异形态**(`mesif_state_spec.hh:638`)同款。

**附带**:原「`mesif` 注入 ⇒ 红」的毒化形态**已作废**(`mesif` 变合法)⇒ 必须换新毒化形态,且脚本须**响亮报红防空转**(per Entry #103 同类先例)。

**落点**:`test/test_cep_adapter_schema_contracts.cc`;判据 AC-5。

---

## D-ι 收尾批内序:**冻结面重算 → 归档 → KPI 回填**

**决策**:归档批的批内序 SHALL 写死为:

| 序 | 动作 |
|---|------|
| ① | `mesif.status` → `implemented` + `INV-CEP-12-IC3` 计数同步(**非**冻结面) |
| ② | `07 号` 漂移修正 10 处 + 新 `## Status Update` 第十五则(**冻结面**) |
| ③ | commit 冻结面产物 |
| ④ | `cep_freeze_check.sh --update --authorized-by "Entry #N"` |
| ⑤ | commit 清单 |
| ⑥ | `openspec archive cpptlm-cep-mesif-adapter`(**完成归档**语义 ⇒ delta spec 合并进 `openspec/specs/`) |
| ⑦ | KPI / 状态列回填(`obj §4.0` / `§5.2` / `phase-10.md` / `.rddf/roadmap.md` / `AGENTS.md`) |

**理由**:① `INV-CEP-12-IC3`(`status` 变更 ⇒ 计数同步)**无机器闸** ⇒ 翻转与计数同步必须**同批**,否则出现「JSON 说 `implemented`、`07 号` 说 2 个」的**无闸窗口**;② **KPI 只能在归档之后改**(先例 Entry #95 / #101)—— 归档前改 KPI 会与实际并发数矛盾。

**冻结面改动面**:**仅** `07 号` 一件 ⇒ **一次**两步授权。

### `07 号` 漂移普查清单(10 处活口径)

| 行 | 现状 | 目标 |
|---|------|------|
| `:75` | 「SharerTracker 5 实现 + caps descriptor(§5.2)」 | → **6** 实现 |
| `:86` | 「SharerTracker 5 实现 + caps descriptor(token 归属见 §5.1.3)」 | → **6** 实现 |
| `:276` | 「IC3:5 个条目生命周期事件名 + 时序契约在 5 种实现一致」 | → **6** 种实现 |
| `:278` | 「IC6:5 种实现 hazard 行为契约 `Hazard_NONE`/`Hazard_STALL`/`Hazard_CHAIN` 字面值收敛」 | → **6** 种实现 |
| `:323` | §5.1.1 MESIF 行状态列「📋 已排期」+「实现层 change 待建」 | → **✅ 已落地** + 落点索引 |
| `:330` | §5.1.1 状态注「`mesif`…**余 2 项缺口**未闭 ⇒ 排期 ≠ 就绪」+「`chi_rnf` = 第 **2** 个」 | → 缺口**已闭** + `mesif` = 第 **3** 个 |
| `:573` | 「`ADR-CEP-04 §2` SharerTracker 5 实现」 | → **6** 实现 |
| `:679` | §5.2「SharerTracker 5 实现 \| 🟡 **1 / 5**」+ 行号引用 `:178-195` | → 「**6** 实现 \| 🟡 **2 / 6**」+ 行号刷新 |
| `:681` | §5.2「ProtocolEngine adapter 实例 \| 🟡 **1 个已落地**」 | → **3 个**(+ `mesif` 落点) |
| `:721` | Status Update 活口径段「SharerTracker 5 实现(**1/5**)· ProtocolEngine adapter 实例(**1 个**)」 | → 同步 |

> ⚠️ **历史 Status Update 块不可改**(append-only):`:701` / `:702`(2026-10-25 块)、`:905`(第十二则「余 2 项缺口 ⇒ 队列 #9 仍不可启动」)⇒ 由**新的 Status Update 第十五则**覆盖,**不得回改**。
> ⚠️ **invariant 计数不得变**:改 `:276` / `:278` 的「5 种实现」措辞**不改变** invariant 条数(66 / 编号位 67 / 15 组)⇒ 改后 `cep_docs_sync_check.sh` **必须** rc=0。

---

## D-κ S1-3 的**计数口径** = `implemented` 实例数 ≥ 3

**决策**:`S1-3` 的判据 SHALL 读作「**`implemented`** 实例数 ≥ 3」(**不是**「声明实例数 ≥ 3」—— `protocol_instances.json` 今日**已有 3 个声明实例**,`mesif.status` 仍是 `planned`)。

**触发证据三件套**:`mesif.status == "implemented"` + `obj §4.0` 队列 #9 行改 + 台账 Entry。

**真实义务**:`ADR-CEP-12 §8.4` 登记的 **SLICC 式「协议本地虚拟网络 + 装配期绑定」备选重评** —— 该备选的登记理由是「当前为 0 个 `implemented` 实例 —— 这是将来最便宜的改点,**别让这个选项无声死掉**」⇒ 归档后必须出结论(维持嵌套形态 / 迁移 SLICC 式)。

**承接项 2** = **滚动复核的第 2 次**(节奏源 umbrella design D4.5);`obj §4.0` 队列 #6 行已标「✅ 本 change 已交付(2026-10-27 归档)」⇒ 本次**不得**与该行形成台账矛盾。
