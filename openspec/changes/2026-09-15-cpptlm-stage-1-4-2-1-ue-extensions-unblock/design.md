# Design: cpptlm-stage-1-4-2-1-ue-extensions-unblock — Path A 上游接线

> **状态**: 🔄 Proposed v1.2（2026-09-15，Oracle v1.1 复评 Quick 修订）
> **核心约束**: 24 ABI 冻结契约不动；71/71 ctest 保持；零 ABI 变更

## 接线架构（修复前后）

```
                  修复前（断链）                            修复后（Path A 接链）

   ┌─────────────────┐                          ┌─────────────────┐
   │ UE ABI          │                          │ UE ABI          │
   │ pcie_config_*   │                          │ pcie_config_*   │
   │ mmio_*          │                          │ mmio_* (+power  │
   │                 │                          │   check)        │
   └────────┬────────┘                          └────────┬────────┘
            │                                            │
   ┌────────▼────────┐    断！                  ┌────────▼────────┐
   │ ABI 24 frozen   │                          │ ABI 24 frozen   │
   │ (不变)          │                          │ (不变)          │
   └────────┬────────┘                          └────────┬────────┘
            │                                            │
   ┌────────▼────────┐                          ┌────────▼────────┐
   │ DGpuBoard       │                          │ DGpuBoard       │
   │ dynamic_cast    │ ──X──→                   │ pass-through    │
   │ <PcieEndpointTLM│        │                 │ accessors (8a58)│
   │ >               │        │                 └────────┬────────┘
   └────────┬────────┘        │                          │
            │                 │                 ┌────────▼────────┐
   ┌────────▼────────┐        │                 │ PcieEndpointIP  │
   │ PcieEndpointTLM │        │                 │ (pcie_ep type)  │
   │ (frozen legacy) │        │                 │ + PM Cap 0x01   │
   │ 只有 MSI-X Cap  │        │                 │ + PMCSR cb      │
   └─────────────────┘        │                 │ + PCIe Cap 0x10 │
                             │                 │ + ACS Ext 0x0D  │
                             │                 │ + ReBAR Ext 0x0015│
                             │                 │ + power state   │
                             │                 │   machine +     │
                             │                 │   mmio_gated_   │
                             │                 └─────────────────┘
```

## 6 项接线任务详解（A-1, A-2, A-2b, A-3, A-4, A-5）

### A-1: profile 切换（0.25d）

**修改**：`configs/dgpu_board_v1.json`

```json
{
  "pcie_ep": {
    "type": "PcieEndpointIP",     // was: "PcieEndpointTLM"
    "config_size": 4096,
    "num_msix_vectors": 4,
    "bar_sizes": [268435456, 1048576, 268435456, 268435456]
  }
}
```

> **关键约束**：`bar_sizes` 必须是**数值数组**（单位：字节），不是字符串。`dgpu_board_shell.cc:65` 调 `.get<uint64_t>()`，字符串会抛 nlohmann::json `type_error` → `last_exception_` 永久 rethrow → ABI 全灭（且 `cpptlm_emulator_create` 忽略返回值，故障完全静默）。
>
> **已知限制**：doorbell 路径需后续 worktree 补 IP 侧 `bar_router()` 接入；`bar0_registers`/`bar_sizes` 字段保留（已知 IP 不消费 → `warn_unconsumed` warning）。

> **cap 安装载体**：本 change **不**走 JSON-driven `capabilities`/`extended_capabilities` 数组（与 `init_all()` wipe 不变量冲突）。所有 cap（PM @0x40 / PCIe @0x50 / ACS @0x100 / ReBAR @0x140）统一在 `PcieEndpointIP::install_capabilities()` 挂钩内注册，**且**在 ctor / `init()` / `do_reset()` 三处调用，确保每次复位后 cap 重装。

### A-2: board shell pass-through accessors（0.5d）

**修改**：`src/tlm/gpu/dgpu_board_shell.cc`

> **范围**：7+1 处 `dynamic_cast`（src 7 处 + `.hh:95` 内联 1 处）+ 5 个 IP accessor 新增（`bar_router()` / `has_config_space()` / `lookup_register(offset, &info)` / `msix()` / `sync_msix_cap_table_size()`）

```cpp
// 修复前：dynamic_cast<PcieEndpointTLM*>
- auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
- if (!ep) return -ENOSYS;

// 修复后：手动 dynamic_cast（getInternalInstanceAs<T> 不存在）+ IP 侧新 accessor
+ auto* ep = dynamic_cast<PcieEndpointIP*>(soc_->getInternalInstance("pcie_ep"));
+ if (!ep) return -ENOSYS;
+ // 调用 IP 新增 accessor（A-2b 阶段实现）：config_of(0).read/write()
+ return ep->config_of(0).read(offset, width, val);  // 委托给 IP
```

### A-3: ABI mmio power-state check（0.5d）

**修改**：`src/abi/cpptlm_emulator.cc` 或 `dgpu_board_shell.cc::mmio_read`

```cpp
// 在 mmio_read 路径加 power-state check
int cpptlm_emulator_mmio_read(cpptlm_emulator_t* emu, uint8_t bar, uint64_t offset,
                              void* buf, uint32_t len) {
    auto* board = emu->board;
    
    // 新增：INV-A power-state gating
    if (board->is_mmio_gated()) {
        return -EIO;  // D3 时所有 BAR 访问均返 -EIO
    }
    
    // 既有 mmio_regs_ 回退路径（保留兼容非 doorbell 写）
    return board->mmio_read(bar, offset, buf, len);
}
```

**约束**：
- 仅在 INV-A mmio_gated_ 状态下返 -EIO（其他状态保持既有行为）
- doorbell 写仍走 mmio_regs_ 同步存储（修复 #5 兼容）
- 71/71 ctest 不回归（非 D3 测试场景不受影响）

### A-4: PCIe Cap (0x10) + LNKCTL→enable_aspm 接线（0.5d）

**修改**：`src/tlm/pcie/pcie_endpoint_ip.cc` + `pcie_config_space_mvp.cc`

```cpp
// 在 PcieEndpointIP 构造函数添加 PCIe Cap
PcieEndpointIP::PcieEndpointIP(...) {
    // ... 既有 PM Cap 装入 @0x40 ...
    // install_capabilities() 挂在 ctor / init() / do_reset 三处调用

    // 新增：PCIe Cap (0x10) + LNKCTL write intercept
    cfg_space_->add_capability(0x10, 0x50, 0x00, 0x0002);  // PCIe Cap id, version 2
    cfg_space_->set_lnkctl_write_cb(  // 特化方法（匹配 PMCSR 模式）
        [this](uint16_t value) {
            // 调 phy enable_aspm（per PCI-SIG: bit0=L0s, bit1=L1）
            if (value & 0x0001) phy_->enable_aspm(AspmLevel::L0s);
            if (value & 0x0002) phy_->enable_aspm(AspmLevel::L1);
        }
    );
    cfg_space_->add_register(0x60, 2, 0x0010);  // LNKCTL (offset 0x60 within cap @0x50)
    cfg_space_->add_register(0x62, 2, 0x0000);  // LNKSTA (offset 0x62 within cap @0x50)
}
```

### A-5: ReBAR Ext Cap (0x0015) 安装（0.5d）

**修改**：`src/tlm/pcie/pcie_endpoint_ip.cc`

```cpp
// 扩展 ResizableBar 与 config space 集成（followup §3 已做 bar_store_ 验证）
cfg_space_->add_extended_capability(0x0015, 0x140);  // ReBAR Ext Cap header (id=0x0015, per PCI-SIG)
cfg_space_->add_extended_register(0x140, 4, 0x00010015);  // Cap header (id=0x0015, version=1)
cfg_space_->add_extended_register(0x148, 4, 0x00080810);  // Control: BAR0 size=256MB, Number of Resizable BARs=1 (bits[7:4]=1) (bits[12:8]==8)
```
// 注：reprogram_size() 仍走 C++ API（INV-G），不暴露 ABI
```

---

## 测试策略

### 新增 binary

| Binary | 验证目标 | 命令 |
|--------|---------|------|
| `test_pcie_endpoint_ip_cap_install_standalone` | PM Cap + PCIe Cap + ACS + ReBAR 全部装入 | `ctest -R test_pcie_endpoint_ip_cap_install` |

### 扩展既有 binary

| Binary | 扩展内容 | 新增验证 |
|--------|---------|---------|
| `test_dgpu_board_shell_full_abi_standalone` | mmio_read in D3 → -EIO | 71→72 PASS |
| `test_dgpu_pcie_config_standalone` | PM Cap id=0x01 在 offset 0x40 | 既有测试扩展 |

### 71/71 ctest 保持 + 新增测试

- 既有 71 个 ctest 全部保持 PASS
- 新增 1 个 cap install 测试 binary（72 total）

---

## 风险与缓解

| 风险 | 严重度 | 缓解 |
|------|--------|------|
| R1: profile 切换破坏既有 profile 行为 | 高 | dgpu_board_v1.json 加 capabilities 字段而非替换；切 type 保留其他配置 |
| R2: board shell dynamic_cast 改写后既有调用方未跟进 | 高 | 全仓 grep dynamic_cast<PcieEndpointTLM*> 替换为 SimModule accessors；扩展既有测试覆盖 |
| R3: mmio -EIO 引入 71 ctest 回归 | 中 | 仅在 board->is_mmio_gated() 时返 -EIO；既有测试默认非 D3 状态 |
| R4: ReBAR Ext Cap 安装与 ResizableBar 集成冲突 | 中 | followup §3 已完成 bar_store_ 验证，本 change 仅加 ext cap header |
| R5: `is_mmio_gated()` 归属 DGpuBoard 层（A-3 新增 API） | 低 | `DGpuBoard::is_mmio_gated()` **新增**后委托 `ep->mmio_gated()`；非已有实现 |
| R6: MSI-X cap install 决策悬而未决（A-2b） | 中 | A-2b 独立 task；IP 侧可选装/不装 MSI-X Cap |
| R7: doorbell 路径需后续 worktree 补 IP 侧 bar_router 接入 | 低 | design.md §A-1 已注"已知限制"；不影响本 change scope |
| R8: MSI-X Cap 移除导致既有 MSI-X 功能失效 | 中 | A-2b MSI-X cap install 决策需明确 IP 侧是否装 MSI-X Cap 使 host cfg 可见 |
| R9: pcie_ep.* 4 条 connection 在新 profile 下处置不当导致 SoC instantiate 静默失败 | 高 | §0 Verify 必须包含 connection 处置方案；建议删除或改写为 composite 内部端口 |
| R10: TLM 退役不可逆 | 低 | Layer-3 隐性承诺；PcieEndpointTLM 仍注册维护测试路径，但生产 profile 永久切换 |

---

## 工期估算

| 阶段 | 工作量 |
|------|:------:|
| A-1 profile 切换 | 0.25d |
| A-2 pass-through accessors（7+1 cast + 5 accessor） | 0.5d |
| A-2b MSI-X cap install 决策 | 0.5-1d |
| A-3 mmio power check | 0.5d |
| A-4 PCIe Cap + LNKCTL + set_lnkctl_write_cb | 0.5d |
| A-5 ReBAR Ext Cap (0x0015) | 0.5d |
| 集成测试 + Oracle 复审 | 0.5d |
| **合计** | **3.5-5d** |