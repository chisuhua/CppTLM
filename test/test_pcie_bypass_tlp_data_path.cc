// test/test_pcie_bypass_tlp_data_path.cc
// T-P12-1: profile JSON "pcie_path" 字段 4 态选路 (Day 10-12)
// Change B B.5 rewrite: removed pcie_path() mechanism assertions per Oracle bg_14b6dd83
// AD-B3 + Plan Task 5.1. Test now asserts data path behavior without facade wiring
// (these tests don't load SOC, so controller_ip_ stays nullptr — facade irrelevant here).
// Profile JSON "pcie_path" stays for back-compat; Change C will delete mechanism.
// 作者 CppTLM Team / 日期 2027-09-17 (B.5 rewrite 2026-10-08)
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/spec.md
// §profile-pcie-path-routing

#include "catch_amalgamated.hpp"

#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/pcie/pcie_mock_ip.hh"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

    int mmio_read_with_pump(DGpuBoard& board, uint8_t bar, uint64_t offset, void* buf, size_t len) {
        std::atomic<bool> done{false};
        std::thread pump([&]() {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!done.load() && std::chrono::steady_clock::now() < deadline) {
                board.tick();
                std::this_thread::yield();
            }
        });
        int rc = board.mmio_read(bar, offset, buf, len);
        done.store(true);
        pump.join();
        return rc;
    }

} // namespace

TEST_CASE("BypassTLP: axi_bypass profile - data path 验证",
          "[pcie-bypass-tlp][profile][axi_bypass]") {
    DGpuBoard board("test_board_axi_bypass");

    board.init();

    uint64_t data = 0xDEADBEEF;
    int rc = board.mmio_write(0, 0x1000, &data, sizeof(data));
    REQUIRE(rc == 0);

    board.tick();
    REQUIRE(board.endpoint_bar_store_value(0, 0, 0x1000) == 0);

    board.shutdown();
}

TEST_CASE("BypassTLP: mock profile - PcieMockIP 独立组件验证", "[pcie-bypass-tlp][profile][mock]") {
    cpptlm::pcie::PcieMockIP mock;
    json cfg;
    cfg["bar_sizes"] = json::array({0x1000000, 0x1000000, 0, 0, 0, 0});
    mock.attach_composition(cfg);

    const uint64_t test_data = 0xCAFEBABE;
    int ret = mock.mmio_write(0, 0x1000, &test_data, sizeof(test_data));
    REQUIRE(ret == 0);

    uint64_t readback = 0;
    ret = mock.mmio_read(0, 0x1000, &readback, sizeof(readback));
    REQUIRE(ret == 0);
    REQUIRE(readback == test_data);

    ret = mock.msix_init(4, 0);
    REQUIRE(ret == 0);

    int callback_count = 0;
    uint32_t last_vector = 0;
    mock.register_msi_callback([&](uint32_t vector, uint32_t /*trans_id*/) {
        callback_count++;
        last_vector = vector;
    });

    ret = mock.msix_update_pending(0);
    REQUIRE(ret == 0);
    REQUIRE(callback_count == 1);
    REQUIRE(last_vector == 0);

    REQUIRE_FALSE(mock.msix_tlp_pending());
}

TEST_CASE("BypassTLP: legacy profile - 既有 mmio_regs_ 行为",
          "[pcie-bypass-tlp][profile][legacy]") {
    DGpuBoard board("test_board_legacy");

    board.init();

    std::vector<uint8_t> write_data(8);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<uint8_t>(i * 3 + 7);
    }
    REQUIRE(board.mmio_write(0, 0x2000, write_data.data(), write_data.size()) == 0);

    board.tick();

    std::vector<uint8_t> read_data(8, 0xFF);
    REQUIRE(mmio_read_with_pump(board, 0, 0x2000, read_data.data(), read_data.size()) == 0);
    REQUIRE(read_data == write_data);

    uint64_t zero_val = 0xFFFFFFFFFFFFFFFFULL;
    REQUIRE(mmio_read_with_pump(board, 0, 0x3000, &zero_val, sizeof(zero_val)) == 0);
    REQUIRE(zero_val == 0);

    board.shutdown();
}
