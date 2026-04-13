// include/framework/transaction_tracker.hh
// Transaction Tracker - 交易追踪单例（CppTLM v2.0）
#ifndef TRANSACTION_TRACKER_HH
#define TRANSACTION_TRACKER_HH

#include "ext/transaction_context_ext.hh"
#include <map>
#include <vector>
#include <string>
#include <cstdint>

/**
 * @brief 交易记录
 * 
 * 用于追踪每个交易的完整生命周期
 */
struct TransactionRecord {
    uint64_t transaction_id = 0;
    uint64_t parent_id = 0;
    uint8_t  fragment_id = 0;
    uint8_t  fragment_total = 1;
    std::string source_module;
    std::string type;  // "READ" / "WRITE" / "ATOMIC"
    uint64_t create_timestamp = 0;
    uint64_t complete_timestamp = 0;
    std::vector<std::pair<std::string, uint64_t>> hop_log;  // (module, latency)
    bool is_complete = false;
    
    // 父子关系
    std::vector<uint64_t> child_transactions;
};

/**
 * @brief 交易追踪器（单例模式）
 * 
 * 功能：
 * - 创建/销毁交易记录
 * - 记录交易跳变（hop）
 * - 父子交易关联
 * - 分片交易追踪
 * - 导出追踪日志
 * 
 * 使用方式：
 *   auto& tracker = TransactionTracker::instance();
 *   tracker.initialize();
 *   uint64_t tid = tracker.create_transaction(payload, "cpu", "READ");
 */
class TransactionTracker {
private:
    std::map<uint64_t, TransactionRecord> transactions_;
    std::map<uint64_t, std::vector<uint64_t>> parent_child_map_;
    uint64_t global_timestamp_ = 0;
    
    bool enable_coarse_grained_ = true;   // 粗粒度追踪（父交易）
    bool enable_fine_grained_ = true;     // 细粒度追踪（分片）
    bool initialized_ = false;
    
    // 单例私有构造
    TransactionTracker() = default;
    ~TransactionTracker() = default;
    TransactionTracker(const TransactionTracker&) = delete;
    TransactionTracker& operator=(const TransactionTracker&) = delete;

public:
    /**
     * @brief 获取单例实例
     * @return TransactionTracker 引用
     */
    static TransactionTracker& instance() {
        static TransactionTracker tracker;
        return tracker;
    }
    
    /**
     * @brief 初始化追踪器
     * @param export_path 导出路径（可选）
     */
    void initialize(const std::string& export_path = "") {
        if (initialized_) return;
        
        transactions_.clear();
        parent_child_map_.clear();
        global_timestamp_ = 0;
        initialized_ = true;
        
        (void)export_path;  // 暂不实现导出
    }
    
    /**
     * @brief 创建交易记录
     * @param payload tlm_generic_payload pointer
     * @param source 源模块名称
     * @param type 交易类型（READ/WRITE）
     * @return 交易 ID
     */
    uint64_t create_transaction(tlm::tlm_generic_payload* payload, 
                                const std::string& source, 
                                const std::string& type) {
        // 分配新交易 ID
        uint64_t tid = next_transaction_id_++;
        
        TransactionRecord record;
        record.transaction_id = tid;
        record.source_module = source;
        record.type = type;
        record.create_timestamp = global_timestamp_;
        
        // 同步到 Extension
        if (payload) {
            auto* ext = create_transaction_context(payload, tid, 0, 0, 1);
            ext->source_module = source;
            ext->type = type;
            ext->create_timestamp = global_timestamp_;
        }
        
        transactions_[tid] = record;
        return tid;
    }
    
    /**
     * @brief 记录交易跳变（经过一个模块）
     * @param tid 交易 ID
     * @param module 模块名称
     * @param latency 延迟
     * @param event 事件类型
     */
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency, const std::string& event) {
        if (transactions_.count(tid) == 0) return;
        
        auto& record = transactions_[tid];
        record.hop_log.emplace_back(module, latency);
        
        // 同时记录到 Extension
        // （需要 payload 指针，暂不实现）
        (void)event;
    }
    
    /**
     * @brief 完成交易
     * @param tid 交易 ID
     */
    void complete_transaction(uint64_t tid) {
        if (transactions_.count(tid) == 0) return;
        
        auto& record = transactions_[tid];
        record.is_complete = true;
        record.complete_timestamp = global_timestamp_;
        
        // 如果是子交易，检查父交易是否可以完成
        if (record.parent_id != 0) {
            check_parent_completion(record.parent_id);
        }
    }
    
    /**
     * @brief 链接父子交易
     * @param parent_id 父交易 ID
     * @param child_id 子交易 ID
     */
    void link_transactions(uint64_t parent_id, uint64_t child_id) {
        if (transactions_.count(parent_id) == 0) return;
        if (transactions_.count(child_id) == 0) return;
        
        auto& parent = transactions_[parent_id];
        auto& child = transactions_[child_id];
        
        child.parent_id = parent_id;
        parent.child_transactions.push_back(child_id);
        
        parent_child_map_[parent_id].push_back(child_id);
        
        // 同步到 Extension
        if (child.transaction_id == child_id) {
            // （需要 payload 指针，暂不实现）
        }
        (void)child;
    }
    
    /**
     * @brief 设置分片信息
     * @param tid 交易 ID
     * @param fragment_id 分片 ID
     * @param fragment_total 分片总数
     */
    void set_fragment_info(uint64_t tid, uint8_t fragment_id, uint8_t fragment_total) {
        if (transactions_.count(tid) == 0) return;
        
        auto& record = transactions_[tid];
        record.fragment_id = fragment_id;
        record.fragment_total = fragment_total;
    }
    
    /**
     * @brief 获取交易记录
     * @param tid 交易 ID
     * @return 交易记录指针（不存在返回 nullptr）
     */
    const TransactionRecord* get_transaction(uint64_t tid) const {
        auto it = transactions_.find(tid);
        if (it == transactions_.end()) return nullptr;
        return &it->second;
    }
    
    /**
     * @brief 获取子交易列表
     * @param parent_id 父交易 ID
     * @return 子交易 ID 列表
     */
    std::vector<uint64_t> get_children(uint64_t parent_id) const {
        auto it = parent_child_map_.find(parent_id);
        if (it == parent_child_map_.end()) return {};
        return it->second;
    }
    
    /**
     * @brief 获取所有活跃交易
     * @return 活跃交易 ID 列表
     */
    std::vector<uint64_t> get_active_transactions() const {
        std::vector<uint64_t> active;
        for (const auto& [tid, record] : transactions_) {
            if (!record.is_complete) {
                active.push_back(tid);
            }
        }
        return active;
    }
    
    /**
     * @brief 推进全局时间
     * @param delta 时间增量
     */
    void advance_time(uint64_t delta) {
        global_timestamp_ += delta;
    }
    
    /**
     * @brief 获取当前时间
     * @return 当前全局时间戳
     */
    uint64_t get_current_time() const {
        return global_timestamp_;
    }
    
    /**
     * @brief 配置粗粒度追踪
     * @param enable 是否启用
     */
    void enable_coarse_grained(bool enable) {
        enable_coarse_grained_ = enable;
    }
    
    /**
     * @brief 配置细粒度追踪
     * @param enable 是否启用
     */
    void enable_fine_grained(bool enable) {
        enable_fine_grained_ = enable;
    }
    
    /**
     * @brief 检查是否已初始化
     */
    bool is_initialized() const {
        return initialized_;
    }
    
    /**
     * @brief 获取活跃交易数量
     */
    size_t active_count() const {
        return get_active_transactions().size();
    }
    
    /**
     * @brief 获取完成交易数量
     */
    size_t completed_count() const {
        size_t count = 0;
        for (const auto& [_, record] : transactions_) {
            if (record.is_complete) count++;
        }
        return count;
    }

    /**
     * @brief 测试重置：清空所有状态
     */
    void reset_for_testing() {
        transactions_.clear();
        parent_child_map_.clear();
        global_timestamp_ = 0;
        next_transaction_id_ = 1;
        initialized_ = false;
    }

private:
    uint64_t next_transaction_id_ = 1;
    
    /**
     * @brief 检查父交易是否可以完成
     * @param parent_id 父交易 ID
     */
    void check_parent_completion(uint64_t parent_id) {
        if (transactions_.count(parent_id) == 0) return;
        
        auto& parent = transactions_[parent_id];
        
        // 检查所有子交易是否都完成
        bool all_complete = true;
        for (uint64_t child_id : parent.child_transactions) {
            auto it = transactions_.find(child_id);
            if (it != transactions_.end() && !it->second.is_complete) {
                all_complete = false;
                break;
            }
        }
        
        // 所有子交易完成，标记父交易完成
        if (all_complete) {
            parent.is_complete = true;
            parent.complete_timestamp = global_timestamp_;
        }
    }
};

#endif // TRANSACTION_TRACKER_HH
