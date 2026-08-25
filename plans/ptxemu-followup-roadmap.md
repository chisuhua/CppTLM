# CppTLM × PTX-EMU 后续任务 Roadmap

> **文档编号**: CPPTLM-PTXEMU-FOLLOWUP
> **版本**: v1.0
> **创建日期**: 2026-08-25
> **基于**: HSK-8 Phase 2 完成 (`530bd6ca` → `0e0ba7ad`) + v0.5 MVP S1 已 archive
> **状态**: 🟡 规划中 - 等待 owner 决策 (D1-Full vs v0.5 MVP 优先级)
> **关联 commits**:
> - CppTLM: `505333b` (build_ptx_emu.sh) / `a07c9e1` / `a2d1a75` (submodule bumps)
> - PTX-EMU: `0e0ba7ad` (12/12 IPtxEmuDevice delegation complete + ANTLR4 fix)
> **关联 ADRs**:
> - `docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md` (v0.5 MVP 切片设计)

---

## 0. 当前进度摘要 (2026-08-25)

### 已完成 ✅ (本 roadmap 创建前)

| Task | Commit | 说明 |
|------|--------|------|
| PTX-EMU submodule bump `fcdad151` → `530bd6ca` | `beb3db8` | docs-only archive sync |
| PTX-EMU submodule bump `530bd6ca` → `5d3cea8e` | `a07c9e1` | HSK-8 Phase 2.2/2.3 device-api-delegation |
| PTX-EMU submodule bump `5d3cea8e` → `0e0ba7ad` | `a2d1a75` | 12/12 delegation + ANTLR4 path fix |
| `scripts/build/build_ptx_emu.sh` v1.0 | `505333b` | ON 路径构建脚本 |
| `scripts/build/build_ptx_emu.sh` v1.1 | (uncommitted, in `a2d1a75`) | 移除 ANTLR4 symlink workaround |
| AGENTS.md COMMANDS 节同步 | `505333b` | dGPU/APU SoC 默认路径 |
| `.gitignore` 修复 | `505333b` | `build/` → `/build/` (避免递归匹配) |
| `docs_sync_check.sh` 修正 | `505333b` | VIRTUAL_PATHS 补充 memory_bridge 已删 |

### PTX-EMU 端能力 (12/12 IPtxEmuDevice delegation 已就绪)

| 能力 | 实现状态 | CppTLM 端是否使用 |
|------|---------|-----------------|
| `initialize/shutdown` | ✅ 真实 lifecycle | ✅ PtxEmuSubmoduleMVP::init/shutdown |
| `exe_once/sm_exe_once` | ✅ 真实 delegation | ✅ KernelLaunchTLM 调用 |
| `warp_exe_once` | ✅ 真实 delegation (Phase 2.2.1) | ⚠️ PtxEmuSubmoduleMVP 包装但未在 CP/PM4 路径使用 |
| `set_scoreboard/active_mask/next_pc` | ✅ 真实 delegation (Phase 2.2) | ⚠️ 包装但未注入 ScoreboardTLM |
| `get_thread_state/get_warp_status` | ✅ 真实 delegation (Phase 2.2.1) | ⚠️ 包装但未在 timing adapter 使用 |
| `attach_timing` | ✅ namespace bridge (Phase 2.3) | ⚠️ 包装但未连接到 Scoreboard/Pipeline/TensorCore |
| `is_finished` | ✅ 真实 lifecycle | ✅ KernelLaunchTLM tick loop |
| `read_register_*` / `read_global_*` | ❌ PTX-EMU stub | ❌ PtxEmuSubmoduleMVP 提供 u32/u64 接口但 stub 返回 false |

### CppTLM 端测试现状

| 路径 | 测试结果 | 备注 |
|------|---------|------|
| OFF (default) | 817/817 PASS, 18864 assertions | 默认 cmake build |
| ON (PTX-EMU) | 817/817 PASS, 18864 assertions | `./scripts/build/build_ptx_emu.sh` |
| perf tests | 1 case flaky (`test_latency_tlm_perf.cc:97`) | 机器负载敏感, 待 P4 修复 |

### 已 commit 的 Submodule 版本

```
HEAD = a2d1a75 (chore(submodule): bump external/PTX-EMU 5d3cea8e → 0e0ba7ad)
├── a07c9e1 (chore(submodule): bump external/PTX-EMU 530bd6ca → 5d3cea8e)
├── 505333b (feat(scripts): add build_ptx_emu.sh + sync AGENTS.md)
├── beb3db8 (chore(submodule): bump external/PTX-EMU fcdad151 → 530bd6ca)
└── 12b9e0f (docs(openspec): archive cpptlm-ptxemu-public-device-api change)
```

---

## 1. Phase 划分总览

```
Phase I: P0 — 立刻可做 (1-2 周)
  ├─ 任务1: 激活 attach_timing 链路
  └─ 任务 2: PtxEmuSubmoduleMVP stub 增强
Phase II: P1 — D1-Full 收尾 (3-4 周)
  ├─ 4.7 kernel_launch_ptx_integration test
  ├─ G-D2/G-D3/G-D8
  └─ 4.9 + G-D5 microbenchmark vs gpgpu-sim
Phase III: P2 — v0.5 MVP S2 (2-3 周)
  └─ DGpuBoardTLM + 4 IOCTL stub + SubmitQueue WDU
Phase IV: P3 — v0.5 MVP S3 (4-6 周)
  └─ NVIDIA method packet + TMU 反压 + 全链路 E2E + v0.5.0-MVP tag
Phase V: P4 — 维护性任务 (并行)
  └─ perf flaky fix + flag 清理 + 文档同步
```

---

## 2. Phase I — P0 立刻可做 (1-2 周)

### 背景

PTX-EMU 端 12/12 IPtxEmuDevice 方法已全部 delegation (从 stub → 真实实现)。CppTLM 端 `PtxEmuSubmoduleMVP` facade 已包装所有 12 个方法,但部分包装**未实际连接到下游模块**。这造成 PTX-EMU 端能力"就绪但未使用"的状态。

### Task 1: 激活 `attach_timing` 链路 🔴

**目标**: `attach_timing` 接收的 `IScoreboard*` 等转换为 `ScoreboardTLM/PipelineTLM/TensorCoreTLM*` 并实际注入 SMContext。

| ID | 任务 | 依赖 | 工作量 |
|----|------|------|--------|
| 1.1 | 阅读 `cuda_core_adapter_mvp.{hh,cc}` 现状 | — | 0.5d |
| 1.2 | 设计 `attach_timing` → ScoreboardTLM/PipelineTLM/TensorCoreTLM 桥接 | 1.1 | 0.5d |
| 1.3 | 实现桥接 (参照 D1-Full 1.4.1 已实现 ScoreboardTLM 的 IScoreboard 继承) | 1.2 | 1d |
| 1.4 | 单元测试: timing 注入后 ScoreboardTLM query 行为符合预期 | 1.3 | 0.5d |
| 1.5 | 集成测试: `attach_timing` 后 KernelLaunchTLM::tick() 行为变化 | 1.3 | 1d |

**Acceptance**:
- [ ] `cuda_core_adapter_mvp.cc::attach_timing()` 调用 `scoreboard_tlm_.set_*` 等真实方法
- [ ] `test_cuda_core_adapter_timing.cc` 新增 ≥5 个测试覆盖注入路径
- [ ] 全部 ON/OFF 测试 817/817 PASS 不变

### Task 2: PtxEmuSubmoduleMVP stub 方法增强 🔴

**目标**: 在 PTX-EMU 端 `read_register_*` / `read_global_*` 还是 stub 期间, CppTLM 端 facade 提供**占位但语义正确**的实现,避免调用方因 stub 返回 false 而崩溃。

| ID | 任务 | 依赖 | 工作量 |
|----|------|------|--------|
| 2.1 | 审计所有 `read_register_*` / `read_global_*` 调用方 (若有) | — | 0.5d |
| 2.2 | 在 `ptx_emu_submodule_mvp.cc` 添加占位实现 (返回 `false` 但记录 metric + log) | 2.1 | 1d |
| 2.3 | 单元测试: stub 调用行为契约 (调用方知道 "返回 false = 未实现") | 2.2 | 0.5d |
| 2.4 | 文档化: stub vs delegation 矩阵 (与 `include/ptxemu/AGENTS.md` 同步) | 2.3 | 0.5d |

**Acceptance**:
- [ ] `ptx_emu_submodule_mvp.cc::read_register_u32` 等实现 "失败日志 + metric + 返回 false"
- [ ] `test_ptx_emu_submodule_stub.cc` 新增 ≥4 个测试覆盖 stub 契约
- [ ] AGENTS.md §PTX-EMU 集成 或 include/ptxemu/AGENTS.md 反映 stub vs delegation 矩阵

---

## 3. Phase II — P1 D1-Full 收尾 (3-4 周)

### 背景

`openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/` 当前 **44/51 tasks done**。剩余 7 个任务均依赖 **Phase 4 Wave 2** (gpgpu-sim 参考数据 + 真实 SMContext::exe_once 集成测试)。

### 任务清单

| ID | 任务 | 状态 | 依赖 | 工作量 |
|----|------|------|------|--------|
| 4.7 | `test/test_kernel_launch_ptx_integration.cc` (G-D2/G-D3/G-D8 覆盖) | ⏳ 未做 | P0 任务 1 | 2-3d |
| G-D2 | `set_blocked_cycles_for_active()` warp 内活跃线程正确延迟 | ⏳ 未做 | 4.7 | 1d |
| G-D3 | `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle | ⏳ 未做 | 4.7 | 1d |
| G-D8 | exe_once stall → re-schedule → release → re-issue 完整循环 | ⏳ 未做 | 4.7 | 2d |
| 4.9 | `test/python/test_gpgpu_sim_comparison.py` (G-D5 5 类 microbenchmark) | ⏳ 未做 | 4.7 + gpgpu-sim 参考数据 | 3-5d |
| G-D5 | 5 类 microbenchmark vs gpgpu-sim ±15% | ⏳ 未做 | 4.9 | 5d |
| 4.6 | Latency 精确对齐 (placeholder→gpgpu-sim + `is_placeholder_=false`) | ⏳ Deferred | 4.9 | 3-5d |

**Gating 决策点**: G-D5 vs gpgpu-sim ±15% 需要 gpgpu-sim 模拟器输出。如果没有,需要先准备 (1-2 天额外)。

### 路径 1: 快速收尾 (2-3 周, 不含 G-D5)

如果优先考虑 D1-Full archive 速度:
1. P0 任务 1 (activate attach_timing) → 解锁 4.7
2. 4.7 kernel_launch_ptx_integration test → 解锁 G-D2/G-D3/G-D8
3. G-D2 + G-D3 + G-D8 合并 PR
4. D1-Full archive (50/51 tasks done, 仅 G-D5 + 4.9 残留)

### 路径 2: 完整收尾 (4-5 周, 含 G-D5)

如果优先考虑准确度:
1. P0 任务 1
2. 4.7 + G-D2 + G-D3 + G-D8
3. 准备 gpgpu-sim 参考数据 (1-2d)
4. 4.9 + G-D5 (1 周)
5. 4.6 Latency 精确对齐 (1 周)
6. D1-Full archive (51/51 tasks done)

---

## 4. Phase III — P2 v0.5 MVP S2 (2-3 周)

### 背景

`openspec/changes/2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/` 当前 **0/45 tasks**。S1 已 archive (PtxEmuSubmoduleMVP + CudaCoreAdapter 已完成),S2 编译依赖解锁。

### 核心任务 (来自 s2 proposal)

| 模块 | 文件 | 复杂度 | 依赖 |
|------|------|--------|------|
| DGpuBoardTLM (8 组件包装) | `dgpu_board_mvp.{hh,cc}` | 中 | S1 + P0 任务 1 |
| Doorbell (SQ tail register) | `doorbell_mvp.{hh,cc}` | 低 | 无 |
| SubmitQueue WDU 分发 | `submit_queue_mvp.{hh,cc}` | 高 | NVIDIA Hopper 蓝图 |
| CompletionRing (push + host_notify) | `completion_ring_mvp.{hh,cc}` | 中 | 无 |
| 4 IOCTL stub (0x27/0x28/0x29/0x01) | `usrlxemu_ioctl_stub_mvp.{hh,cc}` | 中 | Phase F-H.3 决策 |
| CP 5-state FSM 骨架 | `command_processor_mvp.{hh,cc}` | 高 | PM4 解析器骨架 |
| PM4 解析器接口 | `pm4_decoder_mvp.hh` | 低 | 无 |
| PM4 数据类型 | `pm4_types_mvp.hh` | 低 | 无 |
| **测试** (10+ 个) | `test/test_dgpu_board_*.cc` | 中 | 各模块 |

### 路径

1. 创建 8 个新模块文件 (per s2 proposal §1)
2. 注册到 `src/CMakeLists.txt` (per s1 经验)
3. 编写 10+ 单元测试
4. 端到端测试: UsrLinuxEmu IOCTL 0x27 → DGpuBoard → SubmitQueue → PtxEmuSubmoduleMVP mock
5. S2 archive

---

## 5. Phase IV — P3 v0.5 MVP S3 (4-6 周)

### 背景

`openspec/changes/2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/` 当前 **0/38 tasks**。S2 archive 后解锁 S3。

### 核心任务

| 模块 | 复杂度 | 依赖 |
|------|--------|------|
| CP fetch NVIDIA method packet | 高 | S2 (CP 骨架) |
| TMU 反压停 fetch (替代 LIFO) | 中 | S2 (TMU 组件) |
| 全链路 E2E: cuLaunchKernel → kernel → cuStreamSynchronize | 高 | S2 + CudaCore |
| `v0.5.0-MVP` git tag | 低 | 全部测试 PASS |

### 路径

1. NVIDIA method packet 集成 (对齐 UsrLinuxEmu `unpackPm4Header`)
2. TMU 反压逻辑实现
3. 全链路 E2E 测试 (`test/test_e2e_v05_mvp.cc`)
4. 性能基准 + v0.5.0-MVP tag

---

## 6. Phase V — P4 维护性任务 (可与上述并行)

| 任务 | 优先级 | 工作量 | 阻塞 |
|------|--------|--------|------|
| `test_latency_tlm_perf.cc:97` flaky 阈值 (`< 1000.0` → `< 1500.0`) | 低 | 0.5d | 无 |
| `--f12b-ld` flag 清理 (`src/main.cpp:41,75-76`) | 低 | 0.5d | 无 |
| `include/cudart/AGENTS.md` 文档描述差异 (虽已加 VIRTUAL_PATHS) | 低 | 1d | 无 |
| AGENTS.md "ANTLR4 symlink workaround" 注释清理 | 低 | 0.5d | 无 |
| HSK-9 监控: PTX-EMU 端 `PTXEMU_API_VERSION` 变化 | 监控 | — | 外部 |

---

## 7. 推荐执行顺序

### 路径 A: 快速产出 (2 周内可见成果)

```
Week 1: P0 任务1 (activate attach_timing) + 任务 2 (stub 增强)
Week 2: D1-Full 4.7 + G-D2/G-D3/G-D8 → 启动 S2 准备
```

### 路径 B: 完整 v0.5 MVP (4-6 周)

```
Week 1-2: P0 任务 1+2
Week 3-4: v0.5 MVP S2 (10-15d)
Week 5-8: v0.5 MVP S3 (20-30d)
Week 9-10: D1-Full 4.9 + G-D5 microbenchmark vs gpgpu-sim
```

### 路径 C: 平衡 (推荐)

```
Week 1: P0 任务 1+2 + D1-Full 4.7 + G-D2/G-D3/G-D8 准备
Week 2-3: v0.5 MVP S2 (DGpuBoardTLM 板卡 + 4 IOCTL)
Week 4-6: v0.5 MVP S3 (CP+PM4+TMU)
Week 7: v0.5.0-MVP tag
并行: 4.9 + G-D5 在 Week 2-7 期间完成 (gpgpu-sim 数据并行准备)
```

---

## 8. 决策点 (待 owner 确认)

### D1-Full vs v0.5 MVP 优先级

| 选项 | 说明 | 影响 |
|------|------|------|
| A. D1-Full 优先 | 先收尾 D1-Full (51/51) | 4 周 archive, 期间 v0.5 MVP 停滞 |
| B. v0.5 MVP 优先 | 优先 S2+S3 (v0.5.0-MVP tag) | 6 周收官, D1-Full 残留 G-D5 待补 |
| C. 平衡 (推荐) | 见路径 C | 7 周收官, 两者兼顾 |

### G-D5 vs gpgpu-sim 数据

| 选项 | 说明 |
|------|------|
| 已有数据 | 直接用 4.9 + G-D5 实现 |
| 无数据 | 准备 gpgpu-sim 模拟数据 (1-2d) 或 DEFER G-D5 到 Phase II 后 |

### AGENTS.md "ANTLR4 symlink workaround" 注释

| 选项 | 说明 |
|------|------|
| 立即清理 | 假设 PTX-EMU ≥ 2148e15c (本仓库当前 0e0ba7ad 满足) |
| 保留 | 兼容旧版 PTX-EMU 用户 |
| 软告警 | build_ptx_emu.sh v1.1 已实现 (软检测 PTX-EMU 版本) |

---

## 9. 关联资源

### OpenSpec 活跃 changes

- `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/` (44/51 tasks)
- `openspec/changes/2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/` (0/45 tasks)
- `openspec/changes/2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/` (0/38 tasks)

### 已 archive changes

- `openspec/changes/archive/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/` (PtxEmuSubmoduleMVP + CudaCoreAdapter 已交付)
- `openspec/changes/archive/cpptlm-d1-p1-pipeline-scoreboard/` (旧版,被新提案取代)

### 构建脚本

- `scripts/build/build.sh` (默认 OFF 路径)
- `scripts/build/build_ptx_emu.sh` v1.1 (ON 路径, dGPU/APU SoC 默认)
- `./scripts/build/build_ptx_emu.sh` 自动检测 PTX-EMU ≥ 2148e15c 跳过 ANTLR4 symlink

### 测试

- OFF: 817/817 PASS, 18864 assertions
- ON: 817/817 PASS, 18864 assertions
- 1 个 flaky perf test (`test_latency_tlm_perf.cc:97`)

### 文档

- `docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md` (v0.5 MVP 切片设计)
- `docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md` (HSK-8 协议)
- `docs/architecture/01-hybrid-architecture-v2.1.md` (v2.1 架构)

---

## 10. 验收标准 (本 roadmap 自身)

完成判定 (本 roadmap 持续维护):

- [x] PTX-EMU 端能力清单完成 (12/12)
- [x] CppTLM 端 facade 包装完成 (12/12)
- [ ] P0 任务 1 完成 (activate attach_timing)
- [ ] P0 任务 2 完成 (stub 增强)
- [ ] D1-Full 51/51 archive
- [ ] v0.5 MVP S2 archive
- [ ] v0.5 MVP S3 + v0.5.0-MVP tag

**下次更新**: 每个 Phase 完成后更新本文档, 记录实际时间 vs 估算。