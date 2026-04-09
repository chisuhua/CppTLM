# ADR-X.2: 错误处理策略（最终确认版）

> **版本**: 4.0 ✅  
> **日期**: 2026-04-09  
> **状态**: ✅ 已确认  
> **影响**: v2.0 - 错误与调试系统架构

---

## 1. 决策汇总

| 问题 | 决策 | 状态 |
|------|------|------|
| **Q1**: 错误分类？ | **分层（传输/资源/一致性/协议/安全/性能）** | ✅ 已确认 |
| **Q2**: Extension 设计？ | **多 Extension（ErrorContext + DebugTrace）** | ✅ 已确认 |
| **Q3**: 追踪方式？ | **内存索引 + 查询接口（非 CSV）** | ✅ 已确认 |
| **Q4**: 回放支持？ | **replay_transaction / replay_address_history** | ✅ 已确认 |
| **Q5**: 与交易整合？ | **共享 DebugTracker 框架** | ✅ 已确认 |

---

## 2. 核心架构

```
┌─────────────────────────────────────────────────────────────┐
│  应用层：模块错误检测                                        │
│  - 检测错误，设置 error_code                                 │
│  - 附加 ErrorContextExt                                     │
├─────────────────────────────────────────────────────────────┤
│  Extension 层                                                │
│  - ErrorContextExt (错误上下文)                              │
│  - DebugTraceExt (调试追踪)                                  │
│  - TransactionContextExt (交易上下文)                        │
├─────────────────────────────────────────────────────────────┤
│  追踪层：DebugTracker (单例)                                 │
│  - 错误记录（按 transaction_id 索引）                        │
│  - 状态快照（按地址索引）                                    │
│  - 查询/回放接口                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 错误分类体系

### 3.1 错误类别

```cpp
enum class ErrorCategory : uint8_t {
    TRANSPORT    = 0x01,  // 传输层错误
    RESOURCE     = 0x02,  // 资源层错误
    COHERENCE    = 0x03,  // 一致性层错误 ⭐
    PROTOCOL     = 0x04,  // 协议层错误
    SECURITY     = 0x05,  // 安全层错误
    PERFORMANCE  = 0x06,  // 性能层错误
    CUSTOM       = 0x10,  // 用户自定义
};
```

### 3.2 错误码（按类别组织）

```cpp
enum class ErrorCode : uint16_t {
    // 0x01xx: 传输层
    TRANSPORT_INVALID_ADDRESS = 0x0100,
    TRANSPORT_TIMEOUT = 0x0103,
    
    // 0x02xx: 资源层
    RESOURCE_BUFFER_FULL = 0x0200,
    RESOURCE_STARVATION = 0x0203,
    
    // 0x03xx: 一致性层 ⭐
    COHERENCE_STATE_VIOLATION = 0x0300,
    COHERENCE_DEADLOCK = 0x0301,
    COHERENCE_LIVELOCK = 0x0302,
    COHERENCE_DATA_INCONSISTENCY = 0x0303,
    COHERENCE_INVALID_TRANSITION = 0x0304,
    
    // 0x04xx: 协议层
    PROTOCOL_ID_CONFLICT = 0x0400,
    PROTOCOL_OUT_OF_ORDER = 0x0401,
};
```

---

## 4. Extension 设计

### 4.1 ErrorContextExt

```cpp
struct ErrorContextExt : public tlm::tlm_extension<ErrorContextExt> {
    ErrorCode error_code;
    ErrorCategory error_category;
    std::string error_message;
    std::string source_module;
    uint64_t timestamp;
    
    // 堆栈追踪
    struct StackFrame {
        std::string module;
        std::string function;
        std::string context;
    };
    std::vector<StackFrame> stack_trace;
    
    // 上下文数据
    std::map<std::string, uint64_t> context_data;
    
    // 一致性特定字段
    CoherenceState expected_state;
    CoherenceState actual_state;
    std::vector<uint32_t> sharers;
};
```

### 4.2 DebugTraceExt

```cpp
struct DebugTraceExt : public tlm::tlm_extension<DebugTraceExt> {
    struct TraceEvent {
        uint64_t timestamp;
        std::string event_type;  // "ERROR", "STATE_CHANGE"
        std::string description;
        uint64_t data;
    };
    std::vector<TraceEvent> events;
};
```

---

## 5. DebugTracker 接口

### 5.1 记录接口

```cpp
class DebugTracker {
    // 记录错误
    uint64_t record_error(payload, code, message, module);
    
    // 记录状态转换
    void record_state_transition(addr, from, to, event, tid);
};
```

### 5.2 查询接口

```cpp
    // 按错误 ID 查询
    const ErrorRecord* get_error(uint64_t error_id);
    
    // 按交易 ID 查询错误
    std::vector<ErrorRecord> get_errors_by_transaction(uint64_t tid);
    
    // 按错误类别查询
    std::vector<ErrorRecord> get_errors_by_category(ErrorCategory cat);
    
    // 按模块查询
    std::vector<ErrorRecord> get_errors_by_module(const std::string& module);
    
    // 获取地址状态历史
    std::vector<StateSnapshot> get_state_history(uint64_t address);
```

### 5.3 回放接口

```cpp
    // 回放交易执行过程
    std::string replay_transaction(uint64_t tid);
    
    // 回放地址状态变化
    std::string replay_address_history(uint64_t address);
```

---

## 6. 使用示例

```cpp
// 初始化
DebugTracker::instance().initialize(
    true,   // enable_error_tracking
    true,   // enable_state_history
    true    // stop_on_fatal
);

// 模块层记录错误
if (error_condition) {
    pkt->set_error_code(ErrorCode::COHERENCE_STATE_VIOLATION);
    
    if (ErrorContextExt* ext = get_error_context(pkt->payload)) {
        ext->expected_state = CoherenceState::EXCLUSIVE;
        ext->actual_state = CoherenceState::MODIFIED;
        ext->add_stack_frame(name_, "handlePacket");
    }
    
    DebugTracker::instance().record_error(
        pkt->payload, code, message, name_);
}

// 查询/回放
auto errors = DebugTracker::instance().get_errors_by_transaction(100);
std::string replay = DebugTracker::instance().replay_transaction(100);
```

---

## 7. 与交易处理架构整合

| 层次 | 错误处理 | 交易处理 | 整合方式 |
|------|---------|---------|---------|
| **模块层** | 设置 error_code | 传播 transaction_id | 共享 Packet |
| **Extension** | ErrorContextExt | TransactionContextExt | 共享 payload |
| **追踪层** | DebugTracker | TransactionTracker | 共享时间线索引 |
| **查询层** | 按错误/交易查询 | 按交易 ID 查询 | 双向关联 |

---

## 8. 文档位置

| 文档 | 位置 |
|------|------|
| 错误与调试架构 | `02-architecture/03-error-debug-architecture.md` |
| 交易处理架构 | `02-architecture/02-transaction-architecture.md` |
| 架构 v2.0 | `02-architecture/01-hybrid-architecture-v2.md` |

---

**版本**: v4.0 ✅  
**状态**: 已确认  
**最后更新**: 2026-04-09
