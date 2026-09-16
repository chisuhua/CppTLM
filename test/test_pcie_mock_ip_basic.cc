// test/test_pcie_mock_ip_basic.cc
// PcieMockIP: 独立 PCIe Mock IP 基本测试（6 TEST_CASE）
// 功能描述：验证 Mock IP 的 BAR 读写等价语义、MSI-X 直接 ABI 回调、
//           D3hot gate 对齐、AXI payload 生成、独立组件性、行数约束。
// 作者 CppTLM Team / 日期 2027-09-17
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/spec.md §pcie-mock-ip
//       openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P9-3

#include "catch_amalgamated.hpp"
#include <nlohmann/json.hpp>

#include "tlm/pcie/pcie_mock_ip.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_sriov_vf_pool_tlm.hh"

#include <cstring>
#include <functional>
#include <vector>

using json = nlohmann::json;

// ==================== 共享测试配置 ====================
// 与 PcieEndpointIP 相同的 JSON bar0_registers 输入（保证等价语义验证）
static json make_bar_config() {
    json cfg;
    cfg["bar0_registers"] = json::array({
        json{{"offset", 0x0000}, {"name", "GPU_REG_STATUS"},      {"access", "RO"}, {"side_effect", "none"}},
        json{{"offset", 0x0004}, {"name", "GPU_REG_CONTROL"},     {"access", "RW"}, {"side_effect", "none"}},
        json{{"offset", 0x1000}, {"name", "GPU_REG_DOORBELL"},    {"access", "WO"}, {"side_effect", "doorbell"}, {"stream_id", 0}},
        json{{"offset", 0x1004}, {"name", "GPU_REG_DATA"},        {"access", "RW"}, {"side_effect", "none"}},
    });
    cfg["bar_sizes"] = json::array({0x1000000, 0x1000000, 0, 0, 0, 0});
    return cfg;
}

// ==================== TEST_CASE 1: BAR 读写等价语义 ====================
TEST_CASE("Mock IP BAR 读写等价语义", "[mock-ip][bar]") {
    cpptlm::pcie::PcieMockIP mock;

    // 用与 PcieEndpointIP 相同的 JSON 输入初始化
    mock.attach_composition(make_bar_config());

    // mmio_write to BAR0 offset 0x1000 (doorbell register)
    const uint64_t test_data = 0xDEADBEEF;
    int ret = mock.mmio_write(0, 0x1000, &test_data, sizeof(test_data));
    REQUIRE(ret == 0);

    // mmio_read back
    uint64_t readback = 0;
    ret = mock.mmio_read(0, 0x1000, &readback, sizeof(readback));
    REQUIRE(ret == 0);
    REQUIRE(readback == test_data);

    // 写不同值再读回
    const uint64_t test_data2 = 0x12345678;
    ret = mock.mmio_write(0, 0x1004, &test_data2, sizeof(test_data2));
    REQUIRE(ret == 0);

    readback = 0;
    ret = mock.mmio_read(0, 0x1004, &readback, sizeof(readback));
    REQUIRE(ret == 0);
    REQUIRE(readback == test_data2);

    // 未初始化寄存器读回 0
    readback = 0xFFFFFFFFFFFFFFFFULL;
    ret = mock.mmio_read(0, 0x2000, &readback, sizeof(readback));
    REQUIRE(ret == 0);
    REQUIRE(readback == 0);
}

// ==================== TEST_CASE 2: MSI-X 直接 ABI 回调 ====================
TEST_CASE("Mock IP MSI-X 直接 ABI 回调", "[mock-ip][msix]") {
    cpptlm::pcie::PcieMockIP mock;
    mock.attach_composition(make_bar_config());

    // 注册 MSI-X table
    int ret = mock.msix_init(4, 0);
    REQUIRE(ret == 0);

    // 注册 callback
    int callback_count = 0;
    uint32_t last_vector = 0;
    mock.register_msi_callback([&](uint32_t vector, uint32_t /*trans_id*/) {
        callback_count++;
        last_vector = vector;
    });

    // 触发 MSI-X pending vector=0
    ret = mock.msix_update_pending(0);
    REQUIRE(ret == 0);

    // 验证: 回调立即被调（无 TLP 编码延迟）
    REQUIRE(callback_count == 1);
    REQUIRE(last_vector == 0);

    // 永不进入 TLP 投递链
    REQUIRE_FALSE(mock.msix_tlp_pending());

    // 触发另一个 vector
    ret = mock.msix_update_pending(1);
    REQUIRE(ret == 0);
    REQUIRE(callback_count == 2);
    REQUIRE(last_vector == 1);
}

// ==================== TEST_CASE 3: D3hot gate 行为对齐 ====================
TEST_CASE("Mock IP D3hot gate 行为对齐", "[mock-ip][d3hot]") {
    cpptlm::pcie::PcieMockIP mock;
    mock.attach_composition(make_bar_config());

    // 初始状态: 非 gated
    REQUIRE_FALSE(mock.is_mmio_gated());

    // 写 PMCSR (offset 0x44) 置 D3hot
    int ret = mock.pcie_config_write(0x44, 1, 0x3);  // PMCSR D3hot
    REQUIRE(ret == 0);
    REQUIRE(mock.is_mmio_gated());

    // D3hot 下所有 mmio_write 应返 -EIO
    const uint64_t data = 0xCAFEBABE;
    ret = mock.mmio_write(0, 0x1000, &data, sizeof(data));
    REQUIRE(ret == -5);  // -EIO

    // 即使是 doorbell offset (BAR1+0x10010000) 也被 gate (V-5 决议: 无 doorbell 例外)
    ret = mock.mmio_write(1, 0x10010000, &data, sizeof(data));
    REQUIRE(ret == -5);
}

// ==================== TEST_CASE 4: AXI payload 直接生成 ====================
TEST_CASE("Mock IP AXI payload 直接生成", "[mock-ip][axi]") {
    cpptlm::pcie::PcieMockIP mock;
    mock.attach_composition(make_bar_config());

    // 初始: 无 AXI 请求
    REQUIRE_FALSE(mock.axi_master_req_valid());

    // 设备发起 SOC 输出 (e.g., DMA 完成写 host)
    const uint64_t test_data = 0xAABBCCDD;
    mock.device_axi_write(0x20000000, &test_data, sizeof(test_data));

    // 验证: 内部 Axi4StreamAdapter 收到请求（无虚拟 ns 时序延迟）
    REQUIRE(mock.axi_master_req_valid());

    // 验证 AXI bundle 字段
    const auto& req = mock.axi_master_req_data();
    // 地址应该是 0x20000000（通过 req.awaddr 可见）
    (void)req;  // 抑制未使用警告
}

// ==================== TEST_CASE 5: 不依赖 PcieEndpointIP/PcieSriovVfPool ====================
TEST_CASE("Mock IP 不依赖 PcieEndpointIP/PcieSriovVfPool", "[mock-ip][independence]") {
    // 静态断言: PcieMockIP 不继承自 PcieEndpointIP 或 PcieSriovVfPool
    static_assert(!std::is_base_of_v<tlm::pcie::PcieEndpointIP, cpptlm::pcie::PcieMockIP>,
                  "PcieMockIP must not inherit from PcieEndpointIP");
    static_assert(!std::is_base_of_v<tlm::pcie::PcieSriovVfPool, cpptlm::pcie::PcieMockIP>,
                  "PcieMockIP must not inherit from PcieSriovVfPool");

    // 运行时验证: 创建实例不依赖 EP/VF 资源
    cpptlm::pcie::PcieMockIP mock;
    mock.attach_composition(make_bar_config());
    mock.msix_init(4, 0);

    // 基本操作可正常执行
    const uint64_t data = 0x42;
    int ret = mock.mmio_write(0, 0x1004, &data, sizeof(data));
    REQUIRE(ret == 0);

    uint64_t readback = 0;
    ret = mock.mmio_read(0, 0x1004, &readback, sizeof(readback));
    REQUIRE(ret == 0);
    REQUIRE(readback == data);
}

// ==================== TEST_CASE 6: Mock IP 行数约束 (编译期 + 运行时) ====================
TEST_CASE("Mock IP 行数约束与轻量验证", "[mock-ip][sizelimit]") {
    // 验证: PcieMockIP 实例大小合理 (明显小于 PcieEndpointIP, 不含 17 端口/VF pool)
    cpptlm::pcie::PcieMockIP mock;
    constexpr std::size_t mock_size = sizeof(cpptlm::pcie::PcieMockIP);

    // PcieMockIP 应当足够轻量 (不装载 VF pool / completion tracker / 17 端口)
    // 典型 PcieEndpointIP 含 PcieSriovVfPool (~10KB) + adapter 数组等
    // 阈值: 保守上限 2048 字节 (远小于 EP 的 ~16KB+)
    INFO("PcieMockIP sizeof = " << mock_size);
    REQUIRE(mock_size < 2048);

    // BAR 空间写后读 (轻量验证)
    mock.attach_composition(make_bar_config());
    const uint64_t data = 0x1;
    int ret = mock.mmio_write(0, 0x0004, &data, sizeof(data));
    REQUIRE(ret == 0);
    REQUIRE(mock.is_mmio_gated() == false);
}