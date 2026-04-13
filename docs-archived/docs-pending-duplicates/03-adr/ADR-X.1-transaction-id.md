# ADR-X.1: 事务追踪 ID 分配策略

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **影响**: v2.0 - 请求/响应匹配机制

---

## 问题

事务追踪 ID（`ch_uint<64>`）如何保证全局唯一性？谁负责分配 ID？

### 当前设计

- `CacheReqBundle.transaction_id: ch_uint<64>`
- `CacheRespBundle.transaction_id: ch_uint<64>`
- **假设**: ID 由上游模块分配，下游模块透传

---

## 选项对比

| 选项 | 设计 | 优点 | 缺点 |
|------|------|------|------|
| **A) 上游分配** | CPU/Initiator 分配 ID，下游透传 | 简单，无需协调 | ID 可能冲突（多上游） |
| **B) 框架分配** | `ModuleRegistry` 分配 ID，注入 Bundle | 全局唯一 | 需要修改 Bundle，耦合框架 |
| **C) 时间戳 + 本地序号** | `timestamp[32] + counter[32]` | 时间本地唯一性 | 跨节点可能冲突 |
| **D) 哈希分配** | `hash(src_id, dst_id, timestamp)` | 分布式唯一 | 哈希碰撞风险，性能开销 |

---

## 推荐方案

**A) 上游分配**

**理由**:
1. **简化设计**: Bundle 不携带分配逻辑，模块专注业务
2. **常见模式**: AXI/CHI 协议也由主设备分配 ID
3. **应用层控制**: CPU/Initiator 最清楚事务顺序
4. **避免耦合**: 框架不介入 ID 生成，保持模块独立性

---

## 实施建议

### 1. ID 分配责任

- **CPU/Initiator 模块**: 负责分配 `transaction_id`
- **Cache/Crossbar 模块**: 负责透传 `transaction_id`
- **Memory 模块**: 使用透传的 `transaction_id` 构建响应

### 2. ID 分配约束

```cpp
// 示例：CPU 模块分配 ID
class CPUSim : public SimObject {
private:
    uint64_t next_transaction_id_ = 0;
    
public:
    ch_stream<CacheReqBundle> req_out;
    
    void tick() override {
        CacheReqBundle req;
        req.transaction_id = next_transaction_id_++;  // 分配 ID
        req.address = ...;
        req.is_write = ...;
        req.data = ...;
        
        req_out.payload = req;
        req_out.valid = true;
    }
};
```

### 3. 多上游冲突处理

**场景**: 多个 CPU/Initiator 共享总线

**解决方案**:
- **方案 1**: `transaction_id = (node_id << 32) + local_id`
- **方案 2**: 每个上游分配器带唯一前缀 `node_id`
- **方案 3**: 使用仲裁器（Arbiter）串行化访问

---

## 需要确认的问题

| 问题 | 选项 |
|------|------|
| **Q1**: ID 分配策略？ | A) 上游分配 |
| **Q2**: ID 字段大小？ | A) `ch_uint<64>`（已确定） |
| **Q3**: 多上游冲突处理？ | ⏳ 待确认 |

---

**下一步**: 请老板确认 ADR-X.1 的实施细节
