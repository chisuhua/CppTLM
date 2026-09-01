// src/tlm/pcie/pcie_phy_digital_ctrl_tlm.cc
// PciePhyDigitalCtrl 实现: LTSSM FSM + Gen3+ 均衡 + 速率切换 + 热插拔
// 作者 CppTLM Team / 日期 2026-10-06
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §5
//       decisions.md §Q3/§Q14/§Q16

#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"

#include "tlm/pcie/pcie_link_layer_tlm.hh"

#include <memory>
#include <string>
#include <unordered_map>

namespace tlm::pcie {

namespace {

// PcieEndpointTLM ↔ PciePhyDigitalCtrl composition 注册表（按 EP 模块名）
// 冻结 .h 布局不能加成员 → EP 侧通过 on_config_loaded() 挂接，这里持有生命周期。
std::unordered_map<std::string, std::unique_ptr<PciePhyDigitalCtrl>>&
phy_registry() {
    static std::unordered_map<std::string, std::unique_ptr<PciePhyDigitalCtrl>> reg;
    return reg;
}

} // namespace

PciePhyDigitalCtrl* PciePhyDigitalCtrl::attach_to_endpoint(
    const std::string& endpoint_name, EventQueue* eq) {
    auto& reg = phy_registry();
    auto it = reg.find(endpoint_name);
    if (it != reg.end()) {
        it->second = std::make_unique<PciePhyDigitalCtrl>(eq);
        return it->second.get();
    }
    auto [new_it, _] = reg.emplace(endpoint_name,
                                   std::make_unique<PciePhyDigitalCtrl>(eq));
    return new_it->second.get();
}

PciePhyDigitalCtrl* PciePhyDigitalCtrl::for_endpoint(
    const std::string& endpoint_name) noexcept {
    auto& reg = phy_registry();
    auto it = reg.find(endpoint_name);
    return (it != reg.end()) ? it->second.get() : nullptr;
}

void PciePhyDigitalCtrl::detach_from_endpoint(
    const std::string& endpoint_name) noexcept {
    auto& reg = phy_registry();
    reg.erase(endpoint_name);
}

void PciePhyDigitalCtrl::start_link_training() noexcept {
    // Detect → Polling → Configuration → L0
    // TLM 侧简化为: 训练同步完成 (无真实比特/符号锁定延迟窗口)
    state_ = LtState::L0;
    link_up_ = true;
    initialized_ = true;
}

void PciePhyDigitalCtrl::set_link_up(bool up) noexcept {
    link_up_ = up;
    initialized_ = true;
    if (!up) {
        state_ = LtState::Detect;
        equalizing_ = false;
        eq_phase_ = EqPhase::Idle;
    }
}

void PciePhyDigitalCtrl::set_rate(PcieEncodingLatencyModel::Rate r) noexcept {
    rate_ = r;
}

void PciePhyDigitalCtrl::start_rate_switch(PcieEncodingLatencyModel::Rate to) {
    // Recovery 触发速率变更: 当前速率 → Recovery → (延迟) → 新速率 → L0
    const PcieEncodingLatencyModel::Rate from = rate_;
    if (from == to) {
        return;  // 同速率无切换
    }
    rate_switching_ = true;
    rate_switch_to_ = to;
    state_ = LtState::Recovery;
    link_up_ = false;
    // 调用链路层使 wire 不可用 (修 Phase 2 评审 #1)
    if (link_layer_) {
        link_layer_->trigger_rate_switch(from, to);
    } else {
        // 无链路层 → 模拟延迟 (TLM 简化: 立即完成切换)
        rate_switching_ = false;
        state_ = LtState::L0;
        rate_ = to;
        link_up_ = true;
    }
}

void PciePhyDigitalCtrl::set_active_state(LtState s) noexcept {
    // per Q16: 仅建模 L0s/L1/L2 状态切换对链路可用性的影响 (elec_idle)
    if (s == LtState::L0s || s == LtState::L1 || s == LtState::L2) {
        state_ = s;
        link_up_ = false;  // 低功耗: 链路不可用 (wire 挂起)
    }
}

void PciePhyDigitalCtrl::enter_l0s() noexcept { set_active_state(LtState::L0s); }
void PciePhyDigitalCtrl::enter_l1() noexcept { set_active_state(LtState::L1); }

void PciePhyDigitalCtrl::enter_l2() noexcept {
    // L2 电源关闭: 仅 PERST# 唤醒 (per design §5 状态转换表)
    set_active_state(LtState::L2);
}

void PciePhyDigitalCtrl::exit_low_power() noexcept {
    // L0s/L1 短空闲唤醒 → L0 (per design §5); L2 仅 PERST# 唤醒 (Q16)
    if (state_ == LtState::L0s || state_ == LtState::L1) {
        state_ = LtState::L0;
        link_up_ = true;
    }
    // L2: 不自动退出 (PERST# deassert 后由 set_link_up(true) 恢复)
}

// ========== Gen3+ 均衡协商 ==========

void PciePhyDigitalCtrl::start_equalization() noexcept {
    // Gen3+ 均衡: TS1/TS2 + 8 Preset 协商 (per Gen5 spec §8.3.1)
    equalizing_ = true;
    eq_phase_ = EqPhase::TS1_Seq;
    eq_preset_ = 0;
}

void PciePhyDigitalCtrl::set_eq_preset(uint8_t preset) {
    // 8 Preset (0..7), 越界忽略 (per spec §8.3.1)
    if (preset > 7) {
        return;
    }
    eq_preset_ = preset;
}

void PciePhyDigitalCtrl::emit_ts1() noexcept {
    // TS1 序列: 均衡协商开始或 phase 推进时发送
    if (eq_phase_ == EqPhase::Idle) {
        eq_phase_ = EqPhase::TS1_Seq;
    }
    equalizing_ = true;
}

void PciePhyDigitalCtrl::emit_ts2() noexcept {
    // TS2 序列: Phase 2/3 EQ 完成后的锁定阶段
    if (eq_phase_ != EqPhase::Complete) {
        eq_phase_ = EqPhase::TS2_Seq;
    }
}

bool PciePhyDigitalCtrl::advance_equalization() noexcept {
    // EQ FSM: TS1_Seq → TS2_Seq → Phase2 (TX) → Phase3 (RX) → Complete
    // 每个阶段由 tick/配置窗口驱动推进; 收敛后 equalizing_ = false
    switch (eq_phase_) {
    case EqPhase::TS1_Seq:
        eq_phase_ = EqPhase::TS2_Seq;
        break;
    case EqPhase::TS2_Seq:
        eq_phase_ = EqPhase::Phase2;
        break;
    case EqPhase::Phase2:
        eq_phase_ = EqPhase::Phase3;
        break;
    case EqPhase::Phase3:
        eq_phase_ = EqPhase::Complete;
        equalizing_ = false;
        break;
    default:
        break;
    }
    return eq_phase_ == EqPhase::Complete;
}

// ========== 热插拔信号 ==========

void PciePhyDigitalCtrl::signal_prsnt(bool present) noexcept {
    // PRSNT# 低有效: present=false 意味着槽位空 (低有效信号被解除)
    const bool was_present = prsnt_present_;
    prsnt_present_ = present;
    if (was_present && !present) {
        // Surprise Removal (per Q14): drain(1µs) + abort + 回 Detect
        ++surprise_removal_count_;
        // drain in-flight: TLM 简化 (in-flight 由链路层/事务层处理)
        state_ = LtState::Detect;
        link_up_ = false;
        equalizing_ = false;
        eq_phase_ = EqPhase::Idle;
    } else if (!was_present && present) {
        // 插入: 等待其余信号就绪后触发链路训练
        ++hotplug_insertion_count_;
    }
}

void PciePhyDigitalCtrl::signal_mrl(bool latched) noexcept {
    mrl_latched_ = latched;
}

void PciePhyDigitalCtrl::signal_pwrgood(bool ok) noexcept {
    pwrgood_ok_ = ok;
}

void PciePhyDigitalCtrl::signal_refclk(bool ok) noexcept {
    refclk_ok_ = ok;
}

void PciePhyDigitalCtrl::signal_perst(bool asserted) noexcept {
    perst_asserted_ = asserted;
    if (asserted) {
        // PERST# assert → LTSSM Detect (复位)
        state_ = LtState::Detect;
        link_up_ = false;
        equalizing_ = false;
        eq_phase_ = EqPhase::Idle;
    } else {
        // PERST# deassert + 所有信号就绪 → 训练
        if (prsnt_present_ && mrl_latched_ && pwrgood_ok_ && refclk_ok_) {
            state_ = LtState::L0;
            link_up_ = true;
        }
    }
}

// ========== 周期推进 ==========

void PciePhyDigitalCtrl::tick() {
    // Recovery 内速率切换完成检测: ready 后切换到新速率并回 L0
    if (rate_switching_) {
        const uint64_t ready_ns = link_layer_ ? link_layer_->rate_switch_ready_ns()
                                              : 0u;
        const uint64_t now_ns = eq_ ? eq_->getCurrentCycle() : 0u;
        if (now_ns >= ready_ns) {
            rate_switching_ = false;
            rate_ = rate_switch_to_;
            state_ = LtState::L0;
            link_up_ = true;
            // C2: 速率切换完成后, 同步链路层编码延迟模型到新速率
            // (仅当编码已启用时更新 rate, 后续 TLP 按新 Gen 计费)
            if (link_layer_) {
                link_layer_->on_rate_switch_complete(rate_);
            }
        }
    }
}

} // namespace tlm::pcie