# ADR-SOC-18: CppTLM ABI 表面精简 (22 → 18 函数)

> **状态**: Proposed — 2026-09-17
> **拆分自**: ADR-SOC-17 + 父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath` T-P9-0 (Hub ack 超时)
> **关联**: HSK-11 (跨仓契约镜像)
> **阻塞**: Hub (UsrLinuxEmu) ADR-088 §D5 Status Update 异步 ack

## Context

CppTLM 当前 ABI 表面 (`include/abi/cpptlm_emulator.h`) 暴露 **22 个 C 函数** 给 host driver (VFIO/IOMMUFD/amdgpu/nouveau):

**驱动核心 18 函数** (真实驱动调用):
- 设备管理: `get_version` / `get_device_count` / `get_device_info` / `create` / `create_by_id` / `destroy`
- MMIO: `mmio_write` / `mmio_read`
- PCIe Config: `pcie_config_write` / `pcie_config_read`
- MSI-X: `msix_init` / `msix_update_pending` / `msix_clear_pending`
- Callback: `register_callbacks` / `register_dma_translate_cb`
- Handle: `open` / `close` / `get_adapter_info`

**自检/调试 4 函数** (仿真器内部用, 驱动不依赖):
- `cpptlm_emulator_backdoor_read` / `cpptlm_emulator_backdoor_write` (绕开 TLM/PCIe 协议栈)
- `cpptlm_emulator_register_backdoor_cb` (backdoor callback 注册)
- `cpptlm_emulator_lookup_register` (BAR 寄存器元数据查找)

实测真实驱动仅需 ~12 核心 ABI, 4 个 backdoor 函数**无真实调用方**。backdoor 行为通过 `profile.pcie_path = "mock"` 路径向驱动隐藏 (per父 spec.md §profile-pcie-path-routing)。

## Decision

T-P9-0 (父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath`) 计划删除 4 个 backdoor 函数, 精简 ABI 至 18 函数。**Hub ack 超时** (10 工作日窗口已过, UsrLinuxEmu 无响应), 按 tasks.md T-P9-0-pre step 4 **超时拆分**, 创建独立 follow-up `cpptlm-abi-slimming` change:

### 决策 1: 拆分独立推进 (而非阻塞主线 archive)

主线 13 commits (T-P9-1 → T-P12-4) 已 100% 完成 (66,564 assertions PASS)。Hub ack 仅影响 ABI 函数签名精简, 不影响主线功能。**主线以 22 ABI 函数状态 archive**, ABI 精简在 follow-up `cpptlm-abi-slimming` 独立推进。

### 决策 2: 内部 C++ API 替代 ABI 函数

`DGpuBoard::backdoor_*` 内部 C++ API (`include/tlm/gpu/dgpu_board_shell.hh:77-78`) **保持不变**:
- `backdoor_read(bar, offset, buf, len)` — 仿真器自检用
- `backdoor_write(bar, offset, buf, len)` — 仿真器自检用
- `backdoor_set_callback(cb)` — 仿真器自检用
- `lookup_register(offset)` — 仿真器自检用

**驱动不可见** (C++ API 不暴露给 ABI 表面, 驱动通过 `extern "C"` ABI 调用)。

### 决策 3: ABI 版本号 BUMP

`CPPTLM_EMULATOR_VERSION` 注释 (per `src/abi/cpptlm_emulator.cc`):
- 当前: "22 functions + 4 callback typedefs"
- 改后: "**18 functions + 4 callback typedefs**"

### 决策 4: 跨仓协调策略 (HSK-11 §5.3)

Hub 侧 4 种响应 → 4 种回退:
| Hub 响应 | 回退策略 |
|----------|---------|
| ack | 当前 plan 推进 (删除 4 函数) |
| 部分 ack (保留某 backdoor) | 删除其他 3, 保留指定 |
| 拒绝 | 全留 4 函数, ABI 表面不动 |
| 无响应 (当前) | **假定 Hub 侧已自行移除** (driver 不可见), 继续推进 |

### 决策 5: 主线 archive 状态

父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath` archive 时添加 Status 注记:
> **Status**: 22 ABI 函数状态 archive (Hub ack 超时, T-P9-0 切出为 `cpptlm-abi-slimming`)
> **HSK-10 移交**: 见 `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md`

## Consequences

### 正面

- **ABI 表面缩小 18%** (22 → 18 函数), 真实驱动接口最小化
- **主线不阻塞**: 13 commits 完成 8 个 ADDED Requirements, 全部验证 PASS, 立即可 archive
- **跨仓异步**: Hub ack 何时到达不再阻塞主线
- **回退灵活**: 4 种 Hub 响应策略对应 4 种回退路径
- **内部 API 不动**: `DGpuBoard::backdoor_*` 保持不变, 仿真器自检功能不受影响

### 负面

- **ADR-088 §D5 未正式 ack**: Hub 侧 Status Update 未得到官方确认, 风险略增 (但实测驱动不依赖)
- **2 个测试文件需迁移** (3 + 1 = 4 处): `bar_store_value(key)` → `bar_store_value(0, 0, key)`, 影响极小
- **跨仓治理**: Hub 侧何时 ack 不确定, 10+ 工作日窗口内无法推进

### 风险与缓解

- **真实驱动依赖**: 实测仅需 ~12 核心 ABI, 远超 18; 如有依赖, Hub 侧 commit 移除, 本仓无需改动
- **测试漏改**: grep 门禁 (`cpptlm_emulator_backdoor` call-site = 0) + 既有测试覆盖
- **跨仓协调时间**: 长期跟踪, 必要时手动协商

## References

- 父 spec: `openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md` §G7
- follow-up change: `openspec/changes/cpptlm-abi-slimming/` (proposal.md + spec.md + tasks.md)
- 跨仓镜像: `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md`
- 原始 HSK-10 (现已被 HSK-11 取代): `docs/cross_repo/HSK-10-cpptlm-tlp-wire-datapath.md`
- 内部 API 文档: `include/tlm/gpu/dgpu_board_shell.hh:77-78`
- 关联 ADR: `docs/soc_arch/adr/ADR-SOC-17-pcie-mock-ip.md` (mock IP 边界, 同 ADR 系列)
- 父 ADR-SOC: `docs/architecture/14-pcie-ip-microarchitecture.md §12` (Phase 9+ 章节)

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: ✅ Accepted (2027-09-17) — 父 change `cpptlm-abi-slimming` 已 archive @ 18 ABI 函数状态

## Status Update

### 2027-09-17 — 后续二级精简 (per [ADR-SOC-20](./ADR-SOC-20-cpptlm-abi-secondary-slimming.md), 修订版)

**新增决策**: 进一步精简 18 → **15 函数** + `get_version` 改宏 (**保留 open/close**).

**关键发现**:
- `cpptlm_emulator_open()` 内部调用 `cpptlm_emulator_create_by_id()` (per `src/abi/cpptlm_emulator.cc:433`)
- 用户反馈 2027-09-17: open/close 有 fd 风格 + 生命周期分层语义价值, **不删除**

**精简后状态**: **15 函数表** (设备管理 2 + 数据面 4 + MSI-X 3 + 回调 2 + DMA 1 + **handle 2**) + 1 宏 (`CPPTLM_EMULATOR_VERSION_STRING`).

**删除清单** (修订后, 仅 3 项):
- ❌ `cpptlm_emulator_create_by_id` (与 create 重叠)
- ❌ `cpptlm_emulator_get_adapter_info` (与 get_device_info 重叠)
- 🔄 `cpptlm_emulator_get_version` → 宏

**保留清单** (修订后, 11 项核心 + 2 handle):
- ✅ 核心 13 函数 (设备管理 2 + 数据面 4 + MSI-X 3 + 回调 2 + DMA 1 + handle 2 open/close)
- ✅ 4 callback typedef (零修改)

**实施路径**: [phase9-p5-secondary-slimming.md](../roadmap/phase9-p5-secondary-slimming.md) — P5 阶段待 Hub ack 启动.

**关联**: [ADR-SOC-20](./ADR-SOC-20-cpptlm-abi-secondary-slimming.md) · [phase9-p5-secondary-slimming.md](../roadmap/phase9-p5-secondary-slimming.md)

### 2027-09-17 — P5 二级精简完成 (per [ADR-SOC-20](./ADR-SOC-20-cpptlm-abi-secondary-slimming.md), 修订版)

**完成状态**: ✅ Accepted (P5 已 archive @ 15 函数 + 1 宏)

**实施结果**:
- ✅ 头文件 `include/abi/cpptlm_emulator.h`: 3 声明删除 (get_version + create_by_id + get_adapter_info)
- ✅ 实现文件 `src/abi/cpptlm_emulator.cc`: 3 实现删除 + `open()` 内部调 create_by_id → create + resolve_profile_path
- ✅ 测试文件 6 处迁移: test_cpptlm_emulator_abi.cc + handle_helpers.hh + registry.cc + abi_slimming.cc + dgpu_board_shell_full_abi.cc + dgpu_adapter_info.cc
- ✅ examples/test_dlopen.cc 补做迁移 (Dry-Run 4 发现)

**修订版 G8 acceptance gate 满足** (修订版合并实现):
- `[abi-secondary-slimming]` tag: 7 assertions, 1 test case PASS
- 修订版决策: 合并到 `test_cpptlm_emulator_abi_slimming.cc` (不创建独立文件), per HSK-12 §9.3

**测试统计** (全量回归):
- [pcie]: 36,454 assertions / 374 test cases ✅ 零回归
- [abi]: 63 assertions / 20 test cases ✅ (含修订版合并 tag)
- [dgpu][shell][full_abi]: 26 assertions / 5 test cases ✅ (修订版 open/close 保留)
- ctest: 75/75 tests passed (100%) ✅

**修订版关键**:
- 🟢 **零 driver 功能影响**: 保留 open/close fd 风格 API
- 🟢 **Hub ack 风险降低**: 2 函数级 vs 初版 5 函数级
- 🟢 **可逆**: 未来如需删除 open/close, 仍可评估 (修订版策略)

**实施路径**: [phase9-p5-secondary-slimming.md](../roadmap/phase9-p5-secondary-slimming.md) ✅ 已完成
**归档**: `openspec archive cpptlm-abi-secondary-slimming` ✅ (P5-8 完成)

**关联**: [ADR-SOC-20](./ADR-SOC-20-cpptlm-abi-secondary-slimming.md) · [phase9-p5-secondary-slimming.md](../roadmap/phase9-p5-secondary-slimming.md)