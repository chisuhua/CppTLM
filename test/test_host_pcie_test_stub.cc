// test/test_host_pcie_test_stub.cc
// HostPcieTestStub 单元测试 (per X1 v1.1 spec AC G2)
//
// 测试标签: [pcie][host-test-stub][unit]
#include "catch_amalgamated.hpp"

#include "tlm/pcie/host_pcie_test_stub.hh"

#include <cstdint>

using tlm::pcie::ErrorParams;
using tlm::pcie::ErrorType;
using tlm::pcie::HostPcieTestStub;

namespace {
    // Mock DGpuBoard (前向 decl + 简单 stub, 实际 board 调用由 e2e 测试 driver 负责)
    class MockDGpuBoard {
    public:
        int trigger_flr_count = 0;
        int resize_bar_count = 0;
        uint8_t last_resize_bar_idx = 0;
        uint64_t last_resize_new_size = 0;
    };
    MockDGpuBoard
        g_mock_board; // 简化: 实际 X1 v1.1 trigger_flr/trigger_resize_bar 通过 host 调用真实 board
} // anonymous namespace

TEST_CASE("HostPcieTestStub: unified ctor signature + board attach",
          "[pcie][host-test-stub][unit][ctor]") {
    // 统一构造签名 (v1.1 修复 v1.0 三处不一致)
    HostPcieTestStub stub(nullptr, "configs/dgpu_soc_v2_realip_p2.json");
    CHECK(stub.board_attached() == false); // nullptr board
    CHECK(stub.config_path() == "configs/dgpu_soc_v2_realip_p2.json");
}

TEST_CASE("HostPcieTestStub: TLP sender increments counter",
          "[pcie][host-test-stub][unit][tlp-sender]") {
    HostPcieTestStub stub(nullptr, "config.json");

    stub.send_config_read(0, 0, 0, 0x10, 4);
    CHECK(stub.tlp_send_count() == 1);
    CHECK(stub.last_tlp_kind() == 0); // CFG_READ = 0

    stub.send_config_write(0, 0, 0, 0x20, 4, 0xDEADBEEF);
    CHECK(stub.tlp_send_count() == 2);
    CHECK(stub.last_tlp_kind() == 1); // CFG_WRITE = 1

    stub.send_mmio_read(0, 0x100, 4);
    CHECK(stub.tlp_send_count() == 3);
    CHECK(stub.last_tlp_kind() == 2); // MMIO_READ = 2

    stub.send_mmio_write(0, 0x200, 4, nullptr);
    CHECK(stub.tlp_send_count() == 4);
    CHECK(stub.last_tlp_kind() == 3); // MMIO_WRITE = 3
}

TEST_CASE("HostPcieTestStub: error injection FIFO", "[pcie][host-test-stub][unit][error-inject]") {
    HostPcieTestStub stub(nullptr, "config.json");

    CHECK(stub.pending_error_count() == 0);
    stub.inject_error(ErrorType::POISONED_TLP, ErrorParams{0x100, 0, 4});
    stub.inject_error(ErrorType::COMPLETION_TIMEOUT);
    CHECK(stub.pending_error_count() == 2);

    CHECK(stub.next_error() == ErrorType::POISONED_TLP);
    // ErrorParams{0x100, 0, 4} → offset=0x100, vector=0, width=4 (aggregate init 顺序)
    CHECK(stub.last_error_params().offset == 0x100);
    CHECK(stub.pending_error_count() == 1);

    CHECK(stub.next_error() == ErrorType::COMPLETION_TIMEOUT);
    CHECK(stub.pending_error_count() == 0);

    CHECK(stub.next_error() == ErrorType::NONE); // empty queue
}

TEST_CASE("HostPcieTestStub: trigger_flr + trigger_resize_bar uint64",
          "[pcie][host-test-stub][unit][state-trigger]") {
    HostPcieTestStub stub(nullptr, "config.json");

    // trigger_flr (v1.1: 计数独立于 board 状态, test fixture 用途)
    stub.trigger_flr();
    CHECK(stub.flr_trigger_count() == 1);

    // trigger_resize_bar (BAR 索引 + uint64 new_size, 修 v1.0 uint32 溢出)
    stub.trigger_resize_bar(2, 0x100000000ULL); // 4GB
    CHECK(stub.resize_trigger_count() == 1);
    CHECK(stub.last_resize_bar_idx() == 2);
    CHECK(stub.last_resize_new_size() == 0x100000000ULL); // 4GB
}

TEST_CASE("HostPcieTestStub: MSI-X handler registration + delivery",
          "[pcie][host-test-stub][unit][msix]") {
    HostPcieTestStub stub(nullptr, "config.json");

    int delivered_vector = -1;
    int delivery_count = 0;
    stub.register_msix_handler([&](uint32_t vector) {
        delivered_vector = static_cast<int>(vector);
        ++delivery_count;
    });
    CHECK(stub.msix_handler_registrations() == 1);

    // 模拟 MSI-X 投递
    stub.simulate_msix_delivery(0);
    CHECK(delivered_vector == 0);
    CHECK(delivery_count == 1);
    CHECK(stub.msix_delivery_count() == 1);

    stub.simulate_msix_delivery(7);
    CHECK(delivered_vector == 7);
    CHECK(delivery_count == 2);
    CHECK(stub.msix_delivery_count() == 2);
}
