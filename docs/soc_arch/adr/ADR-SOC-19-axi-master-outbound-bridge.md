# ADR-SOC-19: PcieEndpointIP AXI Master/Slave 角色边界 — 通用 SoC Master→PCIe Outbound 桥接缺口

> **状态**: 📋 Proposed
> **日期**: 2027-09-17
> **影响**: PcieEndpointIP 当前 AXI 接口语义明确化；暴露"通用 SoC AXI Master → PCIe Outbound TLP"**未提供**的设计缺口
> **类别**: SoC 架构 / PCIe EP 接口语义 / AXI-to-PCIe 桥接
> **关联 ADR**:
> - [`ADR-SOC-11-pcie-endpoint-ip.md`](./ADR-SOC-11-pcie-endpoint-ip.md) — PcieEndpointIP 整体架构
> - [`ADR-SOC-13-axi-stream-adapter-mapper.md`](./ADR-SOC-13-axi-stream-adapter-mapper.md) — AXI Stream Adapter / AXI4Mapper
> - [`ADR-SOC-17-pcie-mock-ip.md`](./ADR-SOC-17-pcie-mock-ip.md) — PcieMockIP 边界
> - [`ADR-SOC-18-cpptlm-abi-slimming.md`](./ADR-SOC-18-cpptlm-abi-slimming.md) — ABI 22→18 精简
> **关联 OpenSpec**:
> - [`openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/`](../../openspec/changes/archive/2026-09-16-2026-09-16-cpptlm-pcie-tlp-wire-datapath/) — Phase 9+ TLP 链路闭合
> - [`openspec/changes/2026-09-16-cpptlm-abi-slimming/`](../../openspec/changes/archive/2026-09-16-cpptlm-abi-slimming/) — ABI 精简 follow-up
> **关联 HSK**: 待创建 `HSK-12-cpptlm-axi-outbound-bridge.md`（跨仓契约镜像）
> **关联 Roadmap**: [`docs/soc_arch/roadmap/phase9-p4-axi-outbound-bridge.md`](../roadmap/phase9-p4-axi-outbound-bridge.md) — P4 战略评估阶段

---

## 1. 背景

### 1.1 dGPU 双路径需求

dGPU 作为 PCIe Endpoint，与 Host 通信有两条数据路径：

| 路径 | 触发方 | 数据方向 | PCIe 事务 |
|------|--------|----------|-----------|
| **dGPU receive** (Host → Device) | Host driver | 写 EP BAR / CFG | PCIe MWr (Posted) / MRd → CplD (Non-Posted) |
| **dGPU send** (Device → Host) | Device (SDMA / MSI-X / 中断) | 推 host memory | PCIe MWr (Posted) / MRd → CplD (Non-Posted) |

dGPU send 路径进一步分为两类需求：

| 子类 | 当前实现 | 示例组件 |
|------|---------|---------|
| **专用 SoC 组件** (有专属 PCIe master 协议) | C++ API 直调 `PcieRequesterEngine::mrd_read()` → `link_layer_->tx_tlp()` | SDMA H2D / MSI-X pending → MWr |
| **通用 SoC AXI Master** (标准 AXI 发起器) | **❌ 当前无任何桥接** | CUDA core / GPC core / Display controller / 未来扩展组件 |

### 1.2 当前 AXI 接口角色（per `include/framework/axi4_stream_adapter.hh:25-28`）

`PcieAxiAdapter` 持有 `Axi4StreamAdapter` 三端口：

```
EP AXI 三端口:
├── axi_master_out  ← EP 是 Master, 驱动数据进 SoC (PCIe Rx → SoC)
├── axi_slave_in    ← EP 是 Slave, 接收 SoC Master 写 (SoC → EP)
└── cfg_slave_in    ← EP 是 Slave, 接收 SoC Master 写 (SoC → EP cfg)
```

**真实消费位置**（per `src/tlm/pcie/pcie_endpoint_ip.cc:333-433`）：
- `axi_slave_in` 接收的 AW/W/AR 请求 **仅写入本地 `bar_store_` / config space**
- **不转发为 PCIe Outbound TLP**
- `axi_master_out` 仅在 Phase 8 M1 桥接修复（commit `429327d`）后,HostBypass 转发 host-side master_resp 回 EP 用

### 1.3 当前 SoC → Host memory 真实路径

**唯一路径**：`SoC 内部组件` → **`PcieRequesterEngine` (C++ API)** → **`PcieLinkLayer::tx_tlp()`** → **PCIe MRd TLP** → **Host memory**

证据：
- `src/tlm/pcie/pcie_requester_engine.cc:32-60` — `mrd_read()` 直接调 `link_layer_->tx_tlp(tlp, bdf)`
- `src/tlm/gpu/sdma_engine_tlm.cc:421` — SDMA H2D 描述符触发 RequesterEngine
- `include/tlm/gpu/sdma_engine_tlm.hh:316-326` — SDMA 通过 `set_request_engine()` 注入 C++ 指针

**`axi_slave_in` 当前完全未被 dGPU send 路径使用**。

### 1.4 缺口：通用 SoC AXI Master → PCIe Outbound 无桥接

如果将来 dGPU 需要支持：
- **CUDA kernel 直接访问 pinned host memory** (`cudaMemcpy` host↔device)
- **GPU 内核 zero-copy**（GPU 共享映射 host pinned page）
- **Display controller 拉 host framebuffer** (zero-copy 渲染)
- **任何标准 SoC AXI Master 想直接访问 host memory**

**当前架构无法支持**——因为：
1. `axi_slave_in` 只写 EP 本地 `bar_store_`，不转 PCIe TLP
2. `PcieRequesterEngine` 不暴露为标准 AXI Master 接口
3. 没有 `axi_to_pcie_bridge` 组件把 SoC AXI Master 事务 → PCIe TLP

### 1.5 与 Host 侧 RC 对称性缺失

| 维度 | Host 侧 | Device 侧 |
|------|---------|---------|
| 组件 | `PcieRootComplexTLM` (Phase 7) | `PcieEndpointIP` |
| AXI-to-PCIe 桥接 | ✅ RC 接收 CPU AXI/CHI 写 → PCIe TLP | ❌ EP 不接收 SoC AXI 写转 PCIe |
| CplD 处理 | ✅ RC 把 CplD 写回 CPU AXI memory | ⚠️ EP 仅写本地 `bar_store_`, SoC master 看不到 |

**严格来说**：EP 当前**不完整**——它实现了 Device-side Bus Master (DBM) 协议语义的一半（接收 host→device 用 `axi_master_out`），缺另一半（接收 device→host 用 AXI Master 接口）。

---

## 2. 决策

✅ **决策 A**: **明确当前边界** — `PcieEndpointIP` 的 `axi_slave_in` / `cfg_slave_in` **不承担 "AXI-to-PCIe bridge" 角色**，仅作 Host 侧向 EP 本地 BAR/Config Space 写入的转发点（当前由 HostBypass 桥接 host→EP）。

✅ **决策 B**: **明确 PcieRequesterEngine 不暴露为标准 AXI Master 接口** —— 保持当前 C++ API 形式，由 SDMA / MSI-X 投递链等**专用组件**调用，不面向通用 SoC AXI Master。

✅ **决策 C**: **不立即实现 "通用 SoC AXI Master → PCIe Outbound" 桥接** —— 当前需求范围（SDMA H2D + MSI-X pending→MWr）已闭环，通用桥接**未列入 v1.0 必需**。但**记录为架构缺口**，作为 Phase 10+ 评估项。

✅ **决策 D**: **若未来需求触发通用桥接**，按 **P4 阶段评估** —— 见 [`docs/soc_arch/roadmap/phase9-p4-axi-outbound-bridge.md`](../roadmap/phase9-p4-axi-outbound-bridge.md) 三方案对比 + 决策矩阵。

### 2.1 当前 PcieEndpointIP AXI 接口语义矩阵（明确化）

| AXI 端口 | 角色 | 接收方 | 数据落点 | 用途 |
|---------|------|--------|---------|------|
| `axi_master_out` | EP (Master) | SoC | SoC interconnect → SoC memory | 接收 PCIe Rx 数据推 SoC（如 host MWr → bar_store_ 落地后副作用） |
| `axi_slave_in` | EP (Slave) | SoC (Master) | EP 本地 `bar_store_` | Host 通过 HostBypass 写 EP BAR（Phase 8 M1 桥接） |
| `cfg_slave_in` | EP (Slave) | SoC (Master) | EP 本地 config space | Host 写 PMCSR / 其他 CFG 寄存器 |

### 2.2 SoC → Host memory 三种实现方式

| 方式 | 当前状态 | 适用场景 | 路径 |
|------|---------|---------|------|
| **C++ API 直调** | ✅ 已实现 | SDMA / MSI-X / 专用组件 | `SoC 组件` → `PcieRequesterEngine::mrd_read()` → `LL::tx_tlp()` |
| **标准 AXI 桥接** | ❌ **未提供** | 通用 SoC AXI Master | 待实现 — 详见 P4 阶段 |
| **PcieMockIP `device_axi_write()`** | ✅ 已实现 (Phase 9+) | Mock profile (`pcie_path="mock"`) | `PcieMockIP::device_axi_write()` → `Axi4StreamAdapter` |

---

## 3. 实施

### 3.1 当前范围（无新增代码）

- ✅ **无新增代码**：当前架构闭环，无需新组件
- ✅ **文档同步**：本 ADR + `dgpu-soc-pcie-slice.md` §"AXI 接口语义矩阵" 章节
- ✅ **跨仓通知**：HSK-12 创建（若需要 Hub 侧 ack 通用 AXI outbound 设计意图）

### 3.2 Phase 9+ 已实施内容（per 父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath`）

| Task | 内容 | 文件 | 状态 |
|------|------|------|:----:|
| T-P9-1 | `PcieTlpCodec` wire-format 编解码 (CRC-32/LCRC/DLLP-CRC16) | `include/tlm/pcie/pcie_tlp_codec.{hh,cc}` | ✅ |
| T-P9-2 | `PcieTlpWireBundle` payload 数组 (`std::array<uint32_t, 1024>`) | `include/bundles/pcie_bundles_tlm.hh` | ✅ |
| T-P9-3 | `PcieMockIP` (gem5 风格独立组件) | `include/tlm/pcie/pcie_mock_ip.{hh,cc}` | ✅ |
| T-P10-1 | `PcieCompleterEngine` 全分支 + CplD credit case 修复 | `include/tlm/pcie/pcie_completer_engine.{hh,cc}` | ✅ |
| T-P10-2 | `bar_store_` 三维 key `(bdf, bar, addr)` 修复 | `src/tlm/pcie/pcie_endpoint_ip.cc:395, 397, 419` | ✅ |
| T-P10-3 | `PcieEndpointIP::tick()` 三态分派 (`Full`/`Partial`/`Bypass`) | `src/tlm/pcie/pcie_endpoint_ip.cc:325-434` | ✅ |
| T-P11-1 | `PcieRequesterEngine` MRd 发起 + tag 关联 + 超时 | `include/tlm/pcie/pcie_requester_engine.{hh,cc}` | ✅ |
| T-P11-2 | SDMA H2D 经 RequesterEngine 出口 | `src/tlm/gpu/sdma_engine_tlm.cc:421, 579, 614` | ✅ |
| T-P11-3 | MSI-X pending → MWr 投递链 | (与 T-P11-1 同批次) | ✅ |
| T-P12-1 | profile JSON `"pcie_path"` 4 态选路 | `dgpu_board_shell.cc` + 待补 JSON | ⚠️ 部分 |
| **T-P9-0** | **ABI 精简 22→18** | **Hub ack 超时切出为 `cpptlm-abi-slimming`** | ✅ follow-up 已 archive |

### 3.3 Phase 9+ 残留债务（per 当前代码实测）

| 债务 | 优先级 | 影响 | 备注 |
|------|:------:|------|------|
| **profile `"pcie_path"` JSON 字段缺失** | P1 | 配置层不闭环 | `grep "pcie_path" configs/*.json` = 0 命中 |
| **`test_pcie_endpoint_ip_tlp_path_e2e.cc` 可能未创建** | P1 | E2E 验证缺失 | tasks.md T-P12-2 要求但 `ls test/` 未确认 |
| **HSK-10 / HSK-11 跨仓契约镜像** | P2 | 跨仓变更缺失 | 待确认 `docs/cross_repo/` 状态 |
| **`ch_uint<512>` 实为 64-bit 存储** | P3 | 架构根本性限制 | wire bundle 用 `std::array<uint32_t, N>` 绕开 |
| **通用 SoC AXI Master → PCIe Outbound 桥接** | P4 | 未来扩展 | **本 ADR 记录此缺口**，详见 P4 阶段 |

---

## 4. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| 真实驱动 (VFIO/IOMMUFD/amdgpu/nouveau) 依赖被删 backdoor 函数 | 🟢 低 | ABI 18 已实测够用 | grep 实测零调用 |
| Hub ack 仍无响应 (持续 10+ 工作日) | 🟡 中 | 跨仓治理风险 | HSK-11 §5.3 4 种回退策略 |
| 通用 SoC Master 需求出现，但未预留桥接 | 🟡 中 | 需临时返工 | P4 阶段提前评估 + ADR 锁定设计意图 |
| 当前 `axi_slave_in` 用途文档不清晰 → 新代码误用 | 🔴 高 | 接口语义歧义 | 本 ADR §2.1 明确化 + 模块文档同步 |

---

## 5. 备选方案（已拒绝）

### 备选 1: 立即实现 `axi_to_pcie_bridge` 通用桥接

**拒绝理由**:
- 当前需求范围（SDMA + MSI-X）已闭环，新增组件 ROI 低
- 增加 PcieEndpointIP 内部复杂度（与 Phase 9+ 简化 ABI 方向冲突）
- 引入新错误模式（AXI↔PCIe 转换的 outstanding 跟踪、FC 反压回退路径）
- 推迟 v1.0 整合节奏

### 备选 2: 把 PcieRequesterEngine 暴露为标准 AXI4 Master 接口

**拒绝理由**:
- 当前 SDMA 是唯一调用方，专用接口已够用
- AXI4 Master 化会引入额外 outstanding 跟踪（已有 AXI4Mapper 负责）
- 与 PcieMockIP `device_axi_write()` 单方向出口不一致（语义会变混乱）
- 真有需求时可在 Phase 10+ 重新评估

---

## 6. 参考文献

- [ADR-SOC-11-pcie-endpoint-ip.md](./ADR-SOC-11-pcie-endpoint-ip.md) — PcieEndpointIP 17 端口替代
- [ADR-SOC-13-axi-stream-adapter-mapper.md](./ADR-SOC-13-axi-stream-adapter-mapper.md) — AXI Stream Adapter / AXI4Mapper 集成
- [ADR-SOC-17-pcie-mock-ip.md](./ADR-SOC-17-pcie-mock-ip.md) — PcieMockIP gem5 风格独立组件
- [ADR-SOC-18-cpptlm-abi-slimming.md](./ADR-SOC-18-cpptlm-abi-slimming.md) — ABI 22→18 精简 + Hub 跨仓协调
- [openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/](../../openspec/changes/archive/2026-09-16-2026-09-16-cpptlm-pcie-tlp-wire-datapath/) — Phase 9+ 完整 TLP 链路
- [docs/soc_arch/roadmap/phase9-p4-axi-outbound-bridge.md](../roadmap/phase9-p4-axi-outbound-bridge.md) — P4 阶段评估文件
- [docs/soc_arch/modules/dgpu-soc-pcie-slice.md](../modules/dgpu-soc-pcie-slice.md) — dGPU SOC PCIe slice 微架构（待同步更新）
- `include/framework/axi4_stream_adapter.hh:25-28` — AXI Stream Adapter 三端口角色官方注释
- `src/tlm/pcie/pcie_endpoint_ip.cc:333-433` — `axi_slave_in` 实际消费逻辑（仅写 `bar_store_`）
- `src/tlm/pcie/pcie_requester_engine.cc:32-60` — `PcieRequesterEngine::mrd_read()` 实现

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Proposed — 等待 P4 阶段评估 + Phase 10+ 触发需求时再决策

## Status Update

No updates yet (initial version, 2027-09-17).