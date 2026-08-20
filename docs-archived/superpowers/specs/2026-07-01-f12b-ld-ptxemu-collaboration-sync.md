# F12b-LD 协作同步：CppTLM ↔ PTX-EMU

> **Purpose**: CppTLM ↔ PTX-EMU 协作规范 — Timing Model、协作流程、双路径（D1 Compute + D2 Memory）
> **Date**: 2026-07-01 (revised 2026-07-14)
> **CppTLM Contact**: CppTLM Team (main branch)

---

## 0. PTX-EMU 团队入口（必读起点）

**👉 推荐入口文档**: [`PTX-EMU-README.md`](./PTX-EMU-README.md) — 含 9 个改造任务路径规划 + 验收标准 + 协作流程图

**必读 4 个核心文档**（按顺序，~4 小时）:

| 优先级 | 文件（CppTLM 仓库） | 为什么需要读 |
|--------|----------------------|-------------|
| 🔴 | [`PTX-EMU-README.md`](./PTX-EMU-README.md) | **入口文档** — 9 任务路径规划 + 验收标准 + 协作流程图（~250 行） |
| 🔴 | [`2026-07-14-ptxemu-comprehensive-modification-plan.md`](./2026-07-14-ptxemu-comprehensive-modification-plan.md) | **顶层任务书**（920 行）。§2 MemoryBridge + §3 D1-Full Compute + §6 任务汇总 |
| 🟡 | [`2026-07-03-ptxemu-phase8b-d1full-plan.md`](./2026-07-03-ptxemu-phase8b-d1full-plan.md) | D1-Full 接口对齐详细设计（439 行）— 作为 §3 子集参考 |
| 🟡 | [`docs/adr/ADR-NV-02-phase8b-d1-strategy.md`](../../adr/ADR-NV-02-phase8b-d1-strategy.md) | 决策依据 + G-D1~G-D8 验收标准 + 风险 R1~R8 |

**本文档（f12b-ld-ptxemu-collaboration-sync.md）的定位**：协作规范（Timing Model §7、§13 D1-Full 协作节、协作流程），作为协作期间的参考。

**已弃用（不要读）**：
- ~~`2026-06-30-f12-ptxemu-ldpreload-design.md`~~ — 🗑️ 2026-07-14 已删除（被 comprehensive-plan.md §2.2/§3 吸收）
- ~~`f12-ptxemu-ldpreload-integration.md`~~ — 🗑️ 2026-07-14 已删除（被 comprehensive-plan.md §4 取代）

---

## 1. CppTLM 当前状态

### 1.1 F12a 已完成（2026-07-01）

已在 `namespace tlm` 中实现 5 个模块组件（4 个具有独立单元测试的类 + `SubCoreSlot` header-only 辅助结构体，通过 `GpuComputeUnitTLM` 间接测试），全部已注册：

| 类 | 头文件 | 测试标签 | 用途 |
|----|--------|----------|------|
| `SubCoreSlot` | `include/tlm/gpu/sub_core_slot.hh` | — (header-only) | 4-way sub-core slot 状态机 |
| `WavefrontTLM` | `include/tlm/gpu/wavefront_tlm.hh` | `[wavefront][gpu][phase7b]` | Wavefront 数据载体 |
| `VectorRegFileTLM` | `include/tlm/gpu/vector_regfile_tlm.hh` | `[vector_regfile][gpu][phase7b]` | 简化向量寄存器文件 + bank conflict |
| `MinimalWarpSchedulerTLM` | `include/tlm/gpu/minimal_warp_scheduler_tlm.hh` | `[warp_scheduler][gpu][phase7b]` | Round-robin warp 调度器 |
| `GpuComputeUnitTLM` | `include/tlm/gpu/gpu_compute_unit_tlm.hh` | `[compute_unit][gpu][phase7b]` | SM 抽象，含 4 × SubCoreSlot + scheduler |

**测试基线**: 764/764 pass (15517 assertions), docs_sync 0 missing, format clean.

### 1.2 `MinimalWarpSchedulerTLM` 接口（与 PTX-EMU `WarpScheduler` 对齐）

```cpp
class MinimalWarpSchedulerTLM : public ChStreamModuleBase {
public:
    void add_warp(uint32_t warp_id);
    void remove_warp(uint32_t warp_id);
    std::optional<uint32_t> schedule_next();
    bool all_warps_finished() const;
    void update_state(uint32_t warp_id, bool blocked, uint32_t blocked_cycles);
    void tick() override;
    // ...
};
```

这 5 个方法与 PTX-EMU `WarpScheduler` 的接口名**精确对齐**，为 Phase 8.B 替换做准备。

### 1.3 `GpuComputeUnitTLM::tick()` 执行顺序

```
1. 推进所有 SubCoreSlot 执行
2. 从 scheduler 派发新 warp 到空闲 slot
3. 推进 scheduler 内部计数器
4. adapter_->tick()
```

### 1.4 Phase 8.A 状态

- Tasks 1-4: 🧊 Frozen (Oracle APPROVED)
- Tasks 5-8: 🚀 **F12a 完成后已解除阻塞，等待实施**

---

## 2. F12b-LD 目标

让用户能用真实 CUDA 程序运行 CppTLM 驱动的仿真：

```bash
LD_PRELOAD=/path/to/libcpptlm_cudart.so ./my_cuda_app
```

CppTLM 负责 timing + NoC/cache/memory 路由，PTX-EMU 负责 PTX 指令级执行 + 寄存器/LDS/CTA 管理。

---

## 3. A-B-A 关键决策

| # | 决策 | 结论 | 说明 |
|---|------|------|------|
| 1 | `cudaMalloc`/`cudaMemcpy` 路由 | **A** → PTX-EMU 内部 | F12b-LD 阶段不通过 CppTLM 路由；H2D/D2H timing 推迟到 Phase 8.B |
| 2 | `libcpptlm_cudart.so` 构建 | **B** → PTX-EMU 构建目标 | PTX-EMU 的 CMake 构建 `libcpptlm_cudart.so`，链接 `cpptlm_core` 静态库；ANTLR4/Java/CUDA 依赖不出 CppTLM CI |
| 3 | GPU 配置 | **A** → CppTLM 生成 JSON | CppTLM 从 `GpuTopology` 生成 JSON，PTX-EMU `GPUContext::load_json_config()` 加载 |

---

## 4. PTX-EMU 侧需要的修改

### 4.1 新增文件

| 文件 | 内容 |
|------|------|
| `include/cudart/cpptlm_bridge.h` (new) | `CppTLMBridge` 抽象接口（见 §5） |
| `include/cudart/cpptlm_bridge_impl.h` (new) | Bridge 默认实现（stub，方便测试） |

### 4.2 修改文件

| 文件 | 修改内容 |
|------|----------|
| `src/cudart/cudart_sim.cpp` | `cudaLaunchKernel`：if bridge → async submit；else → fallback standalone |
| `src/ptxsim/instructions/memory.cpp` | GLOBAL LD/ST：if bridge → route to MemoryBridge via `global_access()` (timing-only)，data 仍在 `SimpleMemory` |

### 4.3 `cudaLaunchKernel` Async 模式

```cpp
// src/cudart/cudart_sim.cpp, cudaLaunchKernel 函数内
if (g_cpptlm_bridge) {
    uint64_t kernel_id = generate_kernel_id();
    uint64_t stream_id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(stream));
    const char* kernel_name = func2name[(uint64_t)func].c_str();
    const void** args_ptr = reinterpret_cast<const void**>(args);
    size_t args_count = count_kernel_args(args);

    int ret = g_cpptlm_bridge->submit_kernel(
        kernel_id, kernel_name,
        gridDim.x, gridDim.y, gridDim.z,
        blockDim.x, blockDim.y, blockDim.z,
        args_ptr, args_count,
        sharedMem,
        stream_id);

    if (ret != 0) return cudaError_t(ret);
    register_pending_kernel(kernel_id, stream_id, func, args, gridDim, blockDim, sharedMem);
    return cudaSuccess;  // 立即返回！
}

// 原有路径 — 当 g_cpptlm_bridge 为 nullptr 时（向后兼容）
g_ptx_interpreter->launchPtxInterpreter(...);
g_gpu_context->wait_for_completion();
return cudaSuccess;
```

> **完整实现**: 参考综合任务书 `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md` §2.1 Task #2。

### 4.4 `HardwareMemoryManager` GLOBAL 拦截

> **注意**: 实际修改点不在 `hardware_memory_manager.cpp`，而在 `src/ptxsim/instructions/memory.cpp` 的 LdHandler/StHandler（参见综合任务书 §2.1 Task #4）。

```cpp
// src/ptxsim/instructions/memory.cpp, LdHandler::processOperation 内
if (g_cpptlm_bridge && is_global_space(device_addr)) {
    uint64_t latency = g_cpptlm_bridge->global_access(device_addr, 0, /*LD=*/0);
    if (latency != UINT64_MAX) {
        // 数据从 SimpleMemory 读取（功能正确性）
        uint64_t value = 0;
        SimpleMemory::read(device_addr, &value);
        thread->write_register(stmt.dest_registers[0], value);
        return latency;  // NoC 路由延迟（用于设置 blocked_cycles）
    }
    // UINT64_MAX = 地址未映射，fallback 到原有路径
}

// 原有 PTX-EMU 内部路径
return LdHandler::processOperation_internal(stmt, thread);
```

**关键语义**：
- `global_access` 为 **timing-only 预计算**：返回的 latency 仅用于设置 `blocked_cycles_remaining`
- 数据立即在 SimpleMemory 中完成读写（Phase 8.B 语义）
- 地址空间判定由 PTX-EMU 端 `is_global_space()` 负责，不再通过 `get_mmap_base()` 做地址转换
```

---

## 5. `CppTLMBridge` 接口定义

> **权威来源**: `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md` §2.1 Task #1

**文件**: `include/cudart/cpptlm_bridge.h`（PTX-EMU 侧新增，零 CppTLM 依赖）

```cpp
#define CPPTLMBRIDGE_VERSION 1

/// PTX-EMU ↔ CppTLM 桥接接口
class CppTLMBridge {
public:
    virtual ~CppTLMBridge() = default;

    /// 返回桥接实现的 ABI 版本（必须等于 CPPTLM_BRIDGE_VERSION）
    virtual int version() const = 0;

    /// 异步提交 kernel launch（立即返回）
    /// @return 0=成功, 非0=cudaError_t 错误码
    virtual int submit_kernel(
        uint64_t kernel_id, const char* kernel_name,
        uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
        uint32_t block_x, uint32_t block_y, uint32_t block_z,
        const void** kernel_args, size_t args_count,
        size_t shared_mem, uint64_t stream_id) = 0;

    /// 轮询 kernel 完成状态。返回 0 表示已完成，>0 表示剩余 cycles，
    /// UINT64_MAX 表示未知 kernel_id
    virtual uint64_t poll_kernel(uint64_t kernel_id) = 0;

    /// 同步等待 stream 上所有 pending kernels 完成
    /// @return 0=成功, 非0=cudaError_t 错误码
    virtual int synchronize_stream(uint64_t stream_id) = 0;

    /// GLOBAL 内存访问 — timing-only 预计算。返回延迟 cycles；UINT64_MAX = 地址未映射
    virtual uint64_t global_access(uint64_t device_addr, uint64_t val,
                                   uint8_t type) = 0;
};

/// 全局 bridge 指针（nullptr = 独立模式，行为字节级兼容）
extern CppTLMBridge* g_cpptlm_bridge;

/// 编译期断言 cudaStream_t 宽度可存入 uint64_t
static_assert(sizeof(cudaStream_t) <= sizeof(uint64_t),
               "cudaStream_t wider than uint64_t — bridge stream_id field must be enlarged");
```

### 5.1 错误语义

| 条件 | 返回值 | 日志 |
|------|--------|------|
| `submit_kernel` 在 bridge 未初始化时调用 | `cudaErrorNotYetInitialized` | ERROR "bridge not initialized" |
| `submit_kernel` 参数无效 | `cudaErrorInvalidValue` | ERROR "invalid kernel params" |
| `global_access` 地址未映射 | `UINT64_MAX` | WARN "address not mapped" |
| `poll_kernel` 未知 kernel_id | `UINT64_MAX` | WARN "unknown kernel_id" |
| Bridge 版本不匹配 | 编译期 `static_assert` | FATAL 中止 |

**地址映射**：由 PTX-EMU 端 `is_global_space()` 判定地址空间（CUDA 虚拟地址 → GLOBAL/LOCAL/SHARED）。`global_access()` 传入的 `device_addr` 为 CUDA device address，CppTLM NoC 路由表直接基于此地址查表，无需 mmap base 转换。

---

## 6. CppTLM 侧新增/修改

| 文件 | 内容 |
|------|------|
| `include/tlm/gpu/memory_bridge.hh` (new) | `MemoryBridge`：实现 `CppTLMBridge`，将 GLOBAL 请求转为 `ComputeReqBundle`，通过 Crossbar→Cache→Memory 路由 |
| `src/tlm/gpu/memory_bridge.cc` (new) | MemoryBridge 实现 |
| `include/tlm/gpu/kernel_launch_tlm.hh` (modify) | 新增 `submit(KernelLaunchRequest&&)` 方法，持有 `ptxsim::GPUContext*` 实例 |
| `cpptlm_config/builder/gpu_config_emitter.py` (new) | JSON 配置生成器：`GpuTopology` → PTX-EMU `GPUConfig` |

---

## 7. Timing Model

- CppTLM `EventQueue` tick 是唯一的时钟源
- CppTLM tick 驱动 PTX-EMU `GPUContext::exe_once()` × 1
- Memory latency 由 CppTLM `MemoryBridge` 返回
- PTX-EMU `InstructionLatencyTable` 负责非访存指令延迟
- PTX-EMU `blocked_cycles_remaining` 接收 CppTLM 返回的延迟值

### 7.1 时序语义细节

1. **Tick 先后顺序**：CppTLM 先完成一个 tick 推进，_然后_ `KernelLaunchTLM::tick()` 调用 `exe_once()`。PTX-EMU `cycle_counter_` 与 CppTLM `cur_cycle` 同步（退化为相对计数器）。
2. **空闲循环熔断**：两次 `global_access` 调用之间，PTX-EMU 可执行多次 `exe_once()`，但 simulation cycle counter **不**推进。
3. **上限防护**：每个 CppTLM tick 最多执行 `max_ptx_steps_per_tick`（默认 10,000）次 `exe_once()`，防止无限循环/死锁。

> 双时序模型对齐的详细风险分析见综合任务书 `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md` §0.3 和 §1.1。

---

## 8. 资源所有权

| 资源 | Owner | 备注 |
|------|-------|------|
| SM count / GPU config | CppTLM → PTX-EMU | CppTLM topology 生成 JSON |
| CTA / Warp 管理 | PTX-EMU | 不变 |
| 共享内存 / 寄存器 | PTX-EMU | 容量由 CppTLM topology 约束 |
| **全局内存 timing** | **CppTLM** | 核心集成价值 |
| 全局内存 data | PTX-EMU `SimpleMemory` | data 留在 PTX-EMU |

---

## 9. 待协商的开放问题

| # | 问题 | 建议方向 |
|---|------|----------|
| 1 | CMake 集成方式 | `find_package(CppTLM)` vs FetchContent vs subdirectory？建议 PTX-EMU 侧决定 |
| 2 | `SimpleMemory` mmap base 如何暴露给 `MemoryBridge` | 已决策：不再暴露 mmap base。由 PTX-EMU 端 `is_global_space()` 判定地址空间，CppTLM NoC 路由表直接基于 CUDA device address 查表。参见综合任务书 §2.2 Task #C1 |
| 3 | ANTLR parser 对用户 CUDA 代码的 fallback | 当 parser 失败时是否回退到 standalone 路径？ |
| 3 | ANTLR parser 对用户 CUDA 代码的 fallback | 当 parser 失败时是否回退到 standalone 路径？ |
| 4 | `cudaMalloc`/`cudaMemcpy` 是否 Phase 8.B 路由到 CppTLM | 当前决定 A（留在 PTX-EMU），未来可改 |
| 5 | `CppTLMBridge` 实现由哪方提供 | CppTLM 提供 `MemoryBridge`，PTX-EMU 提供 mock/stub |

## 10. 阻塞性风险与约束

### 10.1 全局单例约束（P0）

PTX-EMU 当前使用全局单例（`g_gpu_context`、`g_ptx_interpreter`、`CudaDriver::instance()`、`HardwareMemoryManager::instance()`），在 CppTLM 驱动的多实例仿真中会导致**静默状态损坏**。

**F12b-LD 硬性约束**：仅支持单实例、单生命周期仿真。PTX-EMU 侧需添加 `SingletonGuard` 运行时检测（~20 行），检测到单例重复初始化时立即 FATAL 中止。

> 全局单例的完全重构（context objects / 依赖注入）推迟至 F12c/d 或 Phase 8.B。

---

## 11. 验收标准 (F12b-LD)

- [ ] `libcpptlm_cudart.so` 构建成功
- [ ] 参考 CUDA 程序在 `LD_PRELOAD` 下正常运行不崩溃
- [ ] CppTLM counter 观察到 GLOBAL memory access
- [ ] Kernel 输出与 CPU/reference 一致
- [ ] 性能偏差在 standalone PTX-EMU 的 ±10% 内（或有文档说明）
- [ ] 755+ CppTLM tests 全部通过
- [ ] docs_sync 0 missing paths

---

## 12. 下一步

1. **PTX-EMU 团队阅读** §0 列出的 4 个 CppTLM 文件
2. **讨论 §9 的开放问题**，达成共识
3. **PTX-EMU 侧创建** `include/cudart/cpptlm_bridge.h` 接口
4. **CppTLM 侧实现** `MemoryBridge` + `KernelLaunchTLM` 扩展
5. **联调**：参考 CUDA 程序端到端验证

---

## 13. Phase 8.B D1-Full 协作（2026-07-14 追加）

> **关联**: `docs/adr/ADR-NV-02-phase8b-d1-strategy.md`（2026-07-14 Status Update: D1-Lite → D1-Full）

### 13.1 与本协作文档的关系

本协作文档的 §7（Timing Model）描述了 F12b-LD 的 **D2 路径**（MemoryBridge 注入 memory timing）。Phase 8.B D1-Full 扩展此路径，增加 **D1 路径**（SMContext 注入 compute timing）：

```
                    ┌── D1 路径 (Phase 8.B) ──────────────┐
                    │  SMContext::exe_once()                │
                    │  ├── IScoreboard (hazard 检测)        │
                    │  ├── IPipelineLatencyProvider (延迟)  │
                    │  ├── ITensorCoreTiming (TC timing)    │
                    │  └── WarpScheduler (调度) ← 已有      │
CUDA kernel ──→ PTX-EMU ──┤                              ├── Timing
                    │  ┌── D2 路径 (F12b-LD) ──────────┐   │
                    │  │  MemoryBridge                  │   │
                    │  │  ├── GLOBAL load/store         │   │
                    │  │  └── CppTLM NoC routing        │   │
                    │  └────────────────────────────────┘   │
                    └───────────────────────────────────────┘
```

### 13.2 PTX-EMU 侧改造要求

PTX-EMU 团队需在 `SMContext` 中新增 3 个纯虚接口注入点（`IScoreboard` / `IPipelineLatencyProvider` / `ITensorCoreTiming`）。完整改造任务书见：

- **`docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`** — 综合任务书（§2 F12b-LD + §3 D1-Full，~9 天工时）
- **`docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`** — D1-Full 设计细节

### 13.3 与 §7 Timing Model 的协同

| 层面 | F12b-LD (D2) | Phase 8.B (D1-Full) |
|------|:---:|:---:|
| **注入点** | MemoryBridge → GLOBAL 访问 | SMContext → exe_once 内三步 |
| **覆盖指令** | S_LD / S_ST (global memory) | 全部计算指令 + 矩阵指令 |
| **延迟来源** | CppTLM NoC + L2 查表 | PipelineTLM / TensorCoreTLM |
| **blocked_cycles** | 已有（仅 S_LD） | 扩展至全部指令（需 `set_blocked_cycles_for_active()`） |
| **PTX-EMU 修改** | 仅 Bridge 头文件 | SMContext 3 注入点 + 2 新 API |

### 13.4 双路径协作

D1 (compute timing) 和 D2 (memory timing) 在 `exe_once()` 内协作：
- D1 Pipeline 返回**分数 cycle 延迟**（替代 `InstructionLatencyTable`）
- D2 MemoryBridge 对 GLOBAL access 返回**CppTLM NoC 路由延迟**
- 两者通过 `blocked_cycles_remaining` 统一注入，PTX-EMU `decrement_blocked_cycles()` 统一递减

---

*本文档由 CppTLM Team 生成，用于协作同步。如有疑问请联系 CppTLM Team。*