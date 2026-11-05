// test/cep_consistency_completion_model.hh
// 「一致性 × 完成域」复合实验装置 —— B4 达成证据载体(per `ADR-CEP-06` SU#4)
//
// 功能:把 h3 的**一致性不变量**面(SWMR + data-value + 漏探/superset)与 h4 的**完成域**面
//   (requester-scoped vs group-scoped ⇒ 错标即死锁)**合流到同一条周期化事务流**,
//   使**单个 run** 同时实测两类违例类(`memory_violations` + `deadlock_violations`)。
//
// ⚠️ **为什么必须合流(不是两段拼接)**:B4 要求「违例类必须实测为 0」,而现有四个装置族
//   无一能同时实测两类(h2 皆无 / h3 测 memory / h4·h6 测 deadlock)⇒ B4 结构性不可达。
//   本装置的**同一事务流**保证:承载内存不变量检查的事务,**同时**受完成域阻塞规则支配
//   ⇒ 「阻塞平凡化一致性测量」的形态在结构上不可能(per SU#4 §② 约束 5/6)。
//
// ⚠️ **与既有件的复用边界**:
//   · 完成域语义与死锁机理 = 复用 `cep_cd::CompletionDomainModel`(h4 装置,已验证的循环等待);
//   · 一致性语义与不变量 = 复用 `SwmrDataValueDetector` + `DcohSymmetric` + `HomeNodeModel`
//     (h3 装置同款)⇒ **H3 分支 1 的数值可由本装置同参数重证**;
//   · 活性判定 = 复用 `ProgressWatchdog`(in-flight 非空 且 连续 N cycle 无 retire)。
//   ⇒ 三个件都是**既有测试件**,本文件只做**编排 + 身份仪表化**,⛔ 不新造协议语义。
//
// ⚠️ **观测键仪表化(SU#4 §② 约束 5 的可判定性前置)**:
//   `ProgressWatchdog` 只有**计数**入参,⛔ 不承担键语义 ⇒ 键集合由本模型**显式导出**:
//     · `memory_checked_txn_ids` —— 已执行内存操作(写+读)的**实例**;
//     · `blocking_subject_txn_ids` —— 完成条件受**完成域规则 / 服务容量**支配的**实例**;
//     · `pending_*` —— 在途未完成(阻塞窗口内)实例与其地址。
//   ⇒ 「同一事务流」判据 = **实例级**交集(同一实例既受阻塞支配又执行了内存操作);
//     地址级交集另作「同流同地址空间」的辅证。
//
// 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #226 / `ADR-CEP-06` SU#4)

#ifndef CPPTLM_TEST_CEP_CONSISTENCY_COMPLETION_MODEL_HH
#define CPPTLM_TEST_CEP_CONSISTENCY_COMPLETION_MODEL_HH

#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include "cep_completion_domain_model.hh"
#include "cep_directory_experiment.hh"
#include "cep_experiment_harness.hh"
#include "tlm/cep/cxl_dcoh_tracker.hh"
#include "tlm/cep/sharer_tracker.hh"

namespace cep_cc {

using tlm::cep::node_id_t;
using tlm::cep::responder_mask_of;
using tlm::cep::responder_set_t;
using tlm::cep::SnoopState;

/// **完成域**(coherence 侧)—— per `ADR-CEP-01 §3.2`。
/// ⚠️ **直接复用** `cep_cd::CompletionDomain`(h4 装置的既有语义类型)——
///   ⛔ 不另造同义枚举(否则两处语义可能各自漂移)。
using CompletionDomain = cep_cd::CompletionDomain;

/// 三个候选节点(SDMA / VRAM / GMMU)—— 事务身份与漏探判定共用。
inline constexpr node_id_t kNodeCandidates[3] = {tlm::cep::kNodeIdSdma, tlm::cep::kNodeIdVram,
                                                 tlm::cep::kNodeIdGmmu};

/// 一条 coherence 事务的**实例身份** —— 观测键的粒度(per SU#4 §② 约束 5:必须是**事务级**)。
struct TxnInstance {
    std::uint64_t txn_id = 0;
    std::uint64_t addr = 0;
    node_id_t node = 0;
    std::uint64_t version = 0;
    responder_set_t truth = 0;  ///< 本事务期望的 sharer 集(漏探判定用)
    std::uint64_t issue_cycle = 0;
    std::uint64_t latency_cycles = 0;  ///< 完成周期 − 签发周期(在途窗口的度量)
};

/// 模型配置。
struct Config {
    std::size_t group_size = 3;          ///< group-scoped 完成的「全组」大小
    std::uint64_t service_per_cycle = 1; ///< 每周期服务数(⇒ 轮内产生**真实在途窗口**)
    std::uint64_t deadlock_threshold = 200;  ///< 连续 N cycle 无 retire ⇒ 判死锁
    std::uint64_t region_base = 0x1000;
    std::uint64_t region_size = 0x1000;
    std::size_t txns_per_round = 3;      ///< 每轮注入的 coherence 事务数(= 每轮地址数)
    CompletionDomain coherence_domain = CompletionDomain::RequesterScoped;
    /// **负控①**:未先失效既有 sharer 的写 ⇒ SWMR 违例
    bool inject_swmr_violation = false;
    /// **负控②**:读观察到过期版本 ⇒ data-value 违例
    bool inject_stale_read = false;
    /// **负控③(反向 fixture)**:目录丢失一个**真实 sharer**(事务仍期望它)⇒ 漏探判定必须红
    ///   —— 用于证明 `superset_violations == 0` **不是恒过**(per AC-9 评审 **S1**)。
    bool inject_missed_sharer = false;
    /// **跨平面阻塞开关**(per SU#4 §② 约束 6 的耦合探针):true = collective 的到达依赖
    /// coherence 全部退休(默认,真实阻塞);false = 不等待 ⇒ 无跨平面阻塞。
    bool blocking_enabled = true;
    /// ⛔ **退化开关**(负向 fixture):把内存操作挂到**独立实例流**上
    ///   —— 模拟「两件实验穿一件风衣」(一致性段与完成域段各跑各的再拼一起)
    ///   ⇒ 观测键**实例级交集为空** ⇒ 装置合格性判定必须红。
    bool detach_memory_ops = false;
};

/// 「一致性 × 完成域」复合装置。
class ConsistencyCompletionModel {
public:
    explicit ConsistencyCompletionModel(const Config& cfg = {});

    /// 推进**一个周期**(含轮次管理:上一轮收尾 ⇒ 自动开下一轮)。
    void advance();

    /// 驱动到「已完成 coherence 事务数 ≥ target」或「判定死锁」为止(带硬上限,防挂死)。
    /// @return true = 达成目标;false = 提前死锁 / 触顶
    bool run_until(std::uint64_t coherence_target, std::uint64_t max_cycles = 100000);

    // ── 观测面 ────────────────────────────────────────────────────────────────
    bool deadlocked() const noexcept;        ///< 权威判据 = watchdog(in-flight 非空 + 无 retire)
    bool watch_dog_stalled() const noexcept { return watchdog_.stalled(); }
    bool completion_model_deadlocked() const noexcept { return cd_.deadlocked(); }
    std::uint64_t in_flight() const noexcept;
    std::uint64_t max_in_flight() const noexcept { return max_in_flight_; }
    std::uint64_t max_idle_run() const noexcept { return watchdog_.max_idle_run(); }
    std::uint64_t cycle() const noexcept { return cycle_; }
    std::uint64_t coherence_retired() const noexcept { return cd_.coherence_retired(); }
    std::uint64_t collective_retired() const noexcept { return cd_.collective_retired(); }
    std::uint64_t coherence_injected() const noexcept { return cd_.coherence_injected(); }
    std::uint64_t superset_violations() const noexcept { return superset_violations_; }
    const cep_exp::SwmrDataValueDetector& detector() const noexcept { return detector_; }

    const std::vector<TxnInstance>& completed() const noexcept { return completed_; }

    // ── 观测键(实例级 / 地址级)────────────────────────────────────────────────
    std::vector<std::uint64_t> memory_checked_txn_ids() const;
    std::vector<std::uint64_t> blocking_subject_txn_ids() const;
    std::vector<std::uint64_t> pending_txn_ids() const;
    std::vector<std::uint64_t> memory_checked_addrs() const;
    std::vector<std::uint64_t> pending_addrs() const;

    /// 「同一事务流」覆盖率 = |内存操作实例 ∩ 受阻塞支配实例| / |内存操作实例|
    /// —— 退化装置(两段拼接) ⇒ 0;正确装置 ⇒ 1。
    double same_stream_coverage() const;

    /// 阻塞窗口内**在途地址** ∩ **已做内存操作地址** ≠ ∅ —— 辅证「同流同地址空间」。
    bool blocked_address_overlap() const;

    /// 死锁判定阈值(= watchdog 的停滞周期数)。
    std::uint64_t deadlock_threshold() const noexcept { return cfg_.deadlock_threshold; }

private:
    void inject_round();
    void execute_memory_ops(const TxnInstance& t);
    void collect_completions();

    Config cfg_;
    cep_cd::CompletionDomainModel cd_;
    tlm::cep::DcohSymmetric dcoh_;
    cep_exp::HomeNodeModel home_;
    cep_exp::SwmrDataValueDetector detector_;
    cep_exp::ProgressWatchdog watchdog_;

    std::deque<TxnInstance> pending_coh_;  ///< 与 `cd_` 的 coherence 队列**同序**(FIFO 对齐)
    std::deque<std::uint64_t> pending_col_; ///< collective 地址(同序)
    std::vector<TxnInstance> completed_;
    std::vector<std::uint64_t> memory_checked_ids_;
    std::set<std::uint64_t> blocking_subject_ids_;
    std::set<std::uint64_t> memory_checked_addr_set_;

    std::uint64_t cycle_ = 0;
    std::uint64_t round_ = 0;
    std::uint64_t next_txn_id_ = 1;
    std::uint64_t next_version_ = 1;
    std::uint64_t processed_coh_ = 0;
    std::uint64_t processed_col_ = 0;
    std::uint64_t retired_prev_ = 0;
    std::uint64_t round_collective_baseline_ = 0;
    std::uint64_t round_coh_target_ = 0;
    bool round_finished_ = true;  ///< 首轮:第一次 advance 即开轮
    bool round_collective_done_ = false;
    std::uint64_t max_in_flight_ = 0;
    std::uint64_t superset_violations_ = 0;
    bool blocked_overlap_seen_ = false;
};

/// 装置**合格性**判定(per SU#4 §② 约束 1/2/3/5)。
///
/// ⚠️ **四条缺一即不合格** —— 缺任何一条 ⇒ emit **拒绝产出**(⛔ 不登记结论):
///   ① `deadlock_detector_live` —— 错标负控**真的**检出死锁(⇒ watchdog 非恒假);
///   ② `memory_detector_live` —— SWMR 注入**与**过期读注入**各自**检出违例(⇒ 检测器非空检查);
///   ③ `same_stream` —— 实例级观测键交集成立 + 地址级重叠(⇒ 非「两段拼接」);
///   ④ `watchdog_not_vacuous` —— 正控确有在途窗口(`max_in_flight > 1`)且未误判停滞。
struct Qualification {
    bool deadlock_detector_live = false;
    bool memory_detector_live = false;
    bool same_stream = false;
    bool watchdog_not_vacuous = false;
    bool qualified() const noexcept {
        return deadlock_detector_live && memory_detector_live && same_stream && watchdog_not_vacuous;
    }
    std::string reason() const;
};

/// 「同一事务流」的最低覆盖率(实例级)。
inline constexpr double kMinSameStreamCoverage = 0.8;

/// 汇总四个场景 ⇒ 装置合格性。
/// @param positive      正控(正确完成域 + 无注入)
/// @param mislabel      负控①:coherence 错标 group-scoped ⇒ 必须检出死锁
/// @param swmr_injected 负控②:SWMR 注入 ⇒ 必须检出 `swmr > 0`
/// @param stale_injected 负控③:过期读注入 ⇒ 必须检出 `data_value > 0`
Qualification qualify(const ConsistencyCompletionModel& positive,
                       const ConsistencyCompletionModel& mislabel,
                       const ConsistencyCompletionModel& swmr_injected,
                       const ConsistencyCompletionModel& stale_injected);


// ══════════════════════════════════════════════════════════════════
// 实现(header-only —— `test/` 只编 `test_*.cc`,与既有 `cep_completion_domain_model.hh` 同形)
// ══════════════════════════════════════════════════════════════════

inline ConsistencyCompletionModel::ConsistencyCompletionModel(const Config& cfg)
    : cfg_(cfg),
      cd_([&cfg] {
          cep_cd::CompletionDomainConfig c;
          c.group_size = cfg.group_size;
          c.service_per_cycle = cfg.service_per_cycle;
          c.deadlock_threshold = cfg.deadlock_threshold;
          c.collective_requires_coherence_retirement = cfg.blocking_enabled;
          return c;
      }()),
      dcoh_(tlm::cep::DcohSymmetric::LocalRegion{cfg.region_base, cfg.region_size}),
      home_(dcoh_),
      watchdog_(cfg.deadlock_threshold) {}

inline void ConsistencyCompletionModel::inject_round() {
    ++round_;
    const std::size_t k = cfg_.txns_per_round;
    const node_id_t(&kCandidates)[3] = kNodeCandidates;
    for (std::size_t i = 0; i < k; ++i) {
        const std::uint64_t addr = cfg_.region_base + i;
        const std::size_t seq = static_cast<std::size_t>((round_ - 1) * k + i);
        const std::size_t want = seq % 3 + 1;
        responder_set_t truth = 0;
        for (std::size_t j = 0; j < want; ++j) {
            const node_id_t node = kCandidates[(seq + j) % 3];
            dcoh_.set_sharer(addr, node, SnoopState::S);
            truth |= responder_mask_of(node);
        }
        TxnInstance t;
        t.txn_id = next_txn_id_++;
        t.addr = addr;
        t.node = kCandidates[seq % 3];
        t.version = next_version_++;
        t.truth = truth;
        t.issue_cycle = cycle_;
        // 完成域侧注入(group-scoped ⇒ 错标负控)
        cd_.inject_coherence(cfg_.coherence_domain, addr, t.node);
        pending_coh_.push_back(t);
        // 「受阻塞支配」的实例集合(完成条件依赖完成域规则 / 服务容量)
        blocking_subject_ids_.insert(t.txn_id);
    }
    // collective:到达依赖 coherence 全退(跨平面 ordering)⇒ **阻塞窗口**来源
    round_collective_baseline_ = cd_.collective_retired();
    const std::uint64_t caddr = cfg_.region_base + ((round_ - 1) % k);
    cd_.inject_collective(round_);
    pending_col_.push_back(caddr);
    round_coh_target_ = processed_coh_ + k;
}

inline void ConsistencyCompletionModel::execute_memory_ops(const TxnInstance& t) {
    // home 侧读服务(h3 同形)
    (void)home_.service_read(t.addr);
    // 漏探(superset)判定:home 视图须覆盖本事务期望的 sharer 集
    // ── 漏探(superset)判定 ──────────────────────────────────────────────
    // ⚠️ 默认路径下 `truth` 的来源就是 `set_sharer` 的参数 ⇒ `truth ⊆ sharers` 同义反复
    // (per AC-9 评审 **S1**)。`inject_missed_sharer` 是**反向 fixture**:先让目录丢掉一个
    // 真实 sharer,再把它并入期望集 ⇒ 判定**必须**红,否则该判定恒过(空检查)。
    responder_set_t expected = t.truth;
    if (cfg_.inject_missed_sharer) {
        const node_id_t ghost = kNodeCandidates[static_cast<std::size_t>(t.txn_id % 3)];
        dcoh_.clear_sharer(t.addr, ghost);
        expected |= responder_mask_of(ghost);
    }
    const auto view = dcoh_.view_of(t.addr);
    if ((view.sharers & expected) != expected) ++superset_violations_;
    if (cfg_.detach_memory_ops) {
        // ⛔ **退化形态**:内存操作挂到**独立实例流**(模拟「两段拼接」)
        //   ⇒ 实例级观测键交集为空 ⇒ 装置合格性判定必须红。
        detector_.write(t.addr, t.version);
        detector_.read(t.addr, t.version);
        memory_checked_ids_.push_back(1000000ULL + t.txn_id);
    } else {
        if (cfg_.inject_swmr_violation)
            detector_.write_without_invalidate(t.addr, t.version);
        else
            detector_.write(t.addr, t.version);
        detector_.read(t.addr, t.version);
        if (cfg_.inject_stale_read && t.version > 1)
            detector_.read(t.addr, t.version - 1);  // 负控:读到过期版本 ⇒ data-value 违例
        memory_checked_ids_.push_back(t.txn_id);
        memory_checked_addr_set_.insert(t.addr);
    }
    // 清理本事务的 sharer(h3 同形:清最后一个)
    dcoh_.clear_sharer(t.addr, t.node);
}

inline void ConsistencyCompletionModel::collect_completions() {
    // ⚠️ 完成顺序 = 签发顺序(`CompletionDomainModel` 的服务队列是 FIFO)⇒ 可按序推断
    while (cd_.coherence_retired() > processed_coh_) {
        if (pending_coh_.empty()) break;
        TxnInstance t = pending_coh_.front();
        pending_coh_.pop_front();
        t.latency_cycles = cycle_ - t.issue_cycle;
        execute_memory_ops(t);
        completed_.push_back(t);
        ++processed_coh_;
    }
    while (cd_.collective_retired() > processed_col_) {
        if (!pending_col_.empty()) pending_col_.pop_front();
        ++processed_col_;
    }
}

inline void ConsistencyCompletionModel::advance() {
    if (round_finished_) {
        inject_round();
        round_finished_ = false;
        round_collective_done_ = false;
    }
    cd_.tick();
    ++cycle_;
    collect_completions();
    const std::uint64_t in_flight_now = in_flight();
    const std::uint64_t retired_total = cd_.coherence_retired() + cd_.collective_retired();
    watchdog_.tick(in_flight_now, retired_total - retired_prev_);
    retired_prev_ = retired_total;
    if (in_flight_now > max_in_flight_) max_in_flight_ = in_flight_now;
    // 阻塞窗口内的**地址级重叠**(历史可见 —— run 末尾 pending 可能已清空)
    if (!memory_checked_addr_set_.empty()) {
        for (std::uint64_t a : pending_addrs()) {
            if (memory_checked_addr_set_.count(a)) {
                blocked_overlap_seen_ = true;
                break;
            }
        }
    }
    if (!round_collective_done_ && cd_.collective_retired() > round_collective_baseline_)
        round_collective_done_ = true;
    if (round_collective_done_ && processed_coh_ >= round_coh_target_) round_finished_ = true;
}

inline bool ConsistencyCompletionModel::run_until(std::uint64_t coherence_target,
                                                  std::uint64_t max_cycles) {
    const std::uint64_t start_cycle = cycle_;
    while (processed_coh_ < coherence_target) {
        if (cycle_ - start_cycle >= max_cycles) return false;
        if (watchdog_.stalled()) return false;
        advance();
    }
    return true;
}

inline bool ConsistencyCompletionModel::deadlocked() const noexcept {
    return watchdog_.stalled();
}

inline std::uint64_t ConsistencyCompletionModel::in_flight() const noexcept {
    return static_cast<std::uint64_t>(pending_coh_.size() + pending_col_.size());
}

inline std::vector<std::uint64_t> ConsistencyCompletionModel::memory_checked_txn_ids() const {
    return memory_checked_ids_;
}

inline std::vector<std::uint64_t> ConsistencyCompletionModel::blocking_subject_txn_ids() const {
    return std::vector<std::uint64_t>(blocking_subject_ids_.begin(), blocking_subject_ids_.end());
}

inline std::vector<std::uint64_t> ConsistencyCompletionModel::pending_txn_ids() const {
    std::vector<std::uint64_t> out;
    for (const auto& t : pending_coh_) out.push_back(t.txn_id);
    return out;
}

inline std::vector<std::uint64_t> ConsistencyCompletionModel::memory_checked_addrs() const {
    return std::vector<std::uint64_t>(memory_checked_addr_set_.begin(),
                                       memory_checked_addr_set_.end());
}

inline std::vector<std::uint64_t> ConsistencyCompletionModel::pending_addrs() const {
    std::vector<std::uint64_t> out;
    for (const auto& t : pending_coh_) out.push_back(t.addr);
    for (std::uint64_t a : pending_col_) out.push_back(a);
    return out;
}

inline double ConsistencyCompletionModel::same_stream_coverage() const {
    if (memory_checked_ids_.empty()) return 0.0;
    std::size_t inter = 0;
    for (std::uint64_t id : memory_checked_ids_)
        if (blocking_subject_ids_.count(id)) ++inter;
    return static_cast<double>(inter) / static_cast<double>(memory_checked_ids_.size());
}

inline bool ConsistencyCompletionModel::blocked_address_overlap() const {
    return blocked_overlap_seen_;
}

inline std::string Qualification::reason() const {
    std::string r;
    r += deadlock_detector_live ? "①死锁检测力=OK;" : "①死锁检测力=缺(错标负控未检出死锁);";
    r += memory_detector_live ? "②内存检测力=OK;" : "②内存检测力=缺(SWMR/过期读注入未检出);";
    r += same_stream ? "③同一事务流=OK;" : "③同一事务流=缺(观测键实例级交集不成立);";
    r += watchdog_not_vacuous ? "④watchdog非空洞=OK" : "④watchdog非空洞=缺(正控无在途窗口)";
    return r;
}

inline Qualification qualify(const ConsistencyCompletionModel& positive,
                              const ConsistencyCompletionModel& mislabel,
                              const ConsistencyCompletionModel& swmr_injected,
                              const ConsistencyCompletionModel& stale_injected) {
    Qualification q;
    q.deadlock_detector_live = mislabel.watch_dog_stalled() && mislabel.completion_model_deadlocked();
    q.memory_detector_live = swmr_injected.detector().counts().swmr > 0 &&
                              stale_injected.detector().counts().data_value > 0;
    q.same_stream = positive.same_stream_coverage() >= kMinSameStreamCoverage &&
                    positive.blocked_address_overlap();
    q.watchdog_not_vacuous = positive.max_in_flight() > 1 &&
                             positive.max_idle_run() < positive.deadlock_threshold();
    return q;
}

}  // namespace cep_cc

#endif  // CPPTLM_TEST_CEP_CONSISTENCY_COMPLETION_MODEL_HH
