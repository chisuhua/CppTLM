// include/bundles/cr_bundles_tlm.hh
// CrBundle: APB-lite 风格配置寄存器访问 wire-format (LightweightInterconnect CR-Bus)
// 修复 Gap H (CR-Bus 寄存器访问) + Gap K (MMIO Read 同步 AR/R)
// 字段设计 (per cpptlm-lwc-bus-hub-mvp design.md §2 + spec.md cr-bundle-wire-format):
//   - address[32]: register file byte offset (与 PCI BAR0 4KB 子窗口对齐)
//   - wdata[32]: 写数据, 仅 write==1 时有效
//   - rdata[32]: 读 response, 仅 write==0 时有效
//   - write[1]: 1=write, 0=read
//   - ready[1]: ready (从 IP target 返)
//   - valid[1]: valid (从 source 发)
//   - error[2]: 0=OK, 1=DECERR, 2=SLVERR (per D-AXI v1.4 B9 SLVERR propagation)
//   - transaction_id[8]: req id (8-bit 足够 APB lite)
// 编译期 BUNDLE_FIELD_WIDTH 锁 max width 64-bit (per 2026-10-06 P0 fix)
// 作者: CppTLM Team · 日期: 2026-10-09
#ifndef BUNDLES_CR_BUNDLES_TLM_HH
#define BUNDLES_CR_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief CrBundle — APB-lite 风格配置寄存器访问 (LWC CR-Bus)
 *
 * 真实物理 IP 对应: NVIDIA H100 / AMD GFX12 PCIe EP 的 APB-lite (CR-Bus)
 * 替代 DGpuBoard::mmio_write 对 GmmuTLM::set_pt_base_*() 的 process-direct 直调.
 */
struct CrBundle : public bundle_base {
    // ========== APB-lite 8 字段 ==========
    ch_uint<32> address;         // register file byte address (BAR0 子窗口 offset)
    ch_uint<32> wdata;           // 32-bit write data (write==1 时有效)
    ch_uint<32> rdata;           // 32-bit read response data (write==0 时有效)
    ch_uint<1>  write;           // 1=write, 0=read
    ch_uint<1>  ready;           // ready (从 target 返)
    ch_uint<1>  valid;           // valid (从 source 发)
    ch_uint<2>  error;           // 0=OK, 1=DECERR, 2=SLVERR
    ch_uint<8>  transaction_id;  // req id (8-bit, APB-lite 同步 ready/valid 单 outstanding)

    CrBundle() = default;

    // ========== 错误码常量 (per D-AXI v1.4 B9) ==========
    static constexpr uint8_t ERR_OK     = 0;
    static constexpr uint8_t ERR_DECERR = 1;
    static constexpr uint8_t ERR_SLVERR = 2;

    // ========== 便捷工厂 (per design §2 集成路径) ==========
    /** @brief 构造 CR write transaction */
    static CrBundle make_write(uint32_t addr, uint32_t data, uint8_t tx_id = 0) {
        CrBundle tx;
        tx.address.write(addr);
        tx.wdata.write(data);
        tx.write.write(1u);
        tx.valid.write(1u);
        tx.transaction_id.write(tx_id);
        return tx;
    }

    /** @brief 构造 CR read transaction */
    static CrBundle make_read(uint32_t addr, uint8_t tx_id = 0) {
        CrBundle tx;
        tx.address.write(addr);
        tx.write.write(0u);
        tx.valid.write(1u);
        tx.transaction_id.write(tx_id);
        return tx;
    }
};

}  // namespace bundles

// 编译期字段宽度契约锁 (per spec cr-bundle-wire-format Scenario 1 + 2026-10-06 P0 fix)
#include "utils/port_contract_asserts.hh"
BUNDLE_FIELD_WIDTH(bundles::CrBundle, address, 32);
BUNDLE_FIELD_WIDTH(bundles::CrBundle, wdata, 32);
BUNDLE_FIELD_WIDTH(bundles::CrBundle, rdata, 32);
BUNDLE_FIELD_WIDTH(bundles::CrBundle, write, 1);
BUNDLE_FIELD_WIDTH(bundles::CrBundle, ready, 1);
BUNDLE_FIELD_WIDTH(bundles::CrBundle, valid, 1);
BUNDLE_FIELD_WIDTH(bundles::CrBundle, error, 2);
BUNDLE_FIELD_WIDTH(bundles::CrBundle, transaction_id, 8);

#endif  // BUNDLES_CR_BUNDLES_TLM_HH
