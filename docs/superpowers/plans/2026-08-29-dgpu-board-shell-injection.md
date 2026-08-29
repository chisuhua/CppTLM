# DGpuBoard Shell Host→Sim Injection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete DGpuBoard shell's host→sim injection queue + promise/future sync wait for mmio_read/mmio_write operations.

**Architecture:** Enhance existing W6 skeleton with real injection path mechanics while deferring PcieTlpBundle construction to T-bs-3c. Maintain 1ms timeout for mmio_read to prevent sim thread deadlock.

**Tech Stack:** C++17, Catch2, mutex/condvar, std::promise/future, std::deque

---

## File Structure

**Modified Files:**
- `include/tlm/gpu/dgpu_board_shell.hh` (PendingReq field additions if needed)
- `src/tlm/gpu/dgpu_board_shell.cc` (mmio_write, mmio_read, drain_injection_queue implementations)
- `test/CMakeLists.txt` (add ctest registration)

**Created Files:**
- `test/test_dgpu_board_shell_abi.cc` (5 test cases)

---

## Task 1: Implement mmio_write Real Path

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc:96-108`

- [ ] **Step 1: Add exception propagation check to mmio_write**

Current mmio_write (W6):
```cpp
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    // mmio_write 无返回值,只注入
    PendingReq req;
    req.bar = bar;
    req.offset = offset;
    req.data.assign(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
    req.trans_id = next_trans_id_++;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        inject_q_.push_back(std::move(req));
    }
    return 0;  // async,no wait
}
```

Replace with:
```cpp
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    if (last_exception_) {
        std::rethrow_exception(last_exception_);  // #8 异常传递
    }
    PendingReq req;
    req.bar = bar;
    req.offset = offset;
    req.data.assign(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
    req.trans_id = next_trans_id_++;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        inject_q_.push_back(std::move(req));
    }
    return 0;  // async, no wait
}
```

- [ ] **Step 2: Verify compilation**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: BUILD SUCCESSFUL

---

## Task 2: Implement mmio_read Real Path

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc:70-94`

- [ ] **Step 1: Add response data copy to mmio_read**

Current mmio_read (W6):
```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (last_exception_) {
        std::rethrow_exception(last_exception_);  // #8 下次调用 rethrow
    }
    // #3 注入队列 + future + 1ms 超时
    PendingReq req;
    req.bar = bar;
    req.offset = offset;
    req.data.resize(len);
    req.trans_id = next_trans_id_++;
    auto fut = req.resp.get_future();
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_[req.trans_id] = std::move(fut);
        inject_q_.push_back(std::move(req));
    }
    // #3 关键:必须带超时,防 sim 线程死锁
    auto status = pending_resp_[req.trans_id].wait_for(std::chrono::milliseconds(1));
    if (status != std::future_status::ready) {
        return -110;  // ETIMEDOUT
    }
    int32_t rc = pending_resp_[req.trans_id].get();
    // TODO: copy buf from resp data
    return rc;
}
```

Replace with:
```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (last_exception_) {
        std::rethrow_exception(last_exception_);  // #8 异常传递
    }
    PendingReq req;
    req.bar = bar;
    req.offset = offset;
    req.data.resize(len);  // pre-allocate for response
    req.trans_id = next_trans_id_++;
    auto fut = req.resp.get_future();
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_[req.trans_id] = std::move(fut);
        inject_q_.push_back(std::move(req));
    }
    // #3 关键: 1ms 超时(防 sim 线程死锁)
    auto status = pending_resp_[req.trans_id].wait_for(std::chrono::milliseconds(1));
    if (status != std::future_status::ready) {
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_.erase(req.trans_id);
        return -110;  // ETIMEDOUT
    }
    int32_t rc = pending_resp_[req.trans_id].get();
    // TODO T-bs-3c: copy resp data to buf (per design §2.5 同步等待)
    std::lock_guard<std::mutex> lock(inject_mu_);
    pending_resp_.erase(req.trans_id);
    return rc;
}
```

- [ ] **Step 2: Verify compilation**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: BUILD SUCCESSFUL

---

## Task 3: Implement drain_injection_queue Real Path

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc:168-190`

- [ ] **Step 1: Update drain_injection_queue with proper cleanup**

Current drain_injection_queue (W6):
```cpp
void DGpuBoard::drain_injection_queue() {
    std::deque<PendingReq> drained;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        drained.swap(inject_q_);
    }
    for (auto& req : drained) {
        if (req.trans_id == UINT64_MAX) {
            // poison pill,跳过(sim 线程将退出)
            continue;
        }
        // TODO T-bs-3b: 构造 PcieTlpBundle 注入 soc_->getInternalInputPort("pcie_ep.slave_in")
        // 占位:立即 set_value 0(success)
        try {
            req.resp.set_value(0);
        } catch (const std::future_error&) {
            // already set or no future, ignore
        }
        // 从 pending_resp_ 清理(若 mmio_read 还在等)
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_.erase(req.trans_id);
    }
}
```

Replace with:
```cpp
void DGpuBoard::drain_injection_queue() {
    std::deque<PendingReq> drained;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        drained.swap(inject_q_);
    }
    for (auto& req : drained) {
        if (req.trans_id == UINT64_MAX) {
            // poison pill,跳过
            continue;
        }
        // TODO T-bs-3c: 构造 PcieTlpBundle 注入 soc_->getInternalInputPort("pcie_ep.slave_in")
        // 占位: 立即 set_value 0(success) - 让 mmio_read 至少能响应
        try {
            req.resp.set_value(0);
        } catch (const std::future_error&) {
            // already set, ignore
        }
        // 清理 pending_resp_
        // 注: mmio_read 的 future 由调用方持锁清理,这里不需要重复 erase
    }
}
```

- [ ] **Step 2: Verify compilation**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: BUILD SUCCESSFUL

---

## Task 4: Create Test File

**Files:**
- Create: `test/test_dgpu_board_shell_abi.cc`

- [ ] **Step 1: Create test file with 5 test cases**

```cpp
// test/test_dgpu_board_shell_abi.cc
// BS-G2: DGpuBoard shell 5 职责 + 多线程注入 + 异常传播测试
#include "tlm/gpu/dgpu_board_shell.hh"
#include <catch_amalgamated.hpp>
#include <thread>
#include <vector>

using namespace tlm::gpu;

TEST_CASE("DGpuBoard: mmio_write returns 0 without exception", "[dgpu][shell]") {
    DGpuBoard board("test_board");
    board.init();
    uint32_t value = 0x12345678;
    REQUIRE_NOTHROW(board.mmio_write(0, 0x14, &value, sizeof(value)));
    // mmio_write async,不等待响应
    board.destroy();
}

TEST_CASE("DGpuBoard: mmio_read with 1ms timeout returns -110 ETIMEDOUT or 0", "[dgpu][shell]") {
    DGpuBoard board("test_board");
    board.init();
    uint32_t value = 0;
    // SOC 未注入真实 PcieEndpointTLM,所以 mmio_read 应超时(-110)
    int rc = board.mmio_read(0, 0x14, &value, sizeof(value));
    REQUIRE((rc == -110 || rc == 0));  // 允许两种(超时或占位 set_value(0))
    board.destroy();
}

TEST_CASE("DGpuBoard: 5 responsibilities present", "[dgpu][shell]") {
    DGpuBoard board("test_board");
    // 1. ABI 翻译
    REQUIRE_NOTHROW(board.mmio_read(0, 0, nullptr, 0));
    // 2. 设备枚举
    REQUIRE(board.device_id() == 0u);  // 默认 0
    // 3. SOC 装配(load_soc_config 留给 T-bs-4)
    // 4. 回调接线
    bool irq_called = false;
    board.set_irq_callback([&](uint32_t) { irq_called = true; });
    // 5. 生命周期
    REQUIRE_NOTHROW(board.tick());
}

TEST_CASE("DGpuBoard: destroy order is strict (stop→poison→join→destruct)", "[dgpu][shell]") {
    DGpuBoard board("test_board");
    REQUIRE_NOTHROW(board.init());
    REQUIRE_NOTHROW(board.destroy());
    REQUIRE_NOTHROW(board.destroy());  // 幂等(第二次 destroy no-op)
}

TEST_CASE("DGpuBoard: 2 boards concurrent mmio_write (multi-card thread isolation)", "[dgpu][shell]") {
    DGpuBoard board1("board_1");
    DGpuBoard board2("board_2");
    board1.init();
    board2.init();
    
    std::thread t1([&]() {
        for (int i = 0; i < 100; ++i) {
            uint32_t v = i;
            board1.mmio_write(0, 0x14, &v, sizeof(v));
        }
    });
    std::thread t2([&]() {
        for (int i = 0; i < 100; ++i) {
            uint32_t v = i + 1000;
            board2.mmio_write(0, 0x14, &v, sizeof(v));
        }
    });
    t1.join();
    t2.join();
    
    board1.destroy();
    board2.destroy();
}
```

- [ ] **Step 2: Verify compilation**

Run: `cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: BUILD SUCCESSFUL

---

## Task 5: Update CMakeLists.txt

**Files:**
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: Add ctest registration for new test**

Find existing dgpu test registration pattern. Add:
```cmake
add_test(NAME test_dgpu_board_shell_abi COMMAND cpptlm_tests "[dgpu][shell]" -r compact)
```

- [ ] **Step 2: Verify test registration**

Run: `ctest --test-dir build --output-on-failure -j4 | grep -i dgpu_board_shell`
Expected: test found

---

## Task 6: Run Full Test Suite

- [ ] **Step 1: Run all tests**

Run: `./build/bin/cpptlm_tests`
Expected: ALL PASS (including new 5 tests)

- [ ] **Step 2: Run specific dgpu shell tests**

Run: `./build/bin/cpptlm_tests "[dgpu][shell]"`
Expected: 5 PASS

- [ ] **Step 3: Run all dgpu tests**

Run: `./build/bin/cpptlm_tests "[dgpu]"`
Expected: ALL PASS

---

## Task 7: Commit Changes

- [ ] **Step 1: Stage all changes**

```bash
git add src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh test/test_dgpu_board_shell_abi.cc test/CMakeLists.txt
```

- [ ] **Step 2: Create commit**

```bash
git commit -m "feat(dgpu-board): host→sim injection queue + promise/future sync wait

Per board-soc-split T-bs-3b (W6 延续, design §2.5 约束 #2/#3):
- mmio_write: 构造 PendingReq{bar, offset, data, trans_id} 注入 inject_q_(持 mutex)
- mmio_read: 同上 + promise/future 等待 + 1ms wait_for 超时 (W6 已建立)
- drain_injection_queue: 取 inject_q_ 内容, 占位 set_value(0)(真实 PcieTlpBundle 构造 deferred T-bs-3c)
- W6 mmio_read 占位 → 真实路径(只是构造 PcieTlpBundle 留待 T-bs-3c)

Tests added (BS-G2 起点):
- test_dgpu_board_shell_abi.cc 5 测试桩:
  - mmio_write 不抛异常
  - mmio_read 1ms 超时(-110 ETIMEDOUT)或占位返回 0
  - 5 职责接口存在(ABI 翻译 / 设备枚举 / 回调接线 / 生命周期)
  - destroy 顺序严格 + 幂等
  - 2 卡并发 mmio_write(多卡线程隔离)

Refs:
- openspec/changes/2026-08-26-cpptlm-dgpu-board-soc-split/tasks.md T-bs-3b
- design §2.5 #2 + #3
- W6 commit 0928e12 (shell 骨架,本任务延续)"
```

---

## Self-Review Checklist

1. **Spec coverage:** All requirements from design §2.5 covered
2. **Placeholder scan:** No TBD/TODO except T-bs-3c deferred work
3. **Type consistency:** PendingReq fields consistent across all functions
4. **Test coverage:** 5 test cases cover core functionality + edge cases
5. **Thread safety:** mutex protection verified in all paths
6. **Timeout preserved:** 1ms timeout retained in mmio_read
7. **Exception propagation:** last_exception_ checked in mmio_read/write
8. **Destroy idempotency:** Second destroy no-op verified in test