## Why

CppTLM 当前 dGPU SoC v1.0 周期精确仿真框架在 GPGPU 端通过 **"timing 参数注入" 模型** 与 PTX-EMU 协同：PTX-EMU `SMContext::exe_once()` 内嵌 SM 内 timing 应用，CppTLM 提供 timing 参数与系统级 timing（per Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP`）。该架构存在 **2 个根本性 PTX 特化**（per Oracle P0-A1 验证基于 `src/tlm/gpu/pipeline_tlm.cc:21-137` 实际值）：

1. **`PipelineTLM::get_fractional_cycles(string, pipe_id)`** 字符串查表依赖 PTX 助记符（`has()` 大小写不敏感子串匹配），返回连续值 4.22 / 2.0 / 8.0 / 20.0 / 200.0 等，**无法用 6 入口枚举表 {1, 4, 8, 16, 32} 简化表达**。CDNA MFMA、SASS 等其他 ISA 助记符也无法支持。
2. **`ScoreboardTLM`** 基于虚拟寄存器 (reg_id, warp_id) 哈希表（CAPACITY=2048），`allocate(reg, warp)→bool` duplicate-reject 语义（per `scoreboard_tlm.hh:35`）是 PTX-EMU `sm_context.cpp:37-43` rollback 依赖。CDNA 采用 **显式硬件计数器** (`vmcnt`/`lgkmcnt`/`expcnt` + `s_waitcnt` 指令)，抽象层完全失配。

**真实工作负载需求**：仅 PTX 不足以验证 MI300X 等 CDNA GPU 上的真实软件栈（ROCm / KFD / AQL）。**精度验证基线断裂**：PTX 模式的"正确性"自洽但缺乏与真实硬件或 MGPUSim/gem5 的可重复对拍基准。

**ADR-SOC-15**（dGPU SoC v1.0 CDNA 真实物理 ISA 演进路线图）已发布。本 change 实施其中**阶段 A（5-8 人天）**：**契约中立化与指令描述符重构**——保留 PipelineTLM / ScoreboardTLM 源文件 100% 原状，新增强中立化抽象层（`InstrDescriptor` + `CdnaPipelineTLM` + `IHazardTracker` + `ScoreboardTLMv2`），在 PTX 模式下 byte-equal 不变，CDNA 模式接口已就位待阶段 C 接入。**注入点改到 `CudaCoreAdapterMVP`**（per Oracle P0-A3：KernelLaunchTLM 当前无 setter 且 tick() 不消费 scoreboard/pipeline）。**新增 `PtxStringToDescriptor` 纯函数**闭环验证链（per Oracle P1-B7）。

**前置**：
- ✅ ADR-SOC-15 已发布（`docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`）
- ✅ 方案设计文档已发布（`docs/soc_arch/architecture/11-cdna-real-isa-integration.md`）
- ✅ PTX-EMU `IPtxEmuDevice` 12/12 wired（HSK-8 ACCEPTED）
- ✅ 全部现有 Catch2 测试 [pcie]/[axi]/[e2e]/[wave2] 全绿
- ✅ Oracle 预审通过（5 P0 + 5 P1 + 3 P2 全部 closed, session `ses_f962d8ef5ffe2tuGZUCI4gfY0T`）

**ADR 一致性显式化**（per Oracle P0-A1）：
本 change **显式偏离** ADR-SOC-15 §4.1 A.3 字面计划"PipelineTLM::get_fractional_cycles 重写为 get_latency(LatencyClass) 枚举查表"。偏离理由：PipelineTLM 实际连续值（4.22/2.0/20.0/200.0）无法用 6 入口枚举表表达，按 ADR 字面计划会破坏 PTX 模式 byte-equal 契约。实际方案是**新增 CdnaPipelineTLM + PipelineTLM 双轨**（PipelineTLM 100% 源文件不变），阶段 C 启动时同步修订 ADR §4.1 表 A.3 描述（tasks.md 7.5 显式追踪）。

**禁止（Anti-patterns）**：
- ❌ 修改 `include/abi/cpptlm_emulator.h` 或 `include/tlm/gpu/pcie_endpoint_tlm.h`（23 ABI 冻结）
- ❌ 引入 PTX-EMU 内部头文件到 CppTLM（Clean Room 原则）
- ❌ 阶段 A 引入 CDNA-EMU 或 IMemoryPort 实体实现（推迟至阶段 B/C）
- ❌ 修改 `src/tlm/gpu/pipeline_tlm.cc` 或 `scoreboard_tlm.cc` 源文件（保留 100% 原状）
- ❌ 修改 `include/tlm/gpu/kernel_launch_tlm.hh` 注入 v2（per Oracle P0-A3：注入点改到 `CudaCoreAdapterMVP`）

## What Changes

### 新增产物

- **新增** `include/tlm/gpu/instruction_descriptor.hh`（POD 头文件）：定义 `PipeClass`(7) / `LatencyClass`(6) / `CtrlBits`(6 字段×1 字节=6 bytes, per arch 11 §11.2.1) / `InstrDescriptor` POD (含 `transaction_id:uint64_t=0` + `reserved[8]:uint8_t` 阶段 B/C/D 预留, `static_assert(sizeof == 48 || == 56)`)
- **新增** `include/tlm/gpu/cdna_pipeline_tlm.hh` + `src/tlm/gpu/cdna_pipeline_tlm.cc`：`CdnaPipelineTLM` override vendored `IPipelineLatencyProvider` 全部**两个**纯虚方法（`get_fractional_cycles` + `get_fractional_cycles_by_type`）；PTX 兼容 shim 逐字复制 `pipeline_tlm.cc:21-94` 函数
- **新增** `include/tlm/gpu/hazard_tracker_interface.hh`：`IHazardTracker` 抽象接口（与 vendored `IScoreboard` 语义对齐：`has_free_entry/try_acquire(instr,sm,wave)→bool/release(instr,sm,wave)/tick` + `mark_waiting()` 阶段 C 扩展点 default no-op）
- **新增** `include/tlm/gpu/scoreboard_tlm_v2.hh` + `src/tlm/gpu/scoreboard_tlm_v2.cc`：`ScoreboardTLMv2` 实现 `IHazardTracker`，支持 `kVirtualReg`（PTX 阶段 A 兼容）+ `kHardwareCounter`（CDNA 阶段 C 预埋，**枚举值**级别 `[[deprecated("stage C only")]]`）
- **新增** `include/tlm/gpu/ptx_string_to_descriptor.hh` + `src/tlm/gpu/ptx_string_to_descriptor.cc`（per Oracle P1-B7）：纯函数 `ptx_string_to_descriptor(string, sm, wave)→InstrDescriptor`，闭环 v2 链路无生产者问题
- **新增** `include/tlm/gpu/AGENTS.md`（per Oracle P2-3）：5 个新头文件索引
- **新增** `test/test_instruction_descriptor.cc`：POD 序列化 + 10k random hash collision 验证
- **新增** `test/test_cdna_pipeline_tlm.cc`：6 LatencyClass 查表 + 6 PipelineId × 全模式矩阵 + by_type 全矩阵
- **新增** `test/test_scoreboard_tlm_v2.cc`：双模式单元 + 越界抛 `std::out_of_range`
- **新增** `test/test_pipeline_parity.cc`：枚举式分支覆盖 + 1000 条随机（固定 seed）双层
- **新增** `test/test_cuda_core_adapter_v2_paths.cc`：默认 / v2 / v2 nullptr 回退 / 混合注入
- **新增** `test/test_ptx_string_to_descriptor.cc`：6 PTX pattern × 2 method

### 修改产物

- **修改** `include/tlm/gpu/cuda_core_adapter_mvp.hh`：**新增** `set_scoreboard_v2(IHazardTracker*)` + `set_pipeline_v2(CdnaPipelineTLM*)` setter（per Oracle P0-A3 注入点从 KernelLaunchTLM 改到此）
- **修改** `src/tlm/gpu/cuda_core_adapter_mvp.cc::inject_timing_modules()`：3 路径调度（默认 / v2 注入 / v2 nullptr 回退）；**不修改**原 `make_unique<ScoreboardTLM/PipelineTLM>` 路径主体
- **不修改** `src/tlm/gpu/pipeline_tlm.cc`（per Oracle P0-A1：保留 100% 原状）
- **不修改** `include/tlm/gpu/kernel_launch_tlm.hh`（per Oracle P0-A3：注入点不在此处）

### 删除产物

- **无**（不删除旧 `PipelineTLM` / `ScoreboardTLM`，保留作为 PTX 阶段 A 的兼容 shim）

## Capabilities

### New Capabilities
- `cdna-isa-abstraction`: CppTLM 端 ISA-agnostic 时序宿主核心抽象（`InstrDescriptor` + `LatencyClass` + `IHazardTracker` 双模 + `CdnaPipelineTLM` + `ScoreboardTLMv2`）

### Modified Capabilities
- `cpptlm-d1-p1-pipeline-scoreboard`（MODIFIED `cpptlm-pipeline` / `cpptlm-scoreboard`）：明确"阶段 A 不修改 PipelineTLM/ScoreboardTLM 源文件"（per Oracle P0-A4 修订主 spec capability 名）

## Impact

**影响代码**：
- `include/tlm/gpu/instruction_descriptor.hh`（新增）
- `include/tlm/gpu/hazard_tracker_interface.hh`（新增）
- `include/tlm/gpu/cdna_pipeline_tlm.{hh,cc}`（新增）
- `include/tlm/gpu/scoreboard_tlm_v2.{hh,cc}`（新增）
- `include/tlm/gpu/ptx_string_to_descriptor.{hh,cc}`（新增）
- `include/tlm/gpu/AGENTS.md`（新增）
- `include/tlm/gpu/cuda_core_adapter_mvp.{hh,cc}`（修改：v2 setter + 3 路径调度）
- `test/test_instruction_descriptor.cc`（新增）
- `test/test_cdna_pipeline_tlm.cc`（新增）
- `test/test_scoreboard_tlm_v2.cc`（新增）
- `test/test_pipeline_parity.cc`（新增）
- `test/test_cuda_core_adapter_v2_paths.cc`（新增）
- `test/test_ptx_string_to_descriptor.cc`（新增）

**影响测试**：
- 现有 `[pcie]/[axi]/[e2e]/[wave2]` 必须保持 100% 通过
- 新增 `[cdna-phase-a]` 标签测试（6 个新文件）

**影响依赖**：
- 0 新增外部依赖
- 不修改 23 ABI 头文件
- 不引入 PTX-EMU 内部头文件（保留 Clean Room 边界）
- 不修改 `pipeline_tlm.cc` / `scoreboard_tlm.cc` / `kernel_launch_tlm.hh`

**影响文档**：
- `docs/soc_arch/architecture/11-cdna-real-isa-integration.md`（已发布）
- `docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`（**阶段 C 启动时需同步修订 §4.1 表 A.3**，per design.md "ADR 一致性显式化" 段）
- `openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md`（MODIFIED `cpptlm-pipeline` / `cpptlm-scoreboard` Requirement 标注"阶段 A 不修改源文件"，archive 时合并）
