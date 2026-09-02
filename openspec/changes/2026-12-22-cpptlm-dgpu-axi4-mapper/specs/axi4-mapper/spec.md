# axi4-mapper Spec — Phase 6 子集

> **配套**: [`proposal.md`](../proposal.md)
> **父 spec**: [`pcie-ip-microarch/spec.md`](../../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/specs/pcie-ip-microarch/spec.md) (umbrella)
> **本 spec 是父 spec 的 Phase 6 子集**

## Purpose

本 spec 定义 Phase 6 实施的子集—— `AXI4_MAPPER` 一个 ADDED Requirement。AXI4Mapper 作为独立模块，在 Phase 5 的 `Axi4Bundle`/`Axi4LiteBundle` 之上提供 outstanding 事务跟踪与 out-of-order completion 调度（通过 `rid` 将乱序 `rdata` 关联回原事务，per design.md §6.4 Oracle 修订），并可被 CrossbarTLM / CacheTLM 复用。

## ADDED Requirements

### Requirement: axi4-mapper

AXI4Mapper **SHALL** 作为独立模块（`include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc`）消费 Phase 5 的 `Axi4Bundle`，支持 outstanding 事务跟踪与 out-of-order completion，且**不内嵌于 PcieEndpointIP**。

理由: AXI4 的 Outstanding / Out-of-order / W-channel 复杂特性应独立建模，避免与 SoC 其他 AXI 用户耦合；独立模块可被 CrossbarTLM / CacheTLM 复用并按 JSON 可选注入。

范围（Oracle Top-4 修订）:
- 独立模块，不内嵌 PcieEndpointIP
- Outstanding 跟踪: 读写独立 ID 空间（awid/arid），容量上限 N，N+1 拒绝新发出
- OOO completion: `rid` 关联乱序 `rdata` 回原事务（核心机制）
- 双向转换: AXI 信号 ↔ `Axi4Bundle`（`axi4_signal_to_bundle` / `axi4_bundle_to_signal`）
- 按 JSON 可选注入（`axi4_mapper_inject: true`，design.md §8）

实现位置:
- `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc`
- `include/framework/axi4_signal_to_bundle.hh` + `.cc`
- `include/framework/axi4_bundle_to_signal.hh` + `.cc`

#### Scenario: AXI4 ↔ Bundle 基础映射

- **WHEN** 构造 `Axi4Mapper` 并将 `Axi4Bundle` 经 bundle_to_signal 转换为 AXI 信号
- **THEN** 所有字段无损往返（awaddr/awlen/awsize/awburst/awid/wdata/wstrb/wlast/bid/bresp/araddr/arlen/arsize/arburst/arid/rid/rdata/rresp/rlast）
- **AND** 反向 signal_to_bundle 还原一致

#### Scenario: Outstanding transaction 跟踪

- **WHEN** 发起 N 个读写请求（独立 ID 空间，容量上限 N）
- **THEN** outstanding 计数正确跟踪，N+1 时拒绝新发出（与 Q12 CompletionTracker 语义一致）
- **AND** 完成一个后释放槽位，可再注册

#### Scenario: Out-of-order completion（rid 关联）

- **WHEN** 多 outstanding 读请求（arid=A/B/C），下游乱序返回 `rdata`（rid 乱序）
- **THEN** 每个 `rdata` 经 `rid` 关联回原事务，正确完成对应请求
- **AND** 乱序完成不影响其他 outstanding（不匹配即不消耗）

#### Scenario: 与 PcieEndpointIP 集成

- **WHEN** JSON 配置含 `axi4_mapper_inject: true`
- **THEN** AXI4Mapper 可选注入 PcieEndpointIP 的 AXI 数据路径（经 Phase 5 Axi4StreamAdapter）
- **AND** 端到端读写事务经 mapper 正确路由与完成

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — Phase 6 实施中（待 Phase 5 Oracle 评审放行）
