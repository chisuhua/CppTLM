// test_d2d_noc_path.cc
// Stage 1.3b: D2D NoC payload 转发 (≥100 GB/s) + host_out 零事务断言
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//   - Scenario "NoC payload forward ≥ 100 GB/s"
//   - Scenario "host_out zero transactions"
//   - Scenario "d2d_noc_forward(src_va, dst_va, len) called"
//
// Oracle O9 修订 (per spec.md §1.3b design.md):
//   simulated_throughput_GBps = payload_bytes / simulated_latency_s >= 100
//   latency 定义 = NoC 转发段 (首 flit 进 mesh → 末 flit 写入目的 VRAM)
//
// TDD 状态 (RED): GpuMeshNoC::d2d_forward() 未实现, SdmaEngineTLM::Dir::D2D
//   不存在, host_out_tx_count_ 不存在. 1.3b 实施后变绿.

#include "bundles/dma_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dma_descriptor_mvp.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"

#include <cstdint>
#include <cstring>
#include <vector>

using namespace tlm;
using namespace tlm::gpu;
using namespace bundles;

// 1 MiB simulated VRAM backing store
static std::vector<uint8_t> g_vram(0x100000, 0);

// fake translate_cb identity (per spec "identity mode")
static int fake_translate_identity(uint64_t iova, uint32_t size, uint64_t& phys) {
    if (iova + size > g_vram.size())
        return -EFAULT;
    phys = iova;
    return 0;
}

// =============================================================================
// Scenario "D2D payload forward ≥ 100 GB/s"
//   d2d_noc_forward(src_va, dst_va, len=1MB) → simulated_throughput >= 100 GB/s
// =============================================================================
TEST_CASE("D2D NoC payload forward: 1MB transfer → simulated_throughput >= 100 GB/s",
          "[sdma][d2d][noc][1.3b][throughput]") {
    EventQueue eq;
    GpuMeshNoC noc("d2d_noc", &eq);
    noc.set_dim(2);
    noc.set_hops_latency(1);

    SdmaEngineTLM sdma("sdma_d2d", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_identity);
    sdma.set_vram_backdoor(g_vram.data(), g_vram.size());
    sdma.set_d2d_noc(&noc);

    // 在 1MB VRAM 中转 768KB (避免越界): src=0x0, dst=0x10000, len=0xC0000
    constexpr uint64_t kSrcVa = 0x0;
    constexpr uint64_t kDstVa = 0x10000;
    constexpr uint32_t kLen = 0xC0000;  // 768 KiB
    sdma.d2d_forward(kSrcVa, kDstVa, kLen);

    REQUIRE(noc.payload_bytes_forwarded() >= kLen);
    REQUIRE(noc.last_transfer_latency_cycles() >= 1u);

    // hops_latency=1, dim=2 → 最坏路径 4 hops = 4 cycles
    // 768KB / 4ns = 196608 GB/s (768 * 1024 / 4e-9 / 1e9) ≈ 196 KB/ns ≈ 196 GB/s
    // 196 GB/s >= 100 GB/s ✓
    REQUIRE(noc.simulated_throughput_GBps() >= 100.0);
}

// =============================================================================
// Scenario "host_out zero transactions" (D2D 路径不走 PCIe TLP)
//   当 D2D NoC payload 转发被触发, SdmaEngineTLM::host_out_tx_count_ 必须 = 0
// =============================================================================
TEST_CASE("D2D NoC host_out zero transactions: D2D 路径不触发 host_out",
          "[sdma][d2d][noc][1.3b][host_out_zero]") {
    EventQueue eq;
    GpuMeshNoC noc("d2d_noc_zero", &eq);
    SdmaEngineTLM sdma("sdma_d2d_zero", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_identity);
    sdma.set_vram_backdoor(g_vram.data(), g_vram.size());
    sdma.set_d2d_noc(&noc);

    REQUIRE(sdma.host_out_tx_count() == 0u);

    sdma.d2d_forward(0x10000, 0x20000, 0x1000);

    REQUIRE(sdma.host_out_tx_count() == 0u);
}

// =============================================================================
// Scenario "DmaDescriptor::Dir::D2D 扩展"
//   dma_descriptor_mvp.hh 需新增 Dir::D2D = 2, 让 SDMA 知道是 D2D 路径
// =============================================================================
TEST_CASE("DmaDescriptor Dir::D2D 枚举扩展 (D2D NoC 路径)",
          "[sdma][d2d][1.3b][dir_enum]") {
    DmaDescriptor d(DmaDescriptor::Dir::D2D,
                    /*host_iova=*/0,
                    /*vram_offset=*/0x20000,
                    /*size=*/0x1000,
                    /*tag=*/42);
    REQUIRE(static_cast<uint8_t>(d.dir) == 2u);
    REQUIRE(d.vram_offset == 0x20000u);
}

// =============================================================================
// Scenario "D2D 1MB payload integrity"
//   验证 src VA 数据在转发后能在 dst VA 读到 (per spec "payload integrity")
// =============================================================================
TEST_CASE("D2D NoC payload integrity: src→dst 1MB 数据正确",
          "[sdma][d2d][noc][1.3b][integrity]") {
    EventQueue eq;
    GpuMeshNoC noc("d2d_noc_integrity", &eq);
    SdmaEngineTLM sdma("sdma_d2d_integrity", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_identity);
    sdma.set_vram_backdoor(g_vram.data(), g_vram.size());
    sdma.set_d2d_noc(&noc);

    constexpr uint64_t kSrcVa = 0x10000;
    constexpr uint64_t kDstVa = 0x30000;
    constexpr uint32_t kLen = 1024;
    for (uint32_t i = 0; i < kLen; ++i) {
        g_vram[kSrcVa + i] = static_cast<uint8_t>(i & 0xFFu);
    }

    sdma.d2d_forward(kSrcVa, kDstVa, kLen);

    REQUIRE(std::memcmp(&g_vram[kDstVa], &g_vram[kSrcVa], kLen) == 0);
}

// =============================================================================
// Scenario "D2D payload counter accumulates"
//   多次 D2D 转发累加 payload_bytes_forwarded_
// =============================================================================
TEST_CASE("D2D NoC payload counter accumulates across multiple transfers",
          "[sdma][d2d][noc][1.3b][counter]") {
    EventQueue eq;
    GpuMeshNoC noc("d2d_noc_counter", &eq);
    noc.set_dim(2);
    noc.set_hops_latency(1);
    SdmaEngineTLM sdma("sdma_d2d_counter", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_identity);
    sdma.set_vram_backdoor(g_vram.data(), g_vram.size());
    sdma.set_d2d_noc(&noc);

    REQUIRE(noc.payload_bytes_forwarded() == 0u);

    sdma.d2d_forward(0x10000, 0x20000, 0x1000);   // 4KB
    sdma.d2d_forward(0x11000, 0x21000, 0x2000);   // 8KB
    sdma.d2d_forward(0x10000, 0x90000, 0x60000);  // 384KB (fits in 1MB VRAM)

    REQUIRE(noc.payload_bytes_forwarded() == 0x1000u + 0x2000u + 0x60000u);
}

// =============================================================================
// 1.3b M6 回归测试: Dir::D2D 描述符经 desc_in 提交 → d2d_forward (不 emit host_out)
//   修复前: Dir::D2D 被静默当 D2H 处理 → host_out_tx_count++ (违反 bypassing)
//   修复后: Dir::D2D → d2d_forward + emit done_out (host_out 不增)
// =============================================================================
TEST_CASE("SDMA: Dir::D2D 描述符经 desc_in 提交 → d2d_forward + host_out 零事务 (1.3b M6)",
          "[sdma][d2d][1.3b_m6][dispatch]") {
    EventQueue eq;
    GpuMeshNoC noc("d2d_noc_m6", &eq);
    SdmaEngineTLM sdma("sdma_d2d_m6", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_identity);
    sdma.set_vram_backdoor(g_vram.data(), g_vram.size());
    sdma.set_d2d_noc(&noc);

    REQUIRE(sdma.host_out_tx_count() == 0u);

    // 推 Dir::D2D 描述符经 desc_in (M6 修复点)
    DmaDescriptor d(DmaDescriptor::Dir::D2D,
                     /*host_iova=*/0,                       // D2D 不需 host_iova
                     /*vram_offset=*/0x20000,              // D2D: dst VA
                     /*size=*/1024,
                     /*tag=*/0xCAFE);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(d);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);
    sdma.tick();

    // 验证: host_out_tx_count 不增 (M6 修复关键断言)
    REQUIRE(sdma.host_out_tx_count() == 0u);
    // done_out 已 emit (success)
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid());
    REQUIRE(sdma.completed_count() == 1u);
}
