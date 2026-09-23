// test/test_pcie_board_routing_characterization.cc
// D1 Step 0 (Characterization Test): 锁定 DGpuBoard 当前路由语义防 D1 回归
//
// 背景: D1 v1.1 (Oracle 审查通过) 将在 DGpuBoard::mmio_read/write 和
// backdoor_read/write 中添加 PcieDisplayDevice 路由层。这些测试必须确保
// **无 device 时** (soc_==nullptr 或 ep->has_display_device()==false) 现有行为不变。
//
// 锁定的 6 项当前行为:
//   1. mmio_read/write 不接 SOC → 走 mmio_regs_ map (roundtrip 仍真实)
//   2. mmio_read/write null buf → 返 -EINVAL
//   3. backdoor_read miss → 返 -ENOENT (修复 #6 已存在)
//   4. backdoor_read/write size mismatch → 返 -EINVAL
//   5. cpptlm_emulator_pcie_config_read 不接 EP → 返 -ENOSYS
//   6. cpptlm_emulator_mmio_read 不接 SOC → 走 mmio_regs_ 路径 (D1 后 device 优先)
//
// 作者: CppTLM Team / 日期: 2026-09-20
// 配套: openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/design.md §9

#include <cerrno>
#include <cstdint>
#include <vector>

#include <catch_amalgamated.hpp>
#include "tlm/gpu/dgpu_board_shell.hh"

using namespace tlm::gpu;

TEST_CASE("DGpuBoard mmio_read/write without SOC uses mmio_regs_ map (roundtrip)",
          "[pcie][display][characterization][regression]") {
    DGpuBoard board("char_test_board_1");
    board.init();

    std::vector<uint8_t> write_data(16, 0xAB);
    REQUIRE(board.mmio_write(0, 0x100, write_data.data(), write_data.size()) == 0);

    std::vector<uint8_t> read_data(16, 0x00);
    REQUIRE(board.mmio_read(0, 0x100, read_data.data(), read_data.size()) == 0);
    REQUIRE(read_data == write_data);

    board.shutdown();
}

TEST_CASE("DGpuBoard mmio_read null buf returns -EINVAL (D1 后不变)",
          "[pcie][display][characterization][regression]") {
    DGpuBoard board("char_test_board_2");
    board.init();

    REQUIRE(board.mmio_read(0, 0x100, nullptr, 16) == -EINVAL);

    board.shutdown();
}

TEST_CASE("DGpuBoard mmio_write null buf returns -EINVAL (D1 后不变)",
          "[pcie][display][characterization][regression]") {
    DGpuBoard board("char_test_board_3");
    board.init();

    REQUIRE(board.mmio_write(0, 0x100, nullptr, 16) == -EINVAL);

    board.shutdown();
}

TEST_CASE("DGpuBoard backdoor_read miss returns -ENOENT (修复 #6 已存在, D1 后不变)",
          "[pcie][display][characterization][regression][backdoor]") {
    DGpuBoard board("char_test_board_4");
    board.init();

    // 从未写入到 vram_segments_ → miss
    std::vector<uint8_t> buf(64, 0xCD);
    REQUIRE(board.backdoor_read(0xDEADBEEF, buf.data(), buf.size()) == -ENOENT);
    // miss 时 buf 不被修改 (无假数据回填)
    REQUIRE(buf == std::vector<uint8_t>(64, 0xCD));

    board.shutdown();
}

TEST_CASE("DGpuBoard backdoor_read/write size mismatch returns -EINVAL (D1 后不变)",
          "[pcie][display][characterization][regression][backdoor]") {
    DGpuBoard board("char_test_board_5");
    board.init();

    std::vector<uint8_t> write_data(64, 0xAB);
    REQUIRE(board.backdoor_write(0x2000, write_data.data(), write_data.size()) == 0);

    // 写入 64 字节, 读 32 字节 → size mismatch
    std::vector<uint8_t> read_data(32, 0x00);
    REQUIRE(board.backdoor_read(0x2000, read_data.data(), read_data.size()) == -EINVAL);

    board.shutdown();
}

TEST_CASE("DGpuBoard cpptlm_emulator_pcie_config_read without SOC returns -ENOSYS",
          "[pcie][display][characterization][regression][config]") {
    DGpuBoard board("char_test_board_6");
    board.init();
    // soc_==nullptr → 返 -ENOSYS (D1 后保持)
    uint32_t val = 0xDEAD;
    REQUIRE(board.pcie_config_read(0x00, 4, &val) == -ENOSYS);

    board.shutdown();
}

TEST_CASE("DGpuBoard mmio_regs_ map survives across BAR/offset writes (no device)",
          "[pcie][display][characterization][regression]") {
    DGpuBoard board("char_test_board_7");
    board.init();

    // 写 BAR 0 offset 0x10
    std::vector<uint8_t> a = {0x01, 0x02, 0x03, 0x04};
    REQUIRE(board.mmio_write(0, 0x10, a.data(), 4) == 0);

    // 写 BAR 0 offset 0x20 (不同 offset, 独立存储)
    std::vector<uint8_t> b = {0x05, 0x06, 0x07, 0x08};
    REQUIRE(board.mmio_write(0, 0x20, b.data(), 4) == 0);

    // 写 BAR 1 offset 0x10 (不同 BAR, 独立存储)
    std::vector<uint8_t> c = {0x09, 0x0A, 0x0B, 0x0C};
    REQUIRE(board.mmio_write(1, 0x10, c.data(), 4) == 0);

    // 分别读回, 互不干扰
    std::vector<uint8_t> ra(4), rb(4), rc(4);
    REQUIRE(board.mmio_read(0, 0x10, ra.data(), 4) == 0);
    REQUIRE(board.mmio_read(0, 0x20, rb.data(), 4) == 0);
    REQUIRE(board.mmio_read(1, 0x10, rc.data(), 4) == 0);
    REQUIRE(ra == a);
    REQUIRE(rb == b);
    REQUIRE(rc == c);

    board.shutdown();
}

TEST_CASE("DGpuBoard backdoor roundtrip via vram_segments_ map (no device)",
          "[pcie][display][characterization][regression][backdoor]") {
    DGpuBoard board("char_test_board_8");
    board.init();

    // 写 backdoor
    std::vector<uint8_t> write_data(256);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<uint8_t>(i ^ 0xAA);
    }
    REQUIRE(board.backdoor_write(0x10000, write_data.data(), write_data.size()) == 0);

    // 读 backdoor
    std::vector<uint8_t> read_data(256, 0xFF);
    REQUIRE(board.backdoor_read(0x10000, read_data.data(), read_data.size()) == 0);
    REQUIRE(read_data == write_data);

    board.shutdown();
}

// D1 行为变更后必须仍然 PASS 的 8 个 characterization test.
// 任何 D1 commit 后, 这些测试必须 100% PASS.
// 若 D1 改动后某 case FAIL → 立即停止 D1, 检查路由分支是否无 device 时也走了 device 路径.