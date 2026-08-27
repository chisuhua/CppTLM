// include/bundles/pcie_bundles_tlm.hh
// PCIe Bundle 定义（轻量级 TLM 侧，PcieTlpBundle + MsiXDeliveryBundle）
// 功能描述：定义 dGPU SOC PCIe Endpoint IP 模型使用的事务 Bundle
//           - PcieTlpBundle：host→endpoint 统一事务（CFG/BAR0 MMIO/BAR1 MEM）
//           - MsiXDeliveryBundle：endpoint→host 中断投递（MSI-X）
//           该文件为 TLM 侧 Bundle，被 PcieEndpointTLM 与 StreamAdapter 使用。
// 作者 CppTLM Team / 日期 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §2
//       ADR-SOC-07 D2 (PCIe slave 归属 SOC)
#ifndef BUNDLES_PCIE_BUNDLES_TLM_HH
#define BUNDLES_PCIE_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief PCIe TLP Bundle（轻量级 TLM 侧）
 *
 * 字段（per design.md §2）：
 *   - kind           : 事务类型（CFG_READ/CFG_WRITE/MMIO_READ/MMIO_WRITE/MEM_READ/MEM_WRITE）
 *   - bar_index      : MMIO/MEM 时有效（0=BAR0 寄存器 / 1=BAR1 VRAM aperture）
 *   - offset         : config offset 或 BAR 内偏移
 *   - size           : 访问字节数（1/2/4/8，MEM 块可任意不超过 BAR1 aperture；
 *                      全文档冻结：size 字段语义为字节数，非拍数、非 dword 数）
 *   - data           : 写数据首 8 字节 / 读回数据（≤8B inline；size > 8 走 backdoor 路径，
 *                      PcieTlpBundle 仅携带 descriptor-only TLP，data=0）
 *   - requester_id   : PCIe Requester ID（bus/dev/fn），诊断用
 *   - trans_id       : 事务关联 ID
 *
 * 设计原则（per design.md §2）：
 *   - 轻量级：仅 POD 字段，无 CppHDL AST 依赖
 *   - C++17 兼容：可在 cpptlm_core（C++17 静态库）中使用
 *   - POD 惯例：不携带 shared_ptr/inline buffer 等堆指针（per ADR-SOC-07 Status Update Q3）
 *
 * 字段宽度：
 *   kind: 8 bits（足够 6 种 Kind + 保留）
 *   bar_index: 8 bits（PCIe spec 最多 6 BAR，但预留 256）
 *   offset: 64 bits（BAR 内偏移或 config 偏移）
 *   size: 32 bits（BAR1 aperture 256MB 够用）
 *   data: 64 bits（≤8B inline）
 *   requester_id: 16 bits（PCIe spec 16-bit Requester ID）
 *   trans_id: 32 bits（事务关联）
 */
struct PcieTlpBundle : public bundle_base {
    // ========== Kind 常量（PCIe 事务类型）==========
    static constexpr uint8_t CFG_READ   = 0;
    static constexpr uint8_t CFG_WRITE  = 1;
    static constexpr uint8_t MMIO_READ = 2;
    static constexpr uint8_t MMIO_WRITE = 3;
    static constexpr uint8_t MEM_READ  = 4;
    static constexpr uint8_t MEM_WRITE = 5;
    // 特殊 kind: 用于 irq_out 端口传输 MsiXDeliveryBundle 语义
    // (per spec.md scenario "MSI-X delivery path")
    static constexpr uint8_t IRQ_DELIVERY = 6;

    ch_uint<8>  kind;             // 事务类型（CFG_READ..MEM_WRITE + IRQ_DELIVERY）
    ch_uint<8>  bar_index;        // MMIO/MEM 时有效
    ch_uint<64> offset;           // config offset 或 BAR 内偏移
    ch_uint<32> size;             // 访问字节数
    ch_uint<64> data;             // 写/读数据（≤8B inline；>8B 块访问 data=0）
    ch_uint<16> requester_id;     // PCIe Requester ID（bus/dev/fn）
    ch_uint<32> trans_id;         // 事务关联 ID

    PcieTlpBundle() = default;

    PcieTlpBundle(uint8_t k, uint8_t bar, uint64_t off, uint32_t sz,
                  uint64_t d, uint16_t rid, uint32_t tid)
        : kind(k), bar_index(bar), offset(off), size(sz),
          data(d), requester_id(rid), trans_id(tid) {}

    // 谓词：是否为 IRQ 投递（irq_out 端口传输 MsiXDelivery 语义时为 true）
    bool is_irq_delivery() const {
        return kind.read() == IRQ_DELIVERY;
    }

    // 谓词：是否为读事务
    bool is_read() const {
        const uint8_t k = kind.read();
        return k == CFG_READ || k == MMIO_READ || k == MEM_READ;
    }

    // 谓词：是否为写事务
    bool is_write() const {
        const uint8_t k = kind.read();
        return k == CFG_WRITE || k == MMIO_WRITE || k == MEM_WRITE;
    }

    // 谓词：是否为 MEM 块访问（用于 BAR1 大块 backdoor 路径判定）
    bool is_bulk_mem() const {
        const uint8_t k = kind.read();
        return (k == MEM_READ || k == MEM_WRITE) && size.read() > 8;
    }
};

/**
 * @brief MSI-X 投递 Bundle（轻量级 TLM 侧）
 *
 * 字段（per design.md §2）：
 *   - vector   : MSI-X vector index (0..num_vectors-1)
 *   - msg_data : MSI-X message data（per PCI-SIG spec）
 *   - msg_addr : MSI-X table 中的地址（diagnostic 用，不参与路由）
 *   - trans_id : 事务关联 ID（与触发 pending 的事务对齐）
 *
 * 设计原则：
 *   - 独立 bundle 类型，与 PcieTlpBundle 严格分离（per spec.md 范围冻结）
 *   - POD 字段，无堆指针
 *
 * 范围冻结：本 change 交付的 pcie_bundles_tlm.hh 仅含 PcieTlpBundle 与 MsiXDeliveryBundle 两类型。
 * DMA descriptor 类（如 DmaDescriptor）由配套的 cpptlm-dgpu-sdma-engine change 在独立文件
 * include/bundles/dma_bundles_tlm.hh 中定义，本 change 不预留字段（避免超前设计）。
 */
struct MsiXDeliveryBundle : public bundle_base {
    ch_uint<16> vector;       // MSI-X vector index (0..num_vectors-1)
    ch_uint<32> msg_data;     // MSI-X message data
    ch_uint<64> msg_addr;     // MSI-X table address (diagnostic)
    ch_uint<32> trans_id;     // 事务关联 ID

    MsiXDeliveryBundle() = default;

    MsiXDeliveryBundle(uint16_t vec, uint32_t mdata, uint64_t maddr, uint32_t tid)
        : vector(vec), msg_data(mdata), msg_addr(maddr), trans_id(tid) {}
};

} // namespace bundles

#endif // BUNDLES_PCIE_BUNDLES_TLM_HH