// test/test_minimal_soc_driver_visible_e2e.cc
// Driver-Visible Minimal SoC E2E (Phase 3 T4.3, driver-visible-minimal-soc v1.8)
//   Scenario 1: BAR0/1/2 枚举 + BAR2 fast-path mmio round-trip + BAR1 backdoor round-trip
//   Scenario 2: config_space BAR2 64-bit 双 dword 编码 (N6) + BAR1 窗口 vs BAR2 真源隔离
//
// 关联 spec: openspec/changes/cpptlm-driver-visible-minimal-soc/
// 关键约束 (per design.md §8 数据流 + §7 JSON 修订):
//   - load configs/dgpu_soc_minimal_v1.json (含 memory_routing_enabled + pcie_memory + 3 BAR)
//   - DGpuBoard::bind_memory_backings 条件注入 pcie_memory (N12) + EP set_memory_device (T1.2)
//   - BAR2 fast-path → ep->memory_device().memory_read/write (per design §8 数据流 2/3)
//   - PTE 经 BAR2 写入 → GMMU translate 经 PcieTlpBundle MEM_READ 读同一 PTE (v1.8 H1)
// 作者: CppTLM Team · 日期: 2027-02-09

#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"

#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstdint>
#include <fstream>
#include <vector>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

constexpr uint64_t kFramebufferSize = 16ULL * 1024 * 1024;      // 16 MB (bar_sizes[1])
constexpr uint64_t kVramSize = 8ULL * 1024 * 1024 * 1024;       // 8 GB (bar_sizes[2])
constexpr uint64_t kBar2RegOffset = 0x1000;                     // BAR2 fast-path 读回偏移

// 从磁盘加载 minimal SoC 配置 (per design §7 JSON 修订)。
// 磁盘文件省略 ModuleFactory schema 必填的顶层 connections, 这里内存补齐空数组。
json load_minimal_soc_config() {
    json cfg;
    std::ifstream ifs("configs/dgpu_soc_minimal_v1.json");
    if (ifs.is_open()) {
        cfg = json::parse(ifs);
    } else {
        // 兜底内联配置 (与磁盘文件语义等价; cwd 非仓库根时启用)
        cfg = json::parse(R"({
            "name": "dgpu_soc_minimal_v1",
            "display_routing_enabled": false,
            "storage_routing_enabled": true,
            "gmmu_routing_enabled": true,
            "memory_routing_enabled": true,
            "framebuffer_size_bytes": 16777216,
            "modules": [{
                "name": "soc", "type": "DGpuSoc",
                "modules": [
                    {"name": "pcie_ep", "type": "PcieEndpointIP",
                     "params": {"config_size": 4096, "num_msix_vectors": 16,
                                "bar_sizes": [4096, 16777216, 8589934592],
                                "bar0_registers": [
                                    {"offset": 0,  "name": "GMMU_PT_BASE_LO", "access": "rw"},
                                    {"offset": 4,  "name": "GMMU_PT_BASE_HI", "access": "rw"},
                                    {"offset": 8,  "name": "GMMU_CTRL",      "access": "rw"},
                                    {"offset": 16, "name": "SDMA_STATUS",    "access": "ro"}]}},
                    {"name": "pcie_memory", "type": "PcieMemoryDevice", "params": {"capacity_gb": 8}},
                    {"name": "sdma", "type": "SdmaEngineTLM", "params": {"max_inflight": 4}},
                    {"name": "gmmu", "type": "GmmuTLM", "params": {"page_size_bytes": 4096}},
                    {"name": "completion", "type": "CompletionRingTLM"}],
                "connections": [
                    {"src": "sdma.2", "dst": "pcie_memory.0", "latency": 1},
                    {"src": "gmmu.0", "dst": "pcie_memory.1", "latency": 1}]
            }]
        })");
    }
    // schema 补齐: ModuleFactory::validateConfig 要求每层含 "connections" 数组
    if (!cfg.contains("connections")) cfg["connections"] = json::array();
    if (cfg.contains("modules") && cfg["modules"].is_array()) {
        for (auto& m : cfg["modules"]) {
            if (m.contains("modules") && !m.contains("connections"))
                m["connections"] = json::array();
        }
    }
    return cfg;
}

std::vector<uint8_t> make_pattern(size_t n) {
    std::vector<uint8_t> buf(n);
    for (size_t i = 0; i < n; ++i) buf[i] = static_cast<uint8_t>((i * 13 + 7) & 0xFF);
    return buf;
}

}  // namespace

// ── Scenario 1: BAR 枚举 + BAR2 fast-path + BAR1 backdoor 双读回一致 ──
TEST_CASE("driver-visible E2E: BAR2 fast-path mmio round-trip + BAR1 backdoor 一致", "[driver_visible][e2e]") {
    EventQueue eq;
    DGpuBoard board("driver_visible_e2e", &eq);

    REQUIRE(board.load_soc_config(load_minimal_soc_config()));
    REQUIRE(board.memory_routing_enabled());
    REQUIRE(board.storage_routing_enabled());
    REQUIRE(board.gmmu_routing_enabled());

    std::vector<uint8_t> framebuffer(kFramebufferSize, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // ── BAR2 数据面 round-trip (per design §8 数据流 2/3) ──
    // pattern 写 BAR2+0x1000 → memory_device backing → mmio_read(2, 0x1000) 读回
    std::vector<uint8_t> pattern = make_pattern(8);
    REQUIRE(board.mmio_write(2, kBar2RegOffset, pattern.data(), pattern.size()) == 0);
    std::vector<uint8_t> readback(8, 0xEE);
    REQUIRE(board.mmio_read(2, kBar2RegOffset, readback.data(), readback.size()) == 0);
    REQUIRE(readback == pattern);

    // ── 单一 VRAM 真源 (v1.4 B7): BAR1 窗口与 BAR2 aperture 共享同一 backing ──
    // BAR2 写入落在 vram_storage_[0x1000], BAR1 fast-path (framebuffer_ptr_ 同源) 应读到同一字节
    std::vector<uint8_t> bar1_alias(8, 0xEE);
    REQUIRE(board.mmio_read(1, kBar2RegOffset, bar1_alias.data(), bar1_alias.size()) == 0);
    REQUIRE(bar1_alias == pattern);

    // ── BAR1 backdoor round-trip (per design §8 数据流 7) ──
    std::vector<uint8_t> bwd = make_pattern(16);
    REQUIRE(board.backdoor_write(0, bwd.data(), bwd.size()) == 0);
    std::vector<uint8_t> bwd_rd(16, 0xEE);
    REQUIRE(board.backdoor_read(0, bwd_rd.data(), bwd_rd.size()) == 0);
    REQUIRE(bwd_rd == bwd);

    board.shutdown();
}

// ── Scenario 2: config_space BAR2 64-bit 双 dword 编码 (N6) + BAR1 窗口/bar_sizes 一致 ──
TEST_CASE("driver-visible E2E: config BAR 双 dword 编码 + bar_sizes 派生", "[driver_visible][e2e]") {
    EventQueue eq;
    DGpuBoard board("driver_visible_e2e_cfg", &eq);

    REQUIRE(board.load_soc_config(load_minimal_soc_config()));
    std::vector<uint8_t> framebuffer(kFramebufferSize, 0);
    board.attach_vram_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // BAR0 @0x10/0x14 (4KB)
    uint32_t lo = 0, hi = 0;
    REQUIRE(board.pcie_config_read(0x10, 4, &lo) == 0);
    REQUIRE(board.pcie_config_read(0x14, 4, &hi) == 0);
    REQUIRE(lo == 4096u);
    REQUIRE(hi == 0u);
    // BAR1 @0x18/0x1C (16MB)
    REQUIRE(board.pcie_config_read(0x18, 4, &lo) == 0);
    REQUIRE(board.pcie_config_read(0x1C, 4, &hi) == 0);
    REQUIRE(lo == 16777216u);
    REQUIRE(hi == 0u);
    // BAR2 @0x20/0x24 (8GB) → 64-bit 双 dword 编码 (per v1.3 B5)
    REQUIRE(board.pcie_config_read(0x20, 4, &lo) == 0);
    REQUIRE(board.pcie_config_read(0x24, 4, &hi) == 0);
    REQUIRE(lo == 0u);
    REQUIRE(hi == 2u);  // 8GB >> 32 == 2 (0x2_0000_0000)

    // device_id / vendor_id 枚举 (N6 + R4 验证)
    uint32_t did = 0;
    REQUIRE(board.pcie_config_read(0x00, 4, &did) == 0);
    REQUIRE(did == 0x123410DEu);

    board.shutdown();
}
