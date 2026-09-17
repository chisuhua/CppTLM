# cpptlm-abi-secondary-slimming: Design (修订版: 18 → 15 + 宏化)

> **配套**: [`proposal.md`](../proposal.md) · [`specs/cpptlm-emulator-abi/spec.md`](../specs/cpptlm-emulator-abi/spec.md) · [`tasks.md`](../tasks.md)
> **父 change**: `cpptlm-abi-slimming` (Phase 9+ 一级精简 22→18, ✅ Accepted)
> **修订版路径**: 18 → **15 函数 + 1 宏** (修订后, open/close 保留)
> **主 ADR**: [`../../../docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md`](../../../docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md)

---

## 1. 架构概览 (修订版)

### 1.1 当前 ABI 表面 (per ADR-SOC-18 / 22→18 已完成)

```
Host Driver (VFIO/IOMMUFD/amdgpu/nouveau)
  │  extern "C" ABI (18 C 函数 + 4 callback typedef + 1 opaque struct)
  ▼
include/abi/cpptlm_emulator.h
  │  18 forward declarations + 4 callback typedefs (零修改) + cpptlm_emulator_t (零修改) + CPPTLM_EMULATOR_VERSION_STRING 宏
  ▼
src/abi/cpptlm_emulator.cc
  │  18 函数实现 (含 open/close + create_by_id/get_adapter_info/get_version)
  │  cpp: open() 内部调用 create_by_id() (per line 433)
  ▼
DGpuBoard C++ API (内部, 驱动不可见)
```

### 1.2 目标 ABI 表面 (本 change archive 后, 修订版)

```
Host Driver (VFIO/IOMMUFD/amdgpu/nouveau)
  │  extern "C" ABI (15 C 函数 + 1 宏 + 4 callback typedef)
  ▼
include/abi/cpptlm_emulator.h
  │  15 forward declarations (含 open/close) + 1 CPPTLM_EMULATOR_VERSION_STRING 宏 + 4 callback typedefs (零修改) + cpptlm_emulator_t (零修改)
  ▼
src/abi/cpptlm_emulator.cc
  │  15 函数实现 (含 open/close, 内部调 create() + 句柄封装)
  │  删除: create_by_id / get_adapter_info / get_version
  ▼
DGpuBoard C++ API (内部, 驱动不可见, 零修改)
```

### 1.3 关键修订 (per ADR-SOC-20 §1.2)

| 维度 | 当前 (18 ABI) | 目标 (15 ABI + 1 宏, 修订版) |
|------|---------------|-------------------------------|
| 总函数数 | 18 | **15** |
| 总宏数 | 0 | **1** (`CPPTLM_EMULATOR_VERSION_STRING`) |
| 设备管理 | 2 (`get_device_count`, `get_device_info`) | 2 (不变) |
| 设备生命周期 | 4 (`create`, `create_by_id`, `destroy`, `open`, `close`) | **4** (`create`, `destroy`, **`open`, `close`**) |
| 设备元数据 | 2 (`get_device_info`, `get_adapter_info`) | **1** (`get_device_info`) |
| 数据面 | 4 (`mmio_*`, `pcie_config_*`) | 4 (不变) |
| MSI-X | 3 (`msix_*`) | 3 (不变) |
| 回调注册 | 2 (`register_callbacks`, `register_dma_translate_cb`) | 2 (不变) |
| 版本查询 | 1 (`get_version`) | **0 (改宏 `CPPTLM_EMULATOR_VERSION_STRING`)** |

**修订版关键**: `open/close` 保留 (fd 风格 + 生命周期分层语义价值)

---

## 2. 设计细节

### 2.1 删除函数清单 (修订版, 仅 3 项)

#### ❌ `cpptlm_emulator_create_by_id` 删除

**实际当前实现** (per `src/abi/cpptlm_emulator.cc:181` + 同文件 `resolve_profile_path()` 工具函数):
```cpp
// 工具函数 (per 同文件匿名命名空间, line 70-81 实测)
namespace {
std::string resolve_profile_path(uint32_t dev_id) {
    // 通过 dev_id 查 profile_path
    auto profile = profile_db_.find(dev_id);
    return profile ? profile->path : "";
}
}  // namespace

// create_by_id 主实现 (per line 181)
cpptlm_emulator_t* cpptlm_emulator_create_by_id(uint32_t dev_id) {
    cpptlm_emulator_t* emu = new cpptlm_emulator_s();
    auto path = resolve_profile_path(dev_id);
    emu->board = make_unique<DGpuBoard>(...);
    emu->board->load_soc_config(load_profile_json(path));
    emu->board->init();
    // ... 全套初始化 (不调 create())
    return emu;
}
```

**关键观察**: `create_by_id` **不调** `create()` — 它直接做完整初始化 (与 `create()` 重叠但实现独立)。

**删除理由**:
- 与 `cpptlm_emulator_create(profile_path)` 功能重叠 (两者最终都创建 DGpuBoard 并 init)
- 驱动可通过两步调 `create()`: 先 `get_device_info(dev_id, &info)` 拿 `info.profile_path`, 再 `create(info.profile_path)`
- 减少 API 表面 (从两个入口到一个)

**内部调整 (修订版关键)**: `cpptlm_emulator_open(dev_id, &handle)` 内部需调整 — 原调 `create_by_id` (line 433) → 改为调 `create` + 句柄封装:

```cpp
// 旧 (line 433): cpptlm_emulator_open() 调 create_by_id()
// 新: cpptlm_emulator_open() 调 create() + 句柄封装
// (注: resolve_profile_path 是匿名命名空间函数, open 在同一编译单元可访问)
int cpptlm_emulator_open(uint32_t dev_id, cpptlm_handle_t* out_handle) {
    auto path = resolve_profile_path(dev_id);              // 复用工具函数
    cpptlm_emulator_t* emu = cpptlm_emulator_create(path.c_str());  // 调 create 而非 create_by_id
    if (!emu) return -1;
    cpptlm_handle_t handle = wrap_as_handle(emu);          // 句柄封装
    if (out_handle) *out_handle = handle;
    return 0;
}
```

**迁移路径优势**: `open` 内部复用 `resolve_profile_path` 工具函数 (同文件匿名命名空间), 无需新增 API; driver 端 `open/close` API 完全不动 (修订版保留 fd 风格)。

#### ❌ `cpptlm_emulator_get_adapter_info` 删除

**当前实现** (per `src/abi/cpptlm_emulator.cc:468`):
```cpp
int cpptlm_emulator_get_adapter_info(cpptlm_handle_t handle, cpptlm_adapter_info_t* info) {
    // 通过 handle 查找 emu, 调 get_device_info
    auto* emu = lookup_emu_by_handle(handle);
    return cpptlm_emulator_get_device_info(emu->dev_id, info);  // 转发
}
```

**删除理由**:
- 与 `cpptlm_emulator_get_device_info(dev_id, ...)` 完全重叠 (纯转发函数)
- 句柄依赖增加复杂度, 无附加价值
- 驱动可通过 `emu->dev_id` (私有字段) + `get_device_info(dev_id, ...)` 直接查询

#### 🔄 `cpptlm_emulator_get_version` 改宏

**当前实现**:
```cpp
const char* cpptlm_emulator_get_version(void) {
    return "v1.0-dgpu-v0";
}
```

**改宏后** (per `include/abi/cpptlm_emulator.h:24`):
```cpp
#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"
```

**驱动迁移**:
```c
// 旧
const char* ver = cpptlm_emulator_get_version();

// 新 (零开销, 预处理器宏)
const char* ver = CPPTLM_EMULATOR_VERSION_STRING;
```

**Hub 协调**: 函数实现可保留 (驱动兼容性), 或 Hub 侧直接改宏 (推荐, 零开销)

### 2.2 保留函数清单 (修订版, 15 项 + 4 callback)

| # | 函数 | 修订决策 | 保留理由 |
|---|------|---------|---------|
| 1 | `cpptlm_emulator_get_device_count` | ✅ 保留 | 设备枚举入口 |
| 2 | `cpptlm_emulator_get_device_info` | ✅ 保留 | 设备元数据 |
| 3 | `cpptlm_emulator_create` | ✅ 保留 | 创建 emulator (按 profile_path) |
| 4 | `cpptlm_emulator_destroy` | ✅ 保留 | 销毁 |
| 5 | `cpptlm_emulator_mmio_write` | ✅ 保留 | MMIO 数据面 |
| 6 | `cpptlm_emulator_mmio_read` | ✅ 保留 | MMIO 数据面 |
| 7 | `cpptlm_emulator_pcie_config_write` | ✅ 保留 | PCIe Config 数据面 |
| 8 | `cpptlm_emulator_pcie_config_read` | ✅ 保留 | PCIe Config 数据面 |
| 9 | `cpptlm_emulator_msix_init` | ✅ 保留 | MSI-X 初始化 |
| 10 | `cpptlm_emulator_msix_update_pending` | ✅ 保留 | MSI-X 触发 |
| 11 | `cpptlm_emulator_msix_clear_pending` | ✅ 保留 | MSI-X 清除 |
| 12 | `cpptlm_emulator_register_callbacks` | ✅ 保留 | 注册 4 类回调 |
| 13 | `cpptlm_emulator_register_dma_translate_cb` | ✅ 保留 | 注册 DMA 翻译 |
| 14 | `cpptlm_emulator_open` | ✅ **保留** (修订) | fd 风格 + 生命周期分层 |
| 15 | `cpptlm_emulator_close` | ✅ **保留** (修订) | fd 风格配对 |
| - | `CPPTLM_EMULATOR_VERSION_STRING` (宏) | 🆕 新增 | 版本号常量 |
| - | 4 callback typedef | ✅ 零修改 | 既有 |
| - | `cpptlm_emulator_t` (opaque struct) | ✅ 零修改 | 既有 |

### 2.3 测试迁移清单 (修订版, 4 文件 ~9 处)

#### `test/test_cpptlm_emulator_abi.cc`

- 移除 `FnGetVersion` 函数指针签名测试 (2 处)

#### `test/test_cpptlm_emulator_handle_helpers.hh`

- RAII helper: `create_by_id(0)` → `create("profile_path")` (1 处)

#### `test/test_cpptlm_emulator_registry.cc`

- 3 处 `create_by_id(0)` → `create("profile_path")`

#### `test/test_dgpu_board_shell_full_abi.cc`

- 4 处 `create_by_id(N)` → `create("profile_path_N")`
- 移除 `get_version()` 调用 (1 处)

#### `test/test_cpptlm_emulator_abi_slimming.cc`

- 移除 `get_version` / `get_adapter_info` 引用 (2 处)

#### `test/test_dgpu_adapter_info.cc`

- 整个文件改写测试 `get_device_info(dev_id, ...)`, 或合并到 `test_cpptlm_emulator_abi.cc`

**总计**: ~9 处 (修订版, open/close 相关测试**零修改**)

### 2.4 新增测试 (修订版)

#### `test/test_cpptlm_emulator_abi_secondary_slimming.cc` (新增)

```cpp
TEST_CASE("abi-secondary-slimming: 3 functions removed + 15 remain + 1 macro",
          "[abi-secondary-slimming]") {
    SECTION("removed functions should not be callable") {
        // 编译期不可见: create_by_id / get_adapter_info / get_version
        // 测试注释说明删除意图, 编译时不调用
    }
    SECTION("15 remaining functions should still be callable") {
        cpptlm_emulator_get_device_count();
        cpptlm_emulator_get_device_info(0, nullptr);
        cpptlm_emulator_create("profile_path");
        cpptlm_emulator_destroy(nullptr);
        cpptlm_emulator_mmio_write(0, 0, nullptr, 0);
        cpptlm_emulator_mmio_read(0, 0, nullptr, 0);
        cpptlm_emulator_pcie_config_write(0, 0, 0);
        cpptlm_emulator_pcie_config_read(0, 0, nullptr);
        cpptlm_emulator_msix_init(0, 0, 0);
        cpptlm_emulator_msix_update_pending(0, 0);
        cpptlm_emulator_msix_clear_pending(0, 0);
        cpptlm_emulator_register_callbacks(nullptr, 0);
        cpptlm_emulator_register_dma_translate_cb(nullptr, 0);
        cpptlm_emulator_open(0, nullptr);   // 修订版保留
        cpptlm_emulator_close(0);            // 修订版保留
    }
    SECTION("CPPTLM_EMULATOR_VERSION_STRING macro value correct") {
        REQUIRE(std::string(CPPTLM_EMULATOR_VERSION_STRING) == "v1.0-dgpu-v0");
    }
}
```

### 2.5 6 个 driver 场景风险评估 (修订版)

| 场景 | 删除 open/close 影响 | 决策 |
|------|---------------------|------|
| 进程间 fd 共享 (SCM_RIGHTS) | 🟢 零 (uint64_t 句柄非真 fd) | 不变 |
| 多线程 ref count | 🟢 零 (之前无此能力) | 不变 |
| **生命周期分层** | 🟡 中 (真实语义价值) | ✅ **保留** |
| 多 emulator 实例 | 🟢 零 (create 已支持) | 不变 |
| handle 状态追踪 | 🟢 零 (无 per-handle 状态) | 不变 |
| **fd API 风格一致性** | 🟡 中 (API 设计价值) | ✅ **保留** |

**结论**: 修订版保留 open/close, 删除真冗余 2 函数 + 改 1 宏, 风险从 🟡 降至 🟢

---

## 3. 实现路径 (TDD 5 步)

### WU-4: 测试迁移 (修订版)

```bash
# Step 1: Write failing test (编译失败: call-site 残留)
cmake --build build -j$(nproc) 2>&1 | grep -E "create_by_id|get_adapter_info|get_version"
# 期望: 编译错误 (因测试文件仍调用被删函数)

# Step 2: Verify fail (确认编译失败)

# Step 3: Implement (4 文件 ~9 处迁移, 见 §2.3)

# Step 4: Verify pass (编译通过 + grep 0 命中)
grep -rn "cpptlm_emulator_create_by_id\|cpptlm_emulator_get_adapter_info\|cpptlm_emulator_get_version" src/ include/ test/
# 期望: 0 命中 (排除 ABI 定义文件已删实现)
```

### WU-5: ABI 头/实现修改 (修订版)

```bash
# Step 1: Write failing test (g++ 编译错误)
cat > /tmp/test_abi_slim.cpp << 'EOF'
extern "C" {
#include "cpptlm_emulator.h"
int main() {
  cpptlm_emulator_create_by_id(0);     // 应编译失败
  cpptlm_emulator_get_adapter_info(0, nullptr);  // 应编译失败
  cpptlm_emulator_get_version();        // 应编译失败
  return 0;
}
}
EOF
g++ -I include/abi /tmp/test_abi_slim.cpp -o /tmp/test_abi_slim
# 期望: implicit declaration / undefined reference 错误

# Step 2: Verify fail (确认编译错误)

# Step 3: Implement (删 3 函数 + 改宏 + 保留 open/close 内部调 create())

# Step 4: Verify pass
g++ -I include/abi /tmp/test_abi_keep.cpp -o /tmp/test_abi_keep  # 15 函数 + open/close 编译通过
grep -c "^[a-zA-Z].*cpptlm_emulator_" include/abi/cpptlm_emulator.h  # 期望: 15 (修订版)
```

---

## 4. 修订版 vs 初版对比表

| 维度 | 初版 (18→14+宏) | 修订 (18→15+宏) |
|------|:---------------:|:---------------:|
| 删除函数数 | 5 | **3** (含 1 改宏) |
| `open/close` 决策 | 删除 | **保留** |
| Hub ack 风险 | 🟡 中 (5 函数级) | 🟢 低 (2 函数级) |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |
| fd API 风格 | 丢失 | **保留** |
| 未来扩展空间 | 不可逆 | 可逆 (可加不能减) |
| 总工作量 | 3.5 d | **3.3 d** |
| 测试迁移处数 | ~15 | **~9** |
| 迁移文件数 | 5-6 | **4** |

---

## 5. 关联文档

- 主 ADR: `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` (修订版)
- 父 ADR: `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` (Status Update 已追加)
- 路线图: `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` (修订版)
- 实施计划: `.rddf/plans/cpptlm-abi-secondary-slimming.md` (修订版 TDD 5 步)
- 关联 HSK: `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` (待创建)
- 父 change: `openspec/changes/archive/cpptlm-abi-slimming/` (✅ Accepted)
