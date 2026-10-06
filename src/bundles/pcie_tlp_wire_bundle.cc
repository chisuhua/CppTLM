// src/bundles/pcie_tlp_wire_bundle.cc
// P1: PcieTlpWireBundle implementations
// Per Oracle bg_ae8d4f14: to_descriptor/from_descriptor retained with
// [[deprecated]] attribute; byte-level methods bridge to PcieTlpCodec.
//
// This file holds method bodies that were inline in pcie_bundles_tlm.hh
// (lines 187-258 pre-migration). Including the old header here gives
// access to the full bundles::PcieTlpBundle definition for construction.
#include "bundles/pcie_tlp_wire_bundle.hh"
#include "bundles/pcie_bundles_tlm.hh"   // for full PcieTlpBundle definition

#include "tlm/pcie/pcie_tlp_codec.hh"     // P1 Task 7 bridge (stub until Task 7)

#include <cstring>

namespace cpptlm::pcie {

bundles::PcieTlpBundle PcieTlpWireBundle::to_descriptor() const {
    uint8_t desc_kind = kind;
    if (desc_kind == CPLD) desc_kind = bundles::PcieTlpBundle::MMIO_READ;

    uint32_t length_dw = (length == 0) ? 1024u : static_cast<uint32_t>(length);
    uint32_t size_bytes = length_dw * 4;

    uint64_t data_val = (static_cast<uint64_t>(payload[1]) << 32) | payload[0];

    return bundles::PcieTlpBundle(
        desc_kind,
        0,                              // bar_index (wire bundle doesn't carry)
        address_lo,
        size_bytes,
        data_val,
        requester_id,
        static_cast<uint32_t>(tag)
    );
}

PcieTlpWireBundle PcieTlpWireBundle::from_descriptor(const bundles::PcieTlpBundle& desc) {
    PcieTlpWireBundle wb;
    uint8_t dk = static_cast<uint8_t>(desc.kind.read());
    wb.kind = dk;

    // NOTE: Lossy fmt/type semantics per Oracle review (per pending fix in
    // follow-up change cpptlm-tlp-descriptor-fmt-fix).
    wb.fmt = (dk == bundles::PcieTlpBundle::CFG_READ || dk == bundles::PcieTlpBundle::CFG_WRITE)
             ? 0b100 : 0b010;
    wb.type = 0b00000;
    wb.tc = 0;
    wb.td = 0;
    wb.ep = 0;
    wb.attr = 0;

    uint32_t size_bytes = static_cast<uint32_t>(desc.size.read());
    uint32_t length_dw = (size_bytes + 3) / 4;
    wb.length = (length_dw >= 1024) ? 0 : static_cast<uint16_t>(length_dw);

    wb.requester_id = static_cast<uint16_t>(desc.requester_id.read());
    wb.tag = static_cast<uint8_t>(desc.trans_id.read() & 0xFF);
    wb.last_be = 0;
    wb.first_be = 0xF;
    wb.address_lo = static_cast<uint32_t>(desc.offset.read() & 0xFFFFFFFF);
    wb.address_hi = 0;

    uint64_t data_val = desc.data.read();
    wb.payload[0] = static_cast<uint32_t>(data_val & 0xFFFFFFFF);
    wb.payload[1] = static_cast<uint32_t>((data_val >> 32) & 0xFFFFFFFF);

    return wb;
}

// P1 Task 7 stub: full implementations bridge to PcieTlpCodec in Task 7 commit.
void PcieTlpWireBundle::tlp_pack_to_bytes(uint8_t out[4096], unsigned& out_len) const noexcept {
    out_len = 0;
    (void)out;
    // TODO(P1-Task-7): bridge to tlm::PcieTlpCodec::encode_*
}

void PcieTlpWireBundle::tlp_unpack_from_bytes(const uint8_t in[4096], unsigned in_len) noexcept {
    (void)in; (void)in_len;
    // TODO(P1-Task-7): bridge to tlm::PcieTlpCodec::decode
}

uint32_t PcieTlpWireBundle::crc32_compute() const noexcept {
    // TODO(P1-Task-7): bridge to tlm::PcieTlpCodec::crc32_pcie
    return 0;
}

} // namespace cpptlm::pcie