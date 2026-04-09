# ADR-X.7: 模块与框架层交易处理职责划分

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **影响**: v2.0 - 模块/框架职责边界

---

## 1. 核心问题

老板提出的问题：

1. **模块如何处理交易上下文**？
   - Crossbar：透传交易
   - Cache：创建子交易
   - Memory：终止交易

2. **框架层如何处理交易**？

3. **多拍（分片）交易如何处理**？

4. **框架层支持粗粒度/细粒度交易 ID 吗**？

5. **模块通过 TlmExtension 设置信息，框架层获取吗**？

---

## 2. 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│  模块层（声明式处理）                                        │
│  ┌────────────────────────────────────────────────────┐     │
│  │  SimObject 基类提供虚方法                            │     │
│  │  - onTransactionStart()  : 交易开始                 │     │
│  │  - onTransactionHop()    : 交易经过                 │     │
│  │  - onTransactionEnd()    : 交易终止                 │     │
│  │  - createSubTransaction(): 创建子交易（可选）       │     │
│  └────────────────────────────────────────────────────┘     │
│  ✅ 模块重写这些方法，声明式处理交易                          │
├─────────────────────────────────────────────────────────────┤
│  框架层（自动追踪）                                          │
│  ┌────────────────────────────────────────────────────┐     │
│  │  TransactionTracker 单例                            │     │
│  │  - 读取 TlmExtension                               │     │
│  │  - 记录 trace_log                                  │     │
│  │  - 处理多拍/分片交易                               │     │
│  │  - 支持粗粒度/细粒度 ID                            │     │
│  └────────────────────────────────────────────────────┘     │
│  ✅ 框架自动调用模块方法 + 追踪                                │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 模块层设计

### 3.1 SimObject 基类扩展

```cpp
// include/core/sim_object.hh
#ifndef SIM_OBJECT_HH
#define SIM_OBJECT_HH

#include "packet.hh"
#include "../ext/transaction_context_ext.hh"

// 交易处理结果
enum class TransactionAction {
    PASSTHROUGH,   // 透传（Crossbar）
    TRANSFORM,     // 转换（Cache 创建子交易）
    TERMINATE,     // 终止（Memory）
    BLOCK          // 阻塞（等待资源）
};

// 交易上下文信息（模块层简化版）
struct TransactionInfo {
    uint64_t transaction_id;
    uint64_t parent_id;
    uint8_t  fragment_id;
    uint8_t  fragment_total;
    TransactionAction action;
};

class SimObject {
protected:
    std::string name_;
    EventQueue* event_queue;
    
public:
    SimObject(const std::string& n, EventQueue* eq) : name_(n), event_queue(eq) {}
    virtual ~SimObject() = default;
    
    // ========== 核心接口（模块必须实现） ==========
    virtual void tick() = 0;
    
    // ========== 交易处理接口（可选重写） ==========
    
    // 交易开始（源模块调用）
    virtual TransactionInfo onTransactionStart(Packet* pkt) {
        TransactionInfo info;
        info.transaction_id = pkt->get_transaction_id();
        info.parent_id = 0;
        info.fragment_id = 0;
        info.fragment_total = 1;
        info.action = TransactionAction::PASSTHROUGH;
        return info;
    }
    
    // 交易经过（中间模块调用）
    virtual TransactionInfo onTransactionHop(Packet* pkt) {
        TransactionInfo info;
        info.transaction_id = pkt->get_transaction_id();
        info.parent_id = 0;
        info.fragment_id = 0;
        info.fragment_total = 1;
        info.action = TransactionAction::PASSTHROUGH;  // 默认透传
        return info;
    }
    
    // 交易终止（目的模块调用）
    virtual TransactionInfo onTransactionEnd(Packet* pkt) {
        TransactionInfo info;
        info.transaction_id = pkt->get_transaction_id();
        info.parent_id = 0;
        info.fragment_id = 0;
        info.fragment_total = 1;
        info.action = TransactionAction::TERMINATE;
        return info;
    }
    
    // 创建子交易（可选：Cache 等转换型模块使用）
    virtual uint64_t createSubTransaction(Packet* parent_pkt, Packet* child_pkt) {
        // 默认：不创建子交易，直接返回父交易 ID
        return parent_pkt->get_transaction_id();
    }
    
    // ========== 便捷方法 ==========
    const std::string& name() const { return name_; }
};

#endif // SIM_OBJECT_HH
```

---

### 3.2 模块示例：Crossbar（透传型）

```cpp
// include/modules/crossbar_v2.hh
#ifndef CROSSBAR_V2_HH
#define CROSSBAR_V2_HH

#include "../core/sim_object.hh"
#include <queue>

class CrossbarV2 : public SimObject {
private:
    std::queue<Packet*> req_buffer;
    
public:
    CrossbarV2(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    
    // ✅ 重写 onTransactionHop：透传交易
    TransactionInfo onTransactionHop(Packet* pkt) override {
        TransactionInfo info;
        info.transaction_id = pkt->get_transaction_id();
        info.parent_id = 0;
        info.fragment_id = 0;
        info.fragment_total = 1;
        info.action = TransactionAction::PASSTHROUGH;  // 透传
        
        // 框架层会读取 action，自动记录 trace_log
        return info;
    }
    
    bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) override {
        // 调用框架层交易处理
        TransactionInfo info = onTransactionHop(pkt);
        
        // 框架层记录追踪
        TransactionTracker::instance().record_hop(
            info.transaction_id, name_, 1, "hopped");
        
        // 路由决策
        int dst_port = route_by_dst(pkt->payload->get_address());
        
        req_buffer.push(pkt);
        scheduleForward(dst_port);
        
        return true;
    }
    
    void tick() override {
        tryForward();
    }
    
private:
    int route_by_dst(uint64_t addr) {
        return (addr >> 24) & 0x3;
    }
    
    bool tryForward() {
        if (req_buffer.empty()) return true;
        Packet* pkt = req_buffer.front();
        // ... 发送逻辑
        return true;
    }
    
    void scheduleForward(int dst_port) {
        event_queue->schedule([this, dst_port]() { /* ... */ }, 1);
    }
};

#endif // CROSSBAR_V2_HH
```

---

### 3.3 模块示例：Cache（转换型）

```cpp
// include/modules/cache_v2.hh
#ifndef CACHE_V2_HH
#define CACHE_V2_HH

#include "../core/sim_object.hh"
#include <map>
#include <queue>

class CacheV2 : public SimObject {
private:
    std::queue<Packet*> req_buffer;
    std::map<uint64_t, uint64_t> cache_lines_;
    uint64_t next_sub_id_ = 1000;  // 子交易 ID 计数器
    
public:
    CacheV2(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    
    // ✅ 重写 onTransactionHop：可能创建子交易
    TransactionInfo onTransactionHop(Packet* pkt) override {
        TransactionInfo info;
        info.transaction_id = pkt->get_transaction_id();
        info.parent_id = 0;
        info.fragment_id = 0;
        info.fragment_total = 1;
        
        uint64_t addr = pkt->payload->get_address();
        bool hit = (addr & 0x7) == 0;
        
        if (hit) {
            // Cache Hit：透传交易
            info.action = TransactionAction::PASSTHROUGH;
        } else {
            // Cache Miss：创建子交易（访问下游）
            info.action = TransactionAction::TRANSFORM;
        }
        
        return info;
    }
    
    // ✅ 重写 createSubTransaction：创建子交易
    uint64_t createSubTransaction(Packet* parent_pkt, Packet* child_pkt) override {
        uint64_t sub_id = next_sub_id_++;
        
        // 设置子交易的 Extension
        if (TransactionContextExt* ext = get_transaction_context(child_pkt->payload)) {
            ext->transaction_id = sub_id;
            ext->parent_id = parent_pkt->get_transaction_id();  // 关联父交易
            ext->fragment_id = 0;
            ext->fragment_total = 1;
        }
        
        child_pkt->set_transaction_id(sub_id);
        
        // 框架层记录父子关系
        TransactionTracker::instance().link_transactions(
            parent_pkt->get_transaction_id(), sub_id);
        
        return sub_id;
    }
    
    bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) override {
        TransactionInfo info = onTransactionHop(pkt);
        
        uint64_t addr = pkt->payload->get_address();
        bool hit = (addr & 0x7) == 0;
        
        if (hit) {
            // Cache Hit：直接响应
            Packet* resp = PacketPool::get().acquire();
            resp->payload = pkt->payload;
            resp->type = PKT_RESP;
            resp->original_req = pkt;
            resp->set_transaction_id(pkt->get_transaction_id());  // 透传 ID
            
            event_queue->schedule([this, resp, src_id]() {
                getPortManager().getUpstreamPorts()[src_id]->sendResp(resp);
            }, 1);
            
            PacketPool::get().release(pkt);
            
            // 框架层记录
            TransactionTracker::instance().record_hop(
                info.transaction_id, name_, 1, "hit");
        } else {
            // Cache Miss：创建子交易，转发到下游
            Packet* child_pkt = PacketPool::get().acquire();
            child_pkt->payload = pkt->payload;
            child_pkt->type = PKT_REQ;
            
            // 创建子交易
            uint64_t sub_id = createSubTransaction(pkt, child_pkt);
            
            req_buffer.push(child_pkt);
            scheduleForward();
            
            // 框架层记录
            TransactionTracker::instance().record_hop(
                info.transaction_id, name_, 1, "miss_sub_transaction");
        }
        
        return true;
    }
    
    void tick() override {
        tryForward();
    }
    
private:
    bool tryForward() {
        if (req_buffer.empty()) return true;
        Packet* pkt = req_buffer.front();
        // ... 发送逻辑
        return true;
    }
    
    void scheduleForward() {
        event_queue->schedule([this]() { tryForward(); }, 1);
    }
};

#endif // CACHE_V2_HH
```

---

### 3.4 模块示例：Memory（终止型）

```cpp
// include/modules/memory_v2.hh
#ifndef MEMORY_V2_HH
#define MEMORY_V2_HH

#include "../core/sim_object.hh"
#include <map>

class MemoryV2 : public SimObject {
private:
    std::map<uint64_t, uint64_t> memory_;
    
public:
    MemoryV2(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    
    // ✅ 重写 onTransactionEnd：终止交易
    TransactionInfo onTransactionEnd(Packet* pkt) override {
        TransactionInfo info;
        info.transaction_id = pkt->get_transaction_id();
        info.parent_id = 0;
        info.fragment_id = 0;
        info.fragment_total = 1;
        info.action = TransactionAction::TERMINATE;  // 终止
        return info;
    }
    
    bool handleDownstreamRequest(Packet* pkt, int src_id, const std::string& src_label) override {
        // 调用交易终止处理
        TransactionInfo info = onTransactionEnd(pkt);
        
        // 执行内存操作
        uint64_t addr = pkt->payload->get_address();
        bool is_write = pkt->cmd == CMD_WRITE;
        
        if (is_write) {
            // 写操作
            memory_[addr] = 0xDEADBEEF;  // 简化
        }
        
        // 创建响应
        Packet* resp = PacketPool::get().acquire();
        resp->payload = pkt->payload;
        resp->type = PKT_RESP;
        resp->original_req = pkt;
        resp->set_transaction_id(pkt->get_transaction_id());  // 透传 ID
        
        // 设置错误码（通过 Extension）
        if (TransactionContextExt* ext = get_transaction_context(resp->payload)) {
            // 可扩展：设置错误码
        }
        
        event_queue->schedule([this, resp, src_id]() {
            getPortManager().getUpstreamPorts()[src_id]->sendResp(resp);
        }, 1);
        
        // 框架层记录交易完成
        TransactionTracker::instance().complete_transaction(info.transaction_id);
        
        PacketPool::get().release(pkt);
        
        return true;
    }
    
    void tick() override {
        // Memory 无缓冲，无需 tick
    }
};

#endif // MEMORY_V2_HH
```

---

## 4. 框架层设计

### 4.1 TransactionTracker（完整设计）

```cpp
// include/framework/transaction_tracker.hh
#ifndef TRANSACTION_TRACKER_HH
#define TRANSACTION_TRACKER_HH

#include "../core/packet.hh"
#include "../ext/transaction_context_ext.hh"
#include <map>
#include <vector>
#include <string>
#include <fstream>

// 交易记录
struct TransactionRecord {
    uint64_t transaction_id;
    uint64_t parent_id;
    uint8_t fragment_id;
    uint8_t fragment_total;
    std::string source_module;
    std::string type;
    uint64_t create_timestamp;
    uint64_t complete_timestamp;
    std::vector<std::pair<std::string, uint64_t>> hop_log;  // (module, latency)
    bool is_complete = false;
};

// 交易追踪器（框架层单例）
class TransactionTracker {
private:
    std::map<uint64_t, TransactionRecord> transactions_;
    std::map<uint64_t, std::vector<uint64_t>> parent_child_map_;  // 父子关系
    uint64_t global_timestamp_ = 0;
    
    // 配置
    bool enable_coarse_grained_ = true;   // 粗粒度追踪
    bool enable_fine_grained_ = true;     // 细粒度追踪（分片）
    bool export_on_complete_ = false;     // 完成时导出
    
    TransactionTracker() = default;
    
    std::ofstream export_file_;
    
public:
    static TransactionTracker& instance() {
        static TransactionTracker tracker;
        return tracker;
    }
    
    // ========== 初始化 ==========
    void initialize(const std::string& export_path = "") {
        if (!export_path.empty()) {
            export_file_.open(export_path, std::ios::app);
            export_on_complete_ = true;
        }
    }
    
    // ========== 交易生命周期 ==========
    
    // 创建交易（源模块调用）
    uint64_t create_transaction(tlm_generic_payload* payload,
                                 const std::string& source,
                                 const std::string& type) {
        uint64_t tid = generate_id();
        
        TransactionRecord record;
        record.transaction_id = tid;
        record.parent_id = 0;
        record.fragment_id = 0;
        record.fragment_total = 1;
        record.source_module = source;
        record.type = type;
        record.create_timestamp = global_timestamp_;
        
        transactions_[tid] = record;
        
        // 同步设置 Extension
        if (TransactionContextExt* ext = get_transaction_context(payload)) {
            ext->transaction_id = tid;
            ext->parent_id = 0;
            ext->fragment_id = 0;
            ext->fragment_total = 1;
            ext->source_module = source;
            ext->type = type;
        }
        
        return tid;
    }
    
    // 记录经过（中间模块调用）
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency, const std::string& event) {
        if (transactions_.count(tid)) {
            auto& record = transactions_[tid];
            record.hop_log.push_back({module, latency});
            
            // 同步 Extension
            if (TransactionContextExt* ext = get_transaction_context(get_payload(tid))) {
                ext->add_trace(module, global_timestamp_, latency, event);
            }
        }
    }
    
    // 完成交易（目的模块调用）
    void complete_transaction(uint64_t tid) {
        if (transactions_.count(tid)) {
            auto& record = transactions_[tid];
            record.complete_timestamp = global_timestamp_;
            record.is_complete = true;
            
            if (export_on_complete_) {
                export_record(record);
            }
        }
    }
    
    // 链接父子交易（Cache 创建子交易时调用）
    void link_transactions(uint64_t parent_id, uint64_t child_id) {
        if (transactions_.count(parent_id)) {
            transactions_[child_id].parent_id = parent_id;
            parent_child_map_[parent_id].push_back(child_id);
        }
    }
    
    // 设置分片信息（多拍交易调用）
    void set_fragment_info(uint64_t tid, uint8_t fragment_id, uint8_t fragment_total) {
        if (transactions_.count(tid)) {
            auto& record = transactions_[tid];
            record.fragment_id = fragment_id;
            record.fragment_total = fragment_total;
            
            // 同步 Extension
            if (TransactionContextExt* ext = get_transaction_context(get_payload(tid))) {
                ext->fragment_id = fragment_id;
                ext->fragment_total = fragment_total;
            }
        }
    }
    
    // ========== 查询接口 ==========
    
    // 获取交易记录
    const TransactionRecord* get_transaction(uint64_t tid) const {
        auto it = transactions_.find(tid);
        return (it != transactions_.end()) ? &it->second : nullptr;
    }
    
    // 获取子交易列表
    std::vector<uint64_t> get_children(uint64_t parent_id) const {
        auto it = parent_child_map_.find(parent_id);
        return (it != parent_child_map_.end()) ? it->second : std::vector<uint64_t>();
    }
    
    // 获取所有活跃交易
    std::vector<uint64_t> get_active_transactions() const {
        std::vector<uint64_t> active;
        for (const auto& [tid, record] : transactions_) {
            if (!record.is_complete) {
                active.push_back(tid);
            }
        }
        return active;
    }
    
    // ========== 时间推进 ==========
    void advance_time(uint64_t delta) {
        global_timestamp_ += delta;
    }
    
    uint64_t get_current_time() const {
        return global_timestamp_;
    }
    
    // ========== 配置 ==========
    void enable_coarse_grained(bool enable) {
        enable_coarse_grained_ = enable;
    }
    
    void enable_fine_grained(bool enable) {
        enable_fine_grained_ = enable;
    }
    
private:
    uint64_t generate_id() {
        static uint64_t counter = 1;
        return counter++;
    }
    
    tlm_generic_payload* get_payload(uint64_t tid) {
        // 简化实现：实际需要从 Packet 中查找
        return nullptr;
    }
    
    void export_record(const TransactionRecord& record) {
        if (export_file_.is_open()) {
            export_file_ << record.transaction_id << ","
                         << record.parent_id << ","
                         << record.source_module << ","
                         << record.type << ","
                         << record.create_timestamp << ","
                         << record.complete_timestamp << std::endl;
        }
    }
};

#endif // TRANSACTION_TRACKER_HH
```

---

## 5. 多拍（分片）交易处理

### 5.1 粗粒度 vs 细粒度

| 粒度 | 设计 | 适用场景 |
|------|------|---------|
| **粗粒度** | 整个交易一个 `transaction_id` | 简单调试，性能分析 |
| **细粒度** | 每个分片一个 `sub_id`，通过 `parent_id` 关联 | 协议级调试，分片追踪 |

### 5.2 框架层支持

```cpp
// 框架层自动检测分片
void handle_fragment(Packet* pkt) {
    uint64_t tid = pkt->get_transaction_id();
    
    if (TransactionContextExt* ext = get_transaction_context(pkt->payload)) {
        if (ext->fragment_total > 1) {
            // 细粒度模式：每个分片独立追踪
            TransactionTracker::instance().set_fragment_info(
                tid, ext->fragment_id, ext->fragment_total);
        } else {
            // 粗粒度模式：整个交易统一追踪
            TransactionTracker::instance().record_hop(tid, "router", 1, "hopped");
        }
    }
}
```

---

## 6. 模块与框架交互流程

```
1. 模块层：
   - 重写 onTransactionStart/Hop/End()
   - 返回 TransactionInfo（包含 action）
   
2. 框架层：
   - 读取 TransactionInfo.action
   - 根据 action 自动追踪
   - 读取/写入 TlmExtension

3. 多拍交易：
   - 模块设置 fragment_id/fragment_total
   - 框架层根据配置选择粗/细粒度追踪
```

---

## 7. 需要确认的问题

| 问题 | 选项 | 推荐 |
|------|------|------|
| **Q1**: 模块处理交易方式？ | A) 手动 / B) 声明式虚方法 | **B) 声明式** |
| **Q2**: 框架层追踪粒度？ | A) 粗 / B) 细 / C) 可配置 | **C) 可配置** |
| **Q3**: Extension 读写方式？ | A) 模块直接 / B) 框架代理 | **A) 模块直接** |
| **Q4**: 多拍交易支持？ | A) 粗粒度 / B) 细粒度 / C) 两者 | **C) 两者** |

---

**下一步**: 请老板确认上述设计细节
