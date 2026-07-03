# F12b-LD 协作同步：CppTLM ↔ PTX-EMU

> **Purpose**: PTX-EMU 团队一次性阅读本文档即可理解 CppTLM 侧的全部现状、需求和接口期望。
> **Date**: 2026-07-01
> **CppTLM Contact**: CppTLM Team (main branch, commit `9f7db6b`)

---

## 0. 快速导航：PTX-EMU 侧需阅读的文件

| 优先级 | 文件（CppTLM 仓库） | 为什么需要读 |
|--------|----------------------|-------------|
| 🔴 | `docs/superpowers/specs/2026-06-30-f12-ptxemu-ldpreload-design.md` | **集成设计全文**（318 行）。含 CppTLMBridge 接口定义、MemoryBridge 转发逻辑、KernelLaunchTLM 扩展、地址映射公式、A-B-A 决策 |
| 🔴 | `docs/superpowers/plans/f12-ptxemu-ldpreload-integration.md` | **Momus 审查过的内部集成计划**（288 行）。含 Oracle 推荐的 Option A 渐进集成路径、correctness validation / performance baseline / interface versioning 要求 |
| 🟡 | `docs/superpowers/plans/2026-06-30-f12a-gpu-core-modules.md` | F12a 实施计划（已完成）。了解 CppTLM 侧已实现的 4 个类的接口语义 |
| 🟡 | `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` §F12 | 总体 roadmap 中 F12 部分，了解完整依赖链 |

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

**测试基线**: 755/755 pass (15517 assertions), docs_sync 0 missing, format clean.

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
| `src/memory/hardware_memory_manager.cpp` | GLOBAL 空间：if bridge → route to `MemoryBridge`，data 仍在 `SimpleMemory` |
| `CMakeLists.txt` | 新增 `libcpptlm_cudart.so` 构建目标，链接 `cpptlm_core` |

### 4.3 `cudaLaunchKernel` Async 模式

```cpp
// src/cudart/cudart_sim.cpp, cudaLaunchKernel 函数内
if (cpptlm_bridge_) {
    cpptlm_bridge_->submit_kernel(kernel_id, launch_params, param_size);
    return cudaSuccess;  // 不等待完成（调用方通过 poll_kernel 轮询完成）
} else {
    g_ptx_interpreter->launchPtxInterpreter(...);
    g_gpu_context->wait_for_completion();  // 原同步路径
}
```

### 4.4 `HardwareMemoryManager` GLOBAL 拦截

```cpp
// src/memory/hardware_memory_manager.cpp
if (req.space == MemorySpace::GLOBAL && cpptlm_bridge_) {
    uint64_t device_addr = req.address - cpptlm_bridge_->get_mmap_base();
    uint64_t latency = cpptlm_bridge_->global_access(
        device_addr, req.data, req.type);
    if (latency == UINT64_MAX) {
        // 地址未映射 → 回退到本地 fast path
        return simple_memory_->direct_access(req.address, req.data, req.size, req.is_write);
    }
    // 数据仍在 SimpleMemory 中
    simple_memory_->direct_access(req.address, req.data, req.size, req.is_write);
    return latency;
}
```

---

## 5. `CppTLMBridge` 接口定义

**文件**: `include/cudart/cpptlm_bridge.h`（PTX-EMU 侧新增）

```cpp
static constexpr uint64_t CppTLMBRIDGE_VERSION = 1;

class CppTLMBridge {
public:
    virtual ~CppTLMBridge() = default;

    /// 异步提交 kernel launch（不等待完成）。返回 false 表示提交失败
    virtual bool submit_kernel(uint64_t kernel_id, const void* params,
                               size_t param_size) = 0;

    /// GLOBAL 内存访问：传入 device_addr，返回 latency cycles。
    /// 返回 UINT64_MAX 表示地址未映射
    virtual uint64_t global_access(uint64_t device_addr, uint64_t val,
                                   uint8_t type) = 0;

    /// 轮询 kernel 完成状态。返回 0 表示已完成，>0 表示剩余 cycles，
    /// UINT64_MAX 表示未知 kernel_id
    virtual uint64_t poll_kernel(uint64_t kernel_id) = 0;

    /// 返回 SimpleMemory 的 mmap 基地址（用于 device_addr 转换）
    virtual uint64_t get_mmap_base() const = 0;

    /// 返回当前 CppTLM EventQueue tick 计数（仿真启动前返回 0）
    virtual uint64_t get_current_cycle() const = 0;
};
```

### 5.1 错误语义

| 条件 | 返回值 | 日志 |
|------|--------|------|
| `global_access` 地址不在 mmap 范围内 | `UINT64_MAX` | WARN "address not in mmap range" |
| `submit_kernel` 在 bridge 未初始化时调用 | `false` | ERROR "bridge not initialized" |
| `submit_kernel` 参数无效 | `false` | ERROR "invalid kernel params" |
| `poll_kernel` 未知 kernel_id | `UINT64_MAX` | WARN "unknown kernel_id" |
| `get_current_cycle()` 在首次 `submit_kernel` 前调用 | `0`（有效，仿真尚未开始） | 无需日志 |
| Bridge 版本不匹配 | 编译期 `static_assert(CppTLMBRIDGE_VERSION >= EXPECTED)` | FATAL 中止 |

**地址映射**: `device_addr = virtual_addr - mmap_base - device_base_offset`（通常 `device_base_offset = 0`）。

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

1. **Tick 先后顺序**：CppTLM 先完成一个 tick 推进，_然后_ bridge 调用 `exe_once()`。`get_current_cycle()` 返回 `exe_once()` 执行**之前**的 tick 值。
2. **空闲循环熔断**：两次 `global_access` 调用之间，PTX-EMU 可执行多次 `exe_once()`，但 simulation cycle counter **不**推进。
3. **上限防护**：每个 CppTLM tick 最多执行 `max_ptx_steps_per_tick`（默认 10,000）次 `exe_once()`，防止无限循环/死锁。

> 双时序模型对齐的详细风险分析见 `docs/superpowers/plans/f12-ptxemu-ldpreload-integration.md` §7 和 §10.1。

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
| 2 | `SimpleMemory` mmap base 如何暴露给 `MemoryBridge` | 通过 `CppTLMBridge::get_mmap_base()` 解决 |
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

*本文档由 CppTLM Team 生成，用于协作同步。如有疑问请联系 CppTLM Team。*