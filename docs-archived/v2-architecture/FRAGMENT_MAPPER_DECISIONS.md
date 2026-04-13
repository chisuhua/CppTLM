# FragmentMapper + Bridge/Mapper 架构决议

> **文档日期**: 2026-04-07  
> **会话**: CppTLM 架构讨论  
> **状态**: ✅ 已确认

---

## 1. 问题背景

在适配器命名讨论中，提出了两个关键问题：

1. **命名问题**：原生适配器（NativeStreamAdapter）和协议适配器（AXI4Adapter）是否有更好的名称？
2. **Transaction ID 传递问题**：RTL 模块内部如何从入口到出口传递 transaction_id？

---

## 2. 解决方案：分层命名架构

### 2.1 架构分层

```
┌─────────────────────────────────────────────────────────────┐
│  层级 1: Bridge（桥接层）— TLM ↔ CppHDL Stream/Flow    │
│  职责：valid/ready 握手，FIFO 缓冲，不分片               │
├─────────────────────────────────────────────────────────────┤
│  层级 2: Mapper（映射层）— Bundle ↔ Fragment/Protocol    │
│  职责：分片/重组，协议特定映射                           │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 适配器命名

| 适配器类型 | 建议名称 | 职责 |
|-----------|---------|------|
| TLM ↔ ch_stream<Bundle> | **StreamBridge** | valid/ready + FIFO |
| TLM ↔ ch_flow<Bundle> | **FlowBridge** | 单向传输 |
| Bundle ↔ ch_fragment | **FragmentMapper** | 分片/重组 |
| Bundle ↔ AXI4 | **AXI4Mapper** | AXI4 协议映射 + 分片 |
| Bundle ↔ CHI | **CHIMapper** | CHI 协议映射 + 分片 |
| Bundle ↔ TileLink | **TileLinkMapper** | TileLink 协议映射 + 分片 |

### 2.3 推荐理由

- ✅ **Bridge 层**：协议无关，通用逻辑
- ✅ **Mapper 层**：协议特定，分片逻辑
- ✅ **易于扩展**：新增协议只需加新 Mapper

---

## 3. Transaction ID 传递方案

### 3.1 核心挑战

`ch_stream<Bundle>` 接口本身**没有 transaction_id 字段**，RTL 模块需要追踪哪个响应对应哪个请求。

### 3.2 解决方案：扩展 Bundle 携带 transaction_id

```cpp
// TLMBundle 嵌入 transaction_id
struct TLMBundle {
    uint64_t transaction_id;  // ← 新增
    uint64_t flow_id;        // 流 ID（可选，用于 QoS）
    uint64_t address;         // 地址
    uint64_t length;         // 数据长度
    uint8_t  data[64];      // 数据负载（最大 64 字节）
    uint8_t  strb;           // 字节使能
    bool     write;           // 写使能
    uint8_t  priority;       // 优先级
};

// 分片版本（用于多拍传输）
using TLMBundleFragment = ch::ch_fragment<TLMBundle>;
```

### 3.3 数据流

```cpp
// StreamBridge: TLM → Bundle
void send_packet(Packet* pkt) {
    auto* ctx = get_transaction_context(pkt->payload);
    
    TLMBundle bundle;
    bundle.transaction_id = ctx->transaction_id;  // ← 嵌入
    bundle.address = pkt->payload->get_address();
    // ...
    stream_interface->send(bundle);
}

// RTL 模块内部：transaction_id 随数据流传递
class MyRTLModule : public ch::Component {
    ch_stream<TLMBundle> in;
    ch_stream<TLMBundle> out;
    
    void tick() {
        if (in.fire()) {
            // transaction_id 在 in.payload.transaction_id
            // 存入内部 FIFO/流水线
            fifo.push(in.payload);
        }
        if (out.fire()) {
            // transaction_id 从 FIFO 取出
            out.payload = fifo.front();
            // out.payload.transaction_id 自动携带
        }
    }
};

// StreamToTLMAdapter: Bundle → TLM
Packet* recv_bundle(TLMBundle bundle) {
    uint64_t tid = bundle.transaction_id;  // ← 提取
    // 创建响应 Packet，携带相同 tid
}
```

### 3.4 分片场景

```cpp
// FragmentMapper 分片
void fragment_and_send(TLMBundle bundle) {
    uint64_t tid = bundle.transaction_id;
    size_t total_beats = calculate_beats(bundle);
    
    for (size_t i = 0; i < total_beats; i++) {
        ch_fragment<TLMBundle> frag;
        frag.data_beat = bundle;  // 完整 Bundle（包含 tid）
        frag.first = (i == 0);
        frag.last = (i == total_beats - 1);
        
        // 每个 fragment 都包含完整的 transaction_id
        stream_out.send(frag);
    }
}

// RTL 模块接收分片
class MyRTLModule : public ch::Component {
    ch_stream<ch_fragment<TLMBundle>> in;
    
    void tick() {
        if (in.fire()) {
            uint64_t tid = in.payload.data_beat.transaction_id;
            bool is_first = in.payload.isFirst();
            bool is_last = in.payload.isLast();
            
            // 使用 tid 追踪这个交易的所有 fragment
            reassembly_buffer[tid].push_back(in.payload.data_beat);
            
            if (is_last) {
                // 重组完成，处理交易
                process_transaction(reassembly_buffer[tid]);
                reassembly_buffer.erase(tid);
            }
        }
    }
};
```

---

## 4. 适配器库最终结构

```
gemsc::adapter::
├── Bridge 层（协议无关）
│   ├── StreamBridge<T>          — TLM ↔ ch_stream<T>
│   ├── FlowBridge<T>            — TLM ↔ ch_flow<T>
│   ├── StreamToTLMBridge<T>     — ch_stream<T> → TLM
│   └── FlowToTLMBridge<T>       — ch_flow<T> → TLM
│
├── Mapper 层（分片/协议）
│   ├── FragmentMapper<T>        — Bundle ↔ ch_fragment<Bundle>
│   ├── AXI4Mapper<T>            — Bundle ↔ AXI4 信号
│   ├── CHIMapper<T>             — Bundle ↔ CHI 信号
│   └── TileLinkMapper<T>        — Bundle ↔ TileLink 信号
│
└── Bundle 类型
    ├── TLMBundle                — 嵌入 transaction_id
    └── TLMBundleFragment        — ch_fragment<TLMBundle>
```

---

## 5. 决策汇总

| 决策点 | 决策 | 理由 |
|--------|------|------|
| **适配器命名** | 分层命名（Bridge/Mapper） | 职责清晰，易于扩展 |
| **Bundle 设计** | 嵌入 transaction_id | 简单直接，随数据流传递 |
| **分片场景** | 每个 fragment 携带完整 Bundle | 任意 fragment 可提取 tid |
| **RTL 模块** | 无需额外追踪逻辑 | tid 随 payload 自然传递 |

---

## 6. 相关决策

本决议与以下已确认的 P0/P1 决策相关：

| 优先级 | 议题 | 决策 |
|--------|------|------|
| P0.1 | TransactionContext | TLM Extension 存储 |
| P1.3 | 适配器库设计 | Bridge 层 + Mapper 层，TLMBundle 嵌入 tid |

---

**状态**: ✅ 已确认  
**后续**: 通过 ACF-Workflow 生成正式 ADR 文档
