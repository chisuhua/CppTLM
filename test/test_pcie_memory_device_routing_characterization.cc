// test/test_pcie_memory_device_routing_characterization.cc
// T0 特征化测试: 锁定 DGpuBoard BAR 2 未映射行为
// 目的: D2 改动后这些行为不变（仅在 memory_device 存在时新增路由）
// Per openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/tasks.md T0.1
#include <cstdint>
#include <cstring>
#include "catch_amalgamated.hpp"
#include "tlm/gpu/dgpu_board_shell.hh"

using tlm::gpu::DGpuBoard;

TEST_CASE("T0 characterization: BAR 2 mmio 无 SOC 走 legacy 路径 (返 0)", "[characterization][pcie][pcie-memory][dgpu-routing]") {
    DGpuBoard board("test_board");
    uint32_t val = 0;
    REQUIRE(board.mmio_write(2, 0x1000, &val, sizeof(val)) == 0);
    REQUIRE(board.mmio_read(2, 0x1000, &val, sizeof(val)) == 0);
    REQUIRE(val == 0);
}

TEST_CASE("T0 characterization: BAR 2 backdoor 走 shell-local vram_segments_", "[characterization][pcie][pcie-memory][dgpu-routing]") {
    DGpuBoard board("test_board");
    uint32_t val = 0xCAFEBABE;
    int rc = board.backdoor_write(0x1000, &val, sizeof(val));
    REQUIRE(rc == 0);
    uint32_t back = 0;
    REQUIRE(board.backdoor_read(0x1000, &back, sizeof(back)) == 0);
    REQUIRE(back == 0xCAFEBABE);
}

TEST_CASE("T0 characterization: memory_routing_enabled 默认 false", "[characterization][pcie][pcie-memory]") {
    DGpuBoard board("test_board");
    REQUIRE(board.memory_routing_enabled() == false);
}