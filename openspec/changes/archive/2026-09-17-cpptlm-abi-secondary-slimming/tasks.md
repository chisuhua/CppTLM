# cpptlm-abi-secondary-slimming: Tasks (修订版, 8 步对应 P5-1..P5-8)

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`specs/cpptlm-emulator-abi/spec.md`](../specs/cpptlm-emulator-abi/spec.md)
> **关联实施计划**: [`../../../.rddf/plans/cpptlm-abi-secondary-slimming.md`](../../../.rddf/plans/cpptlm-abi-secondary-slimming.md) (TDD 5 步)
> **关联阶段**: `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` (修订版)
> **父 change**: `cpptlm-abi-slimming` (Phase 9+ 一级精简 22→18, ✅ Accepted)
> **修订版路径**: 18 → **15 函数 + 1 宏** (修订后, open/close 保留)

---

## 文件清单 (修订版)

| 文件 | 变化 (修订版) | 说明 |
|------|--------------|------|
| `include/abi/cpptlm_emulator.h` | 改 | 删除 3 函数声明 (`create_by_id`, `get_adapter_info`, `get_version`); **保留 `open/close` 声明**; 确认 `CPPTLM_EMULATOR_VERSION_STRING` 宏 |
| `src/abi/cpptlm_emulator.cc` | 改 | 删除 3 函数实现; `open` 内部调 `create` + 句柄封装 (因 `create_by_id` 已删); 更新版本注释 "15 functions + 4 callbacks + 1 macro" |
| `test/test_cpptlm_emulator_abi.cc` | 改 | 移除 `get_version` 签名测试 (2 处) |
| `test/test_cpptlm_emulator_handle_helpers.hh` | 改 | RAII helper 改用 `create("profile_path")` (1 处) |
| `test/test_cpptlm_emulator_registry.cc` | 改 | 3 处 `create_by_id(0)` → `create("profile_path")` |
| `test/test_dgpu_board_shell_full_abi.cc` | 改 | 4 处 `create_by_id(N)` → `create("profile_path_N")`; 移除 `get_version` 调用 (1 处) |
| `test/test_cpptlm_emulator_abi_slimming.cc` | 改 | 移除 `get_version` / `get_adapter_info` 引用 (2 处) |
| `test/test_dgpu_adapter_info.cc` | 改/删 | 整个文件改写测试 `get_device_info` |
| `test/test_cpptlm_emulator_abi_secondary_slimming.cc` | 新 | `[abi-secondary-slimming]` 标签测试 |
| `test/CMakeLists.txt` | 改 | 添加 `test_cpptlm_emulator_abi_secondary_slimming.cc` |
| `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` | 新 | 跨仓契约镜像 (P5-2) |
| `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` | 已存在 (修订版) | 架构决策 (P5-1 已提交) |
| `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` | 改 (已加 ## Status Update) | 标注后续 ADR-SOC-20 二级精简 (P5-1 已提交) |
| `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` | 改 (已修订) | §9.3 ABI 状态 18 → 15 (P5-1 已提交) |
| `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` | 已存在 (修订版) | P5 实施阶段文件 (P5-1 已提交) |
| `docs/soc_arch/roadmap/phase9-post-phase8-roadmap.md` | 改 (已同步) | 6 梯队 + P5 修订版引用 (P5-1 已提交) |

**总测试迁移处数**: ~9 处 (修订版, open/close 相关测试零修改)

---

## T-P 列表 (8 步对应 P5-1..P5-8)

### P5-1: OpenSpec change 创建 + ADR/路线图同步

| 子任务 | 详情 | 状态 |
|--------|------|:----:|
| ✅ 1 | 创建本 change 目录 `openspec/changes/cpptlm-abi-secondary-slimming/` (slug name per openspec 1.4.1 命名规则) | ✅ |
| ✅ 2 | 写 `proposal.md` (修订版: 18→15+宏, open/close 保留, 6 场景风险评估) | ✅ |
| ✅ 3 | 写 `design.md` (修订版: 详细实现路径, TDD 5 步, 修订前后对比表) | ✅ |
| ✅ 4 | 写 `specs/cpptlm-emulator-abi/spec.md` (REMOVED 2 Requirements + ADDED 1 Requirement + MODIFIED 1 Requirement) | ✅ |
| ✅ 5 | 写本 `tasks.md` (8 步对应 P5-1..P5-8) | ✅ |
| ⏳ 6 | 跑 `openspec validate cpptlm-abi-secondary-slimming --strict` → PASS | ⏳ (本步骤 4 验证) |

**验收**:
- ✅ 5 文件创建完成
- ⏳ `openspec validate --strict` PASS (Step 4)

### P5-2: HSK-12 跨仓契约镜像

| 子任务 | 详情 |
|--------|------|
| ⏳ 1 | 写测试: `grep -E "^### 3.1\|## 4\.\|\bcreate_by_id\b" docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` 应失败 (文件不存在) |
| ⏳ 2 | 跑 → **FAIL** (确认文件不存在) |
| ⏳ 3 | 创建 `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` (15 函数清单 + 3 删除项迁移指南 + 1 宏迁移) |
| ⏳ 4 | 跑 → **PASS** (grep 应匹配 3+ 个删除项的迁移指南引用) |
| ⏳ 5 | 推迟 commit |

**验收**:
- HSK-12 包含 15 函数完整清单 (修订版, 含 open/close)
- HSK-12 §3 包含 3 个删除项的具体调用方与迁移代码示例
- HSK-12 §4 包含 `CPPTLM_EMULATOR_VERSION_STRING` 宏迁移
- HSK-12 §5 包含 Hub 侧 4 种响应回退策略

### P5-3: Hub ack 异步跟踪 (修订版)

| 子任务 | 详情 |
|--------|------|
| ⏳ 1 | 提交 ADR-088 §D5 Status Update PR (含 **3 函数**移除清单, 修订版) |
| ⏳ 2 | Hub ack 异步跟踪 10 工作日窗口 (4 种响应之一) |
| ⏳ 3 | **超时 fallback**: 假定 Hub 已自行移除 (driver 不可见), 继续推进 |

**验收**:
- Hub 侧 PR/issue 已提交
- 10 工作日窗口未到不阻塞主线

### P5-4: 测试迁移 (4 文件 ~9 处, 修订版) ⭐ 关键路径

| 子任务 | 详情 | 处数 |
|--------|------|:---:|
| ⏳ 1 | 写测试: `cmake --build build -j$(nproc) 2>&1 \| grep -E "create_by_id\|get_adapter_info\|get_version"` 应失败 (编译错误) | - |
| ⏳ 2 | 跑 → **FAIL** (确认编译错误, call-site 残留) | - |
| ⏳ 3.1 | 迁移 `test/test_cpptlm_emulator_abi.cc`: 移除 `FnGetVersion` 签名测试 | 2 |
| ⏳ 3.2 | 迁移 `test/test_cpptlm_emulator_handle_helpers.hh`: RAII helper 改用 `create("profile_path")` | 1 |
| ⏳ 3.3 | 迁移 `test/test_cpptlm_emulator_registry.cc`: 3 处 `create_by_id(0)` → `create("profile_path")` | 3 |
| ⏳ 3.4 | 迁移 `test/test_dgpu_board_shell_full_abi.cc`: 4 处 `create_by_id(N)` → `create()`; 移除 `get_version` 调用 | 5 |
| ⏳ 3.5 | 迁移 `test/test_cpptlm_emulator_abi_slimming.cc`: 移除 `get_version` / `get_adapter_info` 引用 | 2 |
| ⏳ 3.6 | 迁移 `test/test_dgpu_adapter_info.cc`: 整个文件改写测试 `get_device_info` | 全文件 |
| ⏳ 4 | 跑 → **PASS** (`cmake --build build -j$(nproc)` + grep call-site 0 命中) | - |
| ⏳ 5 | 推迟 commit |

**修订版关键**: `cpptlm_emulator_open` / `cpptlm_emulator_close` 相关测试**零修改**

**验收**:
- ✅ 4 文件 ~9 处 call-site 全部迁移
- ✅ `cmake --build build -j$(nproc)` PASS
- ✅ grep call-site 0 命中被删函数 (排除 ABI 定义文件)

### P5-5: ABI 头/实现修改 (修订版, 保留 open/close)

| 子任务 | 详情 |
|--------|------|
| ⏳ 1 | 写测试: `g++ -I include/abi /tmp/test_abi_slim.cpp -o /tmp/test_abi_slim` 应失败 (被删函数符号不存在) |
| ⏳ 2 | 跑 → **FAIL** (确认编译错误) |
| ⏳ 3.1 | `include/abi/cpptlm_emulator.h`: 删除 3 函数声明 (`create_by_id`, `get_adapter_info`, `get_version`); **保留 `open/close` 声明** |
| ⏳ 3.2 | `src/abi/cpptlm_emulator.cc`: 删除 3 函数实现; **`open` 内部调 `create` + 句柄封装** (而非 `create_by_id`); 更新版本注释 "15 functions + 4 callbacks + 1 macro" |
| ⏳ 3.3 | `CPPTLM_EMULATOR_VERSION_STRING` 宏确认 (line 24 已定义) |
| ⏳ 4 | 跑 → **PASS** (`g++ /tmp/test_abi_keep.cpp` 编译通过, 15 函数 + open/close 仍可用) |
| ⏳ 5 | 推迟 commit |

**验收**:
- ✅ `include/abi/cpptlm_emulator.h` 剩余 **15 函数 + 1 宏 + 4 callback typedef** (修订版)
- ✅ `cpptlm_emulator_open` / `cpptlm_emulator_close` 仍可用 (修订版保留)
- ✅ `cpptlm_emulator_t` 结构体零修改
- ✅ 4 callback typedef 零修改

### P5-6: 新增 `[abi-secondary-slimming]` 标签测试 (修订版: 合并实现)

| 子任务 | 详情 |
|--------|------|
| ✅ 1 | 修订版决策: 不创建独立 `test_cpptlm_emulator_abi_secondary_slimming.cc`, 改为合并到现有 `test_cpptlm_emulator_abi_slimming.cc` (per WU-4 实施) |
| ✅ 2 | 修订版 tag 验证: `test_cpptlm_emulator_abi_slimming.cc:24` 已用 tag `[abi-slimming][abi-secondary-slimming][pcie]` |
| ✅ 3 | `test/CMakeLists.txt` 无需修改 (file GLOB 自动发现 test_*.cc) |
| ✅ 4 | 跑 → **PASS** (`./build/bin/cpptlm_tests "[abi-secondary-slimming]"` → 7 assertions, 1 test case) |
| ⏳ 5 | 推迟 commit |

**修订版决策理由** (per ADR-SOC-20 §2.1 修订版路径 + 一致性原则):
1. **演进连续性**: 一级精简 (22→18) + 二级精简 (18→15+宏) 是 ABI 连续演进, 单一测试文件符合演进叙事
2. **避免碎片化**: 独立文件会导致测试维护成本倍增 (修订版/初版/未来轮次各自一份)
3. **tag 机制天然支持**: Catch2 `[abi-secondary-slimming]` tag 单独可过滤, 不需要独立文件
4. **W3 验收达成**: G8 `[abi-secondary-slimming] 新增测试 PASS` 通过合并实现满足

**验收**:
- ✅ `[abi-secondary-slimming]` 标签测试 PASS (7 assertions, 1 test case)
- ✅ 验证 3 已删 + 15 仍可用 + 1 宏值正确 (合并到 test_cpptlm_emulator_abi_slimming.cc)
- ✅ 与 ADR-SOC-20 修订版路径一致 (不创建独立文件, 减少维护成本)

### P5-7: 全量回归

| 子任务 | 详情 |
|--------|------|
| ⏳ 1 | 跑 `cmake --build build -j$(nproc)` → 退出码 0 |
| ⏳ 2 | 跑 `ctest --test-dir build --output-on-failure -j4` → CTest 全量 PASS |
| ⏳ 3 | 跑 `./build/bin/cpptlm_tests "[pcie]"` → 既有 ≥66,564 assertions 零回归 |
| ⏳ 4 | 跑 `./build/bin/cpptlm_tests "[abi-secondary-slimming]"` → 新增标签 PASS |
| ⏳ 5 | 推迟 commit |

**验收**:
- ✅ 4 项测试套件全部 PASS
- ✅ 既有 `[pcie]` 测试零回归

### P5-8: Archive + Status Update 同步

| 子任务 | 详情 |
|--------|------|
| ⏳ 1 | 跑 `openspec archive cpptlm-abi-secondary-slimming --yes` |
| ⏳ 2 | 更新 `ADR-SOC-18-cpptlm-abi-slimming.md` Status Update 段 (修订版引用, 已提交) |
| ⏳ 3 | 更新 `ADR-SOC-20-cpptlm-abi-secondary-slimming.md` Status Update 段 (✅ Accepted) |
| ⏳ 4 | 更新 `phase9-p5-secondary-slimming.md` Status Update 段 (✅ 完成) |
| ⏳ 5 | 跑 `openspec list --json \| jq '.changes[] \| select(.name == "cpptlm-abi-secondary-slimming")'` → status=archived |
| ⏳ 6 | 最终聚合 commit: `feat(abi-slim): P5 ABI 二级精简 18→15+宏 (修订版, 保留 open/close)` |

**验收**:
- ✅ OpenSpec change 已 archive
- ✅ ADR-SOC-18/20 Status Update 段已追加
- ✅ phase9-p5 Status Update 段已追加
- ✅ 聚合 commit 已提交

---

## Acceptance Gate (修订版, 11 项)

| Gate | 验证 | 步骤 |
|------|------|:---:|
| **G1** | `openspec validate cpptlm-abi-secondary-slimming --strict` PASS | P5-1 |
| **G2** | `grep -rn "cpptlm_emulator_create_by_id\|cpptlm_emulator_get_adapter_info\|cpptlm_emulator_get_version" src/ include/ test/` = **0 匹配** (排除 ABI 定义文件) | P5-4 |
| **G3** | `include/abi/cpptlm_emulator.h` 剩余 **15 函数 + 1 宏 + 4 callback typedef** (`open/close` 保留) | P5-5 |
| **G4** | 15 函数签名零修改 (与父 spec.md §G7 字节比对) | P5-5 |
| **G5** | `CPPTLM_EMULATOR_VERSION_STRING` 宏值正确 (版本号 "v1.0-dgpu-v0") | P5-5 |
| **G6** | `cpptlm_emulator_t` 结构体零修改 | P5-5 |
| **G7** | 既有 `[pcie]` 测试零回归 (≥66,564 assertions PASS) | P5-7 |
| **G8** | `[abi-secondary-slimming]` 新增测试 PASS | P5-6 |
| **G9** | Hub (UsrLinuxEmu) **2 处**调用点删除 PR 合并 (`create_by_id` + `get_adapter_info`; `get_version` 可选) | P5-3 |
| **G10** | ADR-SOC-18 Status Update 段已追加 (修订版引用, 已提交) | P5-1 |
| **G11** | **`open/close` 调用点零修改** (修订版, driver 功能影响零) | P5-5 |

---

## 风险与缓解 (修订版)

| Risk | 等级 | 缓解 |
|------|------|------|
| 真实驱动依赖被删函数 | 🟢 低 (修订版) | 仅删除 2 真冗余函数; 实测 ~12 核心 ABI 远超 15 |
| Hub ack 在 archive 前到达 | 🟢 低 (修订版 2 函数级 vs 初版 5 函数级) | HSK-12 标注 "本 change 与父 cpptlm-abi-slimming 互斥" |
| 测试文件调用漏改 (call-site 残留) | 🟡 中 | G2 grep 门禁 + G7 既有测试零回归覆盖 |
| `open()` 内部仍调 `create_by_id()` 编译失败 | 🟡 中 | P5-5 step 3.2 `open` 内部实现调整为调 `create` + 句柄封装 |
| `get_version` 改宏后 driver 仍调函数 | 🟢 低 | 函数实现已删, 驱动编译失败 → 驱动升级到宏 |
| ABI 版本号遗漏 BUMP | 🟢 低 | G3 + G4 字节比对 |

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — ABI 二级精简 (修订版, 保留 open/close) 推进中
**关键路径**:
1. Hub 侧异步 ack (修订版 2 函数级, 风险降低)
2. D0+ 创建本 change proposal + spec (本任务 P5-1)
3. D1-2 迁移 4 文件 ~9 处 call-site (P5-4)
4. D3-5 删除 3 ABI 函数 + 改 `get_version` 宏 + 调整 `open` 内部实现 (P5-5)
5. D5-6 新增 `[abi-secondary-slimming]` 标签测试 (P5-6)
6. D6-7 全量回归 + openspec validate --strict (P5-7)
7. D8+ archive 本 change (P5-8)

---

## 关联

- 父 change: `openspec/changes/archive/cpptlm-abi-slimming/` (✅ Accepted 22→18)
- 主 ADR: `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` (修订版)
- 父 ADR: `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` (Status Update 已追加)
- 路线图: `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` (修订版)
- 实施计划: `.rddf/plans/cpptlm-abi-secondary-slimming.md` (修订版 TDD 5 步)
- 关联 HSK: `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` (待 P5-2 创建)
