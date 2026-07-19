// include/tlm/gpu/ptx_emu_driver.hh
// IPtxEmuDriver: 跨仓库协同仿真窄驱动接口 — D1-Full P1 Phase 4 Wave 1
// + DriverWrapper: 跨 .so PtxEmuDriverShim 适配层 (Phase 4 Wave 0 跟进)
// 功能: CppTLM(C++17) 定义纯虚接口, PTX-EMU(C++20) 实现 PtxEmuDriverShim,
//       通过 advance() 推进 GPUContext::exe_once() 并注入 CppTLM 端 Scoreboard/
//       Pipeline/TensorCore timing 模块
// 作者 CppTLM Team / 日期 2026-07-18
// 参考:
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/design.md §8.2-8.3
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §4.1-4.6
//   - include/cudart/{scoreboard,pipeline,tensor_core}_interface.h (vendor from HSK-4)
//
// Design Revision 3 (2026-07-18 Oracle P0 审查):
//   - AdvanceResult enum: 替代 uint32_t 返回值, 4 种执行状态显式错误传播
//   - unique_ptr 所有权转移: inject_*() CppTLM 注入后不再持有, PTX-EMU 负责销毁
//   - 纯虚接口零 PTX-EMU 依赖: 仅用已 vendor 的 3 纯虚接口 + std::unique_ptr
//
// Phase 4 Wave 0 跟进 (2026-07-18):
//   - PtxEmuDriverApi: C-compatible vtable 用于跨 .so 调用 PtxEmuDriverShim
//   - DriverWrapper: 将 opaque shim (void* + vtable) 适配为 IPtxEmuDriver
//   - g_ptx_emu_driver: 全局指针供 main.cpp 使用
#ifndef TLM_GPU_PTX_EMU_DRIVER_HH
#define TLM_GPU_PTX_EMU_DRIVER_HH

#include "cudart/pipeline_interface.h"       // IPipelineLatencyProvider + PipelineId
#include "cudart/scoreboard_interface.h"     // IScoreboard
#include "cudart/tensor_core_interface.h"    // ITensorCoreTiming + TcPrecision

#include <cstdint>
#include <memory>

namespace tlm {

/// advance() 返回值 — 区分 4 种执行状态
///
/// 替代原 uint32_t 返回值（歧义: 0 = 成功? 失败?），显式语义支持错误传播。
/// KernelLaunchTLM::tick() switch on 此 enum, AdvanceResult::Error 记录日志 + 终止 tick。
enum class AdvanceResult : uint8_t {
    Executed,        // 推进 ≥1 cycle
    NoOp,            // 无 pending work（kernel 完成或未启动）
    KernelComplete,  // kernel 执行完毕（is_kernel_complete 可查询）
    Error            // PTX-EMU 异常（exe_once 抛异常 / SM 越界 / GPUContext 无效）
};

/// PTX-EMU 执行引擎的抽象驱动接口
///
/// CppTLM (C++17) 定义纯虚接口, PTX-EMU (C++20) 侧通过 PtxEmuDriverShim 实现。
/// 窄接口设计原则:
///   - 零 PTX-EMU 类型依赖: 仅用 uint32_t/uint64_t/unique_ptr + 已 vendor 的纯虚接口
///   - C++17/20 ABI 安全: 纯虚表布局在两语言版本间一致
///   - unique_ptr 转移所有权: CppTLM 注入后不再持有实例, PTX-EMU 负责生命周期
///   - 错误不可静默: AdvanceResult::Error 要求调用方处理
class IPtxEmuDriver {
public:
    virtual ~IPtxEmuDriver() = default;

    // ──── 执行驱动 ────

    /// 推进 PTX-EMU 执行最多 max_cycles 个周期
    ///
    /// @param max_cycles  最多推进的 GPUContext::exe_once() 调用次数
    /// @param actual_cycles [out] 实际推进的周期数
    /// @return AdvanceResult::Executed 如果推进 ≥1 cycle;
    ///         NoOp 如果 kernel 未启动或已完成;
    ///         KernelComplete 如果 kernel 执行完毕;
    ///         Error 如果 GPUContext 无效或 exe_once() 抛异常
    virtual AdvanceResult advance(uint32_t max_cycles, uint32_t& actual_cycles) = 0;

    /// 查询 kernel 是否已完成执行
    ///
    /// @param kernel_id 来自 CppTLM MemoryBridge::submit_kernel 的 kernel ID
    /// @return true 如果 kernel 已执行完毕（GPUContext EXIT 状态或 on_complete 回调触发）
    virtual bool is_kernel_complete(uint64_t kernel_id) = 0;

    // ──── 资源注入（转移所有权）───

    /// 注入 per-SM scoreboard hazard 检测模块（转移所有权）
    ///
    /// CppTLM 调用后不再持有该实例; PTX-EMU 端 SM 销毁时释放。
    /// @param sm_id SM 编号 (0..num_sms()-1), 越界则忽略
    /// @param sb     ScoreboardTLM 实例, CppTLM 创建通过 std::make_unique<ScoreboardTLM>()
    virtual void inject_scoreboard(uint32_t sm_id, std::unique_ptr<IScoreboard> sb) = 0;

    /// 注入 per-SM pipeline latency 查询模块（转移所有权）
    ///
    /// PipelineTLM 无状态, 可跨 SM 共享同一实例或 per-SM 各创建一个。
    /// @param sm_id SM 编号 (0..num_sms()-1), 越界则忽略
    /// @param p     PipelineTLM 实例
    virtual void inject_pipeline(uint32_t sm_id,
                                 std::unique_ptr<IPipelineLatencyProvider> p) = 0;

    /// 注入 per-SM tensor core timing 查询模块（转移所有权）
    ///
    /// TensorCoreTLM 无状态, 可跨 SM 共享同一实例或 per-SM 各创建一个。
    /// @param sm_id SM 编号 (0..num_sms()-1), 越界则忽略
    /// @param tc    TensorCoreTLM 实例
    virtual void inject_tensor_core(uint32_t sm_id,
                                    std::unique_ptr<ITensorCoreTiming> tc) = 0;

    // ──── 拓扑查询 ────

    /// 获取 PTX-EMU 端 SM 数量
    ///
    /// CppTLM main.cpp 用此查询创建 N 个 per-SM ScoreboardTLM 实例。
    virtual uint32_t num_sms() const = 0;
};

// ══════════════════════════════════════════════════════════════════════
// 跨 .so Driver API — C-compatible vtable (Phase 4 Wave 0 跟进)
// ══════════════════════════════════════════════════════════════════════
//
// PtxEmuDriverShim 不继承 IPtxEmuDriver（避免 PTX-EMU 编译期依赖 CppTLM 头文件）。
// 跨 .so 边界通过 C-compatible vtable 分发:
//   1. CppTLM 定义 PtxEmuDriverApi (vtable struct)
//   2. PTX-EMU populate PtxEmuDriverApi + 调用 cpptlm_set_driver(shim, api)
//   3. CppTLM DriverWrapper 利用 vtable 实现 IPtxEmuDriver
//
// inject_* 接收 raw pointer (caller 已 release unique_ptr):
//   - CppTLM: sb.release() → api.inject_scoreboard(shim, sm_id, raw)
//   - PTX-EMU: wrap in unique_ptr → std::move 到内部 vector

/// C-compatible vtable for PtxEmuDriverShim 跨 .so 调用
struct PtxEmuDriverApi {
    /// advance: 返回 int (0=NoOp, 1=Executed, 2=KernelComplete, -1=Error)
    /// actual_cycles 输出参数接收实际推进周期数
    int32_t (*advance)(void* ctx, uint32_t max_cycles, uint32_t* actual_cycles);

    /// is_kernel_complete: 查询 kernel 是否已完成
    bool (*is_kernel_complete)(void* ctx, uint64_t kernel_id);

    /// inject_*: 接收 raw ptr (caller 已 release), callee 取得所有权
    void (*inject_scoreboard)(void* ctx, uint32_t sm_id, IScoreboard* sb);
    void (*inject_pipeline)(void* ctx, uint32_t sm_id, IPipelineLatencyProvider* p);
    void (*inject_tensor_core)(void* ctx, uint32_t sm_id, ITensorCoreTiming* tc);

    /// 拓扑查询
    uint32_t (*num_sms)(void* ctx);

    /// mark_complete: on_complete 回调调用 (thread-safe)
    void (*mark_complete)(void* ctx, uint64_t kernel_id);

    /// destroy: 释放 shim 资源 (nullable — nullptr = caller 管理)
    void (*destroy)(void* ctx);
};

/// DriverWrapper: 将 opaque shim (void* + PtxEmuDriverApi) 适配为 IPtxEmuDriver
///
/// 用法:
///   PtxEmuDriverApi api = { .advance = ..., .inject_scoreboard = ... };
///   auto wrapper = std::make_unique<DriverWrapper>(shim_ptr, api);
///   some_kernel_launch->set_ptx_emu_driver(wrapper.get());
class DriverWrapper : public IPtxEmuDriver {
public:
    DriverWrapper(void* shim_ctx, PtxEmuDriverApi api)
        : shim_ctx_(shim_ctx), api_(api) {}

    ~DriverWrapper() override {
        if (api_.destroy && shim_ctx_) {
            api_.destroy(shim_ctx_);
        }
    }

    AdvanceResult advance(uint32_t max_cycles, uint32_t& actual_cycles) override {
        if (!shim_ctx_ || !api_.advance) return AdvanceResult::Error;
        int32_t r = api_.advance(shim_ctx_, max_cycles, &actual_cycles);
        switch (r) {
            case 0:  return AdvanceResult::NoOp;
            case 1:  return AdvanceResult::Executed;
            case 2:  return AdvanceResult::KernelComplete;
            default: return AdvanceResult::Error;
        }
    }

    bool is_kernel_complete(uint64_t kernel_id) override {
        if (!shim_ctx_ || !api_.is_kernel_complete) return false;
        return api_.is_kernel_complete(shim_ctx_, kernel_id);
    }

    void inject_scoreboard(uint32_t sm_id,
                           std::unique_ptr<IScoreboard> sb) override {
        if (shim_ctx_ && api_.inject_scoreboard) {
            api_.inject_scoreboard(shim_ctx_, sm_id, sb.release());
        }
    }

    void inject_pipeline(uint32_t sm_id,
                         std::unique_ptr<IPipelineLatencyProvider> p) override {
        if (shim_ctx_ && api_.inject_pipeline) {
            api_.inject_pipeline(shim_ctx_, sm_id, p.release());
        }
    }

    void inject_tensor_core(uint32_t sm_id,
                            std::unique_ptr<ITensorCoreTiming> tc) override {
        if (shim_ctx_ && api_.inject_tensor_core) {
            api_.inject_tensor_core(shim_ctx_, sm_id, tc.release());
        }
    }

    uint32_t num_sms() const override {
        if (!shim_ctx_ || !api_.num_sms) return 0;
        return api_.num_sms(shim_ctx_);
    }

private:
    void* shim_ctx_;
    PtxEmuDriverApi api_;
};

/// 全局 PTX-EMU 驱动指针
///
/// 由 cpptlm_set_driver() 在 PTX-EMU initialize_environment() 时设置。
/// nullptr = PTX-EMU 未连接 (零退化, KernelLaunchTLM 回退到 Phase 8.A 独立模式)
extern IPtxEmuDriver* g_ptx_emu_driver;

}  // namespace tlm

/// 跨 .so ABI 入口 — PTX-EMU 端 initialize_environment() 调用
///
/// @param shim_ctx  PtxEmuDriverShim opaque pointer
/// @param api       C-compatible vtable (tlm::PtxEmuDriverApi)
/// @note 定义在 src/tlm/gpu/ptx_emu_driver_shim.cc
extern "C" void cpptlm_set_driver(void* shim_ctx, tlm::PtxEmuDriverApi api);

#endif  // TLM_GPU_PTX_EMU_DRIVER_HH
