# TransactionContext 设计规范

**版本**: v1.0 | **日期**: 2026-04-07  
**来源**: 多层次混合仿真提案 §3

---

## 1. 概述

`TransactionContext` 是伴随每个交易穿越所有模块边界的上下文对象，用于：
- 端到端交易追踪
- 父子交易关联
- 跨交易干扰记录
- 性能分析置信度评分

---

## 2. 数据结构

```cpp
namespace gemsc {

enum class TransactionType : uint8_t {
    READ = 0,
    WRITE = 1,
    ATOMIC = 2,
    STREAM = 3,
    CREDIT = 4
};

enum class PropagationRule : uint8_t {
    PASSTHROUGH = 0,    // 透传型：原样转发
    TRANSFORM = 1,      // 转换型：创建子交易
    TERMINATE = 2       // 终止型：记录完成
};

struct TraceEntry {
    uint64_t timestamp;         // 全局虚拟时间戳
    std::string module_name;    // 模块名称
    uint64_t latency_cycles;    // 在该模块的延迟（周期）
    enum Event {
        ARRIVAL,                // 到达
        QUEUE_WAIT,             // 队列等待
        PROCESSING,             // 处理中
        CONTENTION,             // 资源竞争
        DEPARTURE               // 离开
    } event;
    std::string details;        // 可选详细信息
};

struct TransactionContext {
    // === 身份标识 ===
    uint64_t transaction_id;        // 唯一交易 ID
    uint64_t parent_id;             // 父交易 ID（0 表示无父交易）
    uint8_t  fragment_id;           // 片段序号（0 表示完整交易）
    uint8_t  fragment_total;        // 总片段数（1 表示完整交易）
    
    // === 时间戳 ===
    uint64_t create_timestamp;      // 创建时间（全局虚拟时间）
    uint64_t complete_timestamp;    // 完成时间（终止时填充）
    
    // === 来源信息 ===
    std::string source_port;        // 来源模块/端口标识
    std::string dest_port;          // 目标模块/端口标识（可选）
    
    // === 交易属性 ===
    TransactionType type;           // 交易类型
    uint8_t priority;               // QoS 优先级 (0-7)
    uint64_t flow_id;               // 流 ID（用于 QoS）
    uint64_t address;               // 交易地址
    uint64_t length;                // 数据长度（字节）
    
    // === 传播规则 ===
    PropagationRule propagation;    // 该交易的传播规则
    
    // === 追踪日志 ===
    std::vector<TraceEntry> trace_log;  // 每跳追踪记录
    
    // === 跨交易干扰 ===
    struct ContentionRecord {
        uint64_t timestamp;
        uint64_t contending_tid;    // 竞争的交易 ID
        uint64_t wait_cycles;       // 等待周期数
        enum ContentionType {
            QUEUE_FULL,             // 队列满
            BANK_BUSY,              // Bank 忙
            ARBITRATION_LOSS,       // 仲裁失败
            BANDWIDTH_SATURATED     // 带宽饱和
        } type;
    };
    std::vector<ContentionRecord> contention_log;
    
    // === 置信度评分 ===
    enum ConfidenceLevel {
        HIGH = 0,       // 基于校准的 RTL 数据或历史测量
        MEDIUM = 1,     // 基于理论计算
        LOW = 2         // 基于默认估算或手动覆盖
    } confidence = MEDIUM;
    
    // === 工具函数 ===
    uint64_t get_end_to_end_latency() const {
        return complete_timestamp - create_timestamp;
    }
    
    void add_trace(const std::string& module, uint64_t latency, 
                   TraceEntry::Event event, const std::string& details = "") {
        trace_log.push_back({
            .timestamp = create_timestamp + latency,  // 简化处理
            .module_name = module,
            .latency_cycles = latency,
            .event = event,
            .details = details
        });
    }
    
    void record_contention(uint64_t contending_tid, uint64_t wait_cycles,
                           ContentionRecord::ContentionType type) {
        contention_log.push_back({
            .timestamp = create_timestamp,  // 简化处理
            .contending_tid = contending_tid,
            .wait_cycles = wait_cycles,
            .type = type
        });
    }
};

} // namespace gemsc
```

---

## 3. 传播规则详解

### 3.1 透传型（Passthrough）

**典型模块**: Crossbar、简单互连、总线

**行为**:
```cpp
void handle_request(Packet* pkt) {
    // 原样转发 TransactionContext
    forward(pkt);
    
    // 仅追加路由延迟到 trace_log
    pkt->context->add_trace(name, routing_delay, 
                            TraceEntry::Event::DEPARTURE);
}
```

**示例**:
```
CPU → Crossbar → Memory
     (透传型)

TransactionContext:
  TID=0x0001, source=cpu_0
  trace_log: [
    {module: "cpu", event: DEPARTURE, latency: 0},
    {module: "crossbar", event: DEPARTURE, latency: 2},  // 路由延迟 2 周期
    {module: "memory", event: ARRIVAL, latency: 2}
  ]
```

---

### 3.2 转换型（Transformer）

**典型模块**: Cache、地址转换单元、协议转换器

**行为**:
```cpp
void handle_request(Packet* pkt) {
    if (cache_hit) {
        // 命中：原交易完成
        pkt->context->add_trace(name, hit_latency, 
                                TraceEntry::Event::PROCESSING, "HIT");
        send_response(pkt);
    } else {
        // 未命中：创建子交易
        Packet* sub_pkt = create_sub_request(pkt->addr);
        sub_pkt->context = TransactionContext{
            .transaction_id = allocate_new_tid(),
            .parent_id = pkt->context->transaction_id,
            .fragment_id = 0,
            .fragment_total = 1,
            .create_timestamp = current_cycle,
            .source_port = name,
            .type = pkt->context->type,
            .priority = pkt->context->priority
        };
        forward_to_lower_level(sub_pkt);
    }
}
```

**示例**:
```
CPU → Cache (未命中) → L2 → Memory
     (转换型)

父交易 TID=0x0001:
  trace_log: [
    {module: "cpu", event: DEPARTURE},
    {module: "cache", event: PROCESSING, details: "MISS"}
  ]

子交易 TID=0x0002 (parent_id=0x0001):
  trace_log: [
    {module: "cache", event: DEPARTURE},
    {module: "l2", event: ARRIVAL},
    ...
  ]
```

---

### 3.3 终止型（Terminator）

**典型模块**: PhysicalMemory、终端外设

**行为**:
```cpp
void handle_request(Packet* pkt) {
    // 处理请求
    uint64_t access_latency = calculate_latency(pkt->addr);
    
    // 标记交易完成
    pkt->context->complete_timestamp = current_cycle + access_latency;
    pkt->context->add_trace(name, access_latency, 
                            TraceEntry::Event::PROCESSING, "MEM_ACCESS");
    
    // 生成响应
    send_response(pkt);
}
```

**示例**:
```
CPU → ... → Memory (终止型)

TransactionContext:
  TID=0x0001
  create_timestamp = 0
  complete_timestamp = 150  // 端到端延迟 150 周期
  trace_log: [
    {module: "cpu", event: DEPARTURE, latency: 0},
    {module: "crossbar", event: DEPARTURE, latency: 2},
    {module: "memory", event: PROCESSING, latency: 150, details: "MEM_ACCESS"}
  ]
  end_to_end_latency = 150 周期
```

---

## 4. 跨交易干扰记录

### 4.1 竞争场景

当多个交易在模块内部竞争资源时：

```cpp
void enqueue_request(Packet* pkt) {
    if (queue.full()) {
        // 队列满，记录竞争
        uint64_t head_tid = queue.front()->context->transaction_id;
        pkt->context->record_contention(
            head_tid,
            estimated_wait_cycles,
            ContentionRecord::ContentionType::QUEUE_FULL
        );
        drop_or_backpressure(pkt);
    }
}
```

### 4.2 分析用途

仿真后可分析：
- 哪些交易经历了竞争？
- 竞争导致的平均等待时间？
- 竞争热点模块是哪些？

---

## 5. 置信度评分

### 5.1 评分标准

| 置信度 | 来源 | 示例 |
|--------|------|------|
| **HIGH** | 校准的 RTL 数据或历史测量 | Cache 命中延迟来自 RTL 仿真 |
| **MEDIUM** | 理论计算或已验证假设 | 内存延迟 = 距离/传播速度 |
| **LOW** | 默认估算或手动覆盖 | 使用文档默认值 |

### 5.2 传播规则

```cpp
// 子交易继承父交易的置信度（或更低）
sub_ctx.confidence = std::max(parent_ctx.confidence, 
                               current_module.confidence);
```

### 5.3 报告用途

性能分析报告为每个指标附带置信度：
```
平均内存延迟：150 周期 [中置信度]
  - 基于理论计算：SRAM 访问时间 + 译码器延迟
  - 建议：RTL 仿真后重新校准
```

---

## 6. 与 Packet 集成

### 6.1 集成方案

**方案 A: 成员变量**（推荐）
```cpp
class Packet {
public:
    TransactionContext context;  // 直接嵌入
    // ... 其他字段
};
```

**方案 B: 指针引用**
```cpp
class Packet {
public:
    TransactionContext* context;  // 指针，需管理生命周期
    // ... 其他字段
};
```

**推荐方案 A**:
- ✅ 内存连续，缓存友好
- ✅ 无需额外生命周期管理
- ⚠️ Packet 体积增大（约 200 字节）

---

### 6.2 Packet 修改

```cpp
// include/core/packet.hh
class Packet {
    // ... 现有字段 ...
    
    // 新增：交易上下文
    gemsc::TransactionContext context;
    
    // 工具函数
    void set_transaction_id(uint64_t tid) {
        context.transaction_id = tid;
    }
    uint64_t get_transaction_id() const {
        return context.transaction_id;
    }
};
```

---

## 7. 单元测试要点

```cpp
TEST_CASE("TransactionContext Propagation") {
    // 测试 1: 透传型传播
    GIVEN("Passthrough module") {
        WHEN("Packet passes through") {
            THEN("Context unchanged except trace_log") {
                // ...
            }
        }
    }
    
    // 测试 2: 转换型创建子交易
    GIVEN("Transformer module with cache miss") {
        WHEN("Sub-transaction created") {
            THEN("parent_id links to original") {
                // ...
            }
        }
    }
    
    // 测试 3: 终止型记录完成时间
    GIVEN("Terminator module") {
        WHEN("Transaction completes") {
            THEN("complete_timestamp set") {
                // ...
            }
        }
    }
    
    // 测试 4: 跨交易干扰记录
    GIVEN("Two transactions competing for queue") {
        WHEN("Queue is full") {
            THEN("Contention recorded in trace_log") {
                // ...
            }
        }
    }
}
```

---

## 8. 验收标准

| 标准 | 验证方法 |
|------|---------|
| **100+ 交易完整传播** | 端到端测试，验证 trace_log 完整性 |
| **父子交易关联正确** | 查询 parent_id 验证关联关系 |
| **干扰事件正确记录** | 注入竞争场景，验证 contention_log |
| **置信度评分合理** | 审查评分逻辑，验证传播规则 |
| **性能开销 < 5%** | 对比有/无 Context 的仿真速度 |

---

## 9. 参考

- 提案文档：`/workspace/mynotes/CppTLM/docs/proposal/多层次混合仿真.md` §3
- 历史设计：`docs/05-LEGACY/v2-improvements/DESIGN_REVIEW_AND_ADJUSTMENTS.md`

---

**状态**: 📋 设计中 | **下一步**: 架构讨论确认后实现
