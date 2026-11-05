// test/cep_experiment_harness.hh
// CEP 实验装置的观测层(最小可信形态)
//
// 功能:为 H2 / H3 / H6 提供三类观测能力 ——
//   ① LatencyCollector      —— issue/complete 周期戳 → 13 必纳 metric 的 9 项分布
//   ② SwmrDataValueDetector —— 运行时 SWMR + data-value 双不变量(INV-CEP-06-IC5)
//   ③ ProgressWatchdog      —— 「N cycle 无事务 retire 且 in-flight 非空」死锁判定
// 作者: CppTLM Team / 日期: 2026-11-04
//
// ── 为什么单独建这一层 ─────────────────────────────────────────
// `experimental/metrics.schema.json` 已定义 13 必纳 metric,但 `include/` + `src/` 里
// **零产出者**(`avg_latency_cycles` / `deadlock_violations` / `vc_stall_per_vc` 均 grep 零命中)。
// `test_cep_simulation_mode_lock.cc` 只断言「schema 里有这个字段名」⇒ 字段存在 ≠ 有数值。
// 本层是**第一处真实产出者**;它住在 `test/` 而非 `include/tlm/cep/`
// (per `examples/cep_poc_spike_s3.cc` 的 spike 硬边界先例:实验装置不得升格为契约面)。
//
// ── 反向 fixture 的设计(判据须能因正确原因红)──────────────────
// 三个组件都**不是**「永远返回好消息」:
//   · LatencyCollector 有 `audit_balanced()` —— issue 数 ≠ complete 数 ⇒ false
//     (漏打时间戳的 fixture 必须让调用方看到 false,⛔ 不得静默产 0)
//   · SwmrDataValueDetector 有 `checks_enabled` —— 测试构造 disabled 变体验证
//     「空检查检出 0」,与 enabled 变体的 >0 形成红绿分离
//   · ProgressWatchdog 有 `stalled()` —— 只有「in-flight 非空 且 连续 N cycle 无 retire」才为真
//     (空闲但无 in-flight ⇒ **不是**死锁 ⇒ 防恒真)
#ifndef CPPTLM_TEST_CEP_EXPERIMENT_HARNESS_HH
#define CPPTLM_TEST_CEP_EXPERIMENT_HARNESS_HH

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace cep_exp {

    // ══════════════════════════════════════════════════════════════════
    // ① 延迟采集
    // ══════════════════════════════════════════════════════════════════

    /// issue/complete 周期戳 → 延迟分布。
    ///
    /// ⚠️ **完整量纪律**:`issued_` / `completed_` 双计数,`audit_balanced()` 要求两者相等。
    ///    只记 issue 不记 complete(或反之)的 fixture ⇒ audit 必须 false。
    class LatencyCollector {
    public:
        void note_issue() { ++issued_; }

        /// @return false ⇔ 该样本被拒(complete < issue,即时间戳倒挂)
        bool note_complete(std::uint64_t issue_cycle, std::uint64_t complete_cycle) {
            ++completed_;
            if (complete_cycle < issue_cycle) {
                ++underflow_;
                return false;
            }
            samples_.push_back(complete_cycle - issue_cycle);
            return true;
        }

        std::size_t issued() const noexcept { return issued_; }
        std::size_t completed() const noexcept { return completed_; }
        std::size_t underflow() const noexcept { return underflow_; }
        std::size_t sample_count() const noexcept { return samples_.size(); }

        /// issue 与 complete 的**配对完整性** —— 实验收尾前的自检门。
        bool audit_balanced() const noexcept {
            return issued_ == completed_ && underflow_ == 0;
        }

        double avg() const {
            if (samples_.empty())
                return 0.0;
            double sum = 0.0;
            for (auto v : samples_)
                sum += static_cast<double>(v);
            return sum / static_cast<double>(samples_.size());
        }

        /// 最近秩法(nearest-rank):第 ceil(p/100 × n) 个有序样本。
        std::uint64_t percentile(double p) const {
            if (samples_.empty())
                return 0;
            std::vector<std::uint64_t> s = samples_;
            std::sort(s.begin(), s.end());
            double clamped = std::clamp(p, 0.0, 100.0);
            std::size_t rank = static_cast<std::size_t>(
                std::ceil((clamped / 100.0) * static_cast<double>(s.size())));
            if (rank == 0)
                rank = 1;
            if (rank > s.size())
                rank = s.size();
            return s[rank - 1];
        }

        std::uint64_t p50() const { return percentile(50.0); }
        std::uint64_t p95() const { return percentile(95.0); }
        std::uint64_t p99() const { return percentile(99.0); }
        std::uint64_t max() const {
            if (samples_.empty())
                return 0;
            return *std::max_element(samples_.begin(), samples_.end());
        }

        /// 抖动(标准差) —— H2 的「差异是否显著」辅助判据。
        double stddev() const {
            if (samples_.size() < 2)
                return 0.0;
            const double m = avg();
            double acc = 0.0;
            for (auto v : samples_) {
                const double d = static_cast<double>(v) - m;
                acc += d * d;
            }
            return std::sqrt(acc / static_cast<double>(samples_.size() - 1));
        }

        void reset() { *this = LatencyCollector{}; }

    private:
        std::uint64_t issued_ = 0;
        std::uint64_t completed_ = 0;
        std::uint64_t underflow_ = 0;
        std::vector<std::uint64_t> samples_;
    };

    // ══════════════════════════════════════════════════════════════════
    // ② SWMR + data-value 双不变量检测器
    // ══════════════════════════════════════════════════════════════════

    /// 违例分桶 —— 对齐 `metrics.schema.json` 的 `memory_violation_kinds`(swmr / data_value)。
    struct ViolationCounts {
        std::uint64_t swmr = 0;
        std::uint64_t data_value = 0;

        std::uint64_t total() const noexcept { return swmr + data_value; }
    };

    /// 运行时双不变量检测(per `ADR-CEP-06` IC5)。
    ///
    /// **SWMR**:同一地址一旦进入「已写且未失效」态,第二次写若没有先 invalidate
    ///           全部既有 sharer ⇒ 违例。
    /// **data-value**:读返回的版本号 ≠ 该地址最后一次写的版本号 ⇒ 违例。
    ///
    /// ⚠️ `checks_enabled` 是**红绿分离**用的开关:测试构造 `false` 变体证明
    ///    「检测器被变异成空检查时,已知违例 workload 检出 0」—— 从而反证 enabled 变体的 >0 有意义。
    class SwmrDataValueDetector {
    public:
        explicit SwmrDataValueDetector(bool checks_enabled = true)
            : checks_enabled_(checks_enabled) {}

        /// home 侧:记录一次**合法**写(调用方已先完成 invalidate 义务)。
        void write(std::uint64_t addr, std::uint64_t version) {
            auto& e = entries_[addr];
            e.written = true;
            e.version = version;
            e.has_writer = true;
        }

        /// home 侧:记录一次**未先失效既有 sharer** 的写 —— 若该地址已写过 ⇒ SWMR 违例。
        void write_without_invalidate(std::uint64_t addr, std::uint64_t version) {
            if (checks_enabled_) {
                auto it = entries_.find(addr);
                if (it != entries_.end() && it->second.written)
                    ++counts_.swmr;
            }
            write(addr, version);
        }

        /// 读:版本与最后一次写不一致 ⇒ data-value 违例。
        void read(std::uint64_t addr, std::uint64_t observed_version) {
            if (!checks_enabled_)
                return;
            auto it = entries_.find(addr);
            if (it == entries_.end() || !it->second.has_writer)
                return;  // 从未写过 ⇒ 无版本可比 ⇒ 不判
            if (it->second.version != observed_version)
                ++counts_.data_value;
        }

        const ViolationCounts& counts() const noexcept { return counts_; }
        bool checks_enabled() const noexcept { return checks_enabled_; }
        void reset() { entries_.clear(); counts_ = ViolationCounts{}; }

    private:
        struct Entry {
            bool written = false;
            bool has_writer = false;
            std::uint64_t version = 0;
        };
        bool checks_enabled_;
        std::map<std::uint64_t, Entry> entries_;
        ViolationCounts counts_;
    };

    // ══════════════════════════════════════════════════════════════════
    // ③ 死锁 / 活性 watchdog
    // ══════════════════════════════════════════════════════════════════

    /// 「in-flight 非空 且 连续 N cycle 无事务 retire」⇒ 死锁。
    ///
    /// ⚠️ **不得恒真**:`in_flight == 0` 的空闲期**不**计入 stall(否则空跑实验也会报死锁)。
    class ProgressWatchdog {
    public:
        explicit ProgressWatchdog(std::uint64_t threshold_cycles = 1000)
            : threshold_(threshold_cycles) {}

        /// 每周期推进一次。
        /// @param in_flight 当前未完成事务数
        /// @param retired   本周期 retire 的事务数
        void tick(std::uint64_t in_flight, std::uint64_t retired) {
            if (in_flight == 0) {
                idle_cycle_ = 0;
                return;
            }
            if (retired > 0) {
                idle_cycle_ = 0;
                return;
            }
            ++idle_cycle_;
            if (idle_cycle_ > max_idle_)
                max_idle_ = idle_cycle_;
        }

        bool stalled() const noexcept { return max_idle_ >= threshold_; }
        std::uint64_t max_idle_run() const noexcept { return max_idle_; }
        void reset() { idle_cycle_ = 0; max_idle_ = 0; }

    private:
        std::uint64_t threshold_;
        std::uint64_t idle_cycle_ = 0;
        std::uint64_t max_idle_ = 0;
    };

    // ══════════════════════════════════════════════════════════════════
    // ④ metrics.json 组装(13 必纳 + verification_mode / scope)
    // ══════════════════════════════════════════════════════════════════

    /// 一次实验 run 的观测汇总 —— 严格按 `experimental/metrics.schema.json` 的 required 集。
    struct ExperimentMetrics {
        std::uint64_t total_cycles = 0;
        std::uint64_t total_transactions = 0;
        LatencyCollector latency;
        std::uint64_t coherence_stalls = 0;
        std::uint64_t collective_stalls = 0;
        std::uint64_t directory_misses = 0;
        ViolationCounts violations;
        std::uint64_t deadlock_detected = 0;

        /// ⚠️ **实测过**的 13 必纳 metric 名单(per ADR-CEP-06 SU#1 §④ 判据③)
        /// —— 「实测 0」与「未测量」在**数据层同形**(都是 0)⇒ 机器闸无法区分
        /// ⇒ 由**产出侧**显式声明「我测了哪些」;声明为实测者**值可以是 0**。
        /// ⇒ 闸的三分支:报 measured ⇒ 必须有数值(可 0)· 报 not-collected ⇒ 必须 ⊆ 预登记
        ///   out-of-scope · 两者都没报 ⇒ **判红**(漏声明)。
        std::set<std::string> measured;

        /// ⚠️ **未采集**的 13 必纳 metric 名单(per ADR-CEP-06 SU#1 §② 载体 A)
        /// —— 「实测 0」与「未测量」必须在**数据层**可区分 ⇒ 未采集者输出
        /// `"not-collected"`(而非 0),并在此登记 per-metric 原因。
        /// ⇒ 机器闸(tools 判据④)据此校验 `not-collected ⊆ 预登记 out-of-scope`。
        std::map<std::string, std::string> not_collected;

        /// per-VC stall(H6 用;可为空 ⇒ 输出 `{}`)
        std::map<int, std::uint64_t> vc_stall_per_vc;

        /// **观测键**(per `ADR-CEP-06` SU#4 §② 约束 5)—— 事务级 / 地址级
        /// (键名 ⇒ 实例 id 或地址列表)。落 `metrics.json` 顶层**附加键**
        /// (⛔ 不改 13 必纳 metric 的语义)⇒ 「同一事务流」判据有**机器锚**,
        /// 而非靠 `verification_scope` 散文自证。
        std::map<std::string, std::vector<std::uint64_t>> observation_keys;

        /// ⚠️ 必填 —— 不得留空(per ADR-CEP-06 §3:规模上限须可读)
        std::string verification_mode = "sampled";
        std::string verification_scope = "";

        /// 一个必纳 metric 的 JSON 值 —— 未采集者输出 `"not-collected"`(per SU#1 §②)
        inline std::string metric_json(const std::string& name, std::uint64_t value) const {
            auto it = not_collected.find(name);
            if (it != not_collected.end())
                return "\"not-collected\"";
            return std::to_string(value);
        }

        /// 13 必纳字段是否齐备 + 数值自洽(transactions_per_cycle 等派生量可算)
        bool well_formed() const {
            // ⚠️ 2026-11-05(Entry #222):H4 的完成域共存实验**不测延迟**(延迟族全部未采集)
            //   ⇒ 「事务数 == 延迟签发数」这条自洽约束**不适用** ⇒ 延迟族全未采集时豁免它。
            //   ⛔ 但「延迟族**部分**采集」时仍须满足(那才是真的不自洽)。
            const bool latency_fully_uncollected =
                not_collected.count("avg_latency_cycles") > 0 &&
                not_collected.count("p50_latency_cycles") > 0 &&
                not_collected.count("p95_latency_cycles") > 0 &&
                not_collected.count("p99_latency_cycles") > 0 &&
                not_collected.count("max_latency_cycles") > 0;
            const bool latency_consistent =
                latency_fully_uncollected ||
                (total_transactions == latency.issued() && latency.audit_balanced());
            return latency_consistent &&
                   !verification_scope.empty() &&
                   (verification_mode == "exhaustive" ||
                    verification_mode == "bounded_random" ||
                    verification_mode == "sampled");
        }

        std::string to_json() const;
    };

    inline std::string ExperimentMetrics::to_json() const {
        std::ostringstream os;
        const double txn_per_cycle =
            total_cycles == 0 ? 0.0
                              : static_cast<double>(total_transactions) /
                                    static_cast<double>(total_cycles);

        // 延迟族:未采集时 latency 收集器为空 ⇒ avg/p50/... 全 0
        // ⇒ **逐项**看 not_collected(而不是一个 latency_collected 开关)——
        //    否则「显式登记为未采集」会被收集器状态覆盖 ⇒ 写回 0 ⇒ 违反 SU#1 §③ branch 3。

        os << "{\n";
        os << "  \"total_cycles\": " << total_cycles << ",\n";
        os << "  \"total_transactions\": " << total_transactions << ",\n";
        os << "  \"transactions_per_cycle\": " << txn_per_cycle << ",\n";
        os << "  \"avg_latency_cycles\": " << metric_json("avg_latency_cycles", latency.avg()) << ",\n";
        os << "  \"p50_latency_cycles\": " << metric_json("p50_latency_cycles", latency.p50()) << ",\n";
        os << "  \"p95_latency_cycles\": " << metric_json("p95_latency_cycles", latency.p95()) << ",\n";
        os << "  \"p99_latency_cycles\": " << metric_json("p99_latency_cycles", latency.p99()) << ",\n";
        os << "  \"max_latency_cycles\": " << metric_json("max_latency_cycles", latency.max()) << ",\n";
        os << "  \"coherence_stalls\": " << metric_json("coherence_stalls", coherence_stalls) << ",\n";
        os << "  \"collective_stalls\": " << metric_json("collective_stalls", collective_stalls) << ",\n";
        os << "  \"directory_misses\": " << metric_json("directory_misses", directory_misses) << ",\n";
        os << "  \"deadlock_violations\": " << metric_json("deadlock_violations", deadlock_detected) << ",\n";
        os << "  \"memory_violations\": " << metric_json("memory_violations", violations.total()) << ",\n";
        os << "  \"memory_violation_kinds\": {\"swmr\": " << violations.swmr
           << ", \"data_value\": " << violations.data_value << "},\n";
        os << "  \"observation_keys\": {";
        bool first_obs = true;
        for (const auto& kv : observation_keys) {
            if (!first_obs)
                os << ", ";
            first_obs = false;
            os << "\"" << kv.first << "\": [";
            for (std::size_t i = 0; i < kv.second.size(); ++i) {
                if (i != 0) os << ", ";
                os << kv.second[i];
            }
            os << "]";
        }
        os << "},\n";
        os << "  \"vc_stall_per_vc\": {";
        bool first = true;
        for (const auto& kv : vc_stall_per_vc) {
            if (!first)
                os << ", ";
            first = false;
            os << "\"vc" << kv.first << "\": " << kv.second;
        }
        os << "},\n";
        // 实测声明集(per SU#1 §④ 判据③:实测 0 与未测量同形 ⇒ 产出侧显式声明)
        os << "  \"_measured\": [";
        first = true;
        for (const auto& m : measured) {
            if (!first) os << ", ";
            first = false;
            os << "\"" << m << "\"";
        }
        os << "],\n";
        // per-metric 未采集原因表(per SU#1 §②:单字符串在多 metric 未采集时不可判定)
        os << "  \"_not_collected\": {";
        first = true;
        for (const auto& kv : not_collected) {
            if (!first)
                os << ", ";
            first = false;
            os << "\"" << kv.first << "\": \"" << kv.second << "\"";
        }
        os << "},\n";
        os << "  \"verification_mode\": \"" << verification_mode << "\",\n";
        os << "  \"verification_scope\": \"" << verification_scope << "\"\n";
        os << "}\n";
        return os.str();
    }

}  // namespace cep_exp

#endif  // CPPTLM_TEST_CEP_EXPERIMENT_HARNESS_HH
