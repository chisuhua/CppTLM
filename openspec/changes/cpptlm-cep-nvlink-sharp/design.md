# design — cpptlm-cep-nvlink-sharp(实现层)

> **口径**:本 design 是**实现层**的实施指导(how);**当前态**与**架构意图**在
> `docs/designs/` 与 `docs/adr/ADR-CEP-0{1,2,3}`(真源 3)·**不重推语义**,只把裁决**机器化**。
> **唯一真源链**:`ADR-CEP-03`(规约语义)→ `nvlink_sharp_state_spec.hh`(**已归档**规约载体)→ 本层载体 → 判据。

---

## §1 交付面与载体形态决定

### §1.1 复用的既有裁决(**不重开**)

| # | 裁决 | 登记处 | 本层怎么用 |
|---|---|---|---|
| ① | `NvlinkAdapter` = **独立头** + **委托** state spec + 零自有表副本 + **无 `transition_*`** | `ADR-CEP-03` SU 2026-11-03 E.1 / F.4 | §2 |
| ② | `sharer_tracker.impl` = **`BroadcastNull`** + 本层补行为类 | 用户裁决 2026-11-03 / `ADR-CEP-03` F.4 | §3 |
| ③ | `perm` **维持 4 值锁**;NVLink 实例的**具体子集 = pending**(锚点 `N-8`) | Entry #177 / 锚点摘要 `N-8` | §4 |
| ④ | atomic = **`request` 流的独立属性**(`atomic.supported` + `atomic.intent_field`);**协议侧 completion 配对 = pending**(锚点 `N-9`) | Entry #177 / 锚点摘要 `N-9` | §4 |
| ⑤ | **VC 空间 = Decision D**(显式声明门)⇒ 本拓扑 `unavailable`;**不得占用承载 coherence 的 VC 与传输通道**;**能力域 = 节点内**不受此限 | `ADR-CEP-01` SU 2026-11-03(#1) D.2/D.4 + (#2) G.1 / Entry #171 | §5 |
| ⑥ | **reduce 族 = 复用既有 `ReduceOp` 恰 6 值**;载体**不**定义算子族 | `ADR-CEP-03` SU(#2) F.1-b / E.2 | §2.4 |

### §1.2 本层**新**定的 6 项形态决定

| # | 决定项 | 结论 | 依据 |
|---|---|---|---|
| **N-1** | `BroadcastNull` 行为类的**落点** | **独立头** `include/tlm/cep/nvlink_broadcast_null_tracker.hh` | 同队列 #12 的 `cxl_dcoh_tracker.hh` 先例(协议族行为类**不**并入协议无关的 `sharer_tracker.hh` ⇒ 避免**通用层反向依赖协议族**) |
| **N-2** | `BroadcastNull` 的**可观测行为** | `lookup` **恒** `BROADCAST` + `view.valid = false` + `hazard = NONE`;`set_sharer` / `note_sharer_dropped` / `clear_sharer` = **显式 no-op**;snoop 三方法走 `SnoopMechanics` 公共机械 | `scope=NONE` ⇒ 本 tracker **不覆盖任何地址**;`authority=NONE` ⇒ **不给**任何本地结论;`completeness=EXACT(degenerate)` ⇒ 「永不需要关心」;`hazard=NONE` ⇒ 无状态、不需等待(per `ADR-CEP-04 §2` / `§4` 表) |
| **N-3** | `nvlink_4` 的**网络绑定** | `vc_binding` / `ordering_assumption` 绑 **`compute_noc`**(与 `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` 同形) | 该实例的流量是**coherence** 流量 ⇒ Decision D **不**禁止(coherence 占满 0-3 是**既有**事实);被禁的是**集合流量** ⇒ `collective_vc_reservation` 保持 `[]` |
| **N-4** | `nvlink_4` 的 **collective 流量落点** | **节点内本地模型**(`LocalCollectivePlane`)⇒ **不上 fabric** | `ADR-CEP-01` SU(#2) G.1(能力域 = 节点内**不受**传输平面约束)+ Decision D(本拓扑无隔离 VC)⇒ 二者交集 = 只允许本地模型 |
| **N-5** | `collective_planes.json` 的 `registered_regions` 归因 | 增 **1 条**(`mode = nc_drop_cacheable`) | `ADR-CEP-03 §3.1` R3(同一地址不得同时「SHARP 已注册」且「coherence-cacheable」)⇒ 已注册 region **必须**走 non-cacheable 降级 ⇒ `mode` 只能取 `nc_drop_cacheable`;⚠️ schema 的 region 项**无实例归属键**(4 键固定)⇒ 本仓 collective plane 是**单例**模型,归属靠 `plane_binding` 表达 ⇒ **登记为已知局限**(§5.3) |
| **N-6** | `atomic.intent_field` 是否填 | **省略**(只填 `supported: true`) | schema 的 `atomic.required` = `["supported"]` ⇒ 可省;而**具体字段名**在**公开非规范来源**里**没有** ⇒ 填任何名字都是**推理填空**(违「不得推理填空」纪律)⇒ 省略 + 登记 pending(锚点 `N-9` 同族) |

---

## §2 `nvlink_adapter.hh`(载体 ①)

### §2.1 定位

**不是缓存、不是网络组件**:不接 `connections[]`、不进 `ModuleFactory`、不注册 23 ABI。
它是 **NVLink SHARP 语义的实现层载体**,**只消费**已归档的规约载体 `nvlink_sharp_state_spec.hh`。

### §2.2 ⭐ 零自有表副本(禁词表)

本文件的**去注释后**文本 SHALL **不**含下列**成员字面量**:
`IngressDecision::` · `RegisterResult::` · `EpochState::` · `OrderingRule::` ·
`VcReservationState::` · `CollectiveScope::` · `Basis::` · `kBasis*` · `kPendingSemanticItems`

⇒ 语法上**不可能**携带自己的状态机 / 投影表 / basis 表 ⇒ 只能**调访问器**。
**类型名**(如 `using IngressDecision = nvlink_sharp::IngressDecision;`)**允许**(与
`CxlDcohAdapter` 的 `using DevCacheState = cxl_dcoh::DevCacheState;` 同构)。

### §2.3 ⛔ 无 `transition_*`

规约载体**没有**转移表(SHARP 转移语义在**公开非规范来源**里**没有** ⇒ 转移表会要求**推理填空**)
⇒ 本类 SHALL **NOT** 提供任何 `transition_*` / `is_silent_upgrade` 之类**转移判定** API。

### §2.4 本类提供的判据面

| API | 义务 | 归因 id |
|---|---|---|
| `classify_at_fabric_ingress(...)`(委托) | reduce / coherence 分流 | `CP-OPCODE-CLASSIFIER`(复用,规约层已立) |
| **`fabric_ingress_classify(...)`** | **V3** barrier 分类 litmus **双向**:**地址 key ⇒ Ordering** / **组 key ⇒ Collective** | `CP-NVLINK-BARRIER-CLASS`(本层新建) |
| `reduce_op_count()`(**运行期断言**) | 「算子族**恰 6** 值」(断言**既有** `tlm::cep::ReduceOp` 的基数,**不**定义算子族) | `CP-NVLINK-REDUCEOP-SET`(本层新建) |

⚠️ **V3 与规约层 `NSS-01` 不是同一判据**(per `ADR-CEP-03` SU(#2) F.5):
`NSS-01` = reduce / coherence 分流;**V3** = barrier 分类(地址 key vs 组 key)⇒ **两个独立 id**。

### §2.5 边界

· **header-only** · **零自有表副本** · **不暴露**区域表类型(`collective_region_table` **零命中**)
· **不改** `nvlink_sharp_state_spec.hh`(只读消费)· **不改** `sharer_tracker.hh`(接口面零改动)
· **0 新增** ABI · **0 新增** `INV-` 编号(15 组 66 条编号是 `test_cep_canonical_invariants.cc::expected_groups()` 的硬编码)

---

## §3 `nvlink_broadcast_null_tracker.hh`(载体 ②)

### §3.1 定位

本类**兑现** `ADR-CEP-04 §2` caps 表已登记、但**行为面一直空缺**的 `BroadcastNull` 行
⇒ 闭合 07 号「`BroadcastNull` 仅声明层 traits」缺口(per Entry #112 的「无消费者 ⇒ 不排期」登记 —— **本层成为其消费者**)。

### §3.2 ⭐ caps / behavior **逐字段取自 traits**(不另写 caps 字面量)

```cpp
static_assert(kTrackerTraits[4].impl == TrackerImpl::BroadcastNull, "traits 第 4 行 SHALL 是 BroadcastNull");
// caps = kTrackerTraits[4].caps;  behavior = kTrackerTraits[4].behavior;   ← 运行时拷贝,零字面量
```

| 字段 | traits 值(`kTrackerTraits[4]`) | 本类的**可观测**行为 |
|---|---|---|
| `scope` | `NONE` | **不覆盖任何地址** ⇒ 安装面(`set_sharer` 等)一律 **no-op** |
| `completeness` | `EXACT` (degenerate) | 「永不需要关心」⇒ **不**保守多探 |
| `authority` | `NONE` | **不给**任何本地结论 ⇒ `view.valid = false` + `result = BROADCAST` |
| `aggregation_at` | `NONE` | **无**聚合点 |
| `hazard` | `NONE` | 无状态、不需等待 ⇒ `LookupHazard::NONE` |
| `broadcast_fallback` | `true` | 恒广播兜底 |

### §3.3 ⭐ 抓取点 1(调用边恒为零)的**行为侧兑现**

`ADR-CEP-01 §3.3` 抓取点 1 要求 Collective Plane → Directory 的调用边**必须为零**;
`scope=NONE` + `authority=NONE` ⇒ 本 tracker **从不**给出本地 sharer 视图 ⇒ **零目录边**。
⇒ 本类**不得**持有任何目录 / sharer 状态的**副本**(状态容器归协议引擎侧)。

### §3.4 边界

· **header-only** · **不进** `ModuleFactory` · **不注册** ABI
· **不改** `sharer_tracker.hh`(接口面零改动)· **不改** `nvlink_sharp_state_spec.hh`
· 本类**不**建模任何 NVLink 特有概念(通道 / opcode / peer set / 错误语义)

---

## §4 `nvlink_4` 实例声明(载体 ③)

### §4.1 13 键逐键取值

| 键 | 取值 | 依据 |
|---|---|---|
| `id` | `nvlink_4` | 07 号 §5.1.1 NVLink 行的实例位;schema `pattern` `^[a-z0-9][a-z0-9_-]*$` |
| `family` | `industrial` | 07 号 §5.1 口径(族 = 来源集合);schema `enum` |
| `adapter` | `NvlinkAdapter` | §2 的载体名 |
| `status` | `implemented` | schema 注释「**仅** `implemented` 计入 adapter 实例数」⇒ 本层交付后即 `implemented` |
| `protocol_version` | `nvlink-4.0` | `ADR-CEP-11 §3` 锁表(NVLink 4.0 = TL Read/Write/Atomic/Probe;NVSwitch SHARP) |
| `flow_support` | `request` / `completion` / `snoop` / `snoop_resp` / `writeback` / `barrier` = **true**;`forward_data` / `back_invalidate` / `address_resolve` / `snoop_completion_ack` = **false** | TL Read/Write/Atomic/Probe ⇒ request + completion + writeback;Probe ⇒ snoop + snoop_resp;SHARP barrier ⇒ `barrier`(per `ADR-CEP-01` 决策 3「barrier 重钉为 Ordering Point ⇒ **零**流数变更」⇒ 复用既有 10 流的 `barrier` 位,**不**新增第 11 条);`forward_data` / `back_invalidate` / `address_resolve` 是 CXL/host 侧语义 ⇒ false |
| `perm` | `["NONE","READ","UNIQUE","WRITEBACK"]`(4 值锁全量) | 值域锁 4 值(per 裁决 ③);⚠️ **具体子集 pending**(锚点 `N-8`)⇒ 声明**值域**而非**子集**,**不**照抄 `tilelink_c` 的 3 值 |
| `atomic` | `{"supported": true}`(**省略** `intent_field`) | 裁决 ④ + N-6(字段名无公开来源 ⇒ 省略 + 登记 pending) |
| `consistency_labels` | `read = SC`,`write = SC` | NVLink = 一致性互联(per 07 号 §5.1.1 工业族)⇒ SC(与 `chi_rnf` / `cxl_*` 同) |
| `ordering_assumption` | `{"compute_noc": {"0":"unordered","1":"p2p_ordered","2":"unordered","3":"p2p_ordered"}}` | 与 `chi_rnf` / `cxl_*` 同形(N-3) |
| `vc_binding` | `{"compute_noc": {"request":0,"response":1,"snoop":2,"snoop_resp":3}}` | N-3(coherence 流量 ⇒ **不**受 Decision D 约束) |
| `sharer_tracker` | `{"impl": "BroadcastNull", "caps": {scope:NONE, completeness:EXACT, authority:NONE, aggregation_at:NONE}}` | 裁决 ② + `kTrackerTraits[4]`(caps **逐字段**取自 traits,**不**另写字面量) |
| `combination_capabilities` | `concurrent` / `hierarchical` / `heterogeneous` = **true** | 与既有 6 实例同形 |

⇒ **`repo_owned_instance_ids` +1**(`nvlink_4`)—— 本仓自持(非 ArchForge 派生)⇒ 与 `chi_rnf` /
`cxl_2_0` / `tilelink_c` / `cxl_3_0_symmetric` 同列。

### §4.2 ⚠️ 装配期校验(消费本声明的**唯一**读者)

`protocol_instance.schema.json` 的 `required` **实读** ⇒ 13 键缺一 ⇒ 判红;
`sharer_tracker.impl` 必须落在 schema 的 `enum` 内 ⇒ `BroadcastNull` 在枚举内 ✅;
`caps` 4 项必须与 `kTrackerTraits[4]` **逐字段相等** ⇒ **禁止**声明与 traits 漂移。

---

## §5 集体面(载体 ④⑤)

### §5.1 `collective_planes.json` 的三块 + 第四键

| 块 | 本层动作 |
|---|---|
| `plane_binding` | 加 `nvlink_4` → **`"LocalCollectivePlane"`**(N-4:节点内本地模型)⇒ 集体面**首次被消费**(原两张全 `null`) |
| `registered_regions` | 增 **1 条**(`lo`/`hi`/`group`/`mode = nc_drop_cacheable`)(N-5)⇒ `registered_regions` 由**空**变**非空** |
| `collective_vc_reservation` | **保持 `[]`**(本拓扑无隔离 VC ⇒ Decision D) |
| `vc_reservation_declaration` | **四字段已在**(Entry #171 · `2ea09607`)⇒ 本层**不改**内容,只**消费**(`CP-03` 判据) |

### §5.2 V6-c 前提约束(载体 ⑤ · 归因 `CP-03-VC-PRECONDITION`)

**判据**:`vc_reservation_declaration.status == "unavailable"` ⇒ 拓扑中**不得**存在**真实 fabric 集合流量载体**
(仅本地模型)⇒ `plane_binding` 的每个绑定值 SHALL 落在**本地模型白名单**内
(当前 = `{"LocalCollectivePlane"}`)⇒ 否则 **FAIL**。

⚠️ **白名单是**枚举式**的**(不是「非 fabric 即合法」的**否定式**):白名单外的绑定名 ⇒ **FAIL**
⇒ 新增一种 plane 实现时**必须**同时更新白名单 ⇒ **不**会「忘了更新就静默通过」。

### §5.3 CP-05 扩展(载体 ⑤ · 归因 `CP-05` **复用**)

| 扩展 | 内容 | 依据 |
|---|---|---|
| **标识清单** | 把 `collective_region_table` 加入 `kGroupScoped[]`(coherence 侧**不得**出现) | `ADR-CEP-03` SU(#2) F.4「coherence 侧扫描集合扩展 + 标识清单补 `collective_region_table`」+ **复用** `CP-05`(不新造 id) |
| **扫描集合** | coherence 侧扫描从**单文件**扩为**多文件**(把 §2 新增的 adapter 头纳入扫描面) | 同上;⚠️ adapter 头**零** `collective_region_table` 命中 ⇒ 正控天然成立 |

⚠️ **已知局限(登记)**:schema 的 `registered_regions` 项**无实例归属键**(4 键固定)
⇒ 本仓 collective plane 是**单例**模型,归属靠 `plane_binding` 表达 ⇒ **多实例**归属需 schema 扩展
⇒ 本层**不改** schema(Out of Scope)⇒ 登记为 backlog。

### §5.4 ⚠️ `cep-collective-plane-contract` main spec 的 delta(⚠️ **该 spec 在 M1 冻结集内**)

已冻结 main spec 写「声明最小集 SHALL **恰含三组**」+「**6 条**可机器检查的规则」
⇒ 本层加了**第四键** + **第 7 条**规则 ⇒ 若不同步 ⇒ **契约漂移**。
⇒ 本 change 附**一份** delta(第四键 + 「空预留缺声明 ⇒ FAIL」场景 + V6-c 场景 + CP-05 扩展场景)
⇒ 归档时**必须**走**冻结面两步授权**(`cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ commit ⇒ `--update` ⇒ commit)。
⇒ 该 delta 同时闭合台账 §10 的 **Entry #174 ③ S1**(「下一个 openspec change 补 delta」)。

---

## §6 判据面(8 组反向 fixture + 正控)

| 组 | 判据 | 归因 id | 红形态 | 正控 |
|---|---|---|---|---|
| **F-1** | `NvlinkAdapter` 零自有表副本(禁词表扫描) | `CP-NVLINK-ZERO-COPY` | 运行期(源扫描) | 干净载体 ⇒ 零命中 |
| **F-2** | 无 `transition_*`(禁词表扫描) | `CP-NVLINK-ZERO-COPY` | 运行期(源扫描) | 干净载体 ⇒ 零命中 |
| **F-3** | `BroadcastNull` 的 caps **逐字段取自 `kTrackerTraits[4]`** + `scope=NONE` ⇒ 恒 `BROADCAST` + 零目录边 | `CP-NVLINK-BROADCAST-NULL` | 运行期 | 正控:lookup ⇒ `BROADCAST` 且 `view.valid == false` |
| **F-4** | `nvlink_4` 13 键齐备(schema `required` 实读)+ `caps` 与 traits 逐字段相等 | `CP-NVLINK-INSTANCE` | 运行期 | 正控:真实声明文件 ⇒ PASS |
| **F-5** | **V3** barrier 分类 litmus **双向** | `CP-NVLINK-BARRIER-CLASS` | 运行期 | 正控:地址 key ⇒ Ordering / 组 key ⇒ Collective 各一例 |
| **F-6** | 「算子族**恰 6** 值」运行期断言 | `CP-NVLINK-REDUCEOP-SET` | 运行期 | 正控:`ReduceOp` 计数 == 6 |
| **F-7** | **partial failure** 显式建模(`CollectiveEvt::Fault` 被**真实消费**) | `CP-NVLINK-PARTIAL-FAIL` | 运行期 | 正控:成员掉线 ⇒ 产出 `Fault` 事件 |
| **F-8** | **V6-c** 前提约束(`unavailable` ⇒ 仅本地模型) | `CP-03-VC-PRECONDITION` | 运行期 | 正控:当前绑定 ⇒ PASS |
| **F-9** | CP-05 标识清单扩展(`collective_region_table` 零命中)+ 扫描集合扩展 | `CP-05` | 运行期 | 正控:coherence 侧文件 ⇒ PASS |

⇒ **9 组**(其中 F-1 / F-2 共用 `CP-NVLINK-ZERO-COPY`)⇒ improvement 草稿原写「5 组」
⇒ **本层订正为 9 组**(每条义务各有**因正确原因红**的反向 fixture;铁律 ②)。

⚠️ **两类红形态都要支持**(照队列 #13 规约层的探针先例):
① **编译期红** ⇒ `static_assert` 文案带 `<归因 id>:`;② **运行期红** ⇒ `PROBE:FAIL <归因 id>`。
⇒ **编译期锁不得只以「编译红」充当 ctest 判据**(铁律 ③)⇒ 每条 `static_assert` 另配一条运行期判据。

---

## §7 归因 id 登记(**唯一**登记处)

| 义务 | **唯一归因 id** | 状态 |
|---|---|---|
| R1:opcode 永不作分类器 | `CP-OPCODE-CLASSIFIER` | 复用(规约层已立) |
| 源端不得持有区域表可见性 | `CP-05` | **复用**(F.9 扩展) |
| R4 epoch 语义双向 | `CP-EPOCH-SEMANTICS` | 复用 |
| R3 drain 失败不建 epoch | `CP-DRAIN-FAIL` | 复用 |
| R5 divert 计数 + fail-stop | `CP-DIVERT-STOP` | 复用 |
| collective VC 预留状态 | `CP-03` | 复用 |
| `V6-c` 前提约束 | `CP-03-VC-PRECONDITION` | **复用**(`ADR-CEP-01` SU 已印出) |
| adapter 零自有表副本 / 无 `transition_*` | **`CP-NVLINK-ZERO-COPY`** | **本层新建** |
| `BroadcastNull` 行为实现 | **`CP-NVLINK-BROADCAST-NULL`** | **本层新建** |
| `nvlink_4` 实例声明 | **`CP-NVLINK-INSTANCE`** | **本层新建** |
| V3 barrier 分类 litmus | **`CP-NVLINK-BARRIER-CLASS`** | **本层新建**(per `ADR-CEP-01` SU G.4「V3 的归因 id 待实现层建立」) |
| 算子族恰 6 值 | **`CP-NVLINK-REDUCEOP-SET`** | **本层新建**(per `ADR-CEP-03` SU F.1-d) |
| partial failure 显式建模 | **`CP-NVLINK-PARTIAL-FAIL`** | **本层新建**(per `ADR-CEP-03` SU F.3「id 待实现层建立」) |
| collective 绑定 + region 声明 | **`CP-NVLINK-COLLECTIVE-BINDING`** | **本层新建** |

⇒ **同一义务不得有两个 id**;新 id **一律** `CP-<域>-<名>` 形态(per `ADR-CEP-01` 附录「禁在本仓新造**平行** id 体系」)。

---

## §8 pending 边界(9 项 ⇒ 本层纪律)

规约层的 **9 项** `pending`(锚点 `N-1` ~ `N-9`)**不得**被本层推理填空:
reduce 算子穷举集 · 逐操作支持矩阵 · epoch 状态机细节 · VC 编号 · 聚合树构建算法 ·
报文格式 / opcode 布局 · 数值 / 精度语义 · NVLink 实例的 `perm` 具体子集 · NVLink atomic 的协议侧 completion 配对。

⇒ 本层**只**实现**已定稿**的结构性语义;凡落到上述 9 项 ⇒ **一律**留 `pending` + 登记锚点。

---

## §9 文档同步面 + 冻结面授权面

| 面 | 动作 | 时点 |
|---|---|---|
| 07 号 §5.1.1 NVLink 行 | 状态列 → **✅ 已落地**(队列 #13,归档日) | 归档时 |
| 07 号 §11 两行 | SharerTracker `5/6` → **`6/6`**;adapter `6 个` → **`7 个`** | 归档时 |
| 07 号 §5.1.1「6 族 / 8 实例」 | **计数不变**(NVLink 行本就计入 8 实例)⇒ 只核口径 | 归档时 |
| `include/tlm/cep/collective_plane.hh:206` | **失指 change 名** `cpptlm-cep-nvlink-sharp-adapter` → 指向本 change | 本层(代码面) |
| `openspec/specs/cep-collective-plane-base/spec.md` | 同名残余 ⇒ **M1 冻结面** ⇒ 需**两步授权** | 归档时(单独立项或本层一并) |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | 同名残余 ⇒ **真源 3 治理 ADR** ⇒ 属 **architect** 域 ⇒ **不在本层** | 登记 backlog |
| `openspec/specs/cep-collective-plane-contract/spec.md` | **第四键 + 新规则** ⇒ **M1 冻结面** ⇒ **两步授权** | 归档时 |
| `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` | 反向 fixture 组数 **5 → 9**;状态行 → 已启动 | 本层 |

---

## §10 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| **公开来源受限** ⇒ 9 项 pending | 🟡 中 | 本层只实现结构性语义;逐格 basis 在规约层载体;实现层**不**新增 pending 项 |
| **冻结面归档门槛**:本层归档要改 **2 份** M1 冻结产物(main spec × 2)⇒ 漏授权 ⇒ `cep_freeze_check` 报红 | 🟡 中 | 归档 Phase 显式排**两步授权**(先 `--update --authorized-by` ⇒ commit ⇒ `--update` ⇒ commit) |
| **`registered_regions` 无实例归属键** ⇒ 多实例归属不可表达 | 🟢 低 | 本层单例模型可用;登记 backlog(§5.3) |
| **V3 / partial failure 的载体形态**与既有行为面不同构 ⇒ 判据可能退化为「自证」 | 🟡 中 | F-5 / F-7 的**正控**必须用**真实**路径(不是构造出来的假事件)⇒ 评审重点 |
