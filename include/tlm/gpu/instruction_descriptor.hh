// include/tlm/gpu/instruction_descriptor.hh
// InstrDescriptor POD: ISA-agnostic 指令描述符 (per architecture/15 §15.5.6 + ADR-SOC-16 §2.3)
//
// 功能:
//   - PipeClass (7 枚举): 指令发射到哪个管道
//   - LatencyClass (6 枚举): 延迟类, 用于 HazardTracker
//   - CtrlBits (6 字段×1 字节): 控制位 (EXEC mask, branch target, etc.)
//   - InstrDescriptor POD: 完整指令描述符 (含 isa_type + instr_id + result_value[] + memory_data)
//
// 跨仓契约 (per HSK-9 §3):
//   - PTX-EMU 端通过 set_instr_descriptor_buf() 注入此 POD
//   - CppTLM SM 端 FetchUnit → DecodeUnit → IssueUnit 流水消费
//
// 作者 CppTLM Team / 日期 2027-02-09 (SM 重构 Task 4)
// 参考: docs/soc_arch/architecture/15-sm-microarchitecture-design.md §15.5.6
//       docs/soc_arch/adr/hsk9-announcement-draft.md §3
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

// === 控制位 (6 字段, per architecture/15 §15.5.6) ===
// 每个字段 1 字节, 共 6 字节
struct CtrlBits {
    uint8_t exec_mask_lo = 0xFF;   // 低 32 位 EXEC mask (lo)
    uint8_t exec_mask_hi = 0xFF;   // 高 32 位 EXEC mask (hi) -- 64-bit total
    uint8_t branch_type = 0;       // 0=NONE, 1=COND, 2=UNCOND, 3=CALL
    uint8_t is_accvgpr = 0;        // 写入 ACCVGPR (CDNA MFMA accumulator)
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
};

// === 完整指令描述符 POD ===
// sizeof = 4 (isa_type + instr_id + result_num + reserved) + 8 (result_value[4]) + 8 (memory_data + memory_data_valid) +
//          4 (exec_cycles + cycles_remaining + warpid + pc) + 1 (pipe) + 1 (latency) + 6 (ctrl) +
//          4 (lane_mask_lo + lane_mask_hi + reserved) = ~40 字节
struct InstrDescriptor {
    // === 4 字节 header ===
    IsaType isa_type = IsaType::kUnknown;  // 1 字节
    uint8_t  result_num = 0;                // 1 字节 (number of result_value[] entries: 0-2)
    uint16_t instr_id = 0;                  // 2 字节 (PTX-EMU 唯一标识, 用于 is_instruction_completed)

    // === 16 字节 result_value[] (2 × 64-bit) ===
    // 由 SM Exec 计算的真值 (timing truth) per F1.4 双计算决策
    // CDNA MFMA 用 result_value[0] (32-bit packed) 或 result_value[0..1] (64-bit split)
    uint64_t result_value[2] = {0, 0};

    // === 8 字节 memory_data ===
    uint64_t memory_data = 0;
    uint8_t  memory_data_valid = 0;        // 1 字节 (非对齐 8 字节, 与 memory_data 一起 ~9 字节)

    // === 4 字节 exec info ===
    uint16_t exec_cycles = 0;              // 本指令占用的 SM cycle 数
    uint16_t cycles_remaining = 0;        // HazardTracker 剩余等待
    uint8_t  warpid = 0;                   // warp 编号
    uint8_t  pc_lo = 0;                    // PC 低 8 位
    uint16_t pc = 0;                       // PC 16-bit

    // === 1 字节 pipe + 1 字节 latency ===
    PipeClass    pipe = PipeClass::kScalarALU;
    LatencyClass latency_class = LatencyClass::kFixed1Cycle;

    // === 6 字节 CtrlBits ===
    CtrlBits ctrl{};

    // === 4 字节 lane_mask (intra-warp) ===
    uint32_t lane_mask = 0xFFFFFFFF;       // 32-bit 默认全 1 (所有 lane 活跃)
};

// 静态断言: InstrDescriptor 大小应 <= 64 字节 (per HSK-9 §3 R9.2 47 bytes 估计, 实际 ~56 字节)
static_assert(sizeof(InstrDescriptor) <= 64, "InstrDescriptor too large (>64 bytes)");

}  // namespace gpu
}  // namespace cpptlm

#endif