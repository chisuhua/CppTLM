# Proposal: cpptlm-stage-1-2-msix — MSI-X 中断（修复 #4 中断链断裂）

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **优先级**: P0（前置 UsrLinuxEmu 5.5.7 启动 gate 部分解锁）
> **工期**: 0.5 周
> **前置依赖**:
> - CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` ship（修复 #3/#5/#6 完成）
> - 4 bug 修复后 ABI 通道真实化
> **关联 change**:
> - 父 change：`2026-09-09-cpptlm-pcie-ep-foundation`（spec SSOT）
> - 上游：`2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`（基础必备）
> - 下游：`2026-09-10-ue-stage-1-2-msix-integration`（UE 集成）
> **关联 ADR**:
> - [ADR-091](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2 — 4 象限
> - [ADR-092](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-092-hal-adapter-and-bypass-binding.md) ✅ Accepted v0.2 — HAL adapter

---

## Why

阶段 1.2 MSI-X 中断修复（修复 #4 中断链断裂）的实施缺口。5.5.6 dGPU E2E 主线 P0 ship 后暴露"`cpptlm_emulator_msix_init/update/clear` intr_cb 真实接线但 `trigger_irq_async` 全仓无调用方"，造成测试验证 0 触发。

**Why 单独 change（方案 B 拆分）**：
1. **原子 Wave**：单 Wave 0.5 周可完整实施 + archive
2. **独立进度**：5.5.7 gate 解锁仅需 stage-1-1 + stage-1-2 + stage-1-3a + bridge-sync ship，stage-1-2 MSI-X 是其中之一
3. **清晰边界**：与 stage-1-1（4 bug 修复）和 stage-1-3（SDMA）解耦

## What Changes

实施 `src/tlm/pcie/pcie_endpoint_ip.cc` intr_cb 真实接线 + `cpptlm_emulator.cc` msix_update_pending 触发路径：
1. **intr_cb 真实触发**：`trigger_irq_async(vector, payload)` 真实实现（当前 stub 抛 -ENOSYS）
2. **inject_q_ → sim_loop drain**：MSI-X TLP 推入 inject_q_，由 sim_loop 在 tick 内 drain
3. **完成后调 intr_cb(vector, payload)** 通知 UE bridge
4. **msix_init 后立即可用 trigger_irq_async**：消除"接线但无调用方"

### 修改文件清单

- `src/tlm/pcie/pcie_endpoint_ip.cc` — `trigger_irq_async` 真实实现
- `src/abi/cpptlm_emulator.cc` — `register_callbacks` intr_cb 路径验证
- `test/test_dgpu_msix.cc` — 新建单元测试
- `docs/soc_arch/architecture/18-pcie-endpoint-entry.md` §12 v0.5 entry sync

## Impact

### 影响的 specs
- 新建 `cpptlm-stage-1-2-msix` spec — 修复 #4 中断链 ADDED Requirements

### 影响的代码
- **修改**：`pcie_endpoint_ip.cc` (trigger_irq_async) + `cpptlm_emulator.cc` (msix 路径)
- **新建**：`test/test_dgpu_msix.cc`
- **不改**：
  - ❌ `include/abi/cpptlm_emulator.h`（23 ABI 冻结）
  - ❌ `wire-format-snapshot.json`（5 ports 冻结）

### 影响的下游
- **UsrLinuxEmu `2026-09-10-ue-stage-1-2-msix-integration` change** 集成测试可启动
- **5.5.7 启动 gate 部分解锁**（条件：stage-1-1 + stage-1-2 + stage-1-3a + bridge-sync ship）

### 风险评估

- **低风险**：trigger_irq_async 路径独立，不影响其他 ABI
- **需澄清**：msix vector 路由由 driver 指定（非硬编 0-3），符合 design.md §9.1

---

## Oracle 复审（验收）

- **完成条件**：test_dgpu_msix PASS + 既有 ABI 测试零回归
- **Oracle session**：[Oracle 续接 Gate E / 1 次轻量复审]
- **验收标准**：见 specs/cpptlm-stage-1-2-msix/spec.md ADDED Requirements

## refs

- ADR-091 4 象限布局
- ADR-092 HAL adapter + bypass binding
- 上游 change `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`（阶段 1.1 4 bug 修复）
- 下游 change `2026-09-10-ue-stage-1-2-msix-integration`（UE 集成）
- 探索报告 Oracle `ses_f76db853affeOKwc99O4walT80`
