# cpptlm-emulator-abi Spec — ABI 表面二级精简 (18 → 15 函数 + 1 宏, 修订版)

> **配套**: [`../proposal.md`](../proposal.md) · [`../design.md`](../design.md)
> **父 spec**: [`../../cpptlm-abi-slimming/specs/cpptlm-emulator-abi/spec.md`](../../cpptlm-abi-slimming/specs/cpptlm-emulator-abi/spec.md) (Phase 9+ 一级精简 22→18, ✅ Accepted)
> **本 spec 是父 spec 的"ABI 表面二级精简"子集**
> **修订版路径**: 18 → **15 函数 + 1 宏** (修订后, open/close 保留)

## Purpose

本 spec 定义 **CppTLM Emulator ABI 表面二级精简**: 在一级精简完成 18 个 C 函数基础上, **修订版**删除 2 个真冗余函数 (`create_by_id` + `get_adapter_info`) + `get_version` 改宏 (`CPPTLM_EMULATOR_VERSION_STRING`), 精简至 **15 个 C 函数 + 1 宏**。

驱动 (VFIO/IOMMUFD/amdgpu/nouveau) 通过 ABI 表面调用仿真器, 3 个真冗余函数是 **内部转发/查询**, 与现有函数重叠无附加价值; `get_version` 返回常量字符串, 改为预处理器宏零开销。

**修订版关键决策** (per ADR-SOC-20 §1.2): **`cpptlm_emulator_open` / `cpptlm_emulator_close` 保留** — fd 风格 API + 生命周期分层语义价值 (per 用户反馈 2027-09-17, 6 场景风险评估)。

---

## REMOVED Requirements

### Requirement: cpptlm-emulator-create-by-id-removed

CppTLM **SHALL NOT** 暴露 `cpptlm_emulator_create_by_id` 函数。

理由:
- 当前实现 (per `src/abi/cpptlm_emulator.cc:181`): `cpptlm_emulator_create_by_id(dev_id)` 内部通过 dev_id 查 profile_path, 然后调 `cpptlm_emulator_create(profile_path)` — **纯转发函数**
- 与 `cpptlm_emulator_create(profile_path)` 直接调用重叠
- 驱动可通过 `cpptlm_emulator_get_device_info(dev_id, ...)` 获取 profile_path, 再调 `create()`
- 减少一层间接调用, ABI 表面更扁平

迁移路径:
- 测试代码 `create_by_id(N)` → `create("profile_path_N")` (~9 处, 4 文件)
- **`cpptlm_emulator_open` 内部实现调整**: 原调 `create_by_id()` → 改为调 `create()` + 句柄封装 (因 `create_by_id` 已删)
- Hub 侧 (UsrLinuxEmu) 同步删除 `create_by_id` 调用点

#### Scenario: create_by_id 已从 ABI 头文件删除

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 不应包含 `cpptlm_emulator_create_by_id` 函数声明
- **AND** 头文件中保留 15 个驱动核心函数 (修订版, 含 `open/close`) + 1 宏 (`CPPTLM_EMULATOR_VERSION_STRING`)
- **AND** 4 callback typedef (`cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t`) 保持不动

#### Scenario: create_by_id 已从实现文件删除

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 不应包含 `cpptlm_emulator_create_by_id` 函数实现
- **AND** `cpptlm_emulator_open` 内部实现已调整为调 `cpptlm_emulator_create()` + 句柄封装 (而非 `create_by_id`)
- **AND** 头文件 + 实现文件 ABI 函数签名完全一致 (15 个)

#### Scenario: 头文件 + 实现 call-site 清零

- **WHEN** `grep -rn "cpptlm_emulator_create_by_id" src/ include/ test/`
- **THEN** 排除 ABI 定义文件 (`include/abi/cpptlm_emulator.h`, `src/abi/cpptlm_emulator.cc`) 后应**为 0 匹配**

---

### Requirement: cpptlm-emulator-get-adapter-info-removed

CppTLM **SHALL NOT** 暴露 `cpptlm_emulator_get_adapter_info` 函数。

理由:
- 当前实现 (per `src/abi/cpptlm_emulator.cc:468`): `cpptlm_emulator_get_adapter_info(handle, info)` 通过 handle 查找 emu, 调 `cpptlm_emulator_get_device_info(dev_id, info)` — **纯转发函数**
- 与 `cpptlm_emulator_get_device_info(dev_id, info)` 直接调用重叠
- 句柄依赖增加复杂度, 无附加价值 (驱动可通过 emu 内部 dev_id 字段查询)
- 减少一层间接调用, ABI 表面更扁平

迁移路径:
- 测试代码 `get_adapter_info(handle, ...)` → `get_device_info(dev_id, ...)` (~3 处, 2 文件)
- `test_dgpu_adapter_info.cc` 整个文件改写测试 `get_device_info`
- Hub 侧 (UsrLinuxEmu) 同步删除 `get_adapter_info` 调用点

#### Scenario: get_adapter_info 已从 ABI 头文件删除

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 不应包含 `cpptlm_emulator_get_adapter_info` 函数声明
- **AND** 头文件中保留 15 个驱动核心函数 (修订版)

#### Scenario: get_adapter_info 已从实现文件删除

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 不应包含 `cpptlm_emulator_get_adapter_info` 函数实现
- **AND** 头文件 + 实现文件 ABI 函数签名完全一致 (15 个)

#### Scenario: 头文件 + 实现 call-site 清零

- **WHEN** `grep -rn "cpptlm_emulator_get_adapter_info" src/ include/ test/`
- **THEN** 排除 ABI 定义文件后应**为 0 匹配**

---

## ADDED Requirements

### Requirement: cpptlm-emulator-version-string-macro-added

CppTLM **SHALL** 暴露 `#define CPPTLM_EMULATOR_VERSION_STRING` 宏作为版本号常量。

理由:
- `cpptlm_emulator_get_version()` 返回常量字符串 (per `src/abi/cpptlm_emulator.cc`): `"v1.0-dgpu-v0"` — 无计算逻辑
- 改为预处理器宏零开销 (编译期常量替换, 无函数调用开销)
- 减少跨仓协调成本 (Hub 侧无需同步删除 `get_version` 函数调用, ABI 函数兼容)
- 宏已在 `include/abi/cpptlm_emulator.h:24` 定义 (per父 change 父 ABI)

迁移路径:
- 测试代码 `get_version()` → `CPPTLM_EMULATOR_VERSION_STRING` 宏 (~3 处, 3 文件)
- Hub 侧 (UsrLinuxEmu) 可选迁移: 保留函数调用或改宏

#### Scenario: CPPTLM_EMULATOR_VERSION_STRING 宏已定义且值正确

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 包含 `#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"`
- **AND** 不应包含 `cpptlm_emulator_get_version` 函数声明 (修订版删除)
- **AND** 头文件中保留 15 个驱动核心函数 (修订版)

#### Scenario: get_version 函数实现已删除

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 不应包含 `cpptlm_emulator_get_version` 函数实现 (修订版删除)
- **AND** 头文件 + 实现文件 ABI 函数签名完全一致 (15 个, 不含 `get_version`)

#### Scenario: get_version call-site 清零

- **WHEN** `grep -rn "cpptlm_emulator_get_version" src/ include/ test/`
- **THEN** 排除 ABI 定义文件后应**为 0 匹配**

---

## MODIFIED Requirements (修订版, 关键保留)

### Requirement: cpptlm-emulator-open-and-close-retained

CppTLM **SHALL** 保留 `cpptlm_emulator_open` 和 `cpptlm_emulator_close` 函数 (修订版决策, 与初版 18→14+宏 路径不同)。

理由:
- **fd 风格 API 一致性**: 模仿 kernel driver fd 模式 (`open("/dev/dri/renderD128")` → ioctl(fd, ...) → `close(fd)`), driver 开发者心智模型统一
- **生命周期分层语义价值**: 主进程持有 `cpptlm_emulator_t*`, 业务模块借用 `cpptlm_handle_t`, 提供清晰的句柄边界
- **6 个 driver 场景风险评估** (per ADR-SOC-20 §1.2 修订理由):
  - 进程间 fd 共享 (SCM_RIGHTS): 🟢 零影响 (uint64_t 句柄非真 fd)
  - 多线程 ref count: 🟢 零影响 (之前无此能力)
  - **生命周期分层: 🟡 中等价值, 保留**
  - 多 emulator 实例: 🟢 零影响 (create 已支持)
  - handle 状态追踪: 🟢 零影响 (无 per-handle 状态)
  - **fd API 风格统一: 🟡 中等价值, 保留**

迁移路径:
- `cpptlm_emulator_open` / `cpptlm_emulator_close` 相关测试**零修改** (修订版保留)
- Hub 侧 (UsrLinuxEmu) **零修改** (修订版保留 fd 风格 API)
- 内部实现调整: `open()` 内部从调 `create_by_id` 改为调 `create` + 句柄封装 (因 `create_by_id` 已删)

#### Scenario: open/close 函数已从 ABI 头文件保留

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 包含 `cpptlm_emulator_open(int dev_id, cpptlm_handle_t* out_handle)` 函数声明
- **AND** 包含 `void cpptlm_emulator_close(cpptlm_handle_t handle)` 函数声明
- **AND** 头文件中保留 15 个驱动核心函数 (修订版, 含 `open/close`)

#### Scenario: open/close 函数实现已保留且内部调整

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 包含 `cpptlm_emulator_open` 函数实现 (内部调 `cpptlm_emulator_create()` + 句柄封装, 而非 `create_by_id`)
- **AND** 包含 `cpptlm_emulator_close` 函数实现
- **AND** 头文件 + 实现文件 ABI 函数签名完全一致 (15 个)

#### Scenario: open/close 调用点零修改

- **WHEN** `grep -rn "cpptlm_emulator_open\|cpptlm_emulator_close" src/ include/ test/`
- **THEN** 排除 ABI 定义文件后应**保持原有调用点** (修订版零修改)

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — ABI 二级精简 (修订版, 保留 open/close) 推进中
**Hub 协调**: 详见 HSK-12 §5 跨仓协调时间线 (待 P5-2 创建)
**关联**:
- 父 spec: `openspec/changes/archive/cpptlm-abi-slimming/specs/cpptlm-emulator-abi/spec.md` (Phase 9+ 一级精简)
- 主 ADR: `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` (修订版)
- 父 ADR: `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` (Status Update 已追加)
- 路线图: `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` (修订版)
- 实施计划: `.rddf/plans/cpptlm-abi-secondary-slimming.md` (修订版 TDD 5 步)
