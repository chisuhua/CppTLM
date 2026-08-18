# Spec: ISmExecutor — 汇合点 3 ABI 规格

> **配套**: [../design.md §3.6](../design.md) · [../proposal.md](../proposal.md) · [../tasks.md §P2 收敛](../tasks.md)
> **状态**: 📋 Spec — W4-6 实施，接口冻结（与 UsrLinuxEmu ADR-090 v2 §D3.3 同步）
> **Owner**: CppTLM Team
> **关联 ADR**: [ADR-X.15-cpptlm-v3-dgpu-extract §4.3 P2 收敛](../../../docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md)
> **关联上游**: UsrLinuxEmu ADR-090 v2 §D3.3 (ISmExecutor 3 ABI)

---

## 1. 范围 (Scope)

本规格定义 `ISmExecutor` 接口契约——dGPU 板卡架构中的 SM 执行器汇合点：
- **ABI 1**: `installImage` (PTXIR bytes 在 VRAM via H2D DMA)
- **ABI 2**: `dispatch` (args buffer 在 VRAM)
- **ABI 3**: `setCompletionCallback` (实际转调 CompletionRing)

**实现**: `SmExecutorImpl`（`src/tlm/gpu/sm_executor_impl.cc`）内部调 `PtxEmuSubmodule` + CompletionRing。

---

## 2. 公共 API

### 2.1 接口定义

```cpp
// include/tlm/gpu/is_m_executor.hh
#ifndef CPPTLM_ISM_EXECUTOR_H
#define CPPTLM_ISM_EXECUTOR_H

#include <cstdint>
#include <functional>

namespace tlm {

// Image ID 类型 (per PtxEmuSubmodule::image_load 返回值)
using SmImageId = uint64_t;

// Dispatch 参数 (per CppTLM KernelLaunchRequest 子集)
struct DispatchParams {
    uint32_t gx, gy, gz;    // grid 维度
    uint32_t bx, by, bz;    // block 维度
    size_t   shared_mem;    // 动态共享内存字节数
};

// Completion callback 类型
using CompletionCallback = std::function<void(SmImageId image_id, int32_t status)>;

class ISmExecutor {
public:
    virtual ~ISmExecutor() = default;

    // ABI 1: install image (PTXIR bytes 已在 VRAM via H2D DMA)
    virtual int installImage(uint64_t vram_addr, size_t size,
                              SmImageId* out_id) = 0;

    // ABI 2: dispatch (args buffer 也在 VRAM)
    virtual int dispatch(SmImageId image,
                          uint64_t args_vram_addr, size_t args_size,
                          const DispatchParams& params) = 0;

    // ABI 3 (optional): completion callback — 实际用 CompletionRing
    // per UsrLinuxEmu ADR-090 v2 §D3.4 重设计: 替代为 CompletionRing::set_host_notify
    // 保留接口以兼容老 code, 实现转调 CompletionRing
    virtual int setCompletionCallback(CompletionCallback cb) = 0;
};

}  // namespace tlm

#endif  // CPPTLM_ISM_EXECUTOR_H
```

### 2.2 命名空间

- `namespace tlm` — 与 CppTLM 仓全局惯例一致（per `include/tlm/AGENTS.md` + ADR-X.15 §0.3 namespace 约定）

---

## 3. ABI 1: installImage

### 3.1 签名

```cpp
virtual int installImage(uint64_t vram_addr, size_t size,
                          SmImageId* out_id) = 0;
```

### 3.2 行为契约

- **Precondition**:
  - `vram_addr` 是有效的 VRAM 地址（来自 `DGpuBar::vram_base()` + offset）
  - `[vram_addr, vram_addr + size)` 区间已被 H2D DMA 填充 PTXIR bytes
  - `size` > 0
  - `out_id` 非空

- **Postcondition**:
  - 成功: `*out_id` 设置为非 0 的 `SmImageId`，返回 0
  - 失败: `*out_id` 未修改，返回负的 `cudaError_t` 错误码

- **实现路径**:
  1. `SmExecutorImpl::installImage` 调 `map_vram_to_host(vram_addr, size)` 获取 host pointer
  2. 调 `PtxEmuSubmodule::image_load(host_ptr, size)` 拿 handle
  3. handle 即为 `SmImageId`，写入 `*out_id`

### 3.3 错误处理

| 错误码 | 语义 |
|---|---|
| `0` | 成功 |
| `CUDA_ERROR_INVALID_VALUE` | vram_addr 无效 / size=0 / out_id=NULL |
| `CUDA_ERROR_OUT_OF_MEMORY` | VRAM 映射失败 |
| `CUDA_ERROR_UNKNOWN` | PTX-EMU 端 image_load 返回非 0 |

---

## 4. ABI 2: dispatch

### 4.1 签名

```cpp
virtual int dispatch(SmImageId image,
                      uint64_t args_vram_addr, size_t args_size,
                      const DispatchParams& params) = 0;
```

### 4.2 行为契约

- **Precondition**:
  - `image` 是 `installImage` 返回的有效 SmImageId
  - `args_vram_addr` 是有效的 VRAM 地址（args buffer）
  - `[args_vram_addr, args_vram_addr + args_size)` 区间已被 H2D DMA 填充
  - `params` 各字段合法（grid/block 维度非 0，shared_mem 合理）

- **Postcondition**:
  - 成功: 返回 0，dispatch 异步执行（不等待完成）
  - 失败: 返回负的 cudaError_t

- **实现路径**:
  1. `SmExecutorImpl::dispatch` 调 `map_vram_to_host(args_vram_addr, args_size)` 获取 args pointer
  2. 调 `PtxEmuSubmodule::image_execute(image, params, args_ptr, args_size)`
  3. image_execute 是**异步**的（PTX-EMU 端 advance cycles），立即返回 status
  4. 完成后通过 `CompletionRing::push(image_id, status)` + `host_notify` 通知 host

### 4.3 错误处理

| 错误码 | 语义 |
|---|---|
| `0` | 成功提交（异步）|
| `CUDA_ERROR_INVALID_HANDLE` | image 无效（未通过 installImage）|
| `CUDA_ERROR_INVALID_VALUE` | args_vram_addr 无效 / params 非法 |
| `CUDA_ERROR_UNKNOWN` | PTX-EMU 端 image_execute 失败 |

### 4.4 异步语义

- `dispatch()` 返回 0 表示**成功提交**，不代表 kernel 完成
- Kernel 完成通过 `CompletionRing` + `host_notify` 链路通知（per [completion-ring.md](completion-ring.md) §3）
- host 端用 `cuStreamSynchronize` 等 fence 完成

---

## 5. ABI 3: setCompletionCallback

### 5.1 签名

```cpp
virtual int setCompletionCallback(CompletionCallback cb) = 0;
```

### 5.2 行为契约

- **Precondition**:
  - `cb` 可为空（表示清除 callback）

- **Postcondition**:
  - 成功: 注册 callback（实际转调 `CompletionRing::set_host_notify`），返回 0
  - 失败: 返回负 cudaError_t

- **重设计说明** (per UsrLinuxEmu ADR-090 v2 §D3.4):
  - 保留 `setCompletionCallback` 接口以**兼容老 code**
  - 实际实现将 callback 转调 `CompletionRing::set_host_notify`
  - **避免** `std::function` 回调表在多 stream 并发下的死锁/饥饿问题

### 5.3 实现路径

```cpp
// SmExecutorImpl::setCompletionCallback
int SmExecutorImpl::setCompletionCallback(CompletionCallback cb) {
    if (!cb) {
        completion_ring_->set_host_notify(nullptr);
        return 0;
    }
    // 转调 CompletionRing: 把 callback 包装成 host_notify hook
    completion_ring_->set_host_notify([cb, this]() {
        // CompletionRing pop 所有 entry，调 callback
        while (auto entry = completion_ring_->pop()) {
            cb(entry.image_id, entry.status);
        }
    });
    return 0;
}
```

### 5.4 错误处理

| 错误码 | 语义 |
|---|---|
| `0` | 成功注册 |
| `CUDA_ERROR_UNKNOWN` | CompletionRing 内部错误 |

---

## 6. 集成契约 (Integration Contracts)

### 6.1 PtxEmuSubmodule 集成 [Oracle D.2 改写]

`SmExecutorImpl` 持有 `PtxEmuSubmodule` 实例（构造时 dlopen `libptxemu_device.so`）。

```cpp
class SmExecutorImpl : public ISmExecutor {
public:
    // [Oracle D.2 改写]: 构造函数接收 DSO 路径(默认 soname)
    // 生产环境: 默认 "libptxemu_device.so"(由 PTX-EMU 真值源 ccd34155 提供)
    // 测试环境: 通过 JSON params 注入 mock .so 绝对路径(per T-P1-A4 + T-P3-4)
    explicit SmExecutorImpl(const std::string& dso_path = "libptxemu_device.so")
        : ptx_emu_(dso_path, /*dlopen_flags=*/RTLD_NOW | RTLD_LOCAL),
          completion_ring_(std::make_unique<CompletionRing>()) {}

private:
    PtxEmuSubmodule ptx_emu_;
    std::unique_ptr<CompletionRing> completion_ring_;
};
```

> **dlopen flags 备忘**:`RTLD_NOW` 立即解析所有符号(失败早暴露);`RTLD_LOCAL` 防符号污染到全局命名空间(per Oracle B.3);`RTLD_GLOBAL` 仅在需要 `dlsym(RTLD_DEFAULT, ...)` 时才加。

### 6.2 SubmissionQueue 集成

`SubmissionQueue::tick()` 触发 `ISmExecutor::dispatch`：

```cpp
// src/tlm/gpu/sm_executor_impl.cc::tick()
void SmExecutorImpl::tick() {
    if (sq_->tick()) {  // 消费一个 entry
        // sq_->tick() 内部已调 dispatch_handler（即本类的 dispatch 方法）
    }
}
```

---

## 7. 测试要求

### 7.1 单元测试

| 测试 | 内容 | Catch2 标签 |
|---|---|---|
| installImage 成功 | mock PtxEmuSubmodule → 返回有效 handle | `[sm-executor]` |
| installImage 失败 (vram_addr 无效) | 返回 `CUDA_ERROR_INVALID_VALUE` | `[sm-executor]` |
| installImage 失败 (image_load 失败) | 返回 `CUDA_ERROR_UNKNOWN` | `[sm-executor]` |
| dispatch 成功 | mock image_execute → 返回 0 + CompletionRing push | `[sm-executor]` |
| dispatch 失败 (image 无效) | 返回 `CUDA_ERROR_INVALID_HANDLE` | `[sm-executor]` |
| dispatch 异步语义 | 返回 0 后 CompletionRing 才 push | `[sm-executor]` |
| setCompletionCallback 转调 | callback 注册后 CompletionRing host_notify 触发 callback | `[sm-executor]` |

### 7.2 Mock 库 [Oracle B.3 改写 — 双层策略]

> **Oracle B.3 评审**:共享库 mock 方向正确(dlsym 行为只能用真只测),但应**双层**。

| Mock 类型 | 路径 | 用途 | 覆盖测试 |
|---|---|---|---|
| **Header-only C++ Mock** | `test/mock/mock_ptx_emu_submodule.hh` | C++ 替身,实现 `PtxEmuSubmodule` 行为(可重写 8 方法) | `test_sm_executor_impl.cc`、`test_is_m_executor.cc` 等单测 |
| **Header-only C++ Mock** | `test/mock/mock_dgpu_bar.hh`、`test/mock/mock_doorbell.hh`、`test/mock/mock_submission_queue.hh`、`test/mock/mock_completion_ring.hh` | DGpuBar/Doorbell/SQ/CompletionRing 的 C++ 替身 | 所有依赖这些组件的单测(无需 .so) |
| **Shared library mock** | `test/mock/mock_libptxemu_device.so` | 提供 8 个 dlsym 函数的可预测实现 | **仅** `test_ptx_emu_submodule.cc`(验 dlopen/dlsym/module_version) |

**删除原 `mock_smock_pcie_device.so`**:DGpuBar/Doorbell/SQ 是纯 C++ 类,header mock 足够,做 .so 是过度工程。

**路径修正** [Oracle C-NEW-1]:所有 `tests/mock_*.so` → `test/mock/mock_*.so`(对齐根 CMakeLists 的 `test/` 目录)。

---

## 8. 接口稳定性 (Interface Stability)

### 8.1 冻结接口（P2 收敛后禁止变更）

- ✅ `ISmExecutor` 3 ABI 签名
- ✅ `SmImageId` / `DispatchParams` / `CompletionCallback` 类型定义
- ✅ `namespace tlm` 命名空间

### 8.2 跨仓对齐

- 与 UsrLinuxEmu ADR-090 v2 §D3.3 ISmExecutor 设计**字节级一致**
- 如果 UsrLinuxEmu 后续调整 ISmExecutor，CppTLM 需同步（per ADR-X.15 §7 跨仓协调点）

---

**维护**: CppTLM Team
**状态**: 📋 Spec — W4-6 实施，W4 接口冻结
**下次更新**: W4 P2 启动时 / UsrLinuxEmu 端调整时
