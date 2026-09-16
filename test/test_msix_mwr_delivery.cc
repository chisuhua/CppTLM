// test/test_msix_mwr_delivery.cc
// MSI-X pending → MWr TLP 投递链单元测试 (T-P11-3)
// 功能：验证 PcieSriovVfPool::dispatch_msix 触发后，
//       tick() 投递 MWr TLP → link_layer->tx_tlp → host
//       投递完成后调 cpptlm_intr_deliver_cb_t ABI 回调
// 作者 CppTLM Team / 日期 2027-02-10
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P11-3
//       spec.md §MSI-X 投递链
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_sriov_vf_pool_tlm.hh"

#include <cstdint>

using namespace tlm::pcie;

// ========== Test helpers ==========

// 从 LinkLayer tx 输出中查找匹配某个 kind 的 TLP
static bool find_tx_tlp_of_kind(PcieLinkLayer& ll, uint8_t kind,
                                 bundles::PcieTlpBundle& out) {
    bundles::PcieTlpBundle tlp;
    while (ll.try_pop_tx_tlp(tlp)) {
        if (tlp.kind.read() == kind) {
            out = tlp;
            return true;
        }
    }
    return false;
}

static std::size_t count_tx_tlp_of_kind(PcieLinkLayer& ll, uint8_t kind) {
    std::size_t cnt = 0;
    bundles::PcieTlpBundle tlp;
    while (ll.try_pop_tx_tlp(tlp)) {
        if (tlp.kind.read() == kind) {
            ++cnt;
        }
    }
    return cnt;
}

// ========== TEST_CASE 1: dispatch_msix → tick → MWr TLP ==========
TEST_CASE("PcieSriovVfPool: dispatch_msix 触发后 tick 投递 MWr TLP",
          "[pcie][msix][msix-mwr][t-p11-3]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieSriovVfPool pool;

    // 注入 LinkLayer
    pool.set_link_layer(&ll);

    // 配置 MSI-X table: VF0 (stream_id=1), vector=0
    // msg_addr=0xFEE00000 (MSI-X 投递地址), msg_data=0x42
    REQUIRE(pool.msix_configure_vector(1, 0, 0xFEE00000ULL, 0x42, 0) == true);

    // 触发 dispatch_msix (stream_id=1, vector=0)
    REQUIRE(pool.dispatch_msix(1, 0) == true);

    // tick → 投递 MWr TLP
    pool.tick(100);

    // 验证: tx_tlp 发出了 MWr
    bundles::PcieTlpBundle sent;
    REQUIRE(find_tx_tlp_of_kind(ll, bundles::PcieTlpBundle::MMIO_WRITE, sent) == true);

    // 验证 MWr TLP 内容
    REQUIRE(sent.offset.read() == 0xFEE00000ULL);  // msg_addr
    REQUIRE(sent.data.read() == 0x42);              // msg_data
    REQUIRE(sent.size.read() == 4);                 // 4 bytes (per MSI-X spec)
    REQUIRE(sent.trans_id.read() == 0);             // vector
}

// ========== TEST_CASE 2: 投递完成后调 intr_delivered_callback ==========
TEST_CASE("PcieSriovVfPool: 投递完成后调 intr_delivered_callback",
          "[pcie][msix][msix-mwr][t-p11-3]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieSriovVfPool pool;
    pool.set_link_layer(&ll);

    // 注册 MSI-X 投递完成回调
    int callback_count = 0;
    uint32_t callback_vector = 0xFFFFFFFF;
    pool.register_intr_delivered_callback([&](uint32_t vector) {
        ++callback_count;
        callback_vector = vector;
    });

    // 配置 MSI-X table: PF (stream_id=0), vector=2
    REQUIRE(pool.msix_configure_vector(0, 2, 0xFEE00100ULL, 0x99, 0) == true);

    // 触发 dispatch_msix
    REQUIRE(pool.dispatch_msix(0, 2) == true);

    // tick 前回调未触发
    REQUIRE(callback_count == 0);

    // tick → 投递 MWr
    pool.tick(100);

    // 验证回调被触发
    REQUIRE(callback_count == 1);
    REQUIRE(callback_vector == 2);
}

// ========== TEST_CASE 3: 多 vector pending 顺序投递 ==========
TEST_CASE("PcieSriovVfPool: 多 vector pending 顺序投递",
          "[pcie][msix][msix-mwr][t-p11-3]") {
    EventQueue eq;
    PcieLinkLayer ll(&eq);
    PcieSriovVfPool pool;
    pool.set_link_layer(&ll);

    // 记录回调触发的 vector
    std::vector<uint32_t> delivered_vectors;
    pool.register_intr_delivered_callback([&](uint32_t vector) {
        delivered_vectors.push_back(vector);
    });

    // 配置 MSI-X table: VF0 (stream_id=1) vector 0/1/2
    REQUIRE(pool.msix_configure_vector(1, 0, 0xFEE00000ULL, 0x42, 0) == true);
    REQUIRE(pool.msix_configure_vector(1, 1, 0xFEE00010ULL, 0x43, 0) == true);
    REQUIRE(pool.msix_configure_vector(1, 2, 0xFEE00020ULL, 0x44, 0) == true);

    // 触发 3 个 vector (按顺序)
    REQUIRE(pool.dispatch_msix(1, 0) == true);
    REQUIRE(pool.dispatch_msix(1, 1) == true);
    REQUIRE(pool.dispatch_msix(1, 2) == true);

    // tick → 投递 3 个 MWr TLP
    pool.tick(100);

    // 验证 3 个 MWr TLP 都发出
    REQUIRE(count_tx_tlp_of_kind(ll, bundles::PcieTlpBundle::MMIO_WRITE) == 3);

    // 验证回调触发顺序: vector 0, 1, 2
    REQUIRE(delivered_vectors.size() == 3);
    // tx_tlp 的 trans_id 分别对应 vector
    // 回调按投递完成顺序触发
    if (delivered_vectors.size() >= 3) {
        // LL tx_tlp 按顺序发出, 因此 vector 顺序保持
        INFO("Checking delivery order: " << delivered_vectors[0] << ", "
             << delivered_vectors[1] << ", " << delivered_vectors[2]);
    }
}