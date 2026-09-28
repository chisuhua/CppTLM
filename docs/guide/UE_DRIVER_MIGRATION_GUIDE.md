# UE Driver 移植指南: framebuffer_size 单一真源约定

> **版本**: v1.0
> **最后更新**: 2027-09-17 (Phase 9 P0.5-landing)
> **状态**: ✅ 完整
> **目标读者**: UE driver 作者 (UsrLinuxEmu / Unreal Engine / host driver 实现)

---

## 0. TL;DR

**`framebuffer_size` 的权威来源是 `bar_sizes[1]`**。UE 端 driver **不要硬编码 framebuffer 大小**。通过 `cpptlm_emulator_get_device_info()` 获取 BAR1 大小后, 整个 driver 生命周期使用该值。

错误示范（**绝对不要这样做**）:

```c
// ❌ WRONG — 硬编码 FB size, 与 SoC 配置脱节
const uint64_t kFramebufferSize = 16ULL * 1024 * 1024;  // 16MB
mmio_write(emu, 1, off, buf, min(len, kFramebufferSize));
```

正确做法:

```c
// ✅ CORRECT — 从 ABI 派生 FB size (bar_sizes[1])
cpptlm_device_info_t info;
if (cpptlm_emulator_get_device_info(dev_id, &info) == 0) {
    const uint64_t fb_size = info.bar_sizes[1];
    mmio_write(emu, 1, off, buf, min(len, fb_size));
}
```

---

## 1. 背景

### 1.1 为什么 framebuffer_size 必须从 BAR1 派生?

`DGpuBoard::load_soc_config` 实现的"单一真源"约定 (per gem5 `PhysicalMemory` + QEMU `vfio_map_bar` 模式):

```cpp
// 派生链 (per design §1.3 #4)
JSON 配置 pcie_ep.params.bar_sizes[1] → 派生 framebuffer_size_
                                       ↓
                                       init() 自动分配 framebuffer_storage_
                                       ↓
                                       bind_memory_backings() 注入 mem/sdma/gmmu
```

**任何硬编码 framebuffer 大小**:
- 与 SoC 配置脱节 (UE 看到的 FB 大小可能 ≠ 仿真实际)
- 错过 `[pcie][minimal_dgpu_soc]` 测试覆盖的 64GB cap 防护
- 错过 override (`framebuffer_size_bytes`) 机制

### 1.2 ABI 表面 (per ADR-088 §D5, frozen)

UE driver 可调用的 ABI (15 函数 + 1 宏):

| 函数 | 用途 |
|------|------|
| `cpptlm_emulator_create(profile_path)` | 加载 SoC JSON 配置, 返回 emu 句柄 |
| `cpptlm_emulator_destroy(emu)` | 释放 emu |
| `cpptlm_emulator_get_device_count()` | 枚举设备 |
| `cpptlm_emulator_get_device_info(dev_id, *info)` | **查询 BAR sizes + 设备拓扑** ← framebuffer_size 来源 |
| `cpptlm_emulator_mmio_read/write` | BAR0/BAR1 读写 |
| `cpptlm_emulator_pcie_config_read/write` | PCIe config space |
| `cpptlm_emulator_msix_init/update_pending/clear_pending` | MSI-X 中断 |
| `cpptlm_emulator_register_callbacks` | 注册 host-side 中断/error/reset/power 回调 |
| `cpptlm_emulator_register_dma_translate_cb` | 注册 host 侧 GMMU 翻译 |
| `cpptlm_emulator_open/close` | fd 风格句柄 API |

---

## 2. UE Driver 集成流程

### 2.1 完整流程图

```
┌─────────────────────────────────────────────────────────┐
│ 1. 加载 SoC 配置 + 创建 emulator                           │
│    cpptlm_emulator_create("configs/dgpu_soc_minimal_v1.json") │
└─────────────────┬───────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│ 2. 注册 host-side 回调                                    │
│    cpptlm_emulator_register_callbacks(intr_cb, ...)      │
│    cpptlm_emulator_register_dma_translate_cb(gmmu_cb)   │
│    cpptlm_emulator_msix_init(emu, table_size, mask)      │
└─────────────────┬───────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│ 3. ⭐ 查询 BAR sizes — 单一真源                            │
│    info = cpptlm_device_info_t{}                          │
│    cpptlm_emulator_get_device_info(dev_id, &info)        │
│    fb_size = info.bar_sizes[1]                            │
│    // UE driver 后续 BAR1 读写都用此 fb_size 派生边界    │
└─────────────────┬───────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│ 4. BAR0 GMMU 寄存器写 (PT_BASE_LO/HI + CTRL)             │
│    mmio_write(emu, 0, 0, &pt_lo, 4)                      │
│    mmio_write(emu, 0, 4, &pt_hi, 4)                      │
│    mmio_write(emu, 0, 8, &enable, 4)                     │
└─────────────────┬───────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│ 5. PTE 写入 framebuffer (经 BAR1 存储路由)                │
│    pte = (phys & ~0xFFF) | 1                              │
│    mmio_write(emu, 1, pte_offset, &pte, 8)                │
└─────────────────┬───────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│ 6. SDMA 提交 (H2D 描述符 + doorbell)                       │
│    // 注: ring entry 注入需要 ABI 扩展 (Phase 2+ 评估)    │
│    mmio_write(emu, 1, kBar1DoorbellOffset, &wptr, 4)     │
└─────────────────┬───────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│ 7. 等待 MSI-X 回调 (intr_cb)                             │
│    // 异步, 跨线程触发                                    │
└─────────────────┬───────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│ 8. 验证 framebuffer 数据 (backdoor 或 BAR1 读)           │
│    mmio_read(emu, 1, 0, out, 4096)                       │
│    // out 数据 == host_iova 经 PTE 翻译后的 phys 区域      │
└─────────────────────────────────────────────────────────┘
```

### 2.2 关键调用: get_device_info

```c
#include "abi/cpptlm_emulator.h"

cpptlm_device_info_t info;
memset(&info, 0, sizeof(info));

int rc = cpptlm_emulator_get_device_info(dev_id, &info);
if (rc != 0) {
    // ENOENT (-2) — dev_id 无效
    // 重新枚举: for (uint32_t i = 0; i < count; ++i) { get_device_info(i, ...); }
    return -1;
}

// info 字段 (per cpptlm_emulator.h):
//   vendor_id / device_id: 0x10DE / 0x1234 (NVIDIA-like, 当前 hard-code)
//   bar_sizes[0..5]: 6 个 BAR 大小 (BAR1 = bar_sizes[1])
//   visible_vram_size / invisible_vram_size: VRAM 总量
//   gpu_id / gfx_version / bdf: 设备拓扑
//   profile_path: 当前加载的 JSON 路径

const uint64_t fb_size = info.bar_sizes[1];  // ★ 单一真源
```

**注意**: `bar_sizes[1]` 默认 0 表示 BAR1 禁用 (config 未声明或 size=0). UE driver 应处理此边界 (mmio 返 OUT_OF_RANGE).

---

## 3. 常见错误模式 + 修复

### 3.1 ❌ 错误: 硬编码 FB size

```c
// ❌ BAD: 硬编码 16MB, 与 SoC JSON 配置脱节
const uint64_t kFB = 16 * 1024 * 1024;
mmio_write(emu, 1, 0, data, len);  // len 可能超过实际 FB
```

**修复**:
```c
// ✅ GOOD: 从 bar_sizes[1] 派生
const uint64_t fb_size = info.bar_sizes[1];
size_t safe_len = min(len, fb_size);
mmio_write(emu, 1, 0, data, safe_len);
```

### 3.2 ❌ 错误: 假设 FB 总是 ≥ 某值

```c
// ❌ BAD: 假设 BAR1 ≥ 1GB
if (info.bar_sizes[1] < 1024 * 1024 * 1024) return -EINVAL;
```

**修复**:
```c
// ✅ GOOD: 直接使用实际 size, 不做硬编码假设
const uint64_t fb_size = info.bar_sizes[1];
// fb_size 可以是 4KB (BAR1 禁用) 到 16MB (minimal) 到 1GB (Phase 8 dgpu_board_v1)
```

### 3.3 ❌ 错误: 忽略 64GB cap

```c
// ❌ BAD: 配置巨大 FB size 期望系统支持
mmio_write(emu, 1, 0, data, 1ULL << 40);  // 1TB — 触发 size cap, create 返 nullptr
```

**修复**:
```c
// ✅ GOOD: 接受 64GB cap, fail gracefully
const uint64_t kMaxFBSize = 64ULL << 30;  // 64GB 硬上限
if (fb_size > kMaxFBSize) {
    fprintf(stderr, "FB too large: %llu > 64GB\n", fb_size);
    return -1;
}
```

### 3.4 ❌ 错误: 误用 `attach_framebuffer_for_testing`

`attach_framebuffer_for_testing()` 是**测试 harness API**, UE driver **不应调用**:

```c
// ❌ BAD: UE driver 用测试钩子 (链接错误或语义错误)
// attach_framebuffer_for_testing(buf, size);  // <-- C++ 私有, ABI 不暴露
```

**正确做法**: UE driver 只通过 23 ABI, framebuffer 自动由 SoC JSON + `bar_sizes[1]` 派生。

---

## 4. 平台差异: ctypes (Python) vs FFI (C/Rust)

### 4.1 Python (ctypes) — UE 端推荐用法

```python
import ctypes
from pathlib import Path

# 加载库
lib = ctypes.CDLL("build/lib/libcpptlm_emulator.so")

# 绑定 ABI (15 函数 + 1 宏)
lib.cpptlm_emulator_create.restype = ctypes.c_void_p
lib.cpptlm_emulator_create.argtypes = [ctypes.c_char_p]

lib.cpptlm_emulator_destroy.restype = None
lib.cpptlm_emulator_destroy.argtypes = [ctypes.c_void_p]

lib.cpptlm_emulator_get_device_info.restype = ctypes.c_int
lib.cpptlm_emulator_get_device_info.argtypes = [
    ctypes.c_uint32,
    ctypes.POINTER(CpptlmDeviceInfo),  # mirror cpptlm_device_info_t
]

# ... (其他 13 个函数同模式)

# 创建 + 派生 FB size
emu = lib.cpptlm_emulator_create(b"configs/dgpu_soc_minimal_v1.json")
assert emu, "create failed"

count = lib.cpptlm_emulator_get_device_count()
for dev_id in range(count):
    info = CpptlmDeviceInfo()
    if lib.cpptlm_emulator_get_device_info(dev_id, ctypes.byref(info)) == 0:
        fb_size = info.bar_sizes[1]  # ★ 单一真源
        print(f"device {dev_id}: BAR1={fb_size} bytes")
        break

# BAR1 写 (安全: len ≤ fb_size)
data = (ctypes.c_uint8 * 4096)()
lib.cpptlm_emulator_mmio_write(emu, 1, 0, data, min(4096, fb_size))

lib.cpptlm_emulator_destroy(emu)
```

### 4.2 C / Unreal Engine — UE linux_compat 集成

```c
#include "abi/cpptlm_emulator.h"

void ue_driver_init(void) {
    cpptlm_emulator_t* emu = cpptlm_emulator_create("configs/dgpu_soc_minimal_v1.json");
    if (!emu) return;

    // 查询 BAR sizes
    cpptlm_device_info_t info = {0};
    if (cpptlm_emulator_get_device_info(0, &info) == 0) {
        const uint64_t fb_size = info.bar_sizes[1];  // ★ 单一真源
        g_fb_size = fb_size;
        g_emu = emu;
    }
}

void ue_driver_submit(void* host_buf, size_t len) {
    // 安全: 截断到 FB 边界
    if (len > g_fb_size) len = g_fb_size;
    cpptlm_emulator_mmio_write(g_emu, 1, 0, host_buf, len);
}
```

### 4.3 Rust (UE 4.27+ UnrealEngineRust)

```rust
use std::os::raw::{c_char, c_void, c_int, c_uint8, c_uint32, c_uint64};

#[repr(C)]
#[derive(Default)]
struct CpptlmDeviceInfo { /* mirror cpptlm_device_info_t */ }

extern "C" {
    fn cpptlm_emulator_create(profile: *const c_char) -> *mut c_void;
    fn cpptlm_emulator_destroy(emu: *mut c_void);
    fn cpptlm_emulator_get_device_info(dev_id: c_uint32, info: *mut CpptlmDeviceInfo) -> c_int;
    fn cpptlm_emulator_mmio_write(emu: *mut c_void, bar: u8, off: u64,
                                  buf: *const c_uint8, len: usize) -> c_int;
}

fn ue_driver_init() -> Option<*mut c_void> {
    let path = std::ffi::CString::new("configs/dgpu_soc_minimal_v1.json").ok()?;
    let emu = unsafe { cpptlm_emulator_create(path.as_ptr()) };
    if emu.is_null() { return None; }

    let mut info = CpptlmDeviceInfo::default();
    let rc = unsafe { cpptlm_emulator_get_device_info(0, &mut info) };
    if rc == 0 {
        // info.bar_sizes[1] — 单一真源 (经 C 头 mirror)
        unsafe { (*emu).fb_size = info.bar_sizes[1]; }
    }
    Some(emu)
}
```

---

## 5. 进阶: Override 与 Cap 行为

### 5.1 JSON `framebuffer_size_bytes` override

`configs/dgpu_soc_minimal_v1.json`:
```json
{
  "name": "minimal_soc_overlay",
  "framebuffer_size_bytes": 33554432,  // 32MB override
  "modules": [...]
}
```

**UE driver 影响**:
- `info.bar_sizes[1]` 仍是 BAR 编码大小 (config 中 `pcie_ep.params.bar_sizes[1]`), 不一定等于 `framebuffer_size_bytes`
- UE 写入的边界应基于 **实际可写范围**, 由 `bar_sizes[1]` + `framebuffer_size_bytes` 取 min
- 当前实现: override 胜出 (design §1.3 #4), BAR1 cfg space 仍按 `bar_sizes[1]` 编码

### 5.2 64GB Cap

```cpp
constexpr uint64_t kMaxFramebufferSize = 64ULL << 30;  // 64GB
```

**UE driver 影响**:
- `cpptlm_emulator_create()` 配置 framebuffer_size_bytes > 64GB 时返 nullptr
- UE driver 应检查 create 返回值, 不要假设成功

### 5.3 attach_framebuffer_for_testing (测试 harness, UE 不该用)

测试代码可调 `attach_framebuffer_for_testing(external_buf, size)` override framebuffer backing, **但**:
- 仅在 `load_soc_config` **之后**调用 (顺序 Inv-2)
- P0.5-landing 后为**可选 override**, 非必需 (framebuffer auto-allocates)
- **UE driver 不暴露此 API** — UE 端只用 ABI

---

## 6. 测试与验证

UE driver 应在以下场景验证:

| 测试 | 验证 |
|------|------|
| `create("configs/dgpu_soc_minimal_v1.json")` 返非 null | 配置 + ABI 路径正确 |
| `get_device_info(0, &info).bar_sizes[1] == 16777216` | BAR1 size 派生正确 |
| `mmio_write(emu, 1, 0, buf, fb_size)` 返 0 | BAR1 写闭环 |
| `mmio_read(emu, 1, 0, out, fb_size)` 返 0 且数据一致 | BAR1 读闭环 |
| `create("configs/dgpu_board_v1.json")` 也返非 null | Phase 8 默认配置兼容 |
| `create(64GB_plus.json)` 返 nullptr | size cap 防护 |

测试模板见: `examples/test_cpptlm_emulator_dlopen/test_dlopen_minimal_soc.cc` (c++) + Python ctypes demo (可选后续).

---

## 7. 关联文档

- **架构设计**: `docs/designs/dgpu-soc/architecture.md` (Phase 9 minimal SoC, 2026-09-26 迁移)
- **ABI 表面**: `openspec/specs/cpptlm-emulator-abi/spec.md` (15 + 1 ABI 定义)
- **测试覆盖**: `openspec/specs/minimal-dgpu-soc-abi-landing/spec.md` (P0.5-landing ADDED Requirements)
- **dlopen 示例**: `examples/test_cpptlm_emulator_dlopen/` (C 实现模板)
- **P0.5-landing KEY INVARIANT**: `AGENTS.md` (framebuffer 自动分配条目)

---

## 8. 变更历史

| 日期 | 变更 |
|------|------|
| 2027-09-17 | v1.0 首次发布 (Phase 9 P0.5-landing 配套) |

---

**Owner**: CppTLM Team
**维护**: Phase 9 + Phase 10 driver 集成期
**License**: 同 CppTLM 项目