# Tasks: cpptlm-pcie-ep-foundation — dGPU PCIe EP 基础必备能力补完

> **TDD 纪律**: 每个 task 顺序 = Write test → Verify fail → Implement → Verify pass → Commit
> **状态**: 🔄 Proposed v1.0（2026-09-09）
> **前置基线**: CppTLM 现有 22 ABI 函数（7 个根本错误）+ 14 PCIe 头文件骨架 + 10 PCIe .cc 实现
> **工期**: 2-3 周（基础必备 4 步）+ 1 周（性能增强 1 步）
> **关联**: [proposal.md](proposal.md) + [design.md](design.md) + [specs/cpptlm-pcie-ep-foundation/spec.md](specs/cpptlm-pcie-ep-foundation/spec.md)

---

## §1 任务总览

| Wave | 子任务 | 工期 | 依赖 | 优先级 |
|------|--------|:---:|------|:------:|
| **阶段 1.1** | PCIe EP 基础（修复 #3 + #5 + #6 + #7） | 0.5-1 周 | — | P0 |
| **阶段 1.2** | MSI-X 中断（修复 #4） | 0.5 周 | 阶段 1.1 | P0 |
| **阶段 1.3** | DMA 引擎（修复 #2） | 0.5 周 | 阶段 1.2 | P0 |
| **阶段 1.4** | 电源管理 | 0.5 周 | 阶段 1.3 | P0 |
| **阶段 2.1** | P2P + Resizable BAR | 1 周 | 阶段 1.4 | P1 |
| **总计** | | **3-4 周** | | |

**关键路径**: 阶段 1.1 → 1.2 → 1.3 → 1.4 → 2.1

---

## §2 阶段 1.1: PCIe EP 基础

### 任务 1.1.1：建立 CppTLM ABI 单元测试骨架

- [ ] **Write test**: `tests/abi/test_cpptlm_emulator_abi.cc`
  - `TEST_CASE("abi: mmio_read returns real data (not garbage)", "[abi][mmio]")`
  - `TEST_CASE("abi: mmio_write data actually persisted", "[abi][mmio]")`
  - `TEST_CASE("abi: backdoor_read miss returns -ENOENT (not len)", "[abi][backdoor]")`
  - `TEST_CASE("abi: pcie_config_read returns vendor_id 0x10DE", "[abi][config]")`
- [ ] **Verify fail**: 当前实现下：
  - mmio_read 返 0 但 buf 未填充（assert 失败）
  - mmio_write 数据未持久化（assert 失败）
  - backdoor_read miss 返 4（assert 失败）
  - pcie_config_read 返 -ENOSYS（assert 失败）
- [ ] **Implement**: 测试骨架（无修复代码）
- [ ] **Verify pass**: 测试 fail 符合预期（用于驱动 4 步修复）

### 任务 1.1.2：修复 #3（pcie_config_read/write -ENOSYS）

- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:151-157`
  - `DGpuBoard::config_read` 转发 `ep_->cfg_space_->read(offset, width, value)`
  - `DGpuBoard::config_write` 转发 `ep_->cfg_space_->write(offset, width, value)`
- [ ] **Verify pass**: test_pcie_config_read 返 0 + value = Vendor ID 0x10DE

### 任务 1.1.3：修复 #5（mmio_read 数据缺口）+ #7（mmio_write 同步阻塞）

- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:106-133`
  - `mmio_read` 等待 sim_loop drain 完整 + 真实填充 buf
  - `mmio_write` 同步阻塞至 sim_loop drain 完成 + 数据真正存储
  - 实现 TLP 注入 TODO T-bs-3c
- [ ] **Verify pass**: mmio_read/write 测试用例全 PASS

### 任务 1.1.4：修复 #6（backdoor_read 语义错位）

- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:180`
  - backdoor_read 未命中返 -ENOENT（不是 len）
  - backdoor 数据落 MemoryTLM 'vram' 真实容量（让 roundtrip 跨段可读）
- [ ] **Verify pass**: test_backdoor_read_enoent PASS + test_backdoor_read_data_roundtrip PASS

### 任务 1.1.5：PCIe Link 管理（LTSSM + 速率/宽度协商）

- [ ] **Modify**: `src/tlm/pcie/pcie_link_layer_tlm.cc` 补全状态机
- [ ] **Implement**: L0/L0s/L1/L2/L3 状态切换 + x16 Gen4/Gen5 协商
- [ ] **Verify pass**: link_state_machine 单元测试

---

## §3 阶段 1.2: MSI-X 中断

### 任务 1.2.1：修复 #4（中断链断裂）

- [ ] **Modify**: `src/abi/cpptlm_emulator.cc` + `src/tlm/pcie/pcie_endpoint_ip.cc`
  - `register_callbacks` 5 cb 全部真实接线（含 reset/power）
  - 新增 `pcie_ep.irq_out` → `board->trigger_irq_async` 接线
- [ ] **Write test**: `test_msix_update_pending_triggers_intr_cb`
- [ ] **Verify fail**: 当前实现下 cb_called = 0（200ms 后）
- [ ] **Verify pass**: cb_called ≥ 1（msix_update_pending 后 200ms 内）

### 任务 1.2.2：MSI-X Capability + 向量表

- [ ] **Modify**: `src/tlm/pcie/pcie_msix_per_vf_tlm.cc`
  - 实现 MSI-X Extended Capability
  - 至少 4-8 个中断向量
- [ ] **Verify pass**: msix_init + msix_update_pending 真实生效

### 任务 1.2.3：中断节流（Interrupt Coalescing）

- [ ] **Implement**: 基础中断合并机制（避免高频小任务导致中断风暴）
- [ ] **Verify pass**: msix_throttle_test PASS

---

## §4 阶段 1.3: DMA 引擎（**已细分 4 子阶段** per design.md §3.3）

> **重要修订背景**：Oracle 2026-09-09 审查指出，原"阶段 1.3 = 0.5 周"严重低估（现有 `sdma_engine_tlm.cc` 是 descriptor 直投而非 Ring Buffer 架构，需"补架构"而非"修 bug"）。按 SDMA 内部设计（[`docs/soc_arch/architecture/17-sdma-engine-design.md`](../../../docs/soc_arch/architecture/17-sdma-engine-design.md) §1-§14 全 14 章节）拆分如下。量化 AC 待下次 Oracle 1 次轻量复审确认。

### §4.1 阶段 1.3a: PCIe SDMA 基础

> **工期**: 1 周 | **对应 SDMA 设计**: §2-§6（类 + Ring + RPTR/WPTR + Doorbell + Packet）| **原任务**: 1.3.1 + 1.3.2 + 新增 Ring/RPTR/WPTR/Doorbell 绑定

**量化 AC（Oracle 复审确认 2026-09-09，per design §3.2/§3.3/§2/§13）**:
- Ring Buffer 容量: `cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}`，entry 32B（basic）/ 64B（含 SG），最大 1024 条目 @64B（per design §3.2；原 256KB 与 §3.3 BAR1 布局冲突，AMEND）
- RPTR / WPTR 位宽: 32 位（per design §2 `std::atomic<uint32_t>` / §3.3 寄存器 4B）
- Doorbell 寄存器: `BAR1 + 0x10010000`，4B，仅 WPTR 写入触发（per design §3.3 / §5.1 L296；原 0x18 无仓内出处，AMEND）
- SG 描述符支持: 单描述符链长度 ≥ 8（per design §2 L157 / §13 L785 `MAX_SG_ENTRIES = 8`；原 ≥16 与设计冻结值不符，AMEND）

**任务清单**:
- [ ] **任务 1.3a.1**: 新建 `src/tlm/gpu/sdma_ring_buffer.h/cc` — Ring Buffer 数据结构（`cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}`，最大 1024 entries @64B + 32-bit RPTR/WPTR + 内存屏障；per design §3.2）
- [ ] **任务 1.3a.2**: 新建 `src/tlm/gpu/sdma_packet.h/cc` — Packet 数据结构（含 SG 描述符链，链长 ≥ 8；per design §2/§13 `MAX_SG_ENTRIES=8`）
- [ ] **任务 1.3a.3**: `src/tlm/gpu/sdma_engine_tlm.cc` 改造 — descriptor 直投 → Ring Buffer + RPTR/WPTR + Doorbell 绑定（`BAR1 + 0x10010000`；per design §3.3/§5.1 L296）
- [ ] **任务 1.3a.4**: `src/tlm/gpu/dma_descriptor_mvp.hh` + `dma_bundles_tlm.hh` — `Dir::D2D` 扩展 + SG 描述符
- [ ] **Write test**: `test_sdma_ring_rptr_wptr`（Ring Buffer RPTR/WPTR 正确性）+ `test_sg_descriptor_chain`（SG 链正确性）
- [ ] **Verify pass**: Ring Buffer RPTR/WPTR + SG 描述符 + Doorbell 绑定测试全 PASS

### §4.2 阶段 1.3b: D2D SDMA 路径

> **工期**: 0.5-1 周 | **对应 SDMA 设计**: §10（D2D 路径）| **原任务**: 全新增（无 1.3.1-1.3.3 对应）

**量化 AC（Oracle 复审确认 2026-09-09，per design §10.4/§10.3/§10.1）**:
- NoC 数据面带宽: ≥ 100 GB/s（per design §10.4 "数百 GB/s" 保守下限；原 32 GB/s 是 PCIe P2P 列张冠李戴，AMEND）
- 显存控制器 bypass: 路径不经过 host_out 端口（断言 host_out 零事务；per design §10.3）
- D2D descriptor: `Dir::D2D` 类型 + NoC target address（显存 VA；per design §10.1）

**任务清单**:
- [ ] **任务 1.3b.1**: 新建 `src/tlm/gpu/d2d_noc_path.h/cc` — D2D NoC 路径（payload 转发）
- [ ] **任务 1.3b.2**: `src/tlm/gpu/gpu_mesh_noc.h/cc` — NoC 从延迟模型扩为 payload 转发（带宽 ≥ 100 GB/s；per design §10.4）
- [ ] **任务 1.3b.3**: 显存控制器 bypass 路径 — 写入直达 VRAM（绕过 PCIe TLP）
- [ ] **Write test**: `test_d2d_noc_path`（NoC payload 转发正确性）+ `test_host_out_zero_transactions`（断言 host_out 零事务）
- [ ] **Verify pass**: D2D 路径正确 + host_out 零事务测试 PASS

### §4.3 阶段 1.3c: dma_translate_cb + GART/IOMMU + CP→SDMA

> **工期**: 0.5 周 | **对应 SDMA 设计**: §8（地址翻译）+ §11（CmdProc 集成）| **原任务**: 1.3.1 + 1.3.3

**量化 AC（Oracle 复审确认 2026-09-09，per design §8.1/§8.2/§8.3/§11.2）**:
- 4 级翻译链（per design §8.1，CPU VA→PA→IOVA→GPU MC 四**阶段**，非 x86 四级页表 walk；措辞对齐避免实现者误读）
- 双模式: identity mapping + IOMMU 翻译（VT-d / AMD IOMMU 兼容；per design §8.1/§8.2）
- CP→SDMA 转发: PM4 DMA opcode `0x4600-0x4900` 范围（per design §11.2 表）
- 修复 #2: identity 模式 `pa = iova` 返回 0；IOMMU 模式 cb 失败传播**负 errno**（-ENOSYS/-EIO）至 `error_cb`（per design §8.2/§8.3；原 "0 错误码（IOMMU 模式失败）" 0=成功 语义颠倒，AMEND）

**任务清单**:
- [ ] **任务 1.3c.1**: `src/abi/cpptlm_emulator.cc:443-460` — 修复 #2（lambda 内移除 `(void)cb`，真实调用 UsrLinuxEmu cb）
  - 签名适配两套：board shell 层 vs SDMA 引擎层
  - cb 失败处理（per design §8.2/§8.3）:
    - identity 模式: 返 `pa = iova` 返回 0
    - IOMMU 模式: cb 失败传播**负 errno**（-ENOSYS/-EIO）至 `error_cb`
- [ ] **任务 1.3c.2**: `src/tlm/pcie/pcie_endpoint_ip.cc` — GART/IOMMU 4 级翻译链（identity + IOMMU 双模式）
- [ ] **任务 1.3c.3**: `src/tlm/gpu/command_processor_mvp.cc` — DISPATCH 态 dma_req 分支（PM4 opcode 0x4600-0x4900 映射）
- [ ] **Write test**: `test_register_dma_translate_cb_returns_iova`（identity mapping）+ `test_dma_translate_iommu`（IOMMU 4 级翻译）
- [ ] **Verify fail**: 当前 pa=0（identity 是 iova，不是 0）
- [ ] **Verify pass**: cb 真实调用，pa == iova；IOMMU 翻译正确

### §4.4 阶段 1.3d: SDMA 完成通知

> **工期**: 0.5 周 | **对应 SDMA 设计**: §9（完成通知）| **原任务**: 全新增（无 1.3.1-1.3.3 对应，修复 #4）

**量化 AC（Oracle 复审确认 2026-09-09，per design §9.2/§9.1 + UE entry §5.1/§7.1）**:
- Fence 命令（Ring 内）正确触发完成事件（per design §9.2）
- 完成通知非量化 AC: done_out → CompletionRing → MSI-X → intr_cb 链路在测试 **200ms 超时窗口内触发 ≥1 次**（对齐 entry §5.1/§7.1 验证协议；原 ≤1ms 引用链断裂 + TLM wall-clock 无可重复性，REJECT 改为非量化）
- MSI-X vector 非量化 AC: vector 由 entry/驱动指定并正确路由至对应 intr_cb；vector 值须在 `msix_init` table_size 合法范围内（per design §9.1 `trigger_msix(vector)` 透传；原 vector 0-3 硬编无设计出处，REJECT 改为非量化）
- 修复 #4: 中断链断裂（per entry §9 Oracle 风险行 + §9.1 调用链完整）

**任务清单**:
- [ ] **任务 1.3d.1**: `src/tlm/gpu/sdma_engine_tlm.cc` — Fence 命令支持（Ring 内 Fence descriptor）
- [ ] **任务 1.3d.2**: 新建 `src/tlm/gpu/sdma_completion_ring.h/cc` — done_out → CompletionRing 转发
- [ ] **任务 1.3d.3**: `src/abi/cpptlm_emulator.cc` — MSI-X vector 路由（vector 由 entry/驱动指定，须在 `msix_init` table_size 合法范围内；per design §9.1）
- [ ] **Write test**: `test_sdma_fence`（Fence 命令触发完成事件）+ `test_msix_completion`（MSI-X 触发延迟）
- [ ] **Verify pass**: Fence + MSI-X 接线测试全 PASS；修复 #4 验证（UsrLinuxEmu 侧 intr_cb 在 200ms 内 ≥1 次触发）

### §4.5 累计影响面

**修改文件（总计）**:
- `src/abi/cpptlm_emulator.cc` — 修复 #2（1.3c）+ MSI-X 接线（1.3d）
- `src/tlm/gpu/sdma_engine_tlm.cc` — Ring/RPTR/WPTR/Doorbell + SG（1.3a）+ Fence（1.3d）
- `src/tlm/pcie/pcie_endpoint_ip.cc` — IOMMU 翻译（1.3c）
- `src/tlm/gpu/command_processor_mvp.cc` — DISPATCH dma_req（1.3c）
- `src/tlm/gpu/dma_descriptor_mvp.hh` + `dma_bundles_tlm.hh` — Dir::D2D + SG（1.3a）
- `src/tlm/gpu/gpu_mesh_noc.h/cc` — NoC payload 转发（1.3b）
- 新建：`src/tlm/gpu/sdma_ring_buffer.h/cc`（1.3a）+ `sdma_packet.h/cc`（1.3a）+ `d2d_noc_path.h/cc`（1.3b）+ `sdma_completion_ring.h/cc`（1.3d）

**总计**: 工期 **2.5-3 周**（原 0.5 周；Oracle 修订后），任务数 **+13**（原 3 任务，拆分为 4 子阶段约 16 checkbox）。

---## §5 阶段 1.4: 电源管理

### 任务 1.4.1：PCIe PM Capability

- [ ] **Modify**: `src/tlm/pcie/pcie_config_space_per_vf_tlm.cc`
  - PM Capabilities / Control / Status 寄存器
- [ ] **Verify pass**: pm_capability_test PASS

### 任务 1.4.2：D0/D3hot/D3cold 状态切换

- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc` + `src/tlm/gpu/dgpu_soc.cc`
  - 状态机实现
- [ ] **Verify pass**: power_state_transition_test PASS

### 任务 1.4.3：ASPM（L0s / L1 低功耗）

- [ ] **Implement**: ASPM 自动协商
- [ ] **Verify pass**: aspm_test PASS

---

## §6 阶段 2.1: P2P + Resizable BAR

### 任务 2.1.1：P2P DMA

- [ ] **Modify**: `src/tlm/pcie/pcie_bypass_mux.cc` + `src/tlm/pcie/pcie_ari_router_tlm.cc`
- [ ] **Verify pass**: p2p_dma_test PASS

### 任务 2.1.2：ACS Capability

- [ ] **Implement**: ACS Capability 寄存器 + 路由策略
- [ ] **Verify pass**: acs_test PASS

### 任务 2.1.3：Resizable BAR

- [ ] **Modify**: `src/tlm/pcie/pcie_config_space_per_vf_tlm.cc`
  - Resizable BAR Capability
- [ ] **Verify pass**: resizable_bar_test PASS

---

## §7 关键路径

**阶段 1.1** (PCIe EP 基础) → **1.2** (MSI-X) → **1.3a** (SDMA 基础) → **1.3b** (D2D 路径) → **1.3c** (翻译+CP→SDMA) → **1.3d** (完成通知) → **1.4** (电源) → **2.1** (P2P)

**总工时**: 0.5-1 + 0.5 + 1 + 0.5-1 + 0.5 + 0.5 + 0.5 + 1 = **4.0-5.0 周**（per Oracle 2026-09-09 修订：原 1.3 = 0.5 周严重低估）

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