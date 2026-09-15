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

// 修复 #4 review Gap-2: masked vector 的 update_pending 仅置 PBA 不入队 (per PCI-SIG MSI-X),
// wrapper 不得触发 host intr_cb; unmask auto-deliver 后重新断言中断须恢复触发
// (pre-fix: wrapper 无条件 trigger → masked 也触发 cb → cb_count==1 ≠ 0 → RED)
TEST_CASE("msix_update_pending_masked_does_not_trigger_intr_cb", "[dgpu][msix][intr-cb][mask]") {
    DGpuBoard board("msix_mask_test_board");
    board.init();
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));

    std::atomic<int> cb_count{0};
    std::atomic<uint32_t> captured_vector{0xFFFFFFFFu};
    board.set_irq_callback([&](uint32_t vector_id) {
        captured_vector.store(vector_id);
        cb_count.fetch_add(1);
    });

    // mask 全部 8 个 vector, 然后 update_pending(0): 仅置 PBA, 不投递 → cb 永不触发
    REQUIRE(board.msix_init(8, 0xFFu) == 0);
    REQUIRE(board.msix_update_pending(0) == 0);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(cb_count.load() == 0); // masked: cb MUST NOT fire

    // unmask vector 0 → PBA 累积的 pending 经 auto-deliver 入队 (MsiXTable 内部);
    // 随后 driver 重新断言中断 (第二次 update_pending 走 wrapper) 必须恢复触发 cb
    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);
    REQUIRE(ep->msix().set_mask(0, false) == true);
    REQUIRE(board.msix_update_pending(0) == 0);

    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (cb_count.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(cb_count.load() >= 1);
    REQUIRE(captured_vector.load() == 0);

    board.shutdown();
}
