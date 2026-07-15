# PTX-EMU 改造综合任务书：F12b-LD MemoryBridge + D1-Full Compute 全栈协同

> **文档类型**: PTX-EMU 团队综合改造任务书
> **日期**: 2026-07-14
> **作者**: CppTLM Team
> **Supersedes**: `docs/superpowers/specs/2026-07-03-ptxemu-modification-task.md`（仅 D1-Full Compute 部分，已作为本文 §3 子集纳入）
> **关联**:
> - `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`（D1-Full 设计）
> - `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`（F12b-LD 协作规范）
> - `docs/superpowers/specs/PTX-EMU-README.md`（PTX-EMU 团队入口文档）
> - `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`（协作同步 spec）

---

## 0. 核心架构愿景：CppTLM 作为时钟真相源

### 0.1 当前现实（2026-07-14）

```
PTX-EMU 完全自驱动:
  cudaLaunchKernel() ─→ wait_for_completion()
                       └─→ exe_once() × N (循环)
                           └─→ SMContext::exe_once()
                               ├─→ cycle_counter_++   ← PTX-EMU 自有时钟
                               ├─→ blocked_cycles 递减 (PTX-EMU InstructionLatencyTable)
                               ├─→ 指令执行 (PTX-EMU SimpleMemory)
                               └─→ 无 CppTLM 任何参与
```

**当前 PTX-EMU 是 clock-of-truth，CppTLM 与 PTX-EMU 之间零集成代码**。

### 0.2 目标愿景

```
CppTLM 是唯一时钟真相源（主动驱动方）:
  CppTLM EventQueue 主循环 ─→ 推进 cur_cycle
       │
       │  ─★─【控制流方向】CppTLM 主动调用，PTX-EMU 被动响应 ─★─
       │
       ├─→ 每 tick 调用 PTX-EMU::exe_once() × 1   (KernelLaunchTLM::tick 触发)
       │       │
       │       └─→ SMContext::exe_once()
       │           ├─→ cycle_counter_ 与 cur_cycle 同步 (退化为相对计数器)
       │           ├─→ 计算指令延迟查询:
       │           │       pipeline_provider_->get_fractional_cycles() ★ CppTLM 提供
       │           │       tensor_core_timing_->get_latency()         ★ CppTLM 提供
       │           ├─→ 全局访存延迟查询:
       │           │       MemoryBridge::global_access()             ★ CppTLM 提供 (timing-only)
       │           ├─→ Scoreboard hazard 检测:
       │           │       scoreboard_->has_free_entry() / allocate() / release()
       │           └─→ 指令功能执行 (PTX-EMU SimpleMemory 提供正确性)
       │
       └─→ CppTLM 内部 NoC 模块 tick:
              ├─→ CrossbarTLM.tick()
              ├─→ CacheTLM.tick()
              └─→ MemoryTLM.tick()

  ★ 注意: PTX-EMU 不再调用 CppTLM 反向 tick
           cudaDeviceSynchronize/cudaStreamSynchronize 通过 host-side 事件循环
           触发 CppTLM EventQueue::run_one_tick()，而非通过 bridge->tick()
```

### 0.3 职责分工

| 维度 | PTX-EMU 职责 | CppTLM 职责 |
|------|-------------|------------|
| **时钟推进** | 退化为被动响应 | 唯一主动推进方 |
| **指令功能正确性** | ✓ 提供（PTX-EMU SimpleMemory） | — |
| **指令周期精确性** | — | ✓ 提供（PipelineTLM / TensorCoreTLM） |
| **全局访存延迟** | — | ✓ 提供（MemoryBridge → NoC 路由） |
| **数据所有权** | ✓ 数据存于 SimpleMemory | — |
| **Scoreboard hazard** | — | ✓ 提供（ScoreboardTLM） |
| **TMA async 完成** | 当前阻塞，未来 callback | ✓ Phase 9+ 通过 `IAsyncCompletion` 注入 |

---

## 1. 路径调整说明

### 1.1 原实施路径（错误）

```
旧: D1-Full Compute → F12b-LD MemoryBridge
   问题: Compute timing 注入依赖 MemoryBridge 的 cycle 同步机制
```

### 1.2 正确实施路径

```
新: F12b-LD MemoryBridge → D1-Full Compute → Phase 9+ Async Seams
   原因: MemoryBridge 是 clock-of-truth 基础设施，必须先于 Compute timing 注入
```

### 1.3 阶段划分

| 阶段 | 名称 | 周期 | 阻塞依赖 |
|:---:|------|:---:|:---:|
| **§2 (P0)** | F12b-LD MemoryBridge — clock-of-truth 基础设施（含 stream_id + cudaStreamSynchronize） | 5.5 天 | 无 |
| **§3 (P1)** | D1-Full Compute 注入 — 计算指令 timing | 2.5 天 | §2 接口定义可并行；§2 tick 模型锁定后才能完整测试 #9 |
| **§4 (P2)** | Phase 9+ Async Seams — 未来兼容性预留 | 1 小时 | §3 stub |
| **§5 (P3)** | 集成验证 — E2E CUDA kernel 验证 | 1 周 | §2 + §3 |

---

## §2 (P0 阶段): F12b-LD MemoryBridge 改造

> **目标**: CppTLM 成为唯一时钟真相源，PTX-EMU 退化为被动执行器
> **工期**: 5 天（PTX-EMU 端 3 天 + CppTLM 端 2 天）
> **优先级**: 🔴 最高 — 阻塞所有后续阶段

### §2.0 F12b-LD 前置条件确认（来自原 F12+PTX-EMU Integration Plan §11）

| # | 决策项 | 状态 | 证据/备注 |
|---|--------|:----:|----------|
| 1 | F12a 完成 + Phase 8.A 集成测试稳定 | ✅ 已满足 | ADR-NV-02 §1.2；NV-01 Status Update 2026-07-03（`e8280fe`） |
| 2 | PTX-EMU 集成 OpenSpec change 审批 | 🗑️ 已 superseded | 原拟创建 `2026-06-xx-ptxemu-integration-strategy`，未执行。本文档替代其角色 |
| 3 | PTX-EMU 版本/commit + ANTLR4 构建依赖锁定 | ❌ 待确认 | 需在 Task #5 CMake 片段中追加 `CPPTLM_PTXEMU_VERSION` 约束 |
| 4 | PTX-EMU 团队确认接口变更并承诺版本稳定 | ✅ 已满足 | ADR-NV-02 §5 R4 风险消除；Status Update 2026-07-14 |
| 5 | `libcpptlm_cudart.so` CMake 集成方式经编译验证 | ⚠️ 已设计未验证 | Task #5 已有 cmake 草案，0.5 day 实施时完成验证 |
| 6 | 参考 CUDA 程序 + 预期输出 + 正确性验证方法确定 | ✅ 已满足 | §5.1 5 类场景 + gpgpu-sim baseline + `test_gpgpu_sim_comparison.py` |
| 7 | 合成内存流量烟雾测试（先于完整 CUDA kernel 测试） | ❌ 待确认 | 尚需双方团队协商：是否增设 G-F0 验收标准，或明确取消此要求 |
| 8 | 性能验证方案（vs standalone PTX-EMU baseline） | ✅ 已满足 | §5 ±15% + openspec G5 ±10% 双轨对照 |
| 9 | PTX-EMU 全局单例处理策略确定并验证 | ⚠️ 已决策未编码 | 协作规约 §10.1 方案已设计；`SingletonGuard` 实现归入 Task #3/#5 |
| 10 | Top 3 hidden risks 缓解方案文档化并审查 | ❌ 待澄清 | 需在 ADR-NV-02 §5 补全 R7（静默数据损坏）和 R8（性能有效性幻觉）。参见原 Integration Plan §10 Risk #7/#8 |

### §2.1 PTX-EMU 端改造任务

#### Task #1: `CppTLMBridge` 接口定义（新增）

**文件**: `include/cudart/cpptlm_bridge.h`（PTX-EMU 侧零外部依赖）

```cpp
#ifndef PTX_CPPTLM_BRIDGE_H
#define PTX_CPPTLM_BRIDGE_H

#include <cstdint>

/// Bridge ABI 版本号 — 编译期断言双方实现版本一致
/// 每次接口签名变更必须同步递增此值
#define CPPTLMBRIDGE_VERSION 1

/// PTX-EMU ↔ CppTLM 桥接接口
/// PTX-EMU 仅持原始指针，所有权归 libcpptlm_cudart.so
class CppTLMBridge {
public:
    virtual ~CppTLMBridge() = default;

    /// 返回桥接实现的 ABI 版本（必须等于 CPPTLMBRIDGE_VERSION）
    virtual int version() const = 0;

    /// 提交一个 kernel（异步！立即返回）
    /// @param kernel_id PTX-EMU 生成的唯一 ID（用于后续 poll_kernel 查询）
    /// @param kernel_name PTX 函数名（如 "myKernel"），以 \0 结尾
    /// @param grid_x/y/z grid 维度
    /// @param block_x/y/z block 维度
    /// @param kernel_args 指向 kernel 参数数组的指针（host 端已对齐）
    /// @param args_count kernel 参数个数（用于 deep-copy）
    /// @param shared_mem 动态共享内存字节数
    /// @param stream_id stream 句柄（0 = 默认 stream）
    /// @return 0=成功, 非0=cudaError_t 错误码
    /// @note kernel_args 的内存所有权归 PTX-EMU；CppTLM 必须在 submit 返回前 deep-copy
    virtual int submit_kernel(
        uint64_t kernel_id,
        const char* kernel_name,
        uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
        uint32_t block_x, uint32_t block_y, uint32_t block_z,
        const void** kernel_args, size_t args_count,
        size_t shared_mem,
        uint64_t stream_id) = 0;

    /// 轮询 kernel 完成状态
    /// @return 0=已完成, >0=剩余 cycle 数, UINT64_MAX=未知 kernel_id
    virtual uint64_t poll_kernel(uint64_t kernel_id) = 0;

    /// 同步等待 stream 上所有 pending kernels 完成
    /// @param stream_id stream 句柄（0 = 默认 stream）
    /// @return 0=成功, 非0=cudaError_t 错误码
    virtual int synchronize_stream(uint64_t stream_id) = 0;

    /// 全局内存访问 — 同步返回 NoC 路由延迟（cycle 数）
    /// @param device_addr GLOBAL 空间虚拟地址
    /// @param val 写入值（ST 指令）或 0（LD 指令）
    /// @param type 0=LD, 1=ST
    /// @return 延迟 cycle 数；UINT64_MAX = 地址未映射（fallback 到 PTX-EMU 内部）
    /// @note Phase 8.B 语义：timing-only。数据在 PTX-EMU SimpleMemory 中立即可用。
    ///       CppTLM NoC 模块在 KernelLaunchTLM::tick() 中独立推进。
    ///       Phase 9+ TMA async 通过独立的 IAsyncCompletion 路径，不走 global_access。
    virtual uint64_t global_access(uint64_t device_addr, uint64_t val, uint8_t type) = 0;
};

/// 全局 bridge 指针（PTX-EMU 持有）
/// nullptr = 独立模式（PTX-EMU 自驱，行为字节级兼容）
extern CppTLMBridge* g_cpptlm_bridge;

/// 编译期断言 cudaStream_t 宽度可存入 uint64_t
#include <cuda_runtime.h>
static_assert(sizeof(cudaStream_t) <= sizeof(uint64_t),
              "cudaStream_t wider than uint64_t — bridge stream_id field must be enlarged");

#endif
```

**设计说明**:
- `g_cpptlm_bridge` 默认为 `nullptr`，保证现有测试零退化
- 接口在 PTX-EMU 侧零外部依赖（不 include CppTLM 任何头文件）
- CppTLM 侧提供 `CppTLMBridge` 实现（通过 libcpptlm_cudart.so 注入）
- **`CPPTLMBRIDGE_VERSION`**：编译期断言双方 ABI 版本一致（每次签名变更需同步递增）
- **`kernel_args` deep-copy 策略**：调用方（PTX-EMU）在 `submit_kernel` 返回前保证 args 内存有效；CppTLM 必须在调用栈内 deep-copy 后才能返回——避免 host 内存生命周期问题
- **`stream_id` 字段**：即使 Phase 8.B `KernelLaunchTLM` 仍 FIFO 处理（不实施 stream 优先级），字段已预埋以避免 Phase 9+ 数据结构重构
- **`synchronize_stream`**：与 `poll_kernel` 分离，提供 stream 级别的同步原语；`cudaStreamSynchronize` 直接转发到此接口，避免单 stream 设计被多流程序误用
- **不再包含 `tick()`**：CppTLM EventQueue 主动调用 `KernelLaunchTLM::tick()` 推进仿真，PTX-EMU 不应反向驱动 CppTLM 时钟。`tick()` 由 CppTLM 内部实现，不再暴露在桥接接口上
- **`global_access` timing-only 语义**：返回的 latency 仅用于设置 `blocked_cycles`；数据在 PTX-EMU `SimpleMemory` 中立即可用。`query_latency` 在 CppTLM 侧为**预计算**（基于地址路由表查表），不阻塞等待 NoC 实际推进
- **`cudaStream_t` → `uint64_t` 宽度断言**：`static_assert` 防止未来 cudaStream_t 宽度变化导致隐式截断

---

#### Task #2: 异步 `cudaLaunchKernel`（修改 `cudart_sim.cpp`）

**文件**: `src/cudart/cudart_sim.cpp` 第 332-386 行

**当前代码（阻塞）**:
```cpp
cudaError_t cudaLaunchKernel(...) {
    g_ptx_interpreter->launchPtxInterpreter(...);
    g_gpu_context->wait_for_completion();  // ← 阻塞！
    return cudaSuccess;
}
```

**改造后（异步优先，bridge 缺失时回退）**:
```cpp
cudaError_t cudaLaunchKernel(const void *func, dim3 gridDim, dim3 blockDim,
                             void **args, size_t sharedMem,
                             cudaStream_t stream) {
    // ★ NEW: 如果有 CppTLM bridge，走异步路径
    if (g_cpptlm_bridge) {
        uint64_t kernel_id = generate_kernel_id();
        uint64_t stream_id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(stream));

        // ★ 同步 deep-copy 关键参数 (kernel_name 在 func2name 表中生命周期足够长，无需拷贝)
        const char* kernel_name = func2name[(uint64_t)func].c_str();
        const void** args_ptr = reinterpret_cast<const void**>(args);
        size_t args_count = count_kernel_args(args);  // 由 PTX-EMU 提供的 helper

        int ret = g_cpptlm_bridge->submit_kernel(
            kernel_id, kernel_name,
            gridDim.x, gridDim.y, gridDim.z,
            blockDim.x, blockDim.y, blockDim.z,
            args_ptr, args_count,
            sharedMem,
            stream_id);

        if (ret != 0) return cudaError_t(ret);  // 错误码直接转发

        // ★ 注册 pending kernel（包含 stream_id 用于后续 stream-sync 过滤）
        register_pending_kernel(kernel_id, stream_id, func, args, gridDim, blockDim, sharedMem);
        return cudaSuccess;  // 立即返回！
    }

    // ★ 原有路径 — 当 g_cpptlm_bridge 为 nullptr 时（向后兼容）
    g_ptx_interpreter->launchPtxInterpreter(...);
    g_gpu_context->wait_for_completion();
    return cudaSuccess;
}
```

**新增 helper 函数和数据结构**:
```cpp
// cudart_sim.cpp 顶部
static std::atomic<uint64_t> next_kernel_id{1};
uint64_t generate_kernel_id() { return next_kernel_id.fetch_add(1); }

// ★ PendingKernel 增加 stream_id 字段 — Phase 8.B 即预埋以避免 Phase 9+ 数据结构重构
struct PendingKernel {
    uint64_t kernel_id;
    uint64_t stream_id;       // ★ NEW — 0=默认 stream
    const void* func;
    dim3 grid_dim;
    dim3 block_dim;
    size_t shared_mem;
    // args 由 CppTLM 端在 submit_kernel 调用栈内已 deep-copy，此处不持有
};

// ★ F12b-LD 阶段：单线程假设；Phase 9+ 添加 mutex 保护
std::unordered_map<uint64_t, PendingKernel> g_pending_kernels;

void register_pending_kernel(uint64_t id, uint64_t stream_id,
                             const void* func, void** args,
                             dim3 grid, dim3 block, size_t shared_mem) {
    g_pending_kernels[id] = PendingKernel{id, stream_id, func, grid, block, shared_mem};
}
```

**关键约束**:
- `kernel_args` 的内存所有权归 PTX-EMU（host 端）；CppTLM 必须在 `submit_kernel` 调用栈内 deep-copy 参数（CppTLM 端实现保证，见 #C2）
- `kernel_name` 取自 `func2name[]` 表（PTX-EMU 内部长期存储），无需拷贝
- `args_count` 由 PTX-EMU 端根据 PTX 元数据提供（CppTLM 不需要解析 PTX）

---

#### Task #3: stream/device 同步原语（修改 `cudart_sim.cpp`）

**文件**: `src/cudart/cudart_sim.cpp`（新增 2 个函数）

```cpp
// ★ 单 stream 同步 — 轮询该 stream 上的所有 pending kernels
cudaError_t cudaStreamSynchronize(cudaStream_t stream) {
    if (!g_cpptlm_bridge) {
        // 原有 PTX-EMU 路径：当前实现立即返回 cudaSuccess
        return cudaSuccess;
    }

    uint64_t target_stream = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(stream));

    while (true) {
        bool stream_empty = true;
        // ★ 修复迭代器失效 bug — 先收集待删除 id，再统一删除
        std::vector<uint64_t> completed_ids;

        for (const auto& [id, info] : g_pending_kernels) {
            if (info.stream_id != target_stream) continue;  // ★ stream 过滤
            uint64_t remaining = g_cpptlm_bridge->poll_kernel(id);
            if (remaining == 0) {
                completed_ids.push_back(id);  // 先标记，不直接 erase
            } else if (remaining != UINT64_MAX) {
                stream_empty = false;
            }
        }

        // 统一删除已完成的 kernel
        for (uint64_t id : completed_ids) {
            g_pending_kernels.erase(id);
        }

        if (stream_empty) break;

        // ★ 由 CppTLM 主动推进仿真时钟 — 等待 PTX-EMU 外部事件循环触发
        // （此函数通常在 host 端 sleep 或 yield 后由 host 端循环重新调用）
    }
    return cudaSuccess;
}

// ★ 设备级同步 — 同步所有 stream
cudaError_t cudaDeviceSynchronize() {
    if (!g_cpptlm_bridge) {
        g_gpu_context->wait_for_completion();
        return cudaSuccess;
    }

    // 同步所有 stream — 简单遍历已知 stream ids
    // Phase 8.B: 默认 stream (0) + 用户创建的 stream
    for (uint64_t stream_id : g_active_streams) {
        cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(stream_id));
    }
    return cudaSuccess;
}

// ★ 跟踪活跃 stream（PTX-EMU 端维护）
std::unordered_set<uint64_t> g_active_streams{0};  // 默认 stream 始终存在
cudaError_t cudaStreamCreate(cudaStream_t* pStream) {
    uint64_t id = next_kernel_id.fetch_add(1);
    g_active_streams.insert(id);
    *pStream = reinterpret_cast<cudaStream_t>(static_cast<uintptr_t>(id));
    return cudaSuccess;
}
```

**关键设计决策**:
- **无 `bridge->tick()` 调用**：cppTLM EventQueue 是**主动**时钟推进方，不接受反向驱动；`cudaStreamSynchronize` 通过 host 端事件循环（外部）触发 CppTLM 推进
- **迭代器失效修复**：先收集待删除 id → 再统一 erase（替代原版 range-for 中直接 erase）
- **stream_id 过滤**：`cudaStreamSynchronize` 只轮询匹配 stream 的 kernels，不影响其他 stream 的并发执行
- **`cudaStreamCreate` 句柄编码**：使用 `next_kernel_id` 生成 64-bit 唯一 ID（与 stream_id 空间共享足够大，不会冲突）
- **Phase 8.B 单线程约束**：`g_pending_kernels` 和 `g_active_streams` 未加锁；F12b-LD 文档 §10.1 明确 F12b-LD 阶段假设 host 端单线程调用 CUDA API

---

#### Task #4: `hardware_memory_manager` GLOBAL 空间桥接（修改）

**文件**: `src/ptxsim/memory/hardware_memory_manager.cpp`

**当前流程（PTX-EMU 内部 SimpleMemory）**:
```
GLOBAL LD/ST → hardware_memory_manager::access()
              → SimpleMemory::read/write()
              → 返回 cycle 延迟（查表）
```

**改造后（桥接到 CppTLM NoC）**:
```cpp
// 修改点不在 HardwareMemoryManager::access（它返回 void），
// 而在 src/ptxsim/instructions/memory.cpp 的 LdHandler::processOperation() 和 StHandler::processOperation()
//
// LD 指令 handler 改造示意:
uint64_t LdHandler::processOperation(StatementContext& stmt, ThreadContext* thread) {
    uint64_t device_addr = compute_effective_address(stmt, thread);
    
    // ★ NEW: 如果有 bridge 且为 GLOBAL 空间，走 CppTLM NoC
    if (g_cpptlm_bridge && is_global_space(device_addr)) {
        uint64_t latency = g_cpptlm_bridge->global_access(device_addr, 0, /*LD=*/0);
        if (latency != UINT64_MAX) {
            // 数据从 SimpleMemory 读取（功能正确性）
            uint64_t value = 0;
            SimpleMemory::read(device_addr, &value);  // Phase 8.B: bypass cache
            thread->write_register(stmt.dest_registers[0], value);
            return latency;  // 返回 NoC 路由延迟（用于设置 blocked_cycles）
        }
        // UINT64_MAX = 地址未映射，fallback 到原有路径
    }
    
    // 原有 PTX-EMU 内部路径
    return LdHandler::processOperation_internal(stmt, thread);
}

// ST 指令 handler 类似 — write 路径
```

**关键语义说明**:
- **`global_access` timing-only**：返回的 latency **仅用于设置 `blocked_cycles_remaining`**；数据读写立即完成（Phase 8.B），无需等待 NoC 实际完成
- **Phase 8.B cache bypass**：直接读写 SimpleMemory，不经过 CppTLM CacheTLM——这是 timing-only 模型的内在限制
- **Phase 9+ 演进路径**：当 `IAsyncCompletion` 真实实现时，LD/ST handler 改为：写入 NoC 请求 → 返回 transaction_id → 不阻塞，立即让 warp 继续 → 通过 `IAsyncCompletion` 回调在后续 tick 写入目标寄存器。这正是 §4 预留 seam 的用途
- **地址映射**：`is_global_space()` 由 PTX-EMU 现有机制判定（CUDA 虚拟地址空间 → GLOBAL/LOCAL/SHARED）

---

#### Task #5: `libcpptlm_cudart.so` 集成构建（修改 CMakeLists.txt）

**文件**: `CMakeLists.txt`

**PTX-EMU 端**:
- 当 `cpptlm_FOUND` 时，链接 `cpptlm_core`
- 当 `cpptlm_FOUND` 且 `BUILD_LIB_CPPTLM_CUDART=ON`，构建 `libcpptlm_cudart.so`
- 默认 `g_cpptlm_bridge = nullptr`（保证现有测试零退化）

```cmake
# CMakeLists.txt 末尾追加
option(BUILD_LIB_CPPTLM_CUDART "Build libcpptlm_cudart.so bridge" OFF)
find_package(cpptlm QUIET)

if(cpptlm_FOUND AND BUILD_LIB_CPPTLM_CUDART)
    add_subdirectory(src/cudart/cpptlm_bridge)
    target_link_libraries(ptxemu_runtime PRIVATE cpptlm::core)
endif()
```

> **PTX-EMU 版本 pin**: commit `<待 PTX-EMU 团队提供>` — 需锁定包含 Task #1 `CppTLMBridge` 完整接口的 commit
> **ANTLR4 版本**: `>= 4.13.2`（PTX-EMU CI 使用，不进入 CppTLM CI；仅链接 `libcpptlm_cudart.so` 时需 ABI 匹配）
> **`libcpptlm_cudart.so` 发现方式**: 待 PTX-EMU 团队确认（候选选项：`ExternalProject_Add` pin 到 commit / `find_library` + 环境变量 `CPPTLM_PTXEMU_LIBDIR` / `pkg-config`）
> **需 PTX-EMU 团队行动**:
>   1. 提供包含 Task #1（`CppTLMBridge` 接口）的 PTX-EMU commit hash
>   2. 确认 ANTLR4 runtime 版本号
>   3. 确定 `libcpptlm_cudart.so` 的 CMake 暴露方式

---

### §2.2 CppTLM 端配套任务

#### Task #C1: `MemoryBridge` 实现（新增）

**文件**: `include/tlm/gpu/memory_bridge.hh` + `src/tlm/gpu/memory_bridge.cc`

```cpp
// MemoryBridge — 实现 PTX-EMU 的 CppTLMBridge 接口
class MemoryBridge : public CppTLMBridge {
public:
    // 构造时接收 NoC 拓扑入口
    MemoryBridge(KernelLaunchTLM* kernel_launch,
                 CrossbarTLM* gpu_xbar,
                 MemoryController* gpu_memory);

    int version() const override { return CPPTLMBRIDGE_VERSION; }

    int submit_kernel(uint64_t kernel_id,
                      const char* kernel_name,
                      uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                      uint32_t block_x, uint32_t block_y, uint32_t block_z,
                      const void** kernel_args, size_t args_count,
                      size_t shared_mem,
                      uint64_t stream_id) override {
        // ★ Deep-copy kernel 参数 — 防止 PTX-EMU host 端 args 内存失效
        //    CppTLM 内部维护一份 std::vector<void*>，每个元素 deep-copy
        std::vector<std::vector<uint8_t>> copied_args;
        copied_args.reserve(args_count);
        for (size_t i = 0; i < args_count; ++i) {
            const void* arg_ptr = kernel_args[i];
            // 从 PTX 元数据获取每个参数的大小（PTX-EMU 提供或 CppTLM 维护 PTX 类型表）
            size_t arg_size = get_ptx_arg_size(kernel_name, i);
            copied_args.emplace_back(static_cast<const uint8_t*>(arg_ptr),
                                     static_cast<const uint8_t*>(arg_ptr) + arg_size);
        }

        return kernel_launch_->enqueue(KernelLaunchRequest{
            .kernel_id = kernel_id,
            .kernel_name = kernel_name,
            .grid = {grid_x, grid_y, grid_z},
            .block = {block_x, block_y, block_z},
            .args = std::move(copied_args),
            .shared_mem = shared_mem,
            .stream_id = stream_id
        });
    }

    uint64_t poll_kernel(uint64_t kernel_id) override {
        return kernel_launch_->poll_completion(kernel_id);
    }

    int synchronize_stream(uint64_t stream_id) override {
        return kernel_launch_->wait_stream(stream_id);
    }

    uint64_t global_access(uint64_t device_addr, uint64_t val, uint8_t type) override {
        // ★ Phase 8.B 语义：timing-only 预计算
        //    - 不实际驱动 NoC 路由（NoC 在 KernelLaunchTLM::tick() 中独立推进）
        //    - query_latency 基于 CUDA device address 路由表查表，返回预计算延迟
        //    - 地址映射假设：PTX-EMU 传 CUDA device address（已在 is_global_space() 空间判定后）
        //    - 数据读写立即在 PTX-EMU SimpleMemory 中完成（bypass CppTLM cache）
        return gpu_xbar_->query_latency(device_addr);  // 预计算路由延迟
    }

private:
    size_t get_ptx_arg_size(const char* kernel_name, size_t arg_index);  // 查 PTX 类型表
};
```

#### Task #C2: `KernelLaunchTLM` EventQueue 集成（修改）

**文件**: `src/tlm/gpu/kernel_launch_tlm.cc`

```cpp
// KernelLaunchTLM 持有 GPUContext* 和 pending kernel 队列
class KernelLaunchTLM : public ChStreamModuleBase {
public:
    // 持有 PTX-EMU 的 GPUContext 指针（CppTLM 是 clock-of-truth）
    void set_ptx_emu_gpu_context(ptxsim::GPUContext* ctx) { gpu_context_ = ctx; }

    // 每个 tick — 由 CppTLM EventQueue 主动调用
    void tick() override {
        // 1. 启动新 kernel（如果 SM 空闲）
        if (gpu_context_->all_sm_idle() && !pending_kernels_.empty()) {
            auto& next = pending_kernels_.front();
            // Phase 8.B: FIFO 调度（忽略 stream 优先级；Phase 9+ 改为 stream 顺序）
            gpu_context_->launch_kernel(next.kernel_name, next.grid, next.block,
                                       next.args.data(), next.shared_mem);
            pending_kernels_.pop();
        }

        // 2. ★ PTX-EMU 推进 1 个 cycle（由 CppTLM 驱动！）
        gpu_context_->exe_once();

        // 3. CppTLM NoC 模块推进
        crossbar_->tick();
        cache_->tick();
        memory_->tick();

        cur_cycle_++;
    }

    int enqueue(KernelLaunchRequest req) {
        pending_kernels_.push(std::move(req));
        return 0;
    }

    uint64_t poll_completion(uint64_t kernel_id) {
        // Phase 8.B: 检查该 kernel 是否还在 pending 队列中
        //   - 在队列中: 返回预估剩余 cycle 数
        //   - 不在队列中: 返回 0（已完成）
        // Phase 9+: 实现基于 IAsyncCompletion 的真实完成检测
        return kernel_launch_state_.count(kernel_id) ? kernel_launch_state_[kernel_id] : 0;
    }

    int wait_stream(uint64_t stream_id) {
        // Phase 8.B: 简单阻塞 — 持续调用 gpu_context_->exe_once() 直到该 stream 队列为空
        // 由 cudaStreamSynchronize 的 host-side loop 触发，不在此函数内 tick
        return 0;
    }

private:
    ptxsim::GPUContext* gpu_context_ = nullptr;  // ★ 持有 PTX-EMU 对象
    std::queue<KernelLaunchRequest> pending_kernels_;
    std::unordered_map<uint64_t, uint64_t> kernel_launch_state_;
};
```

---

### §2.3 §2 阶段验收标准

| 编号 | 标准 | 验证 |
|:---:|------|------|
| **G-F1** | `g_cpptlm_bridge == nullptr` 时，PTX-EMU 原有测试零退化（字节级兼容） | `ptxemu_tests` 全 pass |
| **G-F2** | 有 bridge 时，`cudaLaunchKernel` 立即返回（异步），不阻塞 | 单测：测量 `cudaLaunchKernel` 耗时 < 100μs |
| **G-F3** | `global_access()` 返回的延迟值与 CppTLM NoC 路由延迟一致（误差 ≤ 5%） | 集成测试：注入已知拓扑，对比延迟 |
| **G-F4** | `cudaDeviceSynchronize` 正确等待所有 kernel 完成 | 单测：发起 5 个 kernel，等待，全部完成 |
| **G-F5** | F12b-LD 集成测试：1 个 GEMM kernel 经 CppTLM NoC 路由完成，结果与 standalone PTX-EMU 一致 | `cpptlm_tests [gpu][f12b]` |

---

## §3 (P1 阶段): D1-Full Compute 注入

> **目标**: 在 §2 clock-of-truth 基础上，注入 Scoreboard/Pipeline/TensorCore 计算指令 timing
> **工期**: 2.5 天（PTX-EMU 端 1.5 天 + CppTLM 端 1 天）
> **依赖**: §2 完成（可并行启动 #1~#4 接口定义）

### §3.1 PTX-EMU 端改造任务（5b/5c 任务）

> **重要**: 本节任务为原 `2026-07-03-ptxemu-modification-task.md` 的精炼版（该文档已于 2026-07-14 删除，吸收为本综合计划 §3）。原任务编号 #0~#6 与本节 Task #6~#11 一一对应。

#### Task #6: 3 个纯虚接口头文件（新增）

> **对应原任务 #1~#3** — 接口定义零 CppTLM 依赖

**文件**: `include/ptxsim/scoreboard_interface.h`、`pipeline_interface.h`、`tensor_core_interface.h`

完整代码见 `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`（D1-Full 设计文档）§2.1~§2.3。**总行数 ~120 行，15 分钟/文件**。

---

#### Task #7: SMContext 头文件修改

> **对应原任务 #4**

**文件**: `include/ptxsim/sm_context.h`

在 `SMContext` 类内追加：

```cpp
// ★ Phase 9+ 预留: IAsyncCompletion 注入点（Phase 8.B 阶段 stub）
void set_async_completion(IAsyncCompletion* async) {
    async_completion_ = async;
}
IAsyncCompletion* get_async_completion() const { return async_completion_; }

private:
    IAsyncCompletion* async_completion_ = nullptr;
```

**配套新增 `include/ptxsim/async_completion_interface.h`**:
```cpp
class IAsyncCompletion {
public:
    virtual ~IAsyncCompletion() = default;
    virtual void register_completion_callback(
        uint64_t transaction_id,
        std::function<void()> callback) = 0;
    virtual bool is_completed(uint64_t transaction_id) const = 0;
    virtual void tick() = 0;
};
```

**Phase 8.B 阶段**: IAsyncCompletion 接口存在但 PTX-EMU 内部不调用；CppTLM 侧 Adapter 不实现。Phase 9+ TMA async 填充。

---

#### Task #8: `WarpContext::set_blocked_cycles_for_active()` 新增

> **对应原任务 #5a**

**文件**: `include/ptxsim/warp_context.h` + `src/ptxsim/core/warp_context.cpp`

```cpp
// warp_context.h 公开方法
void set_blocked_cycles_for_active(uint32_t cycles) {
    for (int lane = 0; lane < WARP_SIZE; ++lane) {
        if (is_lane_active(lane)) {
            auto* t = get_thread(lane);
            if (t) {
                t->get_state().blocked_cycles_remaining = cycles;
                t->get_state().is_blocked = (cycles > 0);
            }
        }
    }
}
```

---

#### Task #9: `SMContext::exe_once()` 三段式注入

> **对应原任务 #5b** — 在 §2 clock-of-truth 基础上修改

**文件**: `src/ptxsim/core/sm_context.cpp`

**关键变化**: 在 §2 的 CppTLM 驱动机制下，`exe_once()` 退化为被动响应。每个 cycle 由 CppTLM EventQueue 调用一次。

完整代码见 `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`（D1-Full 设计文档）§3.5，**核心结构不变**：

```
exe_once() {
    cycle_counter_++;  // 与 CppTLM cur_cycle 同步（§2 已建立）
    decrement_blocked_cycles_for_all_warps();  // 现有逻辑
    
    if (warp = warp_scheduler_->schedule_next()) {
        stmt = get_next_statement(warp);
        
        // === NEW Step A: Scoreboard 检查 ===
        if (scoreboard_) {
            scoreboard_->tick();
            if (!scoreboard_->has_free_entry()) goto skip_warp_execution;
            for (auto reg_id : get_dest_registers(*stmt)) {
                if (!scoreboard_->allocate(reg_id, warp->get_physical_warp_id())) {
                    goto skip_warp_execution;  // RAW hazard
                }
            }
        }
        
        // === NEW Step B: 延迟查询 (priority: Pipeline > TC > InstructionLatencyTable) ===
        uint32_t latency = 0;
        if (pipeline_provider_) {
            double frac = pipeline_provider_->get_fractional_cycles_by_type(
                static_cast<int>(stmt->type), map_instruction_to_pipeline(*stmt));
            if (frac > 0.0) latency = static_cast<uint32_t>(std::ceil(frac));
        }
        if (latency == 0 && tensor_core_timing_ && is_tensor_core_instruction(*stmt)) {
            latency = tensor_core_timing_->get_latency(map_instruction_to_tc_precision(*stmt));
        }
        if (latency == 0) {
            latency = InstructionLatencyTable::instance().get(stmt->type).cycles;
        }
        if (latency > 0) warp->set_blocked_cycles_for_active(latency);
        
        // === 原有: 指令执行 ===
        warp->execute_warp_instruction(*stmt, pc);
        
        // === NEW Step C: Scoreboard 释放 ===
        if (scoreboard_) {
            for (auto reg_id : get_dest_registers(*stmt)) {
                scoreboard_->release(reg_id, warp->get_physical_warp_id());
            }
        }
        
        // === NEW Step D (Phase 9+ 预留): Async completion tick ===
        if (async_completion_) {
            async_completion_->tick();
        }
        
        check_reconvergence();
    }
    
skip_warp_execution:
    update_state();
    return sm_state;
}
```

---

### §3.2 CppTLM 端配套任务

#### Task #C3: 4 个 Adapter 实现（新增）

**文件**: `include/tlm/gpu/{warp_scheduler_adapter,scoreboard_adapter,pipeline_adapter,tensorcore_adapter}.hh`

- `CppTLMWarpSchedulerAdapter : public ptxsim::WarpScheduler`
- `ScoreboardAdapter : public IScoreboard`
- `PipelineAdapter : public IPipelineLatencyProvider`
- `TensorCoreAdapter : public ITensorCoreTiming`

每个 Adapter 实现 PTX-EMU 纯虚接口，桥接到 CppTLM 内部 `tlm::I*Internal` 接口。

#### Task #C4: `tlm::I*Internal` 接口 + 6 核心模块实现

- `tlm::IScoreboardInternal` + `ScoreboardTLM` (Task 9 in phase8b plan)
- `tlm::IPipelineLatencyInternal` + `PipelineTLM` (Task 11)
- `tlm::ITensorCoreTimingInternal` + `TensorCoreTLM` (Task 12)

#### Task #C5: `IAsyncCompletion` 占位实现（Phase 9+ 预留）

```cpp
// include/tlm/gpu/async_completion_adapter.hh (Phase 8.B 占位 — 不调用)
class AsyncCompletionAdapter : public IAsyncCompletion {
public:
    void register_completion_callback(uint64_t id, std::function<void()> cb) override {
        // Phase 8.B: 存回调但不触发 — Phase 9+ 填充
        pending_[id] = cb;
    }
    bool is_completed(uint64_t id) const override { return completed_.count(id) > 0; }
    void tick() override { /* Phase 8.B 空实现 */ }
private:
    std::unordered_map<uint64_t, std::function<void()>> pending_;
    std::unordered_set<uint64_t> completed_;
};
```

---

### §3.3 §3 阶段验收标准

| 编号 | 标准 | 验证 |
|:---:|------|------|
| **G-D1** | 3 纯虚接口编译通过，无 CppTLM 头文件污染 PTX-EMU | `ptxemu_tests` 全 pass |
| **G-D2** | `set_blocked_cycles_for_active()` 对 warp 内活跃线程设置延迟 | 单测 |
| **G-D3** | `exe_once()` Step A/B/C 注入后 `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle（5 类 microbenchmark 对比，参见 ADR-NV-02 §6.2 G-D5） | 集成测试 |
| **G-D4** | 4 Adapter 静态类型断言（12 端点 0-5 双向一致） | `static_assert` 编译期 |
| **G-D5** | 5 类 microbenchmark 注入后带宽 vs gpgpu-sim ±15% | GEMM/FlashAttn/vector_add/stencil/sparse |
| **G-D6** | `g_cpptlm_bridge == nullptr` 时 PTX-EMU 零退化 | 全量回归 |
| **G-D7** | `scoreboard_/pipeline_provider_/tensor_core_timing_` 任意为 nullptr 时回退到 InstructionLatencyTable | 单测 |

---

## §4 (P2 阶段): Phase 9+ Async Seams 预留

> **目标**: 在 §3 基础上预埋 TMA async 完成回调 seam
> **工期**: 1 小时（仅接口定义，无实现）
> **阻塞依赖**: §3 Task #7 SMContext 头文件修改

### §4.1 PTX-EMU 端

#### Task #10: `IAsyncCompletion` 接口 + SMContext 注入点（已在 §3 Task #7 完成）

不需要新工作 — §3 Task #7 已经包含 `IAsyncCompletion` 接口头文件和 SMContext setter。

### §4.2 CppTLM 端

#### Task #C6: `AsyncCompletionAdapter` 占位实现（已在 §3 Task #C5 完成）

不需要新工作 — §3 Task #C5 已经包含占位实现。

### §4.3 Phase 9+ 启用条件

| 触发条件 | 启用 IAsyncCompletion |
|---------|:---:|
| Phase 8.B 独立模式 | ❌（`async_completion_ = nullptr`） |
| Phase 9+ TMA async copy | ✅ CppTLM 注入 `AsyncCompletionAdapter` 真实实现 |

**Phase 9+ TMA 启用时**，只需将 `AsyncCompletionAdapter` 占位实现替换为真实 DMA 引擎实现（参考 `PTX-EMU/src/ptxsim/async/tc_queue.cpp` 中的 `commit/wait` 模式），无需改动 §3 任何代码。

---

## §5 (P3 阶段): 集成验证

> **目标**: E2E CUDA kernel 验证 + gpgpu-sim 对照
> **工期**: 1 周
> **阻塞依赖**: §2 + §3 完成

### §5.1 测试场景

| 场景 | baseline | CppTLM 路径 | 验证项 |
|------|:---:|------|------|
| **🚦 G-F0** `vector_add (n=1024²)` | standalone PTX-EMU | MemoryBridge global_access 路径（纯 F12b-LD，不含 Compute 注入） | 延迟 ≤ 2× baseline，输出逐元素与 baseline 一致 🟢 先跑（< 20s）— 隔离 Memory 故障域 |
| GEMM (FP16, M=N=K=4096) | gpgpu-sim 700 GB/s | 全链路 NoC + Compute | 带宽 ±15% |
| FlashAttn (b=8, h=16, seq=512) | 470 GB/s | 全链路 | 带宽 ±15% |
| vector_add (n=1024²) | 1176 GB/s | 全链路 | 带宽 ±15% |
| stencil (3D 7-point, N=512³) | 940 GB/s | 全链路 | 带宽 ±15% |
| sparse SpMV (10k×10k, 0.01) | 230 GB/s | 全链路 | 带宽 ±15% |

### §5.2 关键验证点

| 验证项 | 命令 | 通过标准 |
|--------|------|---------|
| PTX-EMU 零退化 | `ptxemu_tests` | 100% pass |
| CppTLM 6 模块独立 | `cpptlm_tests [gpu]` | 100% pass |
| F12b-LD 集成 | `cpptlm_tests [gpu][f12b]` | 100% pass |
| D1-Full 集成 | `cpptlm_tests [gpu][d1full]` | 100% pass |
| gpgpu-sim 对照 | `test_gpgpu_sim_comparison.py` | 5 类 ±15% pass |
| 性能 | `1 GB203 × 1M < 60s` | < 60 秒 |
| 文档同步 | `scripts/test/docs_sync_check.sh --strict` | 0 missing |

---

## §6 改造任务汇总

| 编号 | 阶段 | 端 | 文件 | 内容 | 工时 |
|:---:|:---:|:---:|------|------|:---:|
| **#1** | §2 | PTX-EMU | `include/cudart/cpptlm_bridge.h` (新) | CppTLMBridge 接口定义（含 submit/poll/synchronize_stream/global_access/version + CPPTLMBRIDGE_VERSION + cudaStream_t 静态断言） | 45 min |
| **#2** | §2 | PTX-EMU | `src/cudart/cudart_sim.cpp` (改) | cudaLaunchKernel 异步路径（完整参数传递 + PendingKernel 注册 + stream_id 字段） | 1 day |
| **#3** | §2 | PTX-EMU | `src/cudart/cudart_sim.cpp` (改) | cudaStreamSynchronize（按 stream_id 过滤 + 迭代器失效修复）+ cudaDeviceSynchronize（遍历所有 stream）+ cudaStreamCreate | 1 day |
| **#4** | §2 | PTX-EMU | `src/ptxsim/instructions/memory.cpp` (改) | LdHandler/StHandler 走 CppTLMBridge::global_access (timing-only)，数据仍读/写 SimpleMemory | 0.5 day |
| **#5** | §2 | PTX-EMU | `CMakeLists.txt` (改) | libcpptlm_cudart.so 集成构建（含 CPPTLMBRIDGE_VERSION 断言） | 0.5 day |
| **#6** | §3 | PTX-EMU | `include/ptxsim/{scoreboard,pipeline,tensor_core}_interface.h` (新) | 3 个纯虚接口 | 45 min |
| **#7** | §3 | PTX-EMU | `include/ptxsim/{sm_context,warp_context,async_completion_interface}.h` (改) | 3 setter + 1 async_completion setter + get_dest_registers helper | 1 hr |
| **#8** | §3 | PTX-EMU | `src/ptxsim/core/warp_context.cpp` (改) | set_blocked_cycles_for_active 实现 | 30 min |
| **#9** | §3 | PTX-EMU | `src/ptxsim/core/sm_context.cpp` (改) | exe_once() 四步注入 (A/B/C/D) | 1.5 day |
| **#10** | §4 | PTX-EMU | (已在 #7 完成) | IAsyncCompletion stub | 0 |
| **#C1** | §2 | CppTLM | `src/tlm/gpu/memory_bridge.{hh,cc}` (新) | MemoryBridge 实现（version + submit/poll/synchronize/global_access + kernel_args deep-copy） | 1 day |
| **#C2** | §2 | CppTLM | `src/tlm/gpu/kernel_launch_tlm.cc` (改) | EventQueue 集成 + PTX-EMU 驱动 + KernelLaunchRequest 数据结构 + FIFO 调度 | 1 day |
| **#C3** | §3 | CppTLM | `include/tlm/gpu/*_adapter.hh` (新) | 4 个 Adapter | 0.5 day |
| **#C4** | §3 | CppTLM | `include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.hh` (改) | 3 核心模块 + tlm::I*Internal 接口 | 1 day |
| **#C5** | §4 | CppTLM | `include/tlm/gpu/async_completion_adapter.hh` (新) | 占位 Adapter | 15 min |
| **合计** | | | | | **~9 天** |

---

## §7 协作流程

```
阶段 §2 (F12b-LD MemoryBridge):                  阶段 §3 (D1-Full Compute):
─────────────────────────────────                ────────────────────────────────
PTX-EMU Team           CppTLM Team               PTX-EMU Team          CppTLM Team
     │                      │                          │                      │
     ├─ #1: 接口定义 ────→  │ (CppTLM 审阅)            ├─ #6: 3 接口头文件 ──→ │ (审阅)
     │   (30 min)            │                          │   (45 min)            │
     │                      ├─ #C1: MemoryBridge ──────→│                      │
     │                      │   (1 day)                │                      │
     ├─ #2, #3: cudaLaunchKernel 异步 ─→              ├─ #7: SMContext + WarpContext (改) ─→│
     │   (1.5 day)                                  │   (1 hr)              │
     ├─ #4: hardware_memory_manager ─→              ├─ #8: set_blocked_cycles_for_active ─→
     │   (0.5 day)                                  │   (30 min)            │
     │                      ├─ #C2: KernelLaunchTLM ───→│                      │
     │                      │   (1 day)                │                      │
     ├─ #5: libcpptlm_cudart 集成 ─→                 ├─ #9: exe_once() 四步注入 ─→
     │   (0.5 day)                                  │   (1.5 day)           │
     │                      │                          │                      ├─ #C3: 4 Adapter ────→
     │                      │                          │                      │   (0.5 day)
     │                      │                          │                      ├─ #C4: 3 核心模块 ──→
     │                      │                          │                      │   (1 day)
     │                      │                          │                      ├─ #C5: async 占位 ──→
     │←── #5 集成完成 ──────┤                          │←── #9 改造完成 ──────┤
     │                      │                          │                      │
     ├─ §5 (P3): E2E 验证 ───────────────────────────→├─ §5 (P3): E2E 验证 ───→
     │   1 周                                         │   1 周                │
```

**关键路径**: §2 必须先于 §3 的 #9 完成。§3 的 #6/#7/#8 可与 §2 并行。

---

## §8 联系人

- **CppTLM Team**: CppTLM 仓库 `main` 分支
- **本文档**: `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`
- **Supersedes**: `docs/superpowers/specs/2026-07-03-ptxemu-modification-task.md`（仅 D1-Full Compute 部分，作为本文 §3 子集）
- **ADR**: `docs/adr/ADR-NV-02-phase8b-d1-strategy.md`（2026-07-14 Status Update 需追加 §9 §2 MemoryBridge 引用）
- **协作同步**: `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`

---

## §9 修订历史

- **2026-07-14 v1.0 (本文)** — 初版签发
  - 整合 F12b-LD MemoryBridge（§2）+ D1-Full Compute（§3）+ Phase 9+ Async Seam（§4）+ 集成验证（§5）
  - 明确 CppTLM 作为 clock-of-truth 的核心架构愿景
  - 修正原 modification-task.md 的实施顺序错误（D1-Full 依赖 F12b-LD）
  - 预埋 `IAsyncCompletion` 接口避免 Phase 9+ 架构重构

- **2026-07-14 v1.1 (Oracle/Metis 审查后修订)** — 当前版本
  - **Task #1 接口补全**：`submit_kernel` 签名扩展为完整参数列表（kernel_name, grid, block, args, shared_mem, stream_id）+ 新增 `version()` / `synchronize_stream()`
  - **新增 `CPPTLMBRIDGE_VERSION` 编译期断言**：双方 ABI 版本一致
  - **新增 `cudaStream_t` 宽度 `static_assert`**：防止 stream 句柄截断
  - **Task #2/3 重构**：`PendingKernel` 增加 `stream_id` 字段；`cudaStreamSynchronize` 按 stream 过滤；修复原版迭代器失效 bug（先收集再统一 erase）
  - **删除 `bridge->tick()`**：澄清控制流方向——CppTLM EventQueue 主动驱动，PTX-EMU 不反向触发 CppTLM tick
  - **Task #4 修复点迁移**：从 `hardware_memory_manager.cpp` 迁移到 `src/ptxsim/instructions/memory.cpp` 的 LdHandler/StHandler（PTX-EMU 实际修改点）
  - **Task #C1 MemoryBridge 补全**：`version()` 实现 + `kernel_args` deep-copy 策略 + `global_access` 明确为 timing-only 预计算
  - **Task #C2 KernelLaunchTLM 重构**：FIFO 调度（Phase 8.B）+ KernelLaunchRequest 数据结构 + `set_ptx_emu_gpu_context()` API
  - **§1.3 工期调整**：§2 从 5 天 → 5.5 天（+0.5 天用于 stream 支持）；总工时 ~8 天 → ~9 天
  - **§0.2 愿景图明确控制流方向**：标注"CppTLM 主动驱动，PTX-EMU 被动响应"

---

*本文档由 CppTLM Team 生成。PTX-EMU 团队有任何疑问优先在 #0（Task #1）阶段澄清。*