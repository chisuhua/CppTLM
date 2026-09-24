// test/test_dgpu_board_framebuffer.cc
// DGpuBoard framebuffer_ + backdoor 单元测试 (Phase A2 of openspec/changes/cpptlm-minimal-dgpu-soc-v1)
#include <cstdint>
#include <cstring>
#include <vector>
#include "event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include <catch2/catch_all.hpp>

using tlm::gpu::DGpuBoard;

TEST_CASE("dgpu_framebuffer: backdoor read/write roundtrip", "[dgpu_framebuffer]") {
    DGpuBoard board("test_board");
    constexpr uint64_t fb_size = 64 * 1024;
    std::vector<uint8_t> fb(fb_size, 0);
    board.attach_framebuffer_for_testing(fb.data(), fb_size);

    std::vector<uint8_t> data(64);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i & 0xFF);
    REQUIRE(board.backdoor_write(0x100, data.data(), data.size()) == 0);

    std::vector<uint8_t> out(64);
    REQUIRE(board.backdoor_read(0x100, out.data(), out.size()) == 0);
    REQUIRE(out == data);
    REQUIRE(fb[0x100] == 0);
    REQUIRE(fb[0x13F] == 0x3F);
}

TEST_CASE("dgpu_framebuffer: backdoor out-of-range returns -EINVAL", "[dgpu_framebuffer]") {
    DGpuBoard board("test_board");
    constexpr uint64_t fb_size = 64 * 1024;
    std::vector<uint8_t> fb(fb_size, 0);
    board.attach_framebuffer_for_testing(fb.data(), fb_size);

    std::vector<uint8_t> data(16);
    REQUIRE(board.backdoor_write(fb_size - 8, data.data(), data.size()) == -EINVAL);
    REQUIRE(board.backdoor_write(fb_size + 8, data.data(), data.size()) == -EINVAL);
}

TEST_CASE("dgpu_framebuffer: vram_segments_ fallback when framebuffer empty", "[dgpu_framebuffer]") {
    DGpuBoard board("test_board");
    std::vector<uint8_t> data(32, 0xAB);

    REQUIRE(board.backdoor_write(0x100, data.data(), data.size()) == 0);

    std::vector<uint8_t> out(32, 0);
    REQUIRE(board.backdoor_read(0x100, out.data(), out.size()) == 0);
    REQUIRE(out == data);
}

TEST_CASE("dgpu_framebuffer: storage_routing_enabled_ default false", "[dgpu_framebuffer]") {
    DGpuBoard board("test_board");
    REQUIRE_FALSE(board.storage_routing_enabled());
    REQUIRE_FALSE(board.gmmu_routing_enabled());
}

TEST_CASE("dgpu_framebuffer: BAR1 mmio_write routes to framebuffer when flag on", "[dgpu_framebuffer][bar1_routing]") {
    DGpuBoard board("test_board");
    constexpr uint64_t fb_size = 64 * 1024;
    std::vector<uint8_t> fb(fb_size, 0);
    board.attach_framebuffer_for_testing(fb.data(), fb_size);
    board.set_storage_routing_enabled(true);

    constexpr uint64_t off = 0x800;
    uint32_t val = 0xDEADBEEF;
    int rc = board.mmio_write(1, off, &val, sizeof(val));
    REQUIRE(rc == 0);

    uint32_t readback = 0;
    int rc_r = board.mmio_read(1, off, &readback, sizeof(readback));
    REQUIRE(rc_r == 0);
    REQUIRE(readback == val);

    uint8_t fb_byte = 0;
    REQUIRE(board.backdoor_read(off, &fb_byte, 1) == 0);
    REQUIRE(fb_byte == (val & 0xFF));
}

TEST_CASE("dgpu_framebuffer: framebuffer_size_=0 falls back to vram_segments_", "[dgpu_framebuffer][boundary]") {
    DGpuBoard board("test_board");
    board.set_storage_routing_enabled(true);
    board.attach_framebuffer_for_testing(nullptr, 0);

    std::vector<uint8_t> data(32, 0xAB);
    REQUIRE(board.backdoor_write(0x100, data.data(), data.size()) == 0);

    std::vector<uint8_t> out(32, 0);
    REQUIRE(board.backdoor_read(0x100, out.data(), out.size()) == 0);
    REQUIRE(out == data);
}

TEST_CASE("dgpu_framebuffer: BAR1 non-4/8-byte len passes through", "[dgpu_framebuffer][boundary]") {
    DGpuBoard board("test_board");
    constexpr uint64_t fb_size = 64 * 1024;
    std::vector<uint8_t> fb(fb_size, 0);
    board.attach_framebuffer_for_testing(fb.data(), fb_size);
    board.set_storage_routing_enabled(true);

    constexpr uint64_t off = 0x200;
    uint8_t val1 = 0xA1;
    REQUIRE(board.mmio_write(1, off, &val1, 1) == 0);
    uint8_t out1 = 0;
    REQUIRE(board.mmio_read(1, off, &out1, 1) == 0);
    REQUIRE(out1 == val1);

    uint16_t val2 = 0xB2B3;
    REQUIRE(board.mmio_write(1, off + 8, &val2, 2) == 0);
    uint16_t out2 = 0;
    REQUIRE(board.mmio_read(1, off + 8, &out2, 2) == 0);
    REQUIRE(out2 == val2);

    uint32_t val3 = 0xC4C5C6C7;
    REQUIRE(board.mmio_write(1, off + 16, &val3, 3) == 0);
    uint8_t out3[3] = {0, 0, 0};
    REQUIRE(board.mmio_read(1, off + 16, out3, 3) == 0);
    REQUIRE(out3[0] == (val3 & 0xFF));
    REQUIRE(out3[1] == ((val3 >> 8) & 0xFF));
    REQUIRE(out3[2] == ((val3 >> 16) & 0xFF));
}