# cpptlm-pcie-power-management: PCIe Power Management (ASPM + D0/D1/D2/D3)

> **状态**: 📋 Draft (待 X1 归档后落盘到 `openspec/changes/`)
> **估时**: ~5d 单人 · **起点**: X1 (`cpptlm-pcie-host-test-flr-rebar`) completion tag
> **关联**:
> - 前置: `cpptlm-pcie-host-test-flr-rebar` (X1) — 依赖 X1 完成的 host-side FLR/state trigger 基础设施
> - 后续: `cpptlm-pcie-aer-ats-pri` (X3) — 依赖 X2 完成的 power state 状态机
> **平行**: `cpptlm-dgpu-soc-internal-noc-e1` (Draft, 暂停), `cpptlm-p2-integration-unblock` (Proposed, 异步)
> **冻结面**: 23 ABI 签名零 diff (per ADR-DGPU-08) + 4 frozen headers 零 diff

---

## Why

X1 (FLR + Resizable BAR + HostPcieTestStub) 完成后,**PCIe 电源管理 (PM) 仍零实施**。`cpptlm_emulator.h` 已有 `cpptlm_power_cb_t` hook(typedef 已就位)但 0 调用方,无 D-state 状态机,无 ASPM 链路状态管理。真实 dGPU 设备必须支持:

- **D0**: 全功率活跃态
- **D1 / D2**: 中间低功耗态(轻睡眠/深睡眠)
- **D3hot**: 软件控制低功耗(可软件唤醒)
- **D3cold**: 完全断电(需硬件事件唤醒)
- **ASPM L0s**: 短空闲自动断链(亚毫秒级)
- **ASPM L1**: 长空闲深度断链(毫秒级)

**主因(按 ROI 排序)**:

1. **PM 协议零实施**: `cpptlm_power_cb_t` typedef 是空 hook,任何 power transition 都不触发
2. **移动 dGPU 场景不可验证**: 笔记本 / 嵌入式 dGPU 场景需 PM 验证
3. **host driver 重置 / 错误恢复路径不完整**: AER (X3) + FLR (X1) + PM 缺失 → driver 完整电源生命周期不可验证

**预期收益**:

- 完整 4-state D-state 状态机 + 2-state ASPM 链路管理
- `cpptlm_power_cb_t` 真实触发(已存在 typedef)
- `[pcie][pm]` 标签 + 5+ cases
- 与 X1 FLR / X3 AER 协同: driver 完整电源生命周期

---

## What Changes

### 新增文件 (~3 文件, ~400 行)

| 文件 | 行数 | 关键 API |
|------|:----:|----------|
| `include/tlm/pcie/pcie_power_state_machine.hh` | ~200 | `PciePowerStateMachine` (D0/D1/D2/D3hot + ASPM L0s/L1 状态转换 + 转换规则) |
| `src/tlm/pcie/pcie_power_state_machine.cc` | ~150 | 状态转换实现 + 回调触发 |
| `include/abi/cpptlm_emulator_pm.h` | ~80 | `cpptlm_emulator_set_power_state()` + `cpptlm_emulator_get_power_state()` (新 ABI 函数族) |
| `test/test_pcie_pm_e2e.cc` | ~150 | PM 端到端: 4-state D-state + 2-state ASPM |

### 修改文件 (~4 文件, ~300 行)

| 文件 | 改动 | 说明 |
|------|------|------|
| `include/tlm/gpu/dgpu_board_shell.hh` | + `cpptlm_emulator_set_power_state()` 入口 | 暴露 23 ABI 风格 API |
| `src/tlm/gpu/dgpu_board_shell.cc` | + PM state machine 实例化 + 触发 cb | 真实调用 `cpptlm_power_cb_t` |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | + `current_power_state` 字段 + `transition_power_state()` API | 状态机入口 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | + ASPM L0s/L1 链路管理 | link state 切换 |

### Frozen surface (0 修改, per ADR-DGPU-08)

- 23 ABI 签名 0 修改,新追加 `cpptlm_emulator_pm.h` (per X1 G8 precedent)
- 4 frozen headers 0 diff

---

## Non-Goals

- ❌ **FLR / Resizable BAR** — 属 X1
- ❌ **AER / ATS/PRI** — 属 X3
- ❌ **Multi-VC NoC (E.2) / GSP-RM firmware** — 推 ArchForge
- ❌ **跨仓 UsrLinuxEmu driver** — 不在 scope

---

## Capabilities

### New Capabilities

- **`pcie-power-management`**: 4-state D-state (D0/D1/D2/D3hot) + 2-state ASPM (L0s/L1) 状态机 + `cpptlm_power_cb_t` 真实触发
- **`pcie-power-callback`**: `cpptlm_power_cb_t` ABI 入口

### Modified Capabilities

- `pcie-host-test-infrastructure` (X1): + `trigger_power_state_transition()` API 扩展
- `pcie-ip-integration`: + `current_power_state` 字段 + transition handler

---

## Acceptance Gate (5 项)

- [ ] **G1**: `openspec validate cpptlm-pcie-power-management --strict` PASS
- [ ] **G2**: D-state transition ≥4 cases (D0→D1, D1→D0, D0→D3hot, D3hot→D0)
- [ ] **G3**: ASPM L0s/L1 transition ≥2 cases (链路空闲 → L0s → L1)
- [ ] **G4**: `cpptlm_power_cb_t` 真实触发(测试桩订阅,验证 cb 调用次数)
- [ ] **G5**: 12 套件 0 回归 (与 X1 后基线对齐)

---

## Risks

| # | 风险 | 等级 | 缓解 |
|---|------|:----:|------|
| 1 | D-state transition 与 FLR (X1) 状态机互锁 | 🟡 中 | FLR 仅清零 PCIe state, 不动 PM 状态; PM transition 触发 cb, FLR 后保持 PM 状态 |
| 2 | ASPM L1 链路断链 + 23 ABI in-flight 死锁 | 🟡 中 | L1 transition 仅在 PCIe link idle 时触发, 23 ABI 入口需等待 wake-up (TBD 协议) |
| 3 | `cpptlm_power_cb_t` 既有 typedef 签名 vs 新触发语义 | 🟢 低 | typedef 已存在 (per `cpptlm_emulator.h`), 复用不改 |

---

## Timeline (5d)

| Commit | 内容 | 估时 |
|:------:|------|:----:|
| 1 | `PciePowerStateMachine` 实现 + 4-state D-state 转换 | 2d |
| 2 | ASPM L0s/L1 链路管理 + `cpptlm_power_cb_t` 触发 | 1.5d |
| 3 | 23 ABI 入口 + 单元 + E2E 测试 | 1d |
| 4 | 文档 + openspec archive | 0.5d |

---

## 关联资源

- 起点 tag: X1 completion (`cpptlm-pcie-host-test-flr-rebar-completion`)
- 已有 hook: `cpptlm_power_cb_t` in `include/abi/cpptlm_emulator.h`
- 后续 change: X3 (cpptlm-pcie-aer-ats-pri)
- 平行 change: `cpptlm-dgpu-soc-internal-noc-e1` (Draft, 暂停), `cpptlm-p2-integration-unblock` (Proposed, 异步)
