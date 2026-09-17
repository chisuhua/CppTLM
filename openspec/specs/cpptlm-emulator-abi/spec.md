# cpptlm-emulator-abi Specification

## Purpose

本 spec 定义 **CppTLM Emulator C ABI 表面**（当前状态：15 个 C 函数 + 1 预处理器宏 + 4 callback typedef + cpptlm_emulator_t opaque 结构体）。

**演进历史**:
- **v1.0 初始状态**: 23 个 C 函数（per Hub ADR-088 §D5）
- **第一轮精简 (2026-09-17, per [ADR-SOC-18](../soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md))**: 22 → 18 函数（删除 4 个 backdoor/lookup）
- **第二轮精简 (2027-09-17, per [ADR-SOC-20](../soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md) 修订版)**: 18 → 15 函数 + 1 宏（删除 2 真冗余 `create_by_id` + `get_adapter_info`，`get_version` 改 `CPPTLM_EMULATOR_VERSION_STRING` 宏，**保留 `open/close` fd 风格句柄 API**）

**修订版关键决策** (per ADR-SOC-20 §2.1):
- 🟢 `open/close` 保留（fd 风格 + 生命周期分层语义价值）
- 🟢 Hub ack 风险降低 60%（2 函数级 vs 初版 5 函数级）
- 🟢 零 driver 功能影响（实测 ~12 核心 ABI 远超 15）

**ABI 表面稳定性保证**:
- 15 个驱动核心函数签名零修改（与父 ADR-SOC-18 §G7 字节比对）
- 4 callback typedef 不动（`cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t`）
- `cpptlm_emulator_t` opaque 结构体零修改

## Requirements
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

