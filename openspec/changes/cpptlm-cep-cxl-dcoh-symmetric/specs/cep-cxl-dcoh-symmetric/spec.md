# cep-cxl-dcoh-symmetric Specification

## Purpose
定义 **CXL 3.0 对称(DCOH)**在 CEP 平台上的**实现层可执行契约**:
**独立** `CxlDcohAdapter`(零自有表副本 · 委托规约载体)· `DcohSymmetric` **行为实现**
(traits 首次兑现)· `cxl_3_0_symmetric` 实例声明(13 键 · `perm` 4 值)。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #159** / **#154** / **#153** / **#122**
+ `ADR-CEP-04` 的 `## Status Update(2026-11-02)` + `ADR-CEP-02` / `ADR-CEP-04` / `ADR-CEP-11` / `ADR-CEP-12`)
⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改任何 ADR 正文 · 不改 `protocol_instance.schema.json`(值级枚举已含 `REQUESTER` ⇒ 零改动)
> · 不改 `CxlCacheAdapter` / `cxl_state_spec.hh` / `cxl_dcoh_state_spec.hh`(规约层**只读**消费)
> · **不**改 `adapter_schema_contracts.hh`(`AC-05` **复用**既有族无关化)
> · adapter **不暴露** transition API · **不**新增 ABI · **不**新增 invariant 编号数 · **不产出性能结论**
>
> **合规边界**:CXL 规范受 **Evaluation Copy Agreement §3** + **AI Acceptable Use Policy** 约束
> ⇒ **禁止**把规范正文投喂任何 AI ⇒ 本能力全部 basis 指向**公开非规范来源**的锚点摘要条目号。

---

## ADDED Requirements

### Requirement: cep-cxl-dcoh-symmetric

CEP SHALL 为 **CXL 3.0 对称(DCOH)**提供一份**可执行实现层契约**:
独立零副本 adapter · `DcohSymmetric` 行为实现 · `cxl_3_0_symmetric` 实例声明。

#### Scenario: CDX-01 —— `CxlDcohAdapter` SHALL **零自有表副本**

- **WHEN** 扫描 `include/tlm/cep/cxl_dcoh_adapter.hh` 的**去注释后**文本
- **THEN** 子条件 ① SHALL 成立:其 SHALL **不**含 `DevCacheState::` / `DcohViewState::` / `HomeDirState::` /
  `CxlMemChannel::` / `CxlCacheChannel::` / `ChannelDirection::` / `Perm::` **任何成员字面量**
- **AND** 子条件 ② SHALL 成立:其 SHALL **不**含任何**表名**(`kPermProjection` / `kDeviceInitiatedSnoopSet` /
  `kCxlMemChannelDirection` / `kBasis*` / `kPendingSemanticItems`)
- **AND** 子条件 ③ SHALL 成立:**类型名**(`using DevCacheState = CxlDcohStateSpec::DevCacheState;`)SHALL **允许**
- **AND** 子条件 ④ SHALL 成立:**反向 fixture**:植入任一成员字面量或表名 ⇒ SHALL **必须因正确原因红**
- **AND** 子条件 ⑤ SHALL 成立:**正控** —— 同扫描机制 SHALL 能发现**确实存在**的标识符

> **锚点**:`test/test_cep_cxl_dcoh_adapter.cc` 的零副本静态扫描 + 扫描器**鉴别力**自测

#### Scenario: CDX-02 —— adapter SHALL **委托**规约载体

- **WHEN** 核对 adapter 的每个查询面
- **THEN** 基数常量 / `perm` 投影 / 对称判别式 SHALL **全部**委托 `cxl_dcoh_state_spec.hh`
- **AND** adapter SHALL **不**持有任何**自有副本**
- **AND** adapter SHALL **不暴露任何「表」**(通道词表 / 方向表 / snoop 集合 / 投影表 / basis 表)—— ⚠️ **理由**:**「引用载体表」与「自有表」在文本上**不可分辨** ⇒ 若允许引用,零副本判据即失效 ⇒ 解法 = 连引用都不做,只暴露**标量**
- **AND** 委托 SHALL 为**编译期可见**(规约一改 ⇒ adapter **编译期即红**)

> **锚点**:`test/test_cep_cxl_dcoh_adapter.cc` 的委托面逐项核对

#### Scenario: CDX-03 —— adapter SHALL **不暴露** transition API

- **WHEN** 核对 adapter 的公开接口面
- **THEN** 它 SHALL **不**提供 `transition_*` 类的成员
- **AND** **理由** SHALL 记录:规约载体**无转移表**(对称模式的转移语义在公开来源里**没有**
  ⇒ 转移表会要求**推理填空** ⇒ 禁止)⇒ adapter 提供该 API 即等于**自造转移表**

> **锚点**:`test/test_cep_cxl_dcoh_adapter.cc` 的接口面扫描

#### Scenario: CDX-04 —— `DcohSymmetric` 行为 SHALL 符合 traits

- **WHEN** 对 `DcohSymmetric` 做行为核对
- **THEN** `scope` SHALL 为 `LOCAL_MEMORY_REGION` · `completeness` SHALL 为 `CONSERVATIVE` ·
  `authority` SHALL 为 `LOCAL_HINT` · `aggregation_at` SHALL 为 `HOME` · `hazard` SHALL 为 `STALL`
- **AND** `broadcast_fallback` SHALL 为 `true`(⚠️ **公开来源未见依据** ⇒ **登记 pending**,**不改** traits)
- **AND** 行为 SHALL **逐字段**可核对(数据级断言,非仅「常量存在」)

> **锚点**:`test/test_cep_cxl_dcoh_adapter.cc` 的 tracker 行为断言

#### Scenario: CDX-05 —— `LOCAL_HINT` 的**语义代价** SHALL **显式登记**

- **WHEN** 核对 `LocalView` 的使用面
- **THEN** `LocalView` SHALL **零改动** ⇒ 其 `forward_to` 字段 SHALL **语义重载**
- **AND** 登记 SHALL 写明:tracker 的答案**只是 hint**,**不可当权威** ⇒ 判读前**必须**先读 `result`
- **AND** 该义务 SHALL 可从台账 / spec **寻址**

> **锚点**:`openspec/specs/cep-sharer-tracker-base/spec.md` 的追加段 + 台账 §10 的 backlog 行

#### Scenario: CDX-06 —— 实例声明 SHALL 齐 **13** 键且 `perm` 为 **4 值**

- **WHEN** 读取 `protocol_instances.json` 的 `cxl_3_0_symmetric`
- **THEN** schema 的 13 个 `required` 键 SHALL **齐备**
- **AND** `perm` SHALL 恰为 **4** 值(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`)
- **AND** `family` SHALL 为 `industrial` · `sharer_tracker.impl` SHALL 为 `DcohSymmetric`
- **AND** `flow_support.back_invalidate` SHALL 为 **true**(BISnp 走 canonical `back_invalidate` 流)—— 与 `cxl_2_0` 的 `false` **不同**

> **锚点**:`test/test_cep_cxl_dcoh_instance_decl.cc` 的实例声明断言

#### Scenario: CDX-07 —— 实例 SHALL **不声明**静默升级

- **WHEN** 核对 `perm_upgrades.json`
- **THEN** 它 SHALL **不**含 `cxl_3_0_symmetric` 行
- **AND** 理由 SHALL 记录:E→M 静默升级在本族**未确证**(公开来源**没有**)⇒ 声明即**假声明**
- **AND** `no_upgrade_instances` SHALL **恰含 2** 个(`tilelink_c` + `cxl_3_0_symmetric`)⇒ 既有计数断言同步更新
- **AND** `combination_capabilities.hierarchical` SHALL 为 `true`(IC-2 判据**只**查该项)

> **锚点**:`test/test_cep_cxl_dcoh_instance_decl.cc` 的静默升级声明断言

#### Scenario: CDX-08 —— `AC-05` 对本族 SHALL 走 **SKIP**

- **WHEN** 以本族 adapter 填充 AC-05 的输入
- **THEN** 装载层 SHALL 判定本族**无** unique-clean 态 ⇒ AC-05 SHALL 为 **SKIP**
- **AND** 它 SHALL **不**为 FAIL,SHALL **不**记为 PASS
- **AND** `adapter_schema_contracts.hh` SHALL **零改动**(**复用**队列 #11 的族无关化)
- **AND** **回归锁**:既有族的 AC-05 判定 SHALL **逐条不变**

> **锚点**:`test/test_cep_cxl_dcoh_instance_decl.cc` 的 AC-05 SKIP 断言
