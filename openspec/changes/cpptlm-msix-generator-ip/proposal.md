# cpptlm-msix-generator-ip: MSI-X Generator IP + trigger_msix API (修复 Gap L)

> **状态**: 🟡 Proposed Draft (Phase 2, Change E, 2026-10-12 lock)
> **作者**: CppTLM Team
> **版本**: v0.5.0 (初始草稿, Phase 2 Change E 启动)
> **关联 OpenSpec**:
>   - ✅ `cpptlm-lwc-bus-hub-mvp` archived (Change B, Phase 0, LWC Hub + MsixBundle 已落地 T8)
>   - ✅ `cpptlm-vram-controller-functional-integration` archived (Change A, Phase 0, VramController functional-mode + handle_functional_request API)
>   - ✅ `cpptlm-gmmu-pte-bus-correction` archived (Change C, Phase 1, GMMU dedicated client + NOC 装配)
>   - **业务定位锁定**: `docs/designs/dgpu-soc/architecture.md` §0.1 Gap 矩阵 + §0.2 总体实施计划 (Phase 2, Change E)
>
> **🔗 Cross-Reference (per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3, 2026-10-10)**:
> Gap L 真实病因 = Tranche 1 CRITICAL C3: `trigger_msix` API 全仓零匹配 (per `cpptlm-pcie-host-test-flr-rebar` v1.1 §3.3 C3 修复: 删除 X1 v1.0 错误提议的 `cpptlm_emulator_msix.h`; **正确路径** = MSI-X Generator IP + C++ 入口在 DGpuBoard/PcieEndpointIP, **不暴露为 ABI 函数**). Change E MSI-X Generator IP 与 `cpptlm-pcie-tlp-entry-layer` (Change D, v0.5.2) 共享 `PcieTlpSerializer` (TLP 序列化层); Change E **复用** Change D `submit_tlp()` 入口 (per Change D v0.5.2 spec §2.2 `pe-pcie-endpoint-submit-tlp-additive`). 详见 [cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md §L](../../archive/cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md)。
> **修复 Gap**: **L** (HIGH) — §0.1 Gap 矩阵 16 行中 1 行
> **优先级**: **P2** (Phase 2, Gap L HIGH 修复)
> **估时**: **5-7d** (含 TDD 5-step + LWC 集成 + TLP 链路)
> **依赖**: Change A ✅ archived + Change B ✅ archived + Change C ✅ archived (Phase 1 完成)
> **目标 tag**: `cpptlm-msix-generator-ip-completion`

---

## Why

§0.1 Gap 矩阵锁定的 16 行中, 1 行由本 change 修复:

### Gap L (HIGH): MSI-X 直调 + CompletionRing dormant

- **现状 (`dgpu_board_shell.cc:1122-1127`)**: `trigger_irq_async()` 直调, **0 个真实 MSI-X TLP 报文**:
  ```cpp
  // 当前 (Gap L violation)
  void DGpuBoard::trigger_irq_async(uint16_t vector) {
      // 直调 PCIe EP 中断寄存器, 无 MSI-X TLP 生成
      ep_->acknowledge_irq(vector);  // 直调 ack, 无 TLP
  }
  ```
- **真实物理 IP 边界** (per Oracle RTL 调研, `cpptlm-dgpu-soc-internal-noc-e1` v1.1 §2.4):
  - **PCIe EP 硬件 MSI-X Generator**: 物理 IP 有 dedicated MSI-X Generator 模块, 接收 software/firmware 配置的 MSI-X Table (BAR 指向的 memory) + PBA (Pending Bit Array), 生成 MSI-X TLP 报文 (MWr 64-bit with addr + data) 发往 host
  - **MSI-X Table**: 每个 vector 含 addr (64-bit) + data (32-bit) + control (masked bit)
  - **PBA**: 每个 vector 1 bit, 标记 pending interrupt
  - **MSI-X 语义**: 64-bit MWr (Memory Write), TLP Data Payload = data[31:0], TLP Address = addr[63:2] (aligned to 4-byte boundary per PCIe spec §6.8.2)
- **根因 (per arch-truth-cleanup §L)**: 当前实现缺少 MSI-X Generator IP 模块 + `trigger_msix` API (CRITICAL C3: 全仓零匹配); CompletionRing dormant (未连接)
- **本 change 修复路径**: 新建 `MsixGeneratorIP` 模块 + `DGpuBoard::trigger_msix` C++ API + per-VF MSI-X PBA/Vector table + LWC hub 路由 + `PcieEndpointIP::submit_tlp()` 链路

### 真实物理 IP 对应 (per Oracle RTL 调研)

| 真实 IP | MSI-X Generator 行为 | 本 change 建模 |
|---------|---------------------|---------------|
| PCIe EP MSI-X Generator (ARM DW PCIe/NI) | 硬件 MSI-X Generator, 接收 MSI-X table 配置, 生成 MWr TLP | `MsixGeneratorIP` 模块 |
| NVIDIA H100 | MSI-X Generator (per PCIe Base Spec §6.8) | 同上 |
| ARM GICv3 Distributor | MSI-X 报文格式 vs SPI/PPI | 同上 |

### 业务定位同步 (per §0.1 §X + Phase 2 路线图)

Gap L 是 Phase 2 唯一的 HIGH 优先级 Gap. 修复后 Phase 2 (Gap L + F) 完成, §0.1 Gap 矩阵 Phase 1-2 范围全部关闭.

---

## What Changes

### 新增文件 (3 个, ~500 行)

| 文件 | 行数 | 关键内容 |
|------|:----:|----------|
| `include/tlm/pcie/msix_generator_ip.hh` | **~200** | `MsixGeneratorIP` 类: PBA + Vector table per-VF/per-PF, `trigger_msix(vector, addr, data, masked)` API, LWC SlavePort (MsixBundle in), PcieTlpBundle out → `PcieEndpointIP::submit_tlp()` |
| `src/tlm/pcie/msix_generator_ip.cc` | **~200** | MSI-X table lookup + PBA check + TLP 生成逻辑 + LWC 路由 |
| `include/tlm/pcie/msix_table.hh` | **~100** | `MsixTable` / `MsixPba` per-VF/per-PF table 管理 (64 entries default, per PCIe spec §6.8) |

### 修改文件 (4 个, ~150 行)

| 文件 | 改动 | 备注 |
|------|------|------|
| `src/tlm/gpu/dgpu_board_shell.cc` | `trigger_irq_async()` → `msix_generator_->trigger_msix(vector, addr, data, masked)`; 删 `ep_->acknowledge_irq` 直调 | 关键 invasive change (~30 行) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | **0 改动** (per ADR-DGPU-08 §2.3 约束, 不加新 ABI); C++ `trigger_msix` 在 DGpuBoard 内部, 不进 EP | 复用 Change D `submit_tlp()` 准 frozen API |
| `include/tlm/lwc/lwc_bus_hub.hh` | 0 改动 (MsixBundle 已定义 per Change B T8) | 无需改 LWC hub |
| `configs/dgpu_soc_minimal_v1.json` | 加 `msix_generator` 节点 + connections (LWC hub → msix_generator → pcie_ep) | 新模块注册 |

### 实施铁律 (per §0.1 + CRITICAL C3 修复)

1. **0 新增 23 ABI 函数 (Hard Invariant)**: `trigger_msix` C++ 入口在 `DGpuBoard` 内部, **不** 暴露为 ABI 函数 (per ADR-DGPU-08 §2.3 + arch-truth-cleanup CRITICAL C3 修复)
2. **MSI-X Generator 真实语义**: MWr 64-bit with addr + data (per PCIe spec §6.8.2), 不走 legacy INTx
3. **LWC 装配复用 Change B 设施**: `MsixGeneratorIP` 经 LWC hub 接收 interrupt 请求 (MsixBundle in), 经 `submit_tlp()` 链路上报
4. **TLP 序列化复用 Change D v0.5.2**: `MsixGeneratorIP` 内部调用 `PcieTlpSerializer` 生成 MSI-X TLP (复用 `include/utils/pcie_tlp_serializer.hh`, host_pcie_test_stub 已在用)
5. **per-VF MSI-X 表**: 每个 PF + 16 VF 有独立 MSI-X Table + PBA (per PCIe spec §6.8); 17 × 64 entries
6. **Frozen surface 守恒**: 23 ABI + 4 frozen headers 零 diff

### Frozen Surface (0 修改)

- `include/abi/cpptlm_emulator.h` (per ADR-088 §D5, 零新增 ABI)
- `include/tlm/gpu/pcie_endpoint_tlm.h`
- `include/tlm/gpu/pcie_display_device.hh`
- `include/tlm/pcie/pcie_bundles_tlm.hh`
- `include/tlm/pcie/pcie_endpoint_ip.hh` (准 frozen, 复用 `submit_tlp()` API)

### 推迟到后续 Change (out of Change E scope)

| 项目 | 推迟到 | 原因 |
|------|--------|------|
| MSI-X PBA 中断 coalescing | Future | 多 vector pending 时延后处理 |
| Multi-Vector MSI-X (不止 1 vector) | Future | 当前 single-vector trigger_msix |
| CompletionRing → MSI-X 联动 | Future | CompletionRing dormant 待后续连接 |

---

## Acceptance

### 功能验收

- [ ] `MsixGeneratorIP` 模块存在, 含 PBA + Vector table per-VF/per-PF
- [ ] `DGpuBoard::trigger_msix(vector, addr, data, masked)` C++ API 存在 (0 ABI)
- [ ] MSI-X Generator 经 LWC hub 路由 (MsixBundle → submit_tlp)
- [ ] MSI-X TLP (MWr 64-bit) 经 `PcieEndpointIP::submit_tlp()` 发往 host
- [ ] `[msix_generator]` Catch2 测试套 ≥ 5 cases

### 非功能性验收

- [ ] 23 ABI 签名 0 diff (per ADR-088 §D5)
- [ ] 4 frozen headers 0 diff
- [ ] 既有 7 套件测试 0 回归

---

## Capabilities

### 新增能力

1. **MSI-X Generator IP**: 仿真 PCIe EP MSI-X Generator 硬件行为 (Table + PBA + TLP 生成)
2. **`trigger_msix` C++ API**: DGpuBoard 内部 API (不暴露 ABI), 用于 device IP (SDMA/DMA/等) 触发 MSI-X 中断
3. **真实 MSI-X TLP 链路**: device IP → trigger_msix → LWC hub → MsixGeneratorIP → PcieTlpSerializer → submit_tlp → PCIe link → host
4. **per-VF MSI-X 表**: 17 × 64 entries (1 PF + 16 VF)

### 复用既有设施

| 既有设施 | 复用方式 |
|---------|---------|
| Change B LWC Hub + MsixBundle | MsixGeneratorIP 经 LWC hub 接收 interrupt 请求 |
| Change D `PcieTlpSerializer` | MSI-X TLP 序列化 (MWr 64-bit, per PCIe spec §6.8.2) |
| Change D `PcieEndpointIP::submit_tlp()` | TLP 入口 (准 frozen additive API) |
| Change A `handle_functional_request` | 0 关联 (独立路径) |
| Change C GMMU dedicated client | 0 关联 (独立路径) |

---

## Impact

### 影响范围

| 受影响模块/文件 | 影响 | 兼容性 |
|---------------|------|---------|
| `DGpuBoard::trigger_irq_async()` | 改为调用 `msix_generator_->trigger_msix()` | invasive change, 需 E2E 验证 |
| `DGpuBoard` + LWC hub | 新增 `msix_generator_` 成员 + LWC connections | JSON topology 变更 |
| `configs/dgpu_soc_minimal_v1.json` | 加 msix_generator 节点 | 新模块注册 |
| CompletionRing | 当前 dormant, 不在本 change 范围 | Future |

### 测试覆盖

| 测试 | 当前 | 实施后 |
|------|------|--------|
| `[msix_generator]` | 0 cases | **新增 ≥ 5 cases** (vector table lookup + boundary + error injection) |
| `[dgpu_soc_minimal]` | 21/145 | 保持 (加 msix_generator 节点) |
| `[pcie]` | 430/36672 | 保持 (MSI-X 路径补充) |
| 既有 7 套件 | 全绿 | 0 regression |
