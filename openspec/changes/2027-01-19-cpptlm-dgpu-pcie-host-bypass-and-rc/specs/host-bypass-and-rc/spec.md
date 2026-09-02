# host-bypass-and-rc Spec — Phase 7 子集

> **配套**: [`proposal.md`](../proposal.md)
> **父 spec**: [`pcie-ip-microarch/spec.md`](../../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/specs/pcie-ip-microarch/spec.md) (umbrella)
> **本 spec 是父 spec 的 Phase 7 子集**

## Purpose

本 spec 定义 Phase 7 实施的子集—— `HOST_BYPASS_AND_RC` 一个 ADDED Requirement。完整 dGPU PCIe EP 仿真需要 Host 侧: `HostBypassTLM` 在软件 bring-up 阶段跳过 PCIe RC BFM 直接桥接 AXI 接口（依赖 Phase 5），`PcieRootComplexTLM`（可选）自研 RC 模型镜像 PcieEndpointIP（依赖 P3/P4）。

## ADDED Requirements

### Requirement: host-bypass-and-rc

完整 dGPU PCIe EP 仿真 **SHALL** 提供 Host 侧组件: `HostBypassTLM`（软件 bring-up 跳过 RC BFM，桥接 AXI 接口）与 `PcieRootComplexTLM`（可选，自研 RC 镜像）。

理由: 软件 bring-up 阶段无需完整 RC BFM 即可验证 EP 数据路径；自研 RC 提供不依赖外部 VIP/QEMU 的枚举与配置访问验证（Oracle C11 修订: P5 必须，HostBypass 桥接 AXI）。

范围:
- `HostBypassTLM`: 独立组件，经 Phase 5 AXI 接口桥接 PcieEndpointIP
- `PcieRootComplexTLM`: 可选 RC 模型（镜像 PcieEndpointIP，支持枚举/配置访问）
- E2E demo: PcieEndpointIP ↔ Host 全链路

实现位置:
- `include/tlm/pcie/host_bypass_tlm.hh` + `src/tlm/pcie/host_bypass_tlm.cc`
- `include/tlm/pcie/pcie_root_complex_tlm.hh` + `.cc`
- `examples/demo_pcie_full_e2e.py`

#### Scenario: Host Bypass 基础桥接

- **WHEN** 软件经 HostBypassTLM 发起对 EP 的访问（AXI Bridge 路径）
- **THEN** 事务经 AXI 接口正确送达 PcieEndpointIP
- **AND** 响应正确返回，无丢事务

#### Scenario: 软件 bring-up 场景

- **WHEN** 模拟软件 bring-up（配置空间写 → 读回 → BAR 访问）
- **THEN** 配置访问正确送达 EP 内部配置空间
- **AND** 读回值正确，BAR 访问经 AXI 路由

#### Scenario: PCIe 枚举（若 RC 实现）

- **WHEN** PcieRootComplexTLM 执行 PCIe 枚举
- **THEN** 设备/功能被发现，配置空间可读
- **AND** 分配 BAR，访问路由到 EP

#### Scenario: E2E demo

- **WHEN** 运行 `examples/demo_pcie_full_e2e.py`
- **THEN** PcieEndpointIP ↔ Host 全链路端到端成功
- **AND** 输出可验证结果（事务计数/状态）

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — Phase 7 实施中