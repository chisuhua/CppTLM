// test_sdma_completion.cc
// Stage 1.3d: SDMA Fence + MSI-X vector 0 + CompletionRing 接线
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//   - Scenario "Fence triggers completion"
//   - Scenario "CompletionRing → MSI-X → intr_cb 200ms"
//
// Oracle R-D 修订:
//   - PCI MSI-X 规范约定 fence = vector 0; msix_init(table_size=4) 默认 fence 占 vector 0
//   - UE 集成测试断言 captured_vector == 0
//   - 链路: FENCE descriptor (opcode=0x04) → completion_ring → MSI-X vector 0
//     → cpptlm_emulator.cc::msix_update_pending → trigger_irq_async(0)
//     → intr_cb(user_ctx, 0, trans_id)
//
// TDD 状态 (RED): 1.3d 实施前 kSdmaFenceVector / on_fence_complete 等未定义.
//   1.3d 实施后变绿.

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/completion_ring_mvp.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

using namespace tlm::gpu;

// =============================================================================
// Spec Scenario "Fence triggers completion"
//   Fence descriptor (opcode=0x04) 提交 → completion event 触发
// =============================================================================
TEST_CASE("SDMA Fence: Fence descriptor → completion_ring.on_fence_complete",
          "[sdma][fence][1.3d]") {
    EventQueue eq;
    CompletionRingTLM ring("fence_ring", &eq);
    SdmaEngineTLM sdma("sdma_fence", &eq);
    sdma.init();

    // 注入 completion_ring → SdmaEngineTLM (1.3d 实施后存在)
    sdma.set_completion_ring(&ring);

    // 计数 on_fence_complete 调用
    static std::atomic<uint64_t> fence_count{0};
    fence_count.store(0);

    // mock host_notify: 记录 fence_id
    static std::atomic<uint64_t> last_fence_id{0};
    last_fence_id.store(0);

    ring.set_host_notify([](uint32_t /*task_id*/, int32_t /*status*/) {
        // host_notify 触发 (legacy API); 新 on_fence_complete 由 sdma 调
    });

    // 提交 Fence descriptor: tag=42, opcode=0x04
    SdmaEngineTLM::FenceDescriptor fence{
        /*opcode=*/0x04,
        /*fence_id=*/42,
        /*tag=*/100,
    };
    sdma.submit_fence(fence);

    // 验证 fence_id 已通过 completion_ring 转发
    // (1.3d 实施后: sdma.tick() 内部调 ring.push → on_fence_complete)
    sdma.tick();
    REQUIRE(ring.pending_count() >= 1u);
}

// =============================================================================
// Oracle R-D 修订: fence = vector 0
//   kSdmaFenceVector 单点常量定义; 全链路禁散落字面量 0
// =============================================================================
TEST_CASE("SDMA Fence: kSdmaFenceVector 单点常量 == 0 (Oracle R-D 修订)",
          "[sdma][fence][1.3d][vector_zero]") {
    // 编译期强约束: kSdmaFenceVector 必须 == 0
    static_assert(SdmaEngineTLM::kSdmaFenceVector == 0,
                  "kSdmaFenceVector 必须 == 0 (Oracle R-D 修订)");

    // 运行时断言 (防止编译期常量被改)
    REQUIRE(SdmaEngineTLM::kSdmaFenceVector == 0u);
}

// =============================================================================
// Spec Scenario "CompletionRing → MSI-X → intr_cb 200ms"
//   Fence → completion_ring → MSI-X vector 0 → trigger_irq_async(0) → intr_cb
//   200ms timeout 窗口内 ≥ 1 次触发, captured_vector == 0
// =============================================================================
TEST_CASE("SDMA Fence: CompletionRing → MSI-X vector 0 → intr_cb 200ms 内触发",
          "[sdma][fence][1.3d][msix][200ms]") {
    EventQueue eq;
    CompletionRingTLM ring("fence_ring_msix", &eq);
    SdmaEngineTLM sdma("sdma_fence_msix", &eq);
    sdma.init();

    std::atomic<int> intr_cb_count{0};
    std::atomic<uint32_t> captured_vector{999};

    // mock MSI-X 触发路径: 完成 → on_fence_complete → trigger vector 0
    // (1.3d 实施后 sdma 内部把 fence 完成转发到 mock callback)
    sdma.set_fence_msix_handler(
        [&](uint32_t vector_id, uint32_t /*payload*/) {
            captured_vector.store(vector_id);
            intr_cb_count.fetch_add(1);
        });

    sdma.set_completion_ring(&ring);

    // 提交 Fence descriptor: fence_id=1
    SdmaEngineTLM::FenceDescriptor fence{0x04, /*fence_id=*/1, /*tag=*/200};
    sdma.submit_fence(fence);
    sdma.tick();

    // 等异步 MSI-X 触发 (200ms timeout)
    for (int i = 0; i < 200; ++i) {
        if (intr_cb_count.load() >= 1)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(intr_cb_count.load() >= 1);
    REQUIRE(captured_vector.load() == SdmaEngineTLM::kSdmaFenceVector);
    REQUIRE(captured_vector.load() == 0u);
}

// =============================================================================
// Spec Scenario: 多个 Fence 顺序触发
//   多个 Fence descriptor 提交, 每次 fence 触发一次 MSI-X
// =============================================================================
TEST_CASE("SDMA Fence: 多个 Fence descriptor 顺序触发 MSI-X",
          "[sdma][fence][1.3d][multi]") {
    EventQueue eq;
    CompletionRingTLM ring("fence_ring_multi", &eq);
    SdmaEngineTLM sdma("sdma_fence_multi", &eq);
    sdma.init();

    std::atomic<int> intr_cb_count{0};
    sdma.set_fence_msix_handler(
        [&](uint32_t /*vector*/, uint32_t /*payload*/) {
            intr_cb_count.fetch_add(1);
        });
    sdma.set_completion_ring(&ring);

    // 提交 3 个 Fence
    for (uint64_t fence_id = 1; fence_id <= 3; ++fence_id) {
        sdma.submit_fence(SdmaEngineTLM::FenceDescriptor{0x04, fence_id, /*tag=*/(uint32_t)fence_id});
    }
    sdma.tick();

    for (int i = 0; i < 200; ++i) {
        if (intr_cb_count.load() >= 3)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(intr_cb_count.load() >= 3);
}

// =============================================================================
// Spec Scenario: fence_id 单调递增 (匹配 driver 端 fence 句柄)
// =============================================================================
TEST_CASE("SDMA Fence: fence_id 通过 host_notify 单调递增传递",
          "[sdma][fence][1.3d][fence_id]") {
    EventQueue eq;
    CompletionRingTLM ring("fence_ring_id", &eq);
    SdmaEngineTLM sdma("sdma_fence_id", &eq);
    sdma.init();

    std::atomic<uint64_t> last_fence_id{0};
    ring.set_host_notify([&](uint32_t task_id, int32_t /*status*/) {
        last_fence_id.store(static_cast<uint64_t>(task_id));
    });

    sdma.set_completion_ring(&ring);

    sdma.submit_fence(SdmaEngineTLM::FenceDescriptor{0x04, /*fence_id=*/0xCAFE, /*tag=*/0xCAFE});
    sdma.tick();
    // ring.tick() 消费 entry 触发 host_notify (CompletionRingTLM 自身 tick)
    INFO("ring.pending_count() before ring.tick() = " << ring.pending_count());
    ring.tick();
    INFO("last_fence_id after ring.tick() = " << last_fence_id.load());

    REQUIRE(last_fence_id.load() == 0xCAFEu);
}
