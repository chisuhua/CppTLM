# coherence-protocol 微架构文档

> **类别**: Coherence > Protocol
> **状态**: 🟡 规划中
> **Header**: (规划) `include/core/coherence_protocol.hh`
> **蓝图来源**: gem5 `src/mem/protocol/`（MOESI_AMD 状态机，slicc → C++ switch）
> **首版 commit**: 蓝图（来自调研 §2.6 + Phase 7.C）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.6
> - 邻接: [cache-protocol.md](./cache-protocol.md) (Cache 侧协议) | [coherence-bridge.md](./coherence-bridge.md) (跨域桥接) | [coherence-domain.md](./coherence-domain.md) (✅ 基础设施已实施)

---

## 1. 设计目标（蓝图）

`tlm::CoherenceProtocol` 是 CppTLM Phase 7.C 规划的 **协议无关抽象层**——定义 6 状态 MOESI 完整状态机、snoop probe/response 消息类型、协议转换占位。**与 gem5 对位**: `gem5::MOESI_AMD_Base-dir`（slicc 描述，CppTLM 用 C++ `switch` 表简化）。

**核心特征**：
- **6 状态 MOESI 完整状态机**（I/S/E/M/O/T）
- **协议无关 Bundle**（`SnoopProbe` / `SnoopResp` / `CoherenceMsg`）
- **多协议实现**（MOESI_AMD / MESI_GPU / MESI_Three_Level / Custom）
- **协议转换函数**（`translate(probe, from, to)`）
- **统计独立**（per-protocol miss rate / invalidation 计数）

> **与 [cache-protocol.md](./cache-protocol.md) 的关系**: `cache-protocol.md` 关注**单 cache 行的 6×6 状态转换**，本文档关注**协议抽象层**（多协议、消息类型、协议转换）。两者共同构成 Phase 7.C coherence 体系。

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│              CoherenceProtocol 抽象层                          │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  CoherenceProtocol (基类, abstract)               │     │
│  │    - handle_probe(probe) → SnoopResp             │     │
│  │    - handle_request(req, line) → Decision        │     │
│  │    - can_coexist(state_a, state_b) → bool        │     │
│  │    - translate(probe, from_proto, to_proto)       │     │
│  └──────────────────────────────────────────────────┘     │
│                          ↑ 实现                              │
│  ┌──────────────────────────────────────────────────┐     │
│  │  MoesiAmdProtocol                                 │     │
│  │    - 6 状态机 (I/S/E/M/O/T)                       │     │
│  │    - AMD 私有 O 态 (Owned)                         │     │
│  │    - T 态 (Transient)                             │     │
│  └──────────────────────────────────────────────────┘     │
│  ┌──────────────────────────────────────────────────┐     │
│  │  MesiGpuProtocol                                  │     │
│  │    - 4 状态机 (I/S/M/E)                          │     │
│  │    - GPU 简化版（无 O/T）                          │     │
│  └──────────────────────────────────────────────────┘     │
│  ┌──────────────────────────────────────────────────┐     │
│  │  MesiThreeLevelProtocol                           │     │
│  │    - 4 状态机 (I/S/M/E)                          │     │
│  │    - 三级 cache hierarchy（v0 不实施）            │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 6 状态 MOESI 含义

| 状态 | 全称 | 含义 | gem5 蓝图 |
|------|------|------|-----------|
| **I** | Invalid | 副本无效 | MOESI_AMD_State_I |
| **S** | Shared | 共享（多 cache 持有） | MOESI_AMD_State_S |
| **E** | Exclusive | 独占（单 cache 持有，未修改） | MOESI_AMD_State_E |
| **M** | Modified | 修改（单 cache 持有，已修改） | MOESI_AMD_State_M |
| **O** | Owned | 持有（单 cache 持有，已修改，但其他 cache 可有只读副本） | MOESI_AMD_State_O |
| **T** | Transient | 瞬态（中间状态，等待 snoop response） | MOESI_AMD_State_T |

### 2.2 6 状态对位

```
                  ┌── SnoopProbe ──►
                  │
  CoherenceMsg ──►│  ┌────────────────────────────────┐
  Request        │  │  CoherenceProtocol::handle_  │
                 │  │   request + line state        │
                 │  │                                │
                 ▼  ▼                                │
              ┌────────────────┐                      │
              │ 6 状态机       │  ◄── Cache Line State│
              │ I/S/E/M/O/T   │                      │
              │ (15 transitions)                     │
              └────────────────┘                      │
                                                     │
              ┌────────────────┐                      │
              │ SnoopResp      │ ──► back to CoherentXBar
              │ (ACK/NACK)     │                      │
              └────────────────┘                      │
```

## 3. 接口（规划）

```cpp
namespace tlm {

// === 状态枚举（与 cache-protocol.md 对齐） ===
enum class CoherenceState {
    I, S, E, M, O, T
};

// === Snoop probe 类型 ===
enum class SnoopType {
    INVALIDATE,        // 强制置 I
    DOWNGRADE_TO_S,    // 强制从 M/E → S
    READ_SHARED,       // 请求共享副本
    READ_EXCLUSIVE,    // 请求独占
};

// === Snoop response ===
enum class SnoopResp {
    ACK,               // 收到且已处理
    NACK,              // 拒绝（资源不足）
    DATA,              // 收到且提供数据
    DATA_AND_ACK,      // 提供数据 + 确认
};

// === 抽象基类 ===
class CoherenceProtocol {
public:
    virtual ~CoherenceProtocol() = default;

    // 处理 snoop probe
    virtual SnoopResp handle_probe(SnoopType type, CoherenceState current_state) const = 0;

    // 处理 cache 请求（决定状态转换）
    virtual CoherenceState handle_request(
        bool is_write, bool is_read,
        CoherenceState current_state, uint32_t num_sharers) const = 0;

    // 检查两个状态是否可共存
    virtual bool can_coexist(CoherenceState a, CoherenceState b) const = 0;

    // 协议转换（用于跨域桥接）
    virtual SnoopProbe translate(const SnoopProbe& probe,
                                 const CoherenceProtocol* from,
                                 const CoherenceProtocol* to) const = 0;

    // 协议名查询
    virtual std::string get_protocol_name() const = 0;
};

// === MOESI AMD 协议 ===
class MoesiAmdProtocol : public CoherenceProtocol {
public:
    SnoopResp handle_probe(SnoopType type, CoherenceState state) const override;
    CoherenceState handle_request(
        bool is_write, bool is_read,
        CoherenceState state, uint32_t num_sharers) const override;
    bool can_coexist(CoherenceState a, CoherenceState b) const override;
    SnoopProbe translate(const SnoopProbe& probe,
                         const CoherenceProtocol* from,
                         const CoherenceProtocol* to) const override;
    std::string get_protocol_name() const override { return "MOESI_AMD"; }
};

// === MESI GPU 协议（简化版） ===
class MesiGpuProtocol : public CoherenceProtocol {
    // GPU 专用，4 状态，无 O/T（v0 简化）
};

// === Bundle 类型 ===
struct SnoopProbe {
    uint64_t addr;
    uint64_t transaction_id;
    SnoopType type;
    std::string source_protocol;
    std::string target_protocol;
    uint32_t requestor_cache_id;
};

struct SnoopRespBundle {
    uint64_t addr;
    uint64_t transaction_id;
    SnoopResp resp;
    uint64_t data;  // 可选（如 DATA 类型）
    std::string source_protocol;
};

struct CoherenceMsg {
    uint64_t addr;
    uint64_t transaction_id;
    uint8_t msg_type;  // protocol-specific
    uint64_t data;
};

}  // namespace tlm
```

## 4. 6×6 状态转换表（与 cache-protocol.md 对齐）

| 当前状态 | 操作 | 新状态 | sharers 更新 | snoop probe 类型 |
|----------|------|--------|------------|---------------|
| **I** | read | **S** | +self | READ_SHARED |
| **I** | write | **M**（取独占）/ **T→M** | +self | READ_EXCLUSIVE |
| **I** | (no op) | I | — | — |
| **S** | read | S | — | — |
| **S** | write | **T→M** | -self 后 +self | INVALIDATE→all |
| **S** | snoop(INVALIDATE) | **I** | -self | — |
| **S** | snoop(DOWNGRADE_TO_S) | S | — | — |
| **S** | snoop(READ_SHARED) | S | +requester | — |
| **S** | snoop(READ_EXCLUSIVE) | **I** | -self | — |
| **E** | read | E | — | — |
| **E** | write | **M** | — | — |
| **E** | snoop(READ_SHARED) | **S** | +requester | — |
| **E** | snoop(READ_EXCLUSIVE) | **I** | -self | — |
| **E** | snoop(INVALIDATE) | I | -self | — |
| **M** | read | M (响应数据) | — | — |
| **M** | write | M | — | — |
| **M** | snoop(READ_SHARED) | **O** | +requester，data 转发 | — |
| **M** | snoop(READ_EXCLUSIVE) | **I** | -self，data 转发 | — |
| **M** | snoop(INVALIDATE) | I | -self，data 写回 | — |
| **O** | read | O (响应数据) | — | — |
| **O** | write | **M** | -其他，self 升 M | INVALIDATE→others |
| **O** | snoop(READ_SHARED) | O | +requester，data 转发 | — |
| **O** | snoop(READ_EXCLUSIVE) | **I** | -self，data 转发 | — |
| **O** | snoop(INVALIDATE) | I | -self，data 写回 | — |
| **T** | (等待 snoop response) | (snoop response 决定) | — | — |
| **T** | snoop_response(ACK) | 状态由原始 request 决定 | — | — |

## 5. 行为流程（规划）

### 5.1 handle_probe

```cpp
SnoopResp MoesiAmdProtocol::handle_probe(SnoopType type, CoherenceState state) const {
    switch (state) {
        case CoherenceState::I:
            return SnoopResp::ACK;  // 无副本，无需处理

        case CoherenceState::S:
            switch (type) {
                case SnoopType::INVALIDATE:
                case SnoopType::READ_EXCLUSIVE:
                    return SnoopResp::ACK;  // 即将置 I
                case SnoopType::DOWNGRADE_TO_S:
                case SnoopType::READ_SHARED:
                    return SnoopResp::ACK;
            }

        case CoherenceState::E:
            switch (type) {
                case SnoopType::READ_SHARED:
                case SnoopType::READ_EXCLUSIVE:
                case SnoopType::INVALIDATE:
                    return SnoopResp::ACK;  // 即将降级
                default:
                    return SnoopResp::NACK;
            }

        case CoherenceState::M:
        case CoherenceState::O:
            switch (type) {
                case SnoopType::READ_SHARED:
                    return SnoopResp::DATA_AND_ACK;  // 提供数据
                case SnoopType::READ_EXCLUSIVE:
                case SnoopType::INVALIDATE:
                    return SnoopResp::DATA_AND_ACK;  // 提供数据 + 失效
                default:
                    return SnoopResp::NACK;
            }

        case CoherenceState::T:
            // T 态应串行化
            return SnoopResp::NACK;
    }
    return SnoopResp::NACK;
}
```

### 5.2 协议转换示例

```cpp
SnoopProbe MoesiAmdProtocol::translate(const SnoopProbe& probe,
                                        const CoherenceProtocol* from,
                                        const CoherenceProtocol* to) const {
    // MOESI_AMD → MESI_GPU: 简化协议（去除 O/T）
    if (from->get_protocol_name() == "MESI_GPU" &&
        to->get_protocol_name() == "MOESI_AMD") {
        SnoopProbe translated = probe;
        // MESI_GPU 的 INVALIDATE 翻译为 MOESI_AMD 的 INVALIDATE
        // MESI_GPU 的 READ_SHARED 翻译为 MOESI_AMD 的 READ_SHARED
        // （消息类型 1:1 映射）
        translated.source_protocol = "MESI_GPU";
        translated.target_protocol = "MOESI_AMD";
        return translated;
    }
    // MOESI_AMD → MOESI_AMD: 透传
    return probe;
}
```

## 6. Bundle 字段使用

| 字段 | CoherenceProtocol 使用 |
|------|---------------|
| `addr` | **关键**——状态查找 |
| `transaction_id` | **关键**——snoop response 匹配 |
| `type` | **关键**——snoop type |
| `source_protocol` / `target_protocol` | 协议转换路由 |
| `requestor_cache_id` | sharer 更新 |
| `data` | snoop response with data |

## 7. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/protocol/MOESI_AMD-headers.sm` | `MoesiAmdProtocol` | 简化：v0 6 状态 + 4 snoop type |
| `src/mem/protocol/MOESI_AMD-cache.sm` | (cache-protocol.md 实施) | 关注单 cache 行 |
| `src/mem/protocol/MOESI_AMD-dir.sm` | (CoherenceDomain 实施) | 关注目录协议 |
| `src/mem/protocol/Message.sm` | `SnoopProbe` / `SnoopResp` | 简化：v0 4 种 snoop type |
| `src/mem/protocol/TransitionTable.sm` | `handle_request` 6×6 表 | 简化：v0 switch 表 |
| `src/mem/ruby/slicc_interface/AbstractCacheEntry.sm` | (cache-protocol.md 实施) | 关注 cache line 状态 |
| `src/mem/protocol/Protocol.rb` | `MoesiAmdProtocol::get_protocol_name` | 同语义 |

## 8. 实施路径

### 8.1 Phase 7.C 步骤

1. 新建 `include/core/coherence_protocol.hh`（~300 行）
2. 实现 `CoherenceProtocol` 抽象基类
3. 实现 `MoesiAmdProtocol`（6 状态 + 4 snoop type）
4. 实现 `MesiGpuProtocol`（4 状态简化版）
5. 实现 `translate()` 协议转换占位
6. 与 `CoherenceDomain` 集成
7. 与 `CacheTLM` 集成（cache-protocol.md 实施）
8. 加 Catch2 测试：`test/test_coherence_protocol.cc`

### 8.2 Phase 7.D 步骤

1. GPU 域使用 `MesiGpuProtocol`
2. APU 整体使用 `MoesiAmdProtocol`
3. 跨域通过 `coherence-bridge.md` + `translate()` 桥接

### 8.3 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[coherence]"` 全部通过
- [ ] 6 状态机正确性（与 gem5 reference 对照）
- [ ] 协议转换正确性
- [ ] MOESI ↔ MESI_GPU 转换路径验证

### 8.4 估计工作量

- 设计: 1 周
- 基础版实施（MOESI_AMD + MESI_GPU）: 2 周
- 与 CoherenceDomain/CacheTLM 集成: 1 周
- 测试: 1 周
- **总计: 5 周**

## 9. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **T 态死锁**——等待 snoop response 超时 | 中 | 高 | T 态超时强制完成（默认 1000 cycle） |
| R2 | **O 态数据竞争**——多 reader 持有 O 副本 + writer | 中 | 中 | O→M 时强制 invalidate 所有 reader |
| R3 | **协议转换语义丢失**——MESI_GPU 无 O/T，跨域不兼容 | 中 | 中 | MOESI_AMD↔MESI_GPU 转换表严格实现 |
| R4 | **协议扩展性**——新增协议需大量重复代码 | 中 | 中 | 模板元编程优化（v0 简化） |
| R5 | **snoop probe 风暴**——大 cache 数量下 broadcast 慢 | 中 | 中 | 依赖 SnoopFilter（snoop_filter.md） |
| R6 | **状态机 cycle 检测**——配置错误导致死循环 | 低 | 高 | 单元测试覆盖所有 6×6 转换 |
| R7 | **snoop response 丢失**——probe 已发但 response 未回 | 中 | 高 | snoop_inflight_ 超时强制完成 |
| R8 | **跨协议性能差异**——MOESI 比 MESI 慢 | 中 | 低 | 文档明确：复杂度 vs 一致性强度权衡 |

## 10. 决策点

### D1 默认协议

- **Q**: 默认 MoesiAmdProtocol 还是 MesiGpuProtocol？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: MoesiAmdProtocol（CPU + GPU 通用）
- **依赖**: Phase 7.D GPU 域需求

### D2 T 态超时

- **Q**: T 态等待 snoop response 的默认超时？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 1000 cycle
- **依赖**: NoC 端到端延迟

### D3 协议转换策略

- **Q**: 跨协议桥接时如何处理 O/T 语义？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: MESI_GPU → MOESI_AMD：M → M，E → E，S → S；O 模拟为 M
- **依赖**: coherence-bridge.md 实施

### D4 snoop probe 类型

- **Q**: 完整 6 种 snoop type 还是简化 4 种？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 4 种基础（INVALIDATE / DOWNGRADE_TO_S / READ_SHARED / READ_EXCLUSIVE）
- **依赖**: gem5 SnoopReqType 简化

## 11. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.6）
- **2026-06-12**: B3 批次设计 — 6×6 状态转换表 + 蓝图对齐 + 风险列表
- **Phase 7.C (未来)**: MoesiAmdProtocol 实施 + CacheTLM 集成
- **Phase 7.D (未来)**: MesiGpuProtocol 实施 + GPU 域集成
- **Phase 7 备选 dGPU (未来)**: 跨协议转换
