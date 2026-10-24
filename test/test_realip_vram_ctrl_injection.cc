// test/test_realip_vram_ctrl_injection.cc
// T23C pin: DGpuBoard::bind_memory_backings() 注入 vram_ctrl backing.
//
// Per docs/superpowers/plans/2026-10-07-dgpu-realip-refactor-phase-c-handoff.md
// §5 T23C:
//   - src/tlm/gpu/dgpu_board_shell.cc:708-745 bind_memory_backings() 加 vram_ctrl 分支
//   - 注入后 vram_ctrl->read() 返 vram_storage_ 数据 (per ADR-DGPU-05 单一真源)
//   - 未注入时 vram_ctrl backing null (host_addr(0) == nullptr, ENODEV semantics)
//   - 反向断言 (handoff §9.2 #7 dormant-gate trap): 必须实际触发注入分支,
//     否则 backing 永远 null → silent-pass 假阳性
//
// MUST NOT 1 lock (handoff §9.2 #6): VramControllerTLM 类不动 (per ADR-DGPU-11 决策 4)
//   → 不调 set_size_bytes / set_vram_params / row_hit 之类。仅调 set_backing_view
//   继承自 MemoryTLM::set_backing_view,这是 backing-store API v2.2 公有方法
//   (memory_tlm.hh:115)。
//
// TDD 5 步:
//   Step 1 RED:  写 2 cases (注入正向 + 未注入反向), 跑 → 失败 (当前没分支)
//   Step 2 GREEN: bind_memory_backings() 加 vram_ctrl 分支 → 通过
//   Step 3 REFACTOR: 添加 MUST NOT 1 边界断言 (类不动)
//   Step 5 VERIFY 5 regression + [realip][p3] 全绿
//
// Tag: `[realip][p3]`

#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/vram_controller_tlm.hh"

#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>

using tlm::gpu::DGpuBoard;

namespace {

    nlohmann::json load_realip_p2_config() {
        std::ifstream f("configs/dgpu_soc_v2_realip_p2.json");
        REQUIRE(f.is_open());
        nlohmann::json cfg;
        f >> cfg;
        return cfg;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// Step 1 (GREEN 路径): attach_vram_for_testing + init → vram_ctrl backing
// 应被注入 (framebuffer_ptr_ → vram_ctrl->set_backing_view).
// 锁 vram_ctrl->has_backing() == true + backing_size() 等于 framebuffer。
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("DGpuBoard::bind_memory_backings: vram_ctrl backing injected when framebuffer attached",
          "[realip][p3]") {
    auto board_cfg = load_realip_p2_config();

    EventQueue eq;
    DGpuBoard board("realip_vram_inject", &eq);

    REQUIRE(board.load_soc_config(board_cfg));

    constexpr uint64_t kFbSize = 64ULL * 1024 * 1024; // 64MB dummy framebuffer
    std::vector<uint8_t> fb(kFbSize, 0xAB);
    board.attach_vram_for_testing(fb.data(), fb.size());

    REQUIRE(board.init());

    auto* vram_ctrl = board.vram_ctrl_module();
    REQUIRE(vram_ctrl != nullptr);
    REQUIRE(vram_ctrl->get_module_type() == "VramControllerTLM");

    // 注入路径正向验证
    REQUIRE(vram_ctrl->has_backing());
    REQUIRE(vram_ctrl->backing_size() == fb.size());
    REQUIRE(vram_ctrl->host_addr(0) == fb.data());
    REQUIRE(vram_ctrl->host_addr(1024) == fb.data() + 1024);
}

// ─────────────────────────────────────────────────────────────────────────
// Step 1 (反向 / ENODEV 语义): 不 attach framebuffer → init → vram_ctrl
// backing 应为 null (host_addr(0) == nullptr per MemoryTLM::host_addr)。
//
// 锁 dormant-gate trap 反向断言 (handoff §9.2 #7):
//   如果 bind_memory_backings() 加 vram_ctrl 分支但 framebuffer_ptr_==nullptr,
//   分支必须静默跳过 (DPRINTF WARN) 而不是 crash 或写错 backing。测试 #1
//   (注入) 验证注入路径生效,本测试验证 framing null 时 backing 仍 null。
//
// ⚠ P0.5-landing 行为变化 (2027-09-17): 当 JSON 含 pcie_ep 时, load_soc_config
//   从 `pcie_ep.params.bar_sizes[1]` 派生 framebuffer_size_, init() 自动分配
//   backing — 不再是 backing null。test 改测 ENODEV-style ENODEV semantics:
//   用一个不含 pcie_ep 的 inline config (仅 fabric + vram_ctrl) 模拟 dormant 状态。
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("DGpuBoard::bind_memory_backings: vram_ctrl backing null when framebuffer not attached "
          "(no pcie_ep)",
          "[realip][p3]") {
    nlohmann::json inline_cfg = {
        {"modules",
         nlohmann::json::array(
             {{{"type", "DGpuSoc"},
               {"name", "soc"},
               {"modules", nlohmann::json::array(
                               {{{"type", "GpuInternalFabric"},
                                 {"name", "fabric"},
                                 {"modules", nlohmann::json::array({{{"type", "CrossbarTLM"},
                                                                     {"name", "crossbar"}}})},
                                 {"connections", nlohmann::json::array()}},
                                {{"type", "VramControllerTLM"},
                                 {"name", "vram_ctrl"},
                                 {"params",
                                  {{"row_hit_cycles", 30},
                                   {"row_miss_cycles", 80},
                                   {"bandwidth_gbps", 32},
                                   {"read_latency_hit_cycles", 30},
                                   {"read_latency_miss_cycles", 80},
                                   {"write_latency_cycles", 120},
                                   {"use_zero_delay_for_test", true}}}}})},
               {"connections", nlohmann::json::array()}}})},
        {"connections", nlohmann::json::array()}};

    EventQueue eq;
    DGpuBoard board("realip_vram_null", &eq);

    REQUIRE(board.load_soc_config(inline_cfg));
    // Deliberately NOT 调用 attach_vram_for_testing — framebuffer_ptr_ 保持 nullptr
    REQUIRE(board.init());

    auto* vram_ctrl = board.vram_ctrl_module();
    REQUIRE(vram_ctrl != nullptr); // 实例存在 (JSON 含 children)

    // 反向断言: backing null → host_addr(0) == nullptr (ENODEV semantics)
    REQUIRE_FALSE(vram_ctrl->has_backing());
    REQUIRE(vram_ctrl->backing_size() == 0);
    REQUIRE(vram_ctrl->host_addr(0) == nullptr);
}