# ADR-X.7: 模块与框架层交易处理职责划分

> **版本**: 2.0
> **日期**: 2026-04-09
> **状态**: ✅ 已实施
> **影响**: v2.0 - 模块/框架职责边界

---

## 1. 核心决策

**交易生命周期钩子放在 `TLMModule`（SimObject 的派生类）上，而非 SimObject 基类上**。

| 模块类型 | 职责 | 说明 |
|---------|------|------|
| **Initiator**（CPU） | 分配 transaction_id | 通过 `set_transaction_id()` |
| **Pass-through**（Crossbar） | 透传 transaction_id | 直接传递 Bundle |
| **Transform**（Cache） | 可能创建子交易 | 通过 `createSubTransaction()` |
| **Terminate**（Memory） | 使用 transaction_id 构建响应 | 透传到响应中 |

**注意**：实际 TLM 模块通过 StreamAdapter 框架自动调用 `tick()`，不需要手动调用 TransactionTracker。

---

## 2. 模块层设计

### 2.1 SimObject 基类（交易无关）

```cpp
// include/core/sim_object.hh
class SimObject {
protected:
    std::string name_;
    EventQueue* event_queue;

public:
    SimObject(const std::string& n, EventQueue* eq) : name_(n), event_queue(eq) {}
    virtual ~SimObject() = default;

    // 核心接口
    virtual void tick() = 0;
    virtual void init() {}
    virtual void reset() {}

    const std::string& name() const { return name_; }
};
```

### 2.2 TLMModule 基类（交易相关）

```cpp
// include/core/tlm_module.hh
class TLMModule : public SimObject {
protected:
    // 子交易计数器（线程安全）
    std::atomic<uint64_t> sub_transaction_counter_{100000};

public:
    using SimObject::SimObject;

    // ========== 交易生命周期钩子 ==========
    virtual void onTransactionStart(Packet* pkt, TransactionContextExt* ext) {}
    virtual void onTransactionHop(Packet* pkt, TransactionContextExt* ext) {}
    virtual void onTransactionEnd(Packet* pkt, TransactionContextExt* ext) {}

    // ========== 子交易创建 ==========
    virtual uint64_t createSubTransaction(Packet* parent_pkt, Packet* child_pkt) {
        uint64_t sub_id = sub_transaction_counter_++;
        if (child_pkt->payload) {
            TransactionContextExt* ext = get_transaction_context(child_pkt->payload);
            if (!ext) {
                ext = create_transaction_context(child_pkt->payload, sub_id);
            }
            ext->parent_id = parent_pkt->get_transaction_id();
            ext->transaction_id = sub_id;
            ext->fragment_id = 0;
            ext->fragment_total = 1;
        }
        child_pkt->set_transaction_id(sub_id);
        return sub_id;
    }

    // ========== 分片重组 ==========
    void processWithFragmentation(Packet* pkt) {
        // 分片缓冲处理逻辑
    }
};
```

### 2.3 TransactionInfo 结构体

```cpp
// include/core/sim_object.hh
enum class TransactionAction {
    PASSTHROUGH,   // 透传（Crossbar）
    TRANSFORM,      // 转换（Cache）
    TERMINATE,      // 终止（Memory）
    BLOCK           // 阻塞
};

struct TransactionInfo {
    uint64_t transaction_id = 0;
    uint64_t parent_id = 0;
    uint8_t  fragment_id = 0;
    uint8_t  fragment_total = 1;
    TransactionAction action = TransactionAction::PASSTHROUGH;

    bool is_root() const { return parent_id == 0 && fragment_total == 1; }
    bool is_fragmented() const { return fragment_total > 1; }
};
```

---

## 3. 框架层设计：TransactionTracker

```cpp
// include/framework/transaction_tracker.hh
class TransactionTracker {
private:
    std::map<uint64_t, TransactionRecord> transactions_;
    std::map<uint64_t, std::vector<uint64_t>> parent_child_map_;
    uint64_t global_timestamp_ = 0;

public:
    static TransactionTracker& instance();

    // 创建事务
    uint64_t create_transaction(tlm_generic_payload* payload,
                                 const std::string& source,
                                 const std::string& type);

    // 记录经过
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency, const std::string& event);

    // 完成事务
    void complete_transaction(uint64_t tid);

    // 链接父子交易
    void link_transactions(uint64_t parent_id, uint64_t child_id);

    // 设置分片信息
    void set_fragment_info(uint64_t tid, uint8_t fragment_id, uint8_t fragment_total);

    // 查询
    const TransactionRecord* get_transaction(uint64_t tid) const;
    std::vector<uint64_t> get_children(uint64_t parent_id) const;
    std::vector<uint64_t> get_active_transactions() const;

    // 时间
    void advance_time(uint64_t delta);
    uint64_t get_current_time() const { return global_timestamp_; }

    // 配置
    void enable_coarse_grained(bool enable);
    void enable_fine_grained(bool enable);

    // 测试支持
    void reset_for_testing();

private:
    void check_parent_completion(uint64_t child_id);
};

// 交易记录
struct TransactionRecord {
    uint64_t transaction_id = 0;
    uint64_t parent_id = 0;
    uint8_t  fragment_id = 0;
    uint8_t  fragment_total = 1;
    std::string source_module;
    std::string type;
    uint64_t create_timestamp = 0;
    uint64_t complete_timestamp = 0;
    bool is_complete = false;
    std::vector<std::pair<std::string, uint64_t>> hop_log;
    std::vector<uint64_t> child_transactions;  // 新增：子交易列表
};
```

---

## 4. 实际模块示例

### 4.1 CrossbarTLM（透传型）

```cpp
// include/tlm/crossbar_tlm.hh
class CrossbarTLM : public TLMModule {
private:
    ch_in<NoCReqBundle> req_in;
    std::vector<ch_out<NoCReqBundle>> req_outs;

public:
    void processRequest() {
        auto req = req_in.payload;

        // 透传 transaction_id
        int dst_port = route(req.address);
        req_outs[dst_port].payload = req;
        req_outs[dst_port].valid = true;
    }

    int route(uint64_t addr) { return (addr >> 24) & 0x3; }
};
```

### 4.2 CacheTLM（转换型）

```cpp
// include/tlm/cache_tlm.hh
class CacheTLM : public TLMModule {
private:
    ch_in<CacheReqBundle> req_in;
    ch_out<CacheReqBundle> req_out;
    ch_out<CacheRespBundle> resp_out;
    std::map<uint64_t, uint64_t> cache_lines_;

public:
    void processRequest() {
        auto req = req_in.payload;
        uint64_t addr = req.address;

        if (cacheHit(addr)) {
            // Cache Hit：直接响应，透传 transaction_id
            CacheRespBundle resp;
            resp.transaction_id = req.transaction_id;
            resp.data = cache_lines_[addr];
            resp_out.payload = resp;
            resp_out.valid = true;
        } else {
            // Cache Miss：透传到下游
            req_out.payload = req;
            req_out.valid = true;
        }
    }

    bool cacheHit(uint64_t addr) { return cache_lines_.count(addr); }
};
```

### 4.3 MemoryTLM（终止型）

```cpp
// include/tlm/memory_tlm.hh
class MemoryTLM : public TLMModule {
private:
    ch_in<CacheReqBundle> req_in;
    ch_out<CacheRespBundle> resp_out;
    std::map<uint64_t, uint64_t> memory_;

public:
    void processRequest() {
        auto req = req_in.payload;
        uint64_t addr = req.address;

        // 内存访问
        if (req.is_write) {
            memory_[addr] = req.data;
        }

        // 创建响应，透传 transaction_id
        CacheRespBundle resp;
        resp.transaction_id = req.transaction_id;
        resp.data = memory_[addr];
        resp_out.payload = resp;
        resp_out.valid = true;
    }
};
```

---

## 5. Bundle ↔ Packet 转换

```
CPU (Bundle) → StreamAdapter → Packet → ... → Packet → StreamAdapter → Memory (Bundle)
  └─ transaction_id        └─ stream_id + Ext        └─ transaction_id
```

StreamAdapter 在转换时保持 `transaction_id` 不变。

---

## 6. 分片交易处理

| 粒度 | 说明 | 支持 |
|------|------|------|
| **粗粒度** | 整个交易一个 `transaction_id` | ✅ |
| **细粒度** | 每个分片一个 sub_id，通过 `parent_id` 关联 | ✅ |

### FragmentBuffer（分片重组缓冲）

```cpp
// include/core/sim_object.hh
template<typename K, typename V>
class FragmentBuffer {
private:
    std::map<K, std::vector<V>> buffer_;
    std::map<K, uint8_t> expected_;
    std::map<K, std::vector<V>> complete_;

public:
    void add_fragment(K key, V fragment, uint8_t total);
    bool is_complete(K key) const;
    std::vector<V> get_complete(K key) const;
    void onFragmentGroupComplete(K key);
};
```

---

## 7. 实际实现 vs 文档设计

| 维度 | ADR-X.7 设计 | 代码实际 | 一致？ |
|------|-------------|---------|--------|
| `TransactionAction` 枚举 | ✅ SimObject | ✅ sim_object.hh | ✅ |
| `TransactionInfo` 结构体 | ✅ | ✅ + `is_root()`/`is_fragmented()` | ✅ |
| `onTransactionStart/Hop/End()` | SimObject 基类 | ⚠️ `TLMModule` 派生类 | ⚠️ 更合理 |
| `createSubTransaction()` | SimObject 基类 | ⚠️ `TLMModule`，含 `std::atomic` | ✅ 线程安全 |
| `FragmentBuffer` | 未提及 | ✅ 新增 | ✅ |
| `processWithFragmentation()` | 未提及 | ✅ 新增 | ✅ |
| `TransactionTracker::create_transaction()` | ✅ | ✅ | ✅ |
| `TransactionTracker::record_hop()` | ✅ | ✅ | ✅ |
| `TransactionTracker::complete_transaction()` | ✅ | ✅ + 父交易完成检查 | ✅ |
| `TransactionTracker::link_transactions()` | ✅ | ✅ | ✅ |
| `TransactionTracker::set_fragment_info()` | ✅ | ✅ | ✅ |
| `TransactionTracker::check_parent_completion()` | 未提及 | ✅ 新增（自动传播完成） | ✅ |
| `TransactionRecord::child_transactions` | 未提及 | ✅ 新增 | ✅ |

---

## 8. 模块基类差异说明

| 模块类型 | 基类 | 交易钩子 | 使用场景 |
|---------|------|---------|---------|
| **Legacy 模块** | `SimObject` | 无 | 传统模块，手动管理 Packet |
| **TLM 模块** | `TLMModule` | 有 | ChStream 模块，自动 `tick()` |

**设计理由**：不是所有 SimObject 都需要交易生命周期钩子，将这些方法放在 `TLMModule` 上更合理。

---

## 9. 相关文档

| 文档 | 位置 |
|------|------|
| SimObject 基类 | `include/core/sim_object.hh` |
| TLMModule 基类 | `include/core/tlm_module.hh` |
| TransactionTracker | `include/framework/transaction_tracker.hh` |
| TransactionContextExt | `include/ext/transaction_context_ext.hh` |
| CacheTLM | `include/tlm/cache_tlm.hh` |
| CrossbarTLM | `include/tlm/crossbar_tlm.hh` |
| MemoryTLM | `include/tlm/memory_tlm.hh` |

---

**状态**: ✅ 已实施<br>
**最后更新**: 2026-05-01
