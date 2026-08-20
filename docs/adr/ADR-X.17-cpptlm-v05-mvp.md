# ADR-X.17: cpptlm-v05-mvp — dGPU Board MVP (CP→TMU→Cuda Core → PTX-EMU warp)

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
| **PCIe 设备语义** | DGpuBar(BAR0 MMIO + BAR1 VRAM)+ Doorbell(SQ tail)+ SQ + CQ(CompletionRing) | ADR-X.15 §4.2 / v0.4 design §3 / ADR-090 §D3.3 |
| **PM4 解析** | 由 CppTLM CP 模块解析(PCIe 设备语义对齐真实硬件) | ADR-X.16 D5 |
| **PM4 风格** | Mesa-style TYPE3 header(`IT` bit 0 + `opcode` bits 2-9 + `count` bits 16-29 + `type` bits 30-31 = `0b11`) | ADR-X.16 §3.2 |
| **TMU Glue** | TmuDispatchProcessor(`submit` / `on_complete` / `try_chain_dependent`)内部组件,~200 LOC | v0.4 design §3.8 |
| **PTX-EMU 集成** | git submodule `external/PTX-EMU`(静态链接,非 dlopen)+ adapter pattern | ADR-X.16 D2/D4 |
| **CPPTLMBRIDGE_VERSION** | = 2 永久冻结,新路径走 C++ 源码契约,不发 C ABI v3 | ADR-X.16 D7 |
| **双路径共存** | 黑盒 `image_execute`(快速)+ 白盒 per-warp step(精确)| ADR-X.16 §3.3 |

### 1.2 现有 PTX-EMU 真实接口(已验证)

`PTX-EMU@87820951`(本地仓位于 `/workspace/project/PTX-EMU` + CppTLM `external/PTX-EMU` submodule,per **DP1=B** 决策)已具备:

| 接口 | 位置 | 状态 |
|------|------|:---:|
| `WarpContext::execute_warp_instruction(StatementContext&, int target_pc=-1)` | `include/ptxsim/warp_context.h:62` | ✅ 已有 |
| `SMContext::exe_once() → EXE_STATE` | `include/ptxsim/sm_context.h:59` | ✅ 已有(D1-Full 3 步注入点已就位) |
| `ptxemu_image_load / ptxemu_image_execute` (8 ABI + 3 multi-kernel) | `include/cudart/cpptlm_module.h:18-52` | ✅ 已 ship |
| `cpptlm_attach_bridge / cpptlm_set_driver` (跨仓 ABI) | `include/cudart/cpptlm_bridge.h:162/211` | ✅ 已 ship |
| `stepOneWarpInstruction(uint32_t warp_id, ...)` (白盒 API) | 提案中(ADR-X.16 计划) | 🟡 需 PTX-EMU 端新增 |

**关键观察**: PTX-EMU **per-warp instruction 骨架已存在**(`execute_warp_instruction`),MVP 可直接通过该入口调用,无需等 `stepOneWarpInstruction` 新 API。

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
  - ~~HSK-7~~(per DP4=C 决策)— **不发出**:MVP **永久仅黑盒路径**,不需要 `stepOneWarpInstruction` API;
    per-warp 精度推到 v0.5 完整版(per ADR-X.16 12 周 P0'-P4')

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
| **S2 Real-Board-Bind** | W3-4 | `DGpuBoardTLM` + `DGpuBar` + `Doorbell` + `SQ/CQ` + JSON config + **接 UsrLinuxEmu IOCTL 0x27/0x28 stub** | Catch2: E2E `cuModuleLoadData + cuLaunchKernel` 跑通 |
| **S3 Warp-Precision** | W5-6 | `CommandProcessor` + `Pm4Decoder`(Mesa TYPE3)+ `TmuDispatchProcessor` + **CudaCore 调 PTX-EMU `WarpContext::execute_warp_instruction`** | Catch2: per-warp 精度测试 + cycle 计数验证 |
| **S4 Production** | W7-10 | ScoreboardTLM + PipelineTLM 升级 + 完整 JSON E2E + `validate_topology` + v0.5.0-MVP tag | 全部 ≥880 测试 PASS |

**MVP 切片边界**(关键裁剪):

| 保留(v0.5 必需) | 裁剪(推迟到 v0.5 完整版) |
|-----------------|------------------------|
| PTX-EMU submodule 静态链接 | 12 SM 模块全部 production 升级(只升 2 个) |
| PtxEmuSubmoduleMVP adapter(唯一 include PTX-EMU 头)| TMD 字段 6 区精细化 |
| 黑盒 `image_execute` + 白盒 `execute_warp_instruction` 双路径 | 双路径 byte-identical 全量验证(只做 1 个 microbenchmark) |
| CommandProcessor + Pm4Decoder(Mesa TYPE3 + 4 MVP opcodes) | 18 opcodes 全量支持(只 4 个:DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM) |
| TmuDispatchProcessor 简化版(dep chain + LIFO) | Scheduler Cache LRU + 256 slot 完整版 |
| DGpuBar + Doorbell + SQ + CQ 最小完整集 | PCIe strong-order MMU pipe 精确建模(仅延迟区间断言 250-700ns) |
| UsrLinuxEmu IOCTL 0x27/0x28 真实路径接入 | MSI-X 中断 + DMA channel + 多板卡 |
| ChStreamModuleBase + JSON config + validate_topology | PREEXIT/ACQBULK 指令仿真(由 PTX-EMU 自含) |

### D2. PTX-EMU warp 调用 = **单路径(MVP 仅黑盒,per DP4=C)**

✅ **MVP 永久仅黑盒路径**(`image_execute`),白盒路径代码占位但**永久不启用**(per **DP4=C 决策** 2026-08-20)— 不需要 PTX-EMU 新增 `stepOneWarpInstruction` API:

**黑盒 MVP 路径**(S1 唯一启用路径):
```
CudaCoreAdapter::dispatch_blackbox()
  → PtxEmuSubmoduleMVP::image_execute(handle, grid, block, shared_mem, args, argc)
    → PTX-EMU libptxemu_device.so::ptxemu_image_execute
      → GPUContext → SMContext::exe_once() × N cycle
        → (PTX-EMU 内部)WarpContext::execute_warp_instruction × M times
```

**白盒路径**(代码占位,**永久不启用** per DP4=C):
```
// 保留 CudaCoreAdapter::dispatch_whitebox() + PtxEmuSubmoduleMVP::stepOneWarpInstruction()
// 代码框架,但 MVP 范围内 enable_whitebox=false 强制不调用
// per-warp 精度推到 v0.5 完整版(per ADR-X.16 12 周 P0'-P4')
```

**MVP 选择理由**(per DP4=C 决策):
- 降低跨仓协作复杂度:不需要 HSK-7 公告,不需要 PTX-EMU 端新增 API
- 缩短时间线:S3 不再需要等待 PTX-EMU 接受 API(节省 2 周 + 减少跨仓风险)
- per-warp 精度延迟到 v0.5 完整版:不损失最终能力,仅调整交付时机
- per ADR-X.16 D8 验证基础:"PTX-EMU 自家参考 + 双路径内部一致性"白盒路径在 v0.5 完整版统一实现

**接口保留**(per D8.1 冻结接口):
- ✅ `CudaCoreAdapter::dispatch_whitebox(warp_count, max_cycles)` — 接口保留,代码占位
- ✅ `PtxEmuSubmoduleMVP::stepOneWarpInstruction(warp_id, &pc, &status, &cycle_count)` — 接口保留,代码占位
- 🟡 MVP 范围内 `enable_whitebox=false` 强制,JSON params `enable_whitebox_path=false` 锁定默认

### D3. PTX-EMU 集成 = git submodule + adapter 编译防火墙

✅ **沿用 ADR-X.16 D2/D4**:

- `external/PTX-EMU`(git submodule, pin commit hash `87820951`,per **DP1=B** 决策)
- `src/tlm/gpu/ptx_emu_submodule_mvp.cc` = **唯一** include PTX-EMU 头(编译防火墙)
- 其他 CppTLM `.cc/.hh` 仅前向声明 `namespace ptxsim { class SMContext; class WarpContext; }`
- 验证命令:`git grep "include.*ptxsim" -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"` 应仅命中 `ptx_emu_submodule_mvp.cc`

### D4. UsrLinuxEmu 接入 = IOCTL 0x27/0x28 真实路径

✅ **MVP 接入 UsrLinuxEmu `cuModuleLoadData + cuLaunchKernel` 真实 IOCTL 路径**:

- **MVP 阶段**: CppTLM 提供 **IOCTL stub server**(模拟 UsrLinuxEmu `GpgpuDevice::ioctl`),端到端跑通 `ioctl(0x27) → H2D DMA → DGpuBar.vram_base` 与 `ioctl(0x28) → DGpuBar.write_reg(0x1000+stream_id) → Doorbell ring`
- **不依赖 UsrLinuxEmu 仓编译**: stub 模式由 CppTLM 独立提供,UsrLinuxEmu 真实集成由 HSK-8 协议公告(后续)
- **测试覆盖**: `test/test_usrlxemu_ioctl_stub.cc` 覆盖 0x27/0x28/0x29 三个 IOCTL 端到端

### D5. 模块架构 = CP→TMU→Cuda Core 链路(新增 5 模块)

✅ **MVP 引入 5 个新模块**(均派生自 `ChStreamModuleBase` 或内部组件):

| 模块 | 位置 | 角色 | MVP 简化 |
|------|------|------|---------|
| `CommandProcessor` | `include/tlm/gpu/command_processor_mvp.hh` | 5-state FSM(PM4 解析入口)| IDLE→FETCH→DECODE→DISPATCH→COMPLETE |
| `Pm4Decoder` | `include/tlm/gpu/pm4_decoder_mvp.hh` | Mesa-style TYPE3 解析 | 4 opcodes(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM) |
| `TmuDispatchProcessor` | `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` | TMU glue(SQ ↔ CudaCore) | 简化版:dep chain + LIFO + 32 slot |
| `CudaCoreAdapter` | `include/tlm/gpu/cuda_core_adapter_mvp.hh` | **新概念** — 包装 Cuda Core(SM),调 PTX-EMU warp | 双路径 dispatch(blackbox + whitebox) |
| `PtxEmuSubmoduleMVP` | `include/tlm/gpu/ptx_emu_submodule_mvp.hh` | PTX-EMU adapter(编译防火墙) | 黑盒 `image_execute` + 可选白盒 `stepOneWarpInstruction` |

**保留沿用**: `DGpuBar`(已 v0.4 实施)+ `DGpuBoardTLM`(新 ChStreamModuleBase 包装)+ `Doorbell`/`SubmissionQueue`/`CompletionRing`(新,per v0.4 spec §3.2-3.4)。

### D6. JSON config 驱动 + validate_topology 集成

✅ **MVP 阶段 2 引入 JSON 端到端配置**:

- 新建 `configs/dgpu_board_v1_mvp.json`(per `test_dgpu_board_from_config.cc` 模式,见 ADR-X.15 §4.4 Gate #9)
- Catch2 测试 `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION: validate_topology / instantiateAll / H2D / launch / host_notify / 负面路径)
- 纳入 `validate_topology` CMake target(`scripts/test/docs_sync_check.sh`)
- **JSON params 完整列表**(9 项,默认,见 `dgpu-board.md` §7):`ptx_emu_root` / `vram_size_mb` / `max_streams` / `sq_depth` / `tmu_max_active_tasks` / `tmu_lifo_evict` / `enable_whitebox_path` / `enable_dgpu_bar_mmio` / `usrlxemu_ioctl_stub_mode`

### D7. CPPTLMBRIDGE_VERSION = 2 永久冻结(沿用 ADR-X.16 D7)

✅ **保持冻结**: MVP 不消费 C ABI v3,所有 PTX-EMU 集成走 C++ 源码契约。`include/cudart/abi_guards.h` 17 条 static_assert 保持(per HSK-6 P0-1)。

### D8. 接口稳定性

#### 8.1 MVP 冻结接口(S4 完成前禁止变更)

- ✅ `CommandProcessor::submit_kernel(...)` API
- ✅ `Pm4Decoder::parse_type3(header, payload, max_dwords) → Pm4Packet`
- ✅ `TmuDispatchProcessor::submit / on_complete / try_chain_dependent`
- ✅ `CudaCoreAdapter::dispatch_blackbox(DispatchParams)` + `dispatch_whitebox(warp_count, max_cycles)`
- ✅ `PtxEmuSubmoduleMVP::image_load / image_execute`(8 ABI 透传)
- ✅ `PtxEmuSubmoduleMVP::stepOneWarpInstruction`(白盒可选)
- ✅ `DGpuBoardTLM::install_kernel_module(vram_addr, size, ...)` + `submit_kernel(req)` + `write_reg(offset, value)`
- ✅ `Doorbell::ring(stream_id, tail)` + strong-ordered write 延迟断言(250-700ns 区间)
- ✅ `CompletionRing::push(image_id, status)` + `set_host_notify(hook)`

#### 8.2 MVP 可演进接口(S1-S3 期间允许调整)

- 🟡 `CommandProcessor` 5-state FSM 内部状态转换细节
- 🟡 `Pm4Decoder` 字段解析顺序与子模块拆分
- � `TmuDispatchProcessor` LIFO 驱逐策略与 dep chain 推进规则
- � `DGpuBoardTLM::tick()` 调度顺序

#### 8.3 内部接口(S4 后允许调整)

- 🟡 `PtxEmuSubmoduleMVP` 内部 `ptxsim::SMContext*` 实例化细节
- 🟡 `CudaCoreAdapter` 与 PTX-EMU::WarpScheduler 注入协议

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
│    → [v0.5/MVP 重写] 直调 CppTLM CudaCoreAdapter(UMD shim)         │
│    → 构造 KernelLaunchRequest(已解码 fields)                        │
│    → 调用 DGpuBoardTLM::submit_kernel(req)                          │
│                                                                     │
│  cuStreamSynchronize(stream) → FenceRegistry.wait → return CUDA_SUCCESS │
└────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (PCIe TLP / MMIO direct)
┌─────────────────────────────────────────────────────────────────────┐
│                    CppTLM DGpuBoardTLM v0.5-MVP                       │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────� │
│  │ PCIe Substrate (DGpuBar + Doorbell)                            │ │
│  │ BAR0 MMIO 0x0000-0x0FFF: device regs                           │ │
│  │ BAR0 MMIO 0x1000-0x1FFF: doorbell ring space(per subchannel)  │ │
│  │ BAR1: VRAM backing(H2D DMA target,256MB)                      │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐ │
│  │ CommandProcessor (CP, 5-state FSM)                            │ │
│  │ ├─ IDLE: 等 doorbell wake                                      │ │
│  │ ├─ FETCH: 读 gpu_gpfifo_entry.payload[0](PM4 header)         │ │
│  │ ├─ DECODE: Pm4Decoder(Mesa-style TYPE3)                       │ │
│  │ │        - 4 MVP opcodes: DISPATCH_DIRECT(0x15)              │ │
│  │ │                       EVENT_WRITE(0x46)                     │ │
│  │ │                       RELEASE_MEM(0x49)                     │ │
│  │ │                       ACQUIRE_MEM(0x58)                     │ │
│  │ ├─ DISPATCH(op=0x15): TmuDispatchProcessor::submit(record)    │ │
│  │ └─ COMPLETE: advance to next entry                              │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐ │
│  │ TmuDispatchProcessor (TMU Glue, 32 slot 简化版)                │ │
│  │ ├─ submit: hash map insert(table_slot_id)                       │ │
│  │ ├─ pre_dispatch: dep latch 匹配 + pre_exit_policy check       │ │
│  │ ├─ dispatch: CudaCoreAdapter::issueTask(TaskEntry)            │ │
│  │ └─ on_complete: map evict + CompletionRing::push               │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐ │
│  │ CudaCoreAdapter (per-warp step 入口,新概念)                   │ │
│  │ ├─ 黑盒路径(默认):                                              │ │
│  │ │    PtxEmuSubmoduleMVP::image_execute(handle, grid, block)    │ │
│  │ │       → PTX-EMU GPUContext → SMContext::exe_once() × N       │ │
│  │ │         → (PTX-EMU 内部)WarpContext::execute_warp_instruction│ │
│  │ └─ 白盒路径(S3 可选):                                          │ │
│  │      循环 PtxEmuSubmoduleMVP::stepOneWarpInstruction          │ │
│  │         → 返回 PC + cycle_count + status                        │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                              │                                        │
│  ┌───────────────────────────▼──────────────────────────────────┐ │
│  │ SubmissionQueue (per-stream FIFO) + CompletionRing            │ │
│  │ ├─ SQ::tick() → CudaCoreAdapter::dispatch_blackbox → CQ::push │ │
│  │ └─ host_notify_() → fence_signal → cuStreamSynchronize 返回  │ │
│  └──────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 4. MVP 切片与 ADR-X.16 完整版的关系

```
                    ADR-X.17 (本 ADR, MVP)
                    ┌─────────────────────────┐
                    │ S1-MVP-Cut (2 周)        │
                    │ S2-Real-Board-Bind (2 周)│
                    │ S3-Warp-Precision (2 周) │
                    │ S4-Production (4 周)     │
                    └─────────────┬───────────┘
                                  │ 验证通过 + user sign-off
                                  ▼
                    ADR-X.16 (完整版, 12 周)
                    ┌─────────────────────────┐
                    │ P0'-W1: submodule + ADR  │
                    │ P1'-W2-4: CP + Decoder   │
                    │ P2'-W5-7: ComputeUnit v2 │
                    │ P3'-W8-10: dual-path 验证│
                    │ P4'-W11-12: v0.5.0 tag   │
                    └─────────────────────────┘
```

**MVP 切片 = 完整版的最小可运行子集**:
- S1 = P0' + P1'-mini(CommandProcessor + Pm4Decoder 骨架 + PtxEmuSubmoduleMVP)
- S2 = P2'-mini(DGpuBoardTLM + Doorbell + SQ/CQ + JSON config + UsrLinuxEmu IOCTL stub)
- S3 = P1'-rest(TmuDispatchProcessor + CudaCoreAdapter + 白盒 warp 调用)
- S4 = P3'-mini(ScoreboardTLM/PipelineTLM 升级 + validate_topology + v0.5.0-MVP tag)

**MVP 不替代**: P2' ComputeUnit v2 完整升级(12 SM production 路径)/ P3' dual-path byte-identical 全量验证 / P4' v0.5.0 完整版 tag。

---

## 5. Acceptance Gates (MVP)

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **G-MVP-1** S1 submodule + 内部链路跑通 | CppTLM | ⏳ W1-2 | `ctest -R "test_ptx_emu_submodule_mvp\|test_cuda_core_adapter_mvp" --output-on-failure` PASS |
| **G-MVP-2** S2 DGpuBoard + Doorbell + SQ/CQ + JSON | CppTLM | ⏳ W3-4 | `ctest -R "test_dgpu_board_v1_mvp_from_config" --output-on-failure` 6 SECTION PASS |
| **G-MVP-3** S3 CP + PM4 + TMU + warp 调用 | CppTLM | ⏳ W5-6 | `ctest -R "test_command_processor_mvp\|test_pm4_decoder_mvp\|test_tmu_dispatch_processor_mvp" --output-on-failure` + per-warp cycle PASS |
| **G-MVP-4** S4 Production + validate_topology | CppTLM | ⏳ W7-10 | `cmake --build build --target validate_topology` + 全部 ≥880 测试 PASS |
| **G-MVP-5** UsrLinuxEmu IOCTL 0x27/0x28 真实路径 | CppTLM | ⏳ W4 | `ctest -R "test_usrlxemu_ioctl_stub" --output-on-failure` 3 IOCTL PASS |
| **G-MVP-6** 编译防火墙验证 | CppTLM | ⏳ W2 | `git grep "include.*ptxsim"` 仅命中 `ptx_emu_submodule_mvp.cc` |
| **G-MVP-7** v0.5.0-MVP tag | CppTLM | ⏳ W10 | `git tag -a v0.5.0-MVP -m "..."` |
| **G-MVP-8** HSK-7/8 公告发出(可选,若需 PTX-EMU 新 API) | CppTLM + PTX-EMU | ⏳ W1 | `git log --oneline \| grep hsk-7` |

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
- **白盒路径可选**: 若 PTX-EMU 维护者拒绝 `stepOneWarpInstruction` API,MVP 仅黑盒,per-warp 精度推迟
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

---

**维护**: CppTLM Team (Sisyphus)
**下次 review**: Phase D 完成(submodule add 落地后)→ 升 ✅ Accepted
**Status Update 触发**: ~~PTX-EMU `stepOneWarpInstruction` API 拒收~~(DP4=C 消除);UsrLinuxEmu IOCTL 0x28 真实接口与 stub 偏差 >15%;MVP 6 周节点延迟;submodule commit hash 漂移
