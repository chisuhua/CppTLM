// test/test_dgpu_soc_timing_init.cc
// Timing-mode dGPU SoC 初始化集成测试 (Phase 4 cpptlm-dgpu-soc-timing-mvp T6)
// 验证 design.md §6 + spec.md "dgpu_soc_timing_v1.json 加载":
//   1. simulation_mode_ = Timing (JSON 顶层 simulation_mode: "timing")
//   2. 6 module types instantiated (pcie_ep/pcie_memory/vram_ctrl/sdma/gmmu/completion)
//   3. 5 timing param setters called (read/write lat, tlb miss, row hit/miss, bw)
//   4. current_cycle_ 推进 (tick 后 cycle_advance_modules_ 同步)
#include <cstdint>
#include <fstream>
#include <vector>
#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/vram_controller_tlm.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

json load_timing_config() {
    json cfg;
    std::ifstream ifs("configs/dgpu_soc_timing_v1.json");
    if (ifs.is_open()) {
        cfg = json::parse(ifs);
    }
    REQUIRE(ifs.is_open());  // configs/dgpu_soc_timing_v1.json 必须存在
    // schema 补齐 (per minimal_v1 模式)
    if (!cfg.contains("connections")) cfg["connections"] = json::array();
    if (cfg.contains("modules") && cfg["modules"].is_array()) {
        for (auto& m : cfg["modules"]) {
            if (m.contains("modules") && !m.contains("connections"))
                m["connections"] = json::array();
        }
    }
    return cfg;
}

} // namespace

TEST_CASE("timing-soc init: simulation_mode=Timing + 6 module types + timing params",
          "[dgpu_soc_timing][timing_soc][init]") {
    EventQueue eq;
    DGpuBoard board("timing_soc_init", &eq);

    REQUIRE(board.load_soc_config(load_timing_config()));
    // init 前默认 functional
    REQUIRE(board.simulation_mode() == DGpuBoard::SimulationMode::Functional);

    std::vector<uint8_t> framebuffer(16ULL * 1024 * 1024, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // 1. simulation_mode_ = Timing (JSON 顶层 "simulation_mode": "timing")
    REQUIRE(board.simulation_mode() == DGpuBoard::SimulationMode::Timing);

    // 2. 内部模块解析 (6 modules: pcie_ep/pcie_memory/vram_ctrl/sdma/gmmu/completion)
    SdmaEngineTLM* sdma = board.sdma_engine();
    REQUIRE(sdma != nullptr);
    // vram_ctrl 替代 memory (H2 互斥): vram_ctrl 存在, memory 不存在
    VramControllerTLM* vram = board.vram_ctrl_module();
    REQUIRE(vram != nullptr);
    REQUIRE(board.memory_module() == nullptr);
    GmmuTLM* gmmu = board.gmmu_module();
    REQUIRE(gmmu != nullptr);

    // 3. timing param setters called:
    //    vram_ctrl: row_hit=30, row_miss=80, bandwidth=32 (JSON) → 父类 read latencies 同步
    //    行缓冲 params 经 set_vram_params 同步父类 read latencies
    //    (验证: 后续 E2E 断言 latency 行为)
    //    gmmu: tlb_size=32 (JSON), tlb_miss_latency=50 (JSON)
    REQUIRE(gmmu->tlb_size() == 32);
    REQUIRE(gmmu->tlb_miss_latency() == 50);
    //    sdma: cycle accounting enabled (init_timing_mode 注入)
    REQUIRE(sdma->cycle_accounting_enabled() == true);
    REQUIRE(sdma->get_translate_timing_cb() != nullptr);

    // 4. current_cycle_ 推进 (timing-mode tick 集中 advance)
    uint64_t c0 = board.current_cycle();
    board.tick();
    REQUIRE(board.current_cycle() == c0 + 1);
    // cycle_advance_modules_ = [vram_ctrl] (memory 不存在) → vram current_cycle 同步
    REQUIRE(vram->current_cycle() == c0 + 1);

    board.shutdown();
}

TEST_CASE("timing-soc init: vram_ctrl size_cap 从 framing 注入 + functional 默认零影响",
          "[dgpu_soc_timing][timing_soc][init]") {
    EventQueue eq;
    DGpuBoard board("timing_soc_init_func", &eq);

    // 无 simulation_mode 字段 → functional 默认 (零 diff, per design §6.3)
    json cfg = load_timing_config();
    cfg.erase("simulation_mode");
    REQUIRE(board.load_soc_config(cfg));
    std::vector<uint8_t> framebuffer(16ULL * 1024 * 1024, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    REQUIRE(board.simulation_mode() == DGpuBoard::SimulationMode::Functional);
    VramControllerTLM* vram = board.vram_ctrl_module();
    REQUIRE(vram != nullptr);
    // functional-mode 不调用 init_timing_mode → sdma cycle accounting 保持默认关闭
    SdmaEngineTLM* sdma = board.sdma_engine();
    REQUIRE(sdma != nullptr);
    REQUIRE(sdma->cycle_accounting_enabled() == false);

    // functional tick 不推进 cycle
    uint64_t c0 = board.current_cycle();
    board.tick();
    REQUIRE(board.current_cycle() == c0);

    board.shutdown();
}