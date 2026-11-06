# cep-ualink-instance-assembly Specification (delta)

## ADDED Requirements

### Requirement: cep-instance-assembly

CEP 协议实例的**实例级装配** SHALL 由**三层证据**共同判定 —— ① **声明层**:实例出现在 `protocol_instances.json` 的 `instances[]`;② **拓扑层**:实例出现在某配置的 `cep.bridges[].protocol_instance` 且该 bridge 引用的模块**确实在拓扑中**;③ **平面层**:实例在 `collective_planes.json` 的 `plane_binding` 中的绑定**非空**且**已在 plane 注册表登记**。

判定 SHALL 由**装配闸**(`instance_assembly_gate.hh`)执行,归因 tag 为 `[IAG-01]` ~ `[IAG-07]`,且 SHALL 具备**反向 fixture**(每条判据都有「按正确原因红」的负控)。⚠️ 本闸只判**装配完整性**,⛔ 不判协议语义(语义判据在 collective plane 契约面 CP-03 与 channel dependency 闸)。

#### Scenario: IAG-01 —— 三层证据齐备 ⇒ 装配闭合

- **WHEN** 对含 `cep.bridges[]` 的拓扑跑装配闸
- **THEN** 每个被挂载的实例 SHALL 在声明层存在 · `family` SHALL 可解析 · bridge 引用的模块 SHALL 在拓扑中 · `bridge_id` SHALL 唯一
- **AND** 若 `plane_binding` 含该实例键 ⇒ 绑定 SHALL 非空且 SHALL 已在 plane 注册表登记

#### Scenario: IAG-02 —— 负控:未声明 / 未绑定 / 未登记 / 模块缺失 / id 重复 ⇒ 必红

- **WHEN** 分别构造:bridge 指向**未声明**实例 · `plane_binding` 值为空 · plane 名**未登记** · bridge 引用**不在拓扑**的模块 · `bridge_id` **重复** · `cep.bridges` 为空
- **THEN** 装配闸 SHALL 判 FAIL,且错误信息 SHALL 命中对应归因 tag(`[IAG-01]` ~ `[IAG-07]`)
