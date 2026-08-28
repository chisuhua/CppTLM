// test_pcie_endpoint_msix_state.cc
// Tier 2 前置测试 (T-prereq-3, P1-C) — MSI-X 状态机细粒度
//
// 参考:
//   openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/{proposal,design,tasks}.md
//   §T-prereq-3 spec.md Requirement msix-state-machine-fine-grained + Scenarios VFIO SET_IRQS 契约
//   (per ADR-088 + ADR-SOC-08 §D3.4)
//
// 5 用例验证 PBA (Pending Bit Array) 语义 + 多 vector 并发触发顺序:
//   1. mask 期间 pending 不丢失 (PBA 语义的核心)
//   2. PBA bit 与 vector pending 状态同步性
//   3. 多 vector 并发触发顺序 (FIFO per vector, cross-vector 顺序由 irq_out 决定)
//   4. mask 永久 → 反复 mask/unmask 后仍能触发
//   5. vector 配置/释放 (configure_vector + clear_mask 触发 PBA 自动投递)

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/msix_table_mvp.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"

using namespace tlm::gpu;
using namespace bundles;

// =====================================================================
// 1. mask 期间 pending 不丢失 (PBA 语义)
// =====================================================================
//
// spec Scenario "mask 期间 pending 不丢失":
//   WHEN mask → pending → unmask
//   THEN pending preserved across mask period; trigger fires after unmask
TEST_CASE("PcieEndpoint MSI-X state: mask 期间 pending 不丢失 (PBA 语义)",
          "[pcie][endpoint][msix][state][pba]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    ep.msix().configure_vector(3, 0xFEE00000ULL, 0xCAFEBABEu);

    // 1. mask → update_pending (set PBA only, no queue)
    ep.msix().set_mask(3, true);
    REQUIRE(ep.msix().update_pending(3, /*trans_id=*/42) == true);
    REQUIRE(ep.msix().pending_count() == 0u); // queue 仍空
    REQUIRE(ep.msix().is_pba_set(3) == true); // PBA bit 置位
    REQUIRE(ep.msix().is_pending(3) == true); // 组合查询显示 pending

    // 2. 多次 update_pending 在 masked 状态 → PBA bit 仍为 1 (累积语义)
    for (int i = 0; i < 5; ++i) {
        REQUIRE(ep.msix().update_pending(3) == true);
    }
    REQUIRE(ep.msix().is_pba_set(3) == true);
    REQUIRE(ep.msix().pending_count() == 0u); // 仍仅 1 个 PBA bit, queue 空

    // 3. unmask → 自动投递累积 IRQ (PBA 语义核心)
    ep.msix().clear_mask(3);
    REQUIRE(ep.msix().pending_count() == 1u); // set_mask(v,false) 自动入队
    REQUIRE(ep.msix().is_pba_set(3) == true); // PBA bit 保留 (driver EOI 才清)

    // 4. tick() → IRQ 投递到 irq_out 端口
    ep.tick();
    REQUIRE(ep.resp_out[3].valid() == true);
    const auto& out = ep.resp_out[3].data();
    REQUIRE(out.is_irq_delivery() == true);
    REQUIRE(out.offset.read() == 3u);        // vector → offset
    REQUIRE(out.size.read() == 0xCAFEBABEu); // msg_data → size

    // 5. driver EOI → clear_pending 清除 PBA
    ep.msix().consume_irq_out();
    REQUIRE(ep.msix().pending_count() == 0u);
    ep.msix().clear_pending(3);
    REQUIRE(ep.msix().is_pba_set(3) == false);
    REQUIRE(ep.msix().is_pending(3) == false);

    // 6. 后续 update_pending 正常 (PBA + queue 一致)
    REQUIRE(ep.msix().update_pending(3) == true);
    REQUIRE(ep.msix().pending_count() == 1u);
    REQUIRE(ep.msix().is_pba_set(3) == true);
}

// =====================================================================
// 2. PBA bit 与 vector pending 同步性
// =====================================================================
//
// spec Scenario "PBA bit 与 vector pending 同步":
//   WHEN vector N has pending=true
//   THEN PBA bit N is set; reading PBA returns bitmask consistent with vector pending
TEST_CASE("PcieEndpoint MSI-X state: PBA bit 与 vector pending 同步性",
          "[pcie][endpoint][msix][state][pba]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 配置多个 vector
    ep.msix().configure_vector(2, 0xFEE00000ULL, 0xAABBCCDDu);
    ep.msix().configure_vector(5, 0xFEE00000ULL, 0x11223344u);
    ep.msix().configure_vector(7, 0xFEE00000ULL, 0x55667788u);

    // 初始: 所有 vector PBA bit 清零
    REQUIRE(ep.msix().is_pba_set(2) == false);
    REQUIRE(ep.msix().is_pba_set(5) == false);
    REQUIRE(ep.msix().is_pba_set(7) == false);
    REQUIRE(ep.msix().pba_count() == 0u);

    // 触发 vector 2 + 7 (未 mask, 直接同步投递)
    REQUIRE(ep.msix().update_pending(2) == true);
    REQUIRE(ep.msix().update_pending(7) == true);

    // PBA bit 反映状态 (vector 5 仍未 pending)
    REQUIRE(ep.msix().is_pba_set(2) == true);
    REQUIRE(ep.msix().is_pba_set(5) == false);
    REQUIRE(ep.msix().is_pba_set(7) == true);
    REQUIRE(ep.msix().pba_count() == 2u); // 2 + 7

    // 触发 vector 5 但先 mask → PBA bit 仍置位
    ep.msix().set_mask(5, true);
    REQUIRE(ep.msix().update_pending(5) == true);
    REQUIRE(ep.msix().is_pba_set(5) == true);
    REQUIRE(ep.msix().pba_count() == 3u);

    // driver EOI 仅清 vector 2 + 7 (queue 顺序)
    ep.msix().clear_pending(2);
    REQUIRE(ep.msix().is_pba_set(2) == false);
    REQUIRE(ep.msix().is_pba_set(5) == true); // 仍未清 (masked PBA)
    REQUIRE(ep.msix().is_pba_set(7) == true);
    REQUIRE(ep.msix().pba_count() == 2u);

    ep.msix().clear_pending(7);
    REQUIRE(ep.msix().is_pba_set(7) == false);
    REQUIRE(ep.msix().pba_count() == 1u); // 仅 5 仍 pending (masked PBA)

    // 解除 mask 后 5 自动投递
    ep.msix().clear_mask(5);
    REQUIRE(ep.msix().pending_count() == 1u); // 自动入队
    ep.msix().clear_pending(5);               // driver EOI
    REQUIRE(ep.msix().pba_count() == 0u);     // 全部清
}

// =====================================================================
// 3. 多 vector 并发触发顺序
// =====================================================================
//
// spec: 多个 vector 同时触发, irq_out 队列按 vector update 顺序排列
TEST_CASE("PcieEndpoint MSI-X state: 多 vector 并发触发顺序",
          "[pcie][endpoint][msix][state][multi-vector]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    ep.msix().configure_vector(1, 0xFEE00000ULL, 0x11111111u);
    ep.msix().configure_vector(2, 0xFEE00000ULL, 0x22222222u);
    ep.msix().configure_vector(3, 0xFEE00000ULL, 0x33333333u);

    // vector 1 → 3 → 2 顺序触发
    REQUIRE(ep.msix().update_pending(1) == true);
    REQUIRE(ep.msix().update_pending(3) == true);
    REQUIRE(ep.msix().update_pending(2) == true);

    REQUIRE(ep.msix().pending_count() == 3u);
    REQUIRE(ep.msix().pba_count() == 3u);

    // 直接验证 msix 队列顺序 (FIFO, 按 update 顺序排列)
    // 注意: 调用 ep.tick() 会将所有 IRQ 一次性 consume 到 resp_out[3],
    //       这里改为直接验证 msix_ queue 顺序以避免 tick consume 副作用

    const auto* evt1 = ep.msix().try_pop_irq_out();
    REQUIRE(evt1 != nullptr);
    REQUIRE(evt1->vector == 1u);
    ep.msix().consume_irq_out();

    const auto* evt2 = ep.msix().try_pop_irq_out();
    REQUIRE(evt2 != nullptr);
    REQUIRE(evt2->vector == 3u);
    ep.msix().consume_irq_out();

    const auto* evt3 = ep.msix().try_pop_irq_out();
    REQUIRE(evt3 != nullptr);
    REQUIRE(evt3->vector == 2u);
    ep.msix().consume_irq_out();

    REQUIRE(ep.msix().pending_count() == 0u);
    REQUIRE(ep.msix().try_pop_irq_out() == nullptr);
}

// =====================================================================
// 4. mask 长期后 unmask 仍正常触发
// =====================================================================
//
// spec 衍生: "长期 mask 后 unmask 仍可触发" (per design §2.3 scenario 5)
TEST_CASE("PcieEndpoint MSI-X state: 长期 mask 后 unmask 仍可触发",
          "[pcie][endpoint][msix][state][long-mask]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    ep.msix().configure_vector(4, 0xFEE00000ULL, 0xDEADBEEFu);

    // 1. mask → 写 1 → mask 持久 → 再写 5 次 → unmask
    ep.msix().set_mask(4, true);
    ep.msix().update_pending(4, /*trans_id=*/1);
    for (uint32_t i = 2; i <= 6; ++i) {
        ep.msix().update_pending(4, i);
    }
    REQUIRE(ep.msix().is_pba_set(4) == true);
    REQUIRE(ep.msix().pending_count() == 0u);

    // 2. 长期 mask 期间反复 mask(redundant)操作不破坏 PBA
    for (int i = 0; i < 10; ++i) {
        ep.msix().set_mask(4, true); // idempotent
        REQUIRE(ep.msix().is_pba_set(4) == true);
    }
    REQUIRE(ep.msix().pending_count() == 0u);

    // 3. 重新配置 vector (改 data + 保持 mask) → 不破坏 PBA bit
    ep.msix().configure_vector(4, 0xFEE00000ULL, 0xCAFEFEEDu, /*control=*/0x1);
    REQUIRE(ep.msix().is_pba_set(4) == true);
    REQUIRE(ep.msix().is_masked(4) == true);

    // 4. unmask → 自动投递累积 IRQ (用最新 entry 数据)
    ep.msix().clear_mask(4);
    REQUIRE(ep.msix().pending_count() == 1u);

    const auto* evt = ep.msix().try_pop_irq_out();
    REQUIRE(evt != nullptr);
    REQUIRE(evt->vector == 4u);
    REQUIRE(evt->msg_data == 0xCAFEFEEDu); // latest entry data propagated on unmask
    ep.msix().consume_irq_out();

    // 5. driver EOI → 后续 update 正常
    ep.msix().clear_pending(4);
    REQUIRE(ep.msix().is_pba_set(4) == false);

    REQUIRE(ep.msix().update_pending(4) == true);
    REQUIRE(ep.msix().pending_count() == 1u);
}

// =====================================================================
// 5. vector 配置/释放 (configure_vector + clear_mask 触发 PBA 自动投递)
// =====================================================================
//
// spec 衍生: 配置后 verify 配置生效; mask 后 unmask 触发 PBA 自动投递;
//           释放后 vector 重新可用。
TEST_CASE("PcieEndpoint MSI-X state: vector 配置/释放 + PBA 自动投递",
          "[pcie][endpoint][msix][state][config]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 1. 配置 vector 6
    REQUIRE(ep.msix().configure_vector(6, 0xFEE00000ULL, 0x99887766u, /*control=*/0x1) == true);
    REQUIRE(ep.msix().is_masked(6) == true); // control bit 0 = mask
    REQUIRE(ep.msix().update_pending(6) == true);
    REQUIRE(ep.msix().is_pba_set(6) == true);
    REQUIRE(ep.msix().pending_count() == 0u);

    // 2. clear_mask 触发 PBA 自动投递
    REQUIRE(ep.msix().clear_mask(6) == true);
    REQUIRE(ep.msix().is_masked(6) == false);
    REQUIRE(ep.msix().pending_count() == 1u);

    const auto* evt = ep.msix().try_pop_irq_out();
    REQUIRE(evt != nullptr);
    REQUIRE(evt->vector == 6u);
    REQUIRE(evt->msg_data == 0x99887766u);
    ep.msix().consume_irq_out();

    // 3. driver EOI → clear_pending 清 PBA + queue
    REQUIRE(ep.msix().clear_pending(6) == true);
    REQUIRE(ep.msix().is_pba_set(6) == false);
    REQUIRE(ep.msix().pending_count() == 0u);

    // 4. 重新配置 (改 mask control = 0) → 自动清 mask + reconfigure
    REQUIRE(ep.msix().configure_vector(6, 0xFEE00000ULL, 0xAABBCC01u, /*control=*/0x0) == true);
    REQUIRE(ep.msix().is_masked(6) == false);
    REQUIRE(ep.msix().update_pending(6) == true);
    REQUIRE(ep.msix().pending_count() == 1u);

    ep.msix().consume_irq_out();
    ep.msix().clear_pending(6);
    REQUIRE(ep.msix().pending_count() == 0u);

    // 5. 越界配置 + 越界 update 不破坏状态
    REQUIRE(ep.msix().configure_vector(99, 0, 0) == false);
    REQUIRE(ep.msix().update_pending(99) == false);
    REQUIRE(ep.msix().is_pba_set(6) == false); // 未被越界污染
    REQUIRE(ep.msix().pending_count() == 0u);
}

// =====================================================================
// Edge Case 1: MsiXTable::MsiXTable(0) throws std::invalid_argument
// =====================================================================
//
// 来源: src/tlm/gpu/msix_table_mvp.cc:13
//   "if (num_vectors == 0) throw std::invalid_argument(...)"
// 防御性 invariant: 0-vector 构造应当拒绝, 避免后续 resize(0) 后行为未定义。
TEST_CASE("PcieEndpoint MSI-X edge: MsiXTable(0) 抛 std::invalid_argument",
          "[pcie][endpoint][msix][state][edge]") {
    // 直接构造 (不走 PcieEndpointTLM), 因为 PcieEndpointTLM 不会传 0
    REQUIRE_THROWS_AS(tlm::gpu::MsiXTable(0), std::invalid_argument);

    // 借用 catch 测试异常类型包含期望信息
    bool caught = false;
    try {
        tlm::gpu::MsiXTable table(0);
    } catch (const std::invalid_argument& e) {
        caught = true;
        REQUIRE(std::string(e.what()).find("num_vectors") != std::string::npos);
    } catch (...) {
        FAIL("Wrong exception type thrown");
    }
    REQUIRE(caught);

    // 边界 1-vector 应允许 (最小有效值)
    REQUIRE_NOTHROW(tlm::gpu::MsiXTable(1));

    // 边界 256-vector 应允许
    REQUIRE_NOTHROW(tlm::gpu::MsiXTable(256));
}

// =====================================================================
// Edge Case 2: MsiXTable::init() 清零 PBA bits
// =====================================================================
//
// 来源: src/tlm/gpu/msix_mvp_table.cc:25 (init() 调用 std::fill(pba_, 0))
// 防御性 invariant: 即使 MsiXTable 已积累 PBA 状态, init() 应当全部清零,
// 否则 PBA bit 可能从上次未 reset 的状态泄漏。
TEST_CASE("PcieEndpoint MSI-X edge: init() 重置清零 PBA bits",
          "[pcie][endpoint][msix][state][edge]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    ep.msix().configure_vector(3, 0xFEE00000ULL, 0xCAFEBABEu);
    ep.msix().set_mask(3, true);

    // 累积 PBA 状态 (mask 期间多次 update)
    for (int i = 0; i < 5; ++i) {
        ep.msix().update_pending(3, /*trans_id=*/i + 1);
    }
    REQUIRE(ep.msix().is_pba_set(3) == true);
    REQUIRE(ep.msix().pba_count() >= 1u);

    // 也注入 vector 5 的 pending 以验证全表清零
    ep.msix().configure_vector(5, 0xFEE00000ULL, 0xDEADBEEFu);
    ep.msix().set_mask(5, true);
    ep.msix().update_pending(5, 1);
    REQUIRE(ep.msix().pba_count() >= 2u);

    // unmask 3 后入队; 累积 queue 与 PBA 混合状态
    ep.msix().clear_mask(3);
    REQUIRE(ep.msix().pending_count() >= 1u);

    // 关键操作: re-init() 必须将**所有**状态清零 (PBA + queue + entries)
    ep.msix().init();

    // 验证 PBA bits 全部清零 (init() 不能忘记 pba_ 字段)
    REQUIRE(ep.msix().pba_count() == 0u);
    REQUIRE(ep.msix().is_pba_set(3) == false);
    REQUIRE(ep.msix().is_pba_set(5) == false);

    // 验证 queue 清零
    REQUIRE(ep.msix().pending_count() == 0u);
    REQUIRE(ep.msix().try_pop_irq_out() == nullptr);

    // 验证 entries_ (mask control) 也清零 — vector 3/5 被 mask 后 init 应重置
    REQUIRE(ep.msix().is_masked(3) == false);
    REQUIRE(ep.msix().is_masked(5) == false);

    // 验证 init 后 functional — update_pending 正常投递
    REQUIRE(ep.msix().update_pending(3, /*trans_id=*/99) == true);
    REQUIRE(ep.msix().is_pba_set(3) == true);
    REQUIRE(ep.msix().pending_count() == 1u);
}
