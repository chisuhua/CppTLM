# CppTLM 实施路线图

> **🚧 历史快照 — 已停止实时维护 🚧**
> **原始版本**: v1.0 · **原始日期**: 2026-07-03 · **原始基线**: 764/764 tests (15547 assertions)
>
> ⚠️ **本文档于 2027-02-09 由 Oracle 评审判定 FAIL 并标记为历史快照**。原内容描述 2026-07-03 时点项目状态，对当前项目（HEAD `dcd4598`，2027-02-09）已严重失真：
> - 测试基线: 764/764 → **当前 1241 cases / 47634 assertions / 2 known fail** (Phase 8 e2e `_config` + `_bar` 已知 Minor)
> - 战略框架: apu_soc/gpu_soc 双路线 → **当前 dGPU SoC v1.0** (NVIDIA Blackwell + AMD CDNA 3/3.5 双 vendor, 8 Phase PCIe EP 全交付)
> - ADR-SOC: 01 (coherence) → **当前 14 份** (01-14, 含 v1.0 新增 6 份)
> - F12b-LD: "blocked 待 PTX-EMU 对齐" → **已实施归档** (`0f0136e`, 2026-07-16)
> - Phase 8.B 核心模块 (Scoreboard/Pipeline/TC): "⏳ 待启动" → **已交付** + 注入 PTX-EMU
> - 维护空窗: 2026-07-03 → 2027-02-09 (≈7 个月) 未更新
>
> **🚀 当前权威实施路线图请参阅**：
> - [`docs/soc_arch/architecture/00-overview.md`](../soc_arch/architecture/00-overview.md) — dGPU SoC v1.0 总架构蓝图 (v3.1-PASS)
> - [`docs/soc_arch/architecture/`](../soc_arch/architecture/) — 10 份子系统架构 (01-10)
> - [`docs/soc_arch/adr/ADR-SOC-09..14`](../soc_arch/adr/) — v1.0 战略新增 6 份 ADR
> - [`docs/soc_arch/adr/revision-plan-v1.0.md`](../soc_arch/adr/revision-plan-v1.0.md) — ADR 修订规划权威源
> - [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../soc_arch/roadmap/roadmap-mvp-to-v05.md) — v0.5 MVP 实施路线 (s1/s2/s3 已归档 + `v0.5.0-MVP` tag 已打)
> - [`openspec/changes/`](../../openspec/changes/) — 11 个活跃 changes + archive 历史
>
> **维护规则变更**：docs/roadmap/ 不再独立维护；新增 OpenSpec change 时由 `openspec/changes/<name>/` 承担项目级协调。

---

## 0. 历史快照（2026-07-03 时点,已过时）

```
┌─────────────────────────────────────────────────────────────────────────┐
│                  CppTLM 实施总览(2026-07-03 快照,已过时)                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  已完成 ✅(2026-07-03 视角)                                              │
│  ├── P0/P1/P1.5 全套 (659→690)                                          │
│  ├── Phase A 8 项 quick wins (690→703)                                   │
│  ├── Phase 7.A 基础设施 (GPU [gpu] tag GREEN)                             │
│  ├── F12a 4 GPU 核心模块 (703→755)                                       │
│  ├── Phase 8.A Tasks 1-7 (4 stub 模块 + 接口 + 顶层 + 集成)                │
│  └── Phase 8.A Task 8 (5 微架构 doc + M1 验收)                            │
│                                                                         │
│  活跃 🚀(2026-07-03 视角)                                                 │
│  ├── Phase 8.B Task 9-14: 6 核心模块单元测试 (Scoreboard→SubCore)          │
│  ├── F4 brainstorming (Phase 7.C 6×6 state table)                        │
│  └── Python Library Phase 0 (C++ 统计注册)                                │
│                                                                         │
│  ⚠️ 以上"活跃/就绪"任务均已在 2026-07..2027-02 期间完成或战略重定位:    │
│     Phase 8.B T9-14: 已交付 + 注入 PTX-EMU                               │
│     F4 (coherence): 已建 CoherentXBarTLM skeleton + tests                 │
│     Python Lib Phase 0/1: cpptlm/ Python 包已就位                       │
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

## 2. 历史阶段划分(2026-07-03 视角,已过时)

### 第一阶段:解锁期 (已完成 ✅)

| 序号 | 任务 | 工期 | 状态 |
|:---:|------|:---:|:---:|
| 1.1 | Phase 8.A Task 8 (doc + M1) | 0.5d | ✅ |
| 1.2 | Phase 8.A Oracle Full Review | 0.1d | ✅ |
| 1.3 | Phase 8.A Archive | 0.1d | ✅ |
| 1.4 | Python Lib Phase 0 (Stats) | 1-2d | ⏳ (已于 2026 H2 完成) |

### 第二阶段:核心推进期 (~2-4 周, 2026-07-03 视角)

| 序号 | 任务 | 工期 | 状态 |
|:---:|------|:---:|:---:|
| 2.1 | Phase 8.B Task 9 (ScoreboardTLM ≥12 entries) | 0.5d | ⏳ → ✅ 已交付 |
| 2.2 | Phase 8.B Task 10-13 (WarpScheduler/Pipeline/TC/L2) | 4d | ⏳ → ✅ 已交付 |
| 2.3 | Phase 8.B Task 14 (SubCoreTLM black-box pipe) | 1d | ⏳ → ✅ 已交付 |
| 2.4 | F4 Brainstorming (6×6 state table) | 2d | ⏳ → ✅ skeleton 已建 |
| 2.5 | F4 Spec 撰写 | 1d | 🔴 → 🟡 进行中 |
| 2.6 | F4 ADR-7C-01 签发 | 0.5d | 🔴 |
| 2.7 | F6 蓝图升级 | 1-2d | ⏳ |
| 2.8 | Python Lib Phase 0 (C++ 统计注册) | 1-2d | ⏳ → ✅ 已交付 |
| 2.9 | Python Lib Phase 1 (纯 Python 配置层) | 2-3d | ⏳ → ✅ 已交付 |

### 第三阶段:GPU 仿真深化期 (~6 周)

| 序号 | 任务 | 工期 | 状态 |
|:---:|------|:---:|:---:|
| 3.1 | Phase 8.B Task 15 (5 类 microbenchmark + gpgpu-sim 区间对照) | 1w | ⏳ → ✅ 已交付 |
| 3.2 | Phase 8.B Task 16 (6 docs + M2 性能 + docs_sync) | 1w | ⏳ → ✅ 已交付 |
| 3.3 | Phase 8.B Oracle + Archive | 0.2w | ⏳ → ✅ |
| 3.4 | F13 Phase 7.D TCC Bridge | 1-2w | ⏳ |

### 第四阶段:集成收官期 (~5 周)

| 序号 | 任务 | 工期 | 状态 |
|:---:|------|:---:|:---:|
| 4.1 | F14 Phase 7.E Multi-CU + NoC | 1-2w | ⏳ |
| 4.2 | F15 Phase 7.F Full APU Demo | 1w | ⏳ |
| 4.3 | F9 多 xbar | 1-2d | ⏳ |
| 4.4 | Phase 8.C (4 高级 + 3 Python 子包) | 3w | ⏳ → ✅ 大部分已交付 |

### 第五阶段:整合期

| 序号 | 任务 | 工期 | 状态 |
|:---:|------|:---:|:---:|
| 5.1 | F12b-LD 实施 (PTX-EMU 集成) | 2-3w | ⏳ → ✅ 已实施归档 (0f0136e) |
| 5.2 | Python Lib Phase 2-3 | 4-6d | ⏳ → ✅ 已交付 |

---

## 3. 后续阶段(2027-02-09 v1.0 战略新增)

### PCIe EP 微架构 Phase 1-8(2026-09 → 2027-02,已全部交付 ✅)

| Phase | 内容 | 关键 commit | OpenSpec change | 状态 |
|:---:|------|------------|----------------|:---:|
| **1** | PCIe Link Layer + DLLP + FC Token Bucket | `e9b1b..` 系列 | `2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc` | ✅ |
| **2** | 128b/130b Encoding Latency | `8d1f1d5` `b21d290` | `2026-09-29-cpptlm-dgpu-pcie-130b-encoding` | ✅ |
| **3** | PHY Digital Ctrl + Bypass Mux | `05be913..bac9267` | `2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl` | ✅ |
| **4** | SR-IOV VF Pool + PcieEndpointIP | `4e9564e` `478cdd9` `a442b65` `536dbfc` `6ebbd7d` | `2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool` | ✅ |
| **5** | AXI Stream Adapter | `6223534..b1811703` | `2026-11-03-cpptlm-dgpu-axi-stream-adapter` | ✅ |
| **6** | AXI4Mapper | `ae8ecd7..f2540e8` | `2026-12-22-cpptlm-dgpu-axi4-mapper` | ✅ |
| **7** | Host Bypass + RC | `a771253..91e515f` | `2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc` | ✅ |
| **8** | 整合交付 | `13ee09d..429327d` | `2027-02-09-cpptlm-dgpu-pcie-ip-integration` | ✅ |

### dGPU SoC v1.0 战略(2027-02-09,归档 ✅)

- 总架构蓝图: `docs/soc_arch/architecture/00-overview.md` (v3.1-PASS)
- 10 份子系统架构: `docs/soc_arch/architecture/01-10` (commit `c568a5a`)
- 6 份新 ADR: `docs/soc_arch/adr/ADR-SOC-09..14` (commit `164224f`)
- 8 份现有 ADR Status Update: `docs/soc_arch/adr/ADR-SOC-01..08` (commit `a478d7b`)
- 54 份模块微架构同步: `docs/soc_arch/modules/` (commit `fd19a7a`)
- AGENTS.md v3.1 (SoC) 迁移: (commit `3511922`)
- 4 份新模块微架构(host-bypass / pcie-root-complex / axi4-stream-adapter / axi4-mapper): (commit `f559bc5`)
- OpenSpec change 归档: `2027-02-09-cpptlm-dgpu-soc-v1-adr-revision` (commit `ef78907` + `dcd4598`)

---

## 4. 测试基线演进

| 时点 | cases | assertions | 状态 |
|:---:|:---:|:---:|------|
| 2026-07-03 | 764 | 15547 | ✅ 快照(本文档原始) |
| 2027-02-09 | **1241** | **47634** | ⚠️ 2 known fail (Phase 8 e2e `_config` + `_bar`,Minor) |

---

## 5. 文件结构(本目录)

```
docs/roadmap/
├── README.md                # 本文档(历史快照,已停止维护)
└── current_status.md        # 看板快照(已停止维护)

# 配套文档(请参阅)
docs/soc_arch/architecture/  # v1.0 总架构蓝图 + 10 份子系统架构(权威)
docs/soc_arch/adr/          # 14 份 ADR-SOC + revision-plan
docs/soc_arch/roadmap/      # v0.5 MVP 实施路线(roadmap-mvp-to-v05.md)
openspec/changes/           # 11 个活跃 changes + archive
```

---

## 6. 维护历史

| 日期 | 版本 | 维护者 | 说明 |
|------|------|--------|------|
| 2026-07-03 | v1.0 | (原维护者) | 初版,764/764 基线 |
| 2027-02-09 | v2.0-historical | Sisyphus | Oracle 评审 FAIL → 降级为历史快照,顶部加横幅,新增 §3 v1.0 战略新增段 |