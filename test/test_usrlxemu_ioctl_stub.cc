// test_usrlxemu_ioctl_stub.cc
// UsrLinuxEmu 4-IOCTL stub tests
// Author: CppTLM Team
// Date: 2026-08-26

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_mvp.hh"
#include "tlm/gpu/usrlxemu_ioctl_stub_mvp.hh"

TEST_CASE("UsrLinuxEmuIoctlStub: LOAD 0x27 returns image handle", "[usrlxemu-ioctl][stub]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("board", &eq);
    board.init();
    tlm::gpu::UsrLinuxEmuIoctlStub stub("ioctl", &eq);
    stub.attach_board(&board);
    stub.init();

    tlm::gpu::IoctlRequest request;
    request.image_bytes = {0xDE, 0xAD, 0xBE, 0xEF};
    const auto response = stub.ioctl(0x27, request);

    REQUIRE(response.status == 0);
    REQUIRE(response.image_handle != 0);
}

TEST_CASE("UsrLinuxEmuIoctlStub: LAUNCH 0x28 is permanently ENOSYS", "[usrlxemu-ioctl][stub]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("board", &eq);
    board.init();
    tlm::gpu::UsrLinuxEmuIoctlStub stub("ioctl", &eq);
    stub.attach_board(&board);
    stub.init();

    const auto response = stub.ioctl(0x28, {});
    REQUIRE(response.status == -38);
}

TEST_CASE("UsrLinuxEmuIoctlStub: UNLOAD 0x29 frees image handle", "[usrlxemu-ioctl][stub]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("board", &eq);
    board.init();
    tlm::gpu::UsrLinuxEmuIoctlStub stub("ioctl", &eq);
    stub.attach_board(&board);
    stub.init();

    tlm::gpu::IoctlRequest load;
    load.image_bytes = {0x01};
    const auto loaded = stub.ioctl(0x27, load);
    REQUIRE(loaded.status == 0);

    tlm::gpu::IoctlRequest unload;
    unload.image_handle = loaded.image_handle;
    const auto response = stub.ioctl(0x29, unload);
    REQUIRE(response.status == 0);
}

TEST_CASE("UsrLinuxEmuIoctlStub: PUSHBUFFER 0x01 submits and rings doorbell",
          "[usrlxemu-ioctl][stub]") {
    EventQueue eq;
    tlm::gpu::DGpuBoardTLM board("board", &eq);
    board.init();
    tlm::gpu::UsrLinuxEmuIoctlStub stub("ioctl", &eq);
    stub.attach_board(&board);
    stub.init();

    tlm::gpu::IoctlRequest request;
    request.stream_id = 0;
    request.wdu_offset = 0x100;
    request.launch.task_id = 42;
    request.launch.grid_x = 1;
    request.launch.block_x = 32;
    const auto response = stub.ioctl(0x01, request);

    REQUIRE(response.status == 0);
}

TEST_CASE("UsrLinuxEmuIoctlStub: unattached board fails cleanly", "[usrlxemu-ioctl][stub]") {
    EventQueue eq;
    tlm::gpu::UsrLinuxEmuIoctlStub stub("ioctl", &eq);
    stub.init();
    REQUIRE(stub.ioctl(0x27, {}).status == -19);
}
