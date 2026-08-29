// test_completion_ring_mvp.cc (former test_doorbell_strong_order_mvp.cc)
// CompletionRing exactly-once 测试 (T-bs-2d 移除了原 [doorbell] 3 用例,改由
// test_pcie_endpoint_doorbell_queue.cc 4 用例 PcieBarRouter 路径覆盖)
// Author: CppTLM Team
// Date: 2026-08-26 (T-bs-2d 修订 2026-08-29)

#include "catch_amalgamated.hpp"
#include "tlm/gpu/completion_ring_mvp.hh"

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
