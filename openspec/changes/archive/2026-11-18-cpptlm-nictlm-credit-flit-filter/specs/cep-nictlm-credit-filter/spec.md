# cep-nictlm-credit-filter Specification (delta)

## ADDED Requirements

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

> **锚点**：`src/tlm/nic_tlm.cc` 的 `handle_net_resp` · `test/test_nictlm_credit_filter.cc`(AC-01 / T2.1c)
> **同款判据的一侧**：`openspec/specs/cep-cohif-network-adapter/spec.md` 的「网络 ingress SHALL 丢弃 credit flit」Scenario（`CohIfNICTLM` 侧 · 已由 change `cpptlm-router-credit-flit-leak-to-consumer` 修复）⇒ 两 capability 行为**可对拍**
> **⛔ 不改变端口角色**：`openspec/specs/port-type-system/spec.md` 中 `NICTLM` 的 `PE` / `NETWORK` 端口声明**保持不变**（本决策只终止「credit 被当数据消费」，不改端口契约）

#### Scenario: 同 transaction_id 的在途包 SHALL NOT 被 credit flit 劫持

- **WHEN** `NICTLM` 的 `pending_packets_` 中存在某个 `transaction_id == T` 的在途包，且此时收到 `flit_category == CATEGORY_CREDIT` 且 `transaction_id == T` 的 flit
- **THEN** 本模块 SHALL **丢弃**该 credit flit
- **AND** SHALL **NOT** 新建 / 改写 / 提前 `erase` 该在途包条目
- **AND** 后续同 `T` 的 TAIL flit 到达时 SHALL 产出**恰好一条**响应（⛔ 不得因 credit 参与重组而多产 / 少产）

> **锚点**：`src/tlm/nic_tlm.cc` 的 `reassemble`（以 `transaction_id` 单键查 `pending_packets_`）· `test/test_nictlm_credit_filter.cc`(AC-06 / T2.1b)
> **为何必需**：⛔ 「credit 被当数据消费」的后果不止是「垃圾响应」—— credit 若被当作 HEAD 处理会**新建在途包条目**（`FLIT_HEAD` 分支 `push_back` 且该条目**永不清除**）⇒ 污染在途包状态比计数虚增更严重。

#### Scenario: 观测面 SHALL 暴露丢弃计数

- **WHEN** 读取本模块的公开观测 API
- **THEN** 其 SHALL 提供 `credits_dropped()`（与 `CohIfNICTLM::credits_dropped()` **同名同义**）
- **AND** `get_stats_group()` 中 SHALL 存在键名 `credits_dropped` 的标量
- **AND** 该计数 SHALL **仅**对 `flit_category == CATEGORY_CREDIT` 的 flit 递增
- **AND** SHALL **NOT** 对 REQUEST / RESPONSE 类 flit 递增
- **AND** SHALL **NOT** 混入 `flits_received` / `packets_received`

> **锚点**：`src/tlm/nic_tlm.cc` 的 `stats_credits_dropped_` · `include/tlm/nic_tlm.hh` 的 `credits_dropped()` · `test/test_nictlm_credit_filter.cc`(AC-01 / AC-04 / T2.3)
> **对称性依据**：`CohIfNICTLM` 侧已有同款 API（`include/tlm/cep/cohif_nic.hh`）⇒ 两侧可直接对拍
> **⚠️ 计数边界**：该计数**看不到** adapter 层的静默丢包（`DualPortStreamAdapter::process_net_response_input` 在 `resp_in.valid()` 为真时整包丢弃且 `Packet` 不 release）—— 那部分 credit **到不了**本模块 ⇒ 本计数**不是**「本 NIC 收到的全部 credit 数」
