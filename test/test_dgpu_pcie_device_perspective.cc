// test_dgpu_pcie_device_perspective.cc
// PCIe driver perspective tests for the CppTLM dGPU SOC DUT
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "tlm/gpu/dgpu_board_mvp.hh"
#include "tlm/gpu/usrlxemu_ioctl_stub_mvp.hh"
#include "core/event_queue.hh"
#include <array>
#include <cstdint>
#include <cstring>

namespace {
    constexpr uint32_t GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH = 0x01;
    constexpr uint32_t GPU_IOCTL_LOAD_KERNEL_MODULE = 0x27;
    constexpr uint32_t GPU_IOCTL_LAUNCH_KERNEL_MODULE = 0x28;
    constexpr uint32_t GPU_IOCTL_UNLOAD_KERNEL_MODULE = 0x29;
    constexpr uint32_t GPU_REG_GPFIFO_PUT = 0x0000;
    constexpr uint32_t GPU_REG_DOORBELL = 0x0014;
}

TEST_CASE("PCIe driver perspective: BAR0 doorbell write wakes CP", "[pcie][dGPU][s2]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("pcie_dgpu", &eq);
    board.init();

    REQUIRE(board.cp_is_idle());
    board.write_reg(GPU_REG_DOORBELL, 0x00000001);
    REQUIRE_FALSE(board.cp_is_idle());
    REQUIRE(board.doorbell_sq_tail(0) == 0);
}

TEST_CASE("PCIe driver perspective: BAR1 VRAM write/read round trip", "[pcie][dGPU][s2]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("pcie_dgpu", &eq);
    board.init();

    REQUIRE(board.bar0_size() == 0x10000);
    REQUIRE(board.bar1_size() == 256ULL * 1024ULL * 1024ULL);

    const std::array<uint8_t, 16> image = {
        0x50, 0x54, 0x58, 0x49, 0x52, 0x00, 0x01, 0x00,
        0xDE, 0xAD, 0xBE, 0xEF, 0x11, 0x22, 0x33, 0x44};
    std::array<uint8_t, image.size()> readback{};

    REQUIRE(board.write_vram(0x2000, image.data(), image.size()) == 0);
    REQUIRE(board.read_vram(0x2000, readback.data(), readback.size()) == 0);
    REQUIRE(readback == image);
}

TEST_CASE("PCIe driver perspective: IOCTL 0x27/0x28/0x29 ABI path", "[pcie][dGPU][ioctl][s2]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("pcie_dgpu", &eq);
    board.init();
    tlm::gpu::UsrLinuxEmuIoctlStub ioctl("usr_linux_emu", &eq);
    ioctl.attach_board(&board);
    ioctl.init();

    tlm::gpu::IoctlRequest load;
    load.image_bytes = {0x50, 0x54, 0x58, 0x49, 0x01};
    const auto loaded = ioctl.ioctl(GPU_IOCTL_LOAD_KERNEL_MODULE, load);
    REQUIRE(loaded.status == 0);
    REQUIRE(loaded.image_handle != 0);

    const auto launch = ioctl.ioctl(GPU_IOCTL_LAUNCH_KERNEL_MODULE, {});
    REQUIRE(launch.status == -38);

    tlm::gpu::IoctlRequest unload;
    unload.image_handle = loaded.image_handle;
    REQUIRE(ioctl.ioctl(GPU_IOCTL_UNLOAD_KERNEL_MODULE, unload).status == 0);
}

TEST_CASE("PCIe driver perspective: PUSHBUFFER submit then MMIO doorbell", "[pcie][dGPU][ioctl][s2]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("pcie_dgpu", &eq);
    board.init();
    tlm::gpu::UsrLinuxEmuIoctlStub ioctl("usr_linux_emu", &eq);
    ioctl.attach_board(&board);
    ioctl.init();

    tlm::gpu::IoctlRequest push;
    push.stream_id = 0;
    push.wdu_offset = 0x100;
    push.launch.task_id = 7;
    push.launch.grid_x = 1;
    push.launch.block_x = 32;

    REQUIRE(ioctl.ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, push).status == 0);
    REQUIRE(board.sq_pending_count() == 1);
    REQUIRE_FALSE(board.cp_is_idle());

    board.tick();
    REQUIRE(board.sq_pending_count() == 0);
    REQUIRE(board.sq_active_count() == 1);
}

TEST_CASE("PCIe driver perspective: invalid BAR1 DMA bounds fail with errno", "[pcie][dGPU][s2]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("pcie_dgpu", &eq);
    board.init();
    uint8_t byte = 0xFF;

    REQUIRE(board.write_vram(board.bar1_size(), &byte, 1) == -22);
    REQUIRE(board.read_vram(board.bar1_size(), &byte, 1) == -22);
    REQUIRE(board.write_vram(0, nullptr, 1) == -22);
}

TEST_CASE("PCIe driver perspective: GPFIFO PUT register is an MMIO address", "[pcie][dGPU][s2]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("pcie_dgpu", &eq);
    board.init();

    board.write_reg(GPU_REG_GPFIFO_PUT, 4);
    REQUIRE(board.cp_is_idle());
    REQUIRE(board.sq_inflight_count() == 0);
}
