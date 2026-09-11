// test/test_dgpu_msix_cap.cc
// C2 (cpptlm-stage-1-2-msix §2.5.1 任务 2.5.1 基础任务 1.2.2):
// MSI-X Capability + 4-8 向量表
//   - test_msix_cap_table_size_field_reflects_init_n:
//       on_config_loaded 后 MSI-X Cap (id 0x11) 存在,
//       Message Control 的 Table Size 字段 bits[15:11] = n-1,
//       MSI-X Enable (bit 0) 在 driver 使能前保持 0
//   - test_msix_init_resizes_table_enforces_4_to_8:
//       msix_init 触发 MsiXTable::resize → num_vectors() 跟随最新值,
//       update_pending 越界 → -EINVAL (-22)
// 复用 D15 inline JSON pattern (test_dgpu_board_msix_wrappers.cc:52-74) 避免 cwd 依赖
#include <catch_amalgamated.hpp>
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

// C2: 内联 SOC + PcieEndpointTLM 配置, 无 cwd 依赖。key 选择 MSI-X vector 数配置键名:
//   - "msix_num_vectors" (新名, 优先)
//   - "num_msix_vectors" (旧名, 回退; pre-fix 被忽略 → 默认 16 vectors → RED 路径)
static json msix_cap_board_cfg(const std::string& key) {
    return json{
        {"name", "msix_cap_board"},
        {"params",
         {{"device_id", "0x1234"},
          {"quantum_cycles", 1000},
          {"ptx_emu_root", "/tmp/test-ptx-emu"}}},
        {"modules",
         json::array(
             {{{"name", "soc"},
               {"type", "DGpuSoc"},
               {"modules", json::array({{{"name", "pcie_ep"},
                                         {"type", "PcieEndpointTLM"},
                                         {"params",
                                          {{"config_size", 4096},
                                           {key, 8},
                                           {"bar_sizes", json::array({65536, 268435456})}}}}})},
               {"connections", json::array()}}})},
        {"connections", json::array()}};
}

// C2 测试 1: MSI-X Cap 存在 + Message Control Table Size 反映 init 的 vector 数。
// pre-fix: on_config_loaded 不插入 MSI-X Cap → capability 遍历找不到 id 0x11 → RED
TEST_CASE("test_msix_cap_table_size_field_reflects_init_n", "[dgpu][msix][cap]") {
    DGpuBoard board("msix_cap_board");
    board.init();
    // 新 JSON 名 msix_num_vectors (优先) → MsiXTable(8), on_config_loaded 同步 MSI-X Cap
    REQUIRE(board.load_soc_config(msix_cap_board_cfg("msix_num_vectors")));
    REQUIRE(board.msix_init(8, 0) == 0);

    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);

    // 遍历 capability chain 找 MSI-X Cap (id == 0x11)
    const PcieConfigSpace::Capability* msix_cap = nullptr;
    for (std::size_t i = 0; i < ep->config_space().capability_count(); ++i) {
        const auto* cap = ep->config_space().get_capability(i);
        if (cap && cap->id == 0x11) {
            msix_cap = cap;
            break;
        }
    }
    REQUIRE(msix_cap != nullptr);

    // Message Control: bits[15:11] = Table Size = n-1
    // ((control >> 11) & 0x1F) + 1 == n (n = 8)
    REQUIRE((((msix_cap->control >> 11) & 0x1Fu) + 1u) == 8u);
    // MSI-X Enable (bit 0) 保持 0 (driver 使能前)
    REQUIRE((msix_cap->control & 0x1u) == 0u);

    board.shutdown();
}

// C2 测试 2: msix_init 触发 MsiXTable::resize, 越界 update_pending 返 -EINVAL。
// pre-fix: 旧 JSON 名 num_msix_vectors 被忽略 → 默认 16 vectors →
//          update_pending(8) == 0 (≠ -22) → RED; msix_init(4) 不 resize →
//          update_pending(4) == 0 (≠ -22) → RED
TEST_CASE("test_msix_init_resizes_table_enforces_4_to_8", "[dgpu][msix][cap]") {
    DGpuBoard board("msix_cap_board");
    board.init();
    // 旧 JSON 名 num_msix_vectors: pre-fix 忽略 → 默认 16 vectors (描述 RED 场景)
    REQUIRE(board.load_soc_config(msix_cap_board_cfg("num_msix_vectors")));

    // 扩到 8: update_pending(7) 有效, update_pending(8) 越界 → -22
    REQUIRE(board.msix_init(8, 0) == 0);
    REQUIRE(board.pcie_ep()->msix().num_vectors() == 8u);
    REQUIRE(board.msix_update_pending(7) == 0);
    REQUIRE(board.msix_update_pending(8) == -22);

    // 缩到 4: update_pending(3) 有效, update_pending(4) 越界 → -22
    REQUIRE(board.msix_init(4, 0) == 0);
    REQUIRE(board.pcie_ep()->msix().num_vectors() == 4u);
    REQUIRE(board.msix_update_pending(3) == 0);
    REQUIRE(board.msix_update_pending(4) == -22);

    board.shutdown();
}

// C2 测试 3 (HIGH-1 / MEDIUM-2 修复验证): MSI-X Cap dword host-visible。
//   - add_capability 把完整 cap dword (id | next<<8 | control<<16) 写入 regs_
//     → host CFG_READ(0x40) 可见。pre-fix: regs_ 未写 → read(0x40)==0 → RED
//   - msix_init 运行时 resize 后同步 Cap Table Size 字段 (bits[31:16]=(n-1)<<11)
//     → host 视角一致。pre-fix: Cap 字段 stale → RED
// 期望 dword 数学: dword = id(0x11) | next(0)<<8 | control<<16, control = (n-1)<<11
//   n=8 → control = 7<<11 = 0x3800 → dword = 0x38000011
//   n=4 → control = 3<<11 = 0x1800 → dword = 0x18000011
TEST_CASE("test_msix_cap_host_visible_after_resize", "[dgpu][msix][cap][host-visible]") {
    DGpuBoard board("msix_cap_board");
    board.init();
    // 新 JSON 名 msix_num_vectors (优先) → MsiXTable(8), on_config_loaded 同步 MSI-X Cap
    REQUIRE(board.load_soc_config(msix_cap_board_cfg("msix_num_vectors")));

    // 初始 8 vectors: msix_init → sync_msix_cap_table_size(8)
    REQUIRE(board.msix_init(8, 0) == 0);
    auto* ep = board.pcie_ep();
    REQUIRE(ep != nullptr);

    const uint32_t dword8 = ep->config_space().read(0x40);
    REQUIRE(dword8 == 0x38000011u);
    REQUIRE((dword8 & 0xFFu) == 0x11u);             // cap id 低位可见
    REQUIRE(((dword8 >> 16) & 0xFFFFu) == 0x3800u); // Table Size = 8-1

    // 运行时 resize 到 4: msix_init → sync_msix_cap_table_size(4)
    REQUIRE(board.msix_init(4, 0) == 0);
    const uint32_t dword4 = ep->config_space().read(0x40);
    REQUIRE(dword4 == 0x18000011u);
    REQUIRE((dword4 & 0xFFu) == 0x11u);             // cap id 低位可见
    REQUIRE(((dword4 >> 16) & 0xFFFFu) == 0x1800u); // Table Size = 4-1

    board.shutdown();
}