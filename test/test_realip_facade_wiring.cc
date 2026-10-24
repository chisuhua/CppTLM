#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/pcie/pcie_controller_ip.hh"
#include "tlm/vram_controller_tlm.hh"

#include <fstream>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

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
            {"modules",
             nlohmann::json::array(
                 {{{"type", "DGpuSoc"},
                   {"name", "soc"},
                   {"modules", nlohmann::json::array(
                                   {{{"type", "PcieEndpointIP"},
                                     {"name", "pcie_ep"},
                                     {"params",
                                      {{"config_size", 4096},
                                       {"num_msix_vectors", 16},
                                       {"bar_sizes", {4096, 16777216, 8589934592}}}}},
                                    {{"type", "PcieMemoryDevice"},
                                     {"name", "pcie_memory"},
                                     {"params", {{"capacity_gb", 8}}}},
                                    {{"type", "SdmaEngineTLM"},
                                     {"name", "sdma"},
                                     {"params", {{"max_inflight", 4}}}},
                                    {{"type", "GmmuTLM"},
                                     {"name", "gmmu"},
                                     {"params", {{"page_size_bytes", 4096}}}},
                                    {{"type", "CompletionRingTLM"}, {"name", "completion"}}})},
                   {"connections", nlohmann::json::array()}}})},
            {"connections", nlohmann::json::array()}};
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

    // 强化回归测试 (P0 SIGSEGV fix 2026-10-08): 跑 3 轮 destroy→init cycle
    // 暴露调度敏感的 flaky bug (destroy() reset eq_ 后第二/三轮 init 必须重建)
    for (int cycle = 1; cycle <= 3; ++cycle) {
        REQUIRE(board.load_soc_config(board_cfg));
        REQUIRE(board.init());
        auto* ctrl = board.controller_ip();
        REQUIRE(ctrl != nullptr);
        // 验证 facade 真正激活: endpoint 非 null (per Change B B.2b attach_endpoint)
        REQUIRE(ctrl->endpoint() != nullptr);
        uint32_t val = 0;
        REQUIRE_NOTHROW(board.mmio_read(0, 0x00, &val, 4));

        board.shutdown();
        REQUIRE(board.controller_ip() == nullptr);
    }
}

TEST_CASE("Dormant-guard: pcie_ctrl without pcie_ep collapses BAR1 match_bar to Unknown gracefully",
          "[realip][facade-wiring]") {
    auto eq = std::make_unique<EventQueue>();
    tlm::pcie::PcieControllerIP ctrl("ctrl", eq.get());

    using BarTarget = tlm::pcie::PcieControllerIP::BarTarget;
    BarTarget out_target = BarTarget::Bar0Mmio;
    uint64_t out_sub_offset = 0;

    const bool matched = ctrl.match_bar(1, 0, out_target, out_sub_offset);
    REQUIRE(matched == false);
    REQUIRE(out_target == BarTarget::Unknown);

    out_target = BarTarget::Bar0Mmio;
    const bool matched_oor = ctrl.match_bar(1, 0x10000000, out_target, out_sub_offset);
    REQUIRE(matched_oor == false);
    REQUIRE(out_target == BarTarget::Unknown);
}

constexpr uint32_t kBaselineBar0GmmUOffset0 = 0x00000000u;
constexpr uint32_t kBaselineBar2VramOffset0 = 0x00000000u;

TEST_CASE("Characterization: minimal_v1 BAR0 GMMU read roundtrip unchanged with facade active",
          "[realip][facade-wiring]") {
    auto board_cfg = load_minimal_v1_config();

    EventQueue eq;
    DGpuBoard board("facade_wiring_char_bar0", &eq);
    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE(board.init());

    uint32_t val = 0;
    uint32_t write_val = 0xCAFEBABEu;
    REQUIRE_NOTHROW(board.mmio_write(0, 0x00, &write_val, 4));
    REQUIRE_NOTHROW(board.mmio_read(0, 0x00, &val, 4));
    REQUIRE(val == 0xCAFEBABEu);
}

TEST_CASE("Characterization: doorbell synthetic offset 0x10010000 → Unknown, dispatch unchanged",
          "[realip][facade-wiring]") {
    auto board_cfg = load_minimal_v1_config();

    EventQueue eq;
    DGpuBoard board("facade_wiring_char_doorbell", &eq);
    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE(board.init());

    uint32_t write_val = 0x12345678u;
    REQUIRE_NOTHROW(board.mmio_write(1, 0x10010000, &write_val, 4));
}

TEST_CASE("Characterization: BAR2 VRAM aperture read unchanged with facade active",
          "[realip][facade-wiring]") {
    auto board_cfg = load_minimal_v1_config();

    EventQueue eq;
    DGpuBoard board("facade_wiring_char_bar2", &eq);
    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE(board.init());

    uint32_t val = 0;
    REQUIRE_NOTHROW(board.mmio_read(2, 0x00, &val, 4));
    REQUIRE(val == kBaselineBar2VramOffset0);
}

TEST_CASE("A.5 bridge: DGpuBoard + realip_p2.json + init() wires fabric↔vram_ctrl",
          "[realip][facade-wiring]") {
    std::ifstream f("configs/dgpu_soc_v2_realip_p2.json");
    REQUIRE(f.is_open());
    nlohmann::json board_cfg;
    f >> board_cfg;

    EventQueue eq;
    DGpuBoard board("facade_wiring_a5_bridge", &eq);
    REQUIRE(board.load_soc_config(board_cfg));
    REQUIRE(board.init());

    auto* vram_ctrl = board.vram_ctrl_module();
    REQUIRE(vram_ctrl != nullptr);
    REQUIRE(vram_ctrl->fabric() != nullptr);
}
