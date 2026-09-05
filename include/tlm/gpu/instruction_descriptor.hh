// include/tlm/gpu/instruction_descriptor.hh
// InstrDescriptor POD: ISA-agnostic 指令描述符 (per architecture/15 §15.5.6 + ADR-SOC-16 §2.3)
//
// 功能:
//   - PipeClass (7 枚举): 指令发射到哪个管道
//   - LatencyClass (6 枚举): 延迟类, 用于 HazardTracker
//   - CtrlBits: 控制位 (EXEC mask 在 InstrDescriptor::exec_mask 字段独立 64-bit)
//   - InstrDescriptor POD: 完整指令描述符 (含 isa_type + instr_id + result_value[] + memory_data)
//
// 跨仓契约 (per HSK-9 §3):
//   - PTX-EMU 端通过 set_instr_descriptor_buf() 注入此 POD
//   - CppTLM SM 端 FetchUnit → DecodeUnit → IssueUnit 流水消费
//
// 作者 CppTLM Team / 日期 2027-02-09 (SM 重构 Task 4 + Task 8.5 修复)
// 参考: docs/soc_arch/architecture/15-sm-microarchitecture-design.md §15.5.6
//       docs/soc_arch/adr/hsk9-announcement-draft.md §3
//       external/PTX-EMU/include/ptxemu/device_api.h (IPtxEmuDevice 11 preserved)
#ifndef TLM_GPU_INSTRUCTION_DESCRIPTOR_HH
#define TLM_GPU_INSTRUCTION_DESCRIPTOR_HH

#include <cstdint>
#include <cstddef>

namespace cpptlm {
namespace gpu {

// === Pipe 类别 (per architecture/15 §15.5.6) ===
enum class PipeClass : uint8_t {
    kScalarALU = 0,    // 标量 ALU
    kVectorALU = 1,    // 向量 ALU (V-pipe)
    kMatrixCore = 2,   // 矩阵核心 (CDNA MFMA / NV Tensor Core)
    kSIMTLane = 3,     // SIMT lane 控制
    kLsuGlobal = 4,    // 全局内存
    kLsuLDS = 5,       // 共享内存
    kBranch = 6,       // 分支
};

// === Latency 类别 (per architecture/15 §15.5.6) ===
enum class LatencyClass : uint8_t {
    kFixed1Cycle = 0,   // 1 cycle (简单算术)
    kFixed4Cycle = 1,   // 4 cycles (FFMA 等)
    kFixed8Cycle = 2,   // 8 cycles
    kFixed16Cycle = 3,  // 16 cycles
    kFixed32Cycle = 4,  // 32 cycles (MFMA)
    kMemory = 5,        // 内存 (延迟由 memory subsystem 决定)
};

// === ISA 类别 (per architecture/15 §15.5.6) ===
enum class IsaType : uint8_t {
    kUnknown = 0,
    kCDNA64 = 1,        // AMD CDNA (GFX9, GFX10, GFX11)
    kPTX70 = 2,         // NVIDIA PTX 7.0+ (compatible with ptxemu)
    kSASS = 3,          // NVIDIA SASS (not yet supported)
};

// === 控制位 (per architecture/15 §15.5.6) ===
// EXEC mask 是 64-bit (CDNA wave64) — per Oracle P1-3 Task 8 review
struct CtrlBits {
    uint8_t branch_type = 0;       // 0=NONE, 1=COND, 2=UNCOND, 3=CALL
    uint8_t is_accvgpr = 0;        // 写入 ACCVGPR (CDNA MFMA accumulator)
    uint8_t reserved_ctrl0 = 0;
    uint8_t reserved_ctrl1 = 0;
    // exec_mask 是 64-bit, 占用 CtrlBits 之外 (InstrDescriptor::exec_mask 字段)
};

// === 完整指令描述符 POD ===
// sizeof 目标 <= 128 字节 (扩展 from HSK-9 §3 R9.2 47 bytes 估计, 容纳 result_value[4] + dst/src_regs[4] + 64-bit masks)
struct InstrDescriptor {
    // === 8 字节 header ===
    IsaType isa_type = IsaType::kUnknown;  // 1 字节
    uint8_t  result_num = 0;                // 1 字节 (number of result_value[] entries: 0-4)
    uint8_t  num_src = 0;                  // 1 字节 (source operand count, for Writeback/Hazard)
    uint8_t  num_dst = 0;                  // 1 字节 (destination operand count)
    uint32_t reserved_hdr = 0;             // 4 字节 padding (align 8)

    // === 8 字节 instr_id + pc + sm_id (16 字节) ===
    uint64_t instr_id = 0;                 // **8 字节** -- PTX-EMU 唯一标识 (per design §15.5.6, 与
                                           // is_instruction_completed(uint64_t) 匹配; per Oracle
                                           // P0-1 Task 8 review 修复 uint16_t → uint64_t 截断 bug)
    uint32_t pc = 0;                       // 4 字节 PC (per Oracle P1-2 修复)
    uint32_t sm_id = 0;                   // 4 字节 SM id (multi-SM 标识)

    // === 16 字节 exec info ===
    uint16_t exec_cycles = 0;              // 本指令占用的 SM cycle 数
    uint16_t cycles_remaining = 0;        // HazardTracker 剩余等待
    uint8_t  warpid = 0;                   // warp 编号
    uint8_t  reserved_exec = 0;            // padding
    uint64_t exec_mask = 0xFFFFFFFFFFFFFFFFull;  // 64-bit EXEC mask (per Oracle P1-3)

    // === 32 字节 result_value[4] + dst_regs[4] + src_regs[4] ===
    uint64_t result_value[4] = {0, 0, 0, 0};      // 4×8=32B, SM Exec 计算真值 (per F1.4 双计算决策)
    uint16_t dst_regs[4] = {0, 0, 0, 0};          // 4×2=8B, 目标寄存器 (per design §15.5.6, Oracle P1-1)
    uint16_t src_regs[4] = {0, 0, 0, 0};          // 4×2=8B, 源寄存器 (per design §15.5.6, Oracle P1-1)

    // === 16 字节 memory_data ===
    uint64_t memory_data = 0;                      // 8B
    uint8_t  memory_data_valid = 0;                // 1B
    bool     is_memory = false;                    // 1B (per Oracle P1-1 缺失字段)
    uint8_t  mem_size = 0;                         // 1B (per Oracle P1-1 缺失字段)
    uint32_t reserved_mem = 0;                    // 4B padding
    uint64_t target_vaddr = 0;                    // 8B (per Oracle P1-1 缺失字段, LsuGlobal 需要)

    // === 2 字节 pipe + latency ===
    PipeClass    pipe = PipeClass::kScalarALU;
    LatencyClass latency_class = LatencyClass::kFixed1Cycle;

    // === 4 字节 CtrlBits ===
    CtrlBits ctrl{};

    // === 8 字节 lane_mask (intra-warp, 64-bit for wave64) ===
    uint64_t lane_mask = 0xFFFFFFFFFFFFFFFFull;    // 64-bit (per Oracle P1-3 统一)
};

// 静态断言: InstrDescriptor 大小应 <= 128 字节 (扩展 from HSK-9 §3 R9.2 47 bytes 估计)
static_assert(sizeof(InstrDescriptor) <= 128, "InstrDescriptor too large (>128 bytes)");

}  // namespace gpu
}  // namespace cpptlm

#endif