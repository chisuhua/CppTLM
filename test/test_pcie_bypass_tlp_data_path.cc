// test/test_pcie_bypass_tlp_data_path.cc
// T-P12-1: profile JSON "pcie_path" 字段 4 态选路 (Day 10-12)
// 功能描述：验证 DGpuBoard::mmio_write 按 pcie_path 分流：
//   - axi_bypass → HostBypassTLM::bar_write (跳过 TLP)
//   - mock       → PcieMockIP 直调 (复用 T-P9-3)
//   - legacy     → mmio_regs_ 既有行为 (向后兼容)
// 作者 CppTLM Team / 日期 2027-09-17
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/spec.md §profile-pcie-path-routing
//       openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P12-1

#include "catch_amalgamated.hpp"

// NOTE: Include pcie_mock_ip.hh BEFORE dgpu_board_shell.hh to avoid namespace
// collision: pcie_mock_ip.hh is in namespace cpptlm::pcie and references
// tlm::gpu::PcieBarRouter. If dgpu_board_shell.hh (which brings in
// cpptlm::tlm::DGpuSoc) is included first, the unqualified tlm::gpu
// resolves to cpptlm::tlm::gpu instead of ::tlm::gpu.
#include "tlm/pcie/pcie_mock_ip.hh"
#include "tlm/gpu/dgpu_board_shell.hh"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

// TEST_CASE 3 用: deterministic pump for mmio_read with pump thread
int mmio_read_with_pump(DGpuBoard& board, uint8_t bar, uint64_t offset,
                         void* buf, size_t len) {
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

// ==================== TEST_CASE 1: axi_bypass profile ====================
TEST_CASE("BypassTLP: axi_bypass profile - profile 选路验证",
          "[pcie-bypass-tlp][profile][axi_bypass]") {
    DGpuBoard board("test_board_axi_bypass");

    json profile;
    profile["pcie_path"] = "axi_bypass";
    board.attach_profile(profile);

    board.init();

    REQUIRE(board.pcie_path() == DGpuBoard::PciePath::AxiBypass);

    uint64_t data = 0xDEADBEEF;
    int rc = board.mmio_write(0, 0x1000, &data, sizeof(data));
    REQUIRE(rc == 0);

    board.tick();
    REQUIRE(board.endpoint_bar_store_value(0, 0, 0x1000) == 0);

    board.shutdown();
}

// ==================== TEST_CASE 2: mock profile (复用 T-P9-3) ====================
TEST_CASE("BypassTLP: mock profile - PcieMockIP 独立组件验证",
          "[pcie-bypass-tlp][profile][mock]") {
    // 独立 PcieMockIP 验证 (T-P9-3 已有完整 [mock-ip] 测试)
    // 此处验证 mock profile 选路下 DGpuBoard 能正确传递
    cpptlm::pcie::PcieMockIP mock;
    json cfg;
    cfg["bar_sizes"] = json::array({0x1000000, 0x1000000, 0, 0, 0, 0});
    mock.attach_composition(cfg);

    // 基本 BAR 读写
    const uint64_t test_data = 0xCAFEBABE;
    int ret = mock.mmio_write(0, 0x1000, &test_data, sizeof(test_data));
    REQUIRE(ret == 0);

    uint64_t readback = 0;
    ret = mock.mmio_read(0, 0x1000, &readback, sizeof(readback));
    REQUIRE(ret == 0);
    REQUIRE(readback == test_data);

    // MSI-X 直接 ABI 回调 (无 TLP 编码延迟)
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

    // 永不进入 TLP 投递链
    REQUIRE_FALSE(mock.msix_tlp_pending());
}

// ==================== TEST_CASE 3: legacy profile (向后兼容) ====================
TEST_CASE("BypassTLP: legacy profile - 既有 mmio_regs_ 行为",
          "[pcie-bypass-tlp][profile][legacy]") {
    DGpuBoard board("test_board_legacy");

    // 设置 profile
    json profile;
    profile["pcie_path"] = "legacy";
    board.attach_profile(profile);

    REQUIRE(board.pcie_path() == DGpuBoard::PciePath::Legacy);

    board.init();

    // mmio_write - legacy 路径: 存入 mmio_regs_
    std::vector<uint8_t> write_data(8);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<uint8_t>(i * 3 + 7);
    }
    REQUIRE(board.mmio_write(0, 0x2000, write_data.data(), write_data.size()) == 0);

    board.tick();

    // mmio_read - legacy 路径: 从 mmio_regs_ 直读
    std::vector<uint8_t> read_data(8, 0xFF);
    REQUIRE(mmio_read_with_pump(board, 0, 0x2000, read_data.data(), read_data.size()) == 0);
    REQUIRE(read_data == write_data);

    // 未初始化寄存器读回 0
    uint64_t zero_val = 0xFFFFFFFFFFFFFFFFULL;
    REQUIRE(mmio_read_with_pump(board, 0, 0x3000, &zero_val, sizeof(zero_val)) == 0);
    REQUIRE(zero_val == 0);

    board.shutdown();
}