// test_dgpu_pcie_device_perspective.cc
// PCIe driver perspective tests using DGpuBoard C++ shell (per ADR-SOC-07 D7).
// Replaces s2 monolith (per design.md §5 stage-1 deprecation).
// Uses:
//   - board.mmio_write()         for BAR0 register access (doorbell, GPFIFO_PUT)
//   - board.backdoor_write/read() for BAR1 VRAM (per ADR-SOC-07 Q3)
//   - board.device_info()        for BAR sizes (replaces bar0_size/bar1_size)
// State-observability assertions (cp_is_idle, sq_pending_count, doorbell_sq_tail)
// deferred to follow-up shell accessor work.
// Author: CppTLM Team
// Date: 2026-08-29 (T-bs-4 stage-1 adaptation)

#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
    constexpr uint32_t GPU_REG_GPFIFO_PUT = 0x0000;
    constexpr uint32_t GPU_REG_DOORBELL = 0x0014;
    constexpr size_t VRAM_TEST_OFFSET = 0x2000;

    nlohmann::json load_board_config() {
        std::ifstream f("configs/dgpu_board_v1.json");
        nlohmann::json j;
        f >> j;
        // ptx_emu_root 必须指向真实路径才能加载 PTX-EMU submodule;
        // 测试环境若未配 PTX-EMU, SOC 初始化会被 PtxEmuSubmodule 失败跳过,
        // 这里用 /tmp 占位使 init() 至少走到 SOC 装配阶段 (load_soc_config 不触发 PTX-EMU 加载).
        j["params"]["ptx_emu_root"] = "/tmp/test-ptx-emu";
        return j;
    }
} // namespace

TEST_CASE("PCIe driver perspective: BAR0 doorbell write (shell path)",
          "[pcie][dGPU][soc][stage-1]") {
    EventQueue eq;
    tlm::gpu::DGpuBoard board("pcie_dgpu", &eq);
    auto cfg = load_board_config();
    REQUIRE(board.load_soc_config(cfg));

    uint32_t val = 0x00000001;
    REQUIRE(board.mmio_write(0, GPU_REG_DOORBELL, &val, sizeof(val)) == 0);
    // cp_is_idle assertion deferred: requires board.soc()->cp() accessor
    // (per design.md §5 stage-1 deprecation path; T-bs-4 后置 work item).
}

TEST_CASE("PCIe driver perspective: BAR1 VRAM write/read round trip (shell backdoor)",
          "[pcie][dGPU][soc][stage-1]") {
    EventQueue eq;
    tlm::gpu::DGpuBoard board("pcie_dgpu", &eq);
    auto cfg = load_board_config();
    REQUIRE(board.load_soc_config(cfg));

    REQUIRE(board.device_info().bar_sizes[1] == 256ULL * 1024ULL * 1024ULL);

    const std::array<uint8_t, 16> image = {0x50, 0x54, 0x58, 0x49, 0x52, 0x00, 0x01, 0x00,
                                           0xDE, 0xAD, 0xBE, 0xEF, 0x11, 0x22, 0x33, 0x44};
    std::array<uint8_t, 16> readback{};

    REQUIRE(board.backdoor_write(VRAM_TEST_OFFSET, image.data(), image.size()) == 0);
    board.tick(); // drain inject_q (per design §2.5 #5)
    REQUIRE(board.backdoor_read(VRAM_TEST_OFFSET, readback.data(), readback.size()) == 0);
    REQUIRE(readback == image);
}

TEST_CASE("PCIe driver perspective: invalid BAR1 DMA bounds fail with errno",
          "[pcie][dGPU][soc][stage-1]") {
    EventQueue eq;
    tlm::gpu::DGpuBoard board("pcie_dgpu", &eq);
    auto cfg = load_board_config();
    REQUIRE(board.load_soc_config(cfg));

    uint8_t byte = 0xFF;
    const auto bar1_size = board.device_info().bar_sizes[1];

    REQUIRE(board.backdoor_write(bar1_size, &byte, 1) == -22);
    REQUIRE(board.backdoor_read(bar1_size, &byte, 1) == -22);
    REQUIRE(board.backdoor_write(0, nullptr, 1) == -22);
}

TEST_CASE("PCIe driver perspective: GPFIFO PUT register is an MMIO address",
          "[pcie][dGPU][soc][stage-1]") {
    EventQueue eq;
    tlm::gpu::DGpuBoard board("pcie_dgpu", &eq);
    auto cfg = load_board_config();
    REQUIRE(board.load_soc_config(cfg));

    uint32_t val = 4;
    REQUIRE(board.mmio_write(0, GPU_REG_GPFIFO_PUT, &val, sizeof(val)) == 0);
    board.tick();
    // sq_inflight_count assertion deferred (requires shell->soc()->sq() accessor).
}

// Deferred to follow-up per design.md §5 stage-1:
//   - "IOCTL 0x27/0x28/0x29 ABI path"
//   - "PUSHBUFFER submit then MMIO doorbell" (depends on UsrLinuxEmuIoctlStub IOCTL stub)
// Both depend on UsrLinuxEmuIoctlStub being refactored to attach DGpuBoard shell,
// which is sequenced into T-bs-4 follow-up + subsequent adapter iteration.