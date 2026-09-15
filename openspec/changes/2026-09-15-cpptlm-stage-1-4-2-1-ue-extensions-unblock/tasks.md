# Tasks: cpptlm-stage-1-4-2-1-ue-extensions-unblock — Path A 上游接线

> **工期**: 1.5-2.5 天 | **状态**: 🔄 Proposed v1.0（2026-09-15）
> **前置基线**:
> - `2026-09-10-cpptlm-stage-1-4-2-1` + followups archived ✅
> - `2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor` archived ✅
> **下游解锁**: UE 仓 `ue-stage-1-4-2-1-extensions`（commit `42b08a6` 必修修订后 0/8 → 16 checkbox）
> **TDD 纪律**: 每 task 顺序 = Write test → Verify fail → Implement → Verify pass → Commit

---

## §0 前置条件

- [ ] **Verify**: `dgpu_board_v1.json` 当前结构（含 capabilities 字段是否存在）
- [ ] **Verify**: `dgpu_board_shell.cc` 当前所有 `dynamic_cast<PcieEndpointTLM*>` 调用点（预计 4 处）
- [ ] **Verify**: `pcie_endpoint_ip.cc` 构造函数当前 cap 安装逻辑
- [ ] **Verify**: 71/71 ctest baseline PASS（避免回归溯源）

---

## §1 任务 A-1：profile pcie_ep 切换 IP（0.25d）

### 任务 1.1：写失败测试
- [ ] **Write test**: `test/test_dgpu_pcie_config_standalone.cc::test_pcie_config_read_offset_0x40_returns_pm_cap`
  - 当前 dgpu_board_v1.json pcie_ep = PcieEndpointTLM，offset 0x40 是 MSI-X Cap (id=0x11)
  - 期望：val = 0x11（MSI-X id）；本 task 后期望 val = 0x01（PM Cap id）
- [ ] **Verify fail**: 当前返 0x11（MSI-X）→ 断言 RED（不是 PM Cap）

### 任务 1.2：Implement
- [ ] **Modify**: `configs/dgpu_board_v1.json`
  - `pcie_ep.type`: `"PcieEndpointTLM"` → `"PcieEndpointIP"`
  - 新增 `pcie_ep.capabilities`: PM Cap (0x01@0x48) + PCIe Cap (0x10@0x50)
  - 新增 `pcie_ep.extended_capabilities`: ACS Ext Cap (0x000D@0x100) + ReBAR Ext Cap (0x0020@0x140)
- [ ] **Modify**: `src/tlm/pcie/pcie_config_space_mvp.cc` 支持 `capabilities` JSON 数组

### 任务 1.3：Verify pass
- [ ] **Verify pass**: test_pcie_config_read_offset_0x40 现在期望返 PM Cap（不是 MSI-X）
  - 注意：MSI-X Cap 0x11 需要移到其他 offset（如 0x60），保持 MSI-X 功能
- [ ] **Commit**: `feat(cpptlm): dgpu profile pcie_ep 切 PcieEndpointIP (Path A-1)`

---

## §2 任务 A-2：board shell pass-through accessors（0.5d）

### 任务 2.1：写失败测试
- [ ] **Write test**: `test/test_dgpu_board_shell_full_abi.cc::test_pcie_config_read_pass_through_to_pcie_endpoint_ip`
  - 当前 board shell `dynamic_cast<PcieEndpointTLM*>` → 永远命中 legacy
  - 期望：本 task 后 `getInternalInstanceAs<PcieEndpointIP>` 命中 IP

### 任务 2.2：Implement
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc` 所有 `dynamic_cast<PcieEndpointTLM*>` (4 处)
  - 改为 `soc_->getInternalInstanceAs<PcieEndpointIP>("pcie_ep")`（使用 simmodule-refactor 已恢复的 API）
  - 或用 `dynamic_cast<PcieEndpointIP*>`（若 PcieEndpointIP 提供统一接口）
- [ ] **Verify**: 既有 71/71 ctest 不回归

### 任务 2.3：Verify pass
- [ ] **Verify pass**: test_pcie_config_read_pass_through PASS + 71/71 ctest PASS
- [ ] **Commit**: `feat(cpptlm): board shell pass-through accessors to PcieEndpointIP (Path A-2)`

---

## §3 任务 A-3：ABI mmio power-state check（0.5d）

### 任务 3.1：写失败测试
- [ ] **Write test**: `test/test_dgpu_board_shell_full_abi.cc::test_mmio_read_in_d3_returns_enoent`
  - 当前 mmio_read 路径走 mmio_regs_ backdoor，永不检查 power state
  - 期望：D3 状态下 mmio_read 返 -EIO（不是 0/数据）
- [ ] **Verify fail**: 当前 D3 返 0 → 断言 RED

### 任务 3.2：Implement
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc::mmio_read`
  ```cpp
  if (board->is_mmio_gated()) {
      return -EIO;  // INV-A gate
  }
  ```
- [ ] **Modify**: `src/abi/cpptlm_emulator.cc` 确保 `cpptlm_emulator_mmio_read` 调用 DGpuBoard::is_mmio_gated 检查

### 任务 3.3：Verify pass
- [ ] **Verify pass**: test_mmio_read_in_d3 PASS + 71/71 ctest 不回归
- [ ] **Commit**: `feat(cpptlm): ABI mmio power-state gate -EIO in D3 (Path A-3)`

---

## §4 任务 A-4：PCIe Cap (0x10) + LNKCTL→enable_aspm 接线（0.5d）

### 任务 4.1：写失败测试
- [ ] **Write test**: `test/test_pcie_endpoint_ip_cap_install_standalone.cc::test_pcie_cap_0x10_with_lnkctl_writes_enable_aspm`
  - 当前 PCIe Cap (0x10) 未安装
  - 期望：cap 装入 offset 0x50；LNKCTL write → phy enable_aspm 调用
- [ ] **Verify fail**: 当前 offset 0x50 读 0 → 断言 RED

### 任务 4.2：Implement
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc` 构造函数
  - 加 PCIe Cap (0x10) 安装 @ offset 0x50
  - 加 LNKCTL (0x0010 within Cap) + LNKSTA (0x0012 within Cap) register
  - 加 LNKCTL write intercept → 调 `phy_->enable_aspm(AspmLevel::L0s/L1)`
- [ ] **Modify**: `src/tlm/pcie/pcie_config_space_mvp.cc` 加 `add_capability(id, offset, next, control)` 扩展支持 LNKCTL cb

### 任务 4.3：Verify pass
- [ ] **Verify pass**: test_pcie_cap_0x10 PASS + 71/71 ctest 不回归
- [ ] **Commit**: `feat(cpptlm): PCIe Cap 0x10 + LNKCTL→enable_aspm 接线 (Path A-4)`

---

## §5 任务 A-5：ReBAR Ext Cap (0x0020) 安装（0.5d）

### 任务 5.1：写失败测试
- [ ] **Write test**: `test/test_pcie_endpoint_ip_cap_install_standalone.cc::test_rebar_extended_cap_0x0020_installed`
  - 当前 ReBAR Ext Cap (0x0020) 未安装
  - 期望：offset 0x140 cap header = 0x00200001（id=0x0020, version=1）

### 任务 5.2：Implement
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - 加 Extended Capability (0x0020) 安装 @ offset 0x140
  - 加 ReBAR Cap header + Control register
  - Control register：BAR index + BAR size encoding（默认 BAR0 size=256MB）
- [ ] **Modify**: `src/tlm/pcie/pcie_config_space_mvp.cc` 加 `add_extended_capability` API

### 任务 5.3：Verify pass
- [ ] **Verify pass**: test_rebar_extended_cap PASS + 71/71 ctest 不回归
- [ ] **Commit**: `feat(cpptlm): ReBAR Ext Cap 0x0020 安装 (Path A-5)`

---

## §6 双仓 entry sync

- [ ] **Modify**: `docs/soc_arch/architecture/18-pcie-endpoint-entry.md` v0.11（Path A 接线 ship + Wave 4 UE 端解锁）
- [ ] **Modify**: UE 仓 `docs/02_architecture/pcie-endpoint-entry.md` mirror 同 v0.11
- [ ] **Commit (CppTLM)**: `docs(cpptlm): 18-doc mirror v0.11 path-a unblock`
- [ ] **Commit (UE)**: `docs(pcie-ep): v0.11 path-a unblock mirror`

---

## §7 Oracle 复审（1 次轻量）

> **Oracle 复审位置（Metis M6 修订）**：实施 commit 后、docs mirror commit **前**进行。复审发现问题需追加 commit 而非 amend docs。

- [ ] **Oracle review**: 5 项接线 commit 后
- [ ] **checklist**：
  - `pcie_config_read(0x40)` = PM Cap id=0x01 ✅
  - D3 mmio_read 返 -EIO（非 0）✅
  - LNKCTL write → phy enable_aspm 调用可达 ✅
  - ReBAR Ext Cap id=0x0020 可读 ✅
  - 71/71 ctest 保持 + 1 新增 = 72/72 PASS ✅
- [ ] **发现问题 → 追加 commit**

---

## §8 总计

- **CppTLM commits**: 7（5 接线 + 2 entry sync）
- **UE commits**: 1（mirror）
- **工期**: 1.5-2.5 天
- **Oracle**: 1 次轻量
- **下游**: `ue-stage-1-4-2-1-extensions`（UE 仓 0/8 → 16 checkbox）启动实施解锁

---

## Wave 顺序关系

```
Wave 4 (CppTLM stage-1-4-2-1 + followups) ✅ archived
  └─→ Path A 本 change (当前)
        └─→ ue-stage-1-4-2-1-extensions (UE 仓, commit 42b08a6 必修修订后)
              └─→ 5.5.9 真机双轨验证启动前置
```