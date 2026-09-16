# HSK-11: cpptlm-abi-slimming 跨仓契约镜像

> **镜像源** (Hub): `UsrLinuxEmu/ADR-088 §D5` (Status Update 提交后无 ack, 10 工作日窗口已过)
> **本仓镜像**: `openspec/changes/cpptlm-abi-slimming/{proposal.md,specs/cpptlm-emulator-abi/spec.md,tasks.md}`
> **父 HSK**: HSK-10 (cpptlm-tlp-wire-datapath, 2026-09-17) — 本 HSK 取代其 §4 ABI 清单
> **创建日期**: 2026-09-17
> **关联 ADR**: ADR-SOC-18-cpptlm-abi-slimming
> **维护**: CppTLM Team

---

## 1. Mirror Scope

本 HSK 镜像 **ABI 表面精简** (`cpptlm-abi-slimming` change) 的跨仓契约:

| 实体 | 范围 |
|------|------|
| **ABI 函数表面** | `include/abi/cpptlm_emulator.h` (22 → 18 函数) |
| **回调 typedef** | 4 callback 不动 (`cpptlm_intr_deliver_cb_t` 等) |
| **opaque 结构** | `cpptlm_emulator_t` 结构体零修改 |
| **内部 C++ API** | `DGpuBoard::backdoor_*` 保持不变 (驱动不可见) |
| **profile 选路** | `profile.pcie_path = "mock"` 替代 backdoor 行为 |

---

## 2. ABI 函数清单 (18 函数保留, 4 函数删除)

### 2.1 保留的 18 个驱动核心函数 (per `cpptlm_emulator.h` 行 64-115 实测)

| # | 函数名 | 行号 | 用途 |
|---|--------|------|------|
| 1 | `cpptlm_emulator_get_version` | 64 | 仿真器版本查询 |
| 2 | `cpptlm_emulator_get_device_count` | 66 | 设备数量 |
| 3 | `cpptlm_emulator_get_device_info` | 68 | 设备元数据 |
| 4 | `cpptlm_emulator_create` | 70 | 从 profile 创建 |
| 5 | `cpptlm_emulator_create_by_id` | 72 | 从 dev_id 创建 |
| 6 | `cpptlm_emulator_destroy` | 74 | 销毁 |
| 7 | `cpptlm_emulator_mmio_write` | 76 | BAR 写 |
| 8 | `cpptlm_emulator_mmio_read` | 79 | BAR 读 |
| 9 | `cpptlm_emulator_pcie_config_write` | 82 | PCIe Config 写 |
| 10 | `cpptlm_emulator_pcie_config_read` | 85 | PCIe Config 读 |
| 11 | `cpptlm_emulator_msix_init` | 94 | MSI-X table 初始化 |
| 12 | `cpptlm_emulator_msix_update_pending` | 96 | MSI-X pending 置位 |
| 13 | `cpptlm_emulator_msix_clear_pending` | 98 | MSI-X pending 清零 |
| 14 | `cpptlm_emulator_register_callbacks` | 103 | 注册 MSI-X/Error 回调 |
| 15 | `cpptlm_emulator_register_dma_translate_cb` | 110 | 注册 DMA 翻译回调 |
| 16 | `cpptlm_emulator_open` | 112 | 设备打开 |
| 17 | `cpptlm_emulator_close` | 113 | 设备关闭 |
| 18 | `cpptlm_emulator_get_adapter_info` | 114 | 适配器信息 |

### 2.2 删除的 4 个 backdoor 函数 (per `cpptlm_emulator.h` 行 88-108 实测)

| # | 函数名 | 行号 | 迁移路径 |
|---|--------|------|---------|
| 1 | `cpptlm_emulator_backdoor_read` | 88 | → `DGpuBoard::backdoor_read()` 内部 C++ API |
| 2 | `cpptlm_emulator_backdoor_write` | 91 | → `DGpuBoard::backdoor_write()` 内部 C++ API |
| 3 | `cpptlm_emulator_register_backdoor_cb` | 108 | → `DGpuBoard::backdoor_set_callback()` 内部 C++ API |
| 4 | `cpptlm_emulator_lookup_register` | 100 | → `DGpuBoard::lookup_register()` 内部 C++ API |

### 2.3 4 callback typedef 不动 (per `cpptlm_emulator.h` 行 56-62)

```c
typedef void (*cpptlm_intr_deliver_cb_t)(void* user_ctx, uint32_t vector, uint32_t trans_id);
typedef void (*cpptlm_error_cb_t)(void* user_ctx, uint32_t err_code, const char* err_msg);
typedef void (*cpptlm_reset_complete_cb_t)(void* user_ctx, uint32_t stream_id);
typedef void (*cpptlm_power_cb_t)(void* user_ctx, uint32_t state);
```

---

## 3. ABI 版本号

```
CPPTLM_EMULATOR_VERSION = "18 functions + 4 callback typedefs" (post-slimming)
```

(从 "22 functions + 4 callback typedefs" BUMP)

---

## 4. ABI 表面回退策略 (Hub 响应 → 本仓回退)

| Hub 侧响应 | 本仓回退 | 动作 |
|----------|---------|------|
| **ack** | 当前 plan 推进 | 删除 4 函数, BUMP 版本号, archive |
| **部分 ack** (要求保留某 backdoor) | 删除其他 3 函数, 保留指定 | 仅修改 spec.md + .cc, 保留指定函数 |
| **拒绝** | 全留 4 函数 | 仅修改 spec.md (REMOVED → MODIFIED), 不改 ABI 表面 |
| **无响应 (当前)** | **假定 Hub 侧已自行移除** | 继续推进 (driver 不可见 backdoor) |

---

## 5. 跨仓协调时间线

### 5.1 父 change 提交 + Hub 异步

| 时间 | 事件 |
|------|------|
| **Day 0** (2026-09-17) | 父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath` 创建 |
| **Day 0+** | 提交 Hub 侧 (UsrLinuxEmu) ADR-088 §D5 Status Update PR / issue (per T-P9-0-pre step 2) |
| **Day 1-10** | 等待 Hub review |
| **Day 10+** (2026-09-30) | **Hub 无响应, 超时 10 工作日窗口** |
| **Day 12+** (2026-10-02) | **触发 fallback**: 父 change 以 22 ABI 函数状态 archive |

### 5.2 本 change (cpptlm-abi-slimming) 推进

| 时间 | 事件 |
|------|------|
| **Day 0+** (2026-09-17) | 创建本 HSK-11 + proposal.md + spec.md + tasks.md + ADR-SOC-18 |
| **Day 1-2** | T-ABI-1 测试迁移 (`bar_store_value` 三维 key 签名) |
| **Day 2-3** | T-ABI-2 删除 4 函数 + BUMP 版本号 |
| **Day 3-4** | T-ABI-3 ADR + HSK 同步 (本 HSK-11) |
| **Day 4-5** | T-ABI-4 全量回归 + openspec validate --strict |
| **Day 5+** | T-ABI-5 archive 本 change |

### 5.3 Hub 拒绝回退策略 (per HSK-10 §5.3)

由于 Hub 侧目前无响应, **回退策略全部标注 "待协同确定"**:

- 等待 Hub ack → 任一方案 (ack/部分 ack/拒绝)
- Hub ack 到达后, 本 HSK-11 §4 表格对应策略执行

---

## 6. ABI 表面稳定性保证

**18 函数签名零修改** (per `cpptlm_emulator.h` 行 64-115 与 HSK-11 §2.1 字节比对)
**4 callback typedef 不动** (per §2.3)
**`cpptlm_emulator_t` 结构体零修改** (内部成员不变)

---

## 7. 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Proposed — ABI 表面精简跟进
**关联**: HSK-10 (本仓 Phase 9+ 主线跨仓契约镜像) — §4 ABI 清单被本 HSK-11 取代
**阻塞**: Hub (UsrLinuxEmu) ADR-088 §D5 Status Update 异步 ack