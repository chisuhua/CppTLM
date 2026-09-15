// test/test_dgpu_msix_throttle.cc
// C3 (cpptlm-stage-1-2-msix §2.5.2 基础任务 1.2.3): DGpuBoard MSI-X 中断合并
// (interrupt coalescing) — threshold + timeout 合并器, 位于 msix_update_pending
// 与 trigger_irq_async 之间的唯一 choke point:
//   - test_msix_throttle_burst_8_in_one_intr: 8 次紧密 msix_update_pending(0)
//     合并为恰 1 次 intr_cb (阈值路径确定性, 无实时依赖);
//     disable 后恢复直发语义 (≥8 次 cb)
//   - test_msix_throttle_timeout_flushes_within_200ms: threshold 拉高 (1024) 后
//     仅 timeout 触发 → 单次 update_pending 在 200ms 内 flush 1 次 cb
//   - test_msix_coalesce_resize_flushes_pending: msix_init resize (8→4) 时
//     coalesce_force_flush 排空 armed 状态; 之后 8 连发仍合并为 1 次 cb
// 复用 D15 inline JSON pattern (test_dgpu_board_msix_wrappers.cc:52-74) 避免 cwd 依赖
#include <atomic>
#include <chrono>
#include <thread>
#include <catch_amalgamated.hpp>
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

// D15 inline JSON pattern (与 test_dgpu_board_msix_wrappers.cc 保持一致):
// 内联 SOC + PcieEndpointTLM 配置, 无 cwd 依赖
static json d15_mini_board_cfg() {
    return json::parse(R"({
        "name": "d15_probe_board",
        "params": {"device_id": "0x1234", "quantum_cycles": 1000,
                   "ptx_emu_root": "/tmp/test-ptx-emu"},
        "modules": [{
            "name": "soc", "type": "DGpuSoc",
            "modules": [{
                "name": "pcie_ep", "type": "PcieEndpointIP",
                "params": {
                    "config_size": 4096, "num_msix_vectors": 16,
                    "bar_sizes": [65536, 268435456],
                    "bar0_registers": [
                        {"offset": 0, "name": "GPU_REG_GPFIFO_PUT", "access": "rw"},
                        {"offset": 20, "name": "GPU_REG_DOORBELL", "access": "wo", "side_effect": "doorbell"}
                    ]
                }
            }],
            "connections": []
        }],
        "connections": []
    })");
}

// spin-wait helper: 1ms step, ≤200ms 上限 (吸收 CI jitter)
static void spin_wait_until(const std::atomic<int>& counter, int target) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (counter.load() < target && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// C3 阈值路径: 8 次紧密调用 (无 sleep) → 恰 1 次 intr_cb, captured_vector == 0。
// 确定性保证: 拉长 timeout (100ms) 使 timer 不可能在 burst 中途触发 →
// 唯一 drain 路径是第 8 次调用的阈值 drain (同步, 无实时依赖)。
// 随后 disable 合并 → 直发语义恢复 (8 次调用 → ≥8 次 cb)。
TEST_CASE("test_msix_throttle_burst_8_in_one_intr", "[dgpu][msix][throttle]") {
    DGpuBoard board("msix_throttle_burst_board");
    board.init();
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));

    // 阈值路径确定性: 拉长 timeout 窗口, timer 不得在 8 连发中途触发
    board.set_msix_coalesce_timeout(std::chrono::milliseconds(100));

    std::atomic<int> cb_count{0};
    std::atomic<uint32_t> captured_vector{0xFFFFFFFFu};
    board.set_irq_callback([&](uint32_t vector_id) {
        captured_vector.store(vector_id);
        cb_count.fetch_add(1);
    });

    REQUIRE(board.msix_init(8, 0) == 0);
    // 8 次紧密调用, 无 sleep: 前 7 次累计, 第 8 次达阈值 → 同步 drain 恰 1 次 cb
    for (int i = 0; i < 8; ++i) {
        REQUIRE(board.msix_update_pending(0) == 0);
    }

    // 等待 detached cb 线程完成 (1ms step, ≤200ms)
    spin_wait_until(cb_count, 1);
    REQUIRE(cb_count.load() == 1);        // 8 次投递合并为 1 次中断
    REQUIRE(captured_vector.load() == 0); // 代表 vector = 首个投递的 vector 0

    // disable 合并 → 恢复 C1 直发语义: 每次 update_pending 触发 1 次 cb
    board.set_msix_coalesce_enabled(false);
    const int before_disable = cb_count.load();
    for (int i = 0; i < 8; ++i) {
        REQUIRE(board.msix_update_pending(0) == 0);
    }
    spin_wait_until(cb_count, before_disable + 8);
    REQUIRE(cb_count.load() >= before_disable + 8);

    // restore enabled (供后续测试/复用)
    board.set_msix_coalesce_enabled(true);
    board.shutdown();
}

// C3 timeout 路径: threshold 拉高 (1024) 使阈值永不触发 → 仅 timer 是 drain 源。
// 单次 update_pending → armed → 50us (默认) 后 timer flush → 恰 1 次 cb (≤200ms 窗口)。
TEST_CASE("test_msix_throttle_timeout_flushes_within_200ms", "[dgpu][msix][throttle]") {
    DGpuBoard board("msix_throttle_timeout_board");
    board.init();
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));

    // threshold 拉高: 单次投递永远达不到阈值 → 只有 timeout 能 flush
    board.set_msix_coalesce_threshold(1024);

    std::atomic<int> cb_count{0};
    std::atomic<uint32_t> captured_vector{0xFFFFFFFFu};
    board.set_irq_callback([&](uint32_t vector_id) {
        captured_vector.store(vector_id);
        cb_count.fetch_add(1);
    });

    REQUIRE(board.msix_init(8, 0) == 0);
    REQUIRE(board.msix_update_pending(0) == 0); // 1 次投递 → armed, 等 timeout

    spin_wait_until(cb_count, 1); // ≤200ms 窗口 (默认 50us timeout 应远早触发)
    REQUIRE(cb_count.load() == 1);
    REQUIRE(captured_vector.load() == 0);

    board.shutdown();
}

// C3 resize 语义 (Oracle caveat ①): msix_init resize 期间不得缓存/悬空 armed 状态。
// arm 1 次 pending (cb_count==0, 长 timeout 防止自触发) → msix_init(4,0) (8→4 resize)
// 触发 coalesce_force_flush 排空 armed 状态 (释放 pending 计数) → 新 resize 后
// 8 连发仍合并为恰 1 次 cb (状态一致)。
TEST_CASE("test_msix_coalesce_resize_flushes_pending", "[dgpu][msix][throttle]") {
    DGpuBoard board("msix_throttle_resize_board");
    board.init();
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));

    // 长 timeout: 保证 armed 状态在 force_flush 前不被 timer 自触发 (确定性)
    board.set_msix_coalesce_timeout(std::chrono::seconds(30));

    std::atomic<int> cb_count{0};
    std::atomic<uint32_t> captured_vector{0xFFFFFFFFu};
    board.set_irq_callback([&](uint32_t vector_id) {
        captured_vector.store(vector_id);
        cb_count.fetch_add(1);
    });

    REQUIRE(board.msix_init(8, 0) == 0);
    REQUIRE(board.msix_update_pending(0) == 0); // arm: count=1, 未达阈值
    REQUIRE(cb_count.load() == 0);              // pre-flush: 无 cb (timer 未到)

    // resize 8→4: coalesce_force_flush 排空 armed pending 并通知 timer
    REQUIRE(board.msix_init(4, 0) == 0);
    REQUIRE(board.pcie_ep()->msix().num_vectors() == 4u); // resize 生效

    // force_flush 已释放 pending (触发 1 次 cb 或至少清空 armed 状态)
    spin_wait_until(cb_count, 1);
    const int after_flush = cb_count.load();
    REQUIRE(after_flush >= 1);

    // resize 后状态一致: 8 连发 (阈值路径) 仍合并为恰 1 次 cb
    const int before_burst = cb_count.load();
    for (int i = 0; i < 8; ++i) {
        REQUIRE(board.msix_update_pending(0) == 0);
    }
    spin_wait_until(cb_count, before_burst + 1);
    REQUIRE(cb_count.load() == before_burst + 1);
    REQUIRE(captured_vector.load() == 0);

    board.shutdown();
}
