// test/test_dgpu_backdoor.cc
// Stage 1.1 task 1.1.2 (修复 #6): DGpuBoard::backdoor_read 返值语义
//   miss         → -ENOENT (未找到, 不再伪装成功返 len)
//   hit          → 0 + memcpy 数据回填 buf
//   size mismatch→ -EINVAL
//   null buf     → -EINVAL
// Per openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes
#include <cerrno>
#include <cstdint>
#include <vector>
#include <catch_amalgamated.hpp>
#include "tlm/gpu/dgpu_board_shell.hh"

using namespace tlm::gpu;

TEST_CASE("DGpuBoard: backdoor_read miss returns -ENOENT not len", "[dgpu][backdoor]") {
    DGpuBoard board("test_board");
    board.init();

    // 0xDEADBEEF 从未写入 → miss, 返 -ENOENT 而非 len(伪装成功)
    std::vector<uint8_t> buf(64, 0xCD);
    REQUIRE(board.backdoor_read(0xDEADBEEF, buf.data(), buf.size()) == -ENOENT);
    // spec: miss 时 buf 不被修改(无假数据回填)
    REQUIRE(buf == std::vector<uint8_t>(64, 0xCD));

    board.shutdown();
}

TEST_CASE("DGpuBoard: backdoor_read hit returns 0 with data roundtrip", "[dgpu][backdoor]") {
    DGpuBoard board("test_board");
    board.init();

    std::vector<uint8_t> write_data(64);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<uint8_t>(i * 3 + 7);
    }
    REQUIRE(board.backdoor_write(0x1000, write_data.data(), write_data.size()) == 0);

    std::vector<uint8_t> read_data(64, 0xFF);
    REQUIRE(board.backdoor_read(0x1000, read_data.data(), read_data.size()) == 0);
    REQUIRE(read_data == write_data); // memcpy roundtrip

    board.shutdown();
}

TEST_CASE("DGpuBoard: backdoor_read size mismatch returns -EINVAL", "[dgpu][backdoor]") {
    DGpuBoard board("test_board");
    board.init();

    std::vector<uint8_t> write_data(64, 0xAB);
    REQUIRE(board.backdoor_write(0x1000, write_data.data(), write_data.size()) == 0);

    // 已写 64B, 读 32B → size mismatch → -EINVAL
    std::vector<uint8_t> read_data(32, 0);
    REQUIRE(board.backdoor_read(0x1000, read_data.data(), read_data.size()) == -EINVAL);

    board.shutdown();
}

TEST_CASE("DGpuBoard: backdoor_read null buf returns -EINVAL", "[dgpu][backdoor]") {
    DGpuBoard board("test_board");
    board.init();

    // null buf → -EINVAL (即使 bar_sizes[1]==0 未初始化 board 也拒绝, 避免 memcpy nullptr)
    REQUIRE(board.backdoor_read(0x0, nullptr, 64) == -EINVAL);

    board.shutdown();
}
