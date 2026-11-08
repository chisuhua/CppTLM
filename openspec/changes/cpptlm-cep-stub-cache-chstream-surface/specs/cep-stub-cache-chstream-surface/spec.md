# cep-stub-cache-chstream-surface Specification (delta)

## ADDED Requirements

### Requirement: cep-stub-cache-chstream-surface

系统 SHALL 提供 **`StubCacheEndpointTLM`**(可注册的 ChStream 端点)与 **`CohIfResponderTLM`**(home 简化模型),使 `TrafficGenTLM` 的真实业务流量可经 `StubCacheCore` 抵达协议引擎与 home 应答端。

#### Scenario: 端口面 wrapper SHALL 保持 `StubCacheCore` 为纯类

- **WHEN** 检查 `include/tlm/cep/stub_cache_core.hh`
- **THEN** 该文件 SHALL **零 diff**(⛔ 不加 ChStream 端口、⛔ 不派生 `ChStreamModuleBase`)
- **AND** `StubCacheEndpointTLM` SHALL 内部**持有** `StubCacheCore` 实例(组合,非继承)
- **AND** `INV-SC1` ~ `INV-SC6` 与既有 14 个单测 SHALL 全绿

> **锚点**:`test/test_cep_stub_cache_e2e.cc` 的 wrapper 构造 + `git diff` 零 diff 断言

#### Scenario: wrapper 两侧 Bundle SHALL 异构

- **WHEN** 构造 `StubCacheEndpointTLM`
- **THEN** cpu 侧 SHALL 为 `CacheReqBundle` / `CacheRespBundle`,CohIf 侧 SHALL 为 `CohIfBundle` / `CohIfBundle`
- **AND** 两侧类型**不相等** SHALL 由编译期断言固定(⛔ 不得退化为对称单 BundleT)

> **锚点**:`test/test_cep_stub_cache_e2e.cc` 的 `static_assert(!is_same_v<...>)`

#### Scenario: wrapper SHALL 只暴露原始观测事实

- **WHEN** 读取 wrapper 的公开观测 API
- **THEN** 其 SHALL 提供 per-txn **issue / complete 周期戳**与 **in-flight 计数**(原始事实)
- **AND** SHALL **NOT** 提供 latency 聚合 / 指标组装 / 分位统计(⛔ 实验装置不得升格进契约面;聚合归 `test/` 的 harness)

> **锚点**:`test/test_cep_stub_cache_e2e.cc` 的 API 面断言

#### Scenario: 命中路径 SHALL 就地回 CPU 侧响应且保持零网络流量

- **WHEN** `cpu_side` 命中
- **THEN** wrapper SHALL **就地生成** `CacheRespBundle`(一请求一响应恒成立)
- **AND** `net_side` SHALL **不**产生任何 flit(`INV-SC6` 的 wrapper 侧等价断言)
- **AND** wrapper 的 CohIf egress **SHALL NOT** 被表述为「协议引擎发射」

> **锚点**:`test/test_cep_stub_cache_e2e.cc` 的 hit 路径计数断言

#### Scenario: home 简化模型 SHALL 承载**最小**应答语义

- **WHEN** `CohIfResponderTLM` 收到 `Request`
- **THEN** 其 SHALL 经**参数化延迟**生成 `Completion`
- **AND** 其 SHALL 按读请求**可配发** `Snoop`(可配 = 默认关,开时按显式配置)
- **AND** 其收到 `Writeback` SHALL **接收即消费、零响应**(fire-and-forget)
- **AND** 其 SHALL **NOT** 承载家族级 / 条款级语义(**credit · retry · ordering point · 多拍聚合 · Perm 规则 · 容量与队列模型**)
- **AND** 其 SHALL **NOT** 出现于 `protocol_instances.json`(⛔ 不是协议实例;per `ADR-CEP-19` SU · 三 裁定**甲**)

> **锚点**:`test/test_cep_stub_cache_e2e.cc` 的 `Completion` 回流 + `minimal-semantics` 负向断言

#### Scenario: 拓扑 SHALL 两档且 SHALL NOT 重叠(J-9)

- **WHEN** 装配两份 config
- **THEN** 拓扑 **A**(`configs/cep_chi_rnf_e2e_a.json`)SHALL 省略 `cep.bridges` 键 ∧ 全部 `connections` 的 `latency` SHALL 为 `0` ⇒ 装配闸 SHALL `SKIP`
- **AND** 拓扑 **B**(`configs/cep_chi_rnf_e2e_b.json`)SHALL 恰有 **1** 条 `cep.bridges` ∧ 该条 `modules` SHALL 恰为 **3** ∧ **至少 1** 条 `connections` 的 `latency` SHALL `> 0` ⇒ 装配闸 SHALL `PASS`
- **AND** B 的 bridge entry SHALL 带 `protocol_instance` 且 SHALL 为**已声明**实例(满足 `[IAG-02]`)
- **AND** 三个模块 SHALL 各自暴露于同一 `bridge_id` 的桥指针(同一投影语境)
- **AND** ⛔ 拓扑 B SHALL **NOT** 承载「13 键全绿」

> **锚点**:`test/test_cep_stub_cache_e2e.cc` 的 J-9 分档断言

#### Scenario: 台账元数据 SHALL 可对账

- **WHEN** 一次 E2E run 完成并登记台账
- **THEN** 台账条目 SHALL 含 **7 字段**(`run_id` · `config_sha256` · `metrics_sha256` · `metrics_13` · `verification_mode`+`verification_scope` · 三态 `ic3_verdict`+`hypothesis_id` · `topology_tag`)
- **AND** `metrics_13` 的每个 `not-collected` 键 SHALL **同时带 reason**
- **AND** 仓内每份已提交 `metrics.json` SHALL 对应**一个**台账条目(⛔ orphan),且 sha256 SHALL 相等
- **AND** ⛔ 台账条目 SHALL **NOT** 被表述为「实验可复算 / 可重放」

> **锚点**:`test/test_cep_stub_cache_e2e.cc` + 双向闸脚本的负例

#### Scenario: 计数纪律 SHALL 逐字生效

- **WHEN** 本 change 归档
- **THEN** `B3b-2` SHALL 维持 **0/4**
- **AND** `test/cep_b3b2_deferrals.json` SHALL **零 diff**
- **AND** `protocol_instances.json` 与 `protocol_instance.schema.json` SHALL **零 diff**
