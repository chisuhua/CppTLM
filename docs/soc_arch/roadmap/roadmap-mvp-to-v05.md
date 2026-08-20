# Roadmap: cpptlm-v05-mvp → cpptlm-v05-full (MVP 切片到完整版)

> **类别**: SoC Architecture > Roadmap · **状态**: 🔵 MVP 切片 (per ADR-X.17)
> **日期**: 2026-08-19 · **维护者**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D1
> **关联 OpenSpec**:
> - MVP: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> - 完整版: [`openspec/changes/2026-08-19-cpptlm-v05-redo/`](../../../openspec/changes/2026-08-19-cpptlm-v05-redo/) (12 周 P0'-P4',MVP 验证后启动)
> **关联模块**: [DGpuBoardTLM](../modules/dgpu-board.md) + [CommandProcessor](../modules/command-processor.md) + [Pm4Decoder](../modules/pm4-decoder.md) + [TmuDispatchProcessor](../modules/tmu-dispatch-processor.md) + [CudaCoreAdapter](../modules/cuda-core-adapter.md) + [PtxEmuSubmoduleMVP](../modules/ptx-emu-submodule-mvp.md)

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
│  │ S2 Real-Board-Bind  │  ← 接 UsrLinuxEmu IOCTL 0x27/0x28                  │
│  │   - DGpuBoardTLM     │                                                   │
│  │   - DGpuBar/Doorbell │                                                   │
│  │   - SQ/CQ            │                                                   │
│  │   - JSON config      │                                                   │
│  │   - IOCTL stub       │                                                   │
│  └──────────┬──────────┘                                                   │
│             │ 验收通过                                                      │
│             ▼                                                                │
│  ┌─────────────────────┐                                                   │
│  │ S3 Warp-Precision    │  ← CP + PM4 + TMU + per-warp 调用                │
│  │   - CommandProcessor │                                                   │
│  │   - Pm4Decoder       │                                                   │
│  │   - TmuDispatch      │                                                   │
│  │   - 白盒 warp 路径   │                                                   │
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

## 2. S1 MVP-Cut(W1-2)— 基础链路跑通

### 2.1 目标

- `PtxEmuSubmoduleMVP` 加载 PTX-EMU submodule
- `CudaCoreAdapter` 黑盒 dispatch 路径跑通
- 5 单测 PASS(per ADR-X.17 G-MVP-1)

### 2.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `git submodule add external/PTX-EMU` | `git submodule status` 显示 PTX-EMU commit hash | ⏳ W1 |
| `CMakeLists.txt` `add_subdirectory(external/PTX-EMU)` + `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` | `cmake --build build` 通过 | ⏳ W1 |
| `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `.cc` | 编译通过 | � W1-2 |
| `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `.cc` | 黑盒 dispatch_blackbox 通过 | ⏳ W2 |
| `test/test_ptx_emu_submodule_mvp.cc` 8 ABI 单测 | ctest PASS | ⏳ W2 |
| `test/test_cuda_core_adapter_mvp.cc` 黑盒 dispatch | ctest PASS | � W2 |

### 2.3 关键 Commit

```bash
# W1
git commit -am "chore(submodule): add external/PTX-EMU@<commit_hash>"
git commit -am "build(cmake): add_subdirectory(external/PTX-EMU) — submodule static link"
git commit -am "feat(ptx-emu-mvp): PtxEmuSubmoduleMVP adapter with 8 ABI passthrough"

# W2
git commit -am "feat(cuda-core-mvp): CudaCoreAdapter with dispatch_blackbox (image_execute path)"
```

### 2.4 风险

- **R1**: PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2)— `PTX_EMU_BUILD_TESTS=OFF` 缓解
- **R2**: 编译防火墙破裂 — `git grep` CI 拦截
- **R3**: PTX-EMU 维护者拒收 API — MVP 黑盒路径不依赖新 API

---

## 3. S2 Real-Board-Bind(W3-4)— 接入 UsrLinuxEmu 真实板卡驱动

### 3.1 目标

- `DGpuBoardTLM` 包装 5 内部组件
- 接入 UsrLinuxEmu IOCTL 0x27/0x28 stub(端到端跑通)
- JSON config 驱动 `instantiateAll`
- 6 SECTION E2E 测试 PASS(per ADR-X.17 G-MVP-2)

### 3.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `include/tlm/gpu/dgpu_board_mvp.hh` + `.cc`(5 组件包装) | 编译通过 | ⏳ W3 |
| `include/tlm/gpu/doorbell_mvp.hh` + `.cc`(SQ tail + strong-order) | strong-order 延迟区间 250-700ns 测试 PASS | ⏳ W3 |
| `include/tlm/gpu/submission_queue_mvp.hh` + `.cc`(per-stream FIFO) | enqueue/tick PASS | ⏳ W3 |
| `include/tlm/gpu/completion_ring_mvp.hh` + `.cc`(push + host_notify) | push/host_notify PASS | ⏳ W3 |
| `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc`(0x27/0x28/0x29 stub) | 3 IOCTL PASS | ⏳ W4 |
| `include/chstream_register.hh` 加 `REGISTER_CHSTREAM(DGpuBoardTLM)` + `UsrLinuxEmuIoctlStub` | 编译通过 | � W4 |
| `configs/dgpu_board_v1_mvp.json.in`(CMake configure_file 注入 `${PTX_EMU_ROOT}`) | `validate_topology` PASS | ⏳ W4 |
| `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION) | ctest PASS | ⏳ W4 |
| `test/test_usrlxemu_ioctl_stub.cc`(3 IOCTL) | ctest PASS | ⏳ W4 |

### 3.3 关键 Commit

```bash
# W3
git commit -am "feat(doorbell-mvp): SQ tail register with strong-order write path (250-700ns)"
git commit -am "feat(submission-queue-mvp): per-stream FIFO consumer"
git commit -am "feat(completion-ring-mvp): push + host_notify hook"

# W4
git commit -am "feat(dgpu-board-mvp): DGpuBoardTLM ChStreamModuleBase with 5 components"
git commit -am "feat(usrlxemu-ioctl-stub): IOCTL 0x27/0x28/0x29 stub for Mode B dGPU board"
git commit -am "feat(configs): dgpu_board_v1_mvp.json with validate_topology support"
git commit -am "test(dgpu-board-v1-mvp): 6 SECTION E2E test + 3 IOCTL tests"
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
  → [Mode B 重写] DGpuBoardTLM::submit_kernel(req)
                                                 → CommandProcessor (S3 启用)
                                                 → TmuDispatchProcessor::submit()
                                                 → CudaCoreAdapter::issueTask()
                                                    → PtxEmuSubmoduleMVP::image_execute()
                                                       → PTX-EMU GPUContext
                                                       → SMContext::exe_once() × N
                                                          → WarpContext::execute_warp_instruction × M
    → 返回 image_id                               → kernel 完成 → on_complete
                                                      → CompletionRing::push()
                                                      → host_notify()
                                                         → cuStreamSynchronize 返回
```

### 3.5 风险

- **R4**: TmuDispatchProcessor LIFO 频繁驱逐(32 slot MVP)— 溢出率 >5% 触发 review
- **R5**: Doorbell strong-order 延迟区间违反(PCIe Gen5 x16 250-700ns)— 测试断言区间
- **R6**: UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 — stub 严格遵循 `gpu_ioctl.h`

---

## 4. S3 Warp-Precision(W5-6)— per-warp 指令调用

### 4.1 目标

- `CommandProcessor` 5-state FSM 解析 PM4
- `Pm4Decoder` Mesa-style TYPE3(4 MVP opcodes)
- `TmuDispatchProcessor` dep chain + LIFO
- `CudaCoreAdapter` **白盒路径**调 PTX-EMU `WarpContext::execute_warp_instruction`
- per-warp cycle 跟踪 PASS

### 4.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `include/tlm/gpu/command_processor_mvp.hh` + `.cc`(5-state FSM) | 5 transition 测试 PASS | ⏳ W5 |
| `include/tlm/gpu/pm4_decoder_mvp.hh` + `.cc`(Mesa TYPE3 + 4 opcodes) | 4 opcode + bit field round-trip PASS | ⏳ W5 |
| `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc`(32 slot + dep) | submit / on_complete / LIFO / dep chain PASS | ⏳ W5 |
| `CudaCoreAdapter` 升级白盒 dispatch_whitebox | per-warp step PASS | ⏳ W6 |
| `PtxEmuSubmoduleMVP` 升级白盒 stepOneWarpInstruction | PTX-EMU 新 API 接入(S3 触发 HSK-7 公告)| ⏳ W6 |
| `test/test_command_processor_mvp.cc` | 5 transition PASS | ⏳ W5 |
| `test/test_pm4_decoder_mvp.cc` + `test_pm4_decoder_mvp_integration.cc` | PASS | ⏳ W5 |
| `test/test_tmu_dispatch_processor_mvp.cc` | PASS | ⏳ W5 |
| `test/test_cuda_core_adapter_mvp_whitebox.cc` | per-warp cycle PASS | ⏳ W6 |

### 4.3 关键 Commit

```bash
# W5
git commit -am "feat(pm4-decoder-mvp): Mesa-style TYPE3 header parsing (4 MVP opcodes)"
git commit -am "feat(command-processor-mvp): 5-state FSM with Pm4Decoder integration"
git commit -am "feat(tmu-dispatch-mvp): TMU Glue with dep chain + LIFO (32 slot MVP)"

# W6
git commit -am "feat(cuda-core-mvp): whitebox dispatch_whitebox + per-warp WarpState"
git commit -am "feat(ptx-emu-mvp): stepOneWarpInstruction whitebox API"
```

### 4.4 per-warp 调用接口

**黑盒路径**(MVP 默认,无需新 API):
```
CudaCoreAdapter::issueTask()
  → PtxEmuSubmoduleMVP::image_execute()
    → PTX-EMU GPUContext (内部完整执行)
      → SMContext::exe_once() × N (每 cycle 内部)
        → (PTX-EMU 内部)WarpContext::execute_warp_instruction × M
```

**白盒路径**(S3 启用,需 PTX-EMU 新 API):
```
CudaCoreAdapter::dispatch_whitebox(warp_count, max_cycles)
  → 循环 PtxEmuSubmoduleMVP::stepOneWarpInstruction(warp_id, &pc, &status, &cycle_count)
    → PTX-EMU SMContext::stepOneWarpInstruction(warp_id, ...)
      → WarpContext::execute_warp_instruction(stmt, target_pc)
      → 返回 PC + cycle_count + status
  → 更新 WarpState[warp_id] = { pc, cycle_count, register_deps }
```

### 4.5 风险

- **R7**: CommandProcessor 5-state FSM 状态转换遗漏 — TDD 5 transition 测试
- **R8**: Pm4Decoder Mesa-style 与 KFD convention 冲突 — 同时验证两种 convention
- **R9**: PTX-EMU 维护者拒收 `stepOneWarpInstruction` API — MVP 仅黑盒,白盒推迟到 v0.5 完整版

---

## 5. S4 Production(W7-10)— ScoreboardTLM/PipelineTLM 升级 + v0.5.0-MVP tag

### 5.1 目标

- `ScoreboardTLM` 升级 production(per-warp cycle tracking)
- `PipelineTLM` 升级 production(latency issue)
- `validate_topology` 集成
- 全部 ≥880 测试 PASS
- `git tag -a v0.5.0-MVP`

### 5.2 关键交付

| 交付 | 验证 | 状态 |
|------|------|:---:|
| `include/tlm/gpu/scoreboard_tlm_v05_mvp.hh` per-warp cycle | 单元测试 PASS | ⏳ W7 |
| `include/tlm/gpu/pipeline_tlm_v05_mvp.hh` latency issue | 单元测试 PASS | ⏳ W7 |
| `ScoreboardTLM::tick()` per-warp tracking 与 PTX-EMU 同步 | 集成测试 PASS | ⏳ W8 |
| `PipelineTLM::issue(latency)` API | 集成测试 PASS | ⏳ W8 |
| `validate_topology` CMake target 集成 | `cmake --build build --target validate_topology` PASS | ⏳ W9 |
| 全部 ≥880 测试 PASS(per ADR-X.17 G-MVP-4) | `build/bin/cpptlm_tests` PASS | ⏳ W9 |
| `CHANGELOG.md` 记录 v0.5.0-MVP | 文档同步 | ⏳ W10 |
| `git tag -a v0.5.0-MVP` | tag 创建 | ⏳ W10 |

### 5.3 关键 Commit

```bash
# W7
git commit -am "feat(scoreboard-v05-mvp): per-warp cycle tracking (upgrade from Legacy)"

# W8
git commit -am "feat(pipeline-v05-mvp): latency issue API (upgrade from Legacy)"

# W9
git commit -am "build: integrate validate_topology target for dgpu_board_v1_mvp.json"

# W10
git commit -am "docs(changelog): record v0.5.0-MVP release (MVP slice)"
git tag -a v0.5.0-MVP -m "cpptlm-v05-mvp: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → Cuda Core → PTX-EMU warp"
```

### 5.4 风险

- **R10**: ScoreboardTLM 升级与 PTX-EMU 同步错误 — TDD 5 步结构 + 集成测试
- **R11**: PipelineTLM latency issue 与 PTX-EMU 内部不一致 — 白盒路径校准
- **R12**: 880 测试达不到 — S1-S3 累计 ≥50 新增测试,baseline 850 + 50 = 900(目标 ≥880)

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
| **PTX-EMU** | submodule pin + (可选)`stepOneWarpInstruction` 新 API | 🟡 W1 submodule + W6 可选白盒 API | 🟡 12 周内可推动 |
| **UsrLinuxEmu** | IOCTL 0x27/0x28 真实路径 + HSK-8 协议(后续) | 🟡 S2 stub 模式(W3-4) | 🟡 完整集成(后续) |
| **TaskRunner** | `cuModuleLoadData` 解析(已有) | ✅ 已 ship | - |
| **CppTLM** | 本 roadmap + ADR-X.17 + openspec change | 🔵 MVP 实施 | 🟡 v0.5 完整版(后续) |

**MVP 跨仓 commit 顺序**(per ADR-035 §R5.1):
```
[1] PTX-EMU submodule pin(已有 ccd34155) → CppTLM S1
[2] CppTLM S2 stub 模式 → 不依赖 UsrLinuxEmu 编译
[3] CppTLM S3 可选白盒 → 需 PTX-EMU 接受 stepOneWarpInstruction
[4] CppTLM S4 v0.5.0-MVP tag → user sign-off
[5] (可选) v0.5 完整版 12 周 + HSK-8 UsrLinuxEmu 联调
```

---

## 8. 验收门(MVP 汇总)

| Gate | 内容 | 状态 | 周 |
|------|------|:---:|-----|
| **G-MVP-1** | S1 submodule + 内部链路跑通 | ⏳ | W1-2 |
| **G-MVP-2** | S2 DGpuBoard + Doorbell + SQ/CQ + JSON | ⏳ | W3-4 |
| **G-MVP-3** | S3 CP + PM4 + TMU + warp 调用 | ⏳ | W5-6 |
| **G-MVP-4** | S4 Production + validate_topology | ⏳ | W7-10 |
| **G-MVP-5** | UsrLinuxEmu IOCTL 0x27/0x28 真实路径 | ⏳ | W4 |
| **G-MVP-6** | 编译防火墙验证 | ⏳ | W2 |
| **G-MVP-7** | v0.5.0-MVP tag | � | W10 |
| **G-MVP-8** | HSK-7/8 公告发出(可选,若需 PTX-EMU 新 API) | ⏳ | W1 |

---

## 9. 风险登记(MVP 整体)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | PTX-EMU 维护者拒收 `stepOneWarpInstruction` API | 中 | 中 | MVP 仅黑盒,白盒推迟到 v0.5 完整版;fork 兜底 |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R4 | CommandProcessor 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R5 | TmuDispatchProcessor LIFO 频繁驱逐 | 中 | 中 | 32 slot MVP;溢出率 >5% 触发 review |
| R6 | 6-10 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ 严格 TDD 5 步 |
| R7 | PtxEmuSubmoduleMVP 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 + CI 拦截 |
| R8 | PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R9 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | MVP 仅"内部一致性验证",不声称真实对齐 |
| R10 | ScoreboardTLM/PipelineTLM 升级与 PTX-EMU 同步错误 | 中 | 中 | TDD 5 步结构 + 集成测试 |
| R11 | v0.5.0-MVP tag 后 user 不启动 v0.5 完整版 | 中 | 低 | MVP 本身已可独立生产(黑盒 + 内部一致性) |
| R12 | DGpuBar MMIO 写入触发其他模块状态机错误 | 中 | 中 | 单测覆盖 + 集成测试 |

---

## 10. 修订历史

- **2026-08-19**: 初版 — per ADR-X.17 D1 切片(MVP 4 阶段 6-10 周 + 可选 v0.5 完整版 12 周)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
