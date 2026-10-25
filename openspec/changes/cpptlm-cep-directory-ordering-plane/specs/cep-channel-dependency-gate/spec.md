## ADDED Requirements

### Requirement: cep-channel-dependency-gate

CEP SHALL 将 **装配期通道依赖图**(顶点 `(协议实例, network, vc_id)` 三元组)+ **三态 SKIP/PASS/FAIL** + **verdict 逐 (network, vc)** 固化成**可执行契约**。

> **定位**:per `ADR-CEP-09 Decision A` (top-level view) + `INV-CEP-09-IC1`;本 capability 承接 **S1-1 ②建图层**(per Entry #55)。
> **输入源**:`include/tlm/cep/protocol_instances.json`(本仓机器可读声明,per **裁决 D-γ**;**诞生即不冻结**,per Entry #56 先例)。
> **红绿**：「判据实现退化」= 用例红;「判定结论为红」= 用例**仍绿**且结论**登记为交付物**。
> **不可验证条款**：**SKIP 的形态**是真动作的「**打日志**」而非「装作 PASS」(per metis blocker);verdict 走日志通道,断言用正则匹配。

#### Scenario: 顶点 = `(协议实例, network, vc_id)` 三元组(per `ADR-CEP-09 §8.2`)

- **WHEN** 构造通道依赖图的顶点
- **THEN** 顶点 SHALL 形如 `(instance_id, network_id, vc_id)` 三元组(**不**再叫 `flat(实例, vc_id)`)
- **AND** `vc_id` 值域 = `uint8_t` 0-3(per `noc_bundles_tlm.hh:38` 和 `fabric.schema.json.maximum=4`,per `ADR-CEP-09 决议`「依赖传播契约」)
- **AND** 与 S3 spike 同型(per `cep_poc_spike_s3.cc:185-207` 顶点叙),**不硬编码镜像**

> **锚点**:`test_cep_channel_dependency_gate.cc` 的顶点三元组断言

#### Scenario: 边由 `vc_binding` + `ordering_assumption` 导出(**不硬编码镜像**)

- **WHEN** 构造通道依赖图的边
- **THEN** 边 SHALL 由 `protocol_instances.json` 的 `channel_dependency_edges`(**ArchForge `32 号 §6.0.3` 的派生副本**)导出;`vc_binding` 只决定**顶点集**,不决定边
- **AND** 判据实现(`channel_dependency_graph.hh`)SHALL **不含**任何网络名/边字面量(去注释扫描 + 正控守;边表漂移只能由**声明**漂移体现)
- **AND** 抗漂移角度(S1-1 ②建图层 per Entry #55):**`protocol_instances.json` 的内容若与 ArchForge `32 号 §6.1/§6.2` 的镜像不符**(那是唯一权威)⇒ parser **判红**
- **AND** 同轴化检查(per Entry #55):fixture 与 `cep_poc_spike_s3.cc` 的同一 8-顶点/8-边结论**一致**

> **锚点**:`test_cep_channel_dependency_gate.cc` 的边由声明导出断言

#### Scenario: 无环 = **PASS**(8 顶点 / 8 边 / **0 跨网边**)

- **WHEN** `moesi_gpu` fixture 跑通后
- **THEN** 断言 SHALL 返回 `(8, 8, 0)`(**比 S3 spike 的硬编码值少一个**:per `cep_poc_spike_s3.cc` 的机器可导出的等值形态)
- **AND** 顶点 / 边的**展开值** SHALL 与 S3 spike 对账(Entry #55 的「镜像↔权威抗漂移**已闭合**」)

> **锚点**:`test_cep_channel_dependency_gate.cc` 的 moesi_gpu fixture 断言

#### Scenario: 注入环 ⇒ 配置 **FAIL**(**反向验证,必需**)

- **WHEN** `protocol_instances.json` 中注入**成环**的 `vc_binding`(如 request→VC1 / response→VC0 互等)
- **THEN** `instantiateAll` 整条管线 SHALL **显式返回 false**(per `INV-CEP-09-IC1`,配置 fail,**不得以运行时替代**)
- **AND** verdict SHALL **逐 `(network, vc)` 标注**(per `ADR-CEP-09 §8.3`)——**不得只给全局结论**

> **锚点**:`test_cep_channel_dependency_gate.cc` 的注入环 fail + verdict 逐 (network, vc) 断言

#### Scenario: verdict 输出通道 SHALL 机器可 grep(per format `[verdict]` 前缀)

- **WHEN** 检查 `instantiateAll` 全链路跑完后外部输出
- **THEN** SHALL **逐 (network, vc) 输出**判定;**格式** = **`[verdict] net=<network> vc=<id> PASS|FAIL (from Alice...)`**(per metis 建议 #7)
- **AND** verdict **不得**只给全局 TODO 语(per `ADR-CEP-09 §8.3`)

> **锚点**:`test_cep_channel_dependency_gate.cc` 的 verdict 逐 (network, vc) 格式断言

#### Scenario: `protocol_instances.json` **缺失 / 为空 ⇒ SKIP**(三态显式化)

- **WHEN** `include/tlm/cep/protocol_instances.json` 不存在或为空(本仓的**当前真实状态**)
- **THEN** 闸 SHALL 显式打印 **`[SKIP]`**(与 PASS **可区分**),per `cep_decl_alignment_check.sh` 的 `[SKIP]` 先例
- **AND** **不得**把空文件装作 **PASS**(per metis blocker + 判据 ③ 纯负向断言需正控)

> **锚点**:`test_cep_channel_dependency_gate.cc` 的 SKIP / PASS 可区分断言

#### Scenario: `validate_topology` 与 `instantiateAll` 双挂 SHALL 共享同一判据实现

- **WHEN** 运行 `cmake --build build --target validate_topology` 或跑 fixture 的 `instantiateAll(config)` 端到端链
- **THEN** 两处 SHALL **共享同一判据实现**(`channel_dependency_graph.hh`),**禁双实现**(per metis 建议 #8 + 纪律 ⑤「守卫须与它守护的命令等价」)

> **锚点**:`test_cep_channel_dependency_gate.cc` 的共享判据实现断言(两挂点**同一**函数签名)
