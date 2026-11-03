# cep-nvlink-sharp Specification(delta)

## Purpose

定义 **NVLink SHARP** 在 CEP 平台上的**实现层可执行契约**:
`NvlinkAdapter`(独立头 · 零自有表副本 · 无转移表)· `BroadcastNull` 行为实现 ·
`nvlink_4` 实例声明(13 键)· 集体面绑定(首个消费者)· **V3** barrier 分类 litmus ·
**算子族恰 6 值**断言 · **partial failure** 显式建模 · 装配期校验。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #186** · **#179 / #178** · **#171** ·
`ADR-CEP-01` SU 2026-11-03(Decision D)+ `ADR-CEP-02` / `03` / `04` / `11` / `12`)
⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改任何 ADR 正文 · 不改 `protocol_instance.schema.json` · 不改 `collective_planes.schema.json` ·
> 不改 `sharer_tracker.hh` **接口面** · **只读消费**已归档的 `nvlink_sharp_state_spec.hh` ·
> **不定义** reduce 算子族(唯一真源 `tlm::cep::ReduceOp`)· **不建模**报文格式 / opcode 布局 /
> 数值精度 / 聚合树构建算法 · **0 新增** ABI · **0 新增** `INV-` 编号 · **不产出任何性能结论**
>
> **合规边界**:NVLink SHARP 正式文档受 NDA 约束 ⇒ **禁止**把规范正文投喂任何 AI
> ⇒ 本能力的全部语义指向 `ADR-CEP-0x` 的**已定稿**裁决;**9 项**细粒度语义维持 `pending`
> (锚点 `N-1` ~ `N-9`)⇒ 本层**不得**推理填空。

---

## ADDED Requirements

### Requirement: cep-nvlink-sharp

CEP SHALL 为 **NVLink SHARP** 提供一份**可执行实现层契约**:
adapter 载体 · tracker 行为 · 实例声明 · 集体面绑定 · 装配期校验 · 跨平面判据。

#### Scenario: NVA-01 —— `NvlinkAdapter` SHALL **零自有表副本**且**无转移表**

- **WHEN** 读取 `include/tlm/cep/nvlink_adapter.hh` 的**去注释后**文本
- **THEN** SHALL **不**含任何**成员字面量**(`IngressDecision::` / `RegisterResult::` / `EpochState::` /
  `OrderingRule::` / `VcReservationState::` / `CollectiveScope::` / `Basis::` / `kBasis*` / `kPendingSemanticItems`)
- **AND** SHALL **不**含任何 `transition_*` 标识
- **AND** SHALL **不**出现 `collective_region_table` 标识(区域表类型**不**暴露)
- **AND** **类型名**(`using ... = nvlink_sharp::...;`)SHALL **允许**(它是**类型**别名,不是**成员**访问)
- **AND** **反向 fixture**:毒化载体(引入一个成员字面量 / 加一个 `transition_*`)⇒ 判据 SHALL 必红(归因 `CP-NVLINK-ZERO-COPY`)

#### Scenario: NVA-02 —— `BroadcastNull` SHALL 有**行为实现**且 caps **逐字段取自 traits**

- **WHEN** 读取 `include/tlm/cep/nvlink_broadcast_null_tracker.hh` 并构造其实例
- **THEN** 其 `caps` / `behavior` SHALL **逐字段取自** `kTrackerTraits[4]`(SHALL **不**另写 caps 字面量)
- **AND** `scope = NONE` ⇒ `lookup` SHALL **恒**返回 `BROADCAST` 且 `view.valid == false`
- **AND** `authority = NONE` ⇒ SHALL **不**给出任何本地 sharer 结论 ⇒ **零目录边**(per `ADR-CEP-01 §3.3` 抓取点 1)
- **AND** `hazard = NONE` ⇒ SHALL **不**返回 `STALL` / `CHAIN`
- **AND** 安装面(`set_sharer` / `note_sharer_dropped` / `clear_sharer`)SHALL 为**显式 no-op**
- **AND** **反向 fixture**:把 `lookup` 改成返回本地答案 / 把 caps 改成字面量 ⇒ 判据 SHALL 必红(归因 `CP-NVLINK-BROADCAST-NULL`)

#### Scenario: NVA-03 —— `nvlink_4` 实例声明 SHALL **13 键齐备**且与 traits **不漂移**

- **WHEN** 加载 `include/tlm/cep/protocol_instances.json` 并取出 `nvlink_4`
- **THEN** SHALL 齐备 `protocol_instance.schema.json` 的 `required` **实读**的 **13 键**(缺一 ⇒ 判红)
- **AND** `protocol_version` SHALL 为 `nvlink-4.0`(per `ADR-CEP-11 §3` 锁表)
- **AND** `sharer_tracker.impl` SHALL 为 `BroadcastNull`,且其 `caps` 4 项 SHALL 与 `kTrackerTraits[4]` **逐字段相等**
- **AND** `repo_owned_instance_ids` SHALL 含 `nvlink_4`
- **AND** `atomic.supported` SHALL 为 `true`,且 SHALL **不**出现无公开来源依据的 `intent_field`
- **AND** **反向 fixture**:删一个键 / 改一个 caps 值 / 删 `repo_owned_instance_ids` 条目 ⇒ 判据 SHALL 必红(归因 `CP-NVLINK-INSTANCE`)

#### Scenario: NVA-04 —— **V3** barrier 分类 litmus SHALL **双向**成立

- **WHEN** 对 barrier 请求分别以**地址 key** 与**组 key** 求分类
- **THEN** **地址 key** ⇒ SHALL 判为 **Ordering**;**组 key** ⇒ SHALL 判为 **Collective**
- **AND** ⚠️ 该判据 SHALL **不**与规约层 `NSS-01`(reduce / coherence 分流)混用 ⇒ 二者 SHALL 有**不同**归因 id
- **AND** 本条 SHALL 同时作为**正控**存在(两向各一例)⇒ 纯负向断言无对照 = 判据可能没生效
- **AND** **反向 fixture**:把任一向判反 ⇒ 判据 SHALL 必红(归因 `CP-NVLINK-BARRIER-CLASS`)

#### Scenario: NVA-05 —— 「算子族恰 6 值」SHALL 有**运行期断言**

- **WHEN** 运行期统计 `tlm::cep::ReduceOp` 的取值数
- **THEN** SHALL **恰为 6**(`Sum` / `Min` / `Max` / `BitAnd` / `BitOr` / `BitXor`)
- **AND** 本层 SHALL **不**定义 / 新增算子族(唯一真源 = 既有 `ReduceOp`)
- **AND** **反向 fixture**:毒化(新增枚举值)⇒ 判据 SHALL 必红(归因 `CP-NVLINK-REDUCEOP-SET`)

#### Scenario: NVA-06 —— **partial failure** SHALL 有**显式建模**(成员掉线 ⇒ `Fault` 事件)

- **WHEN** 集体面某 group 的成员掉线
- **THEN** SHALL 产出 `CollectiveEvt` 的 **`Fault`** 变体(不得静默)
- **AND** SHALL **不**建模任何 NVLink 特有错误语义(错误语义归协议侧)
- **AND** **正控**:掉线路径 SHALL 产出 `Fault`;**反向 fixture**:静默丢弃 ⇒ 判据 SHALL 必红(归因 `CP-NVLINK-PARTIAL-FAIL`)

#### Scenario: NVA-07 —— 集体面绑定 SHALL 表达**节点内本地模型**且 region 走 **non-cacheable 降级**

- **WHEN** 读取 `include/tlm/cep/collective_planes.json`
- **THEN** `plane_binding` SHALL 含 `nvlink_4` → **本地模型**(`LocalCollectivePlane`)
- **AND** `registered_regions` SHALL **非空**,且每条 `mode` SHALL 为 `nc_drop_cacheable`
  (per `ADR-CEP-03 §3.1` R3:同一地址不得同时「SHARP 已注册」且「coherence-cacheable」)
- **AND** `collective_vc_reservation` SHALL 保持**空**且 `vc_reservation_declaration` **四字段齐全**(per Decision D)
- **AND** **反向 fixture**:把绑定改成非本地模型 / 把 `mode` 改成 `normal` / 删声明字段 ⇒ 判据 SHALL 必红
  (归因 `CP-NVLINK-COLLECTIVE-BINDING`;声明字段缺失归因 `CP-03`)

#### Scenario: NVA-08 —— **V6-c** 前提约束 SHALL 可执行(`unavailable` ⇒ 仅本地模型)

- **WHEN** `vc_reservation_declaration.status == "unavailable"`
- **THEN** `plane_binding` 的**每个**绑定值 SHALL 落在**本地模型白名单**内 ⇒ 否则 SHALL 判 **FAIL**
- **AND** 白名单 SHALL 为**枚举式**(白名单外的绑定名 ⇒ FAIL)⇒ 新增 plane 实现时**必须**同步更新白名单
- **AND** **正控**:当前绑定 ⇒ PASS;**反向 fixture**:毒化(绑定一个 fabric 承载的 plane)⇒ 判据 SHALL 必红
  (归因 `CP-03-VC-PRECONDITION`)

#### Scenario: NVA-09 —— **CP-05** 标识清单 SHALL 含 `collective_region_table` 且扫描集合 SHALL 扩展

- **WHEN** 运行完成域判别(coherence 侧扫描)
- **THEN** 标识清单 SHALL 含 `collective_region_table`(coherence 侧**不得**出现)
- **AND** 扫描面 SHALL 覆盖本层新增的 adapter 头
- **AND** **正控**:coherence 侧文件 ⇒ PASS;**反向 fixture**:在扫描面注入 `collective_region_table` ⇒ 判据 SHALL 必红(归因 `CP-05`)

---

> **锚点**:`test/test_cep_nvlink_sharp.cc` 的编译期断言 + 运行期兜底判据 + **9 组反向 fixture**
> (每组带**可归因 id**)+ `scripts/test/cep_nvlink_sharp_probe.cc` / `..._reverse_verify.sh`
> + `test/CMakeLists.txt` 的 `ctest` 注册。
>
> ⚠️ **归因 id 的唯一登记处 = 本 change 的 `design §7`**;本 spec 只引用,不重复登记。
