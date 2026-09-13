// include/tlm/gpu/sdma_packet.hh
// SdmaPacket: SG descriptor chain (Stage 1.3a, SD-RB-2)
// 功能描述：SG 描述符链 (MAX_SG_ENTRIES=8)，每个 SG 描述符含 iova/size/vram_offset/next_offset。
//           SdmaEngineTLM 在 H2D/D2H 处理时通过 sg_chain 拆解为多个 sub-desc。
// 作者 CppTLM Team / 日期 2026-09-13
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//       Scenario "SG descriptor chain ≥ 8"
#ifndef CPPTLM_TLM_GPU_SDMA_PACKET_HH
#define CPPTLM_TLM_GPU_SDMA_PACKET_HH

#include <array>
#include <cstdint>
#include <cstring>

#include "tlm/gpu/dma_descriptor_mvp.hh"

namespace tlm::gpu {

    /**
     * @brief SG 描述符 (Scatter-Gather element)
     *
     * 字段 (per spec.md Scenario "SG descriptor chain ≥ 8"):
     *   - iova         : host 侧 IOVA (经 IOMMU translate)
     *   - size         : 本 segment 字节数
     *   - vram_offset  : SOC VRAM 内偏移
     *   - next_offset  : ring 内偏移到下一个 SG (0 = chain end)
     *   - flags        : reserved (MVP 不用)
     *
     * Ring layout: 64B per entry, SG 描述符占前 sizeof(SgDescriptor) 字节
     * (spec 没有严格规定 SG 字段在 64B 内的 offset, MVP 用 32B 紧凑布局)
     */
    struct SgDescriptor {
        uint64_t iova = 0;
        uint32_t size = 0;
        uint64_t vram_offset = 0;
        uint32_t next_offset = 0;  // ring offset (0 = end of chain)
        uint32_t flags = 0;        // reserved, MVP 不用
    };

    /**
     * @brief SdmaPacket: 单个 SDMA 描述符 + SG chain (Stage 1.3a)
     *
     * 设计原则 (per spec "SG ≥ 8"):
     *   - MAX_SG_ENTRIES = 8 (硬上限; 超过 add_sg 返回 false)
     *   - add_sg() 追加到 chain; sg_count() 返回当前大小
     *   - sg_at(i) 返回第 i 个 SG 的常量引用 (i 越界未定义, MVP 不 assert)
     *   - serialize_descriptor() / deserialize_descriptor(): 64B ring entry 编解码
     */
    class SdmaPacket {
    public:
        // spec: "SG ≥ 8" — 硬上限 8 (chain 链式, >8 不实现)
        static constexpr uint32_t MAX_SG_ENTRIES = 8;
        // Ring entry 大小 (per spec "max 1024 entries @64B")
        static constexpr size_t kEntryBytes = 64;

        SdmaPacket() = default;
        ~SdmaPacket() = default;

        SdmaPacket(const SdmaPacket&) = delete;
        SdmaPacket& operator=(const SdmaPacket&) = delete;

        // 添加 SG descriptor; 满 (count >= MAX_SG_ENTRIES) 返回 false
        bool add_sg(const SgDescriptor& sg) {
            if (sg_count_ >= MAX_SG_ENTRIES) {
                return false;
            }
            sg_chain_[sg_count_] = sg;
            sg_count_++;
            return true;
        }

        uint32_t sg_count() const {
            return sg_count_;
        }

        const SgDescriptor& sg_at(uint32_t i) const {
            return sg_chain_[i];
        }

        // serialize_descriptor / deserialize_descriptor: 64B ring entry helper
        //   - serialize: DmaDescriptor → 64B array (ring entry 字节布局)
        //   - deserialize: 64B array → DmaDescriptor (round-trip 校验)
        // 注: Stage 1.3a wire-format 与 PcieTlpBundle 编码不同 (无 KIND_DMA_DESC),
        //   直接 memcpy 截断 sizeof(DmaDescriptor) 部分, 后续 bit field 留 0.
        static std::array<uint8_t, kEntryBytes> serialize_descriptor(const DmaDescriptor& d) {
            std::array<uint8_t, kEntryBytes> buf{};
            std::memcpy(buf.data(), &d, sizeof(DmaDescriptor));
            return buf;
        }
        static DmaDescriptor deserialize_descriptor(const uint8_t* data, size_t len) {
            DmaDescriptor d{};
            if (data != nullptr && len >= sizeof(DmaDescriptor)) {
                std::memcpy(&d, data, sizeof(DmaDescriptor));
            }
            return d;
        }

    private:
        std::array<SgDescriptor, MAX_SG_ENTRIES> sg_chain_{};
        uint32_t sg_count_ = 0;
    };

}  // namespace tlm::gpu

#endif  // CPPTLM_TLM_GPU_SDMA_PACKET_HH
