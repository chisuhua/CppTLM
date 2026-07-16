// include/tlm/gpu/memory_bridge.hh
// MemoryBridge: CppTLMBridge 接口实现（PTX-EMU ↔ CppTLM 端桥接层）
// 功能: 实现 5 个纯虚方法 (version/submit_kernel/poll_kernel/synchronize_stream/global_access)
//       供 PTX-EMU 端 libcpptlm_cudart.so 通过 g_cpptlm_bridge 指针调用
// 作者 CppTLM Team / 日期 2026-07-16
// 参考: include/cudart/cpptlm_bridge.h (ABI 真值源)
//       - HSK-1 原始基线: PTX-EMU commit 8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d (2026-07-15)
//       - 2026-07-16 re-vendor: PTX-EMU commit 603bd8bc (新增 cpptlm_attach_bridge + 可见性宏)
//       - 详见 include/cudart/AGENTS.md (SHA-256 双重锁定, vendor 字节级一致)
//       - CPPTLMBRIDGE_VERSION = 1 (类接口签名未变, version bump 未触发)
//       openspec/changes/cpptlm-f12b-ld-impl/design.md §2.2
#ifndef TLM_GPU_MEMORY_BRIDGE_HH
#define TLM_GPU_MEMORY_BRIDGE_HH

#include "cudart/cpptlm_bridge.h"  // CppTLMBridge ABI 基类 (vendored)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace tlm {

class KernelLaunchTLM;  // forward declare (定义在 kernel_launch_tlm.hh, 在 namespace tlm)

// CrossbarTLM 定义在全局命名空间 (include/tlm/crossbar_tlm.hh, 无外层 namespace)
// 前向声明放在全局作用域中, 但不属于 namespace tlm 内部
}  // namespace tlm
class CrossbarTLM;  // 全局 namespace 前向声明

namespace tlm {

/**
 * @brief PTX-EMU ↔ CppTLM 桥接实现（D1-Full P0 阶段）
 *
 * 设计原则:
 *   - 非所有权持有 KernelLaunchTLM* + CrossbarTLM* (由调用方管理生命周期)
 *   - submit_kernel 在调用栈内 deep-copy kernel_args (host 端 args 调用返回后可能失效)
 *   - poll_kernel P0 阶段立即返回 0 (无真实 PTX-EMU 驱动, 立即完成语义)
 *   - global_access timing-only: 返回 CrossbarTLM::query_latency(), 数据由 PTX-EMU SimpleMemory 完成
 *   - 不走 REGISTER_CHSTREAM (非 ChStreamModuleBase 派生, 手动实例化)
 *   - 不依赖 <cuda_runtime.h> (用 constexpr 占位错误码)
 */
class MemoryBridge : public CppTLMBridge {
public:
    MemoryBridge(KernelLaunchTLM* kernel_launch, CrossbarTLM* gpu_xbar);
    ~MemoryBridge() override = default;

    /// 返回 ABI 版本号 (必须等于 CPPTLMBRIDGE_VERSION)
    int version() const override { return CPPTLMBRIDGE_VERSION; }

    /// 提交 kernel (异步立即返回)。校验 + deep-copy args + 构造 PendingKernel + push FIFO
    int submit_kernel(uint64_t kernel_id, const char* kernel_name,
                      uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                      uint32_t block_x, uint32_t block_y, uint32_t block_z,
                      const void** kernel_args, size_t args_count,
                      size_t shared_mem, uint64_t stream_id) override;

    /// 轮询 kernel 完成状态。P0 立即返回 0 (已完成); UINT64_MAX = 未知 kernel_id
    uint64_t poll_kernel(uint64_t kernel_id) override;

    /// 同步等待 stream 上所有 pending kernels 完成
    int synchronize_stream(uint64_t stream_id) override;

    /// 全局内存访问 — 同步返回 NoC 路由延迟 (cycle 数)。UINT64_MAX = 地址未映射
    uint64_t global_access(uint64_t device_addr, uint64_t val, uint8_t type) override;

private:
    /// deep-copy kernel_args 数组, 防止调用返回后 host 端内存失效
    /// Phase 8.B 简化: 假设每个 arg 是 sizeof(void*) 字节
    /// Phase 9+ 改进: 根据 PTX 类型 metadata 决定大小
    std::vector<std::vector<uint8_t>> deep_copy_args_(const void** args, size_t n) const;

    KernelLaunchTLM* kernel_launch_;  // 非所有权, 用于 push KernelLaunchRequest 到 FIFO
    CrossbarTLM* gpu_xbar_;           // 非所有权, 用于 query_latency 路由延迟查询

    /// pending kernel 记录 (deep-copied args 保活至完成)
    struct PendingKernel {
        uint64_t kernel_id = 0;
        uint64_t stream_id = 0;
        std::vector<std::vector<uint8_t>> deep_copied_args;
        size_t args_count = 0;
    };
    std::unordered_map<uint64_t, PendingKernel> pending_kernels_;
};

}  // namespace tlm

#endif  // TLM_GPU_MEMORY_BRIDGE_HH