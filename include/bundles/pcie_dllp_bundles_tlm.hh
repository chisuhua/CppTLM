// include/bundles/pcie_dllp_bundles_tlm.hh
// PCIe DLLP Bundle + PciePhyConfig 定义（轻量级 TLM 侧）
// 功能描述：定义 PcieEndpointIP 链路层使用的 DLLP Bundle 与 PHY 配置
//           - PcieDllpBundle：链路层 DLLP 通道（6 种 kind + Vendor）
//           - PciePhyConfig：PHY 配置（Gen1-5 速率 / 通道 / preset / SR-IOV）
// 作者 CppTLM Team / 日期 2026-09-08
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §6.2/§6.3
//       specs/link-layer-and-fc/spec.md Scenario "DLLP gen/parse"
#ifndef BUNDLES_PCIE_DLLP_BUNDLES_TLM_HH
#define BUNDLES_PCIE_DLLP_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief PCIe DLLP Bundle（轻量级 TLM 侧）
 *
 * 字段（per design.md §6.2/§6.3 冻结）：
 *   - kind          : DLLP 类型（ACK/NAK/InitFC1/InitFC2/UpdateFC/NOP/Vendor），8 bits
 *   - vc_id         : Virtual Channel ID（PCIe VC 0-7，默认 0，per Q11 单 VC0），4 bits
 *   - credit_P      : Posted token credit（16 bits，12-bit credit + 4-bit 保留）
 *   - credit_NP     : Non-Posted token credit（16 bits，12-bit credit + 4-bit 保留）
 *   - credit_Cpl    : Completion token credit（16 bits，12-bit credit + 4-bit 保留）
 *   - seq_num       : 发送序号（16 bits，PCIe 12-bit seq + 4-bit 保留，wrap 在 4095）
 *   - seq_num_ack   : ACK/NAK 携带的确认序号（16 bits 容器，12-bit 有效值）
 *   - trans_id      : 事务关联 ID（诊断用）
 *
 * 设计原则（per design.md §6.3 "全文档冻结" + pcie_bundles_tlm.hh 惯例）：
 *   - 轻量级 POD：仅字段，无堆指针，memcpy 可序列化（bundle_serialization.hh）
 *   - C++17 兼容，ch_uint<N> 类型
 */
struct PcieDllpBundle : public bundle_base {
    // ========== DLLP Kind 常量（PCIe 5.0 Base Spec §3.5，6 种 + Vendor）==========
    static constexpr uint8_t ACK       = 0;
    static constexpr uint8_t NAK       = 1;
    static constexpr uint8_t INIT_FC1  = 2;
    static constexpr uint8_t INIT_FC2  = 3;
    static constexpr uint8_t UPDATE_FC = 4;
    static constexpr uint8_t NOP       = 5;
    static constexpr uint8_t VENDOR    = 6;  // Vendor-specific（可选扩展）

    ch_uint<8>  kind;          // DLLP 类型（ACK/NAK/InitFC1/InitFC2/UpdateFC/NOP/Vendor）
    ch_uint<4>  vc_id;         // Virtual Channel ID（默认 0）
    ch_uint<16> credit_P;      // Posted token credit
    ch_uint<16> credit_NP;     // Non-Posted token credit
    ch_uint<16> credit_Cpl;    // Completion token credit
    ch_uint<16> seq_num;       // 发送序号（12-bit 有效值，wrap @4095）
    ch_uint<16> seq_num_ack;   // ACK/NAK 确认序号（12-bit 有效值）
    ch_uint<32> trans_id;      // 事务关联 ID

    PcieDllpBundle() = default;

    PcieDllpBundle(uint8_t k, uint8_t vc, uint16_t p, uint16_t np, uint16_t cpl,
                  uint16_t seq, uint16_t ack, uint32_t tid)
        : kind(k), vc_id(vc), credit_P(p), credit_NP(np), credit_Cpl(cpl),
          seq_num(seq), seq_num_ack(ack), trans_id(tid) {}

    // ========== 谓词：kind 判定 ==========
    bool is_ack() const { return kind.read() == ACK; }
    bool is_nak() const { return kind.read() == NAK; }
    bool is_fc() const {
        const uint8_t k = kind.read();
        return k == INIT_FC1 || k == INIT_FC2 || k == UPDATE_FC;
    }
    bool is_nop() const { return kind.read() == NOP; }
    bool is_vendor() const { return kind.read() == VENDOR; }
    bool is_init_fc() const {
        const uint8_t k = kind.read();
        return k == INIT_FC1 || k == INIT_FC2;
    }
    bool is_update_fc() const { return kind.read() == UPDATE_FC; }
};

/**
 * @brief PCIe PHY 配置（轻量级 TLM 侧，§1 PHY Digital Ctrl 消费）
 *
 * 字段（per design.md §6.2）：
 *   - max_speed      : 最大速率档位（GT/s 枚举 Gen1-5）
 *   - max_lanes      : 最大通道数（1/2/4/8/16）
 *   - preset_P       : Gen3+ equalization preset Posted
 *   - preset_NP      : Gen3+ equalization preset Non-Posted
 *   - preset_Cpl     : Gen3+ equalization preset Completion
 *   - sr_iov_vf_pool_size : SR-IOV VF pool 大小（0 = 未启用）
 *   - hot_plug_supported   : 热插拔支持标记
 */
struct PciePhyConfig : public bundle_base {
    // ========== 速率档位（GT/s per-lane）==========
    static constexpr uint8_t GEN1 = 0;  // 2 GT/s
    static constexpr uint8_t GEN2 = 1;  // 5 GT/s
    static constexpr uint8_t GEN3 = 2;  // 8 GT/s
    static constexpr uint8_t GEN4 = 3;  // 16 GT/s
    static constexpr uint8_t GEN5 = 4;  // 32 GT/s

    // 默认通道数 1/2/4/8/16
    static constexpr uint8_t LANES_1  = 1;
    static constexpr uint8_t LANES_2  = 2;
    static constexpr uint8_t LANES_4  = 4;
    static constexpr uint8_t LANES_8  = 8;
    static constexpr uint8_t LANES_16 = 16;

    ch_uint<8>  max_speed;             // 最大速率（GEN1..GEN5）
    ch_uint<8>  max_lanes;             // 最大通道数
    ch_uint<8>  preset_P;              // Gen3+ preset（Posted）
    ch_uint<8>  preset_NP;             // Gen3+ preset（Non-Posted）
    ch_uint<8>  preset_Cpl;            // Gen3+ preset（Completion）
    ch_uint<8>  sr_iov_vf_pool_size;   // SR-IOV VF pool 大小（0=禁用）
    ch_uint<8>  hot_plug_supported;    // 热插拔支持（0/1）

    PciePhyConfig() {
        // 默认：Gen5 16 通道，preset 7（per design §10.1 phy_digital 示例）
        max_speed.write(GEN5);
        max_lanes.write(LANES_16);
        preset_P.write(7);
        preset_NP.write(7);
        preset_Cpl.write(7);
        sr_iov_vf_pool_size.write(0);
        hot_plug_supported.write(0);
    }

    PciePhyConfig(uint8_t speed, uint8_t lanes, uint8_t pp, uint8_t pnp,
                  uint8_t pcpl, uint8_t vfs, bool hotplug)
        : max_speed(speed), max_lanes(lanes), preset_P(pp), preset_NP(pnp),
          preset_Cpl(pcpl), sr_iov_vf_pool_size(vfs),
          hot_plug_supported(hotplug ? 1 : 0) {}
};

} // namespace bundles

#endif // BUNDLES_PCIE_DLLP_BUNDLES_TLM_HH