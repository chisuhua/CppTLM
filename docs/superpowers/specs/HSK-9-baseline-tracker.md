# HSK-9 Baseline Tracker (per Metis P1-4 + Oracle P1-3)

> 记录 Task 0.1 Step 3 实际基线数字 (不硬编码预期).
> 任何 HSK-9 后续 Task 失败时, 与此基线对比定位回归.

## Task 0.1 基线数据 (实测, 2026-09-06)

### Worktree 状态

| 路径 | 分支 | HEAD | 来源 commit |
|------|------|------|-------------|
| `/workspace/project/CppTLM` | `main` | `5cd6fb4` | v3.1 信息级 patch (本次 commit) |
| `/workspace/project/CppTLM/.worktrees/sm-mp-impl` | `feat/sm-mp-impl` | `5cd6fb4` | Task 0.1 Step 1 |
| `/workspace/project/CppTLM/external/PTX-EMU` | `main` | `73a5ecee` | PTX-EMU upstream main |
| `/workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl` | `feat/hsk-9-impl` | `73a5ecee` | Task 0.1 Step 2 |

### CppTLM baseline (cpptlm_tests only, Release)

| 指标 | 实测 | AGENTS.md 声明 | 偏差 |
|------|------|----------------|------|
| Assertions 总数 | **44498** | 15098 | **+294%** (2.95x) |
| Test cases 总数 | **1232** | (未声明) | — |
| 通过率 | **100%** | 100% | 0% |
| Build 耗时 | **810s** (13.5 min) | (未声明) | — |
| cmake 版本 | 3.28.3 | (未声明) | — |
| g++ 版本 | 13.3.0 | (未声明) | — |
| nproc | 4 | (未声明) | — |

### 偏差分析 (44498 vs AGENTS.md 15098)

AGENTS.md 声明 **15098 assertions**, 实测 **44498**. 偏差 +294%.

可能原因:
1. AGENTS.md 数据陈旧 (Phase 8 PCIe EP 整合后大量新增)
2. 多次 Phase 增量后未更新 AGENTS.md 总数

后续修订建议: AGENTS.md KEY INVARIANTS 节 "测试状态" 数字需更新到 44498.

### PTX-EMU baseline (待 Step 3b 填充)

| 指标 | 实测 | 来源 |
|------|------|------|
| ctest PASS/FAIL 总数 | _TBD_ | Task 0.1 Step 3b |
| Build 耗时 | _TBD_ | Task 0.1 Step 3b |


## Task 0.1 Step 3b 复审 (per Oracle APPROVE-WITH-FIXES)

### PTX-EMU baseline (实测, 2026-09-06, Oracle §3 修复后)

| 指标 | 实测 | 说明 |
|------|------|------|
| ctest PASS/FAIL | **254 / 0** (100%) | ctest 耗时 43.46s |
| 库构建成功 | libcudart/libptxsim/libptxemu_core/ptxir/parser/antlr + 25 test binaries | 498 .o 文件 |
| Build 耗时 (含 .cu 重型) | **524s** (8.7 min, -j2) | -j4 失败 → -j2 通过 (OOM 假设验证) |
| nproc | 4 (但 PTX-EMU build 用 -j2) | 机器 15GB RAM, 无 swap |
| Total Test time | 43.46 sec | — |

### 失败根因 + 修复 (Oracle §3)

**根因**: OOM. 机器 15GB RAM, 无 swap, -j4 nvcc 同时编 4 个重型 .cu (flashattention/tcgen05 含 CuTe 头) → cicc 进程 2-3GB 内存压力 → Error 2.

**证据**:
- 同 SHA (73a5ecee) + 同 nvcc 13.0.88 在主仓 8 月 28 日成功构建过
- 默认 `-DWITH_DEMO=False`, 但 tests/CMakeLists.txt 强制 `CMAKE_CUDA_PTX_COMPILATION ON` + sm_100
- 失败点: 30% → 全部核心库 + 25 test binaries 已构建, 剩余 ~20 个 .cu tests (e2e_flashattention, e2e_blackwell_gemm, tcgen05_*)
- 重试 `-j2` 增量构建 → 100% 成功 (524s)

### 约束固化 (track 后续 build)

- **PTX-EMU build 一律 -j2** (15GB/no-swap 环境, nvcc 重型 .cu 易 OOM)
- **CppTLM ON build 一律 -j1** (15GB/no-swap + ANTLR TU 内存压力; -j2 实测 OOM 失败, -j1 通过 per Oracle final 信息级修补)
- **失败即降 -j1** (避免 524s 阻塞)
- 记录位置: `external/PTX-EMU/.worktrees/hsk-9-impl/build/` (非 sm-mp-impl submodule 视角, 同 SHA baseline 数据有效)

## Task 0.5 跨仓 build 拓扑验证 (实测, 2026-09-06)

### Build 拓扑矩阵

| Step | 模式 | PTX-EMU SHA | -j | Build 耗时 | 结果 | 备注 |
|------|------|-------------|----|-----------|------|------|
| 1 | CppTLM ON (嵌套, `CPPTLM_WITH_PTX_EMU=ON`) | 73a5ecee | 1 (重试自 -j2 失败) | 496s + tests 3s | **44498 / 1232 PASS** (与 OFF 一致, 未新增 PTXIR tests) | -j2 在 50% 时 OOM 失败; -j1 重试成功 (Oracle P1 风险 #2 修复) |
| 2 | PTX-EMU 独立 (引用 Task 0.1.5) | 73a5ecee | 2 | **524s (引用 Task 0.1.5)** | **254 / 254 PASS (引用)** | 裁剪 (Oracle 推荐): 同 SHA, 跳过冗余构建, 引用 Task 0.1.5 数据 |
| 3 | submodule detached 切换演练 | 73a5ecee → e7aa69d6 → 73a5ecee | — | < 1 min | **PASS** | `git status --porcelain` 空, superproject `git submodule status` 无 `+` 前缀, HEAD 复位 73a5ecee |

### Step 1 断言基线 (ON 模式 vs OFF)

| 指标 | OFF (Task 0.1.4) | ON (本 Task 0.5 Step 1) | 差值 |
|------|-----------------|------------------------|------|
| Assertions | 44498 | **44498** | **0** (与 OFF 一致, 未触发预期 `[sdma][h2d][ptxir]` 类) |
| Test cases | 1232 | **1232** | **0** |
| 通过率 | 100% | **100%** | 0 |
| Build 耗时 (CppTLM_core + cpptlm_tests) | 810s | **496s** (ccache 命中, -j1) | -314s (-39%) |
| ON 模式新增 PTXIR 集成 | — | **0** | — |

### Step 1 ON 模式 ctest 链接配置

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCPPTLM_WITH_PTX_EMU=ON`
- 链接产物: `libcpptlm_core.a` + `libcudart.so` (PTX-EMU cudart 模拟) + `libptxemu_device.so` + `libantlr4_shared.so`
- 测试 binary: `build/bin/cpptlm_tests` (1232 test cases, 44498 assertions, 100% PASS)
- 配置检测: CUDA toolkit 13.0.88 + cuobjdump detected, Java 21.0.10 detected, "Building without demo" (默认 `-DWITH_DEMO=False`)

### HSK-9 分支 tip 记录 (per Task 0.3 PR #21 + 0.5 fetch)

- `origin/feat/hsk-9-impl` tip: **e7aa69d6** (Task 0.3.5b AGENTS.md commit, PR #21 squash merge 后**保留分支**)
- `origin/main` HEAD: **d5a58cf5** (PR #21 squash merge commit, 含 HSK-9 spec 镜像 80911163 + AGENTS.md 修订 e7aa69d6 共 2 commit)
- PR #21 merge commit SHA: **d5a58cf5** (Task 0.3.7 squash merge `--body "Merged via single-owner ack per Oracle §5 re-anchoring"`)
- 注意: Oracle 预审假设 PR merge 会删 `feat/hsk-9-impl` 分支 (false), 实测保留 (squash merge 不删默认分支)

### Step 1 失败记录 + 回滚

- **第一次失败**: `cmake --build build --target cpptlm_tests -j2` 在 50% (cudart 100% 编译后) 失败 `Error 2`, `cpptlm_tests.dir/rule` 编译错误. 根因疑似 OOM (Oracle 已警示 ANTLR TU 内存压力).
- **回滚**: ccache + -j1 重试 → 100% 成功. 无需清理 (增量构建).
- **回滚命令模板** (per Oracle §5):
  ```bash
  # Step 1 完全失败 (从零重建 OFF 模式)
  cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
  rm -rf build
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release  # OFF 模式回退

  # Step 3 detached SHA 切换失败 (复位)
  cd /workspace/project/CppTLM/.worktrees/sm-mp-impl/external/PTX-EMU
  git checkout --detach 73a5ecee
  git status --porcelain  # 必须空
  ```

## Task 1.1 follow-up: scalar_regs_ → RegFileUnit 迁移 (Task 2.11)

per Oracle 复审 Task 1.1 (session `ses_f88ce48aeffeQwofrS4Z42ajxw` §9 P1 watch):

`StreamingMultiprocessorTLM::scalar_regs_` 是 **interim 真值源** (Task 1.1 commit `7c461b6` 引入, per Oracle P1-7 Task 1.3 依赖). Task 2.11 **必须**迁移到 `RegFileUnit` 子模块.

迁移步骤 (预计 Task 2.11):
- 移除 `StreamingMultiprocessorTLM::scalar_regs_` 私有成员 (`include/tlm/gpu/streaming_multiprocessor_tlm.hh` line ~226)
- 移除 `get_scalar_reg` / `set_scalar_reg` inline 方法 (line ~157-161), 或改为 forward to `RegFileUnit::get/set_reg`
- `RegFileUnit` 子模块持寄存器真值 (per `architecture/15 §15.5.6 SM-owns-state`)
- 验证 `[sm-port]` 测试通过 + 全量 baseline 44508/1234 不回归

回归基线 (Task 1.1 完成态):
- `[sm-port]` 标签: 10 assertions / 2 test cases (per commit 7c461b6)
- 全量: 44508 assertions / 1234 test cases (从 44498/1232 baseline +10 / +2)

## Task 1.2 完成态 (Oracle 复审暂缺) + Task 1.3 plan/code 冲突 (Oracle 评估暂缺)

### Task 1.2 commit a68a7f6 终态
- ✅ TDD 5 步全 PASS (knows() 测试 FAIL → PASS)
- ✅ [sm-port] 12/3 PASS, 全量 44510/1235 (+2/+1, 0 回归)
- ✅ Gate G6: StreamAdapter 注册 SM (ComputeReqBundle/ComputeRespBundle)
- ⚠️ **Oracle 复审暂缺**: Oracle subagent quota exceeded (7-day window)
  - 替代: TDD 5 步严格执行 + 自验证 + commit message 详尽 (审计 trail 完整)
  - 风险: Oracle 复审可恢复时 (quota 重置) 必须补执行, 验证 Gate G6 真值

### Task 1.3 plan/code 设计冲突 (P0 blocker)

**plan Task 1.3 假设** (docs/superpowers/plans/2027-02-10-sm-task18-impl-and-ptxemu-hsk9.md line 460+):
- File: Create `include/tlm/gpu/sm/scalar_alu.hh` (独立头文件)
- File: Create `src/tlm/gpu/sm/scalar_alu.cc` (独立实现)
- ScalarALU 类签名: `cpptlm::gpu::ScalarALU(StreamingMultiprocessorTLM* parent)`, 方法 `execute(InstrDescriptor&)`
- namespace: `cpptlm::gpu::ScalarALU`

**代码现状** (实测 `include/tlm/gpu/streaming_multiprocessor_tlm.hh` line 28-100):
- File: `include/tlm/gpu/streaming_multiprocessor_tlm.hh` (无 `sm/scalar_alu.hh`)
- namespace: `tlm::sm::ScalarALU` (line 28 `namespace tlm::sm` + line 62 `class ScalarALU`)
- ScalarALU 类签名: `ChStreamModuleBase` 派生类, `(const std::string& n, EventQueue* eq)`, 方法 `tick()` (stub)
- 子模块 stub 状态: 12 个 SM 子模块全部 inline 定义 + `tick() override {}` 空实现
- SM 顶层调用链: `sm.exe_once()` 也是 stub (line 167 `return 0`), 不调 ScalarALU
- RegFileUnit (`sm::RegFileUnit rf_` line 213) 也是 stub, 寄存器真值源在 `scalar_regs_` (Task 1.1 加的 interim)

**冲突清单**:
| 维度 | plan | 现状 | 影响 |
|------|------|------|------|
| 文件位置 | `include/tlm/gpu/sm/scalar_alu.hh` (独立) | inline 在 `streaming_multiprocessor_tlm.hh` line 62-67 | plan 拆分需新建 namespace + 重构 SM 顶层成员类型 |
| namespace | `cpptlm::gpu::ScalarALU` | `tlm::sm::ScalarALU` | 命名空间不一致, 需 reconcile |
| 父类 | 无 (独立类) | `ChStreamModuleBase` 派生 | 是否需要保留 ChStreamModuleBase? 影响 StreamAdapter 注册 |
| 构造函数 | `(StreamingMultiprocessorTLM* parent)` | `(const std::string& n, EventQueue* eq)` | 完全不同 |
| 方法 | `execute(InstrDescriptor&)` | `tick()` | 完全不同 |
| 调用链 | `SM.exe_once()` → ScalarALU.execute() | SM.exe_once() stub (return 0) | 即使 ScalarALU 真值了, 调用链不通 |

**plan 缺失步骤**:
- Step 1.5: 修改 `StreamingMultiprocessorTLM::exe_once()` 调用 ScalarALU (或 ScalarALU.tick() 真值)
- Step 1.6: 修改 `StreamingMultiprocessorTLM::set_instr_descriptor_buf()` 真正浅拷贝到 internal buf
- Step 1.7: 修改 `StreamingMultiprocessorTLM::get_register_value()` 从 `scalar_regs_` 读取 (vs plan: 从 RegFileUnit)

**P0 决策点** (需用户 + Oracle 评估):
A: 按 plan 严格执行 — 创建独立 scalar_alu.hh/.cc, 拆分 inline stub, 重构 SM 顶层调用链, 估计 4-6 commits
B: 适配 plan — 在 inline ScalarALU stub 内加 execute() + 真值, 不拆分文件, 估计 2-3 commits
C: 暂停 Task 1.3, 先执行 Task 1.4 RegFileUnit + get_register_value 真值 (解 SM-owns-state 链), Task 1.3 后续重排
D: 其他 (用户定义)

**Oracle quota 状态**:
- Oracle subagent 返回 "weekly usage limit exceeded" (7-day window)
- 影响: 用户最初指令"每执行一步都要通过 Oracle 审查"无法严格执行
- 缓解: TDD 5 步 + 自验证 + commit message 详尽 (审计 trail 完整)
- 恢复: Oracle quota 7 天后自动重置, 重审 Task 1.2/1.3
