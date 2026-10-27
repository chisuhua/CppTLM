# cep-protocol-bridge-contracts Specification

## Purpose
定义 **ProtocolEngine → 具体通信网络**的**特化桥**契约(per `ADR-CEP-19` 决策 3~5):`ProtocolBridgeEr`
(类型擦除基类)+ `ProtocolBridge<InB, OutB, Ordered>`(模板子类,**首次落地**,per `06 号 §4.4` 原型)
+ per-bridge 静态元数据(`protocol_id_` / `bridge_id_`,per `06 号 §4.5` P1 范围)
+ `register_protocol_bridge` 钩子从**死钩子**变**活钩子**的「**注册 + 消费成对**」纪律。

本能力是 `01 号 §1.1` **L5↔L4 seam** 的**可执行形态** —— 把 CohIf 业务语义翻译成**具体网络 Bundle**
(`NoCFlitBundle` / `Axi4Bundle` / …);换一种网络 Bundle 即换一种通信网络,**协议特化点收敛在桥内**。

跨实例复用:后续 7 个 adapter change(`chi-rnf` / `mesif` / `cxl-nonsymmetric` / `tilelink` /
`cxl-dcoh-symmetric` / `nvlink-sharp` / `ualink-inc`)**复用**本能力,只填各自的行。

## ADDED Requirements

### Requirement: cep-protocol-bridge-contracts

CEP SHALL 将 **CohIf → 具体通信网络**的特化桥固化为 **3 条可机器检查的判据**:`pack`/`unpack` **双向可逆** / per-bridge 元数据齐备 / 挂载钩子**注册与消费成对**。

> **实现 / 边界**:`include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` + `ProtocolBridge<InB, OutB, Ordered>` + `AbstractProtocolBridge` identity 基线);**不**引入新 StreamAdapter 子类 / factory 抽象层(per `01 号 §1.1`「不需新增抽象层」+ `ADR-CEP-19` 决策 8);**新增 invariant 数 = 0**。

#### Scenario: CC-03 —— `ProtocolBridge` 的 `pack` / `unpack` SHALL **双向可逆**

- **WHEN** 对 `CohIfBundle → NoCFlitBundle` 的桥做一次 `pack` 后 `unpack`
- **THEN** 子条件 ① SHALL 成立:往返后的 `CohIfBundle` 与原值**逐字段相等**(逐字段 round-trip 恒等)
- **AND** 子条件 ② SHALL 成立:`NoCFlitBundle` 侧承载**网络层**语义(`src_node` / `dst_node` / `vc_id` / `flit_category`),而 `CohIfBundle` 侧**不**含这些字段(⇒ 职责边界可判定)
- **AND** 子条件 ③ SHALL 成立:同一 `ProtocolBridgeEr*` SHALL 能被**多个**具体桥实例共享(类型擦除基类的存在理由)
- **AND** **反向 fixture**:篡改任一字段映射(如把 `perm` 映射到 `flit_category`)⇒ round-trip 断言**必红**

> **锚点**:`test_cep_protocol_bridge_contracts.cc` 的 CC-03 三子条件断言 + 字段映射篡改反向 fixture

#### Scenario: CC-05 —— `register_protocol_bridge` SHALL 从**死钩子**变**活钩子**

- **WHEN** 注册一个协议桥
- **THEN** 子条件 ① SHALL 成立:注册后该桥名 SHALL **可读**(⇒ 不再是 write-only 死钩子;实测现状:`get_bridges()` 位于 `private:` 段,全仓无人读)
- **AND** 子条件 ② SHALL 成立:该桥 SHALL 被 ModuleFactory **消费**(实例化时真正参与装配)⇒ **注册与消费成对**
- **AND** 子条件 ③ SHALL 成立:钩子 SHALL 支持 `(bridge_name, factory_lambda)` 形态(per `06 号 §9.3`)且**不**引入独立的 factory 抽象类(per `06 号 §11.2` P3 子项的**显式偏离**)
- **AND** 子条件 ④ SHALL 成立:per-bridge 元数据 SHALL 齐备 —— `protocol_id_` 非零、`bridge_id_` 唯一(同一 ModuleFactory 内不重复)、`protocol_id_` 与该协议实例声明的 `family` 一致(per `ADR-CEP-12` 的 13 属性)
- **AND** **反向 fixture**:只注册而**不**消费 ⇒ 断言**必红**(否则是新的死钩子)
- **AND** **反向 fixture**:两个桥用**同一** `bridge_id_` ⇒ 断言**必红**
- **AND** **反向 fixture**:`protocol_id_` 与实例声明的 `family` 不符 ⇒ 断言**必红**

> **锚点**:`test_cep_protocol_bridge_contracts.cc` 的 CC-05 四子条件断言 + 3 条反向 fixture(只注册不消费 / 重复 `bridge_id_` / 与声明不符)

#### Scenario: CC-06 —— 6→3 映射表产出的 `CATEGORY_*` SHALL 全部落在网络 Bundle 的 3 个常量内

- **WHEN** 核对队列 #6 的 6→3 消息类映射表产出的链路层类别
- **THEN** 子条件 ① SHALL 成立:产出的每个类别 SHALL ∈ `NoCFlitBundle` 的 **3** 个 `CATEGORY_*` 常量(`REQUEST` / `RESPONSE` / `CREDIT`,per `include/bundles/noc_bundles_tlm.hh:60-62`)
- **AND** 子条件 ② SHALL 成立:映射表 SHALL **非恒等**(至少一行 ≠ 同名),否则特化字典无意义
- **AND** 子条件 ③ SHALL 成立:**本判据 SHALL 引用**队列 #6 `cpptlm-cep-moesi-gpu-adapter` 的 **AC-04** 实现 —— **不重立第二份**(避免两份判据漂移)
- **AND** **反向 fixture**:产出第 4 类 ⇒ 断言**必红**(per 队列 #6 的 AC-04 反向 fixture)

> **锚点**:`test_cep_protocol_bridge_contracts.cc` 的 CC-06 断言(引用队列 #6 的 AC-04 实现)+ 队列 #6 的既有反向 fixture
