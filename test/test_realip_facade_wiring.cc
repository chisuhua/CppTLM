#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/pcie/pcie_controller_ip.hh"
#include "tlm/vram_controller_tlm.hh"

#include <nlohmann/json.hpp>
#include <fstream>
#include <memory>
#include <vector>

using tlm::gpu::DGpuBoard;

namespace {

nlohmann::json load_minimal_v1_config() {
    std::ifstream f("configs/dgpu_soc_minimal_v1.json");
    REQUIRE(f.is_open());
    nlohmann::json cfg;
    f >> cfg;
    return cfg;
}

nlohmann::json inline_no_pcie_ctrl_cfg() {
    nlohmann::json cfg = {
        {"modules", nlohmann::json::array({
            {
                {"type", "DGpuSoc"},
                {"name", "soc"},
                {"modules", nlohmann::json::array({
                    {
                        {"type", "PcieEndpointIP"},
                        {"name", "pcie_ep"},
                        {"params", {
                            {"config_size", 4096},
                            {"num_msix_vectors", 16},
                            {"bar_sizes", {4096, 16777216, 8589934592}}
                        }}
                    },
                    {
                        {"type", "PcieMemoryDevice"},
                        {"name", "pcie_memory"},
                        {"params", {{"capacity_gb", 8}}}
                    },
                    {
                        {"type", "SdmaEngineTLM"},
                        {"name", "sdma"},
                        {"params", {{"max_inflight", 4}}}
                    },
                    {
                        {"type", "GmmuTLM"},
                        {"name", "gmmu"},
                        {"params", {{"page_size_bytes", 4096}}}
                    },
                    {
                        {"type", "CompletionRingTLM"},
                        {"name", "completion"}
                    }
                })},
                {"connections", nlohmann::json::array()}
            }
        })},
        {"connections", nlohmann::json::array()}
    };
    return cfg;
}

} // namespace

TEST_CASE("DGpuBoard::init wires controller_ip_ on minimal_v1 (facade active)",
          "[realip][facade-wiring]") {
    auto board_cfg = load_minimal_v1_config();

    EventQueue eq;
    DGpuBoard board("facade_wiring_minimal", &eq);
    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE(board.init());

    REQUIRE(board.controller_ip() != nullptr);
    uint32_t val = 0;
    REQUIRE_NOTHROW(board.mmio_read(0, 0x00, &val, 4));
}

TEST_CASE("DGpuBoard::init gracefully skips facade when pcie_ctrl absent",
          "[realip][facade-wiring]") {
    auto board_cfg = inline_no_pcie_ctrl_cfg();

    EventQueue eq;
    DGpuBoard board("facade_wiring_no_ctrl", &eq);
    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE_NOTHROW(board.init());

    REQUIRE(board.controller_ip() == nullptr);
}

TEST_CASE("DGpuBoard destroy→init cycle re-wires facade without dangling pointer",
          "[realip][facade-wiring]") {
    auto board_cfg = load_minimal_v1_config();

    EventQueue eq;
    DGpuBoard board("facade_wiring_cycle", &eq);

    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE(board.init());
    auto* first_ctrl = board.controller_ip();
    REQUIRE(first_ctrl != nullptr);
    uint32_t val = 0;
    REQUIRE_NOTHROW(board.mmio_read(0, 0x00, &val, 4));

    board.shutdown();
    REQUIRE(board.controller_ip() == nullptr);

    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE(board.init());
    auto* second_ctrl = board.controller_ip();
    REQUIRE(second_ctrl != nullptr);
    REQUIRE_NOTHROW(board.mmio_read(0, 0x00, &val, 4));
}
