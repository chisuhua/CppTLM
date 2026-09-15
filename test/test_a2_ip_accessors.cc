// test/test_a2_ip_accessors.cc
// A-2 Path A: 验证 5 个 IP accessor（has_config_space / config_space / msix /
// sync_msix_cap_table_size / bar_router）在 PcieEndpointIP 上工作正常。
//
// 参考: openspec/changes/2026-09-15-cpptlm-stage-1-4-2-1-ue-extensions-unblock/v1.2 design.md §A-2
#include <fstream>
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

static json dgpu_mini_cfg() {
    return json::parse(R"({
        "name": "a2_test",
        "params": {"device_id": "0x1234", "quantum_cycles": 1000,
                   "ptx_emu_root": "/tmp/test-ptx-emu"},
        "modules": [{
            "name": "soc", "type": "DGpuSoc",
            "modules": [{
                "name": "pcie_ep", "type": "PcieEndpointIP",
                "params": {
                    "config_size": 4096, "msix_num_vectors": 8,
                    "bar_sizes": [65536, 268435456],
                    "bar0_registers": [
                        {"offset": 0, "name": "GPU_REG_GPFIFO_PUT", "access": "rw"},
                        {"offset": 20, "name": "GPU_REG_DOORBELL", "access": "wo", "side_effect": "doorbell"}
                    ]
                }
            }],
            "connections": []
        }],
        "connections": []
    })");
}

TEST_CASE("A-2: PcieEndpointIP::has_config_space returns true", "[a2][pcie][unblock]") {
    DGpuBoard board("a2_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    REQUIRE(ep->has_config_space());
    board.shutdown();
}

TEST_CASE("A-2: PcieEndpointIP::config_space() reads Vendor ID", "[a2][pcie][unblock]") {
    DGpuBoard board("a2_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    const uint32_t val = ep->config_space().read(0x00);
    REQUIRE((val & 0xFFFFu) == 0x10DEu);  // NVIDIA Vendor ID
    board.shutdown();
}

TEST_CASE("A-2: PcieEndpointIP::msix() runtime API accessible", "[a2][pcie][unblock]") {
    DGpuBoard board("a2_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    REQUIRE(ep->msix().num_vectors() > 0u);
    board.shutdown();
}

TEST_CASE("A-2: PcieEndpointIP::bar_router() populated from bar0_registers JSON", "[a2][pcie][unblock]") {
    DGpuBoard board("a2_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    const auto* doorbell_entry = ep->bar_router().lookup(20);
    REQUIRE(doorbell_entry != nullptr);
    REQUIRE(doorbell_entry->name == "GPU_REG_DOORBELL");
    REQUIRE(doorbell_entry->side_effect == tlm::gpu::PcieBarRouter::SideEffect::DOORBELL);

    const auto* gpfifo_entry = ep->bar_router().lookup(0);
    REQUIRE(gpfifo_entry != nullptr);
    REQUIRE(gpfifo_entry->name == "GPU_REG_GPFIFO_PUT");
    board.shutdown();
}

TEST_CASE("A-2: sync_msix_cap_table_size is no-op (A-2b option 2)", "[a2][pcie][unblock]") {
    DGpuBoard board("a2_test");
    REQUIRE(board.load_soc_config(dgpu_mini_cfg()));
    REQUIRE(board.init());

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    ep->sync_msix_cap_table_size(16);  // no-op 应不抛异常
    // A-2b option 2: cfg_read(0x40) 仍返 PM Cap id=0x01, NOT MSI-X
    REQUIRE((ep->config_space().read(0x40) & 0xFFu) == 0x01u);
    board.shutdown();
}