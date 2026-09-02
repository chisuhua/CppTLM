# cpptlm-dgpu-pcie-host-bypass-and-rc: Host Bypass + Root Complex

> **状态**: Proposed — 2027-01-19
> **父 change**: `2026-09-01-cpptlm-dgpu-pcie-ip-microarch`
> **前置**: Phase 5 (AXI Stream Adapter, 必须) + Phase 3/4 (可选, PcieRootComplexTLM 镜像需要 PcieEndpointIP)

## Why

完整 dGPU PCIe EP 仿真需要 Host 侧:
- **HostBypassTLM**: 软件 bring-up 阶段跳过 PCIe RC BFM，直接桥接 AXI 接口（依赖 P5）
- **PcieRootComplexTLM** (可选): 自研 RC 模型（若不用外部 VIP/QEMU），镜像 PcieEndpointIP
- 可选 wrap `alexforencich/verilog-pcie` 提供真实 PHY 验证

## What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/host_bypass_tlm.hh` + `src/tlm/pcie/host_bypass_tlm.cc` | **新**: HostBypassTLM（独立组件，桥接 AXI ↔ PcieEndpointIP） |
| `include/tlm/pcie/pcie_root_complex_tlm.hh` + `.cc` | **新**: PcieRootComplexTLM（可选镜像 RC） |
| `test/test_host_bypass_basic.cc` | **新**: Host Bypass 基础测试 |
| `test/test_host_bypass_software_bringup.cc` | **新**: 软件 bring-up 场景测试 |
| `test/test_pcie_root_complex_enumeration.cc` | **新**: PCIe 枚举测试（若 RC 实现） |
| `examples/demo_pcie_full_e2e.py` | **新**: E2E demo（PcieEndpointIP ↔ Host） |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 显式注册源与测试 |

## Scope

- HostBypassTLM: 独立组件，软件 bring-up 跳过 RC BFM，桥接 AXI 接口（依赖 Phase 5）
- PcieRootComplexTLM: 可选自研 RC 模型（镜像 PcieEndpointIP，依赖 P3/P4）
- E2E demo: PcieEndpointIP ↔ Host 全链路
- 不在本 Phase 实现真实 PHY wrap（`alexforencich/verilog-pcie` 可选 submodule）

## Acceptance Gate

- `openspec validate --strict` PASS
- 3 个 Phase 7 测试 PASS（basic / software_bringup / root_complex_enumeration）
- Phase 1-6 PCIe/AXI 测试无回归
- `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改
- `examples/demo_pcie_full_e2e.py` 运行成功
