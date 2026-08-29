// test_submit_queue_mvp_concurrent.cc
// SubmitQueue concurrent 单测: 多 CTA 并发生命周期
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/submit_queue_mvp.hh"

TEST_CASE("SubmitQueue: multiple CTAs concurrent lifecycle", "[submit-queue][mvp][concurrent]") {
    tlm::gpu::SubmitQueueTLM sq("sq", nullptr);
    tlm::gpu::CtaDescriptor cta{};

    // Phase 1: enqueue 10 CTAs (task_ids 0..9)
    for (uint32_t i = 0; i < 10; i++) {
        cta.task_id = i;
        REQUIRE(sq.enqueue(cta));
    }
    REQUIRE(sq.inflight_count() == 10);

    // Phase 2: tick → dispatch 4 to active (task_ids 0,1,2,3)
    sq.tick();
    REQUIRE(sq.active_count() == 4);
    REQUIRE(sq.pending_count() == 6);

    // Phase 3: complete 0,1 → active has 2,3
    sq.on_warp_complete(0, 0);
    sq.on_warp_complete(1, 0);
    REQUIRE(sq.active_count() == 2);

    // tick → dispatch 4,5 from pending → active has 2,3,4,5
    sq.tick();
    REQUIRE(sq.active_count() == 4);
    REQUIRE(sq.pending_count() == 4);

    // Phase 4: complete 2,3,4,5 → active empty
    for (uint32_t i = 2; i < 6; i++) {
        sq.on_warp_complete(i, 0);
    }
    REQUIRE(sq.active_count() == 0);

    // Phase 5: tick → dispatch 6,7,8,9 from pending
    sq.tick();
    REQUIRE(sq.active_count() == 4);
    REQUIRE(sq.pending_count() == 0);

    // Complete remaining
    for (uint32_t i = 6; i < 10; i++) {
        sq.on_warp_complete(i, 0);
    }
    REQUIRE(sq.active_count() == 0);
    REQUIRE(sq.inflight_count() == 0);
    REQUIRE(sq.completed_count() == 10);
}

TEST_CASE("SubmitQueue: high throughput 8 rounds of 4 CTAs", "[submit-queue][mvp][concurrent]") {
    tlm::gpu::SubmitQueueTLM sq("sq", nullptr);
    tlm::gpu::CtaDescriptor cta{};

    for (uint32_t round = 0; round < 8; round++) {
        // Enqueue 4 fresh CTAs (sequential task_ids 0..31)
        for (uint32_t i = 0; i < 4; i++) {
            cta.task_id = round * 4 + i;
            REQUIRE(sq.enqueue(cta));
        }

        // Dispatch
        sq.tick();

        // Complete all 4 active (in order)
        for (uint32_t i = 0; i < 4; i++) {
            sq.on_warp_complete(round * 4 + i, 0);
        }

        REQUIRE(sq.active_count() == 0);
        REQUIRE(sq.pending_count() == 0);
    }

    REQUIRE(sq.completed_count() == 32);
    REQUIRE(sq.backpressure_count() == 0);
}

TEST_CASE("SubmitQueue: back-pressure recovery via tick after complete",
          "[submit-queue][mvp][concurrent]") {
    tlm::gpu::SubmitQueueTLM sq("sq", nullptr);
    tlm::gpu::CtaDescriptor cta{};

    // Fill pending to 32 (back-pressured state)
    for (uint32_t i = 0; i < 32; i++) {
        cta.task_id = i;
        sq.enqueue(cta);
    }
    REQUIRE(sq.is_full());

    // Dispatch 4 to active (task_ids 0..3)
    sq.tick();
    REQUIRE(sq.pending_count() == 28);

    // Can now enqueue more (task_id=100)
    cta.task_id = 100;
    REQUIRE(sq.enqueue(cta));
    REQUIRE(sq.pending_count() == 29);

    // Complete 4 active (task_ids 0..3)
    for (uint32_t i = 0; i < 4; i++) {
        sq.on_warp_complete(i, 0);
    }
    REQUIRE(sq.active_count() == 0);

    // Tick dispatches 4 from pending (task_ids 4..7)
    sq.tick();
    REQUIRE(sq.pending_count() == 25);
    REQUIRE(sq.active_count() == 4);

    // Complete 4..7
    for (uint32_t i = 4; i < 8; i++) {
        sq.on_warp_complete(i, 0);
    }
    REQUIRE(sq.active_count() == 0);

    // Tick dispatches 4 more (task_ids 8..11)
    sq.tick();
    REQUIRE(sq.pending_count() == 21);
    REQUIRE(sq.active_count() == 4);

    // Drain remaining in batches: track next_id
    uint32_t next_id = 8;
    while (sq.inflight_count() > 0) {
        // Complete all current active (next_id, next_id+1, ...)
        uint32_t to_complete = static_cast<uint32_t>(sq.active_count());
        for (uint32_t i = 0; i < to_complete; i++) {
            sq.on_warp_complete(next_id++, 0);
        }
        sq.tick();
    }
    REQUIRE(sq.inflight_count() == 0);
    // 32 initial + 1 extra = 33 completed
    REQUIRE(sq.completed_count() == 33);
}
