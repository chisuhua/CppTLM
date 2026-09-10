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

### 任务 2.1: 1 次轻量复审
- [ ] **Oracle review**: intr_cb 真实触发 + 修复 #4 闭环验证
- [ ] **Verify**: `ctest -R "dgpu|msix"` 全 PASS（既有 ABI 测试零回归）

## §3 总计

- **CppTLM 仓 commits**: 3（修复 + UE entry sync + CppTLM 18-doc mirror）
- **工期**: 0.5 周
- **Oracle 复审**: 1 次轻量
- **下游**: UE `2026-09-10-ue-stage-1-2-msix-integration` change 解锁
