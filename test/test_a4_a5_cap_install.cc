// test/test_a4_a5_cap_install.cc
// A-4 + A-5 Path A: PCIe Cap (0x10) + LNKCTL@0x60 + ACS Ext Cap (0x0D) + ReBAR Ext Cap (0x0015) 安装
//
// 参考: openspec/changes/2026-09-15-cpptlm-stage-1-4-2-1-ue-extensions-unblock/v1.2 spec.md
//        Scenarios 5/6/7/8/9/10/11
#include <fstream>
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using namespace tlm::pcie;
using json = nlohmann::json;

static json dgpu_mini_cfg() {
    return json::parse(R"({
        "name": "a4_a5_test",
        "params": {"device_id": "0x1234", "quantum_cycles": 1000,
                   "ptx_emu_root": "/tmp/test-ptx-emu"},
        "modules": [{
            "name": "soc", "type": "DGpuSoc",
            "modules": [{
                "name": "pcie_ep", "type": "PcieEndpointIP",
                "params": {
                    "config_size": 4096, "msix_num_vectors": 8,
                    "bar_sizes": [65536, 268435456],
                    "link_layer": {"enabled": true}
                }
            }],
            "connections": []
        }],
        "connections": []
    })");
}

TEST_CASE("A-4: PCIe Cap (0x10) installed at offset 0x50", "[a4][pcie][unblock]") {
    DGpuBoard board("a4_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);

    const uint32_t val = ep->config_space().read(0x50);
    REQUIRE((val & 0xFFu) == 0x10u);  // PCIe Cap id
    board.shutdown();
}

TEST_CASE("A-4: LNKCTL register at offset 0x60 (cap+0x10)", "[a4][pcie][unblock]") {
    DGpuBoard board("a4_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);

    const uint32_t lnkctl_default = ep->config_space().read(0x60);
    REQUIRE((lnkctl_default & 0xFFFFu) == 0x0010u);  // LNKCTL bit 4 = Link Active
    board.shutdown();
}

TEST_CASE("A-4: LNKCTL write 0x0002 enables ASPM L1 (phy enable_aspm called)", "[a4][pcie][unblock]") {
    DGpuBoard board("a4_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    REQUIRE(ep->phy() != nullptr);  // link_layer enabled

    // LNKCTL write ASPM L1 (bit1)
    board.pcie_config_write(0x60, 2, 0x0002);
    REQUIRE(ep->phy()->aspm_level() == PciePhyDigitalCtrl::AspmLevel::L1);

    // LNKCTL write ASPM L0s (bit0)
    board.pcie_config_write(0x60, 2, 0x0001);
    REQUIRE(ep->phy()->aspm_level() == PciePhyDigitalCtrl::AspmLevel::L0s);
    board.shutdown();
}

TEST_CASE("A-5: ACS Ext Cap (0x000D) installed at offset 0x100", "[a5][pcie][unblock]") {
    DGpuBoard board("a5_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);

    const uint32_t val = ep->config_space().read(0x100);
    REQUIRE((val & 0xFFFFu) == 0x000Du);  // ACS Ext Cap id
    board.shutdown();
}

TEST_CASE("A-5: ReBAR Ext Cap (0x0015, per PCI-SIG) installed at offset 0x140", "[a5][pcie][unblock]") {
    DGpuBoard board("a5_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);

    const uint32_t header = ep->config_space().read(0x140);
    REQUIRE((header & 0xFFFFu) == 0x0015u);  // ReBAR Ext Cap id
    REQUIRE(((header >> 16) & 0x0Fu) == 0x1u);  // version = 1
    board.shutdown();
}

TEST_CASE("A-5: ReBAR Control @0x148 encodes BAR0 size=256MB (bits[12:8]==8)", "[a5][pcie][unblock]") {
    DGpuBoard board("a5_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);

    const uint32_t control = ep->config_space().read(0x148);
    REQUIRE(((control >> 8) & 0x1Fu) == 0x8u);  // BAR Size = 8 → 256MB (per log2(bytes)-20)
    REQUIRE(((control >> 4) & 0x0Fu) == 0x1u);  // Number of Resizable BARs = 1
    board.shutdown();
}