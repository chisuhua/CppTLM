// include/tlm/gpu/ptx_emu_driver.hh
// IPtxEmuDriver: 跨仓库协同仿真窄驱动接口 — D1-Full P1 Phase 4
// + DriverWrapper: 跨 .so PtxEmuDriverShim 适配层 (Wave 0 跟进)
// 功能: CppTLM(C++17) 定义纯虚接口, PTX-EMU(C++20) 实现 PtxEmuDriverShim,
//       通过 advance() 推进 GPUContext::exe_once() 并注入 CppTLM 端 Scoreboard/
//       Pipeline/TensorCore timing 模块
// 作者 CppTLM Team / 日期 2026-07-18
// 参考:
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/design.md §8
//   - include/cudart/cpptlm_bridge.h (vendored, ABI 真值源 — PtxEmuDriverApi)
//   - PTX-EMU commit 13325cd8 (PtxEmuDriverApi vtable 定义)
//   - PTX-EMU openspec/changes/cpptlm-p1-ptxemu-shim (Wave 0 姊妹 change)
#ifndef TLM_GPU_PTX_EMU_DRIVER_HH
#define TLM_GPU_PTX_EMU_DRIVER_HH

#include "cudart/cpptlm_bridge.h"           // PtxEmuDriverApi (ABI 真值源, vendored)
#include "cudart/pipeline_interface.h"       // IPipelineLatencyProvider + PipelineId
#include "cudart/scoreboard_interface.h"     // IScoreboard
#include "cudart/tensor_core_interface.h"    // ITensorCoreTiming + TcPrecision

#include <cstdint>
#include <memory>

// 编译期防御: PtxEmuDriverApi 布局与 PTX-EMU 端一致
// 8 个函数指针 * 8 字节 = 64 字节 (LP64)
static_assert(sizeof(PtxEmuDriverApi) == 64,
              "PtxEmuDriverApi size mismatch — check vendor sync with PTX-EMU cpptlm_bridge.h");

namespace tlm {

enum class AdvanceResult : uint8_t {
    Executed,
    NoOp,
    KernelComplete,
    Error
};

class IPtxEmuDriver {
public:
    virtual ~IPtxEmuDriver() = default;
    virtual AdvanceResult advance(uint32_t max_cycles, uint32_t& actual_cycles) = 0;
    virtual bool is_kernel_complete(uint64_t kernel_id) = 0;
    virtual void inject_scoreboard(uint32_t sm_id, std::unique_ptr<IScoreboard> sb) = 0;
    virtual void inject_pipeline(uint32_t sm_id,
                                 std::unique_ptr<IPipelineLatencyProvider> p) = 0;
    virtual void inject_tensor_core(uint32_t sm_id,
                                    std::unique_ptr<ITensorCoreTiming> tc) = 0;
    virtual uint32_t num_sms() const = 0;
};

class DriverWrapper : public IPtxEmuDriver {
public:
    DriverWrapper(void* shim_ctx, PtxEmuDriverApi api)
        : shim_ctx_(shim_ctx), api_(api) {}

    ~DriverWrapper() override {
        if (api_.destroy && shim_ctx_)
            api_.destroy(shim_ctx_);
    }

    AdvanceResult advance(uint32_t max_cycles, uint32_t& actual_cycles) override {
        if (!shim_ctx_ || !api_.advance) return AdvanceResult::Error;
        int r = api_.advance(shim_ctx_, max_cycles, &actual_cycles);
        switch (r) {
            case 0:  return AdvanceResult::NoOp;
            case 1:  return AdvanceResult::Executed;
            case 2:  return AdvanceResult::KernelComplete;
            default: return AdvanceResult::Error;
        }
    }

    bool is_kernel_complete(uint64_t kernel_id) override {
        if (!shim_ctx_ || !api_.is_kernel_complete) return false;
        return api_.is_kernel_complete(shim_ctx_, kernel_id) != 0;
    }

    void inject_scoreboard(uint32_t sm_id,
                           std::unique_ptr<IScoreboard> sb) override {
        if (shim_ctx_ && api_.inject_scoreboard)
            api_.inject_scoreboard(shim_ctx_, sm_id, static_cast<void*>(sb.release()));
    }

    void inject_pipeline(uint32_t sm_id,
                         std::unique_ptr<IPipelineLatencyProvider> p) override {
        if (shim_ctx_ && api_.inject_pipeline)
            api_.inject_pipeline(shim_ctx_, sm_id, static_cast<void*>(p.release()));
    }

    void inject_tensor_core(uint32_t sm_id,
                            std::unique_ptr<ITensorCoreTiming> tc) override {
        if (shim_ctx_ && api_.inject_tensor_core)
            api_.inject_tensor_core(shim_ctx_, sm_id, static_cast<void*>(tc.release()));
    }

    uint32_t num_sms() const override {
        if (!shim_ctx_ || !api_.num_sms) return 0;
        return api_.num_sms(shim_ctx_);
    }

private:
    void* shim_ctx_;
    PtxEmuDriverApi api_;
};

extern IPtxEmuDriver* g_ptx_emu_driver;

}  // namespace tlm

#endif