// test/test_pcie_memory_device_backing.cc
// PcieMemoryDevice backing 测试
// Per openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/tasks.md T3
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/pcie_memory_device.hh"

using tlm::gpu::PcieMemoryDevice;

TEST_CASE("DGpuBoard 通过 EP 路由 BAR 2 到 PcieMemoryDevice", "[pcie-memory][backing]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    std::vector<uint8_t> backing(8192, 0);
    dev.set_backing_view(backing.data(), backing.size());
    uint64_t data = 0xDEADBEEFCAFEBABEULL;
    REQUIRE(dev.memory_write(0x1000, &data, sizeof(data)) == 0);
    uint64_t back = 0;
    REQUIRE(dev.memory_read(0x1000, &back, sizeof(back)) == 0);
    REQUIRE(back == data);
}

TEST_CASE("PcieMemoryDevice backing view injection", "[pcie-memory][backing]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    REQUIRE(dev.has_backing_view() == false);
    std::vector<uint8_t> backing(8192, 0);
    dev.set_backing_view(backing.data(), backing.size());
    REQUIRE(dev.has_backing_view() == true);
    uint32_t small = 0x12345678;
    REQUIRE(dev.memory_write(0, &small, sizeof(small)) == 0);
    uint64_t near_end = 4096;
    uint64_t val64 = 0xDEADBEEF;
    REQUIRE(dev.memory_write(near_end, &val64, sizeof(val64)) == 0);
    uint64_t read_back = 0;
    REQUIRE(dev.memory_read(near_end, &read_back, sizeof(read_back)) == 0);
    REQUIRE(read_back == val64);
}

TEST_CASE("PcieMemoryDevice memory_read out-of-range", "[pcie-memory][backing]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint32_t data = 0xDEAD;
    uint64_t huge_off = 8ULL * 1024 * 1024 * 1024 + 4096;
    // v1.4 B12: 未注入 backing 时返 -ENODEV
    REQUIRE(dev.memory_write(huge_off, &data, sizeof(data)) == -ENODEV);
    REQUIRE(dev.memory_read(huge_off, &data, sizeof(data)) == -ENODEV);
}