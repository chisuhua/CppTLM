# Design: cpptlm-stage-1-4-2-1-ue-extensions-unblock — Path A 上游接线

> **状态**: 🔄 Proposed v1.0（2026-09-15）
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
                             │                 │ + ReBAR Ext 0x20│
                             │                 │ + power state   │
                             │                 │   machine +     │
                             │                 │   mmio_gated_   │
                             │                 └─────────────────┘
```

## 5 项接线任务详解

### A-1: profile 切换（0.25d）

**修改**：`configs/dgpu_board_v1.json`

```json
{
  "pcie_ep": {
    "type": "PcieEndpointIP",     // was: "PcieEndpointTLM"
    "config_size": 4096,
    "num_msix_vectors": 4,
    "bar_sizes": ["256MB", "1MB", "256MB", "256MB"],
    "capabilities": [              // 新增（路径 A 装入 cap）
      { "id": "0x0001", "offset": "0x40", "next": "0x50" },  // PM Cap
      { "id": "0x0010", "offset": "0x50", "next": "0x00" }   // PCIe Cap (LNKCTL/LNKSTA)
    ],
    "extended_capabilities": [     // 新增
      { "id": "0x000D", "offset": "0x100" },  // ACS Ext Cap
      { "id": "0x0020", "offset": "0x140" }   // ReBAR Ext Cap
    ]
  }
}
```

### A-2: board shell pass-through accessors（0.5d）

**修改**：`src/tlm/gpu/dgpu_board_shell.cc`

```cpp
// 修复前：dynamic_cast<PcieEndpointTLM*>
- auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
- if (!ep) return -ENOSYS;

// 修复后：使用 8a583803 恢复的 pass-through accessors
+ auto* ep = soc_->getInternalInstanceAs<PcieEndpointIP>("pcie_ep");  // SimModule registry
+ if (!ep) return -ENOSYS;
+ // 调用 ep->pcie_config_read/write/mmio_read/write（统一接口）
+ return ep->pcie_config_read(offset, width, *val);  // 委托给 IP
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
    // ... 既有 PM Cap 装入 ...
    
    // 新增：PCIe Cap (0x10) + LNKCTL write intercept
    cfg_space_->add_capability(0x10, 0x50, 0x00, 0x0002);  // PCIe Cap id, version 2
    cfg_space_->register_lnkctl_write_cb(
        [this](uint16_t value) { 
            // 调 phy enable_aspm
            if (value & 0x0001) phy_->enable_aspm(AspmLevel::L1);
            if (value & 0x0002) phy_->enable_aspm(AspmLevel::L0s);
        }
    );
    cfg_space_->add_register(LNKCTL_OFFSET, 2, LNKCTL_DEFAULT_VALUE);  // 0x0010
    cfg_space_->add_register(LNKSTA_OFFSET, 2, 0x0000);  // 0x0012 readback
}
```

### A-5: ReBAR Ext Cap (0x0020) 安装（0.5d）

**修改**：`src/tlm/pcie/pcie_endpoint_ip.cc`

```cpp
// 扩展 ResizableBar 与 config space 集成（followup §3 已做 bar_store_ 验证）
cfg_space_->add_extended_capability(0x0020, 0x140);  // ReBAR Ext Cap header
cfg_space_->add_extended_register(0x140, 4, 0x00200001);  // Cap header (id=0x0020, version=1)
cfg_space_->add_extended_register(0x144, 4, 0x0F000000);  // Control: BAR0 size=256MB
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
| profile 切换破坏既有 profile 行为 | 高 | dgpu_board_v1.json 加 capabilities 字段而非替换；切 type 保留其他配置 |
| board shell dynamic_cast 改写后既有调用方未跟进 | 高 | 全仓 grep dynamic_cast<PcieEndpointTLM*> 替换为 SimModule accessors；扩展既有测试覆盖 |
| mmio -EIO 引入 71 ctest 回归 | 中 | 仅在 board->is_mmio_gated() 时返 -EIO；既有测试默认非 D3 状态 |
| PCIe Cap 装入位置冲突（既有 cap @0x40 = MSI-X） | 中 | MSI-X Cap 0x11 @0x40 保留；PM Cap 0x01 移到 0x48；PCIe Cap 0x10 @0x50；链 next 指针更新 |
| ReBAR Ext Cap 安装与 ResizableBar 集成冲突 | 中 | followup §3 已完成 bar_store_ 验证，本 change 仅加 ext cap header |

---

## 工期估算

| 阶段 | 工作量 |
|------|:------:|
| A-1 profile 切换 | 0.25d |
| A-2 pass-through accessors | 0.5d |
| A-3 mmio power check | 0.5d |
| A-4 PCIe Cap + LNKCTL | 0.5d |
| A-5 ReBAR Ext Cap | 0.5d |
| 集成测试 + Oracle 复审 | 0.5d |
| **合计** | **2.5d** |