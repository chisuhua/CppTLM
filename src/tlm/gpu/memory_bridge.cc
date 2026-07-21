// src/tlm/gpu/memory_bridge.cc
// MemoryBridge 实现: CppTLMBridge 5 纯虚方法 + deep-copy kernel_args
// 作者 CppTLM Team / 日期 2026-07-16
// 参考: openspec/changes/cpptlm-f12b-ld-impl/design.md §2.2
//       include/cudart/cpptlm_bridge.h (ABI 真值源)
#include "tlm/gpu/memory_bridge.hh"

#include "tlm/crossbar_tlm.hh"          // CrossbarTLM::query_latency
#include "tlm/gpu/kernel_launch_tlm.hh"  // KernelLaunchTLM::submit + get_ptx_emu_driver
#include "tlm/gpu/ptx_emu_driver.hh"     // IPtxEmuDriver::is_kernel_complete (S2 P1)


#include <cassert>
#include <climits>

namespace {
// 不依赖 <cuda_runtime.h>, 用 constexpr 占位错误码 (与 cudaError_t 等价 int)
constexpr int kCudaSuccess = 0;
constexpr int kCudaErrorInvalidValue = 11;
}  // namespace

namespace tlm {

MemoryBridge::MemoryBridge(KernelLaunchTLM* kernel_launch, CrossbarTLM* gpu_xbar)
    : kernel_launch_(kernel_launch), gpu_xbar_(gpu_xbar) {
    // 非所有权指针, 调用方须保证生命周期覆盖 MemoryBridge 使用期
    assert(kernel_launch_ != nullptr);
    assert(gpu_xbar_ != nullptr);
}

std::vector<std::vector<uint8_t>> MemoryBridge::deep_copy_args_(
    const void** args, size_t n) const {
    std::vector<std::vector<uint8_t>> copied;
    copied.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (args[i] == nullptr) {
            copied.emplace_back();  // 空 buffer 保留占位
        } else {
            // Phase 8.B 简化假设: 每个 arg 是 sizeof(void*) 字节 (int/float/ptr)
            // Phase 9+ 改进: 根据 PTX 类型 metadata 决定大小
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
    // 1. 校验入参 (与 cudaLaunchKernel 错误码语义对齐)
    if (kernel_name == nullptr) {
        return kCudaErrorInvalidValue;
    }
    if (args_count > 0 && kernel_args == nullptr) {
        return kCudaErrorInvalidValue;
    }

    // 2. deep-copy args (host 端 args 调用返回后可能失效, 必须在栈内深拷)
    auto copied = deep_copy_args_(kernel_args, args_count);

    // 3. 构造 PendingKernel (保活 deep-copied args 至 poll/同步完成)
    PendingKernel pk;
    pk.kernel_id = kernel_id;
    pk.stream_id = stream_id;
    pk.deep_copied_args = std::move(copied);
    pk.args_count = args_count;
    pending_kernels_[kernel_id] = std::move(pk);

    // 4. 构造 KernelLaunchRequest + push 到 KernelLaunchTLM FIFO
    KernelLaunchRequest req;
    req.kernel_id = kernel_id;
    req.stream_id = stream_id;
    req.kernel_name = kernel_name;
    req.grid_x = grid_x;
    req.grid_y = grid_y;
    req.grid_z = grid_z;
    req.block_x = block_x;
    req.block_y = block_y;
    req.block_z = block_z;
    req.shared_mem = shared_mem;
    kernel_launch_->submit(std::move(req));

    return kCudaSuccess;
}

uint64_t MemoryBridge::poll_kernel(uint64_t kernel_id) {
    auto it = pending_kernels_.find(kernel_id);
    if (it == pending_kernels_.end()) {
        return UINT64_MAX;  // 未知 kernel_id (错误)
    }

    // S2 Phase 1.1 P1: 查询 PTX-EMU driver 的 kernel 完成状态
    if (kernel_launch_ && kernel_launch_->get_ptx_emu_driver()) {
        if (kernel_launch_->get_ptx_emu_driver()->is_kernel_complete(kernel_id)) {
            pending_kernels_.erase(it);
            return 0;  // 已完成
        }
        return 1;  // 未完成 — 保留 pending_kernels_ 记录，等待下次 poll
    }

    // Fallback (无 driver): P0 行为 — 立即标记完成
    pending_kernels_.erase(it);
    return 0;
}

int MemoryBridge::synchronize_stream(uint64_t stream_id) {
    // 重要: poll_kernel() 会 erase pending_kernels_ 元素,
    // 直接对 map 做 range-for 会触发迭代器失效 (UB)。
    // 先 snapshot 待 poll 的 kernel_id 到 vector, 再对 vector 迭代。
    while (true) {
        std::vector<uint64_t> ids_to_poll;
        for (const auto& kv : pending_kernels_) {
            if (kv.second.stream_id == stream_id) {
                ids_to_poll.push_back(kv.first);
            }
        }
        if (ids_to_poll.empty()) {
            break;
        }

        bool stream_empty = true;
        for (uint64_t id : ids_to_poll) {
            // poll_kernel 内部 erase from pending_kernels_ (安全: 迭代 vector 非 map)
            uint64_t remaining = poll_kernel(id);
            if (remaining == 0) {
                // 已完成
            } else if (remaining != UINT64_MAX) {
                stream_empty = false;  // 仍有未完成的
            }
        }
        if (stream_empty) {
            break;
        }
        // P0 阶段所有 poll_kernel 返回 0, 顶层一次 break 即退;
        // P1+ 由 PTX-EMU 外部事件循环重新进入 synchronize_stream
    }
    return kCudaSuccess;
}

uint64_t MemoryBridge::global_access(uint64_t device_addr, uint64_t val, uint8_t type) {
    // Phase 8.B timing-only: 返回 NoC 路由延迟, 不实际驱动 NoC 路由
    // (NoC 在 KernelLaunchTLM::tick() 中独立推进)
    // 数据读写由 PTX-EMU 端 LdHandler/StHandler 在 SimpleMemory 完成 (bypass cache)
    uint64_t latency = gpu_xbar_->query_latency(device_addr);
    if (latency == UINT64_MAX) {
        return UINT64_MAX;  // 地址未映射, fallback 到 PTX-EMU 内部
    }
    (void)val;   // P0 不区分 LD/ST 数据值 (timing-only)
    (void)type;  // P0 不区分 LD/ST 类型 (timing-only)
    return latency;
}

}  // namespace tlm