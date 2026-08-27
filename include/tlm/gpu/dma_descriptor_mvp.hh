// include/tlm/gpu/dma_descriptor_mvp.hh
// DMA 描述符 C++ API 类型（组件 API 入口，非 Bundle）
// 功能描述：定义 SdmaEngineTLM 接收的 DMA 描述符 C++ 强类型
//           与 POD bundle DmaDescriptorBundle（include/bundles/dma_bundles_tlm.hh）语义对齐
//           组件 API 接受本类型，内部转换为 DmaDescriptorBundle 经 desc_in 端口下发
// 作者 CppTLM Team / 日期 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/design.md §3
//       ADR-SOC-07 D3 (PCIe master 归属 SOC)
#ifndef CPPTLM_TLM_GPU_DMA_DESCRIPTOR_MVP_HH
#define CPPTLM_TLM_GPU_DMA_DESCRIPTOR_MVP_HH

#include <cstdint>

namespace tlm::gpu {

/**
 * @brief DMA 描述符（组件 API 强类型，非 POD bundle）
 *
 * 字段（per design.md §3）：
 *   - dir         : 传输方向 (H2D / D2H)
 *   - host_iova   : host 侧 IOVA（经 IOMMU translate 为 PA）
 *   - vram_offset : SOC VRAM 内偏移
 *   - size        : 字节数
 *   - tag         : 完成关联 ID（用于 done_out 完成通知回传）
 *
 * 设计原则（per design.md §3 + spec.md）：
 *   - C++ 强类型，非 bundle_base 派生（与 DmaDescriptorBundle 语义对齐，但 API 层独立）
 *   - POD 惯例：仅基本类型字段，无堆指针
 *   - 组件 API 接受本类型，内部转换为 DmaDescriptorBundle 经 desc_in 端口下发
 *
 * 与 DmaDescriptorBundle 的转换：
 *   DmaDescriptor → DmaDescriptorBundle (to_bundle)：逐字段映射
 *   DmaDescriptorBundle → DmaDescriptor (from_bundle)：逐字段映射
 *   详见 sdma_engine_tlm.cc 内部辅助函数
 */
struct DmaDescriptor {
    enum class Dir : uint8_t {
        H2D = 0,  // host→device：经 host_out 读 host，写 VRAM (mem_out)
        D2H = 1,  // device→host：经 mem_in 读 VRAM，写 host (host_out)
    };

    Dir      dir = Dir::H2D;
    uint64_t host_iova = 0;    // host 侧 IOVA（经 IOMMU translate）
    uint64_t vram_offset = 0;  // SOC VRAM 内偏移
    uint32_t size = 0;         // 字节数（0 表示空描述符，触发拒绝）
    uint32_t tag = 0;          // 完成关联

    DmaDescriptor() = default;

    DmaDescriptor(Dir d, uint64_t iova, uint64_t vram_off, uint32_t sz, uint32_t t)
        : dir(d), host_iova(iova), vram_offset(vram_off), size(sz), tag(t) {}
};

} // namespace tlm::gpu

#endif // CPPTLM_TLM_GPU_DMA_DESCRIPTOR_MVP_HH
