// test_dgpu_board_doorbell_routing.cc
// Stage 1.3a integration: DGpuBoard::mmio_write BAR1+0x10010000 doorbell routing
// Verifies: doorbell write → mmio_regs_ stored (修复 #5 兼容) + SOC pcie_ep doorbell_count_++
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: docs/superpowers/specs/2026-09-13-ue-sdma-p0-unblock-design.md §3.2 (目标 1)

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"

#include <cstdint>

using namespace tlm::gpu;

TEST_CASE("DGpuBoard: BAR1+0x10010000 doorbell routes to SOC pcie_ep bar_router",
          "[dgpu-board][doorbell][1.3a][integration]") {
    EventQueue eq;
    DGpuBoard board("test_board", &eq);
    board.init();

    SECTION("doorbell write stores in mmio_regs_ AND routes to SOC") {
        uint32_t wptr = 0x100;  // ring consume up to entry 256
        int ret = board.mmio_write(/*bar=*/1, /*offset=*/0x10010000,
                                   /*buf=*/&wptr, /*len=*/4);

        // 1. doorbell write accepted (return 0, async)
        REQUIRE(ret == 0);

        // 2. mmio_regs_ roundtrip preserved (修复 #5 兼容)
        uint32_t readback = 0;
        int read_ret = board.mmio_read(1, 0x10010000, &readback, 4);
        REQUIRE(read_ret == 0);
        REQUIRE(readback == wptr);

        // 3. SOC pcie_ep doorbell_count_++ (新增: route 到 SOC)
        REQUIRE(board.pcie_ep_doorbell_count() == 1u);
    }
    // 注: 当前 dgpu_board_shell.cc:266-287 mmio_write 真实实现已存 mmio_regs_ (line 271)
    //     并 push to inject_q_; Task 3 实施时需保持此逻辑 + 新增 BAR1+0x10010000 路由

    SECTION("non-doorbell writes do NOT route to SOC pcie_ep") {
        // BAR1 offset 0x14 (GPFIFO_PUT register) - 不是 doorbell
        uint32_t val = 0xDEADBEEF;
        board.mmio_write(1, 0x14, &val, 4);

        // pcie_ep doorbell_count_ 不增加
        REQUIRE(board.pcie_ep_doorbell_count() == 0u);
    }

    SECTION("multiple doorbell writes accumulate doorbell_count") {
        for (uint32_t i = 1; i <= 3; ++i) {
            board.mmio_write(1, 0x10010000, &i, 4);
        }
        REQUIRE(board.pcie_ep_doorbell_count() == 3u);
    }
}
