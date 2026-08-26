// test_submit_queue_mvp_complete.cc
// SubmitQueue complete 单测: on_warp_complete 释放 active 槽
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/submit_queue_mvp.hh"

TEST_CASE("SubmitQueue: on_warp_complete releases active slot", "[submit-queue][mvp][complete]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};
    cta.task_id = 42;
    sq.enqueue(cta);
    sq.tick();  // pending → active

    REQUIRE(sq.active_count() == 1);
    REQUIRE(sq.inflight_count() == 1);

    sq.on_warp_complete(42, 0);
    REQUIRE(sq.active_count() == 0);
    REQUIRE(sq.pending_count() == 0);
    REQUIRE(sq.inflight_count() == 0);
    REQUIRE(sq.completed_count() == 1);
}

TEST_CASE("SubmitQueue: on_warp_complete unknown task_id is silent no-op", "[submit-queue][mvp][complete]") {
    tlm::gpu::SubmitQueue sq;
    sq.on_warp_complete(999, -1);  // 空队列
    REQUIRE(sq.inflight_count() == 0);
    REQUIRE(sq.completed_count() == 0);
}

TEST_CASE("SubmitQueue: on_warp_complete selectively releases matching slot", "[submit-queue][mvp][complete]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};

    for (uint32_t i = 0; i < 4; i++) {
        cta.task_id = i + 100;  // 100, 101, 102, 103
        sq.enqueue(cta);
    }
    sq.tick();  // active = [100, 101, 102, 103]

    // Complete 102 only
    sq.on_warp_complete(102, 0);
    REQUIRE(sq.active_count() == 3);
    REQUIRE(sq.completed_count() == 1);

    // Complete 100
    sq.on_warp_complete(100, 0);
    REQUIRE(sq.active_count() == 2);
    REQUIRE(sq.completed_count() == 2);

    // Complete 101 and 103
    sq.on_warp_complete(101, 0);
    sq.on_warp_complete(103, 0);
    REQUIRE(sq.active_count() == 0);
    REQUIRE(sq.completed_count() == 4);
}

TEST_CASE("SubmitQueue: after release, tick() can dispatch new entries", "[submit-queue][mvp][complete]") {
    tlm::gpu::SubmitQueue sq;
    tlm::gpu::CtaDescriptor cta{};

    // Fill pending 8
    for (uint32_t i = 0; i < 8; i++) {
        cta.task_id = i;
        sq.enqueue(cta);
    }

    // Tick: active=4, pending=4
    sq.tick();
    REQUIRE(sq.active_count() == 4);
    REQUIRE(sq.pending_count() == 4);

    // Complete all 4 active
    for (uint32_t i = 0; i < 4; i++) {
        sq.on_warp_complete(i, 0);
    }
    REQUIRE(sq.active_count() == 0);
    REQUIRE(sq.pending_count() == 4);

    // Tick again: active=4, pending=0
    sq.tick();
    REQUIRE(sq.active_count() == 4);
    REQUIRE(sq.pending_count() == 0);
    REQUIRE(sq.inflight_count() == 4);
}
