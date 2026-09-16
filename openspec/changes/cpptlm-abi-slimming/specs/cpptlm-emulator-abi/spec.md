# cpptlm-emulator-abi Spec — ABI 表面精简 (22 → 18 函数)

> **配套**: [`../proposal.md`](../proposal.md)
> **父 spec**: [`../../2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md`](../../2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md) §G7
> **本 spec 是父 spec 的"ABI 表面精简"子集**

## Purpose

本 spec 定义 **CppTLM Emulator ABI 表面精简**: 从当前 **22 个 C 函数** (per父 spec.md §6 `G7`) 删除 4 个 backdoor/lookup 函数, 精简至 **18 个驱动核心函数**。

驱动 (VFIO/IOMMUFD/amdgpu/nouveau) 通过 ABI 表面调用仿真器, 4 个 backdoor 函数 (`cpptlm_emulator_backdoor_read/write` + `register_backdoor_cb` + `lookup_register`) 是 **仿真器自检/调试辅助**, 不暴露给驱动。backdoor 行为通过 `profile.pcie_path = "mock"` 路径向驱动隐藏 (per父 spec.md §profile-pcie-path-routing)。

## REMOVED Requirements

### Requirement: cpptlm-emulator-backdoor-read-removed

CppTLM **SHALL NOT** 暴露 `cpptlm_emulator_backdoor_read` 函数。

理由:
- 当前实现 (per `src/abi/cpptlm_emulator.cc`): `cpptlm_emulator_backdoor_read(emu, bar, offset, buf, len)` 直接读 `DGpuBoard::bar_regs_`, 绕过 TLM/PCIe 协议栈
- backdoor 行为通过 `profile.pcie_path = "mock"` 切换, 驱动不可见 (per父 spec.md §profile-pcie-path-routing)
- 真实驱动 (VFIO/IOMMUFD/amdgpu/nouveau) **不依赖** backdoor 路径 (实测仅需 ~12 核心 ABI, 远超 18 个)
- 仿真器自检可使用内部 C++ API `DGpuBoard::backdoor_read(bar, offset, buf, len)` (per `include/tlm/gpu/dgpu_board_shell.hh:77`)

迁移路径:
- 既有调用 `cpptlm_emulator_backdoor_read` 的测试代码 (per `grep` 实测: **0 处调用**) 无需迁移
- 仿真器内部代码改用 `DGpuBoard::backdoor_read` 内部 C++ API

#### Scenario: 4 个 backdoor 函数已从 ABI 删除

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 不应包含 `cpptlm_emulator_backdoor_read` 函数声明
- **AND** 头文件中保留 18 个驱动核心函数 (per父 spec.md §6 `G7` 清单)
- **AND** 4 callback typedef (`cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t`) 保持不动

#### Scenario: 4 个 backdoor 函数已从实现删除

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 不应包含 `cpptlm_emulator_backdoor_read` 函数实现
- **AND** 头文件 + 实现文件 ABI 函数签名完全一致 (18 个)

#### Scenario: 头文件 + 实现 call-site 清零

- **WHEN** `grep -rn "cpptlm_emulator_backdoor_read" src/ include/ test/`
- **THEN** 排除 ABI 定义文件 (`include/abi/cpptlm_emulator.h`, `src/abi/cpptlm_emulator.cc`) 后应**为 0 匹配**
- **AND** `grep -rn "cpptlm_emulator_backdoor_read" src/ include/ | grep -v -E "abi/cpptlm_emulator\\.(h|cc)"` = 0 匹配

#### Scenario: 内部 C++ API `DGpuBoard::backdoor_read` 保持不变

- **WHEN** 检查 `include/tlm/gpu/dgpu_board_shell.hh`
- **THEN** `DGpuBoard::backdoor_read(uint8_t bar, uint64_t offset, void* buf, std::size_t len)` 函数声明保持不变
- **AND** 内部 C++ API 不暴露给 ABI 表面 (驱动通过 ABI 调用, 不直接调用 C++ API)

---

### Requirement: cpptlm-emulator-backdoor-write-removed

CppTLM **SHALL NOT** 暴露 `cpptlm_emulator_backdoor_write` 函数。

理由: 同 `cpptlm-emulator-backdoor-read-removed`。

迁移路径:
- `cpptlm_emulator_backdoor_write` 调用 (`grep` 实测: **0 处**) 无需迁移
- 仿真器内部代码改用 `DGpuBoard::backdoor_write` 内部 C++ API

#### Scenario: 4 个 backdoor 函数已从 ABI 删除

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 不应包含 `cpptlm_emulator_backdoor_write` 函数声明

#### Scenario: 4 个 backdoor 函数已从实现删除

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 不应包含 `cpptlm_emulator_backdoor_write` 函数实现

#### Scenario: 头文件 + 实现 call-site 清零

- **WHEN** `grep -rn "cpptlm_emulator_backdoor_write" src/ include/ test/`
- **THEN** 排除 ABI 定义文件后应**为 0 匹配**

---

### Requirement: cpptlm-emulator-register-backdoor-cb-removed

CppTLM **SHALL NOT** 暴露 `cpptlm_emulator_register_backdoor_cb` 函数。

理由:
- 当前实现: `cpptlm_emulator_register_backdoor_cb(emu, cb)` 注册 backdoor callback, 用于仿真器自检时通知驱动
- 真实驱动不需要 backdoor callback (正常 MMIO/MSI-X 流程即可)
- 仿真器自检可在内部直接调 `DGpuBoard::backdoor_set_callback(cb)`

迁移路径:
- `cpptlm_emulator_register_backdoor_cb` 调用 (`grep` 实测: **0 处**) 无需迁移
- 仿真器内部代码改用 `DGpuBoard::backdoor_set_callback` 内部 C++ API

#### Scenario: 4 个 backdoor 函数已从 ABI 删除

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 不应包含 `cpptlm_emulator_register_backdoor_cb` 函数声明

#### Scenario: 4 个 backdoor 函数已从实现删除

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 不应包含 `cpptlm_emulator_register_backdoor_cb` 函数实现

#### Scenario: 头文件 + 实现 call-site 清零

- **WHEN** `grep -rn "cpptlm_emulator_register_backdoor_cb" src/ include/ test/`
- **THEN** 排除 ABI 定义文件后应**为 0 匹配**

---

### Requirement: cpptlm-emulator-lookup-register-removed

CppTLM **SHALL NOT** 暴露 `cpptlm_emulator_lookup_register` 函数。

理由:
- 当前实现: `cpptlm_emulator_lookup_register(emu, offset, info)` 查找 BAR 寄存器元数据 (offset, name, size 等)
- 真实驱动通过标准 MMIO 路径访问寄存器, 不需要内部 register lookup
- 仿真器自检可在内部直接调 `DGpuBoard::lookup_register(offset)`

迁移路径:
- `cpptlm_emulator_lookup_register` 调用 (`grep` 实测: **0 处**) 无需迁移
- 仿真器内部代码改用 `DGpuBoard::lookup_register` 内部 C++ API

#### Scenario: 4 个 backdoor 函数已从 ABI 删除

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** 不应包含 `cpptlm_emulator_lookup_register` 函数声明

#### Scenario: 4 个 backdoor 函数已从实现删除

- **WHEN** 检查 `src/abi/cpptlm_emulator.cc`
- **THEN** 不应包含 `cpptlm_emulator_lookup_register` 函数实现

#### Scenario: 头文件 + 实现 call-site 清零

- **WHEN** `grep -rn "cpptlm_emulator_lookup_register" src/ include/ test/`
- **THEN** 排除 ABI 定义文件后应**为 0 匹配**

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — ABI 精简拆分推进中
**Hub 协调**: 详见 HSK-11 §5 跨仓协调时间线
**Hub 协调**: 详见 HSK-11 §5 跨仓协调时间线