# cpptlm-v05-mvp-s1-ptxemu-integration: PTX-EMU 深度集成基础设施

> **状态**: 📋 Proposed — 2026-08-21 · **日期**: 2026-08-21 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D2/D3
> **取代**: [`openspec/changes/archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/`](../../archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/) (原单 change,2026-08-21 拆为 3 个)
> **目标**: W1-2 交付 PTX-EMU 深度集成基础设施(2 个模块,~12 个测试)

---

## Why

原 [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) 锁定 MVP 切片,但单 change 20+ 文件 + 15 测试,6-10 周,评审 blast radius 大。
Per Oracle 审查(2026-08-20,ses_fe179d02)建议按依赖关系拆 3 个独立 change:
- **s1**(本 change): W1-2 基础设施,**可独立交付**
- **s2**: W3-4 DGpuBoardTLM 板卡,依赖 s1
- **s3**: W5-6 数据面 + S4 tag,依赖 s1+s2

s1 价值独立:
- PTX functional facade 是 MVP 本身的核心目标之一(用户原话:"MVP 要用 CppTLM 搭建接近真实 CudaCore 的模型,指令执行依赖 PTX-EMU")
- SM 微架构探索器(4 timing 模块集成)可独立测试 + 演示 PTX-EMU 集成
- 即使 s2/s3 失败,s1 成果可独立 archive + 复用

**触发事件**:
- 2026-08-13 user commit `87820951` 沉淀 PTX-EMU HSK-4 step machinery
- 2026-08-20 Phase F-H.1/F-H.6 决定深度集成(不调 ABI 黑盒)
- 2026-08-20 Phase I.1/I.2 functional/timing 分离(gpgpu-sim 分层)
- 2026-08-20 Oracle ses_fe179d02 建议拆分
- 2026-08-21 本 change 提交

---

## What Changes

### 1. 新建文件

| 文件 | 用途 |
|------|------|
| `external/PTX-EMU` (git submodule,per DP1=B) | PTX-EMU 源码依赖 |
| `include/tlm/gpu/ptx_emu_submodule_mvp.hh` | **PTX functional facade** 接口(per Phase I.1) |
| `src/tlm/gpu/ptx_emu_submodule_mvp.cc` | **唯一** include PTX-EMU 头的 .cc(编译防火墙) |
| `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `.cc` | **SM 微架构探索器**(per Phase I.2) |
| `test/test_ptx_emu_facade_decode.cc` | PTXIR 格式校验 + magic/version 异常 |
| `test/test_ptx_emu_facade_arith.cc` | ADD/SUB/MUL/DIV 寄存器结果 |
| `test/test_ptx_emu_facade_memory.cc` | LD/ST 共享/全局内存 |
| `test/test_ptx_emu_facade_branch.cc` | SIMT 分支/active mask |
| `test/test_ptx_emu_facade_barrier.cc` | `bar.sync` 多 warp 同步 |
| `test/test_ptx_emu_facade_state.cc` | 状态读写 round-trip |
| `test/test_cuda_core_adapter_mvp_tick.cc` | per-tick cycle 推进 + WarpScheduler 行为 |
| `test/test_cuda_core_adapter_mvp_scoreboard.cc` | RAW hazard + allocate/release 计数 |
| `test/test_cuda_core_adapter_mvp_pipeline.cc` | Pipeline latency 注入 |
| `test/test_cuda_core_adapter_mvp_dispatch.cc` | on_cta_arrival 反压 + resource 管理 |
| `test/test_cuda_core_adapter_mvp_warp_state.cc` | WarpState 镜像正确性(不含 PC) |
| `test/test_cuda_core_adapter_mvp_injection.cc` | 4 timing 模块注入路径 |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `.gitmodules` | 加 `external/PTX-EMU` submodule 入口(per **DP1=B** pin @ `87820951`) |
| `CMakeLists.txt` | `add_subdirectory(external/PTX-EMU)` + `PTX_EMU_BUILD_TESTS=OFF` + `PTX_EMU_BUILD_SHARED=OFF` + `-fvisibility=hidden` |
| `test/CMakeLists.txt` | 加 s1 测试目标(12 个 .cc) |

### 3. **不**修改文件(沿用,per Phase I.2 修订)

- `include/tlm/gpu/scoreboard_tlm.hh`(已存在,**MVP 直接使用**,不创建 `*_v05_mvp.hh`)
- `include/tlm/gpu/pipeline_tlm.hh`(已存在)
- `include/tlm/gpu/tensor_core_tlm.hh`(已存在)
- `include/tlm/gpu/minimal_warp_scheduler_tlm.hh`(已存在)
- `include/cudart/abi_guards.h`(HSK-6 P0-1 已完成,17 条 static_assert 保持)

---

## Acceptance Gate(本 change 独立)

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **s1-G1** submodule + 内部链路跑通 | CppTLM | ⏳ W1-2 | `git submodule status` 显示 `PTX-EMU@87820951` + `cmake --build build` 通过 |
| **s1-G2** PtxEmuSubmoduleMVP functional facade 6 类测试 PASS | CppTLM | ⏳ W1-2 | `ctest -R "test_ptx_emu_facade" --output-on-failure`(6 个文件 PASS) |
| **s1-G3** CudaCoreAdapter timing 6 类测试 PASS | CppTLM | ⏳ W2 | `ctest -R "test_cuda_core_adapter_mvp" --output-on-failure`(6 个文件 PASS) |
| **s1-G4** 编译防火墙验证(per Phase I.1 §1.1)| CppTLM | ⏳ W1-2 | `git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/" -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"` 仅命中 `ptx_emu_submodule_mvp.cc` |
| **s1-G5** functional 调用不增加 cycle 计数(per Phase I.1) | CppTLM | ⏳ W2 | `test_ptx_emu_facade_state.cc` 验证 `sm->get_cycle_count()` 在 `functional_execute_warp()` 前后不变 |

**最终验收(本 change 完成时)**:
- [ ] s1-G1 ~ s1-G5 全部 ✅
- [ ] 12 个测试 PASS(6 functional + 6 timing)
- [ ] 编译防火墙验证 PASS
- [ ] **本 change 可独立 archive**(不依赖 s2/s3)

---

## Cross-Repo Coordination(本 change)

| 仓 | 跟踪载体 | 状态 |
|----|---------|:---:|
| **PTX-EMU** | submodule pin @ `87820951` | 🟢 W1 完成(`be484b1` commit) |
| **UsrLinuxEmu** | 无依赖(本 change 不涉及 IOCTL stub) | ✅ 不涉及 |
| **TaskRunner** | 无依赖 | ✅ 不涉及 |
| **CppTLM** | 本 openspec change + ADR-SOC-06 + 2 个模块设计 | 🔵 实施中 |

**跨仓 commit 顺序**(本 change 子集):
```
[1] git submodule add https://github.com/chisuhua/PTX-EMU.git external/PTX-EMU  ← 已完成 be484b1
[2] CMakeLists.txt 集成 PTX-EMU  ← W1
[3] PtxEmuSubmoduleMVP 实施  ← W1
[4] CudaCoreAdapter 实施  ← W2
[5] 12 个测试 PASS + s1 archive
```

---

## Migration (本 change)

完整 S1 操作步骤 + commit 模板,见 [`tasks.md`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/tasks.md)

---

## References

### 上游决策
- [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) — D2/D3
- [`openspec/changes/archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/`](../../archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/) — 原单 change(已 superseded)

### 模块设计文档
- [`docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../../../docs/soc_arch/modules/ptx-emu-submodule-mvp.md) — PTX functional facade
- [`docs/soc_arch/modules/cuda-core-adapter.md`](../../../docs/soc_arch/modules/cuda-core-adapter.md) — SM 微架构探索器

### 仓 HEAD 锚点
```
PTX-EMU     @87820951 (per ADR-SOC-06 §7.5 + DP1=B)
CppTLM      (本 change 提交时)
```

---

**起草**: Sisyphus (2026-08-21,per Oracle ses_fe179d02 拆分建议)
**Owner**: CppTLM Team
**配合**: [`proposal.md`](../proposal.md) · [`tasks.md`](../tasks.md) · [`specs/`](../specs/)
**状态**: 📋 Proposed — 等 W1 启动后开始实施
