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
#include <array>
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

    // CPLD (Completion with Data) 常量, T-P10-1 新增
    // fc_type_for_kind 修复: CPLD → Completion credit bucket（而非误消耗 Posted）
    static constexpr uint8_t CPLD = 7;

    ch_uint<8>  kind;             // 事务类型（CFG_READ..MEM_WRITE + IRQ_DELIVERY + CPLD）
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

// ===== PcieTlpWireBundle (Phase 9+ T-P9-2) =====
// Wire-format TLP bundle with 4KB payload array (std::array<uint32_t, 1024>)
//
// 用于: (a) golden 快照测试, (b) CRC 校验, (c) 错误注入点位
// **不** 用于替换既有 PcieTlpBundle descriptor, LL 接口签名 const PcieTlpBundle& 不变
//
// 设计决策:
// - 使用 std::array<uint32_t, 1024> 绕开 ch_uint<512> = 64-bit 限制
//   (per AGENTS.md KEY INVARIANTS: "ch_uint<512> 内部 uint64_t")
// - CPLD=7 kind constant 为 T-P10-1 fc_type_for_kind 修复预留
// - 置于 cpptlm::pcie 命名空间以对齐 PcieTlpCodec (wire-format 编解码)
namespace cpptlm::pcie {

struct PcieTlpWireBundle {
    // ========== Kind 常量 (PCIe 事务类型, 与 bundles::PcieTlpBundle 0-6 对齐) ==========
    static constexpr uint8_t CFG_READ   = 0;
    static constexpr uint8_t CFG_WRITE  = 1;
    static constexpr uint8_t MMIO_READ  = 2;
    static constexpr uint8_t MMIO_WRITE = 3;
    static constexpr uint8_t MEM_READ   = 4;
    static constexpr uint8_t MEM_WRITE  = 5;
    static constexpr uint8_t IRQ_DELIVERY = 6;
    static constexpr uint8_t CPLD       = 7;  // 新增 (T-P10-1 fc_type 修复需要)

    // ========== Wire-format header 字段 (PCIe Base Spec §2.2) ==========
    uint8_t  fmt;              // 3 bits, 0=3DW/no-data 1=4DW/no-data 2=3DW/data 3=4DW/data
    uint8_t  type;             // 5 bits
    uint8_t  tc;               // 3 bits, Traffic Class
    uint8_t  td;               // TLP Digest (ECRC present)
    uint8_t  ep;               // Poisoned
    uint8_t  attr;             // 3 bits, Attr[2:0]
    uint16_t length;           // 10 bits, in DW (0 = 1024 DW)
    uint16_t requester_id;     // BDF of requester
    uint8_t  tag;              // 8 bits
    uint8_t  last_be;          // 4 bits, Last DW Byte Enables
    uint8_t  first_be;         // 4 bits, First DW Byte Enables
    uint32_t address_lo;       // 32-bit address low (3DW header)
    uint32_t address_hi;       // 32-bit address high (4DW header, 0 for 3DW)
    // Completion-only fields (CplD)
    uint16_t completer_id;     // CplD only
    uint8_t  status;           // CplD only, 3 bits
    uint16_t byte_count;       // CplD only, 12 bits
    uint8_t  lower_address;    // CplD only, 8 bits

    // ========== Payload 数组 ==========
    // 1024 DW = 4096 字节 (PCIe TLP Length 10-bit 上限, Length=0 = 1024 DW)
    // 绕开 ch_uint<512> = 64-bit 限制 (per AGENTS.md KEY INVARIANTS)
    static constexpr std::size_t MAX_PAYLOAD_DW = 1024;
    std::array<uint32_t, MAX_PAYLOAD_DW> payload{};

    // ========== ECRC / LCRC ==========
    uint32_t ecrc = 0;         // ECRC (TD=1 时有效)
    uint32_t lcrc = 0;         // LCRC (末尾 4 字节)

    // ========== Kind 字段 (与 bundles::PcieTlpBundle 7 种对齐 + CPLD) ==========
    uint8_t kind = MMIO_READ;

    // ========== 默认构造函数 ==========
    PcieTlpWireBundle() = default;

    // ========== 互转 helpers (Wire ↔ Descriptor) ==========
    // 转换为既有 PcieTlpBundle descriptor (轻量级, 仅携带首 8 字节 data)
    bundles::PcieTlpBundle to_descriptor() const {
        // 确定 kind 映射
        uint8_t desc_kind = kind;
        // CPLD 在 PcieTlpBundle 中无对应 → 映射到 MMIO_READ
        if (desc_kind == CPLD) desc_kind = bundles::PcieTlpBundle::MMIO_READ;

        // size: length DW → bytes (length=0 means 1024 DW)
        uint32_t length_dw = (length == 0) ? 1024u : static_cast<uint32_t>(length);
        uint32_t size_bytes = length_dw * 4;

        // data: 首 8 字节 payload (pairs of uint32 → uint64)
        uint64_t data_val = (static_cast<uint64_t>(payload[1]) << 32)
                            | payload[0];

        return bundles::PcieTlpBundle(
            desc_kind,                // kind
            0,                         // bar_index (wire format 不携带)
            address_lo,               // offset
            size_bytes,               // size (bytes)
            data_val,                 // data (首 8 字节)
            requester_id,             // requester_id
            static_cast<uint32_t>(tag) // trans_id
        );
    }

    // 从既有 PcieTlpBundle descriptor 转换回 wire bundle
    static PcieTlpWireBundle from_descriptor(const bundles::PcieTlpBundle& desc) {
        PcieTlpWireBundle wb;
        uint8_t dk = static_cast<uint8_t>(desc.kind.read());

        // 映射 kind: PcieTlpBundle (0-6) → PcieTlpWireBundle (0-6, CPLD=7)
        wb.kind = dk;

        // Header 字段 (默认值, 仅填充 descriptor 可提供的)
        wb.fmt = (dk == bundles::PcieTlpBundle::CFG_READ || dk == bundles::PcieTlpBundle::CFG_WRITE)
                 ? 0b100 : 0b010;  // 4DW cfg or 3DW mem
        wb.type = 0b00000;          // Memory (default)
        wb.tc = 0;
        wb.td = 0;
        wb.ep = 0;
        wb.attr = 0;

        // length: size(bytes) → DW, 向上取整
        uint32_t size_bytes = static_cast<uint32_t>(desc.size.read());
        uint32_t length_dw = (size_bytes + 3) / 4;
        wb.length = (length_dw >= 1024) ? 0 : static_cast<uint16_t>(length_dw);

        wb.requester_id = static_cast<uint16_t>(desc.requester_id.read());
        wb.tag = static_cast<uint8_t>(desc.trans_id.read() & 0xFF);
        wb.last_be = 0;
        wb.first_be = 0xF;
        wb.address_lo = static_cast<uint32_t>(desc.offset.read() & 0xFFFFFFFF);
        wb.address_hi = 0;

        // payload: 从 desc.data 恢复首 8 字节
        uint64_t data_val = desc.data.read();
        wb.payload[0] = static_cast<uint32_t>(data_val & 0xFFFFFFFF);
        wb.payload[1] = static_cast<uint32_t>((data_val >> 32) & 0xFFFFFFFF);

        return wb;
    }
};

} // namespace cpptlm::pcie

#endif // BUNDLES_PCIE_BUNDLES_TLM_HH