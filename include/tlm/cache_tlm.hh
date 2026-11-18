// include/tlm/cache_tlm.hh
// CacheTLM：基于 ch_stream 语义的 Cache 模块（v2.1 新式模型）
// 功能描述：使用 InputStreamAdapter/OutputStreamAdapter 进行模块内部通信，
//           框架层通过 StreamAdapter 自动暴露为 Port
// 作者 CppTLM Team
// 日期 2026-04-12
#ifndef TLM_CACHE_TLM_HH
#define TLM_CACHE_TLM_HH

#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/param_loader.hh"
#include "core/simple_port.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Cache TLM 模块（新式 ch_stream 内部模型）
 *
 * 继承关系：
 *   SimObject
 *   └── ChStreamModuleBase
 *       └── CacheTLM
 *
 * 设计原则：
 * - 模块内部使用 cpptlm::InputStreamAdapter/OutputStreamAdapter（ch_stream 语义）
 * - 框架层通过 StreamAdapter 自动转换为 MasterPort/SlavePort
 * - 业务逻辑不感知外部 Port 的存在
 *
 * JSON 注册名："CacheTLM"
 *
 * ── 容量模型（`cpptlm-module-param-effect-landing` 落地）────────────────────
 * 配置键与语义（**全部可选**;缺省 ⇒ 保持既有行为）:
 * | 键 | 类型 | 缺省 | 语义 |
 * |---|---|---|---|
 * | `size` | 带单位字符串（`"32KB"`） | **不限容量** | 容量上限（字节）;存在即**启用容量模型** |
 * | `line_size` | 整数（字节） | 64 | 容量槽的粒度;`size / line_size` = 槽数;**需要 `size`** |
 * | `associativity` / `assoc` | 整数 | 4 | 每组路数（组相联度）;**需要 `size`** |
 * | `level` | 整数 | 0（未指定） | 层级标签;决定命中延迟（1 → 5 / 2 → 10 / ≥3 → 20 cycle） |
 * | `protocol` | 字符串 | — | **显式拒绝**:协议语义不属于本类（见下方说明） |
 *
 * ⚠️ **协议边界（本类不得含协议语义）**:协议名 / 协议状态 / 目录式追踪**一律不属于**本类
 *    —— 协议侧的实现与状态机在 CEP 层，本类与协议层之间**只允许**经 adapter 交互
 *    （per `docs/adr/ADR-CEP-15-cachetlm-alignment.md` 决策 4）⇒ `protocol` 键
 *    **显式拒绝**（配置写了即装配失败并报出键名），而不是「读进来当标签」。
 *
 * ⚠️ **记账粒度 = 精确地址**（刻意选择，勿与教科书「行粒度」混淆）:命中判定仍是
 *    「该**精确地址**曾被写入」⇒ **默认（未配 `size`）行为与改动前逐位一致**（零回归）;
 *    容量模型只在显式配 `size` 时生效，按**地址**占槽（每槽 1 个被缓存地址），
 *    同一行内的相邻地址各占 1 槽 ⇒ 实际占用字节数可能小于 `size`（保守上界）。
 */
class CacheTLM : public ChStreamModuleBase {
private:
    // 输入/输出适配器（提供 ch_stream valid/ready 语义）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;

    // 业务状态
    std::map<uint64_t, uint64_t> cache_lines_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // ── 容量几何（`size` / `line_size` / `associativity` 派生;见上表）──────
    uint64_t capacity_bytes_ = 0;    // 0 = 不限容量（默认 ⇒ 不做替换记账）
    uint64_t line_size_bytes_ = 64;  // 容量槽粒度（字节）
    uint32_t ways_ = 4;              // 每组路数（组相联度）
    uint64_t effective_ways_ = 4;    // 实际路数（= 槽数 / 组数,受容量下界约束）
    uint64_t num_sets_ = 0;          // 0 = 不限容量;否则 = 组数
    uint32_t level_ = 0;             // 层级标签（0 = 未指定）
    uint64_t hit_latency_ = 5;       // 命中延迟（cycle;随 level 变）
    uint64_t miss_latency_ = 50;     // 未命中延迟（cycle）
    /// 组 → 该组内地址列表（**头部 = 最近使用**;尾部 = 逐出候选）
    std::map<uint64_t, std::list<uint64_t>> set_order_;

    // P3: helper 创建的 PortPair 容器,管理 lazy-bind 的端口对生命周期
    std::vector<std::unique_ptr<PortPair>> helper_pairs_;

    // 性能统计
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_requests_;
    tlm_stats::Scalar& stats_hits_;
    tlm_stats::Scalar& stats_misses_;
    tlm_stats::Distribution& stats_latency_;

public:
    CacheTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq), stats_("cache"),
          stats_requests_(stats_.addScalar("requests", "Total cache requests", "count")),
          stats_hits_(stats_.addScalar("hits", "Cache hits", "count")),
          stats_misses_(stats_.addScalar("misses", "Cache misses", "count")),
          stats_latency_(stats_.addDistribution("latency", "Cache access latency", "cycle")) {
    }

    ~CacheTLM() override = default;

    std::string get_module_type() const override {
        return "CacheTLM";
    }

    // ChStreamModuleBase 接口
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // ── 配置加载（`ADR-X.17` D-1「读取即声明」）──────────────────────────
    //
    // ⚠️ 本类此前**零配置读取面** ⇒ 下列 6 个键全是「配置里有、实现零效果」：
    //    `size` / `level` / `associativity` / `protocol` / `assoc` / `line_size`
    //    ⇒ 本次逐键落地（`size` / `line_size` / `associativity` / `assoc` / `level`
    //    接真实消费;`protocol` **显式拒绝** —— 协议语义不属于本类）。
    void on_config_loaded() override {
        // ① 容量：`size` 存在即**启用容量模型**（缺省 = 不限容量 ⇒ 行为与改动前一致）
        std::string size_text;
        const bool has_size = load_param_opt("size", size_text);

        // ② 槽粒度（`line_size`）
        uint64_t line_size = line_size_bytes_;
        const bool has_line_size = load_param_opt("line_size", line_size);

        // ③ 组相联度（`associativity` 与 `assoc` 是同义别名 ⇒ 同时出现且值不同 ⇒ 失败）
        uint32_t assoc = ways_;
        const bool has_assoc = load_param_opt("associativity", assoc);
        uint32_t assoc_alias = ways_;
        const bool has_assoc_alias = load_param_opt("assoc", assoc_alias);
        if (has_assoc && has_assoc_alias && assoc != assoc_alias) {
            throw cpptlm::ParamValidationError(
                name, "associativity",
                "associativity: conflicting values with its alias 'assoc' (" +
                    std::to_string(assoc) + " vs " + std::to_string(assoc_alias) +
                    ") — specify exactly one");
        }
        if (has_assoc_alias && !has_assoc) assoc = assoc_alias;

        // ④ 层级标签（`level`）⇒ 决定命中延迟
        uint32_t level = level_;
        if (load_param_opt("level", level)) {
            level_ = level;
            hit_latency_ = (level_ <= 1) ? 5u : (level_ == 2 ? 10u : 20u);
        }

        // ⑤ 态②：协议键**显式拒绝**（⛔ 不得静默忽略 —— 那等于把「协议语义」留在本类）
        reject_unsupported_key("protocol");

        // ⑥ 几何键必须与 `size` 同时出现（否则它们没有任何可观测效果 ⇒ 显式失败）
        if (!has_size) {
            const char* orphan = has_line_size ? "line_size"
                               : has_assoc    ? "associativity"
                               : has_assoc_alias ? "assoc"
                                                 : nullptr;
            if (orphan) {
                throw cpptlm::ParamValidationError(
                    name, orphan,
                    std::string(orphan) +
                        ": requires 'size' (the capacity model is enabled by 'size'; without it "
                        "this key has no effect — remove it or add 'size')");
            }
            return;
        }

        // ⑦ 容量与几何的校验 + 推导
        const uint64_t capacity = cpptlm::parse_size_bytes(name, "size", size_text);
        if (capacity == 0) {
            throw cpptlm::ParamValidationError(name, "size", "size: expected a positive size");
        }
        if (line_size == 0 || (line_size & (line_size - 1)) != 0) {
            throw cpptlm::ParamValidationError(
                name, "line_size",
                "line_size: expected a power of two, got " + std::to_string(line_size));
        }
        if (assoc == 0) {
            throw cpptlm::ParamValidationError(
                name, "associativity",
                "associativity: expected a positive number of ways, got 0");
        }
        const uint64_t slots = capacity / line_size;
        if (slots == 0) {
            throw cpptlm::ParamValidationError(
                name, "size",
                "size: capacity " + std::to_string(capacity) +
                    " B is smaller than one line_size unit (" + std::to_string(line_size) +
                    " B) — no slot would fit");
        }

        capacity_bytes_ = capacity;
        line_size_bytes_ = line_size;
        ways_ = assoc;
        num_sets_ = std::max<uint64_t>(1, slots / ways_);
        effective_ways_ = std::max<uint64_t>(1, slots / num_sets_);
    }

    /// 拒绝「本类无对应语义」的键（态② ⇒ 装配期失败，报出键名与原因）
    ///
    /// ⚠️ 错误文本**不得**出现协议名 / 协议状态名（`ADR-CEP-15` 的协议无关性边界
    ///    是**源码级**约束 ⇒ 本文件任何位置出现协议标识符都会红闸）。
    void reject_unsupported_key(const char* key) const {
        if (get_config().contains(key)) {
            throw cpptlm::ParamValidationError(
                name, key,
                std::string(key) +
                    ": this module carries no protocol semantics — protocol-specific behaviour "
                    "belongs to the protocol layer and reaches this module only through an "
                    "adapter (remove this key from the configuration)");
        }
    }

    /// 组索引（容量模型启用时有效;未启用时返回 0）
    uint64_t set_index_of(uint64_t addr) const {
        if (num_sets_ == 0) return 0;
        return (addr / line_size_bytes_) % num_sets_;
    }

    /// 命中时更新组的 LRU 顺序（该地址移到组内头部 = 最近使用）
    void touch(uint64_t addr) {
        if (num_sets_ == 0) return;
        auto& lst = set_order_[set_index_of(addr)];
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            if (*it == addr) {
                lst.erase(it);
                break;
            }
        }
        lst.push_front(addr);
    }

    /// 插入一个被缓存地址（容量模型启用时,组满则逐出**最近最少使用**者）
    void insert_line(uint64_t addr, uint64_t data) {
        if (num_sets_ != 0) {
            auto& lst = set_order_[set_index_of(addr)];
            // 重写已存在的地址 ⇒ 先摘除,避免重复占槽
            for (auto it = lst.begin(); it != lst.end(); ++it) {
                if (*it == addr) {
                    lst.erase(it);
                    break;
                }
            }
            while (lst.size() >= effective_ways_) {
                const uint64_t victim = lst.back();
                lst.pop_back();
                cache_lines_.erase(victim);
            }
            lst.push_front(addr);
        }
        cache_lines_[addr] = data;
    }

    // 模块业务逻辑
    void tick() override {
        // ch_stream 握手：valid && ready 时处理
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();

            uint64_t addr = req.address.read();
            bool is_write = req.is_write.read();

            // 统计：请求数++
            ++stats_requests_;

            // 缓存查找
            bool hit = cache_lines_.count(addr) > 0;
            uint64_t access_latency = hit ? hit_latency_ : miss_latency_; // 延迟随 level 变

            if (is_write) {
                insert_line(addr, req.data.read());
            } else if (hit) {
                touch(addr); // LRU 顺序更新（容量模型启用时才有意义）
            }

            // 统计：命中/未命中
            if (hit) {
                ++stats_hits_;
            } else {
                ++stats_misses_;
            }
            stats_latency_.sample(access_latency);

            // 构建响应
            bundles::CacheRespBundle resp;
            resp.transaction_id.write(req.transaction_id.read());
            resp.data.write(hit ? cache_lines_[addr] : 0);
            resp.is_hit.write(hit ? 1 : 0);
            resp.error_code.write(0);

            resp_out_.write(resp);
            req_in_.consume(); // 握手完成，清除 valid
        }

        // 委托适配器 tick（输出方向数据搬运）
        if (adapter_)
            adapter_->tick();
    }

    void do_reset(const ResetConfig& /*config*/) override {
        cache_lines_.clear();
        set_order_.clear();
        req_in_.reset();
        resp_out_.reset();
        stats_.reset();
    }

    // 访问器（供 StreamAdapter 使用）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        return req_in_;
    }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        return resp_out_;
    }
    // P0-5b: 被动响应模块,无 req 输出/resp 输入。返回静态 dummy 供 StreamAdapter 统一调用。
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out() {
        static cpptlm::OutputStreamAdapter<bundles::CacheReqBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in() {
        static cpptlm::InputStreamAdapter<bundles::CacheRespBundle> dummy;
        return dummy;
    }
    cpptlm::StreamAdapterBase* get_adapter() const {
        return adapter_;
    }

    // 统计访问器
    tlm_stats::StatGroup& stats() {
        return stats_;
    }
    const tlm_stats::StatGroup& stats() const {
        return stats_;
    }

    // ChStreamModuleBase 统计接口
    tlm_stats::StatGroup* get_stats_group() override {
        return &stats_;
    }
    std::string get_stats_path() const override {
        return "system.cache";
    }

    void dumpStats(std::ostream& os) const {
        stats().dump(os);
    }

    // P3: helper 方法 - 隐藏 mem_side 端口方向
    // 借鉴 gem5 caches.py::L1Cache.connectBus
    // P3 partial: 不依赖 D.1, 仅 lazy 注册 mem_side 端口 + bind 总线
    // 注意: 参数类型为 ChStreamModuleBase* (非 SimModule*),
    //   因为 CacheTLM/CrossbarTLM/MemoryTLM 均派生自 ChStreamModuleBase 而非 SimModule
    void connectBus(ChStreamModuleBase* bus);
};

#endif // TLM_CACHE_TLM_HH
