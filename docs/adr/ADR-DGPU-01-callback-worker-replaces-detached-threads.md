# ADR-DGPU-01: CallbackWorker 替代 detached 线程

> **状态**: 📋 提案
> **日期**: 2027-02-09
> **影响**: `include/tlm/gpu/dgpu_board_shell.{hh,cc}` (新增 `CallbackWorker` 内部组件), 0 ABI 变更
> **关联架构文档**: [docs/architecture/14-dgpu-board-ideal-arch.md §3.2](../architecture/14-dgpu-board-ideal-arch.md)
> **配套**: ADR-DGPU-02 (LifecycleProtocol 析构顺序)

---

## 1. 背景

### 1.1 问题

DGpuBoard v1.0 (`src/tlm/gpu/dgpu_board_shell.cc:723-771`) 在 `trigger_irq_async` / `trigger_dma_translate_async` / `trigger_error_async` 三处每次都开 `std::thread(...).detach()` 异步执行 host callback:

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
    // 返回 false: stop_ || draining_ || queue overflow
    bool submit(Task t);

    // 关闭 worker (在 DGpuBoard::destroy() 调用)
    // 严格顺序: stop_=true → draining_=true → notify → 等待 queue 清空 → join
    void shutdown_and_join();

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

    // 回调指针 (持有 callback_mu_, 同 v1.0 但加 stop 校验)
    DGpuBoard::IrqCallback          irq_cb_;
    DGpuBoard::DmaTranslateCallback dma_cb_;
    DGpuBoard::ErrorCallback        err_cb_;
    std::mutex                      callback_mu_;
};
```

### 2.2 替换映射

| v1.0 调用点 | v2.0 调用 |
|------------|----------|
| `trigger_irq_async(vector_id)` | `worker_.submit({Kind::Irq, vector_id, ...})` |
| `trigger_dma_translate_async(iova, size)` | `worker_.submit({Kind::DmaTranslate, 0, iova, size, ...})` |
| `trigger_error_async(err_code, msg)` | `worker_.submit({Kind::Error, 0, 0, 0, err_code, msg})` |

### 2.3 析构顺序集成

`CallbackWorker::shutdown_and_join()` 必须作为 `DGpuBoard::destroy()` 的**第二个子步骤** (per ADR-DGPU-02 §3.4 完整顺序):

1. `LifecycleProtocol` transition → `ShuttingDown` (拒新 ABI)
2. `worker_.shutdown_and_join()` ← 本 ADR 新增
3. `coalesce_timer_thread_.shutdown_and_join()`
4. `sim_thread_.join()` (通过 poison pill)
5. `soc_.reset()`
6. `eq_.reset()`

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
            try { cb(t.iova, t.size); } catch (...) {}
        }
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

---

## 6. 兼容性

| 项 | 影响 |
|----|------|
| **23 ABI** | 0 修改 (字节级冻结) |
| **JSON 配置** | 0 修改 |
| **`src/abi/cpptlm_emulator.cc`** | 0 修改 (ABI wrapper 已正确) |
| **`include/abi/cpptlm_emulator.h`** | 0 修改 |
| **下游 `set_irq_callback` 等 API** | 签名 0 修改, 仅内部实现改用 worker_ |
| **`coalesce_timer_`** | 不变, 仍用单独 joinable 线程 (per §3.2 注释保留) |

---

## 7. 性能影响

| 指标 | v1.0 | v2.0 (target) |
|------|------|---------------|
| MSI-X burst 100 IRQ 处理 | ~100ms (100 detached 线程 + 上下文切换) | < 5ms (worker 顺序处理) |
| 单 IRQ 延迟 | ~10us (线程创建) | < 1us (queue 推入) |
| 内存占用 | 100 detached 线程 × 8KB stack = 800KB | 1 worker 线程 × 8KB = 8KB |
| 调度确定性 | OS 决定 | worker 顺序处理 (确定性) |

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