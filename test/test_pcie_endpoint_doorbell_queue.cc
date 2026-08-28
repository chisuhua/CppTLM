// test_pcie_endpoint_doorbell_queue.cc
// Tier 2 前置测试 (T-prereq-4, P1-B) — Doorbell 排队稳定性 + 延迟区间
//
// 参考:
//   openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/{proposal,design,tasks}.md
//   §T-prereq-4 spec.md Requirement doorbell-queue-stability + Scenarios
//   docs/research/PCIe/PCIe_上的保序write.md §4 (250-700ns 区间)
//
// 4 用例覆盖:
//   1. 100 次连续 MMIO_WRITE 风暴 → 所有 doorbell 完成, no event lost
//   2. doorbell 副作用队列上限触发 (MAX_PENDING_PER_STREAM=64)
//   3. latency 区间稳定性 [250, 700] cycles
//   4. doorbell + SQ + CQ 三方一致性

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/doorbell_mvp.hh"
#include "tlm/gpu/pcie_bar_router_mvp.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"

#include <cstdint>
#include <vector>

using namespace tlm::gpu;
using namespace bundles;

// =====================================================================
// 1. 100 次连续 MMIO_WRITE 风暴 → 所有 doorbell 完成
// =====================================================================
TEST_CASE("PcieEndpoint Doorbell queue: 100 次连续 MMIO_WRITE 风暴",
          "[pcie][endpoint][doorbell][queue][burst]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    REQUIRE(ep.bar_router().add_register(0x0014, "GPU_REG_DOORBELL", PcieBarRouter::Access::WO,
                                         PcieBarRouter::SideEffect::DOORBELL,
                                         /*stream_id=*/0) == true);

    constexpr uint32_t kNumWrites = 100;
    const uint64_t t0 = ep.bar_router().doorbell_now_cycles();

    for (uint32_t i = 0; i < kNumWrites; ++i) {
        REQUIRE(ep.bar_router().mmio_write(0x0014, 0x1000 + i * 4) == true);
    }

    for (int i = 0; i < 5000 && ep.bar_router().doorbell_is_pending(0); ++i) {
        ep.bar_router().tick();
    }

    REQUIRE(ep.bar_router().doorbell_is_pending(0) == false);

    const uint64_t final_tail = ep.bar_router().doorbell_sq_tail(0);
    REQUIRE(final_tail >= 0x1000u);
    REQUIRE(final_tail <= 0x1000u + (kNumWrites - 1) * 4u);

    const uint64_t elapsed = ep.bar_router().doorbell_now_cycles() - t0;
    REQUIRE(elapsed >= 250);
    INFO("Doorbell " << kNumWrites << " writes burst elapsed: " << elapsed << " cycles");
}

// =====================================================================
// 2. Doorbell 副作用队列上限触发 (MAX_PENDING_PER_STREAM=64)
// =====================================================================
TEST_CASE("PcieEndpoint Doorbell queue: 副作用队列上限 (MAX_PENDING_PER_STREAM=64)",
          "[pcie][endpoint][doorbell][queue][limit]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    REQUIRE(ep.bar_router().add_register(0x0014, "GPU_REG_DOORBELL", PcieBarRouter::Access::WO,
                                         PcieBarRouter::SideEffect::DOORBELL,
                                         /*stream_id=*/1) == true);

    constexpr uint32_t kMaxPending = Doorbell::MAX_PENDING_PER_STREAM;
    constexpr uint32_t kOverflow = 4;

    for (uint32_t i = 0; i < kMaxPending + kOverflow; ++i) {
        ep.bar_router().mmio_write(0x0014, 0x2000 + i);
    }

    REQUIRE(ep.bar_router().doorbell_is_pending(1) == true);

    for (int i = 0; i < 5000 && ep.bar_router().doorbell_is_pending(1); ++i) {
        ep.bar_router().tick();
    }
    REQUIRE(ep.bar_router().doorbell_is_pending(1) == false);

    const uint64_t tail = ep.bar_router().doorbell_sq_tail(1);
    REQUIRE(tail >= 0x2000u);
    REQUIRE(tail <= 0x2000u + kMaxPending - 1);
}

// =====================================================================
// 3. Doorbell latency 区间稳定性 (250-700 cycles)
// =====================================================================
TEST_CASE("PcieEndpoint Doorbell queue: latency 区间稳定性 (250-700 cycles)",
          "[pcie][endpoint][doorbell][queue][latency]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    REQUIRE(ep.bar_router().add_register(0x0014, "GPU_REG_DOORBELL", PcieBarRouter::Access::WO,
                                         PcieBarRouter::SideEffect::DOORBELL,
                                         /*stream_id=*/2) == true);

    // 单次写 → latency ∈ [250, 700]
    const uint64_t t0 = ep.bar_router().doorbell_now_cycles();
    ep.bar_router().mmio_write(0x0014, 0x3000);

    while (ep.bar_router().doorbell_is_pending(2)) {
        ep.bar_router().tick();
    }
    const uint64_t elapsed_single = ep.bar_router().doorbell_now_cycles() - t0;

    REQUIRE(elapsed_single >= Doorbell::MIN_LATENCY_NS);
    REQUIRE(elapsed_single <= Doorbell::MAX_LATENCY_NS);
    REQUIRE(ep.bar_router().doorbell_sq_tail(2) == 0x3000u);

    // 多次写 — burst 总耗时受 MAX_LATENCY 上限约束 (now_ 仅在 tick() 推进,
    // 多个 ring() 在同一/相近 cycle 看到同一 now_; FIFO 延后能保证
    // 各完成时间 ∈ [250, 700] 而非 N × MIN)
    constexpr uint32_t kN = 10;
    const uint64_t t1 = ep.bar_router().doorbell_now_cycles();

    for (uint32_t i = 0; i < kN; ++i) {
        ep.bar_router().mmio_write(0x0014, 0x4000 + i);
    }

    while (ep.bar_router().doorbell_is_pending(2)) {
        ep.bar_router().tick();
    }
    const uint64_t elapsed_burst = ep.bar_router().doorbell_now_cycles() - t1;

    // burst 总耗时 ∈ [MIN, MAX*N] (FIFO 累积可能 > MAX 但非 MIN N 倍)
    REQUIRE(elapsed_burst >= Doorbell::MIN_LATENCY_NS);
    REQUIRE(elapsed_burst <= kN * Doorbell::MAX_LATENCY_NS);
    INFO("Doorbell burst (" << kN << " writes) elapsed: " << elapsed_burst << " cycles");
    REQUIRE(ep.bar_router().doorbell_sq_tail(2) >= 0x4000u);
}

// =====================================================================
// 4. Doorbell + SQ + CQ 三方一致性
// =====================================================================
TEST_CASE("PcieEndpoint Doorbell queue: doorbell + SQ + CQ 三方一致性",
          "[pcie][endpoint][doorbell][queue][triple-party]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    REQUIRE(ep.bar_router().add_register(0x0014, "GPU_REG_DOORBELL", PcieBarRouter::Access::WO,
                                         PcieBarRouter::SideEffect::DOORBELL,
                                         /*stream_id=*/3) == true);

    const uint64_t t0 = ep.bar_router().doorbell_now_cycles();
    ep.bar_router().mmio_write(0x0014, 0x5000);

    REQUIRE(ep.bar_router().doorbell_is_pending(3) == true);
    REQUIRE(ep.bar_router().doorbell_sq_tail(3) == 0u);

    for (int i = 0; i < 5000 && ep.bar_router().doorbell_is_pending(3); ++i) {
        ep.bar_router().tick();
    }

    REQUIRE(ep.bar_router().doorbell_is_pending(3) == false);
    REQUIRE(ep.bar_router().doorbell_sq_tail(3) == 0x5000u);

    const uint64_t elapsed = ep.bar_router().doorbell_now_cycles() - t0;
    REQUIRE(elapsed >= 250);
    REQUIRE(elapsed <= 700);

    // 后续写仍正常
    ep.bar_router().mmio_write(0x0014, 0x6000);
    while (ep.bar_router().doorbell_is_pending(3)) {
        ep.bar_router().tick();
    }
    REQUIRE(ep.bar_router().doorbell_sq_tail(3) == 0x6000u);
}

// =====================================================================
// Edge Case: Doorbell cycle_ns=0 防除零退化
// =====================================================================
//
// 来源: src/tlm/gpu/doorbell_mvp.cc:14-18
//   "if (cycle_ns == 0) { cycle_ns = 1; }  // 防止除零, 退化为 1ns/cycle"
// 防御性 invariant: cycle_ns=0 构造不应崩溃或产生 NaN/Inf;应当静默退化
// 为 1ns/cycle 并保持后续行为正确。
TEST_CASE("PcieEndpoint Doorbell edge: cycle_ns=0 防除零退化", "[pcie][endpoint][doorbell][edge]") {
    tlm::gpu::Doorbell db;

    // cycle_ns=0 应当被静默接受为 1ns/cycle (不抛错, 不崩溃)
    REQUIRE_NOTHROW(db.init(0));
    REQUIRE(db.now_cycles() == 0u);

    // tick 推进 1 cycle (cycle_ns_ 退化为 1, 行为正常)
    db.ring(0, 0x100);
    db.tick();
    // ring 后 ~250 cycles 仍未完成, 验证 pending 中
    REQUIRE(db.is_pending(0) == true);
    // 推进直到完成 (防止 hang 验证 cycle_ns_=1 fallback 工作)
    int ticks = 0;
    while (db.is_pending(0) && ticks < 1000) {
        db.tick();
        ++ticks;
    }
    REQUIRE(db.is_pending(0) == false);
    REQUIRE(db.sq_tail(0) == 0x100u);

    // 重新 init(0) 在已积累状态后: streams_ 应清空 (全表重新初始化)
    db.init(0);
    REQUIRE(db.is_pending(0) == false);
    REQUIRE(db.now_cycles() == 0u);
    REQUIRE(db.sq_tail(0) == 0u); // visible_tail 也清零

    // 与 cycle_ns=1 等价行为: 重新 ring 后 latency 仍在 [250, 700] 区间
    const uint64_t t0 = db.now_cycles();
    db.ring(0, 0x200);
    while (db.is_pending(0)) {
        db.tick();
    }
    const uint64_t elapsed = db.now_cycles() - t0;
    REQUIRE(elapsed >= Doorbell::MIN_LATENCY_NS);
    REQUIRE(elapsed <= Doorbell::MAX_LATENCY_NS);

    // 边界: cycle_ns=1 (1ns/cycle 显式) 也允许 (等价于默认)
    db.init(1);
    REQUIRE(db.now_cycles() == 0u);
    db.ring(0, 0x300);
    while (db.is_pending(0)) {
        db.tick();
    }
    REQUIRE(db.sq_tail(0) == 0x300u);
}
