// include/framework/error_category.hh
// CppTLM 错误码定义
// 按类别组织：传输层/资源层/一致性层/协议层/安全层/性能层

#ifndef ERROR_CATEGORY_HH
#define ERROR_CATEGORY_HH

#include <cstdint>
#include <string>

// ========== 错误类别枚举 ==========
enum class ErrorCategory : uint8_t {
    TRANSPORT   = 0x01,  // 传输层错误（地址、超时、协议违例）
    RESOURCE    = 0x02,  // 资源层错误（缓冲区满、队列溢出）
    COHERENCE   = 0x03,  // 一致性层错误（状态违例、死锁、活锁）
    PROTOCOL    = 0x04,  // 协议层错误（ID 冲突、乱序）
    SECURITY    = 0x05,  // 安全层错误（权限、加密）
    PERFORMANCE = 0x06,  // 性能层错误（阈值超限）
    CUSTOM      = 0x10,  // 用户自定义
};

// ========== 错误码枚举 ==========
// 高 8 位：类别，低 8 位：具体错误
enum class ErrorCode : uint16_t {
    // 成功/信息
    SUCCESS = 0x0000,
    RETRY   = 0x0001,  // 需要重试（非错误）
    NACK    = 0x0002,  // 否定确认

    // 传输层错误 (0x01xx)
    TRANSPORT_INVALID_ADDRESS    = 0x0100,
    TRANSPORT_ACCESS_DENIED      = 0x0101,
    TRANSPORT_MEMORY_NOT_MAPPED  = 0x0102,
    TRANSPORT_ALIGNMENT_FAULT    = 0x0103,
    TRANSPORT_TIMEOUT            = 0x0104,
    TRANSPORT_PROTOCOL_VIOLATION = 0x0105,

    // 资源层错误 (0x02xx)
    RESOURCE_BUFFER_FULL    = 0x0200,
    RESOURCE_QUEUE_OVERFLOW = 0x0201,
    RESOURCE_CONFLICT       = 0x0202,
    RESOURCE_STARVATION     = 0x0203,

    // 一致性层错误 (0x03xx)
    COHERENCE_STATE_VIOLATION    = 0x0300,
    COHERENCE_DEADLOCK           = 0x0301,
    COHERENCE_LIVELOCK           = 0x0302,
    COHERENCE_DATA_INCONSISTENCY = 0x0303,
    COHERENCE_INVALID_TRANSITION = 0x0304,
    COHERENCE_SNOOP_CONFLICT     = 0x0305,
    COHERENCE_DIRECTORY_OVERFLOW = 0x0306,
    COHERENCE_SHARERS_LIMIT      = 0x0307,

    // 协议层错误 (0x04xx)
    PROTOCOL_ID_CONFLICT     = 0x0400,
    PROTOCOL_OUT_OF_ORDER    = 0x0401,
    PROTOCOL_SEQUENCE_ERROR  = 0x0402,
    PROTOCOL_INVALID_COMMAND = 0x0403,

    // 安全层错误 (0x05xx)
    SECURITY_PERMISSION_DENIED     = 0x0500,
    SECURITY_ENCRYPTION_ERROR      = 0x0501,
    SECURITY_AUTHENTICATION_FAILED = 0x0502,

    // 性能层错误 (0x06xx)
    PERFORMANCE_THRESHOLD_EXCEEDED = 0x0600,
    PERFORMANCE_HOTSPOT_DETECTED   = 0x0601,
};

// ========== 工具函数 ==========

inline ErrorCategory get_error_category(ErrorCode code) {
    uint16_t v = static_cast<uint16_t>(code);
    if (v <= 0x0002) return ErrorCategory::TRANSPORT;
    return static_cast<ErrorCategory>((v >> 8) & 0xFF);
}

inline std::string error_category_to_string(ErrorCategory cat) {
    switch (cat) {
        case ErrorCategory::TRANSPORT:   return "TRANSPORT";
        case ErrorCategory::RESOURCE:    return "RESOURCE";
        case ErrorCategory::COHERENCE:   return "COHERENCE";
        case ErrorCategory::PROTOCOL:    return "PROTOCOL";
        case ErrorCategory::SECURITY:    return "SECURITY";
        case ErrorCategory::PERFORMANCE: return "PERFORMANCE";
        case ErrorCategory::CUSTOM:      return "CUSTOM";
        default: return "UNKNOWN";
    }
}

inline std::string error_code_to_string(ErrorCode code) {
    uint16_t v = static_cast<uint16_t>(code);
    return error_category_to_string(get_error_category(code)) + "_" + std::to_string(v & 0xFF);
}

inline bool is_error(ErrorCode code) {
    return code != ErrorCode::SUCCESS && code != ErrorCode::RETRY && code != ErrorCode::NACK;
}

inline bool is_fatal_error(ErrorCode code) {
    return code == ErrorCode::COHERENCE_DEADLOCK ||
           code == ErrorCode::COHERENCE_LIVELOCK ||
           code == ErrorCode::COHERENCE_DATA_INCONSISTENCY;
}

#endif // ERROR_CATEGORY_HH
