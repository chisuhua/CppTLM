# CppTLM 实时状态看板

> **最后更新**: 2026-07-02 · **测试基线**: ✅ 755/755 (15517 assertions) · **GPU**: 66 cases
> **配套**: [实施路线图](./README.md) · **更新规则**: 每次任务完成后更新本文件

---

## 🎯 当前下一步任务

| 优先级 | 任务 | 工期 | 入口条件 | 阻塞项 |
|:---:|------|:---:|------|:---:|
| 🔴 P0 | **T5-T7 补完**: GpuClusterSharedInterface + GpuSocTLM + 集成测试 | 1d | F12a ✅, T1-T4 ✅ | 无 |
| 🟡 P1 | Python Lib Phase 0: C++ 统计注册修复 | 编译环境 | 无 |
| 🟡 P1 | F6: compute_unit_v1.json 蓝图升级 | F12a ✅ | 无 |
| 🟡 P1 | Phase 8.A Archive | Oracle APPROVED | 无 |

---

## 📋 Kanban 看板

### 🚀 IN PROGRESS

_无进行中任务_

### ⏳ READY (入口条件满足)

| 任务 | 工期 | 入口条件状态 | 前置任务 |
|------|:---:|------|------|
| Phase 8.A Oracle Full Review | 0.1d | [x] Task 8 完成 | 8.A T8 |
| Phase 8.A Archive | 0.1d | [ ] Oracle APPROVED | Oracle Full |
| Python Lib Phase 0 (Stats) | 1-2d | [x] 编译环境 [x] ChStreamModuleBase 可改 | — |
| Python Lib Phase 1 (Config) | 2-3d | [x] Python 环境 | — |
| F6 蓝图升级 | 1-2d | [x] F12a ✅ | — |

### 🔴 BLOCKED (入口条件不满足)

| 任务 | 工期 | 缺失的入口条件 | 阻塞原因 |
|------|:---:|------|------|
| F4 Brainstorming (state table) | 2d | [ ] Phase 8.A 归档 (降低上下文切换) | 建议等 8.A 完成后启动 |
| F4 Spec | 1d | [ ] Brainstorming 完成 | — |
| F12b-LD 实施 | 2-3w | [ ] PTX-EMU 团队确认 [ ] CMake PoC [ ] 参考 CUDA 程序选定 | PTX-EMU 侧未对齐 |
| Phase 8.B (6 核心模块) | 6w | [ ] Phase 8.A 归档 | — |
| F13 Phase 7.D TCC Bridge | 1-2w | [ ] F6 蓝图升级 (可选, 加速用) | F12a 已满足 |
| Python Lib Phase 2 (Runner) | 2-3d | [ ] Python Lib P0 完成 | — |

### 🔮 FUTURE (下一阶段)

| 任务 | 工期 | 依赖链 |
|------|:---:|------|
| F14 Phase 7.E Multi-CU + NoC | 1-2w | F12a ✅ + F4 |
| F15 Phase 7.F Full APU Demo | 1w | F13 + F14 |
| F9 多 xbar 支持 | 1-2d | F4 |
| Phase 8.C (高级 + Python) | 3w | 8.B |
| Python Lib Phase 3 (Viz) | 2-3d | P0 + P2 |

### ✅ COMPLETED

| 任务 | 完成日期 | Commit | 测试增量 |
|------|:---:|------|:---:|
| P0 全套修复 | 2026-06-19 | `fb56cc3` 等 6 个 | 659→684 |
| P1 incorporate_parent | 2026-06-19 | `04399c8` | 684→690 (+6) |
| P1.5 cu_template | 2026-06-19 | `e8c2a97` | — |
| Phase A (8 项) | 2026-06-22~23 | 8 commits | 690→703 (+13) |
| gpu_soc 架构定义 | 2026-06-24 | `801f8ea` 等 4 个 | — |
| Phase 8.A T1 SharedMemoryTLM | 2026-06-28 | `6410ea9` | +9 cases [smem] |
| Phase 8.A T2 MemoryClusterTLM | 2026-06-28 | `6410ea9` | +8 cases [memcluster] |
| Phase 8.A T3 GpuMeshNoC | 2026-06-28 | `d164497` | +10 cases [noc] |
| Phase 8.A T4 KernelLaunchTLM | 2026-06-28 | `b8bd411` | +7 cases [kernel_launch] |
| Phase 8.A T1-4 Oracle Review | 2026-06-30 | `71e47ff` | APPROVED |
| F12a SubCoreSlot | 2026-06-30 | `e2aa9f3` | header-only |
| F12a WavefrontTLM | 2026-06-30 | `27bc204` | +3 cases [wavefront] |
| F12a VectorRegFileTLM | 2026-06-30 | `8b0a649` | +4 cases [vector_regfile] |
| F12a MinimalWarpSchedulerTLM | 2026-06-30 | `af13e55` | +5 cases [warp_scheduler] |
| F12a GpuComputeUnitTLM | 2026-06-30 | `7ff067e` | +5 cases [compute_unit] |
| F12a Integration Test | 2026-06-30 | `b5ece52` | +1 case [integration] |
| Phase 8.A T5a-e (GpuClusterSharedInterface) | 2026-07-01 | (pending commit) | apu_soc 兼容 |
| Phase 8.A T6 (GpuSocTLM) | 2026-07-01 | (pending commit) | +[gpu][soc] |
| Phase 8.A T7 (集成测试) | 2026-07-01 | (pending commit) | +[phase8a] |
| Phase 8.A T8 (5 microarch docs + M1) | 2026-07-02 | `9789ca0` | 5 docs, 755/755, docs_sync 0 missing |

---

## 📊 测试基线演化

```
659 ──P0──→ 684 ──P1──→ 690 ──Phase A──→ 703 ──F12a/8.A──→ 755
                                                        (当前)
                                                           │
                                         8.A T8  ──→ 755 (不变, 仅 doc)
                                         8.B     ──→ ~762 (+7 模块测试)
                                         8.C     ──→ ~766 (+4 模块测试)
                                         F4      ──→ ~805 (+50 state table)
                                         F6      ──→ ~766 (1 blueprint)
                                         F13     ──→ ~786 (+20 TCC)
                                         F14     ──→ ~801 (+15 Multi-CU)
                                         F15     ──→ ~806 (+5 Python)
                                         终点目标 ──→ ~810/810
```

---

## 🔗 入口条件检查清单

每次启动新任务前，必须通过此清单：

### Phase 8.A Task 8 ✅ (COMPLETED)
- [x] T1-4 模块 (SharedMemory/MemoryCluster/GpuMeshNoC/KernelLaunch) 全部 frozen ✅
- [x] T5a-e GpuClusterSharedInterface 全部完成 ✅
- [x] T6 GpuSocTLM 顶层 + 注册 ✅
- [x] T7 集成测试 pass ✅
- [x] `[gpu]` 66 cases / 167 assertions 全绿 ✅
- [x] `[phase8a]` 34 cases / 82 assertions 全绿 ✅
- [x] `docs_sync_check.sh --strict` 0 missing ✅
- [x] `format.sh --check` clean ✅
- [x] 全量 755/755 tests pass (15517 assertions) ✅
- [x] M1 性能: full suite < 3s ✅
- [x] Commit: `9789ca0` ✅

### Phase 8.A Oracle Full Review
- [ ] Task 8 完成
- [ ] G2-G6 全部通过

### Phase 8.B
- [ ] Phase 8.A 已归档 (在 `archive/` 目录)
- [ ] `[gpu]` 66 cases + apu_soc 兼容测试全绿

### F4 (Phase 7.C)
- [ ] Phase 8.A 已完成归档 (减少并行复杂度)
- [ ] Brainstorming 完成
- [ ] Spec 文档已产出
- [ ] ADR-7C-01 已签发

### F12b-LD
- [ ] F12a 完成 (✅)
- [ ] PTX-EMU 团队确认 bridge 接口 (`CppTLMBridge`)
- [ ] CMake `libcpptlm_cudart.so` PoC 验证通过
- [ ] 参考 CUDA 程序 + 期望输出选定
- [ ] 性能基线对比方法确定
- [ ] PTX-EMU 全局 singleton 处理策略确定

### F13 Phase 7.D
- [ ] F12a 完成 (✅)
- [ ] (可选) F6 蓝图升级加速验证

### F14 Phase 7.E
- [ ] F12a 完成 (✅)
- [ ] F4 CoherentXBarTLM 完成 (coherence domain 前置)

### F15 Phase 7.F
- [ ] F13 TCC Bridge 完成
- [ ] F14 Multi-CU 完成

---

## 🏷️ 标签体系

| 标签 | 含义 | 颜色 |
|------|------|:---:|
| 🔴 P0 | 最高优先级, 当前阻塞其他任务 | 红 |
| 🟡 P1 | 重要但可并行 | 黄 |
| 🟢 P2 | 正常推进 | 绿 |
| 🔮 | 远期目标 | 紫 |
| ⚠️ | 需要决策 / 风险待解决 | 橙 |

---

*自动维护: 每次 `git commit` 后由开发者手动更新本文件*
*下次全量 review: 2026-07-09*