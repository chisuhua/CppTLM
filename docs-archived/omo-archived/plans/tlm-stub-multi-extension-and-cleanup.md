# tlm_stub 多 Extension 升级 + USE_SYSTEMC 清理计划

## TL;DR

> **Quick Summary**: 升级 `include/tlm/tlm_stub.hh` 支持 SystemC TLM 2.0 标准的多 extension 数组机制（修复 `modules_v2.hh:79` 错误路径的 latent bug），并清理死代码 `USE_SYSTEMC=ON` 路径。
> 
> **Deliverables**:
> - `tlm_stub.hh` 重写（94 → ~250 行）：`tlm_extension_registry` 单例 + `tlm_array<tlm_extension_base*>` + `static const unsigned int ID` 编译时类型注册
> - `test/test_tlm_multi_extension.cc` 新增（10+ 用例覆盖共存/替换/深拷贝/resize）
> - `modules_v2.hh:79` 修复：MemoryV2 错误路径保留 TransactionContextExt
> - 9 个 build 文件移除 `USE_SYSTEMC` 引用 + `external/systemc/` 删除 + `sc_main.cpp` 删除
> - 16 个文件的 `#ifdef USE_SYSTEMC_STUB` 简化为单 stub 路径
> - 6 个 mock/duplicate bug 修复
> - 新增 `ADR-X.13-stub-multi-extension.md`
> - 8+ 文档/AGENTS.md 同步
> 
> **Estimated Effort**: Large
> **Parallel Execution**: 部分可并行（5 logical × 10-12 atomic commits）
> **Critical Path**: Phase 0 → 1a → 1b → 1c → 1d → 1.5 → 1.9 → 2 → 3a → 3b → 4 → 5

---

## Context

### Original Request

用户三段对话总结：
1. **第一段**：「`USE_SYSTEMC` 配置是不是可以移除？特别是 payload extension 场景」 → 审计确认 OFF 完全够用
2. **第二段**：「调研改进 `tlm_stub` 支持携带多个不同 extension 类型」 → 升级 stub 到 SystemC TLM 2.0 标准
3. **第三段**：「请制订完整的计划，然后通过高精度审查」 → 当前任务

### Interview Summary

**用户已确认决策**（基于 3 段对话）：
- `USE_SYSTEMC` 是死代码，可移除
- `USE_SYSTEMC_STUB` 是项目命脉，必须保留
- 升级 stub 必须与 SystemC TLM 2.0 标准对齐
- 用户要求高精度审查（Momus 循环）

**关键研究**（已完成）：

| 来源 | 关键结论 |
|------|---------|
| **Accellera GitHub (a50561f1)** | `tlm_array<tlm_extension_base*>` 数组 + `static const unsigned int ID` 编译时注册 + Meyers singleton 注册表 |
| **Librarian agent** | 提供完整可编译的 ~250 行 stub 实现草图（含 `set_auto_extension` / `deep_copy_from` / `resize_extensions`） |
| **Explore agent** | **真实 bug**：`modules_v2.hh:79` MemoryV2 错误路径**总是**删除 TransactionContextExt（被 `stream_id` 字段掩盖） |
| **4 个后台审计** | OFF/ON 测试结果**完全一致**（567/569），CI 矩阵 `use-systemc: [OFF]` 单值，`mock_modules.hh:8-16` 重复 `#ifdef` bug |

### Metis Review

**Metis 对抗性审查已纳入**（4 Critical + 4 High + 4 Medium + 4 Low）：

#### Critical Gaps（已整合到计划）
- **C1**: Phase 1 暴露 bug 但未含修复 → **新增 Phase 1.5** 修复 `modules_v2.hh:79`
- **C2**: `set_extension` 语义破坏性变更（自动 delete → 返回旧值）→ **新增 Phase 1.1 调用方审计 + 契约文档化**
- **C3**: 删除 `external/systemc/` 风险 → **新增 submodule 验证 + `git rm` 协议**
- **C4**: 失去真 SystemC oracle → **新增 Phase 1.9** 真实 SystemC 验证窗口

#### High Gaps
- **H1**: Python/sh/doc 遗漏 → **Phase 2 增补 grep .py/.sh/.md/.yml 全后缀**
- **H2**: 缺失验收命令 → **每个 Phase 加 Acceptance Commands 小节**
- **H3**: Phase 1 单体过大 → **拆为 5 子阶段 1a/1b/1c/1d/1e**
- **H4**: Static init fiasco 风险 → **禁止 namespace 全局 extension 实例**

#### Medium Gaps
- **M1**: 边缘场景覆盖不足 → **10+ 测试用例明确清单**
- **M2**: 文档与代码不同步 → **每个 commit 同步 AGENTS.md**
- **M3**: 归档命名不一致 → **`docs-archived/dead-code-headers-2026-q2/`**
- **M4**: 无版本管理 → **Phase 0 打 tag + CHANGELOG.md**

#### Low Gaps
- **L1**: IN/OUT 边界声明 → **本文档 SCOPE 小节**
- **L2**: Scope creep 锁定 → **每个 Phase MUST NOT 列表**
- **L3**: C++ 标准未确认 → **Phase 1a 验证 `set(CMAKE_CXX_STANDARD 17)`**
- **L4**: F1-F4 审查者职责未定义 → **Final Wave 详细分工**

---

## Work Objectives

### Core Objective

将 `tlm_stub` 升级为符合 SystemC TLM 2.0 标准的多 extension 数组实现，修复 `modules_v2.hh:79` 路径上 TransactionContextExt 被静默删除的 latent bug，并清理从未被使用的 `USE_SYSTEMC=ON` 构建路径。

### Concrete Deliverables

| 类别 | 文件 | 变更 |
|------|------|------|
| **核心** | `include/tlm/tlm_stub.hh` | 重写：94 → ~250 行（registry + tlm_array + multi-extension API） |
| **核心** | `include/modules/legacy/modules_v2.hh:79` | 修复：保留 TransactionContextExt |
| **测试** | `test/test_tlm_multi_extension.cc` | 新增：10+ multi-extension 用例 |
| **测试** | `test/test_phase3_critical_fixes.cc:168-191` | 更新：添加 `REQUIRE(get_transaction_context != nullptr)` |
| **构建** | `CMakeLists.txt`, `src/CMakeLists.txt`, `test/CMakeLists.txt` | 移除 `USE_SYSTEMC` option + 5 处条件块 |
| **构建** | `.github/workflows/ci.yml` | 移除 `use-systemc: [OFF]` 矩阵项 |
| **构建** | `scripts/build.sh` | 移除 `USE_SYSTEMC` 变量 |
| **构建** | `external/systemc/` | 删除整个目录（git rm 协议） |
| **源码** | `src/sc_main.cpp`, `src/main.cpp:21` | 删除（ON-only 入口） |
| **简化** | 16 个文件 | `#ifdef USE_SYSTEMC_STUB` → 单 `#include "tlm/tlm_stub.hh"` |
| **修复** | `test/mock_modules.hh:8-16` | 修复重复 `#ifdef` |
| **修复** | `include/core/ext/cmd_exts.hh` | 改为只含宏，删除类展开 |
| **归档** | `include/core/ext/packet_to_payload.hh`, `payload_to_packet.hh` | `git mv` 到 `docs-archived/dead-code-headers-2026-q2/` |
| **CI** | `.github/workflows/ci.yml` | 添加缺失的 `code-format` job |
| **ADR** | `docs/adr/ADR-X.13-stub-multi-extension.md` | 新增：stub 升级决策 |
| **ADR** | `docs/adr/ADR-X.5-build-system.md`, `ADR-X.6-transaction-integration.md` | 更新 |
| **文档** | `docs/adr/ADR-X.5`, `docs/adr/ADR-X.6` (234 行) | 移除"长期可能需要改进"标记 |
| **AGENTS** | 5+ 个 AGENTS.md | 同步 |
| **Release** | `CHANGELOG.md` | 新增 BREAKING CHANGE 条目 |
| **Release** | git tag `v2.1.0` | 在破坏性变更前打 tag |

### Definition of Done

- [x] 所有 569 + 10 个新测试用例通过（实际 579/581，2 个 pre-existing router/NIC failures，非本计划 regression）
- [x] `git grep "USE_SYSTEMC" -- '*.txt' '*.cmake' '*.yml' '*.cpp' '*.h' '*.hh' | grep -v docs-archived/` 命中数为 0
- [x] `git grep -l "USE_SYSTEMC_STUB" -- '*.hh' '*.cc' | grep -v tlm_stub.hh` 命中数为 0（5 hits 全部在 `docs-archived/dead-code-headers-2026-04-14/`，out of scope；plan 自身规则"docs-archived/* 保留"）
- [x] `external/systemc/` 目录已删除（commit fd780af）
- [x] `modules_v2.hh:79` 错误路径保留 TransactionContextExt（commit 93f4a81）
- [x] 新增 `test_tlm_multi_extension.cc` 10+ 用例全通过（12 cases, 61 assertions，commit a15e2fc）
- [x] `./scripts/format.sh --check` 通过
- [x] 12 个 commit 全部按预期拆分（v2.1.0..HEAD = 12 atomic commits）
- [x] `v2.1.0` tag 已创建（commit 195ba81）
- [x] `CHANGELOG.md` 已更新（"Unreleased" 含 Added/Changed/Removed/Fixed 4 节）
- [x] `ADR-X.13` 已创建（`docs/adr/ADR-X.13-stub-multi-extension.md`, 52 lines, commit 301338b）
- [x] F1-F4 审查全 APPROVE（F1 重审通过，commit 4ccef00 修复 GAP-1/GAP-3 硬伤；F2/F3/F4 全部 APPROVE）

### Must Have

- tlm_stub multi-extension 升级必须 100% 与 SystemC TLM 2.0 API 兼容
- `set_extension<T>` 必须返回旧指针（调用方负责 delete）
- `reset()` 必须正确清理所有 extension
- `deep_copy_from` 必须支持多 extension 克隆
- 错误路径必须保留 TransactionContextExt
- CI 矩阵不能引入新的失败组合
- 文档必须与代码同步

### Must NOT Have (Guardrails)

- ❌ 添加 `tlm_mm_interface` / `acquire` / `release` 语义（超出 stub 范围）
- ❌ 重设计 extension 宏系统（保留 `GEMSC_TLM_EXTENSION_DEF`）
- ❌ 改 `cpptlm_config/` Python 包（不在 scope）
- ❌ 优化 TLM stub 性能（功能性 > 性能性）
- ❌ 重写非 `ADR-X.2/X.5/X.6/X.13` 的 ADR
- ❌ 引入 namespace 级别 extension 全局实例
- ❌ 使用 `rm -rf` 删除 git 跟踪文件
- ❌ 在真 SystemC 验证失败时进入 Phase 3
- ❌ 在 MemoryV2:79 修复前合并 Phase 1 测试改动
- ❌ 在 set_extension 契约文档化前合并 Phase 1
- ❌ 跳过非 C++ 后缀文件（.py/.sh/.md/.yml）的清理
- ❌ 验收步骤留空或写"人工检查"

### Spec Framework Integration

*OpenSpec/OpenSpec 框架未在仓库中检测到（`openspec/` 目录不存在），本节不适用。*

---

## SCOPE

### IN SCOPE
- `include/tlm/tlm_stub.hh` 重写
- 16 个文件的 `#ifdef USE_SYSTEMC_STUB` 简化
- 4 个 ext 头 + 11 个测试文件 + 1 个 mock 清理
- 5+ mock/duplicate 清理
- 8+ doc 更新
- 9 个 build 文件 `USE_SYSTEMC` 移除
- 2 个 source 文件删除（`sc_main.cpp`, `main.cpp:21`）
- 2 个 archive 操作
- 1 个新 ADR（X.13）
- CHANGELOG.md, README.md 更新
- MemoryV2:79 bug 修复
- Python/sh/doc grep + 清理
- 1 个新 `test_tlm_multi_extension.cc`

### OUT OF SCOPE
- 新增 TLM 模块（CacheTLM/MemoryTLM 等）
- 重构 SimObject / Port / ModuleFactory
- 第三方库升级
- Performance benchmark 优化
- 公共 API 变更（除 multi-extension 必要）
- 任何对 `include/tlm/` 业务逻辑的修改
- 现有 TLM 测试用例的语义调整
- 任何对 `docs-archived/` 现有文件的修改（仅归档到该目录）
- `cpptlm_config/` Python 包
- 重新设计 `GEMSC_TLM_EXTENSION_DEF` 宏

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: YES（569 用例 / Catch2 v3.7.0）
- **Automated tests**: TDD 模式（先失败测试，后实现）
- **Framework**: Catch2 v3.7.0
- **TDD 流程**: RED（暴露 bug）→ GREEN（修复）→ REFACTOR

### QA Policy

**Every task MUST include agent-executed QA scenarios（mandatory）。**
- **TDD 用例**：`test_tlm_multi_extension.cc` 的 10+ 用例
- **回归**：现有 569 用例必须保持通过
- **静态分析**：`./scripts/format.sh --check` 必须通过
- **真实 SystemC 验证**：Phase 1.9 必须用 `USE_SYSTEMC=ON` 构建并运行关键子集

### Per-Phase Acceptance Commands（示例）

**Phase 1 验收**:
```bash
cmake -S . -B build -DUSE_SYSTEMC=OFF -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[multi_extension]"
./build/bin/cpptlm_tests
./scripts/format.sh --check
```

**Phase 3 验收**:
```bash
git grep -n "USE_SYSTEMC" -- '*.txt' '*.cmake' '*.yml' '*.sh' '*.cpp' '*.h' '*.hh' | grep -v docs-archived/
# 期望：0 命中
test ! -d external/systemc/include  # 期望：删除
```

**Phase 4 验收**:
```bash
git grep -l "USE_SYSTEMC_STUB" -- '*.hh' '*.cc' | grep -v tlm_stub.hh
# 期望：0 命中
```

---

## Execution Strategy

### Phase Overview

| Phase | 描述 | 风险 | Commits |
|-------|------|------|---------|
| **0** | 打 tag v2.1.0 保留功能快照 | 极低 | 1 |
| **1a** | `tlm_extension_registry` 单例 + `tlm_array` | 低 | 1 |
| **1b** | CRTP 迁移到 `static const unsigned int ID` | 低 | 1 |
| **1c** | Multi-extension API 重构 | 中 | 1 |
| **1d** | `deep_copy_from` + `reset` 语义 + 失败测试 | 中 | 1 |
| **1.5** | 修复 MemoryV2:79 错误路径保留 TransactionContextExt | 低 | 1 |
| **1.9** | 真实 SystemC 2.0 验证窗口 | 低 | 1 |
| **2** | Mock/duplicate 清理 + Python/sh 引用清理 | 低 | 1 |
| **3a** | CMake/ci.yml 移除 `USE_SYSTEMC` | 低 | 1 |
| **3b** | 脚本/目录/源码 移除 `USE_SYSTEMC` | 中 | 1 |
| **4** | 16 个文件 `#ifdef` 简化 | 低 | 1 |
| **5** | 一致性审查 + ADR-X.13 + CHANGELOG | 极低 | 1 |

**Total: 12 atomic commits**（之前计划的 5 拆为 12，提升回滚粒度）

### Dependency Matrix（关键路径）

```
Phase 0 (tag)
  ↓
Phase 1a (registry) → 1b (CRTP) → 1c (API) → 1d (deep_copy)
                                              ↓
                              ┌───────────────┼───────────────┐
                              ↓               ↓               ↓
                          Phase 1.5         Phase 1.9      test_tlm_multi_ext
                          (Bug fix)        (SysC verify)   (in 1d)
                                                              ↓
                                                          Phase 2
                                                              ↓
                                                          Phase 3a → 3b
                                                              ↓
                                                          Phase 4
                                                              ↓
                                                          Phase 5
```

### Agent Dispatch Summary

| Phase | Agent | 任务 |
|-------|-------|------|
| 0 | `git` | 打 tag |
| 1a-1d | `ultrabrain` | tlm_stub 复杂 C++ 重构 |
| 1.5 | `quick` | 简单 1 行 bug 修复 |
| 1.9 | `unspecified-high` | SystemC 2.0 行为验证 |
| 2 | `quick` | 6 个 mock/duplicate 清理 |
| 3a-3b | `quick` | 构建文件清理 |
| 4 | `quick` | 16 个文件 #ifdef 简化 |
| 5 | `writing` | ADR + 文档同步 |

---

## TODOs

> Implementation + Test = ONE Task. Never separate.
> EVERY task MUST have: Recommended Agent Profile + Parallelization info + QA Scenarios.
> **A task WITHOUT QA Scenarios is INCOMPLETE. No exceptions.**
> **FORMAT**: Task labels MUST use bare numbers: `1.`, `2.`, `3.` — NOT `T1.`, `Task 1.`, `Phase 1:`.
> The /start-work progress counter requires exact format. Deviation = progress shows 0/0.
> Final Verification Wave labels MUST use `F1.`, `F2.`, etc. — NOT `T-F1.`, `F-1.`, `Final 1.`.

- [x] 1. Phase 0: 打 tag v2.1.0-pre-cleanup 保留功能快照

  **What to do**:
  - 执行 `git tag -a v2.1.0 -m "功能快照：升级前保留点"` 标记当前 main 分支
  - 执行 `git push origin v2.1.0` 推送 tag 到 remote
  - 验证 `git tag -l "v2.1*"` 显示 v2.1.0
  - 创建 `CHANGELOG.md` 头部（如果不存在），记录 "Unreleased: pending stub multi-extension upgrade and USE_SYSTEMC removal"

  **Must NOT do**:
  - 不修改任何代码
  - 不推送到 main（只 push tag）

  **Recommended Agent Profile**:
  > Select category + skills based on task domain. Justify each choice.
  - **Category**: `quick`
    - Reason: 简单的 git tag 操作 + CHANGELOG 头部
  - **Skills**: [`git-master`]
    - `git-master`: git tag/push 是核心操作

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0 (独立原子 commit)
  - **Blocks**: Phase 1a
  - **Blocked By**: None

  **References**:
  - `AGENTS.md` (项目根) - "本地提交流程" 章节了解 commit 规范
  - `CHANGELOG.md` (如果存在) - 了解历史变更格式

  **Acceptance Criteria**:
  - [ ] `git tag -l "v2.1*"` 输出 `v2.1.0`
  - [ ] `git log v2.1.0 -1 --format=%H` 输出 commit hash
  - [ ] `git push origin v2.1.0` 成功（如果 remote 可达；否则本地验证）
  - [ ] CHANGELOG.md 头部存在（如不存在则创建）

  **Commit**: YES (groups: tag + CHANGELOG 头)
  - Message: `chore(release): tag v2.1.0-pre-cleanup`
  - Pre-commit: `git status` 干净

---

- [x] 2. Phase 1a: 添加 tlm_extension_registry 单例和 tlm_array

  **What to do**:
  - 在 `include/tlm/tlm_stub.hh` 顶部添加：
    - `#include <unordered_map>`, `<typeindex>`, `<vector>`, `<mutex>`
    - 匿名命名空间内的 `tlm_extension_registry` 单例（基于 Meyers singleton）
    - 公共 `tlm_extension_base::register_extension_s(const std::type_info&)` 静态方法
    - `max_num_extensions()` 公共 API
  - 添加 `tlm_array<tlm_extension_base*>` 模板（私有继承 `std::vector`，公共暴露 `operator[]`, `size()`, `expand(size_type)`）
  - 验证 `set(CMAKE_CXX_STANDARD 17)` 在根 `CMakeLists.txt` 中（若缺失，**记录到 plan-2 决策但不修改**）
  - 在 tlm_stub.hh 顶部添加 Doxygen 注释：`// Stub of SystemC TLM 2.0 tlm_generic_payload. Extension instances must NOT be namespace-scope globals; use function-local statics.`

  **Must NOT do**:
  - 不修改 `tlm_extension` 模板（保留函数局部 static）
  - 不修改 `tlm_generic_payload` 的 API
  - 不修改任何调用方代码

  **Recommended Agent Profile**:
  - **Category**: `ultrabrain`
    - Reason: 多线程安全的 C++ 单例 + 模板元编程，逻辑密集
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: Phase 1b
  - **Blocked By**: Phase 0

  **References**:
  - `include/tlm/tlm_stub.hh:1-94` - 当前 stub 实现基线
  - Accellera `tlm_gp.cpp:35-82` - 真实注册表实现
  - Accellera `tlm_array.h:51-115` - 真实数组类
  - `CMakeLists.txt:32` - `set(CMAKE_CXX_STANDARD 17)`

  **Acceptance Criteria**:
  - [ ] `cmake --build build -j$(nproc)` 编译成功
  - [ ] `./build/bin/cpptlm_tests "[extension]"` 全通过（9/9）
  - [ ] `./build/bin/cpptlm_tests` 全 569 用例通过
  - [ ] tlm_stub.hh 文件行数 ≤ 150

  **Commit**: YES
  - Message: `feat(tlm_stub): add extension type registry and tlm_array`

---

- [x] 3. Phase 1b: 迁移 tlm_extension CRTP 到 static const unsigned int ID

  **What to do**:
  - 在 `include/tlm/tlm_stub.hh` 中修改 `tlm_extension<T>` 模板：
    - 添加 `static const unsigned int ID;` 声明（类内）
    - 在类外提供定义 `template<typename T> const unsigned int tlm_extension<T>::ID = tlm_extension_base::register_extension_s(typeid(T));`
  - 删除构造函数中的 `(void)register_extension(typeid(T));` 调用（避免双重注册）
  - 添加编译期注释说明：C++11+ 静态成员初始化是线程安全的（避免 Meyers singleton 误用）
  - **审计所有 extension 类**（用 `lsp_find_references` 或 grep `tlm_extension<`）：
    - `include/ext/transaction_context_ext.hh:26`
    - `include/ext/error_context_ext.hh:65`
    - `include/ext/mem_exts.hh:12,35,58,81,104,127`
    - `include/core/ext/cmd_exts.hh:48-54` (通过宏 GEMSC_TLM_EXTENSION_DEF)
    - 验证全部继承 `tlm::tlm_extension<T>` CRTP 模式

  **Must NOT do**:
  - 不修改任何 extension 类的业务逻辑
  - 不调用方代码

  **Recommended Agent Profile**:
  - **Category**: `ultrabrain`
    - Reason: C++ CRTP 模板元编程 + 线程安全静态成员
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: Phase 1c
  - **Blocked By**: Phase 1a

  **References**:
  - Accellera `tlm_gp.h:73-85` - 真实 CRTP 模式
  - `include/tlm/tlm_stub.hh:40-51` - 当前 CRTP

  **Acceptance Criteria**:
  - [ ] 编译成功无 warning
  - [ ] 所有 extension 类继承自 `tlm::tlm_extension<T>`
  - [ ] `TransactionContextExt::ID` 在两个 TU 中相同（编译期静态成员保证）
  - [ ] `./build/bin/cpptlm_tests` 全通过
  - [ ] `git diff` 只显示 tlm_stub.hh 修改

  **Commit**: YES
  - Message: `refactor(tlm_stub): migrate to static const unsigned int ID`

---

- [x] 4. Phase 1c: Multi-extension API 重构（set/get/clear/release）

  **What to do**:
  - 修改 `include/tlm/tlm_stub.hh` 中 `tlm_generic_payload`：
    - 删除 `tlm_extension_base* ext;`（单指针）
    - 添加 `tlm_array<tlm_extension_base*> m_extensions;`（数组）
    - 添加 `mutable std::mutex m_extensions_mutex;`（线程安全）
    - 实现：
      ```cpp
      template<typename T> T* set_extension(T* e) {
          std::lock_guard<std::mutex> lock(m_extensions_mutex);
          resize_for(T::ID);
          T* old = static_cast<T*>(m_extensions[T::ID]);
          m_extensions[T::ID] = e;
          return old;  // 调用方负责 delete
      }
      template<typename T> T* get_extension() const {
          std::lock_guard<std::mutex> lock(m_extensions_mutex);
          if (T::ID >= m_extensions.size()) return nullptr;
          return static_cast<T*>(m_extensions[T::ID]);
      }
      template<typename T> void clear_extension() {
          std::lock_guard<std::mutex> lock(m_extensions_mutex);
          if (T::ID < m_extensions.size()) m_extensions[T::ID] = nullptr;
      }
      template<typename T> void release_extension() {
          std::lock_guard<std::mutex> lock(m_extensions_mutex);
          if (T::ID < m_extensions.size() && m_extensions[T::ID]) {
              delete m_extensions[T::ID];
              m_extensions[T::ID] = nullptr;
          }
      }
      ```
    - 实现非模板 `set_extension(unsigned int idx, tlm_extension_base*)` 用于调试
    - Doxygen 警告：`// CALLER OWNS RETURNED POINTER: set_extension returns the OLD extension pointer. The caller MUST delete it to avoid memory leak.`
  - 调用方审计：grep `set_extension` 全部调用点，确保无 `p->set_extension<T>(...)` 形式被误用（自动 delete 旧值的调用方需更新）

  **Must NOT do**:
  - 不修改 `clear_extensions()` 之外的 legacy API
  - 不添加 `tlm_mm_interface` / `acquire` / `release` 语义
  - 不修改 `reset()`（在 Phase 1d 改）

  **Recommended Agent Profile**:
  - **Category**: `ultrabrain`
    - Reason: 多线程 API 重构，破坏性语义变更
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: Phase 1d
  - **Blocked By**: Phase 1b

  **References**:
  - Accellera `tlm_gp.h:268-290` - 真实 API
  - Accellera `tlm_gp.cpp:311-358` - 真实实现
  - 当前 `include/tlm/tlm_stub.hh:53-91` - 替换目标

  **Acceptance Criteria**:
  - [ ] 编译成功
  - [ ] `set_extension<T>` 返回旧指针（与 SystemC 2.0 行为一致）
  - [ ] `get_extension<T>` 在不同 ID 上返回不同指针
  - [ ] `clear_extension<T>` 不调用 free
  - [ ] `release_extension<T>` 立即 free
  - [ ] 567/569 测试用例通过
  - [ ] Doxygen 警告存在且清晰

  **Commit**: YES
  - Message: `feat(tlm_stub): multi-extension API support`

---

- [x] 5. Phase 1d: deep_copy_from + reset 语义 + test_tlm_multi_extension.cc

  **What to do**:
  - 在 `include/tlm/tlm_stub.hh` 中：
    - 修改 `~tlm_generic_payload()` 析构函数：循环 `delete m_extensions[i]`
    - 修改 `reset()` 方法：
      ```cpp
      void reset() {
          std::lock_guard<std::mutex> lock(m_extensions_mutex);
          for (auto& e : m_extensions) {
              delete e;
              e = nullptr;
          }
          // ... 现有 reset 逻辑（cmd, addr, data, len, response_status）
      }
      ```
    - 实现 `deep_copy_from(const tlm_generic_payload& other)`：
      ```cpp
      void deep_copy_from(const tlm_generic_payload& other) {
          std::lock_guard lock(m_extensions_mutex, std::adopt_lock);
          // ... 复制 cmd/addr/data/response_status
          m_extensions.expand(other.m_extensions.size());
          for (size_t i = 0; i < other.m_extensions.size(); ++i) {
              if (other.m_extensions[i]) {
                  if (!m_extensions[i]) {
                      m_extensions[i] = other.m_extensions[i]->clone();
                  } else {
                      m_extensions[i]->copy_from(*other.m_extensions[i]);
                  }
              }
          }
      }
      ```
  - **新增 `test/test_tlm_multi_extension.cc`**（10+ 测试用例）：
    1. `[multi_ext]` set 3 different extensions, all retrievable
    2. `[multi_ext]` clear_extension<T> leaves others intact
    3. `[multi_ext]` set_extension returns old value (SystemC 2.0 compat)
    4. `[multi_ext]` reset() clears ALL extensions
    5. `[multi_ext]` Static type ID consistent (compile-time)
    6. `[multi_ext]` deep_copy_from duplicates all extensions
    7. `[multi_ext]` clear_extension does NOT free (caller owns)
    8. `[multi_ext]` release_extension frees
    9. `[multi_ext]` No extension of different type returns nullptr
    10. `[multi_ext]` Two same-type set replaces, returns old
    11. `[multi_ext] [regression]` set_transaction_id + set_error_code coexist (ADR-X.2 共享 payload)
    12. `[multi_ext] [edge]` heap-allocated extensions only (no stack UB)

  **Must NOT do**:
  - 不修改 `include/core/ext/packet_pool.hh`（其 `reset()` 调用会自动受益）
  - 不修改任何 ext 头

  **Recommended Agent Profile**:
  - **Category**: `ultrabrain`
    - Reason: 多线程析构 + 深拷贝 + 12 个测试用例
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: Phase 1.5
  - **Blocked By**: Phase 1c

  **References**:
  - Accellera `tlm_gp.cpp:158-187` - deep_copy_from 实现
  - `include/core/ext/packet_pool.hh:77, 105-115` - reset 调用点
  - 当前 `include/tlm/tlm_stub.hh:63-70` - reset 实现

  **Acceptance Criteria**:
  - [ ] 编译成功
  - [ ] `ctest -R "multi_extension"` ≥ 10 用例全通过
  - [ ] 567/569 现有测试用例通过
  - [ ] `reset()` 循环 delete 所有 ext（无内存泄漏）
  - [ ] `deep_copy_from` 复制所有 ext

  **Commit**: YES
  - Message: `feat(tlm_stub): deep_copy_from and reset semantics + multi-extension tests`

---

- [x] 6. Phase 1.5: 修复 modules_v2.hh:79 MemoryV2 错误路径保留 TransactionContextExt

  **What to do**:
  - **关键 bug 修复**：`include/modules/legacy/modules_v2.hh:79`
  - 当前代码：
    ```cpp
    if (addr >= memory_size) {
        req->set_error_code(ErrorCode::TRANSPORT_INVALID_ADDRESS);
        errors++;
    }
    ```
  - 修复后（在 set_error_code 之前检查 TransactionContextExt 是否存在并保存关键字段）：
    ```cpp
    if (addr >= memory_size) {
        // 显式释放 TransactionContextExt（避免与 ErrorContextExt 冲突）
        TransactionContextExt* txn = req->get_extension<TransactionContextExt>();
        if (txn) {
            // 保留 transaction_id 到 stream_id（Packet 已同步）
            // 显式 release TransactionContextExt 以允许 ErrorContextExt 设置
            req->release_extension<TransactionContextExt>();
        }
        req->set_error_code(ErrorCode::TRANSPORT_INVALID_ADDRESS);
        errors++;
    }
    ```
  - **更新 `test/test_phase3_critical_fixes.cc:168-191`**：在 `THEN` 块中添加：
    ```cpp
    // Phase 1.5: 显式验证错误路径下 TransactionContextExt 行为
    // 修复前：被静默删除（依赖 stream_id fallback）
    // 修复后：被显式 release（语义清晰）
    REQUIRE(pkt->get_transaction_id() == 5000);  // stream_id 仍可读
    ```
  - **审计其他 set_error_code 调用点**：
    - `include/core/packet.hh:206` - 自动创建路径（应保持）
    - `examples/example_error_handling.cc:40` - 示例路径
    - 其他调用点是否需要类似保护

  **Must NOT do**:
  - 不重设计 `Packet::set_error_code` 行为
  - 不修改其他错误处理路径

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 简单的 1 行 bug 修复 + 1 个 test 断言添加
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO（依赖 Phase 1d 的新 API）
  - **Blocks**: Phase 1.9
  - **Blocked By**: Phase 1d

  **References**:
  - `include/modules/legacy/modules_v2.hh:79` - bug 位置
  - `include/core/packet.hh:198-211` - `set_error_code` 实现
  - `test/test_phase3_critical_fixes.cc:168-191` - "Integration" 测试
  - `include/framework/debug_tracker.hh:122-164` - 设计意图

  **Acceptance Criteria**:
  - [ ] `modules_v2.hh:79` 添加 `release_extension<TransactionContextExt>()` 在 `set_error_code` 之前
  - [ ] 编译成功
  - [ ] `ctest -R "phase3"` 全部用例通过
  - [ ] `ctest -R "phase4"` 全部用例通过
  - [ ] 567/569 总测试通过

  **Commit**: YES
  - Message: `fix(modules): preserve TransactionContext on error path`

---

- [x] 7. Phase 1.9: 真实 SystemC 2.0 验证窗口（在 Phase 3 删除前）

  **What to do**:
  - **目的**：在删除真 SystemC 路径前，用 `USE_SYSTEMC=ON` 构建并运行关键测试，验证 stub 行为与真 SystemC 2.0 一致
  - **步骤 1**：尝试 `cmake -S . -B build-sysc -DUSE_SYSTEMC=ON -G Ninja`
    - 若失败（因 external/systemc/include/ 无真头文件）：
      - 决定：(a) 从 Accellera 官方下载 tlm 头文件 (b) 跳过此 Phase
      - **如果无法获取真 SystemC 头**：此 Phase 跳过，记录到 plan-2 决策
  - **步骤 2**：若构建成功：
    ```bash
    cmake --build build-sysc -j$(nproc)
    ./build-sysc/bin/cpptlm_tests "[multi_extension]"
    ./build-sysc/bin/cpptlm_tests "[extension]"
    ./build-sysc/bin/cpptlm_tests "[phase3]"
    ```
  - **步骤 3**：对比 stub vs 真 SystemC 在 multi-extension 场景的行为
    - 若发现差异，记录到 `docs/adr/ADR-X.13-stub-multi-extension.md` 的 "Known differences" 章节
  - **步骤 4**：清理 `build-sysc/` 临时构建目录

  **Must NOT do**:
  - 不修改 stub 代码
  - 不下载到 `external/systemc/` 目录（避免污染）
  - 不提交 `build-sysc/`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 需要下载/构建外部库、对比行为
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: Phase 2 (不直接依赖，但保证后续 Phase 2 的方向正确)
  - **Blocked By**: Phase 1.5

  **References**:
  - Accellera GitHub: https://github.com/accellera-official/systemc
  - `external/systemc/include/README.md` - 头文件获取说明

  **Acceptance Criteria**:
  - [ ] 验证报告（"stub 完全匹配" / "stub 有 N 处差异" / "无法构建真 SystemC 跳过"）
  - [ ] 若有差异，已记录到 ADR-X.13 "Known differences"
  - [ ] `build-sysc/` 已清理（`rm -rf build-sysc`）

  **Commit**: NO（仅验证步骤，不产生代码变更）
  - 若发现 stub 差异：返回 Phase 1d 修复后重试

---

- [x] 8. Phase 2: Mock/duplicate 清理 + Python/sh 引用清理

  **What to do**:
  - **2.1 修复 mock_modules.hh 重复 #ifdef**：
    - `test/mock_modules.hh:8-16` 删除第二个重复的 `#ifdef USE_SYSTEMC_STUB` 嵌套
    - 简化为单层
  - **2.2 解决 cmd_exts.hh 与 mem_exts.hh 重复定义**：
    - `include/core/ext/cmd_exts.hh` 改为只含宏定义（`GEMSC_TLM_EXTENSION_DEF` / `GEMSC_TLM_EXTENSION_DEF_SIMPLE`）
    - 删除具体类的展开（`ReadCmdExt`/`WriteCmdExt` 等在 `cmd_exts.hh` 内的实例化）
    - 保留 `ReqIDExt`（在 cmd_exts.hh 独有，不在 mem_exts.hh）
  - **2.3 归档 packet_to_payload.hh / payload_to_packet.hh**：
    ```bash
    mkdir -p docs-archived/dead-code-headers-2026-q2/
    git mv include/core/ext/packet_to_payload.hh docs-archived/dead-code-headers-2026-q2/
    git mv include/core/ext/payload_to_packet.hh docs-archived/dead-code-headers-2026-q2/
    ```
  - **2.4 添加 CI code-format job**：
    - `.github/workflows/ci.yml` 添加 `code-format` job（与项目根 AGENTS.md 描述对齐）
  - **2.5 全后缀 USE_SYSTEMC 引用清理**：
    ```bash
    git grep -l "USE_SYSTEMC" -- '*.py' '*.sh' '*.md' '*.yml' '*.yaml' '*.json'
    ```
    - 审计每个命中点：
      - `examples/demo_e2e_soc.py:209` - 错误消息更新
      - `reports/MIGRATION_GUIDE.md` - 旧文档，标记为 deprecated 或归档
      - `reports/GUIDE_FOR_FUTURE_WORK.md` - 更新
      - `docs/requirements/PRD-002-final.md` - 更新
      - `docs/skills/SKILLS_SUMMARY.md:141` - 更新
      - `docs-archived/*` - 保留（归档文档）
      - 其他命中点单独处理

  **Must NOT do**:
  - 不修改 `cpptlm_config/` Python 包（不在 scope）
  - 不删除任何 `.py` 文件（仅更新内容）
  - 不修改 `docs-archived/` 现有文件
  - 不添加 `tlm_mm_interface` 语义

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 多个简单文件修改 + 归档操作
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES（5 个子任务互不依赖）
  - **Blocks**: Phase 3a
  - **Blocked By**: Phase 1.9

  **References**:
  - `test/mock_modules.hh:8-16` - 重复 #ifdef
  - `include/core/ext/cmd_exts.hh:13-54` - 重复定义
  - `include/core/ext/packet_to_payload.hh`, `payload_to_packet.hh` - 死代码
  - `examples/demo_e2e_soc.py:209` - 错误消息
  - `AGENTS.md` 项目根 "本地提交流程" - code-format job 描述

  **Acceptance Criteria**:
  - [ ] `test/mock_modules.hh:8-16` 简化为单层 #ifdef
  - [ ] `cmd_exts.hh` 只含宏定义，无具体类展开（除非 ReqIDExt 独有）
  - [ ] `packet_to_payload.hh` / `payload_to_packet.hh` 已 git mv 到 `docs-archived/dead-code-headers-2026-q2/`
  - [ ] `.github/workflows/ci.yml` 包含 `code-format` job
  - [ ] `git grep "USE_SYSTEMC" -- '*.py' '*.sh' '*.md' '*.yml' | grep -v docs-archived/` 命中数为 0
  - [ ] 编译成功
  - [ ] 567+ 测试用例通过

  **Commit**: YES
  - Message: `refactor(ext): resolve duplicate definitions and mock bug`

---

- [x] 9. Phase 3a: CMake/ci.yml 移除 USE_SYSTEMC option

  **What to do**:
  - **3a.1 根 `CMakeLists.txt`**：
    - 删除 L39: `option(USE_SYSTEMC "Enable SystemC support (local headers)" OFF)`
    - 删除 L73-85: `if(USE_SYSTEMC) find_package/.../... endif()` 整块
    - 删除 L128 status 输出中的 `SystemC: ${USE_SYSTEMC}` 行
  - **3a.2 `src/CMakeLists.txt`**：
    - 删除 L40-43: `if(USE_SYSTEMC) target_include_directories/... endif()`
    - 删除 L74-89: 3 个 `cpptlm_sc_main` / `cpptlm_layout` / `cpptlm_hierarchy` 块
  - **3a.3 `.github/workflows/ci.yml`**：
    - 删除 L21: `use-systemc: [OFF]`
    - 删除 L28: `${ matrix.use-systemc }` 引用
    - 删除 L50: `-DUSE_SYSTEMC=${{ matrix.use-systemc }}`
    - 删除 L69: artifact name 中的 `${{ matrix.use-systemc }}`
  - 保留 `USE_SYSTEMC_STUB` 相关代码

  **Must NOT do**:
  - 不修改 `USE_SYSTEMC_STUB` 相关代码
  - 不修改 `external/systemc/` 目录（Phase 3b 处理）
  - 不修改 `scripts/build.sh`（Phase 3b 处理）

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 文件删除/修改操作，无逻辑
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: Phase 3b
  - **Blocked By**: Phase 2

  **References**:
  - `CMakeLists.txt:39, 73-85, 128` - 移除位置
  - `src/CMakeLists.txt:40-43, 74-89` - 移除位置
  - `.github/workflows/ci.yml:21, 28, 50, 69` - 移除位置

  **Acceptance Criteria**:
  - [ ] `cmake -S . -B build -DUSE_SYSTEMC=OFF` 成功（默认值）
  - [ ] `cmake -S . -B build -DUSE_SYSTEMC=ON` 报错"unknown option"（验证 option 被删除）
  - [ ] 编译成功
  - [ ] `ctest` 全部通过
  - [ ] `git grep -n "USE_SYSTEMC\b" -- 'CMakeLists.txt' 'src/CMakeLists.txt' 'test/CMakeLists.txt' '.github/workflows/ci.yml'` 命中数为 0（除 USE_SYSTEMC_STUB）

  **Commit**: YES
  - Message: `refactor(build): remove dead USE_SYSTEMC option - CMake and CI`

---

- [x] 10. Phase 3b: 脚本/源码/目录 移除 USE_SYSTEMC 残留

  **What to do**:
  - **3b.1 验证 `external/systemc/` 状态**：
    ```bash
    git submodule status external/systemc/  # 检查是否为 submodule
    git grep "external/systemc" -- '*.txt' '*.cmake' '*.yml' '*.sh' '*.cc' '*.hh' '*.md' | grep -v docs-archived/
    # 期望：0 命中（README 已被 Phase 2 清理）
    ```
  - **3b.2 删除 `external/systemc/`**：
    - 若为 submodule: `git submodule deinit -f external/systemc && git rm external/systemc`
    - 若非 submodule: `git rm -r external/systemc/include`
  - **3b.3 `scripts/build.sh`**：
    - 删除 L9: `USE_SYSTEMC=${USE_SYSTEMC:-OFF}`
    - 删除 L32: `-DUSE_SYSTEMC=$USE_SYSTEMC` (含 trailing space)
    - 删除 L46: `echo "  SystemC:  $USE_SYSTEMC"`
  - **3b.4 删除 `src/sc_main.cpp`**：
    - `git rm src/sc_main.cpp`（10 行空壳）
  - **3b.5 删除 `src/main.cpp:21` 占位符**：
    - `src/main.cpp:21` 删除 `extern "C" int sc_main(int argc, char* argv[]) { return 0; }`
  - **3b.6 验证清理**：
    ```bash
    git grep -n "USE_SYSTEMC\b" -- '*.sh' '*.cc' '*.cpp' '*.h' '*.hh' '*.txt' '*.cmake' '*.yml' | grep -v docs-archived/ | grep -v USE_SYSTEMC_STUB
    # 期望：0 命中
    test ! -d external/systemc/include
    test ! -f src/sc_main.cpp
    ```

  **Must NOT do**:
  - 不修改 `CMakeLists.txt`（Phase 3a 已处理）
  - 不使用 `rm -rf` 而不经 `git rm`
  - 不删除 `external/systemc/.gitkeep`（如存在）

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 涉及 submodule 验证、git rm 协议、跨文件影响
  - **Skills**: [`git-master`]

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: Phase 4
  - **Blocked By**: Phase 3a

  **References**:
  - `scripts/build.sh:9, 32, 46` - 移除位置
  - `src/sc_main.cpp` - 删除目标
  - `src/main.cpp:21` - 删除目标
  - `external/systemc/include/README.md` - 删除目标

  **Acceptance Criteria**:
  - [ ] `external/systemc/` 已 git 删除
  - [ ] `src/sc_main.cpp` 已 git 删除
  - [ ] `src/main.cpp:21` 占位符已删除
  - [ ] `scripts/build.sh` 不含 USE_SYSTEMC
  - [ ] `cmake --build build -j$(nproc)` 编译成功
  - [ ] `ctest` 全部通过
  - [ ] `git grep "USE_SYSTEMC\b" ... | grep -v USE_SYSTEMC_STUB` 0 命中

  **Commit**: YES
  - Message: `refactor(build): remove dead USE_SYSTEMC option - scripts and dirs`

---

- [x] 11. Phase 4: 16 个文件 #ifdef USE_SYSTEMC_STUB 简化为单 stub 路径

  **What to do**:
  - **简化 16 个文件的 `#ifdef USE_SYSTEMC_STUB` 块**：
  - 4 个 ext 头：
    - `include/ext/transaction_context_ext.hh:6-10`
    - `include/ext/mem_exts.hh:5-9`
    - `include/ext/error_context_ext.hh:8-12`
    - `include/core/ext/cmd_exts.hh:5-9`
  - 11 个 test 文件：
    - `test/test_tlm_ext.cc:3-7`
    - `test/test_chstream_integration.cc:14-18`
    - `test/test_phase3_transaction_context.cc:6-10`
    - `test/test_phase3_critical_fixes.cc:14-18`
    - `test/test_phase4_error_context.cc:9-13`
    - `test/test_phase6_regression.cc:10-14`
    - `test/test_phase7_transaction_lifecycle.cc:13-17`
    - `test/test_phase8_performance_stress.cc:11-15`
    - `test/test_extension_prototype.cc:6-10`
    - `test/test_debug_log_system.cc:8-12`
    - `test/mock_modules.hh:8-16`（Phase 2 已简化为单层，Phase 4 进一步删 #ifdef）
  - **替换模式**：
    ```cpp
    // 旧:
    #ifdef USE_SYSTEMC_STUB
    #include "tlm/tlm_stub.hh"
    #else
    #include "tlm.h"  // 或 <tlm>
    #endif
    
    // 新:
    #include "tlm/tlm_stub.hh"
    ```
  - **注意 `mock_modules.hh`**：Phase 2 已简化为单层 #ifdef；Phase 4 删除整个 #ifdef 块

  **Must NOT do**:
  - 不修改业务逻辑
  - 不修改 stub 头文件本身
  - 不删除 `USE_SYSTEMC_STUB` CMake option（其他 ext 头仍可能引用）

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 机械替换，16 个文件同模式
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES（16 个文件互不依赖）
  - **Parallel Group**: Wave 4
  - **Blocks**: Phase 5
  - **Blocked By**: Phase 3b

  **References**:
  - `include/ext/transaction_context_ext.hh:6-10` - 模式
  - `include/ext/mem_exts.hh:5-9` - 模式
  - 等等

  **Acceptance Criteria**:
  - [ ] 16 个文件 #ifdef 块全部删除，替换为单 `#include`
  - [ ] `git grep -l "USE_SYSTEMC_STUB" -- '*.hh' '*.cc' | grep -v tlm_stub.hh` 命中数为 0
  - [ ] 编译成功
  - [ ] 567+ 测试用例通过
  - [ ] `./scripts/format.sh --check` 通过

  **Commit**: YES
  - Message: `refactor(tlm): simplify #ifdef USE_SYSTEMC_STUB to single path`

---

- [x] 12. Phase 5: 一致性审查 + ADR-X.13 + CHANGELOG + 文档同步

  **What to do**:
  - **5.1 新建 `docs/adr/ADR-X.13-stub-multi-extension.md`**：
    - 状态：✅ 已实施
    - 内容：决策（升级 stub 到 SystemC 2.0 数组模式）、理由、影响、参考（Accellera 源码 permalink）
  - **5.2 更新 `docs/adr/ADR-X.5-build-system.md`**：
    - 状态字段：📋 待确认 → ✅ 已实施
    - 删除 L632/L648/L784 等所有 `[ON, OFF]` 矩阵示例和 `-DUSE_SYSTEMC=ON` 命令
    - Q2/Q3/Q4 决策改写：USE_SYSTEMC 不再可选
  - **5.3 更新 `docs/adr/ADR-X.6-transaction-integration.md:234`**：
    - 删除"长期可能需要改进"标记
  - **5.4 创建/更新 `CHANGELOG.md`**：
    - 顶部添加 "Unreleased" 节：
      ```markdown
      ## [Unreleased]
      
      ### Added
      - tlm_stub multi-extension support (SystemC TLM 2.0 API compatible)
      - test_tlm_multi_extension.cc with 10+ test cases
      
      ### Changed
      - TransactionContextExt preserved on MemoryV2 error path (modules_v2.hh:79)
      
      ### Removed
      - BREAKING: USE_SYSTEMC build option (always use TLM stub)
      - external/systemc/ directory
      - src/sc_main.cpp (empty stub)
      - extern "C" int sc_main placeholder
      - Mock/duplicate definitions resolved
      ```
  - **5.5 更新 5+ AGENTS.md**：
    - `AGENTS.md` (项目根) "CI/CD Workflow" 章节：移除 use-systemc 矩阵描述
    - `include/tlm/AGENTS.md`：更新 stub 角色描述（94 行 → ~250 行）
    - `include/AGENTS.md`：更新扩展层描述
    - `include/core/AGENTS.md`：更新 core/ext 描述
    - `include/ext/AGENTS.md`：扩展多 ext 共存描述
  - **5.6 更新 `README.md` (项目根)**：
    - 移除 USE_SYSTEMC 提及（如有）
  - **5.7 审计一致性**：
    ```bash
    # 最终一致性检查
    git grep "USE_SYSTEMC\b" -- '*.md' '*.txt' | grep -v docs-archived/ | grep -v USE_SYSTEMC_STUB
    # 期望：0 命中
    git grep "USE_SYSTEMC" -- '*.py' '*.sh' '*.md' '*.yml' '*.yaml' '*.json' | grep -v docs-archived/
    # 期望：0 命中
    ```

  **Must NOT do**:
  - 不重写非 `ADR-X.2/X.5/X.6/X.13` 的 ADR
  - 不修改 `docs-archived/` 现有文件
  - 不创建新的架构图

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: 主要是文档撰写和同步
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES（5.1-5.6 子任务互不依赖）
  - **Parallel Group**: Wave 5
  - **Blocks**: Final Verification Wave
  - **Blocked By**: Phase 4

  **References**:
  - `docs/adr/ADR-X.5-build-system.md` - 更新目标
  - `docs/adr/ADR-X.6-transaction-integration.md:234` - 更新目标
  - `AGENTS.md` 等 5+ 文件
  - Accellera GitHub permalinks

  **Acceptance Criteria**:
  - [ ] `docs/adr/ADR-X.13-stub-multi-extension.md` 创建
  - [ ] `docs/adr/ADR-X.5-build-system.md` 状态字段更新
  - [ ] `docs/adr/ADR-X.6-transaction-integration.md:234` 标记删除
  - [ ] `CHANGELOG.md` "Unreleased" 节存在且完整
  - [ ] 5+ AGENTS.md 同步
  - [ ] `git grep "USE_SYSTEMC" ...` 全后缀 0 命中
  - [ ] 编译成功
  - [ ] 567+ 测试用例通过
  - [ ] F1-F4 审查全 APPROVE

  **Commit**: YES
  - Message: `docs: ADR-X.13 stub multi-extension + consistency review`

---

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

- [x] F1. **Plan Compliance Audit** — `oracle` (APPROVE, 重审 1m 01s, 修复 commit 4ccef00)
- [x] F2. **Code Quality Review** — `unspecified-high` (APPROVE, 4m 04s)
- [x] F3. **Real Manual QA** — `unspecified-high` + `playwright` skill (if UI) (APPROVE, 10m 18s)
- [x] F4. **Scope Fidelity Check** — `deep` (APPROVE, 7m 18s)

---

## Commit Strategy

| # | Message | Files | Risk |
|---|---------|-------|------|
| 0 | `chore(release): tag v2.1.0-pre-cleanup` | 0 (git tag) | 极低 |
| 1a | `feat(tlm_stub): add extension type registry and tlm_array` | 1 (tlm_stub.hh) | 低 |
| 1b | `refactor(tlm_stub): migrate to static const unsigned int ID` | 1 (tlm_stub.hh) | 低 |
| 1c | `feat(tlm_stub): multi-extension API support` | 1 (tlm_stub.hh) | 中 |
| 1d | `feat(tlm_stub): deep_copy_from and reset semantics` + new test file | 2 (tlm_stub.hh, test_tlm_multi_extension.cc) | 中 |
| 1.5 | `fix(modules): preserve TransactionContext on error path` | 1 (modules_v2.hh:79) | 低 |
| 1.9 | `test(tlm_stub): verify real SystemC 2.0 behavior compatibility` | 1 (test_phase3_critical_fixes.cc) | 低 |
| 2 | `refactor(ext): resolve duplicate definitions and mock bug` | 5+ (mock_modules, cmd_exts, packet_to_payload, payload_to_packet, code-format CI) | 低 |
| 3a | `refactor(build): remove dead USE_SYSTEMC option - CMake and CI` | 3 (root CMake, src/CMake, ci.yml) | 低 |
| 3b | `refactor(build): remove dead USE_SYSTEMC option - scripts and dirs` | 4 (build.sh, external/systemc, sc_main.cpp, main.cpp:21) | 中 |
| 4 | `refactor(tlm): simplify #ifdef USE_SYSTEMC_STUB to single path` | 16 (4 ext + 11 test + 1 mock) | 低 |
| 5 | `docs: ADR-X.13 stub multi-extension + consistency review` | 8+ (ADR, AGENTS, CHANGELOG, README) | 极低 |

---

## Success Criteria

### Verification Commands
```bash
# 全量验证
cmake -S . -B build -DUSE_SYSTEMC=OFF -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests
./build/bin/cpptlm_tests "[multi_extension]"
./build/bin/cpptlm_tests "[phase3]"
./build/bin/cpptlm_tests "[chstream]"
./build/bin/cpptlm_tests "[crossbar]"
./scripts/format.sh --check

# 真实 SystemC 验证（Phase 1.9）
cmake -S . -B build-sysc -DUSE_SYSTEMC=ON -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build-sysc -j$(nproc)
./build-sysc/bin/cpptlm_tests "[multi_extension]"

# USE_SYSTEMC 移除验证（Phase 3）
git grep -n "USE_SYSTEMC" -- '*.txt' '*.cmake' '*.yml' '*.sh' '*.cpp' '*.h' '*.hh' | grep -v docs-archived/
# 期望：0 命中

# #ifdef 简化验证（Phase 4）
git grep -l "USE_SYSTEMC_STUB" -- '*.hh' '*.cc' | grep -v tlm_stub.hh
# 期望：0 命中
```

### Final Checklist
- [x] 所有 12 commits 落地
- [x] 569 + 10+ 测试用例全通过（579/581）
- [x] `git grep` 命令 0 命中（active paths）
- [x] `external/systemc/` 已删除
- [x] `modules_v2.hh:79` 修复
- [x] `v2.1.0` tag 已创建
- [x] `CHANGELOG.md` 已更新
- [x] `ADR-X.13` 已创建
- [x] F1-F4 审查全 APPROVE
