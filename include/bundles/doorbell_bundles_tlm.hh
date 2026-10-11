// include/bundles/doorbell_bundles_tlm.hh
// DoorbellBundle: PCIe posted write 语义 wire-format (LightweightInterconnect DoorbellBus)
// 修复 Gap J (SDMA ring 提交 doorbell 直调)
// 字段设计 (per cpptlm-lwc-bus-hub-mvp design.md §3 + v1.0.1 B-H4 修订):
//   - wdata[32]: 4-byte write payload (doorbell wptr, PCIe posted write max 4B dword)
//   - ring_id[8]: doorbell ring ID (0-255, 本 SoC 仅 0 = BAR1 ring 0)
//   - posted[1]: 1=posted (PCIe posted write, 不需 completion TLP), 0=non-posted
//   - valid[1]: producer 写 valid (B-H4: ready 是 consumer 驱动, valid 才是 producer 写)
//   - ready[1]: SDMA ring consumer 写 ready (consumer 驱动, 表明已消费)
// 编译期 BUNDLE_FIELD_WIDTH 锁 max width 64-bit
// 作者: CppTLM Team · 日期: 2026-10-09
#ifndef BUNDLES_DOORBELL_BUNDLES_TLM_HH
#define BUNDLES_DOORBELL_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief DoorbellBundle — PCIe posted write doorbell (LWC DoorbellBus)
 *
 * 真实物理 IP 对应: AMD GFX12 / NVIDIA HDA doorbell Posted Write Bus.
 * 替代 DGpuBoard::mmio_write(BAR1, 0x10010000) 对 SdmaEngineTLM::mmio_write() 的直调.
 */
struct DoorbellBundle : public bundle_base {
    // ========== posted-write 5 字段 ==========
    ch_uint<32> wdata;     // 4-byte doorbell wptr payload
    ch_uint<8>  ring_id;   // doorbell ring ID (0 = BAR1 ring 0)
    ch_uint<1>  posted;    // 1=posted write (no completion TLP)
    ch_uint<1>  valid;     // producer 写 valid (发起 doorbell)
    ch_uint<1>  ready;     // SDMA ring consumer 写 ready (消费反馈)

    DoorbellBundle() = default;

    // ========== 便捷工厂 (per design §3 集成路径) ==========
    /** @brief 构造 BAR1 ring 0 doorbell posted write */
    static DoorbellBundle make_doorbell(uint32_t wptr, uint8_t ring = 0) {
        DoorbellBundle db;
        db.wdata.write(wptr);
        db.ring_id.write(ring);
        db.posted.write(1u);
        db.valid.write(1u);
        return db;
    }
};

}  // namespace bundles

// 编译期字段宽度契约锁 (per spec doorbell-bundle-wire-format)
#include "utils/port_contract_asserts.hh"
BUNDLE_FIELD_WIDTH(bundles::DoorbellBundle, wdata, 32);
BUNDLE_FIELD_WIDTH(bundles::DoorbellBundle, ring_id, 8);
BUNDLE_FIELD_WIDTH(bundles::DoorbellBundle, posted, 1);
BUNDLE_FIELD_WIDTH(bundles::DoorbellBundle, valid, 1);
BUNDLE_FIELD_WIDTH(bundles::DoorbellBundle, ready, 1);

#endif  // BUNDLES_DOORBELL_BUNDLES_TLM_HH
