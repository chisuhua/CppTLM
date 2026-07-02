# CppTLM 实施路线图

> **版本**: 1.0 · **日期**: 2026-07-02 · **基线**: 755/755 tests (15517 assertions)
> **配套看板**: [`current_status.md`](./current_status.md) — 实时状态、入口条件、下一步任务

---

## 0. 全景架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         CppTLM 实施总览                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  已完成 ✅                                                               │
│  ├── P0/P1/P1.5 全套 (659→690)                                          │
│  ├── Phase A 8 项 quick wins (690→703)                                   │
│  ├── Phase 7.A 基础设施 (GPU [gpu] tag GREEN)                             │
│  ├── F12a 4 GPU 核心模块 (703→755)                                       │
│  ├── Phase 8.A Tasks 1-7 (4 stub 模块 + 接口 + 顶层 + 集成)                │
│  └── Phase 8.A Task 8 (5 微架构 doc + M1 验收)                            │
│                                                                         │
│  活跃 🚀                                                                 │
│  ├── Phase 8.A Oracle Full Review + Archive                              │
│  ├── F12b-LD (PTX-EMU 集成, 等 PTX-EMU 侧对齐)                            │
│  └── Python Library Phase 0 (C++ 统计注册)                                │
│                                                                         │
│  就绪 ⏳ (无阻塞)                                                        │
│  ├── Phase 8.A Oracle Full Review + Archive                              │
│  ├── Python Library Phase 1 (纯 Python 配置层)                            │
│  └── F6 (compute_unit_v1.json 蓝图升级)                                   │
│                                                                         │
│  下一阶段 🔮                                                              │
│  ├── F4 (Phase 7.C: CoherentXBarTLM 6×6 state table)                    │
│  ├── Phase 8.B (6 核心模块: Scoreboard→SubCore)                           │
│  ├── F13 (Phase 7.D: TCC Bridge + 内存层次)                               │
│  └── F14/F15/F9 (集成 + Demo + 多 xbar)                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 1. 两条并行路线

CppTLM 当前有 **两条并行路线**，共享 F12 产出的 4 个 GPU 核心模块：

| 路线 | 目标 | 周期 | 模块数 | 验证方式 |
|------|------|:---:|:---:|---------|
| **apu_soc** (Phase 7.B→F) | CPU+GPU 融合 APU SoC | ~9 周 | 4 CU + bridge | Python E2E + coherence |
| **gpu_soc** (Phase 8.A→C) | 独立 GPU 芯片仿真 | ~13 周 | 14 新模块 | gpgpu-sim 数值对照 |

两条路线通过 `GpuClusterSharedInterface` 共享 GpuCluster 容器，互不阻塞。

---

## 2. 阶段划分

### 第一阶段：解锁期 (当前 ~ 2026-07 中旬)

> **目标**: 完成 Phase 8.A 全部 gates，为 8.B 和 F4 铺路

| 序号 | 任务 | 工期 | 依赖 | 状态 |
|:---:|------|:---:|------|:---:|
| 1.1 | Phase 8.A Task 8 (doc + M1) | 0.5d | 8.A T1-7 ✅ | ✅ |
| 1.2 | Phase 8.A Oracle Full Review | 0.1d | 1.1 | 🚀 |
| 1.3 | Phase 8.A Archive | 0.1d | 1.2 | ⏳ |
| 1.4 | Python Lib Phase 0 (Stats) | 1-2d | 无 | ⏳ |

**入口条件** (1.2):
- [x] T1-7 全部 commit 已落地
- [x] `[phase8a]` 34 cases pass
- [x] F12 Gate cleared (4 类已实现)
- [x] Task 8 完成 (5 docs, docs_sync 0 missing, format clean, 755/755 pass)

### 第二阶段：核心推进期 (~2-4 周)

> **目标**: 双线并行 — F4 (coherence) + Python Lib + F12b-LD 前置

| 序号 | 任务 | 工期 | 依赖 | 状态 |
|:---:|------|:---:|------|:---:|
| 2.1 | F4 Brainstorming (6×6 state table) | 2d | 无 | 🔴 待启动 |
| 2.2 | F4 Spec 撰写 | 1d | 2.1 | 🔴 待启动 |
| 2.3 | F4 ADR-7C-01 签发 | 0.5d | 2.2 | 🔴 待启动 |
| 2.4 | F6 蓝图升级 | 1-2d | F12a ✅ | ⏳ |
| 2.5 | Python Lib Phase 1 | 2-3d | 无 (纯 Python) | ⏳ |

**入口条件** (2.1):
- [ ] Phase 8.A 已完成归档 (1.3)
- [ ] 与 ADR-SOC-01 重新对齐确认

### 第三阶段：GPU 仿真深化期 (~6 周)

> **目标**: 并行推进 8.B 核心模块 + F13 TCC Bridge

| 序号 | 任务 | 工期 | 依赖 | 状态 |
|:---:|------|:---:|------|:---:|
| 3.1 | Phase 8.B Task 9-14 (6 核心) | 5.5w | 8.A Done | 🔮 |
| 3.2 | Phase 8.B Task 15-16 (集成+doc) | 1w | 3.1 | 🔮 |
| 3.3 | Phase 8.B Oracle + Archive | 0.2w | 3.2 | 🔮 |
| 3.4 | F13 Phase 7.D TCC Bridge | 1-2w | F12a ✅ | 🔮 |

**入口条件** (3.1):
- [ ] Phase 8.A 已归档
- [ ] `[gpu]` 66 cases + `[phase7]` + `[apu_soc]` 全绿

### 第四阶段：集成收官期 (~5 周)

> **目标**: 两条路线各自完成收官

| 序号 | 任务 | 工期 | 依赖 |
|:---:|------|:---:|------|
| 4.1 | F14 Phase 7.E Multi-CU + NoC | 1-2w | F12a + F4 |
| 4.2 | F15 Phase 7.F Full APU Demo | 1w | F13 + F14 |
| 4.3 | F9 多 xbar | 1-2d | F4 |
| 4.4 | Phase 8.C (4 高级 + 3 Python 子包) | 3w | 8.B |

### 第五阶段：整合期

| 序号 | 任务 | 工期 | 依赖 |
|:---:|------|:---:|------|
| 5.1 | F12b-LD 实施 (PTX-EMU 集成) | 2-3w | PTX-EMU 对齐 |
| 5.2 | Python Lib Phase 2-3 | 4-6d | Phase 0 + 8.C |

---

## 3. 依赖图

```
                      F12a ✅ ────┬──→ F6 ──→ F13 ──┬──→ F14 ──→ F15
                      (755/755)   │                    │
                                  │                    └──→ F9
                                  │
Phase 8.A T1-7 ✅ ──→ 8.A T8 ──→ 8.A Oracle/Archive ──→ 8.B ──→ 8.C
                                  │
                                  ├────────────────────────────────────────┐
                                  │                                        │
              Python Lib P0 ─────────────────────────────→ P1 ──→ P2 ──→ P3
                                  │                                        │
              F12b-LD (blocked) ──┘──→ (PTX-EMU 对齐后) ──────────────────┘

F4 (7.C) ←→ 与 F12 并行 ←→ 与 Phase 8.A/B 无关 ←→ 独立推进
```

---

## 4. 关键决策点

| 决策 | 状态 | 触发条件 | 阻塞 |
|------|:---:|---------|:---:|
| PTX-EMU 团队对齐 F12b-LD | ❌ 待确认 | `CppTLMBridge` 接口 + `libcpptlm_cudart.so` CMake | F12b 实施 |
| F4 6×6 state table 方案 | ❌ 待 brainstorming | 需与 ADR-SOC-01 重新对齐 | F4 实施 |
| Python 包结构统一 | ⚠️ 待解决 | Python Lib Plan 与 Phase 8.C 包冲突 | 两方可并行，合并前解决 |

---

## 5. 文件结构

```
docs/roadmap/
├── README.md           # 本文件 — 完整实施路线图
├── current_status.md   # 实时状态看板 (kanban)
└── decisions.md        # (待创建) 已裁决的关键决策记录
```

配套文档：
- `docs/superpowers/plans/` — 详细实施计划
- `docs/superpowers/specs/` — 架构设计规格
- `openspec/changes/` — OpenSpec 变更提案

---

## 6. 维护规则

1. **每次任务完成后**: 更新 [`current_status.md`](./current_status.md)
2. **每次新增 spec/plan**: 更新本文件对应阶段
3. **每周 review**: 核实测试基线、检查依赖变化
4. **阻塞项跟踪**: 状态看板中标 🔴 项优先解决

---

*维护: CppTLM 开发团队 · 下次 review: 2026-07-09*