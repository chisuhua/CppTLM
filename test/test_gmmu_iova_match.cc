// test/test_gmmu_iova_match.cc
// GMMU 异步状态机 iova 匹配测试 (N2, per design.md §5)
// Phase 3 (driver-visible-minimal-soc v1.8): 验证 COMPLETE/WAIT pending_iova_ 匹配
// 防止 SDMA 重试时不同 iova 静默错配。
#include <cstdint>
#include <cstring>
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/ext/packet_pool.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/gmmu_tlm.hh"

using tlm::gpu::GmmuTLM;
using bundles::PcieTlpBundle;

TEST_CASE("gmmu: async translate -EAGAIN + iova 匹配 (N2)", "[gmmu][iova][async]") {
    EventQueue eq;
    GmmuTLM g("g", &eq);
    g.set_enabled(true);
    g.set_pt_base_lo(0x1000);
    g.set_pt_base_hi(0);
    // 不注入 mem_view_ → 走异步 MasterPort 路径

    uint64_t pa = 0;

    // 1. 首个 translate → IDLE → 发 MEM_READ → -EAGAIN
    int rc = g.translate(0x1000, 4096, pa);
    REQUIRE(rc == -EAGAIN);
    REQUIRE(g.req_out().valid());
    auto req = g.req_out().data();
    REQUIRE(req.kind.read() == PcieTlpBundle::MEM_READ);
    REQUIRE(req.offset.read() == 0x1000 + (0x1000 >> 12) * 8);
    REQUIRE(req.size.read() == 8);
    REQUIRE(g.is_async_pending() == true);

    // 2. 不同 iova → 仍 -EAGAIN, pending_iova_ 不变 (N2 关键)
    uint64_t pa2 = 0;
    rc = g.translate(0x2000, 4096, pa2);
    REQUIRE(rc == -EAGAIN);
    REQUIRE(g.pending_iova() == 0x1000);

    // 3. 注入 PTE 响应 (CPLD 带 data=PTE) via InputStreamAdapter::process (Packet 反序列化)
    PcieTlpBundle resp;
    resp.kind.write(PcieTlpBundle::CPLD);
    const uint64_t pte = (0x5000 & ~0xFFFULL) | 1ULL;
    resp.data.write(pte);
    resp.trans_id.write(req.trans_id.read());
    Packet* pkt = PacketPool::get().acquire();
    pkt->payload->set_data_length(sizeof(PcieTlpBundle));
    REQUIRE(bundles::serialize_bundle(resp, pkt->payload->get_data_ptr(),
                                      pkt->payload->get_data_length()));
    REQUIRE(g.resp_in().process(pkt));
    g.tick();  // state_ WAIT → COMPLETE, resp_in consume
    PacketPool::get().release(pkt);

    // 4. 匹配 iova 重试 → 0 + 正确 paddr
    rc = g.translate(0x1000, 4096, pa);
    REQUIRE(rc == 0);
    REQUIRE(pa == 0x5000);
    REQUIRE(g.is_async_pending() == false);

    // 5. 不匹配 iova → 仍 -EAGAIN (COMPLETE 已消费)
    rc = g.translate(0x3000, 4096, pa);
    REQUIRE(rc == -EAGAIN);
}

TEST_CASE("gmmu: dual-mode legacy sync 兼容", "[gmmu][iova][legacy]") {
    EventQueue eq;
    GmmuTLM g("g", &eq);
    g.set_enabled(true);
    g.set_pt_base_lo(0x10000);
    g.set_pt_base_hi(0);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g.set_mem_view(backing.data(), backing.size());

    const uint64_t pte = (0x5000 & ~0xFFFULL) | 1ULL;
    std::memcpy(backing.data() + 0x10000 + 0 * 8, &pte, 8);

    uint64_t pa = 0;
    int rc = g.translate(0x0, 4096, pa);
    REQUIRE(rc == 0);
    REQUIRE(pa == 0x5000);
    REQUIRE(g.is_async_pending() == false);
}
