// test_submit_queue_mvp_enqueue.cc
// SubmitQueue enqueue 单测: 入队 + pending 满拒绝 (不驱逐)
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/submit_queue_mvp.hh"

TEST_CASE("SubmitQueue: enqueue returns true when FIFO not full", "[submit-queue][mvp][enqueue]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};
    cta.task_id = 1;

    REQUIRE(sq.enqueue(cta) == true);
    REQUIRE(sq.pending_count() == 1);
    REQUIRE(sq.inflight_count() == 1);
}

TEST_CASE("SubmitQueue: enqueue multiple entries sequentially", "[submit-queue][mvp][enqueue]") {
    tlm::gpu::SubmitQueue sq;
    for (uint32_t i = 0; i < 5; i++) {
        tlm::gpu::CtaDescriptor cta{};
        cta.task_id = i;
        REQUIRE(sq.enqueue(cta) == true);
    }
    REQUIRE(sq.pending_count() == 5);
}

TEST_CASE("SubmitQueue: enqueue returns false when FIFO full (back-pressure, no eviction)", "[submit-queue][mvp][enqueue]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};

    // 填满 32 槽
    for (uint32_t i = 0; i < 32; i++) {
        cta.task_id = i;
        REQUIRE(sq.enqueue(cta) == true);
    }
    REQUIRE(sq.is_full());
    REQUIRE(sq.pending_count() == 32);

    // 33rd 必须被拒绝 (无驱逐)
    cta.task_id = 100;
    REQUIRE(sq.enqueue(cta) == false);
    REQUIRE(sq.pending_count() == 32);  // 仍 32, 不是 33
    REQUIRE(sq.backpressure_count() == 1);
}

TEST_CASE("SubmitQueue: after dispatch, FIFO can accept more entries", "[submit-queue][mvp][enqueue]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};

    // Fill all 32
    for (uint32_t i = 0; i < 32; i++) {
        cta.task_id = i;
        sq.enqueue(cta);
    }
    REQUIRE(sq.is_full());

    // Dispatch 4 to active
    sq.tick();
    REQUIRE(sq.pending_count() == 28);
    REQUIRE(sq.active_count() == 4);

    // Can now enqueue more
    cta.task_id = 100;
    REQUIRE(sq.enqueue(cta) == true);
    REQUIRE(sq.pending_count() == 29);
}
