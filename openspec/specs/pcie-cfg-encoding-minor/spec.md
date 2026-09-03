# pcie-cfg-encoding-minor Specification

## Purpose
TBD - created by archiving change 2026-09-03-cpptlm-dgpu-pcie-cfg-encoding-minor. Update Purpose after archive.
## Requirements
### Requirement: pci-config-address-decoding

PCIe AXI 配置请求路径 SHALL 按 PCIe 配置空间地址编码解码 AXI 地址：低 2 bit 为对齐保留位，配置空间 byte offset 使用 `((awaddr >> 2) & 0x3f)`；BAR/MMIO 请求 SHALL 保持既有地址语义。

#### Scenario: Aligned configuration address decodes correctly

- **WHEN** 配置请求携带 AXI 地址 `awaddr`
- **THEN** 配置空间 offset 等于 `((awaddr >> 2) & 0x3f)`
- **AND** `awaddr` 的低 2 bit 不改变解码后的 offset

#### Scenario: Configuration boundary accesses are safe

- **WHEN** 请求 offset 0、4 或配置空间末端合法 offset
- **THEN** 请求访问对应配置寄存器
- **AND** 越界请求按照既有错误/拒绝语义处理

#### Scenario: BAR requests retain their address semantics

- **WHEN** 请求被识别为 BAR/MMIO 请求而非 Config Space 请求
- **THEN** 请求不经过 Config Space offset 解码
- **AND** BAR backing store 与既有 E2E 行为保持一致

### Requirement: config-e2e-regression

修复 SHALL 由单元测试和 Phase 8 全链路 E2E 测试覆盖。

#### Scenario: Phase 8 configuration E2E passes

- **WHEN** 执行 `test_pcie_endpoint_ip_full_e2e_config`
- **THEN** 配置空间写入和读取均通过真实 AXI→EP 数据路径完成

#### Scenario: Phase 8 BAR E2E passes

- **WHEN** 执行 `test_pcie_endpoint_ip_full_e2e_bar`
- **THEN** BAR 请求仍按 BAR 地址语义处理并返回真实响应
- **AND** 不引入既有 PCIe/AXI 回归失败

