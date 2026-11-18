/**
 * @file traffic_gen_tlm.hh
 * @brief TrafficGenTLM：流量生成模块（v2.1 TLM）
 * 
 * 支持 SEQUENTIAL / RANDOM / TRACE 3种基础模式，
 * 以及 HOTSPOT / STRIDED / NEIGHBOR / TORNADO 4种压力模式。
 * 通过 set_stress_pattern() 配置，压力模式地址生成覆盖基础模式。
 * 
 * @author CppTLM Development Team
 * @date 2026-04-16
 */

#ifndef TLM_TRAFFIC_GEN_TLM_HH
#define TLM_TRAFFIC_GEN_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include "tlm/stress_patterns.hh"
#include <cstdint>
#include <string>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

// TrafficGenTLM uses GenMode_TLM to avoid ODR conflict with legacy GenMode
enum class GenMode_TLM { SEQUENTIAL, RANDOM, TRACE };

struct TraceRecord_TLM {
    uint64_t addr;
    bool is_write;
};

class TrafficGenTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>  resp_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>  req_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    GenMode_TLM mode_ = GenMode_TLM::SEQUENTIAL;
    uint64_t start_addr_ = 0x1000;
    uint64_t end_addr_ = 0x2000;
    uint64_t cur_addr_ = 0x1000;
    int num_requests_ = 20;
    int completed_ = 0;
    int issued_ = 0;
    std::mt19937 rng_;
    std::uint64_t seed_ = 42;  ///< 随机种子(可复现门 per A4;默认 42 = 既有行为)
    std::uniform_int_distribution<uint64_t> addr_dist_;
    std::vector<TraceRecord_TLM> trace_;
    size_t trace_pos_ = 0;

    // 压力模式支持
    StressPattern stress_pattern_ = StressPattern::SEQUENTIAL;
    std::unique_ptr<StressPatternStrategy> strategy_;

    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_requests_issued_;
    tlm_stats::Scalar& stats_requests_completed_;
    tlm_stats::Scalar& stats_reads_;
    tlm_stats::Scalar& stats_writes_;
    tlm_stats::Distribution& stats_latency_;
    tlm_stats::Distribution& stats_addr_distribution_;
    std::unordered_map<uint64_t, uint64_t> txn_issue_time_;
    uint64_t current_cycle_ = 0;

public:
    explicit TrafficGenTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          cur_addr_(start_addr_),
          completed_(0),
          issued_(0),
          rng_(42),
          addr_dist_(start_addr_, end_addr_),
          strategy_(create_strategy(StressPattern::SEQUENTIAL)),
          stats_("traffic_gen", nullptr),
          stats_requests_issued_(stats_.addScalar("requests_issued", "Total requests issued", "count")),
          stats_requests_completed_(stats_.addScalar("requests_completed", "Total requests completed", "count")),
          stats_reads_(stats_.addScalar("reads", "Read requests", "count")),
          stats_writes_(stats_.addScalar("writes", "Write requests", "count")),
          stats_latency_(stats_.addDistribution("latency", "Request-response latency", "cycle")),
          stats_addr_distribution_(stats_.addDistribution("addr_dist", "Address distribution", "addr")) {
        trace_ = {
            {0x1000, false},
            {0x1004, true},
            {0x1008, false},
            {0x100c, true}
        };
    }

    std::string get_module_type() const override { return "TrafficGenTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // ChStreamModuleBase 统计接口
    tlm_stats::StatGroup* get_stats_group() override { return &stats_; }
    std::string get_stats_path() const override { return "system.traffic_gen"; }

    /// 配置加载：**逐键「读取即声明」**（`ADR-X.17` D-1）—— 一次调用完成
    /// 类型校验 + 范围校验 + 赋值 + 「已消费键」登记。
    ///
    /// ⚠️ 本 change（`cpptlm-module-param-effect-landing`）**之前**，下列 8 个键
    ///    **全部零效果**（只读 `seed`，其余被静默丢弃 ⇒ 配置写了不生效）：
    ///    `pattern` / `num_requests` / `start_addr` / `end_addr` /
    ///    `hotspot_addrs` / `hotspot_weights` / `stride` / `seed`
    ///    ⇒ 本次逐键落地（态①：真实消费）。
    void on_config_loaded() override {
        const auto& cfg = get_config();

        // ① pattern —— 字符串枚举 ⇒ 基础模式(GenMode_TLM) + 压力模式(StressPattern)
        std::string pattern;
        if (load_param_opt("pattern", pattern)) {
            apply_pattern(pattern);
        }

        // ② start_addr / end_addr —— 地址键
        //    ⚠️ 配置管线的 `VarResolver` 会把**数字形态的字符串**转成 JSON 数字
        //       （`"0x1000"` ⇒ `4096.0`，见 `include/utils/var_resolver.hh` 的 `strtod` 分支）
        //       ⇒ 模块侧**必须两种形态都接受** ⇒ 走 `read_addr_value`（非 `load_param`，
        //       因其类型检查只接受单一 JSON 类型）;键仍显式登记为「已消费」。
        ignore_param("start_addr");
        ignore_param("end_addr");
        uint64_t lo = start_addr_;
        uint64_t hi = end_addr_;
        const bool has_lo = read_addr_key(cfg, "start_addr", lo);
        const bool has_hi = read_addr_key(cfg, "end_addr", hi);
        if (has_lo || has_hi) {
            if (hi <= lo) {
                throw cpptlm::ParamValidationError(
                    name, "end_addr",
                    "end_addr: expected end_addr > start_addr, got start=" + std::to_string(lo) +
                        " end=" + std::to_string(hi));
            }
            set_addr_range(lo, hi); // ⚠️ 必须走 setter：addr_dist_ 在构造期绑定边界
        }

        // ③ num_requests
        int n = num_requests_;
        if (load_param_opt("num_requests", n)) {
            if (n < 0) {
                throw cpptlm::ParamValidationError(
                    name, "num_requests",
                    "num_requests: expected a non-negative integer, got " + std::to_string(n));
            }
            num_requests_ = n;
        }

        // ④ seed —— ⚠️ 必须经 set_seed（它同时重播种 rng_，直接写成员不生效）
        std::uint64_t sd = seed_;
        if (load_param_opt("seed", sd)) {
            set_seed(sd);
        }

        // ⑤ hotspot_addrs / hotspot_weights —— **非标量**（数组）⇒ 不由 load_param 承载
        //    （`ADR-X.17` D-2）⇒ 模块内解析 + **显式登记**为「已消费但不经 load_param」
        ignore_param("hotspot_addrs");
        ignore_param("hotspot_weights");
        std::vector<uint64_t> hs_addrs;
        std::vector<double> hs_weights;
        const bool has_hs = parse_hotspot(cfg, hs_addrs, hs_weights);

        // ⑥ stride —— 仅对 STRIDED 压力模式有意义
        std::uint64_t stride = 64;
        const bool has_stride = load_param_opt("stride", stride);

        // ── 一致性裁决：零效果键不得静默失效 ─────────────────────────────
        const bool hotspot_mode = (stress_pattern_ == StressPattern::HOTSPOT);
        const bool strided_mode = (stress_pattern_ == StressPattern::STRIDED);

        if (has_hs && !hotspot_mode) {
            throw cpptlm::ParamValidationError(
                name, "hotspot_addrs",
                "hotspot_addrs: requires pattern=HOTSPOT (current pattern is not HOTSPOT)");
        }
        if (hotspot_mode && !has_hs) {
            // ⚠️ HotspotStrategy 的离散分布在权重表为空时是 UB ⇒ 必须显式失败
            throw cpptlm::ParamValidationError(name, "hotspot_addrs",
                                               "hotspot_addrs: required when pattern=HOTSPOT");
        }
        if (has_hs && hs_weights.size() != hs_addrs.size()) {
            throw cpptlm::ParamValidationError(
                name, "hotspot_weights",
                "hotspot_weights: expected the same count as hotspot_addrs (" +
                    std::to_string(hs_addrs.size()) + "), got " +
                    std::to_string(hs_weights.size()));
        }
        if (has_stride && !strided_mode) {
            throw cpptlm::ParamValidationError(
                name, "stride", "stride: requires pattern=STRIDED (current pattern is not STRIDED)");
        }

        // ── 应用（⚠️ 顺序：pattern 必须先于 hotspot / stride）────────────
        if (hotspot_mode) {
            set_hotspot_config(hs_addrs, hs_weights);
        }
        if (strided_mode && has_stride) {
            set_stride(stride);
        }
    }

    /// `pattern` 值 ⇒ 基础模式 + 压力模式（未知值 ⇒ 装配失败）
    void apply_pattern(const std::string& p) {
        if (p == "SEQUENTIAL") {
            set_mode(GenMode_TLM::SEQUENTIAL);
            set_stress_pattern(StressPattern::SEQUENTIAL);
        } else if (p == "RANDOM") {
            set_mode(GenMode_TLM::RANDOM);
            set_stress_pattern(StressPattern::SEQUENTIAL);
        } else if (p == "TRACE") {
            set_mode(GenMode_TLM::TRACE);
            set_stress_pattern(StressPattern::SEQUENTIAL);
        } else if (p == "HOTSPOT") {
            set_mode(GenMode_TLM::SEQUENTIAL);
            set_stress_pattern(StressPattern::HOTSPOT);
        } else if (p == "STRIDED") {
            set_mode(GenMode_TLM::SEQUENTIAL);
            set_stress_pattern(StressPattern::STRIDED);
        } else if (p == "NEIGHBOR") {
            set_mode(GenMode_TLM::SEQUENTIAL);
            set_stress_pattern(StressPattern::NEIGHBOR);
        } else if (p == "TORNADO") {
            set_mode(GenMode_TLM::SEQUENTIAL);
            set_stress_pattern(StressPattern::TORNADO);
        } else {
            throw cpptlm::ParamValidationError(
                name, "pattern",
                "pattern: unknown value '" + p +
                    "' (expected one of SEQUENTIAL/RANDOM/TRACE/HOTSPOT/STRIDED/NEIGHBOR/"
                    "TORNADO)");
        }
    }

    /// 解析地址字面量（十进制或 `0x` 前缀十六进制）
    /// 读取一个地址值 —— 委托共享实现（`include/core/param_loader.hh`）
    ///
    /// ⚠️ **不得**在本类另写一份 —— 同因同策的类已有 `CPUTLM`，
    ///    两份实现必然漂移（先例：本类与 CPUTLM 曾各写一套，CPUTLM 那份漏了
    ///    `is_number_float` 分支 ⇒ `"0x1000"` 写法在 CPUTLM 上装配失败）。
    uint64_t read_addr_value(const json& v, const char* key) const {
        return cpptlm::parse_addr_json(name, key, v);
    }

    /// 读取**顶层**地址键（键存在才读）
    /// @return true = 键存在且已写入 `out`
    bool read_addr_key(const json& cfg, const char* key, uint64_t& out) const {
        auto it = cfg.find(key);
        if (it == cfg.end()) return false;
        out = read_addr_value(*it, key);
        return true;
    }

    /// 解析 `hotspot_addrs` / `hotspot_weights`（**非标量** ⇒ 模块内解析）
    /// @return true = 配置含 `hotspot_addrs`
    bool parse_hotspot(const json& cfg, std::vector<uint64_t>& addrs,
                       std::vector<double>& weights) const {
        if (!cfg.contains("hotspot_addrs")) return false;
        const auto& a = cfg["hotspot_addrs"];
        if (!a.is_array()) {
            throw cpptlm::ParamValidationError(name, "hotspot_addrs",
                                               "hotspot_addrs: expected an array of addresses");
        }
        for (const auto& e : a) {
            // ⚠️ 数组元素同样经 `VarResolver` ⇒ `"0x1000"` 会变成 `4096.0` ⇒ 两种形态都接受
            addrs.push_back(read_addr_value(e, "hotspot_addrs"));
        }
        if (cfg.contains("hotspot_weights")) {
            const auto& w = cfg["hotspot_weights"];
            if (!w.is_array()) {
                throw cpptlm::ParamValidationError(name, "hotspot_weights",
                                                   "hotspot_weights: expected an array of numbers");
            }
            for (const auto& e : w) {
                if (!e.is_number()) {
                    throw cpptlm::ParamValidationError(
                        name, "hotspot_weights",
                        "hotspot_weights: expected an array of numbers, got " + e.dump());
                }
                weights.push_back(e.get<double>());
            }
        }
        return true;
    }

    void set_mode(GenMode_TLM m) { mode_ = m; }
    void set_num_requests(int n) { num_requests_ = n; }

    // ── 复现门(per A4:种子 / 地址段 / 模式 三个 setter)─────────────────
    //
    // ⚠️ **随机性只在 `RANDOM` 模式生效**;`SEQUENTIAL` / `TRACE` 是确定性序列,
    //    与种子无关 ⇒ 复现实验必须显式 `set_mode(GenMode_TLM::RANDOM)`。
    //
    // ⚠️ `set_addr_range` **必须重建** `addr_dist_` —— 分布对象在**构造期**绑定边界,
    //    只改 `start_addr_` / `end_addr_` 不会改变实际取值区间(per A4.1 实测)。
    void set_seed(std::uint64_t seed) {
        seed_ = seed;
        rng_.seed(seed);
    }
    std::uint64_t seed() const { return seed_; }

    void set_addr_range(std::uint64_t lo, std::uint64_t hi) {
        if (hi <= lo)
            return;  // ⛔ 非法区间不改(避免 `uniform_int_distribution` UB)
        start_addr_ = lo;
        end_addr_ = hi;
        cur_addr_ = lo;
        addr_dist_ = std::uniform_int_distribution<std::uint64_t>(lo, hi);
    }
    std::uint64_t start_addr() const { return start_addr_; }
    std::uint64_t end_addr() const { return end_addr_; }

    /// 已发射 / 已完成计数(A4.3 的比较集合之一)
    std::uint64_t issued() const { return static_cast<std::uint64_t>(issued_); }
    std::uint64_t completed() const { return static_cast<std::uint64_t>(completed_); }

    /// 注入**外部**的访问序列(供 H7 对拍用,per
    /// `docs/architecture/cep/parity-baseline.md` §4.1「两侧消费同一条离线 trace」)。
    /// ⚠️ 与构造函数里写死的 4 条默认 trace **互斥**:调用本方法后,
    ///    `TRACE` 模式将回放**注入的**序列(⇒ 两侧逐条相同)。
    /// ⚠️ 调用方需先 `set_mode(GenMode_TLM::TRACE)`。
    void set_trace(const std::vector<TraceRecord_TLM>& trace) {
        trace_ = trace;
        trace_pos_ = 0;
    }

    // 压力模式配置方法
    void set_stress_pattern(StressPattern p) {
        stress_pattern_ = p;
        strategy_ = create_strategy(p);
    }
    void set_hotspot_config(const std::vector<uint64_t>& addrs,
                             const std::vector<double>& weights) {
        if (auto* h = dynamic_cast<HotspotStrategy*>(strategy_.get())) {
            h->set_hotspot_config(addrs, weights);
        }
    }
    void set_stride(uint64_t s) {
        if (auto* st = dynamic_cast<StridedStrategy*>(strategy_.get())) {
            st->set_stride(s);
        }
    }
    void set_mesh_config(uint64_t w, uint64_t h) {
        if (auto* t = dynamic_cast<TornadoStrategy*>(strategy_.get())) {
            t->set_mesh_config(w, h);
        }
    }

    void tick() override {
        current_cycle_++;

        if (resp_in_.valid() && resp_in_.ready()) {
            auto& resp = resp_in_.data();
            uint64_t txn_id = resp.transaction_id.read();
            auto it = txn_issue_time_.find(txn_id);
            if (it != txn_issue_time_.end()) {
                uint64_t latency = current_cycle_ - it->second;
                stats_latency_.sample(latency);
                txn_issue_time_.erase(it);
            }
            completed_++;
            stats_requests_completed_++;
            resp_in_.consume();
        }

        if (completed_ < num_requests_ && issued_ < num_requests_) {
            if (std::uniform_int_distribution<int>{0, 9}(rng_) == 0) {
                issueRequest();
            }
        }

        if (adapter_) adapter_->tick();
    }

    void issueRequest() {
        bundles::CacheReqBundle req;
        bool is_write = false;
        uint64_t addr = 0;

        switch (mode_) {
            case GenMode_TLM::SEQUENTIAL:
                addr = cur_addr_;
                is_write = (cur_addr_ % 8 == 0);
                cur_addr_ += 4;
                if (cur_addr_ >= end_addr_) cur_addr_ = start_addr_;
                break;
            case GenMode_TLM::RANDOM:
                addr = addr_dist_(rng_);
                is_write = (rng_() % 2 == 0);
                break;
            case GenMode_TLM::TRACE:
                if (trace_pos_ >= trace_.size()) return;
                addr = trace_[trace_pos_].addr;
                is_write = trace_[trace_pos_].is_write;
                trace_pos_++;
                break;
        }

        // 如果使用了 stress pattern 策略（非默认 SEQUENTIAL），覆盖地址生成
        if (stress_pattern_ != StressPattern::SEQUENTIAL && strategy_) {
            addr = strategy_->next_address(start_addr_, end_addr_ - start_addr_);
        }

        req.transaction_id.write(issued_++);
        req.address.write(addr);
        req.is_write.write(is_write ? 1 : 0);
        req.data.write(0);
        req.size.write(4);
        req_out_.write(req);

        txn_issue_time_[req.transaction_id.read()] = current_cycle_;
        stats_requests_issued_++;
        if (is_write) {
            stats_writes_++;
        } else {
            stats_reads_++;
        }
        stats_addr_distribution_.sample(addr);
    }

    void do_reset(const ResetConfig& config) override {
        resp_in_.reset();
        req_out_.reset();
        cur_addr_ = start_addr_;
        completed_ = 0;
        issued_ = 0;
        trace_pos_ = 0;
        txn_issue_time_.clear();
        stats_.reset();
        current_cycle_ = 0;
        if (strategy_) strategy_->reset();
    }

    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in() { return resp_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out() { return req_out_; }
    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }

    // Initiator 不接收请求，这些是空适配器（满足 StreamAdapter 接口）
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        static cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        static cpptlm::InputStreamAdapter<bundles::CacheReqBundle> dummy;
        return dummy;
    }

    tlm_stats::StatGroup& stats() { return stats_; }

    void dumpStats(std::ostream& os) {
        stats_.dump(os, getName(), 50);
    }
};

#endif
