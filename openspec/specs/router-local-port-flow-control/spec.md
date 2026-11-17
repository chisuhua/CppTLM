# router-local-port-flow-control Specification

## Purpose
把 Router 的 LOCAL 端口从「链路端点」重新定义为「终端接入点」：credit 是 router↔router 链路上的 VC buffer 反压机制，而 LOCAL 的下游是终端消费者、没有归还方，把链路级流控套到终端端口既无归还方也无语义依据。本 capability 终止 LOCAL 的 downstream credit 记账（信用预算无穷、反压交给 ChStream 的 valid/ready），并要求 Router 的 mesh 侧 credit 停滞**可观测**——以免「router↔router 有反压、router→终端无反压」这一不对称变成看不见的静默点。

## Requirements

### Requirement: router-local-port-terminal-semantics

`RouterTLM` 的 `LOCAL` 端口(索引 4)SHALL 被定义为**终端接入点**,其 downstream credit 记账 SHALL **终止** —— 即 `LOCAL` 端口 SHALL **NOT** 参与 router↔router 链路的 VC buffer 反压机制。

#### Scenario: LOCAL 端口 SHALL 不消耗 downstream credit

- **WHEN** `RouterTLM` 判定某 `(out_port, out_vc)` 是否有 downstream credit 可用
- **THEN** `out_port == LOCAL` 时 SHALL 恒判定为「有 credit」
- **AND** 对 `LOCAL` 端口 SHALL **NOT** 扣减 `downstream_credits_[LOCAL][*]`
- **AND** 该判定 SHALL **NOT** 绕过 `out_port` / `vc` 的**边界检查**(⛔ 非法 `vc` 不得因短路而被静默放行)

> **锚点**:`src/tlm/router_tlm.cc` 的 `has_credit` / `consume_credit` · `test/test_router_tlm.cc`(AC-01/AC-02)
> **决策源**:`docs/adr/ADR-X.16-router-local-port-terminal-credit.md` D1/D2

#### Scenario: LOCAL 端口语义 SHALL 被显式声明

- **WHEN** 检视 `RouterPort::LOCAL` 的声明与注释
- **THEN** 其 SHALL 显式声明该端口为**终端端口**且**不参与 credit 流控**
- **AND** `openspec/specs/port-type-system/spec.md` 中 `LOCAL` 的 `role = BI_DIRECTIONAL` SHALL **保持不变**(本决策只终止 credit 记账语义,不改端口角色)

> **锚点**:`include/tlm/router_tlm.hh` 的 `RouterPort::LOCAL` 注释

#### Scenario: LOCAL 端口的死状态 SHALL 被显式处置

- **WHEN** `downstream_credits_[LOCAL][*]` 不再被维护
- **THEN** 其处置 SHALL 为以下之一且**显式落地**:(a) 注释说明其不再被维护,或 (b) 从记账循环中跳过
- **AND** 该处置 SHALL **NOT** 留下无说明的死状态

> **锚点**:`src/tlm/router_tlm.cc` 的 credit 记账循环

### Requirement: router-mesh-credit-stall-observability

`RouterTLM` SHALL 提供 mesh 侧 credit 死锁的**可观测面**,使「下游不归还 credit」不再静默。

#### Scenario: credit stall 周期数 SHALL 被计数并可观测

- **WHEN** 某 `(out_port, out_vc)` **存在待发 flit** 且 `has_credit(out_port, out_vc) == false`(即 VA / SA 阶段因缺 credit 而拒绝)
- **THEN** 该 `(port, vc)` 的 stall **周期累计数** SHALL 递增
- **AND** 该计数 SHALL 可经 `RouterTLM` 的既有观测面读出(与 `flits_forwarded` 同面)
- **AND** stall 持续达阈值(16 cycles)时 SHALL 打印一次诊断日志

> **锚点**:`src/tlm/router_tlm.cc` 的 VA / SA 拒绝分支 · `test/test_router_tlm.cc`(AC-03)
> ⚠️ **锚点口径**：计数锚点是「VA / SA 的**拒绝条件**」,**不是** `waiting_for_credit` 状态位 —— 后者在 credit 归零后**保持 false**(flit 停在 `input_buffer_`),按它计数将恒为 0。

#### Scenario: credit stall 计数 SHALL NOT 误报 LOCAL 端口

- **WHEN** `LOCAL` 端口已按 `router-local-port-terminal-semantics` 终止 credit 记账
- **THEN** `LOCAL` 端口 SHALL **NOT** 产生 stall 计数
- **AND** 该交叉性质 SHALL 有判据覆盖(本地投递用例中计数保持 0)

> **锚点**:`test/test_router_tlm.cc`(AC-07)

#### Scenario: credit 停滞 SHALL 不得靠重置掩盖

- **WHEN** 检视 `credit_timeout_` 的默认值与其复位行为
- **THEN** 其默认值 SHALL **保持为 0**(关闭)(⛔ 周期性重置 credit 会虚增下游可用额度 ⇒ mesh 链路超发,且掩盖真实死锁)
- **AND** credit 归零导致的停滞 SHALL **只**通过 stall 计数与诊断日志暴露

> **锚点**:`include/tlm/router_tlm.hh` 的 `credit_timeout_` · `src/tlm/router_tlm.cc` 的 `credit_safety_reset`
