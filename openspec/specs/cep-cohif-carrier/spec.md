# cep-cohif-carrier Specification

## Purpose
定义 **CohIf 承载形态**(CEP 6 层架构里 **Coherence Interface** 层的通信载体,per `ADR-CEP-19` 决策 1~9)的可执行契约:
`bundles::CohIfBundle`(**单一**业务 Bundle,**只覆盖 5 条 RN-side 流**,以 `flow_id` 区分)+
ProtocolEngine 基类(`ChStreamModuleBase` 派生 + `Input/OutputStreamAdapter<CohIfBundle>`)+
`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏与 `register_protocol_bridge` 钩子扩展(**JSON 装配**)。

本能力把 `01 号 §1.1` 的 **L3 ChStream + L4 StreamAdapter** 机制**首次**用到 CEP 的 CohIf 层 ——
此前 CEP 的 7 个头文件**零** `Bundle`/`ChStream` 引用(断点 C1),而 canonical 10 流的承载形态在
`ADR-CEP-07A:49` 与 `ADR-CEP-16:45` **两处**都是悬空引用(断点 C2)。

交付**载体**,**不**交付协议语义 —— 后者归 ArchForge(per `06-bis` 顶部处置表);
`StubCacheCore` **不动**(决策 9,显式延期到 `cpptlm-cep-stub-cache-chstream-surface`)。

## Requirements

### Requirement: cep-cohif-carrier

CEP SHALL 为 **CohIf 提供承载形态**:一个**业务 Bundle**(`CohIfBundle`,**只覆盖 5 条 RN-side 流**)+ ProtocolEngine 的 **ChStream 端口面** + **JSON 装配**连通性。

> **形态 / 边界**:`CohIfBundle`(`include/bundles/cep_coherence_bundles_tlm.hh`)+ `protocol_engine_base.hh`(`ChStreamModuleBase` 派生)+ `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏 —— **不进** 23 ABI · **不改** frozen header · **新增 invariant 数 = 0**;`StubCacheCore` **不动**(D-ζ)。

#### Scenario: CC-01 —— `CohIfBundle` SHALL **只含 5 条 RN-side 流**且**不含**网络层字段

- **WHEN** 核对 `CohIfBundle` 的 `flow_id` 域与字段集
- **THEN** 子条件 ① SHALL 成立:`flow_id` 域枚举 **恰好** 5 个值 —— `request` / `completion` / `snoop` / `snoop_resp` / `writeback`
- **AND** 子条件 ② SHALL 成立:字段集 SHALL **不含**任何网络层字段(`src_node` / `dst_node` / `flit_category` / `vc_id`)
- **AND** 子条件 ③ SHALL 成立:`barrier` / `forward_data` / `back_invalidate` / `address_resolve` / `snoop_completion_ack` **不可能**出现在该 Bundle 的 `flow_id` 域内(⇒ 「对 stub 不可见」是**类型保证**,非运行期纪律)
- **AND** **反向 fixture**:新增第 6 个 `flow_id` ⇒ **静态扫描用例必红**(参照 `cep-stub-cache-core` spec 的「三流名不出现」先例)
- **AND** **反向 fixture**:向字段集加入任一网络层字段 ⇒ **静态扫描用例必红**

> **锚点**:`test_cep_cohif_carrier.cc` 的 CC-01 三子条件断言 + 2 条反向 fixture(加 `flow_id` / 加网络层字段)

#### Scenario: CC-02 —— `BUNDLE_FIELD_WIDTH` 编译期锁与 `is_trivially_copyable` SHALL **生效**

- **WHEN** 核对 `CohIfBundle` 的编译期约束
- **THEN** 子条件 ① SHALL 成立:test TU 内 `static_assert(sizeof(CohIfBundle) == 期望值)` **通过**
- **AND** 子条件 ② SHALL 成立:`static_assert(std::is_trivially_copyable_v<CohIfBundle>)` **通过**
- **AND** 子条件 ③ SHALL 成立:`BUNDLE_FIELD_WIDTH` 宏 SHALL 覆盖 `CohIfBundle` 的**全部**字段
- **AND** **反向 fixture**:以「字段宽度 +1」的**毒化副本**跑 `try_compile` ⇒ 编译**必失败**
- **AND** **反向 fixture**:为 `CohIfBundle` 加虚函数 ⇒ `is_trivially_copyable` 断言**必失败**

> **锚点**:`test_cep_cohif_carrier.cc` 的 CC-02 静态断言组 + `try_compile` 毒化副本用例(⚠️ 编译期锁**不得**只以「编译红」充当 ctest 判据)

#### Scenario: CC-04 —— JSON 装配后 CohIf 事务 SHALL **穿过** adapter 抵达对端,且 **N ≥ 2**

- **WHEN** 用 `REGISTER_CHSTREAM_BIDIRECTIONAL` 注册一个 ProtocolEngine 实例并走 JSON 端口装配
- **THEN** 子条件 ① SHALL 成立:一次 CohIf 事务可**穿过** adapter 抵达对端模块(端到端断言,非仅绑定成功)
- **AND** 子条件 ② SHALL 成立:该实例的端口数 `N` **≥ 2**(`isMultiPort` 要求 `port_count > 1`)
- **AND** 子条件 ③ SHALL 成立:`N = 1` 的实例 SHALL 在**编译期**被 `static_assert(n > 1)` 拒绝(防 `bind_ports` 空操作 shim 导致**静默不绑定**)
- **AND** **反向 fixture**:断开某端口的绑定 ⇒ 端到端断言**必红**
- **AND** **反向 fixture**:以 `N = 1` 编译 ⇒ **必编译失败**

> **锚点**:`test_cep_cohif_carrier.cc` 的 CC-04 端到端断言 + `N=1` 编译失败用例 + 断链反向 fixture
