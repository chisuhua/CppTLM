# cep-cohif-network-adapter Specification

## Purpose
让一致性事务能经由可路由的网状互联抵达对端并带回响应：网络侧目标节点由地址区间解析填充，未命中一律显式丢弃而不静默改投，跨网络的报文类别投影被如实登记为有损。

## Requirements

### Requirement: cep-cohif-network-adapter

系统 SHALL 提供 **`CohIfNICTLM`** —— CEP 的网络接口模块(第五类 CEP 模块:adapter / engine / bridge / home 模型 / **network adapter**),使 `CohIfBundle` 的 coherence 流量可经 `RouterTLM` 构成的 XY mesh **抵达对端**并带回响应。

#### Scenario: 模块 SHALL 以异构双端口面暴露 CohIf 与网络两侧

- **WHEN** 构造 `CohIfNICTLM`
- **THEN** PE 侧 SHALL 为 `CohIfBundle`(`pe_req_in` / `pe_resp_out`),网络侧 SHALL 为 `NoCFlitBundle`(`net_req_out` / `net_resp_in`)
- **AND** 两侧 Bundle 类型**不相等** SHALL 由编译期断言固定
- **AND** 注册 SHALL 走 `registerDualPortAdapter`(⛔ **不得**用要求两侧同 BundleT 的对称 `REGISTER_CHSTREAM_BIDIRECTIONAL`)

> **锚点**:`include/tlm/cep/cohif_nic.hh` 的端口面声明 + `test/test_cep_mesh_e2e.cc` 的 `static_assert(!is_same_v<...>)`

#### Scenario: 模块 SHALL 承担 `dst_node` 的填充职责

- **WHEN** 一笔 `CohIfBundle` 请求被发往网络侧
- **THEN** 其 `NoCFlitBundle.dst_node` SHALL 由**本模块**按 `address` 经 `address_regions` 查得并写入
- **AND** 该职责 SHALL **NOT** 由 `CohIfNoCProtocolBridge` 承担(⛔ 桥明确「无权改写路由器负责的字段」)
- **AND** 理由 SHALL 为:`RouterTLM` 的路由目标**只**从 `flit.bundle.dst_node` 读取

> **锚点**:`src/tlm/cep/cohif_nic.cc` 的正向映射 + `src/tlm/router_tlm.cc` 的 `dst_node.read()` 读取点

#### Scenario: 地址映射未命中 SHALL 显式失败而非静默改投

- **WHEN** 请求的 `address` 不落在任何 `address_regions` 内
- **THEN** 本模块 SHALL **丢弃**该请求并递增 `stats_dropped_unmapped_`
- **AND** SHALL **NOT** 将其改投到节点 0
- **AND** 该行为 SHALL **NOT** 复制 `NICTLM::lookup_node` 的静默兜底

> **锚点**:`test/test_cep_mesh_e2e.cc` 的未命中用例(断言丢弃计数递增且未发出 flit)

#### Scenario: 单 flit 承载 SHALL 成立

- **WHEN** 本模块打包一笔 `CohIfBundle`
- **THEN** 其 `flit_count` SHALL 为 1 且 `flit_type` SHALL 为 `FLIT_HEAD_TAIL`
- **AND** 理由 SHALL 为:`CohIfBundle.data` 是 **1 拍**载荷(⛔ 不需要分片)

> **锚点**:`test/test_cep_mesh_e2e.cc` 的 flit 字段断言

#### Scenario: `flit_category` 投影 SHALL 被登记为有损且 SHALL NOT 承诺多流无损

- **WHEN** 5 条跨 Stub↔PE 边界的 `flow_id` 被投影到 `NoCFlitBundle.flit_category`
- **THEN** 该投影 SHALL 被登记为**有损**(`flit_category` 仅 3 值,装不下 5 个 `flow_id`)
- **AND** 本模块 SHALL **NOT** 声明支持多流无损恢复
- **AND** 本模块 SHALL **NOT** 承载协议语义(credit / retry / ordering point / 多拍聚合 / Perm 规则 / 容量与队列模型)

> **锚点**:`test/test_cep_mesh_e2e.cc` 的 `flit_category` 断言 + 本 spec 的边界句

#### Scenario: 观测面 SHALL 只暴露原始事实

- **WHEN** 读取本模块的公开观测 API
- **THEN** 其 SHALL 提供 `stats_flits_sent_` / `stats_flits_received_` / `stats_dropped_unmapped_` / `last_dst_node()`
- **AND** SHALL **NOT** 提供 latency 聚合 / 指标组装 / 分位统计(⛔ 聚合归 `test/` 的 harness)

> **锚点**:`test/test_cep_mesh_e2e.cc` 的 API 面断言

#### Scenario: 模块 SHALL 不进入协议实例声明面且不计入 B3b

- **WHEN** 检视 `protocol_instances.json` / `protocol_instance.schema.json` / `fabric.json`
- **THEN** 本模块 SHALL **NOT** 出现在其中(⛔ 它不是协议实例)
- **AND** `B3b-2` SHALL 维持 **0/4**
- **AND** 本模块 SHALL **NOT** 作为 T1–T4 的条款级证据

> **锚点**:`test/cep_b3b2_deferrals.json` 零 diff + `git diff` 零 diff 断言

#### Scenario: 端到端 SHALL 证明流量穿过真实 mesh 而非直达

- **WHEN** 在 `RouterTLM` 构成的 mesh 上装配 `CohIfNICTLM` × 2 + `StubCacheEndpointTLM` + 协议引擎 + home 模型
- **THEN** 一笔请求 SHALL 抵达**对端** NIC 的**入向**(且 `last_recv_dst_node()` 为对端节点号,⛔ 不得为自身;⚠️ 观测的是**收到的** flit,⛔ 不是本模块出站查表结果)
- **AND** 参与路径的 router 的 `flits_forwarded` 与 `packets_forwarded` SHALL 均 > 0
- **AND** 对端 NIC 的 `last_recv_hops()` SHALL ≥ 1(router 转发时 `hops` 递增 ⇒ 直连的 flit `hops == 0`)
- **AND** ⛔ 「抵达」断言 SHALL **NOT** 被当作「穿过 mesh」的**唯一**证据(直达亦能抵达)

> **锚点**:`test/test_cep_mesh_e2e.cc` 的 J-2 / J-4 判据
