# pcie-ip-integration Specification

## Purpose
TBD - created by archiving change 2027-02-09-cpptlm-dgpu-pcie-ip-integration. Update Purpose after archive.
## Requirements
### Requirement: integration-delivery

Phase 8 **SHALL** 整合 Phase 1-7 全部产物，交付完整 dGPU SOC + PCIe EP 仿真。

理由: 7 阶段实施后需要顶层交付物（架构文档、示例配置、E2E 测试）+ 旧代码迁移，才能闭环为生产可用。

范围:
- 架构文档迁移：`umbrella/design.md` → `docs/architecture/14-pcie-ip-microarchitecture.md`
- 完整 dGPU SOC + PCIe EP 示例配置：`examples/dgpu_soc_with_pcie_ip.json`
- 全链路 E2E 测试：`test/test_pcie_endpoint_ip_full_e2e.cc`（解决 Phase 7 Oracle M1：EP 真实消费 AXI 请求并返回真实响应）
- 旧代码迁移：`PcieEndpointTLM` 加 `[[deprecated("use PcieEndpointIP")]]` 标注（**保留** `chstream_register.hh` 注册以维持 Phase 4 既有测试 + dgpu_board_shell.cc 等依赖；新增代码统一使用 `PcieEndpointIP`）

实现位置:
- `docs/architecture/14-pcie-ip-microarchitecture.md`
- `examples/dgpu_soc_with_pcie_ip.json`
- `test/test_pcie_endpoint_ip_full_e2e.cc`
- `include/chstream_register.hh`（保留 `PcieEndpointTLM` 注册以维持 Phase 4 测试 + dgpu_board_shell.cc 等既有依赖；并行注册 `PcieEndpointIP`，新增代码统一使用后者）
- `include/tlm/gpu/pcie_endpoint_tlm.h`（加 [[deprecated]]）

#### Scenario: 架构文档完整迁移

- **WHEN** 读取 `docs/architecture/14-pcie-ip-microarchitecture.md`
- **THEN** 涵盖 Phase 1-7 全部内容（含 Phase 7 Oracle M2 标注：RC 枚举 PF0-only 简化模型）
- **AND** 链接到 umbrella design.md 的历史位置

#### Scenario: 完整示例配置可用

- **WHEN** 使用 `examples/dgpu_soc_with_pcie_ip.json` 启动仿真
- **THEN** `cmake --build build --target validate_topology` 通过
- **AND** PcieEndpointIP + HostBypassTLM + PcieRootComplexTLM 全部实例化

#### Scenario: 全链路 E2E 测试真实闭环

- **WHEN** 软件经 HostBypassTLM 发起 BAR 写 → PcieEndpointIP
- **THEN** EP 真实消费 AXI 请求（不是测试手写注入）
- **AND** EP 内部处理（TLP→MMIO→配置空间写入）后返回真实响应
- **AND** HostBypassTLM 接收真实响应（不是手动注入）

#### Scenario: 旧代码迁移标注

- **WHEN** 用户仍使用 `PcieEndpointTLM` 类
- **THEN** 编译期收到 `[[deprecated("use PcieEndpointIP")]]` 警告
- **AND** `chstream_register.hh` 保留 `PcieEndpointTLM` 注册（Phase 4 既有测试 + dgpu_board_shell.cc 依赖），并行注册 `PcieEndpointIP`

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — Phase 8 最终整合交付中

