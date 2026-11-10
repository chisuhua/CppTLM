## MODIFIED Requirements

### Requirement: cep-instance-assembly

CEP 协议实例的**实例级装配** SHALL 由**三层证据**共同判定 —— ① **声明层**:实例出现在 `protocol_instances.json` 的 `instances[]` 且其 `family` **可解析**(缺 `family` 键 ⇒ SHALL 判红);② **拓扑层**:实例出现在某配置的 `cep.bridges[].protocol_instance`,且该 bridge 的 `modules` **非空**且**逐个确实在拓扑中**,且 `bridge_id` **存在且唯一**(缺 `modules` / `bridge_id` ⇒ SHALL 判红);③ **平面层**:实例在 `collective_planes.json` 的 `plane_binding` 中的绑定 —— **非空**时 SHALL 已在 plane 注册表登记(否则 `[IAG-05]`);**为 null / 空**时 SHALL 解释为「该实例**不启用 collective**」(合法状态,如 `moesi_gpu` / `mesif`)⇒ **不判红**;**仅当**该实例被调用方经 `required_plane_instances` **显式要求绑定**时,空 / 缺失绑定 SHALL 判 `[IAG-04]`。

判定 SHALL 由**装配闸**(`instance_assembly_gate.hh`)执行,归因 tag 为 `[IAG-01]` ~ `[IAG-07]` **与 `[BSS-01]`**,且 SHALL 具备**反向 fixture**(每条判据都有「按正确原因红」的负控)。⚠️ 本闸只判**装配完整性**,⛔ 不判协议语义(语义判据在 collective plane 契约面 CP-03 与 channel dependency 闸)。

⚠️ **`[BSS-01]` 的判定口径**:同一模块名 SHALL 不出现在多个 bridge entry 的 `modules` 展开序列中 —— 口径 = 「模块名在**全部** entry 的 `modules` 展开序列中计数 > 1」,**无 entry 内去重** ⇒ **两个子情形都命中**:① **跨 entry 重叠**(模块出现在 ≥2 条 entry 的 `modules` 中);② **单 entry 内重复**(同一 entry 的 `modules` 列出同一模块名两次)。命中时 SHALL 判红并输出 `[BSS-01] module in multiple bridge entries: <name> (<count>)`(**携带计数**)。⛔ **不得**复用 `[IAG-08]`(该 tag 已被「装配闭环与既有闸一致」用例占用)。

⚠️ **required 清单的权威**:闸 SHALL **不自行推断**哪些实例「应当」启用 collective —— 该清单由**调用方**经 `required_plane_instances` 显式传入;当前唯一权威调用点 = `test/test_cep_instance_assembly.cc` 的 IAG-01 正控(`ualink_4` **required**)与 `test/test_cep_moesi_gpu_engine.cc` 的 MGE-05(`moesi_gpu` **不** required)⇒ 避免「判据可被调用方静默稀释」。

#### Scenario: IAG-01 —— 三层证据齐备 ⇒ 装配闭合

- **WHEN** 对含 `cep.bridges[]` 的拓扑跑装配闸
- **THEN** 每个被挂载的实例 SHALL 在声明层存在 · `family` SHALL **存在**且**可解析** · bridge 的 `modules` SHALL **非空**且逐个在拓扑中 · `bridge_id` SHALL **存在**且唯一
- **AND** 若 `plane_binding` 含该实例键且绑定**非空** ⇒ SHALL 已在 plane 注册表登记
- **AND** 若绑定为 **null / 空** ⇒ SHALL 视为「该实例不启用 collective」⇒ **不判红**;**仅当**该实例在 `required_plane_instances` 中 ⇒ SHALL 判 `[IAG-04]`

#### Scenario: IAG-02 —— 负控:未声明 / 未绑定 / 未登记 / 模块缺失 / id 重复 ⇒ 必红

- **WHEN** 分别构造:bridge 指向**未声明**实例 · 实例 `family` **缺失或不可解析** · bridge 的 `modules` **为空或引用不在拓扑**的模块 · `bridge_id` **缺失或重复** · `cep.bridges` 为空 · **required** 实例的 plane 绑定**为空或缺失** · plane 名**未登记**
- **THEN** 装配闸 SHALL 判红

#### Scenario: BSS-01 —— 跨 entry 重叠(同一模块属多个 bridge entry)⇒ 必红

- **WHEN** 构造 `cep.bridges[]` 中**两条不同 `bridge_id`** 的 entry,其 `modules` 均含同一模块名
- **THEN** 装配闸 SHALL 判红且诊断含归因 tag `[BSS-01]` 与该模块名的**出现计数**

#### Scenario: BSS-01 —— 单 entry 内重复列同一模块 ⇒ 必红

- **WHEN** 构造**单条** bridge entry,其 `modules` 列出同一模块名**两次**(如 `["w","chi","w"]`)
- **THEN** 装配闸 SHALL 判红且诊断含归因 tag `[BSS-01]`
- **AND** ⛔ 该子情形 SHALL **不得**被「跨 entry 重叠」的口径收窄排除

#### Scenario: BSS-01 —— 装配路径 SHALL 执行同一判据(执行臂)

- **WHEN** `ModuleFactory::instantiateAll()` 装配的拓扑中,同一模块名出现在多个 bridge entry 的 `modules` 展开序列中
- **THEN** 装配 SHALL **失败**(返回 false)且诊断含归因 tag `[BSS-01]`
- **AND** 装配路径与装配闸 SHALL **共用同一份判据实现**(`include/tlm/cep/bridge_module_membership.hh` —— **唯一实现**;⛔ **禁复制实现**)
- **AND** 装配路径 SHALL 在**桥挂载循环之前**执行该检查 ⇒ 挂载循环**不进入** ⇒ **零桥挂载**(桥挂载槽不被后挂的桥覆盖)
- ⚠️ **失败原子性边界**:该失败发生在对象已创建之后 ⇒ **不新增回滚机制**;⛔ **不得**表述为「零副作用」
- ⚠️ **诊断优先级**:该检查**短路** ⇒ 同一 config 若同时违反其它桥校验条件,**只报** `[BSS-01]` 一类原因 ⇒ **结构性关口先于逐条桥校验**
