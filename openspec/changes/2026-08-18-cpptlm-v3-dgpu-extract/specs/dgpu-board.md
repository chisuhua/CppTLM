# Spec: DGpuBar/Doorbell/SubmissionQueue — dGPU 板卡 PCIe 设备语义规格

> **配套**: [../design.md §3.1-3.3](../design.md) · [../proposal.md](../proposal.md) · [../tasks.md §P1 轨 B](../tasks.md)
> **状态**: 📋 Spec — W1-3 实施，W4-6 接口冻结
> **Owner**: CppTLM Team
> **关联 ADR**: [ADR-X.15-cpptlm-v3-dgpu-extract §4.2 P1 双轨](../../../docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md)
> **关联上游**: UsrLinuxEmu ADR-090 v2 §D3.3 (DGpuBar/Doorbell/SQ/CQ 最小完备集)

---

## 1. 范围 (Scope)

本规格覆盖 dGPU 板卡的 PCIe 设备语义三件套：
- **DGpuBar**: PCIe Config Space + BAR0 MMIO + BAR1 VRAM backing
- **Doorbell**: SQ tail register (host→device 异步信号)
- **SubmissionQueue**: NVMe 模型 SQ consumer

**CompletionRing** 见 [completion-ring.md](completion-ring.md)（独立规格）。

**ISmExecutor** 汇合点见 [sm-executor.md](sm-executor.md)（独立规格）。

---

## 2. DGpuBar 规格

### 2.1 公共 API

```cpp
class DGpuBar {
public:
    // PCIe Config Space IDs (NVIDIA-like)
    static constexpr uint16_t VENDOR_ID = 0x10DE;
    static constexpr uint16_t DEVICE_ID = 0x1234;
    static constexpr uint8_t  REVISION  = 0x01;

    // 生命周期
    void init();                              // 初始化 BAR0 regs + VRAM backing
    void shutdown();                          // 释放 VRAM backing

    // MMIO 访问
    uint32_t read_reg(uint32_t offset);       // 4-byte aligned read
    void write_reg(uint32_t offset, uint32_t value);  // 4-byte aligned write

    // BAR1 VRAM 暴露
    void*  vram_base() const;
    size_t vram_size() const;
};
```

### 2.2 行为契约

#### init()

- **Precondition**: `vram_size_ == 0 && vram_base_ == nullptr`
- **Postcondition**:
  - BAR0 regs 初始化（设备 ID、中断状态、控制位）
  - `vram_base_` 非空，`vram_size_` > 0
  - 默认 VRAM size = 16 GB（per gem5 full-system GPU 工业惯例，可 JSON 配置覆盖）
- **错误**: VRAM 分配失败 → 抛 `std::bad_alloc`

#### read_reg(offset)

- **Precondition**: `offset` 是 4-byte aligned 且 `< sizeof(bar0_regs_)`
- **Postcondition**: 返回 `bar0_regs_[offset / 4]`
- **错误**: 越界访问 → 抛 `std::out_of_range`

#### write_reg(offset, value)

- **Precondition**: 同 read_reg
- **Side effects**:
  - 写 `bar0_regs_[offset / 4]`
  - 特定寄存器（如中断控制）触发对应 handler
- **错误**: 越界访问 → 抛 `std::out_of_range`

#### vram_base() / vram_size()

- **Postcondition**: 返回 BAR1 VRAM 的 host pointer 和 size
- **线程安全**: host 端读并发安全

### 2.3 错误处理

| 错误源 | 处理 |
|---|---|
| VRAM 分配失败 | `std::bad_alloc` 抛 |
| MMIO 越界 | `std::out_of_range` 抛 |
| 中断 handler 内部错误 | 写 BAR0 中断状态寄存器 + 不抛（与真实硬件一致） |

### 2.4 测试要求

- ✅ `init()` 后 `vram_base()` 非空，`vram_size()` > 0
- ✅ `read_reg`/`write_reg` round-trip 一致
- ✅ MMIO 越界抛 `std::out_of_range`
- ✅ VRAM backing 可被 H2D DMA 写入并被 CPU 端读回
- ✅ 并发 read_reg / write_reg 线程安全（per `std::atomic<uint32_t>` 语义）

### 2.5 BAR0 MMIO 地址映射表 [Oracle A.5 新增]

> **触发**: Oracle 评审 A.5 — 真实 PCIe GPU 的 doorbell 是对 BAR0 特定 offset 的 MMIO write;当前 spec 中 DGpuBar 与 Doorbell 是两个无连线的对象,host 直接调 `doorbell->ring()` 绕过了 BAR 抽象。**应统一从 `DGpuBar::write_reg` 入口进入,MMIO trace 可统一记录**。

| BAR0 Offset 范围 | 用途 | 访问语义 |
|---|---|---|
| `0x0000 - 0x00FF` | PCIe Config Space 镜像(VENDOR_ID / DEVICE_ID / STATUS / COMMAND) | R/W,4-byte aligned |
| `0x0100 - 0x0FFF` | 中断控制寄存器(MSI-X msg addr/data, mask) | R/W |
| `0x1000 - 0x1FFF` | **Doorbell SQ tail register space**(per-stream) | W only,8-byte aligned |
| `0x1000 + stream_id * 8` | doorbell ring(stream_id) → `Doorbell::ring(stream_id, value)` | W only |
| `0x2000 - 0x2FFF` | 控制 regs(reset / power state / debug) | R/W |
| `0x3000+` | 保留 / 厂商自定义 | — |

**集成契约**:
- `DGpuBar::write_reg(offset, value)` 检查 offset 范围;若落在 `0x1000-0x1FFF` doorbell 空间,内部转调 `Doorbell::ring((offset - 0x1000) / 8, value)`
- host 侧 API 收敛为:**所有"通知 device"操作都经 `DGpuBar::write_reg` 一个入口**(MMIO trace 一致性)
- 单元测试新增:`test_dgpu_bar.cc` §"Doorbell MMIO routing" 验证 `write_reg(0x1000 + stream*8, tail)` 触发 `Doorbell::ring` 回调

---

## 3. Doorbell 规格

### 3.1 公共 API

```cpp
class Doorbell {
public:
    static constexpr uint32_t MAX_STREAMS = 1024;

    using NotifyCallback = std::function<void(uint32_t stream_id)>;

    void ring(uint32_t stream_id, uint64_t ring_offset);  // host 触发
    void set_notify_callback(NotifyCallback cb);          // device 注册 consumer

    uint64_t sq_tail(uint32_t stream_id) const;           // device 读
};
```

### 3.2 行为契约

#### ring(stream_id, ring_offset)

- **Precondition**: `stream_id < MAX_STREAMS`
- **Side effects**:
  - `sq_tail_[stream_id].store(ring_offset, memory_order_release)`
  - 异步触发 `notify_cb_(stream_id)`（如果注册）
- **错误**: 越界 stream_id → 抛 `std::out_of_range`

#### set_notify_callback(cb)

- **Precondition**: 无
- **Postcondition**: 后续 `ring()` 触发调用 `cb(stream_id)`
- **线程安全**: 仅 device 端单线程调用（设置回调）

#### sq_tail(stream_id)

- **Precondition**: `stream_id < MAX_STREAMS`
- **Postcondition**: 返回 `sq_tail_[stream_id].load(memory_order_acquire)`
- **并发保证**: 与 `ring()` 同一 stream 的 release-acquire 同步

### 3.3 错误处理

| 错误源 | 处理 |
|---|---|
| 越界 stream_id | `std::out_of_range` 抛 |
| 未注册 notify_cb | 静默（无副作用）|
| notify_cb 内部抛 | 不传播（异步路径，与真实硬件 IRQ 一致）|

### 3.4 测试要求

- ✅ `ring()` 后 `sq_tail()` 返回 `ring_offset`
- ✅ `set_notify_callback` 后 `ring()` 触发回调，回调参数 `stream_id` 正确
- ✅ 并发 `ring()` / `sq_tail()` 跨 stream 互不干扰
- ✅ 越界 stream_id 抛 `std::out_of_range`
- ✅ 未注册 notify_cb 时 `ring()` 不崩溃

---

## 4. SubmissionQueue 规格

### 4.1 公共 API

```cpp
class SubmissionQueue {
public:
    static constexpr size_t MAX_ENTRIES = 4096;  // NVMe 风格深度

    // [Oracle C-NEW-4 改写]: 返回 bool 而不是 void
    // - true: 成功入队
    // - false: SQ 满,入队失败（host IOCTL 应返回 -EBUSY）
    bool enqueue(KernelLaunchRequest req);
    size_t pending_count() const;
    bool tick();  // 推进 SQ consumer 状态机（消费一个 entry）

    // 注入 consumer
    using DispatchHandler = std::function<int(const KernelLaunchRequest&)>;
    void set_dispatch_handler(DispatchHandler handler);
};
```

### 4.2 行为契约

#### enqueue(req)

- **Precondition**: 无（满 SQ 由返回值 false 表达）
- **Side effects**:
  - 成功:入队 `req` 到内部 vector,返回 `true`
  - 失败(SQ 满):不入队,返回 `false`(host 端 IOCTL 返回 -EBUSY)
- **并发保证**: 由调用方保证 host 端 enqueue 串行

#### pending_count() const

- **Postcondition**: 返回当前 SQ 深度
- **线程安全**: device 端 tick 与 host 端 enqueue 并发可见（per `std::atomic<size_t>` 语义）

#### tick()

- **Side effects**:
  - 若 SQ 非空 + dispatch_handler 已注册 → 调 handler 消费一个 entry
  - 返回 `true` 表示消费了一个 entry，`false` 表示 SQ 空或 handler 未注册
- **错误**: dispatch_handler 返回非 0 → 记录到 CompletionRing 状态字段（不抛）

#### set_dispatch_handler(handler)

- **Precondition**: 无
- **Postcondition**: 后续 `tick()` 调 handler

### 4.3 错误处理

| 错误源 | 处理 |
|---|---|
| SQ 满 | host 端 IOCTL 返回 -EBUSY（CppTLM 不抛）|
| dispatch_handler 失败 | CompletionRing entry status 记录错误 |

### 4.4 测试要求

- ✅ `enqueue()` 后 `pending_count()` = 1
- ✅ `tick()` 后 `pending_count()` = 0
- ✅ `tick()` 触发 dispatch_handler 调用，参数 = 入队的 req
- ✅ SQ 满时 enqueue 返回 false（不抛）
- ✅ dispatch_handler 返回非 0 时 CompletionRing entry status 正确

---

## 5. 集成契约 (Integration Contracts)

### 5.1 Host 端流程

```cpp
// UsrLinuxEmu + TaskRunner 端（不在 CppTLM scope）
int cuLaunchKernel(CUfunction func, dim3 grid, dim3 block, ...) {
    KernelLaunchRequest req{ .handle=..., .grid=grid, .block=block, ... };
    if (!sq->enqueue(req)) return CUDA_ERROR_UNKNOWN;  // SQ 满
    doorbell->ring(stream_id, sq->pending_count());   // 触发 consumer
    return CUDA_SUCCESS;
}
```

### 5.2 Device 端流程

```cpp
// CppTLM 端 (P2 SmExecutorImpl::tick)
void SmExecutorImpl::tick() {
    if (sq_->tick()) {  // 消费一个 entry
        // dispatch_handler 已注册为 ISmExecutor::dispatch
    }
}
```

---

## 6. 性能特征 (Performance Characteristics)

| 操作 | 时间复杂度 | 备注 |
|---|---|---|
| DGpuBar::init() | O(VRAM size) | VRAM mmap 一次性 |
| DGpuBar::read_reg | O(1) | 4-byte aligned 数组访问 |
| Doorbell::ring | O(1) | atomic store + 异步 notify |
| SQ::enqueue | O(1) amortized | vector push_back |
| SQ::tick | O(1) | 单 entry 处理 |

---

**维护**: CppTLM Team
**状态**: 📋 Spec — W1-3 实施，W4-6 接口冻结
**下次更新**: W1 骨架完成后 / W4 接口冻结时
