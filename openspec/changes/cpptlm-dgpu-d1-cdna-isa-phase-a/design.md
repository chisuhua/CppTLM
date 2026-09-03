## Context

**当前架构现状**（per Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP`）：

CppTLM dGPU SoC v1.0 通过 **"timing 参数注入" 模型** 与 PTX-EMU 协同仿真：
- PTX-EMU `SMContext::exe_once()` 嵌入 SM 内 timing 应用（per-thread cycle 推进）
- CppTLM 提供 3 个 vendored timing 接口实现：`IScoreboard`/`IPipelineLatencyProvider`/`ITensorCoreTiming`（vendor from `external/PTX-EMU/include/cudart/`）
- 系统级 timing 由 CppTLM 完全掌控（NoC / Cache / HBM / SoC）

**PTX 特化点（2 个根本性问题）**：

1. **字符串查表**：`PipelineTLM::get_fractional_cycles(string, pipe)`（`src/tlm/gpu/pipeline_tlm.cc:21-137`）依赖 6 类 PTX 助记符子串匹配：
   - `has(instr, "fma")` → 32 cycles
   - `has(instr, "mul")` → 16 cycles
   - `has(instr, "add")` → 4 cycles
   - `has(instr, "ld")` → 200 cycles（假内存延迟）
   - `has(instr, "st")` → 200 cycles（假内存延迟）
   - `has(instr, "bar")` → 1 cycle
   - 不识别任何非 PTX ISA（CDNA MFMA、SASS、RV-V）

2. **虚拟寄存器 hazard 模型**：`ScoreboardTLM` 基于 `(reg_id, warp_id) → busy_count` 的 `unordered_map`（CAPACITY=2048），硬编码 PTX 寄存器索引语义。CDNA 采用显式 `vmcnt`/`lgkmcnt`/`expcnt` 计数器 + `s_waitcnt` 指令，抽象层完全失配。

**架构演进路径**（per ADR-SOC-15）：4 阶段路线图合计 43-73 人天
- 阶段 A（中立化）→ 阶段 B（内存 Seam）→ 阶段 C（CDNA 引擎）→ 阶段 D（双轨校准）

本 change 实施**阶段 A**（5-8 人天）：消除 2 个 PTX 特化，建立 ISA-agnostic 抽象。

## Goals / Non-Goals

**Goals**：
- ✅ 引入 `InstrDescriptor` POD 标准化指令描述符（CppTLM 仅消费不解析文本）
- ✅ 引入 `IHazardTracker` 抽象接口（双模：`kVirtualReg` 兼容 PTX / `kHardwareCounter` 预埋 CDNA）
- ✅ 引入 `CdnaPipelineTLM`（基于 `LatencyClass` 枚举查表，保留 `PipelineTLM` 作为 PTX 兼容 shim）
- ✅ 全部现有 PTX 模式输出与改造前 **bit-identical**
- ✅ 现有 `[pcie]/[axi]/[e2e]/[wave2]` 测试保持 100% 通过
- ✅ 新增 `[cdna-phase-a]` 标签测试验证抽象正确性
- ✅ 阶段 B（`IMemoryPort`）前置就绪（`InstrDescriptor::is_memory/target_vaddr/mem_size` 字段已就位）

**Non-Goals**：
- ❌ 实施 `IMemoryPort` 异步内存 Seam（推迟至阶段 B change）
- ❌ 接入 CDNA-EMU 执行器（推迟至阶段 C change）
- ❌ 修改 PTX-EMU 端任何代码（Clean Room 边界；阶段 A 仅改造 CppTLM 端）
- ❌ 修改 23 ABI 头文件（`include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h` 冻结）
- ❌ 删除旧 `PipelineTLM`/`ScoreboardTLM`（保留作为 PTX 兼容 shim，5-8 人天范围内）

## Decisions

### D1. POD 描述符设计：保留向后兼容字段

**方案 A（采用）**：在 `InstrDescriptor` 中保留 `original_instr_str` 字段（debugging 用），主路径走 `LatencyClass` 枚举查表。

**理由**：
- 阶段 A 不可破坏 PTX 模式（`bit-identical` 是 Gate A 的硬指标）
- `original_instr_str` 用于 fallback 与未来 ABI 兼容验证
- 增加 8 字节（`const char*`）vs 完全清除，对内存布局影响可忽略

**方案 B（拒绝）**：完全清除字符串，CDNA-EMU 端不传 PTX 字符串。阶段 A 不可行。

### D2. PipelineTLM 双轨实现：保留旧 + 新增 CdnaPipelineTLM

**方案 A（采用）**：
- 旧 `PipelineTLM`（`include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc`）**保留 100% 原状**
- 新增 `CdnaPipelineTLM`（`include/tlm/gpu/cdna_pipeline_tlm.hh`）实现相同 `IPipelineLatencyProvider` 接口
- `KernelLaunchTLM::tick()` 通过 `set_pipeline(...)` 选择注入哪一个

**理由**：
- 不破坏既有 IPipelineLatencyProvider 接口契约（per HSK-8 ACCEPTED）
- PTX-EMU `SMContext` 仍可消费旧 `PipelineTLM`（仅在 PTX 模式下）
- 阶段 C 切换时，`KernelLaunchTLM` 只需换注入实例，旧路径作为 fallback 保留

**方案 B（拒绝）**：直接重写 `PipelineTLM::get_fractional_cycles` 内部实现为枚举查表。**风险**：破坏 PTX 模式 `bit-identical` 契约；增加回归风险；且需要 PTX-EMU 端同步修改 `SMContext` 的查表调用——超出阶段 A 范围。

### D3. HazardTracker 双模实现：单文件双枚举

**方案 A（采用）**：
- `IHazardTracker` 抽象接口（`include/tlm/gpu/hazard_tracker_interface.hh`）：`tick()` + `is_stalled()` + `notify_instruction_issued()` + `notify_instruction_completed()`
- `ScoreboardTLMv2`（`include/tlm/gpu/scoreboard_tlm_v2.hh`）实现 IHazardTracker，构造函数接受 `enum class Mode { kVirtualReg, kHardwareCounter }`
- `kVirtualReg` 模式内部直接复用 `ScoreboardTLM` 实现（`#include` 不复制）
- `kHardwareCounter` 模式仅占位实现（保留计数器字段，CDNA `s_waitcnt` 在阶段 C 接入）

**理由**：
- 单文件双模式降低文件数量与 review 复杂度
- kVirtualReg 路径通过 `#include` 复用旧 ScoreboardTLM，避免逻辑重复
- kHardwareCounter 占位明确标注"阶段 C 启用"，防止误用

**方案 B（拒绝）**：拆分为 `VirtualRegHazardTracker` + `HardwareCounterHazardTracker` 两个独立类。**理由**：增加文件数量（per 架构师 +3 文档 P0 反馈），且阶段 C 之前 HardwareCounter 实现不稳定。

### D4. InstrDescriptor 内存布局：POD + packed struct

**方案 A（采用）**：
```cpp
struct InstrDescriptor {
    PipeClass pipe;            // 1 byte
    LatencyClass latency_class;// 1 byte
    CtrlBits ctrl;             // 6 bytes
    uint16_t dst_regs[4];      // 8 bytes
    uint16_t src_regs[4];      // 8 bytes
    uint8_t  num_dst;          // 1 byte
    uint8_t  num_src;          // 1 byte
    bool     is_memory;        // 1 byte
    uint64_t target_vaddr;     // 8 bytes
    uint32_t mem_size;         // 4 bytes
}; // 总计 ~39 bytes（packed）
```

**理由**：
- 全 POD，`std::memcpy`/`std::hash` 友好
- 大小可放入 SBO（小对象优化）的 `std::function`
- 字段覆盖阶段 B 内存 Seam 所需的所有信息（`is_memory` + `target_vaddr` + `mem_size`）

**方案 B（拒绝）**：`std::variant<PipeClass, MemoryOp>` + 模板特化。**理由**：增加编译开销，对 PTX 阶段 A 的 bit-identical 验证增加复杂度。

### D5. 测试策略：双轨对比

**策略**：4 个新测试文件 + 1 个回归对比测试
1. `test_instruction_descriptor.cc`：POD 结构字段单元测试（序列化、对齐、`std::hash`）
2. `test_cdna_pipeline_tlm.cc`：`LatencyClass` 6 个枚举值的查表精度
3. `test_scoreboard_tlm_v2.cc`：双模式单元测试（kVirtualReg/kHardwareCounter）
4. `test_pipeline_parity.cc`：PTX 模式 bit-identical 验证（生成同一组 PTX 指令序列，断言 PipelineTLM 与 CdnaPipelineTLM 输出完全一致）

**理由**：
- `test_pipeline_parity` 是阶段 A 的核心质量门禁（per ADR-SOC-15 Gate A）
- 不依赖真实 CDNA emulator（阶段 A 实施时无 CDNA-EMU）

### D6. 向后兼容策略：3 路径切换

```cpp
// 路径 1 (现有): PTX 模式注入旧接口
kernel_launch_tlm.set_scoreboard(scoreboard_tlm);
kernel_launch_tlm.set_pipeline(pipeline_tlm);

// 路径 2 (新增): CDNA 模式注入新接口
kernel_launch_tlm.set_scoreboard_v2(scoreboard_v2_vreg);  // kVirtualReg
kernel_launch_tlm.set_pipeline(cdna_pipeline_tlm);

// 路径 3 (阶段 C): 硬件计数器模式
kernel_launch_tlm.set_scoreboard_v2(scoreboard_v2_counter);  // kHardwareCounter
```

**理由**：
- 阶段 A 实施后，PTX 模式走路径 1（不变），CDNA 模式走路径 2（占位）
- 阶段 C 切换到路径 3（真实 CDNA）
- 三路径并存保证阶段 A→B→D 期间不破坏既有测试

## Risks / Trade-offs

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | `PipelineTLM` 与 `CdnaPipelineTLM` 双轨导致 API 膨胀 | 🟡 中 | 阶段 A 末尾 review；阶段 C 启动时合并接口 |
| **R2** | `InstrDescriptor` POD 字段不够 future-proof（阶段 D 可能要扩展） | 🟢 低 | 阶段 A 末尾预留 `uint8_t reserved[8]` 字段；阶段 C 扩展不破坏 ABI |
| **R3** | `kHardwareCounter` 模式占位实现被误用 | 🟢 低 | 阶段 A 构造函数加 `[[deprecated]] kHardwareCounter` 警告（阶段 C 移除） |
| **R4** | `bit-identical` 验证失败（PTX 模式输出漂移） | 🟡 中 | `test_pipeline_parity` 强制；任何漂移立即阻断 Gate A |
| **R5** | `KernelLaunchTLM` 三路径切换逻辑复杂度 | 🟢 低 | 阶段 A 优先保证路径 1/2 切换正确；路径 3 阶段 C 实施 |
| **R6** | 文件数量增加（+6 文件 + 4 测试） | 🟢 低 | 阶段 A 范围内可接受；阶段 C 启动时审视合并 |
| **R7** | PTX-EMU 端 SMContext 仍用旧 IPipelineLatencyProvider 字符串查表 | 🟢 低 | 阶段 A 不修改 PTX-EMU；阶段 C 才同步切换 |