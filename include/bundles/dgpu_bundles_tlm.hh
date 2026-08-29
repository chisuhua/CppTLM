// include/bundles/dgpu_bundles_tlm.hh
// dGPU Board/SOC/Pcie/Sdma Bundle 定义 (轻量级 TLM 侧, POD)
// 功能描述: 定义 dGPU 跨能力域 ChStream 组件间的 POD Bundle:
//           - Pm4DispatchBundle: PM4 method packet (CP→TMU 派发)
//           - CtaDescriptorBundle: CTA descriptor (TMU→SQ 入队)
//           所有权声明 (per design §3.5 陷阱 4):
//           - 本文件 OWNED: Pm4DispatchBundle, CtaDescriptorBundle
//           - NOT OWNED: CompletionBundle (归属 dma_bundles_tlm.hh, sdma 唯一所有者)
// 作者 CppTLM Team / 日期 2026-08-29
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-board-soc-split/design.md §3.5
//       ADR-SOC-07 D5 (Bundle 命名空间 ODR 约束)
#ifndef BUNDLES_DGPU_BUNDLES_TLM_HH
#define BUNDLES_DGPU_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief PM4 method packet dispatch bundle
 * (轻量级 TLM 侧, POD 化 Pm4MethodDispatch)
 *
 * 字段 (per design §3.5):
 *   - method_addr  : PM4 method_addr (15-bit, packed in 16)
 *   - method_type  : Pm4MethodType (TYPE3 opcodes 0x4000-0x4600 enum)
 *   - payload_size : payload dword 数 (0-16)
 *   - trans_id     : 关联 tag (用于 future match 回 host 线程)
 *
 * 用途: CP::execute_method → TMU::dispatch 通道.
 */
struct Pm4DispatchBundle : public bundle_base {
    ch_uint<16> method_addr;
    ch_uint<8>  method_type;   // Pm4MethodType enum cast (0..N)
    ch_uint<8>  payload_size;
    ch_uint<32> trans_id;

    Pm4DispatchBundle() = default;

    Pm4DispatchBundle(uint16_t addr, uint8_t type, uint8_t sz, uint32_t tid)
        : method_addr(addr), method_type(type), payload_size(sz), trans_id(tid) {}
};

/**
 * @brief CTA descriptor bundle (轻量级 TLM 侧, POD 化 CtaDescriptor)
 *
 * 字段 (per design §3.5):
 *   - task_id           : CP dispatch 分配的 task_id
 *   - vram_image_addr   : VRAM image handle (s2 use 0 在 v3.0 用 64-bit)
 *   - grid/block_xyz    : grid + block 各维
 *   - shared_mem_bytes  : 共享内存字节数
 *   - args_vram_addr    : args VRAM offset
 *
 * 用途: TMU::dispatch → SQ::enqueue 通道.
 */
struct CtaDescriptorBundle : public bundle_base {
    ch_uint<32> task_id;
    ch_uint<64> vram_image_addr;
    ch_uint<32> grid_x;
    ch_uint<32> grid_y;
    ch_uint<32> grid_z;
    ch_uint<32> block_x;
    ch_uint<32> block_y;
    ch_uint<32> block_z;
    ch_uint<32> shared_mem_bytes;
    ch_uint<64> args_vram_addr;

    CtaDescriptorBundle() = default;

    CtaDescriptorBundle(uint32_t tid, uint64_t vram, uint32_t gx, uint32_t gy, uint32_t gz,
                         uint32_t bx, uint32_t by, uint32_t bz, uint32_t shm, uint64_t args)
        : task_id(tid), vram_image_addr(vram),
          grid_x(gx), grid_y(gy), grid_z(gz),
          block_x(bx), block_y(by), block_z(bz),
          shared_mem_bytes(shm), args_vram_addr(args) {}
};

// NOTE: CompletionBundle **不在本文件定义** -- 它由 cpptlm-dgpu-sdma-engine
// change 交付的 include/bundles/dma_bundles_tlm.hh 唯一所有. 任何字段扩展
// 必须由 sdma 主导并通知本 change 跟进 (避免 bundles 命名空间 ODR 冲突).

} // namespace bundles

#endif // BUNDLES_DGPU_BUNDLES_TLM_HH
