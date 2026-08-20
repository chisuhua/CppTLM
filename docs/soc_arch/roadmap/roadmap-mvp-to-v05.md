# Roadmap: cpptlm-v05-mvp → cpptlm-v05-full (MVP 切片到完整版)

> **类别**: SoC Architecture > Roadmap · **状态**: 🔵 MVP 切片 (per ADR-SOC-06)
> **日期**: 2026-08-19 / 修订 2026-08-21(Phase K:Oracle ses_fe179d02 拆分 + FIX-C1~C3/H5~H9/B.2/B.3 修复)
> **维护者**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D1
> **关联 OpenSpec**(per Phase K Oracle 拆分,2026-08-21):
> - **s1** PTX-EMU 集成: [`openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../../../openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) (W1-2 基础设施,2 模块,12 测试)
> - **s2** DGpuBoard 板卡: [`openspec/changes/2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/`](../../../openspec/changes/2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/) (W3-4,依赖 s1,6 组件 + 4 IOCTL)
> - **s3** Command 数据面: [`openspec/changes/2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/`](../../../openspec/changes/2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/) (W5-10,依赖 s1+s2,NVIDIA PM4 + TMU 反压 + tag)
> - ~~原 change~~: `openspec/changes/2026-08-19-cpptlm-v05-mvp/` → 已归档 `openspec/changes/archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/` (per Phase K 拆分,2026-08-21)
> - 完整版: [`openspec/changes/2026-08-19-cpptlm-v05-redo/`](../../../openspec/changes/2026-08-19-cpptlm-v05-redo/) (12 周 P0'-P4',MVP 验证后启动)
> **关联模块**: [DGpuBoardTLM](../modules/dgpu-board.md) + [SubmitQueue](../modules/submit-queue.md) + [CommandProcessor](../modules/command-processor.md) + [Pm4Decoder](../modules/pm4-decoder.md) + [TmuDispatchProcessor](../modules/tmu-dispatch-processor.md) + [CudaCoreAdapter](../modules/cuda-core-adapter.md) + [PtxEmuSubmoduleMVP](../modules/ptx-emu-submodule-mvp.md)

---

## 1. 路线图总览

**MVP 切片 = 4 阶段 6-10 周**(本路线图)

```
┌─────────────────────────────────────────────────────────────────────────────�
│                   cpptlm-v05 Roadmap (MVP + 完整版)                          │
│                                                                             │
│  ┌─────────────────────┐                                                   │
│  │ S1 MVP-Cut (W1-2)   │  ← submodule + 内部链路跑通                          │
│  │   - submodule add    │                                                   │
│  │   - PtxEmuSubmodule  │                                                   │
│  │   - CudaCoreAdapter  │                                                   │
│  │   (黑盒 MVP 路径)    │                                                   │
│  └──────────┬──────────┘                                                   │
│             │ 验收通过                                                      │
│             ▼                                                                │
│  �─────────────────────┐                                                   │
│  │ S2 Real-Board-Bind  │  ← 接 UsrLinuxEmu IOCTL 0x27/0x29/0x01 + 0x28 永久 -ENOSYS
│  │   - DGpuBoardTLM     │                                                   │
│  │   - DGpuBar/Doorbell │                                                   │
│  │   - SQ/CQ            │                                                   │
│  │   - JSON config      │                                                   │
│  │   - IOCTL stub       │                                                   │
│  └──────────┬──────────┘                                                   │
│             │ 验收通过                                                      │
│             ▼                                                                │
│  ┌─────────────────────┐                                                   │
│  │ S3 TMU+CP+SQ 链路接通  ← CP + NVIDIA PM4 + TMU + SubmitQueue(WDU 分发网络)│
│  │   - CommandProcessor │   + CudaCore 深度集成 PTX-EMU(per Phase F-H)        │
│  │   - Pm4Decoder       │                                                   │
│  │   - TmuDispatch      │                                                   │
│  │   - SubmitQueue(新)  │                                                   │
│  │   - CudaCoreAdapter  │                                                   │
│  │     (深度集成 PTX-EMU)│                                                   │
│  └──────────┬──────────┘                                                   │
│             │ 验收通过                                                      │
│             ▼                                                                │
│  ┌─────────────────────┐                                                   │
│  │ S4 Production       │  ← ScoreboardTLM/PipelineTLM 升级 + v0.5.0-MVP tag  │
│  │   - Scoreboard 升级  │                                                   │
│  │   - Pipeline 升级    │                                                   │
│  │   - validate_topo    │                                                   │
│  │   - v0.5.0-MVP tag   │                                                   │
│  └──────────┬──────────┘                                                   │
│             │ user sign-off + 全量验收                                      │
│             ▼                                                                │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │ [可选] v0.5 完整版(12 周 P0'-P4', per ADR-X.16)                     │ │
│  │   - 12 SM 模块全部 production 升级(目前仅 2 个)                   │ │
│  │   - TMD 字段 6 区精细化                                              │ │
│  │   - 双路径 byte-identical 全量验证                                  │ │
│  │   - 18 opcodes 全量支持                                              │ │
│  │   - Scheduler Cache LRU + 256 slot                                  │ │
│  │   - PCIe strong-order MMU pipe 精确建模                             │ │
│  │   - MSI-X + DMA channel + 多板卡                                    │ │
│  │   - PREEXIT/ACQBULK 指令仿真(若 PTX-EMU 提供 block/PREEXIT 回调) │ │
│  │   - v0.5.0 release tag                                               │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
 │  累计 6-10 周 MVP + 12 周 v0.5 完整版 = 18-22 周(4-5 月)                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 1.5 进度追踪(per Phase K Oracle 拆分,2026-08-21)

> **关键拆分**:原单 change `2026-08-19-cpptlm-v05-mvp` 按依赖关系拆为 **3 个独立 change**,每个 change 独立可交付、独立 archive。

### 1.5.1 Openspec change 状态总览

| Openspec Change | 周次 | 模块数 | 测试数 | 状态 | 依赖 |
|------------------|------|:---:|:---:|:---:|------|
| **[s1-ptxemu-integration](../../../openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/)** | W1-2 | 2 | 12 | ⏳ 待启动 | — |
| **[s2-dgpu-board](../../../openspec/changes/2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/)** | W3-4 | 4(Doorbell/SubmitQueue/CQ/IOCTL stub) | 7+ | ⏳ 待启动 | s1 |
| **[s3-command-pipeline](../../../openspec/changes/2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/)** | W5-10 | 3(CP/Pm4Decoder/TMU) | 4 | ⏳ 待启动 | s1+s2 |
| **小计** | **6-10 周** | **9 模块** | **~23 测试** | | |

### 1.5.2 4 阶段 vs 3 change 映射(per Phase K)

| 阶段 | 周次 | 范围 | 对应 openspec change |
|------|------|------|---------------------|
| **S1 MVP-Cut** | W1-2 | submodule + 内部链路跑通(PtxEmuSubmoduleMVP + CudaCoreAdapter) | **s1** |
| **S2 Real-Board-Bind** | W3-4 | DGpuBoard 6 组件 + 4 IOCTL stub + JSON config | **s2** |
| **S3 TMU+CP+SQ 链路接通** | W5-6 | CP + NVIDIA PM4 + TMU 反压停 fetch + SubmitQueue | **s3 (前段)** |
| **S4 Production** | W7-10 | validate_topology + 全量 baseline ≥880 + v0.5.0-MVP tag | **s3 (后段)** |

### 1.5.3 累计测试增长(预期)

```
v0.4.1 baseline (850)
   + s1: 12 tests (6 functional + 6 timing)        → 862
   + s2: 7+ tests (6 SECTION + 5 SQ + 4 IOCTL + 1 Doorbell)  → ~872
   + s3: 4 tests (decoder + CP 集成 + tmu + S4 baseline)       → ~876+
   + 实际 MVP 完成时:                                     → ≥880
```

### 1.5.4 关键依赖关系

```
s1 独立 ─────────────────┐
                          ↓
s2 依赖 s1 编译         ──┤  依赖链: s1 → s2 → s3
                          ↓
s3 依赖 s1 + s2 集成    ──┘
```

- **s1 可独立 archive**(无 s2/s3 依赖,Phase F-H/I 重构后 functional/timing 分离,facade + timing 可单独验证)
- **s2 编译依赖 s1**(PtxEmuSubmoduleMVP + CudaCoreAdapter 头文件 + .cc 编译),**可独立 archive**(4 IOCTL stub + SubmitQueue 自身可验证)
- **s3 集成依赖 s1+s2**(CP fetch GPU VA → TMU submit → SQ enqueue → CudaCore on_cta_arrival 全链路,需前两阶段实施完成)

### 1.5.5 Phase K 同步内容(2026-08-21 修订)

**已修复**(per Oracle ses_fe179d02 审查):
- ✅ CRITICAL ADR-SOC-06 §3 端到端图 + §8.1 冻结接口表全过时
- ✅ CRITICAL command-processor.md §4.2 fetch_packet 位提取(`h>>1` + `h>>20`,非 `h` + `h>>24`)
- ✅ CRITICAL tmu-dispatch-processor.md §5.4 LIFO 残留(反压停 fetch 替代)
- ✅ HIGH dgpu-board.md 5→8 组件 + SubchannelContext[5]→[8] + tmu_lifo_evict→tmu_enable_backpressure
- ✅ HIGH dgpu-board.md 0x29 IOCTL 语义(handler 走 FREE_BO 真实工作,非 -ENOSYS)
- ✅ HIGH roadmap §4/§5 整节重写(NVIDIA method packet + 深度集成 + 不升级 v05_mvp)
- ✅ HIGH cuda-core-adapter.md 接口矛盾(facade create_*→make_unique,补 read_blocked_cycles)
- ✅ HIGH proposal.md 死引用(ADR-X.17→ADR-SOC-06)+ SubmitQueue 归属(S3→S2)
- ✅ B.2/B.3 接口矛盾裁定(CudaCoreAdapter 持有 4 TLM 模块 + facade 补 read_blocked_cycles)

**Commits**(2026-08-21,6 commit push to origin):
1. `330e94c` fix CRITICAL + HIGH (Phase K)
2. `a17ea24` fix dgpu-board.md HIGH (H5/H6)
3. `b8b0177` fix roadmap.md HIGH (H7)
4. `46dac69` fix cuda-core-adapter + ptx-emu-submodule-mvp (H8 + B.2/B.3)
5. `9b3f472` fix proposal.md HIGH (H9)
6. `aea457c` split openspec into 3 changes (per Oracle)

**当前状态**: ahead of origin/main by 9 commits · docs_sync_check 仅剩 pre-existing `abi_guards.h` 误报(与本路线图无关)

---

## 2. S1 MVP-Cut(W1-2)— 基础链路跑通

### 2.1 目标

- `PtxEmuSubmoduleMVP` 加载 PTX-EMU submodule
- `CudaCoreAdapter` 黑盒 dispatch 路径跑通
- 5 单测 PASS(per ADR-SOC-06 G-MVP-1)

### 2.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `git submodule add external/PTX-EMU` + pin 到 `PTX-EMU@87820951`(per **DP1=B** 决策,2026-08-13 audit commit) | `git submodule status` 显示 PTX-EMU commit hash | ⏳ W1 |
| `CMakeLists.txt` `add_subdirectory(external/PTX-EMU)` + `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` | `cmake --build build` 通过 | ⏳ W1 |
| `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `.cc` | 编译通过 | � W1-2 |
| `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `.cc` | **深度集成 PTX-EMU 内部 C++ 接口**(per Phase F-H.1):`on_cta_arrival/tick/on_warp_complete` | ⏳ W2 |
| `test/test_ptx_emu_submodule_mvp.cc` **深度集成接口单测**(per Phase F-H.6) | ctest PASS | ⏳ W2 |
| `test/test_cuda_core_adapter_mvp.cc` **驱动式 warp 执行**(per Phase F-H.1) | ctest PASS | ⏳ W2 |

### 2.3 关键 Commit

```bash
# W1
git commit -am "chore(submodule): add external/PTX-EMU@87820951"
git commit -am "build(cmake): add_subdirectory(external/PTX-EMU) — submodule static link"
git commit -am "feat(ptx-emu-mvp): PtxEmuSubmoduleMVP adapter with 8 ABI passthrough"

# W2
git commit -am "feat(cuda-core-mvp): CudaCoreAdapter with on_cta_arrival/tick (per Phase I.2 timing model)"
```

### 2.4 风险

- **R1**: PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2)— `PTX_EMU_BUILD_TESTS=OFF` 缓解
- **R2**: 编译防火墙破裂 — `git grep` CI 拦截
- **R3**: ~~PTX-EMU 维护者拒收 API~~ — **per DP4=C 决策消除该风险**:MVP 仅黑盒路径,不依赖新 API

---

## 3. S2 Real-Board-Bind(W3-4)— 接入 UsrLinuxEmu 真实板卡驱动

### 3.1 目标

- `DGpuBoardTLM` 包装 8 内部组件(含 SubmitQueue WDU 分发网络,per Phase F-H.5)
- 接入 UsrLinuxEmu IOCTL 0x27/0x29/0x01 + 0x28 永久 -ENOSYS(4 IOCTL stub,per Phase F-H.3)
- JSON config 驱动 `instantiateAll`
- 6 SECTION E2E 测试 PASS(per ADR-SOC-06 G-MVP-2)

### 3.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `include/tlm/gpu/dgpu_board_mvp.hh` + `.cc`(8 组件包装,含 SubmitQueue + CudaCoreAdapter + PtxEmuSubmoduleMVP) | 编译通过 | ⏳ W3 |
| `include/tlm/gpu/doorbell_mvp.hh` + `.cc`(SQ tail + strong-order) | strong-order 延迟区间 250-700ns 测试 PASS | ⏳ W3 |
| `include/tlm/gpu/completion_ring_mvp.hh` + `.cc`(push + host_notify) | push/host_notify PASS | ⏳ W3 |
| `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc`(0x27/0x28-ENOSYS/0x29/0x01 pushbuffer) | 4 IOCTL PASS | ⏳ W4 |
| `include/chstream_register.hh` 加 `REGISTER_CHSTREAM(DGpuBoardTLM)` + `UsrLinuxEmuIoctlStub` | 编译通过 | ⏳ W4 |
| `configs/dgpu_board_v1_mvp.json.in`(CMake configure_file 注入 `${PTX_EMU_ROOT}`) | `validate_topology` PASS | ⏳ W4 |
| `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION) | ctest PASS | ⏳ W4 |
| `test/test_usrlxemu_ioctl_stub.cc`(**4 IOCTL**:0x27/0x28-ENOSYS/0x29/0x01) | ctest PASS | ⏳ W4 |

### 3.3 关键 Commit

```bash
# W3
git commit -am "feat(doorbell-mvp): SQ tail register with strong-order write path (250-700ns)"
git commit -am "feat(completion-ring-mvp): push + host_notify hook"

# W4
git commit -am "feat(dgpu-board-mvp): DGpuBoardTLM ChStreamModuleBase with 8 components (per Phase F-H.2)"
git commit -am "feat(usrlxemu-ioctl-stub): 4 IOCTL stub (0x27/0x28-ENOSYS/0x29/0x01) for Mode B dGPU board"
git commit -am "feat(configs): dgpu_board_v1_mvp.json with validate_topology support"
git commit -am "test(dgpu-board-v1-mvp): 6 SECTION E2E test + 4 IOCTL tests (per Phase F-H.3)"
```

### 3.4 端到端数据流(W3-4 验证)

```
UsrLinuxEmu (host)                            CppTLM (dGPU board)
─────────────────                            ────────────────
cuModuleLoadData(image_bytes)
  → ioctl(0x27 LOAD_KERNEL_MODULE)            UsrLinuxEmuIoctlStub::ioctl(0x27)
  → HAL #66 → H2D DMA                           DGpuBoardTLM::install_kernel_module()
    → 写 image_bytes → DGpuBar.vram_base()       → PtxEmuSubmoduleMVP::image_load()
    → 返回 vram_addr                              → 返回 image_handle

cuLaunchKernel(grid, block, args, ...)
  → ioctl(0x01 PUSHBUFFER_SUBMIT_BATCH) → 写 gpfifo_entries[] → DGpuBar.vram.pushbuffer_ring
  → ioctl(0x28 LAUNCH_KERNEL_MODULE) → 永久 -ENOSYS(per ADR-090 §D2.2)
  → [Mode B] 直调 DGpuBoardTLM::submit_kernel(req) (UMD shim 简化路径)
                                                  → CommandProcessor (S3:CP fetch + decode)
                                                  → TmuDispatchProcessor::submit()
                                                  → SubmitQueue::enqueue(cta_descriptor)
                                                  → CudaCoreAdapter::on_cta_arrival(cta_desc)
                                                     → PtxEmuSubmoduleMVP::decode_ptxir + submit_kernel_request
                                                     → PTX-EMU::GPUContext → SMContext::exe_once() × N
                                                       → WarpContext::execute_warp_instruction × M
                                                         (functional,per PtxEmuSubmoduleMVP facade)
                                                  → 完成 → sq_.on_warp_complete → tmu_.on_complete
                                                       → CompletionRing::push()
                                                       → host_notify()
                                                          → cuStreamSynchronize 返回
```

### 3.5 风险

- **R4**: ~~TmuDispatchProcessor LIFO 频繁驱逐(32 slot MVP)~~ — **🗑️ 风险已消除(per Phase F-D.2 H5)**:反压停 fetch,容量满拒绝不驱逐;溢出率 >5% 触发 review(统计 `backpressure_count_/submit_count_`)
- **R5**: Doorbell strong-order 延迟区间违反(PCIe Gen5 x16 250-700ns)— 测试断言区间
- **R6**: UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 — stub 严格遵循 `gpu_ioctl.h`

---

## 4. S3 TMU+CP+SQ 链路接通(W5-6)— NVIDIA method packet + TMU 反压 + SubmitQueue 分发

### 4.1 目标

- `CommandProcessor` 5-state FSM 解析 NVIDIA method packet(per Phase F-H.3)
- `Pm4Decoder` NVIDIA method packet(4 method_addr ranges:0x4000-0x40FF DISPATCH_DIRECT / 0x4200-0x42FF EVENT_WRITE / 0x4400-0x44FF RELEASE_MEM / 0x4500-0x45FF ACQUIRE_MEM)
- `TmuDispatchProcessor` dep chain + 反压停 fetch(per Phase F-D.2 H5,非 LIFO)
- **`SubmitQueue` WDU 分发网络**(per Phase F-H.5,单 SM 路由)
- CudaCoreAdapter 深度集成 PTX-EMU(per Phase I.2,4 timing 模块注入)
- 全链路验证:CP→TMU→SQ→CudaCore→CQ

### 4.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `include/tlm/gpu/command_processor_mvp.hh` + `.cc`(5-state FSM,GPU VA fetch) | 5 transition 测试 PASS | ⏳ W5 |
| `include/tlm/gpu/pm4_decoder_mvp.hh` + `.cc`(NVIDIA method packet) | 4 method_addr range + bit field round-trip PASS | ⏳ W5 |
| `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc`(32 slot + 反压停 fetch) | submit / on_complete / BACKPRESSURED / dep chain PASS | ⏳ W5 |
| `include/tlm/gpu/submit_queue_mvp.hh` + `.cc`(WDU 分发网络) | enqueue / tick / dispatch_to_core / on_warp_complete PASS | ⏳ W5 |
| CudaCoreAdapter 深度集成(per Phase I.2) | 6 timing 测试(tick/scoreboard/pipeline/dispatch/warp-state/injection) | ⏳ W6 |
| `test/test_command_processor_mvp.cc` | 5 transition + NVIDIA method packet decode PASS | ⏳ W5 |
| `test/test_pm4_decoder_mvp.cc` + `test_pm4_decoder_mvp_integration.cc` | NVIDIA method packet + CP 集成 PASS | ⏳ W5 |
| `test/test_tmu_dispatch_processor_mvp.cc` | submit / 反压停 fetch / dep chain / 环检测 PASS | ⏳ W5 |
| `test/test_submit_queue_mvp_route.cc` + `_enqueue.cc` + `_dispatch.cc` + `_complete.cc` + `_concurrent.cc` | 5 单测 PASS | ⏳ W5 |
| ❌ ~~`test_cuda_core_adapter_mvp_whitebox.cc`~~ | **🗑️ 已删除**(per DP4=C 决策,由深度集成路径替代) | — |

### 4.3 关键 Commit

```bash
# W5
git commit -am "feat(pm4-decoder-mvp): NVIDIA method packet parsing (per Phase F-H.3 path 3)"
git commit -am "feat(command-processor-mvp): 5-state FSM with Pm4Decoder (NVIDIA method packet)"
git commit -am "feat(tmu-dispatch-mvp): TMU Glue with dep chain + backpressure (32 slot)"
git commit -am "feat(submit-queue-mvp): WDU distribution network (single-SM, per Phase F-H.5)"

# W6
git commit -am "feat(cuda-core-mvp): SM microarchitecture exploration (timing model, per Phase I.2)"
```

### 4.4 深度集成调用路径(per Phase I.1/I.2)

**深度集成路径(MVP 唯一路径,per Phase F-H.7/I.1)**:
```
SubmitQueue::dispatch_to_core(cta_desc)
  → CudaCoreAdapter::on_cta_arrival(cta_desc)
    → PtxEmuSubmoduleMVP::decode_ptxir(image_bytes, size) → StatementContext[]
    → PtxEmuSubmoduleMVP::submit_kernel_request(gpu_ctx_, KernelLaunchRequest)
      → PTX-EMU::GPUContext::submit_kernel_request (内部 C++ 实例方法)
        → PTX-EMU::SMContext::exe_once() × N cycles (per-tick CudaCoreAdapter 调用)
          → (PTX-EMU 内部)WarpContext::execute_warp_instruction(stmt, target_pc)
            → 寄存器/内存/PC 按指令语义更新(FUNCTIONAL,per PtxEmuSubmoduleMVP facade)
  → CudaCoreAdapter::tick() 镜像 WarpState[warp_id] = {cycle_count, exec_mask, blocked_cycles}
    → **不含 PC**(由 PtxEmuSubmoduleMVP::read_thread_pc 读取)
  → CudaCoreAdapter::on_warp_complete → sq_.on_warp_complete → tmu_.on_complete → CQ::push
```

**❌ 已删除路径**(per Phase I.1 重构):
- ~~黑盒 `image_execute`(8 ABI 黑盒)~~
- ~~白盒 `stepOneWarpInstruction`(per DP4=C 永久禁用)~~

### 4.5 风险

- **R7**: CommandProcessor 5-state FSM 状态转换遗漏 — TDD 5 transition 测试
- **R8**: Pm4Decoder NVIDIA method packet 与 UsrLinuxEmu `unpackPm4Header` 比特字段对齐 — 确认 `gpfifo_translator.h:60-73` 真相源一致
- **R9**: ~~PTX-EMU 维护者拒收 `stepOneWarpInstruction` API~~ — **🗑️ 风险已消除(per DP4=C + Phase I.1)**

---

## 5. S4 Production(W7-10)— 全量集成 + validate_topology + v0.5.0-MVP tag

### 5.1 目标

- **ScoreboardTLM / PipelineTLM / TensorCoreTLM 直接使用现有模块**(不创建 `*_v05_mvp.hh` 升级版,per Phase I.2 修订)
- `validate_topology` 集成
- 全部 ≥880 测试 PASS
- `git tag -a v0.5.0-MVP`

### 5.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `validate_topology` CMake target 集成 | `cmake --build build --target validate_topology` PASS | ⏳ W9 |
| 全部 ≥880 测试 PASS(per ADR-SOC-06 G-MVP-4) | `build/bin/cpptlm_tests` PASS | ⏳ W9 |
| `CHANGELOG.md` 记录 v0.5.0-MVP | 文档同步 | ⏳ W10 |
| `git tag -a v0.5.0-MVP` | tag 创建 | ⏳ W10 |
| ❌ ~~`scoreboard_tlm_v05_mvp.hh` + `pipeline_tlm_v05_mvp.hh`~~ | **🗑️ 已取消**(per Phase I.2,直接使用现有 `include/tlm/gpu/` 下的模块) | — |

### 5.3 关键 Commit

```bash
# W9
git commit -am "build: integrate validate_topology target for dgpu_board_v1_mvp.json"

# W10
git commit -am "docs(changelog): record v0.5.0-MVP release (MVP slice)"
git tag -a v0.5.0-MVP -m "cpptlm-v05-mvp: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → SQ → CudaCore + PTX-EMU functional/timing split"
```

### 5.4 风险

- **R10**: 现有 ScoreboardTLM/PipelineTLM 与 PTX-EMU 注入接口不兼容 — 确认 `sm_context.h:87-95` 注入点已对接(per Phase I.2 §1.3)
- **R11**: 880 测试达不到 — S1-S3 累计 ≥50 新增测试,baseline 850 + 50 = 900(目标 ≥880)

---

## 6. [可选] v0.5 完整版(W11-22)— per ADR-X.16 12 周 P0'-P4'

### 6.1 启动条件

**MVP 验收通过 + user sign-off**:
- G-MVP-1 ~ G-MVP-8 全部 ✅
- v0.5.0-MVP tag 创建
- user 启动 v0.5 完整版 change

### 6.2 12 周 P0'-P4' 路线(per ADR-X.16)

```
P0' (W1)   submodule + ADR + openspec change
P1' (W2-4) CP + Pm4Decoder + PtxEmuSubmoduleV05 adapter
P2' (W5-7) ComputeUnit v2 + ScoreboardTLM/PipelineTLM 升级 production
P3' (W8-10) 双路径 byte-identical 验证 + docs
P4' (W11-12) 收尾 + v0.5.0 tag
```

### 6.3 v0.5 完整版与 MVP 差异

| 维度 | MVP | v0.5 完整版 |
|------|-----|------------|
| PM4 opcodes | 4 个(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM) | 18 个全量 |
| TmuDispatch slot | 32 | 256 |
| Scheduler Cache | hash map 单层 | LRU + 1024 entries 双层 |
| pre_exit_policy | NONE 单档 | NONE + LAST_BLOCK + EXPLICIT_KERNEL_MARKER |
| TMU 字段 | 9 字段(MVP 简化) | 21 字段(TMD 6 区完整) |
| 12 SM 模块升级 | 2 个(ScoreboardTLM + PipelineTLM) | 全部 production 升级 |
| 双路径验证 | 1 个 microbenchmark | 5 个(GEMM/vector_add/FlashAttention/stencil/SpMV) |
| PCIe strong-order | 延迟区间断言 | MMU ordering pipe 精确建模 |
| MSI-X + DMA + 多板卡 | ❌ 推迟 | ✅ |
| PREEXIT/ACQBULK | ❌ 推迟 | ✅(若 PTX-EMU 提供 block/PREEXIT 回调) |
| 真实 GPU 周期对齐 | ❌ 不声称 | ❌ 仍不声称(per ADR-X.16 D8) |

---

## 7. 跨仓协调

| 仓 | 跟踪载体 | MVP 状态 | v0.5 完整版状态 |
|----|---------|:---:|:---:|
| **PTX-EMU** | submodule pin `PTX-EMU@87820951`(per DP1=B)— 白盒 API 不需要(per DP4=C 永久禁用) | 🟡 W1 submodule 锁定 | 🟡 v0.5 完整版白盒补足(per ADR-X.16 P0'-P4') |
| **UsrLinuxEmu** | IOCTL 0x27/0x29/0x01 真实路径 + 0x28 永久 -ENOSYS + ADR-090 v2 ✅ Accepted(2026-08-18) | 🟡 S2 stub 模式(W3-4) | 🟡 完整集成(后续) |
| **TaskRunner** | `cuModuleLoadData` 解析(已有) + tadr-308 待创建(per ADR-090 §C0.3) | ✅ 已 ship | - |
| **CppTLM** | 本 roadmap + ADR-SOC-06 + openspec change | 🔵 MVP 实施 | 🟡 v0.5 完整版(后续) |

**MVP 跨仓 commit 顺序**(per ADR-035 §R5.1 + UsrLinuxEmu ADR-090 v2 §C0.4 跨仓引用新规 + DP1=B + DP4=C 决策):
```
[1] PTX-EMU submodule pin @ PTX-EMU@87820951(per DP1=B,2026-08-13 audit commit) → CppTLM S1
    └─ 不发出 HSK-7 公告(per DP4=C,MVP 深度集成路径,不依赖 stepOneWarpInstruction API)
[2] CppTLM S2 stub 模式 → 不依赖 UsrLinuxEmu 编译
[3] CppTLM S3 CP+PM4+TMU+黑盒 warp 调用 → 不需要 PTX-EMU 新 API(per DP4=C)
[4] CppTLM S4 v0.5.0-MVP tag → user sign-off(维持 4 周 W7-W10,per DP2=A)
[5] (可选) v0.5 完整版 12 周 + 白盒 warp 调用补足(per ADR-X.16 P0'-P4',需 HSK-N 公告)
```

**HSK 协议编号澄清**(per Phase A 修复 S6/NR5 + DP4=C):
- **HSK-1**: PTX-EMU ABI 真相源(`include/cudart/cpptlm_module.h:12-52`,`CPPTLM_MODULE_VERSION=2`)— 已 ship
- **HSK-6**: UsrLinuxEmu ADR-090 v2 §D5 联发协议(PTX-EMU 发起,CppTLM ack,UsrLinuxEmu 利益相关方)— ✅ Accepted 2026-08-18
- ~~HSK-7~~(per DP4=C 决策)— **不发出**:MVP 深度集成路径,不依赖 PTX-EMU 新增 API
- ~~HSK-8~~(原 roadmap 误称)— **废除**(与 ADR-090 §D5 编号冲突)

---

## 8. 验收门(MVP 汇总)

| Gate | 内容 | 状态 | 周 |
|------|------|:---:|-----|
| **G-MVP-1** | S1 submodule + 内部链路跑通 | ⏳ | W1-2 |
| **G-MVP-2** | S2 DGpuBoard + Doorbell + SQ/CQ + JSON | ⏳ | W3-4 |
| **G-MVP-3** | S3 CP + PM4 + TMU + warp 调用 | ⏳ | W5-6 |
| **G-MVP-4** | S4 Production + validate_topology | ⏳ | W7-10 |
| **G-MVP-5** | UsrLinuxEmu IOCTL 0x27/0x29/0x01 + 0x28 永久 -ENOSYS 真实路径(4 IOCTL,per Phase F-H.3)| ⏳ | W4 |
| **G-MVP-6** | 编译防火墙验证 | ⏳ | W2 |
| **G-MVP-7** | v0.5.0-MVP tag | � | W10 |
| **G-MVP-8** | ~~HSK-7/8 公告发出~~(per DP4=C **不需要**,MVP 深度集成路径) | — | — |

---

## 9. 风险登记(MVP 整体)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | ~~PTX-EMU 维护者拒收 `stepOneWarpInstruction` API~~ | — | — | **🗑️ 风险已消除(per DP4=C + Phase I.1)**:MVP 深度集成路径,不依赖该 API |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R4 | CommandProcessor 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R5 | ~~TmuDispatchProcessor LIFO 频繁驱逐~~ | — | — | **🗑️ 风险已消除(per Phase F-D.2 H5)**:反压停 fetch,32 slot MVP 容量满 `BACKPRESSURED` 拒绝不驱逐 |
| R6 | 6-10 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ 严格 TDD 5 步 |
| R7 | PtxEmuSubmoduleMVP 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 + CI 拦截 |
| R8 | PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R9 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | MVP 仅"内部一致性验证",不声称真实对齐 |
| R10 | ScoreboardTLM/PipelineTLM 升级与 PTX-EMU 同步错误 | 中 | 中 | TDD 5 步结构 + 集成测试 |
| R11 | v0.5.0-MVP tag 后 user 不启动 v0.5 完整版 | 中 | 低 | MVP 本身已可独立生产(黑盒 + 内部一致性) |
| R12 | DGpuBar MMIO 写入触发其他模块状态机错误 | 中 | 中 | 单测覆盖 + 集成测试 |

---

## 10. 修订历史

- **2026-08-19**: 初版 — per ADR-SOC-06 D1 切片(MVP 4 阶段 6-10 周 + 可选 v0.5 完整版 12 周)
- **2026-08-20**: Phase A/C/F/F-G 修订 — 阶段链路、决策落地、Phase F Oracle 审查
- **2026-08-20**: **Phase F-H 架构重定义**(用户要求"TMU→CudaCore 之间有分发网络 + MVP 深度集成 PTX-EMU"):
  - **S3 阶段扩展**:从"CP + PM4 + TMU + per-warp 调用"改为"CP + NVIDIA PM4 + TMU + **SubmitQueue(WDU 分发网络)** + CudaCore **深度集成 PTX-EMU**"
  - **S1 S2 单元测试更新**:`8 ABI 单测` → `深度集成接口单测`;`黑盒 dispatch_blackbox` → `驱动式 warp 执行(on_cta_arrival + per-tick exe_once + WarpState 镜像)`
  - **新增模块**:`include/tlm/gpu/submit_queue_mvp.hh` + `.cc`(per `submit-queue.md` §F-H.5 WDU 单 SM 路由 + per-cluster pending FIFO 32 + per-core active 4)
- **2026-08-20**: **Phase I 重构**(functional/timing 分离,per gpgpu-sim 分层):
  - PtxEmuSubmoduleMVP 重定位为 **PTX functional facade**(删除 8 ABI 黑盒 + stepOneWarpInstruction,改 4 类 functional 接口)
  - CudaCoreAdapter 重定位为 **SM 微架构探索器**(集成 4 个 TLM 模块,删除双路径 dispatch_blackbox/whitebox)
  - WarpState 重新设计(timing only,不含 PC,PC 由 facade 负责)
  - `read_blocked_cycles` 补缺(per FIX-H8/B.3,避免裸调 PTX-EMU)
- **2026-08-21**: **Phase K 全面修复 + 拆分**(Oracle ses_fe179d02 审查后):
  - **CRITICAL 修复**(3 项):ADR-SOC-06 §3 + §8.1 全过时;command-processor.md §4.2 fetch_packet 位提取错误(`h>>1` + `h>>20`,非 `h` + `h>>24`);tmu-dispatch-processor.md §5.4 LIFO 残留(反压停 fetch 替代)
  - **HIGH 修复**(6 项):dgpu-board.md 5→8 组件 + SubchannelContext[5]→[8] + 0x29 IOCTL 真实工作;roadmap §4/§5 整节重写;cuda-core-adapter.md 接口矛盾 + read_blocked_cycles;proposal.md 死引用 + SubmitQueue 归属 S3→S2
  - **B.2/B.3 裁定**:CudaCoreAdapter 持有 4 TLM 模块 + facade 补 read_blocked_cycles
  - **拆分**:原单 change 拆为 3 个独立 change(s1 基础设施 / s2 板卡 / s3 数据面+tag,per Oracle 拆分建议)
  - **6 commits 累计 9 commit push 到 origin/main**(ahead by 9)
  - **新增 §1.5 进度追踪**:openspec change 状态表 + 4 阶段 vs 3 change 映射 + 测试增长 + 依赖关系 + Phase K 同步内容

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-20*
