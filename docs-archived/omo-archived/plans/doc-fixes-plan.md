# CppTLM 文档修正与更新计划

## TL;DR

> **目标**: 修正文档中的路径引用错误、统一状态标注、添加 Git  hygiene 配置
>
> **交付物**:
> - 修正 5 个文件中的路径引用错误
> - 统一 FragmentBundle 状态标注
> - 新增 `.gitignore` 忽略 `.omo/` 工作目录
>
> **预估工作量**: Quick (~30 分钟)
> **并行执行**: YES - Wave 1 (6 任务并行) + Wave 2 (2 任务串行) + Final Wave (4 验证并行)
> **关键路径**: Wave 1 (6 任务并行) → Wave 2 (2 任务串行) → Final Wave (4 验证并行)

---

## Context

### 原始请求
用户要求分析项目下的修改并给出文档修正和更新计划。

### 验证发现
通过全量扫描和代码库验证，发现以下问题：

1. **路径引用错误**: 5 个文件仍引用 `docs-pending/02-architecture/`（目录为空或不存在）
2. **PRD-002 路径错误**: 2 处引用不存在的 `../02-architecture/` 路径
3. **FragmentBundle 状态**: 文档声称在 `noc_bundles_tlm.hh` 中，但代码库无实现
4. **Git hygiene**: `.omo/` 工作目录未加入 `.gitignore`

### Metis 审查要点
- 需确认路径修正的完整范围（已验证）
- 需明确不修改实质性内容（已确认）
- 需验证 FragmentBundle 实际状态（已验证：无代码实现）

---

## Work Objectives

### Core Objective
修正文档中所有错误的路径引用，统一状态标注，添加 Git 忽略规则。

### Concrete Deliverables
- `docs/adr/ADR-X-SUMMARY.md` - 修正 `docs-pending/02-architecture/` 路径
- `docs/architecture/examples/README.md` - 修正 `docs-pending/02-architecture/` 路径
- `docs/guide/DEVELOPER_GUIDE.md` - 修正 `docs-pending/02-architecture/` 路径
- `docs/requirements/PRD-001-hybrid-simulation-platform.md` - 修正 2 处路径
- `docs/requirements/PRD-002-final.md` - 修正 2 处路径
- `docs/architecture/01-hybrid-architecture-v2.1.md` - 修正 FragmentBundle 路径声明
- `.gitignore` - 添加 `.omo/` 忽略规则

### Definition of Done
- [x] `grep -r "docs-pending" docs/ --include="*.md"` 返回空
- [x] `grep "02-architecture/2026-04-08" docs/requirements/PRD-002-final.md` 返回空
- [x] `grep "02-architecture/多层次混合仿真" docs/requirements/PRD-002-final.md` 返回空
- [x] FragmentBundle 状态与代码实际一致
- [x] `.gitignore` 包含 `.omo/`
- [x] `git ls-files | grep "\.omo/"` 返回空

### Must Have
- 所有路径引用指向存在的文件或目录
- FragmentBundle 状态反映代码实际（无实现）
- `.omo/` 被 `.gitignore` 忽略
- 提交信息遵循项目规范

### Must NOT Have (Guardrails)
- 不修改任何代码文件（include/, src/, test/）
- 不修改配置文件（configs/）
- 不创建新文档文件
- 不修改 ADR 的决策结论内容（允许修正过时的路径引用）
- 不修改文档的实质性内容（仅修正路径和状态标签）

---

## Verification Strategy

### Test Decision
- **Infrastructure exists**: NO (文档变更，无代码)
- **Automated tests**: None
- **Framework**: None
- **验证方式**: Agent QA via grep + 路径存在性检查

### QA Policy
每个任务完成后通过 grep 验证路径修正结果，确认引用文件存在。

---

## Execution Strategy

### 执行顺序

```
Wave 1 (并行启动 - 6 个独立任务):
├── 任务 1: ADR-X-SUMMARY.md 路径修正
├── 任务 2: architecture/examples/README.md 路径修正
├── 任务 3: DEVELOPER_GUIDE.md 路径修正
├── 任务 4: PRD-001 路径修正
├── 任务 7: FragmentBundle 状态修正 (可并行，独立 commit)
└── 任务 8: 添加 .omo/ 到 .gitignore (可并行，独立 commit)

Wave 2 (顺序执行 - 同一文件修改):
├── 任务 5: PRD-002 line 260 路径移除
└── 任务 6: PRD-002 line 261 路径修正

验证波次 (Final Wave - 4 个并行验证):
├── F1: 路径扫描验证
├── F2: 文件存在性验证
├── F3: Git 状态验证
└── F4: 范围合规检查
```

---

## TODOs

- [x] 1. 修正 ADR-X-SUMMARY.md 路径引用

  **What to do**:
  - 文件: `docs/adr/ADR-X-SUMMARY.md`
  - 位置: 第 231 行
  - 内容: `docs-pending/02-architecture/` → `docs/architecture/`
  - 该路径出现在目录树示例中，应指向当前的 `docs/architecture/`

  **Must NOT do**:
  - 不修改 ADR 的决策结论内容
  - 不修改目录树中的其他路径

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 单文件单处修改，直接编辑即可
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (与 Tasks 2-4, 8 并行执行)
  - **Blocks**: Commit 1 完成
  - **Blocked By**: None

  **References**:
  - `docs/adr/ADR-X-SUMMARY.md:231` - 包含 `docs-pending/02-architecture/` 的目录树示例

  **Acceptance Criteria**:
  - [ ] `grep "docs-pending" docs/adr/ADR-X-SUMMARY.md` 返回空

  **QA Scenarios**:
  ```
  Scenario: 验证路径已修正
    Tool: Bash (grep)
    Steps:
      1. grep -n "docs-pending" docs/adr/ADR-X-SUMMARY.md
    Expected Result: 无输出（0 匹配）
    Evidence: .omo/evidence/task-1-path-fixed.txt
  ```

  **Commit**: YES (Commit 1)
  - Message: `docs: fix stale docs-pending path references`
  - Files: `docs/adr/ADR-X-SUMMARY.md`

- [x] 2. 修正 architecture/examples/README.md 路径引用

  **What to do**:
  - 文件: `docs/architecture/examples/README.md`
  - 位置: 第 3 行
  - 内容: `docs-pending/02-architecture/examples/` → `docs/architecture/examples/`

  **Must NOT do**:
  - 不修改 README 的其他内容

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Commit 1 完成
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] `grep "docs-pending" docs/architecture/examples/README.md` 返回空

  **QA Scenarios**:
  ```
  Scenario: 验证路径已修正
    Tool: Bash (grep)
    Steps:
      1. grep -n "docs-pending" docs/architecture/examples/README.md
    Expected Result: 无输出
    Evidence: .omo/evidence/task-2-path-fixed.txt
  ```

  **Commit**: YES (Commit 1)
  - Files: `docs/architecture/examples/README.md`

- [x] 3. 修正 DEVELOPER_GUIDE.md 路径引用

  **What to do**:
  - 文件: `docs/guide/DEVELOPER_GUIDE.md`
  - 位置: 第 158 行
  - 内容: `docs-pending/02-architecture/` → `docs/architecture/`

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Commit 1 完成
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] `grep "docs-pending" docs/guide/DEVELOPER_GUIDE.md` 返回空

  **QA Scenarios**:
  ```
  Scenario: 验证路径已修正
    Tool: Bash (grep)
    Steps:
      1. grep -n "docs-pending" docs/guide/DEVELOPER_GUIDE.md
    Expected Result: 无输出
    Evidence: .omo/evidence/task-3-path-fixed.txt
  ```

  **Commit**: YES (Commit 1)
  - Files: `docs/guide/DEVELOPER_GUIDE.md`

- [x] 4. 修正 PRD-001 路径引用

  **What to do**:
  - 文件: `docs/requirements/PRD-001-hybrid-simulation-platform.md`
  - 位置: 
    - 第 149 行: `docs-pending/02-architecture/ALL_DISCUSSIONS_COMBINED.md` → `docs-archived/01-ARCHITECTURE/ALL_DISCUSSIONS_COMBINED.md`
    - 第 150 行: `docs-pending/05-legacy/` → 移除或改为 `docs-archived/`

  **关键决策**:
  - ALL_DISCUSSIONS_COMBINED.md 实际位于 `docs-archived/01-ARCHITECTURE/`
  - docs-pending/05-legacy/ 目录不存在，应移除该行

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Commit 1 完成
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] `grep "docs-pending" docs/requirements/PRD-001-hybrid-simulation-platform.md` 返回空
  - [ ] `grep "ALL_DISCUSSIONS_COMBINED" docs/requirements/PRD-001-hybrid-simulation-platform.md` 显示正确路径

  **QA Scenarios**:
  ```
  Scenario: 验证路径已修正
    Tool: Bash (grep)
    Steps:
      1. grep -n "docs-pending" docs/requirements/PRD-001-hybrid-simulation-platform.md
      2. grep -n "ALL_DISCUSSIONS_COMBINED" docs/requirements/PRD-001-hybrid-simulation-platform.md
    Expected Result: (1) 无输出; (2) 显示 docs-archived/ 路径
    Evidence: .omo/evidence/task-4-prd1-fixed.txt
  ```

  **Commit**: YES (Commit 1)
  - Files: `docs/requirements/PRD-001-hybrid-simulation-platform.md`

- [x] 5. 修正 PRD-002 第 260 行路径引用

  **What to do**:
  - 文件: `docs/requirements/PRD-002-final.md`
  - 位置: 第 260 行
  - 内容: `../02-architecture/2026-04-08-architecture-slow-cycle.md`
  - 问题: 文件不存在（`docs-pending/02-architecture/` 为空目录）
  - 处理: 移除该引用行或更新为正确的归档路径

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2
  - **Blocks**: Wave 2 完成
  - **Blocked By**: Wave 1 complete

  **Acceptance Criteria**:
  - [ ] `grep "2026-04-08-architecture-slow-cycle" docs/requirements/PRD-002-final.md` 返回空

  **QA Scenarios**:
  ```
  Scenario: 验证路径已移除
    Tool: Bash (grep)
    Steps:
      1. grep -n "2026-04-08-architecture-slow-cycle" docs/requirements/PRD-002-final.md
    Expected Result: 无输出
    Evidence: .omo/evidence/task-5-prd2-260.txt
  ```

  **Commit**: YES (Commit 3)
  - Files: `docs/requirements/PRD-002-final.md`

- [x] 6. 修正 PRD-002 第 261 行路径引用

  **What to do**:
  - 文件: `docs/requirements/PRD-002-final.md`
  - 位置: 第 261 行（或相邻行）
  - 内容: `../02-architecture/多层次混合仿真.md`
  - 问题: 文件实际位于 `docs/architecture/多层次混合仿真.md`
  - 处理: 修正为 `../architecture/多层次混合仿真.md`

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2
  - **Blocks**: Wave 2 完成
  - **Blocked By**: Task 5 (同一文件修改，行号依赖)

  **Acceptance Criteria**:
  - [ ] `grep "02-architecture/多层次混合仿真" docs/requirements/PRD-002-final.md` 返回空
  - [ ] `grep "architecture/多层次混合仿真" docs/requirements/PRD-002-final.md` 返回匹配

  **QA Scenarios**:
  ```
  Scenario: 验证路径已修正
    Tool: Bash (grep)
    Steps:
      1. grep -n "多层次混合仿真" docs/requirements/PRD-002-final.md
    Expected Result: 显示 ../architecture/ 而非 ../02-architecture/
    Evidence: .omo/evidence/task-6-prd2-261.txt
  ```

  **Commit**: YES (Commit 3)
  - Files: `docs/requirements/PRD-002-final.md`

- [x] 7. 修正 FragmentBundle 状态标注

  **What to do**:
  - 文件: `docs/architecture/01-hybrid-architecture-v2.1.md`
  - 位置: 约第 399 行（组件状态表格）
  - 当前状态: `| FragmentBundle | include/bundles/noc_bundles_tlm.hh | ⚠️ 部分实现 | NoC Bundle 中含 priority/vc_id |`
  - 问题: 
    1. FragmentBundle 不在 `noc_bundles_tlm.hh` 中（代码库搜索无实现）
    2. 状态标注与代码实际不一致
  - 处理: 修正路径声明为实际位置（如果存在）或标注为 ❌ 未实现/未来扩展

  **关键决策**:
  - 代码库中无 FragmentBundle 实现
  - 应修正为: `| FragmentBundle | N/A | ❌ 未实现 | 规划中，暂无代码实现 |`

  **Must NOT do**:
  - 不创建 FragmentBundle 的代码实现
  - 不修改其他 Bundle 的状态

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (与 Tasks 1-4, 8 并行执行)
  - **Blocks**: Commit 2 完成
  - **Blocked By**: None

  **References**:
  - `docs/architecture/01-hybrid-architecture-v2.1.md:~399` - FragmentBundle 状态行
  - `include/bundles/` 目录内容（无 fragment_bundles 文件）

  **Acceptance Criteria**:
  - [ ] FragmentBundle 行显示正确状态（❌ 未实现 或类似）
  - [ ] 路径列显示 N/A 而非错误文件路径

  **QA Scenarios**:
  ```
  Scenario: 验证 FragmentBundle 状态已修正
    Tool: Bash (grep)
    Steps:
      1. grep -A1 "FragmentBundle" docs/architecture/01-hybrid-architecture-v2.1.md
    Expected Result: 显示 "N/A" 或 "未实现"，不显示 "noc_bundles_tlm.hh"
    Evidence: .omo/evidence/task-7-fragment-status.txt
  ```

  **Commit**: YES (Commit 2)
  - Message: `docs: correct FragmentBundle status in v2.1`
  - Files: `docs/architecture/01-hybrid-architecture-v2.1.md`

- [x] 8. 添加 .omo/ 到 .gitignore

  **What to do**:
  - 文件: `.gitignore`（如果不存在则创建）
  - 内容: 添加 `.omo/` 行
  - 当前状态: `.omo/` 不在 .gitignore 中，0 个被跟踪文件

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (与 Tasks 1-4 并行启动)
  - **Blocks**: 无（独立 commit）
  - **Blocked By**: None

  **Acceptance Criteria**:
  - [ ] `.gitignore` 文件存在且包含 `.omo/`
  - [ ] `git ls-files | grep "\.omo/"` 返回 0

  **QA Scenarios**:
  ```
  Scenario: 验证 .gitignore 包含 .omo/
    Tool: Bash (grep + git)
    Steps:
      1. grep "\.omo" .gitignore
      2. git ls-files | grep "\.omo/"
    Expected Result: (1) 有匹配; (2) 无输出
    Evidence: .omo/evidence/task-8-gitignore.txt
  ```

  **Commit**: YES (Commit 4)
  - Message: `chore: add .omo/ to .gitignore`
  - Files: `.gitignore`

---

## Final Verification Wave

> 4 个验证任务按顺序执行。全部通过后才能提交。

- [x] F1. **路径扫描验证**

  **Output**: `docs-pending [0] | PRD-002-p260 [0] | PRD-002-p261 [0] | VERDICT: PASS`

- [x] F2. **文件存在性验证**

  **Output**: `ALL_DISCUSSIONS_COMBINED.md [EXISTS] | 多层次混合仿真.md [EXISTS] | VERDICT: PASS`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 纯命令执行验证
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Final Wave (与 F1, F3-F4 并行启动)
  - **Blocks**: Final Wave 汇总
  - **Blocked By**: All commits complete

  **Acceptance Criteria**:
  - [ ] ALL_DISCUSSIONS_COMBINED.md 存在于 docs-archived/01-ARCHITECTURE/
  - [ ] 多层次混合仿真.md 存在于 docs/architecture/

  **Output**: `Files [N/N exist] | VERDICT: PASS/FAIL`

- [x] F3. **Git 状态验证**

  **Output**: `.omo/ in .gitignore [YES] | git ls-files | grep ".omo/" [0] | VERDICT: PASS`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 纯命令执行验证
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Final Wave (与 F1-F2, F4 并行启动)
  - **Blocks**: Final Wave 汇总
  - **Blocked By**: All commits complete

  **Acceptance Criteria**:
  - [ ] `.gitignore` 包含 `.omo/`
  - [ ] `git ls-files | grep "\.omo/"` 返回 0
  - [ ] 无代码文件被修改

  **Output**: `.gitignore [OK/FAIL] | Tracked [0] | Code changes [0] | VERDICT: PASS/FAIL`

- [x] F4. **范围合规检查**

  **What to do**:
  确认只有预期的文件被修改：
  ```bash
  git diff --name-only
  ```
  Expected files only:
  - docs/adr/ADR-X-SUMMARY.md
  - docs/architecture/examples/README.md
  - docs/guide/DEVELOPER_GUIDE.md
  - docs/requirements/PRD-001-hybrid-simulation-platform.md
  - docs/requirements/PRD-002-final.md
  - docs/architecture/01-hybrid-architecture-v2.1.md
  - .gitignore

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 纯命令执行验证
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Final Wave (与 F1-F3 并行启动)
  - **Blocks**: Final Wave 汇总
  - **Blocked By**: All commits complete

  **Acceptance Criteria**:
  - [ ] 修改文件列表与预期完全一致
  - [ ] 无意外文件被修改

  **Output**: `Files [N/N expected] | Unexpected [0] | VERDICT: PASS/FAIL`

---

## Commit Strategy

- **Commit 1 (Wave 1)**: `docs: fix stale docs-pending path references`
  - Files: `docs/adr/ADR-X-SUMMARY.md`, `docs/architecture/examples/README.md`, `docs/guide/DEVELOPER_GUIDE.md`, `docs/requirements/PRD-001-hybrid-simulation-platform.md`

- **Commit 2 (Wave 1)**: `docs: correct FragmentBundle status in v2.1`
  - File: `docs/architecture/01-hybrid-architecture-v2.1.md`

- **Commit 3 (Wave 2)**: `docs: fix PRD-002 architecture path references`
  - File: `docs/requirements/PRD-002-final.md`

- **Commit 4 (Wave 1)**: `chore: add .omo/ to .gitignore`
  - File: `.gitignore`

---

## Success Criteria

### Verification Commands
```bash
# 验证无残留 docs-pending 引用
grep -r "docs-pending" docs/ --include="*.md" | wc -l  # Expected: 0

# 验证 PRD-002 路径已修正
grep "02-architecture/2026-04-08" docs/requirements/PRD-002-final.md | wc -l  # Expected: 0
grep "02-architecture/多层次混合仿真" docs/requirements/PRD-002-final.md | wc -l  # Expected: 0

# 验证 FragmentBundle 状态正确
grep "FragmentBundle" docs/architecture/01-hybrid-architecture-v2.1.md  # Expected: 显示正确状态

# 验证 .gitignore
grep "\.omo" .gitignore | wc -l  # Expected: >= 1

# 验证 .omo/ 未被跟踪
git ls-files | grep "\.omo/" | wc -l  # Expected: 0
```

### Final Checklist
- [x] 所有 "Must Have" 满足
- [x] 所有 "Must NOT Have" 未触发
- [x] 验证命令全部通过
- [x] 提交信息符合规范
- [x] 无代码文件被修改
