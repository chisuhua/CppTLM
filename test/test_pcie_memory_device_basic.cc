// test/test_pcie_memory_device_basic.cc
// PcieMemoryDevice 单元测试
// Per openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/tasks.md T1.1
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/pcie_memory_device.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

using tlm::gpu::PcieMemoryDevice;

TEST_CASE("PcieMemoryDevice BAR 0 寄存器 round-trip (scratch RW)", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint32_t size_lo = 0x12345678;
    REQUIRE(dev.mmio_write(PcieMemoryDevice::kRegScratch, &size_lo, sizeof(size_lo)) == 0);
    uint32_t read_val = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegScratch, &read_val, sizeof(read_val)) == 0);
    REQUIRE(read_val == size_lo);
}

TEST_CASE("PcieMemoryDevice device_id RO 寄存器", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint32_t val = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegDeviceIdentity, &val, sizeof(val)) == 0);
    uint16_t vid = static_cast<uint16_t>(val & 0xFFFF);
    uint16_t did = static_cast<uint16_t>((val >> 16) & 0xFFFF);
    REQUIRE(vid == PcieMemoryDevice::kVendorId);
    REQUIRE(did == PcieMemoryDevice::kDeviceId);
}

TEST_CASE("PcieMemoryDevice RO 寄存器写静默忽略", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint32_t val = 0xDEADBEEF;
    REQUIRE(dev.mmio_write(PcieMemoryDevice::kRegDeviceIdentity, &val, sizeof(val)) == 0);
    uint32_t read_val = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegDeviceIdentity, &read_val, sizeof(read_val)) == 0);
    uint16_t vid = static_cast<uint16_t>(read_val & 0xFFFF);
    REQUIRE(vid == PcieMemoryDevice::kVendorId);
}

TEST_CASE("PcieMemoryDevice BAR 0 unaligned access", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint8_t byte = 0xAB;
    REQUIRE(dev.mmio_write(0x15, &byte, sizeof(byte)) == 0);
    uint8_t read_byte = 0;
    REQUIRE(dev.mmio_read(0x15, &read_byte, sizeof(read_byte)) == 0);
    REQUIRE(read_byte == byte);
}

TEST_CASE("PcieMemoryDevice BAR 0 out-of-range", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint32_t val = 0x12345678;
    REQUIRE(dev.mmio_write(0x1000, &val, sizeof(val)) == -EINVAL);
    REQUIRE(dev.mmio_read(0x1000, &val, sizeof(val)) == -EINVAL);
}

TEST_CASE("PcieMemoryDevice BAR 0 offset+len > 4096", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint32_t val = 0x12345678;
    REQUIRE(dev.mmio_write(0xFFFC, &val, sizeof(val)) == -EINVAL);
    REQUIRE(dev.mmio_write(0x1000 - 4, &val, sizeof(val)) == 0);
    REQUIRE(dev.mmio_write(0x1000 - 1, &val, sizeof(val)) == -EINVAL);
}

TEST_CASE("PcieMemoryDevice memory_read/write round-trip", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    std::vector<uint8_t> backing(8192, 0);
    dev.set_backing_view(backing.data(), backing.size());
    uint64_t data = 0xDEADBEEFCAFEBABEULL;
    REQUIRE(dev.memory_write(0x1000, &data, sizeof(data)) == 0);
    uint64_t back = 0;
    REQUIRE(dev.memory_read(0x1000, &back, sizeof(back)) == 0);
    REQUIRE(back == data);
}

TEST_CASE("PcieMemoryDevice memory_read/write out-of-range (no backing → ENODEV)", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint64_t data = 0xDEADBEEFCAFEBABEULL;
    uint64_t huge_offset = (1ULL << 40);
    // v1.4 B12: 未注入 backing 时返 -ENODEV (非旧版 -EINVAL)
    REQUIRE(dev.memory_write(huge_offset, &data, sizeof(data)) == -ENODEV);
    REQUIRE(dev.memory_read(huge_offset, &data, sizeof(data)) == -ENODEV);
}

TEST_CASE("PcieMemoryDevice tick 推进 cycle_counter", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    REQUIRE(dev.cycle_counter() == 0);
    dev.tick();
    REQUIRE(dev.cycle_counter() == 1);
    for (int i = 0; i < 1023; ++i) {
        dev.tick();
    }
    REQUIRE(dev.cycle_counter() == 1024);
}

TEST_CASE("PcieMemoryDevice has_backing_view 初为 false (no injection)", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    REQUIRE(dev.has_backing_view() == false);
}

TEST_CASE("PcieMemoryDevice set_backing_view 注入后 memory_write/read 正常", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    std::vector<uint8_t> backing(8192, 0);
    dev.set_backing_view(backing.data(), backing.size());
    REQUIRE(dev.has_backing_view() == true);
    uint64_t data = 0xCAFEBABEULL;
    REQUIRE(dev.memory_write(0, &data, sizeof(data)) == 0);
    uint64_t back = 0;
    REQUIRE(dev.memory_read(0, &back, sizeof(back)) == 0);
    REQUIRE(back == data);
}

TEST_CASE("PcieMemoryDevice STATUS 默认 ready", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    uint8_t status = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieMemoryDevice::kStatusReady) != 0);
}

TEST_CASE("PcieMemoryDevice EP 注入: set_memory_device 后 has_memory_device true", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    tlm::pcie::PcieEndpointIP ep("test_ep", nullptr);
    // Phase 3 T1.2: EP 不构造 memory_device_, 由 board 注入 (N12)
    REQUIRE(ep.has_memory_device() == false);
    ep.set_memory_device(&dev);
    REQUIRE(ep.has_memory_device() == true);
    REQUIRE(&ep.memory_device() == &dev);
}

TEST_CASE("PcieMemoryDevice EP tick 不推进 memory_device cycle_counter (N8)", "[pcie-memory][basic]") {
    EventQueue eq_dev; PcieMemoryDevice dev("dev", &eq_dev);
    tlm::pcie::PcieEndpointIP ep("test_ep", nullptr);
    ep.set_memory_device(&dev);
    REQUIRE(dev.cycle_counter() == 0);
    for (int i = 0; i < 1024; ++i) {
        ep.tick();
    }
    // Phase 3 T1.6 (N8): EP 不双 tick — memory_device 由 ModuleFactory 独立调度
    REQUIRE(dev.cycle_counter() == 0);
}