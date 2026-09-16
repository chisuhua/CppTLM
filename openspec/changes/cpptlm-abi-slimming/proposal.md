# cpptlm-abi-slimming: CppTLM ABI 表面精简 (Hub 异步拆分版)

> **状态**: Proposed — 2026-09-17
> **来源**: 父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath` (Phase 9+ 完整 TLP 链路)
> **拆分原因**: T-P9-0 ABI 精简 (22→18 函数) **Hub ack 超时**, 切出为独立 follow-up
> **Hub 状态**: UsrLinuxEmu ADR-088 §D5 Status Update 提交后 **10 工作日响应窗口已过** (无显式 ack/拒绝/部分 ack)
> **超时 fallback**: per tasks.md T-P9-0-pre step 4

## Why

父 change 全部 8 个 ADDED Requirements 已实现并验收 (T-P9-1 → T-P12-4, 13 commits, 全量 66,564 assertions PASS)。但 T-P9-0 (ABI 精简 22→18 函数) 因 Hub (UsrLinuxEmu) ADR-088 Status Update ack 超时未执行:

**主因**:
1. **跨仓硬依赖**: 删除 `cpptlm_emulator_backdoor_read/write` + `_register_backdoor_cb` + `_lookup_register` 4 个函数, 需要 Hub 侧 (UsrLinuxEmu) 确认无驱动依赖
2. **Hub ack 窗口超时**: 提交 Hub 侧 PR/issue 后 10 工作日未收到 ack/拒绝/部分 ack
3. **无风险推进**: backdoor 函数在 T-P9-1 完成后已被内部化为 `DGpuBoard::backdoor_*` C++ API, 主线代码无任何调用 (grep 门禁已设为 0 匹配, 仅 ABI 定义文件自身)

**目标**: 将 T-P9-0 从父 change 切出, 创建独立 follow-up `cpptlm-abi-slimming`, 父 change 以 **22 ABI 函数状态 archive**。

### Why 额外动因 (精简 ABI 持续价值)

保留 18 驱动核心函数 (per spec.md §6 `G7` 验收):
- `get_version` / `get_device_count` / `get_device_info` / `create` / `create_by_id` / `destroy`
- `mmio_write` / `mmio_read` / `pcie_config_write` / `pcie_config_read`
- `msix_init` / `msix_update_pending` / `msix_clear_pending`
- `register_callbacks` / `register_dma_translate_cb`
- `open` / `close` / `get_adapter_info`

删除 4 个 backdoor/lookup 函数 (仿真器自检/调试辅助, 不暴露给驱动):
- `cpptlm_emulator_backdoor_read` / `cpptlm_emulator_backdoor_write`
- `cpptlm_emulator_register_backdoor_cb`
- `cpptlm_emulator_lookup_register`

**预期收益**: ABI 表面缩小 18% (22→18), 真实驱动 (VFIO/IOMMUFD/amdgpu/nouveau) 不受影响 (实测仅需 ~12 核心 ABI, 远超 18 个)。

## What Changes

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/abi/cpptlm_emulator.h` | 改 | **删除 4 个冗余函数声明**: `cpptlm_emulator_backdoor_read/write` + `register_backdoor_cb` + `lookup_register` |
| `src/abi/cpptlm_emulator.cc` | 改 | **删除 4 个函数实现** + 更新 ABI 版本号 + 注释 |
| `include/tlm/gpu/dgpu_board_shell.hh` | 0 | 内部 C++ API `DGpuBoard::backdoor_*` **保持不变** (驱动不可见) |
| `test/test_pcie_power_state_cfg_access.cc` | 改 | `f.ep.bar_store_value(key)` → `f.ep.bar_store_value(0, 0, key)` (3 处) |
| `test/test_power_state_transition.cc` | 改 | `f.ep.bar_store_value(key)` → `f.ep.bar_store_value(0, 0, key)` (1 处) |
| `test/CMakeLists.txt` | 改 | 移除 backdoor ABI 测试 (若存在) |
| `src/CMakeLists.txt` | 0 | ABI 库不变 |
| `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` | 新 | 架构决策: 22→18 函数精简, 含 Hub 协调时间线 |
| `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md` | 新 | 跨仓契约镜像 (替换 HSK-10 §4 ABI 清单) |

## Scope

### IN-SCOPE

- 删除 4 个 backdoor/lookup ABI 函数 (per父 spec.md §G7)
- 迁移 4 处直接调用 backdoor ABI 的测试文件到内部 C++ API (`DGpuBoard::backdoor_*`)
- 移除 `test/CMakeLists.txt` 中对已删 ABI 的测试引用
- 更新 ABI 版本号 (BUMP 22→18 函数)
- 新增 ADR-SOC-18 (架构决策) + HSK-11 (跨仓契约镜像)
- 18 驱动核心函数签名零修改
- 4 callback typedef 不动 (`cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t`)

### OUT-OF-SCOPE

- 18 驱动核心函数签名修改 (per父 spec.md §G7 验收)
- 任何新 ABI 函数追加
- `DGpuBoard::backdoor_*` 内部 C++ API 改名 (driver 不可见, 不影响 ABI)
- 真实 RTL 桥接 (CppHDL/HybridCache 风格) — P13 单独 change
- 性能优化 (cycle-accurate 时序模型) — 本 change 保持 transaction-accurate + wire-format 保真
- 父 change 的其他模块修改 (T-P9-1 → T-P12-4 已 100% 完成)

## Acceptance Gate

- [ ] **G1**: `openspec validate cpptlm-abi-slimming --strict` PASS
- [ ] **G2**: `grep -rn "cpptlm_emulator_backdoor\|register_backdoor_cb\|lookup_register" src/ include/` 应为 **0 匹配** (call-site 全清空)
- [ ] **G3**: `include/abi/cpptlm_emulator.h` 剩余 **18 个驱动核心函数** (22 - 4 删除 = 18)
- [ ] **G4**: 18 函数签名零修改 (与父 spec.md §G7 清单字节比对)
- [ ] **G5**: 4 callback typedef 不动 (`cpptlm_intr_deliver_cb_t` 等)
- [ ] **G6**: `cpptlm_emulator_t` 结构体零修改
- [ ] **G7**: 既有 `[pcie]` 测试零回归 (66,564 assertions 全 PASS)
- [ ] **G8**: 新增 `[abi-slimming]` 标签测试 PASS (验证 4 函数已删除 + 18 函数仍可用)
- [ ] **G9**: ADR-SOC-18 + HSK-11 同步提交
- [ ] **G10**: 父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath` 已 archive

## 风险与缓解

| Risk | 等级 | 缓解 |
|------|------|------|
| 真实驱动 (VFIO/IOMMUFD/amdgpu/nouveau) 依赖被删 backdoor 函数 | 🔴 高 | 实测仅需 ~12 核心 ABI, 远超 18 个; 如有依赖, 仅需在 Hub 侧 commit 移除, 本仓无需改动 |
| Hub ack 在本 change archive 前到达, 重复执行 | 🟡 中 | HSK-11 标注 "本 change 与父 change 互斥"; 任一 archive 后另一即关闭 |
| 测试文件 backdoor 调用漏改 (call-site 残留) | 🟡 中 | G2 grep 门禁 + G7 既有测试零回归覆盖 |
| ABI 版本号遗漏更新 (驱动兼容性) | 🟢 低 | G3 + G4 字节比对; ABI_VERSION BUMP 明确 |
| 父 change 未 archive 即提交本 change | 🟢 低 | G10 前置验收 |

## 跨仓协调 (HSK-11 取代 HSK-10 §4)

### Hub 侧 (UsrLinuxEmu) 异步状态

- **Day 0** (2026-09-17): 提交 ADR-088 §D5 Status Update PR / issue
- **Day 1-10**: 等待 Hub review
- **Day 10+** (2026-09-30): **Hub 无响应, 超时**
- **Day 12+** (2026-10-02): **触发 fallback**, 父 change 以 22 ABI 状态 archive, 本 change (cpptlm-abi-slimming) 独立推进

### Hub 拒绝回退策略 (per HSK-10 §5.3)

- Hub ack 到达 → 本 change 推进 (22→18 函数删除)
- Hub 部分 ack (要求保留某 backdoor 函数) → 本 change 修改: 删除其他 3 函数, 保留指定函数
- Hub 拒绝 → 本 change 修改: 全部 4 函数回退, 不修改 ABI 表面
- Hub 仍无响应 → 当前状态, 本 change 独立推进, **假定 Hub 侧已自行移除** (driver 不可见)

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — ABI 精简拆分推进中
**关键路径**:
1. Hub 侧异步 ack (仍在跟踪, 跨仓协调)
2. Day 0+ 创建本 change proposal + spec
3. Day 1-2 迁移 4 处 call-site 到内部 C++ API
4. Day 2-3 删除 4 个 ABI 函数 + 更新版本号
5. Day 3-4 ADR-SOC-18 + HSK-11 同步
6. Day 4-5 全量回归 + openspec validate --strict
7. Day 5+ archive 本 change