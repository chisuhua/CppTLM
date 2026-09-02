# ADR-SOC-11: PcieEndpointIP 替代 PcieEndpointTLM — 17 ports 整合模块决策

> **状态**: 📋 Proposed — 2027-02-09
> **日期**: 2027-02-09
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: PcieEndpointTLM → PcieEndpointIP 迁移;SR-IOV 支持;23 ABI 兼容性边界
> **类别**: SoC 架构 / PCIe Endpoint IP
> **取代**: 补充（非取代）[`ADR-SOC-07-dgpu-board-soc-layering.md`](./ADR-SOC-07-dgpu-board-soc-layering.md) D2（D2 原锁定 PcieEndpointTLM 4 端口冻结,本 ADR 在保留 23 ABI 头冻结前提下用 17 ports PcieEndpointIP 替代）
> **关联文档**:
> - [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md)（950 行,Phase 1-7 PCIe IP 微架构整合文档）
> - [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md)（L1 Host Interface 子系统架构,本 ADR 落地）
> - [`docs/soc_arch/modules/dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md)（旧 PcieEndpointTLM 4 端口模块微架构,本 ADR 修订）
> **关联 OpenSpec**: [`2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/`](../../../openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/)、[`2026-09-01-cpptlm-dgpu-pcie-ip-microarch/`](../../../openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/)、[`2027-02-09-cpptlm-dgpu-pcie-ip-integration/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-pcie-ip-integration/)
> **关联 Phase 8 整合交付证据**: git commit `429327d`

---

## 1. Context（背景）

### 1.1 ADR-SOC-07 D2 的历史限制

`ADR-SOC-07-dgpu-board-soc-layering.md` D2 决策（2026-08-26）锁定：

> `PcieEndpointTLM`（4 端口冻结）归属 SOC 片内，**保持 4 端口 layout 不变**

这一决策在 v0.5 MVP 阶段（2026-08-26）正确，但 **Phase 4 起的 PCIe IP 子链路演进**带来了新需求：

- **SR-IOV 支持**：MI300X 8 XCD chiplets × 38 CU = 304 CU（per AMD CDNA 3）+ **per-VF 独立 Config Space / MSI-X / FC / seq#**
- **17 端口需求**：1 PF + 16 VF = 17 端口，需要 per-VF 独立 StreamAdapter 路由
- **stream_id 内部路由**：避免 N×16 端口爆炸（per `architecture/14` §3 端口图）

### 1.2 Phase 1-7 PCIe IP 子链路演进（2026-09..2027-01）

7 阶段 OpenSpec change 已完整实施：

| 阶段 | OpenSpec change | 关键交付 |
|------|-----------------|---------|
| Phase 1 | `2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc` | 链路层 + DLLP + FC token bucket |
| Phase 2 | `2026-09-29-cpptlm-dgpu-pcie-130b-encoding` | 128b/130b 编码延迟模型 |
| Phase 3 | `2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl` | PHY 数字控制 + LTSSM 11 态 + Bypass Mux |
| Phase 4 | `2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool` | **SR-IOV VF Pool:1 PF + 16 VF** |
| Phase 5 | `2026-11-03-cpptlm-dgpu-axi-stream-adapter` | AXI↔TLP 边界 |
| Phase 6 | `2026-12-22-cpptlm-dgpu-axi4-mapper` | AXI4 OOO + rid 关联 |
| Phase 7 | `2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc` | Host Bypass 软件 bring-up + 自研 RC PF0-only |

### 1.3 Phase 8 整合交付（HEAD `429327d`）

Phase 8 整合完成以下关键动作：

- **架构文档迁移**：`openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md`（931 行）→ `docs/architecture/14-pcie-ip-microarchitecture.md`（950 行）
- **完整 dGPU SoC + PCIe EP 示例配置**：`examples/dgpu_soc_with_pcie_ip.json`（per Phase 8 交付）
- **全链路 E2E 测试**：`test/test_pcie_endpoint_ip_full_e2e.cc`（3 TEST_CASE：config/bar/rc）
- **Phase 8 M1 修复**：HostBypassTLM/RC::tick() 自动转发 4 方向 AXI 通道（master_out↔slave_in + slave_resp↔master_resp），让 EP 真实消费请求
- **PcieEndpointTLM 标记 deprecated**：commit `429327d` 在 `include/tlm/gpu/pcie_endpoint_tlm.h` 添加 `[[deprecated("use PcieEndpointIP...")]]`

### 1.4 PcieEndpointIP 真实端口定义（per `include/tlm/pcie/pcie_endpoint_ip.hh:50-53`）

```cpp
class PcieEndpointIP : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_PORTS = 17;  // 0=PF, 1..16=VF0..VF15

    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle> req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> resp_out[NUM_PORTS];
    // ...
};
```

**关键设计**：
- `NUM_PORTS = 17`：1 PF + 16 VF，避免 N×16 端口爆炸
- 内部 `stream_id` 路由：用 `stream_id`（PCIe Requester ID 的 function 部分）区分 VF，避免端口按 VF 数量级展开（per `docs/architecture/14` §3 端口图）

---

## 2. Decision（决策）

### D1. PcieEndpointTLM 加 `[[deprecated("use PcieEndpointIP...")]]` 属性（已实施）

✅ **已实施**（per commit `429327d`）：

- 文件：`include/tlm/gpu/pcie_endpoint_tlm.h`
- 添加 `[[deprecated("use PcieEndpointIP...")]]` 属性
- **layout 完全不变**（仅加属性，无成员改动）
- 23 ABI 冻结原则（per ADR-SOC-07 D5）保持

### D2. `chstream_register.hh` 保留 PcieEndpointTLM 注册

✅ **保留注册**（既有依赖）：

- `chstream_register.hh` 中 `PcieEndpointTLM` 注册**保留**
- **原因**：既有 Phase 4 测试 + `dgpu_board_shell.cc` 依赖
- 测试通过：删除注册会破坏既有测试，破坏零回归原则

### D3. 新代码统一使用 PcieEndpointIP

✅ **新代码统一**：

- `include/tlm/pcie/pcie_endpoint_ip.hh`（per 17 ports 真实定义）
- `include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh`（per-VF Config Space / MSI-X / FC）
- `examples/dgpu_soc_with_pcie_ip.json`（完整 dGPU SoC + PCIe EP 配置）

### D4. 23 ABI 兼容性边界

✅ **23 ABI 头冻结不变量**（per ADR-SOC-07 D5 + ADR-088 §D5）：

- `include/tlm/gpu/pcie_endpoint_tlm.h` **layout 完全不变**（仅加 `[[deprecated]]` 属性）
- `include/abi/cpptlm_emulator.h` **零修改**
- 仓内实现：`src/abi/cpptlm_emulator.cc`（433 行，19/19 函数 + 4 回调 typedef 契约）
- UsrLinuxEmu 集成：未闭环（待 ADR-089 v0.5 节奏）

---

## 3. Consequences（后果）

### 3.1 正面影响

- **SR-IOV 支持**：17 端口设计原生支持 per-VF 独立 Config Space / MSI-X / FC / seq#
- **StreamAdapter 路由**：内部 `stream_id` 路由避免 N×16 端口爆炸
- **Q12 Completion 单一真源**：`PcieEndpointIP.completions()` 委托 `PcieSriovVfPool.completions()`，避免双份 outstanding 失配
- **23 ABI 头冻结**：通过 `[[deprecated]]` 注解实现迁移，不改 layout

### 3.2 负面影响

- **既有代码迁移成本**：依赖 `PcieEndpointTLM` 的模块需要逐步迁移到 `PcieEndpointIP`
- **测试需要重命名**：既有 Phase 4 测试已使用 `PcieEndpointTLM`，需保留但建议迁移
- **文档同步**：`dgpu-soc-pcie-slice.md` 模块微架构需要更新（标注 deprecated + 指向 PcieEndpointIP）

### 3.3 兼容性保证

- **零回归**：`[[deprecated]]` 仅编译器警告，运行行为不变
- **PcieEndpointTLM 4 端口代码保留**：`include/tlm/gpu/pcie_endpoint_tlm.h` 与 `src/tlm/gpu/` 中的实现零修改
- **23 ABI 外部契约不变**：UsrLinuxEmu 集成代码零修改

---

## 4. Implementation（实施）

### 4.1 Phase 8 已完成（HEAD `429327d`）

| 任务 | 状态 | commit / 文件 |
|------|------|--------------|
| 架构文档迁移 | ✅ | `docs/architecture/14-pcie-ip-microarchitecture.md` |
| 完整 dGPU SoC + PCIe EP 配置 | ✅ | `examples/dgpu_soc_with_pcie_ip.json` |
| 全链路 E2E 测试 | ✅ | `test/test_pcie_endpoint_ip_full_e2e.cc`（3 TEST_CASE） |
| M1 修复（4 方向 AXI 桥接） | ✅ | commit `429327d`（HostBypassTLM/RC::tick()） |
| `[[deprecated]]` 标注 | ✅ | `include/tlm/gpu/pcie_endpoint_tlm.h` |

### 4.2 后续迁移任务（OpenSpec change `2027-02-09-cpptlm-dgpu-soc-v1-architecture`）

- **迁移 dgpu-soc-pcie-slice.md**：更新模块微架构标注 deprecated + 指向 PcieEndpointIP
- **新增 PcieEndpointIP 模块微架构文档**：`docs/soc_arch/modules/pcie-endpoint-ip.md`（17 ports + 内部组件）
- **更新 00-overview.md §7.2 引用矩阵**：从 `dgpu-soc-pcie-slice.md` 拆分为 `dgpu-soc-pcie-slice.md`（旧）+ `pcie-endpoint-ip.md`（新）

---

## 5. Risks（风险）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 既有依赖 `PcieEndpointTLM` 的代码未迁移 | 🟡 中 | `[[deprecated]]` 警告 + 保持注册 + 测试零回归 |
| **R2** | 文档未同步导致开发者误解 | 🟡 中 | `dgpu-soc-pcie-slice.md` 标注 deprecated + 指向 PcieEndpointIP |
| **R3** | 23 ABI 边界意外破坏 | 🟢 低 | layout 完全不变（仅加属性），UsrLinuxEmu 集成代码零修改 |

---

## 6. 参考文献

### 6.1 关联 ADR

| ADR | 关联 |
|-----|------|
| ADR-SOC-07 D2 | PcieEndpointTLM 4 端口冻结（原决策，本 ADR 补充） |
| ADR-SOC-07 D5 | 23 ABI 契约不变 |
| ADR-SOC-08 | v5.5+ 系统级硬件仿真集成的前置测试契约（23 ABI 仓内 19/19 函数 + 4 回调 typedef 契约） |

### 6.2 关联 OpenSpec changes

| Change | 关联内容 |
|--------|---------|
| `2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool` | Phase 4 SR-IOV VF Pool 引入 PcieEndpointIP 17 ports |
| `2026-09-01-cpptlm-dgpu-pcie-ip-microarch` | umbrella（Phase 1-7 整合） |
| `2027-02-09-cpptlm-dgpu-pcie-ip-integration` | Phase 8 整合交付 + deprecated 标注 |

### 6.3 关联模块微架构文档

| 文档 | 关联 |
|------|------|
| [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md) | Phase 1-7 PCIe EP 完整微架构（950 行） |
| [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md) | L1 Host Interface 子系统架构（本 ADR 落地） |
| [`docs/soc_arch/modules/dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md) | 旧 PcieEndpointTLM 4 端口（待修订标注 deprecated） |

### 6.4 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/SM/overview.md`](../../research/SM/overview.md) | NVIDIA Hopper/Blackwell SM 内部（含 PCIe/SR-IOV 视角） |

---

## Status Update

- **2027-02-09**: 📋 Proposed。本 ADR 在 ADR-SOC-07 D2 基础上补充 17 ports PcieEndpointIP 替代决策，与 Phase 8 整合交付对齐（HEAD `429327d`）。