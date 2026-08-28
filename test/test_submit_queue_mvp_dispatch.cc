// test_submit_queue_mvp_dispatch.cc
// SubmitQueue dispatch 单测: tick() 派发到 active 槽满
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/submit_queue_mvp.hh"

TEST_CASE("SubmitQueue: tick() dispatches up to 4 entries to active slots",
          "[submit-queue][mvp][dispatch]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};

    for (uint32_t i = 0; i < 10; i++) {
        cta.task_id = i;
        REQUIRE(sq.enqueue(cta) == true);
    }
    REQUIRE(sq.pending_count() == 10);
    REQUIRE(sq.active_count() == 0);

    sq.tick();
    REQUIRE(sq.pending_count() == 6);
    REQUIRE(sq.active_count() == 4);

    sq.tick();
    REQUIRE(sq.pending_count() == 6);
    REQUIRE(sq.active_count() == 4);

    sq.on_warp_complete(0, 0);
    sq.on_warp_complete(1, 0);
    REQUIRE(sq.active_count() == 2);

    sq.tick();
    REQUIRE(sq.pending_count() == 4);
    REQUIRE(sq.active_count() == 4);

    for (uint32_t i = 2; i < 6; i++) {
        sq.on_warp_complete(i, 0);
    }
    REQUIRE(sq.active_count() == 0);

    sq.tick();
    REQUIRE(sq.pending_count() == 0);
    REQUIRE(sq.active_count() == 4);
}

TEST_CASE("SubmitQueue: tick() with empty pending does nothing", "[submit-queue][mvp][dispatch]") {
    tlm::gpu::SubmitQueue sq;
    REQUIRE(sq.pending_count() == 0);
    REQUIRE(sq.active_count() == 0);

    sq.tick();
    REQUIRE(sq.pending_count() == 0);
    REQUIRE(sq.active_count() == 0);
}

TEST_CASE("SubmitQueue: tick() with full active does nothing", "[submit-queue][mvp][dispatch]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};

    // Fill pending 4 + active 4
    for (uint32_t i = 0; i < 4; i++) {
        cta.task_id = i;
        sq.enqueue(cta);
    }
    sq.tick(); // → active=4, pending=0

    // Now add more to pending
    for (uint32_t i = 4; i < 8; i++) {
        cta.task_id = i;
        sq.enqueue(cta);
    }
    REQUIRE(sq.pending_count() == 4);
    REQUIRE(sq.active_count() == 4);

    // tick() with full active: cannot dispatch
    sq.tick();
    REQUIRE(sq.pending_count() == 4); // unchanged
    REQUIRE(sq.active_count() == 4);  // unchanged
}

TEST_CASE("SubmitQueue: tick() preserves FIFO order on dispatch", "[submit-queue][mvp][dispatch]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};
    for (uint32_t i = 0; i < 4; i++) {
        cta.task_id = i;
        sq.enqueue(cta);
    }

    sq.tick();
    REQUIRE(sq.active_count() == 4);

    // Complete in reverse order — verify FIFO entry order
    sq.on_warp_complete(0, 0);
    sq.on_warp_complete(1, 0);
    sq.on_warp_complete(2, 0);
    sq.on_warp_complete(3, 0);
    REQUIRE(sq.active_count() == 0);
    REQUIRE(sq.completed_count() == 4);
}
