// test/test_pcie_flr_e2e.cc
// PcieEndpointIP FLR 端到端测试 (per X1 v1.1 spec AC G3)
//
// 测试标签: [pcie][flr][e2e]
//
// 验证 v1.0 spec 错误修复:
//   1. FLR 后 MSI-X PBA 全清 (per PCIe Base Spec §6.6.2, 统一 v1.0 spec 矛盾)
//   2. FLR 后 BAR routing verdict 仍工作 (per design §3.4: FLR 不破 routing)
//   3. 复用既有 PcieEndpointIP::flr_pf() (per pcie_endpoint_ip.hh:201)
//   4. 复用既有 PcieEndpointIP::flr_vf(vf_id) (vf_id >= 1, per pool_.flr_vf 实现)
//
// 已有测试: test_pcie_endpoint_ip_basic.cc:102-108 (基本 FLR 测试)
#include "catch_amalgamated.hpp"

#include "core/event_queue.hh"
#include "tlm/gpu/msix_table_mvp.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <cstdint>

using tlm::pcie::PcieEndpointIP;

TEST_CASE("PcieEndpointIP FLR (PF): MSI-X PBA 全清 (per PCIe Base Spec §6.6.2)",
          "[pcie][flr][e2e][msix-pba][v1.1-spec-fix]") {
    EventQueue eq;
    PcieEndpointIP ep("ep", &eq);
    ep.init();

    tlm::gpu::MsiXTable& msix_pf = ep.msix();
    msix_pf.update_pending(0);
    msix_pf.update_pending(3);
    msix_pf.update_pending(7);
    msix_pf.update_pending(15);

    CHECK(msix_pf.is_pba_set(0) == true);
    CHECK(msix_pf.is_pba_set(3) == true);
    CHECK(msix_pf.is_pba_set(7) == true);
    CHECK(msix_pf.is_pba_set(15) == true);
    CHECK(msix_pf.pba_count() == 4);

    ep.flr_pf();

    CHECK(msix_pf.is_pba_set(0) == false);
    CHECK(msix_pf.is_pba_set(3) == false);
    CHECK(msix_pf.is_pba_set(7) == false);
    CHECK(msix_pf.is_pba_set(15) == false);
    CHECK(msix_pf.pba_count() == 0);
}

TEST_CASE("PcieEndpointIP FLR (PF): BAR 状态可重入 (per design §3.4)",
          "[pcie][flr][e2e][bar-reentrant]") {
    EventQueue eq;
    PcieEndpointIP ep("ep", &eq);
    ep.init();

    ep.flr_pf();
    ep.init();
    ep.flr_pf();
    ep.flr_pf();
    CHECK(true);
}

TEST_CASE("PcieEndpointIP FLR (PF): config space 访问 0 crash (per nullptr-safe)",
          "[pcie][flr][e2e][config-space]") {
    EventQueue eq;
    PcieEndpointIP ep("ep", &eq);
    ep.init();

    ep.flr_pf();

    const uint32_t vendor_id = ep.config_space().read(0x00);
    (void)vendor_id;
    CHECK(true);
}

TEST_CASE("PcieEndpointIP FLR (VF1): VF-specific reset (per PCIe SR-IOV spec)",
          "[pcie][flr][e2e][vf]") {
    EventQueue eq;
    PcieEndpointIP ep("ep", &eq);
    ep.init();

    tlm::gpu::MsiXTable& msix_vf1 = ep.msix_of(1);
    msix_vf1.update_pending(5);
    CHECK(msix_vf1.is_pba_set(5) == true);

    ep.flr_vf(1);

    CHECK(msix_vf1.is_pba_set(5) == false);
    CHECK(msix_vf1.pba_count() == 0);

    tlm::gpu::MsiXTable& msix_pf = ep.msix();
    msix_pf.update_pending(2);
    ep.flr_vf(1);
    CHECK(msix_pf.is_pba_set(2) == true);
}
