// include/tlm/pcie/pcie_bypass_mux.hh
// PcieBypassMux: 3 态 Bypass Mux (Full / Bypass / Partial) + DrainPolicy
// 功能描述：
//   - 3 态模式切换: Full (PHY+LL+TL+AXI), Bypass (TL+AXI), Partial (LL+TL+AXI)
//   - apply_mode 10 步清理 (per design §7):
//       0. notify_peer_mode_change
//       1. pause link layer (link_paused_ = true)
//       2. drain or abort in-flight TLP
//       3. clear retry buffer to ack seq
//       4. reset seq# counters
//       5. reset FC token buckets
//       6. Partial guard (require PHY initialized)
//       7. clear MSI-X pending
//       8. commit mode
//       9. notify_peer_mode_complete
//      10. resume link layer (link_paused_ = false)
//   - DrainPolicy: GRACEFUL_DRAIN (等待 in-flight 完成) 或 IMMEDIATE_ABORT
//
// 作者 CppTLM Team / 日期 2026-10-06
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §7
//       decisions.md §Q7 + §Q14
//       openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/proposal.md
#ifndef TLM_PCIE_PCIE_BYPASS_MUX_HH
#define TLM_PCIE_PCIE_BYPASS_MUX_HH

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace tlm::pcie {

// 前向声明 (PcieLinkLayer 在 pcie_link_layer_tlm.hh)
class PcieLinkLayer;

/**
 * @brief Bypass Mux 模式枚举 (per design.md §7 / Q7)
 */
enum class BypassMode : uint8_t {
    Full    = 0,  // §1 PHY ↔ §2 LL ↔ §3 TL ↔ §6 AXI (完整 PCIe 链路)
    Bypass  = 1,  // §3 TL ↔ §6 AXI (跳过 PHY+LL, 快速软件 bring-up)
    Partial = 2   // §2 LL ↔ §3 TL ↔ §6 AXI (跳过 PHY, 保留 LL+FC)
};

/**
 * @brief In-flight 事务处理策略 (per design.md §7)
 */
enum class DrainPolicy : uint8_t {
    GRACEFUL_DRAIN  = 0,  // 等在途事务完成 (有超时, 默认 1µs)
    IMMEDIATE_ABORT = 1   // 立即 abort + 记录中断通知
};

/**
 * @brief PcieBypassMux: EP 内部 Bypass Mux (3 态 + DrainPolicy)
 *
 * 不可拷贝 (持有 PcieLinkLayer* / 内部计数器)
 */
class PcieBypassMux {
public:
    PcieBypassMux() = default;
    explicit PcieBypassMux(PcieLinkLayer* ll) noexcept : link_layer_(ll) {}

    // 不可拷贝
    PcieBypassMux(const PcieBypassMux&) = delete;
    PcieBypassMux& operator=(const PcieBypassMux&) = delete;

    // ========== 模式查询 ==========
    [[nodiscard]] BypassMode mode() const noexcept { return mode_; }
    [[nodiscard]] DrainPolicy drain_policy() const noexcept { return drain_policy_; }
    [[nodiscard]] PcieLinkLayer* link_layer() const noexcept { return link_layer_; }

    // ========== 配置 ==========
    void set_link_layer(PcieLinkLayer* ll) noexcept { link_layer_ = ll; }
    void set_drain_policy(DrainPolicy p) noexcept { drain_policy_ = p; }
    void set_phy_initialized(bool ok) noexcept { phy_initialized_ = ok; }
    [[nodiscard]] bool phy_initialized() const noexcept { return phy_initialized_; }

    // ========== 模式切换 (10 步清理) ==========
    // 抛出 std::logic_error 当 Partial 模式要求 PHY 已初始化但未初始化
    void apply_mode(BypassMode new_mode,
                    DrainPolicy policy = DrainPolicy::GRACEFUL_DRAIN);

    // ========== C5: Surprise Removal 清理 (Q14) ==========
    // PRSNT# 移除时: abort in-flight + 清 retry/seq/FC + MSI-X pending → Detect
    void surprise_removal_cleanup() noexcept;

    // ========== 状态查询 (供测试 + 诊断) ==========
    [[nodiscard]] bool link_paused() const noexcept { return link_paused_; }

    // In-flight TLP 计数 (模拟下游提交方注入)
    void add_in_flight_tlp(std::size_t n) noexcept { in_flight_tlps_ += n; }
    [[nodiscard]] std::size_t in_flight_tlps() const noexcept { return in_flight_tlps_; }
    [[nodiscard]] std::size_t aborted_tlps() const noexcept { return aborted_tlps_; }

    // MSI-X pending 计数
    void set_msix_pending(std::size_t n) noexcept { msix_pending_ = n; }
    [[nodiscard]] std::size_t msix_pending() const noexcept { return msix_pending_; }

    // 对端通知次数 (切换开始/完成各 +1)
    [[nodiscard]] std::size_t peer_mode_change_count() const noexcept {
        return peer_mode_change_count_;
    }
    [[nodiscard]] std::size_t peer_mode_complete_count() const noexcept {
        return peer_mode_complete_count_;
    }

    // ========== PcieEndpointTLM composition 注册表（冻结 .h 布局下的集成通道）==========
    // 与 PcieLinkLayer::attach_to_endpoint 同机制：PcieEndpointTLM 头文件类成员布局
    // 冻结（23 ABI），不能加成员 → 通过静态注册表按 EP 模块名关联。
    // 由 src/tlm/gpu/pcie_endpoint_tlm.cc 在 on_config_loaded() 时挂接。
    static PcieBypassMux* attach_to_endpoint(const std::string& endpoint_name,
                                             PcieLinkLayer* ll);
    static PcieBypassMux* for_endpoint(const std::string& endpoint_name) noexcept;
    static void detach_from_endpoint(const std::string& endpoint_name) noexcept;

private:
    PcieLinkLayer* link_layer_ = nullptr;
    BypassMode mode_ = BypassMode::Full;
    DrainPolicy drain_policy_ = DrainPolicy::GRACEFUL_DRAIN;
    bool link_paused_ = false;
    bool phy_initialized_ = false;
    std::size_t in_flight_tlps_ = 0;
    std::size_t aborted_tlps_ = 0;
    std::size_t msix_pending_ = 0;
    std::size_t peer_mode_change_count_ = 0;
    std::size_t peer_mode_complete_count_ = 0;

    void notify_peer_mode_change(BypassMode) noexcept { ++peer_mode_change_count_; }
    void notify_peer_mode_complete(BypassMode) noexcept { ++peer_mode_complete_count_; }
    void pause_link_layer() noexcept { link_paused_ = true; }
    void resume_link_layer() noexcept { link_paused_ = false; }
    void drain_in_flight();
    void clear_retry_buffer_to_ack();
    void reset_seq_counters();
    void reset_fc_buckets();
    void clear_msix_pending();
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_BYPASS_MUX_HH
