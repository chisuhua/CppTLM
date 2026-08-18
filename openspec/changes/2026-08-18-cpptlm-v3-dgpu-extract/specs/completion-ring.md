# Spec: CompletionRing — host_notify 重设计规格

> **配套**: [../design.md §3.4](../design.md) · [../proposal.md](../proposal.md) · [../tasks.md §P1 轨 B + P3 重构](../tasks.md)
> **状态**: 📋 Spec — W1-3 骨架，P3 重构完善
> **Owner**: CppTLM Team
> **关联 ADR**: [ADR-X.15-cpptlm-v3-dgpu-extract §4.2 P1 轨 B](../../../docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md)
> **关联上游**: UsrLinuxEmu ADR-090 v2 §D3.4 (CompletionRing host_notify 重设计)
> **替代**: `AsyncCompletionAdapter::setCompletionCallback` (旧 std::function 回调表方案)

---

## 1. 范围 (Scope)

本规格定义 `CompletionRing` 接口契约——dGPU 板卡 CQ (Completion Queue) 的重设计：

- **替代路径**: `AsyncCompletionAdapter::setCompletionCallback`（旧 std::function 回调表方案）
- **新方案**: `CompletionRing::push` + `CompletionRing::set_host_notify`（NVMe 风格）
- **重设计理由** (per UsrLinuxEmu ADR-090 v2 §D3.4): 避免 `std::function` 回调表在多 stream 并发下的死锁/饥饿问题

---

## 2. 公共 API

### 2.1 接口定义

```cpp
// include/tlm/gpu/completion_ring.hh
#ifndef CPPTLM_COMPLETION_RING_H
#define CPPTLM_COMPLETION_RING_H

#include <cstdint>
#include <functional>
#include <vector>

namespace tlm {

class CompletionRing {
public:
    struct Entry {
        uint64_t image_id;    // 来自 ISmExecutor::dispatch 的 image handle
        int32_t  status;      // 0=成功, 非 0=cudaError_t
        uint64_t timestamp;   // cycle 时间戳 (来自仿真时钟)
    };

    void push(uint64_t image_id, int32_t status);

    // host_notify 钩子: 替代 MSI-X 真实硬件路径
    using HostNotifyHook = std::function<void()>;
    void set_host_notify(HostNotifyHook hook);

    // CQ consumer 读取
    Entry pop();
    size_t pending_count() const;

private:
    std::vector<Entry> ring_;
    HostNotifyHook host_notify_;
};

}  // namespace tlm

#endif  // CPPTLM_COMPLETION_RING_H
```

### 2.2 命名空间

- `namespace tlm` — 与 CppTLM 仓全局惯例一致

---

## 3. 行为契约

### 3.1 push(image_id, status)

- **Precondition**: 无
- **Side effects**:
  1. 创建 `Entry { image_id, status, timestamp=current_cycle() }` 加入内部 vector
  2. **异步触发** `host_notify_()`（如果注册）

- **线程安全**:
  - device 端（push）单线程调用
  - 与 host 端 pop 跨线程并发通过内部 mutex 保护（实现细节）

- **错误**: 无返回值（错误已通过 status 字段表达）

### 3.2 set_host_notify(hook)

- **Precondition**: 无
- **Postcondition**:
  - 成功: 注册 hook，后续 push() 触发调用 `hook()`
  - `hook = nullptr` 表示清除 hook

- **线程安全**:
  - 仅 device 端单线程调用（设置 hook）

- **重入保证**:
  - hook 内部**不应**直接调 `CompletionRing::pop()`（可能死锁）
  - 应通过 `Entry` 队列中间层

### 3.3 pop()

- **Precondition**: 无
- **Postcondition**:
  - 若 ring 非空: 返回并移除最早 Entry
  - 若 ring 空: 返回 `Entry { image_id=0, status=0, timestamp=0 }`（哨兵值）

- **线程安全**:
  - host 端单线程调用（cuStreamSynchronize 等待时）

### 3.4 pending_count() const

- **Postcondition**: 返回 ring 当前深度
- **线程安全**: device push / host pop 跨线程可见

---

## 4. 重设计理由 (per UsrLinuxEmu ADR-090 v2 §D3.4)

### 4.1 旧方案的问题: `AsyncCompletionAdapter::setCompletionCallback`

```cpp
// 旧方案 (v2.1)
class AsyncCompletionAdapter {
public:
    void setCompletionCallback(std::function<void(uint64_t, int32_t)> cb);
    // 内部: std::unordered_map<uint64_t, std::function> callbacks_;
};
```

**问题**:
1. **多 stream 并发死锁**: host 端 `cuStreamSynchronize(streamA)` 同时 device 端 push 到 streamA 的 callback，host 持锁等 pop，device 持锁等 push
2. **std::function 回调表内存增长**: 长时间运行仿真累积大量 std::function 对象
3. **回调嵌套**: callback 内调 cuda API 可能再次触发 callback，无界递归

### 4.2 新方案: CompletionRing + host_notify

```cpp
// 新方案 (v3.0)
class CompletionRing {
    void push(image_id, status);            // device 单线程
    Entry pop();                             // host 单线程（cuStreamSynchronize）
    void set_host_notify(hook);              // 替代 std::function 回调表
};
```

**改进**:
1. **无死锁**: device push（写 vector） + host pop（读 vector）通过 mutex 串行化，无回调嵌套
2. **内存稳定**: vector 容量增长可控（max ring size）
3. **替代 MSI-X 真实硬件路径**: host_notify 是模拟 MSI-X 中断的软件 hook，与真实硬件语义对齐

---

## 5. 与 ISmExecutor 集成

### 5.1 SmExecutorImpl 持有 CompletionRing

```cpp
// src/tlm/gpu/sm_executor_impl.cc
class SmExecutorImpl : public ISmExecutor {
public:
    SmExecutorImpl()
        : ptx_emu_("libptxemu_device.so"),
          completion_ring_(std::make_unique<CompletionRing>()) {}

    int dispatch(SmImageId image, uint64_t args_vram_addr, size_t args_size,
                 const DispatchParams& params) override {
        // 1. map args_vram_addr → host pointer
        const void* args_ptr = map_vram_to_host(args_vram_addr, args_size);

        // 2. 异步调 PtxEmuSubmodule::image_execute
        int32_t status = ptx_emu_.image_execute(image, params, args_ptr, args_size);

        // 3. push 到 CompletionRing（同步路径，PTX-EMU 端 advance 后）
        completion_ring_->push(image, status);

        return 0;  // 成功提交（异步）
    }

private:
    PtxEmuSubmodule ptx_emu_;
    std::unique_ptr<CompletionRing> completion_ring_;
};
```

### 5.2 setCompletionCallback 转调

```cpp
int SmExecutorImpl::setCompletionCallback(CompletionCallback cb) override {
    if (!cb) {
        completion_ring_->set_host_notify(nullptr);
        return 0;
    }
    // 转调 CompletionRing::set_host_notify
    completion_ring_->set_host_notify([cb, this]() {
        // pop 所有 entry，调 callback
        while (auto entry = completion_ring_->pop()) {
            if (entry.image_id != 0) {  // 跳过哨兵值
                cb(entry.image_id, entry.status);
            }
        }
    });
    return 0;
}
```

---

## 6. 与 host_notify 链路集成

### 6.1 host_notify → HAL fence_signal

```cpp
// CppTLM 端 host_notify hook (per UsrLinuxEmu ADR-090 v2 §D3.4)
completion_ring_->set_host_notify([]() {
    // 通知 HAL 层 fence 已 signal
    hal_fence_signal(cpp_hal::FENCE_GPU_COMPLETION);
});
```

### 6.2 UsrLinuxEmu FenceRegistry 集成

- UsrLinuxEmu 端 `FenceRegistry` 注册 `FENCE_GPU_COMPLETION` 类型
- `cuStreamSynchronize` 等 fence 时，HAL 阻塞直到 host_notify 触发
- fence signal 后 host 端继续推进（cuLaunchKernel / cuStreamSynchronize 返回）

---

## 7. 错误处理

### 7.1 push 错误

| 错误源 | 处理 |
|---|---|
| image_id 无效 | status = CUDA_ERROR_INVALID_HANDLE（由 device 端设置）|
| status 字段 | 通过 Entry.status 表达（不抛）|

### 7.2 pop 错误

| 错误源 | 处理 |
|---|---|
| ring 空 | 返回哨兵 Entry `{0, 0, 0}`（host 端判断哨兵值）|

### 7.3 host_notify 错误

| 错误源 | 处理 |
|---|---|
| hook 内部抛 | 不传播（异步路径，与真实硬件 IRQ 一致）|

---

## 8. 测试要求

### 8.1 单元测试

| 测试 | 内容 | Catch2 标签 |
|---|---|---|
| push + pending_count | push 后 pending_count = 1 | `[completion-ring]` |
| push + pop round-trip | push 后 pop 返回相同 Entry | `[completion-ring]` |
| host_notify 触发 | push 后 hook 被调 | `[completion-ring]` |
| pop 哨兵值 | 空 ring pop 返回 `{0, 0, 0}` | `[completion-ring]` |
| 清除 host_notify | `set_host_notify(nullptr)` 后 push 不触发 hook | `[completion-ring]` |
| 并发 push/pop | 跨线程 push + pop 数据一致 | `[completion-ring]` |

### 8.2 集成测试 (W6-8 P3)

- ✅ `SmExecutorImpl::dispatch` → CompletionRing::push 链路
- ✅ CompletionRing::set_host_notify → HAL fence_signal → FenceRegistry 链路
- ✅ cuStreamSynchronize 等 fence 链路
- ✅ Mode A/B dual-rail E2E（5 类 microbenchmark）

---

## 9. 接口稳定性 (Interface Stability)

### 9.1 冻结接口（P3 重构后禁止变更）

- ✅ `CompletionRing::Entry` 结构（image_id + status + timestamp）
- ✅ `push` / `pop` / `pending_count` 签名
- ✅ `HostNotifyHook` 类型

### 9.2 可演进

- 🟡 ring 容量上限（当前无限，可加 max_size + drop-oldest 策略）
- 🟡 hook 重入检测（可加 reentrancy guard）

---

**维护**: CppTLM Team
**状态**: 📋 Spec — W1-3 骨架，P3 重构完善
**下次更新**: W3 骨架完成后 / W8 P3 重构完成时
