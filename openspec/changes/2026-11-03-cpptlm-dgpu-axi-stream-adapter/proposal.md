# cpptlm-dgpu-axi-stream-adapter: AXI Stream Adapter

> **状态**: Proposed — 2026-11-03
> **父 change**: `2026-09-01-cpptlm-dgpu-pcie-ip-microarch`
> **前置**: Phase 1-4(PCIe Link Layer / Encoding / PHY / SR-IOV)

## Why

PcieEndpointIP 需要向 SoC 暴露明确的 AXI 事务边界，使 BAR/MMIO、VRAM aperture、DMA 与配置访问可以连接到 MemoryCluster、Command Processor 和后续 AXI4Mapper，同时保持 CppTLM 的 ChStream 类型安全与多端口适配模式。

## What Changes

| 文件 | 用途 |
|---|---|
| `include/bundles/axi4_bundles_tlm.hh` | Axi4Bundle / Axi4LiteBundle，含请求与响应 ID |
| `include/framework/axi4_stream_adapter.hh` + `src/framework/axi4_stream_adapter.cc` | AXI4/AXI4-Lite 与 ChStream 的适配器 |
| `include/tlm/pcie/pcie_axi_adapter_tlm.hh` + `.cc` | PCIe TLP/事务层到 AXI 事务的边界转换 |
| `test/test_axi4_bundle.cc` | Bundle 字段、序列化与边界测试 |
| `test/test_pcie_axi_adapter_basic.cc` | 基础读写与响应测试 |
| `test/test_pcie_axi_adapter_64byte_burst.cc` | 512-bit/64-byte burst 测试 |
| `test/test_pcie_axi_adapter_backpressure.cc` | valid/ready 反压与不丢事务 |
| `test/test_pcie_axi_adapter_ids.cc` | outstanding 请求 ID 与响应关联 |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | 显式注册源文件与测试 |

## Scope

- `axi_master_out`: PcieEndpointIP 发起 SoC 访问
- `axi_slave_in`: SoC 发起进入 Endpoint 的事务
- `cfg_slave_in`: AXI4-Lite 配置访问
- 支持 64-bit 地址、512-bit 数据、burst length、WSTRB、读写响应、`bid`/`rid`
- 不在本 Phase 实现 AXI4Mapper 的 out-of-order 调度；Phase 6 负责独立 Mapper

## Acceptance Gate

- `openspec validate --strict` PASS
- 4 个 Phase 5 测试 PASS
- Phase 1-4 PCIe/SR-IOV 测试无回归
- `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改
