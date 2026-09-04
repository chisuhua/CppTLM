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

### 15.5.1 12 方法（与 IPtxEmuDevice 同构）

```cpp
class IComputeDevice {
public:
    virtual ~IComputeDevice() = default;
    
    // 生命周期
    virtual bool initialize(const DeviceConfig& cfg) = 0;
    virtual void shutdown() = 0;
    virtual void reset() = 0;
    
    // 单步推进（CppTLM tick 驱动）
    virtual int  exe_once() = 0;
    virtual int  sm_exe_once(uint32_t sm_id) = 0;
    virtual int  wave_exe_once(uint32_t sm_id, uint32_t wave_id) = 0;
    
    // 指令缓冲（PTX-EMU 解码后的 InstrDescriptor 写入 SM）
    virtual void set_instr_descriptor_buf(const InstrDescriptor* buf, uint32_t count) = 0;
    
    // 状态查询
    virtual bool is_wave_stalled(uint32_t sm_id, uint32_t wave_id) = 0;
    virtual bool is_finished() = 0;
    virtual uint64_t get_cycle_count() const = 0;
    
    // SIMT 控制（PTX SIMT stack / CDNA EXEC mask）
    virtual void set_active_mask(uint32_t sm_id, uint32_t wave_id, uint64_t mask) = 0;
};
```

### 15.5.2 数据流

1. **PTX-EMU 端**：PTX-EMU 端拿到 PTX 字节流，解码为 `InstrDescriptor` POD（不查表、不解释字符串）
2. **PTX-EMU → SM**：PTX-EMU 通过 `set_instr_descriptor_buf()` 把 InstrDescriptor 数组注入 SM
3. **CppTLM tick 驱动**：CppTLM 主循环每 cycle 调用 `exe_once()`
5. **SM 内部推进**：FetchUnit 从 instr buf 抓取 → DecodeUnit 分类 → IssueUnit 调度 → Execute 单元执行 → WritebackUnit 写回
6. **内存访问**：Exec 单元触发 LsuGlobal → `MemoryReqBundle` → `IMemoryPort::send_request` → CppTLM NoC
7. **异步回调**：CppTLM NoC 处理完 → `MemoryRespBundle` → LsuGlobal → s_waitcnt decrement
8. **PTX-EMU 读状态**：PTX-EMU 通过 `is_wave_stalled()` / `get_cycle_count()` / `is_finished()` 查询 SM 状态

### 15.5.3 与现状 IPtxEmuDevice 12 方法兼容性

`IPtxEmuDevice`（per `external/PTX-EMU/include/ptxemu/device_api.h`）当前 12 方法：

| # | IPtxEmuDevice 方法 | IComputeDevice 对应 |
|---|---------------------|---------------------|
| 1 | `initialize` | `initialize` |
| 2 | `shutdown` | `shutdown` |
| 3 | `exe_once` | `exe_once` |
| 4 | `sm_exe_once` | `sm_exe_once` |
| 5 | `wave_exe_once` | `wave_exe_once` |
| 6 | `attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)` | **删除**（vendor 接口已废弃）|
| 7 | `set_scoreboard(IScoreboard*)` | **删除**（vendor 接口已废弃）|
| 8 | `set_active_mask` | `set_active_mask` |
| 9 | `get_thread_state` | **`is_wave_stalled` + `get_cycle_count` 合并** |
| 10 | `set_next_pc` | **删除**（PTX-EMU 端解码后已持有 PC）|
| 11 | `get_warp_status` | `is_wave_stalled` |
| 12 | `is_finished` | `is_finished` |
| - | (无) | `reset` |
| - | (无) | `set_instr_descriptor_buf`（新增） |

**净变化**：
- 删除 3 个 vendor 接口方法（6/7/10）
- 合并 2 个状态查询为 1 个（9）
- 新增 2 个方法（reset / set_instr_descriptor_buf）
- 总数：12 → 11 方法（新增 reset 与 set_instr_descriptor_buf，删除 3 + 合并 2）

PTX-EMU 端 facade 实现需要小幅调整，但保持 12 方法大致同构（per 用户决策"完整 IPtxEmuDevice 12 方法保留"）。

---

## 15.6 12 ABI 不变量保护

### 15.6.1 严格禁止（Anti-patterns）

- ❌ **修改 23 ABI 冻结头文件**：`include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h`
- ❌ **修改 PcieEndpointIP 17 端口布局**
- ❌ **修改 PTX-EMU 子模块公共**（`external/PTX-EMU/include/ptxemu/device_api.h`），保持向后兼容
- ❌ **在 CppTLM 核心库中包含 PTX-EMU 内部头文件**（Clean Room 边界）

### 15.6.2 严格保护（Invariants）

- ✅ 23 ABI 冻结
- ✅ `PcieEndpointTLM` 4 端口布局冻结（仅可加 `[[deprecated]]`）
- ✅ `PcieEndpointIP` 17 端口布局冻结
- ✅ `PTXEMU_API_VERSION=1` 冻结
- ✅ CppTLM Clean Room 原则（不感知 PTX-EMU 内部）
- ✅ IComputeDevice 与 IPtxEmuDevice 12 方法大致同构（per 用户决策）

---

## 15.7 删除范围

### 15.7.1 文件删除清单

| # | 路径 | 类型 | 删除理由 |
|---|------|------|----------|
| 1 | `src/tlm/gpu/kernel_launch_tlm.cc` | .cc | KernelLaunchTLM 完全删除 |
| 2 | `include/tlm/gpu/kernel_launch_tlm.hh` | .hh | KernelLaunchTLM 完全删除 |
| 3 | `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `.cc | .hh+cc | CudaCoreAdapterMVP 删除（HSK-6 已删桥接）|
| 4 | `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `.cc | .hh+cc | PtxEmuSubmoduleMVP 删除 |
| 5 | `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc` | .hh+cc | PipelineTLM 删除（字符串查表已废弃）|
| 6 | `include/tlm/gpu/scoreboard_tlm.hh` + `src/tlm/gpu/scoreboard_tlm.cc` | .hh+cc | ScoreboardTLM 删除（被 HazardTracker 替代）|
| 7 | `include/tlm/gpu/tensor_core_tlm.hh` + `src/tlm/gpu/tensor_core_tlm.cc` | .hh+cc | TensorCoreTLM 删除（被 MatrixCoreUnit 替代）|
| 8 | `include/cudart/pipeline_interface.h` | vendor | HSK-4 vendor 接口删除 |
| 9 | `include/cudart/scoreboard_interface.h` | vendor | HSK-4 vendor 接口删除 |
| 10 | `include/cudart/tensor_core_interface.h` | vendor | HSK-4 vendor 接口删除 |
| 11 | `include/cudart/AGENTS.md` | doc | AGENTS 文档删除 |
| 12 | `test/test_kernel_launch_ptx_integration.cc` | test | 已测试（不存在的 KernelLaunchTLM 测试）|
| 13 | `test/test_async_completion_adapter.cc` | test | async_completion_adapter 删除（per Wave 2 deferred）|
| 14 | `test/test_pipeline_tlm.cc` | test | PipelineTLM 测试删除 |
| 15 | `test/test_scoreboard_tlm.cc` | test | ScoreboardTLM 测试删除 |
| 16 | `test/test_tensor_core_tlm.cc` | test | TensorCoreTLM 测试删除 |
| 17 | `test/test_kernel_launch_tlm.cc`（如存在）| test | KernelLaunchTLM 测试删除 |
| 18 | `src/tlm/gpu/gpu_soc_tlm.cc` | .cc | KernelLaunchTLM 引用解除 |

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
- 11 方法（per §15.5.1）

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

- 12 方法 smoke test（每个方法至少 1 assertion）
- 1 tick = 1 cycle 契约（与 Wave 2 G-D3 对齐）
- PTX-EMU facade 兼容性测试（与 HSK-8 同构）

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

**总工作量**：15-20 人天（per 用户选择"大爆炸" + 多原子 commit 拆分）

### 15.9.2 风险缓解

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 单次 PR 太大（15+ 文件改动）| 🟡 中 | 多原子 commit 拆分；每 commit 独立可编译 |
| **R2** | 23 ABI 边界误触 | 🔴 高 | 每 commit 前静态检查 ABI 头文件 `git diff` |
| **R3** | PcieEndpointIP 17 端口破坏 | 🔴 高 | 不动 PcieEndpointIP；仅改 GPU compute 侧 |
| **R4** | PTX-EMU 子模块 facade 兼容性破坏 | 🔴 高 | IComputeDevice 与 IPtxEmuDevice 12 方法同构（per 用户决策）|
| **R5** | 大爆炸回滚成本 | 🟡 中 | 多原子 commit 可逐个 revert |
| **R6** | 测试覆盖率断裂（删除 5 测试无补偿）| 🟡 中 | 12 子模块测试 + 5 集成测试覆盖 |
| **R7** | CDNA mode kHardwareCounter 复杂度 | 🟡 中 | Phase 1 仅实现 kVirtualReg；kHardwareCounter 阶段 C 启用 |
| **R8** | NV Blackwell scoreboard depth ≥ 12 模拟缺失 | 🟢 低 | 阶段 1 不模拟 17-bit 控制码（per §15.2.3 差异）|
| **R9** | ComputeCluster 上游重构缺失 | 🟢 低 | per 用户决策"后续再规划 CommandProcessor" |

---

## 15.10 Gate 验证清单（替代 ADR-SOC-15 阶段 A/B）

### 15.10.1 大爆炸 Gate

- [ ] 23 ABI 头文件 `git diff` 确认零修改
- [ ] PcieEndpointIP 17 端口布局不变
- [ ] `[pcie]/[axi]/[e2e]` 测试保持基线 100% 通过
- [ ] `[gpu]/[sm-microarch]` 新增测试 100% 通过
- [ ] `openspec validate` PASS
- [ ] Oracle 评审确认 0 P0 + ≤ 3 P1 风险
- [ ] ADR-SOC-02 Status Update 段 + ADR-SOC-16 背书（如选此方式）

### 15.10.2 替代 ADR-SOC-15 阶段 A

- [ ] 阶段 A OpenSpec change `cpptlm-dgpu-d1-cdna-isa-phase-a` 标"已由 SM 重构完成"
- [ ] 阶段 B/C/D 路线图仍有效（SM 重构完成阶段 A 抽象 + 提供 B 实施基础）

### 15.10.3 PTX-EMU 集成清理 Gate

- [ ] `include/cudart/` 目录物理删除（3 个 vendor 接口 + AGENTS.md）
- [ ] `src/tlm/gpu/{kernel_launch_tlm,pipeline_tlm,scoreboard_tlm,tensor_core_tlm,cuda_core_adapter_mvp,ptx_emu_submodule_mvp}.cc` 物理删除
- [ ] PTX-EMU 仓 submodule HEAD 不变（仅 CppTLM 端单边清理）
- [ ] HSK-9 公告不再发布（已不需要 `ICMPUTE_API_VERSION` 升级，因架构彻底变更）

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
| `cpptlm-dgpu-d1-cdna-isa-phase-a` | 📋 阶段 A 已启动（被本设计完成） |
| `cpptlm-dgpu-d1-cdna-isa-phase-b` | 📋 阶段 B 待启动（IMemoryPort 由 SM 重构承接） |
| `cpptlm-dgpu-d1-cdna-isa-phase-c` | 📋 阶段 C 待启动（CDNA-EMU 实装由 SM 重构承接） |

---

## Status Update
- **2027-02-09**: 📋 Proposed。完整 SM 微架构重构设计。12 个 ChStream 子模块 + 8 种 Bundle + IComputeDevice 11 方法 + 大爆炸重写策略。**反转 ADR-SOC-02 黑盒优先决策**，开新 ADR-SOC-16 背书（待办）。
- **前置**: 阶段 A OpenSpec change `cpptlm-dgpu-d1-cdna-isa-phase-a`（commit `e607814`）+ 4 个架构文档（commit `5a9eb4c`/`3c76398`/`4105602`/`5efbc6a`/`7708d9f`/`4549ac9`）+ ADR-SOC-15 路线图（commit `3c76398`）。
- **后续**: 用户评审 → writing-plans skill → 实施 7 原子 commit → OpenSpec archive + ADR-SOC-16 发布。