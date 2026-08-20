# cpptlm-v05-mvp: dGPU Board MVP Slice (CP→TMU→Cuda Core → PTX-EMU warp)

> **状态**: 📋 Proposed — 2026-08-19(per Phase J 2026-08-20 对齐)· **日期**: 2026-08-19 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) (Proposed,8 项决策锁定,per Phase I.4 已从 `docs/adr/` 迁回本目录)
> **取代(部分)**:
> - [`docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md`](../../../docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md) (v0.5 redo,12 周完整版,被 MVP 切片取代)
> - [`openspec/changes/archive/2026-08-18-cpptlm-v3-dgpu-extract/`](../../archive/2026-08-18-cpptlm-v3-dgpu-extract/) (v0.4,沿用)
> **目标**: 4 阶段 6-10 周交付 MVP 可运行端到端链路(UsrLinuxEmu IOCTL 0x01 pushbuffer → CppTLM CP→TMU→SQ→CudaCore → PTX-EMU 深度集成 functional/timing 分离)

---

## Why

[`ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)(Proposed 2026-08-19,per Phase J 2026-08-20 对齐)的 8 项决策锁定 MVP 切片(per Phase F-H/I 重构):

| # | 决策 | 取代 |
|---|------|------|
| D1 | MVP 切片 = 4 阶段 6-10 周(Net-new,不替代 12 周完整版)| ADR-X.16 12 周时间线过重 |
| D2 | PTX-EMU warp 调用 = **深度集成 PTX-EMU 内部 C++ 接口**(per Phase F-H.7/I.1,functional/timing 分离)| 原双路径(黑/白)设计被推翻;用户要求"MVP 接近真实 CudaCore 模型" |
| D3 | PTX-EMU 集成 = git submodule + adapter 编译防火墙 | v0.4 黑盒 dlopen 不安全 |
| D4 | UsrLinuxEmu 接入 = IOCTL 0x27 + **0x01 PUSHBUFFER_SUBMIT_BATCH**(0x28 永久 -ENOSYS)| 真实板卡驱动接入 |
| D5 | 模块架构 = **CP→TMU→SQ→CudaCore** 链路(新增 **6 模块**,含 SubmitQueue WDU 分发网络)| Phase 7.A 黑盒链路不仿真真实硬件 |
| D6 | JSON config 驱动 + validate_topology 集成 | 模块配置零散 |
| D7 | CPPTLMBRIDGE_VERSION = 2 永久冻结(per Phase F-H.1,新路径走 C++ 源码契约,不发 C ABI v3) | 不发 C ABI v3 |
| D8 | 接口稳定性(MVP 冻结 8 项,可演进 4 项) | 提前优化接口 |

**触发事件**:
- 2026-08-19 user 提出 MVP 切片需求(可运行 + 接入真实板卡驱动 + CP→TMU→CudaCore 完整链路 + 调用 PTX-EMU warp)
- 2026-08-19 ADR-X.16 v0.5 redo 设计已沉淀 8 项决策,可作为 MVP 决策依据
- 2026-08-19 PTX-EMU `WarpContext::execute_warp_instruction` 已存在(本地验证 `include/ptxsim/warp_context.h:62`)
- 2026-08-19 UsrLinuxEmu ADR-090 v2 §D3.3 dGPU 接入规范已就位(`docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md`)
- **2026-08-20 Phase F-H.7/I.1 架构重定义**:用户要求"MVP 阶段尽量接近最终形态(TMU→CudaCore 之间有分发网络)" + "MVP 接近真实 CudaCore 模型,指令执行依赖 PTX-EMU 项目" → 重新设计为 functional/timing 分离 + 6 模块
- **2026-08-20 Phase I.4 ADR 移动**:ADR-X.17 → ADR-SOC-06(纳入 `docs/soc_arch/adr/` SoC ADR 命名空间)

---

## What Changes

### 1. 新建文件(Per Stage)

| Stage | 文件 | 用途 |
|-------|------|------|
| **S1** | `external/PTX-EMU` (git submodule) | PTX-EMU 源码依赖(per DP1=B pin @ `87820951`) |
| **S1** | `include/tlm/gpu/ptx_emu_submodule_mvp.hh` | **PTX functional facade** 接口(仅前向声明,per Phase I.1) |
| **S1** | `src/tlm/gpu/ptx_emu_submodule_mvp.cc` | **唯一** include PTX-EMU 头的 .cc(编译防火墙) |
| **S1** | `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `.cc` | **新概念** — **SM 微架构探索器**(per Phase I.2 timing model + 4 个 TLM 模块集成) |
| **S1** | `test/test_ptx_emu_facade_*.cc` | 6 类 PTX 指令(arith/memory/branch/barrier/io/misc)功能正确性测试(per Phase I.1) |
| **S1** | `test/test_cuda_core_adapter_mvp_*.cc` | microarchitecture timing 测试:tick/scoreboard/pipeline/injection(per Phase I.2) |
| **S2** | `include/tlm/gpu/dgpu_board_mvp.hh` + `.cc` | DGpuBoardTLM(**6 组件**包装,含 SubmitQueue,per Phase I.2 §1.1) |
| **S2** | `include/tlm/gpu/doorbell_mvp.hh` + `.cc` | SQ tail register + strong-order write |
| **S2** | `include/tlm/gpu/completion_ring_mvp.hh` + `.cc` | push + host_notify 重设计 |
| **S2** | `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc` | IOCTL 0x27/0x28/0x29 + **0x01 PUSHBUFFER_SUBMIT_BATCH** stub(per Phase F-H.3) |
| **S2** | `configs/dgpu_board_v1_mvp.json.in` | JSON config + CMake configure_file |
| **S2** | `test/test_dgpu_board_v1_mvp_from_config.cc` | 6 SECTION E2E(per Phase F-H.3 G-MVP-5 4 IOCTL 端到端) |
| **S2** | `test/test_usrlxemu_ioctl_stub.cc` | **4 IOCTL** stub 端到端(0x27/0x28/0x29/0x01) |
| **S3** | `include/tlm/gpu/command_processor_mvp.hh` + `.cc` | 5-state FSM(IDLE/FETCH/DECODE/DISPATCH/COMPLETE) |
| **S3** | `include/tlm/gpu/pm4_decoder_mvp.hh` + `.cc` | **NVIDIA method packet**(per Phase F-H.3) + 4 method_addr ranges |
| **S3** | `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc` | TMU Glue(32 slot MVP + dep chain + **反压停 fetch**,per Phase F-D.2 H5) |
| **S2** | `include/tlm/gpu/submit_queue_mvp.hh` + `.cc` | **🆕 WDU 分发网络**(per Phase F-H.5,per `docs/research/WDUtoSM/overview.md` NVIDIA Hopper;与 DGpuBoardTLM 6 组件同阶段) |
| **S3** | `test/test_command_processor_mvp.cc` | 5 FSM transition 测试 |
| **S3** | `test/test_pm4_decoder_mvp.cc` + `test_pm4_decoder_mvp_integration.cc` | **NVIDIA method packet** bit field + 4 method_addr range |
| **S3** | `test/test_tmu_dispatch_processor_mvp.cc` | submit / on_complete / 反压停 fetch / dep chain |
| **S2** | `test/test_submit_queue_mvp_*.cc` | 5 单测:route/enqueue/dispatch/complete/concurrent |
| ❌ **S3 删除** | ~~`test_cuda_core_adapter_mvp_whitebox.cc`~~ | **per DP4=C 永久禁用白盒路径**(per Phase I.1 重构) |
| ❌ **S4 删除** | ~~`scoreboard_tlm_v05_mvp.hh` + `pipeline_tlm_v05_mvp.hh` + 2 tests~~ | **MVP 阶段直接使用现有 `scoreboard_tlm.hh` / `pipeline_tlm.hh`**(per Phase I.2,已有模块集成) |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `.gitmodules` | 加 `external/PTX-EMU` submodule 入口 |
| `CMakeLists.txt` | `add_subdirectory(external/PTX-EMU)` + `PTX_EMU_BUILD_TESTS=OFF` + `PTX_EMU_BUILD_SHARED=OFF` + `-fvisibility=hidden` |
| `include/chstream_register.hh` | 加 `REGISTER_CHSTREAM(DGpuBoardTLM)` + `UsrLinuxEmuIoctlStub` |
| `tests/CMakeLists.txt` | 新增 v0.5 MVP 测试目标 |
| `scripts/CMakeLists.txt` | 纳入 `validate_topology` CMake target |
| `scripts/test/docs_sync_check.sh` | 路径同步检查 |

### 3. 配套文档(已存在,per Phase J 2026-08-20 对齐)

- [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) — 8 项决策锁定(per Phase I.4 已从 `docs/adr/` 迁回本目录)
- [`docs/soc_arch/modules/dgpu-board.md`](../../../docs/soc_arch/modules/dgpu-board.md) — DGpuBoardTLM 6 组件包装
- [`docs/soc_arch/modules/command-processor.md`](../../../docs/soc_arch/modules/command-processor.md) — CommandProcessor 5-state FSM(NVIDIA method packet)
- [`docs/soc_arch/modules/pm4-decoder.md`](../../../docs/soc_arch/modules/pm4-decoder.md) — Pm4Decoder(NVIDIA method packet)
- [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../../../docs/soc_arch/modules/tmu-dispatch-processor.md) — TmuDispatchProcessor(反压停 fetch)
- [`docs/soc_arch/modules/submit-queue.md`](../../../docs/soc_arch/modules/submit-queue.md) — **🆕 SubmitQueue(WDU 分发网络)**
- [`docs/soc_arch/modules/cuda-core-adapter.md`](../../../docs/soc_arch/modules/cuda-core-adapter.md) — **SM 微架构探索器**(per Phase I.2 timing model)
- [`docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../../../docs/soc_arch/modules/ptx-emu-submodule-mvp.md) — **PTX functional facade**(per Phase I.1 functional model)
- [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md) — 阶段化路线图

### 4. **不**修改文件(沿用,per Phase J 修订)

- `include/tlm/gpu/dgpu_bar.hh` + `.cc`(v0.4 已实施,沿用)
- `include/cudart/abi_guards.h`(HSK-6 P0-1 已完成,17 条 static_assert 保持)
- `include/tlm/gpu/scoreboard_tlm.hh`(已存在,**MVP 阶段直接使用,不创建 `*_v05_mvp.hh` 升级版**)
- `include/tlm/gpu/pipeline_tlm.hh`(已存在,**MVP 阶段直接使用,不创建 `*_v05_mvp.hh` 升级版**)
- `include/tlm/gpu/tensor_core_tlm.hh`(已存在,供 CudaCoreAdapter::inject_timing_modules 使用,per Phase I.2)
- `include/tlm/gpu/minimal_warp_scheduler_tlm.hh`(已存在,供 CudaCoreAdapter warp 调度使用,per Phase I.2)
- `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`(D1-Full 路径,Phase 1 已 ship 保留)

### 5. **不**归档文件(本 change 不动)

- `docs-archived/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md`(Status Update 审计追溯)
- `docs-archived/adr/ADR-X.16-cpptlm-v05-redo.md`(v0.5 redo 决策依据)
- `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`(D1-Full 决策)

---

## Acceptance Gate

| Gate | Owner | 状态 | 周 |
|------|-------|:---:|-----|
| **G-MVP-1** S1 submodule + 内部链路跑通(PtxEmuSubmoduleMVP facade + CudaCoreAdapter timing)| CppTLM | ⏳ | W1-2 |
| **G-MVP-2** S2 DGpuBoard + Doorbell + CQ + JSON(**6 组件**包装,含 SubmitQueue)| CppTLM | ⏳ | W3-4 |
| **G-MVP-3** S3 CP + PM4(NVIDIA method packet)+ TMU + SubmitQueue + CudaCore 深度集成 | CppTLM | ⏳ | W5-6 |
| **G-MVP-4** S4 Production + validate_topology | CppTLM | ⏳ | W7-10 |
| **G-MVP-5** UsrLinuxEmu IOCTL 0x27/0x28/0x29/0x01 真实路径(per Phase F-H.3 4 IOCTL stub 端到端)| CppTLM | ⏳ | W4 |
| **G-MVP-6** 编译防火墙验证(`git grep "include.*ptxsim\|include.*ptx_ir"` 仅命中 `ptx_emu_submodule_mvp.cc`)| CppTLM | ⏳ | W2 |
| **G-MVP-7** `v0.5.0-MVP` tag | CppTLM | ⏳ | W10 |
| ~~**G-MVP-8** HSK-7/8 公告发出~~ | — | — | **🗑️ 删除(per DP4=C 决定不发出 HSK-7,无跨仓协调需求)** |

**最终验收(MVP 完成时)**:
- [ ] G-MVP-1 ~ G-MVP-8 全部 ✅
- [ ] 全部 ≥790 测试 PASS(v0.4.1 baseline 764 + MVP 新增 ≥50)
- [ ] 编译防火墙验证 PASS
- [ ] docs 同步检查 PASS(`scripts/test/docs_sync_check.sh --strict`)
- [ ] 跨仓协调:PTX-EMU submodule pin 已 bump
- [ ] `git tag -a v0.5.0-MVP -m "..."`

---

## Cross-Repo Coordination

| 仓 | 跟踪载体 | MVP 状态 |
|----|---------|:---:|
| **PTX-EMU** | submodule pin @ `87820951` + **深度集成** 内部 C++ 接口(per Phase I.1) | 🟢 W1 submodule 完成,无 HSK 联署(per DP4=C) |
| **UsrLinuxEmu** | IOCTL 0x27/0x28/0x29/0x01 stub 模式 | 🟡 S2 stub(W3-4)|
| **TaskRunner** | `cuModuleLoadData` 解析(已有)| ✅ 已 ship |
| **CppTLM** | 本 openspec change + ADR-SOC-06 + 6 模块设计 + 阶段化路线图 | 🔵 实施中 |

**跨仓 commit 顺序**(per ADR-035 §R5.1,per Phase J 修订):
```
[1] PTX-EMU submodule pin → CppTLM S1 (git submodule add,完成 be484b1)
[2] CppTLM S2 stub 模式 → 不依赖 UsrLinuxEmu 编译
[3] CppTLM S3 深度集成 PTX-EMU 内部接口 → 不需 PTX-EMU 新增 API
[4] CppTLM S4 v0.5.0-MVP tag → user sign-off
[5] (可选) v0.5 完整版 12 周 + Work Distribution Crossbar 多 SM
```

---

## Migration (Phase 化详细)

完整 S1-S4 操作步骤 + commit 模板,见:
- [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md) — 阶段化路线图
- ADR-SOC-06 §7.1-7.2 acceptance gates + 跨仓协调(per Phase I.4 ADR 移动)

---

## References

### 上游决策

- [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) — 8 项决策锁定(Proposed,per Phase F-H/I 修订)
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
PTX-EMU 新 API PR → PTX-EMU submodule pin → CppTLM S1 → S2 stub → S3 深度集成 PTX-EMU internal → S4 tag
```

---

**起草**: Sisyphus (2026-08-19 初版;2026-08-20 Phase J 对齐:ADR-SOC-06 + functional/timing 分离 + SubmitQueue + DP4=C)
**Owner**: CppTLM Team
**配合**: [`proposal.md`](../proposal.md) · [`tasks.md`](../tasks.md) · [`specs/`](../specs/)
**状态**: 📋 Proposed — 等 W1 S1 启动后开始实施(已完成 2 commit: `be484b1` submodule + `d36613b/5dbaf2b/2c72b7d` Phase I/SubmitQueue 文档)