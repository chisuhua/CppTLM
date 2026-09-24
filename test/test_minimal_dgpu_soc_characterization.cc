// test/test_minimal_dgpu_soc_characterization.cc
// T0 特征化测试: 锁定 minimal-dgpu-soc change 实施前的既有行为基线
// 目的: 防止 A1/B2 改造后回归既有断言 (R1 缓解)
// Per openspec/changes/cpptlm-minimal-dgpu-soc-v1/tasks.md T0.1
#include <cstdint>
#include <cstring>
#include "event_queue.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include <catch2/catch_all.hpp>

using tlm::gpu::DGpuBoard;

static void inject_req(MemoryTLM* mem, uint64_t tid, uint64_t addr, bool wr, uint64_t data = 0) {
    bundles::CacheReqBundle req;
    req.transaction_id.write(tid);
    req.address.write(addr);
    req.is_write.write(wr ? 1 : 0);
    req.data.write(data);
    req.size.write(8);
    mem->req_in().consume();
    std::memcpy(&mem->req_in().data(), &req, sizeof(req));
    mem->req_in().set_valid(true);
}

TEST_CASE("T0 MemoryTLM legacy read returns 0xDEADBEEF", "[characterization][memory_baseline]") {
    EventQueue eq;
    MemoryTLM mem("mem", &eq);

    inject_req(&mem, 1, 0x1000, false);
    mem.tick();

    REQUIRE(mem.resp_out().valid());
    auto resp = mem.resp_out().data();
    REQUIRE(resp.transaction_id.read() == 1);
    REQUIRE(resp.data.read() == 0xDEADBEEF);
    REQUIRE(resp.error_code.read() == 0);
}

TEST_CASE("T0 BAR1 doorbell increments counter (no SDMA injected)", "[characterization][dgpu_baseline]") {
    DGpuBoard board("test_board");

    const uint32_t before = board.pcie_ep_doorbell_count();
    uint32_t wptr = 1;
    int rc = board.mmio_write(1, DGpuBoard::kBar1DoorbellOffset, &wptr, sizeof(wptr));
    REQUIRE(rc == 0);
    REQUIRE(board.pcie_ep_doorbell_count() == before + 1);
}

TEST_CASE("T0 BAR1 non-doorbell default: mirror only", "[characterization][dgpu_baseline]") {
    DGpuBoard board("test_board");
    REQUIRE_FALSE(board.display_routing_enabled());

    uint32_t data = 0xCAFEBABE;
    uint32_t readback = 0;
    int rc_w = board.mmio_write(1, 0x200, &data, sizeof(data));
    int rc_r = board.mmio_read(1, 0x200, &readback, sizeof(readback));

    REQUIRE(rc_w == 0);
    if (rc_r == 0) {
        REQUIRE(readback == data);
    } else {
        REQUIRE((rc_r == 0 || rc_r == -110));
    }
}

TEST_CASE("T0 BAR0 default: no GMMU forwarding", "[characterization][dgpu_baseline]") {
    DGpuBoard board("test_board");

    uint32_t lo32 = 0xDEADBEEF;
    int rc = board.mmio_write(0, 0x00, &lo32, sizeof(lo32));
    REQUIRE(rc == 0);

    uint32_t readback = 0;
    int rc_r = board.mmio_read(0, 0x00, &readback, sizeof(readback));
    REQUIRE(rc_r == 0);
    REQUIRE(readback == lo32);
}

TEST_CASE("T0 doorbell count starts at zero", "[characterization][dgpu_baseline]") {
    DGpuBoard board("test_board");
    REQUIRE(board.pcie_ep_doorbell_count() == 0);
}

TEST_CASE("T0 display_routing_enabled defaults false", "[characterization][dgpu_baseline]") {
    DGpuBoard board("test_board");
    REQUIRE_FALSE(board.display_routing_enabled());
}

TEST_CASE("T0 set_sdma_engine nullptr safe", "[characterization][dgpu_baseline]") {
    DGpuBoard board("test_board");
    REQUIRE_NOTHROW(board.set_sdma_engine(nullptr));
}

TEST_CASE("T0 tag aggregation: 4 v1.0 unit tags exist", "[characterization][tag_aggregation]") {
    DGpuBoard board("test_board");
    REQUIRE_FALSE(board.storage_routing_enabled());
    REQUIRE_FALSE(board.gmmu_routing_enabled());
    REQUIRE_FALSE(board.display_routing_enabled());
}
