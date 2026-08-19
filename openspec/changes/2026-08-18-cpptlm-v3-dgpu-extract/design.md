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

#### 3.2.5 PCIe Strong-Ordered Write Semantics [Oracle v0.4 增量]

`Doorbell::ring(stream_id, tail)` 调用必须通过 **strong-ordered write path**,因为 PCIe TLP posted writes **不保序**(尤其跨 aperture)。

**机制**(per `docs/research/PCIe/PCIe_上的保序write.md` §2-§5):
- MMU ordering pipe 在 aperture-switch 时插入 **dummy non-posted read flush event**
- 性能代价:**PCIe Gen5 x16 ≈ 250-350ns**;多级 switch 400-600ns;跨 RC > 700ns
- 实现链路:weak store → posted write(入队)→ strong store → aperture-switch 检查 → MMU 生成 dummy non-posted read → PCIe switch 强制 flush posted writes → read completion → strong store 发出

**CppTLM 落地**(`Doorbell::ring`):
```cpp
void Doorbell::ring(uint32_t stream_id, uint64_t tail) {
    // Step 1: weak atomic write (本仓内)
    sq_tail_[stream_id].store(tail, std::memory_order_release);

    // Step 2: aperture-switch check (BAR2 = doorbell MMIO)
    if (mmu_->cross_aperture(BAR2_DOORBELL_SPACE)) {
        // Step 3: dummy non-posted read flush
        uint32_t flush_id = mmu_->insert_dummy_read_flush(BAR2_DOORBELL_SPACE);

        // Step 4: schedule completion (250-700ns 延迟)
        eq_->schedule_after_cycles(flush_id, PCIe_GEN5_FLUSH_CYCLES);
    }

    // Step 5: notify (existing 异步)
    notify_cb_(stream_id);
}
```

**测试要求**:`test_doorbell.cc §strong-ordered-path` 验证:
1. read completion 必须等待 weak store 入队后才能发出(强制 flush)
2. latency 区间在 250-700ns 范围
3. 同 stream 多次 ring 顺序保持

**跨仓参考**:NVIDIA 同样使用该模式处理 GPU doorbell write(per `PCIe_上的保序write.md` §5)。

### 3.3 SubmissionQueue (SQ)

**位置**: `include/tlm/gpu/submission_queue.hh`

**契约**:
- NVMe 模型 SQ consumer
- `enqueue(KernelLaunchRequest req)`: host 提交 kernel launch 请求
- `pending_count() const`: 查询 pending kernel 数
- `tick()`: 推进 SQ consumer 状态机（消费一个 entry → 触发 `ISmExecutor::dispatch`）

**复用**: `KernelLaunchRequest` from `include/tlm/gpu/kernel_launch_tlm.hh:30`（已存在，不新写）

#### 3.3.5 Task Dependency Table (per Oracle v0.4)

`SubmissionQueue::tick()` 内部维护 **`inflight_kernel_reqs_ map`** 作为 **Task Dependency Table 抽象**(per US20230236878A1 §3.3 Task Dependency Table 240):

| 字段 | 类型 | 含义 |
|------|------|------|
| `task_id` | uint32_t | PTX-EMU 完成信号关联 |
| `dispatch_record_ptr` | `TmuDispatchRecord*` | 调度记录引用 |
| `wait_on_latch_id` | uint32_t | Wait On 依赖锁存器 |
| `arrive_at_latch_id` | uint32_t | Arrive At 依赖锁存器 |
| `pre_exit_policy` | enum | NONE / LAST_BLOCK / EXPLICIT_KERNEL_MARKER |
| `enqueue_cycle` | uint64_t | 入队 cycle,用于超时检测 |

**容量上限**:`tmu_max_active_tasks` 配置项,默认 **256** (对齐 GPU 一代活跃任务上界)
**溢出策略**:**LIFO eviction** + 软栅栏(fence_ring entry 标记 CUDA_ERROR_LAUNCH_TIMEOUT)

**测试要求**:
1. `test_submission_queue.cc §task-dependency-table` 验证 map 插入/查找/删除
2. `test_submission_queue.cc §overflow-eviction` 验证 256+1 第 257 task 触发 LIFO + 软栅栏
3. `test_submission_queue.cc §latch-matching` 验证 wait_on ↔ arrive_at 配对

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

#### 3.6.4 Dispatch 经 TMU Glue (per Oracle v0.4)

`ISmExecutor::dispatch` 调用通过 **TMU (Task Management Unit) glue logic** 触发(per Option B 决策,见 §3.8):
```
SQ::tick() → TMU::pre_dispatch() (评估 pre-exit policy)
           → TMU::check_dependencies() (验证 wait_on ↔ arrive_at 匹配)
           → ISmExecutor::dispatch()
           → PtxEmuSubmodule::image_execute() (PTX-EMU 自包含 GPU 仿真)
```

**不仿真** PREEXIT/ACQBULK 指令(per `docs/research/TMU/高效任务启动_解析.md` §3.2/3.4)——这些由 PTX-EMU 端 `libptxemu_device.so` 自包含,CppTLM 端只追踪完成信号。

**TMU 内部职责**(详见 §3.8):
- 维护 `inflight_kernel_reqs_ map`(per §3.3.5)
- 依赖锁存器管理(`wait_on_latch_id ↔ arrive_at_latch_id` 匹配)
- pre-exit policy 调度(NONE/LAST_BLOCK/EXPLICIT_KERNEL_MARKER 三档)
- 完成信号回收(CompletionRing::push → TMU::on_complete → map evict)

**新增位置**:`include/tlm/gpu/tmu_dispatch_processor.hh`(约 200 LOC)

### 3.8 Task Dispatch Frontend (TMU Glue Logic) [Oracle v0.4 新增]

本节定义 **TMU (Task Management Unit) glue logic**——位于 `SubmissionQueue` 与 `ISmExecutor` 之间的内部组件。

#### 3.8.1 组件定位

TMU **不属于** openspec spec(非用户面对 ABI),仅作 `DGpuBoardTLM` 内部组件,约 200 LOC,负责 `SQ::tick()` 与 `ISmExecutor::dispatch()` 之间的依赖解耦与调度策略。

#### 3.8.2 数据结构

```cpp
// include/tlm/gpu/tmu_dispatch_processor.hh
namespace tlm::gpu {

enum class PreExitPolicy : uint8_t {
    NONE,                       // 直接 dispatch
    LAST_BLOCK,                 // 等生产者最后一个线程块后 dispatch
    EXPLICIT_KERNEL_MARKER      // 等 PTX-EMU 端 PREEXIT 指令标记
};

struct TmuDispatchRecord {
    uint64_t kernel_entry_pc;
    uint32_t grid_dim[3];
    uint32_t block_dim[3];
    uint64_t args_vram_addr;
    uint32_t args_size;
    uint32_t shared_mem_bytes;
    uint32_t reg_per_thread;
    uint8_t  priority;
    uint8_t  stream_id;
    uint8_t  cluster_id;
    bool     dep_enable;
    uint64_t dep_tmd_handle;
    uint64_t dep_ptr;
    uint8_t  tmd_type;           // TMD_TYPE_GRID (v3.0 固定)
    uint64_t tmd_handle;
    uint64_t completion_ring_slot;
    PreExitPolicy pre_exit_policy;
    uint32_t table_slot_id;
    uint32_t wait_on_latch_id;
    uint32_t arrive_at_latch_id;
};

class TmuDispatchProcessor {
public:
    TmuSubmitResult submit(KernelLaunchRequest req,
                           const DispatchBinding& binding);
    void on_complete(TmuDispatchRecord& rec);
    bool try_chain_dependent(TmuDispatchRecord& rec);

private:
    uint16_t select_cluster(uint64_t stream_id) const;
    std::unordered_map<uint32_t, TmuDispatchRecord> inflight_kernel_reqs_;
    std::vector<SubmissionQueue*> queues_;
    static constexpr uint32_t MAX_ACTIVE_TASKS = 256;
};

}  // namespace tlm::gpu
```

#### 3.8.3 接口契约

| 接口 | 调用方 | 用途 |
|------|--------|------|
| `submit(req, binding)` | `CommandProcessor` (DISPATCH 状态) | 提交 kernel launch 请求 |
| `on_complete(rec)` | `SubmissionQueue::tick()` (完成路径) | 处理完成信号 + 触发 dep 链 |
| `try_chain_dependent(rec)` | `on_complete` 内部 | 链式推进 dep.ptr 指向的下一任务 |

#### 3.8.4 调度策略

**`select_cluster(stream_id)`** (MVP):`stream_id % cluster_count` 固定绑定,生命周期内禁止迁移。

**`pre_dispatch(record)`** 检查:
1. `dep_enable == false` → 直接 dispatch
2. `dep_enable == true && dep_tmd_handle != 0` → 检查 dep 锁存器匹配后 dispatch
3. `pre_exit_policy == LAST_BLOCK` → 等生产者最后一个线程块到达(通过 `inflight_kernel_reqs_[producer_id].last_block_seen` 判定)

#### 3.8.5 容量管理

**`inflight_kernel_reqs_.size() >= MAX_ACTIVE_TASKS`** → **LIFO eviction**:
- 选择最近入队的非 pinned 任务
- 触发 `CompletionRing::push(evicted_id, CUDA_ERROR_LAUNCH_TIMEOUT)`
- host 端 FenceRegistry 返回错误

#### 3.8.6 性能预期

| 操作 | 时间复杂度 | 备注 |
|------|----------|------|
| `submit` | O(1) amortized | hash map insert |
| `select_cluster` | O(1) | 取模 |
| `pre_dispatch` | O(1) | 简单条件检查 |
| `on_complete` | O(1) | hash map erase + 可选 chain |
| LIFO eviction | O(N) | 全表扫描,256 entries 实际可忽略 |

#### 3.8.7 跨文档参考

- US20230236878A1 `Task Dependency Table 240` (per `docs/research/TMU/高效任务启动_解析.md` §3.3)
- TMU 三代形态(per `docs/research/TMU/TMU专利专题整理.md` §1):Kepler → Volta/Ampere → Hopper (WSDU)
- **dGPU v3 路径**:TMU 简化,不实施 PREEXIT/ACQBULK(由 PTX-EMU 自带),不实施 SM 端任务提交(per `docs/research/TMU/高效任务启动_解析.md` §3.1 [0050])

### 3.7 复用组件

- **`MemoryCluster`** (`include/tlm/cluster/memory_cluster.hh`): 多通道 HBM/DDR + Arbiter + VRAM backing
- **`GpuNoC`** (`include/tlm/cluster/gpu_noc_cluster.hh`): GPU 端 mesh interconnect（Garnet 风格）
- **`KernelLaunchRequest`** (`include/tlm/gpu/kernel_launch_tlm.hh:30`): 已存在的请求结构，SQ 直接复用

---

## 4. 端到端数据流 (E2E Flow)

### 4.1 Pipeline Overview

```
┌────────────────────────────────────────────────────────────────────────┐
│                          Host (UsrLinuxEmu + TaskRunner)                │
│                                                                        │
│  cuModuleLoadData(image_bytes)                                         │
│    → IOCTL 0x27 LOAD_KERNEL_MODULE                                    │
│    → HAL 构造 KernelLaunchRequest                                     │
│    → H2D DMA: image_bytes → VRAM (vram_addr)                          │
│    → 返回 CUmodule handle                                              │
│                                                                        │
│  cuLaunchKernel(grid, block, args, shared_mem, ...)                    │
│    → IOCTL 0x01 PUSHBUFFER_SUBMIT_BATCH                               │
│    → [v0.4.1 修订] host 解析 PM4 (UsrLinuxEmu 端 PM4 decoder)        │
│    → 构造 `KernelLaunchRequest`(已填字段)                              │
│    → 调用 `DGpuBoardTLM::submit_kernel(req)` (直接 API, 不走 ring)     │
│    → DGpuBoardTLM 内部 `TmuDispatchProcessor::submit(record)`          │
└────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (PCIe TLP via MMU ordering pipe, 250-700ns)
┌────────────────────────────────────────────────────────────────────────┐
│                      CppTLM DGpuBoardTLM                                │
│                                                                        │
│  �────────────────────────────────────────────────────────────────┐  │
│  │  PCIe Substrate (DGpuBar + Doorbell)                              │  │
│  │  ├─ BAR0: device regs (0x0000-0x0FFF)                            │  │
│  │  │       └─ 0x1000-0x1FFF: doorbell MMIO space (per subchannel)    │  │
│  │  └─ BAR1: VRAM backing (HBM2 region, mmap'd to host)            │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  **[v0.4.1 修订]** PM4 解析委托 host 端(UsrLinuxEmu/TaskRunner) │  │
│  │  ├─ host 构造 `KernelLaunchRequest`(已解码 fields)               │  │
│  │  ├─ host 直接调 `DGpuBoardTLM::submit_kernel(req)`                │  │
│  │  ├─ DGpuBoardTLM 内部 `TmuDispatchProcessor::submit(record)`      │  │
│  │  └─ Doorbell ring 触发 SQ consumer tick                           │  │
│  │  **删除**: CommandProcessor + Pm4Decoder (v3.0 无 PM4 解析)     │  │
│  │  **理由**: host 已实现 CUDA driver API + PM4 解析,                  │  │
│  │           CppTLM 仅消费已解码 `KernelLaunchRequest`                  │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  TmuDispatchProcessor (TMU Glue, §3.8)                            │  │
│  │  ├─ submit: hash map insert (table_slot_id = inflight_kernel_reqs_.size())│  │
│  │  ├─ pre_dispatch: dep + pre_exit_policy check                    │  │
│  │  └─ dispatch: ISmExecutor::dispatch(image, args_vram, params)   │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  SubmissionQueue[cluster] (per-cluster FIFO, §3.3)                 │  │
│  │  ├─ enqueue(KernelLaunchRequest) → trigger TMU submit            │  │
│  │  ├─ tick(): pop → ISmExecutor::dispatch via TMU                   │  │
│  │  └─ on_complete: TMU::on_complete(record) → map evict           │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  ISmExecutor / SmExecutorImpl (PTX-EMU 自包含执行器)              │  │
│  │  ├─ dispatch(image, args_vram, params) → map_vram_to_host()     │  │
│  │  ├─ PtxEmuSubmodule::image_execute(handle, gx,gy,gz, bx,by,bz, ...) │  │
│  │  └─ (PTX-EMU 内部: warp scheduling + scoreboard + pipeline + TC) │  │
│  └────────────────────────────────────────────────────────────────�  │
│                              │                                          │
│  ┌────────────────────────────▼────────────────────────────────────┐  │
│  │  CompletionRing + FenceRegistry (host-side signal)                │  │
│  │  ├─ CompletionRing::push(image_id, status) → release mutex        │  │
│  │  ├─ host_notify_() → signal hook (release lock 后调)           │  │
│  │  └─ host 端: FenceRegistry.complete(request_id, status)          │  │
│  └────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (cuStreamSynchronize 等 fence)
┌────────────────────────────────────────────────────────────────────────┐
│                          Host (UsrLinuxEmu)                             │
│                                                                        │
│  cuStreamSynchronize(stream) → FenceRegistry.wait → 返回 CUDA_SUCCESS  │
└────────────────────────────────────────────────────────────────────────┘
```

### 4.2 关键时序特性

| 阶段 | 延迟 | 备注 |
|------|------|------|
| cuLaunchKernel → gpfifo_entry write | ~100ns | host DMA |
| gpfifo_entry → doorbell ring | ~50ns | host 内 |
| PCIe MMIO weak store | 0ns | 本地 |
| PCIe MMU ordering pipe flush | **250-700ns** | strong-ordered,Gen5 x16 |
| CP FETCH → DECODE → DISPATCH | ~1us | TMU glue |
| SQ tick → ISmExecutor::dispatch | ~50ns | queue pop |
| PTX-EMU image_execute | variable | 自包含 GPU sim |
| CompletionRing::push + host_notify | ~50ns | signal |
| **总 cycle budget** | **500ns-2us + kernel exec** | 取决于 PTX-EMU 内部 |

### 4.3 错误传播

| 错误源 | 错误位置 | 传播路径 |
|---|---|---|
| Doorbell strong-order fail | Doorbell::ring | 同步失败,IOCTL 返回 EAGAIN |
| SQ 满 | SubmissionQueue::enqueue | Busy → CP 重试 ring buffer head 不前移 |
| TMU 容量满(256) | TmuDispatchProcessor | LIFO eviction + 软栅栏 |
| pre_exit_policy 失效 | TMU::pre_dispatch | CompletionRing::push(evicted_id, CUDA_ERROR_LAUNCH_TIMEOUT) |
| dep 锁存器不匹配 | TMU::check_dependencies | log warn + CompletionRing::push(id, CUDA_ERROR_LAUNCH_FAILED) |
| image_load 失败 | PTX-EMU image_execute | CompletionRing::push(image_id, CUDA_ERROR_INVALID_HANDLE) |
| PCIe flush 超时(rare) | MMU ordering pipe | log + retry,最大 3 次 |

---

## 5. 接口稳定性与版本演进 (Interface Stability)

### 5.1 冻结接口（P4 前禁止变更）

- ✅ `ISmExecutor` 3 ABI（`installImage` / `dispatch` / `setCompletionCallback`）
- ✅ `CompletionRing` `Entry { image_id, status, timestamp }` 结构
- ✅ `PtxEmuSubmodule` 8 个 dlsym 名称（per PTX-EMU 真值源）
- ✅ `KernelLaunchRequest` 结构（已存在）
- ✅ `Doorbell::ring` strong-ordered write path（per §3.2.5 PCIe Gen5 x16 250-700ns 区间断言）
- ✅ `SubmissionQueue::inflight_kernel_reqs_ map` 字段 + LIFO eviction（per §3.3.5,256 slot 默认）
- ✅ `TmuDispatchProcessor` 接口契约（submit/on_complete/try_chain_dependent，per §3.8.3）

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

### 5.4 内部接口（P3 后允许调整，非用户面对）

> **声明**: TMU 内部组件属于 `DGpuBoardTLM` 实现细节,**不是 openspec spec**(非 PTX-EMU ABI 边界)。

- 🟡 `TmuDispatchRecord` 21 字段结构（per §3.8.2）
- 🟡 `TmuDispatchProcessor::pre_dispatch` 检查顺序（per §3.8.4）
- 🟡 `PreExitPolicy` 三档枚举：`NONE` / `LAST_BLOCK` / `EXPLICIT_KERNEL_MARKER`
- 🟡 `MMU ordering pipe` flush 模型与 latency 区间（per §3.2.5,PCIe Gen5 x16 250-700ns）
- 🟡 `inflight_kernel_reqs_ map` LIFO eviction 策略（per §3.3.5）
- 🟡 `TmuDispatchRecord::dep_ptr` 环检测深度上限（8）

---

## 6. 测试策略 (Testing Strategy)

### 6.1 单元测试 (W4-6 P2 收敛)

| 模块 | 测试 | Catch2 标签 |
|---|---|---|
| PtxEmuSubmodule | dlsym 8 个函数指针 + module_version 检查 | `[ptx-emu-submodule]` |
| DGpuBar | BAR0 regs 读写 + VRAM backing | `[dgpu-bar]` |
| Doorbell | atomic ring + notify 回调 | `[doorbell]` |
| **Doorbell §strong-ordered-path** | **read completion 等待 + 250-700ns latency + 同 stream 顺序** | `[doorbell][strong-order]` |
| SubmissionQueue | enqueue/tick + pending_count | `[submission-queue]` |
| **SubmissionQueue §task-dependency-table** | **map insert/lookup/delete + overflow-eviction + latch-matching** | `[submission-queue][tdt]` |
| CompletionRing | push/pop + host_notify | `[completion-ring]` |
| **ISmExecutor §tmu-glue** | **dispatch 经 TMU + pre-exit policy 调度** | `[sm-executor][tmu]` |
| **TmuDispatchProcessor** | **submit/on_complete/try_chain_dependent + LIFO eviction** | `[tmu][glue]` |

### 6.2 集成测试 (W6-8 P3 重构)

| 测试 | 内容 | Catch2 标签 |
|---|---|---|
| cuModuleLoadData E2E | IOCTL → H2D DMA → PtxEmuSubmodule::installImage | `[P3][E2E]` |
| cuLaunchKernel E2E | IOCTL → Doorbell (strong-ordered) → SQ → TMU → ISmExecutor → CompletionRing | `[P3][E2E]` |
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

### 6.4 TMD-aware 测试 (W4-6 P2)

| 测试 | 内容 | 标签 |
|------|------|------|
| `T-TMD-01` | CP 解析 PM4 → 构造 TMD 6 区字段，字段偏移正确 | `[tmu][tmd]` |
| `T-TMD-02` | Grid TMD vs Queue TMD 类型判定（v3.0 拒绝 Queue TMD） | `[tmu][tmd]` |
| `T-TMD-03` | Scheduler Cache insert + lookup（key-only fields） | `[tmu][cache]` |
| `T-TMD-04` | TmuDispatchRecord field round-trip（serialize/deserialize） | `[tmu][record]` |
| `T-TMD-05` | dep.enable=0 → reclaim；dep.enable=1 → chain | `[tmu][dep]` |
| `T-TMD-06` | 链式 depth=3 推进，验证 dep.ptr 解引用正确 | `[tmu][dep][chain]` |
| `T-TMD-07` | TMU reclaim 时正确从 Scheduler Cache evict（无泄漏） | `[tmu][lifecycle]` |
| `T-TMD-08` | 异常路径：dep.ptr 悬空 → TMU 拒绝（环检测简化版） | `[tmu][error]` |

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

### 7.1 ADR-X.15 原始风险（R1-R6）

详见 [`ADR-X.15-cpptlm-v3-dgpu-extract.md` §7.3](../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md)（per Oracle 一审 + 二审已锁定）。

### 7.2 v0.4 新增风险（per Oracle TMU Catalog v3）

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| **R7** | **MMU ordering pipe 仿真误差** | 中 | 中 | per `docs/research/PCIe/PCIe_上的保序write.md` §4 latency 区间（PCIe Gen5 x16 250-350ns；多级 switch 400-600ns；跨 RC > 700ns）；MVP 用**区间断言**而非精确点 |
| **R8** | **Task Dependency Table 容量溢出** | 中 | 中 | LIFO eviction + 软栅栏（per §3.3.5）；`tmu_max_active_tasks` 默认 256；溢出率 > 1% 触发 review |
| **R9** | **TMD 字段版本不匹配** | 低 | 中 | 6 区字段表头带 `version` 字段；不匹配则 CP 拒绝 |
| **R10** | **Grid TMD vs Queue TMD 误用** | 中 | 高 | v3.0 硬约束 `TMD_TYPE_GRID only`；CP 解析阶段拒绝 Queue TMD |
| **R11** | **dep.ptr 悬空/环依赖** | 中 | 中 | 简化环检测（链深 ≤ 8）；reclaim 前检查 refcount=0 |
| **R12** | **Scheduler Cache 容量耗尽** | 中 | 中 | 容量可配（默认 1024 entries）；LRU 驱逐；超限拒绝并 log |

### 7.3 升级触发条件

以下条件任一触发，应升级到 v0.5（含硬件 refcount + Event Stream）：

1. **多流真实并发需求**：超过 2 stream × 4 cluster，且 CompletionRing fence 不够用
2. **CUDA Graph-like 设备侧图启动**：需要在 CppTLM 端支持 device-launch graph
3. **TMD 数量 > 1024**：Scheduler Cache 频繁驱逐，性能下降
4. **依赖链深度 > 8**：单级链式推进延迟过高
5. **PREEXIT/ACQBULK 性能优化成为验收项**：当前由 PTX-EMU 端 libptxemu_device.so 自包含

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
