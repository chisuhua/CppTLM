// test/test_dgpu_board_msix_wrappers.cc
// T-W3-3 Phase 2: DGpuBoard 4 wrapper 方法 (msix_init/update_pending/clear_pending +
// lookup_register) SOC 未实例化时返 -ENOSYS (-38);参数错误返 -EINVAL (-22)
#include "tlm/gpu/dgpu_board_shell.hh"
#include <catch_amalgamated.hpp>

using namespace tlm::gpu;

TEST_CASE("DGpuBoard::msix_init returns -ENOSYS when SOC not instantiated",
          "[dgpu][shell][msix][wrapper]") {
    DGpuBoard board("test_board");
    board.init();
    REQUIRE(board.msix_init(16, 0xFFFFu) == -38);
    REQUIRE(board.msix_init(2049, 0u) == -22); // table_size > 2048
    board.shutdown();
}

TEST_CASE("DGpuBoard::msix_update_pending returns -ENOSYS when SOC not instantiated",
          "[dgpu][shell][msix][wrapper]") {
    DGpuBoard board("test_board");
    board.init();
    REQUIRE(board.msix_update_pending(0) == -38);
    board.shutdown();
}

TEST_CASE("DGpuBoard::msix_clear_pending returns -ENOSYS when SOC not instantiated",
          "[dgpu][shell][msix][wrapper]") {
    DGpuBoard board("test_board");
    board.init();
    REQUIRE(board.msix_clear_pending(0) == -38);
    board.shutdown();
}

TEST_CASE("DGpuBoard::lookup_register validates args and returns -ENOSYS without SOC",
          "[dgpu][shell][lookup_register][wrapper]") {
    DGpuBoard board("test_board");
    board.init();
    uint32_t val = 0;
    REQUIRE(board.lookup_register(0x14, nullptr) == -22); // null value
    REQUIRE(board.lookup_register(0x15, &val) == -22);    // unaligned
    REQUIRE(board.lookup_register(0x10000, &val) == -22); // > BAR0
    REQUIRE(board.lookup_register(0x14, &val) == -38);    // SOC null
    board.shutdown();
}
