# ADR-X.1: 事务追踪 ID 分配策略

> **版本**: 2.0
> **日期**: 2026-04-09
> **状态**: ✅ 已实施
> **影响**: v2.0 - 请求/响应匹配机制

---

## 1. 核心决策

**采用 `Packet::set_transaction_id()` 上游分配 + 双层同步方案**。

| 决策 | 说明 |
|------|------|
| **ID 来源** | 上游模块调用 `set_transaction_id()` 分配 |
| **存储位置** | `Packet::stream_id` + `TransactionContextExt::transaction_id`（双层同步） |
| **传播方式** | `Packet::get_transaction_id()` 优先 Extension，回退 stream_id |
| **分片支持** | `parent_id` + `fragment_id` + `fragment_total`（已实现） |
| **多上游冲突** | 暂无处理机制（单仿真环境不受影响） |

---

## 2. 实际实现

### 2.1 ID 分配与同步机制

```cpp
// include/core/packet.hh

// 获取 transaction_id：优先 Extension，回退 stream_id
uint64_t get_transaction_id() const {
    if (payload) {
        TransactionContextExt* ext = nullptr;
        payload->get_extension(ext);
        if (ext) return ext->transaction_id;
    }
    return stream_id;
}

// 设置 transaction_id：同步更新 stream_id + Extension（自动创建）
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
```

### 2.2 模块示例：CPU 分配 ID

```cpp
// include/tlm/cpu_tlm.hh
class CPUTLM : public ChStreamModuleBase {
private:
    uint64_t next_txn_id_ = 0;

public:
    ch_out<CacheReqBundle> req_out;

    void tick() override {
        CacheReqBundle req;
        req.transaction_id = next_txn_id_++;  // 上游分配
        req.address = ...;
        req.is_write = ...;
        req.data = ...;

        req_out.payload = req;
        req_out.valid = true;
    }
};
```

### 2.3 模块示例：Cache 透传 ID

```cpp
// include/tlm/cache_tlm.hh
class CacheTLM : public ChStreamModuleBase {
private:
    ch_in<CacheReqBundle> req_in;
    ch_out<CacheReqBundle> req_out;
    ch_out<CacheRespBundle> resp_out;

    void processRequest() {
        auto req = req_in.payload;

        if (cacheHit(req.address)) {
            // Cache Hit：直接响应，透传 transaction_id
            CacheRespBundle resp;
            resp.transaction_id = req.transaction_id;  // 透传
            resp.data = cacheLine;
            resp_out.payload = resp;
            resp_out.valid = true;
        } else {
            // Cache Miss：透传到下游
            req_out.payload = req;  // transaction_id 保持
            req_out.valid = true;
        }
    }
};
```

---

## 3. TransactionContextExt 字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `transaction_id` | `uint64_t` | 事务 ID（与 Packet::stream_id 同步） |
| `parent_id` | `uint64_t` | 父事务 ID（0 表示无父事务） |
| `fragment_id` | `uint8_t` | 分片 ID |
| `fragment_total` | `uint8_t` | 总分片数（1 表示不分片） |
| `create_timestamp` | `uint64_t` | 创建时间戳 |
| `source_module` | `std::string` | 源模块名称 |
| `type` | `std::string` | 事务类型：READ/WRITE/ATOMIC |
| `priority` | `uint8_t` | QoS 优先级 |
| `trace_log` | `vector<TraceEntry>` | 追踪日志 |

### 辅助方法

```cpp
// is_root() / is_fragmented()
bool is_root() const { return parent_id == 0 && fragment_total == 1; }
bool is_fragmented() const { return fragment_total > 1; }
bool is_first_fragment() const { return fragment_id == 0; }
bool is_last_fragment() const { return fragment_id == fragment_total - 1; }
std::string get_group_key() const { return source_module + ":" + std::to_string(parent_id ?: transaction_id); }
```

---

## 4. 与 Bundle 层的关系

实际系统存在**两层 ID**：

```
┌─────────────────────────────────────────────────────────────┐
│ Bundle 层（ChStream 通信）                                   │
│ CacheReqBundle.transaction_id (ch_uint<64>)                │
│  └─ 由上游模块分配，下游模块透传                             │
├─────────────────────────────────────────────────────────────┤
│ Packet 层（TLM 传输）                                       │
│ Packet::stream_id + TransactionContextExt                 │
│  └─ 通过 StreamAdapter 同步                                 │
└─────────────────────────────────────────────────────────────┘
```

**转换关系**：StreamAdapter 在 Bundle ↔ Packet 转换时保持 `transaction_id` 不变。

---

## 5. 多上游冲突处理（待补充）

| 场景 | 当前状态 | 建议方案 |
|------|---------|---------|
| 多个 CPU/Initiator 共享总线 | 无处理 | `transaction_id = (node_id << 32) + local_id` |

**注意**：当前为单仿真环境，此问题暂不影响。

---

## 6. 实际实现 vs 文档设计

| 组件 | 文档旧设计 | 实际实现 | 状态 |
|------|-----------|---------|------|
| **ID 字段** | `CacheReqBundle.transaction_id: ch_uint<64>` | `Packet::stream_id` + `TransactionContextExt` | ✅ 已实现 |
| **分配机制** | 上游分配，下游透传 | 任意模块调用 `set_transaction_id()` | ✅ 已实现 |
| **多上游冲突** | `(node_id << 32) + local_id` | 无 | ⏳ 待补充 |
| **分片支持** | 未提及 | `parent_id` + `fragment_id` + `fragment_total` | ✅ 已实现 |
| **Extension 自动创建** | 未提及 | `set_transaction_id()` 自动创建 | ✅ 已实现 |

---

## 7. 相关文档

| 文档 | 位置 |
|------|------|
| TransactionContextExt | `include/ext/transaction_context_ext.hh` |
| Packet 扩展方法 | `include/core/packet.hh` |
| TLMModule 基类 | `include/core/tlm_module.hh` |
| CPU 模块示例 | `include/tlm/cpu_tlm.hh` |
| Cache 模块示例 | `include/tlm/cache_tlm.hh` |

---

**状态**: ✅ 已实施<br>
**最后更新**: 2026-05-01
