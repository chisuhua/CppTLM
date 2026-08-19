// test_dgpu_bar.cc
// DGpuBar BAR0 MMIO 寄存器读写测试
// Author: CppTLM Team
// Date: 2026-08-19

#include "catch_amalgamated.hpp"
#include "tlm/gpu/dgpu_bar.hh"

TEST_CASE("DGpuBar: BAR0 regs read/write round-trip", "[dgpu-bar]") {
    tlm::gpu::DGpuBar bar;
    bar.init();

    REQUIRE(bar.read_reg(0x00) == 0x10DE); // VENDOR_ID (DWORD at byte 0)
    bar.write_reg(0x04, 0x1234);           // DEVICE_ID aligned to next DWORD at byte 4
    REQUIRE(bar.read_reg(0x04) == 0x1234);

    bar.write_reg(0x10, 0xDEADBEEF);
    REQUIRE(bar.read_reg(0x10) == 0xDEADBEEF);

    REQUIRE_THROWS(bar.read_reg(0x02)); // unaligned offset throws
}

TEST_CASE("DGpuBar: VRAM backing memory accessible", "[dgpu-bar]") {
    tlm::gpu::DGpuBar bar;
    bar.init();

    REQUIRE(bar.vram_base() != nullptr);
    REQUIRE(bar.vram_size() == 256 * 1024 * 1024); // 256 MB

    bar.shutdown();
    REQUIRE(bar.vram_base() == nullptr);
    REQUIRE(bar.vram_size() == 0);
}
