# 130b-encoding Spec — Phase 2 子集

> **配套**: [`proposal.md`](../proposal.md)
> **父 spec**: [`pcie-ip-microarch/spec.md`](../../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/specs/pcie-ip-microarch/spec.md) (umbrella)
> **本 spec 是父 spec 的 Phase 2 子集**

## Purpose

本 spec 定义 Phase 2 实施的子集—— `ENCODING_LATENCY_MODEL` 一个 ADDED Requirement。Phase 1/Phase 3+ 的 requirement 不在本 spec 范围。

## ADDED Requirements

### Requirement: encoding-latency-model

PcieEndpointIP **SHALL** 建模 128b/130b 编码延迟(Gen1-5 各 Gen 速率对应 128B per-lane 块延迟),**NOT** 实现 bit-level 编码/解码。

理由:SoC 仿真不需要 bit-level 130b 编码;Gen5 ≠ FLIT(FLIT 是 Gen6 特性,per Librarian 修正)。

范围(Oracle 二次评审修订):
- ⚠️ 单位修正: **GT/s per-lane per-direction**(不是 MB/s,不是 MT/s)
- `enum class Rate { GEN1=2, GEN2=5, GEN3=8, GEN4=16, GEN5=32 }` GT/s
- 块延迟(128B / 1024-bit per-lane): Gen5 ≈ 32ns, Gen4 ≈ 64ns, Gen3 ≈ 128ns
- 速率切换延迟: ~µs 级(包含 Gen3+ 均衡协商,**非 Gen5 独有**)
- 多 lane 并行加速:`total_latency = block_latency / active_lanes`

实现位置:
- `include/tlm/pcie/pcie_encoding_latency_model.hh`
- `src/tlm/pcie/pcie_encoding_latency_model.cc`

#### Scenario: Gen5 链路延迟(x1 lane)

- **WHEN** rate=GEN5 且 active_lanes=1,block=128B
- **THEN** 延迟 = 1024 bits / 32 GT/s = **32 ns**
- **AND** 与 PCIe 5.0 spec §4.2.2 一致

#### Scenario: Gen5 链路延迟(x16 lane)

- **WHEN** rate=GEN5 且 active_lanes=16,block=128B
- **THEN** 延迟 = 32 ns / 16 = **2 ns**(并行)
- **AND** 仿真反映真实多 lane 并行加速

#### Scenario: 速率切换延迟

- **WHEN** rate 从 GEN3 切换到 GEN5
- **THEN** 仿真触发 ~µs 级延迟(包含均衡协商)
- **AND** 期间不能传输(链路不可用)

#### Scenario: Gen5 吞吐回归

- **WHEN** 测试条件:credit 充足 + 无拥塞 + 纯 memcpy 流
- **THEN** 吞吐 ≥ 95% 理论带宽(Gen5 x16 ≈ 63 GB/s)
- **AND** ctest `test_pcie_endpoint_throughput` PASS

#### Scenario: Phase 1 LinkLayer 集成

- **WHEN** Phase 1 `PcieLinkLayer` Tx/Rx Path 调用延迟模型
- **THEN** 延迟注入不破坏 Phase 1 7 个测试
- **AND** ctest `-R pcie` 全量 PASS

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — Phase 2 实施中(待 Phase 1 Oracle 评审通过)