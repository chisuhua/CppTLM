# cep-nvlink-sharp Specification(delta)

## Purpose

定义 **NVLink SHARP** 在 CEP 平台上的**实现层可执行契约**:
`NvlinkAdapter`(独立头 · 零自有表副本 · 无转移表)· `BroadcastNull` 行为实现(协议无关)·
`nvlink_4` 实例声明(13 键)· 集体面绑定(首个消费者)· **V3** barrier 分类 litmus ·
**算子族恰 6 值**断言 · **partial failure** 显式建模 · 声明面校验。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #187**(双审修订)· **#186** · **#179 / #178** ·
**#171** · `ADR-CEP-01` SU 2026-11-03(Decision D)+ `ADR-CEP-02` / `03` / `04` / `11` / `12`)
⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改任何 ADR 正文 · 不改 `protocol_instance.schema.json` · 不改 `collective_planes.schema.json` ·
> 不改 `sharer_tracker.hh` 的**接口面** · **只读消费**已归档的 `nvlink_sharp_state_spec.hh` ·
> **不定义** reduce 算子族(唯一真源 `tlm::cep::ReduceOp`)· **不建模**报文格式 / opcode 布局 /
> 数值精度 / 聚合树构建算法 · **0 新增** ABI · **0 新增** `INV-` 编号 · **不产出任何性能结论**
>
> **合规边界**:NVLink SHARP 正式文档受 NDA 约束 ⇒ **禁止**把规范正文投喂任何 AI
> ⇒ 本能力的全部语义指向 `ADR-CEP-0x` 的**已定稿**裁决;**9 项**细粒度语义维持 `pending`
> (锚点 `N-1` ~ `N-9`)⇒ 本层**不得**推理填空。

> ⚠️ **与台账 `Entry #186 ⑥` 的口径差异(已登记订正)**:台账写「SHARP 归约**行为实现**」,
> 而本层交付的是**载体 + 声明 + 判据**,**不含**归约**数据路径**
> (`LocalCollectivePlane::send` 在已注册 + epoch 生效时仍返回 `DataPathNotImplemented`)⇒
> 「行为实现」在本层 = **判据与声明面**的行为,**不是**数据通路的实现。

---

## ADDED Requirements

### Requirement: cep-nvlink-sharp

CEP SHALL 为 **NVLink SHARP** 提供一份**可执行实现层契约**:
adapter 载体 · tracker 行为 · 实例声明 · 集体面绑定 · 判据接线。

#### Scenario: NVA-01 —— `NvlinkAdapter` SHALL **零自有表副本**且**无转移表**

- **WHEN** 读取 `include/tlm/cep/nvlink_adapter.hh` 的**去注释后**文本
- **THEN** SHALL **不**含任何**成员字面量 / 表名**(`IngressDecision::` / `RegisterResult::` / `EpochState::` /
  `OrderingRule::` / `VcReservationState::` / `CollectiveScope::` / `EvidenceKind::` / `SourceId::` /
  `Basis{` / `kSourceLicenseConfirmed` / `all_basis_valid` / `PendingSemanticItem` / `kBasis*` / `kPendingSemanticItems`)
- **AND** SHALL **不**含任何 `transition_*` 标识
- **AND** SHALL **不**出现 `collective_region_table` 标识(区域表类型**不**暴露)
- **AND** **类型名**(`using ... = nvlink_sharp::...;`)SHALL **允许**(它是**类型**别名,不含 `::`)
- **AND** ⚠️ **数值镜像表**不在文本扫描面内 ⇒ 由锚点摘要 §2bis 的 **12 格人工核对** + 载体侧 `static_assert` 兜底
- **AND** **反向 fixture**(2 组):注入成员字面量 / 加 `transition_*` ⇒ SHALL 必红(归因 `NVA-01`)
- **AND** **正控**:同一扫描器对**未毒化**影子 ⇒ 零命中;对**毒化**影子 ⇒ 命中(证明扫描器有牙)

#### Scenario: NVA-02 —— `BroadcastNull` SHALL 有**行为实现**且 caps **逐字段取自 traits**

- **WHEN** 读取 `include/tlm/cep/broadcast_null_tracker.hh` 并构造其实例
- **THEN** 其 `caps` / `behavior` SHALL **逐字段取自** `kTrackerTraits` 中 `TrackerImpl::BroadcastNull` 行
  (SHALL **不**另写 caps 字面量;SHALL 用**查找函数**而非硬编码下标 ⇒ 行序变化不静默错位)
- **AND** `scope = NONE` ⇒ `lookup` SHALL **恒**返回 `BROADCAST` 且 `view.valid == false`
- **AND** `authority = NONE` ⇒ SHALL **不**给出任何本地 sharer 结论
- **AND** `hazard = NONE` ⇒ SHALL **不**返回 `STALL` / `CHAIN`
- **AND** 安装面(`set_sharer` / `note_sharer_dropped` / `clear_sharer`)SHALL 为**显式 no-op**
- **AND** **出参全断言**:`result` / `view.valid` / `view.sharers` / `view.dirty` / `view.forward_to` / `hazard`
  —— ⚠️ `LocalView` 的**默认值恰好符合期望** ⇒ 只断言部分出参会**恒真**
- **AND** ⚠️ **等值断言本身无鉴别力**:`SharerTrackerCaps{}` 的默认值与 traits 行**逐字段相同**
  ⇒ **反向 fixture** SHALL 毒化 **traits 行本身**(影子副本)⇒ 类的可见 caps **必须跟随**;不跟随 ⇒ 必红(归因 `NVA-02`)

#### Scenario: NVA-03 —— `nvlink_4` 实例声明 SHALL **13 键齐备**且与 traits **不漂移**

- **WHEN** 加载 `include/tlm/cep/protocol_instances.json` 并取出 `nvlink_4`
- **THEN** SHALL 齐备 `protocol_instance.schema.json` 的 `required` **实读**的 **13 键**(缺一 ⇒ 判红)
- **AND** `protocol_version` SHALL 为 `nvlink-4.0`(per `ADR-CEP-11 §3` 锁表)
- **AND** `sharer_tracker.impl` SHALL 为 `BroadcastNull`,且其 `caps` 4 项 SHALL 与 traits 行 **逐字段相等**
- **AND** `repo_owned_instance_ids` SHALL 含 `nvlink_4`
- **AND** `atomic.supported` SHALL 为 `true`,且 SHALL **不**出现无公开来源依据的 `intent_field`
- **AND** `perm` SHALL ⊆ 4 值域;⚠️ 本层填的是**值域全集**,**不**断言 NVLink 支持全部 4 种(子集 = pending `N-8`)
- **AND** **反向 fixture**:删一个键 / 改一个 caps 值 / 删 `repo_owned_instance_ids` 条目 ⇒ 判据 SHALL 必红(归因 `NVA-03`)

#### Scenario: NVA-04 —— **V3** barrier 分类 litmus SHALL **双向**成立

- **WHEN** 对 barrier 请求分别以**地址 key** 与**组 key** 求分类
- **THEN** **地址 key** ⇒ SHALL 判为 **Ordering**;**组 key** ⇒ SHALL 判为 **Collective**
- **AND** 分类函数 SHALL 接受**结构体**输入(tagged `BarrierKey{ kind ∈ {Address, Group}, addr, group }`)
  —— ⛔ SHALL **不**接受「仅含 `enum{Address,Group}` 标签」的入参(否则返回值由入参标签决定 = **恒等映射 = 恒真**)
- **AND** **双重 key** 或 **空 key** ⇒ SHALL 判红(不是二选一)
- **AND** ⚠️ 该判据 SHALL **不**与规约层 `NSS-01`(reduce / coherence 分流)混用 ⇒ 二者 SHALL 有**不同**归因
- **AND** ⚠️ 本载体是**判读面**,**无生产消费者**(ingress 分发接线属 P3,**未排期**)⇒ 该事实 SHALL **显式登记**
- **AND** **反向 fixture**:把两类**判成同一类** ⇒ SHALL 必红(归因 **`CP-BARRIER-CLASS`**)

#### Scenario: NVA-05 —— 「算子族恰 6 值」SHALL 有**可判定的**断言

- **WHEN** 对**去注释后**的 `enum class ReduceOp { … }` 块做**枚举项计数**
- **THEN** SHALL **恰为 6**(`Sum` / `Min` / `Max` / `BitAnd` / `BitOr` / `BitXor`)
- **AND** 本层 SHALL **不**定义 / 新增算子族(唯一真源 = 既有 `ReduceOp`)
- **AND** ⚠️ 计数 SHALL **不得**依赖「枚举最大值 + 1」(C++17 无反射 ⇒ **追加**第 7 个值时计数不变 ⇒ 无鉴别力)
- **AND** **反向 fixture**:在 `BitXor` 之后**追加** `Prod = 6` ⇒ 计数 **7** ⇒ 必红(归因 `NVA-05`)

#### Scenario: NVA-06 —— **partial failure** SHALL 有**显式建模**(既有 Fault 路径)

- **WHEN** 某 group 的 `collective_barrier` 已有成员到达(N-1)后调用 `group_destroy`
- **THEN** SHALL 对**已到达**成员产出 `CollectiveEvt` 的 **`Fault`** 变体(不得静默)
- **AND** ⛔ SHALL **不**新增 `CollectivePlaneIf` 方法(接口面**恰 7 个**虚方法,已硬锁)⇒ 「成员掉线」SHALL **不**作为判据文案
- **AND** SHALL **不**建模任何 NVLink 特有错误语义(错误语义归协议侧)
- **AND** **正控**:上述构造序列 ⇒ 产出 `Fault`;**反向 fixture**:移除该产出 ⇒ 必红(归因 **`CP-PARTIAL-FAIL`**)

#### Scenario: NVA-07 —— 集体面绑定 SHALL 表达**节点内本地模型**且 region 走 **non-cacheable 降级**

- **WHEN** 读取 `include/tlm/cep/collective_planes.json`
- **THEN** `plane_binding` SHALL 含 `nvlink_4` → **本地模型**(`LocalCollectivePlane`)
- **AND** `registered_regions` SHALL 含**本 change 引入的那一条**(以 `lo`/`hi`/`group` 锚定),
  且其 `mode` SHALL 为 `nc_drop_cacheable`(per `ADR-CEP-03 §3.1` R3:同一地址不得同时「SHARP 已注册」且「coherence-cacheable」)
  —— ⚠️ **不**断言「全表每条 mode」:未来不启用 collective 的拓扑**不得**被迫塞假 region
- **AND** `collective_vc_reservation` SHALL 保持**空**且 `vc_reservation_declaration` **四字段齐全**(per Decision D)
- **AND** ⚠️ **已知局限 SHALL 显式登记**:schema 的 region 项**无实例归属键** ⇒ 多实例归属不可表达(本层不改 schema)
- **AND** **反向 fixture**:删 `nvlink_4` 绑定 / 把**本层那条** region 的 `mode` 改成 `normal` ⇒ 必红(归因 `NVA-07`)

#### Scenario: NVA-08 —— **V6-c** 前提约束 SHALL 可执行(判据本体在 `CP-03` 族)

- **WHEN** `vc_reservation_declaration.status == "unavailable"`
- **THEN** `plane_binding` 的**每个非空**绑定值 SHALL 落在**本地模型白名单**内 ⇒ 否则 SHALL 判 **FAIL**
- **AND** ⚠️ `null` 绑定(loader 物化为空串)SHALL **不**判 FAIL;`plane_binding` 为**零键** ⇒ SHALL 判 **SKIP**
  (⛔ **禁** vacuous PASS)
- **AND** ⛔ SHALL **不**新增第 7 条 `ContractRule`(规则族仍为 `CP-01`..`CP-06`)⇒ V6-c SHALL 作为 `CP-03` 行的
  **detail 前缀** `[CP-03-VC-PRECONDITION]` 表达;`rule=` 字段**仍为** `CP-03`
- **AND** **正控**:当前绑定 ⇒ PASS;`null` 绑定 ⇒ SKIP;**反向 fixture**:绑定一个**未注册**的 plane 名 ⇒ 必红
  (归因 **`CP-03-VC-PRECONDITION`**,**精确相等**匹配 —— ⛔ 不用前缀匹配,否则 `CP-03` 会命中它)

#### Scenario: NVA-09 —— **CP-05** 标识清单 SHALL 含 `collective_region_table` 且扫描面可扩展

- **WHEN** 运行完成域判别(coherence 侧扫描)
- **THEN** 标识清单 SHALL 含 `collective_region_table`(coherence 侧**不得**出现区域表类型)
- **AND** 扫描集合 SHALL 可**注入扩展**(`ContractPaths` 提供扩展列表)⇒ 扩展面 SHALL 是**真正的 coherence / 源端侧**文件
  —— ⛔ SHALL **不**把 collective 侧的 adapter 头塞进 coherence 侧扫描(那只会「证明没扫错文件」= 恒真)
- **AND** adapter 的「不暴露区域表」由 `NVA-01` **独立**承担 ⇒ ⛔ 不构成第二处归因
- **AND** **正控**:同一注入对既有 coherence 侧文件 **必红** ⇒ 证明扫描器有牙;**反向 fixture**:注入 `collective_region_table` ⇒ 必红(归因 `CP-05`)

---

> **锚点**:`test/test_cep_nvlink_sharp.cc` 的编译期断言 + 运行期兜底判据 + **9 组反向 fixture**
> (每组带**可归因 id**)+ `scripts/test/cep_nvlink_sharp_probe.cc` / `..._reverse_verify.sh`
> + `test/CMakeLists.txt` 的 `ctest` 注册。
>
> ⚠️ **归因 id 的唯一登记处 = 本 change 的 `design §7`**;本 spec 只引用,不重复登记。
> ⚠️ **V6-c / CP-05 的判据本体**在 `collective_plane_contract.hh` + `test_cep_collective_plane_contract.cc`
> (见本 change 的 `cep-collective-plane-contract` delta)⇒ 本 spec **只引用**,⛔ **不重复实现**(禁双实现)。
