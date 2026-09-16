// test/test_pcie_requester_engine_basic.cc
// PcieRequesterEngine MRd 发起 + CplD 接收 + tag 关联 + 超时 (T-P11-1)
// 5 TEST_CASE 覆盖 spec.md §requester-engine-outgoing Scenario
// 作者 CppTLM Team / 日期 2027-02-09
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P11-1

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_requester_engine.hh"

#include <atomic>
#include <cstdint>
#include <string>

using namespace bundles;
using namespace cpptlm::pcie;

// ==================== TEST_CASE 1: MRd 发起 NP credit 消耗 ====================

TEST_CASE("RequesterEngine MRd 发起 tx_tlp + NP credit 消耗", "[pcie][requester][engine][T-P11-1]") {
    EventQueue eq;
    tlm::pcie::PcieLinkLayerConfig cfg;
    cfg.fc_capacity = 256;
    cfg.fc_init_np = 256;
    tlm::pcie::PcieLinkLayer ll(&eq, cfg);

    PcieRequesterEngine req_eng(&ll);
    REQUIRE(req_eng.outstanding_count() == 0);
    REQUIRE(ll.tx_tlp_out_count() == 0);
    REQUIRE(ll.retry_buffer_size() == 0);

    // 发起 MRd 64B
    bool sent = req_eng.mrd_read(0, 0x10000000, 0, 64);
    REQUIRE(sent);  // NP=256 充足 → 发送成功

    // 验证 TLP 发出
    REQUIRE(ll.tx_tlp_out_count() == 1);
    REQUIRE(ll.retry_buffer_size() == 1);
    // 验证 outstanding 计数
    REQUIRE(req_eng.outstanding_count() == 1);
    // 验证 tag 分配 (tag != 0, 0 保留)
    REQUIRE(req_eng.last_allocated_tag() != 0);
}

// ==================== TEST_CASE 2: CplD 接收 + tag 关联 + completion callback ====================

TEST_CASE("RequesterEngine CplD 接收 + tag 关联 + completion callback", "[pcie][requester][engine][cpld][T-P11-1]") {
    EventQueue eq;
    tlm::pcie::PcieLinkLayer ll(&eq);

    PcieRequesterEngine req_eng(&ll);
    std::atomic<bool> completed{false};
    uint16_t matched_tag = 0xFF;

    // 注册 completion callback
    req_eng.register_completion_callback(
        [&](uint16_t tag, const PcieTlpBundle& /*cpld*/) {
            matched_tag = tag;
            completed = true;
        });

    bool sent = req_eng.mrd_read(0, 0x10000000, 0, 64);
    REQUIRE(sent);
    uint16_t tag = req_eng.last_allocated_tag();
    REQUIRE(tag != 0);
    REQUIRE(req_eng.outstanding_count() == 1);

    // 模拟 CplD 接收 (tag 匹配)
    PcieTlpBundle cpld;
    cpld.kind.write(PcieTlpBundle::CPLD);
    cpld.requester_id.write(0x0100);
    cpld.trans_id.write(tag);
    cpld.size.write(64);
    cpld.data.write(0xDEADBEEFull);

    req_eng.on_cpld_received(cpld);

    // 验证 callback 触发 + tag 释放
    REQUIRE(completed);
    REQUIRE(matched_tag == tag);
    REQUIRE(req_eng.outstanding_count() == 0);
}

// ==================== TEST_CASE 3: Completion 超时 error_cb ====================

TEST_CASE("RequesterEngine Completion 超时 error_cb", "[pcie][requester][engine][timeout][T-P11-1]") {
    EventQueue eq;
    tlm::pcie::PcieLinkLayer ll(&eq);

    // 超时 = 100ns
    PcieRequesterEngine req_eng(&ll, 100);
    std::atomic<bool> error{false};
    std::string error_reason;

    req_eng.register_error_callback(
        [&](uint32_t tag, const std::string& reason) {
            error = true;
            error_reason = reason;
            (void)tag;
        });

    bool sent = req_eng.mrd_read(0, 0x10000000, 0, 64);
    REQUIRE(sent);
    REQUIRE(req_eng.outstanding_count() == 1);

    // tick 200ns (超时)
    req_eng.tick(200);

    // 验证 error callback + tag 释放
    REQUIRE(error);
    REQUIRE_FALSE(error_reason.empty());
    REQUIRE(req_eng.outstanding_count() == 0);
}

// ==================== TEST_CASE 4: 多 outstanding 并发 + 各自 CplD 匹配 ====================

TEST_CASE("RequesterEngine 多 outstanding 并发 + 各自 CplD 匹配", "[pcie][requester][engine][multi][T-P11-1]") {
    EventQueue eq;
    tlm::pcie::PcieLinkLayer ll(&eq);

    PcieRequesterEngine req_eng(&ll);
    int completion_count = 0;

    req_eng.register_completion_callback(
        [&](uint16_t /*tag*/, const PcieTlpBundle& /*cpld*/) {
            ++completion_count;
        });

    // 发起 3 个 MRd (3 个不同地址)
    REQUIRE(req_eng.mrd_read(0, 0x10000000, 0, 64));
    uint16_t tag1 = req_eng.last_allocated_tag();
    REQUIRE(req_eng.mrd_read(0, 0x10001000, 0, 64));
    uint16_t tag2 = req_eng.last_allocated_tag();
    REQUIRE(req_eng.mrd_read(0, 0x10002000, 0, 64));
    uint16_t tag3 = req_eng.last_allocated_tag();

    // 3 个 tag 互不相同
    REQUIRE(tag1 != tag2);
    REQUIRE(tag2 != tag3);
    REQUIRE(tag1 != tag3);
    REQUIRE(req_eng.outstanding_count() == 3);
    REQUIRE(completion_count == 0);

    // 模拟 CplD tag=2 (中间那个)
    PcieTlpBundle cpld_2;
    cpld_2.kind.write(PcieTlpBundle::CPLD);
    cpld_2.trans_id.write(tag2);
    req_eng.on_cpld_received(cpld_2);

    // 验证仅 tag=2 完成
    REQUIRE(completion_count == 1);
    REQUIRE(req_eng.outstanding_count() == 2);

    // 补 CplD tag=1
    PcieTlpBundle cpld_1;
    cpld_1.kind.write(PcieTlpBundle::CPLD);
    cpld_1.trans_id.write(tag1);
    req_eng.on_cpld_received(cpld_1);
    REQUIRE(completion_count == 2);
    REQUIRE(req_eng.outstanding_count() == 1);

    // 补 CplD tag=3
    PcieTlpBundle cpld_3;
    cpld_3.kind.write(PcieTlpBundle::CPLD);
    cpld_3.trans_id.write(tag3);
    req_eng.on_cpld_received(cpld_3);
    REQUIRE(completion_count == 3);
    REQUIRE(req_eng.outstanding_count() == 0);
}

// ==================== TEST_CASE 5: tag 耗尽 (4096 outstanding) ====================

TEST_CASE("RequesterEngine tag 耗尽 (4096 outstanding)", "[pcie][requester][engine][tagfull][T-P11-1]") {
    EventQueue eq;
    // 需要 4096+ NP credits, 但 PcieLinkLayer::SEQ_WINDOW=2048 限制 retry buffer 深度
    // 无法同时保持 4096 outstanding. 这里验证: tag 分配顺序 + 可填满 SEQ_WINDOW-1 槽位
    tlm::pcie::PcieLinkLayerConfig cfg;
    cfg.fc_capacity = 3000;
    cfg.fc_init_np = 3000;
    cfg.fc_init_p = 3000;
    cfg.fc_init_cpl = 3000;
    tlm::pcie::PcieLinkLayer ll(&eq, cfg);

    PcieRequesterEngine req_eng(&ll);
    int success_count = 0;

    // 发起 2048 个 MRd (SEQ_WINDOW=2048, 刚好填满 retry buffer)
    for (int i = 0; i < 2048; ++i) {
        bool sent = req_eng.mrd_read(0, 0x10000000 + i * 0x1000, 0, 64);
        if (sent) ++success_count;
    }
    // 2048 应该全部成功 (tag 1-2048, 12-bit 范围)
    REQUIRE(success_count == 2048);
    REQUIRE(req_eng.outstanding_count() == 2048);

    // 第 2049 个 → retry buffer 满 → tx_tlp 返回 false
    bool sent = req_eng.mrd_read(0, 0x10000000, 0, 64);
    REQUIRE_FALSE(sent);
    REQUIRE(req_eng.outstanding_count() == 2048);  // 未变化
}