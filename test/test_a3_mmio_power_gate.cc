// test/test_a3_mmio_power_gate.cc
// A-3: DGpuBoard::is_mmio_gated() + mmio_read/write D3 -EIO 行为
//
// 参考: openspec/changes/2026-09-15-cpptlm-stage-1-4-2-1-ue-extensions-unblock/v1.2 design.md §A-3
//        spec.md Scenario "MMIO read in D3hot returns -EIO"
#include <fstream>
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

static json dgpu_mini_cfg() {
    return json::parse(R"({
        "name": "a3_test",
        "params": {"device_id": "0x1234", "quantum_cycles": 1000,
                   "ptx_emu_root": "/tmp/test-ptx-emu"},
        "modules": [{
            "name": "soc", "type": "DGpuSoc",
            "modules": [{
                "name": "pcie_ep", "type": "PcieEndpointIP",
                "params": {
                    "config_size": 4096, "msix_num_vectors": 8,
                    "bar_sizes": [65536, 268435456]
                }
            }],
            "connections": []
        }],
        "connections": []
    })");
}

TEST_CASE("A-3: is_mmio_gated defaults false (D0)", "[a3][pcie][unblock]") {
    DGpuBoard board("a3_d0_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    REQUIRE_FALSE(board.is_mmio_gated());
    board.shutdown();
}

TEST_CASE("A-3: mmio_read in D0 returns success (no gate)", "[a3][pcie][unblock]") {
    DGpuBoard board("a3_d0_mmio_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    uint32_t val = 0;
    const int rc = board.mmio_read(0, 0, &val, 4);
    REQUIRE(rc == 0);
    board.shutdown();
}

TEST_CASE("A-3: mmio_read in D3 returns -EIO", "[a3][pcie][unblock]") {
    DGpuBoard board("a3_d3_mmio_read_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    ep->set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);
    REQUIRE(board.is_mmio_gated());

    uint32_t val = 0;
    const int rc = board.mmio_read(0, 0, &val, 4);
    REQUIRE(rc == -EIO);
    board.shutdown();
}

TEST_CASE("A-3: mmio_write D3 non-doorbell returns -EIO", "[a3][pcie][unblock]") {
    DGpuBoard board("a3_d3_mmio_write_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    ep->set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);

    uint32_t val = 0;
    const int rc = board.mmio_write(0, 0, &val, 4);
    REQUIRE(rc == -EIO);
    board.shutdown();
}

TEST_CASE("A-3: mmio_write D3 to doorbell is allowed (compatibility bypass)", "[a3][pcie][unblock]") {
    DGpuBoard board("a3_d3_doorbell_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    ep->set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);

    uint64_t doorbell_wptr = 8;
    const int rc = board.mmio_write(1, DGpuBoard::kBar1DoorbellOffset, &doorbell_wptr, 8);
    REQUIRE(rc == 0);  // doorbell 例外
    board.shutdown();
}

// D1 v1.1.1 Task 2: power gate 优先于 display_routing_enabled fast-path
// 防 root cause 3+4 交叉回归（Oracle 复审 ses_f342ea637ffeDWYBBEQDUUE63U）
TEST_CASE("A-3: D3 power gate applies even with display_routing_enabled",
          "[a3][pcie][unblock][display][routing]") {
    DGpuBoard board("a3_d3_display_routing");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    board.set_display_routing_enabled(true);
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    ep->set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);
    REQUIRE(board.is_mmio_gated());

    uint32_t val = 0;
    const int rc = board.mmio_write(0, 0, &val, 4);
    REQUIRE(rc == -EIO);
    board.shutdown();
}

TEST_CASE("A-3: D3 power gate applies to mmio_read with display_routing_enabled",
          "[a3][pcie][unblock][display][routing]") {
    DGpuBoard board("a3_d3_display_routing_read");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    board.set_display_routing_enabled(true);
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    ep->set_power_state(tlm::pcie::PcieEndpointIP::PciePowerState::D3hot);

    uint32_t val = 0;
    const int rc = board.mmio_read(0, 0, &val, 4);
    REQUIRE(rc == -EIO);
    board.shutdown();
}