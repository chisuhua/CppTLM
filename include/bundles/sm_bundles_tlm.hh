// include/bundles/sm_bundles_tlm.hh
// SM 微架构 8 种 Bundle POD 定义 (per architecture/15 §15.4 + ADR-SOC-16 §2.2)
//
// 8 种 Bundle 流向:
//   Fetch → Issue → Exec → Writeback → RegFile
//   LsuGlobal ↔ NoC
//   任意 → HazardTracker
//
// 作者 CppTLM Team / 日期 2027-02-09 (SM 重构 Task 6)
// 参考: docs/soc_arch/architecture/15-sm-microarchitecture-design.md §15.4
//       docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md §2.2
#ifndef BUNDLES_SM_BUNDLES_TLM_HH
#define BUNDLES_SM_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include "tlm/gpu/instruction_descriptor.hh"
#include <cstdint>

namespace bundles::sm {

// === 1. FetchToIssueBundle: Fetch → Issue ===
// 字段: instr_desc (来自 PTX-EMU 已解码的 InstrDescriptor) + warp_id + pc
struct FetchToIssueBundle : public bundle_base {
    cpptlm::gpu::InstrDescriptor instr_desc{};
    uint32_t warp_id = 0;
    uint32_t pc = 0;

    FetchToIssueBundle() = default;
};

// === 2. DecodeToIssueBundle: Decode → Issue ===
// 字段: + PipeClass + LatencyClass (来自 Decode 分类)
struct DecodeToIssueBundle : public FetchToIssueBundle {
    cpptlm::gpu::PipeClass    pipe = cpptlm::gpu::PipeClass::kScalarALU;
    cpptlm::gpu::LatencyClass latency_class = cpptlm::gpu::LatencyClass::kFixed1Cycle;
};

// === 3. IssueToExecBundle: Issue → Exec ===
// 字段: + src_values[2] + src_valid[2] (PTX-EMU 上行同步 -- per F1.4 双计算决策)
// 注意: src_values[] 缩减为 2 entries (per HSK-9 spec size constraint)
//       result_value[] 也是 2 entries, 对应 SM Exec 计算真值
struct IssueToExecBundle : public DecodeToIssueBundle {
    uint64_t src_values[2] = {0, 0};       // PTX-EMU pre-computed source operand values
    bool     src_valid[2] = {false, false};
};

// === 4. ExecToWritebackBundle: Exec → Writeback ===
// 字段: + result_value[2] + memory_data + exec_cycles
struct ExecToWritebackBundle : public IssueToExecBundle {
    uint64_t result_value[2] = {0, 0};    // SM-computed (timing truth, per F1.4)
    uint8_t  result_num = 0;
    bool     memory_data_valid = false;
    uint64_t memory_data = 0;
    uint32_t exec_cycles = 0;             // for HazardTracker release
};

// === 5. WritebackToRegFileBundle: Writeback → RegFile ===
// 字段: warp_id + dst_regs[2] + values[2] + is_accvgpr (CDNA MFMA accumulator)
struct WritebackToRegFileBundle : public bundle_base {
    uint32_t warp_id = 0;
    uint16_t dst_regs[2] = {0, 0};
    uint64_t values[2] = {0, 0};
    uint8_t  num_dst = 0;
    bool     is_accvgpr = false;          // CDNA MFMA accumulator write

    WritebackToRegFileBundle() = default;
};

// === 6. MemoryReqBundle: Lsu → NoC ===
// 字段: vaddr + size + is_write + sm_id + wave_id + tag + lane_mask (intra-SM coalescing)
struct MemoryReqBundle : public bundle_base {
    uint64_t vaddr = 0;
    uint32_t size = 0;
    bool     is_write = false;
    uint32_t sm_id = 0;
    uint32_t wave_id = 0;
    uint64_t tag = 0;
    uint64_t lane_mask = 0;                // intra-SM coalescing mask

    MemoryReqBundle() = default;
};

// === 7. MemoryRespBundle: NoC → Lsu ===
// 字段: tag + sm_id + wave_id + data + is_hit + cycles (HazardTracker release)
struct MemoryRespBundle : public bundle_base {
    uint64_t tag = 0;
    uint32_t sm_id = 0;
    uint32_t wave_id = 0;
    uint64_t data = 0;
    bool     is_hit = true;
    uint32_t cycles = 0;                  // for HazardTracker

    MemoryRespBundle() = default;
};

// === 8. ScoreboardQueryBundle: 任意 → Hazard ===
// 字段: QueryType (IsStalled/Decrement/Increment) + warp_id + sm_id + ctrl
struct ScoreboardQueryBundle : public bundle_base {
    enum class QueryType : uint8_t {
        kIsStalled = 0,
        kDecrement = 1,
        kIncrement = 2,
    };
    QueryType query_type = QueryType::kIsStalled;
    uint32_t warp_id = 0;
    uint32_t sm_id = 0;
    cpptlm::gpu::CtrlBits ctrl{};

    ScoreboardQueryBundle() = default;
};

}  // namespace bundles::sm

#endif