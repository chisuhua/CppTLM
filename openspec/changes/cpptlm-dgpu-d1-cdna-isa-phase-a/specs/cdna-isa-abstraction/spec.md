# Spec: cdna-isa-abstraction

> **Capability**: cdna-isa-abstraction
> **Source change**: [`cpptlm-dgpu-d1-cdna-isa-phase-a`](../)
> **Parent ADR**: [`ADR-SOC-15-cdna-real-isa-roadmap.md`](../../../../docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md)
> **Parent design**: [`docs/soc_arch/architecture/11-cdna-real-isa-integration.md`](../../../../docs/soc_arch/architecture/11-cdna-real-isa-integration.md)
> **Pre-implementation audit**: Oracle `ses_f962d8ef5ffe2tuGZUCI4gfY0T` (5 P0 + 5 P1 + 3 P2 closed)
> **Created**: 2027-02-09 (revised 2027-02-09)
> **Status**: Proposed（阶段 A 启动；post-Oracle-revision）

> **bit-identical 定义**：**本 spec 中"bit-identical"指 `CdnaPipelineTLM::get_fractional_cycles(instr, pipe) / get_fractional_cycles_by_type(type, pipe)` 与 `PipelineTLM` 对应方法在相同输入下返回 bit-equal 的 `double` 值**。shim 不得经过 `LatencyClass` 枚举表以求简化——必须逐字保留 `has()` 的大小写不敏感语义与**分支顺序**（`pipeline_tlm.cc:21-137`），包括 `bar` 等**无任何匹配分支**的退化路径（P0→1.0, P3→200.0）。

---

## ADDED Requirements

### Requirement: cpptlm-instruction-descriptor

The system MUST provide a `cpptlm::gpu::InstrDescriptor` POD struct in `include/tlm/gpu/instruction_descriptor.hh` containing:

- `PipeClass pipe` (1 byte, 7 values): `{kScalarALU, kVectorALU, kMatrixCore, kBranch, kLSU_Global, kLSU_LDS, kSpecialSync}`
  - **注**：`PipeClass`（7 值，InstrDescriptor 内部字段）与 vendored `PipelineId`（6 值，外部接口分发参数）**不是同一类型**；两者映射关系见 `cpptlm-cdna-pipeline` 章节的 P5 PipelineId 映射表。
- `LatencyClass latency_class` (1 byte, 6 values): `{kFixed1Cycle, kFixed4Cycles, kFixed8Cycles, kFixed16Cycles, kMatrixHeavy, kMemoryVariable}`
  - **阶段 A 离散桶语义**：`kFixed4Cycles=4` / `kFixed8Cycles=8` / `kFixed16Cycles=16` / `kMatrixHeavy=32` 是**占位**离散值，**不是**真值；PTX 真值由 `CdnaPipelineTLM` shim 经 `PipelineTLM::get_fractional_cycles()` 走 has() 查表获得（4.22 / 2.0 / 8.0 / 20.0 / 200.0 等连续值）。
  - **kMemoryVariable 哨兵**：返回 `-1.0`，**显式**标识"延迟由 `IMemoryPort` 异步测量决定"（阶段 B 接入时此哨兵**可迁移**至 `std::optional<double>`，本 spec 接受此类迁移）。
- `CtrlBits ctrl` (**6 bytes** = 6 fields × 1 byte each, **not 1 byte**): containing `vmcnt_req`, `lgkmcnt_req`, `expcnt_req`, `wait_vmcnt`, `wait_lgkmcnt`, `wait_expcnt` —— 每字段 `uint8_t`（per arch 11 §11.2.1，默认值 0xFF）
- `uint16_t dst_regs[4]` + `uint16_t src_regs[4]` (16 bytes total)
- `uint8_t num_dst` + `uint8_t num_src` + `bool is_memory` + `uint64_t target_vaddr` + `uint32_t mem_size` (memory op metadata, 阶段 B IMemoryPort 接入)
- **`uint64_t transaction_id` (8 bytes, 阶段 B IMemoryPort::send_request tag 回执匹配用；阶段 A 默认 0)**
- **`uint8_t reserved[8]` (8 bytes, 阶段 C prefetch_hint / 阶段 D tensor tile 字段预留; 阶段 A 末尾静态断言 reserved[0..7] == 0; 任何阶段首次使用前需 `static_assert(reserved_used_bytes <= 8)` 校验不超预留)**

The struct MUST be `std::is_trivially_copyable` (POD), **`sizeof(InstrDescriptor) == 64 bytes`** (per arch 11 §11.2.1 + 字段表精确统计; 不得 packed; 不用 `pragma pack`)，and support `std::hash<InstrDescriptor>` for hashing in unordered containers. The struct MUST NOT contain any platform-specific (PTX/SASS/CDNA) text fields in the **main path**; original PTX instruction string MUST NOT be carried (even as debug sidecar; 调试信息由调用点保留，不进 POD)。

**字段大小精确统计**（验证 `sizeof == 64`，per arch 11 §11.2.1 字段序，**不**重排）:
- pipe (1) + latency_class (1) = 2
- ctrl = 6
- dst_regs[4] (8) + src_regs[4] (8) = 16
- num_dst (1) + num_src (1) + is_memory (1) + 1 byte padding (align target_vaddr) = 4
- target_vaddr = 8
- mem_size = 4
- 4 bytes padding (align transaction_id) = 4
- transaction_id = 8
- reserved[8] = 8
- 末尾 padding = 0
- **总计 = 60 bytes？**（**实际计算：2 + 6 + 16 + 4 + 8 + 4 + 4 + 8 + 8 = 60**——gcc 实测应得 **64**（含末尾 4 bytes 隐式 padding 到结构体对齐边界 8）; tasks.md 1.4 静态断言 `sizeof == 64`，实测若为 60/56 立即阻断）

#### Scenario: cross-ISA pipe class mapping
- **WHEN** a PTX `fma.rn.f32` is decoded
- **THEN** `InstrDescriptor.pipe` is set to `kVectorALU`
- **AND** `InstrDescriptor.latency_class` is set to `kFixed16Cycles` (PTX 阶段 A 离散桶占位；CDNA 阶段 C 改用 `kMatrixHeavy`)
- **AND** no platform-specific text fields are present in the struct body

#### Scenario: CDNA MFMA descriptor readiness
- **WHEN** a CDNA `v_mfma_f32_16x16x16_fp16` is decoded (阶段 C 启用)
- **THEN** `InstrDescriptor.pipe` is set to `kMatrixCore`
- **AND** `InstrDescriptor.latency_class` is set to `kMatrixHeavy`
- **AND** `InstrDescriptor.ctrl.expcnt_req = 0` (no export for MFMA)
- **AND** `InstrDescriptor.transaction_id = 0` (阶段 C 启用前 reserved)

#### Scenario: memory operation metadata
- **WHEN** a PTX `ld.global.f32 [addr]` is decoded
- **THEN** `InstrDescriptor.is_memory` is `true`
- **AND** `InstrDescriptor.target_vaddr` equals `addr`
- **AND** `InstrDescriptor.mem_size` equals 4 (32-bit load)
- **AND** `InstrDescriptor.pipe` is `kLSU_Global`
- **AND** `InstrDescriptor.latency_class` is `kMemoryVariable` (哨兵 -1.0；阶段 B 实际延迟由 `IMemoryPort` 异步测量)
- **AND** `InstrDescriptor.transaction_id = 0` (阶段 A 占位)

#### Scenario: POD serialization
- **WHEN** a caller serializes `InstrDescriptor` via `std::memcpy` or `std::bit_cast`
- **THEN** the bytes are stable across platforms (no pointer fields in main struct)
- **AND** `sizeof(InstrDescriptor)` 满足 tasks.md 1.4 静态断言

#### Scenario: hash quality under mixed ISA workload (P2 阶段 B 前置)
- **WHEN** 10k random `InstrDescriptor` entries are inserted into `std::unordered_set<InstrDescriptor>`
- **THEN** `load_factor()` stays < 0.7 without rehash
- **AND** hash collision rate < 30% (verified by histogram)

---

### Requirement: cpptlm-cdna-pipeline

The system MUST provide a `CdnaPipelineTLM` class in `include/tlm/gpu/cdna_pipeline_tlm.hh` + `src/tlm/gpu/cdna_pipeline_tlm.cc` implementing the **same** `IPipelineLatencyProvider` interface (vendored from PTX-EMU commit `9e7361b9`, frozen) as the existing `PipelineTLM` class. The class MUST override **two** pure virtual methods (not one):
- `double get_fractional_cycles(const std::string& instruction, PipelineId pipe_id) const override`
- `double get_fractional_cycles_by_type(int statement_type, PipelineId pipe_id) const override`

**`get_fractional_cycles` PTX 阶段 A 兼容 shim 语义**：必须**逐字复制** `pipeline_tlm.cc:21-94` 四个匿名命名空间函数（`has()` + `latency_p0/p1/p2/p3`）的**完整逻辑与分支顺序**，包括 `toupper(char)` 大小写不敏感的 quirk。`LatencyClass` 枚举表**仅**服务 CDNA-facing 的 `get_latency()` API，**不**经过 shim 路径。`Mode::kPtxCompat` shim **不**用 LatencyClass 中间层。

**`get_fractional_cycles_by_type` 真值表**（per `pipeline_tlm.cc:120-137` switch 表达式）：
| `PipelineId` | `get_fractional_cycles_by_type` 返回值 |
|---|---|
| `P0_INT_FP32` | 2.0 |
| `V_SIMD` | 1.0 |
| `P1_FP64` | 8.0 |
| `P2_SFU` | 8.0 |
| `P3_LSU` | 200.0 |
| `P4_TC` | 0.0 |

**`get_fractional_cycles` 真值对照表**（per `pipeline_tlm.cc:34-94` 实际值，**非** spec 早期"fma→32"等简化值）：

| `PipelineId` | 子串匹配分支 | 返回值 |
|---|---|---|
| `P0_INT_FP32` | `fma` / `mul.f32` / `add.f32` / `sub.f32` / `div` / `min.f32` / `max.f32` / `abs.f32` / `neg.f32` | 4.22 |
| `P0_INT_FP32` | `mad` / `mul.lo` / `mul.hi` / `sad` / `mul.wide` | 2.0 |
| `P0_INT_FP32` | **不匹配以上**（含 `bar`、空串、`add` 不带 `.f32`） | 1.0 |
| `V_SIMD` | （无字符串依赖）| 1.0 |
| `P1_FP64` | （无字符串依赖）| 8.0 |
| `P2_SFU` | `sin` / `cos` / `tan` | 16.0 |
| `P2_SFU` | `rcp` / `rsqrt` | 4.0 |
| `P2_SFU` | `sqrt` / `lg2` / `ex2` | 8.0 |
| `P2_SFU` | 不匹配 | 8.0 |
| `P3_LSU` | `ld.global` / `ld.volatile` | 200.0 |
| `P3_LSU` | `st.global` (**非 200.0**, per `pipeline_tlm.cc:59`) | 20.0 |
| `P3_LSU` | `ld.shared` / `st.shared` | 1.0 |
| `P3_LSU` | `ld.local` / `st.local` | 5.0 |
| `P3_LSU` | `atom` / `red.` | 200.0 |
| `P3_LSU` | `ld.`（不匹配以上）| 200.0 |
| `P3_LSU` | `st.`（不匹配以上）| 20.0 |
| `P3_LSU` | **不匹配任何**（含 `bar`、空串）| 200.0 |
| `P4_TC` | （无字符串依赖）| 0.0 |

**新方法（CDNA-facing）** `double get_latency(LatencyClass lc) const`:
- 服务 CDNA-EMU 端不传字符串、只传 `LatencyClass` 枚举的路径
- 6 入口查表：kFixed1Cycle→1, kFixed4Cycles→4, kFixed8Cycles→8, kFixed16Cycles→16, kMatrixHeavy→32, kMemoryVariable→-1.0（**哨兵**，阶段 B IMemoryPort 接入时可迁移至 `std::optional<double>`）

#### Scenario: PTX 阶段 A bit-identical parity（6 模式 × 2 method 全矩阵）
- **WHEN** `CdnaPipelineTLM` (Mode::kPtxCompat) is constructed
- **THEN** `get_fractional_cycles("fma.rn.f32", P0_INT_FP32)` returns 4.22 (NOT 32；同 `PipelineTLM`)
- **AND** `get_fractional_cycles("st.global.f32", P3_LSU)` returns 20.0 (NOT 200)
- **AND** `get_fractional_cycles("bar.sync", P0_INT_FP32)` returns 1.0 (PipelineTLM P0 fallback; no `bar` match)
- **AND** `get_fractional_cycles("bar.sync", P3_LSU)` returns 200.0 (PipelineTLM P3 fallback)
- **AND** `get_fractional_cycles("", P0_INT_FP32)` returns 1.0
- **AND** `get_fractional_cycles_by_type(0, P0_INT_FP32)` returns 2.0
- **AND** `get_fractional_cycles_by_type(0, P3_LSU)` returns 200.0
- **AND** all 6 `get_fractional_cycles` 模式 × all 6 PipelineId 矩阵返回与 `PipelineTLM` byte-equal
- **AND** all 6 `get_fractional_cycles_by_type` PipelineId 入口返回与 `PipelineTLM` byte-equal

#### Scenario: CDNA 阶段 C ready (kCdnaStrict mode)
- **WHEN** `CdnaPipelineTLM` is constructed in `Mode::kCdnaStrict` (阶段 C 启用)
- **THEN** `get_latency(kMemoryVariable)` returns -1.0 (哨兵；阶段 B 接入 `IMemoryPort` 时可迁移)
- **AND** `get_latency(kMatrixHeavy)` returns 32 (CDNA MFMA throughput baseline)
- **AND** `get_fractional_cycles` shim throws `std::logic_error` (CDNA 路径不接受 PTX 字符串)

#### Scenario: unrecognized opcode returns PipelineTLM fallback
- **WHEN** `CdnaPipelineTLM` (Mode::kPtxCompat) receives `get_fractional_cycles("", P3_LSU)` or `get_fractional_cycles("__unknown__", P3_LSU)`
- **THEN** returns 200.0 (matching `PipelineTLM::latency_p3` P3 fallback, NOT throwing)

---

### Requirement: cpptlm-hazard-tracker-v2

The system MUST provide an `IHazardTracker` abstract interface in `include/tlm/gpu/hazard_tracker_interface.hh` declaring **与 vendored `IScoreboard` (commit `8acfd2d1`) 语义对齐的接口**，**不**发明新语义：

```cpp
class IHazardTracker {
public:
    virtual ~IHazardTracker() = default;
    // IScoreboard 对齐
    virtual bool has_free_entry() const = 0;
    virtual bool try_acquire(const InstrDescriptor& instr, uint32_t sm_id, uint32_t wave_id) = 0;
    virtual void release(const InstrDescriptor& instr, uint32_t sm_id, uint32_t wave_id) = 0;
    virtual void tick() = 0;
    // P2 阶段 C 扩展点 (default no-op, 不破坏 ABI)
    virtual void mark_waiting(uint32_t /*sm_id*/, uint32_t /*wave_id*/,
                              uint8_t /*wait_vmcnt*/, uint8_t /*wait_lgkmcnt*/,
                              uint8_t /*wait_expcnt*/) { /* default no-op */ }
};
```

**关键语义决策**（与 `IScoreboard::allocate(reg_id, warp_id)→bool` 对齐）：
- `try_acquire` 返回 `bool`（false = duplicate reject / 寄存器已被占用），**保留** `IScoreboard` 的 reject 语义（PTX-EMU `sm_context.cpp:37-43` rollback 依赖此返回值）
- `try_acquire` 内部从 `InstrDescriptor::dst_regs[num_dst]` 提取 reg_id（kVirtualReg 模式）
- `release` 接受完整 `InstrDescriptor` 而非 `reg_id`——便于阶段 C kHardwareCounter 模式从 `instr.ctrl.*_req` 字段反推哪些 counter 应 decrement
- 数组维度（kHardwareCounter 模式）：`vmcnt_[MAX_SM_ID][MAX_WAVE_PER_SM]` 其中 `MAX_SM_ID=64` (per `apu_soc_v1.json` 默认 cu_count=64), `MAX_WAVE_PER_SM=64`，共 4096 项/计数器

The system MUST provide a `ScoreboardTLMv2` class in `include/tlm/gpu/scoreboard_tlm_v2.hh` + `src/tlm/gpu/scoreboard_tlm_v2.cc` implementing `IHazardTracker` with two modes selected at construction:
- `Mode::kVirtualReg`: delegates to existing `ScoreboardTLM` (CAPACITY=2048, `(reg_id, warp_id)` hash map) via **composition + 影子 per-warp outstanding 集合**——`is_stalled` 不在 `IHazardTracker` 接口里（已删除），由消费方 (CudaCoreAdapterMVP) 维护影子状态
- `Mode::kHardwareCounter`: provides `vmcnt_[64][64]` / `lgkmcnt_[64][64]` / `expcnt_[64][64]` arrays；`try_acquire(instr, sm, wave)` 按 `instr.ctrl.vmcnt_req/lgkmcnt_req/expcnt_req` **increment** 对应 counter（CDNA 真实语义：指令 issue 时增，completed 时减）；`release(instr, sm, wave)` decrement 对应 counter；`is_stalled` 在消费方通过比较 `instr.ctrl.wait_*cnt` 与当前 counter 决定

The `Mode::kHardwareCounter` constructor parameter MUST be marked `[[deprecated("stage C only")]]` at the **enumerator level** (C++17 allows enumerator deprecation, **NOT at constructor level** which would deprecate both modes)。

#### Scenario: kVirtualReg backward compat
- **WHEN** `ScoreboardTLMv2` is constructed with `Mode::kVirtualReg` and `try_acquire(instr_with_dst_reg_5, sm=0, wave=0)` is called
- **THEN** returns true (reg 5 未被占用)
- **AND** a second `try_acquire(instr_with_dst_reg_5, sm=0, wave=0)` returns **false** (duplicate reject, matches `IScoreboard::allocate` semantics)
- **AND** after `release(instr_with_dst_reg_5, sm=0, wave=0)`, the next `try_acquire` returns true again

#### Scenario: kHardwareCounter 阶段 C readiness
- **WHEN** `ScoreboardTLMv2` is constructed with `Mode::kHardwareCounter` and `try_acquire(instr_with_vmcnt_req=1_lgkmcnt_req=0_expcnt_req=0, sm=0, wave=0)` is called
- **THEN** `vmcnt_[0][0]` is incremented by 1 (初值 0 → 1)
- **AND** `lgkmcnt_[0][0]` is unchanged (0)
- **AND** `expcnt_[0][0]` is unchanged (0)
- **AND** subsequent `release(instr_with_vmcnt_req=1, sm=0, wave=0)` decrements `vmcnt_[0][0]` by 1 (1 → 0)
- **AND** `mark_waiting(0, 0, wait_vmcnt=0, wait_lgkmcnt=0, wait_expcnt=0)` marks wave 0 as waiting (阶段 C 启用)

#### Scenario: SM id out-of-range defense
- **WHEN** `ScoreboardTLMv2` (Mode::kHardwareCounter) receives any method with `sm_id >= 64` (MAX_SM_ID)
- **THEN** throws `std::out_of_range` (defense: silent UB on out-of-bounds array access)
- **AND** `vmcnt_/lgkmcnt_/expcnt_` arrays are exactly `[64][64]` (4096 entries per counter)

---

### Requirement: cpptlm-cuda-core-adapter-v2-injection

The system MUST make `CudaCoreAdapterMVP` provide **additional** injection methods (without removing existing):

**重大修订背景 (P0-A3)**：原 spec `cpptlm-kernel-launch-v2-setters` 假设在 `KernelLaunchTLM` 注入，但 `kernel_launch_tlm.hh:59-113` **无** `set_scoreboard/set_pipeline` setter，且 `tick()` 不消费 scoreboard/pipeline（仅 `cycle_counter_++` + 按 interval 触发）。**真正的 scoreboard_/pipeline_ 持有者**是 `CudaCoreAdapterMVP`（`src/tlm/gpu/cuda_core_adapter_mvp.cc:60-61`，用 `std::make_unique<ScoreboardTLM/PipelineTLM>`）。
- `void set_scoreboard_v2(IHazardTracker* tracker)` — sets the v2 hazard tracker (preferred)
- `void set_pipeline_v2(CdnaPipelineTLM* pipeline)` — sets the CDNA-aware pipeline (preferred)
- 保留 `inject_timing_modules()` 内部默认实现（HSK-8 已 make_unique 旧 ScoreboardTLM/PipelineTLM）

The `attach_timing_modules()` path MUST support 3 modes:
1. **Default (legacy)**: no v2 setters called → use `make_unique<ScoreboardTLM/PipelineTLM>` + `facade->attach_timing()` (PTX mode unchanged, per HSK-8 ACCEPTED)
2. **CDNA v2**: both v2 setters called → use injected `IHazardTracker` + `CdnaPipelineTLM` (阶段 A 阶段，kVirtualReg 模式)
3. **CDNA v2 + kHardwareCounter**: v2 setters + `Mode::kHardwareCounter` (阶段 C 启用，`[[deprecated("stage C only")]]` 警告)

This MUST NOT break any existing `[pcie]/[axi]/[e2e]/[wave2]` tests that use the default path (which still uses `make_unique<ScoreboardTLM/PipelineTLM>` internally).

#### Scenario: PTX mode unchanged (default path)
- **WHEN** `CudaCoreAdapterMVP` is constructed without any v2 setter calls
- **THEN** `inject_timing_modules()` proceeds with `make_unique<ScoreboardTLM/PipelineTLM>` legacy path
- **AND** no v2 methods are invoked
- **AND** `facade->attach_timing(static_cast<void*>(scoreboard_.get()), static_cast<void*>(pipeline_.get()), ...)` unchanged

#### Scenario: CDNA mode (v2 setters injected)
- **WHEN** `set_scoreboard_v2(ScoreboardTLMv2*)` is called with `Mode::kVirtualReg` and `set_pipeline_v2(CdnaPipelineTLM*)` is called with `Mode::kPtxCompat`
- **THEN** `inject_timing_modules()` uses injected v2 instances instead of `make_unique<...>`
- **AND** `facade->attach_timing` receives the v2 instances

#### Scenario: v2 nullptr clears v2 path (P2-C3 nullptr 防御)
- **WHEN** `set_scoreboard_v2(non_null)` is called, then `set_scoreboard_v2(nullptr)` is called
- **THEN** `inject_timing_modules()` falls back to `make_unique<ScoreboardTLM>` (default path)
- **AND** no nullptr deref occurs (verified by valgrind + asan)

#### Scenario: hybrid injection (only one v2 setter called)
- **WHEN** `set_pipeline_v2(cdna_pipeline)` is called but `set_scoreboard_v2` is NOT called
- **THEN** `inject_timing_modules()` uses v2 pipeline + legacy scoreboard (`make_unique<ScoreboardTLM>`)
- **AND** no nullptr deref occurs

---

## MODIFIED Requirements

**重大修订 (Oracle 二轮 N-P1-1)**：本 capability 文件**不再**承载 MODIFIED `cpptlm-pipeline` / `cpptlm-scoreboard` 两条 Requirement。已迁移到 [`../cpptlm-d1-p1-pipeline-scoreboard/spec.md`](../cpptlm-d1-p1-pipeline-scoreboard/spec.md) 同 change 平行 delta 文件,主 capability 按文件夹归并避免 archive 阶段主 spec 污染。

---

## REMOVED Requirements

**已迁移声明 (Oracle 二轮 N-P1-1)**：原 `cpptlm-kernel-launch-v2-setters` Requirement 已被 `cpptlm-cuda-core-adapter-v2-injection` 取代（per Oracle P0-A3: 注入点从虚构的 `KernelLaunchTLM::set_scoreboard_v2` 改为真实消费者 `CudaCoreAdapterMVP::set_scoreboard_v2`）。**不**算 REMOVED——是 ADDED 替换 + 旧 Requirement 不写在本 spec。

无（阶段 A 不删除任何现有功能；保留 `PipelineTLM` 与 `ScoreboardTLM` 作为 PTX 阶段 A 兼容 shim，MODIFIED 约束在 `../cpptlm-d1-p1-pipeline-scoreboard/spec.md`）

> **原 spec `cpptlm-kernel-launch-v2-setters` 已被 `cpptlm-cuda-core-adapter-v2-injection` 取代**（per Oracle P0-A3: 注入点从虚构的 `KernelLaunchTLM::set_scoreboard_v2` 改为真实消费者 `CudaCoreAdapterMVP::set_scoreboard_v2`）。
