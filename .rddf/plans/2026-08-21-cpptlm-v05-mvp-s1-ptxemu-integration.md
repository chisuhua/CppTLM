# cpptlm-v05-mvp-s1-ptxemu-integration: 实施计划 (W1-2)

> **配套**: [`openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/)
> **单一真相源**: `openspec/changes/.../tasks.md`（plan 仅为执行入口,不同步覆盖 tasks.md）
> **生成时间**: 2026-08-20 · **生成方式**: guide-ship Phase 1 (TDD 5 步结构)
> **Worktree**: `.rddf/wt/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`
> **分支**: `openspec/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration`
> **关联 ADR**: `docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md` D2/D3

---

## Goal (目标)

完成 v0.5 MVP 的 **S1 子项目**：把 PTX-EMU 作为 submodule 集成到 CppTLM，并通过两层编译防火墙（`PtxEmuSubmoduleMVP` + `CudaCoreAdapter`）实现 PTX functional 与 SM timing 的清晰分离，最终为 S2 (dGPU board) / S3 (command pipeline) 提供执行基础。

**完成定义 (DoD)**：
1. ✅ T-s1-1 ~ T-s1-4 全部 acceptance 勾选
2. ✅ 12 个测试 PASS（6 functional + 6 timing）
3. ✅ 编译防火墙验证 PASS（`git grep` 仅 `ptx_emu_submodule_mvp.cc` 含 PTX-EMU 头）
4. ✅ functional 调用不增加 cycle 计数
5. ✅ `cmake --build` + `ctest` 无 regression（≥764 测试仍 PASS）
6. ✅ `scripts/test/docs_sync_check.sh --strict` PASS
7. ✅ 单一聚合 commit（Phase 2.7 worktree-commit-flow）

---

## 关键约束（设计要点摘要）

| 约束 | 来源 | 影响 |
|------|------|------|
| **编译防火墙** | design.md §3 + tasks.md T-s1-3 | `ptx_emu_submodule_mvp.cc` 是**唯一**直接 include `ptxsim/*.h` / `ptx_ir/*.h` / `memory/*.h` / `register/*.h` 的 .cc |
| **WarpState 不含 PC** | tasks.md T-s1-4 + FIX-H8/B.3 | timing-only 模块不得持有 functional state |
| **functional ≠ timing** | tasks.md T-s1-3 + R3 | facade 接口**禁止**暴露 `exe_once` / `set_blocked_cycles` 等 timing API |
| **删除旧 ABI** | tasks.md T-s1-3 | 移除 8 ABI 黑盒 + 1 `stepOneWarpInstruction` 白盒 API |
| **删除旧调度** | tasks.md T-s1-4 | 移除 `dispatch_blackbox` / `dispatch_whitebox` |
| **-fvisibility=hidden** | tasks.md T-s1-2 + Oracle §E.1 R5 | PTX-EMU 静态库防符号泄露 |
| **PTX_EMU_BUILD_TESTS=OFF** | tasks.md T-s1-2 | 不构建 PTX-EMU 自家测试 |
| **antlr 4.13.2** | HSK-2 | PTX-EMU 依赖,submodule 内已固定 |

---

## Work Units（W1-2, TDD 5 步）

> **TDD 5 步 canonical markers** (per `rdd-doctor` `plan-tdd` check — for `execute` skill compatibility):
> 1. **Write the failing test**
> 2. **Run test to verify it fails**
> 3. **Write minimal implementation**
> 4. **Run test to verify it passes**
> 5. **Defer commit**
>
> 本 plan 使用本地化标签（**Write failing test** / **Verify fail** / **Implement** / **Verify pass** / **Commit**）作为子标题，与 canonical markers 一一对应。

### WU-1: T-s1-1 (submodule pin) — 2026-08-22

**Status**: 1/4 acceptance 已完成（commit `be484b1`）

**TDD 5 步**:
1. **Write failing test**: N/A（基础设施配置,无新代码逻辑）
2. **Verify fail**: N/A
3. **Implement**:
   ```bash
   git submodule update --init  # 检出 PTX-EMU @ 87820951
   # 追加到 .gitignore: external/PTX-EMU/build/ 与 external/PTX-EMU/.idea/
   ```
4. **Verify pass**:
   ```bash
   git submodule status  # 应显示 -87820951000734538253f1ef006d3277eda2e3cf external/PTX-EMU
   ls external/PTX-EMU/ | head -10  # 应见 README/, src/, include/ 等
   ```
5. **Commit**:
   ```bash
   git add .gitmodules external/PTX-EMU .gitignore
   git commit -m "chore(submodule): pin external/PTX-EMU@87820951 (per DP1=B)"
   ```

**回滚 plan**: 若 `git submodule update --init` 失败 → 检查 PTX-EMU 端 commit `87820951` 是否存在;不存在则回滚到最近可用 commit。

---

### WU-2: T-s1-2 (CMake 集成) — 2026-08-22 ~ 2026-08-23

**TDD 5 步**:
1. **Write failing test**: 在 `test/CMakeLists.txt` 添加占位测试 `test_s1_smoke.cc`（仅 `REQUIRE(true)`），verify fail（target 不存在）
2. **Verify fail**:
   ```bash
   cmake --build build --target test_s1_smoke 2>&1 | grep "No rule"  # 应见 "No rule to make target"
   ```
3. **Implement**: 修改根 `CMakeLists.txt`：
   ```cmake
   # PTX-EMU submodule 集成 (per ADR-SOC-06 D2)
   set(PTX_EMU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
   set(PTX_EMU_BUILD_SHARED OFF CACHE BOOL "" FORCE)
   set(CMAKE_CXX_VISIBILITY_PRESET hidden)
   add_subdirectory(external/PTX-EMU)
   target_link_libraries(cpptlm_core PUBLIC ptx_emu_core)
   ```
4. **Verify pass**:
   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   build/bin/cpptlm_tests --reporter compact 2>&1 | tail -3
   # 预期: All tests passed (≥764 assertions in ≥764 test cases)
   ```
5. **Commit**:
   ```bash
   git add CMakeLists.txt test/CMakeLists.txt
   git commit -m "build(cmake): add_subdirectory(external/PTX-EMU) — submodule static link"
   ```

**风险**:
- ANTLR4 缺失 → 检查 `find_package(antlr4 REQUIRED)` 在 PTX-EMU 内部是否已处理
- C++ 标准不一致 → CppTLM 用 C++17,确认 PTX-EMU 也是 C++17

---

### WU-3: T-s1-3 (PtxEmuSubmoduleMVP facade) — 2026-08-24 ~ 2026-08-27

**TDD 5 步**:
1. **Write failing tests** (6 个):
   - `test/test_ptx_emu_facade_decode.cc` — `decode_ptxir` magic/version 异常路径
   - `test/test_ptx_emu_facade_arith.cc` — ADD/SUB/MUL/DIV 结果正确性
   - `test/test_ptx_emu_facade_memory.cc` — LD/ST 共享/全局内存
   - `test/test_ptx_emu_facade_branch.cc` — SIMT 分支/active mask
   - `test/test_ptx_emu_facade_barrier.cc` — `bar.sync` 多 warp 同步
   - `test/test_ptx_emu_facade_state.cc` — 状态读写 round-trip
2. **Verify fail**: `cmake --build build` 应报"undefined reference to PtxEmuSubmoduleMVP"
3. **Implement**:
   - 新建 `include/tlm/gpu/ptx_emu_submodule_mvp.hh`（**前向声明** PTX-EMU 类型）
   - 新建 `src/tlm/gpu/ptx_emu_submodule_mvp.cc`（**唯一** include PTX-EMU 头）
   - 实现 `init()` / `shutdown()` / Functional Construction 5 个 / Functional State 11 个 / Module Getters 3 个
   - **删除** 8 ABI 黑盒 + `stepOneWarpInstruction`
4. **Verify pass**:
   ```bash
   # 编译防火墙（4 模式）
   git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/" \
     -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
   # 预期: 仅 src/tlm/gpu/ptx_emu_submodule_mvp.cc

   # Functional 测试
   ctest -R "test_ptx_emu_facade" --output-on-failure
   # 预期: 6 个测试文件全部 PASS

   # Functional 不增 cycle
   # 在 test_ptx_emu_facade_state.cc 内断言: sm->get_cycle_count() 前后不变
   ```
5. **Commit**:
   ```bash
   git add include/tlm/gpu/ptx_emu_submodule_mvp.hh src/tlm/gpu/ptx_emu_submodule_mvp.cc test/test_ptx_emu_facade_*.cc CMakeLists.txt test/CMakeLists.txt
   git commit -m "feat(ptx-emu-mvp): PTX functional facade (depth-integration, per Phase I.1)"
   ```

**风险**:
- PTX-EMU 头路径不稳 → 严格走 `git grep` 验证
- cycle 计数误增 → 必须 `read_blocked_cycles` 而非 `exe_once`

---

### WU-4: T-s1-4 (CudaCoreAdapter timing) — 2026-08-29 ~ 2026-09-04

**TDD 5 步**:
1. **Write failing tests** (6 个):
   - `test/test_cuda_core_adapter_mvp_tick.cc` — per-tick cycle 推进
   - `test/test_cuda_core_adapter_mvp_scoreboard.cc` — RAW hazard + allocate/release
   - `test/test_cuda_core_adapter_mvp_pipeline.cc` — Pipeline latency 注入
   - `test/test_cuda_core_adapter_mvp_dispatch.cc` — `on_cta_arrival` 反压
   - `test/test_cuda_core_adapter_mvp_warp_state.cc` — WarpState 镜像（**不**含 PC）
   - `test/test_cuda_core_adapter_mvp_injection.cc` — 4 timing 模块注入路径
2. **Verify fail**: `cmake --build build` 应报"undefined reference to CudaCoreAdapterMVP"
3. **Implement**:
   - 新建 `include/tlm/gpu/cuda_core_adapter_mvp.hh`（**前向声明** `PtxEmuSubmoduleMVP`，不直接 include PTX-EMU）
   - 新建 `src/tlm/gpu/cuda_core_adapter_mvp.cc`（持有 facade 引用,经 facade 调 functional API）
   - 实现 `init(PtxEmuSubmoduleMVP&)` + `on_cta_arrival()` + `tick()` + `on_warp_complete()`
   - 一次性 `inject_timing_modules()` 调用 4 个 setter:
     - `MinimalWarpSchedulerTLM`
     - `ScoreboardTLM`
     - `PipelineTLM`
     - `TensorCoreTLM`
   - **删除** `dispatch_blackbox` / `dispatch_whitebox`
4. **Verify pass**:
   ```bash
   ctest -R "test_cuda_core_adapter_mvp" --output-on-failure
   # 预期: 6 个 timing 测试文件全部 PASS

   # WarpState 不含 PC 验证
   # 在 test_cuda_core_adapter_mvp_warp_state.cc 内断言: WarpState 无 pc 字段
   ```
5. **Commit**:
   ```bash
   git add include/tlm/gpu/cuda_core_adapter_mvp.hh src/tlm/gpu/cuda_core_adapter_mvp.cc test/test_cuda_core_adapter_mvp_*.cc CMakeLists.txt test/CMakeLists.txt
   git commit -m "feat(cuda-core-mvp): SM microarchitecture exploration (timing model, per Phase I.2)"
   ```

**风险**:
- 误调 PTX-EMU 内部 → 全部经 facade 转发
- WarpState 误含 PC → 编译期断言 + 单元测试

---

## Test Plan（验收清单）

| 测试类型 | 命令 | 预期 |
|---------|------|------|
| 编译防火墙 | `git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/" -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"` | 仅 `src/tlm/gpu/ptx_emu_submodule_mvp.cc` |
| Functional 测试 | `ctest -R "test_ptx_emu_facade" --output-on-failure` | 6 个 PASS |
| Timing 测试 | `ctest -R "test_cuda_core_adapter_mvp" --output-on-failure` | 6 个 PASS |
| 全部回归 | `ctest --test-dir build --output-on-failure -j4` | ≥764 测试 PASS |
| 文档同步 | `scripts/test/docs_sync_check.sh --strict` | 365/365 PASS |
| 格式检查 | `scripts/build/format.sh --check` | 无 diff |

---

## Risk Mitigation（风险登记表-本 change 子集）

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin @ `87820951` + 月度 bump PR |
| R2 | PTX-EMU 头文件 API 变更 | 中 | 高 | `abi_guards.h` 17 条静态断言(HSK-6 P0-1) |
| R3 | Functional 误调 timing-only API | 低 | 高 | 编译期隔离:facade 接口禁止 `exe_once` 等 |
| R4 | Functional 误读 PC 推到 WarpState | 低 | 中 | 文档 + 接口表明确分离;WarpState 不含 PC |
| R5 | CudaCoreAdapter 裸调 PTX-EMU 内部 | 中 | 高 | 全部经 facade 转发 |
| R6 | PTX-EMU 构建依赖扩散(ANTLR4) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R7 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | s1 仅"内部一致性验证",不声称真实对齐 |
| R8 | 编译防火墙误漏（新代码忘改 facade） | 中 | 高 | 每次 commit 前必跑 `git grep` 4 模式检查 |

---

## Done Definition (S1 archive 前)

- [ ] T-s1-1 ~ T-s1-4 全部 acceptance 勾选（54/54）
- [ ] 12 个测试 PASS（6 functional + 6 timing）
- [ ] 编译防火墙验证 PASS
- [ ] functional 调用不增加 cycle 计数验证
- [ ] 现有 ≥764 测试仍通过（无 regression）
- [ ] `docs_sync_check.sh --strict` PASS
- [ ] `format.sh --check` PASS
- [ ] 单一聚合 commit 准备就绪（Phase 2.7 worktree-commit-flow）

---

**Plan Owner**: CppTLM Team · **Plan Generated**: 2026-08-20 by guide-ship Phase 1
**Next Phase**: Phase 2 (execute) — 等待用户选择执行模式
