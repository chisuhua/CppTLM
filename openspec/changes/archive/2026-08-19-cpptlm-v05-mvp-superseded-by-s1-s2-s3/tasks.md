# cpptlm-v05-mvp: dGPU Board MVP Slice — Tasks (S1-S4 6-10 周)

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`specs/`](../specs/)
> **结构**: S1-S4 阶段化任务清单 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) (per Phase I.4 ADR 移动)
> **关联路线图**: [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md) (per Phase F-H.8 / Phase I 修订)
> **状态**: 📋 Tasks — per Phase J 2026-08-20 对齐到 Phase F-H/I 架构重定义

---

## S1 MVP-Cut (W1-2)

### T-S1-1: git submodule add external/PTX-EMU

**Acceptance**:
- [ ] `git submodule add https://github.com/chisuhua/PTX-EMU.git external/PTX-EMU`
- [ ] `.gitmodules` 添加入口(参考 `.gitmodules:1-4` CppHDL 格式)
- [ ] submodule pin commit(per Oracle §E.1 推荐:`87820951` 或当前 HSK 兼容点)
- [ ] `git submodule update --init` 验证 submodule 内容
- [ ] `external/PTX-EMU/build/` 加入 `.gitignore`(防止构建产物入库)

**验证命令**:
```bash
git submodule status  # 显示 PTX-EMU commit hash + path
ls external/PTX-EMU/ | head -10  # 验证 submodule 已检出
```

**Commit**:
```bash
git add .gitmodules external/PTX-EMU .gitignore
git commit -m "chore(submodule): add external/PTX-EMU@<commit_hash>"
```

### T-S1-2: CMakeLists.txt 集成 PTX-EMU

**Acceptance**:
- [ ] `CMakeLists.txt` 添加 `add_subdirectory(external/PTX-EMU)`
- [ ] 设置 `PTX_EMU_BUILD_TESTS=OFF`(不构建 PTX-EMU 自家测试)
- [ ] 设置 `PTX_EMU_BUILD_SHARED=OFF`(强制静态库)
- [ ] 设置 `-fvisibility=hidden`(per Oracle §E.1 风险 R5)
- [ ] cpptlm_core 静态链接 PTX-EMU
- [ ] `tests/test_*` 添加 v0.5 MVP 测试目标
- [ ] 现有 ≥850 测试仍通过

**验证命令**:
```bash
cmake --build build -j$(nproc)
build/bin/cpptlm_tests --reporter compact
# 预期: All tests passed (≥850 assertions in ≥850 test cases)
```

**Commit**:
```bash
git add CMakeLists.txt
git commit -m "build(cmake): add_subdirectory(external/PTX-EMU) — submodule static link"
```

### T-S1-3: PtxEmuSubmoduleMVP (PTX functional facade,per Phase I.1 重构)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `src/tlm/gpu/ptx_emu_submodule_mvp.cc`
- [ ] **关键约束**:`ptx_emu_submodule_mvp.cc` 是**唯一** include PTX-EMU 头(`ptxsim/*.h` + `ptx_ir/*.h` + `memory/*.h` + `register/*.h`)的 .cc(编译防火墙)
- [ ] 其他 CppTLM 代码只见前向声明(`namespace ptxsim { class GPUContext; class SMContext; class WarpContext; }`)
- [ ] **Functional Construction**:`create_gpu_context()` / `get_sm_context()` / `get_warp_context()` / `decode_ptxir()` / `submit_kernel_request()`
- [ ] **Functional Execute**(★ 核心):`functional_execute_warp(warp, stmt, target_pc)` — 转发到 `ptxsim::WarpContext::execute_warp_instruction`,**不**增加 cycle
- [ ] **Functional State**:模板 `read_register<T>` / `write_register<T>` / `read_global_memory<T>` / `write_global_memory<T>` + `read_thread_pc` / `advance_thread_pc` / `read_active_mask` / `is_warp_finished` / `is_thread_exited`
- [ ] **Functional Module Getters**(供 CudaCoreAdapter 注入 timing):`create_scoreboard()` / `create_pipeline_latency_provider()` / `create_tensor_core_timing()`
- [ ] ❌ **删除**原 8 ABI 黑盒(`image_load` / `image_execute` / `image_unload` / `image_kernel_name` / `image_kernel_count` / `image_kernel_name_at` / `image_execute_named` / `module_version`)
- [ ] ❌ **删除**原白盒 `stepOneWarpInstruction` API(由 `warp_execute_instruction` 替代)
- [ ] `init(ptx_emu_root, GPUConfig)` + `shutdown()`(RAII 模式)
- [ ] **functional 调用不增加 cycle 计数**(per `cycle_counter_` 不变验证)— 与 timing model 严格分离
- [ ] `tests/test_ptx_emu_facade_*.cc` 6 个测试文件(per Phase I.1 §6 验收):
  - `[ptx-emu-facade][decode]` PTXIR 格式校验
  - `[ptx-emu-facade][arith]` ADD/SUB/MUL/DIV 寄存器结果
  - `[ptx-emu-facade][memory]` LD/ST 内存
  - `[ptx-emu-facade][branch]` SIMT 分支
  - `[ptx-emu-facade][barrier]` `bar.sync` 同步
  - `[ptx-emu-facade][state]` 状态读写 round-trip

**验证命令**:
```bash
# 编译防火墙检查(扩展为 ptx_ir + memory + register 也检查)
git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/" \
  -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
# 预期: 仅 src/tlm/gpu/ptx_emu_submodule_mvp.cc

# Functional 测试 PASS
ctest -R "test_ptx_emu_facade" --output-on-failure
```

**Commit**:
```bash
git add include/tlm/gpu/ptx_emu_submodule_mvp.hh src/tlm/gpu/ptx_emu_submodule_mvp.cc tests/test_ptx_emu_facade_*.cc CMakeLists.txt
git commit -m "feat(ptx-emu-mvp): PTX functional facade (depth-integration, per Phase I.1)"
```

### T-S1-4: CudaCoreAdapter (SM microarchitecture exploration,per Phase I.2 重构)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `src/tlm/gpu/cuda_core_adapter_mvp.cc`
- [ ] 持有 `PtxEmuSubmoduleMVP&` (facade,不直接 include PTX-EMU 头)
- [ ] **WarpState**(timing only,**不**含 PC):`{ cycle_count, exec_mask, blocked_cycles, scheduler_state }`
- [ ] `on_cta_arrival(cta_desc) → bool`(替代 `issueTask` + `dispatch_blackbox`):
  - 调 `sm->reserve_resources(shared_mem, warp_count)`(SM 资源反压)
  - 通过 facade 解码 PTX IR + 构造 KernelLaunchRequest + 提交
- [ ] `tick()` — ★ timing 主入口,驱动 `sm->exe_once()`(PTX-EMU 内部 3-Step 注入)
- [ ] `on_warp_complete(task_id, status)` — 完成回调
- [ ] `init(PtxEmuSubmoduleMVP& facade)`:注入 timing 模块:
  - `MinimalWarpSchedulerTLM`(per-cycle warp 调度)
  - `ScoreboardTLM`(注入 `IScoreboard`)
  - `PipelineTLM`(注入 `IPipelineLatencyProvider`)
  - `TensorCoreTLM`(注入 `ITensorCoreTiming`)
  - 一次性 `inject_timing_modules()` 调用 `sm->set_*()` 4 个 setter
- [ ] ❌ **删除** `dispatch_blackbox` / `dispatch_whitebox`(per Phase I.1 重构)
- [ ] ❌ **不**直接调 PTX-EMU 内部 functional 接口(`WarpContext::execute_warp_instruction` 等)— 全部通过 PtxEmuSubmoduleMVP facade
- [ ] `tests/test_cuda_core_adapter_mvp_*.cc` 6 个测试文件(per Phase I.2 §7 验收):
  - `[cuda-core][mvp][tick]` per-tick cycle 推进
  - `[cuda-core][mvp][scoreboard]` RAW hazard + allocate/release 计数
  - `[cuda-core][mvp][pipeline]` Pipeline latency 注入
  - `[cuda-core][mvp][dispatch]` on_cta_arrival 反压
  - `[cuda-core][mvp][warp-state]` WarpState 镜像(**不**含 PC)
  - `[cuda-core][mvp][injection]` 4 个 timing 模块注入路径

**验证命令**:
```bash
ctest -R "test_cuda_core_adapter_mvp" --output-on-failure
# 预期: 6 个 timing 测试 PASS
```

**Commit**:
```bash
git add include/tlm/gpu/cuda_core_adapter_mvp.hh src/tlm/gpu/cuda_core_adapter_mvp.cc tests/test_cuda_core_adapter_mvp_*.cc CMakeLists.txt
git commit -m "feat(cuda-core-mvp): SM microarchitecture exploration (timing model, per Phase I.2)"
```

---

## S2 Real-Board-Bind (W3-4)

### T-S2-1: Doorbell + SubmitQueue + CompletionRing(per Phase F-H.5 新 SubmitQueue)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/doorbell_mvp.hh` + `.cc`(SQ tail register + strong-order write)
- [ ] `Doorbell::ring()` 实现 `weak atomic write → strong-ordered store` (per design.md §6)
- [ ] 延迟区间断言 250-700ns(PCIe Gen5 x16,per [`docs/research/PCIe/PCIe_上的保序write.md`](../../../research/PCIe/PCIe_上的保序write.md) §4)
- [ ] 新建 `include/tlm/gpu/completion_ring_mvp.hh` + `.cc`(push + host_notify 重设计)
- [ ] **🆕** 新建 `include/tlm/gpu/submit_queue_mvp.hh` + `.cc`(per Phase F-H.5,WDU 分发网络)
  - `SubmitQueue::enqueue(CtaDescriptor) → bool`(per-cluster pending FIFO 32 slot,per `US20240036952A1`)
  - `SubmitQueue::tick()` 派发(per-core active 4 slot,per `US20240036952A1`)
  - `SubmitQueue::on_warp_complete(task_id, status)` 反向流
  - `select_target_core(cta_desc) → uint8_t` MVP 固定返回 0
- [ ] `tests/test_doorbell_strong_order_mvp.cc`:latency 区间 + 同 stream 顺序 PASS
- [ ] `tests/test_submit_queue_mvp_*.cc` 5 个测试(route/enqueue/dispatch/complete/concurrent)
- [ ] `tests/test_completion_ring_mvp.cc` PASS

**Commit**:
```bash
git add include/tlm/gpu/doorbell_mvp.hh src/tlm/gpu/doorbell_mvp.hh ...
git commit -m "feat(doorbell-mvp): SQ tail register with strong-order write path (250-700ns)"
git commit -m "feat(submit-queue-mvp): WDU distribution network (single-SM, per Phase F-H.5)"
git commit -m "feat(completion-ring-mvp): push + host_notify hook"
```

### T-S2-2: DGpuBoardTLM (6 组件包装,per Phase F-H.2 修订)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc`(~500 LOC,per Phase F-H.2 §2.3.1 架构)
- [ ] `DGpuBoardTLM : public ChStreamModuleBase`
- [ ] 内部成员持有:**6 组件**(per Phase I.2 §1.1):
  - `DGpuBar` + `Doorbell` + **`CommandProcessor`** + **`TmuDispatchProcessor`** + **`SubmitQueue`**(🆕 per Phase F-H.5) + `CompletionRing` + `CudaCoreAdapter` + `PtxEmuSubmoduleMVP`
- [ ] 构造函数 `DGpuBoardTLM(name, EventQueue* eq, const DGpuBoardParams& params)`
- [ ] `tick()` — ★ 4 阶段串联(per Phase F-H.2 §4 + Phase I.2):
  ```cpp
  cp_.tick();        // FETCH + DECODE NVIDIA method packet + DISPATCH Pm4MethodDispatch
  tmu_.tick();       // dep chain advance + pre_dispatch 反压
  sq_.tick();        // WDU 路由 + dispatch_to_core
  cuda_core_.tick(); // 驱动 sm->exe_once() + 镜像 WarpState
  ```
- [ ] `install_kernel_module(vram_addr, size)` + `submit_kernel(req)` + `write_reg(offset, value)`(模拟 UsrLinuxEmu driver)
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(DGpuBoardTLM)`

**验证**:
```bash
cmake --build build -j8
ctest -R "test_dgpu_board_v1_mvp_from_config" --list-tests
```

**Commit**:
```bash
git commit -m "feat(dgpu-board-mvp): DGpuBoardTLM ChStreamModuleBase with 6 components (per Phase F-H.2)"
```

### T-S2-3: UsrLinuxEmuIoctlStub (IOCTL 0x27/0x28/0/29 + 0x01 pushbuffer,per Phase F-H.3 修订)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc`(~300 LOC)
- [ ] 实现 **4 IOCTL**(per Phase F-H.3 H3 修订,4 IOCTL 端到端):
  - 0x27 `LOAD_KERNEL_MODULE` → `DGpuBoardTLM::install_kernel_module()`(handler 真实工作,per UsrLinuxEmu ADR-090 §D2.1)
  - **0x28 `LAUNCH_KERNEL_MODULE` → 永久 -ENOSYS**(per UsrLinuxEmu ADR-090 §D2.2 + ADR-023 §D4 append-only 治理)
  - 0x29 `UNLOAD_KERNEL_MODULE` → `DGpuBoardTLM::uninstall_kernel_module()`
  - **0x01 `PUSHBUFFER_SUBMIT_BATCH` → 写 gpfifo_entries[] 到 DGpuBar.vram.pushbuffer_ring + Doorbell ring**(per Phase F-H.3 真实 launch 入口)
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(UsrLinuxEmuIoctlStub)`
- [ ] `tests/test_usrlxemu_ioctl_stub.cc`:**4 IOCTL** PASS(含 0x28 -ENOSYS 验证 + driver fallback 路径)

**Commit**:
```bash
git commit -m "feat(usrlxemu-ioctl-stub): 4 IOCTL stub (0x27/0x28/0x29/0x01) for Mode B dGPU board"
```

### T-S2-4: JSON config + validate_topology

**Acceptance**:
- [ ] 新建 `configs/dgpu_board_v1_mvp.json.in`(CMake configure_file 注入 `${PTX_EMU_ROOT}`)
- [ ] 1 个 `DGpuBoardTLM` 模块(`ptx_emu_root` 用 `${PTX_EMU_ROOT}` placeholder)
- [ ] 1 个 `MemoryTLM` 模块作为 H2D DMA + VRAM backing
- [ ] 1 个 `UsrLinuxEmuIoctlStub` 绑定 dgpu_board0
- [ ] 根 CMakeLists 配置 `configure_file(...)`
- [ ] 纳入 `validate_topology` CMake target 扫描
- [ ] 新建 `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION 验收)

**6 SECTION E2E 验收**(per ADR-SOC-06 G-MVP-2):
1. `validate_topology` 通过该 JSON
2. `instantiateAll` 返回 true,`getInstance("dgpu_board0")` 存在
3. H2D: 写 VRAM → `install_kernel_module` 返回 0 + image_handle ≠ 0
4. Launch: N=4 stream 各 1 次 IOCTL 0x01 pushbuffer → `eq.run(budget)` 内 CQ 收到 N 个 entry(per Phase F-H.3)
5. host_notify 触发 ≥1 次
6. 负面: `ptx_emu_root` 指向不存在路径 → `instantiateAll` 失败

**验证**:
```bash
cmake --build build --target validate_topology
ctest -R "test_dgpu_board_v1_mvp_from_config" --output-on-failure
# 预期: 6 SECTION PASS
```

**Commit**:
```bash
git commit -m "feat(configs): dgpu_board_v1_mvp.json with validate_topology support"
git commit -m "test(dgpu-board-v1-mvp): 6 SECTION E2E + 4 IOCTL tests (per Phase F-H.3)"
```

---

## S3 Warp-Precision (W5-6)

### T-S3-1: Pm4Decoder (NVIDIA method packet,per Phase F-H.3 路径 3)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/pm4_decoder_mvp.hh` + `.cc`(~200 LOC)
- [ ] **`Pm4MethodHeader` 结构体**(per `unpackPm4Header` NVIDIA 字段布局):
  ```cpp
  uint32_t inc : 1;            // bit 0 (Increment register)
  uint32_t method_addr : 15;   // bits 1-15 (32K method addresses)
  uint32_t subchannel : 4;     // bits 16-19 (NVIDIA 4-bit)
  uint32_t data_count : 4;     // bits 20-23
  uint32_t reserved : 8;       // bits 24-31
  ```
- [ ] **`Pm4MethodDispatch`**(替代原 `Pm4Packet`):
  ```cpp
  struct Pm4MethodDispatch {
      uint16_t method_addr;    // 4 method_addr ranges (per Phase F-H.3 §5.2)
      uint8_t subchannel_id;
      uint8_t data_count;
      // ... decoded fields: grid, block, shared_mem, args_vram_addr, ...
  };
  ```
- [ ] `parse_method(method_header, payload, max_dwords) → Pm4MethodDispatch`(替代原 `parse_type3`)
- [ ] **4 method_addr ranges**(per Phase F-H.3 修订):
  - `0x4000-0x40FF`:DISPATCH_DIRECT
  - `0x4200-0x42FF`:EVENT_WRITE
  - `0x4400-0x44FF`:RELEASE_MEM
  - `0x4500-0x45FF`:ACQUIRE_MEM
- [ ] `tests/test_pm4_decoder_mvp.cc`:NVIDIA method packet bit field round-trip + 4 method_addr range PASS

**Commit**:
```bash
git commit -m "feat(pm4-decoder-mvp): NVIDIA method packet parsing (per Phase F-H.3 path 3)"
```

### T-S3-2: CommandProcessor (5-state FSM,per Phase F-H.3 修订)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/command_processor_mvp.hh` + `.cc`(~250 LOC)
- [ ] 5-state FSM:IDLE → FETCH → DECODE → DISPATCH → COMPLETE
- [ ] `submit_kernel(...)` API
- [ ] `tick()` 由 EventQueue 调度
- [ ] **FETCH**:`mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry))`(per Phase F-C.3 H1 修订)
- [ ] 内部调用 `Pm4Decoder::parse_method` 解析(替代 `parse_type3`)
- [ ] **DISPATCH** → `tmu_.submit(Pm4MethodDispatch)`(替代直接构造 Pm4Packet)
- [ ] 5-state FSM 状态转换正确(测试覆盖所有 transition)
- [ ] `tests/test_command_processor_mvp.cc`:5 transition 测试 PASS
- [ ] `tests/test_pm4_decoder_mvp_integration.cc`:CP + Decoder 集成 PASS

**Commit**:
```bash
git commit -m "feat(command-processor-mvp): 5-state FSM with Pm4Decoder integration (NVIDIA method packet)"
```

### T-S3-3: TmuDispatchProcessor (TMU Glue,反压停 fetch per Phase F-D.2 H5 修订)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc`(~250 LOC)
- [ ] `TmuDispatchRecord` 9 字段(MVP 简化,vs v0.5 21 字段)
- [ ] `PreExitPolicy` 枚举:NONE 档(MVP)
- [ ] `inflight_kernel_reqs_ map` 32 slot + **反压停 fetch**(`BACKPRESSURED` 替代原 LIFO 驱逐,per Phase F-D.2 H5)
- [ ] 接口契约:`submit(record) → TmuSubmitResult` / `on_complete(record)` / `try_chain_dependent(record)`
- [ ] `TmuSubmitResult` 枚举:SUBMITTED / **BACKPRESSURED** / DEP_LATCH_MISMATCH / **SUBMIT_QUEUE_REJECTED**(per Phase F-H.4 修订,新增)
- [ ] 依赖锁存器 `wait_on_latch_id ↔ arrive_at_latch_id` 匹配检查
- [ ] pre-dispatch 3 段条件检查(dep_enable + 容量 + resource)
- [ ] **`tmu_.tick()` 推进 dep chain**(同链路,不发起新 dispatch)
- [ ] **依赖**:`SubmitQueue& submit_queue_` + `CompletionRing& cq_`(per Phase F-H.4 修订,不直接调 CudaCoreAdapter)
- [ ] `tests/test_tmu_dispatch_processor_mvp.cc` ~10 测试 PASS

**Commit**:
```bash
git commit -m "feat(tmu-dispatch-mvp): TMU Glue with dep chain + backpressure (32 slot, per Phase F-D.2 H5)"
```

### T-S3-4: ~~CudaCoreAdapter 白盒路径~~ → **🗑️ 删除**(per DP4=C 决策)

**Status**: ~~**Active**~~ → **🗑️ Cancelled**

~~**Acceptance**:~~
- [ ] ~~升级 `cuda_core_adapter_mvp.cc` 加入白盒 `dispatch_whitebox`~~
- [ ] ~~per-warp WarpState 跟踪(pc + cycle_count + register_deps)~~
- [ ] ~~`PtxEmuSubmoduleMVP::stepOneWarpInstruction` API 接入(若 PTX-EMU 接受)~~
- [ ] ~~`tests/test_cuda_core_adapter_mvp_whitebox.cc`:per-warp cycle 跟踪 PASS~~

**🗑️ 取消理由**(per Phase F-H.1 / Phase I.1 修订):
- DP4=C 决策(2026-08-20):MVP 永久禁用白盒路径
- Phase I.1 重构:CudaCoreAdapter 改为**唯一**深度集成 PTX-EMU 内部接口路径,不再有"白盒"vs"黑盒"二分
- 替代:T-S1-4 CudaCoreAdapter 已通过 `tick()` + 4 个 timing 注入模块实现微架构探索
- 跨仓协调:不需 HSK-7 公告,不需 PTX-EMU 新增 `stepOneWarpInstruction` API

**Commit**: N/A (no work needed)

---

## S4 Production (W7-10,per Phase I.2 修订)

### T-S4-1: ~~ScoreboardTLM 升级 production~~ → **🗑️ 删除**(per Phase I.2 修订)

**Status**: ~~**Active**~~ → **🗑️ Cancelled**

~~**Acceptance**:~~
- [ ] ~~新建 `include/tlm/gpu/scoreboard_tlm_v05_mvp.hh`(继承现有 `scoreboard_tlm.hh`)~~
- [ ] ~~新增 `WarpState { pc, cycle_count, register_deps }` 数据结构~~
- [ ] ~~`tick()` 增加 per-warp tracking~~
- [ ] ~~与 `PtxEmuSubmoduleMVP::stepOneWarpInstruction` cycle_count 同步~~

**🗑️ 取消理由**(per Phase I.2 修订):
- `include/tlm/gpu/scoreboard_tlm.hh` 已存在,作为 D1-Full P1 Phase 1 核心模块已 ship
- CudaCoreAdapter 直接集成现有 `ScoreboardTLM`,**不**创建新的 `*_v05_mvp.hh`
- WarpState 重新设计为 timing only(不含 PC),由 CudaCoreAdapter 定义

**Commit**: N/A (no work needed,use existing ScoreboardTLM)

### T-S4-2: ~~PipelineTLM 升级 production~~ → **🗑️ 删除**(per Phase I.2 修订)

**Status**: ~~**Active**~~ → **🗑️ Cancelled**

~~**Acceptance**:~~
- [ ] ~~新建 `include/tlm/gpu/pipeline_tlm_v05_mvp.hh`(继承现有 `pipeline_tlm.hh`)~~
- [ ] ~~保留 A100 latency 表~~
- [ ] ~~新增 `issue(latency)` API,响应 PTX-EMU::Pipeline::step_b_set_blocked_cycles~~

**🗑️ 取消理由**(per Phase I.2 修订):
- `include/tlm/gpu/pipeline_tlm.hh` 已存在(`IPipelineLatencyProvider` 实现)
- CudaCoreAdapter 直接集成现有 `PipelineTLM`,**不**创建新的 `*_v05_mvp.hh`

**Commit**: N/A (no work needed,use existing PipelineTLM)

### T-S4-3: 全量 baseline 验证(≥880 测试,数字待 S1-S3 完成后重核)

**Acceptance**:
- [ ] `build/bin/cpptlm_tests --reporter compact`
- [ ] 预期 ≥880 test cases PASS(v0.4.1 baseline 850 + MVP 新增 ≥50)
  - 实际数字待 S1-S3 实施完成后重新核对
- [ ] assertions ≥19000
- [ ] 无 regression

**验证命令**:
```bash
build/bin/cpptlm_tests --reporter compact 2>&1 | tail -3
# 预期: All tests passed (≥19000 assertions in ≥880 test cases)
```

**Commit**:
```bash
git tag -a v0.5.0-MVP-rc1 -m "v0.5.0-MVP-rc1: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → SQ → CudaCore + PTX-EMU functional/timing split"
git push origin v0.5.0-MVP-rc1
```

### T-S4-4: v0.5.0-MVP tag + docs

**Acceptance**:
- [ ] `CHANGELOG.md` 记录 v0.5.0-MVP release
- [ ] Tag `v0.5.0-MVP` with commit message
- [ ] `docs/soc_arch/modules/README.md` 已同步 v0.5 MVP **7 模块**(6 + SubmitQueue)
- [ ] `docs/soc_arch/adr/README.md` 已更新 ADR-SOC-06 索引
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS

**Commit**:
```bash
git add CHANGELOG.md
git commit -m "docs(changelog): record v0.5.0-MVP release (MVP slice)"
git tag -a v0.5.0-MVP -m "cpptlm-v05-mvp: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → SQ → CudaCore + PTX-EMU functional/timing split"
```

---

## 风险登记(per ADR-SOC-06 §6.3,per Phase J 修订)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit @ `87820951` + 月度 bump PR |
| ~~R2 | PTX-EMU 维护者拒收 `stepOneWarpInstruction` API~~ | — | — | **🗑️ 风险已消除(per DP4=C + Phase I.1)**:MVP 永久仅深度集成路径,无跨仓依赖 |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构;0x28 永久 -ENOSYS(per Phase F-H.3)|
| R4 | CommandProcessor 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| ~~R5 | TmuDispatchProcessor LIFO 频繁驱逐~~ | — | — | **🗑️ 风险已消除(per Phase F-D.2 H5)**:LIFO → 反压停 fetch,容量满拒绝不驱逐 |
| R6 | 6-10 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ 严格 TDD 5 步 |
| R7 | PtxEmuSubmoduleMVP 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 + CI 拦截(per Phase I.1 §1.1 验证命令) |
| R8 | PTX-EMU submodule 构建依赖扩散 | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R9 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | MVP 仅"内部一致性验证" |
| **R10** | functional/timing 边界误用 | 中 | 高 | 编译期隔离:本 facade 接口**禁止** `WarpContext::execute_warp_instruction` 等;代码评审保证 |
| **R11** | WarpState 误加 PC 字段 | 低 | 中 | 文档 + 接口表明确分离(per Phase I.2 §3);测试验证 |

---

## 验收检查表

最终 v0.5.0-MVP tag 前(per Phase J 修订):
- [ ] T-S1-1 ~ T-S1-2 完成(submodule + CMake)
- [ ] **T-S1-3** PtxEmuSubmoduleMVP **PTX functional facade** 完成
- [ ] **T-S1-4** CudaCoreAdapter **SM microarchitecture** 完成
- [ ] T-S2-1 Doorbell + SubmitQueue + CQ 完成
- [ ] T-S2-2 DGpuBoardTLM **6 组件** 包装完成
- [ ] T-S2-3 UsrLinuxEmuIoctlStub **4 IOCTL** 完成
- [ ] T-S2-4 JSON config + validate_topology 完成
- [ ] T-S3-1 Pm4Decoder **NVIDIA method packet** 完成
- [ ] T-S3-2 CommandProcessor 5-state FSM 完成
- [ ] T-S3-3 TmuDispatchProcessor **反压停 fetch** 完成
- [ ] ~~T-S3-4 CudaCoreAdapter 白盒路径~~ **🗑️ 已取消**
- [ ] ~~T-S4-1 ScoreboardTLM 升级~~ **🗑️ 已取消(用现有)**
- [ ] ~~T-S4-2 PipelineTLM 升级~~ **🗑️ 已取消(用现有)**
- [ ] T-S4-3 全量 baseline ≥880 测试 PASS
- [ ] T-S4-4 v0.5.0-MVP tag + docs
- [ ] 编译防火墙验证 PASS(`git grep "include.*ptxsim\|include.*ptx_ir"` 仅命中 `ptx_emu_submodule_mvp.cc`)
- [ ] 6 SECTION E2E 测试 PASS
- [ ] docs 同步检查 PASS(`scripts/test/docs_sync_check.sh --strict`)
- [ ] 跨仓协调:PTX-EMU submodule pin 已 bump @ `87820951`(**无 HSK 联署**,per DP4=C)
- [ ] `git tag -a v0.5.0-MVP -m "..."`

---

**Cc**: CppTLM Team · PTX-EMU Architecture Team · UsrLinuxEmu Architecture Team

**Refs**:
- [`proposal.md`](../proposal.md)
- [`design.md`](../design.md)
- [`../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)(per Phase I.4 ADR 移动)
- [`../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md)(per Phase F-H.8 / Phase I 修订)

---

**起草**: Sisyphus (2026-08-19 初版;2026-08-20 Phase J 对齐:ADR-SOC-06 + functional/timing 分离 + SubmitQueue + DP4=C 决策)
**Owner**: CppTLM Team
**状态**: 📋 Tasks — 等 W1 S1 启动后开始实施(已完成文档侧 2 commit: `d36613b/5dbaf2b/2c72b7d`)