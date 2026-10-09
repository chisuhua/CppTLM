// include/utils/pcie_tlp_serializer.hh
// PcieTlpSerializer: TLP packet 序列化/反序列化 utility
//
// Per cpptlm-pcie-host-test-flr-rebar v1.1 design.md §3.5:
//   - 独立 utility 类, 复用 include/bundles/pcie_bundles_tlm.hh::PcieTlpBundle (frozen per AC G8)
//   - 单职责: TLP packet 编码/解码
//   - 0 触碰既有 PcieEndpointIP/HostPcieTestStub 状态
//
// 协议层 TLP 类型覆盖 (per AC G2):
//   - Config Read/Write (Type 0000/0001)  ─ 经 23 ABI cpptlm_emulator_config_read/write 入口
//   - Memory Read/Write (Type 0000/0001 with 64-bit addr)  ─ 经 23 ABI cpptlm_emulator_mmio_read/write 入口
//   - Completion (Type 1010)  ─ VRAM → SDMA response
//
// 物理对应 (per Oracle RTL 调研): Synopsys DW PCIe Controller TLP bridge 接口
//
// 作者: CppTLM Team / 日期: 2026-10-09 (X1 v1.1)
#ifndef CPPTLM_UTILS_PCIE_TLP_SERIALIZER_HH
#define CPPTLM_UTILS_PCIE_TLP_SERIALIZER_HH

#include "bundles/pcie_bundles_tlm.hh"  // PcieTlpBundle (frozen)
#include <cstdint>
#include <vector>

namespace cpptlm::utils {

// PcieTlpSerializer: TLP packet 序列化/反序列化 utility
// 设计原则:
//   1. 单职责: 仅做 PcieTlpBundle 字段填充 + 提取
//   2. 复用既有 PcieTlpBundle (8 字段, frozen per AC G8)
//   3. 0 引入 23 ABI 头改动 (per CRITICAL C2 v1.1)
//   4. 0 触碰 host-side 真实 TLP 协议栈 (per Non-Goal #4 v1.1)
class PcieTlpSerializer {
public:
    PcieTlpSerializer() = default;
    ~PcieTlpSerializer() = default;

    // 禁止拷贝
    PcieTlpSerializer(const PcieTlpSerializer&) = delete;
    PcieTlpSerializer& operator=(const PcieTlpSerializer&) = delete;

    // ── TLP 编码 (host → device) ──

    // Config Read TLP (PCIe Type 0000, Format 00)
    // 输出: PcieTlpBundle 含 kind=CFG_READ, offset=寄存器 offset, size=read width
    bundles::PcieTlpBundle encode_config_read(uint16_t bdf, uint16_t offset, uint8_t width) {
        bundles::PcieTlpBundle tlp{};
        tlp.kind.write(bundles::PcieTlpBundle::CFG_READ);
        tlp.bar_index.write(0);  // CFG 不用 BAR
        tlp.offset.write(static_cast<uint64_t>(offset));
        tlp.size.write(static_cast<uint32_t>(width));
        tlp.data.write(0);
        tlp.requester_id.write(bdf);
        tlp.trans_id.write(allocate_trans_id());
        return tlp;
    }

    // Config Write TLP (PCIe Type 0001, Format 00)
    bundles::PcieTlpBundle encode_config_write(uint16_t bdf, uint16_t offset,
                                              uint8_t width, uint32_t value) {
        bundles::PcieTlpBundle tlp{};
        tlp.kind.write(bundles::PcieTlpBundle::CFG_WRITE);
        tlp.bar_index.write(0);
        tlp.offset.write(static_cast<uint64_t>(offset));
        tlp.size.write(static_cast<uint32_t>(width));
        tlp.data.write(static_cast<uint64_t>(value));
        tlp.requester_id.write(bdf);
        tlp.trans_id.write(allocate_trans_id());
        return tlp;
    }

    // MMIO Read TLP (BAR 内 offset 读)
    bundles::PcieTlpBundle encode_mmio_read(uint8_t bar, uint64_t offset, uint32_t size) {
        bundles::PcieTlpBundle tlp{};
        tlp.kind.write(bundles::PcieTlpBundle::MMIO_READ);
        tlp.bar_index.write(bar);
        tlp.offset.write(offset);
        tlp.size.write(size);
        tlp.data.write(0);
        tlp.requester_id.write(0);
        tlp.trans_id.write(allocate_trans_id());
        return tlp;
    }

    // MMIO Write TLP (BAR 内 offset 写, ≤ 8 字节 inline data)
    bundles::PcieTlpBundle encode_mmio_write(uint8_t bar, uint64_t offset, uint32_t size,
                                             uint64_t data) {
        bundles::PcieTlpBundle tlp{};
        tlp.kind.write(bundles::PcieTlpBundle::MMIO_WRITE);
        tlp.bar_index.write(bar);
        tlp.offset.write(offset);
        tlp.size.write(size);
        tlp.data.write(data);
        tlp.requester_id.write(0);
        tlp.trans_id.write(allocate_trans_id());
        return tlp;
    }

    // Completion TLP (CplD, 含 data payload)
    bundles::PcieTlpBundle encode_completion(uint16_t requester_id, uint64_t data, bool success) {
        bundles::PcieTlpBundle tlp{};
        tlp.kind.write(bundles::PcieTlpBundle::CPLD);
        tlp.bar_index.write(0);
        tlp.offset.write(0);
        tlp.size.write(sizeof(uint64_t));
        tlp.data.write(data);
        tlp.requester_id.write(requester_id);
        tlp.trans_id.write(0);  // CPLD 用 requester_id 关联
        (void)success;  // CPLD 通过 is_ok 字段表达, 此处不重复
        return tlp;
    }

    // ── TLP 解码 (device → host) ──
    struct DecodedTlp {
        uint8_t  type;        // PcieTlpBundle::kind 常量
        uint16_t bdf;          // requester_id
        uint64_t address;     // offset (CFG/MMIO) or 0 (CPLD)
        uint32_t byte_count;  // size 字段
        uint64_t data;        // data 字段
    };

    DecodedTlp decode(const bundles::PcieTlpBundle& tlp) const noexcept {
        DecodedTlp d{};
        d.type = tlp.kind.read();
        d.bdf = tlp.requester_id.read();
        d.address = tlp.offset.read();
        d.byte_count = tlp.size.read();
        d.data = tlp.data.read();
        return d;
    }

    // ── 统计 ──
    uint32_t alloc_trans_id_count() const noexcept { return next_trans_id_; }

private:
    // 简单单调 trans_id 分配器 (test fixture 用, 无真实 PCIe 协议语义)
    uint32_t allocate_trans_id() noexcept { return next_trans_id_++; }

    uint32_t next_trans_id_ = 0;
};

} // namespace cpptlm::utils

#endif // CPPTLM_UTILS_PCIE_TLP_SERIALIZER_HH
