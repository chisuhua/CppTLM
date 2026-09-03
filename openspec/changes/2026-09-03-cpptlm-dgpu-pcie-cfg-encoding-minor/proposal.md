# Proposal: cpptlm-dgpu-pcie-cfg-encoding-minor — PCIe Config Space 地址编码修复

> **状态**: Proposed — 2026-09-03
> **父 change**: `2027-02-09-cpptlm-dgpu-pcie-ip-integration`（已归档）
> **范围**: Phase 8 已知 Minor，不修改 PCIe EndpointTLM ABI 布局

## Why

Phase 8 M1 的 AXI↔PCIe Config Space 接线当前直接将 AXI `awaddr` 作为配置空间 offset。该简化导致 `test_pcie_endpoint_ip_full_e2e_config` 与 `_bar` 仍有已知失败，并未遵循 PCIe 配置请求的地址编码约定：低 2 bit 为对齐保留位，配置空间 byte offset 位于 `bits[7:2]`。

本 change 仅修复地址解码边界，保持现有 BAR、MMIO、17 端口 EP 和 23 ABI 冻结不变。

## What Changes

- 在 AXI→PCIe 配置请求路径统一实现 `cfg_offset = (awaddr >> 2) & 0x3f` 的 byte offset 解码语义。
- 明确非配置空间/BAR 请求不走 Config Space 解码路径。
- 增加对齐、边界、读写和 E2E 回归测试。
- 同步 Phase 8 已知问题文档，移除已修复的 config/bar known-fail 表述。

## Scope

**In Scope**:
- `include/tlm/pcie/pcie_axi_adapter_tlm.hh`、`src/tlm/pcie/pcie_axi_adapter_tlm.cc`（或实际承载地址解码的等价文件）
- `test/test_pcie_endpoint_ip_full_e2e.cc` 及相关 PCIe/AXI 单测
- `AGENTS.md` 与架构/验证文档中的已知问题状态

**Out of Scope**:
- `PcieEndpointTLM` 成员、虚函数、端口布局和 ABI 头布局
- `ch_uint<512>` 底层存储宽度限制
- BAR 映射策略、SR-IOV 路由和 RC PF0-only 枚举策略

## Acceptance Gate

- 配置地址 `awaddr` 按 PCIe 约定解码为 `((awaddr >> 2) & 0x3f)`，低 2 bit 不影响配置 offset。
- 配置空间读写、边界和只读保护测试通过。
- `test_pcie_endpoint_ip_full_e2e_config` 与 `_bar` 通过；既有 `[pcie]`/`[axi]` 回归无新增失败。
- 23 ABI 冻结文件的布局不变，仅允许必要的实现/测试/文档修改。
- `openspec validate 2026-09-03-cpptlm-dgpu-pcie-cfg-encoding-minor --strict` PASS。

## Risks

| 风险 | 缓解 |
|---|---|
| 现有测试使用旧的直接 offset 语义 | 先增加编码边界测试，统一更新 E2E 请求构造方式 |
| 配置请求与 BAR 请求共用 AXI 通道 | 仅在明确的配置请求类型/路径执行 cfg 解码，保持 BAR 地址原语义 |
| 地址宽度或截断差异 | 使用明确的无符号定宽转换，并覆盖 offset 0、4、252 等边界 |
