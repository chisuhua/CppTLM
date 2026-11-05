// test/cep_completion_domain_model.hh
// 完成域感知的 VC 共存模型 —— H4「完成域判别规则(requester-scoped vs group-scoped)足以同时容纳
// coherence + collective」的**运行期装置**
//
// 功能:把「完成域判别规则(requester-scoped vs group-scoped)」转成**可判定的死锁差异** ——
//   ① 正控:coherence 流 = requester-scoped(单点 ack)+ collective 流 = group-scoped(全组到达)
//           ⇒ 两类流在**同一实验内**共存 ⇒ 无死锁;
//   ② 反事实负控:把 coherence 流**错误标记为 group-scoped**
//           (精确语义 = 「coherence 请求的完成条件被绑定到 group rendezvous」)
//           ⇒ 死锁发生。
//
// ⚠️ **死锁机制 = 循环等待**(per `ADR-CEP-01 §6.2` 代价 3「**分类错误 = 静默死锁**」):
//   · collective 成员的**到达**依赖 coherence 请求**先退休**(跨平面 ordering/visibility,
//     per `ADR-CEP-03 §3`);
//   · coherence 请求若被错标为 group-scoped,其**完成**又依赖「全组到达」
//     ⇒ **coherence 等 collective 到达,collective 等 coherence 退休** ⇒ 循环等待 ⇒ 死锁。
//   · 正控下 coherence 是 requester-scoped ⇒ 单点 ack 即可退休 ⇒ 无循环 ⇒ 无死锁。
//   ⇒ 这就是「两套纪律不能混用」的**可判定**形态(per `ADR-CEP-01 §3.2`)。
//
// ⚠️ **为什么负控是「装置合格的前提」**(per 台账 Entry #222 ③):
//   若只做正控,本装置测的只是「两类流同实验没死锁」= **H6 换名**(H6 已证 VC 隔离避免死锁)。
//   ⇒ 负控证明**完成域语义**确实改变共存结果。
//   ⛔ **负控不是「coherence 也 blocking」** —— 那是「更多 blocking 更易死锁」的废话(h6 领域)。
//
// ⚠️ **口径限定**(必须随任何引用重复,per 台账 Entry #222 ③):
//   · ⛔ **非完整仲裁器** —— 无 credit 环流 / 无多级仲裁 / 无跨 VC 依赖图
//     (完整仲裁属 Stage 4+,per `ADR-CEP-09` / `objective §8.1` R4)
//   · ⛔ `ADR-CEP-09` SU 2026-11-02 §④ 的「`GrantAck` 可能被 `Grant` 饿死**未建模**」
//     本装置**不覆盖** ⇒ 引用时必须原样带上
//   · ⛔ **非协议级证明** —— 是**模型级**的完成语义对比
//   · 与 CP-05 的关系 = **互补守护面**(CP-05 `evaluate_completion_domain` 守**源文本扫描面**
//     —— 读 coherence 侧源文件查 group-scoped 标识;本装置守**运行期共存面**),**非替代**
//
// 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #222)

#ifndef CPPTLM_TEST_CEP_COMPLETION_DOMAIN_MODEL_HH
#define CPPTLM_TEST_CEP_COMPLETION_DOMAIN_MODEL_HH

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cep_cd {

/// **完成域**(completion domain)—— per `ADR-CEP-01 §3.2`:
/// · `requester-scoped` = 一个请求者发出、一个终结点应答、**完成 = 单点 ack**
/// · `group-scoped`     = 全组到达、**完成 = 全组 ack / 全组到达**
enum class CompletionDomain : std::uint8_t {
    RequesterScoped = 0,  ///< pairwise 完成纪律(coherence 侧的**正确**语义)
    GroupScoped = 1,      ///< group-scoped 完成纪律(collective 侧语义;coherence 侧用它是**分类错误**)
};

/// 模型配置。
struct CompletionDomainConfig {
    /// VC 数(coherence 与 collective 各占一条 ⇒ 隔离)
    int coherence_vc = 0;
    int collective_vc = 3;
    int vc_count = 4;
    /// 每周期每条 VC 的服务数
    std::uint64_t service_per_cycle = 1;
    /// 参与 rendezvous 的成员数(group-scoped 完成的「全组」大小)
    std::size_t group_size = 3;
    /// 连续 N 周期无任何请求退休 ⇒ 判死锁
    std::uint64_t deadlock_threshold = 200;
};

/// **完成域感知的 VC 共存模型** —— 两类流在同一实验内推进,按各自的**完成纪律**决定何时完成。
class CompletionDomainModel {
public:
    explicit CompletionDomainModel(CompletionDomainConfig cfg = {}) : cfg_(cfg) {}

    /// 在途请求 —— 带**已到达成员计数**(group-scoped 完成纪律的进度)。
    struct Pending {
        CompletionDomain domain = CompletionDomain::RequesterScoped;
        std::uint64_t group_id = 0;
        std::uint64_t requester = 0;
        /// 已到达的成员数(group-scoped 完成纪律的进度;requester-scoped 不用)
        std::size_t arrived_members_ = 0;
    };

    /// 注入一条 **coherence 侧**请求 —— `domain` 是**可注入的**(负控在此注入 GroupScoped)。
    void inject_coherence(CompletionDomain domain, std::uint64_t group_id, std::uint64_t requester) {
        Pending p;
        p.domain = domain;
        p.group_id = group_id;
        p.requester = requester;
        coherence_pending_.push_back(p);
        ++coherence_injected_;
    }

    /// 注入一条 **collective 侧**请求(恒为 group-scoped —— collective 面的语义是既定的)。
    /// ⚠️ 该成员的**到达**依赖「coherence 请求已全部退休」(跨平面 ordering,per `ADR-CEP-03 §3`)
    ///    ⇒ 这是死锁循环的另一半。
    void inject_collective(std::uint64_t group_id) {
        Pending p;
        p.domain = CompletionDomain::GroupScoped;
        p.group_id = group_id;
        collective_pending_.push_back(p);
        ++collective_injected_;
    }

    /// 推进一个周期 —— 每条 VC 各自服务 `service_per_cycle` 次。
    void tick() {
        ++cycle_;

        // ── collective 侧的**到达**判定(先于服务)────────────────────
        //   成员的到达依赖 coherence 全部退休(跨平面 ordering/visibility)
        bool coherence_all_retired = (coherence_retired_ == coherence_injected_);
        if (coherence_all_retired) {
            for (auto& col : collective_pending_) {
                if (col.arrived_members_ == 0) {
                    col.arrived_members_ = 1;  // 到达
                    // 到达**推进**所有在途 group-scoped coherence 请求的成员计数
                    // (⇒ 负控下这正是「coherence 等 collective 到达」的那一环)
                    for (auto& coh : coherence_pending_) {
                        if (coh.domain == CompletionDomain::GroupScoped)
                            ++coh.arrived_members_;
                    }
                }
            }
        }

        // ── coherence 侧服务 ────────────────────────────────────────
        std::uint64_t coh_budget = cfg_.service_per_cycle;
        while (coh_budget > 0 && !coherence_pending_.empty()) {
            Pending& head = coherence_pending_.front();
            --coh_budget;
            ++served_[static_cast<std::size_t>(cfg_.coherence_vc)];
            if (head.domain == CompletionDomain::RequesterScoped) {
                // 单点 ack:一次服务即完成 ⇒ **不依赖** collective 到达 ⇒ 无循环
                coherence_pending_.erase(coherence_pending_.begin());
                ++coherence_retired_;
            } else {
                // ⚠️ **负控路径**:完成条件被绑定到 group rendezvous ⇒ 需累计 `group_size` 次到达
                if (head.arrived_members_ >= cfg_.group_size) {
                    coherence_pending_.erase(coherence_pending_.begin());
                    ++coherence_retired_;
                }
            }
        }

        // ── collective 侧服务 ───────────────────────────────────────
        std::uint64_t col_budget = cfg_.service_per_cycle;
        while (col_budget > 0 && !collective_pending_.empty()) {
            Pending& head = collective_pending_.front();
            // ⚠️ 未到达 ⇒ **不服务**且**必须跳出**(否则 while 空转 ⇒ 死循环)
            if (head.arrived_members_ == 0) break;
            --col_budget;
            ++served_[static_cast<std::size_t>(cfg_.collective_vc)];
            if (head.arrived_members_ >= cfg_.group_size) {
                collective_pending_.erase(collective_pending_.begin());
                ++collective_retired_;
            } else {
                ++head.arrived_members_;
            }
        }

        // ── 死锁判定 ─────────────────────────────────────────────────
        const std::uint64_t retired = coherence_retired_ + collective_retired_;
        if (retired == last_retired_ &&
            retired < (coherence_injected_ + collective_injected_)) {
            ++stagnant_cycles_;
        } else {
            stagnant_cycles_ = 0;
        }
        last_retired_ = retired;
    }

    /// 死锁判定 = 连续 `deadlock_threshold` 周期无任何请求退休(且仍有在途请求)
    bool deadlocked() const noexcept {
        return stagnant_cycles_ >= cfg_.deadlock_threshold &&
               (coherence_retired_ + collective_retired_) <
                   (coherence_injected_ + collective_injected_);
    }

    std::uint64_t cycle() const noexcept { return cycle_; }
    std::uint64_t stagnant_cycles() const noexcept { return stagnant_cycles_; }
    std::uint64_t coherence_injected() const noexcept { return coherence_injected_; }
    std::uint64_t coherence_retired() const noexcept { return coherence_retired_; }
    std::uint64_t collective_injected() const noexcept { return collective_injected_; }
    std::uint64_t collective_retired() const noexcept { return collective_retired_; }
    std::uint64_t coherence_backlog() const noexcept { return coherence_pending_.size(); }
    std::uint64_t collective_backlog() const noexcept { return collective_pending_.size(); }
    std::uint64_t coherence_stalls() const noexcept { return coherence_stalls_; }
    std::uint64_t collective_stalls() const noexcept { return collective_stalls_; }
    /// 是否发生「完成语义混淆」—— 即 coherence 侧存在 group-scoped 完成的请求。
    /// ⇒ **负控的成因标记**(证明死锁来自完成纪律混用,而非带宽竞争)。
    bool has_completion_domain_confusion() const noexcept {
        for (const auto& r : coherence_pending_)
            if (r.domain == CompletionDomain::GroupScoped) return true;
        return false;
    }

private:
    CompletionDomainConfig cfg_;
    std::vector<Pending> coherence_pending_;
    std::vector<Pending> collective_pending_;
    std::vector<std::uint64_t> served_ =
        std::vector<std::uint64_t>(static_cast<std::size_t>(cfg_.vc_count), 0);
    std::uint64_t coherence_stalls_ = 0;
    std::uint64_t collective_stalls_ = 0;
    std::uint64_t cycle_ = 0;
    std::uint64_t coherence_injected_ = 0;
    std::uint64_t coherence_retired_ = 0;
    std::uint64_t collective_injected_ = 0;
    std::uint64_t collective_retired_ = 0;
    std::uint64_t stagnant_cycles_ = 0;
    std::uint64_t last_retired_ = 0;
};

/// **装置合格性判定**(per `ADR-CEP-06` **SU#3** §②B(i))——
///
/// 负控(反事实:coherence 被错标 group-scoped)必须**死锁**;否则**装置不合格**
/// ⇒ ⛔ **不产出 run**、⛔ **不登记结论**。
///
/// ⚠️ **为什么独立成函数**:判定逻辑必须**可机器测**(⛔ 不可只活在 emit 的控制流里
/// —— 那会变成「纯承诺 = 测试给缺口盖章」,per C1 根因教训)。
/// ⇒ 本函数是判定逻辑的**唯一真源**;emit 与测试**共用**它 ⇒ 语义漂移不可���。
inline bool device_qualified(bool negative_control_deadlocked) noexcept {
    return negative_control_deadlocked;
}

}  // namespace cep_cd

#endif  // CPPTLM_TEST_CEP_COMPLETION_DOMAIN_MODEL_HH
