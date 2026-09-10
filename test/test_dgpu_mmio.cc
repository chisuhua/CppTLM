// test/test_dgpu_mmio.cc
// Stage 1.1 task 1.1.3 (修复 #5): DGpuBoard::mmio_read 真实数据回填
//   roundtrip → 0 + buf 匹配写入数据 (经 drain_injection_queue 响应 payload, 非 TODO T-bs-3c 占位)
//   timeout   → -110 (ETIMEDOUT), buf 不变
//   null buf  → -EINVAL
// Per openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes
// (Oracle R7 裁决: 成功返 0, 不返 byte count)
#include <cerrno>
#include <cstdint>
#include <vector>
#include <catch_amalgamated.hpp>
#include "tlm/gpu/dgpu_board_shell.hh"

using namespace tlm::gpu;

TEST_CASE("DGpuBoard: mmio_write→mmio_read roundtrip returns 0 with real data copy",
          "[dgpu][shell][mmio]") {
    DGpuBoard board("test_board");
    board.init(); // sim 线程负责 drain inject_q_

    std::vector<uint8_t> write_data(16);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<uint8_t>(i * 5 + 1);
    }
    REQUIRE(board.mmio_write(0, 0x1000, write_data.data(), write_data.size()) == 0);

    board.tick(); // 显式 drain 写请求(与 sim 线程 drain 幂等)

    // 读回: 必须是真实数据回填, 不是占位 0 / garbage (TODO T-bs-3c 修复)
    std::vector<uint8_t> read_data(16, 0xFF);
    REQUIRE(board.mmio_read(0, 0x1000, read_data.data(), read_data.size()) == 0);
    REQUIRE(read_data == write_data);

    board.shutdown();
}

TEST_CASE("DGpuBoard: mmio_read timeout returns -110 and leaves buf unchanged",
          "[dgpu][shell][mmio]") {
    DGpuBoard board("test_board");
    // 不 init(): 无 sim 线程 drain → 1ms WAIT_TIMEOUT_MS 超时

    std::vector<uint8_t> buf(16, 0xCD);
    int rc = board.mmio_read(0, 0x1000, buf.data(), buf.size());
    REQUIRE(rc == -110);                            // ETIMEDOUT
    REQUIRE(buf == std::vector<uint8_t>(16, 0xCD)); // buf 未被修改

    board.shutdown();
}

TEST_CASE("DGpuBoard: mmio_read null buf returns -EINVAL", "[dgpu][shell][mmio]") {
    DGpuBoard board("test_board");

    // null buf 无条件拒绝, 避免 memcpy nullptr (与 backdoor_read 修复 #6 一致)
    REQUIRE(board.mmio_read(0, 0x1000, nullptr, 16) == -EINVAL);

    board.shutdown();
}
