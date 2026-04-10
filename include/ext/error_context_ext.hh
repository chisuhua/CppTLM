// include/ext/error_context_ext.hh
// Error Context Extension for CppTLM v2.0
// 严格遵循 ADR-X.2: 错误处理策略

#ifndef ERROR_CONTEXT_EXT_HH
#define ERROR_CONTEXT_EXT_HH

#include "tlm.h"
#include "core/error_category.hh"
#include <vector>
#include <string>
#include <map>
#include <cstdint>

/**
 * @brief 一致性状态枚举
 * 
 * 用于追踪缓存一致性状态转换
 */
enum class CoherenceState : uint8_t {
    INVALID = 0,
    SHARED = 1,
    EXCLUSIVE = 2,
    MODIFIED = 3
};

/**
 * @brief 错误上下文扩展
 * 
 * 功能：
 * - 错误码和类别存储
 * - 错误消息和来源模块
 * - 堆栈追踪
 * - 上下文数据（地址、期望值、实际值等）
 * - 一致性特定字段
 * 
 * 遵循 ADR-X.2 决议：
 * - 分层错误分类
 * - 与 TransactionContextExt 共享 payload
 * - 支持 DebugTracker 索引
 */
struct ErrorContextExt : public tlm::tlm_extension<ErrorContextExt> {
    // ========== 核心错误信息 ==========
    ErrorCode error_code = ErrorCode::SUCCESS;
    ErrorCategory error_category = ErrorCategory::SUCCESS;
    std::string error_message;
    std::string source_module;
    uint64_t timestamp = 0;
    
    // ========== 堆栈追踪 ==========
    struct StackFrame {
        std::string module;
        std::string function;
        std::string context;  // 行号或其他上下文
    };
    std::vector<StackFrame> stack_trace;
    
    // ========== 上下文数据 ==========
    std::map<std::string, uint64_t> context_data;
    
    // ========== 一致性特定字段 ==========
    CoherenceState expected_state = CoherenceState::INVALID;
    CoherenceState actual_state = CoherenceState::INVALID;
    std::vector<uint32_t> sharers;  // 共享者掩码
    
    // ========== TLM Extension 必需方法 ==========
    
    tlm_extension* clone() const override {
        return new ErrorContextExt(*this);
    }
    
    void copy_from(const tlm_extension_base& ext) override {
        const auto& other = static_cast<const ErrorContextExt&>(ext);
        error_code = other.error_code;
        error_category = other.error_category;
        error_message = other.error_message;
        source_module = other.source_module;
        timestamp = other.timestamp;
        stack_trace = other.stack_trace;
        context_data = other.context_data;
        expected_state = other.expected_state;
        actual_state = other.actual_state;
        sharers = other.sharers;
    }
    
    // ========== 辅助方法 ==========
    
    /**
     * @brief 添加堆栈帧
     */
    void add_stack_frame(const std::string& module, 
                         const std::string& function,
                         const std::string& context = "") {
        stack_trace.push_back({module, function, context});
    }
    
    /**
     * @brief 设置上下文数据
     */
    void set_context_data(const std::string& key, uint64_t value) {
        context_data[key] = value;
    }
    
    /**
     * @brief 获取上下文数据
     */
    uint64_t get_context_data(const std::string& key, uint64_t default_val = 0) const {
        auto it = context_data.find(key);
        return (it != context_data.end()) ? it->second : default_val;
    }
    
    /**
     * @brief 检查是否为严重错误
     */
    bool is_fatal() const {
        return ::is_fatal_error(error_code);
    }
    
    /**
     * @brief 检查是否可恢复
     */
    bool is_recoverable() const {
        return ::is_recoverable_error(error_code);
    }
    
    /**
     * @brief 重置错误上下文
     */
    void reset() {
        error_code = ErrorCode::SUCCESS;
        error_category = ErrorCategory::SUCCESS;
        error_message.clear();
        source_module.clear();
        timestamp = 0;
        stack_trace.clear();
        context_data.clear();
        expected_state = CoherenceState::INVALID;
        actual_state = CoherenceState::INVALID;
        sharers.clear();
    }
};

// ========== 便捷函数 ==========

/**
 * @brief 从 payload 获取 ErrorContextExt
 */
inline ErrorContextExt* get_error_context(tlm::tlm_generic_payload* p) {
    if (!p) return nullptr;
    ErrorContextExt* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

/**
 * @brief 设置错误上下文
 */
inline void set_error_context(tlm::tlm_generic_payload* p, const ErrorContextExt& src) {
    if (!p) return;
    ErrorContextExt* ext = new ErrorContextExt(src);
    p->set_extension(ext);
}

/**
 * @brief 创建并设置错误上下文
 */
inline ErrorContextExt* create_error_context(
    tlm::tlm_generic_payload* p,
    ErrorCode code,
    const std::string& message,
    const std::string& source
) {
    if (!p) return nullptr;
    ErrorContextExt* ext = new ErrorContextExt();
    ext->error_code = code;
    ext->error_category = get_error_category(code);
    ext->error_message = message;
    ext->source_module = source;
    p->set_extension(ext);
    return ext;
}

#endif // ERROR_CONTEXT_EXT_HH
