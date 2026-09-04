## Context

**当前架构现状**（per Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP`）：

CppTLM dGPU SoC v1.0 通过 **"timing 参数注入" 模型** 与 PTX-EMU 协同仿真：
- PTX-EMU `SMContext::exe_once()` 嵌入 SM 内 timing 应用（per-thread cycle 推进）
- CppTLM 提供 3 个 vendored timing 接口实现：`IScoreboard`/`IPipelineLatencyProvider`/`ITensorCoreTiming`（vendor from `external/PTX-EMU/include/cudart/`, commits `8acfd2d1`/`9e7361b9`/`463038e0`，**ABI 冻结**）
- 系统级 timing 由 CppTLM 完全掌控（NoC / Cache / HBM / SoC）
- 真正的 scoreboard_/pipeline_ 持有者是 **`CudaCoreAdapterMVP`**（per `src/tlm/gpu/cuda_core_adapter_mvp.cc:60-61`，以 `std::make_unique<ScoreboardTLM/PipelineTLM>` 自建 + `facade->attach_timing(sb, pl, tc)` 3→1 聚合）
- **`KernelLaunchTLM` 不持有 scoreboard/pipeline 指针**（per `kernel_launch_tlm.hh:59-113` 验证）——P0/P1 路径的 `setMemoryBridge/set_ptx_emu_driver` 已在 HSK-6 物理删除（commit `369cf71`）

**PTX 特化点（2 个根本性问题，per Oracle P0-A1 验证）**：

1. **字符串查表**：`PipelineTLM::get_fractional_cycles(string, pipe_id)` 依赖 `has()` 大小写不敏感子串匹配（`pipeline_tlm.cc:21-94` 实际值）：

   **P0_INT_FP32**:
   - `fma` / `mul.f32` / `add.f32` / `sub.f32` / `div` / `min.f32` / `max.f32` / `abs.f32` / `neg.f32` → **4.22** (FP32 arithmetic)
   - `mad` / `mul.lo` / `mul.hi` / `sad` / `mul.wide` → **2.0** (wide int multiply)
   - 其他（含空串、`bar`、`add` 不带 `.f32`）→ **1.0** (default)

   **V_SIMD**: 1.0（无字符串依赖）

   **P1_FP64**: 8.0（无字符串依赖）

   **P2_SFU**:
   - `sin` / `cos` / `tan` → 16.0
   - `rcp` / `rsqrt` → 4.0
   - `sqrt` / `lg2` / `ex2` → 8.0
   - 其他 → 8.0

   **P3_LSU**:
   - `ld.global` / `ld.volatile` / `atom` / `red.` → 200.0
   - `st.global` → **20.0** (NOT 200; write buffer)
   - `ld.shared` / `st.shared` → 1.0
   - `ld.local` / `st.local` → 5.0
   - `ld.`（其他）→ 200.0
   - `st.`（其他）→ 20.0
   - 其他（含 `bar`、空串）→ 200.0

   **P4_TC**: 0.0

   `get_fractional_cycles_by_type(int, pipe_id)`（per `pipeline_tlm.cc:120-137` switch 真值表）：P0=2, V=1, P1=8, P2=8, P3=200, P4=0
   不识别任何非 PTX ISA（CDNA MFMA、SASS、RV-V）

2. **虚拟寄存器 hazard 模型**：`ScoreboardTLM` 基于 `(reg_id, warp_id) → busy` 的 `unordered_map`（CAPACITY=2048），硬编码 PTX 寄存器索引语义。`allocate(reg, warp)→bool` duplicate-reject 语义（per `scoreboard_tlm.hh:35`）是 PTX-EMU `sm_context.cpp:37-43` rollback 依赖。CDNA 采用显式 `vmcnt`/`lgkmcnt`/`expcnt` 计数器 + `s_waitcnt` 指令，抽象层完全失配。

**架构演进路径**（per ADR-SOC-15）：4 阶段路线图合计 43-73 人天
- 阶段 A（中立化）→ 阶段 B（内存 Seam）→ 阶段 C（CDNA 引擎）→ 阶段 D（双轨校准）

本 change 实施**阶段 A**（5-8 人天）：消除 2 个 PTX 特化，建立 ISA-agnostic 抽象。

## Goals / Non-Goals

**Goals**：
- ✅ 引入 `InstrDescriptor` POD 标准化指令描述符（CppTLM 仅消费不解析文本）；**含 `transaction_id:uint64_t=0` + `reserved[8]:uint8_t` 字段**（per Oracle P2-C1 阶段 B/C/D 前置预留）
- ✅ 引入 `IHazardTracker` 抽象接口（**与 vendored `IScoreboard` (commit `8acfd2d1`) 语义对齐**：5 方法 + 1 P2 阶段 C 扩展点 `mark_waiting()`，per Oracle P0-A6/P2-C2）
- ✅ 引入 `CdnaPipelineTLM`（**override 两个纯虚函数** + `LatencyClass` 枚举查表 + 保留 PTX 兼容 shim，per Oracle P0-A2）
- ✅ 全部现有 PTX 模式输出与改造前 **byte-equal**（`test_pipeline_parity.cc` 6 PipelineId × 全模式 × 2 method 矩阵 + 1000 条随机，per Oracle P1-3）
- ✅ 现有 `[pcie]/[axi]/[e2e]/[wave2]` 测试保持 100% 通过
- ✅ 新增 `[cdna-phase-a]` 标签测试验证抽象正确性
- ✅ 阶段 B（`IMemoryPort`）前置就绪（`InstrDescriptor::is_memory/target_vaddr/mem_size/transaction_id` 字段已就位）
- ✅ 引入 `PtxStringToDescriptor` 纯函数（per Oracle P1-B7 闭环验证链，不引入 PTX-EMU 内部头，Clean Room）

**Non-Goals**：
- ❌ 实施 `IMemoryPort` 异步内存 Seam（推迟至阶段 B change）
- ❌ 接入 CDNA-EMU 执行器（推迟至阶段 C change）
- ❌ 修改 PTX-EMU 端任何代码（Clean Room 边界；阶段 A 仅改造 CppTLM 端）
- ❌ 修改 23 ABI 头文件（`include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h` 冻结）
- ❌ 删除旧 `PipelineTLM`/`ScoreboardTLM`（保留作为 PTX 阶段 A 的兼容 shim，5-8 人天范围内）
- ❌ **不修改** `KernelLaunchTLM`（per Oracle P0-A3：v2 注入点改到 `CudaCoreAdapterMVP`）
- ❌ **不重写 PipelineTLM 内部为 LatencyClass 枚举表**（per Oracle P0-A1：保留 100% 源文件，shim 逐字复制 has() + 分支顺序）

## Decisions

### D1. POD 描述符设计：拒绝任何平台特化文本字段

**方案 A（采用，per Oracle P1-1 spec 权威）**：
- `InstrDescriptor` **不**含任何 PTX/SASS/CDNA 文本字段
- **不**保留 `original_instr_str` 字段（即使是 debug sidecar）
- 调试信息由调用点保留（如 `test_ptx_string_to_descriptor.cc` 的输入变量），不进 POD
- 阶段 C 接入 CDNA-EMU 时，descriptor 通过结构化字段（`pipe`/`latency_class`/`ctrl`/`dst_regs`/`src_regs`/`is_memory`/`target_vaddr`/`mem_size`/`transaction_id`）传递，零文本

**理由**：
- 阶段 A 不可破坏 PTX 模式（`byte-equal` 是 Gate A 的硬指标）
- `std::hash` 对指针字段无意义；`memcpy` 跨进程/跨平台对指针不稳定
- 为阶段 B 异步 IMemoryPort + 阶段 C CDNA `s_waitcnt` 语义铺路

**方案 B（拒绝）**：保留 `original_instr_str` 8 字节。**理由**：违反 spec "MUST NOT contain platform-specific text fields" + POD 序列化语义破坏。

### D2. PipelineTLM 双轨实现：保留旧 + 新增 CdnaPipelineTLM（per Oracle P0-A1/P0-A2）

**方案 A（采用）**：
- 旧 `PipelineTLM`（`include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc`）**保留 100% 原状**（不修改任何代码）
- 新增 `CdnaPipelineTLM`（`include/tlm/gpu/cdna_pipeline_tlm.hh`）实现相同 `IPipelineLatencyProvider` 接口
- **`CdnaPipelineTLM::get_fractional_cycles` shim 必须逐字复制 `pipeline_tlm.cc:21-94` 四个匿名命名空间函数**（`has()` + `latency_p0/p1/p2/p3`）**完整逻辑与分支顺序**，包括 `toupper(char)` 大小写不敏感 quirk
- **shim 不得经过 `LatencyClass` 枚举表**——`LatencyClass` 表**仅**服务 CDNA-facing 的 `get_latency()` API
- 注入点改到 `CudaCoreAdapterMVP`（per Oracle P0-A3）

**理由**：
- 不破坏既有 `IPipelineLatencyProvider` 接口契约（per HSK-8 ACCEPTED + 23 ABI 冻结）
- PTX-EMU `SMContext` 仍可消费旧 `PipelineTLM`（仅在 PTX 模式下）
- 阶段 C 切换时，`CudaCoreAdapterMVP` 只需换注入实例，旧路径作为 fallback 保留

**方案 B（拒绝）**：直接重写 `PipelineTLM::get_fractional_cycles` 内部实现为枚举查表。**风险**：(a) 破坏 PTX 模式 `byte-equal` 契约（Oracle P0-A1 已证明按 spec 描述的枚举表数学上不可能达成）；(b) 增加回归风险；(c) 需 PTX-EMU 端同步修改 `SMContext` 的查表调用——超出阶段 A 范围。

**方案 C（拒绝）**：重写 `PipelineTLM` 内部但保留 `get_fractional_cycles` 旧接口（内部委托给枚举表 + 字符串 fallback）。**风险**：与方案 B 同（破坏 byte-equal）。

### D3. HazardTracker 双模实现：单文件双枚举

**方案 A（采用，per Oracle P0-A6）**：
- `IHazardTracker` 抽象接口（`include/tlm/gpu/hazard_tracker_interface.hh`）：与 vendored `IScoreboard` (commit `8acfd2d1`) **语义对齐**：
  - `has_free_entry()` (同 IScoreboard)
  - `try_acquire(const InstrDescriptor&, sm, wave)→bool`（对齐 `IScoreboard::allocate(reg, warp)→bool`，duplicate reject）
  - `release(const InstrDescriptor&, sm, wave)`（对齐 `IScoreboard::release`）
  - `tick()` (同 IScoreboard)
  - `mark_waiting(sm, wave, wait_vmcnt, wait_lgkmcnt, wait_expcnt)` —— **P2 阶段 C 扩展点**，default no-op
- `ScoreboardTLMv2`（`include/tlm/gpu/scoreboard_tlm_v2.hh`）实现 IHazardTracker，构造函数接受 `enum class Mode { kVirtualReg, kHardwareCounter }`
- `kVirtualReg` 模式内部**组合** + 影子 per-warp outstanding 集合（`#include` 复用旧 ScoreboardTLM 实现）
- `kHardwareCounter` 模式仅占位实现（保留计数器字段，CDNA `s_waitcnt` 在阶段 C 接入）
- **`[[deprecated("stage C only")]]` 加在枚举值 `kHardwareCounter` 上**（C++17 enumerator deprecation），**NOT 在构造函数上**（会连累 kVirtualReg）

**理由**：
- 与 IScoreboard 语义对齐保留 PTX-EMU rollback 依赖（duplicate-allocate → false → sm_context rollback）
- 阶段 C `s_waitcnt` 联合 wait 语义通过 mark_waiting 扩展点接入，**不破坏 ABI**
- 数组维度 [MAX_SM_ID=64][MAX_WAVE_PER_SM=64] per `apu_soc_v1.json` 默认 cu_count=64

**方案 B（拒绝）**：拆分为 `VirtualRegHazardTracker` + `HardwareCounterHazardTracker` 两个独立类。**理由**：增加文件数量（per Oracle P2-3），且阶段 C 之前 HardwareCounter 实现不稳定。

**方案 C（拒绝）**：自由发明新接口（`notify_instruction_issued/completed` void API）。**理由**：(a) 丢失 IScoreboard bool reject 语义；(b) 与 vendored ABI 失配（Oracle P0-A4）。

### D4. InstrDescriptor 内存布局：POD + reserved 填平 padding hole（per Oracle P1-B4）

**方案 A（采用）**：
```cpp
struct CtrlBits {
    uint8_t vmcnt_req;       // 1 byte
    uint8_t lgkmcnt_req;     // 1 byte
    uint8_t expcnt_req;      // 1 byte
    uint8_t wait_vmcnt;      // 1 byte
    uint8_t wait_lgkmcnt;    // 1 byte
    uint8_t wait_expcnt;     // 1 byte
}; // 总计 6 bytes (per arch 11 §11.2.1 默认值 0xFF)

struct InstrDescriptor {
    PipeClass pipe;                 // 1 byte
    LatencyClass latency_class;     // 1 byte
    CtrlBits ctrl;                  // 6 bytes
    uint16_t dst_regs[4];           // 8 bytes
    uint16_t src_regs[4];           // 8 bytes
    uint8_t  num_dst;               // 1 byte
    uint8_t  num_src;               // 1 byte
    bool     is_memory;             // 1 byte
    // 1 byte padding to align target_vaddr
    uint64_t target_vaddr;          // 8 bytes
    uint32_t mem_size;              // 4 bytes
    // 4 bytes padding to align transaction_id
    uint64_t transaction_id;        // 8 bytes (阶段 B IMemoryPort tag)
    uint8_t  reserved[8];           // 8 bytes (阶段 C prefetch / 阶段 D tensor tile 预留)
}; // 总计 56 bytes (with padding)
```

**理由**：
- 全 POD，`std::memcpy`/`std::hash` 友好
- 不用 `pragma pack`（避免非对齐访问）—— 用具名 `reserved[8]` 填平 padding hole + 阶段 C/D 字段预留
- `static_assert(sizeof(InstrDescriptor) == 48 || == 56, "size drift from arch 11")` 锁死布局
- 字段覆盖阶段 B IMemoryPort 所需的所有信息（`is_memory` + `target_vaddr` + `mem_size` + `transaction_id`）

**方案 B（拒绝）**：`std::variant<PipeClass, MemoryOp>` + 模板特化。**理由**：增加编译开销，对 PTX 阶段 A 的 byte-equal 验证增加复杂度。

**方案 C（拒绝）**：`#pragma pack(push,1)` 实现 39 字节 packed。**理由**：引发非对齐访问 + `reserved` 字段难以自然嵌入。

### D5. 测试策略：枚举式分支覆盖 + 随机双层

**策略**：6 个新测试文件 + 1 个端到端 parity 测试
1. `test_instruction_descriptor.cc`：POD 结构字段单元测试（序列化、对齐、`std::hash` collision 验证）
2. `test_cdna_pipeline_tlm.cc`：`LatencyClass` 6 个枚举值的查表精度 + `get_fractional_cycles` 6 PipelineId × 全模式矩阵 + `get_fractional_cycles_by_type` 全 6 管线矩阵
3. `test_scoreboard_tlm_v2.cc`：双模式单元测试（kVirtualReg/kHardwareCounter）+ 越界抛 `std::out_of_range`
4. `test_pipeline_parity.cc`：**枚举式分支覆盖 + 1000 条随机（固定 seed）双层**：
   - 硬编码覆盖 latency_p0/p1/p2/p3 每个 return 分支 ≥2 用例
   - 6 PipelineId × 全模式矩阵
   - `get_fractional_cycles_by_type` 全 6 管线矩阵
   - 边界（空串/无匹配/大小写变体）
5. `test_cuda_core_adapter_v2_paths.cc`：默认路径不变 / v2 注入 / v2 nullptr 回退 / 混合注入
6. `test_ptx_string_to_descriptor.cc`：覆盖 6 个 PTX pattern × 2 method（per Oracle P1-B7 闭环验证链）
7. **端到端 PTX parity**：跑现有 `[wave2]`/PTX 模式 E2E，记录改造前后 `kernels_launched`/cycle 计数做 diff（per Oracle P1-5）

**理由**：
- `test_pipeline_parity` 是阶段 A 的核心质量门禁（per ADR-SOC-15 Gate A）
- 端到端 parity 验证类级 parity 通过后仿真输出无漂移
- 不依赖真实 CDNA emulator（阶段 A 实施时无 CDNA-EMU）

### D6. 向后兼容策略：3 路径切换在 CudaCoreAdapterMVP（per Oracle P0-A3）

```cpp
// 路径 1 (默认, HSK-8 既有): PTX 模式无 setter 调用
// CudaCoreAdapterMVP::inject_timing_modules() 内部 make_unique<ScoreboardTLM/PipelineTLM>
facade->attach_timing(static_cast<void*>(scoreboard_.get()),
                     static_cast<void*>(pipeline_.get()),
                     static_cast<void*>(tensor_core_.get()));

// 路径 2 (新增): CDNA 模式注入 v2
adapter.set_scoreboard_v2(scoreboard_v2_vreg);  // kVirtualReg
adapter.set_pipeline_v2(cdna_pipeline_tlm);     // kPtxCompat
// inject_timing_modules() 优先使用 v2 实例

// 路径 3 (阶段 C): 硬件计数器模式
adapter.set_scoreboard_v2(scoreboard_v2_counter);  // kHardwareCounter [[deprecated]]
adapter.set_pipeline_v2(cdna_pipeline_tlm);         // kCdnaStrict
// kHardwareCounter 启用时, IMemoryPort 异步测量 memory latency
```

**理由**：
- **注入点改到 `CudaCoreAdapterMVP` 而非 `KernelLaunchTLM`**（per Oracle P0-A3：KernelLaunchTLM 当前无 setter 且 tick() 不消费 scoreboard/pipeline）
- 阶段 A 实施后，PTX 模式走路径 1（不变），CDNA 模式走路径 2（kVirtualReg）
- 阶段 C 切换到路径 3（真实 CDNA + kHardwareCounter）
- 三路径并存保证阶段 A→B→D 期间不破坏既有测试
- **`KernelLaunchTLM::tick()` 真实驱动 `facade_->exe_once()` 仍是阶段 B ADR-SOC-15 B.4 待办**（per `kernel_launch_tlm.hh:13`）

## Risks / Trade-offs

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | `PipelineTLM` 与 `CdnaPipelineTLM` 双轨导致 API 膨胀 | 🟡 中 | 阶段 A 末尾 review；阶段 C 启动时合并接口（提交 [CDNA-Phase-C] Cleanup-Trigger issue 强制追踪） |
| **R2** | `InstrDescriptor` POD 字段不够 future-proof（阶段 D 可能要扩展）| 🟢 低 | **阶段 A 已用 `reserved[8]` 预留**（per Oracle P2-C1 强烈建议） |
| **R3** | `kHardwareCounter` 模式占位实现被误用 | 🟢 低 | **枚举值级别 `[[deprecated("stage C only")]]`**（per Oracle P2-1 修正；C++17 允许）；测试文件内 `#pragma GCC diagnostic ignored` 局部抑制 |
| **R4** | `byte-equal` 验证失败（PTX 模式输出漂移）| 🟡 中 | `test_pipeline_parity` 强制枚举式 + 随机双层；端到端 `kernels_launched`/cycle 计数 diff；任何漂移立即阻断 Gate A |
| **R5** | `CudaCoreAdapterMVP` 三路径切换逻辑复杂度 | 🟢 低 | 阶段 A 优先保证路径 1/2 切换正确；路径 3 阶段 C 实施 |
| **R6** | 文件数量增加（+11 文件 = 5 头 + 4 实现 + 6 测试）| 🟢 低 | 阶段 A 范围内可接受；阶段 C 启动时审视合并 |
| **R7** | PTX-EMU 端 SMContext 仍用旧 IPipelineLatencyProvider 字符串查表 | 🟢 低 | 阶段 A 不修改 PTX-EMU；阶段 C 才同步切换 |
| **R8**（新增） | `PtxStringToDescriptor` 解析与 PTX-EMU decode 路径不一致（per Oracle P1-4）| 🟢 低 | **本 change 不消费 PTX-EMU 内部头**（Clean Room）；仅供单测与 parity；阶段 B 接入时若发现 drift, 用 PTX-EMU 端 decode 转换器替换 |
| **R9**（新增） | `[[deprecated("stage C only")]]` 构造 vs enumerator 落点（per Oracle P2-1）| 🟢 低 | 已修正：枚举值级别 + 测试文件内 `#pragma GCC diagnostic ignored`；CMake 无 -Werror 时仅 IDE 警告噪音 |
| **R10**（新增） | `get_latency` 返回 `-1.0` 哨兵（kMemoryVariable）污染 double timing API 下游（per Oracle P2-2 / P1-B6）| 🟡 中 | spec 显式承认"阶段 B 接入 IMemoryPort 时可迁移 `std::optional<double>`"；tasks.md 5.x 加注 |
| **R11**（新增） | `set_pipeline_v2(CdnaPipelineTLM*)` 收具体类而非接口（per Oracle P2-C6）| 🟢 低 | 阶段 A 接受；阶段 C 启动时改为 `IPipelineLatencyProvider*` 收口 |

## ADR 一致性显式化

> **本 change 显式偏离 ADR-SOC-15 §4.1 A.3 字面计划**：
> - ADR §4.1 A.3 写"PipelineTLM::get_fractional_cycles 重写为 get_latency(LatencyClass) 枚举查表"
> - 本 change 实际方案是**新增 CdnaPipelineTLM + PipelineTLM 双轨**（per design D2），**不修改** PipelineTLM 源文件
> - 偏离理由：per Oracle P0-A1，按 ADR 字面计划"枚举表 {1, 4, 8, 16, 32}"无法表示 PipelineTLM 实际连续值 (4.22, 2.0, 20.0, 200.0)，会破坏 PTX 模式 byte-equal 契约
> - **阶段 C 启动时需同步修订 ADR §4.1 表 A.3 描述**（本 spec 在 Gate A 验收任务 8.5 中显式追踪）
