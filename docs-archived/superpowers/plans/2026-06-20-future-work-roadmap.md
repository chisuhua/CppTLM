# 未来工作 Roadmap (P2+) — CppTLM

**Status**: Active Planning · **Date**: 2026-06-24 (refresh) · **Author**: Sisyphus · **Branch**: main
**Scope**: 完整索引 P0 + P1 + P1.5 + Phase A + Phase 8 之后的所有未完成工作, 按优先级/工期/依赖排序, 给执行者一个清晰的"接下来做什么"清单。

> **2026-06-24 Refresh (gpu_soc 启动)**: 6 轮 brainstorming 完成 gpu_soc 架构目标定义, 签发 ADR-NV-01 (`3aa810b`), spec (`801f8ea`), 主 plan (`3993129`)。**新增 Phase 8 路径**（与 Phase 7 apu_soc 并行）：3 个 OpenSpec change + 1 个 per-phase plan (8.A, commit `f7e67bc`)。8.B/8.C plan 待用户审查 8.A 后生成。

---

## 0. 当前状态摘要

| 阶段 | Commit | 状态 | 测试基线 |
|------|--------|:---:|---------|
| **P0** 全套修复 | `fb56cc3` `5abba12` `4964619` `5a964c5` `9746272` `788b207` | ✅ Done | 684/684 |
| **P1** ApuSoC::incorporate_parent | `04399c8` | ✅ Done | 690/690 (+6) |
| **P1.5** GPU cu_template 完整传播 | `e8c2a97` | ✅ Done | 690/690 (peer_count 3→16+) |
| **format cleanup (P1)** | `c3bdb45` | ✅ Done | — |
| **Phase A** (F1/F2/F3/F5/F7/F8/F10/F11) | 8 commits (见 §7) | ✅ Done | 690 → 703/703 (+13) |
| **gpu_soc 架构定义** (2026-06-24) | `801f8ea` `3993129` `3aa810b` `f7e67bc` | ✅ Done | 703/703 (无破坏) |
| **Phase 8.A 基础设施** | `e8280fe` | ✅ Archived (8 tasks + Oracle + 归档) | 703 → 764/764 (+61) |

**E2E**: `[SUCCESS]` · **Format**: ✅ clean · **docs_sync_check**: ✅ 0 missing · **Origin/main**: 22 commits ahead

**P0+P1+P1.5+Phase A+gpu_soc 架构定义 合计**: 21 commits, ~2400 LOC 生产代码 + ~5300 LOC 待写（Phase 8）

**测试基线验证** (single source of truth):
```bash
./build/bin/cpptlm_tests --reporter compact
# 期望: "All tests passed (N assertions in 764 test cases)"
# 当前 verified: 15547 assertions in 764 test cases (2026-07-02, Phase 8.A 完成)
# 历史基线: 755/755 (2026-06-30, F12a + Phase 8.A Tasks 5-7) · 703/703 (2026-06-23, Phase A 后)
# 注: AGENTS.md 历史值 "659/659" 已不准确, 本 roadmap 是 single source of truth
```

---

## 0.5 依赖图 + 总工期

### 总工期估算 (Phase A + Phase 8.A + F12a 已完成, 当前活跃 12 项)

> **2026-07-03 更新**: Phase 8.A 已归档 (764/764), F12a (4 GPU 核心模块) 已通过 Phase 8.A Tasks 5/7 落地。当前活跃任务: **Phase 7 path (4 项) + Phase 8 path (15 项) + Python Lib (2 项) + F12b-LD (1 项) = 12 项**。
> **注意**: F12 主体已完成, 剩余 F12b (PTX-EMU LD_PRELOAD 集成) 为外部依赖阻塞项。

| 等级 | Task | 工期 (工作日) | 累计 |
|:---:|------|:---:|:---:|
| 🔴 | F4 Phase 7.C 6×6 state table | 10 | 10 |
| 🔴 | Phase 8.B Task 9 (ScoreboardTLM) | 0.5 | 10.5 |
| 🔴 | Phase 8.B Task 10-14 (5 核心模块) | 5w | — |
| 🔴 | F12b-LD (PTX-EMU LD_PRELOAD 集成) | 2-3w | 阻塞 |
| 🟡 | Phase 8.B Task 15 (集成+对照) | 1w | — |
| 🟡 | Phase 8.B Task 16 (6 docs + M2) | 1w | — |
| 🟡 | F6 blueprint 升级 | 1.5 | — |
| 🟡 | F13 Phase 7.D TCC Bridge | 10 | 阻塞 F12b-LD |
| 🟡 | Python Lib Phase 0 (C++ 统计注册) | 1-2 | — |
| 🟡 | Python Lib Phase 1 (纯 Python 配置层) | 2-3 | — |
| 🟢 | Phase 8.C Task 17-25 (4 高级 + 3 Python 子包) | 3w | 锁 8.B |
| 🟢 | F14 Phase 7.E Multi-CU+NoC | 7 | 锁 F4 + F12b |
| 🟢 | F15 Phase 7.F Full APU Demo | 5 | 锁 F13 + F14 |
| 🟢 | F9 多 xbar 实例 | 1.5 | 锁 F4 |

**总工期估算**: ~50 工作日 (~10 周单人, 或 5 周 4 人并行关键路径)

### 依赖图 (2026-06-23 重画, Phase A 已落地)

```
       ┌──────────── F12 (Phase 7.B 三类, 10d) ─────────────┐
       │              (F6 F13 F14 都依赖)                     │
       │                                                     ↓
       ├─→ F6 (蓝图升级, 1.5d) ─→ F13 (Phase 7.D TCC, 10d) ─┐
       │                                                      ↓
       └──────────────────→ F4 (Phase 7.C state table, 10d) ─┴─→ F14 (Phase 7.E Multi-CU, 7d) ─┐
                                  (与 F12 并行)                                            ↓
                                                                                     F15 (Phase 7.F Demo, 5d) ─┐
                                                                                                              ↓
                                                                                                  F9 (多 xbar, 1.5d)  [F4 后]
```

**关键路径**: F12 → F13 → F14 → F15 (~32 工作日, 阻塞主线 Phase 7 推进)
**并行加速**: F12 ∥ F4 (10d 并行, 不串行) 节省 10 工作日
**快速收益 (按需)**: F9 (1.5d, F4 后) ≈ 1.5 工作日

---

## 1. 完整待办索引 (P2+)

> **2026-07-03 更新**: Phase A 8 项 (F1/F2/F3/F5/F7/F8/F10/F11) 已落地, 移至 §7; F12a (4 GPU 核心模块) 已通过 Phase 8.A Tasks 5/7 落地; Phase 8.A 已归档。当前活跃 12 项 (Phase 7 path 4 项 + Phase 8 path 9 任务 + Python Lib 2 项 + F12b-LD 1 项)。
> **优先级语义**: 🔴 = 最高风险/最高紧迫性 (与 `roadmap.md` Phase 7 风险分级对齐) · 🟡 = 正常推进 · 🟢 = 按需/cosmetic
> **依赖说明**: 全部依赖均已展开; 隐藏依赖标注在对应 task 详细规范节。

### 1.1 🔴 高优先级 (1-2 周内启动 / 最高风险)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **F4** | **Phase 7.C: CoherentXBarTLM 6×6 state table 改造** (取代 write-through 透传, 引入 MOESIF CoherenceState + 状态机) | ADR-SOC-01 §2 | 1-2 周 | **高** | 无 |
| **8.B-T9** | **Phase 8.B Task 9: ScoreboardTLM ≥12 entries** (raw hazard 检测, 13th allocate 失败) | `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` | 0.5d | 中 | Phase 8.A ✅ |
| **8.B-T10** | **Phase 8.B Task 10: WarpSchedulerTLM CGGTY 5-warp 阈值** (round-robin + priority 队列) | 同上 | 1d | 中 | 8.B-T9 |
| **8.B-T11** | **Phase 8.B Task 11: PipelineTLM 5+V 抽象** (分数 cycle 查表) | 同上 | 1d | 中 | 8.B-T9 |
| **8.B-T12** | **Phase 8.B Task 12: TensorCoreTLM 6 精度统一管线** (FP4/FP6/FP8/FP16/BF16/TF32) | 同上 | 1d | 中 | 8.B-T9 |
| **8.B-T13** | **Phase 8.B Task 13: L2PartitionTLM multi-slice** (近/远分区, per GPC) | 同上 | 0.5d | 中 | 8.B-T9 |
| **8.B-T14** | **Phase 8.B Task 14: SubCoreTLM black-box pipe 封装** | 同上 | 1d | 中 | 8.B-T10-13 |
| **F12b** | **F12b-LD: PTX-EMU LD_PRELOAD 集成** (`CppTLMBridge` + `libcpptlm_cudart.so`) | `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md` | 2-3w | 中-高 | PTX-EMU 团队对齐 |

### 1.2 🟡 中优先级 (1-2 月内启动)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **8.B-T15** | **Phase 8.B Task 15: 5 类 microbenchmark + gpgpu-sim 区间对照** (`test_gpu_soc_phase8b.cc` + `test_gpgpu_sim_comparison.py`) | `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` | 1w | 高 | 8.B-T14 |
| **8.B-T16** | **Phase 8.B Task 16: 6 微架构 doc + M2 性能 + docs_sync** | 同上 | 1w | 中 | 8.B-T15 |
| **F6** | **compute_unit_v1.json 蓝图升级** (Phase 7.B 接入 GpuComputeUnitTLM, 扩 ScalarCache + VectorRegFile + Wavefront) | Handoff PENDING §6 | 1-2 天 | 中-高 | F12a ✅ (无锁) |
| **F13** | **Phase 7.D: TCC Bridge + 内存层次** (DualPortStreamAdapter write coalescing + MemoryTLM hbm_mode) | `roadmap.md` Phase 7.D + ADR-SOC-04 | 1-2 周 | 高 | F12a ✅ (基础) + F12b-LD (TCC 路径) |
| **PyLib-0** | **Python Library Phase 0: C++ 统计注册** (修复 cpptlm.metrics 暴露给 Python) | `docs/superpowers/plans/cpptlm-python-library-plan.md` | 1-2d | 低 | 编译环境 |
| **PyLib-1** | **Python Library Phase 1: 纯 Python 配置层** (无 C++ 改动) | 同上 | 2-3d | 低 | Python 环境 |

### 1.3 🟢 低优先级 (按需启动)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **8.C-T17** | **Phase 8.C Task 17-20: 4 高级模块单元测试** (Tcc/Tma/Dsm/PowerModel) | `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/` | 2w | 中 | 8.B-T16 |
| **8.C-T21** | **Phase 8.C Task 21: cpptlm.nvidia 拓扑生成 + 4 SKU 预置** | 同上 | 0.5w | 中 | 8.B-T16 |
| **8.C-T22** | **Phase 8.C Task 22: cpptlm.gpu_workload + 5 pattern 构造器** | 同上 | 0.5w | 中 | 8.B-T16 |
| **8.C-T23** | **Phase 8.C Task 23: cpptlm.gpu_soc 顶层 simulate + report** | 同上 | 0.5w | 中 | 8.C-T21/22 |
| **8.C-T24** | **Phase 8.C Task 24: apu_soc 集成测试 + 完整验证报告** | 同上 | 0.5w | 高 | 8.C-T17-23 |
| **8.C-T25** | **Phase 8.C Task 25: 4 微架构 doc + roadmap + AGENTS.md 更新** | 同上 | 0.5w | 低 | 8.C-T24 |
| **F14** | **Phase 7.E: Multi-CU + NoC** (ComputeUnitTLM 数组模式 + BidirectionalPortAdapter + apu_soc_v1 完整 GPU 接入) | `roadmap.md` Phase 7.E | 1-2 周 | 中-高 | F12a ✅ + F4 |
| **F15** | **Phase 7.F: Full APU SoC Demo** (`apu_full_soc.json` + `test_apu_soc.py` + 222/222 Python 端到端) | `roadmap.md` Phase 7.F | 1 周 | 中 | F13, F14 |
| **F9** | **多 xbar 实例支持** (当前单 xbar 设计, Phase 7.C+ 多 xbar 场景) | P1 spec §7 风险表 | 1-2 天 | 中 | F4 |

---

## 2. 推荐执行顺序 (按 ROI 排序)

> **2026-06-23 更新**: Phase A (1-2 周 quick wins) 8 项已全部完成, 见 §7。当前规划从 Phase B 起点开始, 即 F12 与 F4 并行启动。

### Phase A: ✅ 已完成 (1-2 周 quick wins) — 见 §7

> Phase A 全部 8 项 (F1/F2/F3/F5/F7/F8/F10/F11) 于 2026-06-22 ~ 2026-06-23 期间落地, 8 commits, +13 tests (690 → 703/703)。

### Phase B: 2-3 周架构演进 + 风险任务 (F4 + F6 + F12a 后续 + F12b-LD 阻塞)

```
[F12a ✅ (Phase 8.A 落地) ∥ F4 (10d) ∥ Phase 8.B Task 9-14 (5w)]
   ↓
[F6 (1.5d) 蓝图升级] [PyLib-0 (1-2d)] [PyLib-1 (2-3d)]
   ↓
[F12b-LD 阻塞中 (PTX-EMU 团队对齐)]
```

| 步骤 | 依赖 | commit pattern |
|------|------|----------------|
| F4: Phase 7.C state table | 无 (但需新 brainstorming cycle) | `feat(tlm): CoherentXBarTLM 6×6 state table (per ADR-SOC-01)` |
| F6: blueprint upgrade | F12a ✅ (无锁) | `feat(templates): upgrade compute_unit_v1.json to GpuComputeUnitTLM (per ADR-SOC-02/03)` |
| 8.B-T9-14: Phase 8.B 核心模块 | Phase 8.A ✅ | `feat(tlm/gpu): ScoreboardTLM / WarpScheduler / Pipeline / TensorCore / L2Partition / SubCore` |
| PyLib-0/1: Python Library | 编译/Python 环境 | `feat(cpptlm): metrics Python exposure + pure-Python config layer` |
| F12b-LD: PTX-EMU 集成 | 外部 PTX-EMU 团队对齐 | `feat(tlm): CppTLMBridge + libcpptlm_cudart.so` |

**Phase B 总工期**: 2-3 周 (F4 + Phase 8.B Task 9-14 并行), 1 个新 brainstorming cycle (F4)

### Phase C: 2-3 周 Phase 7.D-E 推进 + Phase 8.B 集成对照

```
F13 (10d) [依赖 F12a ✅ + F12b-LD] → F14 (7d) [依赖 F12a ✅ + F4]
   ↓
8.B-T15 (1w) [依赖 8.B-T14] → 8.B-T16 (1w) [依赖 8.B-T15]
   ↓
[TCC Bridge + 内存层次 + Multi-CU + NoC + BidirectionalPortAdapter + gpgpu-sim 对照 + 6 docs]
```

| 步骤 | 依赖 | commit pattern |
|------|------|----------------|
| F13: Phase 7.D TCC | F12a ✅ (基础) + F12b-LD (TCC 路径) | `feat(tlm): TCC Bridge + MemoryTLM hbm_mode (per ADR-SOC-04)` |
| F14: Phase 7.E Multi-CU | F12a ✅ + F4 | `feat(tlm): ComputeUnitTLM 数组模式 + BidirectionalPortAdapter` |
| 8.B-T15: 集成+对照 | 8.B-T14 | `test(gpu_soc): 5 类 microbenchmark + gpgpu-sim ±15% (Phase 8.B Task 15)` |
| 8.B-T16: 6 docs + M2 | 8.B-T15 | `docs(gpu_soc): 6 microarch docs + M2 perf + docs_sync (Phase 8.B Task 16)` |

**Phase C 总工期**: 2-3 周, 4-5 commits

### Phase D: 集成演示 + 按需扩展 (Phase 7.F 收官)

```
F15 (5d) [依赖 F13 + F14] → F9 (1.5d) [依赖 F4]
   ↓
[Full APU Demo + 多 xbar]
```

| 步骤 | 依赖 | commit pattern |
|------|------|----------------|
| F15: Phase 7.F Demo | F13 + F14 | `feat(demo): apu_full_soc.json + test_apu_soc.py` |
| F9: 多 xbar | F4 (CoherenceDomain) | `feat(simmodule): 多 xbar 实例支持 (P1 spec §7 风险表)` |

**Phase D 总工期**: 1-2 周, 2 commits

### Phase E: gpu_soc 独立 SoC 路径 (与 Phase 7 并行, 2026-06-24 新增)

> **来源**: 6 轮 brainstorming + 调研 gpgpu-sim SM_120 paper + 本地 notes `04_Knowledge/D01-gpu-architecture/`
> **架构目标**: 工业性能建模 (phase-accurate, 借鉴 gpgpu-sim 不集成)
> **完整 spec**: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` (`801f8ea`)
> **主 plan**: `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` (`3993129`)
> **ADR**: `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` (`3aa810b`)

```
[8.A (4w) 基础设施] → [8.B (6w) 核心仿真] → [8.C (3w) 高级特性 + apu_soc 集成] → M3
   ↓
[每个阶段后: Oracle 审查 + OpenSpec 归档]
```

| 阶段 | OpenSpec Change | per-phase Plan | 周 | 任务数 | 验收点 |
|:---:|------|------|:---:|:---:|------|
| **8.A** 基础设施 | `2026-06-24-gpu-soc-phase8a-infra` | `2026-06-24-gpu-soc-phase8a.md` (`f7e67bc`) | 4 | 8 + 2 (Oracle+归档) | M1: 1 SM × 1M < 5s, apu_soc 兼容 |
| **8.B** 核心仿真 | `2026-06-24-gpu-soc-phase8b-core` | `2026-06-24-gpu-soc-phase8b.md` (`33991d6`) | 6 | 8 + 2 (Oracle+归档) | M2: 5 类 microbenchmark, gpgpu-sim ±15% |
| **8.C** 高级特性 | `2026-06-24-gpu-soc-phase8c-advanced` | `2026-06-24-gpu-soc-phase8c.md` (`33991d6`) | 3 | 9 + 2 (Oracle+归档) | M3: 完整验证, apu_soc 集成 |
| **总** | 3 changes | 3 plans ✅ 全部就绪 | **13** | **25 + 6** | |

**8.A 实际完成状态**:
- Commit: `e8280fe` (archive) · 前置实施 commits: `b8bd411`, `6410ea9`, `d164497`, `840ecb7`, `9789ca0`
- 测试基线: 764/764 (15547 assertions)
- M1 验收: 1 SM × 1M cycles < 5s, `[gpu]` 75 cases + `[phase7]` + apu_soc 全绿
- OpenSpec change 已归档至 `openspec/changes/archive/2026-06-24-gpu-soc-phase8a-infra/`

**关键设计原则**:
- 与 apu_soc 共享 GpuCluster/GpcCluster/TpcCluster/ComputeCluster（通过 `GpuClusterSharedInterface` 抽象层）
- 复用 F12 三类（GpuComputeUnitTLM/VectorRegFileTLM/WavefrontTLM）
- 14 个新模块（3650 LOC impl + 650 tests + 800 Python = ~5100 LOC）
- 验证：5 类场景（GEMM/FlashAttn/vector_add/stencil/sparse）vs gpgpu-sim 带宽 ±15%, 延迟 ±20%

**执行策略**:
- **8.A 已完成并归档**（`e8280fe`）
- **8.B / 8.C per-phase plans 已就绪**（`33991d6`）
- 执行顺序：✅ 8.A → Oracle 审查 → 归档 → 8.B → Oracle 审查 → 归档 → 8.C → Oracle 审查 → 归档 → Phase 8 收官
- 每个阶段独立归档：Oracle 批准后 `git mv openspec/changes/.../ openspec/changes/archive/.../`

**Phase E 总工期**: 13 周 (单人关键路径) / 3 周 (4 人并行)

### 整体规划 (2026-07-02 重算)

**B → C → D 全程**: ~9 周 (单人, 关键路径 32 工作日) / ~3 周 (4 人并行)
**Phase 8**: 8.A 已归档, 8.B → 8.C 仍需 **~9 周** (13 周总计划 - 4 周 8.A = 9 周)

---

## 3. 各任务详细规范 (仅活跃 6 项, Phase A 8 项 + Phase 8.A 已归档至 §7)

> **2026-07-02 更新**: Phase 8.A 已完成并归档, 从活跃任务中移除。当前活跃 6 项: F4 / F6 / F9 / F12 / F13 / F14 / F15 中的 F12 已拆分为 F12a (Done) + F12b-LD (blocked), 因此实际活跃为 F4 / F6 / F9 / F13 / F14 / F15。
> F1 / F2 / F3 / F5 / F7 / F8 / F10 / F11 详细节已合并到 §7 已完成历史。

---

### F4: Phase 7.C: CoherentXBarTLM 6×6 state table 改造

**来源**: `docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md` §2 (分步走策略 §2 决定)

**当前状态** (P0): `snoop_broadcast` write-through 透传 (不下场做 coherence 决策)

**目标** (Phase 7.C):
- CacheTLM 引入 `CoherenceState` 标签 (`I` / `S` / `E` / `M` / `O` / `F`)
- CoherentXBarTLM 加 `CoherenceDomain` 集成 + 6×6 state transition switch 表
- snoop 时先读 cache state 再决定是否广播
- 与 gem5 `MOESI_AMD_Base` 协议对齐 (但用 C++ `switch` 而非 slicc DSL)

**预估**:
- 新增 ~500-800 LOC
- 涉及 `CoherenceDomain` 集成 (P0 留 stub), `CacheTLM` 内部状态机, `CoherentXBarTLM` 决策逻辑
- 需新 brainstorming cycle (架构重大变更)
- 测试: 50+ new TEST_CASEs (state transitions 6×6 = 36 cases + snoop scenarios)

**风险**: 中-高（架构变更, 可能影响现有 690 tests）

**前置**: 需先与 ADR-SOC-01 重新对齐确认

---

### F6: compute_unit_v1.json 蓝图升级

**来源**: Handoff PENDING §6

**当前蓝图** (`configs/templates/compute_unit_v1.json`):
- `scalar_cache` (CacheTLM)
- `l1_cache` (CacheTLM)
- 共 2 cache

**升级目标** (Phase 7.B 接入 `GpuComputeUnitTLM`):
- `scalar_cache` → 保留 (CacheTLM 仍可用)
- `vector_regfile` (新 VectorRegFileTLM)
- `wavefront` (新 WavefrontTLM, 含 coalescing)
- `l1_cache` 升级为 `l1_data_cache` + `l1_inst_cache` (Harvard)

**依赖**:
- **F12** (Phase 7.B 三类实现: `GpuComputeUnitTLM` / `VectorRegFileTLM` / `WavefrontTLM`) 是 F6 的实际前置
- ~~`F4` (coherence state table) 不依赖 — F4 是 crossbar 侧, F6 是 CU 蓝图侧, **正交**~~

**风险**: 中（需先实现 3 个新 TLM 类, 见 F12）

---

### F9: 多 xbar 实例支持

**来源**: P1 spec §7 风险表

**当前限制**: ApuSoC::incorporate_parent 找单个 xbar（按 `coherent_xbar_name` 配置）

**目标**: 支持 ApuSoC 多个 xbar, 每个 xbar 收集自己的 peer cache 子集

**依赖**:
- F4 (CoherenceDomain 集成) 必须先做
- 需要 new JSON schema: `xbar_instances: [{name, peer_caches: [...]}, ...]`

**风险**: 中-高（架构变更）

---

### F12: Phase 7.B 核心 — GpuComputeUnitTLM / VectorRegFileTLM / WavefrontTLM / MinimalWarpScheduler 四类实现 (**Option A 扩展**)

**来源**: `roadmap.md` Phase 7.B + ADR-SOC-02 (CU 粒度) + ADR-SOC-03 (Wavefront coalescing) + **Phase 8.A Task 5/7 依赖门** (Metis 2026-06-28 决策)

**当前状态** (2026-06-30): ✅ **F12a 已完成** (standalone 4 类 + tests + integration), 755/755 tests pass
- 下一步: **F12b-LD** PTX-EMU LD_PRELOAD 集成 (见 `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`)
- `GPUTLM v0` 黑盒发起器 (单端口 Initiator, 周期发出 ComputeReqBundle)
- `ComputeReqBundle` / `ComputeRespBundle` 类型已定义 (4 GPU 维度字段)

**目标** (Phase 7.B, **Option A 扩展后**):
- `GpuComputeUnitTLM` (新增, ~300-400 LOC,含内部 `SubCoreSlot[4]`) — 真实 CU 行为: 接收 wavefront → minimal scheduler 派发 → 4 sub-core slot 并行 → SIMT 黑盒执行 → 写回
- `VectorRegFileTLM` (新增, ~150 LOC) — vector register file 抽象, 支持 read/write/bank conflict (简化)
- `WavefrontTLM` (新增, ~200 LOC) — wavefront 调度 + `coalescing_factor` 合并 (8.A 用常数,8.B 升级地址模式)
- **`MinimalWarpSchedulerTLM` (新增, ~300-400 LOC, Option A 必加)** — round-robin 派发,CU 内部 4 sub-core slot,8.B 升级到 greedy/CGGTY。无此模块 → 8.A Task 5/7 端到端测试 `requests_completed > 0` 无法触发 (循环依赖)
  - 配套测试: `test/test_gpu_compute_unit_tlm.cc` + `test/test_vector_regfile_tlm.cc` + `test/test_wavefront_tlm.cc` + `test/test_minimal_warp_scheduler_tlm.cc` + `test/test_gpu_compute_unit_integration.cc` (~16 TEST_CASEs)
  - F12a 实施计划: `docs/superpowers/plans/2026-06-30-f12a-gpu-core-modules.md`
  - F12b-LD 集成设计 (PTX-EMU `LD_PRELOAD`): `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`
  - 跨类集成测试: 4 wavefront → minimal scheduler 派发 → 4 sub-core slot → tick → `requests_completed == 4`

**依赖**:
- 无外部依赖 (新增类, 不改现有 TLM 接口)
- 需新 brainstorming cycle (4 个新类的设计,Option A 增量 1 个)
- F6 (compute_unit 蓝图) 依赖此 F12
- **Phase 8.A Tasks 5/7 依赖此 F12** (含 MinimalWarpScheduler,否则循环依赖)
- Phase 8.B (WarpScheduler 升级) 与 F12 MinimalWarpScheduler 接口对齐

**预估**:
- 新增 **~350 LOC** (F12a 实际: 4 个 .hh + 4 个 .cc + 5 个 test + 1 集成测试), 16 new TEST_CASEs
- 工期: **1 天** (F12a 实际耗时 1 天; F12b-LD 另行评估)
- 状态: **F12a 已完成**, commit 序列 `e2aa9f3` → `27bc204` → `8b0a649` → `af13e55` → `7ff067e` → `b5ece52`

**风险**: 低 (F12a 已完成; F12b-LD 集成风险: 中, 需处理 PTX-EMU 全局 singleton + `LD_PRELOAD` 工作流)

**前置**: ✅ F12a 已完成; F12b-LD 需先完成 OpenSpec change 提案 + PTX-EMU 侧 `libcpptlm_cudart.so` 实现

---

### F13: Phase 7.D — TCC Bridge + 内存层次

**来源**: `roadmap.md` Phase 7.D + ADR-SOC-04 (HSAPP 极简化)

**目标**:
- `TccTLM` (新增, ~200 LOC) — Translation Cache Coherent Bridge, 真实 GCN 架构参考
- `MemoryTLM::hbm_mode` 扩展 (新增, ~50 LOC 修改) — HBM 带宽模型 + 通道并行
- `DualPortStreamAdapter` 写合并 (write coalescing, ~100 LOC) — 多 request 合并为单 MemoryTLM 写

**依赖**:
- **F12** (CU 类需先实现, F13 是 CU 写回的 downstream)
- 无 brainstorming (架构已在 ADR-SOC-04 中确定)

**预估**:
- 新增 ~350-400 LOC
- 20+ new TEST_CASEs (TCC snoop + write coalescing correctness + HBM 带宽)
- 工期: 1-2 周

**风险**: 中-高 (TCC 状态机 + HBM 模型)

---

### F14: Phase 7.E — Multi-CU + NoC

**来源**: `roadmap.md` Phase 7.E

**目标**:
- `ComputeUnitTLM` 数组模式 (扩展 F12 的 CU) — 1 TPC 容纳 N 个 CU
- `BidirectionalPortAdapter` 真实多 CU 总线 (新增, ~150 LOC)
- `apu_soc_v1.json` 完整 GPU 接入 (4GPC × 2TPC × 2CU = 16 CU, 替换当前的 16 简化 xbar)
- 配套 E2E 测试: GPU 多 CU 并发执行, 验证 NoC 路由 + bandwidth saturation

**依赖**:
- F12 (CU 基础)
- F4 (CoherenceDomain 集成, 多 CU 共享 L2)
- 无 brainstorming (在 F12 基础上扩展)

**预估**:
- 新增 ~400-500 LOC
- 15+ new TEST_CASEs
- 工期: 1-2 周

**风险**: 中 (多 CU 同步 + 真实 NoC 流量)

---

### F15: Phase 7.F — Full APU SoC Demo

**来源**: `roadmap.md` Phase 7.F

**目标**:
- `apu_full_soc.json` (新增) — 完整 APU 拓扑: 2 CPU + 16 CU + 2x2 NoC + DDR4 + 简化 PCI
- `test_apu_soc.py` (新增) — Python E2E 验证脚本: 启动仿真, 注入 CPU + GPU 混合工作负载, 验证 cache coherence + snoop 正确性
- 222 → 230+ Python tests pass
- 演示 demo: `examples/demo_e2e_apu_soc.py` (端到端 CPU+GPU 工作负载)

**依赖**:
- F13 (TCC Bridge)
- F14 (Multi-CU 接入)
- 无 brainstorming (整合已有组件)

**预估**:
- 新增 ~300 LOC (JSON + Python)
- 5+ new Python TEST_CASEs
- 工期: 1 周

**风险**: 低-中 (整合, 主要风险在 F13/F14)

---

---

## 4. 实施模板（每个 task 推荐结构）

每个新 task 推荐按以下 structure (与现有 P0/P1/P1.5 流程一致):

1. **Brainstorming** (新设计 task 必需) — brainstorming skill
2. **Spec** 写 `docs/superpowers/specs/YYYY-MM-DD-<topic>-design.md` — source of truth
3. **Plan** 写 `docs/superpowers/plans/YYYY-MM-DD-<topic>.md` — bite-sized tasks
4. **Implementation** — subagent-driven-development skill
5. **Final review** — verify + clean debug + format + commit + push
6. **Update** 本 roadmap 移除 completed task

---

## 5. ADR 待写清单 (2026-06-23 更新)

| # | ADR | 状态 | 触发条件 |
|---|-----|:---:|---------|
| ~~INC-01~~ | ~~incorporate_parent late-binding semantics~~ | ✅ **已写** (`docs/adr/ADR-INC-01-incorporate-parent-late-binding.md`, commit `25c0051`) | F3 已完成 |
| 7B-01 | Phase 7.B GpuComputeUnitTLM + VectorRegFileTLM + WavefrontTLM 三类设计 | 🔴 待写 (F12) | F12 启动前 (新 brainstorming 必需) |
| 7C-01 | Phase 7.C 6×6 state table 决策 | 🔴 待写 (F4) | F4 启动前 (新 brainstorming 必需) |
| 7D-01 | Phase 7.D TCC Bridge + HBM 内存模型决策 | 🟡 待写 (F13) | F13 启动前 |
| 7E-01 | Phase 7.E Multi-CU + NoC 架构 | 🟡 待写 (F14) | F14 启动前 |
| 7F-01 | Phase 7.F Full APU Demo 集成策略 | 🟢 待写 (F15) | F15 启动前 |
| ~~LIB-01~~ | ~~cpptlm.library Python API 决策~~ | ✅ **已追溯补 ADR** (`docs/adr/ADR-LIB-01-cpptlm-library-python-higher-cluster-factories.md`, commit `140fffd`) | F5 已完成 |
| GPU-01 | GpuComputeUnitTLM + 蓝图升级决策 | 🟡 待写 (F6) | F6 启动前 |
| ~~METRIC-01~~ | ~~CPUTLM/CacheTLM/MemoryTLM 性能 metric 收集框架~~ | ✅ **已追溯补 ADR** (`docs/adr/ADR-METRIC-01-cputlm-cache-memory-telemetry.md`, commit `66d9674`) | F10 已完成 |
| **NV-01** | gpu_soc 独立 SoC 仿真目标 (与 apu_soc 并行; 14 新模块 + 3 阶段 + 共享 GpuCluster 子模块 + 借鉴 gpgpu-sim 不集成) | ✅ **已签发** (`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`, commit `3aa810b`) | Phase 8.A 启动前 (已就绪) |
| MULTI-XBAR-01 | 多 xbar 实例架构 | 🟢 待写 (F9) | F9 启动前 |

新 ADR 应在 `docs/adr/ADR-<PREFIX>-<NN>-<topic>.md` 落地, 索引在 `docs/adr/README.md` 更新。

---

## 6. 维护规则

1. **状态更新**: 每完成 1 task, 从本 roadmap 移除 (移至"已完成历史"区)
2. **新增发现**: 在 PENDING 阶段发现的 follow-up task, 加到对应优先级表
3. **优先级回顾**: 每季度 review 一次, 根据业务需要调整优先级
4. **依赖追踪**: 任务之间依赖必须明确标出, 避免后续冲突
5. **历史归档**: 完成 6+ 月的 task 可归档到 `docs-archived/future-work/`

---

## 7. 已完成历史 (从本 roadmap 移除后归档)

| 任务 | Commit | 完成日期 | 备注 |
|------|--------|----------|------|
| P0 全套修复 | `fb56cc3` `5abba12` `4964619` `5a964c5` `9746272` `788b207` | 2026-06-19 | D.1 + CoherentXBarTLM + dead code |
| P1 ApuSoC::incorporate_parent | `04399c8` | 2026-06-19 | 4 phase + 1 bug fix + 6 tests |
| P1.5 GPU cu_template 完整传播 | `e8c2a97` | 2026-06-19 | wrap_template_as_module + TpcCluster |
| format cleanup (P1) | `c3bdb45` | 2026-06-19 | clang-format 4 P1 files |
| **Phase A — F2** pre-commit clang-format hook | `1fc5461` | 2026-06-22 | `.pre-commit-config.yaml` + CONTRIBUTING guide |
| **Phase A — F3** ADR-INC-01 incorporate_parent late-binding | `25c0051` | 2026-06-22 | ADR 记录 P1 1A+2A+3A 决策 |
| **Phase A — F11** E2E 回归守护 | `2ac4e58` | 2026-06-22 | 删 test_phase6_integration.cc:153-155 直接驱动 + 新增 E2E TEST_CASE |
| **Phase A — F1** P2 5 项残留 △ 补测 | `d927fff` | 2026-06-22 | TpcCluster cu 计数 / routing="FIFO" / l1_size·l2_size / channel_size / metric 结构 |
| **Phase A — F8** cosmetic cleanup | `1a087f5` | 2026-06-22 | top_xbar→xbar, v2.4.1, clang-format pass |
| **Phase A — F5** cpptlm.library Python 高级工厂 | `140fffd` | 2026-06-23 | `cpu_nested_cluster` + `memory_cluster_hierarchical` + `gpu_topology` |
| **Phase A — F7** params.ports schema 校验 | `84af502` | 2026-06-23 | LINT007: `validate params.ports against class max_ports` |
| **Phase A — F10** CPUTLM 性能 metric 收集 | `66d9674` | 2026-06-23 | CPUTLM telemetry + F10 integration test |
| Phase A doc hygiene (Metis review) | `98c4a80` | 2026-06-22 | close P1 doc hygiene fixes (Metis review action items) |
| Phase 7.A 计划归档 | `a0c4736` | 2026-06-22 | archive Phase 7.A plan (completed 2026-06-11, GPU [gpu] tag GREEN) |
| 28 stale plans 归档 | `92002e5` | 2026-06-22 | archive 28 stale plans & specs to docs-archived/ |
| openspec 2026-06-22-refine 归档 | `f3c6067` | 2026-06-22 | archive 2026-06-22-refine-future-work-roadmap |
| Phase A close (本 refresh) | (pending) | 2026-06-23 | roadmap refresh: 690→703/703 baseline, Phase A 8 项已落 §7 |

---

**Roadmap 维护**: CppTLM 开发团队
**下次 review**: 2026-09-20 (Q3 末)
**触发 update 条件**: 任何 task 完成 / 新 task 发现 / 优先级调整