# ADR-DGPU-01: CallbackWorker 替代 detached 线程

> **状态**: 📋 提案
> **日期**: 2027-02-09
> **影响**: `include/tlm/gpu/dgpu_board_shell.{hh,cc}` (新增 `CallbackWorker` 内部组件), 0 ABI 变更
> **关联架构文档**: [docs/architecture/14-dgpu-board-ideal-arch.md §3.2](../architecture/14-dgpu-board-ideal-arch.md)
> **配套**: ADR-DGPU-02 (LifecycleProtocol 析构顺序)

> **评审修订 (评审报告 R-E2, 2027-02-09)**: §1.1 detached 线程位置 `748-753` 改为 `741-755` (`trigger_dma_translate_async` 函数起始行应为 741,detach 在 753)。Codegrep 验证 (2027-02-09): `grep -n "std::thread.*detach()\\|}).detach()" src/tlm/gpu/dgpu_board_shell.cc` 命中 737、753、769 行,分别对应 trigger_irq_async / trigger_dma_translate_async / trigger_error_async 三个函数。

> **Oracle v2.0.1 修订 (2027-02-09) — callback 单一所有权 (Oracle-1)**:
> - §2.1 强化声明: **CallbackWorker 是 3 类 callback (`irq_cb_` / `dma_cb_` / `err_cb_`) 的唯一所有者**, v2.0 不再保留 DGpuBoard 侧的 `callback_mu_` / `irq_cb_` 等副本 (v1.0 dgpu_board_shell.hh:146-148, dgpu_board_shell.cc:723-771 全部迁移到 worker_)
> - §2.1 API 补充: `set_irq_callback` / `set_dma_translate_callback` / `set_error_callback` + `clear_callbacks()` 公开方法 (per 评审报告 R-R3 集成测试需要)
> - §3 Inv-3 强化: destroy 顺序中"callback nullification"= `worker_.clear_callbacks()`, 由 worker 持锁,与 §2.3 destroy 协议对齐
> - **Oracle-2 v2.0.1 (DmaTranslate 契约)**: §3 Inv-4 case DmaTranslate 补充返回值处理 (当前 `cb(t.iova, t.size)` 返回值丢弃, Oracle 识别为设计空洞);新 §5.4 "DMA translate 完成路径" 章节定义 host ABI `cpptlm_emulator_dma_translate_poll(handle, iova, &out_paddr)` 同步查询, callback 内部写入共享 buffer
> - **Oracle-5 v2.0.1 (性能指标)**: §2.1 `submit()` 注释加"返 false 时调用方应递增 irq_dropped_total_ + 可选 ErrorCallback"; §7 性能表重定义为 P99 延迟 + 无丢弃持续 IRQ 率

> **Oracle v2.0.2 修订 (2027-02-09) — ABI 24 完整规格 + cb 边界语义 (P0-3)**:
> - §5.4 完整化 24 号 ABI: 补 `dma_translate_wait` + `dma_translate_cancel` 完整 C 函数原型 + 返回码表 + timeout 语义
> - §5.4 cb 边界语义: 明确"cb 未注册 / 任务被 Inv-2 stop 校验丢弃"时 wait 永远有界, 返 `-ENODATA` (与 timeout `-ETIMEDOUT` 区分)
> - §5.4 table 增长控制: `dma_translate_table_` 加 LRU 上限 1024 entries (per Oracle-P0-3d), 超出淘汰最旧
> - §2.1 补 DGpuBoard 转发方法声明: `dma_translate_poll` / `dma_translate_wait` / `dma_translate_cancel` (per Oracle-P0-3b)
> - §6 兼容性表补 `cpptlm_emulator.cc` wrapper 访问路径: `handle->board->worker_.dma_translate_table_*` (per Oracle-P0-3b)
> - §6 兼容性表补 **DoD 仲裁**: 既有 23 ABI 签名 0 diff + 允许末尾追加 24 号 ABI (per Oracle-P0-3e, 修架构 §5.1/§8.1 矛盾)

---

## 1. 背景

### 1.1 问题

DGpuBoard v1.0 (`src/tlm/gpu/dgpu_board_shell.cc:723-771`) 在 `trigger_irq_async` (723-739) / `trigger_dma_translate_async` (741-755) / `trigger_error_async` (757-771) 三处每次都开 `std::thread(...).detach()` 异步执行 host callback (per 评审报告 R-E2):

```cpp
void DGpuBoard::trigger_irq_async(uint32_t vector_id) {
    IrqCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mu_);
        cb = irq_cb_;
    }
    if (cb) {
        std::thread([cb, vector_id]() {
            try { cb(vector_id); } catch (...) {}
        }).detach();   // <-- detached, 无法 join
    }
}
```

### 1.2 风险分析

| 风险 | 严重性 | 触发场景 |
|------|--------|---------|
| **H1**: host callback 访问已析构对象 (use-after-free) | 🔴 高 | DGpuBoard 析构后, 仍在执行的 detached 线程调用 host 端的 cb |
| **H2**: detached 线程 join 阻塞 host 端等待 | 🟢 低 | (不严重, host 通常立即返回) |
| **H3**: 线程创建/销毁开销 | ⚠️ 中 | MSI-X burst 100 IRQ = 100 std::thread 创建 (~1ms/thread) |
| **H4**: 调度不确定性 | ⚠️ 中 | detached 线程调度受 host OS 控制, 仿真确定性受影响 |

### 1.3 现状约束

- 已存在的 joinable 线程: `sim_thread_` (per §2.5 #1) + `coalesce_timer_` (per C3)
- 已存在的 detached 线程: 上述 3 处 trigger_async
- 不可破坏 ABI: 23 ABI 函数字节级冻结 (per AD-088 §D5)

---

## 2. 决策

引入 **`CallbackWorker`** (单线程 joinable worker + task queue) 替代所有 detached 线程。

### 2.1 组件 API

```cpp
// 嵌套类, 仅 DGpuBoard 内部使用
class DGpuBoard::CallbackWorker {
public:
    enum class Kind : uint8_t { Irq, DmaTranslate, Error };

    struct Task {
        Kind            kind;
        uint32_t        vector_id;       // IRQ
        uint64_t        iova;            // DMA
        size_t          size;            // DMA
        int             err_code;        // Error
        std::string     msg;             // Error
    };

    // 启动 worker (在 DGpuBoard::init() 调用)
    void start();

    // 提交任务 (非阻塞, 推入 queue_ + notify cv_)
    // 返回 false: stop_ || draining_ || queue overflow / 背压
    // per Oracle-1 v2.0.1: 返 false 时调用方应递增 irq_dropped_total_ metric + 可选触发 ErrorCallback
    bool submit(Task t);

    // 关闭 worker (在 DGpuBoard::destroy() 调用)
    // 严格顺序: stop_=true → draining_=true → notify → 等待 queue 清空 → join
    void shutdown_and_join();

    // ─── Callback 单一所有权 API (per Oracle-1 v2.0.1) ───
    // v2.0 起 CallbackWorker 是 callback 的唯一所有者;
    // DGpuBoard 的 set_*_callback API 全部转发到 worker (per §2.2 集成测试需要)

    // 设置 callback (持 callback_mu_, 线程安全)
    // per Oracle-5 v2.0.1: set 时若 lifecycle ≥ ShuttingDown 静默拒设 (与 ADR-DGPU-02 §2.3 ABI guard 一致)
    void set_irq_callback(DGpuBoard::IrqCallback cb);
    void set_dma_translate_callback(DGpuBoard::DmaTranslateCallback cb);
    void set_error_callback(DGpuBoard::ErrorCallback cb);

    // 一次性清空所有 callback (per Oracle-1: destroy Step 1 在 shutdown_and_join 之前调)
    // 实现: lock(callback_mu_) → 3 个 callback 置 nullptr → unlock
    void clear_callbacks() noexcept;

    // 测试/诊断 accessors
    size_t pending() const;
    bool   is_running() const noexcept { return worker_thread_.joinable(); }

private:
    // worker 线程主循环
    void worker_loop();

    // 派发单任务到对应 callback
    void dispatch_task(const Task& t);

    // 线程 + 同步
    std::thread              worker_thread_;
    std::mutex               mu_;
    std::condition_variable  cv_;
    std::deque<Task>         queue_;
    std::atomic<bool>        stop_{false};       // 退出信号
    std::atomic<bool>        draining_{false};   // shutdown 进入, 拒新 submit
    static constexpr size_t  kMaxQueueSize = 1024;

    // 回调指针 (callback 单一所有权, per Oracle-1 v2.0.1)
    // v2.0 起 worker 是 irq_cb_ / dma_cb_ / err_cb_ 的唯一所有者;
    // DGpuBoard 侧不再保留副本, set_*_callback API 转发到此 (per §2.1 公开 API)
    DGpuBoard::IrqCallback          irq_cb_;
    DGpuBoard::DmaTranslateCallback dma_cb_;
    DGpuBoard::ErrorCallback        err_cb_;
    std::mutex                      callback_mu_;   // 保护上述 3 个 callback 指针

    // per Oracle-2 v2.0.1 + Oracle v2.0.2 (P0-3d): DMA translate 完成路径 (host ABI 24 同步查询)
    //  - LRU 淘汰: 默认上限 kMaxDmaTranslateEntries=1024, 超出淘汰最久未用
    //  - 线程安全: 所有读写均持 dma_translate_table_mu_
    struct DmaTranslateEntry {
        uint64_t iova;
        uint64_t paddr;
        uint64_t last_used_tick;
    };
    std::mutex                                dma_translate_table_mu_;
    std::condition_variable                   dma_translate_table_cv_;
    std::list<DmaTranslateEntry>              dma_translate_lru_;
    std::unordered_map<uint64_t,
                       std::list<DmaTranslateEntry>::iterator> dma_translate_table_;
    static constexpr size_t                  kMaxDmaTranslateEntries = 1024;
    uint64_t                                  current_tick_{0};   // dispatch 时单调递增, LRU 用
```

### 2.2 替换映射

| v1.0 调用点 | v2.0 调用 |
|------------|----------|
| `trigger_irq_async(vector_id)` | `worker_.submit({Kind::Irq, vector_id, ...})` |
| `trigger_dma_translate_async(iova, size)` | `worker_.submit({Kind::DmaTranslate, 0, iova, size, ...})` |
| `trigger_error_async(err_code, msg)` | `worker_.submit({Kind::Error, 0, 0, 0, err_code, msg})` |

### 2.3 析构顺序集成 (per Oracle-1 v2.0.1, 8 步)

`DGpuBoard::destroy()` **8 步**顺序 (per 评审报告 R-R5 命名统一, 与 ADR-DGPU-02 §2.2 一致; 步骤编号以本节为准):

1. `LifecycleProtocol` transition → `ShuttingDown` (拒新 ABI)
2. **`worker_.clear_callbacks()` ← Oracle-1 v2.0.1 新增, 必须在 shutdown_and_join 之前** (持 callback_mu_ 置 null, 即使 worker in-flight task 取出后 dispatch_task 看到 nullptr 也跳过)
3. `worker_.shutdown_and_join()` ← 本 ADR 原始
4. `coalesce_timer_thread_.shutdown_and_join()`
5. `sim_thread_.join()` (通过 poison pill)
6. `soc_.reset()`
7. `eq_.reset()`
8. `LifecycleProtocol` final transition → `Destructed` (per ADR-DGPU-02 §2.2 Step 7)

**修订说明** (per 评审报告 R-R5 v2.0.1):
- 旧文档"6 步顺序"经 Oracle-1 v2.0.1 修订后扩展为 **8 步** (新增 #2 `worker_.clear_callbacks()` + #8 `LifecycleProtocol` final transition)
- ADR-DGPU-02 §2.2 的"7 步"是步骤 #1-#7 (不含 Step 0 的 lifecycle transition, 实际 #1 即 ADR-02 的 Step 0; 加上本节 #8 = ADR-02 的 Step 7)
- 两份 ADR 步骤计数差异源于: ADR-DGPU-01 从"worker shutdown"视角数; ADR-DGPU-02 从"destroy 全流程"视角数 (含 lifecycle transition)
- 本节明确 8 步, 与 ADR-DGPU-02 §2.2 步骤 0-7 一一对应, 终结两份 ADR 的步骤计数不一致

---

## 3. 关键不变性 (Invariants)

### Inv-1: 提交校验

```cpp
bool CallbackWorker::submit(Task t) {
    if (stop_.load(std::memory_order_acquire) ||
        draining_.load(std::memory_order_acquire)) {
        return false;   // 拒新提交
    }
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.size() >= kMaxQueueSize) {
        return false;   // 背压
    }
    queue_.push_back(std::move(t));
    cv_.notify_one();
    return true;
}
```

**保证**: shutdown 期间不再 accept 任务, 避免 in-flight task race。

### Inv-2: worker_loop 双重 stop 校验

```cpp
void CallbackWorker::worker_loop() {
    while (true) {
        Task t;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this] {
                return stop_.load(std::memory_order_acquire) ||
                       !queue_.empty();
            });
            if (stop_.load(std::memory_order_acquire) && queue_.empty()) {
                return;  // 退出 worker
            }
            t = std::move(queue_.front());
            queue_.pop_front();
        }
        // 锁外 dispatch, 但调用 callback 前再校验 stop_
        if (!stop_.load(std::memory_order_acquire)) {
            dispatch_task(t);
        }
    }
}
```

**保证**: 即便 in-flight task 在锁内被取出, 锁外 dispatch 时 stop_ 已置, 不再调用 callback。

### Inv-3: 析构顺序严格性

```cpp
void CallbackWorker::shutdown_and_join() {
    draining_.store(true, std::memory_order_release);  // Step 1: 拒新 submit
    {
        std::lock_guard<std::mutex> lock(mu_);
        stop_.store(true, std::memory_order_release);  // Step 2: 唤醒 worker
    }
    cv_.notify_all();                                  // Step 3: 通知 cv
    if (worker_thread_.joinable()) {                   // Step 4: join
        worker_thread_.join();
    }
    // Step 5: 此时 worker 已退出, 回调指针安全释放
}
```

**保证**: 调用方在 `shutdown_and_join()` 返回后, 可以安全释放 callback 指针 (因为 worker 已 drain)。

### Inv-4: 回调异常隔离

```cpp
void CallbackWorker::dispatch_task(const Task& t) {
    switch (t.kind) {
    case Kind::Irq: {
        std::lock_guard<std::mutex> lock(callback_mu_);
        auto cb = irq_cb_;
        if (cb) {
            try { cb(t.vector_id); } catch (...) {}  // host 异常不反向影响 worker
        }
        break;
    }
    case Kind::DmaTranslate: {
        std::lock_guard<std::mutex> lock(callback_mu_);
        auto cb = dma_cb_;
        if (cb) {
            // per Oracle-2 v2.0.1 + Oracle v2.0.2 (P0-3c): cb 返回值处理契约
            //   合约 (1): cb 返回 translated paddr (uint64_t)
            //   合约 (2): worker 持 callback_mu_ 时, 写入内部 `dma_translate_table_[iova] = paddr`
            //            (lock 释放后, host ABI `cpptlm_emulator_dma_translate_poll()` 同步查询)
            //   合约 (3): 仿真侧任何线程后续查询该 iova, 可读到最新值 (memory_order_release)
            //   失败语义 (Oracle v2.0.2 修正):
            //     - cb 抛异常  → table[iova]=0 + notify (driver poll 返 0 但 paddr==0, 需校验)
            //     - cb 返回 0  → table[iova]=0 + notify (同上)
            //     - cb 为 null → **跳过整块, 不写 table, 不 notify**
            //                   driver wait 永远有界 (立即返 -ENODATA, 不等 timeout)
            //   关键区别: cb 异常/返 0 与 cb 未注册语义**不同**!
            //     前者: 翻译尝试过, 失败原因在 cb 内
            //     后者: 翻译尝试未发生, driver 应放弃并 abort DMA
            uint64_t translated = 0;
            try { translated = cb(t.iova, t.size); } catch (...) { translated = 0; }
            {
                std::lock_guard<std::mutex> table_lock(dma_translate_table_mu_);
                dma_translate_table_[t.iova] = translated;
                // LRU 淘汰 (per Oracle-P0-3d, kMaxDmaTranslateEntries=1024)
                auto it = dma_translate_lru_.insert(dma_translate_lru_.begin(),
                    {t.iova, translated, current_tick_});
                if (dma_translate_lru_.size() > kMaxDmaTranslateEntries) {
                    auto victim = std::prev(dma_translate_lru_.end());
                    dma_translate_table_.erase(victim->iova);
                    dma_translate_lru_.erase(victim);
                }
            }
            dma_translate_table_cv_.notify_all();
        }
        // cb 为 null 时: 故意不写 table, 不 notify
        // driver 端 dma_translate_wait 会立即返 -ENODATA (per §5.4 ABI 24.2)
        break;
    }
    case Kind::Error: {
        std::lock_guard<std::mutex> lock(callback_mu_);
        auto cb = err_cb_;
        if (cb) {
            try { cb(t.err_code, t.msg); } catch (...) {}
        }
        break;
    }
    }
}
```

**保证**: host callback 抛异常不影响 worker 线程继续处理后续任务 (try/catch 隔离)。

---

## 4. 实施步骤

### 4.1 阶段 A: 基础设施 (W25)

| 任务 | 产出 |
|------|------|
| A1: 新增 `dgpu_board_callback_worker.hh` 头文件 | API 完整 |
| A3: 单元测试 `test_dgpu_board_callback_worker.cc` | 10 case |

### 4.2 阶段 B: 集成 (W26)

| 任务 | 产出 |
|------|------|
| B1: `DGpuBoard` 添加 `CallbackWorker worker_` 成员 | 替换 detached |
| B2: `trigger_irq_async` / `trigger_dma_translate_async` / `trigger_error_async` 改为 `worker_.submit(...)` | 0 detached |
| B3: `init()`/`destroy()` 中加 `worker_.start()` / `worker_.shutdown_and_join()` | 集成析构 |

### 4.3 验证

- [ ] `test_dgpu_board_callback_worker.cc` 10 case 100% PASS
- [ ] `grep -r "std::thread.*detach\(\)" include/tlm/gpu/dgpu_board_shell.cc` 为空
- [ ] 44498 assertions 100% PASS (0 regression)
- [ ] Oracle 评审通过

---

## 5. 测试策略

### 5.1 单元测试

```cpp
TEST_CASE("CallbackWorker basic submit + drain") {
    DGpuBoard::CallbackWorker w;
    int count = 0;
    w.set_irq_callback([&](uint32_t) { ++count; });
    w.start();
    for (uint32_t i = 0; i < 100; ++i) {
        REQUIRE(w.submit({Kind::Irq, i, 0, 0, 0, ""}));
    }
    w.shutdown_and_join();
    REQUIRE(count == 100);
    REQUIRE(w.pending() == 0);
}

TEST_CASE("CallbackWorker shutdown rejects new submit") {
    DGpuBoard::CallbackWorker w;
    w.start();
    w.shutdown_and_join();
    REQUIRE(!w.submit({Kind::Irq, 0, 0, 0, 0, ""}));
}

TEST_CASE("CallbackWorker use-after-free prevention") {
    DGpuBoard::CallbackWorker w;
    std::atomic<bool> called{false};
    w.set_irq_callback([&](uint32_t) {
        called.store(true);  // 即使 shutdown 后被调, 也不会崩 (因为 captured 引用)
    });
    w.start();
    w.shutdown_and_join();
    // shutdown 返回后, 不可能有 task 被处理
    REQUIRE(!called.load());
}
```

### 5.2 集成测试

```cpp
TEST_CASE("DGpuBoard full lifecycle: CallbackWorker joinable") {
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg));
    REQUIRE(board.init());
    board.tick();
    // simulate MSI-X burst
    for (int i = 0; i < 100; ++i) {
        board.msix_update_pending(0);
    }
    board.shutdown();
    // shutdown 后, 所有 worker task 已 drain
    REQUIRE(board.callback_worker_pending() == 0);
}
```

### 5.4 DMA translate 完成路径 (per Oracle-2 v2.0.1)

**问题**: v2.0 前 `dispatch_task` case DmaTranslate 调用 `cb(iova, size)` 但**返回值被丢弃**; 翻译结果无回路, 仿真侧任何线程无法获取翻译值。

**方案**: callback 返回值写入 worker 内部的 `dma_translate_table_` (iova → paddr),host 通过 **24 号 ABI** 同步查询。

**24 号 ABI 设计 — v2.0.2 完整规格** (新增, 但标记为 v2.0.1 配套, 不破坏现有 23 ABI 字节级冻结; per Oracle-P0-3a/b/c):

```c
// include/abi/cpptlm_emulator.h (新增 24 号 ABI, 不修改既有 23 ABI 签名)
//
// ─── 24.1 dma_translate_poll (非阻塞查询) ───
// int cpptlm_emulator_dma_translate_poll(
//     cpptlm_emulator_handle_t* handle,
//     uint64_t iova,
//     uint64_t* out_paddr);
// 返回值:
//   0          : 翻译完成, *out_paddr 已填充
//   -EAGAIN    : 翻译尚未完成 (callback 已提交但 cb 未返回), driver 应改用 _wait
//   -ENODATA   : 翻译不可能完成 (cb 未注册 / 已 cancel / 任务被 stop 校验丢弃)
//   -EINVAL    : handle 非法 / out_paddr null
//   -ESHUTDOWN : board lifecycle ≥ ShuttingDown
//
// ─── 24.2 dma_translate_wait (阻塞等待) ───
// int cpptlm_emulator_dma_translate_wait(
//     cpptlm_emulator_handle_t* handle,
//     uint64_t iova,
//     uint64_t* out_paddr,
//     uint32_t timeout_ms);   // 0 = 无限等待 (不推荐); UINT32_MAX = 永不超时
// 返回值:
//   0           : 翻译完成, *out_paddr 已填充
//   -ETIMEDOUT  : 在 timeout_ms 内未完成 (callback 仍在跑 或 已被丢弃)
//   -ENODATA    : 翻译不可能完成 (同 poll); 立即返, 不等待
//   -ECANCELED  : driver 已调 _cancel() (本线程或另一线程)
//   -EINVAL     : handle 非法 / out_paddr null / timeout_ms==0 且翻译未完成
//   -ESHUTDOWN  : board lifecycle ≥ ShuttingDown; wait 立即返
//
// 阻塞语义:
//   wait 永远有界 — timeout_ms 必须 > 0 或调用方显式承担无限等待风险
//   cb 未注册 / 任务被 Inv-2 stop 校验丢弃 → 立即返 -ENODATA (不等 timeout)
//   callback 抛异常 → table[iova]=0 + notify cv, wait 返 0 (driver 必须校验 paddr==0)
//
// ─── 24.3 dma_translate_cancel (取消等待) ───
// int cpptlm_emulator_dma_translate_cancel(
//     cpptlm_emulator_handle_t* handle,
//     uint64_t iova);
// 返回值:
//   0           : 成功标记取消 (不阻止 cb 执行, 仅 driver 后续 poll/wait 立即返 -ECANCELED / -ENODATA)
//   -ENODATA    : 该 iova 从未提交翻译 (no-op cancel)
//   -EINVAL     : handle 非法
//   -ESHUTDOWN  : board lifecycle ≥ ShuttingDown (cancel 无效, 返回 -ESHUTDOWN)
//
// 取消语义 (per Oracle-2 v2.0.1 §5.4 设计意图):
//   cancel 只标记 "driver 放弃等结果", 不阻止 cb 执行
//   cb 若仍在跑, 完成时 result 仍写入 dma_translate_table_[iova] (LRU 替换)
//   driver 后续 poll/wait 立即返 -ECANCELED / -ENODATA (取消标记后)
//   cancel 幂等: 同一 iova 多次 cancel 安全
```

**DGpuBoard 转发方法声明** (per Oracle-P0-3b, 实现细节待 Phase A):

```cpp
// in class DGpuBoard (public):
//
//   // 转发到 worker_ (per Oracle-1 callback 单一所有权)
//   // C ABI wrapper 在 src/abi/cpptlm_emulator.cc 通过 handle->board->worker_ 访问
//   int dma_translate_poll(uint64_t iova, uint64_t* out_paddr);
//   int dma_translate_wait(uint64_t iova, uint64_t* out_paddr, uint32_t timeout_ms);
//   int dma_translate_cancel(uint64_t iova);
//   // 内部: 各自 lock(dma_translate_table_mu_), poll 用 try-lock + table.find(),
//   //        wait 用 cv_.wait_for(lock, chrono::ms(timeout_ms), predicate)
//   //        cancel 用 table erase + cv_.notify_all (唤醒所有 waiter)
```

**dma_translate_table_ 增长控制** (per Oracle-P0-3d):

```cpp
// 在 CallbackWorker::dma_translate_table_ 旁补 LRU 容器:
//   static constexpr size_t kMaxDmaTranslateEntries = 1024;   // 默认上限
//   struct DmaTranslateEntry { uint64_t iova; uint64_t paddr; uint64_t last_used_tick; };
//   std::list<DmaTranslateEntry>                          dma_translate_lru_;     // LRU 链表
//   std::unordered_map<uint64_t, std::list<...>::iterator> dma_translate_table_;   // iova → list iter
//
// dispatch_task 写入时:
//   1. lock(dma_translate_table_mu_);
//   2. 若 table_.count(iova) == 0 且 size() >= kMaxDmaTranslateEntries:
//      → 淘汰 lru_.back() (最久未用) + table_.erase(lru_.back().iova);
//   3. lru_.push_front({iova, paddr, current_tick}); table_[iova] = lru_.begin();
//   4. unlock + notify_all
//
// 读取 (poll/wait) 时更新 LRU 位置 (mark recently used):
//   1. lock(mu_);
//   2. 若找到: lru_.splice(lru_.begin(), lru_, it->second);  // 移到 front
//   3. 读 paddr + unlock
```

**driver 用例**:

```c
// 典型 driver 路径 (driver 模拟 DMA 读)
int ret = cpptlm_emulator_dma_translate_poll(handle, iova, &paddr);
if (ret == -EAGAIN) {
    // 翻译未完成, 等 100ms 或非阻塞返回
    ret = cpptlm_emulator_dma_translate_wait(handle, iova, &paddr, 100);
    if (ret == -ETIMEDOUT) {
        // 翻译超时, abort DMA
        return -ETIMEDOUT;
    }
}
if (ret == 0) {
    // 翻译成功, 用 paddr 发起 DMA
    dma_read(paddr, buf, size);
}
```

**仿真侧路径** (用于验证):

```cpp
// 仿真侧测试代码
DGpuBoard board("test");
REQUIRE(board.load_soc_config(simple_cfg));
REQUIRE(board.init());

// 注册 dma translate callback
board.set_dma_translate_callback([](uint64_t iova, size_t size) -> uint64_t {
    return mock_paddr_for(iova, size);  // 简单映射
});

// 触发翻译
board.trigger_dma_translate_async(0x1000, 4096);

// 仿真侧同步查询 (测试断言)
uint64_t paddr;
REQUIRE(board.dma_translate_poll(0x1000, &paddr) == 0);
REQUIRE(paddr == mock_paddr_for(0x1000, 4096));
```

**保证 (per Oracle-2 v2.0.1 + Oracle v2.0.2 P0-3)**:
- callback 写入 table 时持 `dma_translate_table_mu_` + 通知 cv (memory_order_release)
- driver poll 等待时持锁 + wait cv (memory_order_acquire), 读到 callback 写入的值
- cancel 不影响 callback 执行, 仅 driver 不再 poll
- **cb 未注册 / 任务被 Inv-2 stop 校验丢弃**: worker_loop 不 dispatch, table 不写, cv 不通知; driver wait 立即返 -ENODATA (永远有界)
- **同 iova 并发翻译碰撞**: 后者覆盖前者 (LRU splice 到 front), 所有 waiter 被 notify_all 唤醒但读到同一份最新结果 (driver 需自行校验 size 一致)
- **table 增长**: LRU 上限 kMaxDmaTranslateEntries=1024, 超出淘汰最久未用 (per Oracle-P0-3d), 长时间仿真无内存泄漏
- **callback 抛异常**: table[iova]=0 + notify (driver poll 返 0 但 paddr==0, 必须校验)
- **callback 返 0**: 同上 (table[iova]=0, driver 必须区分 "未翻译" 与 "翻译到 0 地址" — 文档化为 driver 责任)

**不属于 23 ABI 字节级冻结 (per Oracle v2.0.2 P0-3e DoD 仲裁)**:
- 24 号 ABI 是**新增**, 不修改既有 23 ABI 签名 (签名级冻结)
- 但 `include/abi/cpptlm_emulator.h` 文件本身会被修改 (末尾追加新函数声明)
- 架构文档 §5.1 + §8.1 DoD 检查项更新: `git diff HEAD -- include/abi/cpptlm_emulator.h` 应**仅含末尾追加块** (per Oracle-P0-3e 仲裁)
- 验证脚本: `git diff HEAD -- include/abi/cpptlm_emulator.h | grep -E '^[+-]' | grep -v '^+++ \|^--- ' | grep -v 'cpptlm_emulator_dma_translate_poll\|_wait\|_cancel' | head` 必须为空 (排除 hunk 头与 24 号 ABI 行, 检查无其他改动)

---

## 6. 兼容性

| 项 | 影响 |
|----|------|
| **23 ABI 签名** | 0 修改 (字节级冻结, per ADR-088 §D5) |
| **24 号 ABI** `cpptlm_emulator_dma_translate_poll/_wait/_cancel` | **新增** (per Oracle-2 v2.0.1 + v2.0.2 P0-3a/b/c 完整规格), 不属于 ADR-088 §D5 "23 ABI 字节级冻结" 范围 (冻结仅约束签名 0 修改, 不禁止新增) |
| **JSON 配置** | 0 修改 |
| **`src/abi/cpptlm_emulator.cc`** | 0 修改既有 23 ABI wrapper; **末尾新增 §5.4 24 号 ABI wrapper** (访问路径: `handle->board->worker_.dma_translate_table_*`) |
| **`include/abi/cpptlm_emulator.h`** | 0 修改既有 23 ABI 签名; **末尾追加 §5.4 24 号 ABI 声明** (per Oracle v2.0.2 P0-3b) |
| **DoD 检查项** (per Oracle v2.0.2 P0-3e) | 架构 §5.1 + §8.1 改: `git diff HEAD -- include/abi/cpptlm_emulator.h` **允许末尾追加块**, 验证脚本 `git diff ... | grep -E '^[+-]' | grep -v 'cpptlm_emulator_dma_translate_poll\|_wait\|_cancel' | head` 应仅含 hunk 头 |
| **下游 `set_irq_callback` 等 API** | 签名 0 修改, 仅内部实现改用 worker_ (per Oracle-1 v2.0.1 转发) |
| **`coalesce_timer_`** | 不变, 仍用单独 joinable 线程 (per §3.2 注释保留) |

---

## 7. 性能影响 (per Oracle-5 v2.0.1, 指标重定义)

**修订原因**: v1.0 "burst 100 IRQ 100ms → 5ms" 指标瞄准的是错误基准——真实 driver 不会 burst IRQ, 关注的是 (a) 单 IRQ 回调延迟 P99 (b) 无丢弃前提下最大持续 IRQ 率 (c) 内存占用。新指标对齐真实 driver 关注点。

| 指标 | v1.0 | v2.0 (target) | 测试方法 |
|------|------|---------------|---------|
| **单 IRQ 回调延迟 P99** | ~10us (detached 线程唤醒 + 上下文切换) | < 5us (queue 推入 + notify_one) | `perf_test_p99_irq_latency`: 触发 10000 IRQ, 测量 `trigger_irq_async` 到 worker dispatch cb 的延迟, 取 P99 |
| **无丢弃最大持续 IRQ 率** | ∞ (detached 永不丢弃, 但 UAF 风险) | ≥ 10000 IRQ/s (worker drain 速率, kMaxQueueSize=1024) | `perf_test_sustained_irq_rate`: 持续注入 IRQ, 监控 `irq_dropped_total_`, 找到首 drop 点 |
| **DMA 翻译回调延迟** (per Oracle-2) | N/A (无回调回路) | P99 < 10us (callback 完成后 host poll 立即可见) | `perf_test_dma_translate_poll_latency` |
| **背压丢弃观测性** | 无 (v1.0 永不丢弃) | `irq_dropped_total_` 计数 + 触发 ErrorCallback(可选) | `test_callback_worker_backpressure_drop_notification` |
| **内存占用** | 100 detached 线程 × 8KB stack = 800KB (worst case) | 1 worker 线程 × 8KB + queue ≤ 1024 × sizeof(Task) ≈ 200KB | top/heaptrack |
| **调度确定性** | OS 决定 | worker 顺序处理 (确定性, 排除 detached 调度抖动) | 同一 test 10000 次, callback 顺序必须一致 |

**Oracle-5 修订要点**:
- **指标 1 重定义**: "MSI-X burst 100 IRQ 100ms → 5ms" 删除 (错误基准)
- **新增指标**: 单 IRQ P99 延迟 + 无丢弃持续 IRQ 率 + DMA 翻译 poll 延迟
- **新增观测性**: `irq_dropped_total_` 计数 (per Oracle-1 §2.1 `submit()` 注释), submit 返 false 时递增
- **新增契约**: 提交返 false 时调用方可触发 ErrorCallback(可选行为, 见 §2.1 `submit()` 注释)
- **风险表新增 R5-背压**: callback 阻塞 worker (head-of-line blocking) 会延迟后续 IRQ,需文档化 host callback 非阻塞契约

---

## 8. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: worker 顺序处理, 丧失 IRQ 并行性 | 中 | 极端 burst 下可能积压 | kMaxQueueSize=1024 背压保护 + C3 MSI-X 合并已减少 burst 频次 |
| **R2**: shutdown_and_join 时有 in-flight task | 中 | host callback use-after-free | Inv-2 双重 stop_ 校验, 锁外 dispatch 前再校验 |
| **R3**: callback 阻塞 worker | 低 | 后续 task 延迟 | 文档化 host callback 非阻塞契约, 提供 `[[nodiscard]]` 注释 |
| **R4**: queue 满导致 submit 失败 | 低 | IRQ 丢失 | 返回 false 让调用方决定降级策略 |

---

## 9. 参考

- **架构文档**: [14-dgpu-board-ideal-arch.md §3.2](../architecture/14-dgpu-board-ideal-arch.md)
- **ADR-DGPU-02**: LifecycleProtocol 状态机 + 析构顺序
- **v1.0 实现**: `src/tlm/gpu/dgpu_board_shell.cc:723-771`
- **同类模式参考**: `coalesce_timer_` (DGpuBoard 内部 C3 MSI-X 合并器)
- **同类模式参考**: `sim_thread_` (DGpuBoard 主仿真循环)

---

**维护**: CppTLM 开发团队
**状态**: 📋 提案