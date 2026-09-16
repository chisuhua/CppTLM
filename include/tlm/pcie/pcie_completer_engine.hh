// include/tlm/pcie/pcie_completer_engine.hh
// PcieCompleterEngine: PCIe Completer Engine (CFGrd/CFGwr/MRdr/MWr/MEMr/MEMw 全分支)
// 功能描述：替换 PcieSriovVfPool::dispatch_tlp 的 default: return true 占位，
//           实现 CFGrd 真实读取 ConfigSpace + CplD 回发、MMIO/MEM BAR 路由等。
//           - BDF→BAR 路由表从 config space BAR 寄存器动态构建
//           - CplD 通过 PcieTlpCodec::encode_cpld API 生成（禁止 inline CRC）
//           - 自包含状态，可脱离 PcieEndpointIP 独立测试
// 作者 CppTLM Team / 日期 2026-09-17
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P10-1
#ifndef TLM_PCIE_PCIE_COMPLETER_ENGINE_HH
#define TLM_PCIE_PCIE_COMPLETER_ENGINE_HH

#include "bundles/pcie_bundles_tlm.hh"

#include <array>
#include <cstdint>
#include <unordered_map>

namespace cpptlm::pcie {

/**
 * @brief PCIe Completer Engine — 替换 dispatch_tlp 占位
 *
 * 自包含工具类，内部持有 config_space_ (4KB) + bar_store_ (BAR 空间)。
 * 在 dispatch_tlp 集成中，通过 config_space_write/read 同步真实 ConfigSpace 值。
 *
 * handle_tlp() 是核心入口：
 *   - CFG_READ/MMIO_READ/MEM_READ → 返回 CplD (PcieTlpBundle with kind=CPLD)
 *   - CFG_WRITE/MMIO_WRITE/MEM_WRITE → 更新内部状态，返回空的 PcieTlpBundle (kind=0)
 *
 * CplD 生成强制走 PcieTlpCodec::encode_cpld API，不 inline 实现 CRC/TLP header。
 */
class PcieCompleterEngine {
public:
    static constexpr std::size_t CONFIG_SPACE_SIZE = 4096;  // 4KB

    PcieCompleterEngine();
    ~PcieCompleterEngine() = default;

    // ========== ConfigSpace 读/写 ==========
    void config_space_write(uint16_t offset, uint8_t width, uint32_t value);
    uint32_t config_space_read(uint16_t offset, uint8_t width);

    // ========== BAR 路由表 ==========
    /** 从 config_space_ BAR 寄存器动态重建 BDF→BAR 路由表 (CFG_WRITE BAR 后触发) */
    void rebuild_bdf_bar_route_table();
    /** 查询 BDF+bar_index → BAR 基地址 */
    uint64_t route_bdf_to_bar(uint16_t bdf, uint8_t bar) const;

    // ========== 核心: 处理 TLP, 返回 CplD ==========
    /** 处理 TLP: 读 → CplD; 写 → 更新状态 + 返回空 bundle (kind=0) */
    bundles::PcieTlpBundle handle_tlp(const bundles::PcieTlpBundle& tlp);

    // ========== bar_store_ 测试 helper ==========
    uint64_t bar_store_value(uint8_t bar, uint64_t offset) const;
    void bar_store_write(uint8_t bar, uint64_t offset, uint64_t value);

    // ========== 便捷 accessor ==========
    uint16_t completer_id() const { return completer_id_; }
    void set_completer_id(uint16_t id) { completer_id_ = id; }

private:
    // ========== 私有处理函数 ==========
    bundles::PcieTlpBundle handle_cfgrd(const bundles::PcieTlpBundle& req);
    bundles::PcieTlpBundle handle_cfgwr(const bundles::PcieTlpBundle& req);
    bundles::PcieTlpBundle handle_mmio_read(const bundles::PcieTlpBundle& req);
    bundles::PcieTlpBundle handle_mmio_write(const bundles::PcieTlpBundle& req);
    bundles::PcieTlpBundle handle_mem_read(const bundles::PcieTlpBundle& req);
    bundles::PcieTlpBundle handle_mem_write(const bundles::PcieTlpBundle& req);

    /** 生成 CplD (强制使用 PcieTlpCodec::encode_cpld) */
    bundles::PcieTlpBundle generate_cpld(uint16_t requester_id,
                                          uint8_t tag, uint16_t byte_count,
                                          const uint8_t* data, std::size_t len);

    /** 内部 helper: (bar_index, offset) → bar_store key */
    static uint64_t bar_store_key(uint8_t bar, uint64_t offset) {
        return (static_cast<uint64_t>(bar) << 60) | (offset & 0x0FFFFFFFFFFFFFFFull);
    }

    // ========== 内部状态 ==========
    std::array<uint32_t, CONFIG_SPACE_SIZE / 4> config_space_{};
    std::unordered_map<uint64_t, uint64_t> bar_store_;
    // BAR 基地址缓存 (rebuild_bdf_bar_route_table 后有效)
    std::array<uint64_t, 6> bar_bases_{};
    uint16_t completer_id_ = 0x0200;  // 默认 Completer ID
};

} // namespace cpptlm::pcie

#endif // TLM_PCIE_PCIE_COMPLETER_ENGINE_HH