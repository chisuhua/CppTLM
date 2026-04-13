# CppTLM 错误与调试架构 v1.0

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **关联**: 架构 v2.0, 交易处理架构 v1.0

---

## 1. 架构愿景

**核心目标**: 基于现有 TLM Extension 机制，构建统一的错误处理与调试系统，支持错误分类、追踪、回放，与交易处理架构无缝整合。

**设计原则**:
1. **Extension 优先**: 复用现有 `tlm_extension` 机制，向后兼容
2. **统一追踪**: 错误与交易共享同一追踪框架
3. **可扩展**: 错误类别可动态扩展
4. **可视化友好**: 数据结构支持 GUI 展示

---

## 2. 架构分层

```
┌─────────────────────────────────────────────────────────────┐
│  应用层：模块错误检测                                        │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Cache/Crossbar/Memory 检测错误                      │   │
│  │  - 设置 error_code                                   │   │
│  │  - 附加 ErrorContextExt                              │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Extension 层：错误上下文                                    │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  ErrorContextExt (继承 tlm_extension)                │   │
│  │  - error_code, error_category                        │   │
│  │  - stack_trace, context_data                         │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  追踪层：统一错误/交易追踪                                   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  DebugTracker (单例)                                 │   │
│  │  - 错误记录（按 transaction_id 索引）                │   │
│  │  - 状态快照（按时间索引）                            │   │
│  │  - 查询接口（按错误类型/模块/时间）                  │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  展示层：错误可视化（未来 GUI）                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  - 错误时间线                                        │   │
│  │  - 状态转换图                                        │   │
│  │  - 依赖关系图                                        │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 错误分类体系

### 3.1 错误类别枚举

```cpp
// include/framework/error_category.hh
#ifndef ERROR_CATEGORY_HH
#define ERROR_CATEGORY_HH

#include <cstdint>
#include <string>

// ========== 错误类别（可扩展） ==========
enum class ErrorCategory : uint8_t {
    TRANSPORT       = 0x01,  // 传输层错误
    RESOURCE        = 0x02,  // 资源层错误
    COHERENCE       = 0x03,  // 一致性层错误
    PROTOCOL        = 0x04,  // 协议层错误
    SECURITY        = 0x05,  // 安全层错误
    PERFORMANCE     = 0x06,  // 性能层错误
    // 0x10-0xFF: 用户自定义类别
    CUSTOM          = 0x10
};

// ========== 错误码（按类别组织） ==========
enum class ErrorCode : uint16_t {
    // ========== 成功 ==========
    SUCCESS                 = 0x0000,
    RETRY                   = 0x0001,
    NACK                    = 0x0002,
    
    // ========== 传输层错误 (0x01xx) ==========
    TRANSPORT_INVALID_ADDRESS   = 0x0100,
    TRANSPORT_ACCESS_DENIED     = 0x0101,
    TRANSPORT_ALIGNMENT_FAULT   = 0x0102,
    TRANSPORT_TIMEOUT           = 0x0103,
    TRANSPORT_PROTOCOL_VIOLATION = 0x0104,
    
    // ========== 资源层错误 (0x02xx) ==========
    RESOURCE_BUFFER_FULL        = 0x0200,
    RESOURCE_QUEUE_OVERFLOW     = 0x0201,
    RESOURCE_CONFLICT           = 0x0202,
    RESOURCE_STARVATION         = 0x0203,
    
    // ========== 一致性层错误 (0x03xx) ==========
    COHERENCE_STATE_VIOLATION   = 0x0300,
    COHERENCE_DEADLOCK          = 0x0301,
    COHERENCE_LIVELOCK          = 0x0302,
    COHERENCE_DATA_INCONSISTENCY = 0x0303,
    COHERENCE_INVALID_TRANSITION = 0x0304,
    COHERENCE_SNOOP_CONFLICT    = 0x0305,
    
    // ========== 协议层错误 (0x04xx) ==========
    PROTOCOL_ID_CONFLICT        = 0x0400,
    PROTOCOL_OUT_OF_ORDER       = 0x0401,
    PROTOCOL_SEQUENCE_ERROR     = 0x0402,
    
    // ========== 安全层错误 (0x05xx) ==========
    SECURITY_PERMISSION_DENIED  = 0x0500,
    SECURITY_ENCRYPTION_ERROR   = 0x0501,
    
    // ========== 性能层错误 (0x06xx) ==========
    PERFORMANCE_THRESHOLD_EXCEEDED = 0x0600,
    PERFORMANCE_HOTSPOT_DETECTED   = 0x0601,
};

// ========== 工具函数 ==========
inline ErrorCategory get_error_category(ErrorCode code) {
    uint16_t v = static_cast<uint16_t>(code);
    if (v == 0x0000) return ErrorCategory::TRANSPORT;  // SUCCESS
    return static_cast<ErrorCategory>((v >> 8) & 0xFF);
}

inline std::string error_category_to_string(ErrorCategory cat) {
    switch (cat) {
        case ErrorCategory::TRANSPORT: return "TRANSPORT";
        case ErrorCategory::RESOURCE: return "RESOURCE";
        case ErrorCategory::COHERENCE: return "COHERENCE";
        case ErrorCategory::PROTOCOL: return "PROTOCOL";
        case ErrorCategory::SECURITY: return "SECURITY";
        case ErrorCategory::PERFORMANCE: return "PERFORMANCE";
        default: return "UNKNOWN";
    }
}

inline std::string error_code_to_string(ErrorCode code) {
    uint16_t v = static_cast<uint16_t>(code);
    return error_category_to_string(get_error_category(code)) + 
           "_" + std::to_string(v & 0xFF);
}

inline bool is_error(ErrorCode code) {
    return code != ErrorCode::SUCCESS;
}

inline bool is_fatal_error(ErrorCode code) {
    // 致命错误：需要捕获快照
    return code == ErrorCode::COHERENCE_DEADLOCK ||
           code == ErrorCode::COHERENCE_DATA_INCONSISTENCY ||
           code == ErrorCode::COHERENCE_LIVELOCK;
}

#endif // ERROR_CATEGORY_HH
```

---

### 3.2 一致性状态枚举

```cpp
// include/framework/coherence_state.hh
#ifndef COHERENCE_STATE_HH
#define COHERENCE_STATE_HH

#include <cstdint>
#include <string>
#include <vector>

// ========== 缓存行一致性状态（MOESI） ==========
enum class CoherenceState : uint8_t {
    INVALID     = 0x00,
    SHARED      = 0x01,
    EXCLUSIVE   = 0x02,
    MODIFIED    = 0x03,
    OWNED       = 0x04,
    TRANSIENT   = 0x10,  // 过渡状态
};

inline std::string coherence_state_to_string(CoherenceState state) {
    switch (state) {
        case CoherenceState::INVALID: return "I";
        case CoherenceState::SHARED: return "S";
        case CoherenceState::EXCLUSIVE: return "E";
        case CoherenceState::MODIFIED: return "M";
        case CoherenceState::OWNED: return "O";
        case CoherenceState::TRANSIENT: return "T";
        default: return "?";
    }
}

// ========== 状态转换记录 ==========
struct StateTransition {
    uint64_t timestamp;
    uint64_t address;
    CoherenceState from_state;
    CoherenceState to_state;
    std::string trigger_event;  // "LOAD", "STORE", "SNOOP_READ", "SNOOP_WRITE"
    uint32_t core_id;
    uint64_t transaction_id;
    
    std::string to_string() const {
        return coherence_state_to_string(from_state) + "→" +
               coherence_state_to_string(to_state) +
               " [" + trigger_event + " @T" + std::to_string(timestamp) + "]";
    }
};

// ========== 一致性违例记录 ==========
struct CoherenceViolation {
    uint64_t timestamp;
    uint64_t transaction_id;
    uint64_t address;
    CoherenceState expected_state;
    CoherenceState actual_state;
    std::string violation_type;
    uint32_t core_id;
    std::vector<uint32_t> other_sharers;
    std::string description;
    
    std::string to_string() const {
        return "VIOLATION: " + violation_type + 
               " @ addr=0x" + format_hex(address) +
               " expected=" + coherence_state_to_string(expected_state) +
               " actual=" + coherence_state_to_string(actual_state);
    }
};

#endif // COHERENCE_STATE_HH
```

---

## 4. Extension 设计

### 4.1 ErrorContextExt

```cpp
// include/ext/error_context_ext.hh
#ifndef ERROR_CONTEXT_EXT_HH
#define ERROR_CONTEXT_EXT_HH

#include "tlm.h"
#include "../framework/error_category.hh"
#include "../framework/coherence_state.hh"
#include <vector>
#include <map>
#include <string>

// ========== 错误上下文扩展 ==========
struct ErrorContextExt : public tlm::tlm_extension<ErrorContextExt> {
    // ========== 核心字段 ==========
    ErrorCode error_code;
    ErrorCategory error_category;
    std::string error_message;
    std::string source_module;
    uint64_t timestamp;
    
    // ========== 堆栈追踪 ==========
    struct StackFrame {
        std::string module;
        std::string function;
        uint64_t timestamp;
        std::string context;
    };
    std::vector<StackFrame> stack_trace;
    
    // ========== 上下文数据（键值对） ==========
    std::map<std::string, uint64_t> context_data;
    
    // ========== 一致性特定字段 ==========
    CoherenceState expected_state;
    CoherenceState actual_state;
    std::vector<uint32_t> sharers;
    
    // ========== TLM Extension 必需方法 ==========
    tlm_extension* clone() const override {
        return new ErrorContextExt(*this);
    }
    
    void copy_from(tlm_extension const& e) override {
        auto& ext = static_cast<const ErrorContextExt&>(e);
        error_code = ext.error_code;
        error_category = ext.error_category;
        error_message = ext.error_message;
        source_module = ext.source_module;
        timestamp = ext.timestamp;
        stack_trace = ext.stack_trace;
        context_data = ext.context_data;
        expected_state = ext.expected_state;
        actual_state = ext.actual_state;
        sharers = ext.sharers;
    }
    
    // ========== 辅助方法 ==========
    void add_stack_frame(const std::string& module, 
                         const std::string& function,
                         const std::string& context = "") {
        stack_trace.push_back({
            module, function, 
            TransactionTracker::instance().get_current_time(),
            context
        });
    }
    
    void set_context(const std::string& key, uint64_t value) {
        context_data[key] = value;
    }
    
    uint64_t get_context(const std::string& key, uint64_t default_val = 0) const {
        auto it = context_data.find(key);
        return (it != context_data.end()) ? it->second : default_val;
    }
    
    std::string to_string() const {
        return error_category_to_string(error_category) + ": " + 
               error_code_to_string(error_code) + " - " + error_message;
    }
};

// ========== 便捷函数 ==========
inline ErrorContextExt* get_error_context(tlm_generic_payload* p) {
    ErrorContextExt* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

inline void set_error_context(tlm_generic_payload* p, const ErrorContextExt& src) {
    ErrorContextExt* ext = new ErrorContextExt(src);
    p->set_extension(ext);
}

inline void create_error_context(tlm_generic_payload* p,
                                  ErrorCode code,
                                  const std::string& message,
                                  const std::string& module) {
    ErrorContextExt* ext = new ErrorContextExt();
    ext->error_code = code;
    ext->error_category = get_error_category(code);
    ext->error_message = message;
    ext->source_module = module;
    ext->timestamp = TransactionTracker::instance().get_current_time();
    ext->add_stack_frame(module, "create_error");
    p->set_extension(ext);
}

#endif // ERROR_CONTEXT_EXT_HH
```

---

### 4.2 DebugTraceExt

```cpp
// include/ext/debug_trace_ext.hh
#ifndef DEBUG_TRACE_EXT_HH
#define DEBUG_TRACE_EXT_HH

#include "tlm.h"
#include "../framework/coherence_state.hh"
#include <vector>

// ========== 调试追踪扩展（与 TransactionContextExt 互补） ==========
struct DebugTraceExt : public tlm::tlm_extension<DebugTraceExt> {
    // ========== 追踪事件 ==========
    struct TraceEvent {
        uint64_t timestamp;
        std::string event_type;  // "ERROR", "STATE_CHANGE", "DEADLOCK_CHECK"
        std::string description;
        uint64_t data;
    };
    
    std::vector<TraceEvent> events;
    
    // ========== TLM Extension 必需方法 ==========
    tlm_extension* clone() const override {
        return new DebugTraceExt(*this);
    }
    
    void copy_from(tlm_extension const& e) override {
        auto& ext = static_cast<const DebugTraceExt&>(e);
        events = ext.events;
    }
    
    // ========== 辅助方法 ==========
    void add_event(const std::string& type, 
                   const std::string& desc,
                   uint64_t data = 0) {
        events.push_back({
            TransactionTracker::instance().get_current_time(),
            type, desc, data
        });
    }
    
    void add_error_event(ErrorCode code, const std::string& desc) {
        add_event("ERROR", error_code_to_string(code) + ": " + desc);
    }
    
    void add_state_event(CoherenceState from, CoherenceState to, 
                         const std::string& trigger) {
        add_event("STATE_CHANGE", 
                  coherence_state_to_string(from) + "→" + 
                  coherence_state_to_string(to) + 
                  " [" + trigger + "]");
    }
};

#endif // DEBUG_TRACE_EXT_HH
```

---

## 5. 统一追踪器设计

### 5.1 DebugTracker（单例）

```cpp
// include/framework/debug_tracker.hh
#ifndef DEBUG_TRACKER_HH
#define DEBUG_TRACKER_HH

#include "../ext/error_context_ext.hh"
#include "../ext/debug_trace_ext.hh"
#include "../ext/transaction_context_ext.hh"
#include <map>
#include <vector>
#include <functional>

// ========== 错误记录（用于查询和展示） ==========
struct ErrorRecord {
    uint64_t error_id;
    uint64_t timestamp;
    uint64_t transaction_id;
    ErrorCode error_code;
    ErrorCategory error_category;
    std::string source_module;
    std::string error_message;
    std::vector<std::string> stack_trace;
    std::map<std::string, uint64_t> context_data;
    
    // 一致性特定
    CoherenceState expected_state;
    CoherenceState actual_state;
    std::vector<uint32_t> sharers;
    
    // 关联的错误 ID（因果链）
    std::vector<uint64_t> related_errors;
    
    std::string to_string() const {
        return "[T" + std::to_string(timestamp) + "] " +
               error_category_to_string(error_category) + ": " +
               error_code_to_string(error_code) + " - " +
               error_message + " (Module=" + source_module + ")";
    }
};

// ========== 状态快照（用于回放） ==========
struct StateSnapshot {
    uint64_t timestamp;
    uint64_t address;
    CoherenceState state;
    std::vector<uint32_t> sharers;
    std::string trigger_event;
    uint64_t transaction_id;
};

// ========== 调试追踪器（框架层单例） ==========
class DebugTracker {
private:
    std::map<uint64_t, ErrorRecord> errors_;  // error_id -> record
    std::map<uint64_t, std::vector<StateSnapshot>> state_history_;  // address -> history
    std::map<uint64_t, std::vector<uint64_t>> transaction_errors_;  // transaction_id -> error_ids
    
    uint64_t next_error_id_ = 1;
    
    // 配置
    bool enable_error_tracking_ = true;
    bool enable_state_history_ = true;
    bool stop_on_fatal_ = false;
    
    DebugTracker() = default;
    
public:
    static DebugTracker& instance() {
        static DebugTracker tracker;
        return tracker;
    }
    
    // ========== 初始化 ==========
    void initialize(bool enable_error_tracking,
                    bool enable_state_history,
                    bool stop_on_fatal) {
        enable_error_tracking_ = enable_error_tracking;
        enable_state_history_ = enable_state_history;
        stop_on_fatal_ = stop_on_fatal;
    }
    
    // ========== 记录错误 ==========
    uint64_t record_error(tlm_generic_payload* payload,
                          ErrorCode code,
                          const std::string& message,
                          const std::string& module) {
        if (!enable_error_tracking_) return 0;
        
        ErrorRecord record;
        record.error_id = next_error_id_++;
        record.timestamp = TransactionTracker::instance().get_current_time();
        record.error_code = code;
        record.error_category = get_error_category(code);
        record.source_module = module;
        record.error_message = message;
        
        // 获取 transaction_id
        if (TransactionContextExt* trans_ext = get_transaction_context(payload)) {
            record.transaction_id = trans_ext->transaction_id;
            
            // 关联到交易
            transaction_errors_[record.transaction_id].push_back(record.error_id);
        }
        
        // 获取错误上下文
        if (ErrorContextExt* err_ext = get_error_context(payload)) {
            for (const auto& frame : err_ext->stack_trace) {
                record.stack_trace.push_back(
                    frame.module + "::" + frame.function + " - " + frame.context);
            }
            record.context_data = err_ext->context_data;
            record.expected_state = err_ext->expected_state;
            record.actual_state = err_ext->actual_state;
            record.sharers = err_ext->sharers;
        }
        
        errors_[record.error_id] = record;
        
        // 致命错误处理
        if (stop_on_fatal_ && is_fatal_error(code)) {
            DPRINTF(DEBUG, "[FATAL] %s\n", record.to_string().c_str());
            // 可选择停止仿真
        }
        
        return record.error_id;
    }
    
    // ========== 记录状态转换 ==========
    void record_state_transition(uint64_t address,
                                  CoherenceState from,
                                  CoherenceState to,
                                  const std::string& event,
                                  uint64_t tid) {
        if (!enable_state_history_) return;
        
        StateSnapshot snapshot;
        snapshot.timestamp = TransactionTracker::instance().get_current_time();
        snapshot.address = address;
        snapshot.state = to;
        snapshot.trigger_event = event;
        snapshot.transaction_id = tid;
        
        state_history_[address].push_back(snapshot);
    }
    
    // ========== 查询接口 ==========
    
    // 按错误 ID 查询
    const ErrorRecord* get_error(uint64_t error_id) const {
        auto it = errors_.find(error_id);
        return (it != errors_.end()) ? &it->second : nullptr;
    }
    
    // 按交易 ID 查询错误
    std::vector<ErrorRecord> get_errors_by_transaction(uint64_t tid) const {
        std::vector<ErrorRecord> result;
        auto it = transaction_errors_.find(tid);
        if (it != transaction_errors_.end()) {
            for (uint64_t eid : it->second) {
                auto err_it = errors_.find(eid);
                if (err_it != errors_.end()) {
                    result.push_back(err_it->second);
                }
            }
        }
        return result;
    }
    
    // 按错误类别查询
    std::vector<ErrorRecord> get_errors_by_category(ErrorCategory cat) const {
        std::vector<ErrorRecord> result;
        for (const auto& [id, record] : errors_) {
            if (record.error_category == cat) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    // 按模块查询
    std::vector<ErrorRecord> get_errors_by_module(const std::string& module) const {
        std::vector<ErrorRecord> result;
        for (const auto& [id, record] : errors_) {
            if (record.source_module == module) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    // 获取地址的状态历史
    std::vector<StateSnapshot> get_state_history(uint64_t address) const {
        auto it = state_history_.find(address);
        return (it != state_history_.end()) ? it->second : std::vector<StateSnapshot>();
    }
    
    // 获取所有错误
    const std::map<uint64_t, ErrorRecord>& get_all_errors() const {
        return errors_;
    }
    
    // ========== 回放接口 ==========
    
    // 回放交易执行过程
    std::string replay_transaction(uint64_t tid) const {
        std::ostringstream oss;
        
        auto errors_it = transaction_errors_.find(tid);
        if (errors_it == transaction_errors_.end()) {
            return "No errors for transaction " + std::to_string(tid);
        }
        
        oss << "=== Transaction " << tid << " Replay ===\n";
        
        for (uint64_t eid : errors_it->second) {
            auto err_it = errors_.find(eid);
            if (err_it != errors_.end()) {
                oss << err_it->second.to_string() << "\n";
                
                // 堆栈追踪
                for (const auto& frame : err_it->second.stack_trace) {
                    oss << "  @ " << frame << "\n";
                }
            }
        }
        
        return oss.str();
    }
    
    // 回放地址状态变化
    std::string replay_address_history(uint64_t address) const {
        std::ostringstream oss;
        auto history = get_state_history(address);
        
        oss << "=== Address 0x" << std::hex << address << " History ===\n";
        
        for (const auto& snap : history) {
            oss << "[T" << snap.timestamp << "] "
                << snap.trigger_event << ": "
                << coherence_state_to_string(snap.state)
                << " (TID=" << snap.transaction_id << ")\n";
        }
        
        return oss.str();
    }
    
    // ========== 统计接口 ==========
    size_t error_count() const { return errors_.size(); }
    
    size_t error_count(ErrorCategory cat) const {
        return get_errors_by_category(cat).size();
    }
    
    std::map<ErrorCategory, size_t> errors_by_category() const {
        std::map<ErrorCategory, size_t> stats;
        for (const auto& [id, record] : errors_) {
            stats[record.error_category]++;
        }
        return stats;
    }
    
    std::map<std::string, size_t> errors_by_module() const {
        std::map<std::string, size_t> stats;
        for (const auto& [id, record] : errors_) {
            stats[record.source_module]++;
        }
        return stats;
    }
};

#endif // DEBUG_TRACKER_HH
```

---

## 6. 模块层使用示例

### 6.1 Cache 模块（一致性错误检测）

```cpp
// include/modules/cache_coherence_v2.hh
class CacheCoherenceV2 : public TLMModule {
private:
    std::map<uint64_t, CoherenceState> cache_states_;
    std::map<uint64_t, std::set<uint32_t>> sharers_;
    
public:
    void handlePacket(Packet* pkt) override {
        uint64_t addr = pkt->payload->get_address();
        bool is_write = pkt->cmd == CMD_WRITE;
        uint32_t core_id = get_core_id(pkt);
        
        CoherenceState current = cache_states_[addr];
        
        if (is_write) {
            // 写操作：检查状态转换合法性
            if (current == CoherenceState::INVALID) {
                // 非法转换：Invalid → Modified
                pkt->set_error_code(ErrorCode::COHERENCE_INVALID_TRANSITION);
                
                // 创建错误上下文
                if (ErrorContextExt* err_ext = get_error_context(pkt->payload)) {
                    err_ext->expected_state = CoherenceState::EXCLUSIVE;
                    err_ext->actual_state = CoherenceState::MODIFIED;
                    err_ext->add_stack_frame(name_, "handlePacket", 
                        "Invalid→Modified without Exclusive");
                }
                
                // 记录到 DebugTracker
                DebugTracker::instance().record_error(
                    pkt->payload,
                    ErrorCode::COHERENCE_INVALID_TRANSITION,
                    "Invalid→Modified without acquiring Exclusive",
                    name_);
                
                send_error_response(pkt);
                return;
            }
            
            // 合法转换：记录状态历史
            DebugTracker::instance().record_state_transition(
                addr, current, CoherenceState::MODIFIED,
                "WRITE", pkt->get_transaction_id());
            
            cache_states_[addr] = CoherenceState::MODIFIED;
        }
        
        send_response(pkt);
    }
};
```

---

### 6.2 Crossbar 模块（死锁检测）

```cpp
// include/modules/crossbar_v2.hh
class CrossbarV2 : public TLMModule {
private:
    std::map<uint32_t, uint32_t> waiting_for_;  // core_id -> waiting_for
    
public:
    void handlePacket(Packet* pkt) override {
        uint32_t core_id = get_core_id(pkt);
        int dst_port = route_by_dst(pkt->payload->get_address());
        
        // 检查目标端口是否可用
        if (!is_port_available(dst_port)) {
            // 记录等待关系（死锁检测）
            waiting_for_[core_id] = dst_port;
            
            // 检查死锁
            if (has_cycle()) {
                // 检测到死锁
                pkt->set_error_code(ErrorCode::COHERENCE_DEADLOCK);
                
                if (ErrorContextExt* err_ext = get_error_context(pkt->payload)) {
                    err_ext->add_stack_frame(name_, "handlePacket",
                        "Deadlock detected: cycle in wait-for graph");
                    err_ext->set_context("cycle_nodes", encode_cycle_nodes());
                }
                
                DebugTracker::instance().record_error(
                    pkt->payload,
                    ErrorCode::COHERENCE_DEADLOCK,
                    "Deadlock detected in crossbar",
                    name_);
                
                send_error_response(pkt);
                return;
            }
            
            // 缓冲等待
            buffer_packet(pkt);
            return;
        }
        
        // 清除等待关系
        waiting_for_.erase(core_id);
        forward_to_port(dst_port, pkt);
    }
    
private:
    bool has_cycle() {
        // DFS 检测等待图循环
        // ...
        return false;
    }
};
```

---

## 7. 使用示例

### 7.1 初始化

```cpp
// main.cpp
int sc_main() {
    // 初始化 DebugTracker
    DebugTracker::instance().initialize(
        true,   // enable_error_tracking
        true,   // enable_state_history
        true    // stop_on_fatal
    );
    
    // 创建模块
    auto* cpu0 = new CPUSim("cpu_0", eq);
    auto* cache0 = new CacheCoherenceV2("l1_0", eq);
    auto* crossbar = new CrossbarV2("crossbar", eq);
    auto* memory = new MemoryV2("memory", eq);
    
    // 运行仿真
    eq.run(100000);
    
    // 查询错误统计
    auto stats = DebugTracker::instance().errors_by_category();
    DPRINTF(DEBUG, "Error Statistics:\n");
    for (const auto& [cat, count] : stats) {
        DPRINTF(DEBUG, "  %s: %lu\n", 
                error_category_to_string(cat).c_str(), count);
    }
    
    // 回放特定交易的错误
    std::string replay = DebugTracker::instance().replay_transaction(100);
    DPRINTF(DEBUG, "%s\n", replay.c_str());
    
    // 回放地址状态历史
    std::string history = DebugTracker::instance().replay_address_history(0x1000);
    DPRINTF(DEBUG, "%s\n", history.c_str());
    
    return 0;
}
```

---

### 7.2 查询接口

```cpp
// 查询所有一致性错误
auto coherence_errors = DebugTracker::instance().get_errors_by_category(
    ErrorCategory::COHERENCE);

for (const auto& err : coherence_errors) {
    DPRINTF(DEBUG, "%s\n", err.to_string().c_str());
}

// 查询特定模块的错误
auto cache_errors = DebugTracker::instance().get_errors_by_module("l1_0");

// 查询特定交易的错误
auto trans_errors = DebugTracker::instance().get_errors_by_transaction(100);

// 获取地址状态历史
auto history = DebugTracker::instance().get_state_history(0x1000);
for (const auto& snap : history) {
    DPRINTF(DEBUG, "[T%lu] %s: %s\n", 
            snap.timestamp, snap.trigger_event.c_str(),
            coherence_state_to_string(snap.state).c_str());
}
```

---

## 8. 与交易处理架构整合

| 层次 | 错误处理职责 | 交易处理整合 |
|------|-------------|-------------|
| **模块层** | 检测错误，设置 `error_code` | 通过 `TransactionInfo` 返回 action |
| **Extension 层** | `ErrorContextExt` + `DebugTraceExt` | `TransactionContextExt` 共享 |
| **追踪层** | `DebugTracker` 统一记录 | `TransactionTracker` 共享时间线 |
| **查询层** | 按错误/交易/地址查询 | 按交易 ID 关联错误 |

---

## 9. 架构优势

| 特性 | 传统方案 | 本方案 |
|------|---------|--------|
| **错误扩展** | 固定错误码 | 可扩展类别 + 用户自定义 |
| **错误追踪** | 日志文件 | DebugTracker 内存索引 |
| **回放支持** | 无 | `replay_transaction()` / `replay_address_history()` |
| **一致性调试** | 手动分析 | 自动状态追踪 + 违例捕获 |
| **GUI 友好** | 文本日志 | 结构化数据（未来 GUI 展示） |

---

## 10. 需要确认的问题

| 问题 | 选项 | 推荐 |
|------|------|------|
| **Q1**: 错误分类？ | A) 扁平 / B) 分层（传输/资源/一致性/...） | **B) 分层** |
| **Q2**: Extension 设计？ | A) 单一 / B) 多 Extension（ErrorContext + DebugTrace） | **B) 多 Extension** |
| **Q3**: 追踪方式？ | A) CSV 导出 / B) 内存索引 + 查询接口 | **B) 内存索引** |
| **Q4**: 回放支持？ | A) 无 / B) replay_transaction/replay_address | **B) 支持** |
| **Q5**: 与交易整合？ | A) 独立 / B) 共享追踪框架 | **B) 共享框架** |

---

## 11. 相关文档

| 文档 | 位置 |
|------|------|
| 交易处理架构 | `02-architecture/02-transaction-architecture.md` |
| 错误处理（旧版） | `03-adr/ADR-X.2-error-handling.md` |
| ADR-X 汇总 | `03-adr/ADR-X-SUMMARY.md` |

---

**版本**: v1.0  
**创建日期**: 2026-04-09  
**状态**: 📋 待确认
