# cpptlm-v3-dgpu-extract: 架构设计 (Design)

> **配套**: [proposal.md](proposal.md) · [tasks.md](tasks.md) · [specs/](specs/)
> **状态**: 📐 Design — 与 ADR-X.15 §4 替代路径实施路线同步
> **Owner**: CppTLM Team
> **关联 ADR**: [ADR-X.15-cpptlm-v3-dgpu-extract](../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md)
> **关联上游**: UsrLinuxEmu ADR-090 v2 §D3.3/§D3.4 + ADR-088 v3 + PTX-EMU HSK-6
> **实施指南**: `/tmp/cpptlm-action-plan.md` (22 个操作步骤完整代码骨架)

---

## 1. 设计目标 (Goals)

### 1.1 主要目标

1. **角色反转**: CppTLM 从"桥接层 (bridge)"转变为"被驱动 dGPU 板卡 (PCIe device)"
2. **物理隔离**: 11 项 (HSK-6 §2.1) 物理删除前，所有替代路径完整可运行
3. **PCIe 设备语义对齐**: BAR0 MMIO + Doorbell SQ tail + CQ host_notify
4. **HSK-6 协议闭环**: 完成 CppTLM 端 ack (`369cf71`) + P0-1 门禁 (`fa2b3ec`) + 9 周实施路线

### 1.2 非目标 (Non-Goals)

- ❌ 不替代 PTX-EMU 内部 GPU 模拟器（仅消费 8 函数 ABI）
- ❌ 不实现完整 PCIe 协议栈（CFG + BAR0 + BAR1 最小子集）
- ❌ 不实现 CUDA driver API（由 UsrLinuxEmu + TaskRunner 端承担）
- ❌ 不实现真实 GPU 指令发射路径（由 PTX-EMU 端 `libptxemu_device.so` 承担）
- ❌ **不实施 SMContext Adapter 注入（ADR-NV-02 D1-Full 路径废止）**[Oracle 二次审查 T-P3-5]
  - **理由**: PTX-EMU `libptxemu_device.so::image_execute` 已是自包含 GPU 仿真器(含 warp scheduling / scoreboard / pipeline / tensor core / barrier / atomic),CppTLM 端 12 个 SM 模块(GpuComputeUnitTLM/ScoreboardTLM/PipelineTLM/TensorCoreTLM 等)仅为 timing reference stubs,**不构成可执行 SM**(per ADR-NV-02 §2.4 "PTX-EMU 无独立 Scoreboard 组件")
  - **影响**: D1-Full 4 Adapter (CppTLMWarpSchedulerAdapter / CppTLMScoreboardAdapter / CppTLMPipelineAdapter / CppTLMTensorCoreAdapter) 不实施;Phase 8.B D1-Full 升级(ADR-NV-02 §3.2-3.6)路径废止;G-D5 验收标准从 tasks.md Gate #6 移除
- ❌ ANTLR4 不在 CppTLM scope（per HSK-6 §3.3 + Oracle session 决议）

---

## 2. 架构对比 (v2.1 → v3.0)

### 2.1 v2.1 桥接层架构（即将废弃）

```
┌─────────────────┐
│    PTX-EMU       │  ← 真值源持有方 (ccd34155)
│  (libcudart.so)  │
│                  │
│  g_cpptlm_bridge │  ← 全局指针 (cppTLMBridge* 虚接口)
│   ──────↑────────│
└──────┼────────────┘
       │ cpptlm_attach_bridge()
       ▼
┌─────────────────┐
│    CppTLM        │  ← 桥接层 (libcpptlm_cudart.so)
│                  │
│  MemoryBridge    │  ← 实现 CppTLMBridge 虚接口 (同步路径)
│   ──────↑────────│
│                  │
│  IPtxEmuDriver   │  ← 反向 ABI (PTX-EMU → CppTLM)
│  DriverWrapper   │  ← 跨 .so PtxEmuDriverShim 适配
│  g_ptx_emu_driver│  ← 全局符号
└─────────────────┘

通信：同步 (cudaLaunchKernel → submit_kernel → CppTLM NoC 路由 → 立即返回 latency)
错误传播：CUDA-style 错误码 + TLM extension
```

### 2.2 v3.0 被驱动 dGPU 板卡架构（目标）

```
┌─────────────────�
│  UsrLinuxEmu    │  ← CUDA app 入口 (host)
│  TaskRunner     │  ← tadr-308: cuModuleLoadData → IGpuDriver::load_kernel_module
└────────┬────────┘
         │ PCIe MMIO / IOCTL / Doorbell
         ▼
┌─────────────────────────────────────────────┐
│  CppTLM (dGPU board)                         │
│  ─────────────────────────────────────────  │
│  PCIe Config Space (DGpuBar)                 │
│  ├── BAR0: 设备 ID + 中断 + 控制 regs        │
│  └── BAR1: VRAM backing (H2D DMA target)    │
│                                              │
│  Doorbell (SQ tail register)                 │
│  ├── atomic<uint64_t> sq_tail_[stream_id]    │
│  └── NotifyCallback → SQ consumer tick      │
│                                              │
│  SubmissionQueue (SQ)                        │
│  └── enqueue(KernelLaunchRequest) → tick     │
│                                              │
│  ISmExecutor (汇合点)                         │
│  ├── installImage(vram_addr, size) → handle  │
│  ├── dispatch(handle, args_vram_addr, ...)   │
│  └── 内部调 PtxEmuSubmodule                  │
│                                              │
│  PtxEmuSubmodule (dlopen libptxemu_device.so)│
│  └── 8 function pointers (image_load, ...)   │
│                                              │
│  CompletionRing (重设计)                      │
│  ├── push(image_id, status)                  │
│  └── host_notify → HAL fence_signal          │
│                                              │
│  MemoryCluster (复用)                         │
│  ├── VRAM backing                            │
│  └── 跨 channel 仲裁                          │
│                                              │
│  GpuNoC (复用)                                │
│  └── GPU 端 interconnect (Garnet 风格 mesh)  │
└─────────────────────────────────────────────┘
         │ H2D DMA / fence
         ▼
�─────────────────┐
│  PTX-EMU        │  ← 真值源 (ccd34155)
│  (libcudart.so) │  ← 仅消费 8 函数 ABI (CPPTLMBRIDGE_VERSION=2 永久冻结)
└─────────────────┘

通信：异步 (cuLaunchKernel → Doorbell ring → SQ consumer tick → 异步 dispatch → CompletionRing push → host_notify)
错误传播：CompletionRing entry status + fence
```

---

## 3. 组件契约 (Component Contracts)

### 3.1 DGpuBar (PCIe BAR0 MMIO)

**位置**: `include/tlm/gpu/dgpu_bar.hh`

**契约**:
- 模拟 PCIe Config Space + BAR0 寄存器（设备 ID + 中断 + 控制）
- BAR1 提供 VRAM backing (per ADR-069)
- `init()`: 初始化 BAR0 regs（VENDOR_ID=0x10DE NVIDIA-like, DEVICE_ID=0x1234, REVISION=0x01）
- `read_reg(offset)` / `write_reg(offset, value)`: MMIO 访问
- `vram_base()` / `vram_size()`: BAR1 VRAM 暴露给 host

**接口稳定性**: 🔴 待 P4 物理删除前验证接口冻结（避免实施中途变更导致回退）

### 3.2 Doorbell (SQ tail register)

**位置**: `include/tlm/gpu/doorbell.hh`

**契约**:
- 模拟 SQ tail register（host → device 异步信号，NVMe 模型）
- `atomic<uint64_t> sq_tail_[stream_id]`: 每 stream 独立 tail 指针
- `ring(stream_id, ring_offset)`: host 触发 doorbell（写 sq_tail_[stream_id]）
- `set_notify_callback(NotifyCallback cb)`: 通知 SQ consumer tick
- 异步语义：ring 立即返回，consumer 在后续 tick 处理

**并发保证**: `sq_tail_` 用 `std::atomic<uint64_t>` 保证 host ring / device consume 的可见性

### 3.3 SubmissionQueue (SQ)

**位置**: `include/tlm/gpu/submission_queue.hh`

**契约**:
- NVMe 模型 SQ consumer
- `enqueue(KernelLaunchRequest req)`: host 提交 kernel launch 请求
- `pending_count() const`: 查询 pending kernel 数
- `tick()`: 推进 SQ consumer 状态机（消费一个 entry → 触发 `ISmExecutor::dispatch`）

**复用**: `KernelLaunchRequest` from `include/tlm/gpu/kernel_launch_tlm.hh:30`（已存在，不新写）

### 3.4 CompletionRing (重设计 — 替代 AsyncCompletionAdapter)

**位置**: `include/tlm/gpu/completion_ring.hh`

**契约**:
- **重设计理由** (per UsrLinuxEmu ADR-090 v2 §D3.4): 避免 `AsyncCompletionAdapter::setCompletionCallback` 的 `std::function` 回调表在多 stream 并发下的死锁/饥饿问题
- `Entry { image_id, status, timestamp }`: 完成事件结构
- `push(image_id, status)`: device 推送完成事件
- `set_host_notify(HostNotifyHook hook)`: 注册 host_notify 钩子（替代 MSI-X 真实硬件路径）
- `pop() / pending_count() const`: CQ consumer 读取

**接口稳定性**: 🟢 关键接口，W1-3 骨架先实现，P3 重构完善

### 3.5 PtxEmuSubmodule (PTX-EMU 8 函数 ABI 封装)

**位置**: `include/tlm/gpu/ptx_emu_submodule.hh` + `src/tlm/gpu/ptx_emu_submodule.cc`

**契约**:
- 封装 PTX-EMU 8 函数 ABI (`CPPTLMBRIDGE_VERSION=2`)
- DSO: `libptxemu_device.so` (`dlopen` RTLD_NOW)
- 8 dlsym function pointers:
  1. `image_load(image_bytes, image_size) → handle`
  2. `image_kernel_name(handle, buf, buf_size) → int`
  3. `image_execute(handle, gx, gy, gz, bx, by, bz, shared_mem, args, argc) → int`
  4. `image_unload(handle) → int`
  5. `module_version() → int`
  6. `image_kernel_count(handle) → int`
  7. `image_kernel_name_at(handle, idx, buf, buf_size) → int`
  8. `image_execute_named(handle, name, ...) → int`

**来源**: per `PTX-EMU@ccd34155:include/cudart/cpptlm_module.h:18-52`

**错误处理**:
- `dlopen` 失败 → `throw std::runtime_error` with `dlerror()`
- dlsym 失败 + `module_version() < 2` → 报错（多 kernel API 缺失）
- 启动时一次性解析，运行时零开销

### 3.6 ISmExecutor (汇合点)

**位置**: `include/tlm/gpu/is_m_executor.hh`

**契约** (3 ABI):
```cpp
class ISmExecutor {
public:
    // ABI 1: install image (PTXIR bytes 已在 VRAM via H2D DMA)
    virtual int installImage(uint64_t vram_addr, size_t size,
                              SmImageId* out_id) = 0;

    // ABI 2: dispatch (args buffer 也在 VRAM)
    virtual int dispatch(SmImageId image,
                          uint64_t args_vram_addr, size_t args_size,
                          const DispatchParams& params) = 0;

    // ABI 3 (optional): completion callback — 实际用 CompletionRing
    // per §D3.4 重设计: 替代为 CompletionRing::set_host_notify
    // 保留接口以兼容老 code, 实现转调 CompletionRing
    virtual int setCompletionCallback(CompletionCallback cb) = 0;
};
```

**汇合关系**: `ISmExecutor` 由 `SmExecutorImpl` 实现（`src/tlm/gpu/sm_executor_impl.cc`），内部：
- `installImage` → 调 `PtxEmuSubmodule::image_load`
- `dispatch` → 调 `PtxEmuSubmodule::image_execute` + CompletionRing push
- `setCompletionCallback` → 调 `CompletionRing::set_host_notify`

### 3.7 复用组件

- **`MemoryCluster`** (`include/tlm/cluster/memory_cluster.hh`): 多通道 HBM/DDR + Arbiter + VRAM backing
- **`GpuNoC`** (`include/tlm/cluster/gpu_noc_cluster.hh`): GPU 端 mesh interconnect（Garnet 风格）
- **`KernelLaunchRequest`** (`include/tlm/gpu/kernel_launch_tlm.hh:30`): 已存在的请求结构，SQ 直接复用

---

## 4. 端到端数据流 (E2E Flow)

### 4.1 路径 1: kernel launch (host → device → host notify)

```
1. Host (UsrLinuxEmu)
   cuLaunchKernel(grid, block)
   → IOCTL PUSHBUFFER_SUBMIT_BATCH
   → HAL #66 (cuLaunchKernel IOCTL handler)
   → CppTLM Doorbell::ring(stream_id, ring_offset)
   → atomic<uint64_t> sq_tail_[stream_id] = new_tail
   → NotifyCallback 触发 SQ consumer tick

2. Device (CppTLM)
   SQ consumer tick()
   → dequeue KernelLaunchRequest
   → ISmExecutor::dispatch(image, args_vram_addr, ...)
     → 内部: map_vram_to_host(args_vram_addr) → host pointer
     → PtxEmuSubmodule::image_execute(handle, args_ptr, ...)
       → DSO call into libptxemu_device.so
       → 异步执行 (PTX-EMU 端 advance cycles)
       → 完成后返回 status
     → CompletionRing::push(image_id, status)
     → host_notify() 触发 HAL fence_signal

3. Host (UsrLinuxEmu)
   cuStreamSynchronize(stream)
   → 等 fence (CompletionRing host_notify 链路)
   → 返回 CUDA_SUCCESS
```

### 4.2 路径 2: kernel load (host → H2D DMA → device)

```
1. Host (UsrLinuxEmu)
   cuModuleLoadData(image_bytes)
   → IOCTL 0x27 (LOAD_KERNEL_MODULE)
   → HAL #66
   → CppTLM IGpuDriver::load_kernel_module
   → H2D DMA: image_bytes → VRAM (MemoryCluster vram_addr)
   → 返回 vram_addr 给 host

2. Device (CppTLM)
   PtxEmuSubmodule::installImage(vram_addr, size)
   → map_vram_to_host(vram_addr) → host pointer
   → PtxEmuSubmodule::image_load(host_ptr, size) → SmImageId
   → 返回 SmImageId 给 host
```

### 4.3 错误传播

| 错误源 | 错误位置 | 传播路径 |
|---|---|---|
| `dlopen` 失败 | PtxEmuSubmodule 构造 | `throw runtime_error` + 进程退出 |
| `image_load` 失败 | Ptx-EMU 端 | CompletionRing::push(image_id, ERROR) + host_notify |
| `image_execute` 失败 | PTX-EMU 端 | 同上 |
| H2D DMA 失败 | MemoryCluster | IOCTL 返回错误码给 host |
| Doorbell ring 失败 | CppTLM Doorbell | 返回 -EINVAL 给 host IOCTL handler |
| SQ 满 | SubmissionQueue | host 端 IOCTL 返回 -EBUSY |

---

## 5. 接口稳定性与版本演进 (Interface Stability)

### 5.1 冻结接口（P4 前禁止变更）

- ✅ `ISmExecutor` 3 ABI（`installImage` / `dispatch` / `setCompletionCallback`）
- ✅ `CompletionRing` `Entry { image_id, status, timestamp }` 结构
- ✅ `PtxEmuSubmodule` 8 个 dlsym 名称（per PTX-EMU 真值源）
- ✅ `KernelLaunchRequest` 结构（已存在）

### 5.2 可演进接口（P1-P3 期间允许调整）

- 🟡 `DGpuBar` BAR0 寄存器布局（W1-3 骨架先实现，P2 收敛时锁定）
- 🟡 `Doorbell` `NotifyCallback` 类型（W1-3 骨架先实现，P3 重构时考虑 std::function → function_ref）
- 🟡 `SubmissionQueue::tick()` 状态机（W4-6 P2 收敛时锁定）
- 🟡 `PtxEmuSubmodule` 构造函数（DSO 路径可在 P2 期间调整）

### 5.3 临时接口（P4 后删除）

- 🗑️ `MemoryBridge`（P4 物理删除）
- 🗑️ `IPtxEmuDriver` + `DriverWrapper`（P4 物理删除）
- 🗑️ `g_ptx_emu_driver` + `cpptlm_set_driver`（P4 物理删除）
- �️ `ptx_emu_driver_shim.cc`（P4 物理删除）
- 🗑️ 4 vendored cudart 头（cpptlm_bridge.h + pipeline_interface.h + scoreboard_interface.h + tensor_core_interface.h）（P4 物理删除）

---

## 6. 测试策略 (Testing Strategy)

### 6.1 单元测试 (W4-6 P2 收敛)

| 模块 | 测试 | Catch2 标签 |
|---|---|---|
| PtxEmuSubmodule | dlsym 8 个函数指针 + module_version 检查 | `[ptx-emu-submodule]` |
| DGpuBar | BAR0 regs 读写 + VRAM backing | `[dgpu-bar]` |
| Doorbell | atomic ring + notify 回调 | `[doorbell]` |
| SubmissionQueue | enqueue/tick + pending_count | `[submission-queue]` |
| CompletionRing | push/pop + host_notify | `[completion-ring]` |

### 6.2 集成测试 (W6-8 P3 重构)

| 测试 | 内容 | Catch2 标签 |
|---|---|---|
| cuModuleLoadData E2E | IOCTL → H2D DMA → PtxEmuSubmodule::installImage | `[P3][E2E]` |
| cuLaunchKernel E2E | IOCTL → Doorbell → SQ → ISmExecutor → CompletionRing | `[P3][E2E]` |
| cuStreamSynchronize E2E | fence 等待 → CompletionRing pop | `[P3][E2E]` |

### 6.3 Dual-rail 验证 (W6-8 P3 重构)

**目的**: Mode A (MemoryBridge 同步路径) vs Mode B (DGpu/Doorbell/SQ/CQ 异步路径) cycle 数对比

**5 类 microbenchmark**:
1. **GEMM** (FP32/FP16/TensorCore)
2. **vector_add** (基础算子)
3. **FlashAttention** (复杂访存模式)
4. **stencil** (规则访存)
5. **SpMV** (稀疏访存)

**通过条件**: cycle 数 ±15% tolerance（per CppTLM #19 v3.0 RFC Gate 4.7）

---

## 7. 风险与缓解 (Risk & Mitigation)

详见 ADR-X.15 §7.3 风险登记（R1-R6）。

---

## 8. 配套文档 (Related Documents)

- [proposal.md](proposal.md) — 实施提案（Why / What Changes / Acceptance Gate / Cross-Repo Coordination）
- [tasks.md](tasks.md) — P0-P4 Phase-Organized Tasks
- [specs/dgpu-board.md](specs/dgpu-board.md) — DGpuBar/Doorbell/SQ/CQ PCIe 设备语义规格
- [specs/sm-executor.md](specs/sm-executor.md) — ISmExecutor 3 ABI 规格
- [specs/completion-ring.md](specs/completion-ring.md) — CompletionRing host_notify 重设计规格
- [ADR-X.15-cpptlm-v3-dgpu-extract.md](../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) — 本地决策锁定
- [HSK-6 ack 响应](../../superpowers/specs/2026-08-18-hsk-6-response.md) — 跨仓 ack

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 与 ADR-X.15 §4 + UsrLinuxEmu 行动计划 §P1-P4 同步
**下次更新**: W4 P2 启动时（接口冻结）
