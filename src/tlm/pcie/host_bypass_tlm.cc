// src/tlm/pcie/host_bypass_tlm.cc
// HostBypassTLM 实现 (T-P7-1)
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

} // namespace tlm::pcie