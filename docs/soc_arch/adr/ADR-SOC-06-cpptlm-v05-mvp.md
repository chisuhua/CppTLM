# ADR-SOC-06: cpptlm-v05-mvp — dGPU Board MVP (CP→TMU→Cuda Core → PTX-EMU warp)

> **状态**: 📋 Proposed — 2026-08-19 (Phase A/C 修复后,等待 Phase D 升 ✅ Accepted) · **日期**: 2026-08-19 / 修订 2026-08-20 · **Owner**: CppTLM Team (Sisyphus)
> **取代**: 部分撤销 [`ADR-X.16-cpptlm-v05-redo.md`](../docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md) 的 12 周完整时间线(保留其 8 项决策),抽取 **MVP 切片**作为先导
> **撤销**: 无 — 不撤销任何已有 ADR(基于 ADR-X.15 / X.16 / NV-02 / d1-p1-pipeline-scoreboard 的设计沉淀)
> **关联 OpenSpec**:
> - MVP 提案: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> - 完整版: [`openspec/changes/archive/2026-08-19-cpptlm-v05-redo/`](../../../openspec/changes/archive/2026-08-19-cpptlm-v05-redo/) (12 周全特性,本 ADR 切片不替代)
> **关联模块设计**: [`docs/soc_arch/modules/dgpu-board.md`](../../soc_arch/modules/dgpu-board.md) + [`command-processor.md`](../../soc_arch/modules/command-processor.md) + [`pm4-decoder.md`](../../soc_arch/modules/pm4-decoder.md) + [`tmu-dispatch-processor.md`](../../soc_arch/modules/tmu-dispatch-processor.md) + [`cuda-core-adapter.md`](../../soc_arch/modules/cuda-core-adapter.md) + [`ptx-emu-submodule-mvp.md`](../../soc_arch/modules/ptx-emu-submodule-mvp.md)
> **关联路线图**: [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../soc_arch/roadmap/roadmap-mvp-to-v05.md) — 4 阶段 6-10 周
> **调研基础**: [`docs-archived/superpowers/specs/2026-08-19-dgpu-v4-design.md`](../../superpowers/specs/2026-08-19-dgpu-v4-design.md) §4 (TMU Catalog v3)

---

## 1. Context (背景)

### 1.1 三方文档沉淀的共识

[`ADR-X.15-cpptlm-v3-dgpu-extract.md`](../docs-archived/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md)(v0.4,v3.0-extract)+ [`ADR-X.16-cpptlm-v05-redo.md`](../docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md)(v0.5 redo)+ [`openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`](../../../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/)(D1-Full)+ [`docs-archived/superpowers/specs/2026-08-19-dgpu-v4-design.md`](../../superpowers/specs/2026-08-19-dgpu-v4-design.md)(v0.4 设计)+ **UsrLinuxEmu [`ADR-088-dgpu-complete-simulation.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted 2026-08-16 + UsrLinuxEmu [`ADR-090 v2-ptxir-via-h2d-dma-v2.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) ✅ Accepted 2026-08-18** 沉淀出以下不冲突共识(per Phase A 修复 NR1):

| 维度 | 沉淀共识 | 来源 |
|------|---------|------|
| **角色** | CppTLM = 被驱动的 dGPU 板卡(PCIe device semantics),非桥接层 | ADR-X.15 D1 / ADR-X.16 反转决策 / ADR-088 §C2 |
| **PCIe 设备语义** | DGpuBar(BAR0 MMIO + BAR1 VRAM)+ Doorbell(SQ tail)+ SubmitQueue(WDU 分发)+ CQ(CompletionRing) | ADR-X.15 §4.2 / v0.4 design §3 / ADR-090 §D3.3 / Phase F-H.5 |
| **PM4 解析** | 由 CppTLM CP 模块解析(PCIe 设备语义对齐真实硬件) | ADR-X.16 D5 |
| **PM4 风格** | **NVIDIA method packet**(`inc` bit 0 + `method_addr` bits 1-15 + `subchannel` bits 16-19 + `data_count` bits 20-23,per `unpackPm4Header`)—— 替代原 Mesa-style TYPE3(per Phase F-H.3/C.1) | UsrLinuxEmu `gpfifo_translator.h:60-73` / Phase F-H.3 |
| **TMU Glue** | TmuDispatchProcessor(`submit` / `on_complete` / `try_chain_dependent`)内部组件,32 slot + **反压停 fetch**(非 LIFO) | v0.4 design §3.8 / Phase F-D.2 H5 |
| **PTX-EMU 集成** | git submodule `external/PTX-EMU`(静态链接,非 dlopen)+ **深度集成内部 C++ 接口**(编译防火墙,非 8 ABI 黑盒) | ADR-X.16 D2/D4 / Phase F-H.1 + I.1 |
| **CPPTLMBRIDGE_VERSION** | = 2 永久冻结,新路径走 C++ 源码契约,不发 C ABI v3 | ADR-X.16 D7 |
| **F/T 分离** | **PTX functional facade**(功能正确性)+ **SM 微架构探索器**(timing),per gpgpu-sim 分层(替代原黑/白盒双路径) | Phase I.1 / I.2 |

### 1.2 现有 PTX-EMU 真实接口(已验证)

`PTX-EMU@87820951`(本地仓位于 `/workspace/project/PTX-EMU` + CppTLM `external/PTX-EMU` submodule,per **DP1=B** 决策)已具备:

| 接口 | 位置 | 状态 |
|------|------|:---:|
| `WarpContext::execute_warp_instruction(StatementContext&, int target_pc=-1)` | `include/ptxsim/warp_context.h:62` | ✅ 已有(MVP 深度集成核心入口) |
| `SMContext::exe_once() → EXE_STATE` | `include/ptxsim/sm_context.h:59` | ✅ 已有(D1-Full 3 步注入点已就位) |
| `SMContext::set_scoreboard / set_pipeline_latency_provider / set_tensor_core_timing` | `include/ptxsim/sm_context.h:87-95` | ✅ 已有(timing 模块注入点) |
| `GPUContext::submit_kernel_request / clear_requests / get_sm` | `include/ptxsim/gpu_context.h:109/144/124` | ✅ 已有(深度集成可用) |
| `PtxirReader::read()` + `PtxirHeader` + `PTXIR_VERSION=4` | `include/ptx_ir/ptxir_reader.h:19` + `ptxir_format.h` | ✅ 已有(PTX IR 解码) |
| `ptxemu_image_load / ptxemu_image_execute` (8 ABI + 3 multi-kernel) | `include/cudart/cpptlm_module.h:18-52` | ✅ 已 ship(**MVP 不使用**,per Phase F-H.1) |
| ~~`stepOneWarpInstruction(uint32_t warp_id, ...)`~~ (白盒 API) | 不存在(Phase H/I 确认全仓 0 命中) | 🔴 **已删除**(per DP4=C,MVP 深度集成路径替代) |

**关键观察**(per Phase F-H.1/I.1):PTX-EMU **per-warp instruction 骨架已存在**(`execute_warp_instruction`),MVP 通过 **内部 C++ 接口**直接驱动,不调 ABI 黑盒 `ptxemu_image_execute`,也不需 `stepOneWarpInstruction` 新 API。

### 1.3 UsrLinuxEmu 板卡驱动接入方式(已验证)

`UsrLinuxEmu@37a91b6`([`docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/37a91b6/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) ✅ Accepted 2026-08-18)dGPU 板卡驱动链路:

| 调用链 | IOCTL / 接口 | Mode B(v3.0+/v0.5)路径 |
|--------|--------------|------------------------|
| `cuModuleLoadData` | `ioctl(fd, GPU_IOCTL_LOAD_KERNEL_MODULE, 0x27)` | HAL #66 → H2D DMA → `DGpuBar.vram_base()` 写入 image bytes |
| `cuLaunchKernel` | `ioctl(fd, GPU_IOCTL_LAUNCH_KERNEL_MODULE, 0x28)` | **handler 永久返回 -ENOSYS**(per UsrLinuxEmu ADR-090 §D2.2 编号永久保留规则)→ driver 改走 CppTLM `DGpuBoardTLM::submit_kernel` → `write_reg(0x1000+stream_id, tail)` → **Doorbell ring** → SQ consumer tick |
| `cuModuleUnload` | `ioctl(fd, GPU_IOCTL_UNLOAD_KERNEL_MODULE, 0x29)` | handler 走 `FREE_BO` 路径 → CppTLM `DGpuBoardTLM::uninstall_kernel_module(vram_addr)` |

**关键观察**(per Phase A 修复 NR5 + Phase C DP4 决策):
- UsrLinuxEmu `0x28` LAUNCH IOCTL handler **永久锁定返回 `-ENOSYS`**(per UsrLinuxEmu ADR-090 §D2.2 + ADR-023 §D4 append-only 治理)
- Mode B 必须由 CppTLM 提供 DGpuBar MMIO 路径替代(per ADR-090 §D3.3 dGPU 板卡最小完整集)— **这就是 MVP 必须做的"接入真实板卡驱动"**
- **HSK 协议编号澄清**(per ADR-090 §D5 + DP4=C 决策):
  - ~~HSK-8~~(原 §1.3 误称)— **废除**(与 ADR-090 §D5 编号冲突)
  - **HSK-6** = UsrLinuxEmu ADR-090 v2 §D5 联发协议(PTX-EMU 发起,CppTLM ack,UsrLinuxEmu 利益相关方)— ✅ Accepted 2026-08-18
  - ~~HSK-7~~(per DP4=C 决策)— **不发出**:MVP 通过 **深度集成**路径直接驱动 `execute_warp_instruction`,不需要 `stepOneWarpInstruction` API 也不需跨仓协调
  - per-warp 精度通过深度集成已达(per Phase I.1),无需等待 v0.5 完整版

### 1.4 MVP 切片动机

`ADR-X.16` 的 12 周 P0'-P4' 时间线过重,无法快速验证:
- submodule 集成可工作性
- CP→TMU→Cuda Core 链路可正确性
- UsrLinuxEmu 真实 IOCTL 接入可执行性

**MVP 切片**(本 ADR)目标:**4 阶段 6-10 周内交付可运行的端到端链路**——UsrLinuxEmu `cuLaunchKernel` IOCTL → CppTLM CP 解析 PM4 → TMU glue → Cuda Core 调 PTX-EMU warp 指令执行。验证通过后,再启动 12 周完整版补足(per-warp precision production 升级 + 12 SM 模块升级)。

---

## 2. Decision (决策)

### D1. MVP 切片 = 4 阶段 6-10 周(Net-new,不替代 12 周完整版)

✅ **MVP-Cut → Real-Board-Bind → Warp-Precision → Production** 4 阶段实施:

| 阶段 | 周 | 关键交付 | 验证 |
|------|----|---------|------|
| **S1 MVP-Cut** | W1-2 | `PtxEmuSubmoduleMVP` + 内部 CP→TMU→CudaCore 链路(无真实驱动) | Catch2: 5 单测 PASS |
| **S2 Real-Board-Bind** | W3-4 | `DGpuBoardTLM` + `DGpuBar` + `Doorbell` + `SQ/CQ` + JSON config + **接 UsrLinuxEmu IOCTL 0x27/0x29 stub + 0x01 pushbuffer** | Catch2: E2E `cuModuleLoadData + PUSHBUFFER_SUBMIT_BATCH` 跑通 |
| **S3 TMU+CP+SQ 链路接通**(原 Warp-Precision,W5-6,per Phase F-B.1 C3 + F-H.8 修订) | W5-6 | `CommandProcessor` + `Pm4Decoder`(NVIDIA method packet per Phase F-H.3)+ `TmuDispatchProcessor`(反压停 fetch)+ **`SubmitQueue`(WDU 分发网络)** + `CudaCoreAdapter`(深度集成 PTX-EMU internal + 4 timing 模块注入) | Catch2: NVIDIA method packet 解码 + TMU dep chain + CP 5-state FSM + SQ 路由 + microarchitecture timing 验证 | |**
| **S4 Production** | W7-10 | ScoreboardTLM + PipelineTLM 升级 + 完整 JSON E2E + `validate_topology` + v0.5.0-MVP tag | 全部 ≥880 测试 PASS |

**MVP 切片边界**(关键裁剪):

| 保留(v0.5 必需) | 裁剪(推迟到 v0.5 完整版) |
|-----------------|------------------------|
| PTX-EMU submodule 静态链接 + **深度集成内部 C++ 接口**(编译防火墙) | 12 SM 模块全部 production 升级 |
| PtxEmuSubmoduleMVP **PTX functional facade**(6 类 functional 测试) | TMD 字段 6 区精细化 |
| CudaCoreAdapter **SM 微架构探索器**(timing,4 个 TLM 模块注入) | 多 SM + Work Distribution Crossbar 多 SM 路由(per `US20240356866A1`) |
| **functional/timing 分离**(per gpgpu-sim 分层,替代原黑/白盒双路径) | 双路径 byte-identical 全量验证(已删除,per DP4=C) |
| CommandProcessor + Pm4Decoder(**NVIDIA method packet** + 4 method_addr ranges) | 18 opcodes 全量支持 |
| TmuDispatchProcessor 简化版(dep chain + **反压停 fetch**,非 LIFO) | Scheduler Cache LRU + 256 slot 完整版 |
| **SubmitQueue**(WDU 分发网络,per-cluster pending 32 + per-core active 4) | Work Distribution Crossbar 逐周期动态目的选择 |
| DGpuBar + Doorbell + SubmitQueue + CQ + 4 个已有 TLM 模块 | PCIe strong-order MMU pipe 精确建模(仅延迟区间断言 250-700ns) |
| UsrLinuxEmu IOCTL 0x27/0x29/0x01 + 0x28 永久 -ENOSYS(per Phase F-H.3) | MSI-X 中断 + DMA channel + 多板卡 |
| ChStreamModuleBase + JSON config + validate_topology | PREEXIT/ACQBULK 指令语义(由 PTX-EMU 自含);device-side 调度动作推 v0.5 完整版 |

### D2. PTX-EMU warp 调用 = **深度集成路径(per Phase F-H.7 架构重定义)**

✅ **MVP 永久唯一深度集成路径**(per Phase F-H.1/H.6 修订,2026-08-20)— **不再使用 ABI 黑盒**:

**深度集成 MVP 路径**(S1 唯一启用路径):
```
CudaCoreAdapter::on_cta_arrival(cta_desc)
  → PtxEmuSubmoduleMVP::decode_ptxir(image_bytes, size) → std::vector<StatementContext>
  → PtxEmuSubmoduleMVP::create_gpu_context() → std::unique_ptr<ptxsim::GPUContext>
  → PtxEmuSubmoduleMVP::submit_kernel_request(gpu_ctx_, KernelLaunchRequest&&)
    → PTX-EMU::ptxsim::GPUContext::submit_kernel_request (内部 C++ 实例方法,**非** ABI 函数)
      → PTX-EMU::ptxsim::SMContext::exe_once() × N cycles (per-tick CudaCoreAdapter 调用)
        → (PTX-EMU 内部)WarpContext::execute_warp_instruction(stmt, target_pc)
        → 镜像 PC: warp_get_thread_pc(warp, 0) → CudaCoreAdapter::WarpState
```

**架构重定义理由**(per Phase F-H.7):
- 用户原话:"**MVP 要用 CppTLM 搭建接近真实 CudaCore 的模型,而指令的执行依赖于 PTX-EMU 项目**" — ABI 黑盒 `ptxemu_image_execute` 内部循环 `exe_once` × N 整 kernel 黑盒执行,无法观测 PC/cycle,与"接近真实 CudaCore"目标矛盾
- **DP4=C 决策**(2026-08-20)**推翻**:原"白盒路径代码占位"措辞**作废**;现 MVP 默认就是真实白盒 warp 级驱动执行
- CppTLM 必须**直接 include** PTX-EMU 内部头文件:`ptxsim/gpu_context.h` / `ptxsim/sm_context.h` / `ptxsim/warp_context.h` / `ptx_ir/ptxir_reader.h` / `ptx_ir/statement_context.h`
- 编译防火墙:`PtxEmuSubmoduleMVP::cc` 是 CppTLM 唯一 include PTX-EMU 头文件的位置(per `ptx-emu-submodule-mvp.md` §2.1)

**接口变更**(per Phase F-H.7 + Phase I.1 重构):
- ❌ 删除:8 ABI 黑盒透传(`ptxemu_image_load/execute/execute_named/unload/kernel_name/kernel_count/kernel_name_at/module_version`)
- ❌ 删除:`CudaCoreAdapter::dispatch_blackbox/whitebox` 双路径
- ❌ 删除:`PtxEmuSubmoduleMVP::stepOneWarpInstruction`(由 `functional_execute_warp` 替代)
- ✅ 新增(PtxEmuSubmoduleMVP **PTX functional facade**,per Phase I.1):
  - Functional Construction:`create_gpu_context` / `decode_ptxir` / `submit_kernel_request` / `get_sm_context` / `get_warp_context`
  - Functional Execute:`functional_execute_warp` / `functional_barrier_sync` / `functional_exit_warp`(**不增加 cycle**)
  - Functional State:`read_register<T>` / `write_register<T>` / `read_global_memory<T>` / `write_global_memory<T>` / `read_thread_pc` / `advance_thread_pc` / `read_active_mask` / `is_warp_finished` / `is_thread_exited`
  - Module Getters(供 CudaCoreAdapter 注入):`create_scoreboard` / `create_pipeline_latency_provider` / `create_tensor_core_timing`
- ✅ 新增(CudaCoreAdapter **SM 微架构探索器**,per Phase I.2):`on_cta_arrival` / `tick()`(驱动 `sm->exe_once()` + WarpState 镜像)/ `on_warp_complete` / `inject_timing_modules`
- ✅ 新增(WarpState timing only):`cycle_count / exec_mask / blocked_cycles / scheduler_state`,**不含 PC**(由 PtxEmuSubmoduleMVP 负责)

### D3. PTX-EMU 集成 = git submodule + adapter 编译防火墙

✅ **沿用 ADR-X.16 D2/D4**:

- `external/PTX-EMU`(git submodule, pin commit hash `87820951`,per **DP1=B** 决策)
- `src/tlm/gpu/ptx_emu_submodule_mvp.cc` = **唯一** include PTX-EMU 头(编译防火墙)
- 其他 CppTLM `.cc/.hh` 仅前向声明 `namespace ptxsim { class SMContext; class WarpContext; }`
- 验证命令:`git grep "include.*ptxsim" -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"` 应仅命中 `ptx_emu_submodule_mvp.cc`

### D4. UsrLinuxEmu 接入 = IOCTL 0x27/0x29 + 0x01 pushbuffer(per Phase F-B.3 H3 修订)

✅ **MVP 接入 UsrLinuxEmu `cuModuleLoadData + cuLaunchKernel` 真实 IOCTL 路径**:

- **MVP 阶段**: CppTLM 提供 **IOCTL stub server**(模拟 UsrLinuxEmu `GpgpuDevice::ioctl`)
  - 0x27 `GPU_IOCTL_LOAD_KERNEL_MODULE` → H2D DMA → `DGpuBar.vram_base()`(handler 真实工作,per UsrLinuxEmu ADR-090 §D2.1)
  - 0x29 `GPU_IOCTL_UNLOAD_KERNEL_MODULE` → handler 走 `FREE_BO` 路径 → `DGpuBoardTLM::uninstall_kernel_module(vram_addr)`
  - **0x28 `GPU_IOCTL_LAUNCH_KERNEL_MODULE` handler 永久锁定返回 `-ENOSYS`**(per UsrLinuxEmu ADR-090 §D2.2 + ADR-023 §D4 append-only)— **不调通**;driver fallback 至 **`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH(0x01)`** + `dispatch opcode`(per UsrLinuxEmu `gpu_ioctl.h:736` 注释)— 真实 launch 数据面
- **不依赖 UsrLinuxEmu 仓编译**: stub 模式由 CppTLM 独立提供,UsrLinuxEmu 真实集成由 HSK-N 协议公告(后续)
- **测试覆盖**(per Phase F-B.3 H3 修订): `test/test_usrlxemu_ioctl_stub.cc` 覆盖 **3 IOCTL 端到端**:
  - 0x27 LOAD → 验证 H2D DMA 落 VRAM
  - **0x28 LAUNCH → 验证 handler 返回 -ENOSYS + driver fallback 路径**(per ADR-090 §D2.2 永久锁定)
  - 0x29 UNLOAD → 验证 BO 释放
  - **新增**:**0x01 PUSHBUFFER_SUBMIT_BATCH → 验证 pushbuffer 端到端**(真实 launch 数据面,previously missed by G-MVP-5)

> **修订注记**(per Oracle ses_fe29aa0d 审查 + Phase F-B.3 H3):
> 原 D4 内部自相矛盾(line 53 说"0x28 永久 -ENOSYS",但 D4 bullet 又说"端到端跑通 ioctl(0x28) → write_reg")。
> 现统一语义:0x28 stub 必返回 -ENOSYS,真实 launch 走 0x01 pushbuffer。
> G-MVP-5 验收补充 0x01 pushbuffer 端到端测试覆盖真实 launch 数据面。

### D5. 模块架构 = CP→TMU→SQ→Cuda Core 链路(per Phase F-H.2 架构重定义:新增 8 模块)

✅ **MVP 引入 6 个新模块**(均派生自 `ChStreamModuleBase` 或内部组件):

| 模块 | 位置 | 角色 | MVP 简化 |
|------|------|------|---------|
| `CommandProcessor` | `include/tlm/gpu/command_processor_mvp.hh` | 5-state FSM(PM4 解析入口)| IDLE→FETCH→DECODE→DISPATCH→COMPLETE |
| `Pm4Decoder` | `include/tlm/gpu/pm4_decoder_mvp.hh` | **NVIDIA method packet 解析**(per Phase F-H.3) | 4 method_addr ranges(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM) |
| `TmuDispatchProcessor` | `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` | TMU glue(dep 解耦 + 反压) | 简化版:dep chain + 反压停 fetch + 32 slot |
| **`SubmitQueue`(🆕 per Phase F-H.5)** | `include/tlm/gpu/submit_queue_mvp.hh` | **WDU 分发网络**(per `docs/research/WDUtoSM/overview.md`)| 单 SM 简化路由 + per-cluster pending FIFO(32 slot)+ per-core active(4 slot) |
| `CudaCoreAdapter` | `include/tlm/gpu/cuda_core_adapter_mvp.hh` | **新概念** — 包装 Cuda Core(SM),**深度集成 PTX-EMU 内部 C++ 接口**(per Phase F-H.1) | 持有 `std::unique_ptr<ptxsim::GPUContext>` + 驱动 PC + 逐指令 `WarpContext::execute_warp_instruction` |
| `PtxEmuSubmoduleMVP` | `include/tlm/gpu/ptx_emu_submodule_mvp.hh` | PTX-EMU 深度集成 adapter(编译防火墙)(per Phase F-H.6) | 8 个内部 C++ 接口(create_gpu_context/decode_ptxir/submit_kernel_request/sm_exe_once/warp_execute_instruction/warp_get_thread_pc/sm_is_idle/sm_get_cycle_count) |

**链路**(per Phase F-H.2 §2.3.1):
```
host pushbuffer → CP.fetch → CP.decode(Pm4MethodDispatch) → TMU.submit
  → SQ.enqueue(cta_desc) → SQ.dispatch_to_core → CudaCore.on_cta_arrival
    → per-tick exe_once + 镜像 WarpState → on_warp_complete
    → SQ.on_warp_complete → TMU.on_complete → CQ.push → host_notify
```

**保留沿用**: `DGpuBar`(已 v0.4 实施)+ `DGpuBoardTLM`(新 ChStreamModuleBase 包装)+ `Doorbell`/`CompletionRing`(新,per v0.4 spec §3.2-3.4)。
**❌ 删除**:原 `SubmissionQueue`(per v0.4 spec §3.2-3.4)被 `SubmitQueue` 替代并定位为分发网络(per Phase F-H.5);原 `LIFO eviction` 被 `反压停 fetch` 替代(per Phase F-D.2 H5)。

### D6. JSON config 驱动 + validate_topology 集成

✅ **MVP 阶段 2 引入 JSON 端到端配置**:

- 新建 `configs/dgpu_board_v1_mvp.json`(per `test_dgpu_board_from_config.cc` 模式,见 ADR-X.15 §4.4 Gate #9)
- Catch2 测试 `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION: validate_topology / instantiateAll / H2D / launch / host_notify / 负面路径)
- 纳入 `validate_topology` CMake target(`scripts/test/docs_sync_check.sh`)
- **JSON params 完整列表**(9 项,默认,见 `dgpu-board.md` §7):`ptx_emu_root` / `vram_size_mb` / `max_streams` / `sq_depth` / `tmu_max_active_tasks` / `tmu_lifo_evict` / `enable_whitebox_path` / `enable_dgpu_bar_mmio` / `usrlxemu_ioctl_stub_mode`

### D7. CPPTLMBRIDGE_VERSION = 2 永久冻结(沿用 ADR-X.16 D7)

✅ **保持冻结**: MVP 不消费 C ABI v3,所有 PTX-EMU 集成走 C++ 源码契约。`include/cudart/abi_guards.h` 17 条 static_assert 保持(per HSK-6 P0-1)。

### D8. 接口稳定性

#### 8.1 MVP 冻结接口(S4 完成前禁止变更,per Phase I.1/I.2 重构)

**CommandProcessor + Pm4Decoder(per Phase F-H.3 NVIDIA method packet)**:
- ✅ `CommandProcessor::submit_kernel(...)` API
- ✅ `Pm4Decoder::parse_method(method_header, payload, max_dwords) → Pm4MethodDispatch`(替代原 `parse_type3`)
- ✅ `Pm4MethodDispatch { method_addr, subchannel_id, data_count, decoded_fields }`
- ✅ 4 method_addr ranges:0x4000-0x40FF DISPATCH_DIRECT / 0x4200-0x42FF EVENT_WRITE / 0x4400-0x44FF RELEASE_MEM / 0x4500-0x45FF ACQUIRE_MEM

**TmuDispatchProcessor + SubmitQueue(per Phase F-D.2 H5 + F-H.5)**:
- ✅ `TmuDispatchProcessor::submit(record) → TmuSubmitResult`(含 `BACKPRESSURED` 反压)/ `on_complete` / `try_chain_dependent`
- ✅ `TmuDispatchProcessor::set_backpressure(true)`
- ✅ `SubmitQueue::enqueue(cta_descriptor) → bool` / `tick()` / `on_warp_complete(task_id, status)`
- ✅ `CtaDescriptor { task_id, vram_image_addr, grid_xyz, block_xyz, shared_mem_bytes, ... }`

**CudaCoreAdapter(per Phase I.2,SM 微架构探索器)**:
- ✅ `CudaCoreAdapter::on_cta_arrival(cta_desc) → bool`(替代 `dispatch_blackbox`)
- ✅ `CudaCoreAdapter::tick()` — 驱动 `sm->exe_once()` + 镜像 WarpState
- ✅ `CudaCoreAdapter::on_warp_complete(task_id, status)`
- ✅ `CudaCoreAdapter::warp_state(warp_id) → WarpState`(cycle_count / exec_mask / blocked_cycles / scheduler_state,**不含 PC**)
- ✅ `CudaCoreAdapter::init(PtxEmuSubmoduleMVP& facade)`(注入 4 个 timing 模块)
- ❌ **删除**:`dispatch_blackbox` / `dispatch_whitebox`(per DP4=C 重构)

**PtxEmuSubmoduleMVP(per Phase I.1,PTX functional facade)**:
- ✅ `PtxEmuSubmoduleMVP::init(ptx_emu_root, GPUConfig) + shutdown()`
- ✅ `create_gpu_context() / decode_ptxir() / submit_kernel_request()`
- ✅ `functional_execute_warp(warp, stmt, target_pc)` — ★ 不增加 cycle
- ✅ `read_register<T> / write_register<T> / read_global_memory<T> / write_global_memory<T>`
- ✅ `read_thread_pc / advance_thread_pc / read_active_mask / is_warp_finished / is_thread_exited`
- ✅ `create_scoreboard() / create_pipeline_latency_provider() / create_tensor_core_timing()`(供 CudaCoreAdapter 注入)
- ❌ **删除**:8 ABI 黑盒(`image_load / image_execute / image_unload / image_kernel_name / image_kernel_count / image_kernel_name_at / image_execute_named / module_version`)
- ❌ **删除**:`stepOneWarpInstruction`(由 `functional_execute_warp` 替代)

**DGpuBoardTLM(per Phase F-H.2,8 组件包装)**:
- ✅ `DGpuBoardTLM::install_kernel_module(vram_addr, size)` + `submit_kernel(req)` + `write_reg(offset, value)`
- ✅ `tick()` 串联 4 阶段(cp_.tick() → tmu_.tick() → sq_.tick() → cuda_core_.tick())

**Doorbell + CompletionRing**:
- ✅ `Doorbell::ring(stream_id, tail)` + strong-ordered write 延迟断言(250-700ns 区间)
- ✅ `CompletionRing::push(task_id, status)` + `set_host_notify(hook)`

#### 8.2 MVP 可演进接口(S1-S3 期间允许调整)

- 🟡 `CommandProcessor` 5-state FSM 内部状态转换细节
- 🟡 `Pm4Decoder` method packet sub-fields 解析顺序与子模块拆分
- 🟡 `TmuDispatchProcessor` dep chain 推进规则(反压事件后重试策略)
- 🟡 `SubmitQueue` pending/active slot 容量(`MAX_PENDING_PER_CLUSTER=32` + `MAX_ACTIVE_PER_CORE=4`)
- 🟡 `DGpuBoardTLM::tick()` 调度顺序

#### 8.3 内部接口(S4 后允许调整)

- 🟡 `PtxEmuSubmoduleMVP` 内部 `ptxsim::GPUContext*` 实例化策略
- 🟡 `CudaCoreAdapter::inject_timing_modules()` 注入时序(per Phase I.2 §5.1)
- 🟡 `MinimalWarpSchedulerTLM` 调度策略(目前 Round-Robin)

---

## 3. 端到端数据流(MVP)

```
┌─────────────────────────────────────────────────────────────────────┐
│              Host (UsrLinuxEmu + TaskRunner)                         │
│                                                                     │
│  cuModuleLoadData(image_bytes)                                      │
│    → ioctl(0x27 LOAD_KERNEL_MODULE) → HAL #66 → H2D DMA            │
│    → 写 image_bytes → DGpuBar.vram_base()                          │
│                                                                     │
│  cuLaunchKernel(grid, block, args, ...)                             │
│    → ioctl(0x01 PUSHBUFFER_SUBMIT_BATCH) → 写 gpfifo_entries[]      │
│      → DGpuBar.vram.pushbuffer_ring                                 │
│    → ioctl(0x28 LAUNCH_KERNEL_MODULE) → 永久 -ENOSYS                │
│      (handler 锁定,per UsrLinuxEmu ADR-090 §D2.2)                   │
│                                                                     │
│    **OR**(per Phase F-E.1 M3 修订,MVP 阶段 UMD shim 简化路径):       │
│    → 直接调用 DGpuBoardTLM::submit_kernel(KernelLaunchRequest)        │
│      (UMD shim 在 driver 侧构造,**已解码 fields**,对应 v0.4.1 host  │
│       解码模式,但 MVP 阶段保留为可选简化)                            │
│                                                                     │
│  cuStreamSynchronize(stream) → FenceRegistry.wait → return CUDA_SUCCESS │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (PCIe TLP / MMIO direct)
┌─────────────────────────────────────────────────────────────────────┐
│                    CppTLM DGpuBoardTLM v0.5-MVP                       │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ PCIe Substrate (DGpuBar + Doorbell)                            │  │
│  │ BAR0 MMIO 0x0000-0x0FFF: device regs                           │  │
│  │ BAR0 MMIO 0x1000-0x1FFF: doorbell ring space(per subchannel)  │  │
│  │ BAR1: VRAM backing(H2D DMA target,256MB)                      │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐  │
│  │ CommandProcessor (CP, 5-state FSM,per Phase F-H.3)            │  │
│  │ ├─ IDLE: 等 doorbell wake                                      │  │
│  │ ├─ FETCH: mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry))     │  │
│  │ │     entry.format==FORMAT_PM4 → payload[0] = method header    │  │
│  │ ├─ DECODE: Pm4Decoder(NVIDIA method packet,per Phase F-C.1)   │  │
│  │ │        - 4 method_addr ranges: 0x4000-0x40FF DISPATCH_DIRECT│  │
│  │ │                               0x4200-0x42FF EVENT_WRITE      │  │
│  │ │                               0x4400-0x44FF RELEASE_MEM      │  │
│  │ │                               0x4500-0x45FF ACQUIRE_MEM      │  │
│  │ │        → Pm4MethodDispatch {method_addr, subchannel, ...}    │  │
│  │ ├─ DISPATCH(0x4000 range): tmu_.submit(Pm4MethodDispatch)     │  │
│  │ └─ COMPLETE: advance to next entry                              │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐  │
│  │ TmuDispatchProcessor (TMU Glue, 32 slot + 反压停 fetch,       │  │
│  │                           per Phase F-D.2 H5)                 │  │
│  │ ├─ submit: hash map insert + 反压检查(BACKPRESSURED→retry)    │  │
│  │ ├─ pre_dispatch: dep latch 匹配 + pre_exit_policy(NONE)       │  │
│  │ ├─ dispatch: submit_queue_.enqueue(cta_descriptor)            │  │
│  │ └─ on_complete: map evict + try_chain_dependent + CQ::push    │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐  │
│  │ SubmitQueue (WDU 分发网络,per Phase F-H.5)                    │  │
│  │ ├─ enqueue(cta_desc) → per-cluster pending FIFO(32 槽)        │  │
│  │ ├─ tick() → dispatch_to_core(cta_desc)                        │  │
│  │ │     select_target_core() → MVP 固定 0(单 SM 路由)           │  │
│  │ └─ cuda_core_[target].on_cta_arrival(cta_desc)                 │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐  │
│  │ CudaCoreAdapter (★ SM 微架构探索器,per Phase I.2)             │  │
│  │ ├─ on_cta_arrival → decode_ptxir + submit_kernel_request      │  │
│  │ ├─ tick() → sm->exe_once()                                    │  │
│  │ │     ├─ Step A: ScoreboardTLM.allocate() (RAW hazard)        │  │
│  │ │     ├─ Step B: PtxEmuSubmoduleMVP.functional_execute_warp() │  │
│  │ │     │      → PTX-EMU WarpContext::execute_warp_instruction  │  │
│  │ │     ├─ Step C: ScoreboardTLM.release()                      │  │
│  │ │     └─ PipelineTLM.get_fractional_cycles() → blocked_cycles │  │
│  │ ├─ WarpState[warp_id] 镜像(cycle/exec_mask/blocked,不含 PC)  │  │
│  │ └─ 完成 → sq_.on_warp_complete → tmu_.on_complete → CQ        │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐  │
│  │ CompletionRing (push + host_notify hook)                      │  │
│  │ └─ host_notify_() → fence_signal → cuStreamSynchronize 返回   │  │
│  └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 4. MVP 切片与 ADR-X.16 完整版的关系

```
                    ADR-SOC-06 (本 ADR, MVP)
                    ┌─────────────────────────┐
                    │ S1-MVP-Cut (2 周)        │
                    │ S2-Real-Board-Bind (2 周)│
                    │ S3-TMU+CP+SQ 链路接通 (2 周)│
                    │ S4-Production (4 周)     │
                    └─────────────┬───────────┘
                                  │ 验证通过 + user sign-off
                                  ▼
                    ADR-X.16 (完整版, 12 周,已归档)
                    ┌─────────────────────────┐
                    │ P0'-W1: submodule + ADR  │
                    │ P1'-W2-4: CP + Decoder   │
                    │ P2'-W5-7: ComputeUnit v2 │
                    │ P3'-W8-10: F/T 分离验证   │
                    │ P4'-W11-12: v0.5.0 tag   │
                    └─────────────────────────┘
```

**MVP 切片 = 完整版的最小可运行子集**(per Phase F-H.2 + Phase I.1/I.2 修正):
- S1 = P0' + P1'-mini(PtxEmuSubmoduleMVP **PTX functional facade** + CudaCoreAdapter **SM 微架构探索器** + 4 timing 模块注入)
- S2 = P2'-mini(DGpuBoardTLM **8 组件** + Doorbell + SubmitQueue + CQ + JSON config + UsrLinuxEmu IOCTL stub)
- S3 = P1'-rest(CommandProcessor + Pm4Decoder NVIDIA method packet + TmuDispatchProcessor 反压停 fetch + SubmitQueue WDU 分发)
- S4 = P3'-mini(全量 baseline + validate_topology + v0.5.0-MVP tag;ScoreboardTLM/PipelineTLM 复用现有,不升级 v05_mvp 版本)

**MVP 不替代**: P2' ComputeUnit v2 完整升级(12 SM production 路径)/ P3' 多 SM Work Distribution Crossbar / P4' v0.5.0 完整版 tag。

---

## 5. Acceptance Gates (MVP)

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **G-MVP-1** S1 submodule + functional/timing 内部链路跑通 | CppTLM | ⏳ W1-2 | `ctest -R "test_ptx_emu_facade\|test_cuda_core_adapter_mvp" --output-on-failure` PASS(6 functional + 6 timing) |
| **G-MVP-2** S2 DGpuBoard + Doorbell + SubmitQueue + CQ + JSON | CppTLM | ⏳ W3-4 | `ctest -R "test_dgpu_board_v1_mvp_from_config" --output-on-failure` 6 SECTION PASS |
| **G-MVP-3** S3 CP + NVIDIA PM4 + TMU + SubmitQueue + CudaCore 深度集成(per Phase F-H.8 修订) | CppTLM | ⏳ W5-6 | `ctest -R "test_command_processor_mvp\|test_pm4_decoder_mvp\|test_tmu_dispatch_processor_mvp\|test_submit_queue_mvp" --output-on-failure` + microarchitecture timing PASS |
| **G-MVP-4** S4 Production + validate_topology | CppTLM | ⏳ W7-10 | `cmake --build build --target validate_topology` + 全部 ≥880 测试 PASS |
| **G-MVP-5** UsrLinuxEmu IOCTL 0x27/0x29 + 0x01 pushbuffer 真实路径(per Phase F-B.3 H3 修订) | CppTLM | ⏳ W4 | `ctest -R "test_usrlxemu_ioctl_stub" --output-on-failure` 4 IOCTL PASS(0x27/0x29/0x28-stub/0x01-pushbuffer) |
| **G-MVP-6** 编译防火墙验证 | CppTLM | ⏳ W2 | `git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/"` 仅命中 `ptx_emu_submodule_mvp.cc` |
| **G-MVP-7** v0.5.0-MVP tag | CppTLM | ⏳ W10 | `git tag -a v0.5.0-MVP -m "..."` |
| ~~**G-MVP-8** HSK-7/8 公告发出~~ | — | **🗑️ 删除**(per DP4=C,无跨仓协调需求) | — |

---

## 6. Consequences (后果)

### 6.1 正面

- **快速验证可行性**: 6-10 周内端到端跑通 UsrLinuxEmu → CppTLM CP → PTX-EMU warp 链路,降低 12 周完整版失败风险
- **降低耦合**: PTX-EMU submodule + adapter 编译防火墙,主代码与 PTX-EMU 解耦,易于升级/替换
- **接入真实板卡**: UsrLinuxEmu IOCTL 0x27/0x28 真实路径,可直接接 UsrLinuxEmu 进程(后续 HSK-8 联调)
- **MVP-切 vs 完整-切并存**: MVP 通过后启动 ADR-X.16 12 周完整版,共享基础设施不浪费

### 6.2 负面

- **PM4 仅 4 opcodes**: 真实工作负载可能触发 14 个 deferred opcodes,需 S5+ 补足
- **TmuDispatchProcessor 32 slot**: 真实多流并发可能 256+ slot 溢出,LIFO eviction 频繁触发
- **白盒路径可选**: ~~若 PTX-EMU 维护者拒绝 `stepOneWarpInstruction` API,MVP 仅黑盒,per-warp 精度推迟~~ **per DP4=C 决策(2026-08-20)**:MVP 永久仅黑盒路径,不依赖该 API,per-warp 精度统一推到 v0.5 完整版
- **UsrLinuxEmu IOCTL stub**: MVP 不直接接 UsrLinuxEmu 编译,真实集成由后续 HSK-8 推动

### 6.3 风险登记

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | ~~PTX-EMU 维护者拒收 `stepOneWarpInstruction` API~~ | — | — | **per DP4=C 决策消除该风险**:MVP 永久仅黑盒路径,不依赖该 API |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构(per UsrLinuxEmu@37a91b6:plugins/gpu_driver/shared/gpu_ioctl.h:723-779),后续真实集成由独立 HSK 跟踪 |
| R4 | CommandProcessor 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 + 真实 PM4 流量回放 |
| R5 | TmuDispatchProcessor LIFO 频繁驱逐 | 中 | 中 | 默认 32 slot + 软栅栏,溢出率 >5% 触发 review |
| R6 | 6-10 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ 严格 TDD 5 步 |
| R7 | PtxEmuSubmoduleMVP 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 + CI 拦截 |
| R8 | PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R9 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | MVP 仅"内部一致性验证",不声称真实对齐 |

---

## 7. References (关联文档)

### 7.1 已有 ADR(本 ADR 引用)

- [`ADR-X.15-cpptlm-v3-dgpu-extract.md`](../docs-archived/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) — dGPU 角色反转 + 11 项删除清单(部分由 ADR-X.16 撤销)
- [`ADR-X.16-cpptlm-v05-redo.md`](../docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md) — 12 周完整版 8 项决策(本 ADR 切片保留其决策)
- [`ADR-NV-01-gpu-soc-architecture-target.md`](./ADR-NV-01-gpu-soc-architecture-target.md) — gpu_soc 独立 SoC 仿真目标
- [`ADR-NV-02-phase8b-d1-strategy.md`](./ADR-NV-02-phase8b-d1-strategy.md) — D1-Full 注入策略(本 ADR 不实施 D1-Full Adapter 注入,仅保留 ScoreboardTLM/PipelineTLM 升级)

### 7.2 已有 OpenSpec change

- [`openspec/changes/archive/2026-08-19-cpptlm-v05-redo/`](../../../openspec/changes/archive/2026-08-19-cpptlm-v05-redo/) — 12 周完整版(本 ADR 切片)
- [`openspec/changes/archive/2026-08-18-cpptlm-v3-dgpu-extract/`](../../../openspec/changes/archive/2026-08-18-cpptlm-v3-dgpu-extract/) — v0.4(本 ADR 沿用)
- [`openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`](../../../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/) — D1-Full 注入(MVP 不实施)

### 7.3 模块设计文档(本 ADR 配套输出)

- [`docs/soc_arch/modules/dgpu-board.md`](../../soc_arch/modules/dgpu-board.md) — DGpuBoardTLM 模块设计
- [`docs/soc_arch/modules/command-processor.md`](../../soc_arch/modules/command-processor.md) — CommandProcessor 5-state FSM
- [`docs/soc_arch/modules/pm4-decoder.md`](../../soc_arch/modules/pm4-decoder.md) — Pm4Decoder Mesa-style TYPE3
- [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../../soc_arch/modules/tmu-dispatch-processor.md) — TmuDispatchProcessor TMU Glue
- [`docs/soc_arch/modules/cuda-core-adapter.md`](../../soc_arch/modules/cuda-core-adapter.md) — CudaCoreAdapter(新概念)per-warp step 入口
- [`docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../../soc_arch/modules/ptx-emu-submodule-mvp.md) — PtxEmuSubmoduleMVP adapter

### 7.4 路线图

- [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../soc_arch/roadmap/roadmap-mvp-to-v05.md) — 4 阶段 6-10 周路线图

### 7.5 跨仓锚点(per ADR-090 §C0.4 新规:`repo@commit:path:line` 锚点)

- **PTX-EMU**(`PTX-EMU@87820951`,per **DP1=B 决策**(2026-08-20)— 本地最新,含 `docs(audit): add PTX-EMU HAL backend cross-repo defect audit (2026-08-13)` commit):
  - [`PTX-EMU@87820951:include/cudart/cpptlm_module.h:12-52`](https://github.com/chisuhua/PTX-EMU/blob/87820951/include/cudart/cpptlm_module.h#L12-L52) — **8 ABI 函数真相源**(`CPPTLM_MODULE_VERSION=2`)
  - [`PTX-EMU@87820951:include/ptxsim/sm_context.h:59`](https://github.com/chisuhua/PTX-EMU/blob/87820951/include/ptxsim/sm_context.h#L59) — `EXE_STATE exe_once()`
  - [`PTX-EMU@87820951:include/ptxsim/warp_context.h:62`](https://github.com/chisuhua/PTX-EMU/blob/87820951/include/ptxsim/warp_context.h#L62) — `execute_warp_instruction(StatementContext&, int)`
  - [`PTX-EMU@87820951:include/cudart/cpptlm_bridge.h:162/211`](https://github.com/chisuhua/PTX-EMU/blob/87820951/include/cudart/cpptlm_bridge.h#L162-L211) — `cpptlm_attach_bridge / cpptlm_set_driver`
  - **🔴 注意**:`stepOneWarpInstruction` API 在 PTX-EMU@87820951 **当前不存在**(2026-08-20 全仓 grep 0 命中)— **per DP4=C 决策,MVP 永久仅黑盒路径,不需要该 API**

- **UsrLinuxEmu**(`UsrLinuxEmu@37a91b6`,本地仓 `/workspace/project/UsrLinuxEmu`):
  - [`UsrLinuxEmu@37a91b6:docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/37a91b6/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) ✅ Accepted 2026-08-18 — dGPU 板卡驱动接入规范 + HSK-6 联发协议
  - [`UsrLinuxEmu@37a91b6:docs/00_adr/adr-088-dgpu-complete-simulation.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/37a91b6/docs/00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted 2026-08-16 — dGPU 完整子系统仿真(23 ABI)
  - [`UsrLinuxEmu@37a91b6:plugins/gpu_driver/shared/gpu_ioctl.h:723-779`](https://github.com/chisuhua/UsrLinuxEmu/blob/37a91b6/plugins/gpu_driver/shared/gpu_ioctl.h#L723-L779) — IOCTL 0x27/0x28/0x29 struct 定义
  - IOCTL 0x27 `GPU_IOCTL_LOAD_KERNEL_MODULE`(H2D DMA 写 CppTLM VRAM,handler = HAL #66)
  - IOCTL 0x28 `GPU_IOCTL_LAUNCH_KERNEL_MODULE`(handler **永久锁定返回 -ENOSYS** per ADR-090 §D2.2 + ADR-023 §D4 append-only)— Mode B 由 CppTLM DGpuBar MMIO 替代
  - IOCTL 0x29 `GPU_IOCTL_UNLOAD_KERNEL_MODULE`(handler 走 `FREE_BO` 路径)

- **TaskRunner**:
  - `tadr-307` 当前 📋 PROPOSED **STALE**(per ADR-090 §C0.3)— 3 个 IGpuDriver 方法与 HSK-1 真相源 + ADR-023 append-only 不兼容
  - `tadr-308` 待创建(per ADR-090 §C0.3)— 基于 annex §B v2 重写版
  - 追踪载体:[`TaskRunner #10`](https://github.com/chisuhua/TaskRunner/issues/10) closed;per **DP4=C** 不需 HSK 联署

---

## 8. 修订记录

| 日期 | 修订 | 修订人 |
|------|------|--------|
| 2026-08-19 | 初版 — 8 项决策 + MVP 4 阶段 6-10 周 | Sisyphus |
| 2026-08-20 | **Phase A 修复**: M2/M4/S1-S6/NR1/NR2/NR5 + **Phase C 决策落地**(DP1=B/DP2=A/DP4=C)— D2 改为单路径黑盒(MVP 永久禁用白盒),R2 风险消除,HSK-7 不发出,submodule pin `PTX-EMU@87820951` | Sisyphus |
| 2026-08-20 | **Phase F 修订**(Oracle ses_fe29aa0d 审查后):**选定路径 3**(GPFIFO 外壳 + PM4 嵌入式,与 UsrLinuxEmu `gpfifo_translator.cpp:103` 先例对齐)— 修复 **3 CRITICAL + 3 HIGH + 3 MEDIUM/LOW**: C1 PM4 格式家族(NVIDIA method packet 替代 Mesa TYPE3)/C2 双 dispatch 路径消歧(SQ shim 唯一)/C3 S3 交付物与 D2 矛盾/H2 5 subchannel→8 subchannel/H3 0x28 stub 语义统一(必返 -ENOSYS)/H5 TMU LIFO eviction 改反压停 fetch;**待 Phase F-F 后续 review 升 Accepted** | Sisyphus |
| 2026-08-20 | **Phase F-G 修订**(Oracle ses_fe1e321d 用户质疑复核后):**纠正 F-C.2 过度修正**——用户指出 §2.3.1 图与 D5/S1 均约定 SQ→TMU→CudaCore 单链路,F-C.2 早期修订把 `tmu_.tick()` 归档出 tick 循环与三处文档矛盾。**正确路径**:只 `注释 cp_.tick()`(CP 在 MVP 无 GPFIFO 输入源),保留 `sq_->tick() + tmu_.tick()`(同一链路,TMU 推进 dep chain 不发起新 dispatch)。**移除未登记的 `enable_mvp_dataflow_only_` flag**。F-D.3 保留正确(ABI 证据:PTX-EMU `cpptlm_module.h` 8 个 ABI 均为 kernel 级,`ptxemu_image_execute` 内部循环 `exe_once()` × N 整 kernel 黑盒;`stepOneWarpInstruction` 全仓 0 命中;用户"warp 级指令 = 白盒"主张与 ABI 边界事实不符,warp 级执行是 PTX-EMU 黑盒内部实现,不是 CppTLM 调用方式) | Sisyphus |
| 2026-08-20 | **Phase F-H 架构重定义**(用户要求"TMU 到 CudaCore 之间有一个分发的网络,MVP 阶段能尽量接近最终形态"+"MVP 要用 CppTLM 搭建接近真实 CudaCore 的模型,指令执行依赖 PTX-EMU"):**D2 反转**——MVP 不再调 ABI 黑盒 `ptxemu_image_execute`,改为**深度集成 PTX-EMU 内部 C++ 接口**(`ptxsim::GPUContext` / `SMContext` / `WarpContext` / `ptx_ir::PtxirReader`),CppTLM 驱动 PC,逐指令 `execute_warp_instruction`。**D5 模块表扩展为 8 模块**,新增 `SubmitQueue`(per `docs/research/WDUtoSM/overview.md` NVIDIA WDU + Work Distribution Crossbar 简化版)填补 CP→TMU→SQ→CudaCore 链路。**D5 链路重画**:`host pushbuffer → CP.fetch → CP.decode(Pm4MethodDispatch) → TMU.submit → SQ.enqueue → SQ.dispatch → CudaCore.on_cta_arrival → per-tick exe_once + WarpState 镜像 → on_warp_complete → SQ.on_warp_complete → TMU.on_complete → CQ.push`。**Dp4=C 含义反转**:原"白盒永久禁用"改为"深度集成路径即白盒 + PTX-EMU 默认 scoreboard/pipeline 透明"。涉及文件:cuda-core-adapter.md, dgpu-board.md, command-processor.md, tmu-dispatch-processor.md, submit-queue.md (新), ptx-emu-submodule-mvp.md, roadmap-mvp-to-v05.md | Sisyphus |

---

**维护**: CppTLM Team (Sisyphus)
**下次 review**: Phase D 完成(submodule add 落地后)→ 升 ✅ Accepted
**Status Update 触发**: ~~PTX-EMU `stepOneWarpInstruction` API 拒收~~(DP4=C 消除);UsrLinuxEmu IOCTL 0x28 真实接口与 stub 偏差 >15%;MVP 6 周节点延迟;submodule commit hash 漂移
