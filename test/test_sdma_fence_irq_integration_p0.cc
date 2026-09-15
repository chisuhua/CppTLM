// test_sdma_fence_irq_integration_p0.cc
// Stage 1.3a integration (P0 unblock Task 7):
//   submit_fence → SdmaEngineTLM::process_fence_queue → DGpuBoard::sdma_fence_complete
//   → msix_update_pending(kSdmaFenceVector=0) → trigger_irq_async(0) → UE intr_cb
//   全链路真实路径 (set_dgpu_board, 非 mock fence_msix_handler_)
// Author: CppTLM Team
// Date: 2026-09-13
//
// 参考: docs/superpowers/specs/2026-09-13-ue-sdma-p0-unblock-design.md §3.2 目标 3

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

// 内联最小 SOC + PcieEndpointTLM 配置 (无 cwd 依赖), 与 test_dgpu_msix.cc 同模式
json make_mini_board_cfg() {
    return json::parse(R"({
        "name": "p0_fence_test_board",
        "params": {"device_id": "0x1234", "quantum_cycles": 1000,
                   "ptx_emu_root": "/tmp/test-ptx-emu"},
        "modules": [{
            "name": "soc", "type": "DGpuSoc",
            "modules": [{
                "name": "pcie_ep", "type": "PcieEndpointIP",
                "params": {
                    "config_size": 4096, "num_msix_vectors": 16,
                    "bar_sizes": [65536, 268435456]
                }
            }],
            "connections": []
        }],
        "connections": []
    })");
}

}  // namespace

TEST_CASE("SDMA Fence: end-to-end fence → DGpuBoard → trigger_irq_async(vector=0)",
          "[sdma][fence][1.3a][integration]") {
    EventQueue eq;

    DGpuBoard board("test_board_fence_p0", &eq);
    REQUIRE(board.load_soc_config(make_mini_board_cfg()));
    board.init();
    REQUIRE(board.msix_init(4, 0) == 0);

    SdmaEngineTLM sdma("sdma_fence_p0", &eq);
    sdma.init();
    sdma.set_dgpu_board(&board);

    std::atomic<int> intr_cb_count{0};
    std::atomic<uint32_t> captured_vector{999};
    board.set_irq_callback([&](uint32_t vector_id) {
        intr_cb_count.fetch_add(1, std::memory_order_acq_rel);
        captured_vector.store(vector_id, std::memory_order_relaxed);
    });

    SdmaEngineTLM::FenceDescriptor fence{0x04, /*fence_id=*/1, /*tag=*/100};
    sdma.submit_fence(fence);
    sdma.tick();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (intr_cb_count.load(std::memory_order_acquire) < 1
           && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(intr_cb_count.load() >= 1);
    REQUIRE(captured_vector.load() == SdmaEngineTLM::kSdmaFenceVector);
    REQUIRE(captured_vector.load() == 0u);

    board.shutdown();
}
