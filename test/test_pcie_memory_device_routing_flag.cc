// test/test_pcie_memory_device_routing_flag.cc
// memory_routing_enabled flag 测试
// Per openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/tasks.md T3.5
#include "catch_amalgamated.hpp"
#include "tlm/gpu/dgpu_board_shell.hh"

using tlm::gpu::DGpuBoard;

TEST_CASE("memory_routing_enabled 默认 false", "[pcie-memory][flag]") {
    DGpuBoard board("test_board");
    REQUIRE(board.memory_routing_enabled() == false);
}

TEST_CASE("set_memory_routing_enabled 开关", "[pcie-memory][flag]") {
    DGpuBoard board("test_board");
    REQUIRE(board.memory_routing_enabled() == false);
    board.set_memory_routing_enabled(true);
    REQUIRE(board.memory_routing_enabled() == true);
    board.set_memory_routing_enabled(false);
    REQUIRE(board.memory_routing_enabled() == false);
}

TEST_CASE("display_routing_enabled 与 memory_routing_enabled 独立", "[pcie-memory][flag]") {
    DGpuBoard board("test_board");
    REQUIRE(board.display_routing_enabled() == false);
    REQUIRE(board.memory_routing_enabled() == false);
    board.set_display_routing_enabled(true);
    REQUIRE(board.display_routing_enabled() == true);
    REQUIRE(board.memory_routing_enabled() == false);
    board.set_memory_routing_enabled(true);
    REQUIRE(board.display_routing_enabled() == true);
    REQUIRE(board.memory_routing_enabled() == true);
}

TEST_CASE("memory_routing_enabled 开关不影响既有的 vram_segments_ 路径", "[pcie-memory][flag]") {
    DGpuBoard board("test_board");
    REQUIRE(board.memory_routing_enabled() == false);
    uint32_t data = 0xDEADBEEF;
    int rc_w = board.backdoor_write(0x1000, &data, sizeof(data));
    REQUIRE(rc_w == 0);
    uint32_t back = 0;
    int rc_r = board.backdoor_read(0x1000, &back, sizeof(back));
    REQUIRE(rc_r == 0);
    REQUIRE(back == data);
}