# ADR-DGPU-02: LifecycleProtocol 状态机 + 统一析构顺序 + callback nullification 协议

> **状态**: 📋 提案
> **日期**: 2027-02-09
> **影响**: `include/tlm/gpu/dgpu_board_shell.{hh,cc}` (新增 `LifecycleProtocol` 内部组件 + 析构协议), 0 ABI 变更
> **关联架构文档**: [docs/architecture/14-dgpu-board-ideal-arch.md §3.1](../architecture/14-dgpu-board-ideal-arch.md)
> **配套**: ADR-DGPU-01 (CallbackWorker), ADR-DGPU-03 (DispatchRegistry)

---

## 1. 背景

### 1.1 问题

DGpuBoard v1.0 在生命周期管理上有 3 类缺陷:

#### 问题 A: 析构顺序依赖注释约定

`src/tlm/gpu/dgpu_board_shell.cc:585-616` 的 `destroy()` 严格 6 步顺序,但**仅靠代码注释 + try/catch 约定**:

```cpp
void DGpuBoard::destroy() {
    // Step 1: stop_=true
    stop_.store(true);

    // Step 2: 推 poison pill 唤醒 sim 线程
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        PendingReq poison;
        poison.trans_id = UINT64_MAX;
        inject_q_.push_back(std::move(poison));
    }

    // Step 3: join sim 线程
    if (sim_thread_.joinable()) {
        sim_thread_.join();
    }

    // Step 3.5 (C3): stop + notify + join 合并 timer 线程
    coalesce_stop_.store(true);
    coalesce_cv_.notify_all();
    if (coalesce_timer_.joinable()) {
        coalesce_timer_.join();
    }

    // Step 4: 析构 SOC
    soc_.reset();

    // Step 5: 析构 EventQueue
    eq_.reset();
}
```

**风险**: 任何后续维护者误改步骤顺序 (e.g., 把 Step 4 调到 Step 3 之前), 即引入 use-after-free。`coalesce_timer_thread` join 之前未清空 `irq_cb_` 就是真实存在的 race (per 审查报告 H2)。

#### 问题 B: callback nullification 缺失

v1.0 `coalesce_timer_thread_` join 路径(`dgpu_board_shell.cc:605-609`):

```cpp
coalesce_stop_.store(true);
coalesce_cv_.notify_all();
if (coalesce_timer_.joinable()) {
    coalesce_timer_.join();   // join 前, timer 仍可能持有 irq_cb_ 引用
}
```

但 timer 内部 `coalesce_arm_or_drain` 在阈值触发后调 `trigger_irq_async(rep)`, 后者读 `irq_cb_` 并**开 detached 线程执行**。

**race window**: timer join → detached 线程已起 → host callback 已使用 `irq_cb_` 引用 → DGpuBoard 析构 → `irq_cb_` 持有对象已析构 → use-after-free。

#### 问题 C: ABI 调用顺序契约缺失

v1.0 API 调用顺序完全靠注释 (`dgpu_board_shell.hh:102-104`):

```cpp
// 1. SOC 装配
bool load_soc_config(const nlohmann::json& board_cfg);
bool init();
void shutdown();

// 2. ABI 翻译入口(被 23 ABI C 函数调用,定义在 abi-export change)
int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len);
```

但**没有运行时校验**:
- 错序调用 `mmio_read` (在 `load_soc_config` 之前): silently 返回 -EINVAL (因为 SOC null)
- `set_irq_callback` 之后调 `shutdown`: callback 不被显式清空,可能在析构后被触发
- 同一 DGpuBoard 多次 `init()`: sim_thread 重复启动 (current code 用 `if (!sim_thread_.joinable())` 防, 但状态机无统一保障)

---

## 2. 决策

引入 **`LifecycleProtocol`** (5 态状态机) + **统一析构协议** (callback nullification + 严格 join 顺序)。

### 2.1 LifecycleProtocol 状态机

```cpp
namespace tlm::gpu {

enum class BoardState : uint8_t {
    Constructed   = 0,  // ctor 完成, 不可调内存
    Configured    = 1,  // load_soc_config 成功, SOC 已实例化
    Initialized   = 2,  // init() 完成, sim_thread 已起
    Running       = 3,  // tick() 正常推进, 接受所有 ABI
    ShuttingDown  = 4,  // shutdown/destroy 进入, 拒新 ABI
    Destructed    = 5,  // ~DGpuBoard 完成
};

// 状态机转换图:
//
//   ┌──────────────┐ load_soc_config  ┌──────────────┐
//   │ Constructed  │ ───────────────► │  Configured   │
//   └──────────────┘                  └───────┬──────┘
//                                            │ init()
//                                            ▼
//   ┌──────────────┐   first tick()   ┌──────────────┐
//   │ Destructed    │                  │  Initialized  │
//   └──────────────┘                  └───────┬──────┘
//                                            │ (auto, in init() or first tick)
//                                            ▼
//                                    ┌──────────────┐
//                                    │   Running     │
//                                    └───────┬──────┘
//                                            │ shutdown() or destroy()
//                                            ▼
//                                    ┌──────────────┐
//                                    │ ShuttingDown │
//                                    └───────┬──────┘
//                                            │ ~DGpuBoard()
//                                            ▼
//                                    ┌──────────────┐
//                                    │  Destructed   │
//                                    └──────────────┘
//
// 反向转换 (Destructed → Constructed): ❌ 不允许 (board 一次性使用)
//
// 拒入规则 (transition guard):
//   load_soc_config:    Constructed → Configured
//   init:               Configured  → Initialized
//   tick (first):       Initialized → Running (auto)
//   mmio_*/backdoor_*/config_*/msix_*/lookup_*: 需 ≥ Configured
//   trigger_*_async:    需 ≥ Initialized
//   set_*_callback:     需 ∈ [Configured, Initialized, Running] (≤ ShuttingDown)
//   shutdown():         any ≠ Destructed → ShuttingDown
//   ~DGpuBoard():       ShuttingDown → Destructed

class DGpuBoard::LifecycleProtocol {
public:
    // 原子状态转换 (失败时返 -EINVAL, 不破坏调用方)
    // 例如: transition(BoardState::Configured, BoardState::Initialized, "init()")
    int transition(BoardState expected, BoardState next, const char* who);

    // 校验当前状态是否 ≥ target (public 用于 ABI 调用 guard)
    bool is_at_least(BoardState target) const noexcept;

    // 当前状态访问器 (测试/诊断)
    BoardState current() const noexcept;

    // 历史追踪 (测试断言: 状态转换序列)
    struct HistoryEntry {
        BoardState from;
        BoardState to;
        std::string who;
        uint64_t sequence;
    };
    std::vector<HistoryEntry> history_snapshot() const noexcept;

private:
    std::atomic<BoardState> current_{BoardState::Constructed};
    mutable std::mutex      history_mu_;
    std::vector<HistoryEntry> history_;
    std::atomic<uint64_t>   sequence_{0};
};

} // namespace tlm::gpu
```

### 2.2 统一析构协议 (callback nullification + 严格 join 顺序)

```cpp
// DGpuBoard::destroy() 重构后
void DGpuBoard::destroy() {
    // ─── Step 0: 进入 ShuttingDown (拒新 ABI) ───
    lifecycle_.transition(BoardState::Running,      BoardState::ShuttingDown, "destroy()");
    // (如果当前是 Configured/Initialized/Constructed, 也允许 transition 到 ShuttingDown,
    //  用 is_at_least 守卫)

    // ─── Step 1: callback nullification ───
    // 关键: 在 join 任何 worker thread 之前, 先让 callback 指针 null,
    // 防止 in-flight task 触发 use-after-free。
    // (worker loop 在 dispatch_task 之前还会再校验 stop_, 但 nullification
    //  是双重保险)
    {
        std::lock_guard<std::mutex> lock(callback_mu_);
        irq_cb_ = nullptr;
        dma_translate_cb_ = nullptr;
        error_cb_ = nullptr;
    }

    // ─── Step 2: CallbackWorker shutdown (per ADR-DGPU-01) ───
    worker_.shutdown_and_join();

    // ─── Step 3: coalesce_timer shutdown (C3 MSI-X 合并器) ───
    coalesce_stop_.store(true);
    coalesce_cv_.notify_all();
    if (coalesce_timer_.joinable()) {
        coalesce_timer_.join();
    }

    // ─── Step 4: sim_thread shutdown (poison pill) ───
    stop_.store(true);
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        PendingReq poison;
        poison.trans_id = UINT64_MAX;
        inject_q_.push_back(std::move(poison));
    }
    if (sim_thread_.joinable()) {
        sim_thread_.join();
    }

    // ─── Step 5: 析构 SOC ───
    soc_.reset();

    // ─── Step 6: 析构 EventQueue ───
    eq_.reset();

    // ─── Step 7: lifecycle 终态 ───
    lifecycle_.transition(BoardState::ShuttingDown, BoardState::Destructed, "~DGpuBoard");
}
```

### 2.3 ABI 调用 guard

每个 ABI 入口添加 `lifecycle_.is_at_least(BoardState::Configured)` 检查:

```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (!lifecycle_.is_at_least(BoardState::Configured)) {
        return -ENOSYS;  // 未配置不可用
    }
    if (lifecycle_.is_at_least(BoardState::ShuttingDown)) {
        return -ESHUTDOWN;  // 关闭中拒新请求
    }
    // ... 原有逻辑
}

void DGpuBoard::set_irq_callback(IrqCallback cb) {
    if (lifecycle_.current() == BoardState::Constructed ||
        lifecycle_.current() == BoardState::Destructed) {
        return;  // 配置前 / 析构后拒设
    }
    if (lifecycle_.current() == BoardState::ShuttingDown) {
        return;  // 关闭中拒设
    }
    std::lock_guard<std::mutex> lock(callback_mu_);
    irq_cb_ = std::move(cb);
}
```

---

## 3. 关键不变性

### Inv-1: 状态转换原子性

```cpp
int LifecycleProtocol::transition(BoardState expected, BoardState next, const char* who) {
    BoardState cur = current_.load(std::memory_order_acquire);
    if (cur != expected) {
        return -EINVAL;  // 错序转换
    }
    if (!current_.compare_exchange_strong(cur, next)) {
        return -EINVAL;  // CAS 失败 (并发转换)
    }
    // 记录历史 (用于测试断言 + 调试)
    {
        std::lock_guard<std::mutex> lock(history_mu_);
        history_.push_back({expected, next, who, sequence_.fetch_add(1)});
    }
    return 0;
}
```

**保证**: 错序转换返 -EINVAL, 不破坏当前状态。

### Inv-2: callback nullification 在 worker join 之前

```cpp
void DGpuBoard::destroy() {
    lifecycle_.transition(...);  // Step 0
    {
        std::lock_guard<std::mutex> lock(callback_mu_);
        irq_cb_ = nullptr;
        dma_translate_cb_ = nullptr;
        error_cb_ = nullptr;
    }   // Step 1 (callback nullification)
    worker_.shutdown_and_join();   // Step 2
    // ... 后续
}
```

**保证**: 即使 worker 的 in-flight task 在 join 前取出, dispatch 时 callback 指针已 null (由 Inv-3 二次校验)。

### Inv-3: worker dispatch 二次 stop 校验 (per ADR-DGPU-01 Inv-2)

```cpp
void CallbackWorker::dispatch_task(const Task& t) {
    if (stop_.load(std::memory_order_acquire)) {
        return;  // 已停止, 不再 dispatch
    }
    // ...
    std::lock_guard<std::mutex> lock(callback_mu_);
    auto cb = irq_cb_;   // 即使调用前为非空, 已被 destroy Step 1 置 null
    if (cb) cb(t.vector_id);
}
```

**保证**: 三重保险 — stop_ atomic + callback nullification + lock 内的指针拷贝。

### Inv-4: ABI 调用 guard 一致性

| ABI 函数 | 最低状态 | 最高状态 | 失败返值 |
|---------|---------|---------|---------|
| `mmio_read/write` | Configured | Running | -ENOSYS / -ESHUTDOWN |
| `backdoor_read/write` | Configured | Running | -ENOSYS / -ESHUTDOWN |
| `pcie_config_read/write` | Configured | Running | -ENOSYS / -ESHUTDOWN |
| `msix_init/update_pending/clear_pending` | Configured | Running | -ENOSYS / -ESHUTDOWN |
| `lookup_register` | Configured | Running | -ENOSYS / -ESHUTDOWN |
| `set_irq_callback` | Configured | Running | (no return, void) |
| `set_dma_translate_callback` | Configured | Running | (void) |
| `set_error_callback` | Configured | Running | (void) |
| `trigger_irq_async` | Initialized | Running | (void, no-op if false) |
| `trigger_dma_translate_async` | Initialized | Running | (void) |
| `trigger_error_async` | Initialized | Running | (void) |
| `tick` | Initialized | ShuttingDown | (void) |
| `shutdown` | any ≠ Destructed | — | (void) |
| `~DGpuBoard` | — | — | (void) |

---

## 4. 实施步骤

### 4.1 阶段 A: 基础设施 (W25)

| 任务 | 产出 |
|------|------|
| A1: 新增 `dgpu_board_lifecycle.hh` 头文件 | LifecycleProtocol API 完整 |
| A2: 单元测试 `test_dgpu_board_lifecycle.cc` (16 case, 含 transition guard + history) | 100% PASS |

### 4.2 阶段 B: 集成 (W26)

| 任务 | 产出 |
|------|------|
| B1: `DGpuBoard` 添加 `LifecycleProtocol lifecycle_` 成员 | 状态机就位 |
| B2: `DGpuBoard::destroy()` 重构为 7 步 (含 callback nullification) | 析构协议强制 |
| B3: 23 ABI 函数添加 `is_at_least` guard | 拒错序调用 |
| B4: `set_*_callback` 添加状态守卫 | 防关闭中设置 |
| B5: `init()`/`load_soc_config`/`shutdown` 调用 `lifecycle_.transition()` | 状态转换显式 |

### 4.3 验证

- [ ] `test_dgpu_board_lifecycle.cc` 16 case 100% PASS
- [ ] 44498 assertions 100% PASS (0 regression)
- [ ] Oracle 评审通过 (析构协议是关键安全特性)

---

## 5. 测试策略

### 5.1 状态机单元测试

```cpp
TEST_CASE("LifecycleProtocol basic transition") {
    DGpuBoard::LifecycleProtocol lp;
    REQUIRE(lp.current() == BoardState::Constructed);

    REQUIRE(lp.transition(BoardState::Constructed, BoardState::Configured,
                          "load_soc_config()") == 0);
    REQUIRE(lp.current() == BoardState::Configured);

    REQUIRE(lp.transition(BoardState::Configured, BoardState::Initialized,
                          "init()") == 0);
    REQUIRE(lp.current() == BoardState::Initialized);
}

TEST_CASE("LifecycleProtocol rejects wrong order") {
    DGpuBoard::LifecycleProtocol lp;
    REQUIRE(lp.transition(BoardState::Constructed, BoardState::Initialized,
                          "skip Configured") == -EINVAL);
    REQUIRE(lp.current() == BoardState::Constructed);
}

TEST_CASE("LifecycleProtocol history tracking") {
    DGpuBoard::LifecycleProtocol lp;
    lp.transition(BoardState::Constructed, BoardState::Configured, "test1");
    lp.transition(BoardState::Configured, BoardState::Initialized, "test2");

    auto h = lp.history_snapshot();
    REQUIRE(h.size() == 2);
    REQUIRE(h[0].from == BoardState::Constructed);
    REQUIRE(h[0].to == BoardState::Configured);
    REQUIRE(h[0].who == "test1");
    REQUIRE(h[1].from == BoardState::Configured);
    REQUIRE(h[1].to == BoardState::Initialized);
}

TEST_CASE("LifecycleProtocol is_at_least") {
    DGpuBoard::LifecycleProtocol lp;
    REQUIRE(!lp.is_at_least(BoardState::Configured));
    lp.transition(BoardState::Constructed, BoardState::Configured, "test");
    REQUIRE(lp.is_at_least(BoardState::Configured));
    REQUIRE(!lp.is_at_least(BoardState::Initialized));
}
```

### 5.2 析构协议集成测试

```cpp
TEST_CASE("DGpuBoard destroy: callback nullification before worker join") {
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg));

    std::atomic<bool> called{false};
    board.set_irq_callback([&](uint32_t) { called.store(true); });
    REQUIRE(board.init());
    board.tick();

    // 在 init 状态下调 msix_update_pending 触发 callback
    board.msix_update_pending(0);
    board.tick();

    // 进入 destroy
    board.shutdown();
    REQUIRE(!board.lifecycle_is_at_least(BoardState::Configured));
    REQUIRE(board.lifecycle_current() == BoardState::Destructed);
    // callback 已 null, 即使 host 端仍持引用, 也不会被调
    REQUIRE(!called.load());
}

TEST_CASE("DGpuBoard ABI guard rejects pre-init calls") {
    DGpuBoard board("test");
    // 未调 load_soc_config
    uint8_t buf[4];
    REQUIRE(board.mmio_read(0, 0, buf, 4) == -ENOSYS);
}

TEST_CASE("DGpuBoard ABI guard rejects post-shutdown calls") {
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg));
    REQUIRE(board.init());
    board.shutdown();
    uint8_t buf[4];
    REQUIRE(board.mmio_read(0, 0, buf, 4) == -ESHUTDOWN);
}
```

---

## 6. 兼容性

| 项 | 影响 |
|----|------|
| **23 ABI 签名** | 0 修改 |
| **23 ABI 语义** | 新增 -ENOSYS / -ESHUTDOWN 返回值 (符合 POSIX 语义) |
| **JSON 配置** | 0 修改 |
| **下游 `set_*_callback` API** | 签名不变, 内部增加守卫 |
| **`shutdown()` API** | 签名不变, 内部增加状态转换 |
| **`~DGpuBoard()`** | 析构逻辑重构, 外部行为不变 |

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: 状态机本身引入死锁 | 低 | 严重 | 测试覆盖 16 transition, history 跟踪确保无漏 transition |
| **R2**: ABI guard -ENOSYS 破坏现有测试预期 | 中 | 测试 FAIL | 测试通常调 load_soc_config 后再调 mmio_*, 实际不破; 全量回归覆盖 |
| **R3**: callback nullification 与 in-flight task race | 低 | use-after-free | Inv-2 + Inv-3 三重保险 (atomic stop + null + 指针拷贝) |
| **R4**: shutdown 重复调用 | 低 | 状态机混乱 | transition CAS 失败返 -EINVAL, 不破坏状态 |

---

## 8. 参考

- **架构文档**: [14-dgpu-board-ideal-arch.md §3.1](../architecture/14-dgpu-board-ideal-arch.md)
- **ADR-DGPU-01**: CallbackWorker (worker shutdown 协议)
- **ADR-DGPU-03**: DispatchRegistry (不涉及)
- **v1.0 实现**: `src/tlm/gpu/dgpu_board_shell.cc:585-616` (destroy)
- **同类模式参考**: `coalesce_timer_` shutdown (per §3.5) — 当前实现的不完整 version
- **同类模式参考**: SimModule::do_reset (ResetConfig 模式)

---

**维护**: CppTLM 开发团队
**状态**: 📋 提案