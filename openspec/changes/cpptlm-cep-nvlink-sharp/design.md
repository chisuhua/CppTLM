# design — cpptlm-cep-nvlink-sharp(实现层)

> **口径**:本 design 是**实现层**的实施指导(how);**当前态**与**架构意图**在
> `docs/designs/` 与 `docs/adr/ADR-CEP-0{1,2,3}`(真源 3)·**不重推语义**,只把裁决**机器化**。
> **唯一真源链**:`ADR-CEP-03`(规约语义)→ `nvlink_sharp_state_spec.hh`(**已归档**规约载体)→ 本层载体 → 判据。
>
> ⚠️ **本版已按 oracle + metis 双审结论修订**(2026-11-03,台账 Entry #187)。修订点见 §0。

---

## §0 双审修订摘要(2026-11-03)

| # | 审查结论 | 本版处置 |
|---|---|---|
| A-1 | **`fabric_ingress_classify` 名已被占用**(`INV-CEP-04-IC5` = region/epoch divert,per `ADR-CEP-04:301`)+ `collective_plane.hh:206` 的**接线侧**注释也用它 ⇒ V3 不得复用 | V3 改名 **`barrier_key_class`**,落点改**协议中立**的 `collective_plane.hh`(§2.4);`collective_plane.hh:206` 的失指 change 名**只订正名、不改承诺**(§9) |
| A-2 | **V6-c 正控必红**:现 `plane_binding` = `{moesi_gpu: null, mesif: null}` ⇒ `null ∉ 白名单` | 判据只作用于**非空**绑定值;空 `plane_binding` ⇒ **SKIP**(禁 vacuous PASS)(§5.2) |
| A-3 | **`CP-03-VC-PRECONDITION` 无 verdict 通道**(`kContractRuleCount == 6` 被 `test_cep_collective_plane_contract.cc:166` 硬断言) | 不新增第 7 条规则;V6-c 折进 `CP-03` 行,`detail` 以 `[CP-03-VC-PRECONDITION]` 开头;探针**单独**输出精确相等的 id(§5.2) |
| A-4 | **冻结面被低估**:07 号在 `FROZEN_GLOBS:32` ⇒ 实为 **3 份 + manifest** | `tasks 0.2` / `§9` / `§10` 全部订正为 3 份(§9) |
| A-5 | **F-3 天然恒真**:`SharerTrackerCaps{}` 默认值与 traits 第 4 行**逐字段相同** ⇒ 从不赋值也 100% 通过 | 正控改为**毒化 traits 第 4 行**(影子副本)⇒ 类的可见 caps **必须跟随**(§3.3 / §6 F-3) |
| A-6 | **F-6 恒真**:`ReduceOp` 无 count 表 + C++17 无法反射枚举基数 ⇒ 「追加第 7 个值」计数不变 | 判据改为**去注释后源文本枚举项计数**;毒化形态 = 在 `BitXor` 之后**追加** `Prod = 6`(§2.5 / §6 F-6) |
| A-7 | **F-7 的触发源不存在**:平台**无** member-leave API | 正控改为既有真实路径:`collective_barrier` 至 N-1 后 `group_destroy` ⇒ 对已到达成员发 `Fault`(§6 F-7) |
| A-8 | **CP-05 扫描集合语义反了**:adapter 是 collective 侧,不是 coherence 侧 | CP-05 拆**两个 profile**:① coherence 侧 × group-scoped 清单(补 `collective_region_table`)② **coherence/源端侧**文件扩展(可注入);adapter 的「不暴露区域表」由 NVA-01 独立承担(§5.3) |
| A-9 | **「装配期校验 = 唯一读者」是事实错误**(既有读者 ≥ 6 个) | 改为**扩展**既有 `load_adapter_contract_inputs` 校验项 + 复用 `run_ordering_gate` / `run_channel_gate`;⛔ **不新增第二套装载器**(§4.2) |
| A-10 | **归因 id 通胀**:5 条自设机制判据被升成 `CP-*` | 只保留 ADR 强制的 **2 个**新建 `CP-*`(V3 / V5);其余 5 条用 **Scenario id**(`NVA-xx`)作归因键(照 #12 先例)⇒ id 集合**双向可核对**(§6 / §7) |
| A-11 | **fixture 组数四套数字**(5 / 8 / 9 / 14) | 全仓统一为「**9 组** fixture,1:1 对应 NVA-01 ~ NVA-09;归因 = **2** 个新建 `CP-*` + **7** 个 `NVA-xx`;**复用** id 由既有判据承担,本层**不**重复立 fixture」(§6) |
| A-12 | **台账 `Entry #186 ⑥` 的「归约行为实现」措辞**与本层交付面不符(本层**无**数据路径) | proposal 增「与台账移交口径的差异声明」;台账**追加**订正句(append-only,不改原文) |
| A-13 | **6 处硬编码实例计数**会集体转红 | `tasks` 新增**计数同步清单**(逐条列出文件:行 + 新值 + 断言)(§9) |
| A-14 | `sharer_tracker.hh` 头注释将变假陈述;「接口面零 diff」需**机械定义** | 注释订正 + 接口面 = **4 个纯虚签名 + `kTrackerTraits` 6 行值** 的可判定子集(§2.6 / §9) |
| A-15 | 「只读消费 state spec」**无机器门禁** | 新增 **sha256 钉版断言**(该头**不在** `FROZEN_GLOBS` ⇒ 用钉版补冻结面缺位)(§2.6) |
| A-16 | `collective_planes.json` 的 `current_state` 自述会变假陈述 | 与声明面**同批**改(§5.1) |
| A-17 | `scripts/README.md` 漏登记 + 计数漂移(实测 18 个 vs 写 12 个) | `tasks` 新增子项(§9) |

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
| ⑥ | **reduce 族 = 复用既有 `ReduceOp` 恰 6 值**;载体**不**定义算子族 | `ADR-CEP-03` SU(#2) F.1-b / E.2 | §2.5 |

### §1.2 本层**新**定的 7 项形态决定

| # | 决定项 | 结论 | 依据 |
|---|---|---|---|
| **N-1** | `BroadcastNull` 行为类的**落点 + 命名** | **独立头 `include/tlm/cep/broadcast_null_tracker.hh`** —— **协议无关**(不叫 `nvlink_*`!) | `BroadcastNull` 是 `ADR-CEP-04 §2` 的**第 5 种通用** tracker(非 NVLink 专有)⇒ 挂在 `nvlink_*` 名下 ⇒ 下一个消费者(队列 #14 `ualink_inc`)被迫 include NVLink 头 ⇒ **协议族横向依赖**(与 `cxl_dcoh_tracker.hh:20-27` 想避免的问题同型、换维度);落点同 `cxl_dcoh_tracker.hh` 先例(**依赖**通用层 `sharer_tracker.hh`,**不**反向) |
| **N-2** | `BroadcastNull` 的**可观测行为** | `lookup` **恒** `BROADCAST` + `view.valid = false` + `hazard = NONE`;`set_sharer` / `note_sharer_dropped` / `clear_sharer` = **显式 no-op**;snoop 三方法走 `SnoopMechanics` 公共机械 | `scope=NONE` ⇒ 本 tracker **不覆盖任何地址**;`authority=NONE` ⇒ **不给**任何本地结论;`completeness=EXACT(degenerate)` ⇒ 「永不需要关心」;`hazard=NONE` ⇒ 无状态、不需等待(per `ADR-CEP-04 §2` / `§4` 表) |
| **N-3** | `nvlink_4` 的**网络绑定** | `vc_binding` / `ordering_assumption` 绑 **`compute_noc`**(与 `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` 同形) | 该实例的流量是**coherence** 流量 ⇒ Decision D **不**禁止(coherence 占满 0-3 是**既有**事实);被禁的是**集合流量** ⇒ `collective_vc_reservation` 保持 `[]` |
| **N-4** | `nvlink_4` 的 **collective 流量落点** | **节点内本地模型**(`LocalCollectivePlane`)⇒ **不上 fabric** | `ADR-CEP-01` SU(#2) G.1(能力域 = 节点内**不受**传输平面约束)+ Decision D(本拓扑无隔离 VC)⇒ 二者交集 = 只允许本地模型 |
| **N-5** | `collective_planes.json` 的 `registered_regions` 归因 | 增 **1 条**(`mode = nc_drop_cacheable`)—— **且只**本层引入的这一条(以 `lo`/`hi`/`group` 锚定) | `ADR-CEP-03 §3.1` R3(同一地址不得同时「SHARP 已注册」且「coherence-cacheable」)⇒ 已注册 region **必须**走 non-cacheable 降级 ⇒ `mode` 只能取 `nc_drop_cacheable`;⚠️ schema 的 region 项**无实例归属键**(4 键固定)⇒ 本仓 collective plane 是**单例**模型 ⇒ **登记为已知局限**(§5.3) |
| **N-6** | `atomic.intent_field` 是否填 | **省略**(只填 `supported: true`) | schema 的 `atomic.required` = `["supported"]` ⇒ 可省;而**具体字段名**在**公开非规范来源**里**没有** ⇒ 填任何名字都是**推理填空**(违「不得推理填空」纪律)⇒ 省略 + 登记 pending(锚点 `N-9` 同族) |
| **N-7** | **V3** 的载体落点 + 命名 + 签名 | **协议中立**:落 `include/tlm/cep/collective_plane.hh`;函数名 **`barrier_key_class`**;返回 `enum class BarrierKeyClass : uint8_t { Ordering, Collective }`;输入 = tagged struct `BarrierKey{ kind ∈ {Address,Group}, addr, group }` | ① 名冲突:`fabric_ingress_classify` **已被** `INV-CEP-04-IC5` 占用(`ADR-CEP-04:301` = region/epoch divert)⇒ 不得复用;② 落点:barrier 分类是**协议中立**判读面 ⇒ 放 NVLink 头 ⇒ 队列 #14 被迫依赖 NVLink 头;③ 输入必须是**结构体**(真字段 `addr` / `group`),**不得**用 `enum{Address,Group}` 入参 ⇒ 否则返回值由入参标签直接决定 = **恒等映射 = 恒真**;④ 双重 key / 空 key ⇒ **判红**(不是二选一) |

---

## §2 `nvlink_adapter.hh`(载体 ①)

### §2.1 定位

**不是缓存、不是网络组件**:不接 `connections[]`、不进 `ModuleFactory`、不注册 23 ABI。
它是 **NVLink SHARP 语义的实现层载体**,**只消费**已归档的规约载体 `nvlink_sharp_state_spec.hh`。

### §2.2 ⭐ 零自有表副本(禁词表)

本文件的**去注释后**文本 SHALL **不**含下列**成员字面量 / 表名**:
`IngressDecision::` · `RegisterResult::` · `EpochState::` · `OrderingRule::` · `VcReservationState::` ·
`CollectiveScope::` · `EvidenceKind::` · `SourceId::` · `Basis{` · `kSourceLicenseConfirmed` ·
`all_basis_valid` · `PendingSemanticItem` · `kBasis*` · `kPendingSemanticItems`

⇒ 语法上**不可能**携带自己的状态机 / 投影表 / basis 表 ⇒ 只能**调访问器**。
**类型名**(如 `using IngressDecision = nvlink_sharp::IngressDecision;`)**允许**(不含 `::`,与
`CxlDcohAdapter` 的 `using DevCacheState = cxl_dcoh::DevCacheState;` 同构)。

⚠️ **文本扫描的**已知盲区**:**数值镜像表**(如 `static constexpr uint8_t kMap[12]`)**不在文本扫描面内**
⇒ 由**人工审计**(锚点摘要 §2bis 的 12 格核对)+ 载体侧的 `static_assert` 兜底
(照队列 #12 的「7 成员字面量 + 11 表名」双清单先例)。

### §2.3 ⛔ 无 `transition_*`

规约载体**没有**转移表(SHARP 转移语义在**公开非规范来源**里**没有** ⇒ 转移表会要求**推理填空**)
⇒ 本类 SHALL **NOT** 提供任何 `transition_*` / `is_silent_upgrade` 之类**转移判定** API。

### §2.4 本类提供的判据面

| API | 义务 | 归因 |
|---|---|---|
| `classify_at_fabric_ingress(...)`(委托) | reduce / coherence 分流 | `CP-OPCODE-CLASSIFIER`(**复用**;判据本体在规约层,本层**不**立 fixture) |
| `reduce_op_count()`(**运行期断言**) | 「算子族**恰 6** 值」(断言**既有** `tlm::cep::ReduceOp` 的基数,**不**定义算子族) | `NVA-05`(本层机制判据) |

⚠️ **V3 不在本类**(见 N-7):V3 的载体 = `collective_plane.hh` 的 `barrier_key_class`(协议中立)。

### §2.5 「算子族恰 6 值」的**可判定形态**(A-6 修订)

C++17 **无法**反射枚举基数(规约层脚本 `cep_nvlink_sharp_state_spec_reverse_verify.sh:63-66` 已明文记录该局限)
⇒ 「`static_cast<size_t>(ReduceOp::BitXor) + 1 == 6`」**抓不到**「在 `BitXor` 之后**追加** `Prod = 6`」。

⇒ **判据形态**:**去注释后**扫描 `enum class ReduceOp { … }` 块内的**枚举项个数**
⇒ 毒化形态 = 在 `BitXor` 之后追加 `Prod = 6` ⇒ 计数 **7** ⇒ 必红。
⇒ 正控 = 真实文件计数 **== 6**。

### §2.6 边界与「零改动」的可判定定义

· **header-only** · **零自有表副本** · **不暴露**区域表类型(`collective_region_table` **零命中**)
· **不改** `nvlink_sharp_state_spec.hh`(**只读消费**;该头**不在** `FROZEN_GLOBS` ⇒ 用 **sha256 钉版断言**补冻结面缺位,A-15)
· **不改** `sharer_tracker.hh` 的**接口面** —— **接口面 = 可判定的最小子集**:
  ① `SharerTrackerIf` 的 **4 个纯虚方法签名** ② `kTrackerTraits` 的 **6 行值** ③ `kTrackerTraits` 的**查找函数**;
  ⛔ **注释行**可改(§9 登记 A-14)⇒ 「零 diff」指**代码行 diff == 0**
· **0 新增** ABI · **0 新增** `INV-` 编号(15 组 66 条编号是 `test_cep_canonical_invariants.cc::expected_groups()` 的硬编码)

---

## §3 `broadcast_null_tracker.hh`(载体 ② · **协议无关**)

### §3.1 定位

本类**兑现** `ADR-CEP-04 §2` caps 表已登记、但**行为面一直空缺**的 `BroadcastNull` 行
⇒ 闭合 07 号「`BroadcastNull` 仅声明层 traits」缺口(per Entry #112 的「无消费者 ⇒ 不排期」登记 —— **本层成为其消费者**)。

⚠️ **协议无关**(N-1)⇒ 文件头注明「协议无关;由队列 #13 实现层交付为**首个消费者**」。

### §3.2 caps / behavior **逐字段取自 traits**(不另写 caps 字面量)

```cpp
// ⚠️ 用**查找函数**而非硬编码 [4] ⇒ 行序变化不会静默错位(铁律 ④)
static constexpr const TrackerTraits* traits_of() noexcept {
    return find_tracker_traits(TrackerImpl::BroadcastNull);   // per sharer_tracker.hh
}
static_assert(traits_of() != nullptr, "BroadcastNull traits 行 SHALL 存在");
```

| 字段 | traits 值 | 本类的**可观测**行为 |
|---|---|---|
| `scope` | `NONE` | **不覆盖任何地址** ⇒ 安装面(`set_sharer` 等)一律 **no-op** |
| `completeness` | `EXACT` (degenerate) | 「永不需要关心」⇒ **不**保守多探 |
| `authority` | `NONE` | **不给**任何本地结论 ⇒ `view.valid = false` + `result = BROADCAST` |
| `aggregation_at` | `NONE` | **无**聚合点 |
| `hazard` | `NONE` | 无状态、不需等待 ⇒ `LookupHazard::NONE` |
| `broadcast_fallback` | `true` | 恒广播兜底 |

### §3.3 ⚠️ 「caps 取自 traits」的**鉴别力陷阱**(A-5 修订)

`SharerTrackerCaps{}` 的**默认值** = `{Scope::NONE, Completeness::EXACT, Authority::NONE, AggregationAt::NONE}`
⇒ 与 traits 第 4 行**逐字段相同** ⇒ 一个**从不赋值**的实现也 **100% 通过**「caps == traits」的等值断言
⇒ 该等值断言**无鉴别力**,「把 caps 改成字面量」的毒化也**抓不到**(字面量 == traits 值)。

⇒ **有鉴别力的判据形态**:
① **毒化 traits 第 4 行**(影子副本把 `Completeness` 改成 `CONSERVATIVE`)⇒ 类的**可见 caps 必须跟随**;
   **不跟随** ⇒ 必红(归因 `NVA-02`);
② **出参全断言**:`result` / `view.valid` / `view.sharers` / `view.dirty` / `view.forward_to` / `hazard`
   —— ⚠️ `LocalView` 的默认值恰好符合期望 ⇒ 只断言部分出参会**恒真** ⇒ **全部**断言,且 `result` 是**出参**
   (调用方须先置一个**非期望值**,避免依赖未初始化/默认值)。

### §3.4 「零目录边」的**定义性 vs 可观测**(A-8 同型修订)

`scope=NONE` + `authority=NONE` ⇒ **零目录边** 是**由 traits 字段语义直接推出的定义性结论**,
**不**是可独立观测的事实 ⇒ 判据只断言**可观测**项:
① 5+1 个出参(§3.3 ②)② 本类**不持有任何** `std::map` / `std::vector` 状态容器(源扫描)。

### §3.5 边界

· **header-only** · **不进** `ModuleFactory` · **不注册** ABI
· **不改** `sharer_tracker.hh` 的**接口面**(§2.6 定义)· **不改** `nvlink_sharp_state_spec.hh`
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
| `flow_support` | `request` / `completion` / `snoop` / `snoop_resp` / `writeback` / `barrier` = **true**;`forward_data` / `back_invalidate` / `address_resolve` / `snoop_completion_ack` = **false** | TL Read/Write/Atomic/Probe ⇒ request + completion + writeback;Probe ⇒ snoop + snoop_resp;`barrier` 位依据 = **`ADR-CEP-11:133`**(锁表语义子集含 barrier)且**仅指 requester-scoped 可见性点**(`ADR-CEP-02:48` 流 9)—— ⚠️ **SHARP rendezvous 不走此流**(per `ADR-CEP-02` 决策 3:barrier **不**承载 rendezvous barrier)⇒ **不**新增第 11 条流;`forward_data` / `back_invalidate` / `address_resolve` 是 CXL/host 侧语义 ⇒ false |
| `perm` | `["NONE","READ","UNIQUE","WRITEBACK"]`(4 值锁全量) | ⚠️ **值域 ≠ 支持子集**:schema 描述是「4 值**锁**」;`tilelink_c` 填 3 值(子集)⇒ 本实例的**子集 pending**(`N-8`)⇒ 本层填**值域全集**并**显式**记「本字段填的是**值域**,**不**断言 NVLink 支持全部 4 种」⇒ 判据只锁「⊆ 4 值域」 |
| `atomic` | `{"supported": true}`(**省略** `intent_field`) | 裁决 ④ + N-6 |
| `consistency_labels` | `read = SC`,`write = SC` | ⚠️ **本仓建模假设**:07 号 §5.1.1 未给 NVLink 的 SC 结论(`ADR-CEP-12` 决策 3 的「常规 SWMR ⇒ SC」是**推法**而非 NVLink 结论)⇒ 登记为**建模假设**(backlog) |
| `ordering_assumption` | `{"compute_noc": {"0":"unordered","1":"p2p_ordered","2":"unordered","3":"p2p_ordered"}}` | 与 `chi_rnf` / `cxl_*` 同形(N-3) |
| `vc_binding` | `{"compute_noc": {"request":0,"response":1,"snoop":2,"snoop_resp":3}}` | N-3(coherence 流量 ⇒ **不**受 Decision D 约束) |
| `sharer_tracker` | `{"impl": "BroadcastNull", "caps": {scope:NONE, completeness:EXACT, authority:NONE, aggregation_at:NONE}}` | 裁决 ② + traits 第 4 行(caps **逐字段**取自 traits,**不**另写字面量) |
| `combination_capabilities` | `concurrent` / `hierarchical` / `heterogeneous` = **true** | 与既有 6 实例同形(⚠️ 同为**存量纸面 caps**,`ADR-CEP-04:336` 禁令所指 —— **不是**本 change 引入,登记 backlog) |

⇒ **`repo_owned_instance_ids` +1**(`nvlink_4`)—— 本仓自持(非 ArchForge 派生)⇒ 与 `chi_rnf` /
`cxl_2_0` / `tilelink_c` / `cxl_3_0_symmetric` 同列。

### §4.2 ⚠️ 校验落点 = **扩展既有 loader**(A-9 修订)

⛔ **不得**新增「装配期校验」第二套实现(既有读者 ≥ 6:`load_adapter_contract_inputs` /
`test_cep_adapter_schema_contracts` / `test_cep_moesi_gpu_adapter` / `test_cep_tilelink_instance_decl` /
`test_cep_mesif_instance_decl` / `run_ordering_gate` + `run_channel_gate`)⇒ 「禁双实现」。

⇒ **扩展** `include/tlm/cep/adapter_contracts_loader.hh` 的 `load_adapter_contract_inputs` 校验项:
① schema `required` **实读** 13 键 ② `sharer_tracker.caps` 与 traits **逐字段相等**
③ `repo_owned_instance_ids` 含本实例 ④ `plane_binding` 的键**必须**是已声明实例(声明自洽)。

---

## §5 集体面(载体 ④⑤)

### §5.1 `collective_planes.json` 的三块 + 第四键

| 块 | 本层动作 |
|---|---|
| `plane_binding` | 加 `nvlink_4` → **`"LocalCollectivePlane"`**(N-4:节点内本地模型)⇒ 集体面**首次被消费** |
| `registered_regions` | 增 **1 条**(`lo`/`hi`/`group`/`mode = nc_drop_cacheable`)(N-5)⇒ 由**空**变**非空** |
| `collective_vc_reservation` | **保持 `[]`**(本拓扑无隔离 VC ⇒ Decision D) |
| `vc_reservation_declaration` | **四字段已在**(Entry #171 · `2ea09607`)⇒ 本层**不改**内容,只**消费**(`CP-03` 判据) |

⚠️ **同批改** `derived_from.current_state`:现写「`plane_binding` **全为 null** … `registered_regions` **为空** …」
⇒ 本层落地后**逐条变假** ⇒ 必须同批改(并把「多实例归属不可表达 ⇒ 冻结前置 = 加 `owner_instance`」写进声明源,A-16)。

### §5.2 V6-c 前提约束(载体 ⑤ · 归因 `CP-03-VC-PRECONDITION`)

**判据**:`vc_reservation_declaration.status == "unavailable"` ⇒ 拓扑中**不得**存在**真实 fabric 集合流量载体**
(仅本地模型)⇒ `plane_binding` 的**每个非空**绑定值 SHALL 落在**本地模型白名单**内 ⇒ 否则 **FAIL**。

| 审查点 | 处置 |
|---|---|
| **A-2 空绑定** | loader 把 `null` 物化成**空串**以保留键 ⇒ 判据只作用于**非空**值;`plane_binding` 为**零键** ⇒ **SKIP**(报告出 SKIP 行;**禁** vacuous PASS) |
| **单源白名单** | 白名单**从注册表派生**:`collective_plane.hh` 的两个 plane 类各带 `kIsLocalModel`,契约头遍历**定义点**的注册表 ⇒ 新增实现**必须在定义点**声明局部性;**未注册的绑定名 ⇒ FAIL**(顺带闭合「自由字符串不解析到类」的缺口) |
| **可毒化** | `ContractPaths` 增 `local_plane_whitelist`(可选覆盖)⇒ 反向 fixture 可注入错误白名单 ⇒ 证明判据有牙 |
| **A-3 id 通道** | ⛔ **不新增第 7 条 `ContractRule`**(`kContractRuleCount == 6` 被 `test_cep_collective_plane_contract.cc:166` 硬断言)⇒ V6-c **折进既有 `CP-03` 行**,`detail` 以 **`[CP-03-VC-PRECONDITION]`** 开头;`rule=` 字段**仍为 `CP-03`**;探针**单独**输出**精确相等**的 `PROBE:FAIL CP-03-VC-PRECONDITION`(⛔ **不**用前缀匹配 —— 否则 `CP-03` 会命中 `CP-03-VC-PRECONDITION`) |

⚠️ **本判据只覆盖**声明面(plane_binding)⇒ 一个**未被任何绑定引用**的 fabric plane 实现**不会被发现**
⇒ 「拓扑真相」不在本判据面(登记为局限)。

### §5.3 CP-05 扩展(载体 ⑤ · 归因 `CP-05` **复用** · A-8 修订)

CP-05 的语义是「group-scoped 标识**不得**出现在 **coherence 侧**」⇒ **不得**把 collective 侧的
adapter 头塞进「coherence 侧扫描」(那只会「证明没扫错文件」= **恒真**)。

⇒ CP-05 拆**两个 profile**(id 仍**一个** `CP-05`;`detail` 标 profile 名):

| profile | 面 | 标识清单 | 扫描集合 |
|---|---|---|---|
| **P-a** | coherence 侧 | `kGroupScoped[]`(**补** `collective_region_table`) | 既有单路径 + **新增** `ContractPaths.coherence_side_extra`(vector)注入的**真正 coherence/源端侧**文件 |
| **P-b** | 源端可见性 | `{ collective_region_table }` | adapter 头(由 `NVA-01` 独立承担 ⇒ 本 profile **不重复**) |

⇒ **正控**:同一注入对 `ordering_point_checker.hh` **必红** ⇒ 证明扫描器有牙。

⚠️ **已知局限(登记)**:schema 的 `registered_regions` 项**无实例归属键**(4 键固定)
⇒ 本仓 collective plane 是**单例**模型,归属靠 `plane_binding` 表达 ⇒ **多实例**归属需 schema 扩展
⇒ 本层**不改** schema(Out of Scope)⇒ 登记 backlog(§5.1 已写进声明源)。

### §5.4 ⚠️ `cep-collective-plane-contract` main spec 的 delta(⚠️ **该 spec 在 M1 冻结集内**)

已冻结 main spec 写「声明最小集 SHALL **恰含三组**」⇒ 本层加了**第四键** ⇒ 若不同步 ⇒ **契约漂移**。
⇒ 本 change 附**一份** delta:
① **MODIFIED** 既有 Requirement —— 实质变化**仅 1 个 Scenario**(「声明最小集」三组 → 四组 + 空预留门)
   + 「5 条契约判据」计数订正为 **6**(口径 = `CP-01..CP-06`);
② **ADDED** 新 Requirement `cep-collective-plane-contract-vc-precondition` —— 承载 **V6-c** 与
   **CP-05 扩展**两个 Scenario(⇒ **不再**与既有 Requirement 的 8 个 Scenario 混在一块 ⇒ review 时逐行可比)。

⇒ 「**规则条数不变**」成立:新增的是 **`CP-03` 族**子判据与 **`CP-05` 族**清单扩展 ⇒ 仍是 **6 族**。
⇒ 归档时**必须**走**冻结面两步授权**(§9)。
⇒ 该 delta 同时闭合台账 §10 的 **Entry #174 ③ S1**(「下一个 openspec change 补 delta」)。

---

## §6 判据面(9 组反向 fixture ↔ 9 个 Scenario)

### §6.1 fixture ↔ id ↔ 毒化锚点 ↔ 正控(**四列登记**,A-11 修订)

| 组 | Scenario | 义务 | 归因 | 毒化锚点 | 红形态 | 正控 |
|---|---|---|---|---|---|---|
| **F-1** | `NVA-01` | adapter **零自有表副本** | `NVA-01` | 影子副本注入 `IngressDecision::` 成员访问 | 运行期(源扫描) | 同一扫描器对**未毒化**影子 ⇒ 零命中;对**毒化**影子 ⇒ 命中 |
| **F-2** | `NVA-01` | adapter **无 `transition_*`** | `NVA-01` | 影子副本加 `transition_x()` | 运行期(源扫描) | 同上 |
| **F-3** | `NVA-02` | `BroadcastNull` caps **取自 traits** + 行为 | `NVA-02` | 影子副本**毒化 traits 第 4 行**(`Completeness` ⇒ `CONSERVATIVE`) | 运行期 | 类的可见 caps **必须跟随** traits;不跟随 ⇒ 红(A-5) |
| **F-4** | `NVA-03` | `nvlink_4` **13 键齐备** + caps 与 traits 相等 + `repo_owned` | `NVA-03` | 影子 JSON 删 1 键 / 改 1 caps 值 / 删 `repo_owned` 条目 | 运行期 | 真实声明文件 ⇒ PASS |
| **F-5** | `NVA-04` | **V3** barrier 分类 litmus **双向** | **`CP-BARRIER-CLASS`** | 影子副本把两类**判成同一类** | 运行期 | 地址 key ⇒ Ordering / 组 key ⇒ Collective 各一例;**双重 key / 空 key ⇒ 判红** |
| **F-6** | `NVA-05` | 「算子族**恰 6** 值」 | `NVA-05` | 影子副本在 `BitXor` 后**追加** `Prod = 6` | 运行期(源文本计数) | 真实文件计数 == **6**(A-6) |
| **F-7** | `NVA-06` | **partial failure** 显式建模 | **`CP-PARTIAL-FAIL`** | 影子副本**移除** `group_destroy` 的 Fault 产出 | 运行期 | `collective_barrier` 至 N-1 → `group_destroy` ⇒ 对已到达成员产出 `Fault`(A-7) |
| **F-8** | `NVA-07` | 集体面绑定 + region 声明(**本层引入的那一条**) | `NVA-07` | 影子 JSON:删 `nvlink_4` 绑定 / 把该 region 的 `mode` 改成 `normal` | 运行期 | 真实 JSON ⇒ PASS |
| **F-9** | `NVA-08` | **V6-c** 前提约束 | **`CP-03-VC-PRECONDITION`** | 影子 JSON:把 `nvlink_4` 绑到一个**未注册**的 plane 名 | 运行期(**detail 前缀** + 探针精确 id) | 真实绑定 ⇒ PASS;`null` 绑定 ⇒ **SKIP**(非 FAIL)(A-2 / A-3) |

⇒ **9 组**,1:1 对应 **NVA-01 ~ NVA-09**;归因 = **2 个**本层新建 `CP-*`(F-5 / F-7)+ **7 个** `NVA-xx`。
⇒ **复用**的既有 id(`CP-03` / `CP-03-VC-PRECONDITION` 之外的 `CP-05` / `CP-OPCODE-CLASSIFIER` /
`CP-EPOCH-SEMANTICS` / `CP-DRAIN-FAIL` / `CP-DIVERT-STOP`)由**既有判据 / 规约层**承担
⇒ 本层**不**重复立 fixture(禁双实现)。

### §6.2 「每一义务一条 fixture」的**准确表述**(A-11 修订)

⇒ 「**本层新建的 2 个 `CP-*` id + 7 个 `NVA-xx` 场景 id 各有一条 fixture**;
**复用** id 由既有判据承担」——⛔ **不**再写「每条判据都有一条」(那会把复用的 7 个也算进来)。

### §6.3 元测试(A-11 配套)

⇒ 新增**元测试**:`design §7` 的 id 集合 ⊇ fixture 的 id 集合,且每个**本层新建** id 至少出现一次;
**反向**也查(fixture 里的 id 必须存在于 §7)⇒ 照 `scripts/test/cep_doc_ref_registry.tsv` 的 TSV + 校验脚本先例。

### §6.4 两类红形态(铁律 ③)

① **编译期红** ⇒ `static_assert` 文案须以 `<归因 id>:` **开头**(照 `cep_cxl_dcoh_adapter_reverse_verify.sh:118-124`:
「任意编译失败」**不算**命中);
② **运行期红** ⇒ 探针输出 `PROBE:FAIL <归因 id>`,**精确相等**(⛔ 前缀匹配会让 `CP-03` 命中 `CP-03-VC-PRECONDITION`)。
⇒ **编译期锁不得只以「编译红」充当 ctest 判据** ⇒ 每条 `static_assert` 另配一条运行期判据。

---

## §7 归因 id 登记(**唯一**登记处 + 代码可 grep 登记点)

| 义务 | 归因 | 状态 | 代码登记点 |
|---|---|---|---|
| R1:opcode 永不作分类器 | `CP-OPCODE-CLASSIFIER` | **复用**(规约层已立) | 规约层载体 |
| 源端不得持有区域表可见性 | `CP-05` | **复用**(本层扩展其清单与扫描面,§5.3) | `collective_plane_contract.hh` |
| R4 epoch 语义双向 | `CP-EPOCH-SEMANTICS` | **复用** | 规约层载体 |
| R3 drain 失败不建 epoch | `CP-DRAIN-FAIL` | **复用** | 规约层载体 |
| R5 divert 计数 + fail-stop | `CP-DIVERT-STOP` | **复用** | 规约层载体 |
| collective VC 预留状态(空预留门) | `CP-03` | **复用** | `collective_plane_contract.hh` |
| `V6-c` 前提约束 | `CP-03-VC-PRECONDITION` | **复用**(`ADR-CEP-01` SU 已印出) | `collective_plane_contract.hh`(`detail` 前缀) |
| **V3** barrier 分类 litmus | **`CP-BARRIER-CLASS`** | **本层新建**(per `ADR-CEP-01` SU G.4「V3 的归因 id 待实现层建立」) | `collective_plane.hh`(载体头) |
| **partial failure** 显式建模 | **`CP-PARTIAL-FAIL`** | **本层新建**(per `ADR-CEP-03` SU F.3「id 待实现层建立」) | `collective_plane.hh`(载体头) |
| adapter 零副本 / 无 `transition_*` | `NVA-01` | 本层机制判据(**Scenario id** 作归因键) | 测试用例名 |
| `BroadcastNull` 行为实现 | `NVA-02` | 同上 | 测试用例名 |
| `nvlink_4` 实例声明 | `NVA-03` | 同上 | 测试用例名 |
| V3 判据面(同上,归因键) | `NVA-04` | 同上 | 测试用例名 |
| 算子族恰 6 值 | `NVA-05` | 同上 | 测试用例名 |
| partial failure(同上,归因键) | `NVA-06` | 同上 | 测试用例名 |
| 集体面绑定 + region 声明 | `NVA-07` | 同上 | 测试用例名 |
| V6-c(同上,归因键) | `NVA-08` | 同上 | 测试用例名 |
| CP-05 清单扩展(同上,归因键) | `NVA-09` | 同上 | 测试用例名 |

⇒ **同一义务不得有两个 id**;新建 `CP-*` 仅 **2** 个(ADR 强制的 V3 / V5);机制判据用 `NVA-xx`
(照队列 #12 的 `CDX-01..08` 先例:该队列**未**造 `CP-*` 归因 id)。
⇒ ⚠️ **id 归一登记**(A-10 配套):`ADR-CEP-01 §7` 附录已印出的 `BARRIER-CLASSIFY`(V3)与
`COLLECTIVE-PARTIAL-FAIL`(V5)是**离线标签** ⇒ **唯一归因** = `CP-BARRIER-CLASS` / `CP-PARTIAL-FAIL`
(照 `ADR-CEP-03` SU(#3) I.1 对 `CP-03-VC-PRECONDITION` 的处理)。

---

## §8 pending 边界(9 项 ⇒ 本层纪律)

规约层的 **9 项** `pending`(锚点 `N-1` ~ `N-9`)**不得**被本层推理填空:
reduce 算子穷举集 · 逐操作支持矩阵 · epoch 状态机细节 · VC 编号 · 聚合树构建算法 ·
报文格式 / opcode 布局 · 数值 / 精度语义 · NVLink 实例的 `perm` 具体子集 · NVLink atomic 的协议侧 completion 配对。

⇒ 本层**只**实现**已定稿**的结构性语义;凡落到上述 9 项 ⇒ **一律**留 `pending` + 登记锚点。
⇒ ⚠️ **声明层目前无 basis 纪律**(`SC/SC`、`writeback=true`、`combination_capabilities` 三项同形)⇒ 登记为**建模假设** backlog。

---

## §9 文档同步面 + 冻结面授权面

### §9.1 ⚠️ M1 冻结面 = **3 份**(+ manifest 本身)(A-4 修订)

| 冻结产物 | 本层改动 | 授权面 |
|---|---|---|
| `docs/architecture/07-coherence-exploration-platform.md`(在 `FROZEN_GLOBS:32`) | §5.1.1 **NVLink 行状态格**(`:320`)+ §11 **`:679`**(SharerTracker `5/6` → `6/6`)+ **`:681`**(adapter「**6 个已落地**」与「**adapter 实例 5 个**」—— ⚠️ 该格**自身已有两个互相矛盾的数字**,须一并订正)+ **`:690`**(声明源「**6** 实例」⇒ **7**) | **两步授权** |
| `openspec/specs/cep-collective-plane-contract/spec.md` | delta 合并(第四键 + V6-c + CP-05 扩展) | **两步授权** |
| `openspec/specs/cep-collective-plane-base/spec.md` | 失指 change 名残余 | ⚠️ **单独立项**(本层**不并入**,避免冻结面变更面扩大) |
| `docs/adr/ADR-CEP-01-*.md` / `ADR-CEP-03-*.md`(也在冻结集) | 若为 V3 别名 / tracker 归属追加 SU ⇒ **需授权** | **architect 域** ⇒ 本层**只提申请** |

⇒ **授权流程**(顺序不可颠倒,metis 验收点):① 台账追加 **Entry(授权记录)** ⇒ ② `openspec archive`
⇒ ③ `cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ commit ⇒ ④ `--update` ⇒ commit。
⚠️ `--update` 会**重算全部 40 件**的 hash ⇒ 授权 Entry 必须**覆盖本层改动的每一件**。

### §9.2 非冻结面的同步

| 面 | 动作 |
|---|---|
| `include/tlm/cep/collective_plane.hh:206` | **只订正 change 名**(失指 `cpptlm-cep-nvlink-sharp-adapter` ⇒ 本 change)⚠️ **保留**「接线侧属 P3」的承诺 —— 本层**不**接线(A-1)⇒ 附注「判读面由本 change 交付;**接线侧 P3 未排期**」 |
| `include/tlm/cep/sharer_tracker.hh:13-24` | 头注释订正(`BroadcastNull` 行为面**已落地**)(A-14)⇒ 属**注释行**改动(§2.6 接口面定义允许) |
| `include/tlm/cep/collective_planes.json` | `derived_from.current_state` 同批改(A-16) |
| `scripts/README.md` | 修「`cep_*` **12 个**」计数(实测 **18** 个)+ 守门表新增行 + **反向 fixture 登记**新增节(A-17) |
| `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` | 反向 fixture 组数 **5 → 9**;状态行 → 进行中 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | 台账追加订正句(Entry #186 ⑥ 的「归约行为实现」措辞,A-12)· §10 backlog 更新 |

### §9.3 ⚠️ 计数同步清单(A-13 · 加实例必转红处)

| 文件:行 | 现值 | 新值 |
|---|---|---|
| `test/test_cep_adapter_schema_contracts.cc:369` | `instances.size() == 6` | **7** |
| `test/test_cep_adapter_schema_contracts.cc:383` | `no_upgrade_instances.size() == 2` | **3**(`nvlink_4` 无静默升级行 ⇒ **必进**派生集) |
| `test/test_cep_adapter_schema_contracts.cc` 的 `at(0)/at(1)` | 顺序敏感 | 追加 `at(2)` |
| `test/test_cep_chi_rnf_subset.cc:1144` | `owned.size() == 4` | **5** |
| `test/test_cep_mesif_instance_decl.cc:105` | `2u` | **3u** |
| `test/test_cep_moesi_gpu_adapter.cc:397` | `implemented == 6` | **7** |
| `test/test_cep_tilelink_instance_decl.cc:199` / `:205` | `2u` / `6u` | **3u** / **7u** |
| `test/test_cep_collective_plane_contract.cc:100` | `decl.plane_binding.size() == 2` | **3** |
| `test/test_cep_collective_plane_contract.cc:102-103` | 两个 `null` 断言 | 保留 + 增 `nvlink_4` 断言 |

⇒ **反向验收**:不仅断言新值,还要**保留注释说明来源**(照队列 #12 的「+1 实例」注释先例)⇒ 否则下一个人改回旧值也不会红。

---

## §10 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| **公开来源受限** ⇒ 9 项 pending | 🟡 中 | 本层只实现结构性语义;逐格 basis 在规约层载体;实现层**不**新增 pending 项 |
| **冻结面归档门槛**:本层归档要改 **3 份** M1 冻结产物(+ manifest)⇒ 漏授权 ⇒ `cep_freeze_check` 报红 | 🟡 中 | §9.1 的固定顺序 + 授权 Entry **逐件**列名 |
| **V3 无生产消费者**(ingress 分发路径不存在)⇒ 判据**自证**风险 | 🟡 中 | ① 明写「判读面,**无生产消费者**;接线侧 P3 未排期」② F-5 的正控用**两类各一例**+「两者同判 ⇒ 必红」③ 签名锁:**不得**含 `opcode` / `group_id` / `addr` 的**单标签**形态(须用结构体) |
| **F-3 / F-6 类「恒真」判据** | 🟡 中 | 已改为**毒化真源**(traits 第 4 行 / 源文本枚举项计数)⇒ A-5 / A-6 处置 |
| **`registered_regions` 无实例归属键** ⇒ 多实例归属不可表达 | 🟢 低 | 本层单例模型可用;写进声明源 + backlog(§5.1 / §5.3) |
| **声明层的建模假设**(`SC/SC` / `writeback` / `combination_capabilities`) | 🟢 低 | 登记为**建模假设** backlog(§8) |
