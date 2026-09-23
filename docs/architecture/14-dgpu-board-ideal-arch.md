# DGpuBoard 理想架构 (v2.0)

> **版本**: 2.0
> **日期**: 2027-02-09
> **状态**: 📋 提案 (基于 v1.0 审查报告重构)
> **前置**: `DGpuBoard v1.0 设计审查报告` (W24 末, 44998 assertions 全绿但含 11 项已知缺陷)
> **配套 ADR**: ADR-DGPU-01 ~ ADR-DGPU-04
> **Owner**: CppTLM Team
> **影响**: `include/tlm/gpu/dgpu_board_shell.{hh,cc}` + `include/tlm/gpu/dgpu_soc.{hh,cc}` + 5 个新内部组件

---

## 1. 设计目标

### 1.1 背景

DGpuBoard v1.0 (Phase 1-8 完整交付后版本, 44998 assertions 全绿) 在审查中发现 **2 项高严重性问题** (detached 线程 + timer 析构顺序) 与 **9 项中严重性问题** (数据路径 stub、硬编码索引、双重锁、命名空间不一致等)。本架构文档提出 v2.0 理想方案,系统性解决这些问题。

### 1.2 设计目标

| 目标 | 度量 | 优先级 |
|------|------|--------|
| **G1**: 消除 host 端 use-after-free 风险 | `trigger_*_async` 不再开 detached 线程, 改为可 join worker | P0 |
| **G2**: 析构顺序协议化 | 引入 `LifecycleProtocol` 状态机, 不再依赖注释约定 | P0 |
| **G3**: PCIe 路径 dispatch 数据驱动 | `DispatchRegistry` 替代 4 态硬编码 switch | P0 |
| **G4**: 命名空间统一 | `DGpuSoc` 与 `DGpuBoard` 同居 `tlm::gpu::` | P1 |
| **G5**: 测试可访问性隔离 | friend class 模式替代 public 测试 accessor | P1 |
| **G6**: ABI 兼容性 100% | 23 ABI 函数签名 0 修改 | 硬约束 |

### 1.3 非目标

- **NG1**: 不重构 PcieEndpointIP 内部 (v1.0 已稳定)
- **NG2**: 不引入新线程模型框架 (沿用 std::thread)
- **NG3**: 不改变 SOC JSON schema (`dgpu_board_v1.json` 兼容)
- **NG4**: 不引入第三方依赖 (仅 std + nlohmann)

### 1.4 已知推迟项 (v2.1 backlog, per Oracle 二轮复审 §C)

下列 driver 仿真场景**明确推迟到 v2.1**, 本轮 v2.0.2 不实施 (per Oracle 二轮报告: 无追踪锚点会被遗忘, 故在此登记):

| 项 | driver 需求 | 推迟理由 | 追踪 |
|----|-----------|---------|------|
| **MSI-X PBA 读路径** | 中断 handler 读 Pending Bit Array | `msix_update_pending` 已有, PBA 经 BAR MMIO 读回路径未实现 | v2.1 |
| **FLR / 热复位** | 测试 reset-while-DMA-in-flight、错误恢复 | LifecycleProtocol 仅 "一次性 teardown", 无 device-level reset 概念 | v2.1 |
| **BAR sizing probe** | probe 时向 BAR 写全 1 读回 mask | 走 config 空间路径, 与本设计 (MMIO dispatch) 无关; 需 config space 模型扩展 | v2.1 |
| **多进程 / 多 handle** | 两 driver 进程各开一个 handle | 当前 ABI 假设单 handle; 需单例/多例约束设计 | v2.1 |
| **D0/D3 电源状态** | runtime PM 测试 | `mmio_gated()` (D3hot gating) 已有, 但无 `set_power_state` ABI 表面 | v2.1 |
| **Config space 原子性** | 4-byte config 读写原子性与延迟模型 | Inv-4 表只给 guard, 未给语义 | v2.1 |
| **IOMMU/ATC 失效通道** | DMA remap + invalidate | 当前仅一次性 translate 查询; v2.0.2 先补返回值回路 (ABI 24) | v2.1 |

### 1.5 P1/P2 文档改进 backlog (per Oracle 二轮复审 §E)

Oracle 二轮列出的 P1/P2 项, 本轮 v2.0.2 仅修 P0, 下列进 backlog:

| 优先级 | 项 | 说明 | 计划 |
|--------|----|----|------|
| **P1** | 统一 metrics 落点 | `irq_dropped_total_` / `unmapped_mmio_count` 归属 `DGpuBoard::metrics_` struct, 决定是否接入 `include/metrics/` 框架 | v2.0.3 |
| **P1** | ABI 冻结语义裁决 | 架构 §5.1/§8.1 DoD "git diff 必须为空" vs "末尾追加 24 号 ABI" 矛盾 — **本轮 P0-3e 已裁决** ✅ | 已完成 |
| **P1** | `load_soc_config` 返回值对齐 | ADR-DGPU-02 写"返 -EINVAL"但签名是 `bool` — 需改为返 false + `last_error` 或改签名 | v2.0.3 |
| **P2** | v2.1 推迟清单登记 | PBA/FLR/BAR probe/multi-process/PM — **本轮 §1.4 已登记** ✅ | 已完成 |
| **P2** | ABI 24 测试用例 | poll/wait/cancel 的 timeout / cancel / cb 未注册 用例 — 需加入 Phase C 测试清单 | Phase C |
| **P2** | callback 重入契约 | "callback 内禁止重入 close/destroy" (避免 `clear_callbacks` 自死锁) | v2.0.3 |
| **P2** | 版本号 bump | 架构文档 header/footer "v2.0" → "v2.0.2"; 新增 delta summary | v2.0.3 |
| **P2** | 文档债合并 | 60+ 修订标记散布 5 份文档, 需 "v2.0.2 delta summary" 合并 | v2.0.3 |

---

## 2. 5 层架构 (v2.0)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Layer 0: Host (UsrLinuxEmu + Driver stub)                                   │
│  · cpptlm_emulator_mmio_read/write (23 ABI, 字节级冻结)                     │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │ extern "C" (23 ABI)
┌──────────────────────────────────▼──────────────────────────────────────────┐
│ Layer 1: DGpuBoard Shell (v2.0) — 唯一耦合点, NOT a SimModule              │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────┐         │
│  │ LifecycleProtocol (5 态状态机)                                  │         │
│  │   Constructed → Configured → Initialized → Running → ShuttingDown │       │
│  │ · 显式 transition(), 不依赖注释约定                            │         │
│  │ · 每个 ABI 调用校验 current_state (e.g. mmio_read 需 ≥Configured)│         │
│  └────────────────────────────────────────────────────────────────┘         │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────┐         │
│  │ CallbackWorker (单线程 task queue) ← 替代 detached 线程 (H1)   │         │
│  │   · IRQ / DMA-translate / Error 三类回调走同一 worker          │         │
│  │   · Joinable 线程, shutdown 时通过 stop_+drain+join 协议关闭    │         │
│  │   · 零 detached 线程, 零 use-after-free 风险                  │         │
│  └────────────────────────────────────────────────────────────────┘         │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────┐         │
│  │ DispatchRegistry (data-driven) ← 替代 4 态 switch (M5)         │         │
│  │   · (bar, offset_range) → handler 注册表                       │         │
│  │   · 默认: Legacy / AxiBypass / Tlp / Mock 4 handler            │         │
│  │   · 扩展点: PcieDisplayDevice BAR 0 + SdmaEngineTLM BAR1 door- │         │
│  │     bell + msix_init forwarding 全部走统一注册表              │         │
│  └────────────────────────────────────────────────────────────────┘         │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────┐         │
│  │ EpCache (lazy ep 指针缓存) ← 避免重复 dynamic_cast (L8)        │         │
│  │   · 首次 pcie_ep() 调用时缓存, 后续直接返回                    │         │
│  │   · 一次性 resolve (per Oracle-3 v2.0.1), 整生命周期不再失效    │         │
│  │   · invalidate() [[deprecated]], 仅 shutdown clear() (P0-2e)  │         │
│  └────────────────────────────────────────────────────────────────┘         │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────┐         │
│  │ PendingReqGuard (RAII 清理) ← 替代 mmio_read 双重锁 (M2)       │         │
│  │   · 构造: 登记 pending_resp_ + pending_data_                    │         │
│  │   · 析构: 一次性清理 (无论 success/timeout/error)             │         │
│  └────────────────────────────────────────────────────────────────┘         │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │ soc_ = unique_ptr<DGpuSoc>
┌──────────────────────────────────▼──────────────────────────────────────────┐
│ Layer 2: DGpuSoc (SimModule 容器, v2.0 命名空间统一)                       │
│  · `tlm::gpu::DGpuSoc` (从 cpptlm::tlm 迁出)                                │
│  · 持有 8 个子模块 (PcieEndpointIP + SdmaEngineTLM + ...)                   │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
┌──────────────────────────────────▼──────────────────────────────────────────┐
│ Layer 3: PcieEndpointIP (17-port SimModule, v1.0 不变)                       │
│  · PcieSriovVfPool + CompletionTracker + bar_store_ (三维 key)              │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
┌──────────────────────────────────▼──────────────────────────────────────────┐
│ Layer 4: PCIe Devices + GPU (PcieDisplayDevice + SdmaEngineTLM + GpuCluster) │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 关键组件设计

### 3.1 LifecycleProtocol (生命周期状态机)

**问题** (v1.0): 当前 API 调用顺序契约仅靠注释 (`dgpu_board_shell.hh:102-104`), 调用方误用会 silently 失败或崩。

**方案**: 引入显式 5 态状态机:

```cpp
enum class BoardState : uint8_t {
    Constructed   = 0,  // ctor 完成
    Configured    = 1,  // load_soc_config 成功
    Initialized   = 2,  // init() 完成 (sim_thread 已起)
    Running       = 3,  // tick() 正常推进
    ShuttingDown  = 4,  // shutdown() 进入 (拒新请求)
    Destructed    = 5,  // ~DGpuBoard 完成
};

// 转换规则 (transition guard):
//   Constructed → Configured     (load_soc_config)
//   Configured  → Initialized    (init)
//   Initialized → Running        (首次 tick)
//   *           → ShuttingDown   (shutdown/destroy)
//   ShuttingDown → Destructed    (~DGpuBoard 析构)
//
// 拒入规则 (对齐 ADR-DGPU-02 §3 Inv-4 ABI guard 表, 含上下限):
//   mmio_read/write/backdoor_*/config_*: 需 ∈ [Configured, Running]
//                                          (下限: 未配置返 -ENOSYS, 上限: ShuttingDown 返 -ESHUTDOWN)
//   msix_*/lookup_register:              需 ∈ [Configured, Running]
//   set_*_callback:                      需 ∈ [Configured, Running] (≤ ShuttingDown)
//   trigger_*_async:                     需 ∈ [Initialized, Running]
//   tick:                                需 ∈ [Initialized, ShuttingDown] (无 -ESHUTDOWN 检查)
//   shutdown/destroy:                    任何非 Destructed 状态
```

**测试策略**: `BoardStateTest` 验证 16 个 transition (4 状态 × 4 入口) 的合法/非法组合。

### 3.2 CallbackWorker (替代 detached 线程)

**问题** (v1.0 H1): `trigger_*_async` 每次开 detached 线程 (`dgpu_board_shell.cc:731-737, 748-753, 764-769`):
- 大量线程创建/销毁开销 (尤其 MSI-X 突发场景)
- host callback 内部可能访问已被析构对象 (use-after-free)
- 无法 join, destroy 协议不完整

**方案**: 单线程 worker + task queue (类比 `coalesce_timer_` 但更通用):

```cpp
// 内部组件, 仅 DGpuBoard 使用
class DGpuBoard::CallbackWorker {
public:
    enum class Kind : uint8_t { Irq, DmaTranslate, Error };

    struct Task {
        Kind kind;
        uint32_t vector_id;       // IRQ
        uint64_t iova;            // DMA
        size_t   size;            // DMA
        int      err_code;        // Error
        std::string msg;          // Error
    };

    void start();                  // 创建 worker_thread_
    void submit(Task t);           // 推入 queue_, notify cv_
    void shutdown_and_join();      // stop_=true → drain → join (确定性析构)
    size_t pending() const;        // 测试断言

private:
    void worker_loop();            // while (!stop_) pop task → dispatch to callback_

    std::thread                  worker_thread_;
    std::mutex                  mu_;
    std::condition_variable     cv_;
    std::deque<Task>            queue_;
    std::atomic<bool>           stop_{false};
    std::atomic<bool>           draining_{false};  // shutdown 进入, 拒新 submit

    // 回调指针 (持有 callback_mu_, 同 v1.0 但加 stop 校验)
    DGpuBoard::IrqCallback          irq_cb_;
    DGpuBoard::DmaTranslateCallback dma_cb_;
    DGpuBoard::ErrorCallback        err_cb_;
    std::mutex                      callback_mu_;
};
```

**关键不变性** (对齐 ADR-DGPU-01 §3 Inv-1~Inv-4):
1. `submit()` 在 `stop_ || draining_` 时返回 false, 或 `queue_.size() >= kMaxQueueSize` 时返回 false (避免 submit 阶段 race, 背压保护)
2. `worker_loop()` 中调用 callback 前再校验 stop_ 标志 (防止 in-flight task 在锁外 dispatch 时访问已 shutdown 资源)
3. `shutdown_and_join()` 严格顺序: `draining_=true` → `stop_=true` + `cv_.notify_all()` → join worker (确定性析构, 调用方可在返回后安全释放 callback 指针)
4. `dispatch_task()` 用 `try { ... } catch (...) {}` 隔离 host 异常, 不影响 worker 处理后续任务 (per ADR-DGPU-01 §3 Inv-4, 仅 ADR 展开)

**测试策略**:
- `CallbackWorkerTest`: submit 1000 task, shutdown_and_join 后 pending=0
- `CallbackWorkerLifecycleTest`: shutdown 后 submit 返 false
- `CallbackWorkerUseAfterFreeTest`: shutdown 后 callback 不再被调 (mock 验证)
- `CallbackWorkerBackpressureTest`: 队列满 (≥1024) 时 submit 返 false (per Inv-1 背压保护)
- **性能对比**: burst 100 个 IRQ 比 v1.0 detached 路径快 10x+

### 3.3 DispatchRegistry (data-driven dispatch)

**问题** (v1.0 M5): `dispatch_mmio_to_pcie` 4 态 switch 都是空 break (`dgpu_board_shell.cc:775-800`),spec 名实不符。T-P12-2 E2E 测试必失败。

**方案**: data-driven DispatchRegistry (类比 `PcieBarRouter`, **per Oracle v2.0.2 P0-1a 拆 read/write 双签名**):

```cpp
// 内部组件
// per Oracle v2.0.2 P0-1: HandlerFn 拆分为 read_handler / write_handler,
// 原因: read 路径需要把读出数据写回 caller 的 buf, write 路径只需 const data;
//      旧统一签名 HandlerFn(uint8_t, uint64_t, const void*, size_t) data 是 const,
//      handler 无法返 buf 内容, mmio_read 只能依赖镜像回读, 真实读路径无数据回路
struct DispatchEntry {
    enum class Scope : uint8_t { BarExact, BarRange, Global };
    enum class Kind : uint8_t { Read, Write };   // per Oracle-P0-1a: 区分读写
    Scope      scope;
    Kind       kind;        // per Oracle-P0-1a: 区分 read/write, handler 不可混用
    uint8_t    bar;         // BarExact / BarRange
    uint64_t   offset_lo;   // BarRange
    uint64_t   offset_hi;   // BarRange
    uint64_t   offset;      // BarExact
    int        priority;    // 高优先级先匹配

    // per Oracle-P0-1a: 双 handler, write 是 const data, read 带 out_buf
    using ReadHandler  = std::function<int(uint8_t, uint64_t, void* out_buf, size_t len)>;
    using WriteHandler = std::function<int(uint8_t, uint64_t, const void* data, size_t len)>;
    ReadHandler   read_handler;    // kind==Read 时使用, 必须填写 *out_buf
    WriteHandler  write_handler;   // kind==Write 时使用

    std::string name;       // 诊断
};

class DGpuBoard::DispatchRegistry {
public:
    // 注册 handler (按 Kind 区分, 不可混用)
    void register_read_handler(const DispatchEntry& e);    // e.kind == Read
    void register_write_handler(const DispatchEntry& e);   // e.kind == Write

    // 便利 API (D1 / SDMA / PCIe path)
    // per Oracle-P0-1a: display_routing 仅写 (mmio_write); BAR1 doorbell 仅写
    void register_bar0_display_routing(DispatchEntry::WriteHandler fn);   // D1 v1.1.1
    void register_bar1_doorbell(DispatchEntry::WriteHandler fn);          // Stage 1.3a
    void register_pcie_path_write(PciePath path, DispatchEntry::WriteHandler fn);  // T-P12-1
    void register_pcie_path_read(PciePath path, DispatchEntry::ReadHandler fn);   // T-P12-1 (新增)

    // per Oracle-P0-1a: 拆分为 dispatch_read / dispatch_write
    // 读路径: 找 Read entry, 调用 read_handler(out_buf) 写入数据
    int dispatch_read(uint8_t bar, uint64_t offset, void* out_buf, size_t len) const;
    // 写路径: 找 Write entry, 调用 write_handler(data) (旧 dispatch 接口)
    int dispatch_write(uint8_t bar, uint64_t offset, const void* data, size_t len) const;

    // 测试 accessors
    size_t entry_count() const;
    size_t read_entry_count() const;   // 仅统计 Read entries
    size_t write_entry_count() const;  // 仅统计 Write entries
    std::vector<std::string> matched_read_handlers(uint8_t bar, uint64_t offset) const;
    std::vector<std::string> matched_write_handlers(uint8_t bar, uint64_t offset) const;

private:
    std::vector<DispatchEntry> entries_;  // 按 priority 排好序
    // 索引加速查找 (避免每次 dispatch 扫描全表)
    std::unordered_multimap<uint8_t, size_t> read_entries_by_bar_;
    std::unordered_multimap<uint8_t, size_t> write_entries_by_bar_;
};
```

**注册顺序** (在 `init()` 后, per Oracle-P0-1a 拆 read/write):

```cpp
// 默认 Legacy 行为 (向后兼容, write 路径 no-op)
registry_.register_pcie_path_write(PciePath::Legacy,
    [](uint8_t, uint64_t, const void*, size_t) { return 0; });

// PcieDisplayDevice BAR 0 fast-path (D1 v1.1.1, display_routing_enabled)
// write 路径: 写 display device
registry_.register_bar0_display_routing(
    [this](uint8_t, uint64_t off, const void* d, size_t len) {
        auto* dev = ep_cache_->get()->display_device();
        return dev.mmio_write(off, d, len);
    });
// per Oracle-P0-1a: read 路径 (v2.0.2 新增, 修复 v2.0.1 "读只能镜像回读" 空洞)
registry_.register_read_handler(
    {DispatchEntry::Scope::BarRange, DispatchEntry::Kind::Read,
     /*bar=*/0, /*lo=*/0x00, /*hi=*/0xFFF, /*offset=*/0, /*priority=*/100,
     /*read_handler=*/[this](uint8_t, uint64_t off, void* out_buf, size_t len) {
         auto* dev = ep_cache_->get()->display_device();
         return dev.mmio_read(off, out_buf, len);
     }, /*write_handler=*/nullptr, "PcieDisplayDevice BAR0 read"});

// SdmaEngineTLM BAR1+0x10010000 doorbell (Stage 1.3a, 仅 write)
registry_.register_bar1_doorbell(
    [this](uint8_t, uint64_t, const void* d, size_t len) {
        if (sdma_engine_) {
            uint64_t wptr = detail::parse_wptr_from_host(d, len);
            sdma_engine_->mmio_write(/*bar=*/1, kBar1DoorbellOffset, wptr);
        }
        return 0;
    });

// PciePath::Tlp (T-P12-2, 待实施, write + read 双注册)
registry_.register_pcie_path_write(PciePath::Tlp,
    [this](uint8_t bar, uint64_t off, const void* d, size_t len) {
        return pcie_path_tlp_write(bar, off, d, len);
    });
registry_.register_pcie_path_read(PciePath::Tlp,
    [this](uint8_t bar, uint64_t off, void* out_buf, size_t len) {
        return pcie_path_tlp_read(bar, off, out_buf, len);  // per Oracle-P0-1a
    });
```

**读路径说明 (per Oracle v2.0.2 P0-1)**:
- v2.0.1 的 `mmio_read` 因 HandlerFn 签名限制 (`const void* data`), 无法让 handler 写回数据, 只能落 `mmio_regs_` 镜像 — 这是设计空洞
- v2.0.2 拆 read/write 双签名后, `dispatch_read(bar, offset, out_buf, len)` 调用 `read_handler(bar, offset, out_buf, len)`, handler 通过 `out_buf` 写回读出数据
- `mmio_regs_` 镜像降级为"write-then-read roundtrip 的调试旁路", 不再是唯一读路径

**优点**:
- ✅ 单一注册表, 避免 switch case 持续膨胀
- ✅ 可扩展 (新 device/新路径只需 register, 无需改 switch)
- ✅ 测试可注入 mock handler 验证 dispatch 顺序
- ✅ 与 PcieBarRouter 风格一致 (data-driven + priority)

### 3.4 EpCache (lazy ep 指针缓存)

**问题** (v1.0 L8): `pcie_ep()` 每次 `dynamic_cast<PcieEndpointIP*>` (11 处调用, `dgpu_board_shell.cc:247, 321, 387, 408, 421, 461, 501, 520, 539, 554, 571`)。

**方案**: lazy cache + 失效钩子:

```cpp
class DGpuBoard::EpCache {
public:
    // 返回 cached 指针 (init 后锁定)
    tlm::pcie::PcieEndpointIP* get();

    // 显式失效 (per Oracle-3 v2.0.1 + Oracle v2.0.2 P0-2e: deprecated)
    // v2.0 起 board 一次性使用, init() 后整生命周期不再失效 (除 shutdown clear())
    // 保留仅为测试 hook; 生产路径不再调用
    [[deprecated("EpCache::invalidate() deprecated in v2.0.1; use clear() in shutdown path")]]
    void invalidate() noexcept;

    // per Oracle v2.0.2 P0-2e: shutdown 时清空 cached_ (析构阶段)
    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        cached_ = nullptr;
        resolved_.store(false);
    }

private:
    std::mutex                  mu_;
    std::atomic<bool>           resolved_{false};
    tlm::pcie::PcieEndpointIP*  cached_ = nullptr;

    // 首次 resolve 时动态 cast, 之后直接返回
    tlm::pcie::PcieEndpointIP* resolve();
};
```

**关键不变性 (per Oracle-3 v2.0.1 + Oracle v2.0.2 P0-2e 修订)**:
1. `cached_` 仅在 `init()` 完成后设置 (board 一次性使用, 不再有 load_soc_config 重复调用)
2. `invalidate()` 已 `[[deprecated]]` — 生产路径不再调用 (per Oracle-3: 无 reverse transition)
3. `get()` 自身线程安全 (cache 命中走 atomic fast path)
4. `clear()` 在 `destroy()` 的 SOC reset 之前调用 (析构阶段清理)

### 3.5 PendingReqGuard (RAII 清理)

**问题** (v1.0 M2): `mmio_read` 在 self-drain 后再次 lock `inject_mu_` 清理 `pending_data_` + `pending_resp_` (`dgpu_board_shell.cc:288-296`),意图不清晰。

**方案**: RAII 包装:

```cpp
class DGpuBoard::PendingReqGuard {
public:
    PendingReqGuard(DGpuBoard* board, uint64_t trans_id);
    ~PendingReqGuard();

    // 一次性提取响应数据 (从 board 的 pending_data_)
    bool extract_payload(std::vector<uint8_t>& out);

    // 标记 timeout (防止 guard 析构时误清理仍在等待的 future)
    void mark_timeout();

private:
    DGpuBoard*               board_;
    uint64_t                trans_id_;
    bool                    timed_out_ = false;
};
```

**关键不变性**:
1. 析构时根据 `timed_out_` 决定清理行为:
   - 未 timeout: 正常 cleanup (erase pending_resp_ + pending_data_)
   - 已 timeout: 仅清理 pending_data_ (pending_resp_ 已在 timeout 路径 erase)
2. 异常路径自动析构 (no leak)

---

## 4. 与 v1.0 的对比矩阵

| 维度 | v1.0 (现状) | v2.0 (理想) | 改进 |
|------|------------|------------|------|
| **线程模型** | detached 线程 + coalesce_timer + sim_thread (3 类线程) | joinable worker + coalesce_timer + sim_thread (3 类线程, 全部 joinable) | H1 修复 |
| **析构顺序** | 注释约定 + try/join 各自负责 | `LifecycleProtocol` 状态机强制 | H2 修复 |
| **PCIe 路径 dispatch** | 4 态 hardcoded switch + 空 break | `DispatchRegistry` data-driven + priority | M5 修复 |
| **ep 指针访问** | 11 处 dynamic_cast | `EpCache` 首次 resolve 后零开销 | L8 优化 |
| **mmio_read 锁** | 双重锁 + 意图不清 | `PendingReqGuard` RAII 一次性清理 | M2 修复 |
| **命名空间** | `cpptlm::tlm::DGpuSoc` vs `tlm::gpu::DGpuBoard` | 统一 `tlm::gpu::DGpuSoc` | G4 实现 |
| **测试 accessor 隔离** | public `bar_store_value` | friend class `DGpuBoardTestPeer` | M7 修复 |
| **ABI 兼容性** | 23 ABI 冻结 | 23 ABI 冻结 (0 修改) | G6 硬约束 ✅ |
| **新增 JSON 表** | 无 | 无 | NG3 满足 ✅ |
| **新增依赖** | std + nlohmann | std + nlohmann | NG4 满足 ✅ |

---

## 5. 兼容性约束

### 5.1 ABI 兼容性 (硬约束, per Oracle v2.0.2 P0-3e DoD 仲裁)

**23 ABI 函数签名零修改** (字节级冻结, per ADR-088 §D5):

```cpp
// 23 ABI 函数 (签名级字节级兼容)
extern "C" {
    int cpptlm_emulator_open(uint32_t dev_id, cpptlm_emulator_handle_t** handle);
    void cpptlm_emulator_close(cpptlm_emulator_handle_t* handle);
    // ... 其余 21 个 (mmio_read/write, config_read/write, msix_*, lookup_register, etc.)
}

// 24 号 ABI (v2.0.2 新增, per Oracle-P0-3, 不属于 23 ABI 冻结范围)
// 末尾追加于 include/abi/cpptlm_emulator.h, 不修改既有 23 签名
extern "C" {
    int cpptlm_emulator_dma_translate_poll(cpptlm_emulator_handle_t* handle,
                                            uint64_t iova, uint64_t* out_paddr);
    int cpptlm_emulator_dma_translate_wait(cpptlm_emulator_handle_t* handle,
                                            uint64_t iova, uint64_t* out_paddr,
                                            uint32_t timeout_ms);
    int cpptlm_emulator_dma_translate_cancel(cpptlm_emulator_handle_t* handle,
                                              uint64_t iova);
    // 详见 ADR-DGPU-01 §5.4 (v2.0.2 完整规格)
}
```

**验证** (per Oracle v2.0.2 P0-3e):

```bash
# 1. 既有 23 ABI 签名 0 diff (签名级冻结)
git diff HEAD -- include/abi/cpptlm_emulator.h | grep -E '^[+-]' \
    | grep -v '^+++ \|^--- ' \
    | grep -v 'cpptlm_emulator_dma_translate_poll\|_wait\|_cancel' \
    | head
# 必须为空 (排除 hunk 头与 24 号 ABI 行, 检查无其他改动)

# 2. 23 ABI 函数签名 0 diff (二进制级兼容, 更严格)
git diff HEAD -- include/abi/cpptlm_emulator.h | grep -E '^[+-]' \
    | grep -E '^[-+]int cpptlm_emulator_(open|close|mmio|config|msix|lookup|pcie_config)' \
    | head
# 必须为空
```

**DoD 仲裁 (Oracle v2.0.2 P0-3e)**: "23 ABI 字节级冻结" 解释为**签名级冻结**而非文件级冻结;允许末尾追加 24 号 ABI, 不视为破坏 ABI 兼容。验证脚本见上, §8.1 DoD 检查项同步更新。

### 5.2 JSON 配置兼容性

`configs/dgpu_board_v1.json` 不修改,新增字段 (`display_routing_enabled` 等) 已存在,无需 schema 扩展。

### 5.3 测试兼容性

- 现有 44498 assertions 100% 通过 (Phase 1-8 baseline)
- 新增测试覆盖新组件 (LifecycleProtocol, CallbackWorker, DispatchRegistry, EpCache, PendingReqGuard)
- D1 v1.1.1 39 case + 基础 20 case + vblank 7 case + abi-shell routing 3 case = 69 case 保持 PASS

---

## 6. 实施路径 (4 阶段)

### Phase A: 基础设施 (W25, 1 周)

| 任务 | 文件 | ADR |
|------|------|-----|
| A1: 实现 `LifecycleProtocol` 状态机 | `dgpu_board_lifecycle.hh` | ADR-DGPU-02 |
| A2: 实现 `CallbackWorker` 准备替换 detached 线程 | `dgpu_board_callback_worker.hh` | ADR-DGPU-01 |
| A3: 实现 `EpCache` lazy cache | `dgpu_board_ep_cache.hh` | ADR-DGPU-04 |
| A4: 实现 `PendingReqGuard` RAII | `dgpu_board_pending_req_guard.hh` | (本架构 §3.5) |

### Phase B: 核心重构 (W26, 1 周)

| 任务 | 文件 | ADR |
|------|------|-----|
| B1: 重构 `dgpu_board_shell.cc` 使用新组件 | `dgpu_board_shell.cc` | ADR-DGPU-01/02/04 |
| B2: 实现 `DispatchRegistry` + 4 路径 handler | `dgpu_board_dispatch_registry.hh` | ADR-DGPU-03 |
| B3: 迁移 `cpptlm::tlm::DGpuSoc` → `tlm::gpu::DGpuSoc` | `dgpu_soc.hh` | ADR-DGPU-04 |

### Phase C: 测试覆盖 (W26, 1 周)

| 任务 | 文件 |
|------|------|
| C1: `test_dgpu_board_lifecycle.cc` (16 transition) |
| C2: `test_dgpu_board_callback_worker.cc` (10 case, 含 use-after-free) |
| C3: `test_dgpu_board_dispatch_registry.cc` (12 case, 含 priority 排序) |
| C4: `test_dgpu_board_ep_cache.cc` (6 case) |
| C5: `test_dgpu_board_pending_req_guard.cc` (8 case) |
| C6: `test_dgpu_soc_namespace_migration.cc` (4 case) |

### Phase D: 文档同步 + ADR 归档 (W27)

| 任务 |
|------|
| D1: 更新 `AGENTS.md` STRUCTURE 节 |
| D2: 更新 `docs/adr/README.md` 列出 ADR-DGPU-01~04 |
| D3: 更新 `docs/ONBOARDING.md` §5.5 脚本表 |
| D4: 运行 `scripts/test/docs_sync_check.sh --strict` 必须 PASS |

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: CallbackWorker shutdown 时 in-flight callback 持有 board 引用 | 中 | use-after-free | worker_loop 调用 callback 前再校验 stop_ (per §3.2 不变性 3) |
| **R2**: DispatchRegistry 注册顺序错导致 D1 路由失效 | 中 | D1 测试 FAIL | 注册顺序在 init() 中序列化, 单元测试覆盖 priority 排序 |
| **R3**: DGpuSoc 命名空间迁移破坏下游 `using` 指令 | 中 | 编译失败 | deprecation period (v2.0 警告, v3.0 删除); CI 扫 `cpptlm::tlm::DGpuSoc` 引用 |
| **R4**: 44998 assertions 回归 | 低 | 测试全红 | Phase B 增量迁移 + Phase C 全量回归 + Oracle 评审 |
| **R5**: T-P12-2 真实数据路径接线延期 | 中 | DispatchRegistry 部分 handler 空 break | 保留 v1.0 stub 行为, 标 TODO 跟随 T-P12-2 |

### 7.1 Oracle 评审重点关注 (per 评审报告 R-Oracle, 2027-02-09)

下列 4 个安全/正确性关键点是 Oracle 评审 P0 门禁 (per ADR-0025) 的必查项,Phase A 实施完成后必须由 Oracle 复审确认:

#### 🔴 Oracle-1: callback nullification 与 in-flight task race window

**问题**: DGpuBoard::destroy() 中 Step 1 (callback nullification) → Step 2 (worker_.shutdown_and_join()) 顺序, 是否真能避免 in-flight task 访问已置 null 的 callback?

**理论保证** (per ADR-DGPU-02 §3 Inv-2 + Inv-3):
- Inv-2: `worker_loop()` 中调用 callback 前再校验 `stop_`
- Inv-3: dispatch 时 `lock(callback_mu_)` + 指针拷贝 (`auto cb = irq_cb_`)

**Oracle 评审输入**:
- [ ] `test_dgpu_board_callback_worker_use_after_free.cc` (新增 3 case): shutdown 路径下的 callback 调用计数
- [ ] `static FILE* trace = fopen("/tmp/cpptlm_destroy_race.log", "a")` 压测 10000 次 destroy+init 循环, 验证 zero UAF
- [ ] TSan 报告 (per `scripts/build/run_tsan.sh`) 必须 0 race

**回退方案**: 如 Inv-2/Inv-3 不足, 引入第四重保险: `worker_loop` dispatch 前对 `callback_mu_` 加读锁, 与 destroy Step 1 写锁互斥 (读写锁方案)。

---

#### 🔴 Oracle-2: EpCache 一次性 resolve 协议 (per Oracle-3 v2.0.1 + Oracle v2.0.2 P0-2e, 原 "失效协议 race" 已废止)

**原问题** (v2.0.1): ADR-DGPU-04 §3 Inv-3 中 `load_soc_config()` 与 `init()` 都调 `ep_cache_.invalidate()`, 是否会出现两个调用方竞争 invalidate 本身的锁?

**v2.0.2 决议 (Oracle-P0-2e)**: 该协议**已废止** — ADR-DGPU-02 状态机无 reverse transition, board 一次性使用, `load_soc_config` 只能调一次, `invalidate()` 已 `[[deprecated]]`。因此:
- 原并发 invalidate + get 的 TSAN 验证用例**不再需要** (无生产调用方)
- 新增验证: `test_dgpu_board_ep_cache_shutdown_clear.cc` (2 case): 验证 destroy 路径 `ep_cache_.clear()` 后 get() 返 nullptr, 且 resolved_=false
- `EpCache::invalidate()` 虽 deprecated, 保留作测试 hook; CI 用 `-Wno-deprecated-declarations` 抑制警告 (per ADR-DGPU-04 §7 R2)

**回退方案**: 若未来 v2.1 引入 in-place reconfigure, 恢复 invalidate 协议并重新评估 race (届时引入 RCU 风格 deferred-free)。

---

#### 🟡 Oracle-3: DispatchRegistry priority 排序性能

**问题**: `register_*_handler()` 每次触发 `std::stable_sort(entries_.begin(), entries_.end(), priority 降序)`, O(n log n)。如高频 register 场景下(init 多次, 或测试中反复 register), 累积开销是否可接受?

**理论分析**:
- `entries_` 通常 ≤ 10 项 (Legacy/AxiBypass/Tlp/Mock + D1 + BAR1 doorbell + 扩展), n log₂ n ≈ 33 比较
- 单次 register < 1μs (现代 CPU)
- init() 通常调一次, register 频率极低
- per Oracle-P0-1a: read/write 分别注册, 但**匹配时按 bar 索引过滤** (`read_entries_by_bar_` / `write_entries_by_bar_`), 实际扫描更少

**Oracle 评审输入**:
- [ ] 性能基准: 1000 次 register/destroy 循环总耗时 < 1ms (单核 Linux x86_64)
- [ ] 如不满足: 改为 `insert in priority order` (O(n) 但常数小), 或预排序

**回退方案**: 性能不达预期时, 改为 `std::map<int, std::vector<Entry>>` (priority → entries) 数据结构, register O(log k), dispatch O(log k) + O(matched_in_bucket)。

---

#### 🟡 Oracle-4: 23 ABI guard 引入 -ENOSYS/-ESHUTDOWN 后的兼容性

**问题**: ADR-DGPU-02 §6 声明 "23 ABI 语义 0 修改", 但 Inv-4 显式新增 -ENOSYS (未配置) / -ESHUTDOWN (关闭中) 返回值。现有 44498 assertions 是否假设了不同返回值?

**风险场景**:
- 测试假设 `mmio_read()` 在 SOC null 时返 0 (v1.0 隐式行为), 但 v2.0 返 -ENOSYS
- 测试假设多次 `shutdown()` 幂等, 但 v2.0 lifecycle CAS 失败返 -EINVAL

**Oracle 评审输入**:
- [ ] `grep -rn "mmio_read\|mmio_write" test/` 现有测试是否检查返回值范围 (any-negative vs specific)
- [ ] Phase B 增量迁移第一步: 在 init() 早期加 guard, 跑全量测试, 统计 fail 数
- [ ] 如有 fail: 区分 (a) 测试代码需更新 (合规) vs (b) guard 引入副作用 (需修)

**回退方案**: 如 fail 数 > 100, 考虑把 -ENOSYS 降级为 silent no-op (返 0), 但记录 metric (`board.metrics.unconfigured_abi_count++`)。

---

## 8. 度量与验证

### 8.1 完成定义 (DoD)

- [ ] 4 个新组件 (`LifecycleProtocol` / `CallbackWorker` / `DispatchRegistry` / `EpCache` + `PendingReqGuard`) 实现完成
- [ ] `dgpu_board_shell.cc` 重构完成, 移除所有 detached 线程
- [ ] `DGpuSoc` 命名空间迁移完成
- [ ] 新增 50 case (Lifecycle 16 + CallbackWorker 10 + DispatchRegistry 12 + EpCache 6 + PendingReqGuard 8 + NamespaceMigration 4) 100% PASS
- [ ] 现有 44498 assertions 100% PASS (0 regression)
- [ ] `openspec validate --changes --strict` PASS
- [ ] `git diff HEAD -- include/abi/cpptlm_emulator.h` **仅含末尾追加块** (per Oracle v2.0.2 P0-3e 仲裁: 23 ABI 签名 0 diff + 允许末尾追加 24 号 ABI; 验证脚本见 §5.1)
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS
- [ ] Oracle 评审通过 (per ADR-0025 P0 门禁)

### 8.2 性能目标 (per Oracle-5 v2.0.1, 指标重定义)

**修订原因**: v1.0 "MSI-X burst 100 IRQ 100ms → 5ms" 瞄准错误基准——真实 driver 不 burst IRQ,关注 (a) 单 IRQ 回调延迟 P99 (b) 无丢弃前提下最大持续 IRQ 率 (c) 内存占用。新指标对齐真实 driver 关注点 (per ADR-DGPU-01 §7 Oracle-5 修订)。

| 指标 | v1.0 baseline | v2.0 target | 测试方法 |
|------|---------------|-------------|---------|
| **单 IRQ 回调延迟 P99** | ~10us (detached 线程唤醒 + 上下文切换) | < 5us (queue 推入 + notify_one) | `perf_test_p99_irq_latency`: 触发 10000 IRQ, P99 延迟 |
| **无丢弃最大持续 IRQ 率** | ∞ (永不丢弃, 但 UAF 风险) | ≥ 10000 IRQ/s (worker drain 速率) | `perf_test_sustained_irq_rate`: 监控 `irq_dropped_total_` |
| **mmio_read 同步延迟 (p99)** | 50ms (timeout 上限) | < 1ms (新 dispatch) | 现有测试, 删除 mmio_regs_ 镜像路径 (per Oracle-4) |
| **DMA 翻译 poll 延迟 P99** (新增) | N/A (无回调回路) | < 10us (callback 完成 → host poll 立即可见) | per Oracle-2, ADR-DGPU-01 §5.4 |
| **BAR0 显示路由延迟** | < 5us | < 5us (持平) | 现有 D1 测试 |
| **多卡 (4 卡) 并发 mmio 吞吐** | 1000 ops/s | > 5000 ops/s | 现有 D1 v1.1.1 测试 |
| **未映射 BAR 延迟** (新增 per Oracle-4) | N/A (静默成功) | < 1us (立即返 -ENOSYS / 0xFF...FF) | `test_unmapped_bar_returns_error` |
| **背压丢弃观测性** (新增 per Oracle-5) | 无 (v1.0 永不丢弃) | `irq_dropped_total_` 计数 + ErrorCallback (可选) | `test_backpressure_drop_notification` |

### 8.3 安全审计

| 项 | 要求 |
|----|------|
| 零 detached 线程 | `grep -r "std::thread.*detach\(\)" include/tlm/gpu/dgpu_board_shell.cc` 必须为空 |
| 零动态 cast 热路径 | `grep -r "dynamic_cast" include/tlm/gpu/dgpu_board_shell.cc` 仅保留 init 时一次 |
| 23 ABI 字节级兼容 | git diff 必须为空 |
| 析构协议强制 | 所有 ABI 调用校验 LifecycleProtocol state |

---

## 9. 配套 ADR 索引

| ADR | 议题 | 状态 |
|-----|------|------|
| ADR-DGPU-01 | CallbackWorker 替代 detached 线程 | 📋 提案 |
| ADR-DGPU-02 | LifecycleProtocol 状态机 + 统一析构顺序 + callback nullification 协议 | 📋 提案 |
| ADR-DGPU-03 | DispatchRegistry data-driven 替代 4 态 switch | 📋 提案 |
| ADR-DGPU-04 | DGpuSoc 命名空间统一 + EpCache lazy cache | 📋 提案 |

---

## 10. 参考

- **DGpuBoard v1.0 设计审查报告** (W24 末, 44998 assertions 全绿)
- **ADR-X.13**: tlm_stub 多 Extension 升级 (生命周期模式参考)
- **ADR-X.2**: 错误处理策略 (last_exception_ 跨线程模式参考)
- **PcieBarRouter** (`include/tlm/gpu/pcie_bar_router_mvp.hh`): data-driven 注册表设计参考
- **PcieSriovVfPool** (`include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh`): 资源所有权模式参考
- **D1 v1.1.1 修订总结** (`docs/pcie/display-device-mvp.md`): Oracle 复审模式参考

---

## 0. 评审修订记录 (评审报告 R-*, 2027-02-09)

本节记录评审周期内的所有修订,便于追踪文档债与一致性修复。

| 修订 | 位置 | 原内容 | 修订内容 | 评审项 |
|------|------|-------|---------|--------|
| R-C1 | §3.3 DispatchRegistry API | `HandlerFn` (无限定) | `DispatchEntry::HandlerFn` (限定) | 命名空间统一,与 ADR-DGPU-03 §2.2 一致 |
| R-C2 | §3.2 CallbackWorker 不变性 | 编号 1/3/4 跳号 | 编号 1/2/3/4 连续 | 与 ADR-DGPU-01 §3 Inv-1~Inv-4 对齐 |
| R-C3 | §3.1 ABI 拒入规则 | 仅"下限"(≥ Configured) | 含"上限"(∈ [Configured, Running], ShuttingDown 拒) | 与 ADR-DGPU-02 §3 Inv-4 表一致 |
| R-E1 | (配套 ADR-DGPU-04) §1.2 + §4.2 B4 | dynamic_cast 11 处 | dynamic_cast 12 处 (新增 398 行 pcie_config_write) | grep 验证事实修正 |
| R-E2 | (配套 ADR-DGPU-01) §1.1 | `748-753` detached 位置 | `741-755` (`trigger_dma_translate_async` 起始) | grep 验证事实修正 |
| R-R3 | (配套 ADR-DGPU-02) §2.1 | 仅 LifecycleProtocol 直接接口 | 新增 `DGpuBoard::lifecycle_is_at_least/current()` 转发方法 | §5.2 集成测试编译可行性 |
| R-R1+R-R2 | (配套 ADR-DGPU-03) §2.2 | `parse_wptr` 未声明 + `*(uint64_t*)d` UB | 显式声明 `parse_wptr` 来源 + `len` 校验 + `std::memcpy` 替代 | 隐式依赖澄清 + UB 修复 |

### Oracle v2.0.2 修订记录 (2027-02-09, P0 三项)

Oracle 第二轮复审发现 P0 三项必修问题, 本轮 v2.0.2 修复:

| 修订 | 位置 | 原内容 | 修订内容 | Oracle 项 |
|------|------|-------|---------|----------|
| **P0-1a** | §3.3 DispatchRegistry | 单一 `HandlerFn(int, uint64_t, const void*, size_t)` (data 是 const, handler 无法返 buf) | 拆 `ReadHandler(int, uint64_t, void* out_buf, size_t)` + `WriteHandler(...const void* data...)`; `DispatchEntry::Kind::{Read,Write}`; `dispatch_read`/`dispatch_write` 分离 | Oracle-P0-1 (mmio_read 数据回路) |
| **P0-1b** | §3.3 注册顺序示例 | 单一 register_pcie_path | read/write 双注册示例 (D1 BAR0 read + write, Tlp read + write) | Oracle-P0-1 |
| **P0-2e** | §2 Layer1 图 + §3.4 EpCache | "SOC reconfigure 后 invalidate" | 移除 reconfigure 语义; `invalidate()` 标 `[[deprecated]]`; 新增 `clear()` (shutdown 路径) | Oracle-P0-2 (文档自相矛盾) |
| **P0-2e** | §7.1 Oracle-2 | "EpCache 失效协议 race" | 重写为 "一次性 resolve 协议", 原 race 测试废止 | Oracle-P0-2 |
| **P0-3e** | §5.1 + §8.1 DoD | "git diff 必须为空" | "既有 23 ABI 签名 0 diff + 允许末尾追加 24 号 ABI"; 附验证脚本 | Oracle-P0-3 (DoD 仲裁) |

**说明**: §3.2 CallbackWorker API 的 Oracle-1/2/5 更新在 v2.0.1 已完成 (见 ADR-DGPU-01 §2.1); 本节 v2.0.2 仅记录架构文档层面的 P0 修复。

---

**维护**: CppTLM 开发团队
**版本**: v2.0
**最后更新**: 2027-02-09
