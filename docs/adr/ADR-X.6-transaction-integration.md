# ADR-X.6: TransactionContext 与现有 Packet/Extension 整合方案

> **版本**: 2.0
> **日期**: 2026-04-09
> **状态**: ✅ 已实施
> **影响**: v2.0 - 事务追踪架构设计

---

## 1. 核心决策

**保留现有机制 + 增强 TransactionContext**，采用 Extension 双层同步方案。

| 决策 | 说明 |
|------|------|
| **保留 `Packet::stream_id`** | 作为 fallback transaction_id |
| **保留 `Packet::original_req`** | 响应匹配机制不变 |
| **保留 `Packet::route_path`** | 用于调试追踪 |
| **添加 `TransactionContextExt`** | TLM Extension 存储完整上下文 |
| **同步机制** | `get_transaction_id()` 优先 Extension，回退 stream_id |

---

## 2. 实际实现：TransactionContextExt

### 2.1 字段定义

```cpp
// include/ext/transaction_context_ext.hh
struct TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    // 核心字段
    uint64_t transaction_id;      // 事务 ID（与 Packet::stream_id 同步）
    uint64_t parent_id;           // 父事务 ID（0 表示无父事务）
    uint8_t  fragment_id;         // 分片 ID（0 表示不分片）
    uint8_t  fragment_total;      // 总分片数（1 表示不分片）

    // 调试字段
    uint64_t create_timestamp;    // 创建时间戳
    std::string source_module;   // 源模块名称
    std::string type;             // 事务类型：READ/WRITE/ATOMIC
    uint8_t  priority;            // QoS 优先级

    // 追踪日志
    struct TraceEntry {
        std::string module;
        uint64_t timestamp;
        uint64_t latency;
        std::string event;
    };
    std::vector<TraceEntry> trace_log;

    // TLM Extension 必需方法
    tlm_extension* clone() const override {
        return new TransactionContextExt(*this);
    }

    void copy_from(tlm_extension const& e) override {
        auto& ext = static_cast<const TransactionContextExt&>(e);
        transaction_id = ext.transaction_id;
        // ... 其他字段复制
    }

    // 辅助方法
    bool is_root() const { return parent_id == 0 && fragment_total == 1; }
    bool is_fragmented() const { return fragment_total > 1; }
    bool is_first_fragment() const { return fragment_id == 0; }
    bool is_last_fragment() const { return fragment_id == fragment_total - 1; }
    // 分片重组键：parent_id 存在时使用父 ID，否则使用 transaction_id
    uint64_t get_group_key() const { return parent_id != 0 ? parent_id : transaction_id; }
};
```

### 2.2 便捷函数

```cpp
// 获取 TransactionContextExt
inline TransactionContextExt* get_transaction_context(tlm_generic_payload* p) {
    TransactionContextExt* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

// 设置 TransactionContext
inline void set_transaction_context(tlm_generic_payload* p, const TransactionContextExt& src) {
    TransactionContextExt* ext = new TransactionContextExt(src);
    p->set_extension(ext);
}

// 创建完整 TransactionContext（同时设置 transaction_id）
inline TransactionContextExt* create_transaction_context(tlm_generic_payload* p, uint64_t tid) {
    TransactionContextExt* ext = new TransactionContextExt();
    ext->transaction_id = tid;
    ext->parent_id = 0;
    ext->fragment_id = 0;
    ext->fragment_total = 1;
    p->set_extension(ext);
    return ext;
}
```

---

## 3. 实际实现：Packet 扩展

### 3.1 Packet 类整合

```cpp
// include/core/packet.hh
class Packet {
public:
    tlm::tlm_generic_payload* payload;

    // 流控/事务标识
    uint64_t stream_id = 0;
    uint64_t seq_num = 0;
    CmdType cmd;
    PacketType type;

    // 时间统计
    uint64_t src_cycle;
    uint64_t dst_cycle;

    // 请求 - 响应关联
    Packet* original_req = nullptr;
    std::vector<Packet*> dependents;

    // 路由信息
    std::vector<std::string> route_path;
    int hop_count = 0;
    uint8_t priority = 0;
    uint64_t flow_id = 0;
    int vc_id = 0;

    // ========== 事务 ID 方法 ==========
    uint64_t get_transaction_id() const {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) return ext->transaction_id;
        }
        return stream_id;
    }

    void set_transaction_id(uint64_t tid) {
        stream_id = tid;
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (!ext) {
                ext = new TransactionContextExt();
                payload->set_extension(ext);
            }
            ext->transaction_id = tid;
        }
    }

    void add_trace(const std::string& module, uint64_t timestamp, uint64_t latency, const std::string& event) {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) {
                ext->trace_log.push_back({module, timestamp, latency, event});
            }
        }
    }
};
```

---

## 4. 实际实现 vs 文档设计

### 4.1 Packet 类字段对比

| 字段 | ADR-X.6 设计 | 代码实际 | 一致？ |
|------|-------------|---------|--------|
| `Packet::stream_id` | ✅ 保留 | ✅ `uint64_t stream_id = 0` | ✅ |
| `Packet::original_req` | ✅ 保留 | ✅ `Packet* original_req = nullptr` | ✅ |
| `Packet::route_path` | ✅ 保留 | ✅ `std::vector<std::string> route_path` | ✅ |
| `get_transaction_id()` | 优先 Extension，回退 stream_id | 完全一致 | ✅ |
| `set_transaction_id()` | 同步更新 stream_id + Extension | 一致 + 自动创建 Extension | ✅ 代码更好 |
| `add_trace()` | ✅ | ✅ | ✅ |
| `PacketPool` 清理 Extension | ✅ acquire/release 时清理 | ⚠️ `reset()` 不清理 Extension | ⚠️ 部分不一致 |

### 4.2 TransactionContextExt 字段对比

| 字段 | 设计 | 实现 | 一致？ |
|------|------|------|--------|
| `transaction_id` | ✅ | ✅ `uint64_t` | ✅ |
| `parent_id` | ✅ | ✅ `uint64_t` | ✅ |
| `fragment_id` | ✅ | ✅ `uint8_t` | ✅ |
| `fragment_total` | ✅ | ✅ `uint8_t` | ✅ |
| `create_timestamp` | ✅ | ✅ `uint64_t` | ✅ |
| `source_module` | ✅ | ✅ `std::string` | ✅ |
| `type` | ✅ | ✅ `std::string` | ✅ |
| `priority` | ✅ | ✅ `uint8_t` | ✅ |
| `trace_log` | ✅ | ✅ `std::vector<TraceEntry>` | ✅ |
| `clone()` / `copy_from()` | ✅ | ✅ | ✅ |
| `is_root()` | ✅ | ✅ | ✅ |
| `is_fragmented()` | ✅ | ✅ | ✅ |
| `is_first_fragment()` | 未提及 | ✅ 新增 | ✅ |
| `is_last_fragment()` | 未提及 | ✅ 新增 | ✅ |
| `get_group_key()` | ✅ 与 ADR-X.1 一致 | ✅ `uint64_t`（分片重组键） | ✅ |
| `reset()` | 未提及 | ✅ 新增 | ✅ |

### 4.3 便捷函数对比

| 函数 | 设计 | 实现 | 一致？ |
|------|------|------|--------|
| `get_transaction_context()` | ✅ | ✅ 含 const 重载 | ✅ |
| `set_transaction_context()` | ✅ | ✅ | ✅ |
| `create_transaction_context()` | 未提及 | ✅ 新增 | ✅ |

### 4.4 代码超越文档的部分

| 特性 | 说明 |
|------|------|
| **const 重载** | `get_transaction_context()` 同时提供 const 和非 const 版本 |
| **自动创建 Extension** | `set_transaction_id()` 在 Extension 不存在时自动创建 |
| **分片辅助方法** | `is_first_fragment()` / `is_last_fragment()`（新增），`get_group_key()` 已同步至 ADR-X.1 |
| **TLMModule 子交易** | `std::atomic<uint64_t>` 线程安全计数器 |

---

## 5. 已知差异

### 5.1 PacketPool Extension 清理

| 方面 | ADR-X.6 设计 | 代码实际 |
|------|-------------|---------|
| `acquire()` | 清理所有 Extension | ⚠️ 仅 `reset()` payload，不清理 Extension |
| `release()` | 清理所有 Extension | ⚠️ 同上 |

**影响**：当前实现由 `create_transaction_context()` 在需要时自动创建，覆盖旧 Extension。长期可能需要改进。

### 5.2 record_hop / link_transactions 同步

文档设计的 `record_hop()` 和 `link_transactions()` 应同步更新 Extension 的 trace_log，但当前代码中这些方法标记为"暂不实现"（`(void)event;`）。这是 v2.1 的待实现功能。

### 5.3 粒度控制标志未生效

| 方面 | ADR-X.6 设计 | 代码实际 |
|------|-------------|---------|
| `enable_coarse_grained()` | 启用粗粒度追踪 | ⚠️ 标志已存储但逻辑中从不检查 |
| `enable_fine_grained()` | 启用细粒度追踪 | ⚠️ 同上 |

**影响**：`create_transaction()` / `complete_transaction()` 等方法始终执行完整追踪，粒度设置不生效。这是设计预留，v2.1 可能实现或移除。

---

## 6. TransactionTracker（框架层单例）

```cpp
// include/framework/transaction_tracker.hh
class TransactionTracker {
private:
    std::map<uint64_t, TransactionRecord> transactions_;
    std::map<uint64_t, std::vector<uint64_t>> parent_child_map_;
    uint64_t global_timestamp_ = 0;

public:
    static TransactionTracker& instance();

    uint64_t create_transaction(tlm_generic_payload* payload,
                                 const std::string& source,
                                 const std::string& type);
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency, const std::string& event);
    void complete_transaction(uint64_t tid);
    void link_transactions(uint64_t parent_id, uint64_t child_id);
    void set_fragment_info(uint64_t tid, uint8_t fragment_id, uint8_t fragment_total);
    const TransactionRecord* get_transaction(uint64_t tid) const;
    std::vector<uint64_t> get_children(uint64_t parent_id) const;
    std::vector<uint64_t> get_active_transactions() const;
    void advance_time(uint64_t delta);
    void enable_coarse_grained(bool enable);
    void enable_fine_grained(bool enable);
    void reset_for_testing();
};
```

---

## 7. 相关文档

| 文档 | 位置 |
|------|------|
| TransactionContextExt | `include/ext/transaction_context_ext.hh` |
| Packet 定义 | `include/core/packet.hh` |
| TransactionTracker | `include/framework/transaction_tracker.hh` |
| TLMModule | `include/core/tlm_module.hh` |
| Bundle 定义 | `include/bundles/cache_bundles_tlm.hh` |

---

**状态**: ✅ 已实施<br>
**最后更新**: 2026-05-01<br>
**超越设计**: const 重载、自动创建 Extension、分片辅助方法
