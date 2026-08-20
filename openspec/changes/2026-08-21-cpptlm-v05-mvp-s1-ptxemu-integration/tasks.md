# cpptlm-v05-mvp-s1-ptxemu-integration: Tasks (W1-2)

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`specs/`](../specs/)
> **结构**: W1-2 任务清单 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D2/D3

---

## W1 (2026-08-22 ~ 2026-08-28)

### T-s1-1: git submodule add external/PTX-EMU

**Acceptance**:
- [x] `git submodule add https://github.com/chisuhua/PTX-EMU.git external/PTX-EMU` (✅ 已完成 `be484b1`)
- [ ] submodule pin commit @ `87820951`(per DP1=B 决策,2026-08-13 audit commit)
- [ ] `git submodule update --init` 验证 submodule 内容
- [ ] `external/PTX-EMU/build/` 加入 `.gitignore`

**验证命令**:
```bash
git submodule status  # 显示 PTX-EMU commit hash + path
ls external/PTX-EMU/ | head -10  # 验证 submodule 已检出
```

**Commit**:
```bash
git add .gitmodules external/PTX-EMU .gitignore
git commit -m "chore(submodule): pin external/PTX-EMU@87820951 (per DP1=B)"
```

### T-s1-2: CMakeLists.txt 集成 PTX-EMU

**Acceptance**:
- [ ] `CMakeLists.txt` 添加 `add_subdirectory(external/PTX-EMU)`
- [ ] 设置 `PTX_EMU_BUILD_TESTS=OFF`(不构建 PTX-EMU 自家测试)
- [ ] 设置 `PTX_EMU_BUILD_SHARED=OFF`(强制静态库)
- [ ] 设置 `-fvisibility=hidden`(per Oracle §E.1 风险 R5)
- [ ] cpptlm_core 静态链接 PTX-EMU
- [ ] `test/CMakeLists.txt` 添加 s1 测试目标(12 个 .cc)
- [ ] 现有 ≥850 测试仍通过(无 regression)

**验证命令**:
```bash
cmake --build build -j$(nproc)
build/bin/cpptlm_tests --reporter compact 2>&1 | tail -3
# 预期: All tests passed (≥850 assertions in ≥850 test cases)
```

**Commit**:
```bash
git add CMakeLists.txt test/CMakeLists.txt
git commit -m "build(cmake): add_subdirectory(external/PTX-EMU) — submodule static link"
```

### T-s1-3: PtxEmuSubmoduleMVP (PTX functional facade,per Phase I.1 重构)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `src/tlm/gpu/ptx_emu_submodule_mvp.cc`
- [ ] **关键约束**:`ptx_emu_submodule_mvp.cc` 是**唯一** include PTX-EMU 头(`ptxsim/*.h` + `ptx_ir/*.h` + `memory/*.h` + `register/*.h`)的 .cc(编译防火墙)
- [ ] 其他 CppTLM 代码只见前向声明
- [ ] **Functional Construction**:`create_gpu_context` / `get_sm_context` / `get_warp_context` / `decode_ptxir` / `submit_kernel_request`
- [ ] **Functional Execute**(★ 核心):`functional_execute_warp` — **不**增加 cycle
- [ ] **Functional State**:`read_register<T>` / `write_register<T>` / `read_global_memory<T>` / `write_global_memory<T>` / `read_thread_pc` / **`read_blocked_cycles`**(FIX-H8/B.3)/ `advance_thread_pc` / `read_active_mask` / `is_warp_finished` / `is_thread_exited`
- [ ] **Module Getters**(供 CudaCoreAdapter 注入,本期 MVP **不**使用):`create_scoreboard` / `create_pipeline_latency_provider` / `create_tensor_core_timing`
- [ ] ❌ **删除**原 8 ABI 黑盒(`image_load` / `image_execute` / `image_unload` / `image_kernel_name` / `image_kernel_count` / `image_kernel_name_at` / `image_execute_named` / `module_version`)
- [ ] ❌ **删除**原白盒 `stepOneWarpInstruction` API
- [ ] `init(ptx_emu_root, GPUConfig)` + `shutdown()`(RAII 模式)
- [ ] **functional 调用不增加 cycle 计数**(`sm->get_cycle_count()` 前后不变)

**6 个测试文件**(per Phase I.1 §6):
- [ ] `test/test_ptx_emu_facade_decode.cc` — PTXIR 格式校验 + magic/version 异常路径
- [ ] `test/test_ptx_emu_facade_arith.cc` — ADD/SUB/MUL/DIV 寄存器结果正确性
- [ ] `test/test_ptx_emu_facade_memory.cc` — LD/ST 共享/全局内存读写
- [ ] `test/test_ptx_emu_facade_branch.cc` — SIMT 分支/active mask
- [ ] `test/test_ptx_emu_facade_barrier.cc` — `bar.sync` 多 warp 同步
- [ ] `test/test_ptx_emu_facade_state.cc` — 状态读写 round-trip

**验证命令**:
```bash
# 编译防火墙检查(扩展为 4 模式)
git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/" \
  -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
# 预期: 仅 src/tlm/gpu/ptx_emu_submodule_mvp.cc

# Functional 测试 PASS
ctest -R "test_ptx_emu_facade" --output-on-failure
# 预期: 6 个测试文件全部 PASS
```

**Commit**:
```bash
git add include/tlm/gpu/ptx_emu_submodule_mvp.hh src/tlm/gpu/ptx_emu_submodule_mvp.cc test/test_ptx_emu_facade_*.cc CMakeLists.txt
git commit -m "feat(ptx-emu-mvp): PTX functional facade (depth-integration, per Phase I.1)"
```

---

## W2 (2026-08-29 ~ 2026-09-04)

### T-s1-4: CudaCoreAdapter (SM microarchitecture exploration,per Phase I.2 重构)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `src/tlm/gpu/cuda_core_adapter_mvp.cc`
- [ ] 持有 `PtxEmuSubmoduleMVP&` (facade,不直接 include PTX-EMU 头)
- [ ] **WarpState**(timing only,**不**含 PC):`{ cycle_count, exec_mask, blocked_cycles, scheduler_state }`
- [ ] `on_cta_arrival(cta_desc) → bool`:
  - 调 `sm->reserve_resources(shared_mem, warp_count)`(SM 资源反压)
  - 通过 facade 解码 PTX IR + 构造 KernelLaunchRequest + 提交
- [ ] `tick()` — ★ timing 主入口,驱动 `sm->exe_once()`(PTX-EMU 内部 3-Step 注入)
- [ ] `on_warp_complete(task_id, status)` — 完成回调
- [ ] `init(PtxEmuSubmoduleMVP& facade)`:注入 timing 模块(per FIX-H8/B.2 决定):
  - `MinimalWarpSchedulerTLM`(per-cycle warp 调度)
  - `ScoreboardTLM`(注入 `IScoreboard`)
  - `PipelineTLM`(注入 `IPipelineLatencyProvider`)
  - `TensorCoreTLM`(注入 `ITensorCoreTiming`)
  - 一次性 `inject_timing_modules()` 调用 `sm->set_*()` 4 个 setter
- [ ] ❌ **删除** `dispatch_blackbox` / `dispatch_whitebox`
- [ ] ❌ **不**直接调 PTX-EMU 内部 functional 接口
- [ ] tick() 通过 facade 读 `read_active_mask` / `read_blocked_cycles`(per FIX-H8/B.3)

**6 个测试文件**(per Phase I.2 §7):
- [ ] `test/test_cuda_core_adapter_mvp_tick.cc` — per-tick cycle 推进
- [ ] `test/test_cuda_core_adapter_mvp_scoreboard.cc` — RAW hazard + allocate/release 计数
- [ ] `test/test_cuda_core_adapter_mvp_pipeline.cc` — Pipeline latency 注入
- [ ] `test/test_cuda_core_adapter_mvp_dispatch.cc` — on_cta_arrival 反压
- [ ] `test/test_cuda_core_adapter_mvp_warp_state.cc` — WarpState 镜像(**不**含 PC)
- [ ] `test/test_cuda_core_adapter_mvp_injection.cc` — 4 timing 模块注入路径

**验证命令**:
```bash
ctest -R "test_cuda_core_adapter_mvp" --output-on-failure
# 预期: 6 个 timing 测试文件全部 PASS
```

**Commit**:
```bash
git add include/tlm/gpu/cuda_core_adapter_mvp.hh src/tlm/gpu/cuda_core_adapter_mvp.cc test/test_cuda_core_adapter_mvp_*.cc CMakeLists.txt
git commit -m "feat(cuda-core-mvp): SM microarchitecture exploration (timing model, per Phase I.2)"
```

---

## 风险登记(本 change 子集)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit @ `87820951` + 月度 bump PR |
| R2 | PTX-EMU 头文件 API 变更 | 中 | 高 | `abi_guards.h` 17 条静态断言守卫 ABI(per HSK-6 P0-1) |
| R3 | Functional 误调 timing-only API | 低 | 高 | 编译期隔离:本 facade 接口**禁止** `exe_once` / `set_blocked_cycles` 等 timing API |
| R4 | Functional 误读 PC 推到 WarpState | 低 | 中 | 文档 + 接口表明确分离;WarpState 严格不含 PC |
| R5 | CudaCoreAdapter 裸调 PTX-EMU 内部(破坏 firewall) | 中 | 高 | 全部经 facade 转发;`read_blocked_cycles/read_active_mask` 已补 |
| R6 | PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R7 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | s1 仅"内部一致性验证",不声称真实对齐 |

---

## 验收检查表

最终 s1 archive 前:
- [ ] T-s1-1 ~ T-s1-4 完成
- [ ] 12 个测试 PASS(6 functional + 6 timing)
- [ ] 编译防火墙验证 PASS
- [ ] functional 调用不增加 cycle 计数验证
- [ ] docs 同步检查 PASS(`scripts/test/docs_sync_check.sh --strict`)

---

**Cc**: CppTLM Team · PTX-EMU Architecture Team

**Refs**:
- [`proposal.md`](../proposal.md)
- [`design.md`](../design.md)
- [`../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)
- [`../../../docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../../../docs/soc_arch/modules/ptx-emu-submodule-mvp.md)
- [`../../../docs/soc_arch/modules/cuda-core-adapter.md`](../../../docs/soc_arch/modules/cuda-core-adapter.md)

---

**起草**: Sisyphus (2026-08-21,per Oracle ses_fe179d02 拆分建议)
**Owner**: CppTLM Team
**状态**: 📋 Tasks — 等 W1 启动后开始实施
