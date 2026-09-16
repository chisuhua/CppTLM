// include/tlm/pcie/pcie_mock_ip.hh
// PcieMockIP: 独立 PCIe Mock IP（无 TLP/LL/PHY/Mux/SR-IOV 依赖）
// 功能描述：仿 gem5 风格的简化 PCIe 端点组件，profile "pcie_path": "mock" 启用。
//           - BAR 空间：复用 PcieBarRouter 数据驱动机制，mmio_read/write 直接读/写本地 bar_regs_
//           - MSI-X：直接调 cpptlm_intr_deliver_cb_t ABI 回调（无 TLP 投递链）
//           - D3hot gate：对齐 PcieEndpointIP AXI slave 路径（所有 BAR 写 DECERR，无 doorbell 例外）
//           - AXI payload 生成（device-side only）：直接构造 Axi4Bundle 推入内部 Axi4StreamAdapter
//           - 不模拟：TLP 编码/FC/ACK-NAK/PHY/LL/Config Space
//           - 不依赖：PcieEndpointIP / PcieSriovVfPool / 17 端口 SR-IOV
// 目标行数：~500 行（上限 800 行）
// 作者 CppTLM Team / 日期 2027-09-17
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/spec.md §pcie-mock-ip
//       openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P9-3
//       docs/soc_arch/adr/ADR-SOC-17-pcie-mock-ip.md
#ifndef TLM_PCIE_PCIE_MOCK_IP_HH
#define TLM_PCIE_PCIE_MOCK_IP_HH

#include "bundles/axi4_bundles_tlm.hh"
#include "framework/axi4_stream_adapter.hh"
#include "tlm/gpu/pcie_bar_router_mvp.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace cpptlm::pcie {

/**
 * @brief PcieMockIP：独立 PCIe Mock IP（gem5 风格简化端点）
 *
 * 设计原则（per spec.md §pcie-mock-ip + ADR-SOC-17）：
 *   - 独立组件：不继承 SimModule/SimObject，无 TLM 协议栈依赖
 *   - BAR 空间与 PcieEndpointIP 等价语义（复用 PcieBarRouter 数据驱动机制）
 *   - MSI-X 直接 ABI 回调（不经过 TLP 编码/投递链）
 *   - D3hot gate 对齐 PcieEndpointIP AXI slave 路径（全 BAR DECERR，无 doorbell 例外）
 *   - AXI payload 生成（device-side only）：直接构造 Axi4Bundle 推入 Axi4StreamAdapter
 *   - 不模拟：TLP/FC/ACK-NAK/PHY/LL/Config Space/17 端口 SR-IOV
 *   - 不依赖：PcieEndpointIP / PcieSriovVfPool
 *
 * 用法：
 *   PcieMockIP mock;
 *   mock.attach_composition(json_config);
 *   mock.mmio_write(0, 0x1000, &data, 8);  // BAR 写
 *   uint64_t val = mock.mmio_read(0, 0x1000, &buf, 8);  // BAR 读
 *   mock.msix_update_pending(0);  // MSI-X 中断（直接回调）
 */
class PcieMockIP {
public:
    // D3hot 门控返回码（对齐 PcieEndpointIP -EIO 语义, 值 -5）
    static constexpr int kMockEio = -5;

    PcieMockIP() = default;
    ~PcieMockIP() = default;

    // 禁止拷贝/移动（内部含 Axi4StreamAdapter）
    PcieMockIP(const PcieMockIP&) = delete;
    PcieMockIP& operator=(const PcieMockIP&) = delete;
    PcieMockIP(PcieMockIP&&) = delete;
    PcieMockIP& operator=(PcieMockIP&&) = delete;

    // ==================== 配置 ====================
    /** 从 JSON 加载 BAR 配置（复用 PcieBarRouter 数据驱动机制） */
    void attach_composition(const nlohmann::json& cfg);

    // ==================== BAR 空间 ====================
    /** mmio_write：写 BAR 空间。D3hot gate 时返 -EIO。*/
    int mmio_write(uint8_t bar, uint64_t offset, const void* data, std::size_t len);

    /** mmio_read：读 BAR 空间。未初始化寄存器返 0。*/
    int mmio_read(uint8_t bar, uint64_t offset, void* buf, std::size_t len);

    // ==================== PCIe Config ====================
    /** pcie_config_write：处理 PMCSR (offset 0x44) D3hot 写。*/
    int pcie_config_write(uint16_t offset, uint8_t width, uint32_t value);

    /** pcie_config_read：读 Config Space。*/
    int pcie_config_read(uint16_t offset, uint8_t width, uint32_t& value);

    // ==================== MSI-X ====================
    /** msix_init：初始化 MSI-X table（简化，仅记大小）。*/
    int msix_init(uint32_t table_size, uint32_t mask);

    /** msix_update_pending：直接调 intr_cb_（无 TLP 投递链）。*/
    int msix_update_pending(uint32_t vector);

    /** msix_clear_pending：清除 pending 位。*/
    int msix_clear_pending(uint32_t vector);

    /** register_msi_callback：注册中断回调（cpptlm_intr_deliver_cb_t 风格）。*/
    void register_msi_callback(std::function<void(uint32_t vector, uint32_t trans_id)> cb);

    // ==================== Device-Side AXI ====================
    /** device_axi_write：EP 内部设备发起 SOC 输出（如 DMA 完成写 host）。*/
    void device_axi_write(uint64_t addr, const void* data, std::size_t len);

    // ==================== 查询 API ====================
    /** MMIO gate 状态（D3hot 时 true）。*/
    bool is_mmio_gated() const noexcept { return is_mmio_gated_; }

    /** AXI master 请求通道有效。*/
    bool axi_master_req_valid() const;

    /** AXI master 请求数据（测试断言/消费用）。*/
    const bundles::Axi4Bundle& axi_master_req_data() const;

    /** 永不进入 TLP 投递链（Mock 风格）。*/
    bool msix_tlp_pending() const noexcept { return false; }

    // ==================== 内部数据结构（测试可见） ====================
    /** bar_regs_ key = (bar << 60) | (offset & ~0x3) */
    std::unordered_map<uint64_t, uint64_t> bar_regs_;

    /** BAR sizes 数组（6 个 BAR slots）。*/
    std::array<uint64_t, 6> bar_sizes_ = {0, 0, 0, 0, 0, 0};

    /** PcieBarRouter（数据驱动 BAR0 寄存器路由）。*/
    tlm::gpu::PcieBarRouter bar_router_;

    /** Axi4StreamAdapter（device-side AXI payload 生成）。*/
    cpptlm::Axi4StreamAdapter axi_adapter_;

private:
    /** 中断回调。*/
    std::function<void(uint32_t vector, uint32_t trans_id)> intr_cb_ = nullptr;

    /** MSI-X pending bits。*/
    std::vector<uint32_t> msix_pending_;

    /** D3hot MMIO gate。*/
    bool is_mmio_gated_ = false;

    /** 构造 bar_regs_ 复合 key。*/
    static uint64_t bar_key(uint8_t bar, uint64_t offset) noexcept {
        return (static_cast<uint64_t>(bar) << 60) | (offset & ~0x3ULL);
    }
};

} // namespace cpptlm::pcie

#endif // TLM_PCIE_PCIE_MOCK_IP_HH