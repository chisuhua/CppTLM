# Tasks: cpptlm-stage-1-1-pcie-ep-fixes

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 0.5-1 周（实施顺序 #3 → #6 → #5 → #7）
> **TDD 纪律**: 每任务 5 步（write failing test → verify fail → implement → verify pass → commit）

## §1 阶段 1.1.1: 修复 #3 (pcie_config_read/write 转发)

### 任务 1.1.1.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_pcie_config.cc::test_pcie_config_read_vendor_id_returns_0x10DE`
- [ ] **Verify fail**: 当前返 -ENOSYS

### 任务 1.1.1.2: 实施转发
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:151-164`
  - pcie_config_read: null check → 转发 `ep_->cfg_space_->read(offset, width, val)`
  - pcie_config_write: null check → 转发 `ep_->cfg_space_->write(offset, width, val)`
- [ ] **Verify pass**: 测试通过

### 任务 1.1.1.3: Commit
- [ ] **Commit**: `fix(dgpu-board): forward pcie_config_read/write to cfg_space (修复 #3)`

## §2 阶段 1.1.2: 修复 #6 (backdoor_read miss 返 -ENOENT)

### 任务 1.1.2.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_backdoor.cc::test_backdoor_read_miss_returns_enoent_not_len`
- [ ] **Verify fail**: 当前 miss 返 `len` (例如 4)

### 任务 1.1.2.2: 实施返值修正
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:168-190`
  - 加 buf nullptr 检查
  - miss 返 `-ENOENT`（-38）
  - size mismatch 返 `-EINVAL`
  - hit 返 0 + memcpy
- [ ] **Verify pass**: 测试通过

### 任务 1.1.2.3: Commit
- [ ] **Commit**: `fix(dgpu-board): backdoor_read miss returns -ENOENT (修复 #6)`

## §3 阶段 1.1.3: 修复 #5 (mmio_read 数据拷贝)

### 任务 1.1.3.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_mmio.cc::test_mmio_read_real_data_not_garbage`
- [ ] **Verify fail**: 当前 buf 未填充，data 是 garbage

### 任务 1.1.3.2: 实施数据拷贝
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:106-133`
  - 新数据结构 `pending_data_: std::unordered_map<uint64_t, std::vector<uint8_t>>`
  - sim_loop drain 真实化（`drain_injection_queue()` 注入器 set_value + set data）
  - mmio_read 等待后 memcpy data → buf
  - 返值改为 byte count 或负 errno
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc` (sim_loop drain 实现)
- [ ] **Verify pass**: 测试通过（buf 真实填充）

### 任务 1.1.3.3: Commit
- [ ] **Commit**: `fix(dgpu-board): mmio_read real data copy (修复 #5)`

## §4 阶段 1.1.4: 修复 #7 (mmio_write 文档矛盾澄清)

### 任务 1.1.4.1: 修改 design.md
- [ ] **Modify**: `openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md §3.1`
  - 删除 "blocks synchronously" 措辞
  - 添加 "mmio_write returns 0 immediately (async); data is drained by sim_loop on next tick"

### 任务 1.1.4.2: 验证架构文档一致性
- [ ] **Verify**: `architecture/18-pcie-endpoint-entry.md §2.1` 已说 "MMIO Write 是异步"
- [ ] **Verify**: 代码 `dgpu_board_shell.cc:148` 返 0 立即返回

### 任务 1.1.4.3: Oracle 轻量复审
- [ ] **Oracle review**: 1 次轻量复审，确认保持 async 裁决
- [ ] **Commit**: `docs(cpptlm): clarify mmio_write async semantics (修复 #7)`

## §5 集成测试

### 任务 5.1: 完整 PCIe EP 基础测试套件
- [ ] **Run**: `./build/bin/cpptlm_tests "[dgpu][pcie]"` — 4 修复全部测试通过
- [ ] **Run**: `./build/bin/cpptlm_tests "[abi]"` — 既有 ABI 测试零回归
- [ ] **Verify**: 152/152 tests PASS (新增 4 测试)

### 任务 5.2: 跨仓集成测试 (UE 侧)
- [ ] **Verify**: UsrLinuxEmu 桥接层测试（`test_bridge_dgpu_with_real_pcie_ep`）PASS
- [ ] **Verify**: UsrLinuxEmu 5.5.6 profile_real 测试从 "ret != -ENOSYS" 升级到数据正确性

## §6 Oracle Gate E 复审（v1.0 → v1.1 升档）

### 任务 6.1: Oracle 实施后复审
- [ ] **Oracle review**: 1 次完整实施复审，4 修复质量评估
- [ ] **Verify**: ADR-035 §R2 v1.0 → v1.1 升档流程通过

### 任务 6.2: 同步双仓 entry
- [ ] **Modify**: UsrLinuxEmu `entry §11` ADR-088 / `cpptlm-pcie-ep-foundation` 关联行
- [ ] **Verify**: CppTLM `18-doc §11` 同步状态

### 任务 6.3: Archive 本 change
- [ ] **Run**: `openspec archive 2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`
- [ ] **Verify**: spec 提升到 `openspec/specs/cpptlm-stage-1-1-fixes/spec.md`

## 总计

- **任务数**: 17 (4 修复 × 3-4 tasks + 5 集成 + 6.1-6.3)
- **预期 commit**: 4-5 (每修复一个 + 集成测试)
- **工期**: 0.5-1 周（实施）+ 0.5 天（Oracle 复审）
- **下游依赖**: UsrLinuxEmu `2026-09-10-ue-stage-1-1-bridge-sync` change 解锁

---

## §7 阶段 1.2: MSI-X 中断（修复 #4 中断链断裂）

### 任务 7.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_msix.cc::test_msix_intr_cb_called_within_200ms`
- [ ] **Verify fail**: 当前 intr_cb 在测试中 0/200 次触发

### 任务 7.2: 实施 intr_cb 真实接线
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - `trigger_irq_async(vector, payload)` 真实实现（当前 stub 抛 -ENOSYS）
  - inject_q_ 推入 MSI-X TLP，由 sim_loop 在 tick 内 drain
  - 完成后调用 `intr_cb(vector, payload)` 通知 UE bridge
- [ ] **Modify**: `src/abi/cpptlm_emulator.cc`
  - `register_callbacks` 已实现，验证 intr_cb 路径完整
  - msix_init 后立即可用 trigger_irq_async
- [ ] **Verify pass**: 200ms 内 intr_cb ≥1 次触发

### 任务 7.3: Commit
- [ ] **Commit (CppTLM)**: `fix(cpptlm): intr_cb 真实触发 + trigger_irq_async 接线 (修复 #4 MSI-X)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.5 stage 1.2 MSI-X ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.5.4 mirror UE stage 1.2 ship`

### 任务 7.4: Oracle 复审
- [ ] **Oracle review**: 1 次轻量复审，确认 intr_cb 真实触发 + 修复 #4 闭环

---

## §8 阶段 1.3a: PCIe SDMA 基础（Ring Buffer + RPTR/WPTR + Doorbell + SG）

> **AC 已 Oracle 复审确认（d99ab2e0）**: cfg.ring_size 4 档 + 1024 max entries + BAR1+0x10010000 + SG ≥8

### 任务 8.1: 写失败测试
- [ ] **Write test**: `test/test_sdma_ring_rptr_wptr.cc`
  - cfg.ring_size=64KB 测试 Ring Buffer RPTR/WPTR 正确性
  - BAR1+0x10010000 写入 WPTR 触发 Doorbell
- [ ] **Write test**: `test/test_sg_descriptor_chain.cc`
  - MAX_SG_ENTRIES=8 描述符链正确性
- [ ] **Verify fail**: 当前 Ring Buffer 不存在 (descriptor 直投)

### 任务 8.2: 实施 Ring Buffer
- [ ] **New**: `src/tlm/gpu/sdma_ring_buffer.h/cc`
  - `std::atomic<uint32_t> wptr_/rptr_` (32-bit, per design §2 L112-113)
  - `cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}` 4 档可配
  - entry 32B (basic) / 64B (含 SG)，max 1024 条目
  - 内存屏障 (atomic 操作)
- [ ] **New**: `src/tlm/gpu/sdma_packet.h/cc`
  - SG 描述符链（MAX_SG_ENTRIES=8，定长数组）
- [ ] **Modify**: `src/tlm/gpu/sdma_engine_tlm.cc`
  - descriptor 直投 → Ring Buffer + RPTR/WPTR + Doorbell
  - Doorbell 寄存器地址 `BAR1 + 0x10010000`，仅 WPTR 写入触发
- [ ] **Modify**: `src/tlm/gpu/dma_descriptor_mvp.hh` + `dma_bundles_tlm.hh`
  - `Dir::D2D` 扩展 + SG 描述符支持

### 任务 8.3: Commit
- [ ] **Commit (CppTLM)**: `feat(sdma): ring buffer + RPTR/WPTR + doorbell + SG (stage 1.3a)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.6 stage 1.3a SDMA Ring Buffer ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.6.0 mirror UE stage 1.3a ship`

### 任务 8.4: Oracle 复审
- [ ] **Oracle review**: Ring Buffer wire-format + 性能基准（≥100 GB/s simulated）

---

## §9 阶段 1.3b: D2D SDMA 路径（NoC 数据面 + 显存 bypass）

> **AC**: NoC ≥100 GB/s (per design §10.4) + host_out 零事务断言 + Dir::D2D + VRAM VA

### 任务 9.1: 写失败测试
- [ ] **Write test**: `test/test_d2d_noc_path.cc::test_noC_payload_forwarding`
- [ ] **Write test**: `test/test_host_out_zero_transactions.cc::test_bypass_no_host_out`
- [ ] **Verify fail**: 当前 NoC 仅延迟模型，不转发 payload

### 任务 9.2: 实施 D2D 路径
- [ ] **New**: `src/tlm/gpu/d2d_noc_path.h/cc` — D2D NoC payload 转发
- [ ] **Modify**: `src/tlm/gpu/gpu_mesh_noc.h/cc`
  - 从延迟模型扩为 payload 转发
  - 带宽 ≥100 GB/s
- [ ] **Modify**: 显存控制器 bypass 路径
  - 写入直达 VRAM（绕过 PCIe TLP）
- [ ] **Verify pass**: NoC payload 正确 + host_out 零事务

### 任务 9.3: Commit
- [ ] **Commit (CppTLM)**: `feat(sdma): D2D NoC payload + memctl bypass (stage 1.3b)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.7 stage 1.3b D2D ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.7.0 mirror UE stage 1.3b ship`

---

## §10 阶段 1.3c: dma_translate_cb + GART/IOMMU + CP→SDMA（修复 #2）

> **AC**: 4 级翻译链 (CPU VA→PA→IOVA→GPU MC) + identity/IOMMU 双模式 + PM4 opcode 0x4600-0x4900 + cb 失败返负 errno

### 任务 10.1: 写失败测试
- [ ] **Write test**: `test/test_register_dma_translate_cb_returns_iova.cc`
  - identity 模式 pa==iova, ret==0
- [ ] **Write test**: `test/test_dma_translate_iommu.cc`
  - 4 级页表翻译正确性
- [ ] **Verify fail**: 当前 cb 失败返 0（错误码语义颠倒）

### 任务 10.2: 实施 cb 真实化 + IOMMU
- [ ] **Modify**: `src/abi/cpptlm_emulator.cc:443-460`
  - lambda 内移除 `(void)cb`
  - identity 模式：pa=iova, ret=0
  - IOMMU 模式：cb 失败传播负 errno (-ENOSYS/-EIO) 至 error_cb
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - GART/IOMMU 4 级翻译链（identity + IOMMU 双模式）
- [ ] **Modify**: `src/tlm/gpu/command_processor_mvp.cc`
  - DISPATCH 态 dma_req 分支（PM4 opcode 0x4600-0x4900 映射）

### 任务 10.3: Commit
- [ ] **Commit (CppTLM)**: `fix(cpptlm): 修复 #2 dma_translate_cb 真实化 + IOMMU (stage 1.3c)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.8 stage 1.3c dma_translate ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.8.0 mirror UE stage 1.3c ship`

---

## §11 阶段 1.3d: SDMA 完成通知（Fence + MSI-X 接线，修复 #4 完成路径）

> **AC**: Fence 命令触发完成 + 200ms 超时窗口内 intr_cb ≥1 次触发 + vector 路由

### 任务 11.1: 写失败测试
- [ ] **Write test**: `test/test_sdma_fence.cc::test_fence_triggers_completion`
- [ ] **Write test**: `test/test_msix_completion.cc::test_intr_cb_within_200ms`
- [ ] **Verify fail**: 当前 Fence 不触发完成事件

### 任务 11.2: 实施 Fence + 完成通知
- [ ] **Modify**: `src/tlm/gpu/sdma_engine_tlm.cc`
  - Fence 命令支持（Ring 内 Fence descriptor, opcode=0x04）
- [ ] **New**: `src/tlm/gpu/sdma_completion_ring.h/cc`
  - done_out → CompletionRing 转发
- [ ] **Modify**: `src/abi/cpptlm_emulator.cc`
  - MSI-X vector 路由（vector 由 entry/驱动指定，须在 msix_init table_size 合法范围内）
- [ ] **Verify pass**: Fence + MSI-X 接线测试全 PASS；200ms 内 ≥1 次触发

### 任务 11.3: Commit
- [ ] **Commit (CppTLM)**: `feat(sdma): fence + MSI-X 接线 + completion ring (stage 1.3d)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.9 stage 1.3d completion ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.9.0 mirror UE stage 1.3d ship`

---

## §12 阶段 1.4: 电源管理（D0/D3 + ASPM）

### 任务 12.1: 写失败测试
- [ ] **Write test**: `test/test_pm_capability.cc`（PM Capabilities/Control/Status 寄存器）
- [ ] **Write test**: `test/test_power_state_transition.cc`（D0 ↔ D3hot ↔ D3cold）
- [ ] **Write test**: `test/test_aspm.cc`（L0s / L1 自动协商）

### 任务 12.2: 实施电源管理
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc` + `src/tlm/gpu/dgpu_soc.cc`
  - PM Capabilities 寄存器实现
  - D0/D3hot/D3cold 状态机
  - ASPM L0s/L1 自动协商
- [ ] **Verify pass**: 电源管理测试全 PASS

### 任务 12.3: Commit
- [ ] **Commit (CppTLM)**: `feat(cpptlm): PM D0/D3 + ASPM (stage 1.4)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.10 stage 1.4 power mgmt ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.10.0 mirror UE stage 1.4 ship`

---

## §13 阶段 2.1: P2P + Resizable BAR

### 任务 13.1: 写失败测试
- [ ] **Write test**: `test/test_p2p_dma.cc`（Peer-to-Peer DMA）
- [ ] **Write test**: `test/test_acs_capability.cc`（Access Control Services）
- [ ] **Write test**: `test/test_resizable_bar.cc`（BAR 大小动态调整）

### 任务 13.2: 实施 P2P + Resizable BAR
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - P2P DMA 路由（multi-root switch）
  - ACS Capability 寄存器
  - Resizable BAR capability
- [ ] **Verify pass**: P2P + Resizable BAR 测试全 PASS

### 任务 13.3: Commit
- [ ] **Commit (CppTLM)**: `feat(cpptlm): P2P + Resizable BAR (stage 2.1)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.11 stage 2.1 P2P ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.11.0 mirror UE stage 2.1 ship`

---

## §14 总计

- **CppTLM 仓 commits**: 8（每阶段 1 修复 + 1 entry sync）
- **双仓合计 commits**: 16（CppTLM 8 + UE entry sync 8）
- **工期**: 5.0-6.0 周
- **Oracle 复审**: 8 次轻量（每阶段 1 次）
- **下游**: UsrLinuxEmu 5.5.7 启动 gate 解锁（阶段 1.1+1.2+1.3a ship + bridge-sync ship）

