// test/cep_vc_fabric_model.hh
// H6 的最小 VC fabric 模型(隔离 vs 共享)
//
// 功能:把「collective 是否有独立 VC」转成**可判定的饥饿/死锁差异** ——
//   ① isolation:collective 独占一条 VC ⇒ 不与被 coherence 灌满的 VC 竞争
//   ② shared   :collective 与 coherence 共用一条 VC + coherence 固定优先级 ⇒ collective 饿死
// 作者: CppTLM Team / 日期: 2026-11-04
//
// ── 口径限定(必须随结论重复)─────────────────────────────────────
// ⛔ 这不是完整 fabric:无 credit 环流、无多级仲裁、无跨 VC 依赖图。
//    它只判定**一个**命题:「当两类流量共享同一条 VC 且 coherence 固定优先级更高时,
//    collective 是否会被无限期推迟」。完整仲裁属 Stage 4+(per `ADR-CEP-09`)。
// ⚠️ 既有登记义务:`ADR-CEP-09` SU 2026-11-02 §④ 的「D∪E 合并后 `GrantAck` 可能被
//    `Grant` 饿死**未建模**」必须在 H6 报告中原样带上 —— 本模型**不**覆盖它。
#ifndef CPPTLM_TEST_CEP_VC_FABRIC_MODEL_HH
#define CPPTLM_TEST_CEP_VC_FABRIC_MODEL_HH

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cep_exp {

    enum class TrafficClass : std::uint8_t {
        Coherence = 0,
        Collective = 1,
    };

    struct VcFabricConfig {
        /// 共享模式下两类流量使用的 VC;隔离模式下 coherence 用 `coherence_vc`,
        /// collective 用 `collective_vc`。
        int shared_vc = 0;
        int coherence_vc = 0;
        int collective_vc = 3;
        bool isolate_collective = true;
        /// 每 VC 每周期可服务的请求数(带宽上限)
        std::uint64_t service_per_cycle = 1;
        /// collective backlog 连续 N cycle 不下降 ⇒ 判饥饿死锁
        std::uint64_t starvation_threshold = 500;
        /// VC 总数(用于 `vc_stall_per_vc` 的键空间)
        int vc_count = 4;
    };

    /// 最小 VC 仲裁模型。
    ///
    /// **仲裁规则**:同一 VC 上 coherence 固定优先于 collective(这正是共享 VC 下的饥饿根因);
    /// 不同 VC 之间互不影响(隔离生效的机制)。
    class VcFabricModel {
    public:
        explicit VcFabricModel(VcFabricConfig cfg = {}) : cfg_(cfg) {
            stall_.assign(static_cast<std::size_t>(cfg_.vc_count), 0);
            served_.assign(static_cast<std::size_t>(cfg_.vc_count), 0);
        }

        void inject(TrafficClass cls, std::uint64_t n = 1) {
            if (cls == TrafficClass::Coherence)
                coherence_backlog_ += n;
            else
                collective_backlog_ += n;
        }

        /// 推进一个周期。
        void tick() {
            const int coh_vc = vc_of(TrafficClass::Coherence);
            const int col_vc = vc_of(TrafficClass::Collective);

            if (coh_vc == col_vc) {
                // ── 共享 VC:coherence 先服务,剩余带宽才给 collective ──
                std::uint64_t budget = cfg_.service_per_cycle;
                const std::uint64_t coh_served = std::min(budget, coherence_backlog_);
                coherence_backlog_ -= coh_served;
                served_[static_cast<std::size_t>(coh_vc)] += coh_served;
                budget -= coh_served;

                const std::uint64_t col_served = std::min(budget, collective_backlog_);
                collective_backlog_ -= col_served;
                served_[static_cast<std::size_t>(col_vc)] += col_served;

                stall_[static_cast<std::size_t>(coh_vc)] += coherence_backlog_;
                stall_[static_cast<std::size_t>(col_vc)] += collective_backlog_;
                // ★ H6 的核心判据来源:共享 VC 下 collective 的等待**落在 coherence 的 VC 上**
                collective_wait_on_coherence_vc_ += collective_backlog_;
            } else {
                // ── 隔离 VC:两条 VC 各自独立服务 ──
                const std::uint64_t coh_served = std::min(cfg_.service_per_cycle, coherence_backlog_);
                coherence_backlog_ -= coh_served;
                served_[static_cast<std::size_t>(coh_vc)] += coh_served;
                stall_[static_cast<std::size_t>(coh_vc)] += coherence_backlog_;

                const std::uint64_t col_served = std::min(cfg_.service_per_cycle, collective_backlog_);
                collective_backlog_ -= col_served;
                served_[static_cast<std::size_t>(col_vc)] += col_served;
                stall_[static_cast<std::size_t>(col_vc)] += collective_backlog_;
            }

            // ── 饥饿判定:collective backlog 连续不下降 ──
            if (collective_backlog_ > 0 && collective_backlog_ >= last_collective_) {
                ++stagnant_cycles_;
            } else {
                stagnant_cycles_ = 0;
            }
            last_collective_ = collective_backlog_;
            ++cycle_;
        }

        bool starved() const noexcept { return stagnant_cycles_ >= cfg_.starvation_threshold; }
        std::uint64_t stagnant_cycles() const noexcept { return stagnant_cycles_; }
        std::uint64_t cycle() const noexcept { return cycle_; }
        std::uint64_t collective_backlog() const noexcept { return collective_backlog_; }
        std::uint64_t coherence_backlog() const noexcept { return coherence_backlog_; }

        /// 死锁判定 = 饥饿(本模型里 collective 被无限推迟即协议级死锁)
        std::uint64_t deadlock_violations() const noexcept { return starved() ? 1u : 0u; }

        const std::vector<std::uint64_t>& stall_per_vc() const noexcept { return stall_; }
        const std::vector<std::uint64_t>& served_per_vc() const noexcept { return served_; }

        int vc_of(TrafficClass cls) const noexcept {
            if (cfg_.isolate_collective)
                return cls == TrafficClass::Coherence ? cfg_.coherence_vc : cfg_.collective_vc;
            return cfg_.shared_vc;
        }

        /// collective 的等待是否落在 coherence 的 VC 上（true ⇒ 隔离失效）
        bool collective_shares_coherence_vc() const noexcept {
            return vc_of(TrafficClass::Collective) == vc_of(TrafficClass::Coherence);
        }

        /// collective 在 coherence VC 上累计等待的请求·周期数 —— 隔离生效时应为 0
        std::uint64_t collective_wait_on_coherence_vc() const noexcept {
            return collective_wait_on_coherence_vc_;
        }

        std::string scope() const {
            return cfg_.isolate_collective ? "4 VC / collective 独立 VC(VC3) / 1 请求每 VC 每周期"
                                           : "4 VC / collective 与 coherence 共用 VC0 / 1 请求每 VC 每周期";
        }

    private:
        VcFabricConfig cfg_;
        std::uint64_t coherence_backlog_ = 0;
        std::uint64_t collective_backlog_ = 0;
        std::uint64_t last_collective_ = 0;
        std::uint64_t stagnant_cycles_ = 0;
        std::uint64_t cycle_ = 0;
        std::uint64_t collective_wait_on_coherence_vc_ = 0;
        std::vector<std::uint64_t> stall_;
        std::vector<std::uint64_t> served_;
    };

}  // namespace cep_exp

#endif  // CPPTLM_TEST_CEP_VC_FABRIC_MODEL_HH
