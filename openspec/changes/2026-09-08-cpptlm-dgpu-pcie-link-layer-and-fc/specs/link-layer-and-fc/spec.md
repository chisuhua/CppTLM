# link-layer-and-fc Spec — Phase 1 子集

> **配套**: [`proposal.md`](../proposal.md) · [`tasks.md`](../tasks.md)
> **父 spec**: [`pcie-ip-microarch/spec.md`](../../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/specs/pcie-ip-microarch/spec.md) (umbrella)
> **本 spec 是父 spec 的 Phase 1 子集**

## Purpose

本 spec 定义 Phase 1 实施的子集—— `LINK_LAYER` 与 `FLOW_CONTROL_TOKEN_BUCKET` 两个 ADDED Requirement。后续 phase 子 change 不在本 spec 范围。

## ADDED Requirements

### Requirement: link-layer

PcieEndpointIP **SHALL** 实现 PCIe 链路层(DLLP gen/parse / ACK-NAK / Retry buffer / 12-bit Sequence Number / 双向 Rx + 下行 ACK 生成),通过 `PcieLinkLayer` 组件暴露 `link_error_injector_t` API 支持 ACK/NAK retry TDD。

理由:当前 `PcieEndpointTLM` 链路层完全缺失(Board shell 注入 TLP 直接绕过)。

范围: PCIe 5.0 Base Specification §3.4(FC)/§3.5(DLLP)/§3.6(重传)。

实现位置:
- `include/tlm/pcie/pcie_link_layer_tlm.hh`
- `src/tlm/pcie/pcie_link_layer_tlm.cc`

#### Scenario: DLLP gen/parse

- **WHEN** 链路层收到任意 kind ∈ {ACK, NAK, InitFC1, InitFC2, UpdateFC, NOP, Vendor} 的 DLLP
- **THEN** 必须正确 parse 字段(vc_id, credit_P/NP/Cpl × 3, seq_num 12-bit)
- **AND** 必须按 kind 分发到对应处理路径
- **AND** 必须生成正确的对端响应 DLLP

#### Scenario: ACK/NAK retry(累积确认)

- **WHEN** 收到 ACK DLLP(seq=AckSeq)
- **THEN** Retry Buffer 中所有 seq ≤ AckSeq 的条目**累积清空**(非"全清")
- **WHEN** 收到 NAK DLLP(seq=NakSeq)
- **THEN** Retry Buffer 重发所有 seq ≥ NakSeq 的 TLP
- **AND** 12-bit seq wrap(4095 → 0)正确处理

#### Scenario: 下行(host→EP)Rx 路径(双向)

- **WHEN** host→EP 方向 TLP/DLLP 到达
- **THEN** 128b/130b 解码后 DLLP/TLP 分流
- **AND** TLP 解析后送 §3 事务层
- **AND** EP 收 TLP 后生成 ACK DLLP 发回 host(累积确认)
- **AND** EP 收方向 retry buffer 与上行独立

#### Scenario: 错误注入接口(Q15)

- **WHEN** 调用 `link_error_injector_t::inject_nak(seq=N)` / `inject_dllp_loss()` / `inject_tlp_loss(seq=N)`
- **THEN** Link Layer 在收到对应 seq 时模拟 NAK / DLLP 丢包 / TLP 丢包
- **AND** 默认 disable(无副作用),JSON `params.link_error_injection.enabled=true` 启用

---

### Requirement: flow-control-token-bucket

PcieEndpointIP **SHALL** 实现 Flow Control Token Bucket 引擎(P/NP/Cpl 各一个桶,weight + capacity;**仅 UpdateFC DLLP 补充,无自动 refill**)。

理由:当前 `PcieEndpointTLM` FC 完全缺失,无法建模 backpressure → SoC 反压失真。

范围:
- 替代 6-header credit 追踪(per [umbrella decisions.md §Q2](../../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/decisions.md))
- 每 VF 一个桶(SR-IOV 时 per-VF bucket pool,per Q11 单 VC0)
- ⚠️ **无 `refill_rate`**:credit 单调非减,仅由 UpdateFC DLLP 增加(per PCIe spec §3.4);自动 refill 会让反压永不触发

实现位置:
- `include/tlm/pcie/pcie_flow_control_token_bucket.hh`
- `src/tlm/pcie/pcie_flow_control_token_bucket.cc`

#### Scenario: FC Token Bucket 基础行为

- **WHEN** 调用 `consume(P)` 且 token 充足(≥ weight_P)
- **THEN** 返回 true 并扣减 token
- **WHEN** 调用 `consume(P)` 且 token 不足
- **THEN** 返回 false,**不扣减** token

#### Scenario: UpdateFC 补充(唯一补充路径)

- **WHEN** 收到 UpdateFC DLLP(credit = X)
- **THEN** 调用 `update(P, X)` 增加 P 类型 token(不超过 capacity)
- **AND** token 单调非减(无自动 refill)

#### Scenario: 过载与恢复

- **WHEN** 长期 credit 不足(发送阻塞)
- **THEN** UpdateFC 到达后 token 恢复,can_send 重新返回 true
- **AND** 不会死锁(无自动 refill 路径,严格等 UpdateFC)

#### Scenario: Per-VF 桶隔离

- **WHEN** VF0 桶耗尽
- **THEN** VF1 桶不受影响
- **AND** per-VF token 独立计数

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — Phase 1 实施中
