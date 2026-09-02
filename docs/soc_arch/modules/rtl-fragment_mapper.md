# rtl-fragment_mapper 微架构文档

> **类别**: RTL > FragmentMapper · **状态**: ✅ 已实施 + 📋 v1.0 dGPU SoC 战略补充
> **状态**: ✅ 已实施
> **Header**: `include/rtl/fragment_mapper.hh`
> **注册**: **无**（基础设施类，**非模块**）
> **蓝图来源**: ADR-X.6 TransactionContext + ADR-X.13 多 extension API
> **首版 commit**: v2.1 RTL 子树（2026-06-06 附近）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - ADR: X.6 TransactionContext + X.13 多 extension API

---

## 1. 设计目标

`cpptlm::rtl::FragmentMapper` 是 **TLM Packet ↔ RTL Beat 映射层**——把 `Packet*` 提取为轻量级 POD `CacheReqBeatRTL` / `CacheRespBeatRTL`，**真值源**为 `TransactionContextExt`。**与 gem5 对位**: gem5 的 `Packet::data_ptr` + extension 机制（简化版）。

**核心特性**（来自 `fragment_mapper.hh:55-131`）：
- **薄映射函数**（**非 TLM 模块**，仅静态工具）
- 真值源：`TransactionContextExt`（来自 `include/ext/transaction_context_ext.hh`）
- 4 个静态方法：`serialize_req` / `serialize_beat_at` / `write_resp` / `beats_remaining`
- 2 个工具函数：`group_key` / `is_single_beat`
- 兼容 fallback：若 Packet 无 `TransactionContextExt`，用 `pkt->stream_id` 推断

**与 HybridCacheComponent 关系**：FragmentMapper 是 **TLM↔RTL 桥接的核心**——`HybridCacheWrapper` 在 `tick()` 中调用 FragmentMapper 完成 Packet↔beat 转换。

## 2. 架构概览

### 2.1 数据流

```
   TLM 端 (Packet*)                  RTL 端 (POD Beat)
   ──────────────                  ─────────────

   ┌────────────────────┐          ┌──────────────────────────┐
   │ Packet             │          │ CacheReqBeatRTL (POD)   │
   │  - payload         │          │  tid, parent_id, ...    │
   │  - stream_id       │          │  addr, data, strb       │
   │  - payload->get_   │          │  first, last             │
   │    extension<      │   ─►     │                          │
   │    Transaction     │  Map     │ (memcpy-friendly,        │
   │    ContextExt>()   │          │  32 bytes)               │
   │  - payload->get_   │          │                          │
   │    address()       │          │                          │
   │  - payload->get_   │          │                          │
   │    data_length()   │          │                          │
   └────────────────────┘          └──────────────────────────┘
```

### 2.2 真值源优先级

```
1. TransactionContextExt  (preferred)
   ↓ get_transaction_context(payload) → ext != nullptr
2. Fallback:  pkt->stream_id
   └─ 简单单拍，无 fragment 语义
```

## 3. 接口（Public API）

```cpp
namespace cpptlm::rtl {

// RTL-side beat POD
struct CacheReqBeatRTL {
    uint64_t tid;
    uint64_t parent_id;
    uint8_t  fragment_id;
    uint8_t  fragment_total;
    uint64_t addr;
    uint64_t data;
    uint8_t  strb;
    bool     first;
    bool     last;
};

struct CacheRespBeatRTL {
    uint64_t tid;
    uint64_t parent_id;
    uint8_t  fragment_id;
    uint8_t  fragment_total;
    uint64_t data;
    bool     hit;
    uint8_t  error_code;
    bool     first;
    bool     last;
};

class FragmentMapper {
public:
    // TLM → RTL beat
    static CacheReqBeatRTL serialize_req(Packet* pkt);
    static CacheReqBeatRTL serialize_beat_at(Packet* pkt, uint8_t beat_index);

    // RTL beat → TLM packet
    static void write_resp(Packet* resp_pkt, const CacheRespBeatRTL& beat);

    // 工具
    static uint8_t beats_remaining(const CacheReqBeatRTL& beat);
    static uint64_t group_key(const CacheReqBeatRTL& beat);
    static bool is_single_beat(const CacheReqBeatRTL& beat);
};
}
```

## 4. 行为流程

### 4.1 serialize_req（TLM → RTL）

```cpp
static CacheReqBeatRTL FragmentMapper::serialize_req(Packet* pkt) {
    CacheReqBeatRTL beat;
    if (!pkt || !pkt->payload) return beat;  // 默认 0 初始化

    const TransactionContextExt* ext = get_transaction_context(pkt->payload);

    if (ext) {
        beat.tid            = ext->transaction_id;
        beat.parent_id      = ext->parent_id;
        beat.fragment_id    = ext->fragment_id;
        beat.fragment_total = ext->fragment_total;
        beat.first          = ext->is_first_fragment();
        beat.last           = ext->is_last_fragment();
    } else {
        // Fallback: 用 pkt->stream_id 推断
        beat.tid            = pkt->stream_id;
        beat.parent_id      = 0;
        beat.fragment_id    = 0;
        beat.fragment_total = 1;
        beat.first          = true;
        beat.last           = true;
    }

    beat.addr = pkt->payload->get_address();

    if (pkt->payload->get_data_length() >= sizeof(uint64_t)) {
        std::memcpy(&beat.data, pkt->payload->get_data_ptr(), sizeof(uint64_t));
    } else if (pkt->payload->get_data_length() > 0) {
        std::memcpy(&beat.data, pkt->payload->get_data_ptr(),
                    pkt->payload->get_data_length());
    }
    beat.strb = 0xFF;  // 全字节选通（v0 简化）

    return beat;
}
```

### 4.2 serialize_beat_at（多拍切分）

```cpp
static CacheReqBeatRTL FragmentMapper::serialize_beat_at(
        Packet* pkt, uint8_t beat_index) {
    CacheReqBeatRTL beat = serialize_req(pkt);
    beat.fragment_id = beat_index;
    beat.first = (beat_index == 0);
    beat.last  = (beat_index + 1 >= beat.fragment_total);
    return beat;
}
```

**用例**：`HybridCacheComponent` 多拍 fragment 时，对同一 `Packet*` 切 N 个 beat，beat_index ∈ [0, fragment_total)。

### 4.3 write_resp（RTL → TLM）

```cpp
static void FragmentMapper::write_resp(
        Packet* resp_pkt, const CacheRespBeatRTL& beat) {
    if (!resp_pkt || !resp_pkt->payload) return;

    // 释放旧 extension (如果存在)
    resp_pkt->payload->template release_extension<TransactionContextExt>();

    // 写入新 extension
    auto* ext = new TransactionContextExt();
    ext->transaction_id = beat.tid;
    ext->parent_id      = beat.parent_id;
    ext->fragment_id    = beat.fragment_id;
    ext->fragment_total = beat.fragment_total;
    resp_pkt->payload->template set_extension<TransactionContextExt>(ext);

    resp_pkt->set_transaction_id(beat.tid);

    if (resp_pkt->payload->get_data_length() >= sizeof(uint64_t)) {
        std::memcpy(resp_pkt->payload->get_data_ptr(), &beat.data, sizeof(uint64_t));
    }
}
```

### 4.4 工具函数

```cpp
static uint8_t beats_remaining(const CacheReqBeatRTL& beat) {
    if (beat.fragment_total == 0) return 0;
    if (beat.fragment_id >= beat.fragment_total) return 0;
    return static_cast<uint8_t>(beat.fragment_total - beat.fragment_id - 1);
}

static uint64_t group_key(const CacheReqBeatRTL& beat) {
    return beat.parent_id != 0 ? beat.parent_id : beat.tid;
}

static bool is_single_beat(const CacheReqBeatRTL& beat) {
    return beat.fragment_total <= 1;
}
```

## 5. Bundle 字段使用

**无 Bundle 字段**——FragmentMapper 是**纯工具类**，不传输事务数据。

**真值源**：
- `Packet::payload->get_data_length()` / `get_data_ptr()` / `get_address()`（来自 `core/packet.hh`）
- `TransactionContextExt`（来自 `ext/transaction_context_ext.hh`，ADR-X.6 + ADR-X.13）

## 6. 统计

**无 StatGroup**（纯工具类）。

## 7. 蓝图（未来演进）

### 7.1 真实数据提取

- v0: `std::memcpy` 8 字节（限于 `data_length >= 8`）
- v2.2: 任意长度数据 + byte-enable (`strb`) 精确控制

### 7.2 多 beat 真实切分

- 当前 `serialize_beat_at` 单次切一个 beat
- v2.2: 批量切分 API（输入 `vector<beat>`）

### 7.3 错误状态传递

- v0: `error_code=0` 硬编码
- v2.2: 透传 RTL 组件的错误码

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **`strb=0xFF` 硬编码**——非真实 byte-enable | 中 | 中 | v2.2 按真实 data_length 设置 |
| R2 | **`write_resp` 释放旧 extension**——所有权语义 | 中 | 中 | `release_extension<>` API 已提供（ADR-X.13） |
| R3 | **Fallback 路径**——无 TransactionContextExt 时用 `pkt->stream_id` | 中 | 中 | v0 可接受；v2.2 强制要求 extension 存在 |
| R4 | **依赖 `Packet` / `TransactionContextExt`**——`core/packet.hh` 已**无**真实内存（v0 是 L1 桩） | 中 | 中 | v2.2 真实存储后修正 |
| R5 | **依赖 `ext/transaction_context_ext.hh`**——含 `tlm/tlm_stub.hh`（USE_SYSTEMC_STUB 依赖） | 低 | 低 | 已通过 `USE_SYSTEMC_STUB=ON` 桩处理 |
| R6 | **`error_code=0` 硬编码** | 中 | 中 | v2.2 透传 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| TLM → RTL 转换 | ✅ | `serialize_req` 真实实现 |
| RTL → TLM 转换 | ✅ | `write_resp` 真实实现 |
| 多拍切分 | ✅ | `serialize_beat_at` + `beats_remaining` |
| Fragment 语义 | ✅ | 依赖 `TransactionContextExt`（ADR-X.6/X.13） |
| 8 字节数据 | ✅ | `std::memcpy`（`data_length >= 8`） |
| **任意长度数据** | ⚠️ 限于 8 字节 | 见 R1 |

## 10. 修订历史

- **2026-06-06**: FragmentMapper 初版（POD beat + TransactionContext 真值源）
- **2026-06-07**: HybridCacheComponent 集成
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B2 批次）
