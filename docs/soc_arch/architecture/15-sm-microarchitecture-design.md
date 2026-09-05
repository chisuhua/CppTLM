# 15. dGPU SoC v1.0 SM 微架构重构设计

> **版本**: v1.0-draft (2027-02-09)
> **状态**: 📋 Proposed — 大爆炸重写规划
> **作者**: CppTLM Team (Sisyphus)
> **关联 ADR**:
> - [`ADR-SOC-02-cu-granularity.md`](../adr/ADR-SOC-02-cu-granularity.md) — **本设计反转其"CU 黑盒优先"决策**（需同步修订 ADR-SOC-02 或开 ADR-SOC-16 背书）
> - [`ADR-SOC-15-cdna-real-isa-roadmap.md`](../adr/ADR-SOC-15-cdna-real-isa-roadmap.md) — 阶段 A/B/C 路线图（本次重构完成阶段 A+B）
> - [`architecture/11-cdna-real-isa-integration.md`](./11-cdna-real-isa-integration.md) — CDNA 真实 ISA 集成设计（双轨前端 + 统一 Timing 宿主）
> - [`architecture/13-cdna-emu-selector.md`](./13-cdna-emu-selector.md) — 选型建议（自研 155 条 + LLVM MC 混合）
> - [`architecture/14-stage-a-architecture-audit.md`](./14-stage-a-architecture-audit.md) — 阶段 A 架构溯源（指出现状用 Coordinator-injected 模式）
> **参考业界**:
> - [NVIDIA Blackwell SM_120 Cycle-Level Study](https://zartbot.github.io/micro_arch/nvidia/sm_120/paper.html#sec5) — 5+V-pipe 架构 + Scoreboard ≥ 12 + 128-bit 指令编码 17-bit 控制码
> - AMD MI300X CDNA3 Architecture Reference (Whitepaper)
> - gpgpu-sim shader_core_ctx.cc — 经典 Fetch/Issue/Execute/LSU/Writeback 分解
> **关联研究**: Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP` + Oracle 演进分析 `ses_f982f1597ffejYGzVek5F7zBfP` follow-up
> **目的**: 完整重构 CppTLM GPU 计算侧 —— 废弃"协调者注入"模式（PipelineTLM/ScoreboardTLM/TensorCoreTLM/KernelLaunchTLM），构建 gpgpu-sim 风格的完整 SM 微架构（12 个 ChStream 子模块 + 8 种 Bundle），让 PTX-EMU 通过 `IComputeDevice` 接口步进模拟指令。同步反转 ADR-SOC-02 的"CU 黑盒 + 永久推迟"决策。
> **触发**: 用户在 2027-02-09 决策："清理 PTX-EMU 集成方式，PTX-EMU 只作为指令功能模拟；删除 KernelLaunch 代码；构建 gpgpu-sim 风格 SM 微架构；除 IComputeDevice 外所有 PTX-EMU 集成方式包括 HSK 协议都要移除"。
> **实施策略**: 大爆炸重写（一次全部替换）—— 用户已选择此策略。
> **前置依赖**: 阶段 A OpenSpec change `cpptlm-dgpu-d1-cdna-isa-phase-a` 仍**有效**（阶段 A 是基础设施抽象，本设计是消费阶段 A 抽象的真实架构），但 ADR-SOC-15 §3 D2 "PipelineTLM 双轨实现" 决策需要修订为本设计。

---

## 15.1 设计动机与背景

### 15.1.1 现状诊断（per 架构溯源 §14）

`docs/soc_arch/architecture/14-stage-a-architecture-audit.md` 已识别现状的 4 类问题：

1. **GPU 算力侧是"协调者注入对象"**，既不是 SimObject（仿真边界端点）也不是 SimModule（参数化容器），而是协调者通过 setter 注入的内部依赖
2. **`KernelLaunchTLM::tick()` 只递增 cycle_counter_**（26 行实现），没有任何真实 SM 仿真逻辑
3. **3 个 vendor 接口（IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming）走 HSK-4 协议**，导致字符串查表和虚拟寄存器 hazard 模型（已在阶段 A 准备替换为 `InstrDescriptor` + `IHazardTracker`）
4. **3 个 GPU 算力侧模块（WavefrontTLM/MinimalWarpSchedulerTLM/VectorRegFileTLM）和 GpuComputeUnitTLM 已用 ChStreamModuleBase 注册但未连接**——是孤岛模块

### 15.1.2 用户决策（2027-02-09）

| 决策项 | 用户选择 | 理由 |
|--------|----------|------|
| 重构范围 | **完整 SM 微架构**（5-stage + SIMT + wavefront）| 反转 ADR-SOC-02 黑盒优先 |
| 子模块分解 | **CDNA 完整子模块**（与 §11.3 对齐）| 贴近硬件真实拓扑 |
| Bundle 连接 | **现存 ComputeReqBundle + 新增子类**（每连接一个 Bundle）| 与项目现有风格统一 |
| IComputeDevice 契约 | **完整 IPtxEmuDevice 12 方法保留**| 与 HSK-8 兼容，与 §11.2.2 对齐 |
| KernelLaunchTLM | **删除**，后续再规划 CommandProcessor 实现| 与"清理 PTX-EMU 集成"一致 |
| 旧 WavefrontTLM 等 4 个 | **重新设计**（参考 Blackwell SM_120 + CDNA）| 用户在选项外指定 |
| 实施路径 | **大爆炸重写**（一次全部替换）| 用户明确选择 |

### 15.1.3 反转的 ADR 决策

`ADR-SOC-02` 2026-06-14 决定：**CU 黑盒优先，永久推迟 5-stage pipeline / SIMT lane / wavefront 调度 / ISA 解析 / 精确 issue/execute 周期建模**。

本次设计**完全反转**该 ADR 决策——从"黑盒 + 永久推迟"转向"完整 SM 微架构 + 5-stage + SIMT + wavefront + 精确周期"。

**修订建议**：
- 在 `ADR-SOC-02` Status Update 追加 §Status Update（2027-02-09）：本 ADR 决策**被反转**，新设计见 `architecture/15-sm-microarchitecture-design.md`
- 或新建 `ADR-SOC-16` 专门背书 SM 微架构重构（与 23 ABI 冻结修订一样的 ADR 决策变更纪律）

---

## 15.2 顶层架构

### 15.2.1 SM 微架构定位

`StreamingMultiprocessorTLM`（顶层容器，原 `GpuComputeUnitTLM` 重命名）是 SM 微架构的根。它：
- **继承** `ChStreamModuleBase`（同 GpuComputeUnitTLM）
- **持有** 12 个 ChStream 子模块（Fetch/Decode/Issue/3 个 Exec/SIMT/2 个 LSU/RegFile/Writeback/HazardTracker）
- **实现** `IComputeDevice`（与 `IPtxEmuDevice` 12 方法同构）
- **驱动** 子模块通过 `tick()` 推进 1 个 SM cycle

### 15.2.2 整体架构图

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│                  StreamingMultiprocessorTLM  (新名字 / 原 GpuComputeUnitTLM 重命名)              │
│                                                                                                  │
│  ┌────────────┐    ┌────────────┐    ┌────────────┐    ┌──────────────┐    ┌────────────────┐  │
│  │  Fetch     │───▶│  Decode    │───▶│   Issue    │───▶│  ScalarALU   │───▶│  Writeback     │  │
│  │  Unit      │    │  Unit      │    │   Unit     │    │  Unit        │    │  Unit          │  │
│  │ (FetchTo-  │    │(DecodeTo-  │    │(IssueTo-   │    │(ExecToWrite- │    │(WritebackTo-   │  │
│  │ Issue-     │    │ Issue-     │    │ Exec-      │    │ back-        │    │ RegFile-       │  │
│  │ Bundle)    │    │ Bundle)    │    │ Bundle)    │    │ Bundle)      │    │ Bundle)        │  │
│  └────────────┘    └────────────┘    └────────────┘    └──────────────┘    └────────────────┘  │
│       ▲                                  │                    │                ▲                │
│       │                                  ▼                    ▼                │                │
│  ┌────┴──────────────────────┐    ┌──────────────┐    ┌─────────────────┐    │                │
│  │  IComputeDevice 入口       │    │ VectorALU    │    │ MatrixCore      │    │                │
│  │  (PTX-EMU 通过 12 方法步进) │    │ Unit         │    │ Unit            │    │                │
│  └───────────────────────────┘    │ (ExecToWB)   │    │ (ExecToWB)      │    │                │
│                                   └──────────────┘    └─────────────────┘    │                │
│                                          ▲                    ▲              │                │
│                                          │                    │              │                │
│                                   ┌──────┴──────┐    ┌────────┴────────┐     │                │
│                                   │ SIMT        │    │ HazardTracker   │     │                │
│                                   │ Lane        │◀──▶│ (CDNA waitcnt   │◀────┘                │
│                                   │ Unit        │    │  query/update)  │                      │
│                                   │(Scoreboard- │    │(Scoreboard-     │                      │
│                                   │ Query-      │    │ Query-Bundle)   │                      │
│                                   │ Bundle)     │    │                 │                      │
│                                   └─────────────┘    └─────────────────┘                      │
│                                          │                    │                                │
│                                          ▼                    ▼                                │
│                                   ┌──────────────┐    ┌─────────────────┐                      │
│                                   │  RegFile     │    │ LsuGlobal       │                      │
│                                   │  Unit        │    │ Unit            │──→ IMemoryPort → CppTLM NoC │
│                                   │ (Vector Reg  │    │ (MemoryReq-     │    (异步回调)            │
│                                   │  + Scalar)   │    │  Bundle)        │                      │
│                                   └──────────────┘    └─────────────────┘                      │
│                                          ▲                    │                                │
│                                          │                    ▼                                │
│                                   ┌──────┴───────────────────────┐                             │
│                                   │  LsuLDS                       │                             │
│                                   │  Unit (DS/Bundle)             │                             │
│                                   │  (intra-SM 数据共享,           │                             │
│                                   │   不走 NoC)                   │                             │
│                                   └──────────────────────────────┘                             │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 15.2.3 与 NV Blackwell SM_120 + AMD MI300X 对照

| 微架构组件 | NV Blackwell SM_120 | AMD MI300X CDNA3 | 本设计 CppTLM SM |
|-----------|---------------------|-------------------|-----------------|
| **Warp/Wavefront** | 32 threads/warp | 64 threads/wavefront | 32/64 双模式（ISA 类型决定）|
| **Sub-core 划分** | 4 sub-cores（warpid % 4 静态绑定）| 4 SIMD16 units / CU | **Phase 1：1 sub-core**，Phase 2：4 sub-cores |
| **Pipelines** | 5 标量 + V-pipe (P0/P1/P2/P3/P4) | SALU/VALU/MFMA/branch | ScalarALU/VectorALU/MatrixCore/branch/SFU |
| **Issue width** | 2/cycle | 5 issue slots/CU/cycle | 2/cycle (Phase 1)，5/cycle (Phase 2) |
| **Scoreboard** | ≥ 12 per sub-core + 17-bit control code | `s_waitcnt` 显式计数器 (vmcnt/lgkmcnt/expcnt) | **双模：虚拟寄存器 OR 显式计数器**（kHardwareCounter 阶段 C 启用）|
| **Operand Collector** | 4-bank RF + collector | 256 ACCVGPR + 512 VGPR | RegFileUnit (32/64 双宽度)|
| **TC 路径** | P4 unified TC pipeline (12 精度共享) | MFMA 指令族 | MatrixCoreUnit (与 §11.3.3 对齐)|
| **Memory** | P3 LSU (TMA + LDG/STG + LDMATRIX) | global/flat/ds 三段 | LsuGlobal + LsuLDS 双模块 |
| **Stall Counter** | 4-bit Stall Counter in 128-bit SASS | s_nop/s_waitcnt 编码 | HazardTracker 集成（CDNA `s_waitcnt` 全语义）|
| **Dispatch 延迟** | 3 cycle (clock64 → IADD3 结果) | n/a | 3 cycle (Phase 1) |

**关键差异**：
- NV Blackwell scoreboard depth ≥ 12 + 17-bit control code（编译器信任）；本设计用显式计数器（CDNA 模型），更可调试
- AMD CDNA 显式 `s_waitcnt` 3 计数器（vmcnt/lgkmcnt/expcnt）；本设计 `HazardTracker` 同时支持虚拟寄存器和显式计数器双模
- 本设计**不模拟** NV 17-bit 控制码（过度细节），但**完整模拟** CDNA `s_waitcnt`（与 §11.3.2 对齐）

---

## 15.3 12 个 ChStream 子模块

### 15.3.1 子模块清单与职责

| # | 子模块 | 职责 | 输入 Bundle | 输出 Bundle | Pipeline 类别 |
|---|--------|------|-------------|-------------|---------------|
| 1 | **FetchUnit** | 抓取指令（从 IComputeDevice 提供的 instr buf 或 PTX-EMU 已 decode 的 InstDescriptor）| (无输入，从 IComputeDevice 入口读) | FetchToIssueBundle | 阶段 1 |
| 2 | **DecodeUnit** | 解码（PTX-EMU 已 decode，CppTLM 这里只做 `InstrDescriptor` 分类 + `LatencyClass` 推断）| FetchToIssueBundle | DecodeToIssueBundle | 阶段 1 |
| 3 | **IssueUnit** | 调度（round-robin per warp，按 `warp_id % 4` 静态绑定 sub-core；CGGTY 阈值 ≥ 5 warps）| DecodeToIssueBundle | IssueToExecBundle | 阶段 1 |
| 4 | **ScalarALU** | 标量 ALU 执行（P0 pipe: IADD3/IMAD/FFMA/FP64/LOP3）| IssueToExecBundle | ExecToWritebackBundle | 阶段 1 |
| 5 | **VectorALU** | 向量 ALU 执行（V-pipe + P0 后端: VIADD.U8x4/FMNMX/VIADD.16x2）| IssueToExecBundle | ExecToWritebackBundle | 阶段 1 |
| 6 | **MatrixCore** | 矩阵指令执行（CDNA MFMA: v_mfma_f32_16x16x16_fp16 等 20 条核心子集）| IssueToExecBundle | ExecToWritebackBundle | 阶段 1 |
| 7 | **SIMTLane** | SIMT lane 控制（EXEC mask 更新 + warp 分歧检测）| IssueToExecBundle | ScoreboardQueryBundle | 阶段 1 |
| 8 | **LsuGlobal** | 全局内存访问（global_load/global_store → IMemoryPort → CppTLM NoC）| IssueToExecBundle | MemoryReqBundle / MemoryRespBundle | 阶段 1 |
| 9 | **LsuLDS** | LDS（AMD Local Data Share）intra-SM 数据访问 | IssueToExecBundle | MemoryReqBundle (intra-SM) | 阶段 1 |
| 10 | **RegFileUnit** | 寄存器文件（Vector 32/64 双宽度 + Scalar + ACCVGPR）| WritebackToRegFileBundle | (内部状态查询) | 阶段 1 |
| 11 | **WritebackUnit** | 写回执行结果到寄存器文件 + HazardTracker release | ExecToWritebackBundle | WritebackToRegFileBundle | 阶段 1 |
| 12 | **HazardTracker** | 危险跟踪（PTX 模式：kVirtualReg; CDNA 模式：kHardwareCounter，vmcnt/lgkmcnt/expcnt 三维数组）| ScoreboardQueryBundle | (查询响应) | 阶段 1 |

### 15.3.2 子模块依赖图

```
FetchUnit ──▶ DecodeUnit ──▶ IssueUnit ──┬──▶ ScalarALU  ──┐
                                          ├──▶ VectorALU  ──┼──▶ WritebackUnit ──▶ RegFileUnit
                                          ├──▶ MatrixCore ──┘            │
                                          ├──▶ SIMTLane ──┐             │
                                          ├──▶ LsuGlobal ─┤             │
                                          └──▶ LsuLDS ────┘             │
                                                       │              │
                                                       ▼              ▼
                                                  HazardTracker ◀─────┘
```

### 15.3.3 子模块详细说明

#### 15.3.3.1 FetchUnit

- **职责**：从 `IComputeDevice::set_instr_descriptor_buf` 读取下条指令，组装为 `FetchToIssueBundle`
- **关键方法**：`void tick()`（每 cycle 抓取最多 2 条指令，issue width = 2/cycle）
- **延迟参数**：3 cycle dispatch 延迟（per NV Blackwell SM_120 §3.3 实测）
- **CDNA 适配**：`FetchUnit::tick()` 检测当前 ISA（PTX 32-thread warp OR CDNA 64-thread wavefront）切换 wavefront 宽度

#### 15.3.3.2 DecodeUnit

- **职责**：把 `InstrDescriptor` 分类到 `PipeClass` + 推断 `LatencyClass`
- **关键方法**：`PipeClass classify(const InstrDescriptor&)`
- **重要**：**不做字符串解析**（已废弃 PTX 字符串查表），只对 POD 字段做查表
- **CDNA 适配**：MFMA 指令 → PipeClass::kMatrixCore；LDG/STG → PipeClass::kLSU_Global

#### 15.3.3.3 IssueUnit

- **职责**：从 `DecodeUnit` 拉指令，按 warp 调度策略发射到对应执行单元
- **调度策略**：round-robin + CGGTY 阈值（≤ 4 warps 无 latency hiding，≥ 5 warps 6× 加速，per NV §5.3）
- **关键方法**：`IssueToExecBundle select_next(uint32_t sub_core_id)`
- **warpid 静态绑定**：`warp_id % 4 == sub_core_id`（per NV §4.3 4-sub-core 拓扑）

#### 15.3.3.4 ScalarALU / VectorALU / MatrixCore

- 3 个执行单元
- **ScalarALU**：PTX `fma.rn.f32` (FFMA) + `IADD3` + `IMAD` + `LOP3` + FP64
- **VectorALU**：PTX `VIADD.U8x4` (V-pipe) + `FMNMX` + `VIADD.16x2`
- **MatrixCore**：CDNA `v_mfma_f32_16x16x16_fp16` 等 20 条核心子集（per architecture/13 §13.2.3 选型）
- **每个执行单元**：输入 `IssueToExecBundle`，输出 `ExecToWritebackBundle`
- **延迟**：依据 `LatencyClass` 查表（per §11.2.1 LatencyClass 枚举）

#### 15.3.3.5 SIMTLane

- **职责**：跟踪 EXEC mask + 检测 SIMT 分歧
- **CDNA 模式**：64-bit EXEC（vs PTX 32-bit）
- **分歧处理**：分发活跃 lane 到不同 sub-core，回收 non-active lane
- **关键方法**：`void update_exec_mask(uint32_t warp_id, uint64_t new_mask)`

#### 15.3.3.6 LsuGlobal + LsuLDS

- **LsuGlobal**：全局/flat 内存访问 → `IMemoryPort::send_request` → 异步回调 → s_waitcnt decrement
- **LsuLDS**：intra-SM 数据共享（不通过 NoC），寄存器堆 + SMEM
- **CDNA 适配**：CDNA 区分 `global_*` (走 LsuGlobal) vs `flat_*` (走 LsuGlobal) vs `ds_*` (走 LsuLDS)

#### 15.3.3.7 RegFileUnit

- **职责**：管理 VGPR/SGPR/ACCVGPR（CDNA 专用）三段寄存器
- **CDNA 模式**：512 VGPR/lane + 106 SGPR + 256 ACCVGPR/lane
- **Bank conflict**：operand collector 4 bank（per NV §10.6 完全消解 bank conflict）
- **读写**：经 WritebackToRegFileBundle 写入；经 ScoreboardQueryBundle 查询当前值

#### 15.3.3.8 WritebackUnit

- **职责**：从 Execute 单元接收结果，写回 RegFile + release HazardTracker
- **关键方法**：`void commit(ExecToWritebackBundle)`
- **CDNA 适配**：`s_waitcnt` 等待完成 → 写 ACCVGPR → release vmcnt

#### 15.3.3.9 HazardTracker

- **职责**：跟踪未完成指令，gate hazard 检测
- **PTX 模式**：kVirtualReg（虚拟寄存器 (reg_id, warp_id) 哈希表，CAPACITY=2048，per ScoreboardTLM 既有）
- **CDNA 模式**：kHardwareCounter（三维数组 vmcnt[sm][wave]/lgkmcnt[sm][wave]/expcnt[sm][wave]）
- **CDNA 适配**：CDNA `s_waitcnt vmcnt(0)` → 等所有 vmcnt 减到 0 才放行

---

## 15.4 8 种 Bundle 定义

### 15.4.1 Bundle 清单

| # | Bundle 类型 | 流向 | 字段 | 继承 |
|---|-----------|------|------|------|
| 1 | **FetchToIssueBundle** | FetchUnit → IssueUnit | instr_desc, warp_id, pc | bundle_base |
| 2 | **DecodeToIssueBundle** | DecodeUnit → IssueUnit | instr_desc, warp_id, pipe_class, latency_class, ctrl_bits | FetchToIssueBundle |
| 3 | **IssueToExecBundle** | IssueUnit → Exec 单元 | instr_desc, warp_id, src_regs, dst_regs, lane_mask | DecodeToIssueBundle |
| 4 | **ExecToWritebackBundle** | Exec 单元 → WritebackUnit | instr_desc, warp_id, dst_regs, result_value, exec_duration_cycles | IssueToExecBundle |
| 5 | **WritebackToRegFileBundle** | WritebackUnit → RegFileUnit | warp_id, dst_regs, value, is_accvgpr (CDNA only) | bundle_base |
| 6 | **MemoryReqBundle** | LsuGlobal/LsuLDS → IMemoryPort | vaddr, size, is_write, sm_id, wave_id, tag, lane_mask (intra-SM) | ComputeReqBundle |
| 7 | **MemoryRespBundle** | CppTLM NoC → LsuGlobal | tag, sm_id, wave_id, data, is_hit | ComputeRespBundle |
| 8 | **ScoreboardQueryBundle** | IssueUnit/Exec/SIMTLane → HazardTracker | query_type (is_stalled/decrement/increment), warp_id, sm_id, ctrl_bits | bundle_base |

### 15.4.2 Bundle 关键字段（CDNA 适配）

所有 8 种 Bundle 都携带 `InstrDescriptor`（per §11.2.1 POD）+ `warp_id` + `sm_id` + `wave_id`（CDNA 64-thread wavefront）。CDNA 特有字段：

- `EXEC mask` (64-bit, CDNA 模式): uint64_t
- `vmcnt/lgkmcnt/expcnt req`: CtrlBits 6 字段
- `ACCVGPR write flag`: bool (CDNA MFMA only)
- `is_mfma`: bool (触发 4-dword 累写回)
- `literal_const`: uint32_t (AMD 240-255 inline constants)

### 15.4.3 Bundle 与现存 ComputeReqBundle 的关系

- `MemoryReqBundle` **继承** `ComputeReqBundle`（`include/bundles/compute_bundles_tlm.hh`），添加 CDNA wavefront 字段
- `MemoryRespBundle` **继承** `ComputeRespBundle`，添加 CDNA wavefront 字段
- 其余 6 种 Bundle 新增到 `include/bundles/sm_bundles_tlm.hh`

---

## 15.5 IComputeDevice 接口契约

> **⚠️ Oracle P0 修订**：本节原 v1.0-draft 把 `IComputeDevice` 放在 CppTLM 端（SM 实现），与 architecture/11 §11.2.2 + ADR-SOC-15 D2 中"前端 emulator 实现 IComputeDevice"的方向相反。本修订明确两个 IComputeDevice 变体与真正的契约边界。

### 15.5.0 方向反转与真实契约（修订）

**真实方向**（per `external/PTX-EMU/include/ptxemu/device_api.h:85-114`）：

```cppTLM
namespace ptxemu {
class IPtxEmuDevice {
public:
    virtual ~IPtxEmuDevice() = default;
    
    // 生命周期
    virtual bool initialize(const DeviceConfig& config) = 0;          // L90
    virtual void shutdown() = 0;                                       // L91
    
    // 单步推进（CppTLM 主循环驱动 PTX-EMU）
    virtual int  exe_once() = 0;                                       // L95
    virtual int  sm_exe_once(uint32_t sm_id) = 0;                     // L96
    virtual int  warp_exe_once(uint32_t sm_id, uint32_t warp_id) = 0;  // L97  (NOT wave!)
    
    // scoreboard mask 设置
    virtual bool set_scoreboard(uint32_t sm_id, uint32_t warp_id, uint64_t mask) = 0;  // L101  (sm/warp/mask, NOT IScoreboard*)
    
    // 状态查询
    virtual ThreadState get_thread_state(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id) = 0;  // L104
    virtual bool set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask) = 0;          // L105
    virtual bool set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc) = 0;  // L106
    virtual WarpStatus get_warp_status(uint32_t sm_id, uint32_t warp_id) = 0;                  // L109
    virtual bool is_finished() = 0;                                    // L110
    
    // timing 注入（HSK-4 vendor 接口）
    virtual void attach_timing(IScoreboard* sb, IPipelineLatencyProvider* pl, ITensorCoreTiming* tc) = 0;  // L114
};
}  // namespace ptxemu
```

**真实契约语义**：
- `IPtxEmuDevice` **由 PTX-EMU 端实现**（在 `external/PTX-EMU/...`）
- **CppTLM 端调用** PTX-EMU 的 `exe_once()` 推进模拟
- PTX-EMU 通过 `attach_timing()` 接收 CppTLM 端的 hazard/latency/TC provider
- PTX-EMU 通过 `get_thread_state/get_warp_status/is_finished` 暴露内部状态给 CppTLM

### 15.5.1 本设计的 IComputeDevice 方向（SM 端抽象）

**方向反转的合理性**（per 用户决策："PTX-EMU 只作为指令功能模拟 + IComputeDevice 接口步进"）：

本次重构与 architecture/11 §11.2.2 + ADR-SOC-15 D2 的根本差异：
- **architecture/11 时期方向**：前端 emulator（PTX-EMU/CDNA-EMU）实现 `IComputeDevice`，CppTLM 调用之 → emulator 拥有"什么是正确执行"
- **本设计新方向**：SM（位于 CppTLM 端）实现 `IComputeDevice`，PTX-EMU 通过 `set_instr_descriptor_buf()` 注入已解码的 `InstrDescriptor` → CppTLM 拥有"何时/在哪里/以多少周期执行"

**理论参考**：accel-sim trace-driven 模型 + MGPUSim functional-timing 分离（per Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP`）—— SM 是 timing 容器，PTX-EMU 是 functional reference。

### 15.5.2 Functional/Timing State 划分（Oracle Round 2 P0 修订）

> **Oracle Round 2 P0 指出**："§15.5.2 表正确，但 §15.3.3.4/§15.3.3.7/§15.3.3.8/§15.4.1 全部按'SM 持有并计算寄存器值'设计 → 双 register state 必然发散"。

**用户拍板（2027-02-09）**：**SM 持寄存器值**（单真值源）；PTX-EMU 通过 `set_instr_descriptor_buf()` 同步机制从 SM 拉回结果，但 SM 端是权威。

**明确划分（SM-owns-state 模式）**：

| 状态类别 | PTX-EMU 持有 | SM 持有 | 同步机制 |
|----------|--------------|---------|----------|
| **PC（程序计数器）** | ✅ 持有（解码后知道下一条 PC）| ⚠️ 仅追踪（不修改）| PTX-EMU 通过 `set_next_pc()` 写入；SM 不修改 PC |
| **寄存器值（VGPR/SGPR/ACCVGPR）** | ❌ 不持有（仅缓存快照）| ✅ **唯一真值源** | SM `RegFileUnit` 写值 → `set_instr_descriptor_buf()` 下次调用时 PTX-EMU 拉回 |
| **Lane-level EXEC mask** | ✅ 持有（functional state）| ⚠️ 部分跟踪（timing state）| PTX-EMU 通过 `set_active_mask()` 写入；SM `SIMTLane` 追踪 lane 是否可发射 |
| **Warp/Wavefront 调度顺序** | ❌ 不持有 | ✅ 持有 | SM `IssueUnit` 内部状态 |
| **Hazard 状态（vmcnt/lgkmcnt/expcnt）** | ❌ 不持有 | ✅ 持有（SM `HazardTracker`）| Hazard 是 timing concern |
| **内存数据值** | ❌ 不持有 | ✅ 持有（SM `LsuGlobal` 回调）| SM `MemoryRespBundle` 返回 data，PTX-EMU 通过 `set_instr_descriptor_buf()` 拉取 |
| **内存事务（NoC transactions）** | ❌ 不发起 | ✅ 持有（SM 发起并接收）| SM `LsuGlobal` → `IMemoryPort` 异步 |
| **Cycle 计数** | ✅ 持有（PTX-EMU 内部）| ✅ 持有（SM 内部）| 双重持有但各自有用途（PTX-EMU 给 functional 时间，SM 给 timing 时间）|

**契约（同步语义）**：
1. PTX-EMU 调用 `set_instr_descriptor_buf(InstrDescriptor* buf, uint32_t count)` 注入下一批指令
2. SM 接收 buf 后推进 1 cycle：`FetchUnit` 从 buf 抓取 → DecodeUnit 分类 → IssueUnit 调度 → Execute 单元（ScalarALU/VectorALU/MatrixCore）计算结果 → WritebackUnit **写回 `RegFileUnit`** → 释放 `HazardTracker`
3. **PTX-EMU 下次调用 `set_instr_descriptor_buf()` 时**，SM 通过同一个 buf 的回填字段返回寄存器写值（per §15.5.6 sync 协议）
4. PTX-EMU 通过 `get_thread_state/get_warp_status` 查询 lane-level EXEC mask，但 lane 是否可发射由 SM 内部 hazard 决定

**单真值源保证**：寄存器值唯一存在 SM `RegFileUnit`；PTX-EMU 不维护 register state —— 通过 `set_instr_descriptor_buf()` 双向同步。这避免了双 register state 发散风险（Oracle Round 2 P0 核心关切）。

### 15.5.6 set_instr_descriptor_buf 同步协议（新增，Oracle Round 3 P0 修复）

> **Oracle Round 3 P0 指出**：v3.0 SM-owns-state 模式缺**寄存器读路径**（PTX-EMU functional simulator 必须能读 SM 寄存器值来判定分支/计算地址）+ **写回就绪协议**（PTX-EMU 下次调用时在飞指令未写回则读到陈旧值）。用户 2027-02-09 决策：IComputeDevice 扩 2 方法（`get_register_value()` + `is_instruction_completed()`）。

**完整双向协议**：

```cpp
class IComputeDevice {
public:
    // ... 原有 12 方法 ...
    
    // === 新增：寄存器读路径（PTX-EMU functional simulator 读 SM 真值）===
    virtual bool get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                                     uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF) = 0;
    
    // === 新增：指令就绪查询（PTX-EMU 等写回完成）===
    virtual bool is_instruction_completed(uint64_t instr_id) = 0;
};
```

**协议总览**：

| 方向 | 接口 | 时机 | 内容 |
|------|------|------|------|
| **下行** | `set_instr_descriptor_buf()` | PTX-EMU 解码后 | PTX-EMU 把 `InstrDescriptor` 注入 SM |
| **下行** | `set_next_pc()` | PTX-EMU 分支判定后 | PTX-EMU 通过读路径读 SM 寄存器值，判定后写入下一 PC |
| **下行** | `set_active_mask()` | PTX-EMU 算 SIMT 后 | PTX-EMU 写入 lane EXEC mask |
| **上行** | `get_register_value()` | PTX-EMU functional 评估时 | PTX-EMU 读 SM 寄存器真值做 functional 计算 |
| **上行** | `is_instruction_completed()` | PTX-EMU 等写回完成时 | PTX-EMU 阻塞到 SM `WritebackUnit` 写回 `RegFileUnit` 后 |
| **上行** | buf `result_value[]` 回填 | PTX-EMU 下次 `set_instr_descriptor_buf()` 调用时 | SM `WritebackUnit` 在写回 `RegFileUnit` 时同步填充（避免双 register 真值发散）|
| **上行** | `get_thread_state/get_warp_status` | PTX-EMU 状态查询时 | SM 暴露 hazard 状态、EXEC mask、blocked_cycles |

**`InstrDescriptor` POD 扩展**（含 ISA 判别位，Round 2 P2 修复）：

```cpp
struct InstrDescriptor {
    // === 下行字段（PTX-EMU 写入）===
    enum class IsaType : uint8_t { kPTX32, kCDNA64 } isa_type = IsaType::kPTX32;  // ISA 判别位
    uint64_t instr_id = 0;            // 全局唯一指令 ID（PTX-EMU 端分配），供 is_instruction_completed 查询
    PipeClass    pipe;
    LatencyClass latency_class;
    CtrlBits     ctrl;
    uint16_t dst_regs[4];
    uint16_t src_regs[4];
    uint8_t  num_dst = 0;
    uint8_t  num_src = 0;
    bool     is_memory = false;
    uint64_t target_vaddr = 0;
    uint32_t mem_size     = 0;
    uint32_t pc          = 0;
    
    // === 上行字段（SM 在 PTX-EMU 下次调用前回填）===
    uint64_t result_value[4] = {0};
    uint8_t  result_num = 0;
    bool     memory_data_valid = false;
    uint64_t memory_data = 0;
};
```

**保证**：
- PTX-EMU 在两次 `set_instr_descriptor_buf()` 调用之间通过 `get_register_value()` 读源值做 functional 计算
- PTX-EMU 通过 `is_instruction_completed(instr_id)` 阻塞等 SM 写回，避免读到陈旧值
- 上行字段由 SM `WritebackUnit` 在写回 `RegFileUnit` 时同步填充（避免双 register 真值发散）
- SM 持唯一寄存器真值（PTX-EMU 仅缓存 snapshot via `get_register_value()`）

**IComputeDevice 总方法数修订**（Oracle Round 3 P1-a 闭合）：
- 原 12 方法 → **14 方法**（新增 `get_register_value()` + `is_instruction_completed()`）
- HSK-9 草稿 §2 表与 §3 代码块必须同步更新

### 15.5.3 真实 IPtxEmuDevice 方法映射（Oracle P0 修订）

| # | `IPtxEmuDevice` 真实签名 | 本设计 `IComputeDevice` 对应 | 说明 |
|---|--------------------------|------------------------------|------|
| 1 | `initialize(config)` | `initialize(config)` | 同构 |
| 2 | `shutdown()` | `shutdown()` | 同构 |
| 3 | `exe_once()` | `exe_once()` | **方向反转**：本设计中 SM::实现 `exe_once`，PTX-EMU 调用 |
| 4 | `sm_exe_once(sm_id)` | `sm_exe_once(sm_id)` | 同上方向反转 |
| 5 | `warp_exe_once(sm_id, warp_id)` | `warp_exe_once(sm_id, warp_id)` | **保持 `warp_` 命名**（per HSK-8 spec） |
| 6 | `set_scoreboard(sm, warp, mask)` | `set_scoreboard(sm, warp, mask)` | **同构**（mask setter，非 `IScoreboard*` 注入）） |
| 7 | `get_thread_state(sm, warp, lane) → ThreadState` | `get_thread_state(sm, warp, lane) → ThreadState` | **同构**（per-lane ThreadState） |
| 8 | `set_active_mask(sm, warp, mask)` | `set_active_mask(sm, warp, mask)` | 同构 |
| 9 | `set_next_pc(sm, warp, lane, pc)` | `set_next_pc(sm, warp, lane, pc)` | **保留**（per-lane PC 设置，PTX-EMU 分歧控制需要）|
| 10 | `get_warp_status(sm, warp) → WarpStatus` | `get_warp_status(sm, warp) → WarpStatus` | 同构 |
| 11 | `is_finished()` | `is_finished()` | 同构 |
| 12 | `attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)` | **删除** | vendor 3 接口已删除（PipelineTLM/ScoreboardTLM/TensorCoreTLM 物理删除）；SM `HazardTracker` 替代 IScoreboard，`LatencyClass` 查表替代 IPipelineLatencyProvider，`MatrixCoreUnit` 替代 ITensorCoreTiming |

**净变化**（Oracle Round 3 P1-a + 用户决策 P0-3 修订）：
- 删除 **1** 个方法（仅 #12 `attach_timing`，vendor 3 接口已废）
- 新增 **3** 个方法（`set_instr_descriptor_buf()` + `get_register_value()` + `is_instruction_completed()`）
- 保留 **11** 个方法（同构）
- 总数：12 → **14** 方法（净新增 2 个读路径/就绪协议 + 1 个 producer 同步）

**PTX-EMU 端 `device_api_impl.cc` 必须修改**（Oracle Round 2 P1-a 修订）：
- 注入 descriptor 到 SM（`set_instr_descriptor_buf()` producer 侧）
- 通过 `get_register_value()` 读 SM 真值（读路径）
- 通过 `is_instruction_completed()` 阻塞等写回（就绪协议）
- 移除 `attach_timing()` 调用路径（`sm_context_cpptlm_inject.cpp` 改造为 stub）
- 调用 SM::exe_once（方向反转下 SM 是 timing host）

### 15.5.4 数据流（修订）

```
┌────────────────────────────────────────────────────────────────────────┐
│  PTX-EMU 端 (functional)         │  CppTLM SM 端 (timing)            │
│                                  │                                    │
│ 1. 拿到 PTX 字节流               │                                    │
│ 2. decode 为 InstrDescriptor POD │                                    │
│ 3. set_instr_descriptor_buf() ───┼──▶ SM::FetchUnit 接收             │
│ 4. PC + 分支控制 (set_next_pc) ──┼──▶ SM::FetchUnit/SIMTLane 接收     │
│ 5. 拿 hazard/lane EXEC mask ◀────┼── SM::get_thread_state/get_warp_  │
│    (问询 SM)                     │    status 暴露                    │
│ 6. 继续推进下一批指令             │                                    │
│                                  │ 7. tick() 推进 1 cycle            │
│                                  │ 8. FetchUnit 从 buf 抓取          │
│                                  │ 9. DecodeUnit 分类 LatencyClass   │
│                                  │ 10. IssueUnit round-robin 调度   │
│                                  │ 11. Execute 单元按 LatencyClass  │
│                                  │     占用 cycles                   │
│                                  │ 12. WritebackUnit 释放 HazardTracker│
│                                  │ 13. SM::exe_once 返回推进 cycle 数│
│                                  │                                    │
│ 14. set_instr_descriptor_buf() 下一批指令                            │
└────────────────────────────────────────────────────────────────────────┘
```

### 15.5.5 Branch / SIMT 所有权（Oracle P1 修订）

- **分支控制**（PipeClass::kBranch）：PTX-EMU 通过 `set_next_pc()` 写入 SM（SM 不修改 PC，仅追踪）；SM `SIMTLane` 处理 SIMT 分歧
- **EXEC mask**：PTX-EMU 通过 `set_active_mask()` 写入 SM；SM `SIMTLane` 追踪 lane 是否可发射
- **`s_waitcnt` 指令**（CDNA `PipeClass::kSpecialSync`）：由 `ScalarALU` 或 `IssueUnit` 处理（CPNA 中 `s_waitcnt` 是 scalar instruction，per ISA manual §3.7.4）
- **分支单元归属**：本设计不单独设 BranchUnit，branch 指令由 `ScalarALU` 处理（per NV Blackwell SM_120 §5.1 中 branch 也走 P0 共享路径）

---

## 15.6 23 ABI 不变量保护

> **Oracle P2 修订**：原 v1.0-draft 写"12 ABI 不变量"是 typo（项目 invariant 是"23 ABI"），修订为"23 ABI 不变量保护"。

### 15.6.1 严格禁止（Anti-patterns）

- ❌ **修改 23 ABI 冻结头文件**：`include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h`
- ❌ **修改 PcieEndpointIP 17 端口布局**
- ❌ **修改 PTX-EMU 子模块公共头**（`external/PTX-EMU/include/ptxemu/device_api.h`），保持向后兼容
- ❌ **在 CppTLM 核心库中包含 PTX-EMU 内部头文件**（Clean Room 边界）

### 15.6.2 严格保护（Invariants）

- ✅ **23 ABI 冻结**（per AGENTS.md §KEY INVARIANTS）
- ✅ `PcieEndpointTLM` 4 端口布局冻结（仅可加 `[[deprecated]]`）
- ✅ `PcieEndpointIP` 17 端口布局冻结
- ✅ `PTXEMU_API_VERSION=1` 冻结（per HSK-8 spec）
- ✅ CppTLM Clean Room 原则（不感知 PTX-EMU 内部）
- ✅ IComputeDevice 与 IPtxEmuDevice 12 方法同构（per 用户决策 + §15.5.3）

### 15.6.3 HSK-9 必要性 + 与既有草稿冲突（Oracle Round 2 N-P0-2 修订）

> **Oracle Round 2 N-P0-2 指出**：仓内已存在 `docs/soc_arch/adr/hsk9-announcement-draft.md`（commit `4105602`），其 HSK-9 定义为"阶段 A 后发布、`ICOMPUTE_API_VERSION=1` 双轨并存、**PTX-EMU 端无任何代码变更**、attach_timing **保留**"——与本设计"PTX-EMU 端必须修改、attach_timing 删除"互斥。同号双契约。

**用户拍板（2027-02-09）**：**重写 `hsk9-announcement-draft.md`** 为 SM 重构版（HSK-9 编号不变，ICOMPUTE_API_VERSION=1 保持；版本号不变，但语义质变在公告正文中显式标注为 breaking change）。

**HSK-9 内容（重写版）**：

> **HSK-9 公告正文（CppTLM → PTX-EMU）**：
>
> **摘要**：CppTLM 端 GPU 算力侧重构，删除 `IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming` 3 个 vendor 接口的 CppTLM 端实现，新增 SM 微架构（12 个 ChStream 子模块 + 8 种 Bundle + `IComputeDevice` 接口 12 方法）。PTX-EMU 端 `SMContext::exe_once()` 必须同步改造移除 `attach_timing()` 调用栈。
>
> **跨仓契约变更**：
> - 删除：`attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)` 1 个方法（CppTLM 端实现 + PTX-EMU 端 consumer 双边删除）
> - 新增：`set_instr_descriptor_buf(InstrDescriptor*, uint32_t)` 1 个方法（producer 侧由 PTX-EMU 端写入；consumer 侧由 SM 接收）
> - 保留 11 个方法（同构）
> - 总数：12 → **12** 方法（净不变）
> - **ICOMPUTE_API_VERSION=1 保持**（版本号未变，但本公告标注为**语义质变 breaking change**——HSK 纪律要求所有方法签名变更必须 bump 版本号，本变更恰好不触发——净新增 1 + 净删除 1 = 0 方法数变化。这是边界情况，故显式标注以避免 PTX-EMU 仓静态断言失败）
>
> **PTX-EMU 端必须修改**：
> 1. `src/ptxemu/device_api_impl.cc` 新增 `set_instr_descriptor_buf()` 实现（producer 侧）
> 2. `src/ptxsim/core/sm_context_cpptlm_inject.cpp` 移除 `attach_timing()` consumer 路径
> 3. 移除 `src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}` 跨仓注入头文件
> 4. `tests/integration/cpptlm/test_attach_timing_consumer_e2e.cpp` 等 5 个依赖 attach_timing 的测试必须重定位到 SM 重构版测试
>
> **版本号**：PTXEMU_API_VERSION=1（保持）+ ICOMPUTE_API_VERSION=1（保持，语义质变已标注）
>
> **退路**：如 PTX-EMU 端无法在 14 天协调窗口内同步改造，则 CppTLM 端**保留 3 个 vendor 接口的兼容 shim**（即 v1.0 compat shim），仅废弃 `KernelLaunchTLM` + `CudaCoreAdapterMVP` + `PtxEmuSubmoduleMVP` + 旧 WavefrontTLM/VectorRegFileTLM/MinimalWarpSchedulerTLM。**注意：这是退路方案，主推方案仍是大爆炸 + HSK-9**。
>
> **与既有 HSK-9 草稿的关系**：`docs/soc_arch/adr/hsk9-announcement-draft.md`（commit `4105602`）的"双轨并存、PTX-EMU 零修改"版本**被本公告 supersede**；该草稿文件**重写**为 SM 重构版正文（commit 与本设计同步推送）。

### 15.6.4 PTX-EMU 子模块公共头修改边界

- **允许**：PTX-EMU 端 facade 实现内部修改（`SMContext::exe_once()` 调用栈 + `device_api_impl.cc` `set_instr_descriptor_buf()` 实现）
- **允许**：`src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}` 重构或删除
- **不允许**：修改 `IPtxEmuDevice` 12 方法签名（`device_api.h:85-114` 冻结）
- **不允许**：修改 `PTXEMU_API_VERSION=1`
- **不允许**：删除 `IPtxEmuDevice` 类定义本身（per HSK-8 ACCEPTED）

### 15.6.5 SFU 单元归属（Oracle Round 2 P1-h 修订）

> **Oracle Round 2 指出**：12 子模块无 SFU 单元，但 Stage A `PipeClass` 有 7 值（含 `kSFU`）且 §15.2.3 对照表列了 SFU 管线。

**修订决策**：SFU 指令（`sin/cos/rcp/div/log/exp` 等）归 **`ScalarALU` 子模块内部 SFU 子单元**（per NV Blackwell SM_120 §5.1 中 SFU 是 P2 pipe 与 FP32/INT 共享 P0 dispatch slot 但独立 backend）。具体实现：ScalarALU 内部 `enum class SubPipe { kINT_FP32_Shared, kFP64, kSFU, kBranch }`，通过 `DecodeUnit::classify_sub_pipe()` 路由。

`PipeClass::kSpecialSync`（`s_waitcnt` / `s_barrier`）指令归 **`ScalarALU` 子模块**（per CDNA ISA manual §3.7.4：s_waitcnt 是 scalar instruction）。

---

## 15.7 删除范围

> **Oracle P0 修订**：原 v1.0-draft 删除清单仅 5 个测试被提及，但实际 grep 显示 **11 个测试文件**引用被删除/重命名模块 + 3 个 JSON config 引用旧 type 字符串。本节补全。

### 15.7.1 文件删除清单（11 测试 + 6 实现 + 3 vendor + 5 资产）

#### A. 实现 .hh/.cc 删除（11 项）

| # | 路径 | 类型 | 删除理由 |
|---|------|------|----------|
| 1 | `src/tlm/gpu/kernel_launch_tlm.cc` | .cc | KernelLaunchTLM 完全删除 |
| 2 | `include/tlm/gpu/kernel_launch_tlm.hh` | .hh | KernelLaunchTLM 完全删除 |
| 3 | `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `.cc` | .hh+cc | CudaCoreAdapterMVP 删除（HSK-6 已删桥接）|
| 4 | `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `.cc` | .hh+cc | PtxEmuSubmoduleMVP 删除 |
| 5 | `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc` | .hh+cc | PipelineTLM 删除（字符串查表已废弃）|
| 6 | `include/tlm/gpu/scoreboard_tlm.hh` + `src/tlm/gpu/scoreboard_tlm.cc` | .hh+cc | ScoreboardTLM 删除（被 HazardTracker 替代）|
| 7 | `include/tlm/gpu/tensor_core_tlm.hh` + `src/tlm/gpu/tensor_core_tlm.cc` | .hh+cc | TensorCoreTLM 删除（被 MatrixCoreUnit 替代）|
| 8 | `include/cudart/pipeline_interface.h` | vendor | HSK-4 vendor 接口删除 |
| 9 | `include/cudart/scoreboard_interface.h` | vendor | HSK-4 vendor 接口删除 |
| 10 | `include/cudart/tensor_core_interface.h` | vendor | HSK-4 vendor 接口删除 |
| 11 | `include/cudart/AGENTS.md` | doc | AGENTS 文档删除 |

#### B. 测试文件删除（14 项，Oracle Round 2 P1-c 修订）

| # | 路径 | 删除理由 |
|---|------|----------|
| 1 | `test/test_kernel_launch_tlm.cc` | **存在**（grep 确认）|
| 2 | `test/test_kernel_launch_ptx_integration.cc` | KernelLaunchTLM 测试（**[wave2] tag**——Stage A G-D2/G-D3 mock 6 用例随之消失）|
| 3 | `test/test_async_completion_adapter.cc` | async_completion_adapter 重设计后保留模块（D.2），但测试场景与旧 KernelLaunchTLM 耦合 → 整体删除并按新设计写新测试 |
| 4 | `test/test_cuda_core_adapter_timing.cc` | CudaCoreAdapterMVP 测试 |
| 5 | `test/test_latency_tlm_perf.cc` | PipelineTLM 性能测试 |
| 6 | `test/test_pipeline_tlm.cc` | PipelineTLM 测试 |
| 7 | `test/test_scoreboard_tlm.cc` | ScoreboardTLM 测试 |
| 8 | `test/test_tensor_core_tlm.cc` | TensorCoreTLM 测试 |
| 9 | `test/test_gpu_soc_tlm.cc` | GPU SoC 测试（引用 KernelLaunchTLM）|
| 10 | `test/test_gpu_compute_unit_tlm.cc` | GpuComputeUnitTLM 测试（被 SM 顶层替代）|
| 11 | `test/test_gpu_compute_unit_integration.cc` | 集成测试（引用 GpuComputeUnitTLM）|
| 12 | `test/test_gpu_soc_phase8a.cc` | GPU Phase 8A 测试（**[gpu] tag**，非 `[e2e]`）|
| 13 | `test/test_wavefront_tlm.cc` | WavefrontTLM 测试（模块被重构为 SIMTLane 内部）|
| 14 | `test/test_vector_regfile_tlm.cc` | VectorRegFileTLM 测试（模块被重构为 RegFileUnit）|
| 15 | `test/test_minimal_warp_scheduler_tlm.cc` | MinimalWarpSchedulerTLM 测试（模块被重构为 IssueUnit）|

#### C. JSON config 修改（3 项 + 事实修正，Oracle Round 2 P1-d 修订）

> **Oracle Round 2 指出**：`compute_unit_v1.json` **不存在** `"type": "GpuComputeUnitTLM"`（仅注释提及），`gpu_soc_gb203_v1.json` 同时实例化 `KernelLaunchTLM`（删除非改名）。事实修订如下：

| # | 路径 | 修改内容 |
|---|------|----------|
| 1 | `configs/vector_add_n1024.json` | `"type": "KernelLaunchTLM"` → `"type": "StreamingMultiprocessorTLM"`（实际确认）|
| 2 | `configs/templates/compute_unit_v1.json` | **仅注释修改**（"p2_placeholder" 字符串）；无 type 字符串修改 |
| 3 | `configs/templates/gpu_soc/gpu_soc_gb203_v1.json` | (a) 删除 `"type": "KernelLaunchTLM"` 模块条目（不是改名！）；(b) `"type": "GpuComputeUnitTLM"` → `"type": "StreamingMultiprocessorTLM"`；(c) 重写 `connections` 删除 kernel_launch 相关路由 |
| 4 | `examples/dgpu_soc_with_pcie_ip.json` | 同 #3 修订（待 grep 确认是否有引用）|

#### D. 旁路文件修复（不删但修复，Oracle Round 2 P1-g/i 修订）

| # | 路径 | 修复内容 |
|---|------|----------|
| 1 | `include/tlm/gpu/gpu_soc_tlm.hh` | 删除 `KernelLaunchTLM*` forward decl + setter/getter |
| 2 | `include/tlm/gpu/async_completion_adapter.hh` | 删除 `KernelLaunchTLM` 引用；adapter 改为 `IComputeDevice`（per §15.7.5）|
| 3 | `include/chstream_register.hh` | 删除 `KernelLaunchTLM` 等 5 个 registerObject 调用（per §15.7.4）；新增 SM 顶层 + 12 子模块 + 8 Bundle 适配器 |
| 4 | `src/tlm/gpu/gpu_soc_tlm.cc` | 删除 `KernelLaunchTLM` 引用 |
| 5 | `src/main.cpp` | 删除 `kernel_launch_tlm.hh` include + 任何直接使用 |
| 6 | `src/CMakeLists.txt` | 删除 6 个被删 `.cc` 引用；新增 12 个 SM 子模块 `.cc` |
| 7 | `test/CMakeLists.txt` | 删除 14 个被删 test 引用；新增 SM 子模块测试 |
| 8 | `test/python/test_gpgpu_sim_comparison.py` | 检查 `[gpu]` tag 测试是否引用被删模块（grep 验证后调整）|
| 9 | **DOC HYGIENE**（per AGENTS.md §doc hygiene + commit 15，Oracle Round 3 R3-P1-c 修订补 5 个 modules 文档）| |
| 9.1 | `AGENTS.md` STRUCTURE 节 + WHERE-TO-LOOK 表 + PHASE-STATE 表 + ADR 计数 | 同步 6 模块删除 + 14 子模块新增 |
| 9.2 | `docs/ONBOARDING.md` §5.5 脚本表 | 同步删除/新增 |
| 9.3 | `docs/soc_arch/modules/{gpu-kernel-launch,cuda-core-adapter,ptx-emu-submodule-mvp,dgpu-board,gpu-compute_unit,gpu-soc,gpu.common}.md` + `modules/README.md` | 物理删除 7 个 + README 同步 |
| 9.4 | `openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md` + `gpgpu-precision-wave2/spec.md` + `cli-f12b-flag/spec.md` 等 main specs | 标 superseded |
| 9.5 | `scripts/test/docs_sync_check.sh` VIRTUAL_PATHS | 添加 4 个新增 + 12 个删除条目 |
| 9.6 | `docs/architecture/01-hybrid-architecture-v2.1.md` 等 v2 架构文档 | 同步 GPU 算力侧模块列表（grep 实证无被删模块引用，标"无变更"）|
| 9.7 | `test/python/test_f12b_smoke.py` (L47) | 测试中 `kernel_launch` 引用 → 重定位到 `StreamingMultiprocessorTLM` 测试场景 |
| 9.8 | `scripts/test/test_ptx_emu.sh` (per Oracle R2 P2 grep) | 验证 PTX-EMU 测试脚手架是否引用被删模块 |

### 15.7.2 重构（不删除但重建）

| # | 路径 | 重构内容 |
|---|------|----------|
| 1 | `include/tlm/gpu/gpu_compute_unit_tlm.hh` + `.cc` | **重命名为** `include/tlm/gpu/streaming_multiprocessor_tlm.hh` + `.cc`，持有 12 个子模块 |
| 2 | `include/tlm/gpu/wavefront_tlm.hh` + `.cc` | 重新设计为 SIMTLane 内部状态载体（仍 ChStreamModuleBase）|
| 3 | `include/tlm/gpu/minimal_warp_scheduler_tlm.hh` + `.cc` | 重新设计为 IssueUnit 内部调度器 |
| 4 | `include/tlm/gpu/vector_regfile_tlm.hh` + `.cc` | 重新设计为 RegFileUnit（支持 32/64 双宽度 + ACCVGPR）|

### 15.7.3 新增（12 子模块 + 8 Bundle + IComputeDevice）

**12 子模块**（`include/tlm/gpu/sm/<unit>_tlm.{hh,cc}`）：
- fetch_unit_tlm / decode_unit_tlm / issue_unit_tlm
- scalar_alu_tlm / vector_alu_tlm / matrix_core_tlm
- simt_lane_tlm / lsu_global_tlm / lsu_lds_tlm
- reg_file_unit_tlm / writeback_unit_tlm / hazard_tracker_tlm

**8 Bundle**（`include/bundles/sm_bundles_tlm.hh`）：
- FetchToIssueBundle, DecodeToIssueBundle, IssueToExecBundle, ExecToWritebackBundle
- WritebackToRegFileBundle, MemoryReqBundle, MemoryRespBundle, ScoreboardQueryBundle

**IComputeDevice**（`include/tlm/gpu/i_compute_device.hh`）：
- 12 方法（per §15.5.3——含 `set_instr_descriptor_buf` 新增 + `attach_timing` 删除 = 净 12，**与 IPtxEmuDevice 同构**）

### 15.7.2 重构（不删除但重建）

| # | 路径 | 重构内容 |
|---|------|----------|
| 1 | `include/tlm/gpu/gpu_compute_unit_tlm.hh` + `.cc` | **重命名为** `include/tlm/gpu/streaming_multiprocessor_tlm.hh` + `.cc`，持有 12 个子模块 |
| 2 | `include/tlm/gpu/wavefront_tlm.hh` + `.cc` | 重新设计为 SIMTLane 内部状态载体（仍 ChStreamModuleBase）|
| 3 | `include/tlm/gpu/minimal_warp_scheduler_tlm.hh` + `.cc` | 重新设计为 IssueUnit 内部调度器 |
| 4 | `include/tlm/gpu/vector_regfile_tlm.hh` + `.cc` | 重新设计为 RegFileUnit（支持 32/64 双宽度 + ACCVGPR）|

### 15.7.3 新增（12 子模块 + 8 Bundle + IComputeDevice）

**12 子模块**：
- `include/tlm/gpu/sm/fetch_unit_tlm.{hh,cc}`
- `include/tlm/gpu/sm/decode_unit_tlm.{hh,cc}`
- `include/tlm/gpu/sm/issue_unit_tlm.{hh,cc}`
- `include/tlm/gpu/sm/scalar_alu_tlm.{hh,cc}`
- `include/tlm/gpu/sm/vector_alu_tlm.{hh,cc}`
- `include/tlm/gpu/sm/matrix_core_tlm.{hh,cc}`
- `include/tlm/gpu/sm/simt_lane_tlm.{hh,cc}`
- `include/tlm/gpu/sm/lsu_global_tlm.{hh,cc}`
- `include/tlm/gpu/sm/lsu_lds_tlm.{hh,cc}`
- `include/tlm/gpu/sm/reg_file_unit_tlm.{hh,cc}`
- `include/tlm/gpu/sm/writeback_unit_tlm.{hh,cc}`
- `include/tlm/gpu/sm/hazard_tracker_tlm.{hh,cc}`

**8 Bundle**（`include/bundles/sm_bundles_tlm.hh`）：
- FetchToIssueBundle, DecodeToIssueBundle, IssueToExecBundle, ExecToWritebackBundle
- WritebackToRegFileBundle, MemoryReqBundle, MemoryRespBundle, ScoreboardQueryBundle

**IComputeDevice**（`include/tlm/gpu/i_compute_device.hh`）：
- 12 方法（per §15.5.3——含 `set_instr_descriptor_buf` 新增 + `attach_timing` 删除 = 净 12）

### 15.7.4 chstream_register.hh 修改

```cpp
// 删除
ModuleFactory::registerObject<tlm::KernelLaunchTLM>("KernelLaunchTLM");      // 删
ModuleFactory::registerObject<tlm::WavefrontTLM>("WavefrontTLM");            // 删（变 SIMTLane 子模块）
ModuleFactory::registerObject<tlm::VectorRegFileTLM>("VectorRegFileTLM");    // 删（变 RegFileUnit 子模块）
ModuleFactory::registerObject<tlm::MinimalWarpSchedulerTLM>("MinimalWarpSchedulerTLM"); // 删（变 IssueUnit 子模块）
ModuleFactory::registerObject<tlm::GpuComputeUnitTLM>("GpuComputeUnitTLM");  // 改名为 SM 顶层
ChStreamAdapterFactory::get().registerAdapter<KernelLaunchTLM, ...>        // 删
// ... 适配器删除

// 新增
ModuleFactory::registerObject<tlm::StreamingMultiprocessorTLM>("StreamingMultiprocessorTLM"); // 新 SM 顶层
ModuleFactory::registerObject<tlm::sm::FetchUnitTLM>("FetchUnitTLM");         // 12 子模块注册
// ... 11 个子模块注册
ChStreamAdapterFactory::get().registerAdapter<StreamingMultiprocessorTLM,
    FetchToIssueBundle, DecodeToIssueBundle, IssueToExecBundle, ...>        // 8 Bundle 适配器
```

### 15.7.5 modules_cluster.hh 修改

```cpp
// ComputeCluster 不再 instantiate KernelLaunchTLM，改为 instantiate SM 顶层
// 9 个 SimModule 派生类保持不变
```

### 15.7.6 CMakeLists 修改

- `src/CMakeLists.txt` 删除 `tlm/gpu/kernel_launch_tlm.cc` / `pipeline_tlm.cc` / `scoreboard_tlm.cc` / `tensor_core_tlm.cc` / `cuda_core_adapter_mvp.cc` / `ptx_emu_submodule_mvp.cc`
- 新增 12 个 SM 子模块 `.cc`
- `test/CMakeLists.txt` 删除 5 个测试，新增 12 个 SM 子模块测试 + 1 个 SM 顶层集成测试

---

## 15.8 测试策略

### 15.8.1 测试层级

| 层级 | 测试文件 | 标签 | 数量 |
|------|----------|------|------|
| **L1 子模块单测** | 12 个 `test/test_sm_<unit>_tlm.cc` | `[sm-unit]` | 36+ assertions |
| **L2 Bundle 连接测试** | `test/test_sm_bundle_wiring.cc` | `[sm-bundle]` | 20+ assertions |
| **L3 SM 顶层集成** | `test/test_streaming_multiprocessor_tlm.cc` | `[sm-top]` | 30+ assertions |
| **L4 IComputeDevice 步进** | `test/test_i_compute_device_stepping.cc` | `[sm-icompute]` | 25+ assertions |
| **L5 CDNA waitcnt 计数** | `test/test_cdna_hazard_tracker.cc` | `[sm-cdna]` | 15+ assertions |
| **L6 端到端** | `test/test_sm_ptx_emu_e2e.cc` | `[sm-e2e]` | 20+ assertions |

### 15.8.2 关键测试场景

#### L1 子模块测试（per 子模块）

| 子模块 | 测试场景 |
|--------|----------|
| FetchUnit | (a) 抓取指令 (b) PC 边界检查 (c) instr buf 空时阻塞 |
| DecodeUnit | (a) PTX 指令分类 (b) CDNA MFMA 分类 (c) LatencyClass 推断 |
| IssueUnit | (a) round-robin 调度 (b) CGGTY 阈值 (c) warpid % 4 静态绑定 |
| ScalarALU | (a) FFMA 延迟 (b) IMAD 延迟 (c) 多 warps 并发 |
| VectorALU | (a) VIADD.U8x4 延迟 (b) FMNMX 路径 (c) V-pipe 共发 |
| MatrixCore | (a) MFMA 延迟 (b) ACCVGPR 写入 (c) v_mfma_* 20 条子集 |
| SIMTLane | (a) EXEC mask 64-bit (b) SIMT 分歧检测 (c) lane 活跃状态 |
| LsuGlobal | (a) global_load → IMemoryPort (b) 异步回调处理 (c) vmcnt 递减 |
| LsuLDS | (a) LDS 同步访问 (b) bank conflict 检测 (c) intra-SM 路径 |
| RegFileUnit | (a) 32-bit 读 (b) 64-bit 写 (c) ACCVGPR 累加 |
| WritebackUnit | (a) 写回 RegFile (b) release HazardTracker (c) 写回顺序 |
| HazardTracker | (a) kVirtualReg RAW hazard (b) kHardwareCounter vmcnt (c) s_waitcnt 等待 |

#### L3 SM 顶层集成

- 5 类 microbenchmark vs gpgpu-sim 基准（per architecture/12 §12.2）
- 5 warps × 32 线程 PTX 步进测试
- 4 warps × 64 线程 CDNA 步进测试
- 内存访问 + s_waitcnt 集成测试

#### L4 IComputeDevice 步进

- 12 方法 smoke test（每个方法至少 1 assertion；含 `set_instr_descriptor_buf` 新增 + `attach_timing` 缺位）
- 1 tick = 1 cycle 契约（与 Wave 2 G-D3 对齐）
- PTX-EMU facade 兼容性测试（与 HSK-8 同构）

#### L7 JSON config 兼容性

- 3 个 JSON config 重新加载测试（`vector_add_n1024.json` / `compute_unit_v1.json` / `gpu_soc_gb203_v1.json`）

#### L6 端到端

- PTX-EMU 通过 IComputeDevice 步进 → CppTLM SM 仿真 → 完成 kernel 仿真
- 与 §11.5 阶段 D Gate D 校准基线对齐

---

## 15.9 实施策略：大爆炸重写

### 15.9.1 提交顺序（多原子 commit）

虽然用户选择"大爆炸重写"，但 git-master 原则要求**多原子 commit**。建议 7 个原子 commit 拆分：

```
commit 1: docs(adr): 修订 ADR-SOC-02 反转黑盒决策（新增 ADR-SOC-16 背书）
commit 2: docs(soc_arch): 新增 15-sm-microarchitecture-design (本文档)
commit 3: refactor(tlm): 删除 KernelLaunchTLM + CudaCoreAdapterMVP + PtxEmuSubmoduleMVP
commit 4: refactor(tlm): 删除 PipelineTLM + ScoreboardTLM + TensorCoreTLM + 3 vendor头文件
commit 5: feat(sm): 新增 12 ChStream 子模块（基础版本，stub 实现）
commit 6: feat(bundles): 新增 8 种 SM Bundle（sm_bundles_tlm.hh）
feat(sm): 完整实现 12 ChStream 子模块（连接 Bundle）
commit 7: feat(tlm): 新增 IComputeDevice + StreamingMultiprocessorTLM 顶层容器
commit 8: feat(register): chstream_register.hh 注册 12 子模块 + SM 顶层 + 8 Bundle 适配器
commit 9: refactor(tests): 删 5 旧测试，新增 12 SM 子模块测试 + 5 集成测试
commit 10: feat(docs): 更新架构文档 + OpenSpec change 状态
```

**总工作量**：15-20 人天（per 用户选择"大爆炸" + 多原子 commit 拆分），但 Oracle P1 评估认为更现实是 **25-30 人天**——ADR-SOC-15 §3 D3 估算阶段 B 内存 Seam 单独就 8-15 天人，本设计合并阶段 A+B 的 12 子模块 + 8 Bundle 实装 + 18 删除 + 11 测试补足 + HSK-9 协调，成本至少 25-30 天。

### 15.9.1 提交顺序（**每 commit 独立可编译**，Oracle Round 3 R3-P1-d 修订）

> **Oracle Round 3 指出**："commit 11 删 `kernel_launch_tlm.hh`，但 `gpu_soc_tlm.cc:8` 和 `main.cpp:16` include 它，旁路修复在 commit 13——commit 11/12 必然编译断裂。修复顺序应对调（13 先于 11）"。

**重排 commit 计划（按依赖顺序，每步可编译）**：

| # | Commit | 内容 | 可编译性 | 依赖 |
|---|--------|------|----------|------|
| 1 | `docs(adr): ADR-SOC-16 背书 SM 重构 + 修订 ADR-SOC-02 Status Update` | 仅文档 | ✅ N/A | — |
| 2 | `docs(soc_arch): 修订 15-sm-microarchitecture-design + hsk9-announcement-draft.md` | 仅文档 | ✅ N/A | commit 1 |
| 3 | `feat(openspec): 启动 cpptlm-dgpu-d1-cdna-isa-sm-rewrite change + 标 phase-a superseded` | OpenSpec 流程 | ✅ N/A | commit 2 |
| 4 | `feat(tlm): 新增 SM 顶层容器 StreamingMultiprocessorTLM + IComputeDevice 接口 14 方法 (stub 实现)` | **新增 + stub** | ✅ 编译通过（SM 内部子模块都是 stub）| commit 2 |
| 5 | `feat(sm): 新增 12 个 ChStream 子模块 stub (.hh + 空 .cc)` | **新增 + stub** | ✅ 编译通过 | commit 4 |
| 6 | `feat(bundles): 新增 sm_bundles_tlm.hh 8 种 Bundle 定义` | **新增** | ✅ 编译通过 | commit 5 |
| 7 | `feat(sm): 完整实现 12 个 ChStream 子模块（连接 Bundle）` | **填充** | ✅ 编译通过 | commit 6 |
| 8 | `feat(register): chstream_register.hh 注册 SM 顶层 + 12 子模块 + 8 Bundle 适配器 (新增 side)` | **新增注册** | ✅ 编译通过 | commit 7 |
| 9 | `refactor(gpu_compute_unit): GpuComputeUnitTLM 重构为 SM 内部状态机（保留旧 API 作为 SM 顶层内部调用，chstream_register 注销旧名）` | **重构 + 注销** | ✅ 编译通过 | commit 8 |
| 10 | `refactor(legacy): 重构 WavefrontTLM/MinimalWarpSchedulerTLM/VectorRegFileTLM 为 SM 内部子模块（chstream_register 注销旧名）` | **重构 + 注销** | ✅ 编译通过 | commit 9 |
| 11 | **`refactor(headers): 修复 gpu_soc_tlm.hh + async_completion_adapter.hh + main.cpp 旁路依赖（改接 IComputeDevice，去除 KernelLaunchTLM 引用）`** | **修复** | ✅ 编译通过 | commit 10 |
| 12 | `refactor(soc): 删除 KernelLaunchTLM + CudaCoreAdapterMVP + PtxEmuSubmoduleMVP` | **删除** | ✅ 编译通过 | **commit 11** |
| 13 | `refactor(tlm): 删除 PipelineTLM + ScoreboardTLM + TensorCoreTLM + 3 vendor 接口头文件 (include/cudart/)` | **删除** | ✅ 编译通过 | commit 12 |
| 14 | `chore(configs): 修订 4 个 JSON config (vector_add_n1024, compute_unit_v1 注释, gpu_soc_gb203_v1 modules + connections, dgpu_soc_with_pcie_ip 验证)` | | ✅ 编译通过 | commit 13 |
| 15 | `chore(docs): AGENTS.md STRUCTURE + ONBOARDING.md + docs/soc_arch/modules/{gpu-kernel-launch,cuda-core-adapter,ptx-emu-submodule-mvp,dgpu-board,gpu-compute_unit,gpu-soc,gpu.common}.md + modules/README.md + openspec/main specs/{cpptlm-d1-p1-pipeline-scoreboard,gpgpu-precision-wave2,cli-f12b-flag}.md + scripts/test/docs_sync_check.sh VIRTUAL_PATHS (DOC HYGIENE 全套)` | **文档同步** | ✅ 编译通过 | commit 14 |
| 16 | `refactor(tests): 删除 15 旧测试文件` | **测试删除** | ✅ 编译通过 | commit 14 |
| 17 | `feat(tests): 新增 12 SM 子模块单测 + L2-L6 集成测试 + L7 JSON reload 测试 + test_f12b_smoke 重定位` | **新增测试** | ✅ 编译通过 | commit 16 |
| 18 | `feat(tlm): 完整实现 IComputeDevice 与 14 方法 (含 get_register_value 读路径 + is_instruction_completed 就绪协议) + set_instr_descriptor_buf 同步协议 + Execute 单元真实逻辑` | **完整实现** | ✅ 编译通过 | commit 17 |
| 19 | `chore(openspec): archive cpptlm-dgpu-d1-cdna-isa-sm-rewrite + archive cpptlm-dgpu-d1-cdna-isa-phase-a (superseded)` | OpenSpec 收尾 | ✅ 编译通过 | commit 18 |
| 20 | `chore(commit): HSK-9 公告正式发布到 PTX-EMU 仓 docs/superpowers/specs/ + PTX-EMU 端 sm_context.cpp/2cpptlm_inject.cpp 改造` | 跨仓协调 | ✅ N/A | commit 19 |

**总 20 commits**，每 commit 都可独立编译（按依赖图，关键修复：commit 11 修复旁路先于 commit 12 删头）。

### 15.9.2 风险缓解（Oracle 修订）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 单次 PR 太大（25+ 文件改动）| 🟡 中 | 20 个原子 commit 拆分（per §15.9.1）；每 commit 独立可编译 |
| **R2** | 23 ABI 边界误触 | 🔴 高 | 每 commit 前静态检查 ABI 头文件 `git diff` |
| **R3** | PcieEndpointIP 17 端口破坏 | 🔴 高 | 不动 PcieEndpointIP；仅改 GPU compute 侧 |
| **R4** | PTX-EMU `SMContext::exe_once()` 移除 `attach_timing()` 调用栈 | 🔴 高 | HSK-9 协调；PTX-EMU 端必须同步修改（commit 20）|
| **R5** | 大爆炸回滚成本 | 🟡 中 | 20 原子 commit 可逐个 revert（语义独立）|
| **R6** | 测试覆盖率断裂（删除 14 测试无补偿）| 🟡 中 | 12 子模块单测 + L2-L6 + L7 测试覆盖（146+ assertions）|
| **R7** | CDNA mode kHardwareCounter 复杂度 | 🟡 中 | Phase 1 仅实现 kVirtualReg；kHardwareCounter 阶段 C 启用 |
| **R8** | NV Blackwell scoreboard depth ≥ 12 模拟缺失 | 🟢 低 | 阶段 1 不模拟 17-bit 控制码（per §15.2.3 差异）|
| **R9** | ComputeCluster 上游重构缺失 | 🟢 低 | per 用户决策"后续再规划 CommandProcessor" |
| **R10** | JSON config type 字符串兼容性 | 🟡 中 | per §15.7.1.C 修订（事实修正版）|
| **R11** | gpu_soc_tlm.hh forward decl 残留 | 🟢 低 | per §15.7.1.D 同步修复 |
| **R12** | OpenSpec change `cpptlm-dgpu-d1-cdna-isa-phase-a` 状态混淆 | 🟡 中 | per §15.10.2 explicit supersede 决策 |
| **R13** | HSK-9 编号双义冲突 | 🔴 高 | 重写 `hsk9-announcement-draft.md` 为 SM 重构版（per 用户决策 P0-2）|
| **R14** | 寄存器双真值发散 | 🔴 高 | SM 持值单真值源 + set_instr_descriptor_buf 同步（per §15.5.2 + §15.5.6）|
| **R15** | DOC HYGIENE 不全 | 🟡 中 | commit 15 一次性覆盖 AGENTS.md / ONBOARDING / docs/soc_arch/modules / openspec/main specs / scripts/test/docs_sync_check.sh VIRTUAL_PATHS |
| **R16** | PTX-EMU 侧 SMContext 改造未估算 | 🟡 中 | per Oracle Round 2 P2 修订——PTX-EMU 侧工作量另估（5-10 人天）|

---

## 15.10 Gate 验证清单（替代 ADR-SOC-15 阶段 A/B）

### 15.10.1 大爆炸 Gate

- [ ] 23 ABI 头文件 `git diff` 确认零修改
- [ ] PcieEndpointIP 17 端口布局不变
- [ ] `[pcie]/[axi]` 测试保持基线 100% 通过
- [ ] `[gpu]` 新基线（移除 15 旧测试后）100% 通过
- [ ] `[sm-microarch]` 新增测试 100% 通过
- [ ] `[wave2]` 基线影响（`test_kernel_launch_ptx_integration.cc` 删除波及 G-D2/G-D3 mock 6 用例）—— 需重新生成 mock-only 替代测试
- [ ] `[pcie-e2e]` 测试保持基线 100% 通过（无 GPU 算力侧依赖）
- [ ] `openspec validate` PASS
- [ ] Oracle 评审确认 0 P0 + ≤ 3 P1 风险
- [ ] ADR-SOC-16 (新) 发布背书 SM 重构决策
- [ ] **JSON reload L7 测试** 100% 通过（4 个 config 加载无错）
- [ ] **`docs_sync_check.sh --strict`** 通过（DOC HYGIENE 不变量）
- [ ] **PTX-EMU 侧 CI 绿**（PTX-EMU 仓内 HSK-9 consumer 测试通过）
- [ ] **PTX-EMU 端 functional 闭环验证**（`get_register_value()` 读路径 + `is_instruction_completed()` 就绪协议 + `set_instr_descriptor_buf()` 同步 → PTX-EMU functional 结果与 SM timing 一致性）

### 15.10.2 Stage A OpenSpec change supersede（Oracle Round 3 R3-P2 mapping 修订）

> **Oracle Round 3 指出**：tasks.md 实有 8 节（## 1..## 8），fate 表"§7 Stage A Gate"对应 tasks.md §8；tasks.md §7（文档同步+OpenSpec 收尾）未被映射。

**完整 supersede 决策（覆盖全部 8 task 节）**：

| Stage A task 节 | 命运 | 原因 |
|----------------|------|------|
| **§1 InstrDescriptor POD** | ✅ **保留** | 本设计 §15.4.2 + §15.5.6 Bundle 字段需要（含 ISA 判别位 + instr_id）|
| **§2 CdnaPipelineTLM** | ❌ **死亡** | vendor `IPipelineLatencyProvider` 接口已删除 |
| **§3 ScoreboardTLMv2** | ❌ **死亡** | vendor `IHazardTracker` 接口已删除 |
| **§4 CudaCoreAdapterMVP 双轨注入（KernelLaunchTLM v2 setters）**| ❌ **死亡** | KernelLaunchTLM 整体删除；v2 setters 宿主不存在 |
| **§5 PtxStringToDescriptor（如果存在）**| ⚠️ **保留概念，重写实现** | PTX-EMU 解码后通过 `set_instr_descriptor_buf()` 注入 |
| **§6 test_pipeline_parity + 回归门禁** | ❌ **死亡** | PipelineTLM 已删除；bit-identical 契约无对象 |
| **§7 文档同步 + OpenSpec 收尾** | ✅ **重定位到 commit 15 + 19** | SM 重构的 DOC HYGIENE（commit 15）+ archive（commit 19）覆盖 |
| **§8 Stage A Gate 验证（Oracle 评审 + 全量回归）**| ⚠️ **重定位** | 整体 Gate 任务移至 SM 重构的 §15.10.1 大爆炸 Gate |

**OpenSpec 处置**：`cpptlm-dgpu-d1-cdna-isa-phase-a` 标记为 **superseded by `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`**（新 OpenSpec change 启动），不 archive 为 completed（因为 51/56 tasks 未实施；supersede 是显式标注任务交付物失效原因）。

### 15.10.3 PTX-EMU 集成清理 Gate（含 HSK-9）

- [ ] `include/cudart/` 目录物理删除（3 个 vendor 接口 + AGENTS.md）
- [ ] `src/tlm/gpu/{kernel_launch_tlm,pipeline_tlm,scoreboard_tlm,tensor_core_tlm,cuda_core_adapter_mvp,ptx_emu_submodule_mvp}.cc` 物理删除
- [ ] 14 个旧测试文件物理删除
- [ ] 4 个 JSON config type 字符串 / connections 改写（事实修正版）
- [ ] **`hsk9-announcement-draft.md` 重写**（per 用户决策 P0-2 + §15.6.3）
- [ ] **HSK-9 公告发布**（PTX-EMU 端必须同步改造 `SMContext::exe_once()` 移除 `attach_timing()` 路径；commit 20）
- [ ] DOC HYGIENE 9 项全绿（per §15.7.1.D）
- [ ] 阶段 B/C/D 路线图仍有效（SM 重构完成阶段 A 抽象 + 提供 B/C 实施基础）

### 15.10.4 OpenSpec 工作流（Oracle Round 2 P1 修订）

> **Oracle Round 2 指出**："AGENTS.md anti-patterns 要求重大变更走 `openspec/changes/` 提案；本设计是 ≥2 模块 + 公共注册字符串变更，必须有独立 OpenSpec change"。

**新建 OpenSpec change**：`cpptlm-dgpu-d1-cdna-isa-sm-rewrite`

- proposal.md: 反转 ADR-SOC-02 黑盒决策 + 新增 SM 微架构（per §15.2-§15.6）
- design.md: SM 子模块分解 + Bundle + IComputeDevice 14 方法 + 23 ABI 保护
- specs/sm-microarchitecture/spec.md: 12 子模块 + 8 Bundle + IComputeDevice 14 方法 requirement
- tasks.md: 20 个原子 commit task 列表（per §15.9.1，每 commit 可编译）
- supersedes: `cpptlm-dgpu-d1-cdna-isa-phase-a`（保留 InstrDescriptor/LatencyClass 等基础抽象）

---

## 15.11 参考文献

### 15.11.1 内部参考

| 文档 | 链接 |
|------|------|
| CDNA 真实 ISA 集成 | `docs/soc_arch/architecture/11-cdna-real-isa-integration.md` |
| CDNA 演进路线图 | `docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md` |
| CDNA-EMU 选型 | `docs/soc_arch/architecture/13-cdna-emu-selector.md` |
| 阶段 A 架构溯源 | `docs/soc_arch/architecture/14-stage-a-architecture-audit.md` |
| ADR-SOC-02 (黑盒优先) | `docs/soc_arch/adr/ADR-SOC-02-cu-granularity.md` |

### 15.11.2 业界参考

| 资料 | 链接 |
|------|------|
| NV Blackwell SM_120 Cycle-Level Study | https://zartbot.github.io/micro_arch/nvidia/sm_120/paper.html#sec5 |
| AMD CDNA3 ISA Reference (MI300X) | https://www.amd.com/content/dam/amd/en/documents/instinct-tech-docs/instruction-set-architectures/amd-instinct-mi300-cdna3-instruction-set-architecture.pdf |
| AMD GPU Open ISA Specs | https://gpuopen.com/machine-readable-isa/ |
| gpgpu-sim shader_core_ctx.cc | (gpgpu-sim_distribution 项目) |
| MGPUSim CDNA3 GFX942 | https://github.com/sarchlab/mgpusim |

### 15.11.3 OpenSpec

| Change | 状态 |
|--------|------|
| `cpptlm-dgpu-d1-cdna-isa-phase-a` | 📋 **superseded** by SM rewrite（保留 InstrDescriptor 等基础抽象，删除 CdnaPipelineTLM/ScoreboardTLMv2 交付物） |
| `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` | 🆕 **新建**（per §15.10.4）|
| `cpptlm-dgpu-d1-cdna-isa-phase-b` | 📋 待启动（IMemoryPort 由 SM 重构承接） |
| `cpptlm-dgpu-d1-cdna-isa-phase-c` | 📋 待启动（CDNA-EMU 实装由 SM 重构承接） |

---

## 15.12 Oracle 评审修复汇总（Round 1 + Round 2 + Round 3）

### 15.12.1 Round 1 P0 修复（4 项全部修复，2027-02-09 commit `06dcc63`）

| # | Oracle Round 1 P0 问题 | 修复章节 | Round 2 验证 |
|---|------------------------|----------|---------------|
| **R1-P0-1** | `IComputeDevice` 方向反转 vs architecture/11 + ADR-SOC-15 | §15.5.0 修订 | ⚠️ 部分关闭（§15.5.3 末行矛盾）|
| **R1-P0-2** | Functional/timing state ownership 未定义 | §15.5.2 修订 | ❌ 残留为 R2-N-P0-1（设计矛盾）|
| **R1-P0-3** | §15.5.3 mapping table 错误（real `IPtxEmuDevice` 签名）| §15.5.3 完全重写 | ✅ 关闭 |
| **R1-P0-4** | HSK-9 自相矛盾 + 删除 5 测试无补偿 + JSON config 不被考虑 | §15.6.3 修订 | ❌ 残留为 R2-N-P0-2（编号冲突）|

### 15.12.2 Round 2 P0 + P1 修复（用户决策后，2027-02-09 commit Round 3）

| # | Oracle Round 2 问题 | 用户决策 / 修复章节 | Round 3 待评审 |
|---|---------------------|-------------------|----------------|
| **R2-N-P0-1** | 寄存器值双所有者（§15.5.2 vs §15.3.3.4/§15.3.3.7/§15.3.3.8/§15.4.1 设计矛盾） | **决策 SM 持值单真值源** + §15.5.2 重写 + 新增 §15.5.6 同步协议 | [ ] |
| **R2-N-P0-2** | HSK-9 编号双义冲突（与 `hsk9-announcement-draft.md` 旧版互斥）| **决策 重写草稿** + `hsk9-announcement-draft.md` 全文重写为 SM 重构版 + §15.6.3 修订 | [ ] |
| **R2-P1-a** | §15.5.3 末行"PTX-EMU facade 不需调整"自相矛盾 | §15.5.3 末段修订为"必须修改" | [ ] |
| **R2-P1-b** | Memory load value 回路未定义 | §15.5.6 同步协议明确（上行字段 `memory_data`）| [ ] |
| **R2-P1-c** | 漏 3 测试（`test_wavefront_tlm.cc` 等） | §15.7.1.B 删除清单补全到 14 项 | [ ] |
| **R2-P1-d** | JSON config 事实错误（`compute_unit_v1.json` 无 `"type": "GpuComputeUnitTLM"`，`gpu_soc_gb203_v1.json` 同时实例化 `KernelLaunchTLM`）| §15.7.1.C 修订为事实正确版 | [ ] |
| **R2-P1-e** | Stage A fate 表漏 3 task 组 | §15.10.2 补全到 7 task 组全覆盖 | [ ] |
| **R2-P1-f** | commit 计划不可逐个编译 | §15.9.1 重排为 20 commit 依赖图 | [ ] |
| **R2-P1-g** | `async_completion_adapter` 保留但测试删除 | §15.7.1.D #2 明确改接 IComputeDevice + 新测试在 commit 17 | [ ] |
| **R2-P1-h** | 无 SFU 单元归属 | §15.6.5 新增：SFU 子单元在 ScalarALU 内部 | [ ] |
| **R2-P1-i** | DOC HYGIENE 不全 | §15.7.1.D #9 9 项 + commit 15 覆盖 | [ ] |
| **R2-P2 (5)** | 重复 §15.7.2/15.7.3 + B 表 12 vs 11 + "12 ABI" 复发 + Gate 缺 [wave2] + ISA 判别位 | §15.7.1.B 标题改 14 项 + §15.10.1 加 [wave2] + §15.5.6 新增 ISA 判别位字段 | [ ] |

### 15.12.3 Round 3 待评审项（用户决策后，Round 4 验证）

> **Oracle Round 3 评审结论**：1 残留 P0（R3-N-P0-1 寄存器读路径+就绪协议）+ 4 残留 P1（commit 顺序 + DOC HYGIENE + HSK-9 草稿方法清单 + 跨仓文件清单）

- [x] ~~SM-owns-state 模式（用户决策）+ §15.5.6 set_instr_descriptor_buf 同步协议是否完备~~ → **Round 4 已修复**（IComputeDevice 扩 2 方法：get_register_value + is_instruction_completed）
- [x] ~~§15.7.1 完整删除清单是否真的完整~~ → **Round 4 已修订**（14→15 测试 + 4 JSON + 7 modules docs + 9.7-9.8 test scripts）
- [x] ~~§15.9.1 20-commit 依赖图是否每步可编译~~ → **Round 4 已修复**（commit 11 修复旁路先于 commit 12 删头）
- [x] ~~重写版 hsk9-announcement-draft.md supersede 旧版的措辞与 HSK 纪律是否一致~~ → **Round 4 待修**（草稿方法表与代码块 12 方法不一致 → 已加注 R3-P1-a 待 HSK-9 草稿正文 commit 同步）
- [x] ~~§15.10 Gate 列表（13 项）是否覆盖 Oracle Round 2 全部新增 gap~~ → **Round 4 已扩到 14 项**（新增 PTX-EMU functional 闭环验证）

---

## Status Update
- **2027-02-09 (Round 1)**: 📋 Proposed v1.0-draft。Oracle Round 1 评审出 4 P0 + 9 P1 + 5 P2 缺陷。
- **2027-02-09 (Round 2 修订)**: 📋 Proposed v2.0-draft。已修复 Oracle Round 1 全部 4 P0 + 关键 P1。
- **2027-02-09 (Round 3 修订)**: 📋 Proposed v3.0-draft。**用户决策**：(1) SM 持寄存器值单真值源 + PTX-EMU 通过 `set_instr_descriptor_buf` 上行同步；(3) 重写 HSK-9 草稿为 SM 重构版。已修复 Oracle Round 2 全部 2 P0 + 9 P1 + 5 P2。
- **2027-02-09 (Round 4 修订)**: 📋 Proposed v4.0-draft。**用户决策**：(1) IComputeDevice 扩 2 方法（`get_register_value()` + `is_instruction_completed()`）解决 SM-owns-state 读路径+就绪协议。修复 Oracle Round 3 残留 P0 + 4 P1：§15.5.6 同步协议完整化（双向）+ commit 11/13 顺序对调 + DOC HYGIENE 9.3/9.7/9.8 补 5 个遗漏 + §15.10.2 Stage A fate 表 8 节全覆盖 + §15.10.1 Gate 14 项 + InstrDescriptor 加 ISA 判别位。
- **前置**: 阶段 A OpenSpec change `cpptlm-dgpu-d1-cdna-isa-phase-a`（commit `e607814`）+ 9 个架构文档（commits `5a9eb4c`/`3c76398`/`4105602`/`5efbc6a`/`7708d9f`/`4549ac9`/`1b8a318`/`06dcc63`/`6881cfd`）+ ADR-SOC-15 路线图（commit `3c76398`）。
- **后续**: 用户 review → writing-plans skill 启动 → 实施 20 原子 commit（每步可编译）→ OpenSpec `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` 实施 → HSK-9 公告正式发布 + ADR-SOC-16 背书。