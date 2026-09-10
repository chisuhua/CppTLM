// test/test_dgpu_mmio.cc
// Stage 1.1 task 1.1.3 (修复 #5): DGpuBoard::mmio_read 真实数据回填
//   roundtrip → 0 + buf 匹配写入数据 (经 drain_injection_queue 响应 payload, 非 TODO T-bs-3c 占位)
//   timeout   → -110 (ETIMEDOUT), buf 不变
//   null buf  → -EINVAL
//   size mismatch → -EINVAL (spec scenario)
// 确定性驱动: mmio_read 阻塞等待 drain 时由 pump 线程持续 tick() 直到读完成 (有界重试),
//   不依赖 1ms/50ms 等待窗口内 sim 线程能否被调度 (修复 #5 flakiness, Oracle review 5bfc91a4)
// Per openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes
// (Oracle R7 裁决: 成功返 0, 不返 byte count)
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>
#include <catch_amalgamated.hpp>
#include "tlm/gpu/dgpu_board_shell.hh"

using namespace tlm::gpu;

namespace {

    // 确定性 pump: mmio_read 等待 drain 完成期间, 由本线程反复 tick() 驱动 inject_q_ drain,
    // 直到读返回 (done=true) 或达到墙钟 deadline (有界). 与 sim 线程 drain 幂等且线程安全.
    // 用 busy-spin + yield 而非 sleep: 重载主机上 sleep 粒度放大, 曾 50ms 窗口内无 tick 落地
    // (Oracle review 5bfc91a4 HIGH flakiness, 4×yes 满载复现 5/40 失败).
    int mmio_read_with_pump(DGpuBoard& board, uint8_t bar, uint64_t offset, void* buf, size_t len) {
        std::atomic<bool> done{false};
        std::thread pump([&]() {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!done.load() && std::chrono::steady_clock::now() < deadline) {
                board.tick();
                std::this_thread::yield();
            }
        });
        int rc = board.mmio_read(bar, offset, buf, len);
        done.store(true);
        pump.join();
        return rc;
    }

} // namespace

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
    REQUIRE(mmio_read_with_pump(board, 0, 0x1000, read_data.data(), read_data.size()) == 0);
    REQUIRE(read_data == write_data);

    board.shutdown();
}

TEST_CASE("DGpuBoard: mmio_read timeout returns -110 and leaves buf unchanged",
          "[dgpu][shell][mmio]") {
    DGpuBoard board("test_board");
    // 关闭自 drain 回退且不 init(): 无任何外部 drain → 等待 kMmioWaitTimeout 后确定性超时
    board.mmio_self_drain_enabled = false;

    std::vector<uint8_t> buf(16, 0xCD);
    int rc = board.mmio_read(0, 0x1000, buf.data(), buf.size());
    REQUIRE(rc == -110);                            // ETIMEDOUT
    REQUIRE(buf == std::vector<uint8_t>(16, 0xCD)); // buf 未被修改

    board.shutdown();
}

TEST_CASE("DGpuBoard: mmio_read size mismatch returns -EINVAL and leaves buf unchanged",
          "[dgpu][shell][mmio]") {
    DGpuBoard board("test_board");
    board.init();

    std::vector<uint8_t> write_data(8, 0xAB);
    REQUIRE(board.mmio_write(0, 0x2000, write_data.data(), write_data.size()) == 0);
    board.tick();

    // 已写 8B, 读 4B → 长度不匹配 → drain 返 -EINVAL, buf 不修改
    std::vector<uint8_t> read_data(4, 0xCD);
    REQUIRE(mmio_read_with_pump(board, 0, 0x2000, read_data.data(), read_data.size()) == -EINVAL);
    REQUIRE(read_data == std::vector<uint8_t>(4, 0xCD)); // buf 未被修改

    board.shutdown();
}

TEST_CASE("DGpuBoard: mmio_read null buf returns -EINVAL", "[dgpu][shell][mmio]") {
    DGpuBoard board("test_board");

    // null buf 无条件拒绝, 避免 memcpy nullptr (与 backdoor_read 修复 #6 一致)
    REQUIRE(board.mmio_read(0, 0x1000, nullptr, 16) == -EINVAL);

    board.shutdown();
}
