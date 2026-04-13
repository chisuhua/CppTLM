# ADR-X.6: TransactionContext 与现有 Packet/Extension 整合方案

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **影响**: v2.0 - 事务追踪架构设计

---

## 1. 现有架构分析

### 1.1 Packet 类（现有）

```cpp
// include/core/packet.hh
class Packet {
public:
    tlm::tlm_generic_payload* payload;
    
    // 流控/事务标识
    uint64_t stream_id = 0;      // ← 相当于 transaction_id
    uint64_t seq_num = 0;
    CmdType cmd;
    PacketType type;
    
    // 时间统计
    uint64_t src_cycle;
    uint64_t dst_cycle;
    
    // 请求 - 响应关联
    Packet* original_req = nullptr;  // ← 响应包指向原始请求
    std::vector<Packet*> dependents;
    
    // 路由信息
    std::vector<std::string> route_path;
    int hop_count = 0;
    uint8_t priority = 0;
    uint64_t flow_id = 0;
    int vc_id = 0;
    
    // 方法
    uint64_t getDelayCycles() const;
    uint64_t getEnd2EndCycles() const;
};
```

**现有设计特点**:
- `stream_id` 作为事务标识
- `original_req` 用于响应匹配
- `route_path` / `hop_count` 用于追踪
- `src_cycle` / `dst_cycle` 用于延迟统计

---

### 1.2 Extension 机制（现有）

```cpp
// include/core/ext/cmd_exts.hh
GEMSC_TLM_EXTENSION_DEF(ReadCmdExt,   ReadCmd)
GEMSC_TLM_EXTENSION_DEF(WriteCmdExt,  WriteCmd)
GEMSC_TLM_EXTENSION_DEF(WriteDataExt, WriteData)
GEMSC_TLM_EXTENSION_DEF(ReadRespExt,  ReadResp)
GEMSC_TLM_EXTENSION_DEF(WriteRespExt, WriteResp)
GEMSC_TLM_EXTENSION_DEF(StreamIDExt,  StreamUniqID)
GEMSC_TLM_EXTENSION_DEF_SIMPLE(ReqIDExt, int32_t, req_id)

// include/ext/coherence_extension.hh
struct CoherenceExtension : public tlm::tlm_extension<CoherenceExtension> {
    CacheState prev_state = INVALID;
    CacheState next_state = INVALID;
    bool is_exclusive = false;
    uint64_t sharers_mask = 0;
    bool needs_snoop = true;
};
```

**现有设计特点**:
- 基于 SystemC TLM extension 机制
- 每个 Extension 有 `clone()` / `copy_from()` 方法
- 可附加到 `tlm_generic_payload` 上

---

### 1.3 模块使用方式（现有）

```cpp
// include/modules/cache_sim.hh
class CacheSim : public SimObject {
public:
    bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) {
        uint64_t addr = pkt->payload->get_address();
        bool hit = (addr & 0x7) == 0;
        
        if (hit) {
            // 创建响应包
            Packet* resp = PacketPool::get().acquire();
            resp->payload = pkt->payload;
            resp->type = PKT_RESP;
            resp->original_req = pkt;  // ← 关联原始请求
            resp->stream_id = pkt->stream_id;  // ← 透传 stream_id
            
            // 发送响应
            getPortManager().getUpstreamPorts()[src_id]->sendResp(resp);
        }
    }
};
```

---

## 2. 整合方案设计

### 2.1 核心原则

**保留现有机制** + **增强 TransactionContext**：

| 现有机制 | 保留 | 增强 |
|---------|------|------|
| `Packet::stream_id` | ✅ 保留 | 作为 `transaction_id` 使用 |
| `Packet::original_req` | ✅ 保留 | 响应匹配机制不变 |
| `Packet::route_path` | ✅ 保留 | 用于调试追踪 |
| `tlm_extension` | ✅ 保留 | 添加 `TransactionContextExt` |
| `PacketPool` | ✅ 保留 | 管理 Extension 生命周期 |

---

### 2.2 TransactionContextExt 设计

```cpp
// include/ext/transaction_context_ext.hh
#ifndef TRANSACTION_CONTEXT_EXT_HH
#define TRANSACTION_CONTEXT_EXT_HH

#include "tlm.h"
#include <vector>
#include <string>

// 事务上下文扩展（可选启用）
struct TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    // ========== 核心字段（简化版，v2.0） ==========
    uint64_t transaction_id;      // 事务 ID（与 Packet::stream_id 同步）
    uint64_t parent_id;           // 父事务 ID（0 表示无父事务）
    uint8_t  fragment_id;         // 分片 ID（0 表示不分片）
    uint8_t  fragment_total;      // 总分片数（1 表示不分片）
    
    // ========== 调试字段（可选，v2.1） ==========
    uint64_t create_timestamp;    // 创建时间戳
    std::string source_module;    // 源模块名称
    std::string type;             // 事务类型：READ/WRITE/ATOMIC
    uint8_t  priority;            // QoS 优先级
    
    // ========== 追踪日志（框架层专用） ==========
    struct TraceEntry {
        std::string module;
        uint64_t timestamp;
        uint64_t latency;
        std::string event;  // "hopped", "blocked", "completed"
    };
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
    
    bool is_root() const { return parent_id == 0; }
    bool is_fragmented() const { return fragment_total > 1; }
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

inline uint64_t get_or_create_transaction_id(tlm_generic_payload* p) {
    TransactionContextExt* ext = get_transaction_context(p);
    if (ext) {
        return ext->transaction_id;
    }
    // 无 Extension 时，返回 0（由调用者分配）
    return 0;
}

#endif // TRANSACTION_CONTEXT_EXT_HH
```

---

### 2.3 Packet 类整合

```cpp
// include/core/packet.hh (修订版)
class Packet {
    friend class PacketPool;
public:
    tlm::tlm_generic_payload* payload;
    
    // ========== 事务标识（与 Extension 同步） ==========
    uint64_t stream_id = 0;           // ← 与 TransactionContextExt::transaction_id 同步
    uint64_t seq_num = 0;
    
    // ========== 时间统计 ==========
    uint64_t src_cycle;
    uint64_t dst_cycle;
    
    // ========== 请求 - 响应关联 ==========
    Packet* original_req = nullptr;
    std::vector<Packet*> dependents;
    
    // ========== 路由追踪 ==========
    std::vector<std::string> route_path;
    int hop_count = 0;
    uint8_t priority = 0;
    uint64_t flow_id = 0;
    int vc_id = 0;
    
    // ========== 便捷方法（整合 Extension） ==========
    uint64_t get_transaction_id() const {
        // 优先从 Extension 获取
        if (payload) {
            TransactionContextExt* ext = get_transaction_context(payload);
            if (ext) return ext->transaction_id;
        }
        // 回退到 stream_id
        return stream_id;
    }
    
    void set_transaction_id(uint64_t tid) {
        stream_id = tid;  // 同步更新
        if (payload) {
            TransactionContextExt* ext = get_transaction_context(payload);
            if (ext) {
                ext->transaction_id = tid;
            }
        }
    }
    
    // 添加追踪日志
    void add_trace(const std::string& module, uint64_t timestamp, uint64_t latency, const std::string& event) {
        if (payload) {
            TransactionContextExt* ext = get_transaction_context(payload);
            if (ext) {
                ext->add_trace(module, timestamp, latency, event);
            }
        }
    }
};
```

---

### 2.4 模块层整合（简化）

```cpp
// include/modules/cache_sim_v2.hh (新设计)
class CacheSimV2 : public SimObject {
public:
    bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) {
        // ========== 模块层只传播 transaction_id ==========
        uint64_t tid = pkt->get_transaction_id();
        
        uint64_t addr = pkt->payload->get_address();
        bool hit = (addr & 0x7) == 0;
        
        if (hit) {
            // 创建响应包
            Packet* resp = PacketPool::get().acquire();
            resp->payload = pkt->payload;
            resp->type = PKT_RESP;
            resp->original_req = pkt;
            
            // ✅ 透传 transaction_id
            resp->set_transaction_id(tid);
            
            // ✅ 设置错误码（可选）
            if (TransactionContextExt* ext = get_transaction_context(resp->payload)) {
                // 框架层可在此添加追踪日志
                // ext->add_trace(name(), event_queue->getCurrentCycle(), 1, "hit");
            }
            
            event_queue->schedule([this, resp, src_id]() {
                getPortManager().getUpstreamPorts()[src_id]->sendResp(resp);
            }, 1);
            
            PacketPool::get().release(pkt);
        } else {
            // Miss: 转发到下游
            req_buffer.push(pkt);
            scheduleForward(1);
        }
        return true;
    }
};
```

**模块设计者视角**:
- 只调用 `get_transaction_id()` / `set_transaction_id()`
- 不感知完整 Context 结构
- 业务逻辑清晰

---

### 2.5 框架层整合（增强）

```cpp
// include/framework/transaction_tracker.hh
#ifndef TRANSACTION_TRACKER_HH
#define TRANSACTION_TRACKER_HH

#include "../core/packet.hh"
#include "../ext/transaction_context_ext.hh"
#include <map>
#include <memory>

// 事务追踪器（框架层单例）
class TransactionTracker {
private:
    std::map<uint64_t, TransactionContextExt*> active_transactions_;
    uint64_t global_timestamp_ = 0;
    uint64_t next_transaction_id_ = 1;
    
    TransactionTracker() = default;
    
public:
    static TransactionTracker& instance() {
        static TransactionTracker tracker;
        return tracker;
    }
    
    // 创建事务（框架层调用）
    uint64_t create_transaction(tlm_generic_payload* payload, 
                                 const std::string& source, 
                                 const std::string& type) {
        uint64_t tid = next_transaction_id_++;
        
        TransactionContextExt* ext = new TransactionContextExt();
        ext->transaction_id = tid;
        ext->parent_id = 0;
        ext->fragment_id = 0;
        ext->fragment_total = 1;
        ext->create_timestamp = global_timestamp_;
        ext->source_module = source;
        ext->type = type;
        
        payload->set_extension(ext);
        active_transactions_[tid] = ext;
        
        return tid;
    }
    
    // 记录模块经过（框架层调用）
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency) {
        if (active_transactions_.count(tid)) {
            auto* ext = active_transactions_[tid];
            ext->add_trace(module, global_timestamp_, latency, "hopped");
        }
    }
    
    // 完成事务（框架层调用）
    void complete_transaction(uint64_t tid) {
        if (active_transactions_.count(tid)) {
            // 可在此导出 trace_log 到文件
            auto* ext = active_transactions_[tid];
            export_trace(tid, ext);
            
            active_transactions_.erase(tid);
            delete ext;
        }
    }
    
    // 推进全局时间
    void advance_time(uint64_t delta) {
        global_timestamp_ += delta;
    }
    
    // 获取活跃事务数
    size_t active_count() const {
        return active_transactions_.size();
    }
    
private:
    void export_trace(uint64_t tid, const TransactionContextExt* ext) {
        // 导出 trace_log 到文件（v2.1 实现）
        // 格式：tid, timestamp, module, latency, event
    }
};

#endif // TRANSACTION_TRACKER_HH
```

---

### 2.6 PacketPool 整合

```cpp
// include/core/ext/packet_pool.hh (修订版)
class PacketPool {
private:
    std::vector<Packet*> pool_;
    size_t next_index_ = 0;
    
    PacketPool() {
        for (int i = 0; i < POOL_SIZE; ++i) {
            pool_.push_back(new Packet(new tlm::tlm_generic_payload(), 0, PKT_REQ));
        }
    }
    
public:
    static PacketPool& get() {
        static PacketPool pool;
        return pool;
    }
    
    Packet* acquire() {
        Packet* pkt = pool_[next_index_];
        next_index_ = (next_index_ + 1) % POOL_SIZE;
        
        // ✅ 清理 Extension
        if (pkt->payload) {
            // 清理所有 Extension（包括 TransactionContextExt）
            pkt->payload->clear_extensions();
        }
        
        pkt->reset();
        return pkt;
    }
    
    void release(Packet* pkt) {
        // ✅ 清理 Extension
        if (pkt->payload && !pkt->isCredit()) {
            pkt->payload->clear_extensions();
        }
        
        pkt->reset();
    }
};
```

---

## 3. 整合方案对比

| 方面 | 方案 A（完全替换） | 方案 B（整合现有）✅ |
|------|------------------|---------------------|
| **代码改动** | 大（重写 Packet） | 小（扩展 Extension） |
| **向后兼容** | ❌ 不兼容 | ✅ 兼容 |
| **学习成本** | 高 | 低 |
| **实施风险** | 高 | 低 |
| **功能完整** | ✅ | ✅ |

---

## 4. 实施建议

### v2.0 实施（现在）

1. **添加 `TransactionContextExt`**：`include/ext/transaction_context_ext.hh`
2. **修订 `Packet` 类**：添加 `get_transaction_id()` / `set_transaction_id()` 方法
3. **修订 `PacketPool`**：清理 Extension 生命周期
4. **模块迁移**：逐步迁移现有模块使用新方法

### v2.1 实施（未来）

1. **添加 `TransactionTracker`**：框架层单例
2. **添加 trace_log 导出**：CSV/JSON 格式
3. **添加子交易支持**：`parent_id` / `fragment_id` 完整实现

---

## 5. 需要确认的问题

| 问题 | 选项 | 推荐 |
|------|------|------|
| **Q1**: TransactionContext 位置？ | A) Bundle / B) Extension | **B) Extension** |
| **Q2**: 与 Packet 整合方式？ | A) 替换 / B) 共存 | **B) 共存** |
| **Q3**: 模块层传播字段？ | A) 完整 Context / B) 仅 ID | **B) 仅 ID** |
| **Q4**: 框架层追踪？ | A) 无 / B) TransactionTracker | **B) TransactionTracker** |

---

**下一步**: 请老板确认整合方案细节
