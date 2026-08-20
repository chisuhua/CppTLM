# cpptlm-v05-mvp: dGPU Board MVP Slice — Design

> **配套**: [`proposal.md`](../proposal.md) · [`tasks.md`](../tasks.md) · [`specs/`](../specs/)
> **状态**: 📐 Design — 与 ADR-SOC-06 + Phase I/F-H 同步(per Phase J 2026-08-20 对齐)· **Owner**: CppTLM Team
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)

> 本文件是**MVP 切片的架构设计索引**(per Phase J 对齐)。详细契约见各模块设计文档:
> - [`docs/soc_arch/modules/dgpu-board.md`](../../../docs/soc_arch/modules/dgpu-board.md) — DGpuBoardTLM(6 组件包装)
> - [`docs/soc_arch/modules/command-processor.md`](../../../docs/soc_arch/modules/command-processor.md) — CommandProcessor(NVIDIA method packet)
> - [`docs/soc_arch/modules/pm4-decoder.md`](../../../docs/soc_arch/modules/pm4-decoder.md) — Pm4Decoder(NVIDIA method packet)
> - [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../../../docs/soc_arch/modules/tmu-dispatch-processor.md) — TmuDispatchProcessor(反压停 fetch)
> - [`docs/soc_arch/modules/submit-queue.md`](../../../docs/soc_arch/modules/submit-queue.md) — **🆕 SubmitQueue(WDU 分发网络)**
> - [`docs/soc_arch/modules/cuda-core-adapter.md`](../../../docs/soc_arch/modules/cuda-core-adapter.md) — **CudaCoreAdapter(SM 微架构探索器,timing model)**
> - [`docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../../../docs/soc_arch/modules/ptx-emu-submodule-mvp.md) — **PtxEmuSubmoduleMVP(PTX functional facade,functional model)**

---

## 1. 架构概览(MVP,per Phase F-H.2 + Phase I.2)

```
┌─────────────────────────────────────────────────────────────────────┐
│              Host (UsrLinuxEmu + TaskRunner)                         │
│                                                                     │
│  cuModuleLoadData(image_bytes)                                      │
│    → ioctl(0x27 LOAD_KERNEL_MODULE) → HAL #66 → H2D DMA            │
│      → 写 image_bytes → DGpuBar.vram_base()                          │
│                                                                     │
│  cuLaunchKernel(grid, block, args, ...)                             │
│    → ioctl(0x01 PUSHBUFFER_SUBMIT_BATCH) → 写 gpfifo_entries[]      │
│      → DGpuBar.vram.pushbuffer_ring                                  │
│      → ioctl(0x28 LAUNCH_KERNEL_MODULE) → 永久 -ENOSYS              │
│        (handler 锁定,per UsrLinuxEmu ADR-090 §D2.2)                  │
│                                                                     │
│  cuStreamSynchronize(stream) → FenceRegistry.wait → return CUDA_SUCCESS │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (PCIe TLP / MMIO direct)
┌─────────────────────────────────────────────────────────────────────┐
│                    CppTLM DGpuBoardTLM v0.5-MVP                       │
│                                                                     │
│  PCIe Substrate: DGpuBar + Doorbell (strong-order 250-700ns)         │
│         ↓                                                            │
│  CommandProcessor (5-state FSM) → Pm4Decoder (NVIDIA method packet)│
│         ↓ 构造 Pm4MethodDispatch → tmu_.submit()                    │
│  TmuDispatchProcessor (TMU Glue, 32 slot + dep chain + 反压停 fetch) │
│         ↓ pre_dispatch → sq_.enqueue(cta_descriptor)                 │
│  SubmitQueue (WDU 分发网络,per-cluster pending FIFO + per-core active)│
│         ↓ dispatch_to_core(cta_desc) → cuda_core_[target].on_cta_arrival│
│  CudaCoreAdapter (★ SM 微架构探索器,per Phase I.2)                  │
│    ├─ per-tick tick() → sm->exe_once()                              │
│    │     ├─ WarpScheduler 选 warp(MinimalWarpSchedulerTLM)         │
│    │     ├─ Step A: ScoreboardTLM.allocate() (RAW hazard)            │
│    │     ├─ Step B: ★ PtxEmuSubmoduleMVP.functional_execute_warp()   │
│    │     │        → PTX-EMU WarpContext::execute_warp_instruction    │
│    │     │        → 寄存器/内存/PC 按指令语义更新(FUNCTIONAL)       │
│    │     ├─ Step C: ScoreboardTLM.release()                          │
│    │     └─ PipelineTLM.get_fractional_cycles() → blocked_cycles   │
│    ├─ 镜像 WarpState[warp_id](cycle_count / exec_mask / blocked)   │
│    └─ 完成 → sq_.on_warp_complete → tmu_.on_complete → CQ.push    │
│         ↓                                                            │
│  CompletionRing (push + host_notify hook)                            │
│         ↓                                                            │
│  host_notify_() → cuStreamSynchronize 返回                          │
└─────────────────────────────────────────────────────────────────────┘
```

**链路要点**(per Phase F-H.2):CP fetch NVIDIA method packet → TMU 依赖解耦 → **SubmitQueue WDU 分发网络** → CudaCore 深度集成 PTX-EMU(timing 主入口 + 3 个 TLM 模块注入)

---

## 2. **6 个新模块**(MVP,per Phase F-H.5 + Phase I.2 functional/timing 分离)

| 模块 | 位置 | 角色 | MVP 简化 |
|------|------|------|---------|
| `CommandProcessor` | DGpuBoardTLM 内部组件 | 5-state FSM(PM4 解析入口) | **NVIDIA method packet**(4 method_addr ranges,非 Mesa TYPE3,per Phase F-H.3) |
| `Pm4Decoder` | CommandProcessor 内部 | **NVIDIA method packet** 解析 | `parse_method`(inc/method_addr/subchannel/data_count,per `unpackPm4Header`) |
| `TmuDispatchProcessor` | DGpuBoardTLM 内部 | TMU Glue(dep 解耦 + 反压) | 32 slot + dep latch + **反压停 fetch**(per Phase F-D.2 H5,非 LIFO)|
| `SubmitQueue`(🆕) | DGpuBoardTLM 内部 | **WDU 分发网络**(per `docs/research/WDUtoSM/overview.md`)| 单 SM 路由 + per-cluster pending FIFO(32)+ per-core active(4) |
| `CudaCoreAdapter` | DGpuBoardTLM 内部 | **SM 微架构探索器**(timing model,per Phase I.2) | 4 个 TLM 模块集成(`MinimalWarpSchedulerTLM`+`ScoreboardTLM`+`PipelineTLM`+`TensorCoreTLM`);`tick()` 驱动 `sm->exe_once()`;**不**直接调 PTX-EMU 内部接口,全通过 PtxEmuSubmoduleMVP facade 转发 |
| `PtxEmuSubmoduleMVP` | DGpuBoardTLM 内部 | **PTX functional facade**(functional model,per Phase I.1) | 8 个深度集成接口(create_gpu_context/decode_ptxir/submit_kernel_request/sm_exe_once/warp_execute_instruction/warp_get_thread_pc/sm_is_idle/sm_get_cycle_count);**不**关心 cycle/stall/hazard |

**复用组件**:
- `DGpuBar`(v0.4 已实施) — PCIe BAR0 MMIO + VRAM backing
- `MemoryCluster`(已存在) — 多通道 HBM/DDR(VRAM 物理 backing)
- `GpuNoC`(已存在) — GPU 端 mesh interconnect(MVP 可选)
- `MinimalWarpSchedulerTLM` / `ScoreboardTLM` / `PipelineTLM` / `TensorCoreTLM`(`include/tlm/gpu/` 已存在,**CudaCoreAdapter 直接集成**)|

---

## 3. MVP vs v0.5 完整版裁剪(per ADR-SOC-06 D1,per Phase J 修订)

| 保留(MVP 必需) | 裁剪(推迟到 v0.5 完整版) |
|-----------------|------------------------|
| submodule + **深度集成 PTX-EMU 内部 C++ 接口** | 12 SM 模块全部 production 升级 |
| CP + PM4(4 **method_addr ranges** NVIDIA method packet)+ TMU Glue | TMD 6 区精细化 + 18 opcodes 全量 |
| **SubmitQueue 单 SM 路由**(per-cluster pending 32 + per-core active 4)| Work Distribution Crossbar 多 SM(per `US20240356866A1` 动态目的选择)|
| **per-warp functional execute**(PtxEmuSubmoduleMVP facade)| 双路径 byte-identical 全量验证(已删除,per DP4=C)|
| IOCTL 0x27/0x28/0x29/0x01 真实路径(per Phase F-H.3 4 IOCTL)| MSI-X + DMA + 多板卡 |
| ChStreamModuleBase + JSON config | PCIe MMU pipe 精确建模 |
| **4 个已有 TLM 模块** 集成(`ScoreboardTLM`/`PipelineTLM`/`TensorCoreTLM`/`MinimalWarpSchedulerTLM`)| TensorCore 高级特性 + 自定义 WarpScheduler 策略 |
| 编译防火墙(唯一 .cc include PTX-EMU 头)| PREEXIT/ACQBULK 指令语义仿真 |
| functional/timing 严格分离(per gpgpu-sim 分层) | 多 SM + 多 Cluster 跨域分发 |

---

## 4. 端到端数据流(per ADR-X.17 §3)

详见 [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md) §3 端到端数据流。

---

## 5. 接口稳定性(MVP 阶段冻结 per ADR-SOC-06 D8,per Phase J 修订)

### 5.1 冻结接口(S4 完成前禁止变更)

**CommandProcessor & Pm4Decoder(per Phase F-H.3 NVIDIA method packet 修订)**:
- ✅ `CommandProcessor::submit_kernel(...)` API
- ✅ `Pm4Decoder::parse_method(method_header, payload, max_dwords) → Pm4MethodDispatch`(替换原 `parse_type3`)
- ✅ `Pm4MethodDispatch { method_addr, subchannel, data_count, decoded_fields }` 数据结构

**TmuDispatchProcessor & SubmitQueue(per Phase F-D.2 + F-H.5 修订)**:
- ✅ `TmuDispatchProcessor::submit / on_complete / try_chain_dependent`
- ✅ `TmuDispatchProcessor::set_backpressure(true)`(默认 true,反压停 fetch 替代 LIFO 驱逐)
- ✅ `SubmitQueue::enqueue(cta_descriptor) → bool`
- ✅ `SubmitQueue::tick()` + `on_warp_complete(task_id, status)`
- ✅ `CtaDescriptor { task_id, vram_image_addr, grid_xyz, block_xyz, shared_mem_bytes, ... }`

**CudaCoreAdapter(per Phase I.2 重构,timing model 入口)**:
- ✅ `CudaCoreAdapter::on_cta_arrival(cta_desc) → bool`(替代 `dispatch_blackbox`)
- ✅ `CudaCoreAdapter::tick()` — ★ timing 主入口,驱动 `sm->exe_once()`
- ✅ `CudaCoreAdapter::on_warp_complete(task_id, status)`
- ✅ `CudaCoreAdapter::warp_state(warp_id) → WarpState`(cycle_count/exec_mask/blocked_cycles/scheduler_state,**不**含 PC)
- ✅ `CudaCoreAdapter::init(PtxEmuSubmoduleMVP& facade)`(注入 timing 依赖)
- ❌ **删除**(原):`dispatch_blackbox` / `dispatch_whitebox`(白盒永久禁用 per DP4=C)

**PtxEmuSubmoduleMVP(per Phase I.1 重构,PTX functional facade)**:
- ✅ `init(ptx_emu_root, GPUConfig) + shutdown()`(RAII)
- ✅ `create_gpu_context() → std::unique_ptr<ptxsim::GPUContext>`
- ✅ `decode_ptxir(image_bytes, size) → std::vector<ptx_ir::StatementContext>`(校验 PTXIR_VERSION=4)
- ✅ `submit_kernel_request(gpu_ctx, KernelLaunchRequest&&)`
- ✅ `functional_execute_warp(warp, stmt, target_pc)` — ★ 核心入口,**不**增加 cycle
- ✅ `functional_barrier_sync(warp, barrier_id)` + `functional_exit_warp(warp)`
- ✅ 状态读:`read_register<T>` / `read_global_memory<T>` / `read_thread_pc` / `read_active_mask` / `is_warp_finished` / `is_thread_exited`
- ✅ 状态写:`write_register<T>` / `write_global_memory<T>` / `advance_thread_pc`
- ✅ Module getters:`create_scoreboard()` / `create_pipeline_latency_provider()` / `create_tensor_core_timing()`
- ❌ **删除**(原 8 ABI 黑盒):`image_load` / `image_execute` / `image_unload` / `image_kernel_name` / `image_kernel_count` / `image_kernel_name_at` / `image_execute_named` / `module_version`
- ❌ **删除**(原白盒):`stepOneWarpInstruction`(由 `warp_execute_instruction` 替代)

**DGpuBoardTLM(per Phase F-H.2 重构,6 组件)**:
- ✅ `install_kernel_module(image_bytes, size) → vram_addr`
- ✅ `submit_kernel(KernelLaunchRequest) → image_id`
- ✅ `write_reg(offset, value)`(Doorbell ring trigger)
- ✅ `tick()` 串联 4 阶段(cp_.tick() → tmu_.tick() → sq_.tick() → cuda_core_.tick())
- ✅ `set_stream_adapter(...)`(ChStream 端口)

**Doorbell & CompletionRing**:
- ✅ `Doorbell::ring(stream_id, tail)` + strong-ordered write 延迟断言(250-700ns)
- ✅ `CompletionRing::push(task_id, status)` + `set_host_notify(hook)`

### 5.2 可演进接口(S1-S3 期间允许调整)

- 🟡 `CommandProcessor` 5-state FSM 内部状态转换
- 🟡 `Pm4Decoder` method packet sub-fields 解析顺序
- 🟡 `TmuDispatchProcessor` dep chain 推进规则
- 🟡 `SubmitQueue` pending/active slot 容量(`MAX_PENDING_PER_CLUSTER=32` + `MAX_ACTIVE_PER_CORE=4`)
- 🟡 `DGpuBoardTLM::tick()` 4 阶段调度顺序

### 5.3 内部接口(S4 后允许调整)

- 🟡 `PtxEmuSubmoduleMVP` 内部 `ptxsim::GPUContext*` 实例化策略
- 🟡 `CudaCoreAdapter::inject_timing_modules()` 时序(per Phase I.2 §5.1)
- 🟡 `MinimalWarpSchedulerTLM` 调度策略(目前 Round-Robin)

---

## 6. 关键时序特性(MVP 默认,per Phase F-E.2 PCIe strong-order 修订)

| 阶段 | 延迟 | 备注 |
|------|------|------|
| `ioctl(0x27)` → H2D DMA → `install_kernel_module` | 模拟为 1 tick(0 cycle 延迟) | MVP 不仿真 H2D 物理延迟 |
| `write_reg(0x1000+stream_id)` → Doorbell ring | **250-700ns 区间断言**(per [`docs/research/PCIe/PCIe_上的保序write.md`](../../../research/PCIe/PCIe_上的保序write.md) §4 Gen5 250-350ns / 多级 switch 400-600ns / 跨 RC >700ns) | MVP 仅断言区间,**不**仿真 MMU ordering pipe |
| `ioctl(0x01)` pushbuffer 写 → CP FETCH | ~1 tick | CP 从 `mem_read_vram(GPU VA)` 取 entry |
| CP FETCH → DECODE → DISPATCH | ~1 tick(简化) | MVP 不仿真 PM4 解析 cycle 精度 |
| TMU dep latch check + 反压停 fetch | ~1 tick | TMU 容量满时 `BACKPRESSURED` 返回 CP |
| SQ dispatch_to_core(WDU 路由)| ~1 tick | MVP 单 SM 路由,固定返回 0 |
| `CudaCoreAdapter::tick()` 推进 `sm->exe_once()` | 1 cycle | WarpScheduler 选 warp + 3-Step 注入(scoreboard → functional → pipeline) |
| `CudaCoreAdapter::on_warp_complete` → SQ → TMU → CQ::push | ~1 tick | 反向流 |
| `CQ::push + host_notify` | ~50ns 模拟 | signal hook |

**总 cycle budget(MVP 默认)**:`500ns-2us + kernel exec`

---

## 7. 测试策略(per Phase I functional/timing 测试分离)

### 7.1 Functional 单元测试(S1 S2 per Phase I.1 PtxEmuSubmoduleMVP)

| 模块 | 测试 | Catch2 标签 |
|------|------|-------------|
| PtxEmuSubmoduleMVP | `decode_ptxir` magic/version 校验 | `[ptx-emu-facade][decode]` |
| PtxEmuSubmoduleMVP | ADD/SUB/MUL/DIV 寄存器结果 | `[ptx-emu-facade][arith]` |
| PtxEmuSubmoduleMVP | LD/ST 共享/全局内存 | `[ptx-emu-facade][memory]` |
| PtxEmuSubmoduleMVP | SIMT 分支/active mask | `[ptx-emu-facade][branch]` |
| PtxEmuSubmoduleMVP | `bar.sync` 多 warp 同步 | `[ptx-emu-facade][barrier]` |
| PtxEmuSubmoduleMVP | 状态读写 round-trip | `[ptx-emu-facade][state]` |

### 7.2 Timing 单元测试(S1 S2 per Phase I.2 CudaCoreAdapter)

| 模块 | 测试 | Catch2 标签 |
|------|------|-------------|
| CudaCoreAdapter | per-tick cycle 推进 + WarpScheduler 行为 | `[cuda-core][mvp][tick]` |
| CudaCoreAdapter | RAW hazard 检查 + allocate/release 计数 | `[cuda-core][mvp][scoreboard]` |
| CudaCoreAdapter | Pipeline latency 注入 + blocked_cycles 镜像 | `[cuda-core][mvp][pipeline]` |
| CudaCoreAdapter | on_cta_arrival 反压 + resource 管理 | `[cuda-core][mvp][dispatch]` |
| CudaCoreAdapter | WarpState timing 状态镜像(**不**含 PC)| `[cuda-core][mvp][warp-state]` |
| CudaCoreAdapter | IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 注入路径 | `[cuda-core][mvp][injection]` |

### 7.3 集成测试(W3-4 S2)

| 测试 | 内容 | 标签 |
|------|------|------|
| `test_dgpu_board_v1_mvp_from_config.cc` | 6 SECTION: validate_topology / instantiateAll / H2D / launch / host_notify / 负面路径 | `[dgpu-board][mvp][e2e]` |
| `test_pm4_decoder_mvp_integration.cc` | CP + Pm4Decoder 集成(NVIDIA method packet) | `[command-processor][pm4-decoder][integration]` |

### 7.4 E2E 测试(W3-4 S2)

| 测试 | 内容 | 标签 |
|------|------|------|
| cuModuleLoadData E2E | IOCTL 0x27 → DGpuBar.vram_base → image_load | `[P3][E2E][usrlxemu]` |
| cuLaunchKernel E2E(per Phase F-H.3)| IOCTL 0x01 pushbuffer → CP → TMU → SQ → CudaCore → CQ | `[P3][E2E][usrlxemu]` |
| cuStreamSynchronize E2E | fence 等待 → CQ pop | `[P3][E2E][usrlxemu]` |

---

## 8. 风险与缓解(per ADR-SOC-06 §6.3,per Phase J 修订)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit @ `87820951` + 月度 bump PR |
| ~~R2 | PTX-EMU 维护者拒收 `stepOneWarpInstruction` API~~ | — | — | **🗑️ 风险已消除(per DP4=C 决策 + Phase I.1 重构)**:MVP 永久仅深度集成路径,不依赖该 API(已删除) |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构;0x28 永久 -ENOSYS(per Phase F-H.3)|
| R4 | CommandProcessor 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| ~~R5 | TmuDispatchProcessor LIFO 频繁驱逐~~ | — | — | **🗑️ 风险已消除(per Phase F-D.2 H5 修订)**:LIFO → 反压停 fetch,容量满拒绝不驱逐 |
| R6 | 6-10 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ 严格 TDD 5 步 |
| R7 | PtxEmuSubmoduleMVP 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 + CI 拦截(per Phase I.1 §1.1 验证命令) |
| R8 | PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R9 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | MVP 仅"内部一致性验证",不声称真实对齐 |
| **R10** | functional/timing 边界误用(直接调 PTX-EMU 内部 functional 接口)| 中 | 高 | 编译期隔离:本 facade 接口**禁止** `WarpContext::execute_warp_instruction` 等;代码评审保证 |
| **R11** | WarpState 误加 PC 字段 | 低 | 中 | 文档 + 接口表明确分离(per Phase I.2 §3);测试验证 |

---

## 9. 阶段化交付(per [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md),per Phase J 修订)

```
S1 MVP-Cut (W1-2): submodule + PtxEmuSubmoduleMVP(facade)+ CudaCoreAdapter(timing) + 内部链路
S2 Real-Board-Bind (W3-4): DGpuBoard + Doorbell + CQ + JSON + IOCTL stub (4 IOCTL: 0x27/0x28/0x29/0x01)
S3 TMU+CP+SQ 链路接通 (W5-6): CP + NVIDIA PM4 + TMU + SubmitQueue(WDU 分发网络) + CudaCore 深度集成
S4 Production (W7-10): 全量集成 + validate_topology + v0.5.0-MVP tag
```

**注意**(per Phase J):S3 阶段名从原 "S3 Warp-Precision" 改为 "S3 TMU+CP+SQ 链路接通",反映 6 模块真实架构(Warp-Precision 已被 Phase I.1 重新定义,per-warp functional execute 已内嵌于 CudaCoreAdapter::tick())

---

## 10. 配套文档

- [`proposal.md`](../proposal.md) — 实施提案(Why / What Changes / Acceptance Gate / Cross-Repo)
- [`tasks.md`](../tasks.md) — S1-S4 任务清单
- [`specs/`](../specs/) — MVP 切片规格说明
- [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) — 8 项决策锁定(Proposed)
- 7 个模块设计文档(per Phase J 修订) — 见本页 §"模块设计文档"

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 与 ADR-SOC-06 + Phase I/F-H 同步(per Phase J 2026-08-20 对齐)
**下次更新**: W2 S1 完成时(submodule pin + 深度集成接口 PASS)