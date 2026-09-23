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
│  │   · sim_thread 启动前 + SOC reconfigure 后 invalidate            │         │
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
// 拒入规则:
//   mmio_read/write/backdoor_*: 需 ≥ Configured
//   msix_*/lookup_register:      需 ≥ Configured
//   set_*_callback:              需 ≥ Configured, ≤ ShuttingDown
//   trigger_*_async:              需 ≥ Initialized
//   shutdown/destroy:            任何非 Destructed 状态
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

**关键不变性**:
1. `submit()` 在 `stop_ || draining_` 时返回 false (避免 submit 阶段 race)
3. `worker_loop()` 中调用 callback 前再校验 stop_ 标志 (防止 in-flight task)
4. `shutdown_and_join()` 严格顺序: `stop_=true` + `draining_=true` → 等待 queue 清空 → join worker

**测试策略**:
- `CallbackWorkerTest`: submit 1000 task, shutdown_and_join 后 pending=0
- `CallbackWorkerLifecycleTest`: shutdown 后 submit 返 false
- `CallbackWorkerUseAfterFreeTest`: shutdown 后 callback 不再被调 (mock 验证)
- **性能对比**: burst 100 个 IRQ 比 v1.0 detached 路径快 10x+

### 3.3 DispatchRegistry (data-driven dispatch)

**问题** (v1.0 M5): `dispatch_mmio_to_pcie` 4 态 switch 都是空 break (`dgpu_board_shell.cc:775-800`),spec 名实不符。T-P12-2 E2E 测试必失败。

**方案**: data-driven DispatchRegistry (类比 `PcieBarRouter`):

```cpp
// 内部组件
struct DispatchEntry {
    enum class Scope : uint8_t { BarExact, BarRange, Global };
    Scope      scope;
    uint8_t    bar;             // BarExact / BarRange
    uint64_t   offset_lo;       // BarRange
    uint64_t   offset_hi;       // BarRange
    uint64_t   offset;          // BarExact
    using HandlerFn = std::function<int(uint8_t, uint64_t, const void*, size_t)>;
    HandlerFn  handler;
    int        priority;        // 高优先级先匹配
    std::string name;           // 诊断
};

class DGpuBoard::DispatchRegistry {
public:
    // 注册 handler (重载支持 4 种 scope)
    void register_handler(const DispatchEntry& e);
    void register_bar0_display_routing(HandlerFn fn);    // D1 display device
    void register_bar1_doorbell(HandlerFn fn);            // SdmaEngineTLM
    void register_pcie_path(PciePath path, HandlerFn fn); // T-P12-1

    // 匹配 + dispatch (按 priority 降序, 首个匹配胜出)
    int dispatch(uint8_t bar, uint64_t offset, const void* data, size_t len) const;

    // 测试 accessors
    size_t entry_count() const;
    std::vector<std::string> matched_handlers(uint8_t bar, uint64_t offset) const;

private:
    std::vector<DispatchEntry> entries_;  // 按 priority 排好序
};
```

**注册顺序** (在 `init()` 后):

```cpp
// 默认 Legacy 行为 (向后兼容)
registry_.register_pcie_path(PciePath::Legacy,
    [](uint8_t, uint64_t, const void*, size_t) { return 0; });

// PcieDisplayDevice BAR 0 fast-path (D1 v1.1.1, display_routing_enabled)
registry_.register_bar0_display_routing(
    [this](uint8_t, uint64_t off, const void* d, size_t len) {
        auto* dev = ep_cache_->get()->display_device();
        return dev.mmio_write(off, d, len);
    });

// SdmaEngineTLM BAR1+0x10010000 doorbell (Stage 1.3a)
registry_.register_bar1_doorbell(
    [this](uint8_t, uint64_t, const void* d, size_t len) {
        if (sdma_engine_) {
            uint64_t wptr = parse_wptr(d, len);
            sdma_engine_->mmio_write(/*bar=*/1, kBar1DoorbellOffset, wptr);
        }
        return 0;
    });

// PciePath::Tlp (T-P12-2, 待实施)
registry_.register_pcie_path(PciePath::Tlp,
    [this](uint8_t bar, uint64_t off, const void* d, size_t len) {
        return pcie_path_tlp_dispatch(bar, off, d, len);
    });
```

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

    // 显式失效 (load_soc_config 重复调用 / SOC reconfigure)
    void invalidate() noexcept;

private:
    std::mutex                  mu_;
    std::atomic<bool>           resolved_{false};
    tlm::pcie::PcieEndpointIP*  cached_ = nullptr;

    // 首次 resolve 时动态 cast, 之后直接返回
    tlm::pcie::PcieEndpointIP* resolve();
};
```

**关键不变性**:
1. `cached_` 仅在 `init()` 完成后设置 (避免 race with `load_soc_config` 重复调用)
2. `invalidate()` 必须在 SOC 重配置前调 (留给调用方保证)
3. `get()` 自身线程安全 (cache 命中走 atomic fast path)

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

### 5.1 ABI 兼容性 (硬约束)

23 ABI 函数签名零修改,实现内部可重构:

```cpp
// 23 ABI 函数 (字节级兼容)
extern "C" {
    int cpptlm_emulator_open(uint32_t dev_id, cpptlm_emulator_handle_t** handle);
    void cpptlm_emulator_close(cpptlm_emulator_handle_t* handle);
    // ... 其余 21 个 (mmio_read/write, config_read/write, msix_*, lookup_register, etc.)
}
```

**验证**: `git diff HEAD -- include/abi/cpptlm_emulator.h` 必须为空。

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

---

## 8. 度量与验证

### 8.1 完成定义 (DoD)

- [ ] 4 个新组件 (`LifecycleProtocol` / `CallbackWorker` / `DispatchRegistry` / `EpCache` + `PendingReqGuard`) 实现完成
- [ ] `dgpu_board_shell.cc` 重构完成, 移除所有 detached 线程
- [ ] `DGpuSoc` 命名空间迁移完成
- [ ] 新增 50 case (Lifecycle 16 + CallbackWorker 10 + DispatchRegistry 12 + EpCache 6 + PendingReqGuard 8 + NamespaceMigration 4) 100% PASS
- [ ] 现有 44498 assertions 100% PASS (0 regression)
- [ ] `openspec validate --changes --strict` PASS
- [ ] `git diff HEAD -- include/abi/cpptlm_emulator.h` 为空 (23 ABI 字节级兼容)
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS
- [ ] Oracle 评审通过 (per ADR-0025 P0 门禁)

### 8.2 性能目标

| 指标 | v1.0 baseline | v2.0 target |
|------|---------------|-------------|
| MSI-X burst 100 IRQ 处理时间 | 100ms (100 detached 线程) | < 5ms (worker task queue) |
| mmio_read 同步延迟 (p99) | 50ms (timeout 上限) | < 1ms (新 dispatch) |
| BAR0 显示路由延迟 | < 5us | < 5us (持平) |
| 多卡 (4 卡) 并发 mmio 吞吐 | 1000 ops/s | > 5000 ops/s |

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

**维护**: CppTLM 开发团队
**版本**: v2.0
**最后更新**: 2027-02-09