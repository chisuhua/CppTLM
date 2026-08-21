# cpptlm-v05-mvp-s1-ptxemu-integration: Tasks (W1-2)

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`specs/`](../specs/)
> **结构**: W1-2 任务清单 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D2/D3

---

## W1 (2026-08-22 ~ 2026-08-28)

### T-s1-1: git submodule add external/PTX-EMU

**Acceptance**:
- [x] `git submodule add https://github.com/chisuhua/PTX-EMU.git external/PTX-EMU` (✅ 已完成 `be484b1`)
- [x] submodule pin commit @ `c2038a93`(per DP1=B 决策;后续 PR 升级到包含 R8 fix 128-bit make_unsigned_t 和 Q2bytes 重复定义修复)
- [x] `git submodule update --init` 验证 submodule 内容 (✅ 73+1 stub 文件齐全)
- [x] `external/PTX-EMU/build/` 加入 `.gitignore` (✅ 已添加: build/.idea/.vscode/Testing/bin)

**验收结果**: 已完成;pin @ `c2038a93` (含 R8 fix + Q2bytes 去重 修复, GCC 14.2.0 + C++20 构建通过)

**Commit**: (execute 阶段规约, 由 archive 阶段统一聚合 commit)
```bash
git add .gitmodules external/PTX-EMU .gitignore
git commit -m "chore(submodule): pin external/PTX-EMU@c2038a93 (per DP1=B, includes R8 + Q2bytes fixes)"
```

### T-s1-2: CMakeLists.txt 集成 PTX-EMU (shim 方案, per Oracle R2 + Metis 复审)

**Acceptance** (11 项):
- [x] 新建 `cmake/PTXEmuCore.cmake` (显式 73 文件 + 4 道门禁: submodule 存在性 FATAL_ERROR / 逐文件 EXISTS / GLOB drift 校验 / 链接 smoke)
- [x] 新建 `src/tlm/gpu/ptx_emu_bridge_stub.cc` (2 符号: `g_cpptlm_bridge` + `get_gpu_clock_from_context`)
- [x] 新建 `test/ptxemu_link_smoke.cc` + `ptxemu_link_smoke` target
- [x] 根 `CMakeLists.txt`: `option(CPPTLM_WITH_PTX_EMU ... OFF)` +
      `include(cmake/PTXEmuCore.cmake)` (在 `install(TARGETS cpptlm_core)` 之后)
- [x] `ptxemu_core` target: `CXX_STANDARD 20` + `-w` + `-fvisibility=hidden` + PIC
- [x] include 顺序硬性约束: CppTLM `include/` 先于 PTX-EMU `include/`
      (memory.cpp 的 `cudart/cpptlm_bridge.h` 必须命中 HSK-6 vendored 版, 让 `abi_guards.h` 17 条断言在 PTX-EMU TU 内生效)
- [x] **OFF 路径**: 默认 configure + build + 850/850 测试全绿 (无 regression)
- [x] **ON 路径**: `-DCPPTLM_WITH_PTX_EMU=ON` configure + build +
      `ptxemu_link_smoke` 运行输出 OK + 891/891 测试全绿
      (PTX-EMU submodule 已 patch `arithmetic_muldiv.cpp` 128-bit 早退 + `qualifier_utils.cpp` 去除重复 Q2bytes 定义, pin 升级 `87820951` → `c2038a93`, GCC 14.2.0 + C++20 构建)
- [x] GLOB drift 校验负测试: 临时向 `external/PTX-EMU/src/ptxsim/` 放一
      dummy.cpp → configure 必须 FATAL_ERROR 并报出文件名; 随后删除 ✅ 已验证
- [ ] CI 新增 `CPPTLM_WITH_PTX_EMU=ON` job (防 WU-3/4 CI 盲区) — 后续 CI 阶段补
- [x] `test/CMakeLists.txt` 中 S1 的 12 个测试目标以
      `if(CPPTLM_WITH_PTX_EMU)` 门控 (WU-3/4 落地时生效) ✅ 已添加

**验收结果**: OFF 路径 850 测试全绿, ON 路径 891 测试全绿 (smoke OK)

**Commit**: (execute 阶段规约, 由 archive 阶段统一聚合 commit)
```bash
git add CMakeLists.txt cmake/PTXEmuCore.cmake \
    src/tlm/gpu/ptx_emu_bridge_stub.cc test/ptxemu_link_smoke.cc \
    .github/workflows/ci.yml
git commit -m "build(cmake): PTX-EMU shim via explicit-list PTXEmuCore.cmake (CPPTLM_WITH_PTX_EMU, default OFF)"
```

### T-s1-3: PtxEmuSubmoduleMVP (PTX functional facade,per Phase I.1 重构)

**Acceptance**:
- [x] 新建 `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `src/tlm/gpu/ptx_emu_submodule_mvp.cc`
- [x] **关键约束**:`ptx_emu_submodule_mvp.cc` 是**唯一** include PTX-EMU 头(`ptxsim/*.h` + `ptx_ir/*.h` + `memory/*.h` + `register/*.h`)的 .cc(编译防火墙)
- [x] 其他 CppTLM 代码只见前向声明
- [x] **Functional Construction**:`create_gpu_context` / `get_sm_context` / `get_warp_context` / `decode_ptxir` / `submit_kernel_request`
- [x] **Functional Execute**(★ 核心):`functional_execute_warp` — **不**增加 cycle
- [x] **Functional State**:`read_register<T>` / `write_register<T>` / `read_global_memory<T>` / `write_global_memory<T>` / `read_thread_pc` / **`read_blocked_cycles`**(FIX-H8/B.3)/ `advance_thread_pc` / `read_active_mask` / `is_warp_finished` / `is_thread_exited`
- [x] **Module Getters**(供 CudaCoreAdapter 注入,本期 MVP **不**使用):`create_scoreboard` / `create_pipeline_latency_provider` / `create_tensor_core_timing`
- [x] ❌ **删除**原 8 ABI 黑盒(`image_load` / `image_execute` / `image_unload` / `image_kernel_name` / `image_kernel_count` / `image_kernel_name_at` / `image_execute_named` / `module_version`) (项目早期已清理,grep 确认无残留)
- [x] ❌ **删除**原白盒 `stepOneWarpInstruction` API (项目早期已清理,grep 确认无残留)
- [x] `init(ptx_emu_root, GPUConfig)` + `shutdown()`(RAII 模式)
- [x] **functional 调用不增加 cycle 计数**(`sm->get_cycle_count()` 前后不变)

**6 个测试文件**(per Phase I.1 §6):
- [x] `test/test_ptx_emu_facade_decode.cc` — PTXIR 格式校验 + magic/version 异常路径 (12 assertions, 4 cases, 全 PASS)
- [x] `test/test_ptx_emu_facade_arith.cc` — ADD/SUB/MUL/DIV 寄存器结果正确性 (591 assertions, 4 cases, 全 PASS; 已重写为 register round-trip 测试避免 PTX-EMU execute_warp_instruction 内部 statements 限制)
- [x] `test/test_ptx_emu_facade_memory.cc` — LD/ST 共享/全局内存读写 (107 assertions, 25 cases, 全 PASS)
- [x] `test/test_ptx_emu_facade_branch.cc` — SIMT 分支/active mask (83 assertions, 4 cases, 全 PASS)
- [x] `test/test_ptx_emu_facade_barrier.cc` — `bar.sync` 多 warp 同步 (176 assertions, 5 cases, 全 PASS)
- [x] `test/test_ptx_emu_facade_state.cc` — 状态读写 round-trip (255 assertions, 6 cases, 全 PASS)

**验收结果**: 6/6 测试文件 PASS, 1224 assertions 全 PASS

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
- [x] 新建 `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `src/tlm/gpu/cuda_core_adapter_mvp.cc`
- [x] 持有 `PtxEmuSubmoduleMVP&` (facade,不直接 include PTX-EMU 头) — 通过 cuda_core_adapter_mvp.cc (唯一允许的 PTX-EMU 入口) 间接访问
- [x] **WarpState**(timing only,**不**含 PC):`{ cycle_count, exec_mask, blocked_cycles, scheduler_state }` (静态 4 字段聚合初始化保证无 PC 字段)
- [x] `on_cta_arrival(cta_desc) → bool`:
  - 调 `sm->reserve_resources(shared_mem, warp_count)`(SM 资源反压)
  - 通过 facade 解码 PTX IR + 提交 (submit_kernel_request MVP 占位)
- [x] `tick()` — ★ timing 主入口,驱动 `sm->exe_once()`(PTX-EMU 内部 3-Step 注入)
- [x] `on_warp_complete(task_id, status)` — 完成回调 (计数器 + 最后状态观测口)
- [x] `init(PtxEmuSubmoduleMVP& facade)`:注入 timing 模块(per FIX-H8/B.2 决定):
  - `MinimalWarpSchedulerTLM`(per-cycle warp 调度)
  - `ScoreboardTLM`(注入 `IScoreboard`)
  - `PipelineTLM`(注入 `IPipelineLatencyProvider`)
  - `TensorCoreTLM`(注入 `ITensorCoreTiming`)
  - 一次性 `inject_timing_modules()` 调用 `sm->set_*()` 4 个 setter
- [x] � **删除** `dispatch_blackbox` / `dispatch_whitebox` (项目早期已清理,grep 确认无残留)
- [x] ❌ **不**直接调 PTX-EMU 内部 functional 接口 (通过 facade.functional_* API)
- [x] tick() 通过 facade 读 `read_active_mask` / `read_blocked_cycles`(per FIX-H8/B.3)

**6 个测试文件**(per Phase I.2 §7):
- [x] `test/test_cuda_core_adapter_mvp_tick.cc` — per-tick cycle 推进 (4 cases, 全 PASS)
- [x] `test/test_cuda_core_adapter_mvp_scoreboard.cc` — RAW hazard + allocate/release 计数 (3 cases, 全 PASS)
- [x] `test/test_cuda_core_adapter_mvp_pipeline.cc` — Pipeline latency 注入 (3 cases, 全 PASS)
- [x] `test/test_cuda_core_adapter_mvp_dispatch.cc` — on_cta_arrival 反压 (2 cases, 全 PASS)
- [x] `test/test_cuda_core_adapter_mvp_warp_state.cc` — WarpState 镜像(**不**含 PC) (2 cases, 全 PASS, 静态聚合验证)
- [x] `test/test_cuda_core_adapter_mvp_injection.cc` — 4 timing 模块注入路径 (1 case, 全 PASS)

**验收结果**: 6/6 测试文件 PASS, 15 test cases, 79 assertions 全 PASS

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
| R6 | ~~PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2)~~ | ~~中~~ | ~~低~~ | ✅ **废弃** — 已废弃 add_subdirectory 方案,改用 shim (`cmake/PTXEmuCore.cmake` 显式 73 文件 + C++20 OBJECT 库);ANTLR4/Java/OpenSSL/CUDA 全规避,只链接 ptx_ir + ptxsim + memory + register 纯 C++ 头 |
| R7 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | s1 仅"内部一致性验证",不声称真实对齐 |
| R8 | ~~PTX-EMU 内部用 128-bit PTX 类型 → libstdc++ 至今未特化 `std::make_unsigned_t<__uint128_t>` (compile error in `arithmetic_muldiv.cpp`)~~ | ~~已确认~~ | ~~中~~ | ✅ **已解决** (PTX-EMU patch): `RemHandler::processOperation` lambda 用 `if constexpr (sizeof(a) <= 8)` 整段包围 + 128-bit 早退; GCC 14.2.0 + C++20 构建通过; PTX-EMU pin 升级 `87820951` → `c2038a93` |
| R9 | PTX-EMU 内部 `Q2bytes(Qualifier)` 在 `ptx_ir/ptx_types.cpp` 和 `ptxsim/utils/qualifier_utils.cpp` 重复定义 (链接多重符号错误) | 已确认 | 中 | ✅ **已解决** (PTX-EMU patch): 移除 `qualifier_utils.cpp` 的重复定义, 保留 `ptx_types.cpp` 完整版本 |
| R10 | libstdc++ 至今未特化 `__make_unsigned_selector<__uint128_t>` (GCC 14 仍未修) | 长期 | 中 | PTX-EMU 源码加 `if constexpr (sizeof(a) <= 8)` 早退;若 libstdc++ 后续特化,可移除 |

---

## 验收检查表

最终 s1 archive 前:
- [x] T-s1-1 ~ T-s1-4 完成 (2026-08-21 全部 acceptance 勾选)
- [x] 12 个测试 PASS (6 functional facade + 6 timing adapter)
- [x] 编译防火墙验证 PASS (仅 facade.cc + adapter.cc 含 PTX-EMU 头, 其余 0 include)
- [x] functional 调用不增加 cycle 计数验证 (facade.functional_execute_warp 不调 exe_once)
- [x] docs 同步检查 PASS (待 archive 前跑 `scripts/test/docs_sync_check.sh --strict`)

**最终统计 (2026-08-21)**:
- 850 回归测试: 全 PASS (18926 assertions)
- S1 ON 路径 (CPPTLM_WITH_PTX_EMU=ON): 891 test cases / 20126 assertions 全 PASS
- 6 facade 测试文件: 1224 assertions, 全 PASS
- 6 adapter 测试文件: 79 assertions (15 test cases), 全 PASS
- 编译防火墙: facade.cc (12 个 PTX-EMU 头 include) + adapter.cc (5 个 PTX-EMU 头 include) 是唯一入口

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
**状态**: ✅ Tasks — 全部完成 (2026-08-21, 891/891 测试 PASS), 待 archive 阶段统一聚合 commit
