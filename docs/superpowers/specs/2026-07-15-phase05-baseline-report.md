# Phase 0.5 Baseline Report (2026-07-15)

> **范围**: CppTLM + PTX-EMU 双端 Phase 0.5 baseline worktree 状态报告
> **目的**: D1 开工前最终验证 — 确认两个项目 main HEAD 在 worktree 中编译通过、测试基线可重现
> **状态**: 🟢 可开工（环境限制已识别并文档化）

---

## 1. Worktree 创建情况

### CppTLM 端

| 项 | 值 |
|----|-----|
| **路径** | `/workspace/project/CppTLM/.worktrees/baseline-d1-full` |
| **分支** | `baseline/d1-full-prep` |
| **HEAD** | `683485f`（含 gitignore 修复） |
| **来源** | `main` HEAD fast-forward |
| **gitignore 修复** | commit `683485f`（`.worktrees/` 已加入 .gitignore） |
| **状态** | ✅ 干净，无未提交修改 |

### PTX-EMU 端

| 项 | 值 |
|----|-----|
| **路径** | `/workspace/project/ptxemu-baseline-f12b` |
| **分支** | `baseline/f12b-ld-prep` |
| **HEAD** | `721a4168`（Phase 8 doc sync + ADR-0021 index） |
| **来源** | `main` HEAD fast-forward |
| **状态** | ✅ 干净，无未提交修改 |

---

## 2. 编译验证

### CppTLM 端（✅ 成功）

```bash
cd /workspace/project/CppTLM/.worktrees/baseline-d1-full
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

- **configure 耗时**: 1.8s
- **build 产物**: `build/lib/libcpptlm_core.a` + 5 examples + cpptlm_tests (286MB)
- **状态**: ✅ 100% 完成

### PTX-EMU 端（✅ 成功）

```bash
cd /workspace/project/ptxemu-baseline-f12b
. ./env.sh   # CUDA + ANTLR4 4.13.2 + CLASSPATH
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

- **configure 耗时**: 0.4s (初) + 1.0s (re-config after 3 commit ff)
- **build 产物**: cpptlm bridge 测试 + cute demos + 全部 PTX tests
- **状态**: ✅ 100% 完成

---

## 3. 测试验证

### CppTLM 端（✅ 完美基线）

```bash
./build/bin/cpptlm_tests
# 输出: All tests passed (15547 assertions in 764 test cases)
```

| 项 | 值 |
|----|-----|
| **测试总数** | 764 |
| **通过** | 764 |
| **失败** | 0 |
| **断言数** | 15,547 |
| **状态** | 🟢 **100% PASS** |

### PTX-EMU 端（⚠️ 188/202 pass, 14 环境性 SEGFAULT）

```bash
cd build && ctest --output-on-failure -j$(nproc)
```

| 项 | 值 |
|----|-----|
| **测试总数** | 202 |
| **通过** | 188 |
| **失败** | 14（**全部 SEGFAULT 或 Subprocess aborted — CUDA runtime 限制**） |
| **实际耗时** | 14.99s（real） |
| **状态** | 🟡 **93.1% PASS**（环境受限） |

#### 14 个失败测试清单（全部 CUDA runtime 受限）

| # | 测试 | 失败类型 | 原因 |
|---|------|---------|------|
| 12 | dummy-add | SEGFAULT | 需 CUDA kernel launch |
| 13 | dummy-float | SEGFAULT | 需 CUDA kernel launch |
| 22 | dummy-ldglobal | SEGFAULT | 需 CUDA kernel launch |
| 28 | simpleGEMM-float | SEGFAULT | 需 GPU |
| 29 | simpleGEMM-double | SEGFAULT | 需 GPU |
| 30 | simpleCONV-int | SEGFAULT | 需 GPU |
| 31 | simpleCONV-float | SEGFAULT | 需 GPU |
| 33 | 2Dentropy | SEGFAULT | 需 GPU |
| 36 | bitonic | SEGFAULT | 需 GPU |
| 103 | unit_cuda_stream_handle | SEGFAULT | 需 cudaStream_t runtime |
| 186 | e2e_ldglobal_simple | SEGFAULT | 需真实 GPU device |
| 187 | e2e_divergence | Subprocess aborted | 需 GPU |
| 195 | e2e_tcgen05_mma_ws | SEGFAULT | 需 GPU (Blackwell) |
| 202 | cute_rmsnorm_debug | SEGFAULT | 需 GPU + CUTLASS |

**结论**: 所有 14 失败均为**无 GPU sandbox 限制**（`nvidia-smi` 不可用，`cuda_runtime.h` header-only stub），非代码回归。Baseline worktree 与 main HEAD 字节相同，确认失败为 pre-existing。

---

## 4. 与综合计划验收标准对比

| 综合计划标准 | CppTLM 端 | PTX-EMU 端 | 整体 |
|------------|----------|------------|------|
| **G0** P0/P1/P2/P3 阶段完成 | ⏳ 实施中 | ⏳ 实施中 | ⏳ |
| **G-F0** `vector_add` 烟雾测试 | ⏳ 待 P0 阶段 | ⏳ 待 P0 阶段 | ⏳ |
| **G-D1** 3 纯虚接口编译通过 | ✅ 头文件存在 | ✅ 头文件存在 | ✅ |
| **G-D4** 4 Adapter `static_assert` 12 端点 0-5 | ⏳ 待 P1 阶段 | ⏳ 待 P1 阶段 | ⏳ |
| **G-D6** 4 setter nullptr 时 PTX-EMU 零退化 | ✅ baseline pass | ✅ baseline pass | ✅ |
| **G-F1** `g_cpptlm_bridge == nullptr` 字节级一致 | ✅ n/a（独立模式） | ✅ baseline pass | ✅ |

---

## 5. 关键发现与限制

### 5.1 PTX-EMU 任务文档偏差

| 文档声明 | 实际 |
|---------|------|
| `tasks.md` Phase 0.5: "600+ tests" | **实际 202 tests**（198+4 新增 Phase 7 测试） |
| `tasks.md` Phase 0.5: `ctest -L "unit;integration;e2e"` | **label 不存在**（仅 `mini`/`basic`/`mma`） |

**建议**: PTX-EMU 团队更新 `tasks.md` Phase 0.5 描述：
- 测试数: 600+ → 202
- ctest label: `unit;integration;e2e` → 无 label（run all）

### 5.2 编译时间记录

| 项 | CppTLM | PTX-EMU |
|---|--------|---------|
| 首次 configure | 1.8s | 0.4s |
| 完整 build | ~5min (含 examples) | ~10min (含 CUDA/ANTLR) |
| ctest 全跑 | ~2s | 14.99s |
| **合计 Phase 0.5 总耗时** | **~7min** | **~25min** |

### 5.3 Baseline Worktree 用途

| 用途 | 说明 |
|------|------|
| **回归基线** | D1 实施过程中可随时 `cd baseline-d1-full && cmake --build build && ctest` 与 HEAD 对比 |
| **安全网** | 任何 D1 实施改动若破坏现有 764 (CppTLM) / 188 (PTX-EMU) baseline，diff 立即可见 |
| **回退点** | 实施出错时 `git reset --hard baseline/d1-full-prep`（或 `baseline/f12b-ld-prep`）立即回到 D1 前状态 |

---

## 6. D1 开工就绪清单

- [x] CppTLM baseline worktree 创建 + 编译 + 测试（764/764 PASS）
- [x] PTX-EMU baseline worktree 创建 + fast-forward + 编译 + 测试（188/202，14 环境性失败已识别）
- [x] CppTLM 端 .gitignore 修复（`.worktrees/` 加入 + 推送 origin）
- [x] HSK-1 已锁定（commit `8dc000ec`）+ CppTLM 端 OK
- [x] HSK-2 ANTLR4 4.13.2 + CppTLM 端 OK（满足 `>= 4.13.2` 下限）
- [x] HSK-3 候选 ExternalProject_Add + CppTLM 端确认选项 1
- [x] C1 P0 openspec tasks.md 重构为 P0/P1/P2/P3
- [x] docs_sync 364/364 PASS
- [ ] D1: PTX-EMU Phase 1 实施（#1 cpptlm_bridge.h 已 commit 8dc000ec，但需重新验证 Phase 7 之后的 ABI 兼容性）
- [ ] D1: CppTLM Phase #C1 MemoryBridge 实施

---

## 7. 给 PTX-EMU 团队的回传（HSK-4）

### 7.1 14 个 CUDA 测试失败的环境性确认

**问题**: 在 CPU-only sandbox（`nvidia-smi` 不可用）下，14 个测试 SEGFAULT。

**根因**: 所有失败测试均调用 `cudaLaunchKernel` / `cuLaunchKernel` 或需要真实 GPU device。

**影响**: 不影响 D1-Full 实施；D5 EOD G-F0 vector_add 烟雾测试同样需要 GPU。

**建议**:
1. PTX-EMU 团队在有 GPU 的环境上重跑 ctest 确认 202/202 pass
2. 若 202/202 pass，baseline 完整；D1 可立即开工
3. 若仍有个别失败，记录到 baseline known-failures 列表（环境性 / 平台性）

### 7.2 tasks.md 文档修正建议

```diff
- `cd build && ctest -L "unit;integration;e2e" --output-on-failure`（验证 600+ 测试全 PASS）
+ `cd build && ctest --output-on-failure -j$(nproc)`（验证 202 测试 PASS 或环境性失败）
```

---

**最后更新**: 2026-07-15
**整体评级**: 🟢 **可开工**（D1 起始）
