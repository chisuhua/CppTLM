#ifndef PTX_CPPTLM_BRIDGE_H
#define PTX_CPPTLM_BRIDGE_H

// PTX-EMU ↔ CppTLM 跨 so 可见性宏
// PTX-EMU 端导出符号的可见性宏（gcc/clang: visibility("default")；MSVC: dllexport）
#if defined(_WIN32) || defined(__CYGWIN__)
#  define PTXEMU_BRIDGE_API __declspec(dllexport)
#else
#  define PTXEMU_BRIDGE_API __attribute__((visibility("default")))
#endif

// =====================================================================
// CppTLM ↔ PTX-EMU Bridge ABI 真值源
// =====================================================================
//
// **重要**: 本文件是 ABI 真值源（PTX-EMU 是 ABI 提供方，CppTLM 是消费方）。
//
// 任何对本接口的修改必须:
//   1. 同步 bump CPPTLMBRIDGE_VERSION
//   2. 通知 CppTLM 同步 rebase（CppTLM 通过 ExternalProject_Add 引用本头文件）
//   3. 在 docs/dev-process/lessons-learned.md 记录变更
//
// 同步约束（编译器/链接器维度）:
//   - CppTLM MemoryBridge::version() 必须返回与 CPPTLMBRIDGE_VERSION 相同的值
//   - CppTLM CI 双重 static_assert: 12 端点枚举值双向一致（PipelineId + TcPrecision）
//
// 参考文档：
//   - CppTLM 综合任务书 §2.1 Task #1 (cppTLMBridge 接口定义)
//   - CppTLM 协作同步 §5 (cppTLMBridge 接口定义)
//   - ADR-0021 (D-PTX-1: g_cpptlm_bridge 全局指针位置)
//   - ADR-0020 (姊妹 ADR: §3 D1-Full 三段式注入)
//   - openspec/changes/cpptlm-d1-full/

#include <cstddef>
#include <cstdint>

// cudaStream_t 类型定义
// 优先使用 cuda_runtime.h 的定义，否则使用 cudart_intrinsics.h 兼容的 void*
#if defined(__CUDACC_RUNTIME_H__)
#include <cuda_runtime.h>
#elif !defined(CUDA_STREAM_T_DEFINED)
// 与 cudart_intrinsics.h 保持一致：cudaStream_t = void*
typedef void* cudaStream_t;
#define CUDA_STREAM_T_DEFINED
#endif

/// Bridge ABI 版本号 — 编译期断言双方实现版本一致
/// 每次接口签名变更必须同步递增此值
///
/// 修订流程：
///   1. 修改本头文件接口签名
///   2. bump CPPTLMBRIDGE_VERSION（如 1 → 2）
///   3. 通知 CppTLM 同步 rebase（HSK-1 重新发出）
///   4. CppTLM MemoryBridge::version() 返回同步的新版本号
#define CPPTLMBRIDGE_VERSION 2

/// PTX-EMU ↔ CppTLM 桥接接口
///
/// 设计原则：
///   - PTX-EMU 仅持原始指针（extern CppTLMBridge* g_cpptlm_bridge），所有权归 libcpptlm_cudart.so
///   - nullptr = 独立模式（PTX-EMU 自驱，行为字节级兼容）
///   - 所有方法 virtual ~CppTLMBridge() default 析构
///   - 接口在 PTX-EMU 侧零外部依赖（不 include CppTLM 任何头文件）
///
/// F12b-LD 阶段语义：
///   - submit_kernel 立即返回（异步）
///   - poll_kernel 同步返回剩余 cycles / 0 / UINT64_MAX
///   - synchronize_stream 同步等待 stream 上所有 pending kernels 完成
///   - global_access timing-only 语义：返回 latency 设置 blocked_cycles_remaining
///     数据立即在 PTX-EMU SimpleMemory 中完成（Phase 8.B cache bypass）
///
/// 错误码语义（参照 ADR-0021 D-PTX-5）：
///   - submit_kernel 返回 0=成功, 非0=cudaError_t 错误码
///   - poll_kernel 返回 0=已完成, >0=剩余 cycles, UINT64_MAX=未知 kernel_id
///   - synchronize_stream 返回 0=成功, 非0=错误码
///   - global_access 返回 UINT64_MAX=地址未映射（fallback 到 PTX-EMU 内部）
class CppTLMBridge {
public:
  virtual ~CppTLMBridge() = default;

  /// 返回桥接实现的 ABI 版本（必须等于 CPPTLMBRIDGE_VERSION）
  /// CppTLM 端 MemoryBridge::version() 返回相同值
  /// @return ABI 版本号（当前 = 1）
  virtual int version() const = 0;

  /// 提交一个 kernel（异步！立即返回）
  ///
  /// @param kernel_id   PTX-EMU 生成的唯一 ID（用于后续 poll_kernel 查询）
  /// @param kernel_name PTX 函数名（如 "myKernel"），以 \0 结尾
  ///                    取自 PTX-EMU func2name[] 表（PTX-EMU 内部长期存储，无需拷贝）
  /// @param grid_x/y/z grid 维度（uint32_t）
  /// @param block_x/y/z block 维度（uint32_t）
  /// @param kernel_args 指向 kernel 参数数组的指针（host 端已对齐）
  ///                    **重要**: CppTLM 必须在 submit 调用栈内 deep-copy
  ///                              PTX-EMU host 端的 args 内存可能在调用返回后失效
  /// @param args_count  kernel 参数个数（用于 deep-copy 遍历）
  /// @param shared_mem  动态共享内存字节数
  /// @param stream_id   stream 句柄（0 = 默认 stream, 由 cudaStream_t reinterpret_cast）
  /// @return 0=成功, 非0=cudaError_t 错误码
  virtual int submit_kernel(uint64_t kernel_id,
                            const char* kernel_name,
                            uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                            uint32_t block_x, uint32_t block_y, uint32_t block_z,
                            const void** kernel_args, size_t args_count,
                            size_t shared_mem,
                            uint64_t stream_id) = 0;

  /// 轮询 kernel 完成状态
  ///
  /// @param kernel_id 由 submit_kernel 返回的唯一 ID
  /// @return 0         = kernel 已完成
  ///         >0        = 剩余 cycle 数
  ///         UINT64_MAX = 未知 kernel_id（错误）
  virtual uint64_t poll_kernel(uint64_t kernel_id) = 0;

  /// 同步等待 stream 上所有 pending kernels 完成
  ///
  /// @param stream_id stream 句柄（0 = 默认 stream）
  /// @return 0=成功, 非0=cudaError_t 错误码
  virtual int synchronize_stream(uint64_t stream_id) = 0;

  /// 全局内存访问 — 同步返回 NoC 路由延迟（cycle 数）
  ///
  /// **Phase 8.B 语义**：timing-only 预计算
  ///   - 不实际驱动 NoC 路由（NoC 在 KernelLaunchTLM::tick() 中独立推进）
  ///   - query_latency 基于 CUDA device address 路由表查表
  ///   - 地址映射假设：PTX-EMU 传 CUDA device address（已在 is_global_space() 空间判定后）
  ///   - 数据读写立即在 PTX-EMU SimpleMemory 中完成（bypass CppTLM cache）
  ///
  /// **Phase 9+ 演进**：当 IAsyncCompletion 真实实现时，LD/ST handler 改为：
  ///   写入 NoC 请求 → 返回 transaction_id → 不阻塞，立即让 warp 继续
  ///   通过 IAsyncCompletion 回调在后续 tick 写入目标寄存器
  ///   （参见 §4 异步 seam 预留）
  ///
  /// @param device_addr GLOBAL 空间虚拟地址
  /// @param val         写入值（ST 指令）或 0（LD 指令）
  /// @param type        0=LD, 1=ST
  /// @return 延迟 cycle 数；UINT64_MAX = 地址未映射（fallback 到 PTX-EMU 内部）
  virtual uint64_t global_access(uint64_t device_addr, uint64_t val, uint8_t type) = 0;
};

/// 全局 bridge 指针（PTX-EMU 持有）
///
/// nullptr = 独立模式（PTX-EMU 自驱，行为字节级兼容）
///
/// 初始化时机（D-PTX-1）：
///   - 默认 nullptr（编译期定义于 src/cudart/cudart_sim.cpp）
///   - 加载 libcpptlm_cudart.so 后通过 extern "C" 入口 cpptlm_attach_bridge() 赋值
///   - cudaLaunchKernel 入口检查：if (g_cpptlm_bridge) 走异步路径，否则原同步
///
/// 生命周期：
///   - PTX-EMU 是唯一持有方
///   - libcpptlm_cudart.so 卸载时通过 cpptlm_detach_bridge() 重置为 nullptr
extern CppTLMBridge* g_cpptlm_bridge;

/// CppTLM 加载时调用 — 设置 PTX-EMU 端全局 bridge 指针
///
/// 调用方：libcpptlm_cudart.so 的静态构造或显式初始化函数
/// 可重复调用（后调用覆盖前调用）
/// nullptr 参数表示 detach（reset 为 nullptr）
/// 实现位置：src/cudart/cudart_sim.cpp（与 g_cpptlm_bridge 定义同 TU）
extern "C" PTXEMU_BRIDGE_API void cpptlm_attach_bridge(CppTLMBridge* bridge);

/// CppTLM 卸载时调用 — 重置 PTX-EMU 端全局 bridge 指针
///
/// 调用方：libcpptlm_cudart.so 的析构函数
/// 安全重复调用（nullptr 状态下幂等）
/// 实现位置：src/cudart/cudart_sim.cpp
extern "C" PTXEMU_BRIDGE_API void cpptlm_detach_bridge();

// =====================================================================
// CppTLM D1-Full P1: cpptlm_set_driver ABI 入口 (PTX-EMU → CppTLM)
// =====================================================================
// PTX-EMU 通过此入口将 PtxEmuDriverShim 实例 + 方法 vtable 传递给
// CppTLM 对端。CppTLM 侧的 DriverWrapper 通过 vtable 调用 shim 方法。
//
// 方向: PTX-EMU → CppTLM（与 cpptlm_attach_bridge 方向相反）
// 实现: CppTLM 的 libcpptlm_cudart.so 提供强定义；
//       PTX-EMU 的 libcudart.so 提供 __attribute__((weak)) 空实现。
//
// 与 cpptlm_attach_bridge 的区别:
//   - attach_bridge: CppTLM 把 bridge 传给 PTX-EMU
//   - set_driver:    PTX-EMU 把 driver 传给 CppTLM
// =====================================================================

/// 跨 .so 边界的 vtable — 每个函数指针指向 PtxEmuDriverShim 的对应方法。
/// CppTLM 侧 DriverWrapper 通过此 vtable 在不解引用 PTX-EMU 符号的
/// 情况下调用 shim 方法。8 个函数指针必须与 PTX-EMU 侧 PtxEmuDriverApi
/// 定义完全一致。
struct PtxEmuDriverApi {
    /// advance: 0=NoOp, 1=Executed, 2=KernelComplete, -1=Error
    int (*advance)(void* shim, uint32_t max_cycles, uint32_t* actual);

    void (*inject_scoreboard)(void* shim, uint32_t sm_id, void* sb_ptr);
    void (*inject_pipeline)(void* shim, uint32_t sm_id, void* pp_ptr);
    void (*inject_tensor_core)(void* shim, uint32_t sm_id, void* tc_ptr);

    /// is_kernel_complete: 1=complete, 0=not complete or unknown
    int (*is_kernel_complete)(void* shim, uint64_t kernel_id);
    void (*mark_complete)(void* shim, uint64_t kernel_id);

    uint32_t (*num_sms)(void* shim);

    /// destroy: 释放 shim 实例（CppTLM 卸载时调用）
    void (*destroy)(void* shim);
};

/// PTX-EMU 调用此函数向 CppTLM 注册 driver。
/// - shim:   PtxEmuDriverShim 实例的不透明指针
/// - api:    包含 8 个函数指针的 vtable
///
/// 方向: PTX-EMU → CppTLM
extern "C" PTXEMU_BRIDGE_API void cpptlm_set_driver(void* shim, PtxEmuDriverApi api);

/// 编译期断言 cudaStream_t 宽度可存入 uint64_t
///
/// 必要性：
///   - submit_kernel 的 stream_id 字段为 uint64_t（与 kernel_id 同一空间）
///   - cudaStream_t 在不同 CUDA 版本宽度可能不同
///   - 防止未来 cudaStream_t 宽度变化导致隐式截断（silent corruption）
///
/// 触发：
///   - 编译失败即为静态测试失败
///   - CppTLM CI 双重 static_assert（CppTLM 端也加此断言）
static_assert(sizeof(cudaStream_t) <= sizeof(uint64_t),
              "cudaStream_t wider than uint64_t — bridge stream_id field must be enlarged");

// =====================================================================
// G-D4: 12 端点 + 签名级 ABI 一致性静态断言已迁移至 abi_guards.h
// HSK-6 §2.2 P0-1 硬门禁 (commit 25e36f60)
// 详见 docs/superpowers/specs/2026-08-18-hsk-6-response.md
// =====================================================================
#include "cudart/abi_guards.h"  // G-D4 16 条 static_assert 集中托管 (PipelineId 6 + TcPrecision 6 + is_same_v 4)

#endif // PTX_CPPTLM_BRIDGE_H
