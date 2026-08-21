// include/tlm/gpu/ptx_emu_submodule_mvp.hh
// PtxEmuSubmoduleMVP: PTX functional facade (per S1 / T-s1-3 Phase I.1 重构)
// 功能: CppTLM 唯一 include PTX-EMU 头的入口 — 编译防火墙。
//       暴露 PTX functional 接口 (decode / execute_warp / register/memory 读写 /
//       active mask / PC / blocked cycles / warp finished), **不**暴露 timing
//       API (exe_once / set_blocked_cycles) — functional/timing 严格分离。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-3, Sisyphus)
// 参考:
//   - openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/design.md §2.1
//   - Phase I.1: PtxEmuSubmoduleMVP facade 5 构造方法 + 1 functional execute + 11 state 方法
//   - HSK-6 编译防火墙: 仅 src/tlm/gpu/ptx_emu_submodule_mvp.cc 可 include
//     PTX-EMU 头 (`ptxsim/*.h` + `memory/*.h` + `register/*.h`)。
//     本 facade header 可 include `ptx_ir/statement_context.h` 因为 StatementContext
//     是 facade 公开 API 的值类型 (test/CudaCoreAdapter 都需直接使用);
//     其余 CppTLM .cc 仍**禁止** include 任何 PTX-EMU 头。
//
// 命名空间注意事项 (PTX-EMU 不一致):
//   - 全局命名空间: StatementContext, OperandContext, GenericInstr, WarpContext,
//     SMContext, GPUContext, ThreadContext, Dim3, EXE_STATE
//   - ptxsim 命名空间: WarpState, ThreadState, ThreadStatus, SIMTStack,
//     RegisterBankManager
#ifndef TLM_GPU_PTX_EMU_SUBMODULE_MVP_HH
#define TLM_GPU_PTX_EMU_SUBMODULE_MVP_HH

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// PTX-EMU 唯一允许 include 的 header — StatementContext 是 facade 公开 API
// 的值类型 (被 std::vector<StatementContext> 等容器直接使用, 需 sizeof 可见)
#include "ptx_ir/statement_context.h"

// =============================================================================
// 前向声明 — PTX-EMU 内部类 (不暴露给 CppTLM 消费者)
// 编译防火墙: 此 header 不 include 任何 ptxsim/memory/register 头。
// =============================================================================
class GPUContext;     // 全局命名空间 (ptxsim/gpu_context.h)
class SMContext;      // 全局命名空间 (ptxsim/sm_context.h)
class WarpContext;    // 全局命名空间 (ptxsim/warp_context.h)
class ThreadContext;  // 全局命名空间 (ptxsim/thread_context.h)

namespace ptxsim {
class WarpState;
class ThreadState;
class SIMTStack;
class RegisterBankManager;
}  // namespace ptxsim

class IScoreboard;
class IPipelineLatencyProvider;
class ITensorCoreTiming;

// =============================================================================
// 配置与状态类型 — 透传 PTX-EMU 内部定义 (值类型)
// =============================================================================

namespace tlm {

/// PTX functional execution result — 透传 PTX-EMU `enum EXE_STATE { IDLE,
/// RUN, EXIT, BAR_SYNC }` (ptxsim/execution_types.h)。facade 用 plain int
/// 表达以避免 include PTX-EMU 头 — 实现在 .cc 中转换。
using EXE_STATE = int;

/// GPUConfig — PTX-EMU 内部 GPUConfig 透传 (ptxsim/gpu_context.h:22)。
/// MVP 阶段透传 std::max_align_t 大小的 POD 子集;复杂字段
/// (instruction_latencies) 在 init() 内由 facade 默认构造。
struct GPUConfig {
    int num_sms = 1;
    int max_warps_per_sm = 64;
    int max_threads_per_sm = 2048;
    size_t shared_mem_size_per_sm = 64 * 1024;
    int registers_per_sm = 65536;
    int max_blocks_per_sm = 32;
    int warp_size = 32;
    size_t global_mem_size = 4ULL << 30;  // 4 GB
};

/// StatementContext — facade 公开 API 的值类型 (透传 PTX-EMU 全局命名空间下
/// 的 StatementContext, 来自 ptx_ir/statement_context.h)。
using StatementContext = ::StatementContext;

/// PtxEmuSubmoduleMVP — PTX functional facade (Phase I.1 §6, S1 T-s1-3)
///
/// 设计原则 (per design.md §1):
///   - 编译防火墙: 唯一允许 include PTX-EMU 头的 CppTLM .cc = 此类的实现
///   - functional/timing 严格分离: facade 接口禁止 `exe_once` / `set_blocked_cycles`
///     / `set_scoreboard` 等 timing API; 仅暴露 functional 状态读写
///   - functional_execute_warp **不**增加 cycle 计数 (sm->get_cycle_count() 前后不变)
///   - Module Getters (`create_scoreboard` 等) 本期 MVP 不使用, 保留供 v0.5
class PtxEmuSubmoduleMVP {
public:
    /// 默认构造 — **不**使用 = default (Impl 在 .cc 才完整, 必须在此声明
    /// 否则 .hh 内 unique_ptr<Impl> 的 deleter 会因 Impl 不完整而失败)。
    PtxEmuSubmoduleMVP();
    ~PtxEmuSubmoduleMVP();

    PtxEmuSubmoduleMVP(const PtxEmuSubmoduleMVP&) = delete;
    PtxEmuSubmoduleMVP& operator=(const PtxEmuSubmoduleMVP&) = delete;

    // =========================================================================
    // RAII: init() + shutdown()
    // =========================================================================
    bool init(const std::string& ptx_emu_root, const GPUConfig& config);
    void shutdown();

    bool is_initialized() const { return initialized_; }

    // =========================================================================
    // Functional Construction (5 methods)
    // =========================================================================
    /// 创建一个 GPUContext (owned — 由 caller 持有并 delete, raw pointer
    /// 以避免 .hh 内需要 GPUContext 完整定义)
    GPUContext* create_gpu_context();

    /// 取得 SM context (raw pointer; 生命周期由 owning GPUContext 控制)
    SMContext* get_sm_context(GPUContext& gpu, uint32_t sm_idx = 0);

    /// 取得 warp context (raw pointer)
    WarpContext* get_warp_context(SMContext& sm, uint32_t warp_idx);

    /// PTXIR 二进制字节流 → 解析为 IR statements
    std::vector<StatementContext> decode_ptxir(const std::vector<uint8_t>& ptxir_bytes);

    /// 提交 kernel 请求到 GPUContext, 返回 kernel_id (>= 0) 或 -1 (失败)
    /// MVP 阶段: 简单包装 GPUContext::submit_kernel_request。
    int64_t submit_kernel_request(GPUContext& gpu,
                                   const std::vector<StatementContext>& statements);

    // =========================================================================
    // Functional Execute (★ 核心: 不增加 cycle)
    // =========================================================================
    /// 调用 WarpContext::execute_warp_instruction(stmt, target_pc)
    /// @return 该 warp 当前 EXE_STATE (IDLE/RUN/EXIT/BAR_SYNC)
    /// @note cycle_count 前后不变 (functional model)
    EXE_STATE functional_execute_warp(WarpContext& warp,
                                       StatementContext& stmt,
                                       int target_pc);

    // =========================================================================
    // Functional State (11 methods)
    // =========================================================================
    template <typename T>
    T read_register(WarpContext* warp, int lane_id, int reg_id);

    template <typename T>
    void write_register(WarpContext* warp, int lane_id, int reg_id, T value);

    template <typename T>
    T read_global_memory(GPUContext* gpu, uint64_t addr);

    template <typename T>
    void write_global_memory(GPUContext* gpu, uint64_t addr, T value);

    /// 每线程 PC (WarpState 权威源)
    uint32_t read_thread_pc(const WarpContext* warp, int lane_id);

    /// ★ FIX-H8/B.3 — blocked_cycles 镜像 (WarpState 权威源)
    uint32_t read_blocked_cycles(const WarpContext* warp, int lane_id);

    /// 推进每线程 PC
    void advance_thread_pc(WarpContext* warp, int lane_id, uint32_t new_pc);

    /// 32 位 active mask
    uint32_t read_active_mask(const WarpContext* warp);

    /// warp 是否所有 lane 都已退出
    bool is_warp_finished(const WarpContext* warp);

    /// 单 lane 是否已退出
    bool is_thread_exited(const ThreadContext* thread);

    // =========================================================================
    // Module Getters (3 — 本期 MVP 不使用, 保留供 v0.5 CudaCoreAdapter)
    // =========================================================================
    /// 创建 CppTLM Scoreboard 实现实例 — owned, 由 caller 持有生命周期
    IScoreboard* create_scoreboard();
    IPipelineLatencyProvider* create_pipeline_latency_provider();
    ITensorCoreTiming* create_tensor_core_timing();

private:
    bool initialized_ = false;
    GPUConfig config_{};
    std::string ptx_emu_root_;
    // 不透明 impl — 避免在 .hh 暴露 PTX-EMU 内部类型
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tlm

#endif  // TLM_GPU_PTX_EMU_SUBMODULE_MVP_HH