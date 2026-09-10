# Tasks: cpptlm-stage-1-2-msix

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 0.5 周
> **TDD 纪律**: 每任务 5 步（write failing test → verify fail → implement → verify pass → commit）

## §1 实施修复 #4 中断链

### 任务 1.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_msix.cc::test_msix_intr_cb_called_within_200ms`
  - mock intr_cb，msix_init + trigger_irq_async(vector=0, payload=0xDEADBEEF)
  - 等待 200ms，检查 intr_cb_called ≥ 1
- [ ] **Verify fail**: 当前 trigger_irq_async 返 -ENOSYS，cb_called = 0

### 任务 1.2: 实施 trigger_irq_async 真实接线
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - `msix_pending_[vector]` + `msix_payload_[vector]` 数据结构
  - `trigger_irq_async(vector, payload)` 真实实现（构造 MSI-X TLP + inject_q_ + completion_cb）
  - completion_cb 在 sim_loop drain 后调 `intr_cb_(vector, payload)`
- [ ] **Modify**: `src/abi/cpptlm_emulator.cc`
  - 验证 `register_callbacks` 后 intr_cb 路径完整
  - msix_init 后立即可用 trigger_irq_async
- [ ] **Verify pass**: 200ms 内 intr_cb ≥1 次触发，payload = 0xDEADBEEF

### 任务 1.3: Commit
- [ ] **Commit (CppTLM)**: `fix(cpptlm): intr_cb 真实触发 + trigger_irq_async 接线 (修复 #4 MSI-X)`
- [ ] **Commit (UE entry sync)**: `docs(pcie-ep): v0.5 stage 1.2 MSI-X ship`
- [ ] **Commit (CppTLM 18-doc mirror)**: `docs(cpptlm): v0.5.4 mirror UE stage 1.2 MSI-X ship`

## §2 Oracle 复审（验收）

> **Oracle 复审位置（Metis M6 修订 2026-09-10）**：实施 commit 后、docs mirror commit **前**进行。复审发现问题需追加 commit 而非 amend docs。


### 任务 2.1: 1 次轻量复审
- [ ] **Oracle review**: intr_cb 真实触发 + 修复 #4 闭环验证
- [ ] **Verify**: `ctest -R "dgpu|msix"` 全 PASS（既有 ABI 测试零回归）

## §2.5 残余任务（从 cpptlm-pcie-ep-foundation 迁移）

> **Oracle R-E 修订 2026-09-10**：1.2.2 MSI-X Cap + 1.2.3 中断节流原属 foundation 残余 3 任务之一，因 stage-1-2-msix 的 `msix_init(table_size)` 语义依赖 Cap 存在，并入本 change 实施更合理。

### 任务 2.5.1：MSI-X Capability + 4-8 向量表（基础任务 1.2.2）
- [ ] **Modify**: `src/tlm/pcie/pcie_msix_per_vf_tlm.cc`
  - 实现 MSI-X Extended Capability 结构
  - 至少 4-8 个中断向量表项
- [ ] **Write test**: `test/test_dgpu_msix_cap.cc::test_msix_cap_vector_count_4_to_8`
  - 验证 Cap 存在 + table_size ∈ [4, 8]
- [ ] **Verify fail → implement → Verify pass**: 表项可独立 mask/unmask
- [ ] **Commit (CppTLM)**: `feat(cpptlm): MSI-X Cap 4-8 vectors (基础 1.2.2)`

### 任务 2.5.2：中断节流 Interrupt Coalescing（基础任务 1.2.3）
- [ ] **Implement**: `pcie_endpoint_ip.cc` 基础中断合并机制（避免高频小任务导致中断风暴）
  - 可配置 threshold（默认 8）+ timeout（默认 50us）
- [ ] **Write test**: `test/test_dgpu_msix_throttle.cc::test_msix_throttle_burst_8_in_one_intr`
- [ ] **Verify fail → implement → Verify pass**: 8 次连续 trigger 合并为 1 次 intr_cb 触发
- [ ] **Commit (CppTLM)**: `feat(cpptlm): MSI-X interrupt coalescing (基础 1.2.3)`

## §3 总计

- **CppTLM 仓 commits**: 3（修复 + UE entry sync + CppTLM 18-doc mirror）
- **工期**: 0.5 周
- **Oracle 复审**: 1 次轻量
- **下游**: UE `2026-09-10-ue-stage-1-2-msix-integration` change 解锁
