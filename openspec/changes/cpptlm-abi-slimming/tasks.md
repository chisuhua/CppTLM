# cpptlm-abi-slimming: Tasks

> **配套**: [`proposal.md`](../proposal.md)
> **父 change**: `2026-09-16-cpptlm-pcie-tlp-wire-datapath` (Phase 9+ 完整 TLP 链路)
> **拆分原因**: T-P9-0 ABI 精简 Hub ack 超时 (10 工作日窗口), 切出独立推进
> **Hub 状态**: 仍在跟踪 UsrLinuxEmu ADR-088 §D5 异步 ack

---

## 文件清单

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/abi/cpptlm_emulator.h` | 改 (-4 行) | 删除 4 个 backdoor/lookup 函数声明 |
| `src/abi/cpptlm_emulator.cc` | 改 (-X 行) | 删除 4 个函数实现 + 更新 ABI 版本号注释 |
| `test/test_pcie_power_state_cfg_access.cc` | 改 (3 处) | `bar_store_value(key)` → `bar_store_value(0, 0, key)` |
| `test/test_power_state_transition.cc` | 改 (1 处) | `bar_store_value(key)` → `bar_store_value(0, 0, key)` |
| `test/CMakeLists.txt` | 改 | 移除对已删 ABI 的测试 (若存在) |
| `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` | 新 | 架构决策: 22→18 函数精简 + Hub 协调时间线 |
| `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md` | 新 | 跨仓契约镜像 (取代 HSK-10 §4 ABI 清单) |
| `include/tlm/gpu/dgpu_board_shell.hh` | **0** | 内部 C++ API `DGpuBoard::backdoor_*` **保持不变** (驱动不可见) |

---

## T-P 列表 (Hub 异步 + Day 0+ 推进)

### T-ABI-0-pre: Hub 侧 ADR-088 状态更新跟踪（异步）

| 子任务 | 详情 |
|---|---|
| ✅ 1 | Hub 侧 PR/issue 状态: 提交 (per T-P9-0-pre step 2) |
| ⏳ 2 | 跟踪 Hub review 进度 (10 工作日响应上限已过, 当前无响应) |
| ✅ 3 | 触发拆分: 本 change (cpptlm-abi-slimming) 独立推进 |
| ✅ 4 | HSK-11 标注 "Hub ack 异步, 部分 ack 回退策略: 删除 3 保留 1 / 全删 / 全留" |

**验收**:
- ✅ Hub 侧已收到本 change 通知 (per HSK-11 §5.2)
- ✅ 本 change 独立推进 (主线 archive 22 ABI 函数状态)

### T-ABI-1: 测试迁移 (先迁测试 → 内部 C++ API, 1d)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写测试: 验证 4 个 ABI 函数已删 (编译期 + 链接期) |
| ✅ 2 | grep 实测: `grep -rn "cpptlm_emulator_backdoor\|register_backdoor_cb\|lookup_register" src/ include/ test/` 应为 0 call-site (排除 ABI 定义文件) |
| ✅ 3 | **迁移测试**: `test/test_pcie_power_state_cfg_access.cc` 中 `bar_store_value(key)` → `bar_store_value(0, 0, key)` (3 处) |
| ✅ 4 | **迁移测试**: `test/test_power_state_transition.cc` 中 1 处同样迁移 |
| ✅ 5 | 跑既有测试 → 应 PASS (内部 API 等价) |
| ✅ 6 | 提交: `refactor(abi): migrate 4 backdoor ABI call-sites to DGpuBoard C++ API` |

**验收**:
- ✅ 2 个测试文件 4 处 call-site 全部迁移
- ✅ 既有 `[pcie][power-state]` 测试零回归

### T-ABI-2: ABI 精简 (删除 4 函数, 1d, **gate = Hub ack 或超时 fallback**)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写测试: `[abi-slimming]` 标签 — 验证 4 个函数已从 ABI 删除 + 18 函数仍可用 |
| ✅ 2 | 跑 → **FAIL** (4 函数未删除) |
| ✅ 3 | `include/abi/cpptlm_emulator.h`: 删除 4 个函数声明 (`cpptlm_emulator_backdoor_read/write` + `register_backdoor_cb` + `lookup_register`) |
| ✅ 4 | `src/abi/cpptlm_emulator.cc`: 删除 4 个函数实现 + 更新 `CPPTLM_EMULATOR_VERSION` 注释 (从 "22 functions" 改 "18 functions") |
| ✅ 5 | `test/CMakeLists.txt`: 移除对已删 ABI 的测试 (若 `test_dgpu_backdoor.cc` 引用被删函数, 需迁移或删除) |
| ✅ 6 | 跑测试 → **PASS** (4 函数已删, 18 函数可用) |
| ✅ 7 | 提交: `refactor(abi): slim 22→18 functions, remove 4 backdoor/lookup` |

**验收**:
- ✅ `include/abi/cpptlm_emulator.h` 剩余 **18 个驱动核心函数** (22 - 4 = 18)
- ✅ 4 callback typedef 不动
- ✅ `cpptlm_emulator_t` 结构体零修改
- ✅ 18 函数签名零修改 (与父 spec.md §G7 清单字节比对)

### T-ABI-3: 文档同步 (ADR + HSK, 0.5d)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` 草案 |
| ✅ 2 | 写 `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md` 跨仓契约镜像 (取代 HSK-10 §4) |
| ✅ 3 | 提交: `docs(arch): ADR-SOC-18 + HSK-11 ABI 精简跟进` |

**验收**:
- ✅ ADR-SOC-18 含 Hub 协调时间线 + 回退策略
- ✅ HSK-11 含 18 函数完整清单 + 4 callback typedef + ABI 版本号

### T-ABI-4: 全量回归 + openspec validate (0.5d)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | `cmake --build build -j$(nproc)` → 退出码 0 |
| ✅ 2 | `ctest -R "pcie\|axi\|phase"` → 既有 66,564 assertions 零回归 |
| ✅ 3 | `[abi-slimming]` 新测试 PASS |
| ✅ 4 | `openspec validate cpptlm-abi-slimming --strict` PASS |
| ✅ 5 | 提交: `test(abi): verify 18 functions intact + 4 removed` (如有补充测试) |

**验收**:
- ✅ 全量 66,564+ 新增 assertions PASS
- ✅ openspec validate --strict 0 issues

### T-ABI-5: Archive (gate = 全 G 验收 PASS)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 最终验证所有 G1-G10 PASS |
| ✅ 2 | 确认父 change `2026-09-16-cpptlm-pcie-tlp-wire-datapath` 已 archive (22 ABI 函数状态) |
| ✅ 3 | 执行 `openspec archive cpptlm-abi-slimming` |
| ✅ 4 | 提交最终 PR (如适用) |

---

## Acceptance Gate

| Gate | 验证 |
|------|------|
| **G1** | `openspec validate cpptlm-abi-slimming --strict` PASS |
| **G2** | `grep -rn "cpptlm_emulator_backdoor\|register_backdoor_cb\|lookup_register" src/ include/` = **0 匹配** (排除 ABI 定义文件) |
| **G3** | `include/abi/cpptlm_emulator.h` 剩余 **18 个驱动核心函数** |
| **G4** | 18 函数签名零修改 (与父 spec.md §G7 字节比对) |
| **G5** | 4 callback typedef 不动 |
| **G6** | `cpptlm_emulator_t` 结构体零修改 |
| **G7** | 既有 `[pcie]` 测试零回归 (66,564 assertions PASS) |
| **G8** | `[abi-slimming]` 新增测试 PASS |
| **G9** | ADR-SOC-18 + HSK-11 同步提交 |
| **G10** | 父 change 已 archive |

---

## 风险与缓解

| Risk | 等级 | 缓解 |
|------|------|------|
| Hub ack 在 archive 前到达 | 🟡 中 | HSK-11 §5.3 标注: 继续推进 (假定 Hub 侧已自行移除) |
| 真实驱动依赖被删 backdoor 函数 | 🟢 低 | 实测仅需 ~12 核心 ABI, 远超 18; 如有依赖, Hub 侧修改 |
| 测试文件遗漏 call-site 迁移 | 🟡 中 | G2 grep 门禁 + G7 既有测试覆盖 |
| ABI 版本号遗漏 BUMP | 🟢 低 | G3 + G4 字节比对 |

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — ABI 精简拆分推进中
**关键路径**:
1. Hub 侧异步 ack (跨仓协调, 仍在跟踪)
2. Day 0+ 拆分成立, 创建本 change
3. Day 1-2 迁移 4 处 call-site
4. Day 2-3 删除 4 个 ABI 函数 + BUMP 版本号
5. Day 3-4 ADR + HSK 同步
6. Day 4-5 全量回归 + validate
7. Day 5+ archive