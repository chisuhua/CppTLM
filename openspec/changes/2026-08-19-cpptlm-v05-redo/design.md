# cpptlm-v05-redo: dGPU Board v0.5 Architecture Design

> **版本**: v0.5 · **日期**: 2026-08-19 · **状态**: 📐 Design — 等 user review
> **所有者**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-X.16-cpptlm-v05-redo.md`](../../adr/ADR-X.16-cpptlm-v05-redo.md)
> **关联 OpenSpec**: [`openspec/changes/2026-08-19-cpptlm-v05-redo/`](../../../openspec/changes/2026-08-19-cpptlm-v05-redo/)
> **关联设计索引**: [`openspec/changes/2026-08-19-cpptlm-v05-redo/design.md`](../../../openspec/changes/2026-08-19-cpptlm-v05-redo/design.md)

---

## 1. 设计目标

### 1.1 主要目标

1. **per-warp instruction precision** — 暴露 PTX-EMU `WarpContext::stepOneWarpInstruction()`,获得 PC + cycle + status
2. **CPPTLM 端 PM4 解析** — CommandProcessor 模块解析 Mesa-style TYPE3 PM4(反 v0.4.1)
3. **Submodule + Adapter** — PTX-EMU 静态链接(非 dlopen),PtxEmuSubmoduleV05 是唯一包含 PTX-EMU 头的 .cc
4. **双路径共存** — `image_execute` 黑盒(快速模式)+ `stepOneWarpInstruction` 白盒(精确模式)互为对照
5. **12 SM 模块分级升级** — ScoreboardTLM + PipelineTLM 升 production,其他 10 个保留 Legacy

### 1.2 非目标 (Non-Goals)

- ❌ **不替代 PTX-EMU 内部 GPU 仿真器**(仅 adapter 调用其内部类)
- ❌ **不实现完整 PCIe 协议栈**(CFG + BAR0 + BAR1 最小子集,v3.0 沿用)
- ❌ **不实现 CUDA driver API**(由 UsrLinuxEmu + TaskRunner 端承担)
- ❌ **不实施 SMContext Adapter 注入(per Oracle 强推)**(D1-Full 路径废弃,v3.0 同)
- ❌ **不仿真 SM 内部 12 个 module 的全部 production 升级**(仅 2 个作为 demo)
- ❌ **不模拟多板卡、多引擎(GFX+COMPUTE+SDMA+VCN)**
- � **不模拟功耗管理 / DVFS**
- ❌ **ANTLR4 不在 CppTLM scope**(per HSK-6 §3.3)

---

## 2. 架构对比 (v3.0-extract → v0.5 redo)

### 2.1 v3.0-extract(被取代)

```
UsrLinuxEmu + TaskRunner (host) 
  ↕ PCIe MMIO (BAR0) + Doorbell + CQ (mmap'd VRAM)
CppTLM (dGPU board) [BLACK-BOX]
  ├─ DGpuBar (PCIe BAR0 + VRAM)
  ├─ Doorbell (SQ tail register)
  ├─ SubmissionQueue (NVMe FIFO)
  ├─ CompletionRing (try_pop + std::optional + host_notify)
  ├─ PtxEmuSubmodule (dlopen + 8 ABI dlsym)
  │    └─ libptxemu_device.so::image_execute(black-box)
  └─ DGpuBoardTLM (ChStreamModuleBase wrapper)
```

**特点**: 黑盒 dlopen,12 SM Legacy,周期精度仅 PCIe,PM4 委托 host。

### 2.2 v0.5 redo(目标)

```
UsrLinuxEmu + TaskRunner (host) — 仍写 ring buffer
  ↕ PCIe MMIO (BAR0) + Doorbell + CQ (mmap'd VRAM)
CppTLM (dGPU board v0.5)
  ├─ DGpuBar (PCIe BAR0 + VRAM, v3.0 沿用,256MB)
  ├─ Doorbell (SQ tail register, v3.0 沿用,strong-order 250-700ns)
  ├─ CommandProcessor (CP, 5-state FSM, **NEW — 反 v0.4.1**)
  │    ├─ Pm4Decoder (Mesa-style TYPE3, **NEW**)
  │    ├─ SubchannelContext[5] (subchannel 0-4, **NEW**)
  │    └─ CommandDispatcher (PM4 opcode → handler, **NEW**)
  ├─ SubmissionQueue[cluster] (per-cluster FIFO + Task Dependency Table)
  ├─ CompletionRing (try_pop + std::optional + host_notify)
  ├─ PtxEmuSubmoduleV05 (submodule + adapter, **NEW**)
  │    └─ PTX-EMU (git submodule) [WHITE-BOX]
  │         ├─ image_execute (黑盒兼容, 快速模式)
  │         └─ stepOneWarpInstruction (per-warp step, **NEW**)
  ├─ ComputeUnitTLM v2 (ChStreamModuleBase, **UPGRADED**)
  │    ├─ ScoreboardViewTlm (ScoreboardTLM 升级 production, **NEW**)
  │    ├─ PipelineViewTlm (PipelineTLM 升级 production, **NEW**)
  │    └─ WarpScheduler (PTX-EMU 注入,策略类)
  └─ DGpuBoardTLM (ChStreamModuleBase wrapper, 内部重构)
```

**关键差异**:
- **黑盒 → 白盒**: `image_execute` → `stepOneWarpInstruction` per-warp
- **PM4 host → CppTLM CP**: 反 v0.4.1 委托
- **dlopen → submodule**: 静态链接 + adapter 模式
- **12 Legacy → 2 production**: 仅 ScoreboardTLM + PipelineTLM 升级

---

## 3. 核心组件契约

### 3.1 CommandProcessor(NEW — 反 v0.4.1)

**位置**: `include/tlm/gpu/command_processor_v05.hh` + `src/tlm/gpu/command_processor_v05.cc`

**契约**(per design.md §3 反 v0.4.1):

```cpp
class CommandProcessor {
public:
    static constexpr uint32_t MAX_STREAMS = 1024;
    
    void init();
    
    // host → device entry (driver writes PM4 via ring buffer)
    void submit_kernel(const uint8_t* image_bytes, size_t size,
                       uint32_t grid_dim, uint32_t block_dim,
                       uint8_t stream_id, SmImageId* out_id);
    
    // 5-state FSM
    enum class State { IDLE, FETCH, DECODE, DISPATCH, COMPLETE };
    State state() const;
    void tick();  // EventQueue 调度
    
private:
    Pm4Decoder decoder_;      // Mesa-style TYPE3
    SubchannelContext sub_ctx_[5];  // subchannel 0-4
    CommandDispatcher dispatcher_;
    
    // 5 状态 FSM: IDLE → FETCH → DECODE → DISPATCH → COMPLETE
};
```

**测试要求**:
- `[command-processor]` 单包 4 opcode 路径(DISPATCH_DIRECT / EVENT_WRITE / RELEASE_MEM / ACQUIRE_MEM)
- 5-state FSM 转换正确性
- Mesa-style TYPE3 PM4 header 解析

### 3.2 Pm4Decoder(Mesa-style TYPE3)

**位置**: `include/tlm/gpu/pm4_decoder_v05.hh` + `src/tlm/gpu/pm4_decoder_v05.cc`

**契约**(per AMD PM4 spec + Mesa convention):

```cpp
struct Pm4Type3Header {
    uint32_t IT : 1;             // bit 0 (Increment Type)
    uint32_t predicate : 1;     // bit 1
    uint32_t opcode : 7;        // bits 2-8
    uint32_t reserved : 6;      // bits 9-15
    uint32_t count : 14;        // bits 16-29
    uint32_t type : 2;          // bits 30-31 = 0b11
};

class Pm4Decoder {
public:
    Pm4Packet parse_type3(uint32_t header, const uint32_t* payload, size_t max_dwords);
    
    // 18 opcodes 支持(MVP: 4 个 + 余 14 deferred)
    // P0: DISPATCH_DIRECT(0x15), EVENT_WRITE(0x46), RELEASE_MEM(0x49), ACQUIRE_MEM(0x58)
    // P1: DISPATCH_TASK, WAIT_REG_MEM, REG_RMW, SET_CONFIG_REG
    // ...
};
```

**位字段修正**(per Oracle 一审 C-NEW-3):
- `IT` 在 bit 0 (Mesa 风格)
- `predicate` 在 bit 1
- `opcode` 在 bits 2-9 (8 bits, 256 个)
- `count` 在 bits 16-29 (14 bits, 16K dwords)
- `type` 在 bits 30-31 = `0b11`

### 3.3 PtxEmuSubmoduleV05(Adapter pattern)

**位置**: `include/tlm/gpu/ptx_emu_submodule_v05.hh` + `src/tlm/gpu/ptx_emu_submodule_v05.cc`

**关键约束**:
- **唯一** include PTX-EMU 头的 .cc(编译防火墙)
- 其他 CppTLM 代码只见前向声明

**契约**:

```cpp
// include/tlm/gpu/ptx_emu_submodule_v05.hh
// 仅前向声明,不 include PTX-EMU 头
namespace ptxsim { class SMContext; }

class PtxEmuSubmoduleV05 {
public:
    void init(const std::string& ptx_emu_root);  // submodule path
    void shutdown();

    // 黑盒路径(快速模式, v3.0 兼容)
    using ImageHandle = uint64_t;
    ImageHandle image_load(const uint8_t* bytes, size_t size);
    int32_t image_execute(ImageHandle h, uint32_t gx, uint32_t gy, uint32_t gz,
                          uint32_t bx, uint32_t by, uint32_t bz,
                          size_t shared_mem, const void* args, size_t argc);

    // 白盒路径(精确模式, NEW v0.5)
    int32_t stepOneWarpInstruction(uint32_t warp_id,
                                    uint64_t* out_pc,
                                    int32_t* out_status,
                                    uint64_t* out_cycle_count);

    // 双路径验证(per Oracle §F.4)
    bool verify_dual_path_consistency(uint32_t max_warp_steps);

private:
    // 实现细节:持有 PTX-EMU::SMContext 实例(在 .cc 中 include)
    ptxsim::SMContext* sm_context_ = nullptr;
    // ... 其他 PTX-EMU 实例
};
```

### 3.4 ComputeUnitTLM v2(UPGRADED)

**位置**: `include/tlm/gpu/compute_unit_v05.hh` + `src/tlm/gpu/compute_unit_v05.cc`

**架构**:

```cpp
class ComputeUnitTLM : public ChStreamModuleBase {
public:
    ComputeUnitTLM(const std::string& name, EventQueue* eq);
    void init(PtxEmuSubmoduleV05& ptx);

    // 任务接收
    void issueTask(const TaskEntry& entry);

    // 黑盒模式: 走 image_execute
    int32_t dispatch_blackbox(const DispatchParams& params);

    // 白盒模式: 走 stepOneWarpInstruction(per Oracle §F.1)
    int32_t dispatch_whitebox(uint32_t warp_count, uint64_t max_cycles);

    // 状态查询
    CU_State state() const;
    uint32_t pending_warps() const;

private:
    PtxEmuSubmoduleV05& ptx_;
    ScoreboardViewTlm scoreboard_;   // 仅 ScoreboardTLM 升级(cycle 模型)
    PipelineViewTlm pipeline_;        // 仅 PipelineTLM 升级(latency issue)
    std::vector<uint32_t> active_warps_;
};
```

**测试要求**:
- `[compute-unit]` 单元测试(Mock PtxEmuSubmoduleV05)
- 双路径 byte-identical(同一 kernel,黑盒 vs 白盒,寄存器/内存终态逐字节 diff)

### 3.5 ScoreboardTLM 升级(per-warp cycle)

**位置**: `include/tlm/gpu/scoreboard_tlm_v05.hh`(升级现有 scoreboard_tlm.hh)

**关键升级**:
- 现有 `ScoreboardTLM::tick()` 增加 per-warp tracking
- 新增 `WarpState { pc, cycle_count, register_dependencies }` 数据结构
- 与 PTX-EMU::SMContext 通过 stepOneWarpInstruction 同步

### 3.6 PipelineTLM 升级(latency issue)

**位置**: `include/tlm/gpu/pipeline_tlm_v05.hh`(升级现有 pipeline_tlm.hh)

**关键升级**:
- 现有 5+V 阶段 latency 表保留
- 新增 `issue(latency)` API,与 PTX-EMU::Pipeline::step_b_set_blocked_cycles 同步

---

## 4. 端到端数据流 (v0.5 redo)

```
┌────────────────────────────────────────────────────────────────────────�
│                          Host (UsrLinuxEmu + TaskRunner)                │
│                                                                        │
│  cuModuleLoadData(image_bytes)                                         │
│    → IOCTL 0x27 LOAD_KERNEL_MODULE                                    │
│    → 写 image_bytes → DGpuBar.vram_base()                              │
│    → 写入 gpu_gpfifo_entry payload[0..N] (PM4 DISPATCH_DIRECT)        │
│    → Doorbell ring(BAR0 0x1000 + stream_id * 8, new_tail)            │
│      (strong-order 250-700ns PCIe Gen5 x16)                          │
└────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (PCIe TLP)
┌────────────────────────────────────────────────────────────────────────┐
│                      CppTLM DGpuBoardTLM v0.5                           │
│                                                                        │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  PCIe Substrate (DGpuBar + Doorbell)                              │  │
│  │  └─ BAR0: device regs + doorbell MMIO 0x1000-0x1FFF             │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  CommandProcessor (5-state FSM)                                  │  │
│  │  ├─ IDLE: 等 doorbell wake                                       │  │
│  │  ├─ FETCH: 读 gpu_gpfifo_entry.payload[0] (PM4 header)         │  │
│  │  ├─ DECODE: Pm4Decoder (Mesa-style TYPE3, opcode 7-bit)          │  │
│  │  │    ├─ TYPE 11: TYPE3, IT=bit0, predicate=bit1                │  │
│  │  │    ├─ opcode = bits 2-9 (DISPATCH_DIRECT=0x15 etc.)          │  │
│  │  │    └─ count = bits 16-29                                    │  │
│  │  ├─ DISPATCH (op=0x15 DISPATCH_DIRECT):                         │  │
│  │  │    └─ ComputeUnitTLM.issueTask(TaskEntry)                     │  │
│  │  └─ COMPLETE: advance to next entry                               │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  ComputeUnitTLM v2 (ChStreamModuleBase)                          │  │
│  │  ├─ 白盒路径 (per-warp step):                                    │  │
│  │  │    ├─ ScoreboardViewTlm (ScoreboardTLM 升级 production)       │  │
│  │  │    ├─ PipelineViewTlm (PipelineTLM 升级 production)           │  │
│  │  │    └─ PtxEmuSubmoduleV05::stepOneWarpInstruction(per-warp)   │  │
│  │  │       └─ PTX-EMU::SMContext.stepOneWarpInstruction(...)       │  │
│  │  │            └─ 返回 PC + cycle + status                          │  │
│  │  └─ 黑盒路径 (快速模式):                                          │  │
│  │       └─ PtxEmuSubmoduleV05::image_execute(整 kernel)             │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  SubmissionQueue[cluster] (per-cluster FIFO)                     │  │
│  │  └─ tick() → ISmExecutor::dispatch → CompletionRing::push        │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  CompletionRing + FenceRegistry (host-side signal)               │  │
│  │  └─ try_pop() → host drain → return CUDA_SUCCESS                │  │
│  └────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (cuStreamSynchronize 等 fence)
�────────────────────────────────────────────────────────────────────────┐
│                          Host (UsrLinuxEmu)                             │
│  cuStreamSynchronize(stream) → FenceRegistry.wait → return CUDA_SUCCESS  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 5. 接口稳定性与版本演进

### 5.1 冻结接口(P4' 前禁止变更)

- ✅ `CPPTLMBRIDGE_VERSION = 2` 永久冻结(per D7,v3.0 黑盒 `image_execute` 仍可用)
- ✅ `PtxEmuSubmoduleV05::image_load` / `image_execute`(黑盒兼容,v3.0 ABI 透传)
- ✅ `PtxEmuSubmoduleV05::stepOneWarpInstruction` 签名(per-warp step API,v0.5 新增)
- ✅ `CompletionRing` `try_pop()` + `std::optional<Entry>`
- ✅ `KernelLaunchRequest` 结构(已存在,v3.0 沿用)

### 5.2 可演进接口(P1'-P3' 期间允许调整)

- 🟡 `CommandProcessor` 5-state FSM 内部实现
- 🟡 `Pm4Decoder` 字段解析顺序与子模块拆分
- 🟡 `ScoreboardViewTlm` / `PipelineViewTlm` 内部 cycle 模型

### 5.3 内部接口(非用户面对,P4' 后允许调整)

- 🟡 `ptx_emu_submodule_v05.cc` 内 `ptxsim::SMContext*` 实例化细节
- 🟡 `compute_unit_v05.cc` 内 ScoreboardTLM / PipelineTLM 与 PTX-EMU 同步协议

---

## 6. 测试策略

### 6.1 单元测试(W2-10)

| 模块 | 测试 | Catch2 标签 |
|------|------|-------------|
| Pm4Decoder | Mesa-style TYPE3 header 解析 | `[pm4-decoder]` |
| Pm4Decoder | 4 opcode 路径(MVP) | `[pm4-decoder][opcode]` |
| CommandProcessor | 5-state FSM 转换 | `[command-processor]` |
| CommandProcessor | 单包 dispatch 路径 | `[command-processor]` |
| PtxEmuSubmoduleV05 | stepOneWarpInstruction API | `[ptx-emu-v05][step]` |
| PtxEmuSubmoduleV05 | submodule 加载 | `[ptx-emu-v05][init]` |
| ComputeUnitTLM v2 | 双路径调度 | `[compute-unit][dual-path]` |
| ScoreboardTLM | per-warp cycle tracking | `[scoreboard-v05]` |
| PipelineTLM | latency issue | `[pipeline-v05]` |

### 6.2 双路径内部一致性测试(W8-10 关键)

```cpp
TEST_CASE("Dual-path consistency: image_execute vs stepOneWarpInstruction",
          "[v0.5][dual-path]") {
    PtxEmuSubmoduleV05 ptx;
    ptx.init(MOCK_PTXEMU_V05_SO);

    uint8_t bytes[16] = {0xDE, 0xAD};
    auto handle = ptx.image_load(bytes, 16);

    // 黑盒:整 kernel
    DispatchParams p{1, 1, 1, 1, 1, 1, 0};
    auto status_black = ptx.image_execute(handle, p, ...);

    // 白盒:per-warp step
    uint64_t pc, cycles;
    int32_t status;
    int total_cycles = 0;
    while (true) {
        auto rc = ptx.stepOneWarpInstruction(0, &pc, &status, &cycles);
        total_cycles += cycles;
        if (rc != 0) break;
    }

    // 必须 byte-identical 终态
    REQUIRE(status_black == status);
    REQUIRE(total_cycles == cycles_black_total);
}
```

### 6.3 集成测试(W10-12)

| 测试 | 内容 | 标签 |
|------|------|------|
| cuModuleLoadData E2E | IOCTL → CppTLM CP → DGpuBar.vram_base → image_load | `[P3][E2E]` |
| cuLaunchKernel E2E (黑盒) | IOCTL → Doorbell → CP → SQ → image_execute | `[P3][E2E][blackbox]` |
| cuLaunchKernel E2E (白盒) | IOCTL → Doorbell → CP → SQ → stepOneWarpInstruction | `[P3][E2E][whitebox]` |
| Dual-path consistency | 同 kernel 黑盒+白盒 终态 byte-identical | `[P3][E2E][dual]` |

### 6.4 验证范围(per A2)

- ✅ **PTX-EMU 自家 test corpus** — 继续绿
- ✅ **双路径内部一致性** — byte-identical 终态对比
- ❌ **真实 GPU 周期对齐** — 无独立 nvprof/ncu-sys golden,**不声称**
- ❌ **gpgpu-sim / Accel-sim 对比** — 不在本 v0.5 范围

---

## 7. 风险与缓解

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | PTX-EMU maintainer 拒收新 API | 中 | 高 | 上游 PR 先行,本地 fork 兜底 |
| R3 | fork 长期 merge 冲突 | 中 | 中 | 监控 diff 量,>1000 LOC 评估独立 release |
| R4 | UsrLinuxEmu 需同步调整 | 中 | 中 | HSK-7 cc UsrLinuxEmu |
| R5 | submodule 编译依赖扩散(ANTLR4 等) | 中 | 低 | 符号可见性 -fvisibility=hidden |
| R6 | 验证无独立 golden | 已确认 | 中 | 文档化 "内部一致性,无独立参考" |
| R7 | 12 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ P1 推迟其余 |
| R8 | PtxEmuSubmoduleV05 编译防火墙破裂 | 低 | 高 | 严格 `git grep #include.*ptxsim/` 检查 |
| R9 | Mesa-style PM4 bit field 与 KFD 不同 | 中 | 中 | 同时验证 Mesa + KFD 两种 convention |

---

## 8. Migration 路径(P0'-P4' 12 周)

| 阶段 | 周 | 关键交付 |
|------|----|--------|
| **P0'** (W1) | 1 | HSK-7 + submodule 添加 + ADR-X.16 + 本 change |
| **P1'** (W2-4) | 3 | CommandProcessor + Pm4Decoder + PtxEmuSubmoduleV05 adapter |
| **P2'** (W5-7) | 3 | ComputeUnit v2 + ScoreboardTLM/PipelineTLM 升级 |
| **P3'** (W8-10) | 3 | 双路径验证 + docs 同步 |
| **P4'** (W11-12) | 2 | 收尾 + v0.5.0 tag |

**跨仓 commit 顺序**(per ADR-035 §R5.1):
```
[1] PTX-EMU 新 API PR (stepOneWarpInstruction) → upstream review + merge
[2] CppTLM submodule pin → submodule add → adapter 实施
[3] CppTLM 双路径验证 → docs 同步
```

---

## 9. 配套文档

- [`ADR-X.16-cpptlm-v05-redo.md`](../../adr/ADR-X.16-cpptlm-v05-redo.md) — 8 项决策锁定
- [`openspec/changes/2026-08-19-cpptlm-v05-redo/proposal.md`](../../../openspec/changes/2026-08-19-cpptlm-v05-redo/proposal.md) — 实施提案
- [`docs/research/CP/`](../../research/CP/) — 12 Command Processor 专利解析
- [`docs/research/WDU/`](../../research/WDU/) — 5 Work Distribution Unit 专利解析

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 等 user review
**下次更新**: W1 P0' 启动后(submodule pin + HSK-7 公告确认)
