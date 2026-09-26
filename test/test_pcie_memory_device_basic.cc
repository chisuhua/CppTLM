// test/test_pcie_memory_device_basic.cc
// PcieMemoryDevice 单元测试
// Per openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/tasks.md T1.1
#include "catch_amalgamated.hpp"
#include "tlm/gpu/pcie_memory_device.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

using tlm::gpu::PcieMemoryDevice;

TEST_CASE("PcieMemoryDevice BAR 0 寄存器 round-trip", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint32_t size_lo = 0x12345678;
    REQUIRE(dev.mmio_write(PcieMemoryDevice::kRegMemSizeLo, &size_lo, sizeof(size_lo)) == 0);
    uint32_t read_val = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegMemSizeLo, &read_val, sizeof(read_val)) == 0);
    REQUIRE(read_val == size_lo);
}

TEST_CASE("PcieMemoryDevice device_id RO 寄存器", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint32_t val = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegDeviceIdentity, &val, sizeof(val)) == 0);
    uint16_t vid = static_cast<uint16_t>(val & 0xFFFF);
    uint16_t did = static_cast<uint16_t>((val >> 16) & 0xFFFF);
    REQUIRE(vid == PcieMemoryDevice::kVendorId);
    REQUIRE(did == PcieMemoryDevice::kDeviceId);
}

TEST_CASE("PcieMemoryDevice RO 寄存器写静默忽略", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint32_t val = 0xDEADBEEF;
    REQUIRE(dev.mmio_write(PcieMemoryDevice::kRegDeviceIdentity, &val, sizeof(val)) == 0);
    uint32_t read_val = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegDeviceIdentity, &read_val, sizeof(read_val)) == 0);
    uint16_t vid = static_cast<uint16_t>(read_val & 0xFFFF);
    REQUIRE(vid == PcieMemoryDevice::kVendorId);
}

TEST_CASE("PcieMemoryDevice BAR 0 unaligned access", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint8_t byte = 0xAB;
    REQUIRE(dev.mmio_write(0x15, &byte, sizeof(byte)) == 0);
    uint8_t read_byte = 0;
    REQUIRE(dev.mmio_read(0x15, &read_byte, sizeof(read_byte)) == 0);
    REQUIRE(read_byte == byte);
}

TEST_CASE("PcieMemoryDevice BAR 0 out-of-range", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint32_t val = 0x12345678;
    REQUIRE(dev.mmio_write(0x1000, &val, sizeof(val)) == -EINVAL);
    REQUIRE(dev.mmio_read(0x1000, &val, sizeof(val)) == -EINVAL);
}

TEST_CASE("PcieMemoryDevice BAR 0 offset+len > 4096", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint32_t val = 0x12345678;
    REQUIRE(dev.mmio_write(0xFFFC, &val, sizeof(val)) == -EINVAL);
    REQUIRE(dev.mmio_write(0x1000 - 4, &val, sizeof(val)) == 0);
    REQUIRE(dev.mmio_write(0x1000 - 1, &val, sizeof(val)) == -EINVAL);
}

TEST_CASE("PcieMemoryDevice memory_read/write round-trip", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint64_t data = 0xDEADBEEFCAFEBABEULL;
    REQUIRE(dev.memory_write(0x1000, &data, sizeof(data)) == 0);
    uint64_t back = 0;
    REQUIRE(dev.memory_read(0x1000, &back, sizeof(back)) == 0);
    REQUIRE(back == data);
}

TEST_CASE("PcieMemoryDevice memory_read/write out-of-range", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint64_t data = 0xDEADBEEFCAFEBABEULL;
    uint64_t huge_offset = (1ULL << 40);
    REQUIRE(dev.memory_write(huge_offset, &data, sizeof(data)) == -EINVAL);
    REQUIRE(dev.memory_read(huge_offset, &data, sizeof(data)) == -EINVAL);
}

TEST_CASE("PcieMemoryDevice tick 推进 cycle_counter", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    REQUIRE(dev.cycle_counter() == 0);
    dev.tick();
    REQUIRE(dev.cycle_counter() == 1);
    for (int i = 0; i < 1023; ++i) {
        dev.tick();
    }
    REQUIRE(dev.cycle_counter() == 1024);
}

TEST_CASE("PcieMemoryDevice has_memory_backing 初为 false", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    REQUIRE(dev.has_memory_backing() == false);
}

TEST_CASE("PcieMemoryDevice memory_write 触发 lazy alloc", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint64_t data = 0xCAFEBABEULL;
    REQUIRE(dev.memory_write(0, &data, sizeof(data)) == 0);
    REQUIRE(dev.has_memory_backing() == true);
    uint64_t back = 0;
    REQUIRE(dev.memory_read(0, &back, sizeof(back)) == 0);
    REQUIRE(back == data);
}

TEST_CASE("PcieMemoryDevice STATUS 默认 ready", "[pcie-memory][basic]") {
    PcieMemoryDevice dev;
    uint8_t status = 0;
    REQUIRE(dev.mmio_read(PcieMemoryDevice::kRegStatus, &status, sizeof(status)) == 0);
    REQUIRE((status & PcieMemoryDevice::kStatusReady) != 0);
}

TEST_CASE("PcieMemoryDevice EP 注入: has_memory_device true", "[pcie-memory][basic]") {
    tlm::pcie::PcieEndpointIP ep("test_ep", nullptr);
    REQUIRE(ep.has_memory_device() == true);
}

TEST_CASE("PcieMemoryDevice EP tick 推进 memory_device cycle_counter", "[pcie-memory][basic]") {
    tlm::pcie::PcieEndpointIP ep("test_ep", nullptr);
    REQUIRE(ep.memory_device().cycle_counter() == 0);
    for (int i = 0; i < 1024; ++i) {
        ep.tick();
    }
    REQUIRE(ep.memory_device().cycle_counter() == 1024);
}