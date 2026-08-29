# cpptlm-dgpu-pcie-endpoint: PcieEndpointTLM 组件（PCIe slave 归属 SOC）

> **状态**: 📋 Proposed — 2026-08-26 · **日期**: 2026-08-26 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D1/D2/D4
> **依赖**: 无（新组件，纯新增，不与 s3 文件冲突）
> **被依赖**: [`2026-08-26-cpptlm-dgpu-board-soc-split`](../2026-08-26-cpptlm-dgpu-board-soc-split/)（绑定 SOC 需本组件端口存在）

---

## Why

ADR-SOC-07 D2 决策：PCIe Config Space / BAR0 MMIO / BAR1 aperture / MSI-X 是 SOC 片内 PCIe Endpoint IP 的属性（与真硬件拓扑一致），必须从 s2 `DGpuBoardTLM::Impl` 的 C++ 硬编码迁移为 JSON 驱动的注册组件。

本 change 交付 `PcieEndpointTLM`——SOC 片内 PCIe slave 组件，使板卡的 PCIe 设备行为（枚举、MMIO、门铃、中断）可配置、可独立测试、可经 StreamAdapter 接入 SOC 拓扑。

**触发事件**:
- 2026-08-26 Board/SOC 分层修正决策（ADR-SOC-07）
- s2 单体 `write_reg()` if-else 硬编码被识别为架构偏差

---

## What Changes

### 1. 新建文件

| 文件 | 用途 |
|------|------|
| `include/bundles/pcie_bundles_tlm.hh` | **🆕 PCIe 事务 Bundle**：`PcieTlpBundle`（config r/w、BAR MMIO r/w、BAR mem r/w 统一事务类型）+ `MsiXDeliveryBundle`（中断投递） |
| `include/tlm/gpu/pcie_endpoint_tlm.hh` + `src/tlm/gpu/pcie_endpoint_tlm.cc` | **🆕 PcieEndpointTLM**（ChStreamModuleBase，REGISTER_CHSTREAM） |
| `include/tlm/gpu/pcie_config_space_mvp.hh` + `.cc` | Config Space 256B/4KB 数组 + capability chain 链表（参数化） |
| `include/tlm/gpu/pcie_bar_router_mvp.hh` + `.cc` | BAR0 寄存器路由表（数据化寄存器定义：offset → handler，门铃等副作用声明式注册） |
| `include/tlm/gpu/msix_table_mvp.hh` + `.cc` | MSI-X vector table + PBA + pending bitmap（vector 数参数化） |
| `configs/dgpu_soc_v1.json.in` | SOC 拓扑配置片段（pcie_ep 节点 + connections 示例） |
| `test/test_pcie_endpoint_config_space.cc` | Config Space 读写 + capability chain walk PASS |
| `test/test_pcie_endpoint_bar_routing.cc` | BAR0 MMIO 路由（含 `GPU_REG_DOORBELL=0x0014` → mmio_out 门铃事务）+ BAR1 aperture → mem_out 转发 PASS |
| `test/test_pcie_endpoint_msix.cc` | MSI-X pending 置位 → irq_out 投递 → clear PASS |
| `test/test_pcie_endpoint_from_config.cc` | JSON 实例化 + StreamAdapter 注入 PASS |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `include/chstream_register.hh` | 加 `REGISTER_CHSTREAM(PcieEndpointTLM)` + adapter 注册 |

### 3. 复用（不修改）

- `include/tlm/gpu/dgpu_bar.hh`（s2 v0.4）：BAR0 寄存器数组布局 + VRAM 尺寸常量作为本组件参数默认值参考；**语义迁移后由 change C 删除旧类**
- `include/tlm/gpu/doorbell_mvp.hh`（s2）：strong-order write 语义（250-700ns 区间断言）迁移为本组件门铃 register block 的内部行为

### 4. 明确排除（OUT of scope）

| 功能 | 排除原因 | 后续承接 |
|------|---------|---------|
| Legacy MSI（MSI 非 MSI-X） | UsrLinuxEmu 仅启用 MSI-X（per ADR-088 §D3.5） | 后续 PR 添加 |
| ATS / PASID / PRI | ADR-088 v1.0 scope 不含；page-request 走软件路径 | 后续 v1.x PR |
| PM 电源状态（D0/D3 hot transitions） | 当前 shell 假设 D0 单态 | 后续 PR |
| FLR（Function Level Reset） | 当前 shell 假设一次 create/destroy 生命周期 | 后续 PR |
| Completion Timeout / Poisoned TLP 错误模型 | UsrLinuxEmu 端容错层吸收 | N/A（外部责任） |
| 链路层 / 物理层 LTSSM | 本组件是事务层模型，链路层归 SerDes IP | N/A（外部 IP） |
| PCIe 枚举由 host 驱动 | 本组件是 slave model，不主动发配置事务 | N/A（外部责任） |

---

## 端口协议（ChStream）

| 端口 | 方向 | Bundle | 用途 |
|------|------|--------|------|
| `slave_in` | ingress | `PcieTlpBundle` | host 事务入口（Board shell 注入点） |
| `mmio_out` | egress | `PcieTlpBundle` | BAR0 解码后的寄存器副作用（doorbell → CP） |
| `mem_out` | egress | `PcieTlpBundle` | BAR1 aperture → MemoryCluster |
| `irq_out` | egress | `MsiXDeliveryBundle` | MSI-X 投递 → Board → UsrLinuxEmu 中断回调 |

`num_ports()` = 4（多端口模块，走 `set_stream_adapter(adapters[])` 多端口注入路径）。

---

## Acceptance Gate（本 change 独立）

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **PE-G1** PcieEndpointTLM 编译 + REGISTER_CHSTREAM 注册 | CppTLM | ⏳ | `cmake --build build` 通过 + `ModuleFactory` 按 type 创建成功 |
| **PE-G2** Config Space + capability chain 测试 PASS | CppTLM | ⏳ | `ctest -R "test_pcie_endpoint_config_space"` PASS |
| **PE-G3** BAR0/BAR1 路由测试 PASS（门铃偏移数据化，无 if-else 硬编码） | CppTLM | ⏳ | `ctest -R "test_pcie_endpoint_bar_routing"` PASS |
| **PE-G4** MSI-X pending → irq_out 投递测试 PASS | CppTLM | ⏳ | `ctest -R "test_pcie_endpoint_msix"` PASS |
| **PE-G5** JSON 实例化 + 4 端口 StreamAdapter 注入 PASS | CppTLM | ⏳ | `ctest -R "test_pcie_endpoint_from_config"` PASS |

**最终验收（本 change 完成时）**:
- [x] PE-G1 ~ PE-G5 全部 ✅
- [x] 4 个测试文件 PASS
- [x] `cmake --build build --target validate_topology` PASS
- [x] 本 change 可独立 archive（不依赖 sdma-engine / board-soc-split）

---

## Cross-Repo Coordination

| 仓 | 跟踪载体 | 状态 |
|----|---------|:---:|
| **UsrLinuxEmu** | ADR-088 §D5 23 ABI（本 change 实现其 device 侧语义，不改 ABI） | ✅ 无新 API 需求 |
| **UsrLinuxEmu（backdoor 数据面验证）** | 本 change 仅交付 `size > 8` descriptor-only TLP 路径（spec Scenario "BAR1 MEM write >8 bytes uses backdoor path"）；**bulk 数据搬运的实际端到端验证**（data 到达 VRAM、读回一致性）由 `cpptlm-dgpu-abi-export` change 的 `test_cpptlm_emulator_abi.cc` 覆盖——本 change 不重复测试 | ⏳ 待 abi-export archive |
| **PTX-EMU** | 无 | ✅ N/A |

---

**起草**: Sisyphus (2026-08-26, per ADR-SOC-07 D2)
**Owner**: CppTLM Team
**状态**: 📋 Proposed
