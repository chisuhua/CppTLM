# cpptlm-v05-mvp: dGPU Board MVP Slice (CP→TMU→Cuda Core → PTX-EMU warp)

> **状态**: 📋 Proposed — 2026-08-19 · **日期**: 2026-08-19 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md) (Proposed,8 项决策锁定)
> **取代(部分)**:
> - [`docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md`](../../../docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md) (v0.5 redo,12 周完整版,被 MVP 切片取代)
> - [`openspec/changes/archive/2026-08-18-cpptlm-v3-dgpu-extract/`](../../archive/2026-08-18-cpptlm-v3-dgpu-extract/) (v0.4,沿用)
> **目标**: 4 阶段 6-10 周交付 MVP 可运行端到端链路(UsrLinuxEmu IOCTL → CppTLM CP→TMU→Cuda Core → PTX-EMU warp 指令调用)

---

## Why

[`ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md)(Proposed 2026-08-19)的 8 项决策锁定 MVP 切片:

| # | 决策 | 取代 |
|---|------|------|
| D1 | MVP 切片 = 4 阶段 6-10 周(Net-new,不替代 12 周完整版)| ADR-X.16 12 周时间线过重 |
| D2 | PTX-EMU warp 调用 = 双路径(MVP 黑盒 + 精度白盒可选)| 单一黑盒 image_execute 不够 |
| D3 | PTX-EMU 集成 = git submodule + adapter 编译防火墙 | v0.4 黑盒 dlopen 不安全 |
| D4 | UsrLinuxEmu 接入 = IOCTL 0x27/0x28 真实路径(端到端跑通)| 真实板卡驱动接入 |
| D5 | 模块架构 = CP→TMU→Cuda Core 链路(新增 5 模块)| Phase 7.A 黑盒链路不仿真真实硬件 |
| D6 | JSON config 驱动 + validate_topology 集成 | 模块配置零散 |
| D7 | CPPTLMBRIDGE_VERSION = 2 永久冻结 | 不发 C ABI v3 |
| D8 | 接口稳定性(MVP 冻结 8 项,可演进 4 项) | 提前优化接口 |

**触发事件**:
- 2026-08-19 user 提出 MVP 切片需求(可运行 + 接入真实板卡驱动 + CP→TMU→Cuda Core 完整链路 + 调用 PTX-EMU warp)
- 2026-08-19 ADR-X.16 v0.5 redo 设计已沉淀 8 项决策,可作为 MVP 决策依据
- 2026-08-19 PTX-EMU `WarpContext::execute_warp_instruction` 已存在(本地验证 `include/ptxsim/warp_context.h:62`)
- 2026-08-19 UsrLinuxEmu ADR-090 v2 §D3.3 dGPU 接入规范已就位(`docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md`)

---

## What Changes

### 1. 新建文件(Per Stage)

| Stage | 文件 | 用途 |
|-------|------|------|
| **S1** | `external/PTX-EMU` (git submodule) | PTX-EMU 源码依赖 |
| **S1** | `include/tlm/gpu/ptx_emu_submodule_mvp.hh` | Adapter 接口(仅前向声明) |
| **S1** | `src/tlm/gpu/ptx_emu_submodule_mvp.cc` | **唯一** include PTX-EMU 头的 .cc(编译防火墙) |
| **S1** | `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `.cc` | **新概念** — Cuda Core adapter(双路径 dispatch) |
| **S1** | `test/test_ptx_emu_submodule_mvp.cc` | 8 ABI + 编译防火墙测试 |
| **S1** | `test/test_cuda_core_adapter_mvp.cc` | 黑盒 dispatch 测试 |
| **S2** | `include/tlm/gpu/dgpu_board_mvp.hh` + `.cc` | DGpuBoardTLM(5 组件包装) |
| **S2** | `include/tlm/gpu/doorbell_mvp.hh` + `.cc` | SQ tail register + strong-order write |
| **S2** | `include/tlm/gpu/submission_queue_mvp.hh` + `.cc` | per-stream FIFO consumer |
| **S2** | `include/tlm/gpu/completion_ring_mvp.hh` + `.cc` | push + host_notify 重设计 |
| **S2** | `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc` | IOCTL 0x27/0x28/0x29 stub |
| **S2** | `configs/dgpu_board_v1_mvp.json.in` | JSON config + CMake configure_file |
| **S2** | `test/test_dgpu_board_v1_mvp_from_config.cc` | 6 SECTION E2E |
| **S2** | `test/test_usrlxemu_ioctl_stub.cc` | 3 IOCTL stub 端到端 |
| **S3** | `include/tlm/gpu/command_processor_mvp.hh` + `.cc` | 5-state FSM(IDLE/FETCH/DECODE/DISPATCH/COMPLETE) |
| **S3** | `include/tlm/gpu/pm4_decoder_mvp.hh` + `.cc` | Mesa-style TYPE3 + 4 MVP opcodes |
| **S3** | `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc` | TMU Glue(32 slot MVP + dep chain + LIFO) |
| **S3** | `test/test_command_processor_mvp.cc` | 5 FSM transition 测试 |
| **S3** | `test/test_pm4_decoder_mvp.cc` + `test_pm4_decoder_mvp_integration.cc` | Mesa bit field + 4 opcode |
| **S3** | `test/test_tmu_dispatch_processor_mvp.cc` | submit / on_complete / LIFO / dep chain |
| **S3** | `test/test_cuda_core_adapter_mvp_whitebox.cc` | per-warp cycle 跟踪(S3 启用) |
| **S4** | `include/tlm/gpu/scoreboard_tlm_v05_mvp.hh` | per-warp cycle tracking 升级 |
| **S4** | `include/tlm/gpu/pipeline_tlm_v05_mvp.hh` | latency issue API 升级 |
| **S4** | `test/test_scoreboard_v05_mvp.cc` | per-warp cycle 单元测试 |
| **S4** | `test/test_pipeline_v05_mvp.cc` | latency issue 测试 |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `.gitmodules` | 加 `external/PTX-EMU` submodule 入口 |
| `CMakeLists.txt` | `add_subdirectory(external/PTX-EMU)` + `PTX_EMU_BUILD_TESTS=OFF` + `PTX_EMU_BUILD_SHARED=OFF` + `-fvisibility=hidden` |
| `include/chstream_register.hh` | 加 `REGISTER_CHSTREAM(DGpuBoardTLM)` + `UsrLinuxEmuIoctlStub` |
| `tests/CMakeLists.txt` | 新增 v0.5 MVP 测试目标 |
| `scripts/CMakeLists.txt` | 纳入 `validate_topology` CMake target |
| `scripts/test/docs_sync_check.sh` | 路径同步检查 |

### 3. 配套文档(已存在)

- [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md) — 8 项决策锁定
- [`docs/soc_arch/modules/dgpu-board.md`](../../../docs/soc_arch/modules/dgpu-board.md) — DGpuBoardTLM 模块设计
- [`docs/soc_arch/modules/command-processor.md`](../../../docs/soc_arch/modules/command-processor.md) — CommandProcessor
- [`docs/soc_arch/modules/pm4-decoder.md`](../../../docs/soc_arch/modules/pm4-decoder.md) — Pm4Decoder
- [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../../../docs/soc_arch/modules/tmu-dispatch-processor.md) — TmuDispatchProcessor
- [`docs/soc_arch/modules/cuda-core-adapter.md`](../../../docs/soc_arch/modules/cuda-core-adapter.md) — CudaCoreAdapter(新概念)
- [`docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../../../docs/soc_arch/modules/ptx-emu-submodule-mvp.md) — PtxEmuSubmoduleMVP
- [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md) — 阶段化路线图

### 4. **不**修改文件(沿用)

- `include/tlm/gpu/dgpu_bar.hh` + `.cc`(v0.4 已实施,沿用)
- `include/cudart/abi_guards.h`(HSK-6 P0-1 已完成,17 条 static_assert 保持)
- `include/tlm/gpu/scoreboard_tlm.hh`(Legacy,S4 创建 `*_v05_mvp.hh` 升级版)
- `include/tlm/gpu/pipeline_tlm.hh`(Legacy,S4 创建 `*_v05_mvp.hh` 升级版)
- `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`(D1-Full 路径,Phase 1 已 ship 保留)

### 5. **不**归档文件(本 change 不动)

- `docs-archived/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md`(Status Update 审计追溯)
- `docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md`(v0.5 redo 决策依据)
- `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`(D1-Full 决策)

---

## Acceptance Gate

| Gate | Owner | 状态 | 周 |
|------|-------|:---:|-----|
| **G-MVP-1** S1 submodule + 内部链路跑通 | CppTLM | ⏳ | W1-2 |
| **G-MVP-2** S2 DGpuBoard + Doorbell + SQ/CQ + JSON | CppTLM | ⏳ | W3-4 |
| **G-MVP-3** S3 CP + PM4 + TMU + warp 调用 | CppTLM | ⏳ | W5-6 |
| **G-MVP-4** S4 Production + validate_topology | CppTLM | ⏳ | W7-10 |
| **G-MVP-5** UsrLinuxEmu IOCTL 0x27/0x28 真实路径 | CppTLM | ⏳ | W4 |
| **G-MVP-6** 编译防火墙验证(`git grep "include.*ptxsim"` 仅命中 `ptx_emu_submodule_mvp.cc`)| CppTLM | ⏳ | W2 |
| **G-MVP-7** `v0.5.0-MVP` tag | CppTLM | ⏳ | W10 |
| **G-MVP-8** HSK-7/8 公告发出(可选,若需 PTX-EMU 新 API)| CppTLM + PTX-EMU | ⏳ | W1 |

**最终验收(MVP 完成时)**:
- [ ] G-MVP-1 ~ G-MVP-8 全部 ✅
- [ ] 全部 ≥880 测试 PASS(v0.4.1 baseline 850 + MVP 新增 ≥50)
- [ ] 编译防火墙验证 PASS
- [ ] docs 同步检查 PASS(`scripts/test/docs_sync_check.sh --strict`)
- [ ] 跨仓协调:PTX-EMU submodule pin 已 bump
- [ ] `git tag -a v0.5.0-MVP -m "..."`

---

## Cross-Repo Coordination

| 仓 | 跟踪载体 | MVP 状态 |
|----|---------|:---:|
| **PTX-EMU** | submodule pin + (可选)`stepOneWarpInstruction` 新 API | 🟡 W1 submodule + W6 可选白盒 |
| **UsrLinuxEmu** | IOCTL 0x27/0x28 stub 模式 + HSK-8 协议(后续)| 🟡 S2 stub(W3-4)|
| **TaskRunner** | `cuModuleLoadData` 解析(已有)| ✅ 已 ship |
| **CppTLM** | 本 openspec change + ADR-X.17 + openspec/changes/2026-08-19-cpptlm-v05-mvp/ | 🔵 实施中 |

**跨仓 commit 顺序**(per ADR-035 §R5.1):
```
[1] PTX-EMU submodule pin → CppTLM S1 (git submodule add)
[2] CppTLM S2 stub 模式 → 不依赖 UsrLinuxEmu 编译
[3] CppTLM S3 可选白盒 → 需 PTX-EMU 接受 stepOneWarpInstruction
[4] CppTLM S4 v0.5.0-MVP tag → user sign-off
[5] (可选) v0.5 完整版 12 周 + HSK-8 UsrLinuxEmu 联调
```

---

## Migration (Phase 化详细)

完整 S1-S4 操作步骤 + commit 模板,见:
- [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md) — 阶段化路线图
- ADR-X.17 §7.1-7.2 acceptance gates + 跨仓协调

---

## References

### 上游决策

- [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md) — 8 项决策锁定(Proposed)
- [`docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md`](../../../docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md) — v0.5 redo 8 项决策(被本 MVP 切片)
- [`docs-archived/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md`](../../../docs-archived/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) — v3.0-extract(被反转)
- [`openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`](../../cpptlm-d1-pipeline-scoreboard/) — D1-Full 路径(Phase 1 已 ship)
- [`docs/soc_arch/adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md`](../../../docs/soc_arch/adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md) — CP 简化背景

### 调研基础

- [`docs/research/CP/`](../../../docs/research/CP/) — 12 Command Processor 专利
- [`docs/research/TMU/`](../../../docs/research/TMU/) — 16 TMU 专利 + Hopper WSDU
- [`docs/research/PCIe/`](../../../docs/research/PCIe/) — PCIe 保序 write
- [`docs-archived/superpowers/specs/2026-08-19-dgpu-v4-design.md`](../../../docs-archived/superpowers/specs/2026-08-19-dgpu-v4-design.md) — TMU Catalog v3 + TMU Functional Catalog

### 仓 HEAD 锚点

```
PTX-EMU     @87820951 (per ADR-X.16 §7.5,含 HSK-4 + step machinery)
CppTLM      @74c2fd1 (v0.5 proposal commit,v0.5 起点)
UsrLinuxEmu @37a91b6 (ADR-090 v2,dGPU 接入规范)
```

### 跨仓 commit 顺序(per ADR-035 §R5.1)

```
PTX-EMU 新 API PR → PTX-EMU submodule pin → CppTLM S1 → S2 stub → S3 白盒可选 → S4 tag
```

---

**起草**: Sisyphus (2026-08-19,基于 ADR-X.17 8 项决策 + 5 个模块设计 + 路线图)
**Owner**: CppTLM Team
**配合**: [`proposal.md`](../proposal.md) · [`tasks.md`](../tasks.md) · [`specs/`](../specs/)
**状态**: 📋 Proposed — 等 W1 S1 启动后开始实施