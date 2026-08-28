// =====================================================================
// CppTLM ↔ PTX-EMU ABI 真值源 (vendored, DO NOT MODIFY)
// =====================================================================
// Source         : github.com/chisuhua/PTX-EMU @ commit 9e7361b9
// Source path    : include/ptxsim/pipeline_interface.h (29 lines)
// Vendor method  : `git show 9e7361b9:include/ptxsim/pipeline_interface.h`
// Vendor SHA-256 : 见 include/cudart/AGENTS.md 验收检查表 (每次 rebase 后更新)
//
// 任何修改必须先在 PTX-EMU 端提交新 commit，再同步 rebase 此 vendor 文件。
// =====================================================================

#ifndef PTXSIM_PIPELINE_INTERFACE_H
#define PTXSIM_PIPELINE_INTERFACE_H
#include <cstdint>
#include <string>

/// Pipeline identifier enum — MUST match CppTLM tlm::PipelineId (0-5).
/// CppTLM Adapter uses static_assert to verify at compile time.
/// Ref: ADR-0020, CppTLM RFC-P1-003 §3.1
enum class PipelineId : uint32_t {
    P0_INT_FP32 = 0,
    V_SIMD = 1,
    P1_FP64 = 2,
    P2_SFU = 3,
    P3_LSU = 4,
    P4_TC = 5
};

/// Pure virtual interface for fractional-cycle pipeline latency injection.
/// int statement_type (not StatementType) avoids dependency on ptx_ir/ptx_types.h.
/// Ref: ADR-0020, CppTLM RFC-P1-001 §3.2
class IPipelineLatencyProvider {
public:
    virtual ~IPipelineLatencyProvider() = default;
    virtual double get_fractional_cycles(const std::string& instruction,
                                         PipelineId pipe_id) const = 0;
    virtual double get_fractional_cycles_by_type(int statement_type, PipelineId pipe_id) const = 0;
};
#endif
