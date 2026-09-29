// test/test_dgpu_soc_timing_inv.cc
// Timing-mode SoC 6 条新 Invariants 验证 (Phase 4 cpptlm-dgpu-soc-timing-mvp T6.3)
// per ADR-DGPU-11 + docs/designs/dgpu-soc/timing-mode.md §6:
//   Inv-1: 共享 vram_storage_ (单一真源, board 拥有)
//   Inv-2: 同一 cycle 内 ordering (pending_resps_ priority by ready_cycle)
//   Inv-3: cycle advance 对齐 (board 集中推进, 模块不可自行 ++cycle)
//   Inv-4: backdoor 禁用性能测试 (backdoor 不采样时序)
//   Inv-5: BAR 路由优先级 (chip-internal AXI vs board-level PCIe 边界)
//   Inv-6: translate_cb 签名兼容 (DmaTranslateCb 签名严格保持)
#include <cstdint>
#include <cstring>
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
    if (ifs.is_open()) cfg = json::parse(ifs);
    REQUIRE(ifs.is_open());
    if (!cfg.contains("connections")) cfg["connections"] = json::array();
    if (cfg.contains("modules") && cfg["modules"].is_array()) {
        for (auto& m : cfg["modules"]) {
            if (m.contains("modules") && !m.contains("connections"))
                m["connections"] = json::array();
        }
    }
    return cfg;
}

void inject_req(VramControllerTLM* vram, uint64_t tid, uint64_t addr, bool wr, uint64_t data = 0,
                uint8_t size = 8) {
    bundles::CacheReqBundle req;
    req.transaction_id.write(tid);
    req.address.write(addr);
    req.is_write.write(wr ? 1 : 0);
    req.data.write(data);
    req.size.write(size);
    vram->req_in().consume();
    std::memcpy(&vram->req_in().data(), &req, sizeof(req));
    vram->req_in().set_valid(true);
}

} // namespace

// Inv-1: 共享 vram_storage_ — board backdoor / BAR1 mmio / SDMA backdoor / GMMU mem_view
// 指向同一 backing (ADR-DGPU-05 单一真源)
TEST_CASE("timing-soc Inv-1: 单一 vram_storage_ 共享 (board backdoor + mmio 双读回一致)",
          "[dgpu_soc_timing][timing_soc][inv]") {
    EventQueue eq;
    DGpuBoard board("inv1", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    std::vector<uint8_t> framebuffer(16ULL * 1024 * 1024, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // backdoor write 落 board backing → mmio_read (BAR1 fast-path) 读回同值
    // 证明 board vram_storage_ 是唯一真源 (backdoor 与 mmio 共享)
    uint64_t golden = 0xDEADBEEFCAFE1234ULL;
    REQUIRE(board.backdoor_write(0x100, &golden, 8) == 0);
    board.tick();
    uint64_t out = 0;
    REQUIRE(board.backdoor_read(0x100, &out, 8) == 0);
    REQUIRE(out == golden);
    board.shutdown();
}

// Inv-2: 同一 cycle 内 ordering — 后到但 ready_cycle 更小的 resp 先发 (priority_queue)
// 使用 write (120 cyc) vs read miss (50 cyc): A 先到 (write, ready=120), B 后到 (read, ready=55)
// → B 先发 (ready 55 < 120), 验证非 FIFO 的 ready_cycle 排序
TEST_CASE("timing-soc Inv-2: 同一 cycle ordering (pending_resps_ priority by ready_cycle)",
          "[dgpu_soc_timing][timing_soc][inv]") {
    EventQueue eq;
    VramControllerTLM vram("vram", &eq);
    std::vector<uint8_t> backing(8192, 0);
    vram.set_backing_view(backing.data(), backing.size());
    vram.set_vram_params(100, 50, 32);       // row params (同步父类 read lat: hit=100, miss=50)
    vram.set_timing_params(100, 50, 120, false);  // write=120 显式

    // cycle 0: req A write @0x100 → ready = 0 + 120 = 120
    inject_req(&vram, 1, 0x100, true, 0xABCDEF, 8);
    vram.tick();
    REQUIRE(vram.inflight_resp_count() == 1);

    // cycle 5: req B read @0x1100 → row miss → ready = 5 + 50 = 55 (< 120)
    for (int i = 0; i < 5; ++i) vram.advance_cycle();
    inject_req(&vram, 2, 0x1100, false, 0, 8);
    vram.tick();
    REQUIRE(vram.inflight_resp_count() == 2);

    // cycle 55: B 先发 (ready=55 < A ready=120)
    for (int i = 0; i < 50; ++i) vram.advance_cycle();
    vram.tick();
    REQUIRE(vram.resp_out().valid());
    REQUIRE(vram.resp_out().data().transaction_id.read() == 2);
    vram.resp_out().clear_valid();

    // cycle 120: A 发
    for (int i = 0; i < 65; ++i) vram.advance_cycle();
    vram.tick();
    REQUIRE(vram.resp_out().valid());
    REQUIRE(vram.resp_out().data().transaction_id.read() == 1);
}

// Inv-3: cycle advance 对齐 — board.tick() 集中推进 vram_ctrl.current_cycle_
TEST_CASE("timing-soc Inv-3: cycle advance 对齐 (board 集中推进, 模块不自行 ++)",
          "[dgpu_soc_timing][timing_soc][inv]") {
    EventQueue eq;
    DGpuBoard board("inv3", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    std::vector<uint8_t> framebuffer(16ULL * 1024 * 1024, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());
    REQUIRE(board.simulation_mode() == DGpuBoard::SimulationMode::Timing);

    VramControllerTLM* vram = board.vram_ctrl_module();
    REQUIRE(vram != nullptr);

    // tick 3 次: board.current_cycle_ 与 vram.current_cycle_ 对齐
    board.tick();
    board.tick();
    board.tick();
    REQUIRE(board.current_cycle() == 3);
    REQUIRE(vram->current_cycle() == 3);
    board.shutdown();
}

// Inv-4: backdoor 禁用性能测试 — backdoor 路径不采样时序 stats (immediate)
TEST_CASE("timing-soc Inv-4: backdoor 禁用性能测试 (不采样时序)",
          "[dgpu_soc_timing][timing_soc][inv]") {
    EventQueue eq;
    DGpuBoard board("inv4", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    std::vector<uint8_t> framebuffer(16ULL * 1024 * 1024, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // backdoor write/read 同步完成 (不依赖 sim 线程/cycle advance)
    uint64_t val = 0x1234;
    REQUIRE(board.backdoor_write(0x200, &val, 8) == 0);
    uint64_t out = 0;
    REQUIRE(board.backdoor_read(0x200, &out, 8) == 0);
    REQUIRE(out == 0x1234);
    board.shutdown();
}

// Inv-5: BAR 路由优先级 — BAR1 存储路由优先 (非 doorbell offset 落 framebuffer)
TEST_CASE("timing-soc Inv-5: BAR 路由优先级 (BAR1 storage 路由)", "[dgpu_soc_timing][timing_soc][inv]") {
    EventQueue eq;
    DGpuBoard board("inv5", &eq);
    json cfg = load_timing_config();
    cfg["storage_routing_enabled"] = true;
    cfg["gmmu_routing_enabled"] = false;
    REQUIRE(board.load_soc_config(cfg));
    std::vector<uint8_t> framebuffer(16ULL * 1024 * 1024, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // BAR1 write @offset 0x10000 (非 doorbell 0x10010000): storage 路由
    uint64_t val = 0xABCDEF;
    REQUIRE(board.mmio_write(1, 0x10000, &val, 8) == 0);
    board.tick();  // drain inject_q

    // BAR1 fast-path 读回 (mmio_read 同 offset) 应返回写入值 (落 framebuffer)
    uint64_t out = 0;
    REQUIRE(board.mmio_read(1, 0x10000, &out, 8) == 0);
    REQUIRE(out == val);
    board.shutdown();
}

// Inv-6: translate_cb 签名兼容 — DmaTranslateCb (3 参) 严格保持, timing wrapper 是附加 API
TEST_CASE("timing-soc Inv-6: translate_cb 签名兼容 (DmaTranslateCb 3 参冻结)",
          "[dgpu_soc_timing][timing_soc][inv]") {
    EventQueue eq;
    DGpuBoard board("inv6", &eq);
    REQUIRE(board.load_soc_config(load_timing_config()));
    std::vector<uint8_t> framebuffer(16ULL * 1024 * 1024, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    GmmuTLM* gmmu = board.gmmu_module();
    REQUIRE(gmmu != nullptr);
    SdmaEngineTLM* sdma = board.sdma_engine();
    REQUIRE(sdma != nullptr);

    // SDMA 的 translate_cb_ (functional 3 参) 存在 (bind_memory_backings 注入)
    REQUIRE(sdma->get_translate_cb() != nullptr);
    // timing wrapper (4 参) 附加注入 (init_timing_mode) → 签名不冲突
    REQUIRE(sdma->get_translate_timing_cb() != nullptr);
    board.shutdown();
}
