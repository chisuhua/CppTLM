# cep-sharer-tracker-base Specification

## Purpose
定义缓存一致性平台的 **SharerTrackerIf 单接口**（恰好 4 个方法）及其**能力描述符**
（caps 4 属性 `scope`/`completeness`/`authority`/`aggregation_at` + behavior 2 属性），
使 5 种一致性族（MOESI / MESIF / CHI RN-F / CXL 非对称 / TileLink 树）能用同一抽象表达
目录查询与 snoop 生命周期，并让「协议实现的能力边界」成为**可静态判定的声明**而非隐含行为。
本 capability 交付**唯一行为实现** `FullMapDirectory`；其余 4 种实现的声明层登记随队列 #6~#9 落地。

## Requirements

### Requirement: cep-sharer-tracker-base

CEP SHALL 将 **SharerTrackerIf 单接口 + caps descriptor 4 属性 + D-5 全量契约 + FullMapDirectory 唯一行为实现**固化为**可执行契约**;每条 SHALL 有**行为级**断言,SHALL **只收本仓可验证**的判据。

> **定位**:Sharer Tracker 是**协议无关单接口**(per `ADR-CEP-04 §2 Decision 1` α 权限三件套);FullMapDirectory 是**首个行为实现**(其余 4 种跟随各自队列)。

#### Scenario: `SharerTrackerIf` 4 方法 SHALL 存在且命名逐字对齐

- **WHEN** 读取 `include/tlm/cep/sharer_tracker.hh` 的接口面
- **THEN** SHALL 存在**且仅存在** 4 个公开方法:`lookup(addr, intent, result, view, hazard)` · `snoop_begin(addr, expected_responders, handle)` · `snoop_fold(handle, node_id, resp_state, dirty, data_supplied, remaining_mask, merged_state)` · `observe_snoop(addr, from_node, resulting_local_state)`
- **AND** 断言 SHALL 针对**签名**(C++ decltype)且与 `ADR-CEP-04 §3.1/§3.3/§3.4` 的 SystemVerilog 块**逐项对齐**
- **AND** 命名 SHALL **不得**出现初版旧名(`snoop_complete` / `lookup→sharer_bitmap` 平铺形态)

> **锚点**:`test_cep_sharer_tracker_base.cc` 的接口签名 decltype 断言

#### Scenario: caps descriptor 4 属性 SHALL 为 bitmask 64-bit(**spec 首次定义**)

- **WHEN** 读取 caps descriptor struct
- **THEN** SHALL 存在**恰 4 个**属性:`scope` / `completeness` / `authority` / `aggregation_at` per `ADR-CEP-04 §2 决策 3`(与冻结 schema `protocol_instance.schema.json` 的 `sharer_tracker.caps` required 逐项一致)
- **AND** `hazard`(§4 表)与 `broadcast_fallback`(决策 2)SHALL **不在** caps 内,而是并列的行为属性 `SharerTrackerBehavior`
- **AND** `ResponderSet` 形态 SHALL 为 **bitmask 64-bit**(编译期 `static_assert(sizeof(responder_set_t)==8)`)⇒ **spec 首次定义**(沿用 `ADR-CEP-04` 先例措辞)
- **AND** SHALL 有 **bit→node_id 显式映射表**,**bit 0 = NODE_ID_SDMA**(per `soc_internal_noc.hh` 的 NODE_ID 编码)

> **锚点**:`test_cep_sharer_tracker_base.cc` 的 caps 签名 + static_assert 断言

#### Scenario: 5 实现 × 4 caps mapping 表 SHALL 数据级自洽

- **WHEN** 读取 caps 表(FullMap / SparseSF / DcohSymmetric / TreeManager / BroadcastNull × `completeness`/`aggregation_at`/`hazard`/`broadcast_fallback`)
- **THEN** FullMap 行 SHALL 与 `ADR-CEP-04 §4 table` + `§3.5` 一致;DcohSymmetric 行 SHALL 与 `§3.5` 的 P2P/HDM **二元**一致
- **AND** 断言 SHALL 针对**数据一致性**(表值逐项对齐),非仅「常量存在」

> **锚点**:`test_cep_sharer_tracker_base.cc` 的 caps 表数据级契约断言

#### Scenario: FullMapDirectory 唯一行为实现 SHALL 满足 lookup 语义

- **WHEN** 对一个 `FullMapDirectory` 实例做 lookup
- **THEN** 状态完整的请求 SHALL 返回 `ANSWER_HERE` + 精确 sharer 集(per `ADR-CEP-04 §2 Decision 1`/`§3.1` 的 LookupResult 枚举)
- **AND** `completeness = EXACT`、`broadcast_fallback = false`(per `§2 Decision 2`;**无写回 fallback 分支的 `PARTIAL`**)
- **AND** `hazard = STALL`(per `§4 table` 第一行),且 caps 表与实现**逐项对齐**

> **锚点**:`test_cep_sharer_tracker_base.cc` 的 FullMapDirectory 行为断言

#### Scenario: snoop begin / fold / remaining_mask 生命周期 SHALL 派生 RetireOnZeroPending

- **WHEN** `snoop_begin` 注册 `expected_responders` 集合,后续 `snoop_fold` 逐个 append responder
- **THEN** `remaining_mask` SHALL 随每次 fold 单调递减;`remaining_mask == 0` SHALL 等价于 **RetireOnZeroPending**(per `ADR-CEP-04 §3.3` 派生)
- **AND** 断言 SHALL 针对**mask 位变化**且 ≥ 2 组可执行序列

> **锚点**:`test_cep_sharer_tracker_base.cc` 的 lifecycle 断言

#### Scenario: 同地址并发 lookup hazard SHALL 按 caps 表返回

- **WHEN** 同 addr 在 `snoop` 在飞期间做第二次 lookup
- **THEN** 第二次必须按 caps 表返回 `Hazard_STALL`(非 NONE / 非 CHAIN)(per `§4 table` FullMap 行)
- **AND** 断言 SHALL 针对**第二次 lookup 的返回值**被称为「**返回结果改变**」

> **锚点**:`test_cep_sharer_tracker_base.cc` 的 hazard 行为断言

#### Scenario: `INV-CEP-04-IC2` 幂等 + `IC3` 生命周期 SHALL 可执行断言

- **WHEN** `observe_snoop` 被同 `from_node` 连续调用两次(幂等测试)
- **THEN** 第二次 SHALL 产生与第一次**相同的 LocalView 投影**(per `INV-CEP-04-IC2` 幂等语义)
- **AND** IC3 `5 事件名`(per `ADR-CEP-04 §2 IC3`)SHALL 存在

> **锚点**:`test_cep_sharer_tracker_base.cc` 的 IC2 幂等 + IC3 生命周期行为断言

#### Scenario: 禁用 API `SHALL NOT` 出现于接口面(去注释后静态扫描)

- **WHEN** 扫描 `sharer_tracker.hh` 的**去注释后**代码
- **THEN** SHALL **不**出现 `snoop_complete` / `subscribe_remote_snoop` / `is_distributed` / `lookup→sharer_bitmap` 平铺形态(per `ADR-CEP-04 §3.6` 禁止的旧接口)
- **AND** 该扫描 SHALL 配**正控**(同机制必须能发现确实存在的标识符)

> **锚点**:`test_cep_sharer_tracker_base.cc` 的静态扫描断言(去注释 + 正控)

#### Scenario: 非 FullMap 的其余 4 实现 SHALL 只做声明层断言(**deferred**)

- **WHEN** 核对其余 4 实现(SparseSF / DcohSymmetric / TreeManager / BroadcastNull)的覆盖面
- **THEN** 行为断言 SHALL **deferred**;只有 caps 表的**声明层枚举一致性**断言可用
- **AND** 理由 SHALL 记录:真实行为需真实 protocol instance(per metis 建议 #5 + 队列 #2 先例)

> **锚点**:`test_cep_sharer_tracker_base.cc` 的 deferred 登记断言

#### Scenario: `snoop_begin` 的**拒绝**语义与 `set_sharer`/`clear_sharer` 的 **no-op** 语义

- **WHEN** `FullMapDirectory::snoop_begin` / `set_sharer` / `clear_sharer` 收到下列输入之一:
  `expected_responders` 含**未映射位**(bits 4..63)/ 16-bit `snoop_handle_t` 空间**已满** /
  `node` 是未映射节点(`kInvalidNode` 或不在 `kResponderBitToNode` 内)
- **THEN** 闸 SHALL **拒绝**而非静默接受,且**不得**破坏已有状态:
  - 含未映射位 ⇒ `handle = 0`(那些位永远扣不掉 ⇒ 接受即意味着 snoop 永不 retire、该地址永久 `STALL`)
  - handle 空间满(65535 个在飞)⇒ `handle = 0`,**既有条目不得被覆盖**(16-bit 是 `ADR-CEP-04`
    的 RTL 契约,不可 widen ⇒ 只能**线性探测空闲槽** + 满表拒绝)
  - 未映射节点 `set_sharer` / `clear_sharer` ⇒ **显式 no-op**:不得创建
    `sharers == dirty == 0` 的**可见空条目**(与「该地址从未出现过」在 `view_of` 无法区分)
- **AND** `clear_sharer` 对「本非 sharer/dirty 持有者」的节点 SHALL no-op ——
  否则会在 `lookup` 刚 Allocate 的空条目上伪触发 `RetireOnZeroSharers`,
  使 IC3 事件流出现 `Allocate → Retire` 而全程无 sharer 存在(**事件流是本平面的契约可观测面**)
- **AND** 真 retire SHALL 恰好产生 **1 次** `RetireOnZeroSharers`(正控;纯负向断言无正控 = 判据可能没生效)

> **锚点**:`test_cep_sharer_tracker_base.cc` 的「状态机边界(handle 满 / 未映射位 / 伪 retire)」用例
> (三条负向**各配正控**);拒绝与「同址矛盾」共用 `handle = 0`(`ADR-CEP-04` 冻结 4 方法接口面,
> 无法加错误码 ⇒ 调用方不可区分原因,已登记为已知限制)
