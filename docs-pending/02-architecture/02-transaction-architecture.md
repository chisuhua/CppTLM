# CppTLM 交易处理架构 v1.0

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **关联**: 架构 v2.0, ADR-X.6/X.7/X.8

---

## 1. 架构愿景

**核心目标**: 在混合仿真系统中，提供灵活的事务追踪机制，支持粗粒度（整交易）和细粒度（分片）两种追踪模式，同时保持 TLM/RTL 模块的职责分离。

**设计原则**:
1. **模块层简化**: TLM 模块可选择性处理交易，RTL 模块只透传
2. **框架层增强**: TransactionTracker 统一追踪，粗/细粒度可配置
3. **Extension 机制**: 复用现有 TLM Extension，向后兼容
4. **分片支持**: 通过 `parent_id + fragment_id` 标识分片归属

---

## 2. 架构分层

```
┌─────────────────────────────────────────────────────────────┐
│  应用层：用户业务模块                                         │
│  ┌─────────────────┐  ┌─────────────────┐                   │
│  │  TLMModule      │  │  RTLModule      │                   │
│  │  (智能处理)     │  │  (简单透传)     │                   │
│  └─────────────────┘  └─────────────────┘                   │
├─────────────────────────────────────────────────────────────┤
│  框架层：统一追踪                                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  TransactionTracker (单例)                           │   │
│  │  - 粗粒度追踪 (父交易)                               │   │
│  │  - 细粒度追踪 (分片)                                 │   │
│  │  - trace_log 导出                                    │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Extension 层：交易上下文                                    │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  TransactionContextExt                               │   │
│  │  - transaction_id, parent_id                         │   │
│  │  - fragment_id, fragment_total                       │   │
│  │  - trace_log                                         │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Packet 层：现有机制                                         │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Packet + stream_id + original_req                   │   │
│  │  - 与 Extension 同步                                  │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 核心数据结构

### 3.1 TransactionContextExt

```cpp
// include/ext/transaction_context_ext.hh
#ifndef TRANSACTION_CONTEXT_EXT_HH
#define TRANSACTION_CONTEXT_EXT_HH

#include "tlm.h"
#include <vector>
#include <string>

// 追踪日志条目
struct TraceEntry {
    std::string module;
    uint64_t timestamp;
    uint64_t latency;
    std::string event;  // "hopped", "blocked", "hit", "miss", "completed"
};

// 交易上下文扩展
struct TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    // ========== 核心标识字段 ==========
    uint64_t transaction_id;      // 当前交易 ID（分片使用 sub_id）
    uint64_t parent_id;           // 父交易 ID（0 表示根交易）
    uint8_t  fragment_id;         // 分片序号（0~N-1）
    uint8_t  fragment_total;      // 总分片数（1 表示不分片）
    
    // ========== 调试字段（可选） ==========
    uint64_t create_timestamp;    // 创建时间戳
    std::string source_module;    // 源模块名称
    std::string type;             // "READ" / "WRITE" / "ATOMIC"
    uint8_t  priority;            // QoS 优先级（0-7）
    
    // ========== 追踪日志 ==========
    std::vector<TraceEntry> trace_log;
    
    // ========== TLM Extension 必需方法 ==========
    tlm_extension* clone() const override {
        return new TransactionContextExt(*this);
    }
    
    void copy_from(tlm_extension const& e) override {
        auto& ext = static_cast<const TransactionContextExt&>(e);
        transaction_id = ext.transaction_id;
        parent_id = ext.parent_id;
        fragment_id = ext.fragment_id;
        fragment_total = ext.fragment_total;
        create_timestamp = ext.create_timestamp;
        source_module = ext.source_module;
        type = ext.type;
        priority = ext.priority;
        trace_log = ext.trace_log;
    }
    
    // ========== 辅助方法 ==========
    void add_trace(const std::string& module, uint64_t timestamp, uint64_t latency, const std::string& event) {
        trace_log.push_back({module, timestamp, latency, event});
    }
    
    bool is_root() const { return parent_id == 0 && fragment_total == 1; }
    bool is_fragmented() const { return fragment_total > 1; }
    bool is_first_fragment() const { return fragment_id == 0; }
    bool is_last_fragment() const { return fragment_id == fragment_total - 1; }
    
    // 分组键（用于分片重组）
    uint64_t get_group_key() const {
        return parent_id != 0 ? parent_id : transaction_id;
    }
};

// ========== 便捷函数 ==========
inline TransactionContextExt* get_transaction_context(tlm_generic_payload* p) {
    TransactionContextExt* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

inline void set_transaction_context(tlm_generic_payload* p, const TransactionContextExt& src) {
    TransactionContextExt* ext = new TransactionContextExt(src);
    p->set_extension(ext);
}

#endif // TRANSACTION_CONTEXT_EXT_HH
```

---

### 3.2 Packet 扩展

```cpp
// include/core/packet.hh (修订版片段)
class Packet {
    friend class PacketPool;
public:
    tlm::tlm_generic_payload* payload;
    uint64_t stream_id = 0;      // 与 transaction_id 同步
    uint64_t seq_num = 0;
    // ... 其他字段
    
    // ========== 便捷方法（整合 Extension） ==========
    uint64_t get_transaction_id() const {
        if (payload) {
            TransactionContextExt* ext = get_transaction_context(payload);
            if (ext) return ext->transaction_id;
        }
        return stream_id;  // 回退
    }
    
    void set_transaction_id(uint64_t tid) {
        stream_id = tid;  // 同步更新
        if (payload) {
            TransactionContextExt* ext = get_transaction_context(payload);
            if (ext) ext->transaction_id = tid;
        }
    }
    
    void add_trace(const std::string& module, uint64_t timestamp, uint64_t latency, const std::string& event) {
        if (payload) {
            TransactionContextExt* ext = get_transaction_context(payload);
            if (ext) ext->add_trace(module, timestamp, latency, event);
        }
    }
};
```

---

## 4. 模块层设计

### 4.1 模块类型分类

| 类型 | 行为 | 示例 | 重写方法 |
|------|------|------|---------|
| **透传型** | 透传交易，不修改 ID | Crossbar | `onTransactionHop()` |
| **转换型** | 创建子交易，关联父交易 | Cache | `createSubTransaction()` |
| **终止型** | 终止交易，发送响应 | Memory | `onTransactionEnd()` |

### 4.2 TLM 模块基类

```cpp
// include/core/tlm_module.hh
#ifndef TLM_MODULE_HH
#define TLM_MODULE_HH

#include "sim_object.hh"
#include "../ext/transaction_context_ext.hh"
#include <map>

// 交易处理结果
enum class TransactionAction {
    PASSTHROUGH,   // 透传
    TRANSFORM,     // 转换（创建子交易）
    TERMINATE,     // 终止
    BLOCK          // 阻塞
};

// 交易上下文信息
struct TransactionInfo {
    uint64_t transaction_id;
    uint64_t parent_id;
    uint8_t  fragment_id;
    uint8_t  fragment_total;
    TransactionAction action;
};

// 分片重组缓冲
struct FragmentBuffer {
    uint64_t parent_id;
    uint8_t fragment_total;
    std::map<uint8_t, Packet*> fragments;
    uint64_t first_arrival_time;
    
    bool is_complete() const { return fragments.size() == fragment_total; }
    bool has_fragment(uint8_t id) const { return fragments.count(id) > 0; }
};

// TLM 模块基类
class TLMModule : public SimObject {
protected:
    std::map<uint64_t, FragmentBuffer> fragment_buffers_;
    bool enable_fragment_reassembly_ = false;
    
public:
    TLMModule(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    
    // ========== 交易处理接口（可选重写） ==========
    virtual TransactionInfo onTransactionStart(Packet* pkt) {
        return {pkt->get_transaction_id(), 0, 0, 1, TransactionAction::PASSTHROUGH};
    }
    
    virtual TransactionInfo onTransactionHop(Packet* pkt) {
        return {pkt->get_transaction_id(), 0, 0, 1, TransactionAction::PASSTHROUGH};
    }
    
    virtual TransactionInfo onTransactionEnd(Packet* pkt) {
        return {pkt->get_transaction_id(), 0, 0, 1, TransactionAction::TERMINATE};
    }
    
    virtual uint64_t createSubTransaction(Packet* parent, Packet* child) {
        return parent->get_transaction_id();  // 默认不创建
    }
    
    // ========== 分片处理接口 ==========
    virtual void onFragmentReceived(Packet* pkt) {
        if (!enable_fragment_reassembly_ || !pkt->is_fragmented()) {
            handlePacket(pkt);
            return;
        }
        
        // 缓冲重组逻辑
        uint64_t group_key = pkt->get_group_key();
        // ... 缓冲和重组
    }
    
    virtual void onFragmentGroupComplete(FragmentBuffer& buffer) {
        for (auto& [id, pkt] : buffer.fragments) {
            handlePacket(pkt);
        }
    }
    
    virtual void handlePacket(Packet* pkt) = 0;
};

#endif // TLM_MODULE_HH
```

---

### 4.3 RTL 模块基类

```cpp
// include/core/rtl_module.hh
#ifndef RTL_MODULE_HH
#define RTL_MODULE_HH

#include "sim_object.hh"
#include "ch.hpp"
#include "chlib/stream.h"

// RTL 模块基类（简单透传）
template<typename ComponentT, typename ReqBundle, typename RespBundle>
class RTLModule : public SimObject {
protected:
    std::unique_ptr<ch::ch_device<ComponentT>> device_;
    SimplePort* req_port;
    SimplePort* resp_port;
    
public:
    RTLModule(const std::string& name) : SimObject(name) {
        device_ = std::make_unique<ch::ch_device<ComponentT>>();
        req_port = new SimplePort();
        resp_port = new SimplePort();
    }
    
    void tick() override {
        // Port → ch_stream → Component → ch_stream → Port
        // 透传 transaction_id 和 Extension
        // ... 实现见 ADR-X.8
    }
};

#endif // RTL_MODULE_HH
```

---

## 5. 框架层设计

### 5.1 TransactionTracker

```cpp
// include/framework/transaction_tracker.hh
#ifndef TRANSACTION_TRACKER_HH
#define TRANSACTION_TRACKER_HH

#include "../ext/transaction_context_ext.hh"
#include <map>
#include <vector>

struct TransactionRecord {
    uint64_t transaction_id;
    uint64_t parent_id;
    uint8_t fragment_id;
    uint8_t fragment_total;
    std::string source_module;
    std::string type;
    uint64_t create_timestamp;
    uint64_t complete_timestamp;
    std::vector<std::pair<std::string, uint64_t>> hop_log;
    bool is_complete = false;
};

class TransactionTracker {
private:
    std::map<uint64_t, TransactionRecord> transactions_;
    std::map<uint64_t, std::vector<uint64_t>> parent_child_map_;
    uint64_t global_timestamp_ = 0;
    bool enable_coarse_grained_ = true;
    bool enable_fine_grained_ = true;
    
    TransactionTracker() = default;
    
public:
    static TransactionTracker& instance() {
        static TransactionTracker tracker;
        return tracker;
    }
    
    void initialize(const std::string& export_path = "");
    uint64_t create_transaction(tlm_generic_payload* payload, const std::string& source, const std::string& type);
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency, const std::string& event);
    void complete_transaction(uint64_t tid);
    void link_transactions(uint64_t parent_id, uint64_t child_id);
    void set_fragment_info(uint64_t tid, uint8_t fragment_id, uint8_t fragment_total);
    
    const TransactionRecord* get_transaction(uint64_t tid) const;
    std::vector<uint64_t> get_children(uint64_t parent_id) const;
    std::vector<uint64_t> get_active_transactions() const;
    
    void advance_time(uint64_t delta);
    uint64_t get_current_time() const;
    
    void enable_coarse_grained(bool enable);
    void enable_fine_grained(bool enable);
};

#endif // TRANSACTION_TRACKER_HH
```

---

## 6. 完整示例代码

### 6.1 场景 1：简单读请求（无分片）

```cpp
// 1. CPU 创建交易
Packet* req = PacketPool::get().acquire();
req->payload = new tlm_generic_payload();
req->type = PKT_REQ;
req->cmd = CMD_READ;
req->payload->set_address(0x1000);

// 设置交易 ID
uint64_t tid = TransactionTracker::instance().create_transaction(
    req->payload, "cpu_0", "READ");
req->set_transaction_id(tid);

// 发送请求
cpu_port->sendReq(req);

// 2. Crossbar 透传
// CrossbarV2::handleUpstreamRequest() 调用 onTransactionHop()
// 框架层记录：record_hop(tid, "crossbar", 1, "hopped")

// 3. Memory 终止
// MemoryV2::handleDownstreamRequest() 调用 onTransactionEnd()
// 创建响应，透传 tid
// 框架层记录：complete_transaction(tid)
```

---

### 6.2 场景 2：Cache Miss 创建子交易

```cpp
// 1. CPU 发送请求
Packet* parent_req = PacketPool::get().acquire();
uint64_t parent_tid = TransactionTracker::instance().create_transaction(
    parent_req->payload, "cpu_0", "READ");
parent_req->set_transaction_id(parent_tid);

// 2. Cache Miss，创建子交易
Packet* child_req = PacketPool::get().acquire();
child_req->payload = parent_req->payload;

CacheV2* cache = ...;
uint64_t child_tid = cache->createSubTransaction(parent_req, child_req);

// 设置子交易 Extension
if (TransactionContextExt* ext = get_transaction_context(child_req->payload)) {
    ext->parent_id = parent_tid;
}

// 3. 框架层链接父子交易
TransactionTracker::instance().link_transactions(parent_tid, child_tid);

// 4. 子交易完成后，Cache 发送响应给 CPU
Packet* resp = PacketPool::get().acquire();
resp->set_transaction_id(parent_tid);  // 响应使用父交易 ID
```

---

### 6.3 场景 3：多拍分片交易

```cpp
// 1. CPU 创建 4 个分片
const int NUM_FRAGMENTS = 4;
for (int i = 0; i < NUM_FRAGMENTS; i++) {
    Packet* frag = PacketPool::get().acquire();
    frag->payload = new tlm_generic_payload();
    frag->type = PKT_REQ;
    
    uint64_t fragment_tid = TransactionTracker::instance().create_transaction(
        frag->payload, "cpu_0", "WRITE");
    
    // 设置分片信息
    if (TransactionContextExt* ext = get_transaction_context(frag->payload)) {
        ext->parent_id = 100;  // 父交易 ID
        ext->fragment_id = i;
        ext->fragment_total = NUM_FRAGMENTS;
    }
    
    frag->set_transaction_id(fragment_tid);
    cpu_port->sendReq(frag);
}

// 2. Crossbar 透传（每个分片独立记录）
// 框架层：record_hop(fragment_tid, "crossbar", 1, "fragment_hopped")
// 同时记录父交易：record_hop(100, "crossbar", 1, "parent_fragment_hopped")

// 3. Memory 重组所有分片
// MemoryTLMV2::onFragmentReceived() 缓冲每个分片
// 当 4 个分片全部到达后，调用 onFragmentGroupComplete()
// 重组后执行内存操作，发送响应

// 4. 框架层完成父交易
TransactionTracker::instance().complete_transaction(100);
```

---

### 6.4 场景 4：RTL 模块透传

```cpp
// RTL 模块不处理交易上下文，只透传 ch_stream 数据

// 1. 请求路径：Port → ch_stream → Component
void tick() override {
    Packet* req_pkt;
    if (req_port->recv(req_pkt)) {
        // 解码为 Bundle
        ReqBundle bundle = decode_req(req_pkt);
        
        // 驱动 Component
        io().req_in.payload = bundle;
        io().req_in.valid = true;
    }
    
    device_->tick();
    
    // 2. 响应路径：Component → ch_stream → Port
    if (io().resp_out.valid) {
        RespBundle resp_bundle = io().resp_out.payload;
        
        Packet* resp_pkt = encode_resp(resp_bundle);
        
        // ✅ 透传 transaction_id
        resp_pkt->set_transaction_id(req_pkt->get_transaction_id());
        
        // ✅ 透传 Extension
        if (TransactionContextExt* req_ext = get_transaction_context(req_pkt->payload)) {
            TransactionContextExt* resp_ext = new TransactionContextExt(*req_ext);
            resp_pkt->payload->set_extension(resp_ext);
        }
        
        resp_port->send(resp_pkt);
    }
}
```

---

## 7. 配置与使用

### 7.1 启用交易追踪

```cpp
// main.cpp
int sc_main() {
    // 初始化 TransactionTracker
    TransactionTracker::instance().initialize("trace_log.csv");
    
    // 配置追踪粒度
    TransactionTracker::instance().enable_coarse_grained(true);  // 父交易追踪
    TransactionTracker::instance().enable_fine_grained(true);    // 分片追踪
    
    // 创建模块
    auto* cpu = new CPUSim("cpu", eq);
    auto* crossbar = new CrossbarTLMV2("crossbar", eq);
    auto* cache = new CacheV2("cache", eq);
    auto* memory = new MemoryTLMV2("memory", eq);
    
    // 运行仿真
    eq.run(100000);
    
    return 0;
}
```

### 7.2 导出格式

```csv
# trace_log.csv
transaction_id,parent_id,source_module,type,create_time,complete_time,hop_log
100,0,cpu_0,READ,0,150,"crossbar:1,memory:100"
201,200,cpu_0,WRITE,10,200,"crossbar:1,cache:50,memory:100"
202,200,cpu_0,WRITE,10,200,"crossbar:1,cache:50,memory:100"
```

---

## 8. 架构优势

| 特性 | 传统方案 | 本方案 |
|------|---------|--------|
| **模块复杂度** | 所有模块处理交易 | TLM 智能，RTL 简单 |
| **分片支持** | 无/手动 | 自动重组，parent_id 关联 |
| **追踪粒度** | 固定 | 粗/细可配置 |
| **向后兼容** | - | ✅ 复用现有 Packet/Extension |
| **性能开销** | 高 | 低（可选启用） |

---

## 9. 相关文档

| 文档 | 位置 |
|------|------|
| 架构 v2.0 | `01-hybrid-architecture-v2.md` |
| ADR-X.6 整合方案 | `../03-adr/ADR-X.6-transaction-integration.md` |
| ADR-X.7 交易处理 | `../03-adr/ADR-X.7-transaction-handling.md` |
| ADR-X.8 分片处理 | `../03-adr/ADR-X.8-fragment-handling.md` |
| 示例代码 | `../02-architecture/examples/` |

---

**版本**: v1.0  
**创建日期**: 2026-04-09  
**状态**: 📋 待确认
