// test/test_gmmu_tlm.cc
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "event_queue.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include <catch2/catch_all.hpp>

using tlm::gpu::GmmuTLM;

static constexpr uint64_t kPageSize = 4096;

TEST_CASE("gmmu: valid PTE translates to paddr", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_backing(backing.data(), backing.size());

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
    g->set_backing(backing.data(), backing.size());
    g->set_pt_base_lo(0x10000);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: pt_base zero returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_backing(backing.data(), backing.size());
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: backing null returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    g->set_pt_base_lo(0x10000);
    g->set_enabled(true);

    uint64_t pa = 0;
    int rc = g->translate(0x1000, kPageSize, pa);
    REQUIRE(rc == -EIO);
}

TEST_CASE("gmmu: invalid PTE returns -EIO", "[gmmu]") {
    EventQueue eq;
    auto g = std::make_unique<GmmuTLM>("g", &eq);
    std::vector<uint8_t> backing(128 * 1024, 0);
    g->set_backing(backing.data(), backing.size());
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
    g->set_backing(backing.data(), backing.size());
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
    g->set_backing(backing.data(), backing.size());
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
    g->set_backing(backing.data(), backing.size());

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
    factory.addInstanceForTesting("g_test",
                                  std::make_unique<GmmuTLM>("g_test", &eq));
    auto* obj = factory.getInstance("g_test");
    REQUIRE(obj != nullptr);
    REQUIRE(obj->get_module_type() == "GmmuTLM");
    auto* g = dynamic_cast<GmmuTLM*>(obj);
    REQUIRE(g != nullptr);
}