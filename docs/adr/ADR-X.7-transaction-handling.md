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
    bool fragment_reassembly_enabled_ = false;
    std::map<uint64_t, FragmentBuffer> fragment_buffers_;

public:
    using SimObject::SimObject;

    // ========== 交易生命周期钩子 ==========
    virtual TransactionInfo onTransactionStart(Packet* pkt) {
        return TransactionInfo{pkt->get_transaction_id(), 0, 0, 1, TransactionAction::PASSTHROUGH};
    }
    virtual TransactionInfo onTransactionHop(Packet* pkt) {
        return TransactionInfo{pkt->get_transaction_id(), 0, 0, 1, TransactionAction::PASSTHROUGH};
    }
    virtual TransactionInfo onTransactionEnd(Packet* pkt) {
        return TransactionInfo{pkt->get_transaction_id(), 0, 0, 1, TransactionAction::TERMINATE};
    }

    // ========== 子交易创建（线程安全） ==========
    virtual uint64_t createSubTransaction(Packet* parent_pkt, Packet* child_pkt) {
        static std::atomic<uint64_t> g_sub_tid{20000};
        uint64_t tid = g_sub_tid.fetch_add(1);
        if (child_pkt && parent_pkt) {
            child_pkt->set_transaction_id(tid);
            if (auto* ext = get_transaction_context(child_pkt->payload)) {
                ext->parent_id = parent_pkt->get_transaction_id();
            } else {
                create_transaction_context(child_pkt->payload, tid, parent_pkt->get_transaction_id());
            }
        }
        return tid;
    }

    // ========== 分片重组 ==========
    void enableFragmentReassembly(bool e) { fragment_reassembly_enabled_ = e; }
    virtual bool processWithFragmentation(Packet* pkt) {
        // 分片缓冲处理逻辑，返回 true=可处理，false=等待更多分片
        return true;
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
class CrossbarTLM : public ChStreamModuleBase {
private:
    InputStreamAdapter<NoCReqBundle> req_in;
    std::vector<OutputStreamAdapter<NoCReqBundle>> req_outs;
    std::vector<OutputStreamAdapter<NoCRespBundle>> resp_outs;

public:
    void tick() override {
        if (!req_in.available()) return;

        auto req = req_in.read();
        // 透传 transaction_id（通过 Bundle 直接传递）
        int dst_port = route_address(req.address);
        req_outs[dst_port].write(req);
    }

    int route_address(uint64_t addr) { return (addr >> 24) & 0x3; }
};
```

> **注意**：TLM 模块通过 StreamAdapter 与框架交互，不直接调用 `onTransactionHop()`。`TLMModule` 基类定义了这些钩子，但当前实现中这些钩子由 Legacy V2 模块（`CacheV2/CrossbarV2/MemoryV2`）使用。

### 4.2 CacheTLM（转换型）

```cpp
// include/tlm/cache_tlm.hh
class CacheTLM : public ChStreamModuleBase {
private:
    InputStreamAdapter<CacheReqBundle> req_in;
    OutputStreamAdapter<CacheReqBundle> req_out;
    OutputStreamAdapter<CacheRespBundle> resp_out;
    std::map<uint64_t, uint64_t> cache_lines_;

public:
    void tick() override {
        if (!req_in.available()) return;

        auto req = req_in.read();
        uint64_t addr = req.address;

        if (cacheHit(addr)) {
            // Cache Hit：直接响应，透传 transaction_id
            CacheRespBundle resp;
            resp.transaction_id = req.transaction_id;
            resp.data = cache_lines_[addr];
            resp_out.write(resp);
        } else {
            // Cache Miss：透传到下游（无 createSubTransaction）
            req_out.write(req);
        }
    }

    bool cacheHit(uint64_t addr) const { return cache_lines_.count(addr) > 0; }
};
```

> **注意**：新 ChStream 架构的 CacheTLM 不调用 `createSubTransaction()`。子交易机制由 Legacy V2 模块的 `TLMModule` 基类提供。

### 4.3 MemoryTLM（终止型）

```cpp
// include/tlm/memory_tlm.hh
class MemoryTLM : public ChStreamModuleBase {
private:
    InputStreamAdapter<CacheReqBundle> req_in;
    OutputStreamAdapter<CacheRespBundle> resp_out;
    std::map<uint64_t, uint64_t> memory_;

public:
    void tick() override {
        if (!req_in.available()) return;

        auto req = req_in.read();
        uint64_t addr = req.address;

        // 内存访问
        if (req.is_write) {
            memory_[addr] = req.data;
        }

        // 创建响应，透传 transaction_id
        CacheRespBundle resp;
        resp.transaction_id = req.transaction_id;
        resp.data = memory_[addr];
        resp_out.write(resp);
    }
};
```

> **注意**：新 TLM 模块不调用 `TransactionTracker::complete_transaction()`。Legacy V2 模块（`modules_v2.hh` 中的 `MemoryV2`）通过 `TLMModule` 钩子调用 Tracker。

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
// include/core/sim_object.hh（第 45-53 行）
struct FragmentBuffer {
    uint64_t parent_id = 0;          // 父交易 ID（分片重组键）
    uint8_t fragment_total = 0;      // 总分片数
    std::map<uint8_t, Packet*> fragments;  // 分片包（按 fragment_id 索引）
    uint64_t first_arrival_time = 0;      // 首个分片到达时间

    bool is_complete() const { return fragment_total > 0 && fragments.size() == fragment_total; }
    bool has_fragment(uint8_t id) const { return fragments.count(id) > 0; }
};
```

> **注意**：`FragmentBuffer` 是结构体（非模板类），位于 `sim_object.hh`。`TLMModule` 通过 `fragment_buffers_`（`std::map<uint64_t, FragmentBuffer>`）管理重组。

---

## 7. 实际实现 vs 文档设计

| 维度 | ADR-X.7 设计 | 代码实际 | 一致？ |
|------|-------------|---------|--------|
| `TransactionAction` 枚举 | ✅ sim_object.hh | ✅ 文件作用域 | ✅ |
| `TransactionInfo` 结构体 | ✅ | ✅ + `is_root()`/`is_fragmented()` | ✅ |
| `onTransactionStart/Hop/End()` | `TLMModule` 派生类 | ✅ `TLMModule`，返回 `TransactionInfo` | ✅ |
| `createSubTransaction()` | `TLMModule`，局部静态 `atomic` | ✅ `g_sub_tid{20000}` 局部静态 | ✅ |
| `FragmentBuffer` | ✅ sim_object.hh 结构体 | ✅ `struct FragmentBuffer`（非模板类） | ✅ |
| `processWithFragmentation()` | ✅ §2.2 TLMModule 代码块中 | ✅ `bool` 返回值 | ✅ |
| `TransactionTracker::create_transaction()` | ✅ | ✅ | ✅ |
| `TransactionTracker::record_hop()` | ✅ | ✅ | ✅ |
| `TransactionTracker::complete_transaction()` | ✅ | ✅ + 父交易完成检查 | ✅ |
| `TransactionTracker::link_transactions()` | ✅ | ✅ | ✅ |
| `TransactionTracker::set_fragment_info()` | ✅ | ✅ | ✅ |
| `TransactionTracker::check_parent_completion()` | ✅ 在 complete_transaction 中调用 | ✅ 私有方法 | ✅ |
| `TransactionRecord::child_transactions` | ✅ | ✅ | ✅ |

---

## 8. 模块基类差异说明

> ⚠️ **重要说明**：实际 TLM 模块（CacheTLM/CrossbarTLM/MemoryTLM）继承自 `ChStreamModuleBase`，而非 `TLMModule`。`TLMModule` 定义了交易生命周期钩子，但当前这些钩子由 Legacy V2 模块（`modules_v2.hh` 中的 `CacheV2/CrossbarV2/MemoryV2`）使用。

| 模块类型 | 基类 | 交易钩子 | 使用场景 |
|---------|------|---------|---------|
| **Legacy V2 模块** | `SimObject` → `TLMModule` | 有（`onTransactionStart/Hop/End`） | `modules_v2.hh` 中的 V2 模块 |
| **ChStream TLM 模块** | `SimObject` → `ChStreamModuleBase` | 无（通过 StreamAdapter 自动 tick） | `include/tlm/` 中的新 TLM 模块 |
| **Legacy SimObject 模块** | `SimObject` | 无 | 其他传统模块 |

**实际类层次结构**：
```
SimObject
├── ChStreamModuleBase         ← 新 TLM 模块（CacheTLM/CrossbarTLM/MemoryTLM）
│   └── 所有 TLM 模块（通过 StreamAdapter 自动 tick）
└── TLMModule                 ← 存在但未被任何模块使用（Legacy V2 备用）
    └── CacheV2 / CrossbarV2 / MemoryV2（Legacy，modules_v2.hh）
```

**设计原则**：不是所有 SimObject 都需要交易生命周期钩子，将这些方法放在 `TLMModule` 上更合理。但当前实现中 `TLMModule` 是"预留"基类，新 ChStream 架构模块不继承它。

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
