# ADR-SOC-14: v5.5+ 系统级硬件仿真集成修订

> **状态**: 📋 Proposed — 2027-02-09
> **日期**: 2027-02-09
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: 23 ABI 头冻结不变量 + Phase 8 后续状态 + UsrLinuxEmu v5.5+ 跨仓协调
> **类别**: SoC 架构 / 跨仓集成
> **取代/补充**: [`ADR-SOC-08-v55-system-hw-integration-preconditions.md`](./ADR-SOC-08-v55-system-hw-integration-preconditions.md)
> **关联文档**:
> - [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.0 PASS（§4-bis R26/R27 23 ABI 状态）
> - [`include/abi/cpptlm_emulator.h`](../../../include/abi/cpptlm_emulator.h)（19 声明）
> - [`src/abi/cpptlm_emulator.cc`](../../../src/abi/cpptlm_emulator.cc)（433 行，19/19 函数 + 4 回调 typedef）
> **关联 OpenSpec**: [`archive/2026-08-28-2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/`（已归档）](../../../openspec/changes/archive/2026-08-28-2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/)
> **关联 UsrLinuxEmu ADR**:
> - UsrLinuxEmu [`ADR-088`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) §C2/§D3.8 — 23 ABI + dma_translate_cb 外部契约
> - UsrLinuxEmu [`ADR-089 v0.5`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-089-v55-system-hw-simulation.md) — v5.5+ VFIO/IOMMUFD/vDPA/Migration

---

## 1. Context（背景）

### 1.1 ADR-SOC-08 历史决策（2026-08-28）

`ADR-SOC-08-v55-system-hw-integration-preconditions.md` 决策：

- **D1**：接受 UsrLinuxEmu ADR-089 v0.5 范围（VFIO 5.5.1 / IOMMUFD 5.5.2 / vDPA 5.5.3 / Live Migration 5.5.4）
- **D2**：本仓实施 5 项前置测试
  - P1 MSI-X 状态机细粒度（mask/unmask/PBA）
  - P2 wire-format 跨仓字节级一致性
  - P3 DMA translate callback 边界
  - P4 Doorbell 排队/并发
  - P5 backdoor ABI 隔离
- **D3**：向 UsrLinuxEmu owner 提议 4 项增量
- **D4**：OpenSpec change 命名约定
- **D5**：前置测试失败的 governance 处理

### 1.2 Phase 8 完成后状态（2027-02-09 HEAD `429327d`）

**23 ABI 当前状态**：
- **头冻结**：✅ 19 声明（per `include/abi/cpptlm_emulator.h`）
- **仓内实现**：✅ 19/19 函数 + 4 回调 typedef（per `src/abi/cpptlm_emulator.cc` 433 行）
- **UsrLinuxEmu 集成**：❌ 未闭环
- **完整 23 函数交付**：⚠️ 取决于 UsrLinuxEmu ADR-089 v0.5 节奏

### 1.3 Phase 8 完成情况

| ADR-SOC-08 前置测试 | 状态 |
|---------------------|------|
| P1 MSI-X mask/unmask/PBA | ✅ 已实施（per `test_pcie_endpoint_msix_state.cc`）|
| P2 wire-format 跨仓一致性 | ✅ 已实施（per `test_pcie_slice_wire_format_snapshot.cc`）|
| P3 DMA translate callback 边界 | ✅ 已实施（per `test_sdma_engine_iommu_fault.cc`）|
| P4 Doorbell 排队/并发 | ✅ 已实施（per `test_pcie_endpoint_doorbell_queue.cc`）|
| P5 backdoor ABI 隔离 | ✅ 已实施（per `test_sdma_engine_backdoor_isolation.cc`）|

---

## 2. Decision（决策）

### D1. 5 项前置测试当前实施状态评估

✅ **5 项前置测试全部已实施**（per 2026-08-28 prerequisites change 交付，Phase 8 后仍全绿）：

| 测试 | 状态 | 测试文件 |
|------|------|---------|
| P1 MSI-X | ✅ | `test/test_pcie_endpoint_msix_state.cc` |
| P2 wire-format | ✅ | `test/test_pcie_slice_wire_format_snapshot.cc` |
| P3 DMA translate | ✅ | `test/test_sdma_engine_iommu_fault.cc` |
| P4 Doorbell | ✅ | `test/test_pcie_endpoint_doorbell_queue.cc` |
| P5 backdoor | ✅ | `test/test_sdma_engine_backdoor_isolation.cc` |

### D2. 4 项 UsrLinuxEmu 跨仓提议反馈状态

✅ **4 项提议全部待 UsrLinuxEmu 回应**：

| 提议 | 状态 |
|------|------|
| ADR-089 §D7 IOMMUFD consumer 追加 CppTLM 跨仓契约验证段 | ⏳ 待 UsrLinuxEmu |
| ADR-089 §D7 VFIO consumer BAR0 寄存器行为契约来源 | ⏳ 待 UsrLinuxEmu |
| ADR-088 §C2 CppTLM C ABI 范围扩展 | ⏳ 待 UsrLinuxEmu |
| ADR-089 v0.5 更新到 v5.5+ 节奏 | ⏳ 待 UsrLinuxEmu |

### D3. 23 ABI 冻结不变量

✅ **23 ABI 冻结不变量**（per ADR-SOC-07 D5 + ADR-088 §D5）：

- 仓内实现：19/19 函数 + 4 回调 typedef 契约（per `src/abi/cpptlm_emulator.cc` 433 行）
- **不引入新 ABI 增量**：`axi_master_out / axi_slave_in / cfg_slave_in`（per `pcie_axi_adapter_tlm.hh` / `axi4_stream_adapter.hh`） 是 **TLM 端口名**（per `pcie_axi_adapter_tlm.hh`），**不是** ABI 函数名
- 若需增量 ABI，需发起 ADR-088 修订并获 UsrLinuxEmu owner 联签（跨仓治理边界）

### D4. live migration 集成点推迟时间表

✅ **live migration 推迟至 v5.5.4**：

- 取决于 UsrLinuxEmu ADR-089 更新
- 本仓侧接口预留（如 MSI-X mask/pending 状态可导出）
- 完整 live migration 推迟至 v1.1 完整版

---

## 3. Consequences（后果）

### 3.1 正面影响

- **23 ABI 头冻结 + 19/19 函数实现（+4 回调 typedef）**：UsrLinuxEmu 集成基础已就绪
- **5 项前置测试 PASS**：Phase 8 整合交付后无遗留
- **跨仓协调框架**：ADR-08 D5 governance + revision-plan 修订规划双层

### 3.2 负面影响

- **UsrLinuxEmu 集成未闭环**：4 项提议待回应
- **live migration 推迟**：v5.5.4 节奏决定
- **AMD KFD 跨仓依赖**（per ADR-09 D3）：需 UsrLinuxEmu owner 联签

### 3.3 跨仓治理边界

- **不修改 UsrLinuxEmu ADR**（per UsrLinuxEmu ADR-036 3 区分）
- **不在本仓实施 UsrLinuxEmu 端测试**（超出本仓范围）
- **本仓侧仅记录状态**：等待 UsrLinuxEmu 反馈

---

## 4. Implementation（实施）

### 4.1 Phase 8 已完成

| 任务 | 状态 |
|------|------|
| 5 项前置测试 | ✅ 全部 PASS |
| 23 ABI 头冻结 | ✅ |
| 23 ABI 仓内实现 19/19 + 4 回调 typedef | ✅ |
| UsrLinuxEmu 集成 | ❌ 待闭环 |
| Live migration | ❌ 推迟 v5.5.4 |

### 4.2 23 ABI 实现清单（per `src/abi/cpptlm_emulator.cc`）

```cpp
// 仓内已实现 19 函数 + 4 回调 typedef：
int cpptlm_emulator_get_version(...);
int cpptlm_emulator_get_device_count(...);
int cpptlm_emulator_get_device_info(...);
int cpptlm_emulator_create(...);
int cpptlm_emulator_create_by_id(...);
void cpptlm_emulator_destroy(...);
int cpptlm_emulator_mmio_write(...);
int cpptlm_emulator_mmio_read(...);
int cpptlm_emulator_pcie_config_write(...);
int cpptlm_emulator_pcie_config_read(...);
int cpptlm_emulator_backdoor_read(...);
int cpptlm_emulator_backdoor_write(...);
int cpptlm_emulator_msix_init(...);
int cpptlm_emulator_msix_update_pending(...);
int cpptlm_emulator_msix_clear_pending(...);
int cpptlm_emulator_lookup_register(...);
int cpptlm_emulator_register_callbacks(...);
int cpptlm_emulator_register_backdoor_cb(...);
int cpptlm_emulator_register_dma_translate_cb(...);
// ... 共 19 函数 + 4 回调 typedef
```

---

## 5. Risks（风险）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 23 ABI 仓内 19/19 已实现（+4 回调 typedef 契约）与 UsrLinuxEmu 端注册衔接待联签 | 🟡 中 | D3 标注"完整 23 函数闭环需 UsrLinuxEmu 联签" |
| **R2** | UsrLinuxEmu KFD 路径(per ADR-09 D3)未确认 | 🟡 中 | ADR-09 不单方宣布 KFD 实施时间 |
| **R3** | live migration 中断状态 | 🟢 低 | v5.5.4 推迟，本仓侧接口预留 |
| **R4** | 23 ABI 头冻结 vs 增量 ABI 冲突 | 🟡 中 | 若需增量,发起 ADR-088 修订 + UsrLinuxEmu 联签 |

---

## 6. 参考文献

### 6.1 关联 ADR

| ADR | 关联 |
|-----|------|
| ADR-SOC-07 D5 | 23 ABI 契约不变 |
| ADR-SOC-08 | v5.5+ 系统级硬件仿真集成（前置测试契约，本 ADR 补充） |
| ADR-SOC-09 D3 | UsrLinuxEmu 跨仓承诺标注（AMD KFD 路径） |

### 6.2 关联 OpenSpec

| Change | 关联 |
|--------|------|
| `archive/2026-08-28-2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/`（已归档） | ADR-SOC-08 前置测试实施 |

### 6.3 关联真实代码

| 文件 | 角色 |
|------|------|
| `include/abi/cpptlm_emulator.h` | 23 ABI 头（19 声明）|
| `src/abi/cpptlm_emulator.cc` | 23 ABI 实现（433 行，19/19 函数 + 4 回调 typedef）|

### 6.4 关联 UsrLinuxEmu ADR

| ADR | 关联 |
|-----|------|
| UsrLinuxEmu ADR-088 | 23 ABI + dma_translate_cb 外部契约源 |
| UsrLinuxEmu ADR-089 v0.5 | v5.5+ 系统级硬件仿真（VFIO/IOMMUFD/vDPA/Migration）|

---

## Status Update

- **2027-02-09**: 📋 Proposed。Phase 8 完成后 5 项前置测试全部 PASS；23 ABI 头冻结 + 仓内 19/19 函数实现（+4 回调 typedef）；UsrLinuxEmu 集成未闭环（4 项提议待回应）；live migration 推迟 v5.5.4。本 ADR 补充 ADR-SOC-08，记录 Phase 8 后状态变化。