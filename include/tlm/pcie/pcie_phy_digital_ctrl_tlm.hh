// include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh
// PciePhyDigitalCtrl: PCIe PHY Digital Control（LTSSM FSM + Gen3+ 均衡 + 速率切换 + 热插拔）
// 功能描述（per design.md §5）：
//   - LTSSM 11 主状态机: Detect / Polling / Configuration / Recovery / L0 /
//     L0s / L1 / L2 / Disabled / Loopback / Hot_Reset
//     (Hot-Plug 是平台机制非 LTSSM 状态, per Oracle 修订; Hot_Reset 是 Recovery 内子状态)
//   - Gen3+ 均衡协商: 8 Preset (per Gen5 spec §8.3.1), TS1/TS2 序列交互,
//     Phase 2 (TX) / Phase 3 (RX) EQ FSM (per Oracle Q1: Gen5 ≠ FLIT, EQ 是 Gen3+ 通用特性)
//   - 速率切换: 调用 PcieLinkLayer::trigger_rate_switch(from, to) (修 Phase 2 评审 #1)
//   - 热插拔: PRSNT# / MRL / PWRGOOD / REFCLK / PERST# 信号 (per Q14 Surprise Removal)
//
// 事件机制: EventQueue 驱动 (tick() 每 cycle 推进 FSM), 状态转换即时生效。
// 作者 CppTLM Team / 日期 2026-10-06
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §5/§6.2/§7
//       decisions.md §Q3/§Q14/§Q16
//       openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/proposal.md
#ifndef TLM_PCIE_PCIE_PHY_DIGITAL_CTRL_TLM_HH
#define TLM_PCIE_PCIE_PHY_DIGITAL_CTRL_TLM_HH

#include "bundles/pcie_dllp_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_encoding_latency_model.hh"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace tlm::pcie {

class PcieLinkLayer;
class PcieBypassMux;

/**
 * @brief LTSSM 11 主状态 (per design.md §5)
 *
 * Hot_Reset 是 Recovery 内的子状态 (进入 Recovery 后由 PERST#/Hot Reset 触发)。
 * 不包含 Hot-Plug (平台机制, Oracle 修订)。
 */
enum class LtState : uint8_t {
    Detect        = 0,
    Polling       = 1,
    Configuration = 2,
    Recovery      = 3,
    L0            = 4,
    L0s           = 5,
    L1            = 6,
    L2            = 7,
    Disabled      = 8,
    Loopback      = 9,
    Hot_Reset     = 10
};

/**
 * @brief 均衡协商阶段 (Gen3+, Phase 2 = TX / Phase 3 = RX)
 */
enum class EqPhase : uint8_t {
    Idle     = 0,  // 未协商
    TS1_Seq  = 1,  // TS1 序列交换
    TS2_Seq  = 2,  // TS2 序列交换
    Phase2   = 3,  // Phase 2 EQ (TX preset 调整)
    Phase3   = 4,  // Phase 3 EQ (RX preset 调整)
    Complete = 5
};

/**
 * @brief PHY 配置 (per design.md §6.2 PciePhyConfig 冻结字段)
 *
 * C4 修复: 与 bundles::PciePhyConfig (pcie_dllp_bundles_tlm.hh) 对齐
 *   - 新增 sr_iov_vf_pool_size 字段 (0 = SR-IOV 未启用, per design §6.2)
 *   - 默认 preset 对齐 bundle 版 (7) / hot_plug_supported 语义一致
 *   - from_bundle() 转换工厂统一两处定义
 */
struct PciePhyConfig {
    PcieEncodingLatencyModel::Rate max_speed = PcieEncodingLatencyModel::Rate::GEN5;
    uint8_t max_lanes = 16;
    uint8_t preset_P = 7;    // 8 presets (0..7), 对齐 bundle 版默认
    uint8_t preset_NP = 7;
    uint8_t preset_Cpl = 7;
    uint8_t sr_iov_vf_pool_size = 0;  // SR-IOV VF pool 大小 (0 = 未启用)
    bool hot_plug_supported = false;  // 对齐 bundle 版语义 (默认不支持)

    // C4: 从 bundle 版 PciePhyConfig 转换 (统一双类型)
    static PciePhyConfig from_bundle(const bundles::PciePhyConfig& b) noexcept;
};

/**
 * @brief PciePhyDigitalCtrl: LTSSM FSM + EQ + 速率切换 + 热插拔
 *
 * 状态机:
 *   - 默认状态 Detect（链路未训练）
 *   - start_link_training() 推进 Detect → Polling → Configuration → L0
 *   - set_link_up(true)/start_link_training() 完成后续即时置 L0
 *   - enter_recovery() 从 L0/L0s/L1 → Recovery（速率切换触发）
 *   - set_active_state(L1/L0s) 电源管理状态切换 (per Q16: 仅链路可用性影响)
 *   - Hot_Reset: Recovery 内子状态
 *
 * 热插拔 (per Q14):
 *   - signal_prsnt(false) → surprise removal: drain(1µs) + abort + 回 Detect
 *   - signal_perst(true) (asserted) → 复位 → Detect
 *   - 信号顺序: PRSNT# → MRL → PWRGOOD → REFCLK → PERST# deassert
 */
class PciePhyDigitalCtrl {
public:
    explicit PciePhyDigitalCtrl(EventQueue* eq) : eq_(eq) {}

    // ========== 配置 ==========
    void set_config(const PciePhyConfig& cfg) noexcept { cfg_ = cfg; }
    [[nodiscard]] const PciePhyConfig& config() const noexcept { return cfg_; }
    void link_layer(PcieLinkLayer* ll) noexcept { link_layer_ = ll; }
    [[nodiscard]] PcieLinkLayer* link_layer() const noexcept { return link_layer_; }

    // ========== 初始化 / 训练 ==========
    // 均衡协商已完成 (TS1/TS2 + Phase 2/3 收敛) 后可调用
    void start_link_training() noexcept;
    void set_link_up(bool up) noexcept;
    [[nodiscard]] bool is_link_up() const noexcept { return link_up_; }
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

    // ========== LTSSM 状态查询 ==========
    [[nodiscard]] LtState state() const noexcept { return state_; }
    [[nodiscard]] bool in_recovery() const noexcept {
        return state_ == LtState::Recovery || state_ == LtState::Hot_Reset;
    }

    // ========== Rate 管理 ==========
    void set_rate(PcieEncodingLatencyModel::Rate r) noexcept;
    [[nodiscard]] PcieEncodingLatencyModel::Rate rate() const noexcept { return rate_; }
    // 速率切换: Recovery → (延迟) → 新速率 → L0
    // 期间调用 PcieLinkLayer::trigger_rate_switch 使 wire 不可用 (修评审 #1)
    void start_rate_switch(PcieEncodingLatencyModel::Rate to);
    [[nodiscard]] bool is_rate_switching() const noexcept { return rate_switching_; }

    // ========== 电源管理 (per Q16: 仅链路可用性影响, 无 ASPM/CLKREQ#) ==========
    void set_active_state(LtState s) noexcept;  // L0s / L1 / L2
    void enter_l0s() noexcept;
    void enter_l1() noexcept;
    void enter_l2() noexcept;
    void exit_low_power() noexcept;  // 回到 L0

    // ========== Gen3+ 均衡协商 (per Qes) ==========
    void start_equalization() noexcept;
    [[nodiscard]] bool is_equalizing() const noexcept { return equalizing_; }
    void set_eq_preset(uint8_t preset);  // 0..7, 非法越界忽略
    [[nodiscard]] uint8_t eq_preset() const noexcept { return eq_preset_; }
    [[nodiscard]] EqPhase eq_phase() const noexcept { return eq_phase_; }
    // TS1/TS2 序列交互: 送出一帧 TSx 并推进协商
    void emit_ts1() noexcept;
    void emit_ts2() noexcept;
    // 协商推进 (每 TS 交换/配置窗口由 tick 驱动): 返回是否收敛 Complete
    bool advance_equalization() noexcept;
    // EQ FSM 收敛要求: Phase 2 → Phase 3 → Complete (8 preset 协商)
    [[nodiscard]] bool eq_converged() const noexcept {
        return eq_phase_ == EqPhase::Complete;
    }
    void set_eq_phase(EqPhase p) noexcept { eq_phase_ = p; }

    // ========== 热插拔信号 (per Q14) ==========
    void signal_prsnt(bool present) noexcept;  // 槽位存在 (低有效)
    void signal_mrl(bool latched) noexcept;
    void signal_pwrgood(bool ok) noexcept;
    void signal_refclk(bool ok) noexcept;
    void signal_perst(bool asserted) noexcept;

    // C3: 显式 API 进入 Disabled / Loopback / Hot_Reset (测试可达)
    void enter_disabled() noexcept;      // 进入 Disabled 态
    void enter_loopback() noexcept;      // 进入 Loopback 态
    void enter_hot_reset() noexcept;     // 进入 Hot_Reset 子态 (Recovery 内)

    [[nodiscard]] bool prsnt_present() const noexcept { return prsnt_present_; }
    [[nodiscard]] bool mrl_latched() const noexcept { return mrl_latched_; }
    [[nodiscard]] bool pwrgood_ok() const noexcept { return pwrgood_ok_; }
    [[nodiscard]] bool refclk_ok() const noexcept { return refclk_ok_; }
    [[nodiscard]] bool perst_asserted() const noexcept { return perst_asserted_; }

    // Surprise Removal 统计 (drain/abort 计数)
    [[nodiscard]] std::size_t surprise_removal_count() const noexcept {
        return surprise_removal_count_;
    }
    [[nodiscard]] std::size_t hotplug_insertion_count() const noexcept {
        return hotplug_insertion_count_;
    }

    // ========== 周期推进 ==========
    void tick();

    // C3: LTSSM 训练序列推进 (tick 驱动: Detect → Polling → Configuration → L0)
    // 返回 true 表示到达 L0
    bool advance_training() noexcept;

    // ========== PcieEndpointTLM composition 注册表（冻结 .h 布局下的集成通道）==========
    // 与 PcieLinkLayer::attach_to_endpoint 同机制：PcieEndpointTLM 头文件类成员布局
    // 冻结（23 ABI），不能加成员 → 通过静态注册表按 EP 模块名关联。
    // 由 src/tlm/gpu/pcie_endpoint_tlm.cc 在 on_config_loaded() 时挂接。
    static PciePhyDigitalCtrl* attach_to_endpoint(const std::string& endpoint_name,
                                                  EventQueue* eq);
    static PciePhyDigitalCtrl* for_endpoint(const std::string& endpoint_name) noexcept;
    static void detach_from_endpoint(const std::string& endpoint_name) noexcept;

// C5: 挂接 Bypass Mux 以便 Surprise Removal 清理
    void mux(PcieBypassMux* m) noexcept { mux_ = m; }
    [[nodiscard]] PcieBypassMux* mux() const noexcept { return mux_; }

private:
    EventQueue* eq_ = nullptr;
    PcieLinkLayer* link_layer_ = nullptr;
    PcieBypassMux* mux_ = nullptr;
    PciePhyConfig cfg_;
    LtState state_ = LtState::Detect;
    bool link_up_ = false;
    bool initialized_ = false;
    PcieEncodingLatencyModel::Rate rate_ = PcieEncodingLatencyModel::Rate::GEN1;

    // 均衡
    bool equalizing_ = false;
    uint8_t eq_preset_ = 0;
    EqPhase eq_phase_ = EqPhase::Idle;

    // 速率切换
    bool rate_switching_ = false;
    PcieEncodingLatencyModel::Rate rate_switch_to_ = rate_;

    // C3: 训练序列推进状态 (tick 驱动)
    uint8_t training_step_ = 0;
    bool training_in_progress_ = false;

    // 热插拔
    bool prsnt_present_ = true;
    bool mrl_latched_ = true;
    bool pwrgood_ok_ = true;
    bool refclk_ok_ = true;
    bool perst_asserted_ = false;
    std::size_t surprise_removal_count_ = 0;
    std::size_t hotplug_insertion_count_ = 0;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_PHY_DIGITAL_CTRL_TLM_HH