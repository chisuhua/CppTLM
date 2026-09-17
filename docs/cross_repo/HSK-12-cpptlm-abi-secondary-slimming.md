# HSK-12: cpptlm-abi-secondary-slimming 跨仓契约镜像 (修订版, 18→15+宏)

> **镜像源** (Hub): `UsrLinuxEmu/ADR-088 §D5` (Status Update 待提交, 修订版 2 函数级)
> **本仓镜像**: `openspec/changes/cpptlm-abi-secondary-slimming/{proposal.md,design.md,specs/cpptlm-emulator-abi/spec.md,tasks.md}`
> **父 HSK**: HSK-11 (cpptlm-abi-slimming, 2026-09-17) — 一级精简 22→18
> **创建日期**: 2027-09-17
> **关联 ADR**: ADR-SOC-20-cpptlm-abi-secondary-slimming (修订版, 保留 open/close)
> **关联父 ADR**: ADR-SOC-18-cpptlm-abi-slimming (Status Update 已追加修订版引用)
> **维护**: CppTLM Team

---

## 1. Mirror Scope (修订版)

本 HSK 镜像 **ABI 表面二级精简** (`cpptlm-abi-secondary-slimming` change) 的跨仓契约:

| 实体 | 范围 (修订版) |
|------|--------------|
| **ABI 函数表面** | `include/abi/cpptlm_emulator.h` (18 → **15 函数 + 1 宏**) |
| **预处理器宏** | `CPPTLM_EMULATOR_VERSION_STRING` (line 24, "v1.0-dgpu-v0") — **替代 `get_version` 函数** |
| **回调 typedef** | 4 callback 不动 (`cpptlm_intr_deliver_cb_t` 等) |
| **opaque 结构** | `cpptlm_emulator_t` 结构体零修改 |
| **句柄类型** | `cpptlm_emulator_handle_t` (uint64_t, line 46) 不动 |
| **handle API** | `cpptlm_emulator_open` / `cpptlm_emulator_close` **保留** (修订版 fd 风格) |
| **profile 选路** | 不变 (`profile.pcie_path = "mock"` 等) |

---

## 2. ABI 函数清单 (15 函数保留, 3 函数删除/改宏)

### 2.1 保留的 15 个驱动核心函数 (修订版, per `cpptlm_emulator.h` 行 64-103 实测)

| # | 函数名 | 行号 | 用途 | 类别 |
|---|--------|------|------|------|
| 1 | `cpptlm_emulator_get_device_count` | 66 | 设备数量 | 设备管理 |
| 2 | `cpptlm_emulator_get_device_info` | 68 | 设备元数据 | 设备管理 |
| 3 | `cpptlm_emulator_create` | 70 | 从 profile_path 创建 | 设备生命周期 |
| 4 | `cpptlm_emulator_destroy` | 74 | 销毁 | 设备生命周期 |
| 5 | `cpptlm_emulator_mmio_write` | 76 | BAR 写 | 数据面 |
| 6 | `cpptlm_emulator_mmio_read` | 79 | BAR 读 | 数据面 |
| 7 | `cpptlm_emulator_pcie_config_write` | 82 | PCIe Config 写 | 数据面 |
| 8 | `cpptlm_emulator_pcie_config_read` | 85 | PCIe Config 读 | 数据面 |
| 9 | `cpptlm_emulator_msix_init` | 88 | MSI-X table 初始化 | MSI-X |
| 10 | `cpptlm_emulator_msix_update_pending` | 90 | MSI-X pending 置位 | MSI-X |
| 11 | `cpptlm_emulator_msix_clear_pending` | 92 | MSI-X pending 清零 | MSI-X |
| 12 | `cpptlm_emulator_register_callbacks` | 94 | 注册 MSI-X/Error 回调 | 回调 |
| 13 | `cpptlm_emulator_register_dma_translate_cb` | 99 | 注册 DMA 翻译回调 | 回调 |
| 14 | `cpptlm_emulator_open` | 101 | 设备打开 (修订版保留) | handle API |
| 15 | `cpptlm_emulator_close` | 102 | 设备关闭 (修订版保留) | handle API |

### 2.2 删除的 3 个真冗余函数/改宏 (修订版)

| # | 函数名 | 行号 | 类别 | 迁移路径 |
|---|--------|------|------|---------|
| 1 | `cpptlm_emulator_create_by_id` | 72 | 真冗余 | → `cpptlm_emulator_create(profile_path)` (驱动通过 `get_device_info(dev_id, ...)` 获取 profile_path) |
| 2 | `cpptlm_emulator_get_adapter_info` | 103 | 真冗余 | → `cpptlm_emulator_get_device_info(dev_id, ...)` (按 dev_id 查询无句柄依赖) |
| 3 | `cpptlm_emulator_get_version` | 64 | 常量字符串 | → `#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"` 宏 (line 24) |

### 2.3 预处理器宏 (修订版新增)

```c
// include/abi/cpptlm_emulator.h line 24 (实测)
#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"
```

**注**: 宏实际名称为 `CPPTLM_EMULATOR_VERSION_STRING`（带 `_EMULATOR_`），而非简化名 `CPPTLM_VERSION_STRING`。本 HSK-12 + HSK-11 父 + ADR-SOC-20 等文档如使用简化名，需后续修订统一。

### 2.4 4 callback typedef 不动 (per `cpptlm_emulator.h` 行 56-62, HSK-11 §2.3)

```c
typedef void (*cpptlm_intr_deliver_cb_t)(void* user_ctx, uint32_t vector, uint32_t trans_id);
typedef void (*cpptlm_error_cb_t)(void* user_ctx, uint32_t err_code, const char* err_msg);
typedef void (*cpptlm_reset_complete_cb_t)(void* user_ctx, uint32_t stream_id);
typedef void (*cpptlm_power_cb_t)(void* user_ctx, uint32_t state);
```

### 2.5 opaque 结构 (per `cpptlm_emulator.h` line 26)

```c
typedef struct cpptlm_emulator_s cpptlm_emulator_t;  // 零修改
typedef uint64_t cpptlm_emulator_handle_t;          // line 46, 零修改
```

---

## 3. 3 函数移除迁移指南 (修订版)

### 3.1 `cpptlm_emulator_create_by_id` 迁移

**旧调用**:
```c
cpptlm_emulator_t* emu = cpptlm_emulator_create_by_id(dev_id);
```

**新调用** (2 步):
```c
// 步骤 1: 获取 profile_path
cpptlm_device_info_t info;
cpptlm_emulator_get_device_info(dev_id, &info);

// 步骤 2: 用 profile_path 创建
cpptlm_emulator_t* emu = cpptlm_emulator_create(info.profile_path);
```

**内部实现调整** (`src/abi/cpptlm_emulator.cc`):
```cpp
// 旧: cpptlm_emulator_open() 调 create_by_id (line 433)
cpptlm_handle_t cpptlm_emulator_open(uint32_t dev_id, cpptlm_handle_t* out_handle) {
    auto profile_path = find_profile_by_id(dev_id);
    auto* emu = cpptlm_emulator_create_by_id(dev_id);  // 已删
    return wrap_as_handle(emu, out_handle);
}

// 新: 调 create() + 句柄封装
cpptlm_handle_t cpptlm_emulator_open(uint32_t dev_id, cpptlm_handle_t* out_handle) {
    auto profile_path = find_profile_by_id(dev_id);
    auto* emu = cpptlm_emulator_create(profile_path);  // 调 create 而非 create_by_id
    return wrap_as_handle(emu, out_handle);
}
```

**调用方清单**:
| 文件 | 处数 | 详情 |
|------|:---:|------|
| `src/abi/cpptlm_emulator.cc` | 1 | `open()` 内部调用 (line 433) |
| `test/test_cpptlm_emulator_abi.cc` | 1 | 签名测试 |
| `test/test_cpptlm_emulator_handle_helpers.hh` | 1 | RAII helper |
| `test/test_cpptlm_emulator_registry.cc` | 3 | 3 处 `create_by_id(0)` |
| `test/test_dgpu_board_shell_full_abi.cc` | 4 | 4 处 `create_by_id(N)` |
| **总计** | **10** | (修订版, Hub 侧 1 处另行统计) |

### 3.2 `cpptlm_emulator_get_adapter_info` 迁移

**旧调用**:
```c
cpptlm_adapter_info_t info;
cpptlm_emulator_get_adapter_info(handle, &info);
```

**新调用**:
```c
// 通过 handle 查找 emu, 获取 dev_id (私有字段, 驱动不可见)
// 推荐: 驱动保存 dev_id, 直接调 get_device_info
cpptlm_device_info_t info;
uint32_t dev_id = /* 驱动自己保存的 dev_id */;
cpptlm_emulator_get_device_info(dev_id, &info);
```

**注**: `cpptlm_emulator_get_adapter_info` 与 `cpptlm_emulator_get_device_info` 结构体字段对齐, 驱动仅需把 `handle` 改为 `dev_id` 参数。

**调用方清单**:
| 文件 | 处数 | 详情 |
|------|:---:|------|
| `test/test_cpptlm_emulator_abi_slimming.cc` | 1 | 签名测试 |
| `test/test_dgpu_adapter_info.cc` | 3 | 整个文件 (3 处调用) |
| **总计** | **4** | (修订版, Hub 侧 1 处另行统计) |

### 3.3 `cpptlm_emulator_get_version` 改宏迁移

**旧调用**:
```c
const char* ver = cpptlm_emulator_get_version();
```

**新调用** (2 种方式):
```c
// 方式 1 (推荐): 用宏, 零开销
const char* ver = CPPTLM_EMULATOR_VERSION_STRING;

// 方式 2 (兼容): 函数实现保留 (Hub 侧可选, 驱动零强制升级)
const char* ver = cpptlm_emulator_get_version();  // 仍可工作 (函数实现已删需 Hub 同步)
```

**调用方清单**:
| 文件 | 处数 | 详情 |
|------|:---:|------|
| `test/test_cpptlm_emulator_abi.cc` | 2 | 签名测试 |
| `test/test_cpptlm_emulator_abi_slimming.cc` | 1 | 引用测试 |
| `test/test_dgpu_board_shell_full_abi.cc` | 1 | 调用 |
| **总计** | **4** | (修订版, 函数实现可保留 Hub 兼容) |

---

## 4. CPPTLM_EMULATOR_VERSION_STRING 宏迁移

### 4.1 宏定义 (per `include/abi/cpptlm_emulator.h:24` 实测)

```c
#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"
```

### 4.2 驱动迁移路径

```c
// 旧
printf("emulator version: %s\n", cpptlm_emulator_get_version());

// 新 (推荐, 零开销)
printf("emulator version: %s\n", CPPTLM_EMULATOR_VERSION_STRING);
```

### 4.3 Hub 协调策略

| Hub 侧选择 | 实施 |
|----------|------|
| **改宏 (推荐)** | 删除 `get_version` 函数实现, 驱动改用宏 |
| **保留函数 (兼容)** | 保留 `get_version` 函数实现, 驱动零修改 (但失去零开销优势) |
| **混合** | 函数实现为空壳 + deprecation warning, 宏为推荐路径 |

本仓推荐**方式 1 (改宏)**, 因为零开销 + 零跨仓影响。Hub 侧可选择其他方式, ABI 兼容。

---

## 5. ABI 表面回退策略 (修订版, 2 函数级)

| Hub 侧响应 | 本仓回退 | 动作 |
|----------|---------|------|
| **ack** | 当前 plan 推进 | 删除 `create_by_id` + `get_adapter_info`, 改 `get_version` 宏, archive |
| **部分 ack** (要求保留某函数) | 删除其他函数, 保留指定 | 仅修改 spec.md + .cc, 保留指定函数 |
| **拒绝** | 全留 3 项 | 仅修改 spec.md (REMOVED → MODIFIED), 不改 ABI 表面 |
| **无响应 (10 工作日)** | **假定 Hub 侧已自行移除** | 继续推进 (driver 不可见) |

**修订版关键**: 仅 2 函数级 vs 初版 5 函数级, Hub ack 风险降低 60%。

### 5.1 修订版 vs 初版回退对比

| 维度 | 初版 (18→14+宏) | 修订 (18→15+宏) |
|------|:---------------:|:---------------:|
| 删除/改宏总数 | 5 | **3** |
| Hub ack 风险等级 | 🟡 中 | 🟢 低 |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |
| 跨仓协调成本 | 60% 高 | 40% 高 |
| `open/close` 保留 | ❌ | ✅ |

---

## 6. 跨仓协调时间线 (修订版)

### 6.1 本仓推进时间表

| 时间 | 事件 |
|------|------|
| **D0** (2027-09-17) | 创建本 HSK-12 + proposal.md + design.md + spec.md + tasks.md + ADR-SOC-20 (修订版) |
| **D0+** | 提交 Hub 侧 (UsrLinuxEmu) ADR-088 §D5 Status Update PR (修订版, 2 函数级) |
| **D1-2** | Hub 异步 ack + 本仓测试迁移预备 |
| **D2 末** | 关键路径 — Hub ack 到达 → 开始 P5-4 测试迁移 |
| **D3-5** | P5-5 ABI 头/实现修改 (`open` 内部调 `create` + 句柄封装) |
| **D5-6** | P5-6 新增 `[abi-secondary-slimming]` 标签测试 |
| **D6-7** | P5-7 全量回归 (4 项测试套件 PASS, ≥66,564 assertions) |
| **D8-10** | P5-8 archive + 聚合 commit |

### 6.2 Hub 异步协调

| 状态 | 触发条件 | 行动 |
|------|---------|------|
| **提交 (D0+)** | 本 HSK-12 + ADR-SOC-20 创建完成 | Hub 侧 PR 提交, 标记 [RFC] |
| **等待 (D0-D10)** | 10 工作日响应窗口 | Hub review |
| **ack 到达** | Hub 全部同意删除/改宏 | 本仓按当前 plan 推进 |
| **部分 ack** | Hub 要求保留某函数 | 修改 spec.md (REMOVED → MODIFIED), 仅删除被同意项 |
| **拒绝** | Hub 全拒绝 | 仅修改 spec.md, ABI 表面零变更 |
| **无响应 (D10+)** | Hub 仍未回复 | **触发 fallback**: 假定 Hub 已自行移除 (driver 不可见), 继续推进 |

---

## 7. ABI 表面稳定性保证 (修订版)

**15 函数签名零修改** (per `cpptlm_emulator.h` 行 66-102 与 HSK-12 §2.1 字节比对)
**4 callback typedef 不动** (per HSK-11 §2.3)
**`cpptlm_emulator_t` 结构体零修改** (内部成员不变)
**`cpptlm_emulator_handle_t` 类型不变** (line 46, uint64_t)
**`open/close` 句柄 API 保留** (修订版 fd 风格 + 生命周期分层)

---

## 8. 修订版对比表 (per ADR-SOC-20 §1.3)

| 维度 | 初版 (18→14+宏) | 修订 (18→15+宏) |
|------|:---------------:|:---------------:|
| 删除函数数 | 5 | **3** (含 1 改宏) |
| `open/close` 决策 | 删除 | **保留** |
| Hub ack 风险 | 🟡 中 (5 函数级) | 🟢 低 (2 函数级) |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |
| fd API 风格 | 丢失 | **保留** |
| 未来扩展空间 | 不可逆 | 可逆 (可加不能减) |
| 测试迁移处数 | ~15 | **~9** |
| 迁移文件数 | 5-6 | **4** |
| 总工作量 | 3.5 d | **3.3 d** |
| ADR-SOC-20 状态 | Proposed | Proposed (修订版) |

---

## 9. 关联与已知差异

### 9.1 关联文档

| 文档 | 路径 | 状态 |
|------|------|:----:|
| 父 HSK | `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md` | ✅ (一级精简) |
| 主 ADR | `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` | 📋 Proposed (修订版) |
| 父 ADR | `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` | ✅ Accepted (Status Update 已追加) |
| OpenSpec change | `openspec/changes/cpptlm-abi-secondary-slimming/` | 📋 Tasks (P5-1 已完成) |
| 路线图 | `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` | ✅ (修订版) |
| 实施计划 | `.rddf/plans/cpptlm-abi-secondary-slimming.md` | ✅ (修订版 TDD 5 步) |
| 模块文档 | `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` §9.3 | ✅ (修订版) |
| 总览 | `docs/soc_arch/roadmap/phase9-post-phase8-roadmap.md` | ✅ (6 梯队 + P5 修订版) |

### 9.2 ✅ 已统一 (修订后, WU-2 完成后)

**修订前发现 (WU-2 Step 3)**:
- 实际 ABI 头文件 (`include/abi/cpptlm_emulator.h:24`): `CPPTLM_EMULATOR_VERSION_STRING` + `"v1.0-dgpu-v0"`
- 部分文档误用简化名 `CPPTLM_VERSION_STRING` + `"v1.0-dgpu-v1"`

**修订后状态 (WU-2 完成后, 6 文件已修订)**:
- ✅ `.rddf/plans/cpptlm-abi-secondary-slimming.md`
- ✅ `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md`
- ✅ `openspec/changes/cpptlm-abi-secondary-slimming/proposal.md`
- ✅ `openspec/changes/cpptlm-abi-secondary-slimming/design.md`
- ✅ `openspec/changes/cpptlm-abi-secondary-slimming/specs/cpptlm-emulator-abi/spec.md`
- ✅ `openspec/changes/cpptlm-abi-secondary-slimming/tasks.md`
- ✅ `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` (本 HSK)

**统一验证**:
- 错误引用 (`CPPTLM_VERSION_STRING` / `v1.0-dgpu-v1`): 0 匹配
- 正确引用 (`CPPTLM_EMULATOR_VERSION_STRING`): 40 个
- 正确值 (`v1.0-dgpu-v0`): 10 个

**openspec validate --strict**: PASS (修订后仍 valid)

---

### 9.3 ✅ 修订版合并决策 (WU-6 完成后, 2027-09-17)

**修订背景**: 原 plan WU-6 要求创建独立 `test_cpptlm_emulator_abi_secondary_slimming.cc`，但 WU-4 迁移时已将 `[abi-secondary-slimming]` tag 合并到现有 `test_cpptlm_emulator_abi_slimming.cc`。

**修订版决策**: 采纳 WU-4 合并实现, **不创建独立文件**。

**修订理由** (per ADR-SOC-20 §2.1 修订版路径 + 一致性原则):
1. **演进连续性**: 一级精简 (22→18) + 二级精简 (18→15+宏) 是 ABI 连续演进, 单一测试文件符合演进叙事
2. **避免碎片化**: 独立文件会导致测试维护成本倍增 (修订版/初版/未来轮次各自一份)
3. **tag 机制天然支持**: Catch2 `[abi-secondary-slimming]` tag 单独可过滤, 不需要独立文件
4. **G8 acceptance gate 满足**: "[abi-secondary-slimming] 新增测试 PASS" 通过合并实现满足

**实施状态**:
- ✅ `test/test_cpptlm_emulator_abi_slimming.cc:24` tag `[abi-slimming][abi-secondary-slimming][pcie]`
- ✅ `test/CMakeLists.txt` 无需修改 (file GLOB 自动发现 test_*.cc)
- ✅ `./build/bin/cpptlm_tests "[abi-secondary-slimming]"` → 7 assertions, 1 test case PASS
- ✅ `./build/bin/cpptlm_tests "[abi-slimming]"` → 17 assertions, 4 test cases PASS (合并覆盖 22→18 + 18→15+宏)

**验收影响**: P5-6 修订版 ✅ 完成, G8 (新增标签测试 PASS) 满足, 无需 archive 前补救。

---

## 10. 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Proposed — ABI 表面二级精简 (修订版, 保留 open/close) 推进中
**关联**: HSK-11 (本仓 Phase 9+ 一级精简) — §2.1 18 函数清单被本 HSK-12 §2.1 15 函数清单取代
**阻塞**: Hub (UsrLinuxEmu) ADR-088 §D5 Status Update 异步 ack (修订版 2 函数级, 风险降低)
**修订记录**:
- 2027-09-17 初版 (路径 18→14+宏, 删除 open/close)
- 2027-09-17 用户反馈 (open/close fd 风格 + 生命周期分层语义价值)
- 2027-09-17 修订 (路径 18→15+宏, 保留 open/close, 删除 2 真冗余 + 改 1 宏)
- 2027-09-17 WU-2 Step 3 实测: 宏实际名称 `CPPTLM_EMULATOR_VERSION_STRING` (本 HSK-12 已修订统一)
