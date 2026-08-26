// test_tmu_dispatch_processor_mvp_skeleton.cc
// TMU Dispatch Processor 骨架单测: 反压 + 容量管理
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/tmu_dispatch_processor_mvp.hh"

TEST_CASE("TmuDispatchProcessor: starts with 0 in-flight", "[tmu][mvp][skeleton]") {
    tlm::gpu::TmuDispatchProcessor tmu;
    REQUIRE(tmu.inflight_count() == 0);
    REQUIRE(tmu.submitted_count() == 0);
    REQUIRE(tmu.completed_count() == 0);
}

TEST_CASE("TmuDispatchProcessor: submit succeeds and increments count", "[tmu][mvp][skeleton]") {
    tlm::gpu::TmuDispatchProcessor tmu;
    tlm::gpu::TmuDispatchRecord rec{};
    rec.task_id = 1;

    auto result = tmu.submit(rec);
    REQUIRE(result == tlm::gpu::TmuSubmitResult::SUBMITTED);
    REQUIRE(tmu.inflight_count() == 1);
    REQUIRE(tmu.submitted_count() == 1);
}

TEST_CASE("TmuDispatchProcessor: 32 submits succeed, 33rd returns BACKPRESSURED", "[tmu][mvp][skeleton]") {
    tlm::gpu::TmuDispatchProcessor tmu;

    for (uint32_t i = 0; i < 32; i++) {
        tlm::gpu::TmuDispatchRecord rec{};
        rec.task_id = i;
        REQUIRE(tmu.submit(rec) == tlm::gpu::TmuSubmitResult::SUBMITTED);
    }
    REQUIRE(tmu.inflight_count() == 32);

    // 33rd 必须 BACKPRESSURED
    tlm::gpu::TmuDispatchRecord rec{};
    rec.task_id = 100;
    REQUIRE(tmu.submit(rec) == tlm::gpu::TmuSubmitResult::BACKPRESSURED);
    REQUIRE(tmu.inflight_count() == 32);
    REQUIRE(tmu.backpressure_count() == 1);
}

TEST_CASE("TmuDispatchProcessor: on_complete decrements inflight", "[tmu][mvp][skeleton]") {
    tlm::gpu::TmuDispatchProcessor tmu;
    tlm::gpu::TmuDispatchRecord rec{};
    rec.task_id = 1;
    tmu.submit(rec);
    REQUIRE(tmu.inflight_count() == 1);

    tmu.on_complete(1, 0);
    REQUIRE(tmu.inflight_count() == 0);
    REQUIRE(tmu.completed_count() == 1);
}

TEST_CASE("TmuDispatchProcessor: on_complete at 0 is no-op", "[tmu][mvp][skeleton]") {
    tlm::gpu::TmuDispatchProcessor tmu;
    tmu.on_complete(999, -1);
    REQUIRE(tmu.inflight_count() == 0);
    REQUIRE(tmu.completed_count() == 0);
}

TEST_CASE("TmuDispatchProcessor: dep_enable records return DEP_LATCH_MISMATCH", "[tmu][mvp][skeleton]") {
    tlm::gpu::TmuDispatchProcessor tmu;
    tlm::gpu::TmuDispatchRecord rec{};
    rec.task_id = 1;
    rec.dep_enable = true;

    auto result = tmu.submit(rec);
    REQUIRE(result == tlm::gpu::TmuSubmitResult::DEP_LATCH_MISMATCH);
    REQUIRE(tmu.inflight_count() == 0);
    REQUIRE(tmu.dep_latch_mismatch_count() == 1);
}

TEST_CASE("TmuDispatchProcessor: set_handler holds handler; submit calls handler", "[tmu][mvp][skeleton]") {
    class StubHandler : public tlm::gpu::TmuHandlerInterface {
    public:
        int call_count = 0;
        tlm::gpu::TmuHandlerResult on_dispatch(const tlm::gpu::TmuDispatchRecord&) override {
            ++call_count;
            return tlm::gpu::TmuHandlerResult::HANDLED;
        }
    };

    // Note: StubHandler needs to be alive while submit() runs
    auto stub = std::make_unique<StubHandler>();
    auto* stub_ptr = stub.get();

    tlm::gpu::TmuDispatchProcessor tmu;
    tmu.set_handler(std::move(stub));

    tlm::gpu::TmuDispatchRecord rec{};
    rec.task_id = 1;
    auto result = tmu.submit(rec);

    REQUIRE(result == tlm::gpu::TmuSubmitResult::SUBMITTED);
    REQUIRE(stub_ptr->call_count == 1);
}

TEST_CASE("TmuDispatchProcessor: handler SQ_REJECTED propagates to SUBMIT_QUEUE_REJECTED", "[tmu][mvp][skeleton]") {
    class RejectingHandler : public tlm::gpu::TmuHandlerInterface {
    public:
        tlm::gpu::TmuHandlerResult on_dispatch(const tlm::gpu::TmuDispatchRecord&) override {
            return tlm::gpu::TmuHandlerResult::SQ_REJECTED;
        }
    };

    tlm::gpu::TmuDispatchProcessor tmu;
    tmu.set_handler(std::make_unique<RejectingHandler>());

    tlm::gpu::TmuDispatchRecord rec{};
    rec.task_id = 1;
    auto result = tmu.submit(rec);

    REQUIRE(result == tlm::gpu::TmuSubmitResult::SUBMIT_QUEUE_REJECTED);
    REQUIRE(tmu.inflight_count() == 0);  // 没注册 (handler 拒收)
    REQUIRE(tmu.sq_rejected_count() == 1);
}

TEST_CASE("TmuDispatchProcessor: back-pressure recovery via on_complete", "[tmu][mvp][skeleton]") {
    tlm::gpu::TmuDispatchProcessor tmu;

    // Fill to 32
    for (uint32_t i = 0; i < 32; i++) {
        tlm::gpu::TmuDispatchRecord rec{};
        rec.task_id = i;
        tmu.submit(rec);
    }
    REQUIRE(tmu.inflight_count() == 32);

    // Submit one more → BACKPRESSURED
    tlm::gpu::TmuDispatchRecord rec{};
    rec.task_id = 100;
    REQUIRE(tmu.submit(rec) == tlm::gpu::TmuSubmitResult::BACKPRESSURED);

    // Complete 4
    for (uint32_t i = 0; i < 4; i++) {
        tmu.on_complete(i, 0);
    }
    REQUIRE(tmu.inflight_count() == 28);

    // Now can submit 4 more
    for (uint32_t i = 200; i < 204; i++) {
        tlm::gpu::TmuDispatchRecord r{};
        r.task_id = i;
        REQUIRE(tmu.submit(r) == tlm::gpu::TmuSubmitResult::SUBMITTED);
    }
    REQUIRE(tmu.inflight_count() == 32);
    REQUIRE(tmu.submitted_count() == 36);  // 32 initial + 4 new
}
