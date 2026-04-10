// include/framework/debug_tracker.hh
// Debug Tracker - 错误追踪单例（CppTLM v2.0）
// 严格遵循 ADR-X.2: 错误处理策略

#ifndef DEBUG_TRACKER_HH
#define DEBUG_TRACKER_HH

#include "ext/error_context_ext.hh"
#include "core/error_category.hh"
#include <map>
#include <vector>
#include <string>
#include <cstdint>

/**
 * @brief 错误记录
 * 
 * 用于追踪每个错误的完整上下文
 */
struct ErrorRecord {
    uint64_t error_id = 0;
    ErrorCode error_code = ErrorCode::SUCCESS;
    ErrorCategory error_category = ErrorCategory::SUCCESS;
    std::string error_message;
    std::string source_module;
    uint64_t transaction_id = 0;
    uint64_t timestamp = 0;
    std::vector<std::pair<std::string, uint64_t>> stack_trace;  // (module, timestamp)
    
    // 一致性特定信息
    CoherenceState expected_state = CoherenceState::INVALID;
    CoherenceState actual_state = CoherenceState::INVALID;
    
    bool is_fatal = false;
};

/**
 * @brief 状态快照
 * 
 * 记录地址状态变化历史
 */
struct StateSnapshot {
    uint64_t address = 0;
    CoherenceState from_state = CoherenceState::INVALID;
    CoherenceState to_state = CoherenceState::INVALID;
    std::string event;
    uint64_t transaction_id = 0;
    uint64_t timestamp = 0;
};

/**
 * @brief 调试追踪器（单例模式）
 * 
 * 功能：
 * - 错误记录（按 transaction_id 索引）
 * - 状态快照（按地址索引）
 * - 查询接口（按错误/交易/类别/模块）
 * - 回放支持
 * 
 * 遵循 ADR-X.2 决议：
 * - 内存索引 + 查询接口
 * - 与 TransactionTracker 共享框架
 * - 支持错误和状态历史追踪
 */
class DebugTracker {
private:
    std::map<uint64_t, ErrorRecord> errors_;
    std::map<uint64_t, std::vector<StateSnapshot>> state_history_;
    
    bool enable_error_tracking_ = true;
    bool enable_state_history_ = true;
    bool stop_on_fatal_ = false;
    bool initialized_ = false;
    
    uint64_t next_error_id_ = 1;

    // 单例私有构造
    DebugTracker() = default;
    ~DebugTracker() = default;
    DebugTracker(const DebugTracker&) = delete;
    DebugTracker& operator=(const DebugTracker&) = delete;

public:
    /**
     * @brief 获取单例实例
     */
    static DebugTracker& instance() {
        static DebugTracker tracker;
        return tracker;
    }
    
    /**
     * @brief 初始化追踪器
     */
    void initialize(bool enable_errors = true, 
                   bool enable_state = true,
                   bool stop_on_fatal = false) {
        if (initialized_) return;
        
        enable_error_tracking_ = enable_errors;
        enable_state_history_ = enable_state;
        stop_on_fatal_ = stop_on_fatal;
        
        errors_.clear();
        state_history_.clear();
        next_error_id_ = 1;
        initialized_ = true;
    }
    
    /**
     * @brief 记录错误
     * @param payload tlm::tlm_generic_payload pointer
     * @param code 错误码
     * @param message 错误消息
     * @param module 源模块
     * @return 错误 ID
     */
    uint64_t record_error(tlm::tlm_generic_payload* payload,
                         ErrorCode code,
                         const std::string& message,
                         const std::string& module) {
        if (!enable_error_tracking_) return 0;
        
        ErrorRecord record;
        record.error_id = next_error_id_++;
        record.error_code = code;
        record.error_category = get_error_category(code);
        record.error_message = message;
        record.source_module = module;
        record.is_fatal = is_fatal_error(code);
        
        // 从 payload 获取 transaction_id
        if (payload) {
            ErrorContextExt* err_ext = nullptr;
            payload->get_extension(err_ext);
            if (err_ext) {
                record.transaction_id = err_ext->error_code == code ? 
                    static_cast<uint64_t>(0) : static_cast<uint64_t>(0);
            }
            
            TransactionContextExt* txn_ext = nullptr;
            payload->get_extension(txn_ext);
            if (txn_ext) {
                record.transaction_id = txn_ext->transaction_id;
            }
        }
        
        // 同步到 ErrorContextExt
        if (payload) {
            ErrorContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (!ext) {
                ext = create_error_context(payload, code, message, module);
            } else {
                ext->error_code = code;
                ext->error_category = get_error_category(code);
                ext->error_message = message;
                ext->source_module = module;
            }
        }
        
        errors_[record.error_id] = record;
        
        // 如果是严重错误且配置了停止，可以触发断言
        if (stop_on_fatal_ && record.is_fatal) {
            // 暂不实现停止，仅记录
        }
        
        return record.error_id;
    }
    
    /**
     * @brief 记录状态转换
     */
    void record_state_transition(uint64_t address,
                                 CoherenceState from_state,
                                 CoherenceState to_state,
                                 const std::string& event,
                                 uint64_t transaction_id) {
        if (!enable_state_history_) return;
        
        StateSnapshot snapshot;
        snapshot.address = address;
        snapshot.from_state = from_state;
        snapshot.to_state = to_state;
        snapshot.event = event;
        snapshot.transaction_id = transaction_id;
        snapshot.timestamp = 0;  // 可用全局时间
        
        state_history_[address].push_back(snapshot);
    }
    
    /**
     * @brief 按错误 ID 查询
     */
    const ErrorRecord* get_error(uint64_t error_id) const {
        auto it = errors_.find(error_id);
        if (it == errors_.end()) return nullptr;
        return &it->second;
    }
    
    /**
     * @brief 按交易 ID 查询错误
     */
    std::vector<ErrorRecord> get_errors_by_transaction(uint64_t tid) const {
        std::vector<ErrorRecord> result;
        for (const auto& [id, record] : errors_) {
            if (record.transaction_id == tid) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    /**
     * @brief 按错误类别查询
     */
    std::vector<ErrorRecord> get_errors_by_category(ErrorCategory cat) const {
        std::vector<ErrorRecord> result;
        for (const auto& [id, record] : errors_) {
            if (record.error_category == cat) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    /**
     * @brief 按模块查询错误
     */
    std::vector<ErrorRecord> get_errors_by_module(const std::string& module) const {
        std::vector<ErrorRecord> result;
        for (const auto& [id, record] : errors_) {
            if (record.source_module == module) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    /**
     * @brief 获取地址状态历史
     */
    std::vector<StateSnapshot> get_state_history(uint64_t address) const {
        auto it = state_history_.find(address);
        if (it == state_history_.end()) return {};
        return it->second;
    }
    
    /**
     * @brief 清空所有记录
     */
    void clear_all() {
        errors_.clear();
        state_history_.clear();
        next_error_id_ = 1;
    }
    
    /**
     * @brief 获取错误数量
     */
    size_t error_count() const {
        return errors_.size();
    }
    
    /**
     * @brief 获取状态快照数量
     */
    size_t state_snapshot_count() const {
        size_t count = 0;
        for (const auto& [addr, snapshots] : state_history_) {
            count += snapshots.size();
        }
        return count;
    }
    
    /**
     * @brief 检查是否已初始化
     */
    bool is_initialized() const {
        return initialized_;
    }
    
    /**
     * @brief 配置严重错误停止
     */
    void enable_stop_on_fatal(bool enable) {
        stop_on_fatal_ = enable;
    }
    
    /**
     * @brief 测试用重置
     */
    void reset_for_testing() {
        initialized_ = false;
        clear_all();
    }
};

#endif // DEBUG_TRACKER_HH
