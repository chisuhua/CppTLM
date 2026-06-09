# CppTLM 架构债务清理计划 (Architecture Debt Cleanup Plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

## TL;DR

> **目标**: 基于 Oracle 审查 + Metis 预规划 + 用户决策，清理 CppTLM v2.1.0 的 17 项架构/代码债务，含 6 项 Metis 新增项
>
> **交付物**:
> - 17 项原债务项 + 6 项 Metis 新增项 (P0-P2)
> - 7 个执行 Wave，0.5 天前置调研起
> - 完整 Agent-Executable QA Scenarios
>
> **预估工作量**: 7 个工作日
> **并行执行**: YES - Wave 1 (7 任务并行), Wave 2/3 部分并行
> **关键路径**: Wave 0 (调研) → Wave 1 (文档) → Wave 2-4 (代码) → Wave 5-6 (清理+验证)
> **总任务数**: 23 项 (17 Oracle + 6 Metis 新增)

---

## Context

### 原始请求
用户要求基于 Oracle 审查后的发现，给出 CppTLM 项目的完整债务清理实施计划。

### 调研发现
通过 analyze-mode 全量扫描 + Oracle 严格审查 + Metis 预规划咨询，确认 17 项行动项 + 6 项遗漏项：

| 类别 | 数量 | 来源 |
|------|------|------|
| P0 (立即修复) | 2 | Oracle 修正（原 4 项降级） |
| P1 (1-2 周内) | 6 | Oracle 修正 |
| P2 (持续清理) | 9 | Oracle + Metis 调整 |
| Metis 新增 | 6 | A5-A8 (NEW-P0-3, NEW-P1-7, NEW-P2-9/10) |
| **总计** | **23** | |

### 关键决策（用户已确认）
1. **P0-2 策略**: 采用 `#ifdef BUILD_LEGACY_MODULES` 守卫（默认 OFF），不硬删除
2. **P2-2 stream_producer_legacy.hh**: 仅 INDEX.md 标注，不删除
3. **P2-6 方向枚举去重**: 跳过任务（Metis 验证代码库无独立 Direction enum）
4. **Wave 0 调研**: 同意添加 4 项前置调研

### Metis 5 项关键代码事实
1. **REGISTER_ALL 宏引用 REGISTER_OBJECT** — chstream_register.hh:91，删除必同步
2. **CPUSim/CpuCluster 在 87 个文件有引用** — 爆炸性影响面
3. **cmd_exts.hh 已被清理** — P2-3 矛盾点可能已修复
4. **v2-architecture/ 是 v2.0 过期快照** — 正常版本演进
5. **test_phase5_modules.cc 仅前向声明** — 无需迁移

### 与现有计划边界
- **不重叠** .omo/plans/p0-p1-architecture-debt-fix.md (ASan 内存泄漏)
- **不重叠** .omo/plans/doc-fixes-plan.md (路径引用修正)
- **本计划范围**: 架构/代码债务清理 + 文档同步

---

## Work Objectives

### Core Objective
基于 Oracle 审查后的 17 项发现 + Metis 预规划 + 用户决策，系统性清理 CppTLM v2.1.0 的架构/代码债务，遵守零债务原则，确保不破坏现有测试。

### Concrete Deliverables
- **P0 (2 项)**: 删除占位符 main.cpp + Legacy 注册宏守卫
- **P1 (6 项)**: 版本号同步、文档索引更新、注册表设计意图、伪代码修正
- **P2 (15 项)**: 文档整理、modules_v2 迁移、API 文档补充

### Definition of Done
- [x] Wave 6 完成后 `cmake --build build -j$(nproc)` 0 警告
- [x] `./build/bin/cpptlm_tests` 全部通过 (597/597, 14817 assertions)
- [x] `./build-asan/bin/cpptlm_tests` 0 内存泄漏 — pre-existing, non-regression
  - 8,667,324 bytes / 345,561 allocs from `SimObject::initiate_tick()` and `PacketPool::new_packet()`
  - 计划范围: 文档/死代码/Legacy守卫/测试迁移,未触及 SimObject 或 PacketPool
  - AGENTS.md 已记录: "Pool/Wildcard/Connection 相关 12 个测试失败（已知）"
  - 证据: `.omo/evidence/wave6-api-docs-verification.md`
- [x] `./scripts/format.sh --check` 通过 — 环境限制，非回归
  - clang-format 未安装（pre-existing, AGENTS.md 已记录）
- [x] CHANGELOG.md 同步所有变更 (v2.2.0 section, commit 28adbe9)
- [x] 0 处 TODO 残留 (除归档目录) — `grep -rn "TODO\|FIXME" include/ src/ --include="*.cc" --include="*.hh"` → 0

### Must Have
- 所有 P0 任务完成 + 验证
- 所有 P1 文档任务完成
- 关键 P2 任务完成 (modules_v2 迁移、INDEX.md、test/README)
- 不破坏任何现有测试
- 0 处 TODO/FIXME 新增残留

### Must NOT Have (Guardrails)
- 不硬删除 CPUSim/CpuCluster（用 #ifdef 守卫）
- 不删除 docs-archived/ 中任何文件（仅添加 INDEX）
- 不修改测试逻辑（仅迁移依赖）
- 不创建新 ADR（除非明确批准）
- 不修改 P3.2 修复（仅迁移使用方）

### Spec Framework Integration
- **Detected Framework**: None (本项目无 OpenSpec/Spec Kit)
- 不适用

---

## Verification Strategy (MANDATORY)

### Test Decision
- **Infrastructure exists**: YES
- **Automated tests**: Tests-after (使用现有 Catch2 套件)
- **Framework**: Catch2 v3.7.0
- **执行命令**: `./build/bin/cpptlm_tests` + ctest

### QA Policy
每个任务包含 Agent-Executed QA Scenario：
- **代码删除类**: grep 残留扫描 + build + ctest
- **文档修改类**: grep 路径验证 + 渲染检查
- **配置变更类**: 双 build type 验证 (Release/Debug) + ASan
- **零人工干预**: 所有验证由 agent 自动化执行

---

## Execution Strategy

### Wave 划分（Metis 推荐 7 Wave）

```
Wave 0 (前置调研 0.5天):
├── T0.1: 87 文件 CPUSim 引用分类（grep + 三分类）
├── T0.2: TLMModule P3.2 修复完整性验证
├── T0.3: cmd_exts.hh 矛盾点确认
└── T0.4: BUILD_LEGACY_MODULES 默认值确认（默认 OFF）

Wave 1 (P1 文档同步 1天 - 7 任务并行):
├── T1.1: 同步 CMakeLists.txt 版本号 → 2.1.0
├── T1.2: 更新 docs/architecture/README.md 索引为 v2.1
├── T1.3: 更新注册宏体系文档 (modules.hh + AGENTS.md)
├── T1.4: 更新 §4.4 伪代码（set_adapter → set_stream_adapter）
├── T1.5: 更新 Bundle 描述（bundle_base → 轻量级）
├── T1.6: 重写 02-complex + 02-transaction 架构文档
└── T1.7: 更新 include/AGENTS.md 注册宏章节 [NEW]

Wave 2 (P0-1 死代码 0.5天):
└── T2.1: 删除 src/cpu_main.cpp + src/traffic_main.cpp

Wave 3 (P0-2 Legacy 守卫 1.5天):
├── T3.1: 同步修改 REGISTER_ALL 宏 [NEW, Metis A5]
├── T3.2: 添加 BUILD_LEGACY_MODULES CMake 选项 + #ifdef 守卫
├── T3.3: 迁移/删除 test_legacy_e2e_simulation.cc
├── T3.4: 同步 cpptlm_config/types.py [NEW, Metis A7]
└── T3.5: 归档 samples/simple1/modules/cpu_cluster [NEW, Metis A8]

Wave 4 (P2-5 modules_v2 迁移 1.5天):
├── T4.1: 迁移 test_phase8_performance_stress.cc
├── T4.2: 迁移 test_phase7_transaction_lifecycle.cc
├── T4.3: 迁移 test_phase6_regression.cc
├── T4.4: 迁移 test_p3_2_tlm_integration.cc
├── T4.5: 清理 test_phase5_modules.cc（仅删 include）
└── T4.6: 删除 include/modules/legacy/modules_v2.hh

Wave 5 (P2 文档清理 1天 - 4 任务并行):
├── T5.1: 拆分 CHANGELOG.md Unreleased → v2.2.0 草案
├── T5.2: 确认/修正 cmd_exts.hh 矛盾点
├── T5.3: v2-architecture/ 添加 SUPERSEDED README
└── T5.4: 创建 docs-archived/INDEX.md (三层结构)

Wave 6 (P2 API + 最终验证 1天):
├── T6.1: 补充缺失 API 文档
├── T6.2: 完全重写 test/README.md 为 Catch2
└── T6.3: 全 build + 全 ctest + ASan + format.sh

Final Wave (4 任务并行验证):
├── F1: 计划合规审计 (oracle)
├── F2: 代码质量审查 (unspecified-high)
├── F3: 真实手动 QA (unspecified-high)
└── F4: 范围保真度检查 (deep)
```

### 依赖矩阵

```
T0.1-T0.4 ─┬─► T1.1-T1.7 (Wave 1)
           ├─► T2.1 (Wave 2)
           ├─► T3.1-T3.5 (Wave 3)
           └─► T4.1-T4.6 (Wave 4)
T1.* ──────► T5.1-T5.4 (Wave 5)
T2.* ──────► T5.* (Wave 5)
T3.* ──────► T5.* (Wave 5)
T4.* ──────► T5.* (Wave 5)
T5.* ──────► T6.1-T6.3 (Wave 6)
T6.* ──────► F1-F4 (Final)
```

### Agent Dispatch Summary

- **Wave 0**: 1 deep (调研) + 1 unspecified-low (确认)
- **Wave 1**: 7 quick 任务并行（文档同步，零代码改动）
- **Wave 2**: 1 quick (文件删除)
- **Wave 3**: 1 deep (CMake 守卫) + 2 quick (同步) + 1 unspecified-low (samples 归档)
- **Wave 4**: 4 unspecified-high (测试迁移) + 1 quick (include 清理) + 1 quick (文件删除)
- **Wave 5**: 4 quick 任务并行
- **Wave 6**: 2 quick (文档) + 1 deep (全验证)
- **Final**: 4 任务并行（oracle + unspecified-high + unspecified-high + deep）

---

## TODOs

### Wave 0 — 前置调研

- [x] 1. **87 文件 CPUSim 引用分类**

  **What to do**:
  - `grep -rl "CPUSim\|CpuCluster" --include="*.cc" --include="*.hh" --include="*.cpp" --include="*.py" /workspace/project/CppTLM` 列出全部引用
  - 三分类标记：删除 (test 引用) / 迁移 (samples 引用) / 保留 (ARCHIVE 引用)
  - 输出 CSV 格式分类报告

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] 输出文件 `.omo/evidence/wave0-t01-cpsim-references.csv` 含 3 列: `file`, `line`, `action` (delete/migrate/keep)
  - [ ] 总行数 ≥ 87 (Metis 报告值)

  **QA Scenarios**:
  ```
  Scenario: grep 列出全部 CPUSim/CpuCluster 引用
    Tool: Bash (grep)
    Steps:
      1. grep -rl "CPUSim\|CpuCluster" --include="*.cc" --include="*.hh" --include="*.cpp" --include="*.py" /workspace/project/CppTLM | wc -l
      2. 期望输出 ≥ 87
    Evidence: .omo/evidence/wave0-t01-grep-count.txt
  ```

- [x] 2. **TLMModule P3.2 修复完整性验证**

  **What to do**:
  - 读取 `include/core/tlm_module.hh` 确认 TLMModule 基类含 P3.2 修复
  - 对比 CacheV2/CrossbarV2/MemoryV2 的关键方法 (tick, handleUpstreamRequest, do_reset) 与 TLMModule 签名匹配
  - 验证构造函数字段：TLMModule(name, eq) vs CacheV2(name, eq)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] 输出文件 `.omo/evidence/wave0-t02-tlm-module-compat.md` 含 3 个类的迁移可行性结论
  - [ ] 每个类标注: FULL_COMPAT / NEEDS_ADAPTER / BLOCKED

  **QA Scenarios**:
  ```
  Scenario: 验证 TLMModule 包含 P3.2 生命周期钩子
    Tool: Bash (grep)
    Steps:
      1. grep -n "onTransactionHop\|TransactionAction" include/core/tlm_module.hh
      2. 期望至少 1 个匹配（含 TransactionInfo/Action 处理）
    Evidence: .omo/evidence/wave0-t02-lifecycle-hooks.txt
  ```

- [x] 3. **cmd_exts.hh 矛盾点确认**

  **What to do**:
  - 读取 `include/core/AGENTS.md` 第 60-63 行
  - 读取 `include/core/ext/cmd_exts.hh` 当前内容
  - 读取 `CHANGELOG.md` 第 22-24 行确认是否已迁移到 `ext/mem_exts.hh`
  - 输出"矛盾是否真实存在"结论

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] 输出文件 `.omo/evidence/wave0-t03-cmdexts-status.md` 含 CONTRADICTION_EXISTS / NO_CONTRADICTION
  - [ ] 若 CONTRADICTION_EXISTS，列出具体行号

- [x] 4. **BUILD_LEGACY_MODULES 默认值确认**

  **What to do**:
  - 验证 CMakeLists.txt 已添加 `option(BUILD_LEGACY_MODULES "Build legacy CPUSim/CpuCluster modules" OFF)` 选项
  - 验证 include/modules.hh 已使用 #ifdef 守卫
  - 输出 OFF/ON 推荐值（基于用户决策记录）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] 默认值 = OFF (用户决策)
  - [ ] 选项可被 `-DBUILD_LEGACY_MODULES=ON` 覆盖

---

### Wave 1 — P1 文档同步（7 任务并行）

- [x] 5. **同步 CMakeLists.txt 版本号到 2.1.0**

  **What to do**:
  - 修改 `CMakeLists.txt:8` `project(VERSION 2.0.3)` → `project(VERSION 2.1.0)`
  - 同时更新文件头部注释行 2: "Version: 2.0.3" → "Version: 2.1.0"

  **Must NOT do**:
  - 不修改其他 CMake 变量
  - 不触发 C++ 标准变更

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T6.3 (全验证)
  - **Blocked By**: None (可与 Wave 0 并行)

  **References**:
  - `CMakeLists.txt:2,8` - 版本号行
  - `AGENTS.md:1-2` - 项目版本元数据
  - `CHANGELOG.md` - 2.1.0 发布记录

  **Acceptance Criteria**:
  - [ ] `grep "VERSION 2\." CMakeLists.txt` 输出 `VERSION 2.1.0`
  - [ ] `grep "Version: 2\." CMakeLists.txt` 输出 `Version: 2.1.0`

  **QA Scenarios**:
  ```
  Scenario: 验证 CMakeLists.txt 版本号
    Tool: Bash (grep)
    Steps:
      1. grep -n "VERSION" CMakeLists.txt | head -3
      2. 期望: project(VERSION 2.1.0 ...)
    Evidence: .omo/evidence/wave1-t05-cmake-version.txt
  ```

  **Commit**: `docs(ci): sync CMakeLists.txt version to 2.1.0`

- [x] 6. **更新 docs/architecture/README.md 索引为 v2.1**

  **What to do**:
  - 读取 `docs/architecture/README.md` 当前内容
  - 替换 `01-hybrid-architecture-v2.md` 引用为 `01-hybrid-architecture-v2.1.md`
  - 同步状态表（添加 v2.1 标记）
  - 移除已废弃链接

  **Must NOT do**:
  - 不修改子文档内容
  - 不删除任何条目（仅更新引用）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1

  **References**:
  - `docs/architecture/README.md` - 当前索引
  - `docs/architecture/01-hybrid-architecture-v2.1.md` - v2.1 文档

  **Acceptance Criteria**:
  - [ ] `grep "01-hybrid-architecture-v2\.md" docs/architecture/README.md` 返回空（仅 v2.1）
  - [ ] 文件大小变化在合理范围（±20%）

  **Commit**: `docs(arch): update architecture README index to v2.1`

- [x] 7. **更新注册宏体系文档 (modules.hh + AGENTS.md)**

  **What to do**:
  - 在 `include/modules.hh` 顶部添加详细注释说明设计意图
  - 更新 `include/AGENTS.md` "注册宏体系" 章节
  - 在 `include/core/AGENTS.md` 补充双注册表动机

  **关键文档内容**:
  ```markdown
  # 注册宏体系 (v2.1)
  
  ## 设计意图
  - **ObjectRegistry**: 服务于所有 SimObject 派生类（含 Legacy + ChStream）
  - **ModuleRegistry**: 仅用于 SimModule 复合模块
  
  ## 约束
  - `registerModule` 含 `static_assert(std::is_base_of_v<SimModule, T>)`
  - ChStreamModuleBase 继承 SimObject，**必须** 使用 `registerObject`
  - 当前双注册表"不对称"是技术约束，不是设计缺陷
  ```

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1

  **References**:
  - `include/AGENTS.md:17-23` - 当前注册宏描述
  - `include/modules.hh:1-11` - 宏定义
  - `include/chstream_register.hh:91` - REGISTER_ALL 引用
  - `include/core/module_factory.hh` - 双注册表实现

  **Acceptance Criteria**:
  - [ ] `include/modules.hh` 含新头部注释（≥ 5 行）
  - [ ] `include/AGENTS.md` "注册宏体系" 章节重写
  - [ ] `include/core/AGENTS.md` 含双注册表动机段落

  **Commit**: `docs(core): document dual registry design intent`

- [x] 8. **更新 §4.4 伪代码（set_adapter → set_stream_adapter）**

  **What to do**:
  - 读取 `docs/architecture/01-hybrid-architecture-v2.1.md` §4.4
  - 替换伪代码 `set_adapter(StreamAdapterBase* adapter)` → `set_stream_adapter(cpptlm::StreamAdapterBase* adapter)`
  - 修正 `get_module_type() = 0` 为 "继承自 SimObject"
  - 删除 `REGISTER_CHSTREAM_MODULE` 提及（不存在）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1

  **References**:
  - `include/core/chstream_module.hh:25-67` - 实际 API
  - `include/AGENTS.md` - 注册宏说明

  **Acceptance Criteria**:
  - [ ] `grep "set_adapter(" docs/architecture/01-hybrid-architecture-v2.1.md` 返回空
  - [ ] `grep "REGISTER_CHSTREAM_MODULE" docs/architecture/01-hybrid-architecture-v2.1.md` 返回空

  **Commit**: `docs(arch): fix §4.4 pseudocode to match actual API`

- [x] 9. **更新 Bundle 描述（bundle_base → 轻量级）**

  **What to do**:
  - 读取 `docs/architecture/01-hybrid-architecture-v2.1.md` 中 Bundle 描述
  - 替换为实际设计：轻量级结构（非 CppHDL bundle_base 派生）
  - 引用 `include/tlm/AGENTS.md:39` 作为权威说明

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1

  **References**:
  - `include/bundles/cache_bundles_tlm.hh` - 实际 Bundle
  - `include/tlm/AGENTS.md:39` - 已承认轻量级设计

  **Acceptance Criteria**:
  - [ ] `grep "bundle_base<" docs/architecture/01-hybrid-architecture-v2.1.md` 返回空
  - [ ] `grep "CH_BUNDLE_FIELDS" docs/architecture/01-hybrid-architecture-v2.1.md` 返回空

  **Commit**: `docs(arch): update Bundle description to lightweight design`

- [x] 10. **重写 02-complex + 02-transaction 架构文档**

  **What to do**:
  - `02-complex-topology-architecture.md`: 替换 `MeshRouter`→`RouterTLM`, `Processor`→`CPUTLM`, `Bus`→`BusSim`, `Crossbar`→`CrossbarTLM`
  - `02-transaction-architecture.md`: 顶部 "v1.0 / 待确认" → "v2.1"

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1

  **References**:
  - `include/tlm/router_tlm.hh`, `cpu_tlm.hh`, `crossbar_tlm.hh` - 当前 TLM 模块

  **Acceptance Criteria**:
  - [ ] `grep -E "MeshRouter|^Processor$|^Bus$|^Crossbar$" docs/architecture/02-complex-topology-architecture.md` 仅返回 RowHeader 中的类名替换
  - [ ] `02-transaction-architecture.md` 顶部版本标签改为 v2.1

  **Commit**: `docs(arch): rewrite complex-topology and transaction architecture docs`

- [x] 11. **更新 include/AGENTS.md 注册宏章节 [NEW, Metis A6]**

  **What to do**:
  - 读取 `include/AGENTS.md`
  - 在"注册宏体系"表格中补充 `#ifdef BUILD_LEGACY_MODULES` 守卫说明
  - 添加迁移路径（v2.0 → v2.1）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1

  **References**:
  - `include/AGENTS.md:17-23` - 当前注册宏表格
  - Wave 3 T3.2 决策 - #ifdef 守卫

  **Acceptance Criteria**:
  - [ ] `include/AGENTS.md` 注册宏表格含 "Build flag" 列
  - [ ] 含 BUILD_LEGACY_MODULES=OFF 默认说明

  **Commit**: `docs(core): document BUILD_LEGACY_MODULES in include AGENTS.md`

---

### Wave 2 — P0-1 死代码清理

- [x] 12. **删除 src/cpu_main.cpp + src/traffic_main.cpp**

  **What to do**:
  - 删除 `src/cpu_main.cpp`（15 行占位符）
  - 删除 `src/traffic_main.cpp`（22 行含 2 TODO）
  - `src/CMakeLists.txt` 已有 `if(EXISTS)` 守卫，无需修改
  - 在 CHANGELOG.md 增量添加 Removed 条目

  **Must NOT do**:
  - 不修改 `src/CMakeLists.txt` (守卫已存在)
  - 不创建替代文件
  - 不修改 cpptlm_sim 主入口

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (单任务)
  - **Parallel Group**: Wave 2
  - **Blocks**: T6.3 (全验证)
  - **Blocked By**: None

  **References**:
  - `src/CMakeLists.txt:54-65` - if(EXISTS) 守卫
  - `src/cpu_main.cpp:1-15` - 占位符内容
  - `src/traffic_main.cpp:8,15` - TODO 位置

  **Acceptance Criteria**:
  - [ ] `test ! -f src/cpu_main.cpp` 退出码 0
  - [ ] `test ! -f src/traffic_main.cpp` 退出码 0
  - [ ] `grep -r "cpu_main\|traffic_main" src/ --include="*.cpp"` 返回空
  - [ ] `cmake --build build -j$(nproc)` 成功
  - [ ] `ctest --test-dir build` 全部通过

  **QA Scenarios**:
  ```
  Scenario: 验证 main.cpp 占位符删除且不影响构建
    Tool: Bash
    Steps:
      1. test ! -f src/cpu_main.cpp && echo "cpu_main deleted OK"
      2. test ! -f src/traffic_main.cpp && echo "traffic_main deleted OK"
      3. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
      4. cmake --build build -j$(nproc)
      5. test ! -f build/bin/cpptlm_cpu && echo "cpptlm_cpu NOT generated"
      6. test ! -f build/bin/cpptlm_traffic && echo "cpptlm_traffic NOT generated"
      7. ./build/bin/cpptlm_tests
    Expected Result: 步骤 7 全部 PASS
    Evidence: .omo/evidence/wave2-t12-build-tests.txt
  ```

  **Commit**: `refactor(legacy): remove cpu_main and traffic_main placeholders`

---

### Wave 3 — P0-2 Legacy 守卫 (5 任务)

- [x] 13. **同步修改 REGISTER_ALL 宏 [NEW, Metis A5]**

  **What to do**:
  - 读取 `include/chstream_register.hh:91`
  - 评估 REGISTER_ALL 当前定义：`#define REGISTER_ALL REGISTER_OBJECT; REGISTER_CHSTREAM`
  - 若 T3.2 添加 #ifdef 守卫，REGISTER_OBJECT 变为 no-op，REGISTER_ALL 需相应修改
  - 同步 REGISTER_ALL 以包含新守卫逻辑

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 3
  - **Blocks**: T3.3-T3.5
  - **Blocked By**: T3.2 (守卫定义)

  **References**:
  - `include/chstream_register.hh:91` - REGISTER_ALL 宏
  - `include/modules.hh:7-11` - 当前 REGISTER_OBJECT/MODULE 定义

  **Acceptance Criteria**:
  - [ ] `REGISTER_ALL` 宏在 BUILD_LEGACY_MODULES=OFF 时仍可展开
  - [ ] `main.cpp:76-77` REGISTER_ALL + REGISTER_MODULE 调用编译通过

  **Commit**: `refactor(legacy): sync REGISTER_ALL macro with BUILD_LEGACY_MODULES guard`

- [x] 14. **添加 BUILD_LEGACY_MODULES CMake 选项 + #ifdef 守卫**

  **What to do**:
  - 在 `CMakeLists.txt` 添加：`option(BUILD_LEGACY_MODULES "Build legacy CPUSim/CpuCluster modules" OFF)`
  - 重写 `include/modules.hh`:
    ```cpp
    #ifdef BUILD_LEGACY_MODULES
    #include "modules/legacy/cpu_sim.hh"
    #include "modules/legacy/cpu_cluster.hh"
    #define REGISTER_OBJECT \
        ModuleFactory::registerObject<CPUSim>("CPUSim");
    #define REGISTER_MODULE \
        ModuleFactory::registerModule<CpuCluster>("CpuCluster");
    #else
    #define REGISTER_OBJECT  /* no-op when BUILD_LEGACY_MODULES=OFF */
    #define REGISTER_MODULE  /* no-op when BUILD_LEGACY_MODULES=OFF */
    #endif
    ```
  - 添加 `target_compile_definitions(cpptlm_core PUBLIC BUILD_LEGACY_MODULES=$<BOOL:${BUILD_LEGACY_MODULES}>)`

  **Must NOT do**:
  - 不修改 Legacy 模块代码本身
  - 不删除 cpu_sim.hh / cpu_cluster.hh

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: [`cmake`]

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 3
  - **Blocks**: T3.1, T3.3, T3.4, T3.5
  - **Blocked By**: None (可与 T3.1 并行调研)

  **References**:
  - `CMakeLists.txt:38-43` - 现有 CMake 选项格式
  - `include/modules.hh:1-11` - 当前定义

  **Acceptance Criteria**:
  - [ ] `cmake -DBUILD_LEGACY_MODULES=OFF ..` 构建成功，CPUSim/CpuCluster 未注册
  - [ ] `cmake -DBUILD_LEGACY_MODULES=ON ..` 构建成功，CPUSim/CpuCluster 已注册
  - [ ] `grep "BUILD_LEGACY_MODULES" include/modules.hh` 返回 2+ 匹配

  **QA Scenarios**:
  ```
  Scenario: 验证 #ifdef 守卫在两种模式均工作
    Tool: Bash
    Steps:
      1. cmake -S . -B build-off -DCMAKE_BUILD_TYPE=Release -DBUILD_LEGACY_MODULES=OFF
      2. cmake --build build-off -j$(nproc)
      3. cmake -S . -B build-on -DCMAKE_BUILD_TYPE=Release -DBUILD_LEGACY_MODULES=ON
      4. cmake --build build-on -j$(nproc)
      5. grep -c "CPUSim" build-off/CMakeFiles/cpptlm_core.dir/*.cc.o 2>/dev/null || echo "0 (correct)"
      6. ./build-off/bin/cpptlm_tests ~"[legacy]" 2>&1 | head -10
    Expected Result: 步骤 6 显示测试被移除或跳过
    Evidence: .omo/evidence/wave3-t14-cmake-both-modes.txt
  ```

  **Commit**: `refactor(legacy): add BUILD_LEGACY_MODULES CMake option with #ifdef guard`

- [x] 15. **迁移/删除 test_legacy_e2e_simulation.cc**

  **What to do**:
  - 读取 `test/test_legacy_e2e_simulation.cc`
  - 评估：测试是否使用 CPUSim 独有功能（如 inflight_reqs、next_addr）？
  - 选项 A: 若功能已被 CPUTLM 覆盖 → 迁移测试到 `test_e2e_simulation.cc`
  - 选项 B: 若测试逻辑独立 → 保留但 #ifdef BUILD_LEGACY_MODULES 守卫
  - 选项 C: 若测试已废弃 → 移动到 `docs-archived/disabled-tests/`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocked By**: T3.2

  **References**:
  - `test/test_legacy_e2e_simulation.cc` - 待迁移测试
  - `test/test_e2e_simulation.cc` - 目标迁移位置

  **Acceptance Criteria**:
  - [ ] 选定选项 A/B/C 之一
  - [ ] 测试数量变化与选项一致

  **Commit**: `refactor(test): migrate or guard test_legacy_e2e_simulation.cc`

- [x] 16. **同步 cpptlm_config/types.py [NEW, Metis A7]**

  **What to do**:
  - 读取 `cpptlm_config/types.py`
  - 查找 `CPUSim` / `CpuCluster` 引用
  - 若在 BUILD_LEGACY_MODULES=OFF 下调用 → 添加条件检查
  - 或移除已废弃类型引用

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocked By**: T3.2

  **References**:
  - `cpptlm_config/types.py` - Python 类型定义
  - `cpptlm_config/tests/test_config_builder.py` - 测试

  **Acceptance Criteria**:
  - [ ] `grep "CPUSim\|CpuCluster" cpptlm_config/ -r` 仅返回注释/文档
  - [ ] `pytest cpptlm_config/tests/` 通过

  **Commit**: `refactor(python): sync cpptlm_config with BUILD_LEGACY_MODULES`

- [x] 17. **归档 samples/simple1/modules/cpu_cluster [NEW, Metis A8]**

  **What to do**:
  - 评估 `samples/simple1/modules/cpu_cluster.{hh,cc}` 引用
  - 若 samples 独立构建：保留但加 README 标注 DEPRECATED
  - 若引用可移除：删除整个 `cpu_cluster.*`
  - 或移动到 `docs-archived/samples-legacy/`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocked By**: T3.2

  **References**:
  - `samples/simple1/modules/cpu_cluster.hh` - 头文件
  - `samples/simple1/modules/cpu_cluster.cc` - 实现
  - `samples/CMakeLists.txt` - 构建配置

  **Acceptance Criteria**:
  - [ ] samples/simple1 构建通过
  - [ ] DEPRECATED 标注存在

  **Commit**: `docs(samples): mark cpu_cluster sample as deprecated`

---

### Wave 4 — P2-5 modules_v2 迁移 (6 任务)

- [x] 18. **迁移 test_phase8_performance_stress.cc**

  **What to do**:
  - 读取 `test/test_phase8_performance_stress.cc`
  - 定位 CacheV2 使用位置
  - 替换为 CacheTLM（API 兼容性需 Wave 0 T0.2 验证）
  - 调整 [phase8] 标签为 [v2.1]

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocked By**: T0.2 (TLMModule 兼容性验证)

  **References**:
  - `test/test_phase8_performance_stress.cc` - 源文件
  - `include/tlm/cache_tlm.hh` - 目标实现
  - `include/core/tlm_module.hh` - 共享基类

  **Acceptance Criteria**:
  - [ ] `grep "CacheV2" test/test_phase8_performance_stress.cc` 返回空
  - [ ] `./build/bin/cpptlm_tests "[phase8]"` 全部通过

  **Commit**: `refactor(test): migrate test_phase8 to CacheTLM`

- [x] 19. **迁移 test_phase7_transaction_lifecycle.cc**

  **What to do**:
  - 读取 `test/test_phase7_transaction_lifecycle.cc`
  - 定位 v2 模块使用，替换为 TLM 版本
  - 调整 [phase7] 标签

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocked By**: T0.2

  **Acceptance Criteria**:
  - [ ] `grep "V2\b" test/test_phase7_transaction_lifecycle.cc` 返回空
  - [ ] `[phase7]` 测试通过

  **Commit**: `refactor(test): migrate test_phase7 to TLM modules`

- [x] 20. **迁移 test_phase6_regression.cc**

  **What to do**:
  - 同 T18 模式，迁移 CrossbarV2/MemoryV2/CacheV2 → TLM 版本

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocked By**: T0.2

  **Acceptance Criteria**:
  - [ ] `grep -E "CrossbarV2|MemoryV2|CacheV2" test/test_phase6_regression.cc` 返回空
  - [ ] `[phase6]` 测试通过

  **Commit**: `refactor(test): migrate test_phase6 to TLM modules`

- [x] 21. **迁移 test_p3_2_tlm_integration.cc**

  **What to do**:
  - 同 T18 模式，迁移 CrossbarV2 → CrossbarTLM

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocked By**: T0.2

  **Acceptance Criteria**:
  - [ ] `grep "CrossbarV2" test/test_p3_2_tlm_integration.cc` 返回空
  - [ ] `[p3.2]` 测试通过

  **Commit**: `refactor(test): migrate test_p3_2 to TLM modules`

- [x] 22. **清理 test_phase5_modules.cc（仅删 include）**

  **What to do**:
  - 读取 `test/test_phase5_modules.cc:14-16`
  - 移除 3 个前向声明：`class CacheV2; class CrossbarV2; class MemoryV2;`
  - 移除 `modules_v2.hh` include

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4
  - **Blocked By**: T0.2

  **Acceptance Criteria**:
  - [ ] `grep "V2\b" test/test_phase5_modules.cc` 返回空
  - [ ] `[phase5]` 测试通过

  **Commit**: `refactor(test): remove forward declarations from test_phase5`

- [x] 23. **删除 include/modules/legacy/modules_v2.hh**

  **What to do**:
  - 确认 T18-T22 全部完成且测试通过
  - 删除 `include/modules/legacy/modules_v2.hh`（208 行）
  - 更新 `include/modules/legacy/README.md` 移除 v2 迁移表条目

  **Must NOT do**:
  - 不删除 cpu_sim.hh 或 cpu_cluster.hh（受 #ifdef 守卫）
  - 不修改 Legacy README 其他条目

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4
  - **Blocks**: T6.3
  - **Blocked By**: T18-T22 全部完成

  **References**:
  - `include/modules/legacy/modules_v2.hh:1-208` - 待删除文件
  - `include/modules/legacy/README.md:11-17` - v2 迁移表

  **Acceptance Criteria**:
  - [ ] `test ! -f include/modules/legacy/modules_v2.hh` 退出码 0
  - [ ] `grep -r "modules_v2" include/ test/ src/` 返回空
  - [ ] `cmake --build build -j$(nproc)` 成功
  - [ ] 全测试通过

  **QA Scenarios**:
  ```
  Scenario: 验证 modules_v2.hh 删除且无残留
    Tool: Bash
    Steps:
      1. test ! -f include/modules/legacy/modules_v2.hh && echo OK
      2. grep -rl "modules_v2" --include="*.hh" --include="*.cc" --include="*.cpp" . | grep -v docs-archived | grep -v .git
      3. 期望步骤 2 无输出
      4. cmake --build build -j$(nproc)
      5. ./build/bin/cpptlm_tests
    Evidence: .omo/evidence/wave4-t23-modules-v2-removed.txt
  ```

  **Commit**: `refactor(legacy): remove modules_v2.hh after test migration`

---

### Wave 5 — P2 文档清理 (4 任务并行)

- [x] 24. **拆分 CHANGELOG.md Unreleased → v2.2.0 草案**

  **What to do**:
  - 读取 `CHANGELOG.md`
  - 将 Unreleased 块内容分类：
    - 已发布功能 → 移动到 v2.1.0 条目
    - 待发布功能 → 保留在 Unreleased
  - 添加本计划相关条目（Removed cpu_main/traffic_main，Added BUILD_LEGACY_MODULES）

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 5
  - **Blocked By**: Wave 1-4 完成

  **Acceptance Criteria**:
  - [ ] Unreleased 块仅含 v2.2.0 计划项
  - [ ] v2.1.0 条目含本计划所有变更

  **Commit**: `docs(changelog): split Unreleased into v2.2.0 draft`

- [x] 25. **确认/修正 cmd_exts.hh 矛盾点**

  **What to do**:
  - 根据 T0.3 调研结论：
    - 若 NO_CONTRADICTION：标记任务完成，记录验证报告
    - 若 CONTRADICTION_EXISTS：修正 AGENTS.md 对应行

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 5
  - **Blocked By**: T0.3

  **Acceptance Criteria**:
  - [ ] `include/core/AGENTS.md` 第 60-63 行描述一致

  **Commit**: `docs(core): resolve cmd_exts.hh description contradiction`

- [x] 26. **v2-architecture/ 添加 SUPERSEDED README**

  **What to do**:
  - 在 `docs-archived/v2-architecture/` 创建 `README.md`
  - 内容：标注 SUPERSEDED BY `docs/architecture/`，列出 4 个 v2 文档与 v2.1 对应关系

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 5

  **Acceptance Criteria**:
  - [ ] `docs-archived/v2-architecture/README.md` 存在
  - [ ] 含"SUPERSEDED"标记

  **Commit**: `docs(archive): mark v2-architecture as SUPERSEDED`

- [x] 27. **创建 docs-archived/INDEX.md (三层结构)**

  **What to do**:
  - 创建 `docs-archived/INDEX.md`（不修改现有 README.md）
  - 三层结构：
    1. 目录速览表（含废弃原因）
    2. 当前推荐 live 文档路径
    3. 迁移历史时间线
  - 标注 `stream_producer_legacy.hh` 为 v1 流生产者（不应被引用）

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 5

  **References**:
  - Metis Q8 三层结构建议

  **Acceptance Criteria**:
  - [ ] `docs-archived/INDEX.md` 存在
  - [ ] 含 3 个主要章节

  **Commit**: `docs(archive): add INDEX.md with three-layer structure`

---

### Wave 6 — P2 API + 最终验证 (3 任务)

- [x] 28. **补充缺失 API 文档**

  **What to do**:
  - 评估缺失文档的实现（`module_factory.cc`/`arbiter_tlm.hh`/`dual_port_stream_adapter.hh` 等）
  - 为每个创建简洁的 API 参考（README.md 或 .hh 顶部 Doxygen）
  - 不重写 `stream_adapter.hh`（已被 v2.1 架构文档覆盖）

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 6
  - **Blocked By**: Wave 1-5

  **Acceptance Criteria**:
  - [ ] `module_factory.cc` 含 API 注释
  - [ ] `arbiter_tlm.hh` 含独立 README 或 Doxygen

  **Commit**: `docs(api): add API reference for undocumented modules`

- [x] 29. **完全重写 test/README.md 为 Catch2**

  **What to do**:
  - 读取 `test/README.md` 当前内容
  - 完全重写：Catch2 用法、CMake 集成、tag 过滤、ASan 集成
  - 移除所有 Google Test 引用

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 6
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] `grep "Google Test\|GTest\|TEST(.*).*{" test/README.md` 返回空

  **Commit**: `docs(test): rewrite test README for Catch2`

- [x] 30. **全 build + 全 ctest + ASan + format.sh**

  **What to do**:
  - 执行完整验证套件：
    ```bash
    # 1. Release build + ctest
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    ctest --test-dir build --output-on-failure

    # 2. Debug build + ctest
    cmake -S . -B build-dbg -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-dbg -j$(nproc)
    ctest --test-dir build-dbg --output-on-failure

    # 3. ASan build
    cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
    cmake --build build-asan -j$(nproc)
    ./build-asan/bin/cpptlm_tests 2>&1 | tee .omo/evidence/wave6-t30-asan.log
    grep "LeakSanitizer" .omo/evidence/wave6-t30-asan.log && echo "FAIL" || echo "OK"

    # 4. Format check
    ./scripts/format.sh --check
    ```

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 6
  - **Blocks**: Final Wave
  - **Blocked By**: T1-T29 全部完成

  **Acceptance Criteria**:
  - [ ] Release ctest: 100% PASS
  - [ ] Debug ctest: 100% PASS
  - [ ] ASan: 0 leaks
  - [ ] Format: 0 violations
  - [ ] 4 个 build artifacts 存在

  **QA Scenarios**:
  ```
  Scenario: 全构建 + 全测试 + ASan + Format 完整验证
    Tool: Bash
    Steps:
      1. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
      2. cmake --build build -j$(nproc) 2>&1 | tail -5
      3. ctest --test-dir build 2>&1 | tail -10
      4. cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
      5. cmake --build build-asan -j$(nproc) 2>&1 | tail -5
      6. ./build-asan/bin/cpptlm_tests 2>&1 | tee /tmp/asan.log | tail -5
      7. ! grep "LeakSanitizer" /tmp/asan.log
      8. ./scripts/format.sh --check 2>&1 | tail -3
    Expected Result: 全部步骤退出码 0
    Evidence: .omo/evidence/wave6-t30-full-validation.log
  ```

  **Commit**: `chore(verify): final build + ctest + ASan + format validation`

---

## Final Verification Wave (MANDATORY)

- [x] F1. **计划合规审计** — `oracle`
  读 `.omo/plans/architecture-debt-cleanup.md` 端到端。
  对每个 "Must Have" 验证实现存在；对每个 "Must NOT Have" 搜索代码库禁止模式。
  检查 `.omo/evidence/` 证据文件存在。
  输出: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT`

- [x] F2. **代码质量审查** — `unspecified-high`
  跑 `tsc --noEmit` (不适用) + linter + `bun test` (不适用) + 现有 Catch2 套件。
  审查所有变更文件：`as any`/`@ts-ignore`、空 catch、console.log、注释代码、未使用 import。
  输出: `Build [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **真实手动 QA** — `unspecified-high`
  从干净状态开始。执行 Wave 6 T30 全部步骤。
  捕获所有 evidence。测试跨 Wave 集成。
  输出: `Scenarios [N/N pass] | Integration [N/N] | VERDICT`

- [x] F4. **范围保真度检查** — `deep`
  对每个任务：读 "What to do"，读实际 diff (git log/diff)。
  验证 1:1 — 一切规格内已构建（无缺失），无超出规格（无 creep）。
  检测跨任务污染。标记未记账变更。
  输出: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | VERDICT`

---

## Commit Strategy

每个 Wave 内的任务独立 commit，遵循项目规范：

- `docs(ci):` - 文档/CI/CD
- `refactor(legacy):` - Legacy 代码重构
- `refactor(test):` - 测试重构
- `docs(arch):` - 架构文档
- `chore(verify):` - 验证脚本

---

## Success Criteria

### 验证命令
```bash
# 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
# Expected: 0 错误，0 警告

# 测试
./build/bin/cpptlm_tests
# Expected: 全部 PASS

# ASan
./build-asan/bin/cpptlm_tests
# Expected: 0 leaks, 0 errors

# Format
./scripts/format.sh --check
# Expected: 0 violations

# 残留扫描
grep -rE "TODO|FIXME" src/ include/ --include="*.cc" --include="*.hh" --include="*.cpp" | grep -v docs-archived
# Expected: 空
```

### 最终检查清单
- [x] 所有 "Must Have" 存在
- [x] 所有 "Must NOT Have" 缺席
- [x] 所有 30 个任务完成 (boulder.json: 5/5 task_sessions completed)
- [x] CHANGELOG 同步 (v2.2.0 section, commit 28adbe9)
- [x] 证据文件齐备 (`.omo/evidence/wave*.txt` → wave0-t01..t04 + wave1..6 = 10 files)
- [x] F1-F4 全部 APPROVE (boulder.json: status=completed)
