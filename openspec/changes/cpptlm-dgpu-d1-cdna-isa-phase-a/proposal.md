## Why

CppTLM 当前 dGPU SoC v1.0 周期精确仿真框架在 GPGPU 端通过 **"timing 参数注入" 模型** 与 PTX-EMU 协同：PTX-EMU `SMContext::exe_once()` 内嵌 SM 内 timing 应用，CppTLM 提供 timing 参数与系统级 timing（per Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP`）。该架构存在 **2 个根本性 PTX 特化**：

1. **`PipelineTLM::get_fractional_cycles(string, pipe)`** 字符串查表（`pipeline_tlm.cc:21-137` 共 6 个查表函数）依赖 PTX 助记符（"fma"/"mul"/"add"/"ld"/"st"/"bar"），无法支持 CDNA MFMA、SASS 等其他 ISA 助记符。
2. **`ScoreboardTLM` 基于虚拟寄存器 (reg_id, warp_id) 哈希表**（CAPACITY=2048），硬编码 PTX 寄存器索引语义。CDNA 采用 **显式硬件计数器** (`vmcnt`/`lgkmcnt`/`expcnt` + `s_waitcnt` 指令)，抽象层完全失配。

**真实工作负载需求**：仅 PTX 不足以验证 MI300X 等 CDNA GPU 上的真实软件栈（ROCm / KFD / AQL）。**精度验证基线断裂**：PTX 模式的"正确性"自洽但缺乏与真实硬件或 MGPUSim/gem5 的可重复对拍基准。

**ADR-SOC-15**（dGPU SoC v1.0 CDNA 真实物理 ISA 演进路线图）已发布。本 change 实施其中**阶段 A（5-8 人天）**：**契约中立化与指令描述符重构**——在 PTX 模式下消除上述 2 个 PTX 特化，建立 ISA-agnostic 的时序宿主核心抽象，为阶段 B（内存 Seam）+ 阶段 C（CDNA 引擎接入）铺路。

**前置**：
- ✅ ADR-SOC-15 已发布（`docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`）
- ✅ 方案设计文档已发布（`docs/soc_arch/architecture/11-cdna-real-isa-integration.md`）
- ✅ PTX-EMU `IPtxEmuDevice` 12/12 wired（HSK-8 ACCEPTED）
- ✅ 全部现有 Catch2 测试 [pcie]/[axi]/[e2e]/[wave2] 全绿

**禁止（Anti-patterns）**：
- ❌ 修改 `include/abi/cpptlm_emulator.h` 或 `include/tlm/gpu/pcie_endpoint_tlm.h`（23 ABI 冻结）
- ❌ 引入 PTX-EMU 内部头文件到 CppTLM（Clean Room 原则）
- ❌ 阶段 A 引入 CDNA-EMU 或 IMemoryPort 实体实现（推迟至阶段 B/C）

## What Changes

### 新增产物

- **新增** `include/tlm/gpu/instruction_descriptor.hh`（POD 头文件）：定义 `PipeClass`/`LatencyClass`/`CtrlBits`/`InstrDescriptor` 4 个核心类型（CppTLM 仅消费此结构）
- **新增** `include/tlm/gpu/cdna_pipeline_tlm.hh` + `src/tlm/gpu/cdna_pipeline_tlm.cc`（CDNA 微架构适配的 PipelineTLM）：从 `IPipelineLatencyProvider` 接口的 `get_fractional_cycles(string, pipe)` 改为 `get_latency(LatencyClass)` 枚举查表
- **新增** `include/tlm/gpu/hazard_tracker_interface.hh`：定义 `IHazardTracker` 抽象接口（双模适配器基类）
- **新增** `include/tlm/gpu/scoreboard_tlm_v2.hh` + `src/tlm/gpu/scoreboard_tlm_v2.cc`：`ScoreboardTLMv2` 实现 `IHazardTracker`，支持 `kVirtualReg`（PTX 阶段 A 兼容）+ `kHardwareCounter`（CDNA 阶段 C 预埋）两种模式
- **新增** `test/test_instruction_descriptor.cc`：单元测试（POD 结构序列化 + 跨 ISA 映射）
- **新增** `test/test_cdna_pipeline_tlm.cc`：单元测试（6 个 LatencyClass 查表 + PipelineTLM 精度对比）
- **新增** `test/test_scoreboard_tlm_v2.cc`：单元测试（kVirtualReg 与 kHardwareCounter 双模式）
- **新增** `test/test_pipeline_parity.cc`：位精确对比测试（改造前后 PTX 模式输出 bit-identical）

### 修改产物

- **修改** `src/tlm/gpu/pipeline_tlm.cc:21-137`（6 个查表函数）：保留 `PipelineTLM` 原类作为 PTX 兼容 shim；新增 `CdnaPipelineTLM` 抽象版（不破坏现有 IPipelineLatencyProvider 接口）
- **修改** `include/tlm/gpu/kernel_launch_tlm.hh`：**新增** `set_scoreboard_v2(IHazardTracker*)` setter（保留旧 `set_scoreboard(IScoreboard*)` 兼容接口）
- **修改** `src/tlm/gpu/kernel_launch_tlm.cc:18-25`（`tick()` 函数）：当 set_scoreboard_v2 已注入时，调用 `IHazardTracker::tick()` 否则继续走旧 IScoreboard 路径
- **修改** `openspec/specs/pipeline-latency/spec.md`（如不存在则新增）：新增 ADDED Requirement `cpptlm-cdna-pipeline`（LatencyClass 查表）
- **修改** `openspec/specs/hazard-tracker/spec.md`（如不存在则新增）：新增 ADDED Requirement `cpptlm-hazard-tracker-v2`（双模适配器）

### 删除产物

- **无**（不删除旧 PipelineTLM/ScoreboardTLM，保留作为 PTX 阶段 A 的兼容 shim）

## Capabilities

### New Capabilities
- `cdna-isa-abstraction`: CppTLM 端 ISA-agnostic 时序宿主核心抽象（InstrDescriptor + LatencyClass + IHazardTracker 双模）

### Modified Capabilities
- 无（现有 spec 行为不变，PTX 模式输出 bit-identical）

## Impact

**影响代码**：
- `include/tlm/gpu/instruction_descriptor.hh`（新增）
- `include/tlm/gpu/hazard_tracker_interface.hh`（新增）
- `include/tlm/gpu/cdna_pipeline_tlm.{hh,cc}`（新增）
- `include/tlm/gpu/scoreboard_tlm_v2.{hh,cc}`（新增）
- `src/tlm/gpu/kernel_launch_tlm.{hh,cc}`（修改）
- `test/test_instruction_descriptor.cc`（新增）
- `test/test_cdna_pipeline_tlm.cc`（新增）
- `test/test_scoreboard_tlm_v2.cc`（新增）
- `test/test_pipeline_parity.cc`（新增）

**影响测试**：
- 现有 `[pcie]/[axi]/[e2e]/[wave2]` 必须保持 100% 通过
- 新增 `[cdna-phase-a]` 标签测试

**影响依赖**：
- 0 新增外部依赖
- 不修改 23 ABI 头文件
- 不引入 PTX-EMU 内部头文件（保留 Clean Room 边界）

**影响文档**：
- `docs/soc_arch/architecture/11-cdna-real-isa-integration.md`（已发布）
- `docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`（已发布）

**HSK 协议**：
- 本 change **不触发 HSK-N**（`PTXEMU_API_VERSION=1` 未变）
- 阶段 B（IMemoryPort 引入）将触发 HSK-9