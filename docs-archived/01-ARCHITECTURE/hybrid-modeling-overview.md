# 混合建模架构概览

**版本**: v1.0 | **日期**: 2026-04-07  
**整合来源**: v2 设计文档 + 多层次混合仿真提案

---

## 1. 核心愿景

> **一次建模，多粒度仿真**

在同一仿真环境中自由组合：
- **事务级（TLM）** — 快速性能探索，毫秒级仿真速度
- **周期精确级（RTL）** — 精确时序验证，通过 CppHDL 实现

---

## 2. 核心问题

```
┌─────────────────────────────────────────────────────────────┐
│  GemSc (TLM)                    CppHDL (RTL)                │
│  ┌─────────────────┐            ┌─────────────────┐         │
│  │ Packet* (指针)  │  ───────→  │ ch_stream<T>    │         │
│  │ 高层逻辑 (毫秒) │            │ 低层精确 (周期) │         │
│  │ 反压 (bool)     │            │ valid/ready     │         │
│  └─────────────────┘            └─────────────────┘         │
│                                                             │
│  核心不兼容：                                                │
│  1. 数据格式：Packet* vs Bundle<T>                          │
│  2. 时间粒度：毫秒级 vs 周期级                              │
│  3. 流控协议：bool 返回 vs valid/ready 握手                 │
│  4. 内存管理：指针所有权 vs 值语义                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 解决方案：五维适配层

```
┌─────────────────────────────────────────────────────────────┐
│                    五维适配层                                │
├─────────────────────────────────────────────────────────────┤
│  1️⃣  流控协议转换                                            │
│      TLM send() ↔ valid/ready 握手                          │
│      背压传播：RTL ready=0 → TLM trySend()=false            │
├─────────────────────────────────────────────────────────────┤
│  2️⃣  时空映射                                                │
│      TLM 事务 → HW 多拍 beat 序列                            │
│      全局虚拟时间 (GVT) 统一 TLM 事件与 RTL 周期             │
├─────────────────────────────────────────────────────────────┤
│  3️⃣  零拷贝内存                                              │
│      MemoryProxy 直接访问物理内存池                         │
│      大数据传输避免复制 (10-100x 性能提升)                  │
├─────────────────────────────────────────────────────────────┤
│  4️⃣  事务追踪                                                │
│      TransactionContext 端到端穿透                          │
│      transaction_id 关联 TLM 与 RTL 事件                     │
├─────────────────────────────────────────────────────────────┤
│  5️⃣  配置与发现                                              │
│      JSON 参数化适配器行为                                  │
│      自动类型推导与模块注册                                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. 架构分层

```
┌─────────────────────────────────────────────────────────────┐
│  应用层                                                      │
│  流量生成器 | Trace 回放 | 性能仪表盘 | 置信度报告           │
├─────────────────────────────────────────────────────────────┤
│  模块接口层                                                  │
│  Port<T> | 请求/响应端口 | 指标接口 | 配置接口               │
├─────────────────────────────────────────────────────────────┤
│  实现层                                                      │
│  TLM 微模块 (快速近似) | RTL 微模块 (CppHDL 周期精确)       │
│  impl_type 配置：tlm / rtl / compare / shadow               │
├─────────────────────────────────────────────────────────────┤
│  适配层                                                      │
│  TLMToStreamAdapter | TLMToFlowAdapter | ProtocolAdapter    │
│  ReorderBuffer | 时间归一化器 | MemoryProxy                 │
├─────────────────────────────────────────────────────────────┤
│  仿真内核层                                                  │
│  多时钟调度器 | 全局虚拟时间 | 统一事件队列 | 物理内存池     │
├─────────────────────────────────────────────────────────────┤
│  CppHDL 集成层                                               │
│  HybridComponentWrapper | ch::Stream/ch::Flow 集成          │
│  CppHDL 仿真器桥接 | 标准组件库                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. 核心组件

### 5.1 TransactionContext

交易上下文对象，伴随每个交易穿越所有模块边界。

```cpp
struct TransactionContext {
    uint64_t transaction_id;      // 唯一 ID
    uint64_t parent_id;           // 父交易 ID（子交易用）
    uint8_t  fragment_id;         // 片段序号
    uint8_t  fragment_total;      // 总片段数
    uint64_t create_timestamp;    // 创建时间 (GVT)
    std::string source_port;      // 来源标识
    enum Type { READ, WRITE, ATOMIC } type;
    uint8_t  priority;            // QoS 优先级
    std::vector<TraceEntry> trace_log;  // 追踪日志
};
```

**传播规则**:
| 模块类型 | 行为 |
|---------|------|
| **透传型** | 原样转发 Context，仅追加路由延迟到 trace_log |
| **转换型** | 创建子交易，分配 sub_transaction_id，通过 parent_id 关联 |
| **终止型** | 标记交易完成，记录最终延迟和状态 |

---

### 5.2 Port<T> 泛型端口

新一代端口模板，替代 SimplePort。

```cpp
template <typename T>
class Port {
public:
    bool trySend(const T& data);        // 非阻塞，false=背压
    std::optional<T> tryRecv();         // 非阻塞，nullopt=无数据
    
    // 生命周期钩子（可扩展）
    virtual void preSend(const T& data) {}
    virtual void postSend(const T& data) {}
    virtual void preRecv() {}
    virtual void postRecv(const T& data) {}
};
```

**向后兼容**: `PacketPort<T>` 包装现有 SimplePort，现有代码零修改。

---

### 5.3 TLMToStreamAdapter

核心适配器，打通 TLM → RTL 数据流。

```
┌──────────────────────────────────────────────────────────────┐
│                    TLMToStreamAdapter                        │
│  ┌────────────┐    ┌────────────┐    ┌────────────┐         │
│  │ TLM 输入   │ →  │ FIFO 缓冲   │ →  │ valid/ready│ → RTL  │
│  │ Packet<T>  │    │ (可配置深度)│    │ 状态机     │         │
│  └────────────┘    └────────────┘    └────────────┘         │
│       ↑                                       ↓              │
│  trySend()                               ch_stream<T>         │
│  返回 false=背压                          valid/ready          │
└──────────────────────────────────────────────────────────────┘
```

**状态机**:
- **IDLE** — 等待 TLM 数据
- **TRANSMITTING** — 正在通过 valid/ready 发送
- **BACKPRESSURED** — RTL ready=0，等待背压释放

---

### 5.4 ProtocolAdapter

总线协议适配器，将通用协议子集映射到具体总线。

| 协议 | Adapter | 关键映射 |
|------|---------|---------|
| **AXI4** | `AXI4Adapter` | transaction_id → AxID, RREADY/BREADY → ready |
| **CHI** | `CHIAdapter` | transaction_id → TxnID, CompAck → ready, Retry → 重传 |
| **TileLink** | `TileLinkAdapter` | transaction_id → D 通道 param, NACK → 重试 |

**通用协议子集**:
```
addr       : Master → Slave
data       : 双向
valid      : Master → Slave
ready      : Slave → Master
last       : Master → Slave (最后一拍)
transaction_id : 双向 (跨层关联 ID)
resp_status    : Slave → Master
```

---

### 5.5 双并行实现策略

每个模块支持四种实现模式：

| impl_type | 行为 | 用途 |
|-----------|------|------|
| **tlm** | 使用 TLM 微模块实现 | 快速仿真，近似时序 |
| **rtl** | 使用 RTL (CppHDL) 实现 | 周期精确，仿真较慢 |
| **compare** | 并行运行两种实现 | 功能等价性验证 |
| **shadow** | RTL 影子模式运行 | 记录 RTL 行为，不影响系统 |

**时间归一化**:
- 全局虚拟时间 (GVT) 统一 TLM 事件与 RTL 周期
- 按 transaction_id 匹配 TLM 与 RTL 事件
- 生成延迟/吞吐量 A/B 对比报告

---

## 6. 数据流示例

### CPU → Cache → Memory 完整流程

```
1. CPU (TLM) 创建 ReadRequest
   TransactionContext: TID=0x0001, source=cpu_0.out, timestamp=0

2. CPU 通过 Port<ReadRequest> 发送到 TLMToStreamAdapter

3. Adapter 转换为 CppHDL Bundle 信号
   valid=1, ready=1, transaction_id=0x0001

4. AXI4 ProtocolAdapter 映射到 AXI4 协议
   ARADDR, ARVALID, ARID=0x0001

5. Cache RTL (CppHDL) 接收请求
   命中：1 周期处理，生成相同 TID 的响应
   未命中：创建子交易 (parent_id=0x0001, sub_id=0x0002) 发往 L2

6. L2 总线 (TLM) 通过 RTLToTLMApdapter 接收子交易
   转发到 PhysicalMemory

7. PhysicalMemory (TLM) 终止交易
   响应沿链路返回，每个模块向 trace_log 追加延迟

8. CPU 收到 ReadResponse, TID=0x0001
   trace_log: cpu(0) → adapter(2) → cache_hit(1) → 返回路径
```

---

## 7. 设计原则

| 原则 | 说明 |
|------|------|
| **交易穿透** | TransactionContext 必须端到端穿透所有模块边界 |
| **接口不变性** | 模块级交易接口在不同抽象层级间保持一致 |
| **协议无关性** | 支持多总线协议 (AXI/CHI/TileLink) |
| **双并行实现** | TLM/RTL 并行，通过 transaction_id 关联 |
| **渐进式过渡** | 支持混合运行模式，增量验证 |

---

## 8. 待办事项

| 组件 | 状态 | 优先级 |
|------|------|--------|
| TransactionContext 实现 | 📋 设计中 | P1 |
| Port<T> 模板实现 | 📋 设计中 | P1 |
| TLMToStreamAdapter 实现 | 📋 设计中 | P2 |
| ProtocolAdapter (AXI4) | 📋 设计中 | P2 |
| 时间归一化层 | 📋 设计中 | P3 |
| 双并行实现框架 | 📋 设计中 | P3 |

---

## 9. 参考文档

| 文档 | 位置 |
|------|------|
| 交易上下文详细设计 | `transaction-context.md` |
| 适配器 API 设计 | `adapter-design.md` |
| 协议适配器规范 | `protocol-adapters.md` |
| 历史设计文档 (v1/v2) | `docs/05-LEGACY/` |

---

**下一步**: 阅读详细设计文档或参与架构讨论
