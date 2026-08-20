# cpptlm-v3-dgpu-extract v0.4: dGPU Board Architecture Design

> **版本**: v0.4 · **日期**: 2026-08-19 · **状态**: 📐 Design — 等 user review + writing-plans 实施
> **所有者**: CppTLM Team (Sisyphus) · **关联 ADR**: [`ADR-X.15-cpptlm-v3-dgpu-extract.md`](../../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md)
> **关联 OpenSpec**: [`openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/`](../../../openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/)
> **关联设计**: [`openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/design.md`](../../../openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/design.md)（详细契约,以本文件为索引）

---

## 1. 设计目标与边界

### 1.1 主要目标

1. **角色反转**: CppTLM 从"桥接层 (bridge)" 转变为"被驱动 dGPU 板卡 (PCIe device)"
2. **PM4 完整规范**: 完整 AMD PM4 packet 解析(Mesa 风格 bit fields, TYPE3 only MVP)
3. **PCIe 设备语义**: BAR0 MMIO + Doorbell + SQ + CQ,所有 host 写入经 `DGpuBar::write_reg` 路由
4. **strong-ordered write**: Doorbell ring 走 PCIe Gen5 x16 strong-ordered path(250-700ns 区间)
5. **PTX-EMU 委托**: 8 函数 ABI 由 `PtxEmuSubmodule` 透传,执行细节由 PTX-EMU 自包含
6. **TMU Glue Logic**: `TmuDispatchProcessor` 内部组件,负责 dep latches + pre-exit policy + 链式推进

### 1.2 非目标 (Non-Goals)

- ❌ 不替代 PTX-EMU 内部 GPU 模拟器
- ❌ 不实现完整 PCIe 协议栈(CFG + BAR0 + BAR1 最小子集)
- ❌ 不实现 CUDA driver API(由 UsrLinuxEmu + TaskRunner 端承担)
- ❌ 不实现真实 GPU 指令发射路径
- ❌ 不实施 SMContext Adapter 注入(per ADR-NV-02 Status Update,D1-Full 路径废止)
- ❌ 不仿真 PREEXIT/ACQBULK 指令(由 PTX-EMU 自含)
- ❌ 不支持 SM 端任务提交(per US20230236878A1 [0050])
- ❌ ANTLR4 不在 CppTLM scope

---

## 2. 架构对比 (v2.1 → v3.0)

### 2.1 v2.1 (旧) — 桥接层(即将废弃)

```
PTX-EMU (g_cpptlm_bridge 虚接口)
  ↕ cpptlm_attach_bridge()
CppTLM (MemoryBridge 实现 CppTLMBridge)
  ↕ IPtxEmuDriver + DriverWrapper (反向 ABI)
PTX-EMU (g_ptx_emu_driver)
```

### 2.2 v3.0 (新) — 被驱动 dGPU 板卡

```
UsrLinuxEmu + TaskRunner (host)
  ↕ PCIe MMIO (BAR0) + Doorbell + CQ (mmap'd VRAM)
CppTLM (dGPU board)
  ├─ PCIe Substrate (DGpuBar + Doorbell, strong-ordered)
  ├─ CommandProcessor (CP, 5-state FSM, TYPE3-only MVP)
  ├─ TmuDispatchProcessor (TMU Glue, 内部组件, ~200 LOC)
  ├─ SubmissionQueue[cluster] (per-cluster FIFO + Task Dependency Table)
  ├─ ISmExecutor → SmExecutorImpl → PtxEmuSubmodule
  └─ CompletionRing + FenceRegistry (host signal)
```

**关键差异**: 同步 → 异步路径;无设备语义 → 真实 PCIe;CUDA driver 内嵌 → 委托 host;TMU Glue 桥接 SQ ↔ ISmExecutor

---

## 3. 核心组件(详见 design.md §3)

| 组件 | 位置 | 状态 |
|------|------|:---:|
| `DGpuBar` | `include/tlm/gpu/dgpu_bar.hh` | v0.4 稳定(无变更) |
| `Doorbell` | `include/tlm/gpu/doorbell.hh` | **v0.4 增量**(§3.2.5 strong-ordered path) |
| `SubmissionQueue` | `include/tlm/gpu/submission_queue.hh` | **v0.4 增量**(§3.3.5 Task Dependency Table) |
| `CompletionRing` | `include/tlm/gpu/completion_ring.hh` | v0.4 稳定(无变更) |
| `PtxEmuSubmodule` | `include/tlm/gpu/ptx_emu_submodule.hh` | v0.4 稳定(无变更) |
| `ISmExecutor` | `include/tlm/gpu/is_m_executor.hh` | **v0.4 增量**(§3.6.4 dispatch 经 TMU Glue) |
| **`TmuDispatchProcessor`** | **`include/tlm/gpu/tmu_dispatch_processor.hh`** | **� v0.4 新增**(§3.8 TMU Glue Logic) |

---

## 4. TMU Functional Catalog v3 (per Oracle 调研)

### 4.1 TMU 三代形态(per `docs/research/TMU/TMU专利专题整理.md`)

| 形态 | 代表专利 | 关键特征 |
|------|---------|----------|
| Kepler | US20130198760A1 | Task/Work Unit = TMU + WDU |
| Volta/Ampere | US11182207B2 / US9535815B2 | TMU + WDU 并列 |
| **Hopper** | **US20230236878A1** | **WSDU 合并形态(TMU + WDU 融合)** |

### 4.2 关键机制 (13 项 per Oracle Catalog v3)

| 机制 | 来源 | dGPU v3 落地 |
|------|------|-------------|
| TMD 6 区字段 | TMD.md / Atomatic_dependent_task_launch.md | §3.8.2 `TmuDispatchRecord` 21 字段(注：6 区对应 dGPU v3 简化的关键子集,21 字段含 Init/Sched/Exec/Dep/TMD-handle/ring-slot 等;详细字段表见 design.md §3.8.2) |
| **Task Dependency Table 240** | 高效任务启动_解析.md §3.3 | **§3.3.5 `inflight_kernel_reqs_ map`** |
| Wait On/Arrive At Latch | 同上 | dep 锁存器(wait_on_latch_id ↔ arrive_at_latch_id)|
| PREEXIT 指令 330 | 同上 §3.2 | **不仿真**(PTX-EMU 自含) |
| ACQBULK 指令 390 | 同上 §3.4 | **不仿真**(PTX-EMU 自含) |
| Preemptive Memory Flush 320 | 同上 §3.5 | N/A(dGPU 走 PTX-EMU 路径) |
| 指令/常量预取 | 同上 §3.5 | N/A |
| 启动次序保证 | 同上 §3.3 | pre_exit_policy LAST_BLOCK 档 |
| SM 端任务提交 | 同上 §3.1 [0050] | **不仿真**(dGPU v3 仅 host submit) |
| 调度/数据依赖解耦 | 同上 §3.2 | TmuDispatchProcessor::pre_dispatch |
| refcount + OutDependence[] | management_of_dependency.md | **不实施**(v3.4 升级触发) |
| Placeholder 模式 | 同上 | 延期 v3.1 |
| 两级 Cache (Launch + Scheduler) | TMD.md §25-30 | **仅 Scheduler Cache** |

---

## 5. PCIe Strong-Ordered Write (per `docs/research/PCIe/`)

### 5.1 机制

PCIe TLP posted writes **不保序**(尤其跨 aperture)。Doorbell 必须走 strong-ordered path:
- weak atomic store (本仓内)
- aperture-switch check (BAR2 doorbell MMIO)
- dummy non-posted read flush (MMU ordering pipe)
- read completion 等待
- strong store 发出

### 5.2 性能代价

| 路径 | 延迟 |
|------|------|
| PCIe Gen5 x16 | 250-350ns |
| 多级 switch | 400-600ns |
| 跨 RC | > 700ns |

### 5.3 Pipeline 总延迟预算(per design.md §4.2)

| 阶段 | 延迟 |
|------|------|
| cuLaunchKernel → gpfifo_entry | ~100ns |
| doorbell ring (含 strong-order) | **300-750ns** |
| CP FETCH → DECODE → DISPATCH | ~1us |
| SQ tick → ISmExecutor::dispatch | ~50ns |
| PTX-EMU image_execute | variable |
| CompletionRing::push + host_notify | ~50ns |
| **总** | **500ns-2us + kernel exec** |

---

## 6. Acceptance Gates (per proposal.md)

### 6.1 Gates #1-#9(已就位,见 proposal.md)

| Gate | Owner | 状态 |
|------|-------|:---:|
| #1 UsrLinuxEmu ADR-090 v2 ✅ | UsrLinuxEmu | ✅ commit `37a91b6` |
| #2 CppTLM maintainer ack | CppTLM | ✅ commit `369cf71` |
| #3 PTX-EMU HSK-6 发出 | PTX-EMU | ✅ commit `25e36f60` |
| #4 PTX-EMU + UsrLinuxEmu 双 ack | PTX-EMU + UsrLinuxEmu | 🚫 Ack 截止 2026-09-01 |
| #5 G-D4 17 条 static_assert | CppTLM | ✅ commit `fa2b3ec` |
| #6 Mode A/B dual-rail E2E | CppTLM | ⏳ W6-8 |
| #7 baseline 测试验证 | CppTLM | ⏳ W8-9 |
| #8 v3.0.0 tag + 11 项删除 | CppTLM | ⏳ W9 |
| #9 JSON-config E2E | CppTLM | ⏳ W6-8 |

### 6.2 Gate #10 v0.4 增量(本次新增)

| Gate | Owner | 状态 |
|------|-------|:---:|
| **#10.a** Doorbell::ring strong-ordered write path | CppTLM | ⏳ W4-6 |
| **#10.b** Task Dependency Table 256 slot + LIFO eviction | CppTLM | ⏳ W4-6 |
| **#10.c** TMU Glue `TmuDispatchProcessor` 单元测试(覆盖:submit / on_complete / try_chain_dependent / LIFO eviction / dep latches / 环检测) | CppTLM | ⏳ W4-6 |
| **#10.d** TMD-aware 8 用例 (T-TMD-01~08) | CppTLM | ⏳ W4-6 |
| **#10.e** Task Dispatch Pipeline 端到端 | CppTLM | ⏳ W6-8 |

---

## 7. 测试策略(per design.md §6)

| 类别 | 数量 | 阶段 |
|------|:---:|------|
| 单元测试 (§6.1) | 8 行(含 strong-order + TDT + tmu-glue) | W4-6 P2 |
| 集成测试 (§6.2) | 3 E2E(LoadData / LaunchKernel / Sync) | W6-8 P3 |
| Dual-rail 验证 (§6.3) | 5 类 microbenchmark | W6-8 P3 |
| **TMD-aware 测试 (§6.4)** | **8 用例** | **W4-6 P2** |
| **总计** | **~24+** | — |

---

## 8. 风险登记(v0.4 扩展 6 → 12 项,per design.md §7)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1-R6 | per ADR-X.15 §7.3 | — | — | — |
| **R7** | **MMU ordering pipe 仿真误差** | 中 | 中 | 区间断言(250-700ns) |
| **R8** | **Task Dependency Table 容量溢出** | 中 | 中 | LIFO + 256 slot |
| **R9** | **TMD 字段版本不匹配** | 低 | 中 | version 字段校验 |
| **R10** | **Grid vs Queue TMD 误用** | 中 | 高 | 硬约束 GRID only |
| **R11** | **dep.ptr 悬空/环依赖** | 中 | 中 | 环检测(链深 ≤ 8) |
| **R12** | **Scheduler Cache 容量耗尽** | 中 | 中 | LRU + 1024 entries |

---

## 9. 迁移路径(已对齐 v3.0 dGPU 板卡架构)

```
v2.1 桥接层 (MemoryBridge / IPtxEmuDriver / DriverWrapper)
  ↓
W1-P0 冻结 (T-P0-1 ✅ + T-P0-2/3/4 路径修正)
  ↓
W1-3 P1 双轨 (T-P1-A1/A2/A3/A4 mock 基础设施)
  ↓
W4-6 P2 收敛 (T-P2-1 ISmExecutor + T-P2-2 SmExecutorImpl + T-P2-3 E2E + T-P2-4 DGpuBoardTLM)
  ↓
W6-8 P3 重构 (T-P3-1/2/3 KernelLaunchTLM + CompletionRing + dual-rail)
  + T-P3-6 Doorbell strong-ordered
  + T-P3-7 TMU Glue TmuDispatchProcessor
  + T-P3-8 TMD-aware 8 用例
  ↓
W6-8 P3 E2E (T-P3-4 JSON-config + T-P3-9 dual-rail + Gate #10.e)
  ↓
W8-9 P4 物理删除 (T-P4-1 11 项 + T-P4-2 v3.0.0 bump + T-P4-3 测试处置 + T-P4-4 baseline + T-P4-5 AGENTS.md 同步)
```

---

## 10. 跨文档参考

### 10.1 OpenSpec change

- `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/proposal.md` — Acceptance Gate + Cross-Repo
- `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/design.md` — **详细组件契约**(本文索引)
- `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/tasks.md` — P0-P4 任务 + 风险登记
- `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/specs/dgpu-board.md` — DGpuBar/Doorbell/SQ
- `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/specs/sm-executor.md` — ISmExecutor 3 ABI
- `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/specs/completion-ring.md` — CompletionRing

### 10.2 ADR

- [`docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md`](../../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) — 本地决策锁定
- [`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`](../../adr/ADR-NV-01-gpu-soc-architecture-target.md) §3.2 Status Update — Phase-accurate 仅对 synthetic
- [`docs/adr/ADR-NV-02-phase8b-d1-strategy.md`](../../adr/ADR-NV-02-phase8b-d1-strategy.md) Status Update (2026-08-18) — D1-Full 路径废止

### 10.3 研究文档

- `docs/research/TMU/`(16 文件) — TMU 专利解析 + 综合
- `docs/research/TMU/TMD.md` — TMD 完整字段布局
- `docs/research/TMU/高效任务启动_解析.md` — US20230236878A1 + WSDU + PREEXIT/ACQBULK
- `docs/research/TMU/TMU专利专题整理.md` — 23 件专利族 + 三代形态
- `docs/research/PCIe/PCIe_上的保序write.md` — MMU ordering pipe + dummy read flush
- `docs/research/gem5-soc-survey.md` — ComputeUnit patterns + AQL + HSAQueueDescriptor
- `docs/research-cpptlm-gpu-fused-soc-survey.md` — D1-D5 决策对齐

### 10.4 跨仓 commit

- UsrLinuxEmu: ADR-090 v2 (`e03b5a1` + `37a91b6`) + annex §E
- PTX-EMU: HSK-6 (`25e36f60`) + HSK-PROTOCOL-NOTES (`bf1a652d`)
- CppTLM: HSK-6 ack (`369cf71`) + P0-1 G-D4 (`fa2b3ec`) + v0.4 commits (`8b67df8` / `9c9768e` / `07736aa`)

---

## 11. Oracle Sessions

- `ses_ff2106f84ffeM2oItBEa9iu4hL` (2026-08-17) — ADR-076 v1 违规识别
- `ses_fef78854dffeLfDJh7p8ELuMLy` (2026-08-18) — HSK-6 草案 + ADR-090 v2 §D5/D6 + 4 轮评估
- `ses_feaa68b29ffeYt2K5o0tpvxad6` — Section 3 TMU 评审(发现与 SQ 重叠 70%)
- `ses_fea43aad2ffeI04v7hasbMowv2` — TMU Functional Catalog v2 (含 6 新文件)
- `ses_fea359c8effeE0BU4wH98fG8az` — TMU Functional Catalog v3 (TMD-aware + dep chain + LIFO)
- `ses_fe8153216ffeRA7xtyqtDsqOMT` — TMU Catalog v3 + 跨文档(PCIe/SoC)

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 等 user review → writing-plans
**下一步**:
1. User 审阅本文档 + design.md
2. Spec self-review (占位符/一致性/范围/模糊性)
3. Invoke `writing-plans` skill 生成 TDD 5 步结构实施计划
