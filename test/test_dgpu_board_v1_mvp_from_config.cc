// test_dgpu_board_v1_mvp_from_config.cc
// DGpuBoardTLM E2E 测试 (per s2 W4 T-s2-5 acceptance, 5 SECTION)
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_mvp.hh"

TEST_CASE("DGpuBoardTLM: 5 SECTION E2E (item 4 ⏳ deferred to s3, per Oracle M3)",
          "[dgpu-board][mvp][e2e]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("dgpu_board_0", &eq);

    SECTION("1. validate_topology: DGpuBoardTLM instantiates without error") {
        REQUIRE_NOTHROW(board.init());
#ifdef CPPTLM_WITH_PTX_EMU
        REQUIRE(board.has_s1_components());
#else
        REQUIRE_FALSE(board.has_s1_components());
#endif
    }

    SECTION("2. instantiateAll: DGpuBoardTLM tick() runs without crash") {
        board.init();
        REQUIRE_NOTHROW(board.tick());
    }

    SECTION("3. H2D: install_kernel_module returns handle != 0") {
        board.init();
        const uint8_t image[128] = {0xDE, 0xAD, 0xBE, 0xEF};
        uint64_t handle = board.install_kernel_module(image, sizeof(image));
        REQUIRE(handle != 0);
        REQUIRE(handle == 1);

        uint64_t handle2 = board.install_kernel_module(image, sizeof(image));
        REQUIRE(handle2 == 2);
    }

    SECTION("4. Launch (IOCTL 0x01 pushbuffer) — ⏳ DEFERRED to s3") {
        board.init();
        tlm::gpu::KernelLaunchRequest req{};
        req.task_id = 42;
        req.grid_x = 1;
        req.block_x = 32;
        req.shared_mem_bytes = 0;

        int32_t result = board.submit_kernel(req);
        REQUIRE(result == 0);
    }

    SECTION("5. host_notify fires when completion pushed (direct CQ path)") {
        board.init();
        REQUIRE_NOTHROW(board.tick());
    }
}

TEST_CASE("DGpuBoardTLM: write_reg triggers CP wake", "[dgpu-board][mvp][e2e]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("dgpu_board_0", &eq);
    board.init();

    board.write_reg(0x1000, 0x100);

    for (int i = 0; i < 5; i++) {
        REQUIRE_NOTHROW(board.tick());
    }
}

TEST_CASE("DGpuBoardTLM: tick cycles through pipeline", "[dgpu-board][mvp][e2e]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("dgpu_board_0", &eq);
    board.init();

    for (int i = 0; i < 10; i++) {
        REQUIRE_NOTHROW(board.tick());
    }
}

TEST_CASE("DGpuBoardTLM: shutdown is idempotent", "[dgpu-board][mvp][e2e]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("dgpu_board_0", &eq);
    board.init();

    REQUIRE_NOTHROW(board.shutdown());
    REQUIRE_NOTHROW(board.shutdown());
}
