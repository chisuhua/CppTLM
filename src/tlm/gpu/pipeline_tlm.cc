// src/tlm/gpu/pipeline_tlm.cc
// PipelineTLM: IPipelineLatencyProvider 实现 — S4 Phase 2a + S5 Phase 2b 延迟表
// 作者 CppTLM Team / 日期 2026-07-18 (P1) + 2026-07-21 (S4/S5)
// 参考:
//   - PTX 指令延迟模型: A100 whitepaper §5.4.1 + GPGPU-Sim gpu_config.h
//   - PTX-EMU docs/dev-process/cpptlm-co-simulation-plan.md §2a.1/2b.1
//   - include/tlm/gpu/pipeline_tlm.hh
//
// 字符串匹配基于 PTX 指令名格式 (e.g. "add.f32", "ld.global.u32", "fma.rn.f32")
// get_fractional_cycles_by_type 使用管线平均值 (无跨仓库 PTX statement_type 枚举依赖)
#include "tlm/gpu/pipeline_tlm.hh"

#include <algorithm>
#include <cctype>
#include <string>

namespace tlm {
namespace {

/// 大小写不敏感子串查找
bool has(const std::string& haystack, const std::string& needle) {
    if (needle.size() > haystack.size()) return false;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char a, char b) { return std::toupper(a) == std::toupper(b); });
    return it != haystack.end();
}

// ===== P0_INT_FP32: 通用整/浮点管线 =====
//   add/sub/mov/setp/cvt/not/and/or/xor/shl/shr/abs/min/max/neg/popc/clz/bfe/bfi/brev     → 1.0
//   fma / add.f32 / mul.f32 / sub.f32 / div / rcp.approx (非 SFU) / abs/neg (f32 variants)  → 4.22
//   mad / mul.lo / mul.hi / sad                                                              → 2.0
double latency_p0(const std::string& instr) {
    // FMA / FP32 arithmetic: 4.22 cycles
    if (has(instr, "fma")  || has(instr, "mul.f32") || has(instr, "add.f32") ||
        has(instr, "sub.f32") || has(instr, "div")   || has(instr, "min.f32") ||
        has(instr, "max.f32") || has(instr, "abs.f32")|| has(instr, "neg.f32")) {
        return 4.22;
    }
    // Integer multiply-add / wide multiply: 2.0 cycles
    if (has(instr, "mad")   || has(instr, "mul.lo") || has(instr, "mul.hi") ||
        has(instr, "sad")   || has(instr, "mul.wide")) {
        return 2.0;
    }
    return 1.0;
}

// ===== P3_LSU: 加载/存储管线 =====
//   ld.global / ld.volatile   → 200+  (DRAM + NoC + cache miss, A100 ~404 typ)
//   st.global                  → 20+   (write buffer)
//   ld.shared / st.shared      → 1.0   (on-chip SRAM)
//   ld.local / st.local        → 5.0   (L1 cache)
//   atom / red                 → 200+  (global atomic)
double latency_p3(const std::string& instr) {
    if (has(instr, "ld.global") || has(instr, "ld.volatile")) return 200.0;
    if (has(instr, "st.global")) return 20.0;
    if (has(instr, "ld.shared") || has(instr, "st.shared")) return 1.0;
    if (has(instr, "ld.local") || has(instr, "st.local")) return 5.0;
    if (has(instr, "atom") || has(instr, "red.")) return 200.0;
    if (has(instr, "ld.")) return 200.0;
    if (has(instr, "st.")) return 20.0;
    return 200.0;
}

// ===== P1_FP64: 双精度浮点管线 =====
//   add.f64 / mul.f64 / fma.rn.f64 / sub.f64 / div.f64 / abs.f64 / neg.f64 → 8.0
double latency_p1(const std::string& instr) {
    return 8.0;
}

// ===== P2_SFU: 特殊功能单元 =====
//   sin / cos / tan          → 16.0
//   rcp / rsqrt              → 4.0
//   sqrt                     → 8.0
//   lg2 / ex2                → 8.0
double latency_p2(const std::string& instr) {
    if (has(instr, "sin") || has(instr, "cos") || has(instr, "tan")) return 16.0;
    if (has(instr, "rcp") || has(instr, "rsqrt")) return 4.0;
    if (has(instr, "sqrt")) return 8.0;
    if (has(instr, "lg2") || has(instr, "ex2")) return 8.0;
    return 8.0;
}

}  // anonymous namespace

PipelineTLM::PipelineTLM() : is_placeholder_(false) {}

double PipelineTLM::get_fractional_cycles(const std::string& instruction,
                                          PipelineId pipe_id) const {
    switch (pipe_id) {
        case PipelineId::P0_INT_FP32: return latency_p0(instruction);
        case PipelineId::V_SIMD:      return 1.0;
        case PipelineId::P1_FP64:     return latency_p1(instruction);
        case PipelineId::P2_SFU:      return latency_p2(instruction);
        case PipelineId::P3_LSU:      return latency_p3(instruction);
        case PipelineId::P4_TC:       return 0.0;
    }
    return 1.0;
}

double PipelineTLM::get_fractional_cycles_by_type(int /*statement_type*/,
                                                  PipelineId pipe_id) const {
    switch (pipe_id) {
        case PipelineId::P0_INT_FP32: return 2.0;
        case PipelineId::V_SIMD:      return 1.0;
        case PipelineId::P1_FP64:     return 8.0;
        case PipelineId::P2_SFU:      return 8.0;
        case PipelineId::P3_LSU:      return 200.0;
        case PipelineId::P4_TC:       return 0.0;
    }
    return 1.0;
}

}  // namespace tlm
