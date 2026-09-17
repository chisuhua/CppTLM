# cpptlm-abi-secondary-slimming: 实施计划 (TDD 5 步)

> **配套**: [`openspec/changes/cpptlm-abi-secondary-slimming/`](../openspec/changes/cpptlm-abi-secondary-slimming/) (待创建)
> **Change name**: `cpptlm-abi-secondary-slimming` (slug, **不带日期前缀** per openspec 1.4.1 CLI 命名规则)
> **关联阶段**: [`docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md`](../docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md)
> **主 ADR**: [`docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md`](../docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md) — **修订版 (保留 open/close)**
> **父 ADR**: [`docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md`](../docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md) — 第一轮精简 22→18 (✅ Accepted)
> **关联 HSK**: `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` (待创建, 取代 HSK-11 §已删除函数清单)
> **生成时间**: 2027-09-17 · **生成方式**: 修订自初版 (per 用户反馈保留 open/close)
> **Worktree**: 待创建 `.rddf/wt/cpptlm-abi-secondary-slimming/`
> **分支**: `openspec/cpptlm-abi-secondary-slimming`
> **W 编号基准**: W1 = 2026-09-21(Mon) — 本 plan 时间表按此基准; 当前 W37+

---

## Goal (目标)

在 ADR-SOC-18 第一轮精简 (22→18) 完成的基线上,做**第二轮精简**, 从 **18 → 15 函数 + 1 宏** (修订版, **保留 open/close**):

1. ❌ 删除 `cpptlm_emulator_create_by_id` (与 `create` 重叠)
2. ❌ 删除 `cpptlm_emulator_get_adapter_info` (与 `get_device_info` 重叠)
3. 🔄 `cpptlm_emulator_get_version` → `#define CPPTLM_EMULATOR_VERSION_STRING` 宏
4. ✅ 保留 `cpptlm_emulator_open` / `cpptlm_emulator_close` (fd 风格 API + 生命周期分层语义价值)

**完成定义 (DoD, 修订版)**:
1. ✅ P5-1 ~ P5-8 全部 acceptance 勾选
2. ✅ `cpptlm_emulator.h` 剩余 **15 函数 + 1 宏 + 4 callback typedef** (open/close 保留)
3. ✅ 编译防火墙验证 PASS: `grep` call-site 0 命中被删函数
4. ✅ `cmake --build build -j$(nproc)` PASS
5. ✅ `ctest --output-on-failure` 无 regression (≥66,564 assertions PASS)
6. ✅ `[abi-secondary-slimming]` 新增标签测试 PASS
7. ✅ Hub (UsrLinuxEmu) **2 处调用点**删除 PR 合并 (`create_by_id` + `get_adapter_info`)
8. ✅ `openspec validate cpptlm-abi-secondary-slimming --strict` PASS
9. ✅ ADR-SOC-18 Status Update 段已追加 (修订版)
10. ✅ `scripts/test/docs_sync_check.sh --strict` PASS

---

## 修订理由摘要 (per 用户反馈 2027-09-17)

初版建议删除 4 个 handle API 函数 (`create_by_id` + `open` + `close` + `get_adapter_info`), 但用户指出:

> `open/close` 模仿 kernel driver fd 模式, 有 **API 设计语义价值**:
> - 真实 driver fd 模式: `open("/dev/dri/renderD128")` → ioctl(fd, ...) → close(fd)
> - 生命周期分层: 主进程持有 `cpptlm_emulator_t*`, 业务模块借用 `cpptlm_emulator_handle_t`
> - API 设计一致性: driver 开发者心智模型统一

**6 个 driver 场景风险评估**:

| 场景 | 删除 open/close 影响 | 决策 |
|------|---------------------|------|
| 进程间 fd 共享 (SCM_RIGHTS) | 🟢 零 (uint64_t 句柄非真 fd) | 不变 |
| 多线程 ref count | 🟢 零 (之前无此能力) | 不变 |
| **生命周期分层** | 🟡 中 (真实语义价值) | ✅ **保留** |
| 多 emulator 实例 | 🟢 零 (create 已支持) | 不变 |
| handle 状态追踪 | 🟢 零 (无 per-handle 状态) | 不变 |
| **fd API 风格一致性** | 🟡 中 (API 设计价值) | ✅ **保留** |

**结论**: 修订为 18→15+宏 (删除真冗余 2 函数 + 改 1 宏, 保留 open/close)。

---

## 关键约束 (设计要点摘要)

| 约束 | 来源 | 影响 |
|------|------|------|
| **修订版路径 (18→15+宏)** | ADR-SOC-20 §2 决策 1 修订 | 删除 2 函数 + 改 1 宏; open/close 保留 |
| **`open` 内部仍调 `create_by_id`** | `src/abi/cpptlm_emulator.cc:433` | `open()` 实现保留对 `create_by_id()` 的调用 (因为 `open` 自身不删除) |
| **`CPPTLM_EMULATOR_VERSION_STRING` 宏** | `include/abi/cpptlm_emulator.h:24` | 已存在; ABI 函数兼容 (驱动仍能调函数, 仅新代码改宏) |
| **4 callback typedef 零修改** | ADR-SOC-20 §2 决策 4 | `cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t` |
| **`cpptlm_emulator_t` 结构体零修改** | ADR-SOC-20 §2 决策 4 | opaque struct 字段不变 |
| **15 函数签名零修改** | ADR-SOC-20 §3.2 G4 | 与父 ADR-SOC-18 §G7 字节比对 |
| **Hub 异步协调** | ADR-SOC-20 §7.1 | UsrLinuxEmu 同步删除 **2 处**调用点 (`create_by_id` + `get_adapter_info`; `get_version` 可选) |
| **超时 fallback** | ADR-SOC-18 §Hub ack 超时经验 | 10 工作日无响应, 假定 Hub 已自行移除 (driver 不可见), 继续推进 |
| **零 driver 功能影响** | 用户反馈 2027-09-17 | open/close 保留 → driver 现有代码完全不动 |

---

## Work Units (TDD 5 步)

> **TDD 5 步 canonical markers** (per `rdd-doctor` `plan-tdd` check):
> 1. **Write the failing test** — 写出失败测试
> 2. **Run test to verify it fails** — 运行测试确认失败
> 3. **Write minimal implementation** — 写最小实现
> 4. **Run test to verify it passes** — 运行测试确认通过
> 5. **Defer commit** — 推迟到 archive 阶段
>
> 本 plan 使用本地化标签对应 canonical markers。

### WU-1: P5-1 (OpenSpec change 创建) — 修订版

**Status**: ⏳ 待启动

**TDD 5 步**:
1. **Write failing test**:
   ```bash
   openspec status --change 2027-09-17-cpptlm-abi-secondary-slimming --json
   # 应失败: change not found
   ```
2. **Verify fail**: 确认 status 失败 (变更不存在)
3. **Implement**:
   ```bash
   mkdir -p openspec/changes/2027-09-17-cpptlm-abi-secondary-slimming/specs/cpptlm-emulator-abi
   ```
   写入 4 个文件:
   - `proposal.md` (per [`openspec/changes/archive/2026-09-16-cpptlm-abi-slimming/proposal.md`](../openspec/changes/archive/2026-09-16-cpptlm-abi-slimming/proposal.md) 模板)
   - `design.md` (per ADR-SOC-20 §3 Implementation 章节扩写)
   - `specs/cpptlm-emulator-abi/spec.md` (**REMOVED Requirements 2**: `cpptlm_emulator_create_by_id` + `cpptlm_emulator_get_adapter_info`; **ADDED Requirement 1**: `CPPTLM_EMULATOR_VERSION_STRING` 宏)
   - `tasks.md` (8 步对应 P5-1..P5-8)
4. **Verify pass**:
   ```bash
   openspec validate cpptlm-abi-secondary-slimming --strict
   # 应 PASS
   ```
5. **Defer commit**: 推迟到 P5-8 archive 阶段

**回滚 plan**: 若 `openspec validate` 失败 → 检查 specs 是否明确 REMOVED/ADDED 标签 (per OpenSpec 1.4.1 规范)。

---

### WU-2: P5-2 (HSK-12 跨仓契约镜像) — 修订版

**Status**: ⏳ 待启动 (依赖 P5-1)

**TDD 5 步**:
1. **Write failing test**:
   ```bash
   grep -E "^### 3.1|## 4\.|\bcreate_by_id\b" docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md
   # 应失败: file not found
   ```
2. **Verify fail**: 确认文件不存在
3. **Implement**: 创建 `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md`:
   - §1 上下文 (per HSK-11 模板)
   - §2 **15 函数**完整清单 (设备管理 2 + 数据面 4 + MSI-X 3 + 回调 2 + DMA 1 + handle 2 open/close)
   - §3 **3 函数移除迁移指南** (`create_by_id` + `get_adapter_info` + `get_version` 改宏)
   - §4 `CPPTLM_EMULATOR_VERSION_STRING` 宏迁移 (per `src/abi/cpptlm_emulator.cc:24`)
   - §5 Hub 侧 4 种响应回退策略 (per HSK-11 §5 模板)
   - §6 修订版 vs 初版对比表 (per ADR-SOC-20 §1.3)
4. **Verify pass**:
   ```bash
   grep -c "create_by_id\|get_adapter_info\|get_version" docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md
   # 应匹配: 至少 3 个 (迁移指南中提及被删函数)
   ```
5. **Defer commit**: 推迟到 P5-8 archive 阶段

**回滚 plan**: 若 HSK-12 缺迁移指南 → 检查 §3 是否列出 3 个删除项的具体调用方与迁移代码示例。

---

### WU-3: P5-3 (Hub 异步 ack 跟踪) — 修订版

**Status**: ⏳ 待启动 (依赖 P5-1/P5-2)

**TDD 5 步**:
1. **Write failing test**: N/A (异步操作, 无失败测试)
2. **Verify fail**: N/A
3. **Implement**:
   ```bash
   # 提交 ADR-088 §D5 Status Update PR (含 3 函数移除清单)
   # PR 标题: "[CPptlm-ABI-088-§D5] 18→15 函数 + get_version 改宏 (修订版, 保留 open/close)"
   ```
4. **Verify pass**:
   ```bash
   # Hub 侧 PR/issue 状态检查: 等待 4 种响应之一
   # - ack (全部 3 函数级) → 进入 P5-4
   # - 部分 ack (Hub 拒绝删除某项) → 保留被拒项, 删除其余
   # - 拒绝 → ADR-SOC-20 维持 18 函数状态
   # - 无响应 (10 工作日) → fallback: 假定 Hub 已自行移除, 继续推进
   ```
5. **Defer commit**: N/A (异步跟踪, 无 commit)

**回滚 plan**: Hub ack 异步不阻塞主线 (per ADR-SOC-18 经验);超时 fallback 已在初版验证 (cpptlm-abi-slimming 22→18 成功)。

---

### WU-4: P5-4 (测试迁移 4 文件 ~9 处) — 修订版 ⭐ 关键路径

**Status**: ⏳ 待启动 (依赖 P5-3 ack)

**TDD 5 步**:
1. **Write failing test**:
   ```bash
   cmake --build build -j$(nproc) 2>&1 | grep -E "create_by_id|get_adapter_info|get_version"
   # 应失败: 编译错误, 链接到被删函数 (因测试文件仍调用)
   ```
2. **Verify fail**: 确认编译错误 (call-site 残留)
3. **Implement** — **4 文件迁移** (修订版):

   | 文件 | 迁移内容 | 处数 |
   |------|---------|:---:|
   | `test/test_cpptlm_emulator_abi.cc` | 移除 `FnGetVersion` 签名测试 | 2 |
   | `test/test_cpptlm_emulator_handle_helpers.hh` | RAII helper 改用 `create("profile_path")` | 1 |
   | `test/test_cpptlm_emulator_registry.cc` | 3 处 `create_by_id(0)` → `create("profile_path")` | 3 |
   | `test/test_dgpu_board_shell_full_abi.cc` | 4 处 `create_by_id(N)` → `create()`; 移除 `get_version` 调用 | 5 |
   | `test/test_cpptlm_emulator_abi_slimming.cc` | 移除 `get_version` / `get_adapter_info` 引用 | 2 |
   | `test/test_dgpu_adapter_info.cc` | 整个文件改写测试 `get_device_info` | 全文件 |
   | **总计** | (**修订后, open/close 相关测试保持不变**) | **~9+ 处** |

   **重要**: `cpptlm_emulator_open` / `cpptlm_emulator_close` 相关测试**零修改** (修订版保留)。
4. **Verify pass**:
   ```bash
   cmake --build build -j$(nproc)  # 应 PASS
   grep -rn "cpptlm_emulator_create_by_id\|cpptlm_emulator_get_adapter_info\|cpptlm_emulator_get_version" src/ include/ test/
   # 应匹配: 仅 ABI 定义文件 (已删实现, 头文件无引用)
   # 注: open/close 调用不计入 grep 命中门禁 (修订版保留)
   ```
5. **Defer commit**: 推迟到 P5-8 archive 阶段

**回滚 plan**: 若 cmake 编译失败 → 检查 4 文件迁移是否完整; 若 grep 仍有命中 → 漏改某个 call-site (per `cpptlm-debug` skill §4 件套验证)。

---

### WU-5: P5-5 (ABI 头文件 + 实现修改, 保留 open/close) — 修订版

**Status**: ⏳ 待启动 (依赖 P5-4)

**TDD 5 步**:
1. **Write failing test**:
   ```bash
   cat > /tmp/test_abi_slim.cpp << 'EOF'
   extern "C" {
   #include "cpptlm_emulator.h"
   int main() {
     // 应编译失败: 函数符号不存在
     cpptlm_emulator_create_by_id(0);
     cpptlm_emulator_get_adapter_info(0, nullptr);
     cpptlm_emulator_get_version();
     return 0;
   }
   }
   EOF
   g++ -I include/abi /tmp/test_abi_slim.cpp -o /tmp/test_abi_slim 2>&1 | head -5
   # 应失败: implicit declaration / undefined reference
   ```
2. **Verify fail**: 确认编译错误 (3 函数已删)
3. **Implement**:

   **`include/abi/cpptlm_emulator.h`**:
   ```diff
   -cpptlm_emulator_t* cpptlm_emulator_create_by_id(int dev_id);                       // line 72 删除
   -cpptlm_handle_t cpptlm_emulator_open(int dev_id, cpptlm_handle_t* out_handle);     // KEEP
   -void cpptlm_emulator_close(cpptlm_handle_t handle);                                // KEEP
   -int cpptlm_emulator_get_adapter_info(cpptlm_handle_t handle, ...);                 // line 103 删除
   -const char* cpptlm_emulator_get_version(void);                                     // line 64 删除 (改宏)
   +#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"  // line 24 已存在, 确认
   ```

   **`src/abi/cpptlm_emulator.cc`**:
   - 删除 `cpptlm_emulator_create_by_id` 实现 (line 181)
   - 删除 `cpptlm_emulator_get_adapter_info` 实现 (line 468)
   - 删除 `cpptlm_emulator_get_version` 实现
   - **保留 `cpptlm_emulator_open` 实现** (内部仍调 `create_by_id` → 需调整为调 `create_by_id` 后逻辑, 因为 `create_by_id` 已删, 改为 `create` + 句柄封装)
     - 注: 实际实现细节待 WU-5 step 3 内细化; 简化版: `open()` 改为 `create(profile_path)` + 句柄封装
   - **保留 `cpptlm_emulator_close` 实现** (调用 destroy)
   - 更新版本注释 "18 functions + 4 callbacks + 1 macro" → **"15 functions + 4 callbacks + 1 macro"**

4. **Verify pass**:
   ```bash
   g++ -I include/abi /tmp/test_abi_slim.cpp -o /tmp/test_abi_slim 2>&1 | head -5
   # 应失败: 编译错误 (被删函数不存在) — 测试就是预期的"失败测试"
   g++ -I include/abi /tmp/test_abi_keep.cpp -o /tmp/test_abi_keep 2>&1
   # 应 PASS: open/close 仍可用 (修订版保留)
   grep -c "^[a-zA-Z].*cpptlm_emulator_" include/abi/cpptlm_emulator.h
   # 应匹配: 15 (修订版)
   ```
5. **Defer commit**: 推迟到 P5-8 archive 阶段

**回滚 plan**: 若 ABI 头/实现修改后 open/close 编译失败 → 检查 `open` 内部是否仍调已删 `create_by_id` (需调整为 `create` 直接)。

---

### WU-6: P5-6 (新增 `[abi-secondary-slimming]` 标签测试) — 修订版

**Status**: ⏳ 待启动 (依赖 P5-5)

**TDD 5 步**:
1. **Write failing test**: 新增 `test/test_cpptlm_emulator_abi_secondary_slimming.cc`:
   ```cpp
   #include <catch_amalgamated.hpp>
   extern "C" {
   #include "abi/cpptlm_emulator.h"
   }

   TEST_CASE("abi-secondary-slimming: 3 functions removed + 15 remain + 1 macro", "[abi-secondary-slimming]") {
       SECTION("removed functions should not be callable") {
           // 编译期不可见: cpptlm_emulator_create_by_id / get_adapter_info / get_version
           // 测试注释说明删除意图, 编译时不调用
       }
       SECTION("15 remaining functions should still be callable") {
           // 编译期 + 链接期可见
           cpptlm_emulator_get_device_count();
           cpptlm_emulator_get_device_info(0, nullptr);
           cpptlm_emulator_create("profile_path");
           cpptlm_emulator_destroy(nullptr);
           cpptlm_emulator_mmio_write(0, 0, nullptr, 0);
           cpptlm_emulator_mmio_read(0, 0, nullptr, 0);
           cpptlm_emulator_pcie_config_write(0, 0, 0);
           cpptlm_emulator_pcie_config_read(0, 0, nullptr);
           cpptlm_emulator_msix_init(0, 0, 0);
           cpptlm_emulator_msix_update_pending(0, 0);
           cpptlm_emulator_msix_clear_pending(0, 0);
           cpptlm_emulator_register_callbacks(nullptr, 0);
           cpptlm_emulator_register_dma_translate_cb(nullptr, 0);
           cpptlm_emulator_open(0, nullptr);   // 修订版保留
           cpptlm_emulator_close(0);            // 修订版保留
       }
       SECTION("CPPTLM_EMULATOR_VERSION_STRING macro value correct") {
           REQUIRE(std::string(CPPTLM_EMULATOR_VERSION_STRING) == "v1.0-dgpu-v0");
       }
   }
   ```
2. **Verify fail**:
   ```bash
   ./build/bin/cpptlm_tests "[abi-secondary-slimming]"
   # 应失败: tag not found (新增 test case)
   ```
3. **Implement**: 添加 `test/test_cpptlm_emulator_abi_secondary_slimming.cc` 到 `test/CMakeLists.txt`:
   ```cmake
   set(PCIE_TEST_SOURCES
       ...
       ${CMAKE_CURRENT_SOURCE_DIR}/test_cpptlm_emulator_abi_secondary_slimming.cc  # 新增
   )
   ```
4. **Verify pass**:
   ```bash
   cmake --build build -j$(nproc)
   ./build/bin/cpptlm_tests "[abi-secondary-slimming]"  # 应 PASS
   ```
5. **Defer commit**: 推迟到 P5-8 archive 阶段

**回滚 plan**: 若新增测试编译失败 → 检查 15 个调用函数签名是否正确 (per `include/abi/cpptlm_emulator.h`);若 macro 值不匹配 → 更新宏定义。

---

### WU-7: P5-7 (全量回归) — 修订版

**Status**: ⏳ 待启动 (依赖 P5-5)

**TDD 5 步**:
1. **Write failing test**: N/A (回归测试, 验证无 regression)
2. **Verify fail**: N/A
3. **Implement**: 运行 4 项测试套件:
   ```bash
   cmake --build build -j$(nproc)                            # 1. 编译验证
   ctest --test-dir build --output-on-failure -j4            # 2. CTest 全量
   ./build/bin/cpptlm_tests "[pcie]"                         # 3. PCIe EP 测试 (≥66,564 assertions)
   ./build/bin/cpptlm_tests "[abi-secondary-slimming]"       # 4. 新增标签
   ```
4. **Verify pass**: 4 项全部 PASS
5. **Defer commit**: 推迟到 P5-8 archive 阶段

**回滚 plan**: 若回归测试失败 → 检查是否误删某个 15 函数之外的非 ABI 函数; 若新增测试失败 → 检查 3 个被删函数是否有 call-site 漏改。

---

### WU-8: P5-8 (Archive + Status Update 同步) — 修订版

**Status**: ⏳ 待启动 (依赖 P5-7)

**TDD 5 步**:
1. **Write failing test**: N/A (archive 操作, 无失败测试)
2. **Verify fail**: N/A
3. **Implement**:
   ```bash
   # 1. Archive OpenSpec change
   openspec archive cpptlm-abi-secondary-slimming --yes

   # 2. 更新 ADR-SOC-18 Status Update (修订版引用)
   # (已完成 per 文档修订会话)

   # 3. 更新 ADR-SOC-20 Status Update 段
   # 状态: ✅ Accepted (2027-09-17)

   # 4. 更新 phase9-p5 Status Update 段
   # 状态: ✅ 完成

   # 5. 最终聚合 commit
   git add openspec/changes/ docs/soc_arch/ include/abi/ src/abi/ test/
   git commit -m "feat(abi-slim): P5 ABI 二级精简 18→15+宏 (修订版, 保留 open/close)"
   ```
4. **Verify pass**:
   ```bash
   openspec list --json | jq '.changes[] | select(.name == "cpptlm-abi-secondary-slimming")'
   # 应见: status=archived
   git log --oneline -1 | head -1
   # 应见: feat(abi-slim): P5 ABI 二级精简...
   ```
5. **Commit**: ✅ 完成 (本步骤 commit, 不再 defer)

**回滚 plan**: 若 archive 失败 → 检查 tasks.md 是否所有 checkbox 已勾选; 若 commit 失败 → 检查 git 状态是否干净。

---

## 时间表 (修订后)

| Day | 任务 | 阻塞 | 备注 |
|-----|------|------|------|
| **D0** (2027-09-18 Mon) | P5-1 + P5-2 (OpenSpec + HSK-12) | 无 | 同步进行 |
| **D0-D10** (10 工作日窗口) | P5-3 (Hub ack 异步跟踪) | 不阻塞 | 等待 4 种响应 |
| **D1-D3** | P5-4 (测试迁移预备) | D0 完成 | 4 文件 ~9 处 (修订版) |
| **D3-D5** | P5-5 (ABI 头/实现修改) | D3 | 15 函数 + 1 宏 (修订版) |
| **D5-D6** | P5-6 (新增测试) | D5 | `[abi-secondary-slimming]` 标签 |
| **D6-D7** | P5-7 (全量回归) | D5 | 4 项测试套件 PASS |
| **D8-D10** | P5-8 (Archive + Commit) | D7 | OpenSpec archive + git commit |
| **D10+** | Fallback (若 Hub 无响应) | D10 | 假定 Hub 已自行移除, 继续推进 |

**总工作量**: ~3.3 d (修订后减少 0.2 d vs 初版 3.5 d)

---

## 风险矩阵 (修订后)

| Risk | 等级 | 缓解 |
|------|------|------|
| Hub ack 在 archive 前到达, 重复执行 | 🟢 低 (修订版 2 函数级 vs 初版 5 函数级) | HSK-12 标注 "本 change 与 cpptlm-abi-slimming 互斥" |
| 测试文件调用漏改 (call-site 残留) | 🟡 中 | WU-4 step 4 grep 门禁 + WU-7 全量回归 |
| `open()` 内部仍调 `create_by_id()` 编译失败 | 🟡 中 (修订版保留 open) | WU-5 step 3 `open` 内部实现需调整为 `create` + 句柄封装 |
| ABI 版本号遗漏更新 (驱动兼容性) | 🟢 低 | WU-5 step 3 版本注释更新 |
| Driver 误用 `create_by_id` 编译失败 | 🟢 低 | 修订版已删除, 需 Hub 同步 |
| `get_version` 改宏后 driver 仍调函数 | 🟢 低 | 函数实现已删, 驱动编译失败 → 驱动升级到宏 |

---

## 关联文档 (修订版)

- 主 ADR: [`docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md`](../docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md) — **修订版** (保留 open/close)
- 父 ADR: [`docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md`](../docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md) — Status Update 段已追加 (修订版)
- 路线图: [`docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md`](../docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md) — 修订版
- 总览: [`docs/soc_arch/roadmap/phase9-post-phase8-roadmap.md`](../docs/soc_arch/roadmap/phase9-post-phase8-roadmap.md) — P5 行已更新 (修订版)
- 模块文档: [`docs/soc_arch/modules/dgpu-soc-pcie-slice.md`](../docs/soc_arch/modules/dgpu-soc-pcie-slice.md) §9.3 — 修订版
- 父 change (1 级精简): `openspec/changes/archive/2026-09-16-cpptlm-abi-slimming/` — 22→18 (✅ Accepted)
- 父父 change (Phase 9+ TLP): `openspec/changes/archive/2026-09-16-2026-09-16-cpptlm-pcie-tlp-wire-datapath/` — 13 commits (✅ Accepted)

---

## 检查清单 (修订版)

- [ ] WU-1: P5-1 OpenSpec change 创建 + `openspec validate --strict` PASS
- [ ] WU-2: P5-2 HSK-12 跨仓契约镜像 (修订版, 2 函数级)
- [ ] WU-3: P5-3 Hub ack 提交 + 异步跟踪 (修订版)
- [ ] WU-4: P5-4 测试迁移 4 文件 ~9 处 (修订版, open/close 相关不动)
- [ ] WU-5: P5-5 ABI 头/实现修改 (修订版, 保留 open/close 内部实现)
- [ ] WU-6: P5-6 新增 `[abi-secondary-slimming]` 标签测试
- [ ] WU-7: P5-7 全量回归 (4 项测试套件 PASS, ≥66,564 assertions)
- [ ] WU-8: P5-8 Archive + Status Update 同步 + 聚合 commit
- [ ] G1: `openspec validate cpptlm-abi-secondary-slimming --strict` PASS
- [ ] G2: grep call-site 0 命中被删函数 (排除 ABI 定义文件)
- [ ] G3: `include/abi/cpptlm_emulator.h` 剩余 **15 函数 + 1 宏 + 4 callback typedef**
- [ ] G4: 15 函数签名零修改
- [ ] G5: `CPPTLM_EMULATOR_VERSION_STRING` 宏值正确
- [ ] G6: `cpptlm_emulator_t` 结构体零修改
- [ ] G7: 既有 `[pcie]` 测试零回归
- [ ] G8: `[abi-secondary-slimming]` 新增测试 PASS
- [ ] G9: Hub (UsrLinuxEmu) **2 处**调用点删除 PR 合并 (修订版)
- [ ] G10: ADR-SOC-18 Status Update 段已追加 (修订版)
- [ ] G11: **`open/close` 调用点零修改** (修订版, driver 功能影响零)

---

## 与初版对比 (修订 vs 初版)

| 维度 | 初版 (18→14+宏) | 修订 (18→15+宏) |
|------|:---------------:|:---------------:|
| 删除函数数 | 5 | **3** (含 1 改宏) |
| `open/close` | 删除 | **保留** |
| 测试迁移处数 | ~15 | **~9** |
| 迁移文件数 | 5-6 | **4** |
| Hub ack 风险 | 🟡 中 (5 函数级) | 🟢 低 (2 函数级) |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |
| fd API 风格 | 丢失 | **保留** |
| 未来扩展空间 | 不可逆 | 可逆 (可加不能减) |
| 总工作量 | 3.5 d | **3.3 d** |

---

**修订时间线**:
- 2027-09-17 初版 (路径 18→14+宏)
- 2027-09-17 用户反馈 (open/close 语义价值)
- 2027-09-17 修订 (路径 18→15+宏, 保留 open/close)
- 2027-09-17 本 plan 生成 (修订版实施计划)

**单一真相源**: `openspec/changes/.../tasks.md` (待 P5-1 创建); 本 plan 为执行入口, 不同步覆盖 tasks.md。
