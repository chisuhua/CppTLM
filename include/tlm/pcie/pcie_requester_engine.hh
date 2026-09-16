// include/tlm/pcie/pcie_requester_engine.hh
// PcieRequesterEngine: PCIe Requester Engine (MRd 发起 + tag 分配 + CplD 关联 + 超时)
// 功能描述：让 EP 主动发起 MRd/MWr TLP（此前 link_layer()->tx_tlp() 在生产代码中零调用）。
//           - tag 分配 12-bit per-VF (1-4095, 0 保留)
//           - MRd 构造 + tx_tlp 调度 + fc_downstream_ NP credit 消耗
//           - CplD 接收 → tag 反查 outstanding → completion callback
//           - 虚拟 ns 超时扫描 → error_cb 回调
// 作者 CppTLM Team / 日期 2027-02-09
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P11-1
//       spec.md §requester-engine-outgoing
#ifndef TLM_PCIE_PCIE_REQUESTER_ENGINE_HH
#define TLM_PCIE_PCIE_REQUESTER_ENGINE_HH

#include "bundles/pcie_bundles_tlm.hh"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tlm::pcie {

class PcieLinkLayer;

} // namespace tlm::pcie

namespace cpptlm::pcie {

/**
 * @brief PCIe Requester Engine — EP 主动发起 MRd/MWr TLP
 *
 * 自包含工具类，内部持有 outstanding 表 (tag → 请求元数据) + 虚拟 ns 计时器。
 * 核心方法:
 *   - mrd_read(): 分配 tag → 构造 MRd → tx_tlp(link_layer)
 *   - on_cpld_received(): CplD 到达时 tag 反查 → completion callback
 *   - tick(): 驱动超时扫描
 *
 * MRd 编码强制使用 PcieTlpCodec::encode_mrd API (不 inline 实现 CRC/TLP header)。
 */
class PcieRequesterEngine {
public:
    /// Completion 回调: tag + CplD bundle
    using CompletionCallback = std::function<void(uint16_t tag,
                                                   const bundles::PcieTlpBundle& cpld)>;

    /// 错误回调: tag + 错误描述
    using ErrorCallback = std::function<void(uint32_t tag,
                                              const std::string& reason)>;

    /**
     * @brief 构造
     * @param ll 关联的 PcieLinkLayer 实例 (tx_tlp 调用目标)
     * @param timeout_ns 默认 completion 超时 (虚拟 ns, 默认 1ms)
     */
    explicit PcieRequesterEngine(tlm::pcie::PcieLinkLayer* ll,
                                  uint64_t timeout_ns = 1000000);

    ~PcieRequesterEngine() = default;

    // ========== 核心操作 ==========

    /**
     * @brief 发起 MRd TLP
     * @param bdf Requester BDF
     * @param addr 目标地址 (PA/IOVA)
     * @param lower_addr lower address (简化, 当前保留)
     * @param len 请求字节数
     * @return true 发送成功 (tag 已分配 + tx_tlp 成功)
     *         false tag 耗尽 或 FC 不足
     */
    bool mrd_read(uint16_t bdf, uint64_t addr,
                  uint32_t lower_addr, std::size_t len);

    /**
     * @brief CplD 接收 (从 LinkLayer 路由过来)
     * @param cpld 收到的 CplD bundle, trans_id 字段包含 tag
     */
    void on_cpld_received(const bundles::PcieTlpBundle& cpld);

    /**
     * @brief 周期 tick: 推进虚拟 ns 时钟 + 超时扫描
     * @param elapsed_ns 自上次 tick 以来经过的虚拟 ns
     */
    void tick(uint64_t elapsed_ns);

    // ========== 查询 ==========

    /// 当前 outstanding 的 MRd 请求数
    std::size_t outstanding_count() const noexcept {
        return outstanding_.size();
    }

    /// 最近分配的 tag (诊断/测试)
    uint16_t last_allocated_tag() const noexcept {
        return last_tag_;
    }

    // ========== Callback 注册 ==========

    void register_completion_callback(CompletionCallback cb) noexcept {
        completion_cb_ = std::move(cb);
    }

    void register_error_callback(ErrorCallback cb) noexcept {
        error_cb_ = std::move(cb);
    }

private:
    /// outstanding 请求元数据
    struct Outstanding {
        uint16_t tag;
        uint16_t bdf;
        uint64_t addr;
        uint64_t issued_at_ns;
        uint64_t timeout_ns;
    };

    /// 分配 tag (12-bit, 1-4095, 0=无效/耗尽)
    uint16_t allocate_tag();

    /// 完成 outstanding
    void complete_outstanding(uint16_t tag,
                              const bundles::PcieTlpBundle& cpld);

    /// 超时 outstanding
    void timeout_outstanding(uint16_t tag);

    // ========== 内部状态 ==========

    tlm::pcie::PcieLinkLayer* link_layer_ = nullptr;
    std::unordered_map<uint16_t, Outstanding> outstanding_;
    uint64_t current_time_ns_ = 0;
    uint64_t default_timeout_ns_;
    CompletionCallback completion_cb_;
    ErrorCallback error_cb_;
    uint16_t last_tag_ = 0;
};

} // namespace cpptlm::pcie

#endif // TLM_PCIE_PCIE_REQUESTER_ENGINE_HH