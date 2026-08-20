# cpptlm-v05-mvp: dGPU Board MVP Slice — Design

> **配套**: [`proposal.md`](../proposal.md) · [`tasks.md`](../tasks.md) · [`specs/`](../specs/)
> **状态**: 📐 Design — 与 ADR-X.17 同步 · **Owner**: CppTLM Team
> **关联 ADR**: [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md)

> 本文件是**MVP 切片的架构设计索引**。详细契约见各模块设计文档:
> - [`docs/soc_arch/modules/dgpu-board.md`](../../../docs/soc_arch/modules/dgpu-board.md) — DGpuBoardTLM
> - [`docs/soc_arch/modules/command-processor.md`](../../../docs/soc_arch/modules/command-processor.md) — CommandProcessor
> - [`docs/soc_arch/modules/pm4-decoder.md`](../../../docs/soc_arch/modules/pm4-decoder.md) — Pm4Decoder
> - [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../../../docs/soc_arch/modules/tmu-dispatch-processor.md) — TmuDispatchProcessor
> - [`docs/soc_arch/modules/cuda-core-adapter.md`](../../../docs/soc_arch/modules/cuda-core-adapter.md) — CudaCoreAdapter
> - [`docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../../../docs/soc_arch/modules/ptx-emu-submodule-mvp.md) — PtxEmuSubmoduleMVP

---

## 1. 架构概览(MVP)

```
┌─────────────────────────────────────────────────────────────────────┐
│              Host (UsrLinuxEmu + TaskRunner)                         │
│                                                                     │
│  cuModuleLoadData(image_bytes)                                      │
│    → ioctl(0x27 LOAD_KERNEL_MODULE) → HAL #66 → H2D DMA            │
│      → 写 image_bytes → DGpuBar.vram_base()                          │
│                                                                     │
│  cuLaunchKernel(grid, block, args, ...)                             │
│    → [Mode B 重写] DGpuBoardTLM::submit_kernel(req)                  │
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
│  CommandProcessor (5-state FSM) → Pm4Decoder (Mesa TYPE3 + 4 MVP)   │
│         ↓                                                            │
│  TmuDispatchProcessor (TMU Glue, 32 slot + dep chain + LIFO)        │
│         ↓                                                            │
│  CudaCoreAdapter (per-warp step 入口)                               │
│    ├─ 黑盒: PtxEmuSubmoduleMVP::image_execute                       │
│    │    → PTX-EMU GPUContext → SMContext::exe_once() × N            │
│    │       → (内部) WarpContext::execute_warp_instruction × M        │
│    └─ 白盒(S3 可选): PtxEmuSubmoduleMVP::stepOneWarpInstruction     │
│         → per-warp PC + cycle_count + status                        │
│         ↓                                                            │
│  SubmissionQueue (per-stream FIFO) → CompletionRing::push           │
│         ↓                                                            │
│  host_notify_() → cuStreamSynchronize 返回                          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. 5 个新模块(MVP)

| 模块 | 位置 | 角色 | MVP 简化 |
|------|------|------|---------|
| `CommandProcessor` | DGpuBoardTLM 内部组件 | 5-state FSM(PM4 解析入口)| 4 opcodes MVP(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM)|
| `Pm4Decoder` | CommandProcessor 内部 | Mesa-style TYPE3 解析 | bit field 严格对齐 Mesa(IT bit0 + predicate bit1 + opcode bits 2-9)|
| `TmuDispatchProcessor` | DGpuBoardTLM 内部 | TMU Glue(SQ ↔ CudaCore) | 32 slot(v0.5 完整版 256)+ dep latch + LIFO |
| `CudaCoreAdapter` | DGpuBoardTLM 内部 | **新概念** — Cuda Core 包装 | 双路径(blackbox + whitebox)|
| `PtxEmuSubmoduleMVP` | DGpuBoardTLM 内部 | PTX-EMU adapter(编译防火墙) | 8 ABI 透传 + 可选 `stepOneWarpInstruction` |

**复用组件**:
- `DGpuBar`(v0.4 已实施) — PCIe BAR0 MMIO + VRAM backing
- `MemoryCluster`(已存在) — 多通道 HBM/DDR(VRAM 物理 backing)
- `GpuNoC`(已存在) — GPU 端 mesh interconnect(MVP 可选)

---

## 3. MVP vs v0.5 完整版裁剪(per ADR-X.17 D1)

| 保留(MVP 必需) | 裁剪(推迟到 v0.5 完整版) |
|-----------------|------------------------|
| submodule + 黑盒 + 内部链路 | 12 SM 模块全部 production 升级(仅 2 个) |
| CP + PM4(4 opcodes)+ TMU Glue | TMD 6 区精细化 + 18 opcodes 全量 |
| per-warp step 调用(白盒可选)| 双路径 byte-identical 全量验证 |
| IOCTL 0x27/0x28 真实路径 | MSI-X + DMA + 多板卡 |
| ChStreamModuleBase + JSON config | PCIe MMU pipe 精确建模 |
| 编译防火墙 | PREEXIT/ACQBULK 指令仿真 |

---

## 4. 端到端数据流(per ADR-X.17 §3)

详见 [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md) §3 端到端数据流。

---

## 5. 接口稳定性(MVP 阶段冻结 per ADR-X.17 D8)

### 5.1 冻结接口(S4 完成前禁止变更)

- ✅ `CommandProcessor::submit_kernel(...)` API
- ✅ `Pm4Decoder::parse_type3(header, payload, max_dwords) → Pm4Packet`
- ✅ `TmuDispatchProcessor::submit / on_complete / try_chain_dependent`
- ✅ `CudaCoreAdapter::dispatch_blackbox(DispatchParams)` + `dispatch_whitebox(warp_count, max_cycles)`
- ✅ `PtxEmuSubmoduleMVP::image_load / image_execute`(8 ABI 透传)
- ✅ `PtxEmuSubmoduleMVP::stepOneWarpInstruction`(白盒可选)
- ✅ `DGpuBoardTLM::install_kernel_module / submit_kernel / write_reg`
- ✅ `Doorbell::ring(stream_id, tail)` + strong-ordered write 延迟断言(250-700ns)
- ✅ `CompletionRing::push(image_id, status)` + `set_host_notify(hook)`

### 5.2 可演进接口(S1-S3 期间允许调整)

- 🟡 `CommandProcessor` 5-state FSM 内部状态转换
- 🟡 `Pm4Decoder` 字段解析顺序与子模块拆分
- 🟡 `TmuDispatchProcessor` LIFO 驱逐策略与 dep chain 推进规则
- 🟡 `DGpuBoardTLM::tick()` 调度顺序

### 5.3 内部接口(S4 后允许调整)

- 🟡 `PtxEmuSubmoduleMVP` 内部 `ptxsim::SMContext*` 实例化
- 🟡 `CudaCoreAdapter` 与 PTX-EMU::WarpScheduler 注入协议

---

## 6. 关键时序特性(MVP 默认)

| 阶段 | 延迟 | 备注 |
|------|------|------|
| `ioctl(0x27)` → H2D DMA → `install_kernel_module` | 模拟为 1 tick(0 cycle 延迟) | MVP 不仿真 H2D 物理延迟 |
| `write_reg(0x1000+stream_id)` → Doorbell ring | **250-700ns 区间断言** | MVP 仅断言区间,不仿真 MMU pipe |
| CP FETCH → DECODE → DISPATCH | ~1 tick(简化) | MVP 不仿真 PM4 解析 cycle 精度 |
| SQ tick → CudaCoreAdapter::dispatch_blackbox | 1 tick | 黑盒 MVP 路径 |
| CudaCoreAdapter::image_execute → PTX-EMU | variable | 自包含 GPU sim |
| CompletionRing::push + host_notify | ~50ns 模拟 | signal hook |

**总 cycle budget(MVP 默认)**:`500ns-2us + kernel exec`

---

## 7. 测试策略

### 7.1 单元测试(S1-S4 各阶段)

| 模块 | 测试 | Catch2 标签 |
|------|------|-------------|
| PtxEmuSubmoduleMVP | submodule init + 8 ABI 单测 + 编译防火墙 | `[ptx-emu-v05][mvp]` |
| CudaCoreAdapter | 黑盒 dispatch_blackbox | `[cuda-core][mvp]` |
| DGpuBar | BAR0 regs 读写 + VRAM backing | `[dgpu-bar][mvp]` |
| Doorbell | strong-order 延迟区间 + 同 stream 顺序 | `[doorbell][mvp][strong-order]` |
| SubmissionQueue | enqueue/tick + pending_count | `[submission-queue][mvp]` |
| CompletionRing | push/pop + host_notify | `[completion-ring][mvp]` |
| UsrLinuxEmuIoctlStub | IOCTL 0x27/0/28/0x29 | `[usrlxemu-ioctl][mvp]` |
| CommandProcessor | 5-state FSM transition | `[command-processor][mvp]` |
| Pm4Decoder | Mesa bit field + 4 opcode | `[pm4-decoder][mvp]` |
| TmuDispatchProcessor | submit/on_complete/LIFO/dep chain | `[tmu][mvp][glue]` |
| ScoreboardTLM (S4) | per-warp cycle tracking | `[scoreboard-v05][mvp]` |
| PipelineTLM (S4) | latency issue | `[pipeline-v05][mvp]` |

### 7.2 集成测试(W3-4 S2)

| 测试 | 内容 | 标签 |
|------|------|------|
| `test_dgpu_board_v1_mvp_from_config.cc` | 6 SECTION: validate_topology / instantiateAll / H2D / launch / host_notify / 负面路径 | `[dgpu-board][mvp][e2e]` |
| `test_pm4_decoder_mvp_integration.cc` | CP + Pm4Decoder 集成 | `[command-processor][pm4-decoder][integration]` |

### 7.3 E2E 测试(W3-4 S2)

| 测试 | 内容 | 标签 |
|------|------|------|
| cuModuleLoadData E2E | IOCTL 0x27 → DGpuBar.vram_base → image_load | `[P3][E2E][usrlxemu]` |
| cuLaunchKernel E2E | IOCTL 0x28 → Doorbell → CP → SQ → CQ | `[P3][E2E][usrlxemu]` |
| cuStreamSynchronize E2E | fence 等待 → CQ pop | `[P3][E2E][usrlxemu]` |

---

## 8. 风险与缓解(per ADR-X.17 §6.3)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | PTX-EMU 维护者拒收 `stepOneWarpInstruction` API | 中 | 中 | MVP 仅黑盒;白盒推迟到 v0.5 完整版;fork 兜底 |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R4 | CommandProcessor 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R5 | TmuDispatchProcessor LIFO 频繁驱逐 | 中 | 中 | 32 slot MVP;溢出率 >5% 触发 review |
| R6 | 6-10 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ 严格 TDD 5 步 |
| R7 | PtxEmuSubmoduleMVP 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 + CI 拦截 |
| R8 | PTX-EMU submodule 构建依赖扩散(ANTLR4 4.13.2) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R9 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | MVP 仅"内部一致性验证",不声称真实对齐 |

---

## 9. 阶段化交付(per [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md))

```
S1 MVP-Cut (W1-2): submodule + 黑盒 + 内部链路
S2 Real-Board-Bind (W3-4): DGpuBoard + Doorbell + SQ/CQ + JSON + IOCTL stub
S3 Warp-Precision (W5-6): CP + PM4 + TMU + per-warp step 调用
S4 Production (W7-10): ScoreboardTLM/PipelineTLM 升级 + validate_topology + v0.5.0-MVP tag
```

---

## 10. 配套文档

- [`proposal.md`](../proposal.md) — 实施提案(Why / What Changes / Acceptance Gate / Cross-Repo)
- [`tasks.md`](../tasks.md) — S1-S4 任务清单
- [`specs/`](../specs/) — MVP 切片规格说明
- [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md) — 8 项决策锁定
- 6 个模块设计文档 — 见本页 §"模块设计文档"

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 与 ADR-X.17 + S1 启动同步
**下次更新**: W2 S1 完成时(submodule pin + 8 ABI PASS)