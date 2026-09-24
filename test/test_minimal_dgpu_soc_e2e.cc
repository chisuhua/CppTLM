// test/test_minimal_dgpu_soc_e2e.cc
// Minimal dGPU SoC v1.0 端到端测试 (Phase C2 of cpptlm-minimal-dgpu-soc-v1-test-coverage)
//   Scenario 1: 全链路 host ABI → PCIe BAR → SDMA → GMMU → framebuffer_ + 双读回一致
//   Scenario 2: fence 完成 → MSI-X vector 0 (kSdmaFenceVector)
//
// 关联 spec: openspec/changes/cpptlm-minimal-dgpu-soc-v1-test-coverage/
//            specs/minimal-dgpu-soc-test-coverage/spec.md
// 作者: CppTLM Team · 日期: 2027-02-10
//
// 关键约束 (per spec + design.md):
//   - load_soc_config 不消费 JSON 顶层 framebuffer_size_bytes → 测试必须显式
//     attach_framebuffer_for_testing(buf, 16MB), 否则 framebuffer_size_=0 → 全返 OUT_OF_RANGE
//   - 初始化顺序 Inv-2: load_soc_config → attach → init (bind_memory_backings 注入 backdoor)
//   - PTE 编码 (paddr & ~0xFFF) | 1 (禁 <<12 偏移错位)
//   - ring_write_entry 提交 H2D 描述符 + doorbell wptr → GMMU translate → vram backdoor memcpy

#include "catch_amalgamated.hpp"
#include "chstream_register.hh"  // 触发全模块注册 (DGpuSoc/SdmaEngineTLM/GmmuTLM/PcieEndpointIP/...)
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/gpu/gmmu_tlm.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "tlm/gpu/sdma_packet.hh"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <thread>
#include <vector>

using namespace tlm::gpu;
using json = nlohmann::json;

namespace {

constexpr uint64_t kFramebufferSize = 16ULL * 1024 * 1024;  // 16 MB (config 声明的尺寸)
constexpr uint64_t kHostBufferSize = 16ULL * 1024;          // 16 KB (≥ phys + size, per design D6)
constexpr uint64_t kPtBase = 0x10000;                        // PT_BASE 寄存器值
constexpr uint64_t kPhys = 0x2000;                           // PTE[1] 译出的 phys
constexpr uint64_t kIova = 0x1000;                           // 描述符 host_iova (idx=1)
constexpr uint32_t kH2dSize = 4096;                          // 单页 H2D 传输
constexpr uint64_t kPteOffset = kPtBase + (kIova >> 12) * 8; // 0x10008 = pt_base + idx*8

// 从磁盘加载 minimal SoC 配置 (per spec "加载 configs/dgpu_soc_minimal_v1.json")。
// 磁盘文件省略了 ModuleFactory schema 必填的 connections 数组 (其 framebuffer_size_bytes
// 语义由 MUST NOT 约束保持不动), 这里在内存中补齐空数组使 validateConfig 通过。
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
            "framebuffer_size_bytes": 16777216,
            "modules": [{
                "name": "soc", "type": "DGpuSoc",
                "modules": [
                    {"name": "pcie_ep", "type": "PcieEndpointIP",
                     "params": {"config_size": 4096, "num_msix_vectors": 16,
                                "bar_sizes": [4096, 16777216],
                                "bar0_registers": [
                                    {"offset": 0,  "name": "GMMU_PT_BASE_LO", "access": "rw"},
                                    {"offset": 4,  "name": "GMMU_PT_BASE_HI", "access": "rw"},
                                    {"offset": 8,  "name": "GMMU_CTRL",      "access": "rw"},
                                    {"offset": 16, "name": "SDMA_STATUS",    "access": "ro"}]}},
                    {"name": "sdma", "type": "SdmaEngineTLM",
                     "params": {"max_inflight": 4, "vram_size_bytes": 16777216}},
                    {"name": "gmmu", "type": "GmmuTLM", "params": {"page_size_bytes": 4096}},
                    {"name": "memory", "type": "MemoryTLM", "params": {"capacity_gb": 1}},
                    {"name": "completion", "type": "CompletionRingTLM"}]
            }]
        })");
    }
    // schema 补齐: ModuleFactory::validateConfig 要求每层 instantiateAll config 含
    // "connections" 数组; 磁盘 minimal config 未写 (顶层 + soc 层)。
    if (!cfg.contains("connections")) cfg["connections"] = json::array();
    if (cfg.contains("modules") && cfg["modules"].is_array()) {
        for (auto& m : cfg["modules"]) {
            if (m.contains("modules") && !m.contains("connections"))
                m["connections"] = json::array();
        }
    }
    return cfg;
}

// 构造确定性 pattern (非平凡, 便于比对)
std::vector<uint8_t> make_pattern(size_t n) {
    std::vector<uint8_t> buf(n);
    for (size_t i = 0; i < n; ++i) buf[i] = static_cast<uint8_t>((i * 13 + 7) & 0xFF);
    return buf;
}

// 驱动全链路 H2D: GMMU 寄存器 (BAR0) + PTE 写 + ring 描述符 + doorbell。
// 前置: board.init() 已调用 (bind_memory_backings 已注入 sdma backdoor/translate cb)。
// 返回 host backdoor 缓冲 (调用方持有, 需在 board/描述符消费期间存活)。
std::vector<uint8_t> drive_full_h2d_link(DGpuBoard& board, SdmaEngineTLM* sdma) {
    // host backdoor 16KB pattern 缓冲 (PTE 写 ≥ phys+size)
    std::vector<uint8_t> host_buf = make_pattern(kHostBufferSize);
    sdma->set_host_backdoor(host_buf.data(), host_buf.size());

    // ring mode (64B entry, per Stage 1.3a)
    sdma->enable_ring_mode(SdmaRingBuffer::RingSize::KB_64, SdmaRingBuffer::EntrySize::B_64);

    // GMMU BAR0 寄存器: PT_BASE LO/HI + enable (offset 0x00/0x04/0x08)
    uint32_t pt_lo = static_cast<uint32_t>(kPtBase);
    REQUIRE(board.mmio_write(0, 0x00, &pt_lo, 4) == 0);
    uint32_t pt_hi = 0;
    REQUIRE(board.mmio_write(0, 0x04, &pt_hi, 4) == 0);
    uint32_t enable = 1;
    REQUIRE(board.mmio_write(0, 0x08, &enable, 4) == 0);

    // PTE[1] 写入 framebuffer (经 BAR1 存储路由): (phys & ~0xFFF) | 1 = 0x2001
    uint64_t pte = (kPhys & ~0xFFFULL) | 1ULL;
    REQUIRE(pte == 0x2001ULL);
    REQUIRE(board.mmio_write(1, kPteOffset, &pte, 8) == 0);

    // H2D 描述符 (iova=0x1000, vram_offset=0, size=4096) 写入 ring entry 0
    DmaDescriptor h2d(DmaDescriptor::Dir::H2D, kIova, /*vram_offset=*/0, kH2dSize, /*tag=*/0x11);
    auto entry = SdmaPacket::serialize_descriptor(h2d);
    REQUIRE(sdma->ring_write_entry(0, entry.data(), entry.size()));

    // doorbell wptr=1 (BAR1+0x10010000) → board 转发 SOC-internal SDMA → ring consume
    uint32_t wptr = 1;
    REQUIRE(board.mmio_write(1, DGpuBoard::kBar1DoorbellOffset, &wptr, 4) == 0);

    return host_buf;
}

}  // namespace

// ── Scenario 1: 全链路 H2D + 双读回一致 ──
TEST_CASE("minimal dgpu soc E2E: 全链路 H2D + 双读回一致", "[minimal_dgpu_soc][e2e]") {
    EventQueue eq;
    DGpuBoard board("minimal_dgpu_soc_e2e", &eq);

    // 顺序 Inv-2: load → attach → init
    REQUIRE(board.load_soc_config(load_minimal_soc_config()));
    REQUIRE(board.storage_routing_enabled());
    REQUIRE(board.gmmu_routing_enabled());
    REQUIRE_FALSE(board.display_routing_enabled());

    std::vector<uint8_t> framebuffer(kFramebufferSize, 0);
    board.attach_framebuffer_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    // SOC-internal SDMA (bind_memory_backings 在 init() 中注入)
    SdmaEngineTLM* sdma = board.sdma_engine();
    REQUIRE(sdma != nullptr);

    std::vector<uint8_t> host_buf = drive_full_h2d_link(board, sdma);

    // 推进至完成 (doorbell 已在 mmio_write 内同步 consume; tick 服务 SOC/inject_q)
    board.tick();

    // ── 链路不变量: ring 已消费、无错误、PTE 落 framebuffer、数据面已写 ──
    REQUIRE(sdma->ring_consumed_count() == 1u);
    REQUIRE(sdma->completed_count() == 1u);
    REQUIRE(sdma->error_count() == 0u);
    {
        // PTE[1] 经 BAR1 存储路由落 framebuffer (GMMU 直读同一份 backing)
        uint64_t pte_back = 0;
        REQUIRE(board.backdoor_read(kPteOffset, &pte_back, 8) == 0);
        REQUIRE(pte_back == 0x2001ULL);
        // phys=0x2000 → framebuffer[0] 应为 pattern[0x2000]=0x07 (H2D 已搬运)
        uint8_t fb0 = 0;
        REQUIRE(board.backdoor_read(0, &fb0, 1) == 0);
        REQUIRE(fb0 == 0x07);  // (0x2000*13 + 7) & 0xFF == 0x07
    }

    // 期望数据: host_buf[0x2000 .. 0x3000) 经 PTE 译出 → framebuffer[0 .. 4096)
    std::vector<uint8_t> expected(host_buf.begin() + kPhys, host_buf.begin() + kPhys + kH2dSize);

    // THEN BAR1 路由读 (mmio_read(1, 0, ...)) 与 backdoor 读同一份 framebuffer_
    std::vector<uint8_t> out_mmio(kH2dSize, 0xEE);
    REQUIRE(board.mmio_read(1, 0, out_mmio.data(), out_mmio.size()) == 0);
    REQUIRE(out_mmio == expected);

    std::vector<uint8_t> out_backdoor(kH2dSize, 0xEE);
    REQUIRE(board.backdoor_read(0, out_backdoor.data(), out_backdoor.size()) == 0);
    REQUIRE(out_backdoor == expected);
    REQUIRE(out_backdoor == out_mmio);

    board.shutdown();
}

// ── Scenario 2: fence 完成触发 MSI-X vector 0 ──
TEST_CASE("minimal dgpu soc E2E: fence 完成触发 MSI-X vector 0", "[minimal_dgpu_soc][e2e]") {
    EventQueue eq;
    DGpuBoard board("minimal_dgpu_soc_fence_e2e", &eq);

    REQUIRE(board.load_soc_config(load_minimal_soc_config()));
    std::vector<uint8_t> framebuffer(kFramebufferSize, 0);
    board.attach_framebuffer_for_testing(framebuffer.data(), framebuffer.size());
    REQUIRE(board.init());

    SdmaEngineTLM* sdma = board.sdma_engine();
    REQUIRE(sdma != nullptr);

    // MSI-X coalescing 显式 disable + 初始化 MSI-X table (delivery 前必需)
    board.set_msix_coalesce_enabled(false);
    REQUIRE(board.msix_init(4, 0) == 0);

    // 上一 Scenario 的描述符链 (H2D) 照常推进
    std::vector<uint8_t> host_buf = drive_full_h2d_link(board, sdma);

    // host irq 回调捕获 (vector == kSdmaFenceVector)
    std::atomic<int> intr_count{0};
    std::atomic<uint32_t> captured_vector{0xFFFF};
    board.set_irq_callback([&](uint32_t vector_id) {
        intr_count.fetch_add(1, std::memory_order_acq_rel);
        captured_vector.store(vector_id, std::memory_order_relaxed);
    });

    // 真实路径接线: SOC-internal SDMA fence → dgpu_board_->sdma_fence_complete
    sdma->set_dgpu_board(&board);

    // 描述符链含 Fence 描述符
    SdmaEngineTLM::FenceDescriptor fence;
    fence.opcode = 0x04;
    fence.fence_id = 1;
    fence.tag = 0x100;
    sdma->submit_fence(fence);

    // 推进至完成 (board.tick → soc tick → sdma tick → process_fence_queue)
    board.tick();

    // 等待异步 irq 回调 (trigger_irq_async detaches thread)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (intr_count.load(std::memory_order_acquire) < 1 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    REQUIRE(intr_count.load() >= 1);
    REQUIRE(captured_vector.load() == SdmaEngineTLM::kSdmaFenceVector);
    REQUIRE(captured_vector.load() == 0u);

    // 数据面 H2D 完成后仍一致 (确认 fence 与数据路径互不破坏)
    std::vector<uint8_t> expected(host_buf.begin() + kPhys, host_buf.begin() + kPhys + kH2dSize);
    std::vector<uint8_t> out(kH2dSize, 0xEE);
    REQUIRE(board.mmio_read(1, 0, out.data(), out.size()) == 0);
    REQUIRE(out == expected);

    board.shutdown();
}
