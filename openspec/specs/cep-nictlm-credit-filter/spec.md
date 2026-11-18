# cep-nictlm-credit-filter Specification

## Purpose
定义通用网卡（`NICTLM`）对 **in-band credit flit** 的消费边界 —— credit flit 在网络 ingress
被**丢弃**，不进入计数、不进重组、不产出业务响应、不污染在途包状态；与 `CohIfNICTLM` 侧同款判据
（bundle 字段 `flit_category`，⛔ 不用 `Packet::type`）⇒ 两侧观测面同名同义、可直接对拍。

⛔ **限界**：本规格只保证**消费者不被污染**；credit 仍按既有链路语义回流并占用带宽
（`RouterTLM` 侧语义不变），`NICTLM` 仍**不归还** credit —— router 下行流控属 `ADR-X.16` 与
capability `router-local-port-flow-control` 的范围。

## Requirements

### Requirement: nictlm-credit-flit-filter

`NICTLM` SHALL 在网络 ingress **丢弃** in-band credit flit —— 即 `flit_category == CATEGORY_CREDIT` 的 flit SHALL **NOT** 被当作数据 flit 消费。判据 SHALL 使用 **bundle 字段** `flit_category`（传输无关），⛔ **NOT** 使用 `Packet::type`。

> **限界（非规范性）**：本要求只保证**消费者不被污染**；credit 仍按既有链路语义回流并占用带宽（`RouterTLM` 侧语义**不变**），`NICTLM` 仍**不归还** credit。另一面（router 下行流控）见 `ADR-X.16` 与 capability `router-local-port-flow-control`。⚠️ **「补完两个消费者」不等于「credit 泄漏已根治」** —— 任何挂在 router `resp_out` 上的**非 credit-aware** 消费者都须自我防御。

#### Scenario: NICTLM SHALL 丢弃 credit flit

- **WHEN** `NICTLM` 的网络侧 ingress 收到 `flit_category == CATEGORY_CREDIT` 的 flit
- **THEN** 本模块 SHALL **丢弃**该 flit
- **AND** SHALL **NOT** 将其计入 `flits_received`
- **AND** SHALL **NOT** 将其计入 `packets_received`
- **AND** SHALL **NOT** 将其送入 `reassemble`
- **AND** SHALL **NOT** 向 `pe_resp_out` 写出任何响应（⛔ 否则产出事务号 / 地址 / 数据全 0 的垃圾响应）
- **AND** 该丢弃 SHALL **NOT** 影响同一 ingress 周期内其它 flit 的处理

#### Scenario: credit flit SHALL NOT 劫持同 tid 的在途包

- **WHEN** `NICTLM` 的 `pending_packets_` 中存在 `transaction_id == T` 的在途包，且随后收到 `flit_category == CATEGORY_CREDIT` 且 `transaction_id == T` 的 flit
- **THEN** 该 credit flit SHALL 被丢弃
- **AND** 在途包状态 SHALL **NOT** 被该 credit flit 改写（⛔ 不新建、不污染）
- **AND** 当同 `tid == T` 的包尾 flit 后续抵达时，本模块 SHALL **恰好产出 1 条**响应

#### Scenario: credit 丢弃计数 SHALL 与 CohIfNICTLM 侧同语义可对拍

- **WHEN** 同一 credit flit 分别注入 `NICTLM` 与 `CohIfNICTLM`
- **THEN** 两侧 SHALL **均**丢弃该 flit（⛔ 不计入计数 / 不产出业务流）
- **AND** 两侧的 `credits_dropped()` 观测面 SHALL **同名同义**（同判据形态 ⇒ 可直接对拍）
