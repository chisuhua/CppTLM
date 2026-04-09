// include/core/error_category.hh
// 错误码定义 - CppTLM v2.0
#ifndef ERROR_CATEGORY_HH
#define ERROR_CATEGORY_HH

#include <cstdint>
#include <string>

/**
 * @brief 错误类别
 * 
 * 分层错误分类体系
 */
enum class ErrorCategory : uint8_t {
    SUCCESS     = 0x00,  ///< 成功（非错误）
    TRANSPORT   = 0x01,  ///< 传输层错误
    RESOURCE    = 0x02,  ///< 资源层错误
    COHERENCE   = 0x03,  ///< 一致性层错误
    PROTOCOL    = 0x04,  ///< 协议层错误
    SECURITY    = 0x05,  ///< 安全层错误
    PERFORMANCE = 0x06,  ///< 性能层错误
    CUSTOM      = 0x10,  ///< 用户自定义
};

/**
 * @brief 错误码
 * 
 * 按类别组织：高字节为类别，低字节为具体错误
 * 格式：0x{category}_{code}
 */
enum class ErrorCode : uint16_t {
    // ========== 成功 ==========
    SUCCESS = 0x0000,
    
    // ========== 传输层错误 (0x01xx) ==========
    TRANSPORT_INVALID_ADDRESS = 0x0100,
    TRANSPORT_ACCESS_DENIED = 0x0101,
    TRANSPORT_TIMEOUT = 0x0102,
    TRANSPORT_CONNECTION_FAILED = 0x0103,
    TRANSPORT_NO_ROUTE = 0x0104,
    TRANSPORT_BUFFER_OVERFLOW = 0x0105,
    
    // ========== 资源层错误 (0x02xx) ==========
    RESOURCE_BUFFER_FULL = 0x0200,
    RESOURCE_OUT_OF_MEMORY = 0x0201,
    RESOURCE_STARVATION = 0x0202,
    RESOURCE_EXHAUSTED = 0x0203,
    RESOURCE_NOT_AVAILABLE = 0x0204,
    
    // ========== 一致性层错误 (0x03xx) ==========
    COHERENCE_STATE_VIOLATION = 0x0300,
    COHERENCE_DEADLOCK = 0x0301,
    COHERENCE_LIVELOCK = 0x0302,
    COHERENCE_DATA_INCONSISTENCY = 0x0303,
    COHERENCE_INVALID_TRANSITION = 0x0304,
    COHERENCE_SNOOP_TIMEOUT = 0x0305,
    COHERENCE_DIRECTORY_FULL = 0x0306,
    
    // ========== 协议层错误 (0x04xx) ==========
    PROTOCOL_ID_CONFLICT = 0x0400,
    PROTOCOL_OUT_OF_ORDER = 0x0401,
    PROTOCOL_INVALID_COMMAND = 0x0402,
    PROTOCOL_CHECKSUM_ERROR = 0x0403,
    PROTOCOL_FORMAT_ERROR = 0x0404,
    PROTOCOL_TIMEOUT = 0x0405,
    
    // ========== 安全层错误 (0x05xx) ==========
    SECURITY_AUTHENTICATION_FAILED = 0x0500,
    SECURITY_AUTHORIZATION_FAILED = 0x0501,
    SECURITY_ENCRYPTION_ERROR = 0x0502,
    SECURITY_TAMPER_DETECTED = 0x0503,
    
    // ========== 性能层错误 (0x06xx) ==========
    PERFORMANCE_THRESHOLD_EXCEEDED = 0x0600,
    PERFORMANCE_QOS_VIOLATION = 0x0601,
    PERFORMANCE_BANDWIDTH_EXCEEDED = 0x0602,
    
    // ========== 自定义错误 (0x10xx) ==========
    CUSTOM_MIN = 0x1000,
    CUSTOM_MAX = 0x10FF,
};

/**
 * @brief 获取错误类别
 * @param code 错误码
 * @return 错误类别
 */
inline ErrorCategory get_error_category(ErrorCode code) {
    return static_cast<ErrorCategory>((static_cast<uint16_t>(code) >> 8) & 0xFF);
}

/**
 * @brief 错误码转字符串
 * @param code 错误码
 * @return 字符串描述
 */
inline std::string error_code_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        
        // 传输层
        case ErrorCode::TRANSPORT_INVALID_ADDRESS: return "TRANSPORT_INVALID_ADDRESS";
        case ErrorCode::TRANSPORT_ACCESS_DENIED: return "TRANSPORT_ACCESS_DENIED";
        case ErrorCode::TRANSPORT_TIMEOUT: return "TRANSPORT_TIMEOUT";
        case ErrorCode::TRANSPORT_CONNECTION_FAILED: return "TRANSPORT_CONNECTION_FAILED";
        case ErrorCode::TRANSPORT_NO_ROUTE: return "TRANSPORT_NO_ROUTE";
        case ErrorCode::TRANSPORT_BUFFER_OVERFLOW: return "TRANSPORT_BUFFER_OVERFLOW";
        
        // 资源层
        case ErrorCode::RESOURCE_BUFFER_FULL: return "RESOURCE_BUFFER_FULL";
        case ErrorCode::RESOURCE_OUT_OF_MEMORY: return "RESOURCE_OUT_OF_MEMORY";
        case ErrorCode::RESOURCE_STARVATION: return "RESOURCE_STARVATION";
        case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        
        // 一致性层
        case ErrorCode::COHERENCE_STATE_VIOLATION: return "COHERENCE_STATE_VIOLATION";
        case ErrorCode::COHERENCE_DEADLOCK: return "COHERENCE_DEADLOCK";
        case ErrorCode::COHERENCE_LIVELOCK: return "COHERENCE_LIVELOCK";
        case ErrorCode::COHERENCE_DATA_INCONSISTENCY: return "COHERENCE_DATA_INCONSISTENCY";
        case ErrorCode::COHERENCE_INVALID_TRANSITION: return "COHERENCE_INVALID_TRANSITION";
        
        // 协议层
        case ErrorCode::PROTOCOL_ID_CONFLICT: return "PROTOCOL_ID_CONFLICT";
        case ErrorCode::PROTOCOL_OUT_OF_ORDER: return "PROTOCOL_OUT_OF_ORDER";
        case ErrorCode::PROTOCOL_INVALID_COMMAND: return "PROTOCOL_INVALID_COMMAND";
        
        // 默认
        default: return "UNKNOWN_ERROR";
    }
}

/**
 * @brief 错误类别转字符串
 * @param cat 错误类别
 * @return 字符串描述
 */
inline std::string error_category_to_string(ErrorCategory cat) {
    switch (cat) {
        case ErrorCategory::SUCCESS: return "SUCCESS";
        case ErrorCategory::TRANSPORT: return "TRANSPORT";
        case ErrorCategory::RESOURCE: return "RESOURCE";
        case ErrorCategory::COHERENCE: return "COHERENCE";
        case ErrorCategory::PROTOCOL: return "PROTOCOL";
        case ErrorCategory::SECURITY: return "SECURITY";
        case ErrorCategory::PERFORMANCE: return "PERFORMANCE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 检查是否为严重错误
 * @param code 错误码
 * @return true 如果是严重错误
 */
inline bool is_fatal_error(ErrorCode code) {
    return code == ErrorCode::COHERENCE_DEADLOCK ||
           code == ErrorCode::COHERENCE_DATA_INCONSISTENCY ||
           code == ErrorCode::SECURITY_TAMPER_DETECTED ||
           code == ErrorCode::RESOURCE_OUT_OF_MEMORY;
}

/**
 * @brief 检查是否为可恢复错误
 * @param code 错误码
 * @return true 如果可恢复
 */
inline bool is_recoverable_error(ErrorCode code) {
    return code == ErrorCode::RESOURCE_BUFFER_FULL ||
           code == ErrorCode::TRANSPORT_TIMEOUT ||
           code == ErrorCode::RESOURCE_STARVATION;
}

/**
 * @brief 检查是否为成功状态
 * @param code 错误码
 * @return true 如果成功
 */
inline bool is_success(ErrorCode code) {
    return code == ErrorCode::SUCCESS;
}

#endif // ERROR_CATEGORY_HH
