# Internal Plan: cpptlm-f12b-ld-impl — CppTLM 端 D1 实施手册

> **Status**: Proposed
> **Parent**: `proposal.md` + `design.md` + `tasks.md` (cpptlm-f12b-ld-impl)
> **Worktree**: `CppTLM/.worktrees/feature-d1-full-impl` (branch `feature/d1-full-impl`)
> **目标**: 5.2 工作日（P0: 5d + P1: 2.5d + P2: 1h + P3: 1 周验证）

## Day 0: D1 启动前准备（~30 min）

### Step 0.1: 进入 worktree
```bash
cd /workspace/project/CppTLM
git worktree list  # 确认 .worktrees/feature-d1-full-impl 存在
cd .worktrees/feature-d1-full-impl
git status  # 确认干净
git log --oneline -3
```

### Step 0.2: vendor ABI 头文件（从本地 PTX-EMU commit 8dc000ec）
```bash
# 1. 从本地 PTX-EMU 仓库 commit 8dc000ec 显式提取
git show 8dc000ec:include/cudart/cpptlm_bridge.h > include/cudart/cpptlm_bridge.h

# 2. 验证字节级一致
SOURCE_SHA=$(cd /workspace/project/PTX-EMU && git show 8dc000ec:include/cudart/cpptlm_bridge.h | sha256sum | cut -d' ' -f1)
TARGET_SHA=$(sha256sum include/cudart/cpptlm_bridge.h | cut -d' ' -f1)
[ "$SOURCE_SHA" = "$TARGET_SHA" ] && echo "✅ PASS" || (echo "❌ FAIL"; exit 1)
# 预期: c19e66a32de398e6bba2042f3f19923ff89dbc02f10bbf310c073ad3a8ff3dbe
```

### Step 0.3: 写 AGENTS.md
```bash
cat > include/cudart/AGENTS.md << 'EOF'
# cudart/ Vendored Headers

## cpptlm_bridge.h (ABI 头文件)
- **Source**: /workspace/project/PTX-EMU @ commit 8dc000ec
- **Public repo**: github.com/chisuhua/PTX-EMU @ commit 8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d
- **Vendor method**: 从 PTX-EMU commit 显式提取（git show）
- **SHA-256 (commit 8dc000ec)**: c19e66a32de398e6bba2042f3f19923ff89dbc02f10bbf310c073ad3a8ff3dbe
- **Last verified**: 2026-07-15
- **Sync policy**: HSK-1 每次重发时手动同步
- **Replaced by**: 未来 HSK-3 选项 1 (ExternalProject_Add) 动态拉取
EOF
```

### Step 0.4: 验证 baseline 仍 764/764
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests  # 必须 764/764 PASS
```

**Step 0 验收**: baseline 仍绿，SHA-256 一致，AGENTS.md 写好

---

## Day 1-2: #C1 MemoryBridge 实施（~1d）

### Step 1.1: 头文件（30 min）
```cpp
// include/tlm/gpu/memory_bridge.hh
#ifndef TLM_GPU_MEMORY_BRIDGE_HH
#define TLM_GPU_MEMORY_BRIDGE_HH

#include "cudart/cpptlm_bridge.h"  // vendor from PTX-EMU commit 8dc000ec
#include "core/chstream_module.hh"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstring>

namespace tlm {
class KernelLaunchTLM;      // forward
class CrossbarTLM;
class MemoryController;

class MemoryBridge : public CppTLMBridge {
public:
    MemoryBridge(KernelLaunchTLM* kernel_launch,
                 CrossbarTLM* gpu_xbar,
                 MemoryController* gpu_memory);
    ~MemoryBridge() override = default;

    int version() const override { return CPPTLMBRIDGE_VERSION; }

    int submit_kernel(uint64_t kernel_id, const char* kernel_name,
                      uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                      uint32_t block_x, uint32_t block_y, uint32_t block_z,
                      const void** kernel_args, size_t args_count,
                      size_t shared_mem, uint64_t stream_id) override;

    uint64_t poll_kernel(uint64_t kernel_id) override;
    int synchronize_stream(uint64_t stream_id) override;

    uint64_t global_access(uint64_t device_addr, uint64_t val, uint8_t type) override;

private:
    std::vector<std::vector<uint8_t>> deep_copy_args_(const void** args, size_t n) const;
    int translate_error_(int ret) const;

    KernelLaunchTLM* kernel_launch_;
    CrossbarTLM* gpu_xbar_;
    MemoryController* gpu_memory_;

    struct PendingKernel {
        uint64_t kernel_id;
        uint64_t stream_id;
        std::vector<std::vector<uint8_t>> deep_copied_args;
        size_t args_count;
    };
    std::unordered_map<uint64_t, PendingKernel> pending_kernels_;
};

}  // namespace tlm

#endif
```

### Step 1.2: 实现（3h）
```cpp
// src/tlm/gpu/memory_bridge.cc
#include "tlm/gpu/memory_bridge.hh"
#include "tlm/gpu/kernel_launch_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "tlm/memory_tlm.hh"
#include <cuda_runtime.h>

namespace tlm {

MemoryBridge::MemoryBridge(KernelLaunchTLM* kernel_launch,
                            CrossbarTLM* gpu_xbar,
                            MemoryController* gpu_memory)
    : kernel_launch_(kernel_launch),
      gpu_xbar_(gpu_xbar),
      gpu_memory_(gpu_memory) {
    assert(kernel_launch_ != nullptr);
    assert(gpu_xbar_ != nullptr);
}

std::vector<std::vector<uint8_t>> MemoryBridge::deep_copy_args_(
    const void** args, size_t n) const {
    std::vector<std::vector<uint8_t>> copied;
    copied.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (args[i] == nullptr) {
            copied.emplace_back();  // 空 buffer
        } else {
            // Phase 8.B 简化：假设每个 arg 是 8 字节（int/float/ptr）
            // Phase 9+ 改进：根据 PTX 类型 metadata 决定大小
            std::vector<uint8_t> buf(sizeof(void*));
            std::memcpy(buf.data(), args[i], sizeof(void*));
            copied.push_back(std::move(buf));
        }
    }
    return copied;
}

int MemoryBridge::submit_kernel(uint64_t kernel_id, const char* kernel_name,
                                 uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                 uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                 const void** kernel_args, size_t args_count,
                                 size_t shared_mem, uint64_t stream_id) {
    // 1. 校验
    if (kernel_name == nullptr) return cudaErrorInvalidValue;
    if (args_count > 0 && kernel_args == nullptr) return cudaErrorInvalidValue;

    // 2. deep-copy args
    auto copied = deep_copy_args_(kernel_args, args_count);

    // 3. 构造 PendingKernel + push to map
    PendingKernel pk{kernel_id, stream_id, std::move(copied), args_count};
    pending_kernels_[kernel_id] = std::move(pk);

    // 4. 构造 KernelLaunchRequest + push to FIFO
    KernelLaunchRequest req;
    req.kernel_id = kernel_id;
    req.stream_id = stream_id;
    req.kernel_name = kernel_name;
    req.grid_x = grid_x; req.grid_y = grid_y; req.grid_z = grid_z;
    req.block_x = block_x; req.block_y = block_y; req.block_z = block_z;
    req.shared_mem = shared_mem;
    kernel_launch_->submit(std::move(req));

    return 0;  // success
}

uint64_t MemoryBridge::poll_kernel(uint64_t kernel_id) {
    auto it = pending_kernels_.find(kernel_id);
    if (it == pending_kernels_.end()) return UINT64_MAX;  // 未知

    // 查询 PTX-EMU 内部完成状态（通过 KernelLaunchTLM）
    uint64_t remaining = kernel_launch_->poll_ptx_emu_completion(kernel_id);
    if (remaining == 0) {
        pending_kernels_.erase(it);  // 完成
    }
    return remaining;
}

int MemoryBridge::synchronize_stream(uint64_t stream_id) {
    while (true) {
        bool stream_empty = true;
        std::vector<uint64_t> completed_ids;
        for (const auto& [id, info] : pending_kernels_) {
            if (info.stream_id != stream_id) continue;
            uint64_t remaining = poll_kernel(id);
            if (remaining == 0) {
                completed_ids.push_back(id);
            } else if (remaining != UINT64_MAX) {
                stream_empty = false;
            }
        }
        for (uint64_t id : completed_ids) {
            pending_kernels_.erase(id);  // 安全 erase (range-for 已结束)
        }
        if (stream_empty) break;
        // PTX-EMU 外部事件循环会重新调用
    }
    return 0;
}

uint64_t MemoryBridge::global_access(uint64_t device_addr, uint64_t val, uint8_t type) {
    // NoC 路由延迟查询（CrossbarTLM）
    uint64_t latency = gpu_xbar_->query_latency(device_addr);
    if (latency == UINT64_MAX) return UINT64_MAX;  // 地址未映射，fallback

    // 数据读写由 PTX-EMU 端 LdHandler/StHandler 在 SimpleMemory 完成
    // (Phase 8.B timing-only 语义)
    return latency;
}

int MemoryBridge::translate_error_(int ret) const {
    if (ret == 0) return cudaSuccess;
    return static_cast<int>(ret);  // cudaError_t 是 int 类型
}

}  // namespace tlm
```

### Step 1.3: 单测（2h）
- 7 个 Catch2 测试用例
- Mock CrossbarTLM + Mock KernelLaunchTLM
- 覆盖正常 + 错误路径

### Step 1.4: CMake 注册（10 min）
```cmake
# CMakeLists.txt 添加
add_library(memory_bridge STATIC src/tlm/gpu/memory_bridge.cc)
target_link_libraries(memory_bridge PUBLIC cpptlm_core kernel_launch_tlm
                                         crossbar_tlm memory_tlm)
target_include_directories(memory_bridge PUBLIC include)
```

### Step 1.5: 验证
```bash
cmake --build build -j$(nproc) --target memory_bridge
./build/bin/cpptlm_tests "[gpu][f12b]"  # 7 tests PASS
./build/bin/cpptlm_tests  # 仍 764/764 PASS
```

---

## Day 3-4: #C2 KernelLaunchTLM 实施（~1d）

### Step 2.1: 头文件（30 min）
```cpp
// include/tlm/gpu/kernel_launch_tlm.hh
#ifndef TLM_GPU_KERNEL_LAUNCH_TLM_HH
#define TLM_GPU_KERNEL_LAUNCH_TLM_HH

#include "core/chstream_module.hh"
#include "tlm/gpu/memory_bridge.hh"
#include <deque>
#include <memory>

namespace tlm {

// 4 内部接口（D1-Full P1 阶段，Phase 8.B 可为 nullptr）
class IScoreboardInternal;
class IPipelineLatencyInternal;
class ITensorCoreTimingInternal;
class IAsyncCompletion;

struct KernelLaunchRequest {
    uint64_t kernel_id;
    uint64_t stream_id;
    const char* kernel_name;
    uint32_t grid_x, grid_y, grid_z;
    uint32_t block_x, block_y, block_z;
    size_t shared_mem;
    void* func_ptr = nullptr;
};

class KernelLaunchTLM : public ChStreamModuleBase {
public:
    explicit KernelLaunchTLM(const std::string& name, EventQueue* eq);
    ~KernelLaunchTLM() override = default;

    std::string get_module_type() const override { return "KernelLaunchTLM"; }
    void set_stream_adapter(StreamAdapterBase* adapter) override;

    // EventQueue 每 tick 调用
    void tick() override;

    // MemoryBridge 调用
    void submit(KernelLaunchRequest&& req);

    // PTX-EMU 端 gpu_context handle
    void set_ptx_emu_context(void* ctx) { ptx_emu_context_ = ctx; }

    // poll 用于 MemoryBridge::poll_kernel
    uint64_t poll_ptx_emu_completion(uint64_t kernel_id);

    // 4 内部模块 setter（D1-Full P1）
    void set_scoreboard(IScoreboardInternal* s) { scoreboard_ = s; }
    void set_pipeline_provider(IPipelineLatencyInternal* p) { pipeline_provider_ = p; }
    void set_tensor_core_timing(ITensorCoreTimingInternal* t) { tensor_core_timing_ = t; }
    void set_async_completion(IAsyncCompletion* ac) { async_completion_ = ac; }

private:
    void call_ptx_emu_exe_once_();

    std::deque<KernelLaunchRequest> pending_;
    std::unique_ptr<MemoryBridge> bridge_;

    IScoreboardInternal* scoreboard_ = nullptr;
    IPipelineLatencyInternal* pipeline_provider_ = nullptr;
    ITensorCoreTimingInternal* tensor_core_timing_ = nullptr;
    IAsyncCompletion* async_completion_ = nullptr;

    void* ptx_emu_context_ = nullptr;

    static constexpr uint32_t MAX_PTX_STEPS_PER_TICK = 10000;
};

}  // namespace tlm

#endif
```

### Step 2.2: 实现（3h）
- tick()：call bridge_->synchronize_stream(0) + 循环 exe_once + poll
- 4 setter：D1-Full P1 用
- set_ptx_emu_context：PTX-EMU 端设置

### Step 2.3: 单测（2h）

### Step 2.4: CMake + 验证（10 min）

---

## Day 5: G-F0 vector_add 烟雾测试（~0.3d）

### Step 3.1: 测试配置（30 min）
```json
{
  "name": "vector_add_smoke_f12b",
  "modules": [
    {"name": "mem", "type": "MemoryTLM", "size": 33554432},
    {"name": "xbar", "type": "CrossbarTLM", "ports": 4},
    {"name": "kernel_launch", "type": "KernelLaunchTLM"},
    {"name": "memory_bridge", "type": "MemoryBridge"}
  ],
  "connections": [
    {"src": "kernel_launch", "dst": "xbar.0"},
    {"src": "xbar.0", "dst": "mem"}
  ],
  "kernel": {
    "name": "vector_add",
    "type": "vector_add",
    "n": 1048576,
    "args": ["buf_a", "buf_b", "buf_c"]
  }
}
```

### Step 3.2: Python 烟雾测试（2h）
```python
# tests/python/test_f12b_smoke.py
import subprocess
import numpy as np
import json
from pathlib import Path

def test_vector_add_output_byte_equal_with_standalone():
    """G-F0: CppTLM F12b output vs standalone PTX-EMU baseline"""
    # 1. 启动 CppTLM with F12b-LD enabled
    cpptlm_result = run_cpptlm_sim("configs/vector_add_n1024.json",
                                    enable_f12b=True)
    cpptlm_output = parse_output(cpptlm_result.stdout)

    # 2. 对比 baseline（standalone PTX-EMU, 来自 Phase 0.5 baseline worktree）
    baseline_output = load_baseline_output("baseline/vector_add_n1024.bin")

    # 3. 逐元素 diff
    assert np.array_equal(cpptlm_output, baseline_output), \
        f"Output mismatch: {(cpptlm_output - baseline_output).max()}"

def test_vector_add_latency_within_2x_baseline():
    """G-F0: 延迟 ≤ 2× standalone baseline"""
    cpptlm_latency = measure_latency("configs/vector_add_n1024.json",
                                      enable_f12b=True)
    baseline_latency = load_baseline_latency("baseline/vector_add_n1024.json")

    assert cpptlm_latency <= 2 * baseline_latency, \
        f"Latency {cpptlm_latency} > 2x baseline {baseline_latency}"
```

### Step 3.3: 双端联合验证
- CppTLM 端：cpptlm_sim + MemoryBridge enabled
- PTX-EMU 端：libcpptlm_cudart.so + libcpptlm_bridge link
- 双端 commit hash 同步

### Step 3.4: Commit
```bash
git add configs/vector_add_n1024.json tests/python/test_f12b_smoke.py
git commit -m "test(f12b): G-F0 vector_add smoke (D1-Full P0 质量门)

Output byte-equal with standalone PTX-EMU + latency <= 2x baseline.
This is the G-F0 quality gate before P1 D1-Full injection."
```

**D5 EOD 验收门**:
- `pytest tests/python/test_f12b_smoke.py -v` PASS
- 通知 PTX-EMU 团队 G-F0 通过
- 开始 P1 实施

---

## Day 6-7: P1 D1-Full Compute 注入（~2.5d）

### Step 4.1: Day 6 - 3 核心模块
- `include/tlm/gpu/scoreboard_tlm.hh` + `.cc`（≥12 entries hazard table）
- `include/tlm/gpu/pipeline_tlm.hh` + `.cc`（5+V 抽象）
- `include/tlm/gpu/tensorcore_tlm.hh` + `.cc`（6 精度）
- 12 端点 `static_assert`（与 PTX-EMU 端双向一致）
- 3 个单测

### Step 4.2: Day 7 - 4 Adapter
- `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}`
- `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}`
- `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}`
- `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}`
- WarpContext* ↔ uint32_t 转换 + 12 端点 static_assert
- 4 个 Adapter 单测

### Step 4.3: 验证
- `cpptlm_tests [gpu][d1full]` 全 PASS
- 双端 12 端点 enum 一致性验证

---

## Day 8: P2 Async Seam（~1h）

### Step 5.1: IAsyncCompletion 占位
```cpp
// include/tlm/gpu/async_completion_adapter.hh
class AsyncCompletionAdapter : public IAsyncCompletion {
public:
    void register_completion_callback(uint64_t id, std::function<void()> cb) override {
        pending_callbacks_[id] = std::move(cb);
    }
    void fire_completion(uint64_t id) override {
        auto it = pending_callbacks_.find(id);
        if (it != pending_callbacks_.end()) {
            it->second();
            pending_callbacks_.erase(it);
        }
    }
private:
    std::unordered_map<uint64_t, std::function<void()>> pending_callbacks_;
};
```

---

## Day 9-14: P3 集成验证（~1 周）

### Step 6.1: 5 类 microbenchmark
- GEMM (FP16, M=N=K=4096) — gpgpu-sim 700 GB/s ±15%
- FlashAttn (b=8, h=16, seq=512) — 470 GB/s ±15%
- vector_add (n=1024²) — 1176 GB/s ±15%
- stencil (3D 7-point, N=512³) — 940 GB/s ±15%
- sparse SpMV (10k×10k, 0.01) — 230 GB/s ±15%

### Step 6.2: docs_sync + 6 microarchitecture doc
- `bash scripts/test/docs_sync_check.sh --strict` 0 missing
- 6 个微架构 doc（aperture/latency/throughput 角度）

### Step 6.3: 性能 + 全量回归
- 1 GB203 × 1M < 60s
- `ptxemu_tests` 100% pass（PTX-EMU 端验证）
- `cpptlm_tests [gpu][d1full]` 100% pass

---

## 实施纪律

1. **每个 Phase commit 独立可回退**（不混 commit）
2. **每日 5 个动作**：
   1. 看 `tasks.md` 选今日 1-2 项
   2. 在 worktree 实施
   3. 命中同步点时 push hash
   4. 自己仓库跑对应 [tag] 测试
   5. EOD 报状态：完成/阻塞/待 hash
3. **Test-first**：写测试 → 失败 → 实施 → 通过
4. **零退化**：每个 commit 后跑 `cpptlm_tests` 确认 764/764 pass
5. **commit message 格式**：`feat(tlm/gpu): ...` 包含 ref ADR-NV-02 + 综合计划章节

## 风险与升级路径

| 风险 | 升级到 |
|------|--------|
| R1: kernel_args deep-copy 性能瓶颈 | oracle subagent 评估 |
| R2: ptx_emu_context handle 类型不匹配 | PTX-EMU 团队协调 |
| R3: 12 端点 enum 漂移 | oracle subagent + 双端 CI 双重断言 |
| R4: fast/slow path 注入遗漏 | chaos test 1000 次随机序列 |
| R5: 5 类 microbenchmark 偏差 > ±15% | PTX-EMU 团队联合调优 |
