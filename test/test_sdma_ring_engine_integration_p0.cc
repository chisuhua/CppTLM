// test_sdma_ring_engine_integration_p0.cc
// Stage 1.3a integration (P0 unblock Task 4-6):
//   DGpuBoard::mmio_write BAR1+0x10010000 → SdmaEngineTLM::mmio_write → ring consume
// Verifies: doorbell write 真实触发 SDMA ring 处理 (ring_consumed_count 推进)
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: docs/superpowers/specs/2026-09-13-ue-sdma-p0-unblock-design.md §3.2 (目标 2)

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"

#include <cstdint>

using namespace tlm::gpu;

TEST_CASE("DGpuBoard BAR1 doorbell routes to SdmaEngineTLM ring consume",
          "[sdma][ring][integration][1.3a]") {
    EventQueue eq;
    DGpuBoard board("test_board_ring_p0", &eq);
    board.init();

    SdmaEngineTLM sdma("sdma_p0", &eq);
    sdma.init();
    // 启用 ring mode (1.3a 已 ship API)
    sdma.enable_ring_mode(::tlm::gpu::SdmaRingBuffer::RingSize::KB_64,
                          ::tlm::gpu::SdmaRingBuffer::EntrySize::B_64);

    SECTION("doorbell write → SdmaEngineTLM::ring_consumed_count 推进") {
        // Patch (Task 6): DGpuBoard 注入 SdmaEngineTLM 引用
        //   此 SECTION 在 Task 6 实施前编译失败 (set_sdma_engine 未定义)
        board.set_sdma_engine(&sdma);

        // 初始: ring_consumed_count = 0
        REQUIRE(sdma.ring_consumed_count() == 0u);

        // 通过 DGpuBoard::mmio_write 触发 BAR1 doorbell (wptr=8)
        uint32_t wptr = 8;
        int ret = board.mmio_write(/*bar=*/1, /*offset=*/DGpuBoard::kBar1DoorbellOffset,
                                   /*buf=*/&wptr, /*len=*/4);
        REQUIRE(ret == 0);

        // 1.3a 已 ship 的内部 handler 应该被调 (Task 6 wiring)
        //   → SdmaEngineTLM::mmio_write 处理 ring consume
        //   → ring_consumed_count 累加
        REQUIRE(sdma.ring_consumed_count() >= 8u);
    }

    SECTION("non-doorbell mmio_write 不触发 SdmaEngineTLM ring") {
        board.set_sdma_engine(&sdma);

        // BAR1 offset 0x14 (非 doorbell)
        uint32_t val = 0xDEADBEEF;
        board.mmio_write(1, 0x14, &val, 4);

        // ring_consumed_count 仍为 0 (非 doorbell 不触发 ring)
        REQUIRE(sdma.ring_consumed_count() == 0u);
    }
}
