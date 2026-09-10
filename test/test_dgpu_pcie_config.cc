// test/test_dgpu_pcie_config.cc
// 阶段 1.1.1 (修复 #3): DGpuBoard::pcie_config_read/write 转发到
// PcieEndpointTLM::config_space() (PcieConfigSpace)。
// 语义契约 (per openspec/changes/2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes/spec.md):
//   - 有效 SOC+cfg_space: 转发 read/write, 成功返 0, val 含 Vendor ID (低 16 bit)
//   - null val 指针:     返 -EINVAL (-22)
//   - SOC 未实例化:      返 -ENOSYS (-38), 不 segfault
// 测试沿用 D15 (5425c45) 模式: inline JSON + load_soc_config 走真实生产路径。
#include <catch_amalgamated.hpp>
#include <cerrno>
#include "core/event_queue.hh"
#include "tlm/gpu/dgpu_board_shell.hh"
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using json = nlohmann::json;

// D15 mini board cfg: 最小真实 SOC (DGpuSoc + PcieEndpointTLM), inline JSON 避免 cwd 依赖。
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

TEST_CASE("pcie_config_read returns Vendor ID 0x10DE via cfg_space after SOC instantiate",
          "[dgpu][pcie][config][shell]") {
    DGpuBoard board("cfg_test_board");
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));
    REQUIRE(board.init());

    uint32_t val = 0;
    // 转发命中 PcieConfigSpace::read(0x00): regs_[0] = (0x1234 << 16) | 0x10DE
    REQUIRE(board.pcie_config_read(0x00, 4, &val) == 0);
    REQUIRE((val & 0xFFFFu) == 0x10DE); // Vendor ID 低 16 bit (NVIDIA)

    board.shutdown();
}

TEST_CASE("pcie_config_write forwards to cfg_space after SOC instantiate",
          "[dgpu][pcie][config][shell]") {
    DGpuBoard board("cfg_test_board");
    REQUIRE(board.load_soc_config(d15_mini_board_cfg()));
    REQUIRE(board.init());

    // 0x04 (Command/Status 寄存器区) 可写, read-back 验证转发
    REQUIRE(board.pcie_config_write(0x04, 4, 0xDEADBEEFu) == 0);
    uint32_t val = 0;
    REQUIRE(board.pcie_config_read(0x04, 4, &val) == 0);
    REQUIRE(val == 0xDEADBEEFu);

    board.shutdown();
}

TEST_CASE("pcie_config_read null val returns -EINVAL", "[dgpu][pcie][config][shell]") {
    DGpuBoard board("cfg_test_board");
    board.init();
    REQUIRE(board.pcie_config_read(0x00, 4, nullptr) == -EINVAL);
    board.shutdown();
}

TEST_CASE("pcie_config_read/write return -ENOSYS when SOC not instantiated",
          "[dgpu][pcie][config][shell]") {
    DGpuBoard board("cfg_test_board");
    board.init();
    uint32_t val = 0;
    REQUIRE(board.pcie_config_read(0x00, 4, &val) == -ENOSYS);
    REQUIRE(board.pcie_config_write(0x04, 4, 0xDEADBEEFu) == -ENOSYS);
    board.shutdown();
}
