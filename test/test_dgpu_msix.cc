// test/test_dgpu_msix.cc
// C1 (cpptlm-stage-1-2-msix): DGpuBoard::msix_update_pending 成功路径必须触发
// intr_cb(trigger_irq_async 接线) — 修复 #4 中断链断裂
//   - test_msix_intr_cb_called_within_200ms: msix_update_pending(0) 后 200ms 内
//     intr_cb 被调用且 captured_vector == 0 (pre-fix: cb count 0 → RED)
//   - test_msix_update_pending_oob_returns_einval: vector 越界 → -EINVAL (-22)
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
                "name": "pcie_ep", "type": "PcieEndpointTLM",
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

// 修复 #4: msix_update_pending 成功路径必须触发 intr_cb (trigger_irq_async 接线)
// trigger_irq_async 用 detached thread 调 irq_cb_, 故 spin-wait (1ms step, ≤200ms)
// 等待 cb 完成, 且必须在 board.shutdown() 之前等待, 避免 detached thread 访问已释放状态
TEST_CASE("test_msix_intr_cb_called_within_200ms", "[dgpu][msix][intr-cb]") {
    DGpuBoard board("msix_intr_test_board");
    board.init();
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));

    std::atomic<int> cb_count{0};
    std::atomic<uint32_t> captured_vector{0xFFFFFFFFu};
    board.set_irq_callback([&](uint32_t vector_id) {
        captured_vector.store(vector_id);
        cb_count.fetch_add(1);
    });

    REQUIRE(board.msix_init(4, 0) == 0);
    REQUIRE(board.msix_update_pending(0) == 0);

    // spin-wait: 1ms step, ≤200ms 上限
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (cb_count.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(cb_count.load() >= 1);
    REQUIRE(captured_vector.load() == 0);

    board.shutdown();
}

// vector 越界 → MsiXTable::update_pending 返回 false → wrapper 返 -EINVAL (-22)
// (该测试 pre-fix 即通过, 与 intr_cb 测试一起构成 C1 回归)
TEST_CASE("test_msix_update_pending_oob_returns_einval", "[dgpu][msix][intr-cb]") {
    DGpuBoard board("msix_oob_test_board");
    board.init();
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));
    REQUIRE(board.msix_update_pending(999) == -22);
    board.shutdown();
}
