# cpptlm-v05-mvp-s1-ptxemu-integration: Design

> **配套**: [`proposal.md`](../proposal.md) · [`tasks.md`](../tasks.md) · [`specs/`](../specs/)
> **状态**: 📐 Design — 与 ADR-SOC-06 D2/D3 同步 · **Owner**: CppTLM Team
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)

## 1. 架构概览(本 change)

```
┌─────────────────────────────────────────────────────────────────┐
│ CudaCoreAdapter (SM 微架构探索器,per Phase I.2)                │
│                                                                  │
│   on_cta_arrival(cta_desc)                                       │
│     ├─ PtxEmuSubmoduleMVP::decode_ptxir()                        │
│     ├─ PtxEmuSubmoduleMVP::submit_kernel_request()               │
│     └─ PtxEmuSubmoduleMVP::create_gpu_context()                   │
│                                                                  │
│   tick()  ★ timing 主入口                                        │
│     └─ sm->exe_once()  (PTX-EMU 内部 3-Step 注入)               │
│          ├─ Step A: scoreboard->allocate()                       │
│          ├─ Step B: pipeline->get_fractional_cycles()           │
│          └─ Step C: scoreboard->release()                       │
│                                                                  │
│   on_warp_complete() / WarpState 镜像                            │
│     └─ read_active_mask() / read_blocked_cycles()  ← facade       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (经 PtxEmuSubmoduleMVP facade 转发)
┌─────────────────────────────────────────────────────────────────┐
│ PtxEmuSubmoduleMVP (PTX functional facade,per Phase I.1)      │
│                                                                  │
│   Functional Construction:                                       │
│     create_gpu_context() / decode_ptxir() / submit_kernel_      │
│     request() / get_sm_context() / get_warp_context()            │
│                                                                  │
│   Functional Execute(★ 不增加 cycle):                           │
│     functional_execute_warp(warp, stmt, target_pc)              │
│     → WarpContext::execute_warp_instruction()                   │
│                                                                  │
│   Functional State:                                              │
│     read_register<T>() / write_register<T>()                    │
│     read_global_memory<T>() / write_global_memory<T>()          │
│     read_thread_pc() / advance_thread_pc()                       │
│     read_active_mask() / read_blocked_cycles()  (FIX-H8/B.3)    │
│     is_warp_finished() / is_thread_exited()                      │
│                                                                  │
│   Module Getters(本期 MVP 不使用,保留供 v0.5):                  │
│     create_scoreboard() / create_pipeline_latency_provider()    │
│     create_tensor_core_timing()                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (PTX-EMU 内部 C++ 实例方法,深度集成)
┌─────────────────────────────────────────────────────────────────┐
│ PTX-EMU@87820951(git submodule)                                │
│   ptxsim::GPUContext / SMContext / WarpContext / ThreadContext   │
│   ptx_ir::PtxirReader / StatementContext                          │
│   memory::SimpleMemory / register::RegisterBankManager          │
└─────────────────────────────────────────────────────────────────┘
```

**关键架构契约**(per Phase I.1/I.2):
1. **PtxEmuSubmoduleMVP 是 CppTLM 唯一 include PTX-EMU 头文件的位置**(编译防火墙,`git grep` 验证)
2. **CudaCoreAdapter 不直接调 PTX-EMU 内部接口** — 全部通过 PtxEmuSubmoduleMVP facade 转发
3. **functional/timing 严格分离**(per gpgpu-sim 分层) — facade 接口无 cycle/stall/hazard,adapter 接口无 functional 状态

## 2. 接口表(per Phase I.1/I.2,本 change 子集)

### 2.1 PtxEmuSubmoduleMVP(Functional facade)

```cpp
// include/tlm/gpu/ptx_emu_submodule_mvp.hh(前向声明 PTX-EMU 内部类)
class PtxEmuSubmoduleMVP {
public:
    // Construction
    void init(const std::string& ptx_emu_root, const GPUConfig& config);
    void shutdown();

    // Functional Construction
    std::unique_ptr<ptxsim::GPUContext> create_gpu_context();
    ptxsim::SMContext* get_sm_context(ptxsim::GPUContext& gpu, uint32_t sm_idx = 0);
    ptxsim::WarpContext* get_warp_context(ptxsim::SMContext& sm, uint32_t warp_idx);
    std::vector<ptx_ir::StatementContext> decode_ptxir(const uint8_t* image_bytes, size_t size);
    void submit_kernel_request(ptxsim::GPUContext& gpu, ptxsim::KernelLaunchRequest&& req);

    // Functional Execute(★ 不增加 cycle)
    void functional_execute_warp(ptxsim::WarpContext& warp, ptx_ir::StatementContext& stmt, int target_pc);
    void functional_barrier_sync(ptxsim::WarpContext& warp, int barrier_id);
    void functional_exit_warp(ptxsim::WarpContext& warp);

    // Functional State(读)
    template <typename T> T read_register(const ptxsim::WarpContext& warp, int lane_id, const std::string& reg_name);
    template <typename T> T read_global_memory(const ptxsim::GPUContext& gpu, uint64_t address);
    uint32_t read_thread_pc(const ptxsim::WarpContext& warp, int lane_id);
    uint32_t read_blocked_cycles(const ptxsim::WarpContext& warp);  // FIX-H8/B.3 补缺
    uint32_t read_active_mask(const ptxsim::WarpContext& warp);
    bool is_warp_finished(const ptxsim::WarpContext& warp);
    bool is_thread_exited(const ptxsim::WarpContext& warp, int lane_id);

    // Functional State(写)
    template <typename T> void write_register(...);
    template <typename T> void write_global_memory(...);
    void advance_thread_pc(ptxsim::WarpContext& warp, int lane_id, int new_pc);

    // Module Getters(MVP 不使用,保留供 v0.5)
    IScoreboard* create_scoreboard();
    IPipelineLatencyProvider* create_pipeline_latency_provider();
    ITensorCoreTiming* create_tensor_core_timing();
};
```

### 2.2 CudaCoreAdapter(SM 微架构探索器)

```cpp
// include/tlm/gpu/cuda_core_adapter_mvp.hh
class CudaCoreAdapter {
public:
    // WarpState(timing only,**不**含 PC,per Phase I.2 §3)
    struct WarpState {
        uint64_t cycle_count = 0;
        uint32_t exec_mask = 0;
        uint32_t blocked_cycles = 0;
        bool     scheduler_state = false;
    };

    void init(PtxEmuSubmoduleMVP& facade);
    bool on_cta_arrival(const CtaDescriptor& cta);
    void tick();  // ★ timing 主入口
    void on_warp_complete(uint64_t task_id, int32_t status);

    WarpState warp_state(uint32_t warp_id) const;
    bool is_idle() const;
};
```

## 3. 编译防火墙

```
include/tlm/gpu/ptx_emu_submodule_mvp.hh     ← 仅前向声明
src/tlm/gpu/ptx_emu_submodule_mvp.cc       ← ⭐ 唯一 include PTX-EMU 头
include/tlm/gpu/cuda_core_adapter_mvp.hh   ← 不 include PTX-EMU 头
src/tlm/gpu/cuda_core_adapter_mvp.cc       ← 不 include PTX-EMU 头
external/PTX-EMU/                           ← git submodule
```

**验证命令**(per s1-G4):
```bash
git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/" \
  -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
# 预期: 仅 src/tlm/gpu/ptx_emu_submodule_mvp.cc
```

## 4. 关键时序特性(本 change)

| 阶段 | 延迟 | 备注 |
|------|------|------|
| `PtxEmuSubmoduleMVP::decode_ptxir` | 静态(per PTXIR 字节流大小)| O(N) decode,N = 指令条数 |
| `PtxEmuSubmoduleMVP::functional_execute_warp` | **0 cycle** | functional model 不计 cycle(per Phase I.1 契约) |
| `CudaCoreAdapter::tick()` → `sm->exe_once()` | 1 cycle | timing model 主入口(per Phase I.2) |
| `CudaCoreAdapter::on_warp_complete` → SQ | ~1 tick | 反向流(由 s2 处理)|

## 5. 测试策略

### 5.1 Functional 单元测试(6 类,per Phase I.1 §6)

| 模块 | 测试 | Catch2 标签 |
|------|------|-------------|
| PtxEmuSubmoduleMVP | `decode_ptxir` magic/version 校验 | `[ptx-emu-facade][decode]` |
| PtxEmuSubmoduleMVP | ADD/SUB/MUL/DIV 寄存器结果 | `[ptx-emu-facade][arith]` |
| PtxEmuSubmoduleMVP | LD/ST 共享/全局内存 | `[ptx-emu-facade][memory]` |
| PtxEmuSubmoduleMVP | SIMT 分支/active mask | `[ptx-emu-facade][branch]` |
| PtxEmuSubmoduleMVP | `bar.sync` 多 warp 同步 | `[ptx-emu-facade][barrier]` |
| PtxEmuSubmoduleMVP | 状态读写 round-trip | `[ptx-emu-facade][state]` |

### 5.2 Timing 单元测试(6 类,per Phase I.2 §7)

| 模块 | 测试 | Catch2 标签 |
|------|------|-------------|
| CudaCoreAdapter | per-tick cycle 推进 + WarpScheduler 行为 | `[cuda-core][mvp][tick]` |
| CudaCoreAdapter | RAW hazard + allocate/release 计数 | `[cuda-core][mvp][scoreboard]` |
| CudaCoreAdapter | Pipeline latency 注入 + blocked_cycles 镜像 | `[cuda-core][mvp][pipeline]` |
| CudaCoreAdapter | on_cta_arrival 反压 + resource 管理 | `[cuda-core][mvp][dispatch]` |
| CudaCoreAdapter | WarpState timing 状态镜像(**不**含 PC) | `[cuda-core][mvp][warp-state]` |
| CudaCoreAdapter | IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 注入路径 | `[cuda-core][mvp][injection]` |

## 6. 风险与缓解(本 change 子集)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin @ `87820951` + 月度 bump PR |
| R2 | PTX-EMU 头文件 API 变更 | 中 | 高 | `abi_guards.h` 17 条静态断言守卫 ABI(per HSK-6 P0-1) |
| R3 | Functional 误调 timing-only API(如 `exe_once`) | 低 | 高 | 编译期隔离:本 facade 接口**禁止** `exe_once` / `set_blocked_cycles` 等 timing API |
| R4 | Functional 误读 PC 推到 WarpState(混淆两层职责) | 低 | 中 | 文档 + 接口表明确分离;WarpState 严格不含 PC |
| R5 | CudaCoreAdapter 裸调 PTX-EMU 内部(破坏 firewall) | 中 | 高 | 全部经 facade 转发;`read_blocked_cycles/read_active_mask` 已补(per FIX-H8/B.3) |

## 7. 阶段化交付(本 change)

```
s1-W1 (2026-08-22~28): submodule + CMake + PtxEmuSubmoduleMVP + 6 functional 测试
s1-W2 (2026-08-29~09-04): CudaCoreAdapter + 6 timing 测试 + 编译防火墙验证 + archive
```

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 与 ADR-SOC-06 + Phase I/F-H 同步
**下次更新**: W1 S1 完成时(submodule pin + 6 functional PASS)
