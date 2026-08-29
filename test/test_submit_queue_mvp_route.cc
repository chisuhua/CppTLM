// test_submit_queue_mvp_route.cc
// SubmitQueue route 单测: select_target_core 单 SM 路由
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/submit_queue_mvp.hh"

TEST_CASE("SubmitQueue: select_target_core returns 0 (single-SM MVP)",
          "[submit-queue][mvp][route]") {
    tlm::gpu::SubmitQueueTLM sq("sq", nullptr);
    tlm::gpu::CtaDescriptor cta{};
    cta.task_id = 1;
    cta.grid_x = 64;

    REQUIRE(sq.select_target_core(cta) == 0);
}

TEST_CASE("SubmitQueue: select_target_core ignores CTA fields", "[submit-queue][mvp][route]") {
    tlm::gpu::SubmitQueueTLM sq("sq", nullptr);
    tlm::gpu::CtaDescriptor cta1{};
    cta1.task_id = 1;
    cta1.grid_x = 1024;

    tlm::gpu::CtaDescriptor cta2{};
    cta2.task_id = 2;
    cta2.grid_x = 1;

    // MVP: 无论 CTA 内容, select_target_core 始终返回 0
    REQUIRE(sq.select_target_core(cta1) == 0);
    REQUIRE(sq.select_target_core(cta2) == 0);
}
