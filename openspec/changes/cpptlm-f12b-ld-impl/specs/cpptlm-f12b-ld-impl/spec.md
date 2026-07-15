## ADDED Requirements

### Requirement: cpptlm-bridge-memorybridge

The system MUST provide a `MemoryBridge` class in `include/tlm/gpu/memory_bridge.hh` and `src/tlm/gpu/memory_bridge.cc` that inherits from `CppTLMBridge` (defined in PTX-EMU commit `8dc000ec` `include/cudart/cpptlm_bridge.h`) and implements all five pure virtual methods: `version()`, `submit_kernel(uint64_t, const char*, uint32_t×6, const void**, size_t, size_t, uint64_t)`, `poll_kernel(uint64_t)`, `synchronize_stream(uint64_t)`, and `global_access(uint64_t, uint64_t, uint8_t)`. The class MUST be constructible with three CppTLM-side dependencies: `KernelLaunchTLM*`, `CrossbarTLM*`, and `MemoryController*`. The header MUST `deep_copy_args_()` helper that copies `kernel_args` on the call stack to prevent use-after-free (PTX-EMU host-side args may be invalidated after `submit_kernel` returns).

#### Scenario: ABI 真值源 — MemoryBridge 字节级匹配 PTX-EMU 8dc000ec
- **WHEN** CppTLM 通过 `ExternalProject_Add` 引用 PTX-EMU commit `8dc000ec` 的 `cpptlm_bridge.h`
- **THEN** `MemoryBridge` 5 虚方法签名与 PTX-EMU 头文件**逐字节**一致
- **AND** `version()` 返回 `CPPTLMBRIDGE_VERSION` (= 1) 编译期断言通过

#### Scenario: nullptr 全局指针 = 字节级向后兼容
- **WHEN** PTX-EMU 端 `g_cpptlm_bridge == nullptr`（独立模式）
- **THEN** MemoryBridge 不被调用
- **AND** `cudaLaunchKernel` / `cudaStreamSynchronize` / `LdHandler` 走 fallback 路径
- **AND** 现有 764+ CppTLM 测试零退化（与 baseline worktree 字节级一致）

#### Scenario: kernel_args deep-copy 防止 use-after-free
- **WHEN** PTX-EMU 端 `submit_kernel(args)` 在 args 内存即将失效时调用
- **THEN** MemoryBridge `deep_copy_args_()` 在调用栈内 deep-copy 所有 args
- **AND** `pending_kernels_[id].deep_copied_args` 持有独立 ownership
- **AND** PTX-EMU host 端 args 失效不影响 CppTLM 端 `poll_kernel` 返回正确结果

#### Scenario: 12 端点 enum 编译期拦截
- **WHEN** 未来 CppTLM 端 enum 漂移与 PTX-EMU 端不一致
- **THEN** `static_assert` 编译失败，CI 中止
- **AND** 阻塞后续 release，防止 ABI 漂移上线

---

### Requirement: cpptlm-bridge-kernellaunch

The system MUST provide a `KernelLaunchTLM` class in `include/tlm/gpu/kernel_launch_tlm.hh` and `src/tlm/gpu/kernel_launch_tlm.cc` that inherits from `ChStreamModuleBase`. The class MUST be registered via `REGISTER_CHSTREAM(KernelLaunchTLM)`. The `tick()` method MUST poll the bridge via `synchronize_stream(0)`, then loop calling PTX-EMU `exe_once()` up to `MAX_PTX_STEPS_PER_TICK=10000` times per tick, then check kernel completion. The class MUST have FIFO `pending_` queue, `set_ptx_emu_context()` for PTX-EMU 端 handle, and 4 setters for D1-Full P1 阶段 adapters (`set_scoreboard` / `set_pipeline_provider` / `set_tensor_core_timing` / `set_async_completion`).

#### Scenario: EventQueue 主动驱动
- **WHEN** CppTLM EventQueue 每 tick 调用 `KernelLaunchTLM::tick()`
- **THEN** poll bridge `synchronize_stream(0)`（默认 stream）
- **AND** 循环调用 `call_ptx_emu_exe_once_()` 最多 10000 次
- **AND** 每次后检查 `bridge_->poll_kernel()` 完成状态
- **AND** 完成 kernel 从 `pending_` FIFO erase

#### Scenario: 死循环熔断 — MAX_PTX_STEPS_PER_TICK
- **WHEN** PTX-EMU 内部状态卡住（如 deadlock 死循环）
- **THEN** `tick()` 在 10000 次 exe_once 后强制退出
- **AND** 下个 tick 继续（无永久 hang）
- **AND** 日志记录熔断次数以便监控

#### Scenario: g_cpptlm_bridge nullptr 字节级回退
- **WHEN** PTX-EMU 端 `g_cpptlm_bridge == nullptr`
- **THEN** `KernelLaunchTLM::tick()` 走原独立模式路径
- **AND** 不调用 `bridge_->synchronize_stream`
- **AND** 现有 600+ PTX-EMU 测试零退化

---

### Requirement: cpptlm-bridge-3-core-modules

The system MUST provide three core modules in `include/tlm/gpu/`: `ScoreboardTLM` (≥12 entries hazard table implementing `IScoreboardInternal` with `has_free_entry()` / `allocate(reg_id, warp_id)` / `release(reg_id, warp_id)`), `PipelineTLM` (5+V pipeline abstraction implementing `IPipelineLatencyInternal` with `get_fractional_cycles_by_type(instr_type, pipeline_id)`), and `TensorCoreTLM` (6 precision support implementing `ITensorCoreTimingInternal` with `get_latency(precision)`). The 12 endpoints (PipelineId 6 + TcPrecision 6) MUST match PTX-EMU commit `8dc000ec`+ enum values via `static_assert` compile-time guards.

#### Scenario: Scoreboard 12 entries hazard 检测
- **WHEN** 4 Adapter 调用 `scoreboard_->allocate(reg_id, warp_id)`
- **THEN** 检查 ≥12 entries 是否已满
- **AND** 已满 → 返回 false（hazard detected）
- **AND** 未满 → 占用 entry，返回 true

#### Scenario: Pipeline 5+V 抽象返回分数 cycle
- **WHEN** PTX-EMU exe_once 调用 `pipeline_provider_->get_fractional_cycles_by_type(instr_type, pipeline_id)`
- **THEN** 返回 `double` 分数 cycle（≥ 0.0）
- **AND** 例如 `SASS_OP_FMA` + `PipelineId::FMA` → 4.0 cycles
- **AND** 无效组合 → 返回 0.0（fallback 到 `InstructionLatencyTable`）

#### Scenario: TensorCore 6 精度 latency 查询
- **WHEN** TensorCore instruction 调用 `tensor_core_timing_->get_latency(precision)`
- **THEN** 返回 `uint32_t cycles`（如 FP16 → 16, FP32 → 32, INT8 → 8）
- **AND** 未知 precision → 返回 0（fallback）
- **AND** `set_blocked_cycles_for_active()` 设置 `blocked_cycles_remaining`

#### Scenario: 12 端点 enum 双向 static_assert
- **WHEN** CppTLM 端编译时
- **THEN** `static_assert(PipelineId::SASS_0 == 0)` 等 12 个断言通过
- **AND** 与 PTX-EMU 端 enum 值字节级一致
- **AND** 任何修改都会触发编译失败（CI 中止）

---

### Requirement: cpptlm-bridge-4-adapters

The system MUST provide four adapters in `include/tlm/gpu/adapter/`: `CppTLMWarpSchedulerAdapter` (Task 10b), `CppTLMWarpSchedulerAdapter` extending PTX-EMU's `WarpScheduler` class with `WarpContext*` ↔ `uint32_t` translation, and `CppTLMScoreboardAdapter` / `CppTLMPipelineAdapter` / `CppTLMTensorCoreAdapter` extending their respective CppTLM-side `IScoreboardInternal` / `IPipelineLatencyInternal` / `ITensorCoreTimingInternal` interfaces. The 12 endpoint `static_assert` MUST be in the adapter headers to guarantee ABI compatibility at compile time.

#### Scenario: WarpContext* ↔ uint32_t 双向转换
- **WHEN** PTX-EMU 端 `WarpScheduler::schedule_next()` 返回 `WarpContext*`
- **THEN** Adapter 转 `uint32_t warp_id` → 查 `WarpSchedulerTLM` → 返回
- **AND** 反向同理（`warp_id` → `WarpContext*` 查 PTX-EMU 内部表）

#### Scenario: nullptr 行为 — fallback 到 InstructionLatencyTable
- **WHEN** KernelLaunchTLM 4 setter 任意 nullptr
- **THEN** 跳过对应 Adapter 调用
- **AND** 走 `InstructionLatencyTable::instance().get(stmt.type).cycles` fallback
- **AND** exe_once 行为字节级与原同步模式一致

#### Scenario: 4 Adapter 集中注册 — G-D4 12 端点 0-5 双向一致
- **WHEN** CppTLM 端编译时
- **THEN** 4 个 Adapter 全部编译通过
- **AND** 12 端点 `static_assert` 0-5 双向一致（与 PTX-EMU 端）
- **AND** CI 中 4 Adapter 任一编译失败即中止 release

---

### Requirement: cpptlm-bridge-async-completion

The system MUST provide an `AsyncCompletionAdapter` class in `include/tlm/gpu/async_completion_adapter.hh` implementing `IAsyncCompletion` with `register_completion_callback(uint64_t id, std::function<void()> cb)` (store) and `fire_completion(uint64_t id)` (invoke + erase). Phase 8.B MUST keep callbacks stored but not invoked (placeholder); Phase 9+ TMA async copy will invoke callbacks. The class MUST be null-safe — if `async_completion_ == nullptr` in KernelLaunchTLM, no behavior change.

#### Scenario: Phase 8.B 占位 — 存回调但不触发
- **WHEN** Phase 8.B 独立模式（`async_completion_ == nullptr`）
- **THEN** Adapter 不被调用
- **AND** `exe_once` 行为字节级与原同步模式一致
- **AND** 现有 764 CppTLM 测试零退化

#### Scenario: Phase 9+ TMA async copy 触发
- **WHEN** Phase 9+ TMA async copy 完成时调用 `fire_completion(id)`
- **THEN** 找到 `id` 对应的 callback 并 invoke
- **AND** callback 执行后从 `pending_callbacks_` erase
- **AND** 异步完成路径不阻塞 host 线程

---

## Capabilities (Derived)

### New Capabilities

- `cpptlm-bridge-memorybridge`: CppTLMBridge 实现 + kernel_args deep-copy + NoC 路由查询
- `cpptlm-bridge-kernellaunch`: EventQueue 集成 + PTX-EMU 驱动 + FIFO 调度 + 4 内部模块预留
- `cpptlm-bridge-3-core-modules`: ScoreboardTLM (≥12 entries) + PipelineTLM (5+V) + TensorCoreTLM (6 精度)
- `cpptlm-bridge-4-adapters`: 4 CppTLM Adapter 层 + 12 端点 static_assert + WarpContext* ↔ uint32_t 转换
- `cpptlm-bridge-async-completion`: IAsyncCompletion 占位（Phase 8.B 不触发，Phase 9+ TMA 触发）

## 验收门（与 tasks.md Gates 对应）

- [ ] **G-F0** vector_add 烟雾测试：输出逐元素 diff + 延迟 ≤ 2× baseline
- [ ] **G-F1** `g_cpptlm_bridge == nullptr` 时 PTX-EMU 零退化
- [ ] **G-F5** `cpptlm_tests [gpu][f12b]` 全 PASS
- [ ] **G-D1** 3 纯虚接口编译通过，无 CppTLM 头文件污染 PTX-EMU
- [ ] **G-D2** `set_blocked_cycles_for_active()` 正确
- [ ] **G-D3** `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle
- [ ] **G-D4** 4 Adapter `static_assert` 12 端点 0-5 双向一致
- [ ] **G-D6** 4 setter 全 nullptr 时 PTX-EMU 零退化
- [ ] **G-D7** scoreboard/pipeline/TC 任意 nullptr 时回退到 InstructionLatencyTable

## 关联文档

- **综合计划**: `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md` §2-§5
- **ADR**: `docs/adr/ADR-NV-02-phase8b-d1-strategy.md` §5 R1-R9 + §6.2 G-D1~G-D8
- **PTX-EMU 端 HSK-1**: commit `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d`
- **vendor 文件**: `include/cudart/cpptlm_bridge.h`（SHA-256 `c19e66a32de398e6bba2042f3f19923ff89dbc02f10bbf310c073ad3a8ff3dbe`）
- **Phase 0.5 baseline**: `docs/superpowers/specs/2026-07-15-phase05-baseline-report.md` (188/202 + 764/764 pass)
