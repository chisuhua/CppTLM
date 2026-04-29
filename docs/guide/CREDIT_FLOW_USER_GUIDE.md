# Credit Flow Control User Guide

## 1. 概念

Credit-based Flow Control（基于信用的流量控制）是一种背压机制，用于协调 NoC（Network-on-Chip）中上下游路由器的数据流。

```
上游 Router                          下游 Router
┌──────────────┐                   ┌──────────────┐
│ input_buffer │←─── flit ─────────│ downstream   │
│   (N flits)  │                   │  credits     │
└──────┬───────┘                   └──────┬───────┘
       │ credit return                          │
       └──────────── credit ───────────────────┘
```

**核心思想**：
- 下游路由器的 input_buffer 深度 = 初始 credit 数量
- 上游每发送一个 flit，消耗一个下游 credit
- 下游每消费一个 flit，向上游返回一个 credit
- credit 耗尽时上游停止发送，形成背压

## 2. 配置

在 TGMS JSON 拓扑配置中启用 credit flow control：

```json
{
  "topology": {
    "type": "mesh",
    "dimensions": [4, 4]
  },
  "routers": {
    "credit_flow": true,
    "buffer_depth": 8,
    "credit_return_latency": 1,
    "credit_timeout": 0
  }
}
```

### 参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `credit_flow` | bool | true | 是否启用 credit flow control |
| `buffer_depth` | int | 8 | 输入缓冲区深度 = 初始 credit 数 |
| `credit_return_latency` | int | 1 | Credit 返回延迟（与链路延迟相同）|
| `credit_timeout` | int | 0 | 死锁检测超时，0 表示禁用 |

## 3. 工作流程

### 3.1 Credit 循环

```
1. 初始化：
   downstream_credits_[port][vc] = BUFFER_DEPTH (例如 8)

2. 发送 flit（上游 Router A）：
   a. 检查 has_credit(out_port, out_vc) == true?
   b. 是 → 发送 flit 到下游，consume_credit() 将 credit 减 1
   c. 否 → 阻塞，等待 credit 返回

3. 消费 flit（下游 Router B）：
   a. ST 阶段：flit 从 input_buffer 移出
   b. 调用 return_credit_to_upstream(in_port, vc)
   c. credit 进入 pending_credit_returns_ 队列

4. 返回 credit（延迟后）：
   a. 延迟 credit_return_latency 周期
   b. 调用 send_credit_to_upstream(reverse_port, vc)
   c. 通过 BidirectionalPortAdapter 发送 credit 信号

5. 接收 credit（上游 Router A）：
   a. resp_in 收到 credit 信号
   b. 调用 receive_credit(in_port, vc)
   c. downstream_credits_[in_port][vc]++，恢复 credit
```

### 3.2 时序图

```
周期 0:  Router A                    Router B
        ──────                        ──────
        has_credit(A→B) = 8           
                                      
        send flit #1                  
        consume_credit → 7            
                                      
        send flit #2                  
        consume_credit → 6            
                                      
        send flit #3                  
        consume_credit → 5            

周期 1:                          ST 阶段消费 flit #1
                                 return_credit_to_upstream()
                                 credit 入队 (延迟 1 周期)

周期 2:                          ST 阶段消费 flit #2
                                 return_credit_to_upstream()
                                 credit 入队

周期 3:  ←── credit return ──────── send_credit_to_upstream()
        receive_credit() → 6           
        has_credit(A→B) = 6 恢复        ST 阶段消费 flit #3
```

## 4. API 参考

### 4.1 RouterTLM

```cpp
// 检查是否有可用 credit
bool has_credit(unsigned out_port, unsigned vc) const;

// 消耗下游 credit（发送 flit 时调用）
void consume_credit(unsigned out_port, unsigned vc);

// 接收来自下游的 credit 返回
void receive_credit(unsigned in_port, unsigned vc);

// 触发 credit 返回到上游（ST 阶段 flit 消费时调用）
void return_credit_to_upstream(unsigned in_port, unsigned vc);

// 发送 credit 信号到反向端口
void send_credit_to_upstream(unsigned reverse_port, unsigned vc);
```

### 4.2 BidirectionalPortAdapter

```cpp
// 发送 credit 信号到指定端口
void send_credit(unsigned port, unsigned vc);
```

## 5. 示例配置

### 5.1 简单 2x2 Mesh

```json
{
  "name": "simple_mesh_2x2",
  "description": "2x2 mesh with credit flow control",
  "topology": {
    "type": "mesh",
    "dimensions": [2, 2]
  },
  "routers": {
    "type": "RouterTLM",
    "buffer_depth": 4,
    "num_vcs": 2,
    "credit_flow": true,
    "credit_return_latency": 1,
    "credit_timeout": 0
  },
  "links": {
    "type": "LinkTLM",
    "latency": 1
  },
  "connections": [
    {"from": "cpu0", "to": "router00", "port": 0},
    {"from": "router00", "to": "router01", "port": 1},
    {"from": "router00", "to": "router10", "port": 2},
    {"from": "router01", "to": "mem0", "port": 3},
    {"from": "router10", "to": "cpu1", "port": 0},
    {"from": "router11", "to": "mem1", "port": 3}
  ]
}
```

### 5.2 禁用 Credit Flow

如需禁用 credit-based flow control（使用其他流控机制），设置：

```json
{
  "routers": {
    "credit_flow": false
  }
}
```

## 6. 调试

### 6.1 日志

启用 DEBUG_PRINT 宏查看 credit 流动：

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DDEBUG_PRINT=ON ..
```

关键日志关键词：
- `[CREDIT RETURN]` — credit 返回触发
- `[CREDIT SIGNAL]` — credit 信号发送
- `[CREDIT ERROR]` — credit 处理异常

### 6.2 统计指标

| 指标 | 说明 |
|------|------|
| `credits_consumed` | 累计消耗 credit 数 |
| `credits_returned` | 累计返回 credit 数 |
| `credit_blocked_cycles` | 因无 credit 阻塞的周期数 |
| `credit_return_latency` | Credit 返回延迟分布 |

## 7. 限制与注意事项

1. **Credit 超时安全网**：当 `credit_timeout > 0` 时，如果某 VC 超过指定周期未收到 credit，自动重置为 BUFFER_DEPTH，防止死锁
2. **Credit 返回延迟**：默认与链路延迟相同（1 周期），可通过 `credit_return_latency` 调整
3. **与 LinkTLM 配合**：LinkTLM 也有独立的 credit_queue_，用于处理链路层的 credit 延迟

## 8. 相关文档

- [TGMS Specification](../architecture/10-tgms-specifications.md)
- [Metrics Framework](../architecture/08-metrics-collection-framework-v2.2.md)
- [RouterTLM Implementation](../../include/tlm/router_tlm.hh)
- [BidirectionalPortAdapter](../../include/framework/bidirectional_port_adapter.hh)
