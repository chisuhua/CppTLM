# axi-stream-adapter Spec — Phase 5 子集

> **配套**: [`proposal.md`](../proposal.md)
> **父 spec**: [`pcie-ip-microarch/spec.md`](../../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/specs/pcie-ip-microarch/spec.md) (umbrella)
> **本 spec 是父 spec 的 Phase 5 子集**

## Purpose

本 spec 定义 Phase 5 实施的子集—— `AXI_STREAM_ADAPTER` 一个 ADDED Requirement。PcieEndpointIP 需要向 SoC 暴露明确 AXI 事务边界（master/slave/cfg_slave 三端口），使 BAR/MMIO、VRAM aperture、DMA 与配置访问可连接到 MemoryCluster / Command Processor，并为 Phase 6 AXI4Mapper 预留接口。

## ADDED Requirements

### Requirement: axi-stream-adapter

PcieEndpointIP **SHALL** 暴露多端口 AXI Stream Adapter（master / slave / cfg_slave），与 PcieTlpBundle 解耦。Bundle 定义必须包含请求与响应 ID（`awid`/`arid`/`bid`/`rid`）以支持 Phase 6 out-of-order 匹配。

理由: PcieEndpointIP 需要 AXI 接口与 SoC 交互（VRAM DMA / BAR MMIO / SR-IOV admin / 内部配置）。

范围（Oracle Top-4 修订）:
- 3 端口: `axi_master_out`（EP 发起 SoC 访问）、`axi_slave_in`（SoC 发起进入 Endpoint 的事务）、`cfg_slave_in`（AXI4-Lite 配置访问）
- Bundle 类型（新建 `include/bundles/axi4_bundles_tlm.hh`）:
  - `Axi4Bundle`: `awaddr`(64b)/`awlen`/`awsize`/`awburst`/`awid`(16b)/`wdata`(512b)/`wstrb`/`wlast`/`bid`(16b)/`bresp`/`araddr`/`arlen`/`arsize`/`arburst`/`arid`(16b)/`rid`(16b)/`rdata`/`rresp`/`rlast` — per 设计 §6.2/6.3
  - `Axi4LiteBundle`: `awaddr`/`awid`/`wdata`/`wstrb`/`bresp`/`araddr`/`arid`/`rdata`/`rresp`
- 支持 64-bit 地址、512-bit 数据、burst length、写/读响应
- 支持 valid/ready 反压（backpressure），不允许丢事务
- 支持 outstanding 请求 ID 关联（awid→bid / arid→rid，Phase 6 Mapper 消费）

实现位置:
- `include/bundles/axi4_bundles_tlm.hh`
- `include/framework/axi4_stream_adapter.hh`
- `src/framework/axi4_stream_adapter.cc`
- `include/tlm/pcie/pcie_axi_adapter_tlm.hh` + `src/tlm/pcie/pcie_axi_adapter_tlm.cc`

#### Scenario: Axi4Bundle 字段完整性与 ID 关联

- **WHEN** 构造 `Axi4Bundle`（写入读请求 + 写通道）
- **THEN** 所有字段可读回（awaddr/awlen/awsize/awburst/awid/wdata/wstrb/wlast/bid/bresp/araddr/arlen/arsize/arburst/arid/rid/rdata/rresp/rlast）
- **AND** AWID/ARID 为请求 ID，BID/RID 为响应 ID（Phase 6 OOO 匹配需要）
- **AND** `awid != arid`（读写独立 ID 空间）

#### Scenario: 基础读写与响应

- **WHEN** 经 `axi_master_out` 发起读请求（ARID=N, AADDR=A）
- **THEN** 下游返回 RLAST/RID=N/RRESP=OKAY
- **AND** 数据按 `rdata[511:0]` 返回完整
- **WHEN** 经 `axi_master_out` 发起写请求（AWID=M, WLAST）
- **THEN** 下游返回 BRESP/BID=M
- **AND** 无丢事务（valid/ready 握手中 backpressure 不丢数据）

#### Scenario: 64-byte burst（Gen5 AXI 512-bit）

- **WHEN** 发起 `len=4`（4 拍 × 128-bit 或按实现）burst 写
- **THEN** `wlast` 在最后一拍置位
- **AND** 总传输字节 = `(len+1) × 2^awsize`
- **AND** 事务完整到达下游

#### Scenario: backpressure 不丢事务

- **WHEN** 下游 `ready=0`（backpressure）
- **THEN** 当前拍数据不丢失，`valid` 保持，直到 `ready=1` 才推进
- **AND** 全事务在 backpressure 周期内最终完成，无数据丢/重复

#### Scenario: cfg_slave_in AXI4-Lite 配置访问

- **WHEN** SoC 经 `cfg_slave_in` 写配置寄存器
- **THEN** 写入送达 PcieEndpointIP 内部配置空间
- **AND** 读访问返回正确值

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — Phase 5 实施中(待 Phase 4 Oracle 评审放行)