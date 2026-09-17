# Hub PR: [CPptlm-ABI-088-§D5] 18→15 函数 + 宏化 (修订版, 保留 open/close)

> **目标仓库**: `chisuhua/UsrLinuxEmu`
> **目标文件**: `docs/00_adr/adr-088-dgpu-complete-simulation.md` §D5
> **Cross-link**: [CppTLM #28](https://github.com/chisuhua/CppTLM/issues/28) (tracking issue)
> **Hub 端 issue**: [UsrLinuxEmu #33](https://github.com/chisuhua/UsrLinuxEmu/issues/33) (已创建 2027-09-18)
> **创建日期**: 2027-09-17 (本地) · 2027-09-18 (Hub issue 提交)
> **关联 OpenSpec**: `openspec/changes/cpptlm-abi-secondary-slimming/`
> **关联 ADR**: `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` (修订版)
> **关联 HSK**: `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md`
> **修订版路径**: 18 → **15 函数 + 1 宏** (修订后, open/close 保留)
> **Ack 期望**: Hub (UsrLinuxEmu) maintainer @chisuhua 14 天 ack 窗口

---

## PR Title

```
[CPptlm-ABI-088-§D5] 18→15 函数 + get_version 宏化 (修订版, 保留 open/close)
```

---

## PR Body

### Summary

CppTLM 提议对 UsrLinuxEmu ADR-088 §D5 (23→22→18 演进) 进行**第三轮精简**: **18 → 15 函数 + 1 宏**, 删除 2 个真冗余函数 (`create_by_id` + `get_adapter_info`) + `get_version` 改预处理器宏 (`CPPTLM_EMULATOR_VERSION_STRING`), **保留 `open`/`close` fd 风格 API** (修订版关键决策, per 用户反馈 2027-09-17)。

### ⚠️ 修订版 vs 初版对比

| 维度 | 初版 (18→14+宏) | **修订版 (18→15+宏)** |
|------|:---------------:|:---------------:|
| 删除/改宏总数 | 5 (含 open/close) | **3 (含 1 改宏)** |
| `open/close` 决策 | ❌ 删除 | ✅ **保留** |
| Hub ack 风险 | 🟡 中 (5 函数级) | 🟢 低 (2 函数级) |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |
| fd API 风格 | 丢失 | **保留** |

**修订原因** (per [ADR-SOC-20 §1.2](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md)):
- `open/close` 模仿 kernel driver fd 模式 (`open("/dev/dri/renderD128")` → ioctl(fd, ...) → `close(fd)`)
- **6 个 driver 场景风险评估**: 进程间 fd 共享 🟢零 / 多线程 ref count 🟢零 / **生命周期分层 🟡中等价值保留** / 多 emulator 实例 🟢零 / handle 状态追踪 🟢零 / **fd API 风格统一 🟡中等价值保留**

### 删除清单 (修订版, 仅 3 项)

| # | ABI | 行号 (CppTLM 当前) | 删除/改宏 | 迁移路径 |
|---|-----|---------------------|----------|---------|
| 1 | `cpptlm_emulator_create_by_id` | 72 | ❌ 删除 | → `cpptlm_emulator_create(profile_path)` (驱动通过 `get_device_info(dev_id, &info)` 获取 `info.profile_path`) |
| 2 | `cpptlm_emulator_get_adapter_info` | 103 | ❌ 删除 | → `cpptlm_emulator_get_device_info(dev_id, &info)` (按 dev_id 查询无句柄依赖) |
| 3 | `cpptlm_emulator_get_version` | 64 | 🔄 改宏 | → `#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"` (line 24 已定义) |

**`cpptlm_emulator_open` / `cpptlm_emulator_close` — 修订版保留 (fd 风格 API, 无变更)**

### ⚠️ Hub ADR-088 §D5 状态差异

Hub 侧 ADR-088 §D5 当前仍显示**原始 23 ABI 表**, **未包含 Phase 9+ 新增的 3 个 ABI** (`open` / `close` / `get_adapter_info`)。本 PR 同时包含 §D5 的 **完整重写** 以反映:

```
CppTLM 23 → 22 (T-P9-0 超时拆分) → 18 (cpptlm-abi-slimming 22→18 ✅ Accepted)
       → 15 + 1 宏 (cpptlm-abi-secondary-slimming, 修订版, 本 PR)
       = 15 函数 + 1 宏 (CPPTLM_EMULATOR_VERSION_STRING) + 4 callback typedef + cpptlm_emulator_t
```

**新增条目** (Phase 9+ TLP wire datapath 后引入, ADR-088 §D5 当前缺失):
- `cpptlm_emulator_open` (line 101)
- `cpptlm_emulator_close` (line 102)
- `cpptlm_emulator_get_adapter_info` (line 103)

**修订版最终列表** (15 + 1 宏 + 4 callback + cpptlm_emulator_t):

| 类别 | 函数 | 状态 |
|------|------|:----:|
| 设备管理 | `cpptlm_emulator_get_device_count` | ✅ 保留 |
| 设备管理 | `cpptlm_emulator_get_device_info` | ✅ 保留 |
| 设备生命周期 | `cpptlm_emulator_create` | ✅ 保留 |
| 设备生命周期 | `cpptlm_emulator_destroy` | ✅ 保留 |
| 数据面 | `cpptlm_emulator_mmio_read` | ✅ 保留 |
| 数据面 | `cpptlm_emulator_mmio_write` | ✅ 保留 |
| 数据面 | `cpptlm_emulator_pcie_config_read` | ✅ 保留 |
| 数据面 | `cpptlm_emulator_pcie_config_write` | ✅ 保留 |
| MSI-X | `cpptlm_emulator_msix_init` | ✅ 保留 |
| MSI-X | `cpptlm_emulator_msix_update_pending` | ✅ 保留 |
| MSI-X | `cpptlm_emulator_msix_clear_pending` | ✅ 保留 |
| 回调 | `cpptlm_emulator_register_callbacks` | ✅ 保留 |
| DMA | `cpptlm_emulator_register_dma_translate_cb` | ✅ 保留 |
| Handle API | `cpptlm_emulator_open` | ✅ **保留 (修订版关键)** |
| Handle API | `cpptlm_emulator_close` | ✅ **保留 (修订版关键)** |
| 版本查询 | `CPPTLM_EMULATOR_VERSION_STRING` (宏) | 🆕 替代 `get_version` |
| 4 callback typedef | `cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t` | ✅ 零修改 |
| Opaque struct | `cpptlm_emulator_t` | ✅ 零修改 |

### Hub 侧同步需求

| 同步项 | Hub 实施 | 时间窗 |
|--------|:-------:|:------:|
| 删除 `cpptlm_emulator_create_by_id` 调用点 (Hub `src/system_hw/`) | ✅ 必做 | Week 0-1 |
| 删除 `cpptlm_emulator_get_adapter_info` 调用点 | ✅ 必做 | Week 0-1 |
| `cpptlm_emulator_get_version()` → `CPPTLM_EMULATOR_VERSION_STRING` 宏 | 🟡 可选 | Week 0-2 |
| **`cpptlm_emulator_open` / `cpptlm_emulator_close` 调用** | **❌ 不动** | **修订版保留** |

### 测试与验收

| Gate | 验证 | 状态 |
|------|------|:----:|
| G1 | CppTLM `openspec validate cpptlm-abi-secondary-slimming --strict` PASS | ✅ |
| G2 | CppTLM grep call-site 0 命中被删函数 | ⏳ P5-4 |
| G3 | CppTLM `cpptlm_emulator.h` 剩余 15 函数 + 1 宏 + 4 callback | ⏳ P5-5 |
| G7 | CppTLM 既有 [pcie] 测试零回归 (≥66,564 assertions) | ⏳ P5-7 |
| G9 | Hub (UsrLinuxEmu) 2 处调用点删除 PR 合并 | ⏳ **本 PR 跟踪** |
| G11 | `open/close` 调用点零修改 | ⏳ P5-5 |

### Ack 期望

| 项 | 值 |
|----|---|
| **Ack 方** | @chisuhua (UsrLinuxEmu maintainer) |
| **Ack 形式** | 评论 `:+1:` / `approved` / 合并此 PR (修改 ADR-088 §D5) |
| **Ack 时间窗** | **14 天** (per HSK-1~8 历史约定) |
| **超时 fallback** | 假定 Hub 侧已自行移除 (driver 不可见), CppTLM 继续推进 |
| **CppTLM 跟踪 issue** | (待创建, 见 [HSK-12 §6.1](https://github.com/chisuhua/CppTLM/blob/main/docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md)) |

### 关联文档 (CppTLM 端)

- **主 ADR**: [ADR-SOC-20-cpptlm-abi-secondary-slimming.md](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md) (修订版)
- **父 ADR**: [ADR-SOC-18-cpptlm-abi-slimming.md](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md) (✅ Accepted, 22→18)
- **跨仓契约镜像**: [HSK-12-cpptlm-abi-secondary-slimming.md](https://github.com/chisuhua/CppTLM/blob/main/docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md)
- **OpenSpec change**: `openspec/changes/cpptlm-abi-secondary-slimming/` (4 文件, 修订版)
- **路线图**: [phase9-p5-secondary-slimming.md](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md)
- **实施计划**: `.rddf/plans/cpptlm-abi-secondary-slimming.md` (TDD 5 步)

### 父 change Hub ack 经验 (per cpptlm-abi-slimming, T-P9-0 超时拆分)

- 父 change 22→18 已 archive @ 2026-09-17 (CppTLM 端)
- Hub ack 10 工作日窗口超时无响应, CppTLM 触发 fallback 假定 Hub 已自行移除
- 本次 **修订版降低风险** (2 函数级 vs 父 change 5 函数级), Hub ack 期望更高成功率

---

**🤖 Generated by CppTLM Sisyphus Agent via cross-repo coordination protocol (HSK-12)**
**📅 Submission date**: 2027-09-18 (Mon, W38+1)
**🔗 Cross-link**: This PR corresponds to CppTLM OpenSpec change `cpptlm-abi-secondary-slimming` (📋 Proposed, awaiting Hub ack)
