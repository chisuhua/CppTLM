// =====================================================================
// CppTLM ↔ PTX-EMU ABI 真值源 (vendored, DO NOT MODIFY)
// =====================================================================
// Source         : github.com/chisuhua/PTX-EMU @ commit 463038e0
// Source path    : include/ptxsim/tensor_core_interface.h (31 lines)
// Vendor method  : `git show 463038e0:include/ptxsim/tensor_core_interface.h`
// Vendor SHA-256 : 见 include/cudart/AGENTS.md 验收检查表 (每次 rebase 后更新)
//
// 任何修改必须先在 PTX-EMU 端提交新 commit，再同步 rebase 此 vendor 文件。
// =====================================================================

#ifndef PTXSIM_TENSOR_CORE_INTERFACE_H
#define PTXSIM_TENSOR_CORE_INTERFACE_H
#include <cstdint>

/// TensorCore precision enum — MUST match CppTLM tlm::TcPrecision (0-5).
/// CppTLM Adapter uses static_assert to verify at compile time.
/// Ref: ADR-0020, CppTLM RFC-P1-003 §3.2
enum class TcPrecision : uint32_t {
    FP4  = 0,
    FP6  = 1,
    FP8  = 2,
    FP16 = 3,
    BF16 = 4,
    TF32 = 5
};

/// Pure virtual interface for TensorCore timing injection.
/// get_latency_mnk provides a default implementation that degenerates to
/// get_latency(prec) — CppTLM can override for M/N/K-aware timing.
/// Ref: ADR-0020, CppTLM RFC-P1-001 §3.3
class ITensorCoreTiming {
public:
    virtual ~ITensorCoreTiming() = default;
    virtual uint32_t get_latency(TcPrecision prec) const = 0;
    virtual uint32_t get_throughput_cycles(TcPrecision prec) const = 0;
    virtual uint32_t get_latency_mnk(
        TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const {
        return get_latency(prec);
    }
};
#endif
