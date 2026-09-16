// src/tlm/pcie/pcie_requester_engine.cc
// PcieRequesterEngine 实现 (T-P11-1)
// 强制 include "tlm/pcie/pcie_tlp_codec.hh" — MRd 必须走 encode_mrd API
// 作者 CppTLM Team / 日期 2027-02-09
#include "tlm/pcie/pcie_requester_engine.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_tlp_codec.hh"

namespace cpptlm::pcie {

// ==================== 构造 ====================

PcieRequesterEngine::PcieRequesterEngine(tlm::pcie::PcieLinkLayer* ll,
                                          uint64_t timeout_ns)
    : link_layer_(ll), default_timeout_ns_(timeout_ns) {
}

// ==================== tag 分配 (12-bit, 1-4095, 0 保留) ====================

uint16_t PcieRequesterEngine::allocate_tag() {
    for (int i = 1; i <= 4095; ++i) {
        uint16_t candidate = static_cast<uint16_t>(((last_tag_ + i) % 4095) + 1);
        if (outstanding_.find(candidate) == outstanding_.end()) {
            return candidate;
        }
    }
    return 0;
}

// ==================== MRd 发起 ====================

bool PcieRequesterEngine::mrd_read(uint16_t bdf, uint64_t addr,
                                    uint32_t /*lower_addr*/, std::size_t len) {
    uint16_t tag = allocate_tag();
    if (tag == 0) {
        return false;
    }

    uint32_t addr_lo = static_cast<uint32_t>(addr & 0xFFFFFFFF);
    std::size_t len_dw = (len + 3) / 4;
    PcieTlpCodec::encode_mrd(bdf, static_cast<uint8_t>(tag & 0xFF), addr_lo, len_dw);

    bundles::PcieTlpBundle tlp;
    tlp.kind.write(bundles::PcieTlpBundle::MEM_READ);
    tlp.requester_id.write(bdf);
    tlp.trans_id.write(tag);
    tlp.offset.write(addr);
    tlp.size.write(static_cast<uint32_t>(len));
    tlp.data.write(0);
    tlp.bar_index.write(0);

    if (!link_layer_->tx_tlp(tlp, bdf)) {
        return false;
    }

    last_tag_ = tag;
    outstanding_[tag] = Outstanding{tag, bdf, addr,
                                     current_time_ns_, default_timeout_ns_};
    return true;
}

// ==================== CplD 接收 ====================

void PcieRequesterEngine::on_cpld_received(const bundles::PcieTlpBundle& cpld) {
    uint16_t tag = static_cast<uint16_t>(cpld.trans_id.read() & 0xFFFF);
    auto it = outstanding_.find(tag);
    if (it == outstanding_.end()) {
        return;
    }
    complete_outstanding(tag, cpld);
}

// ==================== tick ====================

void PcieRequesterEngine::tick(uint64_t elapsed_ns) {
    current_time_ns_ += elapsed_ns;

    std::vector<uint16_t> timed_out;
    for (const auto& [tag, out] : outstanding_) {
        if (current_time_ns_ - out.issued_at_ns >= out.timeout_ns) {
            timed_out.push_back(tag);
        }
    }
    for (auto tag : timed_out) {
        timeout_outstanding(tag);
    }
}

// ==================== 内部函数 ====================

void PcieRequesterEngine::complete_outstanding(
    uint16_t tag, const bundles::PcieTlpBundle& cpld) {
    outstanding_.erase(tag);
    if (completion_cb_) {
        completion_cb_(tag, cpld);
    }
}

void PcieRequesterEngine::timeout_outstanding(uint16_t tag) {
    outstanding_.erase(tag);
    if (error_cb_) {
        error_cb_(tag, "RequesterEngine completion timeout");
    }
}

} // namespace cpptlm::pcie