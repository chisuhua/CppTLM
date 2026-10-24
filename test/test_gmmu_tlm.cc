// test/test_gmmu_tlm.cc
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "core/module_factory.hh"
#include "event_queue.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/vram_controller_tlm.hh"
#include <catch2/catch_all.hpp>

using ::VramControllerTLM;
using tlm::gpu::GmmuTLM;

static constexpr uint64_t kPageSize = 4096;

TEST_CASE("gmmu: valid PTE translates to paddr", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_mem_view(backing.data(), backing.size());

    g->set_pt_base_lo(0x00010000);
    g->set_pt_base_hi(0x00000000);
    g->set_enabled(true);

    constexpr uint64_t pte = (0x2000ULL & ~0xFFFULL) | 1ULL;
    std::memcpy(backing.data() + 0x10000 + 1 * 8, &pte, 8);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == 0);
    REQUIRE(pa == 0x2000);
}

TEST_CASE("gmmu: disabled returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(0x10000);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: pt_base zero returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_mem_view(backing.data(), backing.size());
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: no backing → async -EAGAIN (v1.8 async PTE walk)", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    // v1.8 H1: 无 mem_view_ → 异步 MasterPort MEM_READ → -EAGAIN
    REQUIRE(rc == -EAGAIN);
    REQUIRE(g->is_async_pending() == true);
    REQUIRE(g->req_out().valid() == true);
}

TEST_CASE("gmmu: invalid PTE returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    uint64_t pte = 0;
    std::memcpy(backing.data() + 0x10000 + 1 * 8, &pte, 8);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: PTE out-of-range returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(8 * 1024, 0);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(0x1000);
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0xFFFFF000ULL, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: cross-page DMA returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(64 * 1024, 0);
    g->set_mem_view(backing.data(), backing.size());
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0xFF0, 4096, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: pt_base_lo + pt_base_hi compose 64-bit pt_base", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    g->set_pt_base_lo(0x00000000);
    g->set_pt_base_hi(0x00000001);
    REQUIRE(g->pt_base() == 0x100000000ULL);

    g->set_pt_base_lo(0x10000);
    g->set_pt_base_hi(0x0);
    REQUIRE(g->pt_base() == 0x10000ULL);
}

TEST_CASE("gmmu: LO-only state after set_pt_base_lo (Inv-4 race lock)", "[gmmu][race]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_mem_view(backing.data(), backing.size());

    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    constexpr uint64_t pte_paddr = 0x5000;
    constexpr uint64_t pte = (pte_paddr & ~0xFFFULL) | 1ULL;
    std::memcpy(backing.data() + 0x10000 + 0 * 8, &pte, 8);

    uint64_t pa = 0;
    int rc = g->translate(0x0, 4096, pa);

    REQUIRE(rc == 0);
    REQUIRE(pa == pte_paddr);
}

TEST_CASE("gmmu: addInstanceForTesting path", "[gmmu][module_factory]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    factory.addInstanceForTesting("g_test", std::make_unique<GmmuTLM>("g_test", &eq));
    auto* obj = factory.getInstance("g_test");
    REQUIRE(obj != nullptr);
    REQUIRE(obj->get_module_type() == "GmmuTLM");
    auto* g = dynamic_cast<GmmuTLM*>(obj);
    REQUIRE(g != nullptr);
}

// ── Change C (cpptlm-gmmu-pte-bus-correction, Gap I): GMMU dedicated client ──
// GMMU PTE fetch 走 VramControllerTLM 专用 functional port (programmatic bridge),
// 不经 pcie_memory / 不经 mesh NoC (per NVIDIA H100 GMMU dedicated memory client 模式)

TEST_CASE("gmmu: set_vram_target 装配 programmatic bridge (Gap I)", "[gmmu][noc-gmmu-bypass]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    auto vram = std::make_unique<VramControllerTLM>("vram", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    vram->set_backing_view(backing.data(), backing.size());
    vram->set_mode(tlm::vram::VramControllerMode::FunctionalZeroDelay);

    // PER-C spec: GMMU 无 backing 注入时 (mem_view_ == nullptr) 走 vram_ctrl_ functional port
    // (不注入 mem_view_, 模拟 async PTE walk 经 VramControllerTLM 专用通道)
    g->set_vram_target(vram.get());
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    // PTE[1] 写入 vram backing (pt_base=0x10000, idx=1 → pte_addr=0x10008)
    constexpr uint64_t pte = (0x2000ULL & ~0xFFFULL) | 1ULL;
    std::memcpy(backing.data() + 0x10000 + 1 * 8, &pte, 8);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    // PER-C: 有 vram_ctrl_ 注入 → 同步走 handle_functional_request, 直接返回 paddr (不 -EAGAIN)
    REQUIRE(rc == 0);
    REQUIRE(pa == 0x2000);
}

TEST_CASE("gmmu: vram_ctrl 缺 backing → translate -ENODEV 语义 (Gap I)",
          "[gmmu][noc-gmmu-bypass]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    auto vram = std::make_unique<VramControllerTLM>("vram", &eq);
    // 不注入 backing → handle_functional_request 返 -ENODEV
    vram->set_mode(tlm::vram::VramControllerMode::FunctionalZeroDelay);

    g->set_vram_target(vram.get());
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    // vram_ctrl backing 缺失 → 翻译失败 -EIO (与 sync path 语义一致)
    REQUIRE(rc == -EIO);
    REQUIRE(g->is_async_pending() == false);
}

TEST_CASE("gmmu: vram_ctrl 无效 PTE → -EIO (Gap I)", "[gmmu][noc-gmmu-bypass]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    auto vram = std::make_unique<VramControllerTLM>("vram", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    vram->set_backing_view(backing.data(), backing.size());
    vram->set_mode(tlm::vram::VramControllerMode::FunctionalZeroDelay);

    g->set_vram_target(vram.get());
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    // PTE[1] = 0 (invalid, bit0=0)
    uint64_t pte = 0;
    std::memcpy(backing.data() + 0x10000 + 1 * 8, &pte, 8);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: translate 经 vram_ctrl 跨页检查仍生效 (Gap I)", "[gmmu][noc-gmmu-bypass]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    auto vram = std::make_unique<VramControllerTLM>("vram", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    vram->set_backing_view(backing.data(), backing.size());
    vram->set_mode(tlm::vram::VramControllerMode::FunctionalZeroDelay);

    g->set_vram_target(vram.get());
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0xFF0, 4096, pa); // 跨页
    REQUIRE(rc == -EIO);
}