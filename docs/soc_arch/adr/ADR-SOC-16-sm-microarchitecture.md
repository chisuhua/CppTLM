# ADR-SOC-16: dGPU SoC v1.0 SM 微架构重构（gpgpu-sim 风格 SM 子模块化）

> **状态**: ✅ Accepted
> **日期**: 2027-02-09
> **影响**: dGPU SoC v1.0 GPU 算力侧（Compute Side）全面重构
> **类别**: SoC 架构 / GPU 微架构 / 反转 ADR-SOC-02 黑盒决策
> **实施**: OpenSpec change `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`（supersedes `cpptlm-dgpu-d1-cdna-isa-phase-a`）
> **关联**: 设计文档 [`docs/soc_arch/architecture/15-sm-microarchitecture-design.md`](../architecture/15-sm-microarchitecture-design.md)（v5.0, commit `650e9e9`）；HSK-9 公告 [`docs/soc_arch/adr/hsk9-announcement-draft.md`](./hsk9-announcement-draft.md)

---

## 1. 背景

ADR-SOC-02 (2026-06-14) 确立"CU 黑盒优先"决策：ComputeUnitTLM 简化为 `tick()` 循环，不做 5-stage pipeline、SIMD lane、寄存器堆、wavefront 调度、ISA 解析、精确的 issue/execute 周期建模。

该决策在 2026 年 Phase 7 阶段成立（当时重点在 PCIe EP + GPU 端到端集成）。但 2026-2027 年 CDNA Real ISA 集成（ADR-SOC-15，4 阶段路线图 43-73 人天）启动后，CU 黑盒不足以承载：

1. **精度对齐需求**：CDNA 校准基线（`architecture/12-cdna-calibration-baseline.md` M1-M5 microbenchmarks）需要精确的 wavefront 调度、SIMT 分歧、scoreboard waitcnt 计数；
2. **PTX-EMU 真值集成**：`IPtxEmuDevice` 12 方法 + `set_instr_descriptor_buf()` 协议要求 SM 端可读寄存器值（`get_register_value`）+ 指令完成状态（`is_instruction_completed`）；
4. **HSK-9 跨仓协调**：PTX-EMU 端 `attach_timing` 接口需废弃，改为 SM-owns-state 模式；
5. **gpgpu-sim/MGPUSim 业界范式**：完整 SM 微架构（Fetch→Decode→Issue→ScalarALU/VectorALU/MatrixCore/SIMTLane/LsuGlobal/LsuLDS/RegFileUnit/WritebackUnit/HazardTracker）是 GPU 建模的标准做法。

## 2. 决策

✅ **完整 SM 微架构重构**：删除 ADR-SOC-02 黑盒 CU，改为 gpgpu-sim 风格 12 子模块 + 8 Bundle + `IComputeDevice` **15 方法**接口。

### 2.1 SM 拓扑（per architecture/15 §15.2）

```
StreamingMultiprocessorTLM (顶层 ChStreamModuleBase + IComputeDevice)
├── FetchUnitTLM        # 指令抓取（per-warp 调度）
├── DecodeUnitTLM       # 指令解码 → InstrDescriptor (isa_type, instr_id, PipeClass, LatencyClass)
├── IssueUnitTLM        # 指令发射（round-robin + CGGTY 阈值）
├── ScalarALU           # 标量 ALU（INT/FP32/FP64/SFU/Branch 子管道）
├── VectorALU           # SIMD 向量 ALU（V-pipe）
├── MatrixCore          # CDNA MFMA 子集（20 条指令，per architecture/13）
├── SIMTLane           # 64-bit EXEC mask + 派态分歧检测
├── LsuGlobal          # 全局内存（接 IMemoryPort 异步）
├── LsuLDS             # 共享内存（intra-SM）
├── RegFileUnit        # 寄存器堆（SM 端唯一真值源）
├── WritebackUnit      # 写回（释放 HazardTracker）
└── HazardTracker      # RAW hazard + vmcnt/lgkmcnt 计数
```

### 2.2 8 种 Bundle（per architecture/15 §15.4）

| Bundle | 方向 | 关键字段 |
|--------|------|----------|
| FetchToIssueBundle | Fetch→Issue | instr_desc, warp_id, pc |
| DecodeToIssueBundle | Decode→Issue | + PipeClass, LatencyClass |
| IssueToExecBundle | Issue→Exec | + src_values[4], src_valid[4]（PTX-EMU 上行同步） |
| ExecToWritebackBundle | Exec→WB | + result_value[4], memory_data, exec_cycles |
| WritebackToRegFileBundle | WB→RegFile | dst_regs[4], values[4], is_accvgpr |
| MemoryReqBundle | Lsu→NoC | vaddr, size, lane_mask |
| MemoryRespBundle | NoC→Lsu | data, cycles |
| ScoreboardQueryBundle | 任意→Hazard | QueryType, ctrl |

### 2.3 IComputeDevice 15 方法（per architecture/15 §15.5 + HSK-9）

11 preserved from `IPtxEmuDevice` + 1 new (`set_instr_descriptor_buf`) + 2 new (Round 4 user decisions) + 1 (`reset`):
- `initialize` / `shutdown` / `exe_once` / `sm_exe_once` / `warp_exe_once`
- `set_scoreboard` / `get_thread_state` / `set_active_mask` / `set_next_pc`
- `get_warp_status` / `is_finished`
- `set_instr_descriptor_buf`（**核心**：PTX-EMU 上行同步通道）
- `get_register_value` / `is_instruction_completed`（**Round 4 新增**）
- `reset`

`attach_timing` 不在此接口（per HSK-9 F3.1，保留为 `IPtxEmuDevice` deprecated stub）。

### 2.4 SM-owns-state 模式（per architecture/15 §15.5.6 + Oracle Round 2 决策）

- SM 持寄存器唯一真值源（`RegFileUnit`）
- PTX-EMU 端通过 `set_instr_descriptor_buf()` 注入已解码 InstrDescriptor + 通过 `get_register_value()`/`is_instruction_completed()` 读路径/就绪协议同步
- **双计算 + Gate bit-exact**（per Oracle Round 4 F1.4）：PTX-EMU functional 用于控制流 + SM Exec 算 timing 真值 + Gate 验证两边 ALU bit-exact

### 2.5 删除范围（per architecture/15 §15.7）

| 类别 | 数量 | 文件 |
|------|------|------|
| 实现删除 | 6 | KernelLaunchTLM + CudaCoreAdapterMVP + PtxEmuSubmoduleMVP + PipelineTLM + ScoreboardTLM + TensorCoreTLM |
| Vendor 接口 | 3 | include/cudart/{pipeline,scoreboard,tensor_core}_interface.h |
| 测试删除 | 15 | test/test_{kernel_launch_tlm,kernel_launch_ptx_integration,async_completion_adapter,cuda_core_adapter_timing,latency_tlm_perf,pipeline_tlm,scoreboard_tlm,tensor_core_tlm,gpu_soc_tlm,gpu_compute_unit_tlm,gpu_compute_unit_integration,gpu_soc_phase8a,wavefront_tlm,vector_regfile_tlm,minimal_warp_scheduler_tlm}.cc |
| JSON config | 4 | vector_add_n1024 + compute_unit_v1 + gpu_soc_gb203_v1 + dgpu_soc_with_pcie_ip |
| 旁路修复 | 4 | gpu_soc_tlm.{h,cc} + async_completion_adapter + main.cpp + chstream_register |
| DOC HYGIENE | 9 项 | AGENTS.md + ONBOARDING + 7 modules docs + 3 main specs + VIRTUAL_PATHS + test scripts |

## 3. 反转的既有 ADR

| ADR | 原决策 | 反转内容 | 替代 |
|-----|--------|----------|------|
| **ADR-SOC-02** | CU 黑盒优先（Phase 7.B） | 改为完整 SM 微架构（12 子模块） | 本 ADR |
| **ADR-SOC-15 §3 D2** | PipelineTLM 双轨实现（阶段 A） | 直接构建完整 SM，无双轨并存 | OpenSpec change `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` |

## 4. 后果

### 4.1 正面

- **精度对齐**：SM 微架构与 AMD CDNA2/CDNA3 ISA Reference、NV Blackwell SM_120 微架构对齐，可支持 MGPUSim/Mi300X 校准基线（per `architecture/12`）；
- **PTX-EMU 集成清理**：HSK-9 公告（修订后 ~210 行）冻结 `IPtxEmuDevice` 12 方法 + 标记 `attach_timing` deprecated + 新增 `IComputeDevice` **15 方法**，跨仓协调清晰；
- **gpgpu-sim 范式**：业界标准 SM 微架构（Fetch→Decode→Issue→Exec→Writeback 5-stage pipeline），可参考 MGPUSim/Gem5 等成熟仿真框架；
- **可扩展性**：12 子模块独立可测 + 8 Bundle 灵活替换 + SM 顶层 15 方法标准化；
- **Gate bit-exact 验证**：双计算 + Gate 比对确保 PTX-EMU functional 与 SM timing 一致。

### 4.2 负面

- **实施工作量大**：25-30 工作日（CppTLM 侧）+ 5-10 工作日（PTX-EMU 侧），共 30-40 工作日；
- **测试迁移成本**：15 旧测试删除 + 146+ assertions 新测试，工作量 6-8 工作日；
- **DOC HYGIENE 工作量**：9 项（AGENTS.md + ONBOARDING + 7 modules docs + 3 main specs + VIRTUAL_PATHS + test scripts）；
- **风险**：SM 微架构 + 双计算 Gate 在第一版可能存在边界 bug，需多轮 Oracle 评审 + 校准验证。

## 5. 实施

20 个原子 commit（per architecture/15 §15.9.1 + `docs/superpowers/plans/2027-02-09-sm-microarchitecture-rewrite.md`），每 commit 可独立编译：

| 阶段 | Tasks | 内容 |
|------|-------|------|
| 文档 + OpenSpec | 1-3 | ADR 背书 + 设计终稿 + OpenSpec change 启动 |
| 接口 + 12 子模块 stub | 4-5 | IComputeDevice + SM 顶层 + 12 子模块 stub |
| Bundle + 子模块 full | 7-8 | 8 Bundle + 12 子模块完整实现 + chstream_register 注册 |
| 旧模块重构 | 9-11 | GpuComputeUnitTLM + 3 旧模块内化 + 旁路修复 |
| 删除 | 12-13 | 6 实现 + 3 vendor + cudart 目录 |
| JSON + 删除测试 + DOC | 14-16 | 4 JSON + DOC HYGIENE + 15 测试删除 |
| 新测试 + 完整实现 | 17-18 | 146+ assertions + IComputeDevice 完整 + bit-exact Gate |
| Archive + 跨仓协调 | 19-20 | OpenSpec archive + HSK-9 公告发布 + PTX-EMU 端跟踪 |

## 7. Gate 14 项（per architecture/15 §15.10）

**状态说明**：以下 Gate 14 项是实施期间必须验证的目标，**当前状态为待验证（pending）**——实施未启动前任何 [x] 都是过度承诺。本节列出 Gate 项 + 验证手段 + 责任人 + 时间窗口；每项在对应 Task 实施 + Oracle 复评通过后才置 [x]。

- [ ] **G1**: SM 拓扑：12 子模块 + ChStreamModuleBase + Bundle 连接 — 验证手段: `nm build/bin/cpptlm_tests | grep -E "FetchUnitTLM|DecodeUnitTLM|..."`（Task 8 完成）; 责任人: Task 8 实施者; 目标日期: Task 8 完成时
- [ ] **G2**: IComputeDevice **15 方法**签名冻结 + 命名空间 cpptlm::gpu（含 11 IPtxEmuDevice 同构保留 + 1 HSK-9 同步通道 + 2 Round 4 读路径 + 1 reset；`get_thread_state` 返回 `ThreadState` per `device_api.h:104`）— 验证手段: `static_assert` in test_i_compute_device_interface.cc（Task 4 完成）; 责任人: Task 4 实施者
- [ ] **G3**: SM-owns-state：RegFileUnit 唯一真值源 + PTX-EMU 通过 set_instr_descriptor_buf 同步 — 验证手段: Task 18 L3 集成测试 30+ assertions
- [ ] **G4**: 8 Bundle POD 字段完整 + 流向正确（Fetch→Decode→Issue→Exec→WB→RegFile）— 验证手段: Task 17 L2 Bundle 接线测试 20+ assertions
- [ ] **G5**: Gate bit-exact：PTX-EMU functional 与 SM Exec ALU 实现 bit-exact — 验证手段: Task 18 Gate bit-exact 测试 (test_bit_exact_gate.cc)
- [ ] **G6**: HSK-9 协议：attach_timing 保留为 IPtxEmuDevice deprecated stub（device_api.h 不动）— 验证手段: `git diff device_api.h` 应为空（除已 [[deprecated]] 标记）; 责任人: 每 Task commit 前静态检查
- [ ] **G7**: SFU 子管道在 ScalarALU 内（INT/FP32/FP64/SFU/Branch 5 子管道）— 验证手段: Task 17 test_sm_scalar_alu_tlm.cc
- [ ] **G8**: 23 ABI 冻结：`include/abi/cpptlm_emulator.h` 零修改 + `pcie_endpoint_tlm.h` 仅可加 [[deprecated]] — 验证手段: `git diff include/abi/cpptlm_emulator.h include/tlm/gpu/pcie_endpoint_tlm.h` 应为空（除 [[deprecated]]）
- [ ] **G9**: 删除范围：**15** 旧测试文件（per architecture/15 §15.7.1.B）+ 6 实现 + 3 vendor + 4 JSON + 旁路修复 + DOC HYGIENE — 验证手段: Task 16 完成 + Task 11 脚本测试 PASS
- [ ] **G10**: 20 原子 commit：每 commit 可独立编译 — 验证手段: `cmake --build build` 在每个 commit 上 PASS
- [ ] **G11**: OpenSpec change sm-rewrite (supersedes phase-a) — 验证手段: `openspec validate cpptlm-dgpu-d1-cdna-isa-sm-rewrite --strict` PASS（已完成）
- [ ] **G12**: HSK-9 公告草稿发布到 PTX-EMU 仓 `docs/superpowers/specs/` — 验证手段: Task 20 完成
- [ ] **G13**: 测试覆盖：12 子模块单测 + L2-L6 集成测试 + L7 JSON reload (146+ assertions) — 验证手段: `cpptlm_tests "[sm-unit][sm-bundle][sm-top][sm-icompute][sm-cdna][sm-e2e][sm-json][sm-gate]"` PASS
- [ ] **G14**: Oracle 复评通过（实施期间每 Task 完成后 + 最终 Round 6 0 P0, 0 P1）— 验证手段: Oracle session PASS

**整体 Gate 通过条件**：G1-G14 全部 [x] + `cpptlm_tests "[pcie][axi][e2e][wave2][gpu][sm-microarch]"` 全绿 + Oracle 最终复评 PASS。

## 8. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| PTX-EMU 端改造延期 | 中 | 中 | Task 20 跟踪 14 天反馈窗口；CppTLM 侧先独立 archive |
| Gate bit-exact 边界 bug | 中 | 高 | 多轮 Oracle 评审 + CDNA 校准基线对比 |
| 删除范围广（15 测试） | 低 | 中 | 旧测试先用 `[[deprecated]]` 标记（Task 10）再删除（Task 16） |
| 旁路修复不完整 | 低 | 中 | Task 11 写失败测试验证 KernelLaunchTLM 符号无残留 |

## 9. 评审汇总（per Oracle 5 轮）

- Round 1（v1.0 → v2.0）：4 P0 + 9 P1 + 5 P2
- Round 2（v2.0 → v3.0）：2 N-P0 + 9 P1（用户决策 P0-1+P0-2 + 9 P1 修复）
- Round 3（v3.0 → v4.0）：1 R3-P0-1 + 4 P1（用户决策 P0-3 + 4 P1 修复）
- Round 4（v4.0 → v5.0）：1 R4-P0-1 + 4 P1（用户决策 F1.4 + F3.1 + 4 P1 修复）
- Round 5（v5.0）：0 P0, 0 P1 → ACCEPTED

## 10. 参考

- [`architecture/15-sm-microarchitecture-design.md`](../architecture/15-sm-microarchitecture-design.md)（v5.0, commit `650e9e9`, 971 行）
- [`hsk9-announcement-draft.md`](./hsk9-announcement-draft.md)（179 行, supersede commit `4105602`）
- [`architecture/12-cdna-calibration-baseline.md`](../architecture/12-cdna-calibration-baseline.md)
- [`architecture/13-cdna-emu-selector.md`](../architecture/13-cdna-emu-selector.md)
- [`ADR-SOC-15-cdna-real-isa-roadmap.md`](./ADR-SOC-15-cdna-real-isa-roadmap.md)
- [`ADR-SOC-02-cu-granularity.md`](./ADR-SOC-02-cu-granularity.md)（被本 ADR 反转）
- [`external/PTX-EMU/include/ptxemu/device_api.h`](../../../external/PTX-EMU/include/ptxemu/device_api.h)（`IPtxEmuDevice` 12/12 frozen per HSK-8）
- NV Blackwell SM_120 paper: <https://zartbot.github.io/micro_arch/nvidia/sm_120/paper.html#sec5>
- AMD MI300X CDNA3 ISA Reference

---

## Status Update

（暂无更新）