// src/tlm/pcie/host_bypass_tlm.cc
// HostBypassTLM 实现 (T-P7-1 + T-P7-2)
// 作者 CppTLM Team / 日期 2027-01-19
#include "tlm/pcie/host_bypass_tlm.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

namespace tlm::pcie {

HostBypassTLM::HostBypassTLM(const std::string& name, EventQueue* eq)
    : name_(name), eq_(eq), axi_() {}

void HostBypassTLM::init() {
    axi_.reset();
}

void HostBypassTLM::attach_to_endpoint(PcieEndpointIP* ep) noexcept {
    ep_ = ep;
}

void HostBypassTLM::detach() noexcept {
    ep_ = nullptr;
}

// ===================== 软件 bring-up API (T-P7-2) =====================

bool HostBypassTLM::config_write(uint16_t offset, uint32_t value, uint16_t stream_id) {
    if (!ep_ || stream_id >= PcieEndpointIP::NUM_PORTS) {
        return false;
    }
    // 经 EP 配置空间单一真源写入（PcieConfigSpace::write 处理对齐/越界/只读保护）
    ep_->vf_pool().config_of(stream_id).write(offset, value);
    return true;
}

uint32_t HostBypassTLM::config_read(uint16_t offset, uint16_t stream_id) {
    if (!ep_ || stream_id >= PcieEndpointIP::NUM_PORTS) {
        return 0xFFFFFFFFu;
    }
    return ep_->vf_pool().config_of(stream_id).read(offset);
}

uint8_t HostBypassTLM::bytes_to_awsize(uint8_t bytes) {
    // bytes ∈ {1,2,4,8} → awsize = log2(bytes)
    uint8_t sz = 0;
    while ((1u << sz) < bytes) {
        ++sz;
    }
    return sz;
}

bool HostBypassTLM::bar_write(uint64_t addr, uint64_t data, uint8_t bytes) {
    if (!ep_) {
        return false;
    }
    if (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8) {
        return false;
    }
    const uint16_t id = static_cast<uint16_t>((bar_tx_id_++ & 0xFFFF) + 1);

    bundles::Axi4Bundle req;
    req.awid.write(id);
    req.awaddr.write(addr);
    req.awlen.write(0);                       // 单拍
    req.awsize.write(bytes_to_awsize(bytes)); // 2^awsize 字节/拍
    req.awburst.write(1);                     // INCR
    req.wdata.write(data);
    req.wstrb.write(static_cast<uint64_t>(0xFFu << 0)); // 全 strobe（8 字节内）
    req.wlast.write(1);

    return axi_.master_req(req);
}

bool HostBypassTLM::bar_read(uint64_t addr, uint64_t& data, uint8_t bytes) {
    if (!ep_) {
        return false;
    }
    if (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8) {
        return false;
    }
    const uint16_t id = static_cast<uint16_t>((bar_tx_id_++ & 0xFFFF) + 1);

    bundles::Axi4Bundle req;
    req.arid.write(id);
    req.araddr.write(addr);
    req.arlen.write(0);
    req.arsize.write(bytes_to_awsize(bytes));
    req.arburst.write(1);

    data = 0;  // 输出初始化（读数据经 axi_master_resp 呈现，此处仅为占位）
    return axi_.master_req(req);
}

} // namespace tlm::pcie