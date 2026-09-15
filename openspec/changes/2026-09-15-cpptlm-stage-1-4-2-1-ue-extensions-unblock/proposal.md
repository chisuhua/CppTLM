# Proposal: cpptlm-stage-1-4-2-1-ue-extensions-unblock — Path A 上游接线

> **状态**: 🔄 **Proposed v1.2** （2026-09-15，Oracle v1.1 复评 Quick 修订）
> **优先级**: P0（解锁 UE 仓 `ue-stage-1-4-2-1-extensions` 5/5 spec Scenario；为 Wave 5a + Wave 4 UE 集成测试的硬性前置）
> **工期**: 3.5-5 天（Oracle 评估 Medium；6 tasks TDD 5 步；含 A-2b MSI-X cap install 决策）
> **关联**:
> - 上游：`cpptlm-stage-1-4-2-1` + `cpptlm-stage-1-4-2-1-followups`（均已 archived）
> - `pcie-endpoint-ip-simmodule-refactor`（已 archived；恢复的 pass-through accessors 可被本 change 直接复用）
> - 解锁下游：`ue-stage-1-4-2-1-extensions`（UE 仓 0/8 → 16 checkbox，TDD 5 步已就绪）
> **Oracle 关键审查结论**：当前 ABI 路径命中冻结的 `PcieEndpointTLM`，与 `PcieEndpointIP` 实施脱节，5/5 spec Scenario 全部假阳性或不可观测

---

## Why

`ue-stage-1-4-2-1-extensions` 的 O7 spec 修订假设 `cpptlm_emulator_pcie_config_read/write` 能打到 `PcieEndpointIP` 实施层（PM Cap 0x01@0x40 + PMCSR 拦截 + power state machine + ACS Ext Cap 0x000D + ReBAR Ext Cap 0x0015 + ASPM L1）。

**实际链路断裂**：

```
ABI 路径： 实际命中：
cpptlm_emulator_pcie_config_read/write → dgpu_board_v1.json pcie_ep = PcieEndpointTLM (frozen legacy)
                                       → dgpu_board_shell.cc dynamic_cast<PcieEndpointTLM*>
                                       → PcieEndpointTLM::cfg_space_ 仅 MSI-X Cap (0x11@0x40)
                                       → 无 PM Cap / 无 PMCSR 拦截 / 无 ACS / 无 ReBAR

cpptlm_emulator_mmio_read              → mmio_regs_ backdoor map（不经过 endpoint tick()）
                                       → INV-A MMIO gating 永不可观测
                                       → D3 下返 0 而非 -EIO
```

**假阳性陷阱**：PMCSR 写 0x44 在 TLM 路径下被静默哑存（无 PMCSR 拦截回调）；切换 profile 后会真触发 IP 的 PMCSR 拦截 + power state machine。对 PcieEndpointTLM 路径，PMCSR 写 0x44 实际落在 MSI-X Cap 的 Message Address Low；dword 原样读回"看似 D3"，但无状态机，MMIO -EIO 永不发生。

**spec 硬错误**：`ue-stage-1-4-2-1-extensions/spec.md` 写 D3hot PowerState bits = 0b10，实际应为 0b11（0b10 是 D2）；Oracle + Metis 双审查已修订（commit `42b08a6`）。

---

## What Changes

### 6 项核心改动（TDD 5 步）

| # | 改动 | 文件 | 工作量 | 解除的 UE 阻塞 |
|---|------|------|:------:|---------------|
| **A-1** | `dgpu_board_v1.json` pcie_ep 切换 `PcieEndpointTLM` → `PcieEndpointIP` | `configs/dgpu_board_v1.json` | 0.25d | 5/5 Scenario 入口 |
| **A-2** | `dgpu_board_shell.cc` 改用 pass-through accessors（不再 `dynamic_cast<PcieEndpointTLM*>`）；7+1 处 cast（src 7 处 + `.hh:95` 内联 1 处）+ 5 个 IP accessor 新增（`bar_router()` / `has_config_space()` / `lookup_register()` / `msix()` / `sync_msix_cap_table_size()`）| `src/tlm/gpu/dgpu_board_shell.cc` | 0.5d | 5/5 Scenario 入口 |
| **A-2b** | MSI-X cap install 决策：IP 侧是否装 MSI-X Cap 使 host 经 cfg 空间可见（需决定 profile JSON 是否含 MSI-X cap install 指令） | `pcie_config_space_mvp.cc` | 0.5-1d | MSI-X 可见性 |
| **A-3** | ABI mmio backdoor 路径加 power-state check（D3 时返 -EIO，否则 INV-A MMIO gating 不可观测） | `dgpu_board_shell.cc` + `cpptlm_emulator.cc` | 0.5d | D0→D3hot Scenario mmio -EIO 断言 |
| **A-4** | 装 PCIe Cap (0x10) + LNKCTL→`PciePhyDigitalCtrl::enable_aspm` 接线（特化方法 `set_lnkctl_write_cb`） | `pcie_endpoint_ip.cc` + `pcie_config_space_mvp.cc` | 0.5d | ASPM L1 Scenario LNKCTL readback |
| **A-5** | ReBAR Ext Cap (0x0015, per PCI-SIG) 安装（header 0x00010015 + control 0x00080810） | `pcie_endpoint_ip.cc` + `ResizableBar` 集成 | 0.5d | ReBAR Scenario cap 可读 |
| **合计** | | | **3.5-5d** | |

### 修改文件清单

- `configs/dgpu_board_v1.json` — pcie_ep type 切换（A-1）
- `src/tlm/gpu/dgpu_board_shell.cc` — pass-through accessors + mmio power check（A-2 + A-3）
- `src/abi/cpptlm_emulator.cc` — mmio backdoor power-state 检查钩子（A-3）
- `src/tlm/pcie/pcie_endpoint_ip.cc` — PCIe Cap + LNKCTL 接线 + ReBAR Ext Cap（A-4 + A-5）
- `src/tlm/pcie/pcie_config_space_mvp.cc` — 扩展 capability 安装 API（A-4 + A-5）
- `test/test_dgpu_board_shell_full_abi.cc` — 扩展集成测试（A-2 + A-3 验证）
- `test/test_pcie_endpoint_ip_cap_install.cc` — 新增 cap 安装验证（A-4 + A-5）

### 新增测试 binary

- `test/test_pcie_endpoint_ip_cap_install_standalone` — 验证 PM Cap + PCIe Cap + ACS Ext Cap + ReBAR Ext Cap 全部装入 config space 且可读

---

## Capabilities

### ADDED Requirements

- **`cpptlm-stage-1-4-2-1-ue-extensions-unblock`**：Path A 上游接线，6 项核心改动（A-1/A-2/A-2b/A-3/A-4/A-5，详见 `specs/cpptlm-stage-1-4-2-1-ue-extensions-unblock/spec.md`）

---

## Impact

- **下游 UE 仓**：`ue-stage-1-4-2-1-extensions`（0/8 → 16 checkbox，含 §0 Gate + TDD 5 步）archive 阻塞解除
- **Wave 5a 启动**：本 change archive 后，UE 仓可立即启动 5/5 spec Scenario 验证
- **71/71 ctest 保持**：不引入既有测试回归（保留非 doorbell 写 mmio_regs_ 旁路）
- **24 ABI 冻结契约**：零 ABI 变更（所有改动在 C++ 实现层）

**Layer-3 隐性承诺**：本 change archive 后，PcieEndpointTLM 在生产 profile `dgpu_board_v1.json` 上永久退役，所有 SOC 内嵌 PCIe EP 路径均经 PcieEndpointIP。PcieEndpointTLM 仍保留注册以兼容既有测试，但仅作为 maintenance-only 路径。

## Alternatives Considered

### A1：扩 ABI（加 link_state_query 等新 ABI）
- 优点：直接观测 LTSSM
- 缺点：ADR-088 修订流程重；污染冻结契约
- 结论：拒绝。零 ABI 变更是原则，LTSSM 覆盖属 CppTLM 侧（test_aspm.cc）

### A2：在 `PcieEndpointTLM` 加 PM Cap（不切 profile）
- 优点：profile 切换风险小
- 缺点：PcieEndpointTLM 无 PMCSR 拦截回调、无 power state machine、无 mmio gating flag；即使装 cap 也不能让 INV-A 可观测
- 结论：拒绝。必须切 profile + 接线 IP 层

### A3：直接把 UE 集成测试降级为 JSON caps 存在性 + OOB 负测试
- 优点：1 行 JSON 改动
- 缺点：5/5 Scenario 中 4 个被降级；Wave 4 测试价值全失
- 结论：拒绝。Wave 5a 集成测试是 dGPU E2E 主线必经阶段

---

## Oracle + Metis 审查依据

- **Oracle session**：`ses_f5cdc6e86ffe7b7W0SaD97dYmi`（2026-09-15，P0 阻塞发现）
- **Metis session**：`ses_f5cd7cd8affeEfZV00Nw16kzte`（2026-09-15，P0 流程 + P1 一致性）
- **用户决策**：2026-09-15 选项 A（路径 A：上游全面接线）+ 选项 A（立即双仓 commit 11 项必修修订）
- **commit `42b08a6`**：UE 仓 ue-stage-1-4-2-1-extensions 11 项必修修订（spec 7 处 + design 重写 + tasks TDD 5 步 + proposal 路径 A 依赖）

## Wave 顺序关系

```
Wave 4 (CppTLM stage-1-4-2-1 + followups) ✅ archived
  └─→ Path A 本 change 🚧 当前
        └─→ ue-stage-1-4-2-1-extensions (UE 仓, 0/8 → 16 checkbox)
              └─→ 5.5.9 真机双轨验证启动前置
```

## refs

- 上游：`2026-09-10-cpptlm-stage-1-4-2-1`（archived）+ `2026-09-10-cpptlm-stage-1-4-2-1-followups`（archived）+ `2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor`（archived）
- 解锁：`ue-stage-1-4-2-1-extensions`（UE 仓，commit `42b08a6` 必修修订后）
- **⚠️ UE 仓 commit `42b08a6` push 状态待 human 确认**；本仓 commit 不依赖 UE 仓 push 完成，但 entry doc mirror 必须 human 在 UE session 中提交
- 父 change：`2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`（5.5.8 阶段 3 ship 后排队）
- 下游：5.5.9-cpptlm-real-hw-verify