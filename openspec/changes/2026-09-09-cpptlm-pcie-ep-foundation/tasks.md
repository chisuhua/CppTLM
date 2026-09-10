# Tasks: cpptlm-pcie-ep-foundation — dGPU PCIe EP 基础必备能力补完

> **TDD 纪律**: 每个 task 顺序 = Write test → Verify fail → Implement → Verify pass → Commit
> **状态**: 🔄 Proposed v1.0（2026-09-09）
> **前置基线**: CppTLM 现有 22 ABI 函数（7 个根本错误）+ 14 PCIe 头文件骨架 + 10 PCIe .cc 实现
> **工期**: 4.0-5.0 周（基础必备 4 步；按 Oracle 2026-09-09 修订拆分 1.3 → 1.3a-d 4 子阶段）+ 1 周（性能增强 1 步）= **5.0-6.0 周**（per UE entry §2.2 总计）
> **关联**: [proposal.md](proposal.md) + [design.md](design.md) + [specs/cpptlm-pcie-ep-foundation/spec.md](specs/cpptlm-pcie-ep-foundation/spec.md)

---

## §1 任务总览

| Wave | 子任务 | 工期 | 依赖 | 优先级 |
|------|--------|:---:|------|:------:|
| **阶段 1.1** | PCIe EP 基础（修复 #3 + #5 + #6 + #7） | 0.5-1 周 | — | P0 |
| **阶段 1.2** | MSI-X 中断（修复 #4） | 0.5 周 | 阶段 1.1 | P0 |
| **阶段 1.3a** | SDMA 基础（Ring Buffer + RPTR/WPTR + Doorbell + SG） | 1 周 | 阶段 1.2 | P0 |
| **阶段 1.3b** | D2D SDMA 路径（NoC 数据面 + 显存 bypass） | 0.5-1 周 | 阶段 1.3a | P0 |
| **阶段 1.3c** | dma_translate_cb + GART/IOMMU + CP→SDMA（修复 #2） | 0.5 周 | 阶段 1.3b | P0 |
| **阶段 1.3d** | SDMA 完成通知（Fence + MSI-X 接线，修复 #4） | 0.5 周 | 阶段 1.3c | P0 |
| **阶段 1.4** | 电源管理 | 0.5 周 | 阶段 1.3d | P0 |
| **阶段 2.1** | P2P + Resizable BAR | 1 周 | 阶段 1.4 | P1 |
| **总计** | | **5.0-6.0 周** | | |

**关键路径**: 阶段 1.1 → 1.2 → 1.3 → 1.4 → 2.1

---

## §2 阶段 1.1: PCIe EP 基础

> **实施跟踪重定向**（Oracle R1 修订 2026-09-10）：任务 1.1.1-1.1.4（测试骨架 + 修复 #3/#5/#6）已迁移至 [`2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`](../2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/tasks.md) §1-§4（4 bug 修复 TDD 5 步）。本 change 仅保留不重复的残余任务。

### 任务 1.1.5：PCIe Link 管理（LTSSM + 速率/宽度协商）★残余

- [ ] **Modify**: `src/tlm/pcie/pcie_link_layer_tlm.cc` 补全状态机
- [ ] **Implement**: L0/L0s/L1/L2/L3 状态切换 + x16 Gen4/Gen5 协商
- [ ] **Verify pass**: link_state_machine 单元测试

---

## §3 阶段 1.2: MSI-X 中断

> **实施跟踪重定向**（Oracle R1 修订 2026-09-10）：任务 1.2.1（修复 #4 中断链断裂）已迁移至 [`2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`](../2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/tasks.md) §7（阶段 1.2 MSI-X，含 intr_cb 真实接线 + 200ms 触发 open-spec）。本 change 仅保留不重复的残余任务。

### 任务 1.2.2：MSI-X Capability + 向量表 ★残余

- [ ] **Modify**: `src/tlm/pcie/pcie_msix_per_vf_tlm.cc`
  - 实现 MSI-X Extended Capability
  - 至少 4-8 个中断向量
- [ ] **Verify pass**: msix_init + msix_update_pending 真实生效

### 任务 1.2.3：中断节流（Interrupt Coalescing）★残余

- [ ] **Implement**: 基础中断合并机制（避免高频小任务导致中断风暴）
- [ ] **Verify pass**: msix_throttle_test PASS

---

## §4 阶段 1.3: DMA 引擎

> **实施跟踪重定向**（Oracle R1 修订 2026-09-10）：4 子阶段（1.3a SDMA 基础 / 1.3b D2D 路径 / 1.3c dma_translate+IOMMU+CP→SDMA / 1.3d 完成通知）已整体迁移至 [`2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`](../2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/tasks.md) §8-§11（含 Oracle 量化 AC：Ring 4 档 / BAR1+0x10010000 / SG≥8 / NoC≥100GB/s / 负 errno）。4 子阶段量化 AC 明细见 [design.md](../2026-09-09-cpptlm-pcie-ep-foundation/design.md) §3.3 + fixes tasks.md §8-§11。

---

## §5 阶段 1.4: 电源管理

> **实施跟踪重定向**（Oracle R1 修订 2026-09-10）：阶段 1.4（PM Capability + D0/D3 + ASPM）已迁移至 [`2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`](../2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/tasks.md) §12。

---

## §6 阶段 2.1: P2P + Resizable BAR

> **实施跟踪重定向**（Oracle R1 修订 2026-09-10）：阶段 2.1（P2P DMA + ACS + Resizable BAR）已迁移至 [`2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`](../2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/tasks.md) §13。

## §7 关键路径

**阶段 1.1** (PCIe EP 基础) → **1.2** (MSI-X) → **1.3a** (SDMA 基础) → **1.3b** (D2D 路径) → **1.3c** (翻译+CP→SDMA) → **1.3d** (完成通知) → **1.4** (电源) → **2.1** (P2P)

**总工时**: 0.5-1 + 0.5 + 1 + 0.5-1 + 0.5 + 0.5 + 0.5 + 1 = **5.0-6.0 周**（per Oracle 2026-09-09 修订：原 1.3 = 0.5 周严重低估；2026-09-10 算术修正 4.0-5.0 → 5.0-6.0）

---

## §8 风险与回退

### 风险 1：mmio_read 修复影响 TLP 注入时序（中）

**症状**：wait_for 超时延长导致 5.5.7.1 profile 测试偶发 -110
**回退**：保留 timeout 但延长到 10ms；cpptlm_sim_loop tick 频率提升

### 风险 2：register_dma_translate_cb 真实调用 cb 触发 UsrLinuxEmu 错误处理（中）

**症状**：UsrLinuxEmu 侧 cb 失败（如未注册 IOMMU）导致 DMA transfer 全部返 0
**回退**：cb 失败时 fallback 返 pa=iova（identity mapping）

### 风险 3：中断链修复后 msix 测试不稳定（中）

**症状**：intr_cb 触发后测试偶发超时
**回退**：测试用 100ms 超时 + retry 1 次

### 风险 4：电源管理状态切换影响 profile 加载（低）

**症状**：profile `dgpu_board_v1.json` 加载时处于 D3 状态导致 mmio_read 失败
**回退**：profile 加载时强制 D0（默认 active 状态）

### 风险 5：跨仓协调——UsrLinuxEmu 端升级断言（低）

**症状**：本 change 完成但 UsrLinuxEmu 5.5.7.1 测试仍是 `CHECK(ret != -ENOSYS)`，未升级为数据内容验证
**回退**：本 change 仅改 CppTLM；UsrLinuxEmu 端独立 follow-up 升级断言

---

## §9 跨引用

- [proposal.md](proposal.md) — Why/What/Capabilities/Impact
- [design.md](design.md) — 技术设计
- [specs/cpptlm-pcie-ep-foundation/spec.md](specs/cpptlm-pcie-ep-foundation/spec.md) — capability 规范
- Oracle 审查三连（PCIe EP 3.5/10 + CP attach 3.5/10 + 5.5.8 8.7/10）
- UsrLinuxEmu 5.5.6+ dGPU E2E 主线解锁（依赖本 change 完成）

---

**任务作者**: CppTLM Architecture Team
**创建日期**: 2026-09-09
**预期完成**: 2026-10-07（3-4 周后）