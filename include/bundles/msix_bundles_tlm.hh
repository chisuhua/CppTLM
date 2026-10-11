// include/bundles/msix_bundles_tlm.hh
// MsixBundle: MSI-X 中断交付 wire-format (LightweightInterconnect, Change E 复用预留)
// 修复 Gap L (MSI-X 中断交付, deferred 到 Change E cpptlm-msix-generator-ip)
// 字段设计 (per cpptlm-lwc-bus-hub-mvp proposal.md §0.1.1 + design.md §2 Bundle 选型表):
//   - vector_id[32]: MSI-X vector index
//   - payload[32]: MSI-X write data (32-bit)
//   - posted[1]: 1=直接投递 (posted), 0=deferred (软件 pending 位)
// 编译期 BUNDLE_FIELD_WIDTH 锁 max width 64-bit
// 作者: CppTLM Team · 日期: 2026-10-09
#ifndef BUNDLES_MSIX_BUNDLES_TLM_HH
#define BUNDLES_MSIX_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief MsixBundle — MSI-X 中断交付 (LWC 中断总线)
 *
 * 真实物理 IP 对应: ARM GICv3 SPI/Distributor MSI/MSI-X 报文.
 * Change B scope 包含 (per architecture.md §0.1.1 + §0.2 表 + D-AXI v1.4 Plan);
 * Change E (cpptlm-msix-generator-ip, Phase 2) 复用 LWC 总线 + 本 bundle 实施.
 */
struct MsixBundle : public bundle_base {
    ch_uint<32> vector_id;   // MSI-X vector index (0-2047)
    ch_uint<32> payload;     // MSI-X write data
    ch_uint<1>  posted;      // 1=direct delivery, 0=deferred (pending bit)

    MsixBundle() = default;

    /** @brief 构造 posted 直接投递中断 */
    static MsixBundle make_irq(uint32_t vector, uint32_t data = 0) {
        MsixBundle msix;
        msix.vector_id.write(vector);
        msix.payload.write(data);
        msix.posted.write(1u);
        return msix;
    }
};

}  // namespace bundles

// 编译期字段宽度契约锁
#include "utils/port_contract_asserts.hh"
BUNDLE_FIELD_WIDTH(bundles::MsixBundle, vector_id, 32);
BUNDLE_FIELD_WIDTH(bundles::MsixBundle, payload, 32);
BUNDLE_FIELD_WIDTH(bundles::MsixBundle, posted, 1);

#endif  // BUNDLES_MSIX_BUNDLES_TLM_HH
