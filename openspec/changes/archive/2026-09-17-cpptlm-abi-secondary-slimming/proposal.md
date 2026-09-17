# cpptlm-abi-secondary-slimming: CppTLM ABI 表面二级精简 (修订版: 18 → 15 + 宏化)

> **状态**: 📋 Proposed — 2027-09-17
> **修订**: 初版建议删除 4 个 handle API (create_by_id + open + close + get_adapter_info), **修订为保留 open/close** (per 用户反馈 2027-09-17, fd 风格 + 生命周期分层语义价值)
> **来源**: 父 change `cpptlm-abi-slimming` (Phase 9+ 一级精简 22 → 18 函数, ✅ Accepted)
> **修订版路径**: 18 → **15 函数 + 1 宏** (修订后, open/close 保留)
> **跨仓**: ✅ (Hub ack required, 修订版 2 函数级 vs 初版 5 函数级)

## Why

父 change `cpptlm-abi-slimming` 完成 22→18 一级精简后, **用户调研 2027-09-17 明确"还可以继续精简"**:

**主因**:
1. **跨仓硬依赖(修订版降低)**: 删除 `cpptlm_emulator_create_by_id` + `cpptlm_emulator_get_adapter_info` 2 个真冗余函数, 需要 Hub 侧 (UsrLinuxEmu) 确认无驱动依赖 (修订版仅 2 函数级, vs 初版 5 函数级)
2. **`get_version` 改宏**: 返回常量字符串, 改为预处理器宏零开销 + 零跨仓影响 (`CPPTLM_EMULATOR_VERSION_STRING` 已在 `include/abi/cpptlm_emulator.h:24` 定义)
3. **保留 open/close (修订版关键)**: 用户反馈 2027-09-17 — fd 风格 + 生命周期分层语义价值 (per ADR-SOC-20 §1.2 6 场景风险评估)

**修订前后对比** (per ADR-SOC-20 §1.3):

| 维度 | 初版 (18→14+宏) | 修订 (18→15+宏) |
|------|:---------------:|:---------------:|
| 删除函数数 | 5 (含 open/close) | **3** (含 1 改宏) |
| `open/close` 决策 | 删除 | **保留** |
| Hub ack 风险 | 🟡 中 (5 函数级) | 🟢 低 (2 函数级) |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |
| fd API 风格 | 丢失 | **保留** |
| 未来扩展空间 | 不可逆 | 可逆 (可加不能减) |

### Why 详细动因

保留 15 驱动核心函数 (per父 change spec.md §G7 + ADR-SOC-20 §2.1):
- `get_device_count` / `get_device_info` / `create` / `destroy`
- `mmio_write` / `mmio_read` / `pcie_config_write` / `pcie_config_read`
- `msix_init` / `msix_update_pending` / `msix_clear_pending`
- `register_callbacks` / `register_dma_translate_cb`
- `open` / `close` (修订版保留, fd 风格 API)
- `CPPTLM_EMULATOR_VERSION_STRING` (宏, 替代 `get_version` 函数)

删除 3 个真冗余函数/改宏 (修订后):
- ❌ `cpptlm_emulator_create_by_id` (与 `create` 重叠, 按 profile_path 创建)
- ❌ `cpptlm_emulator_get_adapter_info` (与 `get_device_info` 重叠, 按 dev_id 查询无句柄依赖)
- 🔄 `cpptlm_emulator_get_version` → `#define CPPTLM_EMULATOR_VERSION_STRING` 宏

**预期收益**:
- ABI 表面缩小 17% (18→15 函数 + 1 宏), 真实驱动 (VFIO/IOMMUFD/amdgpu/nouveau) **零影响** (修订版 open/close 保留)
- 跨仓协调成本降低 60% (修订版 2 函数级 vs 初版 5 函数级)
- 保留 fd 风格 API 一致性 (与 kernel driver 心智模型匹配)
- 未来扩展空间可逆 (可加不能减)

## What Changes

| 文件 | 变化 (修订版) | 说明 |
|------|--------------|------|
| `include/abi/cpptlm_emulator.h` | 改 | 删除 3 个函数声明 (`create_by_id`, `get_adapter_info`, `get_version`); **保留 `open/close` 声明**; 确认 `CPPTLM_EMULATOR_VERSION_STRING` 宏 |
| `src/abi/cpptlm_emulator.cc` | 改 | 删除 3 个函数实现; 移除 `get_adapter_info` 句柄表逻辑; **保留 `open/close` 实现** (内部仍调 `create_by_id` 已删 → 调整为调 `create` + 句柄封装); 更新版本注释 "15 functions + 4 callbacks + 1 macro" |
| `test/test_cpptlm_emulator_abi.cc` | 改 | 移除 `get_version` 签名测试用例 |
| `test/test_cpptlm_emulator_handle_helpers.hh` | 改 | RAII helper 改用 `create("profile_path")` 替代 `create_by_id` |
| `test/test_cpptlm_emulator_registry.cc` | 改 | 3 处 `create_by_id(0)` → `create("profile_path")` |
| `test/test_dgpu_board_shell_full_abi.cc` | 改 | 4 处 `create_by_id(N)` → `create("profile_path_N")`; 移除 `get_version` 调用 |
| `test/test_cpptlm_emulator_abi_slimming.cc` | 改 | 移除 `get_version` / `get_adapter_info` 引用 |
| `test/test_dgpu_adapter_info.cc` | 改/删 | 整个文件改写测试 `get_device_info`, 或合并到 `test_cpptlm_emulator_abi.cc` |
| `test/test_cpptlm_emulator_abi_secondary_slimming.cc` | 新 | `[abi-secondary-slimming]` 标签测试 (修订版, 3 已删 + 15 仍可用) |
| `test/CMakeLists.txt` | 改 | 添加 `test_cpptlm_emulator_abi_secondary_slimming.cc` |
| `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` | 新 | 跨仓契约镜像 (取代 HSK-11 §已删除函数清单) |
| `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` | 已存在 (修订版) | 架构决策: 18→15+宏, 含修订理由 + 6 场景风险评估 |
| `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` | 改 (已加 ## Status Update) | 标注后续 ADR-SOC-20 二级精简 |
| `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` | 改 (已修订) | §9.3 ABI 状态 18 → 15 (修订版) |
| `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` | 已存在 (修订版) | P5 实施阶段文件 |
| `docs/soc_arch/roadmap/phase9-post-phase8-roadmap.md` | 改 (已同步) | 6 梯队 + P5 修订版引用 |

## Scope

### IN-SCOPE

- 删除 3 个真冗余 ABI 函数/改宏 (`create_by_id` + `get_adapter_info` + `get_version` 改宏)
- 迁移 ~9 处测试调用点 (4 文件) 到 `create("profile_path")` / `get_device_info(dev_id, ...)` / `CPPTLM_EMULATOR_VERSION_STRING` 宏
- **保留 `open/close` 句柄 API** (修订版关键决策, fd 风格 + 生命周期分层)
- `cpptlm_emulator_open` 内部实现调整: 从调 `create_by_id` → 调 `create` + 句柄封装 (因 `create_by_id` 已删)
- 新增 `[abi-secondary-slimming]` 标签测试
- 15 函数签名零修改 (与父 change spec.md §G7 字节比对)
- 4 callback typedef 不动
- `cpptlm_emulator_t` 结构体零修改
- Hub (UsrLinuxEmu) 同步删除 **2 处**调用点 (`create_by_id` + `get_adapter_info`; `get_version` 可选, 函数兼容)

### OUT-OF-SCOPE

- `open/close` 删除 (修订版决策, **保留**)
- 15 函数签名修改 (per父 change spec.md §G7 验收)
- 任何新 ABI 函数追加
- 真实 RTL 桥接 (CppHDL/HybridCache 风格) — P13 单独 change
- 性能优化 (cycle-accurate 时序模型)
- 父 change `cpptlm-abi-slimming` 已 archive, 不修改

## Acceptance Gate (修订版, 11 项)

- [ ] **G1**: `openspec validate cpptlm-abi-secondary-slimming --strict` PASS
- [ ] **G2**: `grep -rn "cpptlm_emulator_create_by_id\|cpptlm_emulator_get_adapter_info\|cpptlm_emulator_get_version" src/ include/ test/` = **0 匹配** (排除 ABI 定义文件已删实现)
- [ ] **G3**: `include/abi/cpptlm_emulator.h` 剩余 **15 函数 + 1 宏 + 4 callback typedef** (`open/close` 保留)
- [ ] **G4**: 15 函数签名零修改 (与父 change spec.md §G7 字节比对)
- [ ] **G5**: `CPPTLM_EMULATOR_VERSION_STRING` 宏值正确 (版本号 "v1.0-dgpu-v0")
- [ ] **G6**: `cpptlm_emulator_t` 结构体零修改
- [ ] **G7**: 既有 `[pcie]` 测试零回归 (66,564+ assertions PASS)
- [ ] **G8**: `[abi-secondary-slimming]` 新增测试 PASS (验证 3 已删 + 15 仍可用)
- [ ] **G9**: Hub (UsrLinuxEmu) **2 处**调用点删除 PR 合并 (`create_by_id` + `get_adapter_info`; `get_version` 可选)
- [ ] **G10**: ADR-SOC-18 Status Update 段已追加 (修订版引用)
- [ ] **G11**: **`open/close` 调用点零修改** (修订版, driver 功能影响零)

## 风险与缓解 (修订版)

| Risk | 等级 | 缓解 |
|------|------|------|
| 真实驱动依赖被删函数 | 🟢 低 (修订版) | 仅删除 2 真冗余函数; 实测 ~12 核心 ABI 远超 15 |
| Hub ack 在 archive 前到达 | 🟢 低 (修订版 2 函数级) | HSK-12 标注 "本 change 与父 cpptlm-abi-slimming 互斥" |
| 测试文件调用漏改 (call-site 残留) | 🟡 中 | G2 grep 门禁 + G7 既有测试零回归覆盖 |
| `open()` 内部仍调 `create_by_id()` 编译失败 | 🟡 中 | `open` 内部实现调整为调 `create` + 句柄封装 |
| `get_version` 改宏后 driver 仍调函数 | 🟢 低 | 函数实现已删, 驱动编译失败 → 驱动升级到宏 |
| ABI 版本号遗漏 BUMP | 🟢 低 | G3 + G4 字节比对 |

## 跨仓协调 (HSK-12 取代 HSK-11 §4)

### Hub 侧 (UsrLinuxEmu) 同步删除清单 (修订后)

| 同步项 | Hub 实施 | 备注 |
|--------|:-------:|------|
| `cpptlm_emulator_create_by_id(dev_id)` 调用 | ✅ 必做 | 改 `cpptlm_emulator_create(profile_path)` |
| `cpptlm_emulator_get_adapter_info(handle, ...)` 调用 | ✅ 必做 | 改 `cpptlm_emulator_get_device_info(dev_id, ...)` |
| `cpptlm_emulator_get_version()` 调用 | 🟡 可选 | 改 `CPPTLM_EMULATOR_VERSION_STRING` 宏 (或保留函数调用) |
| `cpptlm_emulator_open` / `cpptlm_emulator_close` 调用 | ❌ **不动** | 修订版保留 fd 风格 API, driver 零影响 |

### 协调节奏

- **D0** (2027-09-18 Mon): OpenSpec + HSK-12 创建 + Hub ack 提交 (2 函数级)
- **D1-2**: Hub 异步 ack + 本仓测试迁移预备
- **D2 末**: 关键路径 — Hub ack 到达 → 开始 P5-4
- **D3-5**: 测试迁移 + ABI 头/实现修改
- **D6-7**: 全量回归 + archive

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — ABI 二级精简 (修订版, 保留 open/close) 推进中
**关键路径**:
1. Hub 侧异步 ack (修订版 2 函数级, 风险降低)
2. D0+ 创建本 change proposal + spec
3. D1-3 迁移 4 文件 ~9 处 call-site
4. D3-5 删除 3 ABI 函数 + 改 `get_version` 宏 + 调整 `open` 内部实现
5. D5-6 新增 `[abi-secondary-slimming]` 标签测试
6. D6-7 全量回归 + openspec validate --strict
7. D8+ archive 本 change

## 关联

- 父 change (Phase 9+ 一级精简): `openspec/changes/archive/cpptlm-abi-slimming/` — 22→18 (✅ Accepted)
- 主 ADR: `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` (修订版)
- 父 ADR: `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` (Status Update 已追加)
- 路线图: `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` (修订版)
- 实施计划: `.rddf/plans/cpptlm-abi-secondary-slimming.md` (修订版 TDD 5 步)
- 关联 HSK: `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` (待创建)
