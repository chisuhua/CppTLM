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
- **失败即降 -j1** (避免 524s 阻塞)
- 记录位置: `external/PTX-EMU/.worktrees/hsk-9-impl/build/` (非 sm-mp-impl submodule 视角, 同 SHA baseline 数据有效)
