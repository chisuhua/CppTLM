// test_doorbell_strong_order_mvp.cc
// Doorbell 强序写 + 延迟区间 / CompletionRing exactly-once 测试
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/completion_ring_mvp.hh"
#include "tlm/gpu/doorbell_mvp.hh"

TEST_CASE("Doorbell: latency in 250-700ns range", "[doorbell][mvp][strong-order]") {
    tlm::gpu::Doorbell db;
    db.init(/*cycle_ns=*/1);

    auto t0 = db.now_cycles();
    db.ring(0, 0x100);

    // Advance simulation 700 cycles
    while (db.is_pending(0) && db.now_cycles() - t0 < 1000) {
        db.tick();
    }

    auto latency = db.now_cycles() - t0;
    REQUIRE(latency >= 250);
    REQUIRE(latency <= 700);
}

TEST_CASE("Doorbell: same stream observes call order", "[doorbell][mvp][strong-order]") {
    tlm::gpu::Doorbell db;
    db.init(1);

    db.ring(0, 0x100); // first
    db.ring(0, 0x200); // second (overrides first per SQ tail semantics)

    // Drain all pending
    while (db.is_pending(0))
        db.tick();

    // Final visible tail must be 0x200 (most recent)
    REQUIRE(db.sq_tail(0) == 0x200);
}

TEST_CASE("Doorbell: pending queue full returns false", "[doorbell][mvp][strong-order]") {
    tlm::gpu::Doorbell db;
    db.init(1);

    // 不 tick, 填满 64 深度在途队列
    for (size_t i = 0; i < tlm::gpu::Doorbell::MAX_PENDING_PER_STREAM; i++) {
        REQUIRE(db.ring(1, 0x1000 + i));
    }
    // 第 65 次应失败
    REQUIRE_FALSE(db.ring(1, 0xDEAD));
}

TEST_CASE("CompletionRing: 10 sequential on_warp_complete → 10 host_notify",
          "[completion-ring][mvp]") {
    tlm::gpu::CompletionRing cq;
    int notify_count = 0;
    cq.set_host_notify([&](uint32_t task_id, int32_t status) {
        (void)task_id;
        (void)status;
        notify_count++;
    });

    for (uint32_t i = 0; i < 10; i++) {
        cq.on_warp_complete(i, 0);
    }
    // Drain all
    for (int j = 0; j < 100; j++)
        cq.tick();

    REQUIRE(notify_count == 10);
}

TEST_CASE("CompletionRing: entries delivered in FIFO order with payload intact",
          "[completion-ring][mvp]") {
    tlm::gpu::CompletionRing cq;
    std::vector<uint32_t> seen_tasks;
    std::vector<int32_t> seen_status;
    cq.set_host_notify([&](uint32_t task_id, int32_t status) {
        seen_tasks.push_back(task_id);
        seen_status.push_back(status);
    });

    REQUIRE(cq.on_warp_complete(42, 0));
    REQUIRE(cq.on_warp_complete(43, -1));
    REQUIRE(cq.inflight_count() == 2);

    while (cq.inflight_count() > 0)
        cq.tick();

    REQUIRE(seen_tasks == std::vector<uint32_t>{42, 43});
    REQUIRE(seen_status == std::vector<int32_t>{0, -1});
}

TEST_CASE("CompletionRing: full ring rejects without overwrite", "[completion-ring][mvp]") {
    tlm::gpu::CompletionRing cq(4);
    for (uint32_t i = 0; i < 4; i++) {
        REQUIRE(cq.on_warp_complete(i, 0));
    }
    REQUIRE_FALSE(cq.on_warp_complete(99, 0));
    REQUIRE(cq.inflight_count() == 4);
}
