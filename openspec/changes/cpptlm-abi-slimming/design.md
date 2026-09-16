# cpptlm-abi-slimming: Design

> **配套**: [`proposal.md`](../proposal.md) · [`specs/cpptlm-emulator-abi/spec.md`](../specs/cpptlm-emulator-abi/spec.md) · [`tasks.md`](../tasks.md)
> **父 change**: `2026-09-16-cpptlm-pcie-tlp-wire-datapath` (Phase 9+ 主线, 已 archive)
> **拆分原因**: T-P9-0 (ABI 精简 22→18) Hub ack 超时 (10 工作日窗口), 切出为独立 follow-up

## 1. 架构概览

### 1.1 当前 ABI 表面 (per HSK-11)

```
Host Driver (VFIO/IOMMUFD/amdgpu/nouveau)
  │  extern "C" ABI (22 C 函数)
  ▼
include/abi/cpptlm_emulator.h
  │  22 forward declarations + 4 callback typedefs + cpptlm_emulator_t
  ▼
src/abi/cpptlm_emulator.cc
  │  22 函数实现 (4 backdoor + 18 驱动核心)
  ▼
DGpuBoard::backdoor_* (内部 C++ API, 驱动不可见)
```

### 1.2 目标 ABI 表面 (本 change archive 后)

```
Host Driver (VFIO/IOMMUFD/amdgpu/nouveau)
  │  extern "C" ABI (18 C 函数) ← 删除 4 个 backdoor
  ▼
include/abi/cpptlm_emulator.h
  │  18 forward declarations + 4 callback typedefs (不动) + cpptlm_emulator_t (不动)
  ▼
src/abi/cpptlm_emulator.cc
  │  18 函数实现 (删除 4 backdoor)
  ▼
DGpuBoard::backdoor_* (内部 C++ API, 不动, 仿真器自检用)
```

### 1.3 数据流 (backdoor 路径)

**Before** (22 ABI):
```
仿真器自检 → cpptlm_emulator_backdoor_read() [ABI] → DGpuBoard::backdoor_read() [C++]
驱动测试 → cpptlm_emulator_backdoor_read() [ABI] → DGpuBoard::backdoor_read() [C++]  ← 路径暴露给驱动
```

**After** (18 ABI):
```
仿真器自检 → DGpuBoard::backdoor_read() [C++ 内部, 不变]  ← 直接调
驱动测试 → profile.pcie_path = "mock" → PcieMockIP::mmio_read() [T-P9-3]  ← 替代 backdoor
```

## 2. 组件设计

### 2.1 ABI 表面变化 (cpptlm_emulator.h)

```diff
+ /** 保留 18 个驱动核心函数 (略, per父 spec.md §6 G7) */
+
+ // cpptlm_emulator_t 结构体 (不动)
+ typedef struct cpptlm_emulator cpptlm_emulator_t;
+
+ // 4 callback typedef (不动)
+ typedef void (*cpptlm_intr_deliver_cb_t)(...);
+ typedef void (*cpptlm_error_cb_t)(...);
+ typedef void (*cpptlm_reset_complete_cb_t)(...);
+ typedef void (*cpptlm_power_cb_t)(...);
+
- // 删除 4 个 backdoor/lookup 函数
- int cpptlm_emulator_backdoor_read(...);
- int cpptlm_emulator_backdoor_write(...);
- int cpptlm_emulator_register_backdoor_cb(...);
- int cpptlm_emulator_lookup_register(...);
```

### 2.2 ABI 实现变化 (cpptlm_emulator.cc)

```diff
+ // 保留 18 个函数实现
+ int cpptlm_emulator_mmio_write(...) { ... }
+ // ... (其他 17 个)

- // 删除 4 个函数实现
- int cpptlm_emulator_backdoor_read(...) { /* 删除整个函数 */ }
- int cpptlm_emulator_backdoor_write(...) { /* 删除整个函数 */ }
- int cpptlm_emulator_register_backdoor_cb(...) { /* 删除整个函数 */ }
- int cpptlm_emulator_lookup_register(...) { /* 删除整个函数 */ }

+ // 更新 ABI 版本号注释
- // CPPTLM_EMULATOR_VERSION = "22 functions + 4 callback typedefs"
+ // CPPTLM_EMULATOR_VERSION = "18 functions + 4 callback typedefs"
```

### 2.3 测试文件迁移 (call-site 改为三维 key)

```diff
// test/test_pcie_power_state_cfg_access.cc
- f.ep.bar_store_value(key)
+ f.ep.bar_store_value(0, 0, key)  // (bdf=0, bar=0, addr=key)

// test/test_power_state_transition.cc
- f.ep.bar_store_value(key)
+ f.ep.bar_store_value(0, 0, key)  // (bdf=0, bar=0, addr=key)
```

**依据**: T-P10-2 (bar_store_ 三维 key 修复) 已将 helper 签名从 `(key)` 改为 `(bdf, bar, addr)`, `bar_store_value` 调用方必须同步更新。

### 2.4 内部 C++ API 保持不变 (per HSK-11 §1 + ADR-SOC-18 决策 2)

```cpp
// include/tlm/gpu/dgpu_board_shell.hh (不变)
class DGpuBoard {
public:
    // backdoor API (驱动不可见, 仿真器自检用)
    int backdoor_read(uint8_t bar, uint64_t offset, void* buf, std::size_t len);
    int backdoor_write(uint8_t bar, uint64_t offset, const void* buf, std::size_t len);
    void backdoor_set_callback(...);
    int lookup_register(uint32_t offset, ...);
    // ...
};
```

## 3. 接口契约

### 3.1 ABI 表面 (18 驱动核心函数签名零修改)

| # | 函数 | 签名 (不变) |
|---|------|------------|
| 1-6 | 设备管理 | `get_version` / `get_device_count` / `get_device_info` / `create` / `create_by_id` / `destroy` |
| 7-10 | MMIO + Config | `mmio_write` / `mmio_read` / `pcie_config_write` / `pcie_config_read` |
| 11-14 | MSI-X + Callback | `msix_init` / `msix_update_pending` / `msix_clear_pending` / `register_callbacks` / `register_dma_translate_cb` |
| 15-18 | Handle | `open` / `close` / `get_adapter_info` |

### 3.2 4 callback typedef (不动)

```c
typedef void (*cpptlm_intr_deliver_cb_t)(void* user_ctx, uint32_t vector, uint32_t trans_id);
typedef void (*cpptlm_error_cb_t)(void* user_ctx, uint32_t err_code, const char* err_msg);
typedef void (*cpptlm_reset_complete_cb_t)(void* user_ctx, uint32_t stream_id);
typedef void (*cpptlm_power_cb_t)(void* user_ctx, uint32_t state);
```

### 3.3 cpptlm_emulator_t opaque 结构体 (不动)

```c
typedef struct cpptlm_emulator cpptlm_emulator_t;
```

### 3.4 删除的 4 函数 (per spec.md §REMOVED Requirements)

| # | 函数 | 行号 (pre-slimming) | 迁移路径 |
|---|------|---------------------|---------|
| 1 | `cpptlm_emulator_backdoor_read` | `cpptlm_emulator.h:88` | `DGpuBoard::backdoor_read` 内部 C++ API |
| 2 | `cpptlm_emulator_backdoor_write` | `cpptlm_emulator.h:91` | `DGpuBoard::backdoor_write` 内部 C++ API |
| 3 | `cpptlm_emulator_register_backdoor_cb` | `cpptlm_emulator.h:108` | `DGpuBoard::backdoor_set_callback` 内部 C++ API |
| 4 | `cpptlm_emulator_lookup_register` | `cpptlm_emulator.h:100` | `DGpuBoard::lookup_register` 内部 C++ API |

## 4. 数据流 (call-site 迁移)

### 4.1 迁移前 (既有)

```
test_pcie_power_state_cfg_access.cc:155  f.ep.bar_store_value(key)
test_pcie_power_state_cfg_access.cc:160  f.ep.bar_store_value(key)
test_power_state_transition.cc:126     f.ep.bar_store_value(key)
```

### 4.2 迁移后

```
test_pcie_power_state_cfg_access.cc:155  f.ep.bar_store_value(0, 0, key)
test_pcie_power_state_cfg_access.cc:160  f.ep.bar_store_value(0, 0, key)
test_power_state_transition.cc:126     f.ep.bar_store_value(0, 0, key)
```

(bdf=0 PF 默认, bar=0 BAR0, addr=key)

## 5. 关键依赖

| 依赖 | 来源 | 版本约束 |
|------|------|----------|
| `include/abi/cpptlm_emulator.h` | 父 change (Phase 9+ 既有) | 22 → 18 函数 |
| `src/abi/cpptlm_emulator.cc` | 父 change (Phase 9+ 既有) | 删除 4 实现 |
| `include/tlm/gpu/dgpu_board_shell.hh` | T-P9-3 (Phase 9+) | 不变, 仅验证存在 |
| `DGpuBoard::bar_store_value(bdf, bar, addr)` | T-P10-2 (Phase 9+) | 3 参数签名 |
| `PcieMockIP::mmio_read/write` | T-P9-3 (Phase 9+) | 替代 backdoor 驱动测试路径 |

## 6. 跨仓协调

详见 `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md` §4-5:

- Hub (UsrLinuxEmu) ADR-088 §D5 Status Update 提交后 10 工作日无响应
- 回退策略 4 选 1 (ack/部分 ack/拒绝/无响应)
- 当前状态: **无响应**, 假定 Hub 侧已自行移除 (driver 不可见), 继续推进

## 7. 风险与缓解

详见 `proposal.md` "风险与缓解" 段。

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: ✅ Draft — cpptlm-abi-slimming 全 4 artifacts (proposal + design + specs + tasks) 齐备, 等待 T-ABI-1 实施启动