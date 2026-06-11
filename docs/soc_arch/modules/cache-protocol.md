# cache-protocol 微架构文档

> **类别**: cache > protocol
> **状态**: 🟡 规划中（Phase 7.C — 最高风险）
> **Header**: (规划) `include/tlm/cache/cache_protocol.hh`（抽象基类 + MOESI 实现）
> **蓝图来源**: gem5 `src/mem/ruby/protocol/MOESI_AMD_Base-{msg,dir,CorePair}.sm` + `src/mem/cache/base_cache.hh`
> **首版 commit**: 蓝图（来自调研 §2.6 + spec §3.3）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

---

## 1. 设计目标（蓝图）

`cache-protocol` 是 Phase 7.C 引入的 **CacheLine 状态机抽象**——实现 6×6 状态转换表（含 MOESI），是 CppTLM v0 → v2.2 coherence 集成的**最关键模块**（roadmap §Phase 7.C 标记"最高风险"）。

**核心抽象**：
- **`CoherenceProtocol` 抽象基类**：暴露 `transition(state, event) → new_state` 接口
- **`MOESIProtocol` 实现**：6 状态 × 6 事件 = 36 种转换
- **`SnoopFilter` 集成**（可选 v2.2，Phase 7.C 后期）
- **`CoherenceDomain` 桥接**（来自 [`coherence-domain.md`](./coherence-domain.md)）

**与 gem5 对位**: `gem5::MOESI_AMD_Base` 协议（slicc 自动生成 ~3000 行代码，CppTLM 用 C++ `switch` 表简化）。

## 2. 架构概览（规划）

### 2.1 MOESI 6 状态

| 状态 | 全称 | 含义 | 拥有者 |
|------|------|------|--------|
| **I** (Invalid) | Invalid | 无有效数据 | 无 |
| **S** (Shared) | Shared | 只读副本，可能多节点拥有 | 多 |
| **E** (Exclusive) | Exclusive | 独占只读，未修改 | 1 |
| **M** (Modified) | Modified | 独占已修改（脏） | 1 |
| **O** (Owned) | Owned | 共享但脏（负责响应其他节点的读） | 1 |
| **T** (Transient) | Transient | 中间状态（in flight 请求） | 1 |

### 2.2 6 事件

| 事件 | 含义 | 触发 |
|------|------|------|
| **`PrRd`** | Processor Read | CPU/GPU 发起读 |
| **`PrWr`** | Processor Write | CPU/GPU 发起写 |
| **`SnoopInv`** | Snoop Invalidate | 来自 CoherenceDomain |
| **`SnoopRd`** | Snoop Read | 来自 CoherenceDomain（其他节点读） |
| **`SnoopRdX`** | Snoop Read Exclusive | 来自 CoherenceDomain（其他节点写） |
| **`MemData`** | Memory Data | MemoryTLM 响应（v0 桩 data=0xDEADBEEF） |

### 2.3 6×6 状态转换表

| 当前状态 | PrRd | PrWr | SnoopInv | SnoopRd | SnoopRdX | MemData |
|----------|------|------|----------|---------|----------|---------|
| **I** | → S / → I（miss） | → M / → I（miss） | → I | → I | → I | → S / → M |
| **S** | → S | → M（upgrade） | → I | → S | → M（downgrade） | — |
| **E** | → E | → M | → I | → S（downgrade） | → I | — |
| **M** | → M | → M | → I（writeback） | → S（writeback） | → I（writeback） | — |
| **O** | → O | → M | → I | → O | → I（writeback） | — |
| **T** | → S / → M | → S / → M | → I | → T | → T | → S / → M |

## 3. 接口（规划）

```cpp
namespace tlm {

enum class CoherenceState {
    INVALID = 0,  SHARED = 1,  EXCLUSIVE = 2,
    MODIFIED = 3, OWNED = 4,  TRANSIENT = 5
};

enum class CoherenceEvent {
    PR_RD = 0,   PR_WR = 1,
    SNOOP_INV = 2, SNOOP_RD = 3, SNOOP_RDX = 4,
    MEM_DATA = 5
};

struct Sharers {
    std::bitset<64> mask;  // 最多 64 个节点
    bool contains(uint32_t node_id) const { return mask.test(node_id); }
    void add(uint32_t node_id) { mask.set(node_id); }
    void remove(uint32_t node_id) { mask.reset(node_id); }
    uint32_t count() const { return mask.count(); }
};

class CoherenceProtocol {
public:
    virtual ~CoherenceProtocol() = default;

    // 状态转换
    virtual CoherenceState transition(
        CoherenceState current,
        CoherenceEvent event,
        const Sharers& current_sharers,
        uint32_t local_node_id,
        uint32_t requesting_node_id,
        bool is_dirty_writeback = false) = 0;

    // 协议名称
    virtual const char* name() const = 0;
};

class MOESIProtocol : public CoherenceProtocol {
public:
    const char* name() const override { return "MOESI"; }

    CoherenceState transition(
        CoherenceState current,
        CoherenceEvent event,
        const Sharers& current_sharers,
        uint32_t local_node_id,
        uint32_t requesting_node_id,
        bool is_dirty_writeback = false) override;
};

class MESIProtocol : public CoherenceProtocol {
public:
    const char* name() const override { return "MESI"; }
    // 实现 4 状态（I/S/E/M），O 状态映射为 M
    CoherenceState transition(...) override;
};

// 工厂
std::unique_ptr<CoherenceProtocol> make_protocol(const std::string& name) {
    if (name == "MOESI") return std::make_unique<MOESIProtocol>();
    if (name == "MESI")  return std::make_unique<MESIProtocol>();
    return nullptr;
}
}
```

## 4. 行为流程（规划）

### 4.1 MOESI 状态转换核心 switch 表

```cpp
CoherenceState MOESIProtocol::transition(
    CoherenceState s, CoherenceEvent e,
    const Sharers& sharers, uint32_t local, uint32_t requester,
    bool is_dirty) {
    // === PrRd: Processor Read ===
    if (e == CoherenceEvent::PR_RD) {
        if (s == INVALID)    return SHARED;     // miss → cache fill
        if (s == SHARED)    return SHARED;     // hit (shared)
        if (s == EXCLUSIVE) return EXCLUSIVE;  // hit (exclusive)
        if (s == MODIFIED)  return MODIFIED;   // hit (dirty)
        if (s == OWNED)     return OWNED;      // hit (owned, shared dirty)
        if (s == TRANSIENT) return TRANSIENT;  // 已在 in-flight
    }

    // === PrWr: Processor Write ===
    if (e == CoherenceEvent::PR_WR) {
        if (s == INVALID)    return MODIFIED;   // miss + write-allocate
        if (s == SHARED)    return MODIFIED;   // upgrade
        if (s == EXCLUSIVE) return MODIFIED;   // hit → write
        if (s == MODIFIED)  return MODIFIED;   // hit (write)
        if (s == OWNED)     return MODIFIED;   // upgrade
        if (s == TRANSIENT) return MODIFIED;   // upgrade (delay)
    }

    // === SnoopInv: Invalidate (from CoherenceDomain) ===
    if (e == CoherenceEvent::SNOOP_INV) {
        return INVALID;  // 任何状态都失效
    }

    // === SnoopRd: Read by other (from CoherenceDomain) ===
    if (e == CoherenceEvent::SNOOP_RD) {
        if (s == INVALID)    return INVALID;
        if (s == SHARED)    return SHARED;     // 仍共享
        if (s == EXCLUSIVE) return SHARED;     // downgrade (S)
        if (s == MODIFIED)  return OWNED;      // writeback + 共享
        if (s == OWNED)     return OWNED;      // 仍 owned
        if (s == TRANSIENT) return TRANSIENT;  // 延迟
    }

    // === SnoopRdX: Read Exclusive (Write) by other ===
    if (e == CoherenceEvent::SNOOP_RDX) {
        if (s == INVALID)    return INVALID;
        if (s == SHARED)    return INVALID;    // 被逐出（请求者变 M）
        if (s == EXCLUSIVE) return INVALID;    // 被逐出
        if (s == MODIFIED)  return INVALID;    // writeback + 失效
        if (s == OWNED)     return INVALID;    // writeback + 失效
        if (s == TRANSIENT) return TRANSIENT;
    }

    // === MemData: Memory 响应（cache fill）===
    if (e == CoherenceEvent::MEM_DATA) {
        if (s == INVALID)    return is_dirty ? MODIFIED : SHARED;
        if (s == TRANSIENT) return is_dirty ? MODIFIED : SHARED;
        return s;  // 已有数据
    }

    return s;  // 默认保持
}
```

### 4.2 与 CacheTLM 集成

```cpp
class CoherentCache : public CacheTLM {  // 升级现有 CacheTLM
private:
    std::unique_ptr<CoherenceProtocol> protocol_;
    Sharers sharers_;  // bitmask<64>

public:
    void on_config_loaded() override {
        const json& cfg = get_config();
        std::string proto_name = cfg.value("protocol", "MOESI");
        protocol_ = make_protocol(proto_name);
        // ...
    }

    void tick() override {
        // 1. 响应消费（同 CacheTLM）
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();
            uint64_t addr = req.address.read();
            bool is_write = req.is_write.read();
            bool is_hit = check_tag(addr);

            // 协议状态转换
            CoherenceEvent ev = is_write ? CoherenceEvent::PR_WR : CoherenceEvent::PR_RD;
            CoherenceState new_state = protocol_->transition(
                current_state(addr), ev, sharers_, local_node_id_, requester_node_id_);
            update_state(addr, new_state);

            // 构造响应（同 CacheTLM）
            // ...
        }
    }
};
```

## 5. Bundle 字段使用（规划）

| 字段 | 协议集成使用 |
|------|------------|
| `address` | 跨域 snoop fanout 路由 |
| `kernel_id` | 跨域一致性标识（Phase 7.D GPU 域） |
| `is_write` | 决定 `PR_RD` vs `PR_WR` 事件 |
| 其他 | 透传 |

## 6. 蓝图对齐

- gem5 `src/mem/ruby/protocol/MOESI_AMD_Base-msg.sm`（消息类型）
- gem5 `src/mem/ruby/protocol/MOESI_AMD_Base-dir.sm`（directory 状态机）
- gem5 `src/mem/ruby/protocol/MOESI_AMD_Base-CorePair.sm`（L1+L2 状态机）
- 调研 §2.6 Coherence + §4 Phase 2 风险

## 7. 实施路径

### 7.1 Phase 7.C 步骤

1. 新建 `include/tlm/cache/cache_protocol.hh`（~200 行）
2. 实现 `MOESIProtocol::transition` switch 表（6×6 = 36 种转换）
3. 实现 `MESIProtocol`（4 状态，O 映射为 M）
4. 写 `make_protocol` 工厂
5. 改造 `CacheTLM` → `CoherentCache` 继承 `CacheTLM` + 嵌入 `protocol_`
6. `CoherenceDomain` 集成：snoop callback
7. 6×6 状态机单元测试
8. 加 5+ 现有 cache 测试（不应回归）
9. **触发升级条件**：若 5+ 回归 → 回 Phase 7.B write-through bypass

### 7.2 验收标准

- [ ] 编译通过
- [ ] 36 种状态转换 100% 单测覆盖
- [ ] `cpptlm_tests "[phase7]"` 全部通过
- [ ] `cpptlm --config configs/coherent_cache_test.json` 端到端
- [ ] `docs_sync_check.sh --strict` 通过
- [ ] 零 TODO/FIXME/XXX in new files

### 7.3 估计工作量

- 设计: 1-2 周（6×6 状态机 + 死锁/livelock 分析）
- 实施: 2-3 周
- 测试: 1-2 周（36 转换 + 集成）
- **总计: 4-7 周**（**最高风险**子阶段）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **死锁**——状态机可能进入不可达状态 | 中 | **高** | 单元测试覆盖所有 36 转换 + 死锁检测器 |
| R2 | **livelock**——状态频繁切换但请求永远未完成 | 中 | **高** | MSHR 容量限制 + timeout 检测 |
| R3 | **5+ 现有 cache 测试回归** | 高 | 高 | 触发升级条件：回 Phase 7.B write-through bypass |
| R4 | **Sharer bitmask 容量**——>64 节点失败 | 低 | 中 | v0 仅 64；v2.2 改 `std::unordered_set<uint32_t>` |
| R5 | **跨域 GPU 一致性**（Phase 7.D TCC 集成） | 高 | 中 | Phase 7.D 同步实施 |
| R6 | **snoop fanout 风暴**——`broadcast_snoop` 可能 O(N) 触发 | 中 | 中 | SnoopFilter 减负（Phase 7.C 后期） |
| R7 | **与 v0 CacheTLM 兼容性** | 高 | 中 | CoherentCache 继承 CacheTLM，保持 `req_in_/resp_out_` 端口契约 |
| R8 | **MSI 协议变体**（MESIF / CHI） | 低 | 低 | v0 仅 MOESI/MESI；CHI 留 v2.2+ |

## 9. 设计决策点

### D1 协议默认

- **Q**: 默认协议是 MOESI 还是 MESI？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: MOESI（与 gem5 APU 形态对齐）
- **依赖**: 与 GPU_VIPER 协议一致性

### D2 T (Transient) 状态细节

- **Q**: T 状态持续时间？何时转回 S/M？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: T 状态在 `MEM_DATA` 到达时退出；超时检测（v2.2+）
- **依赖**: MSHR 集成

### D3 SnoopFilter 集成

- **Q**: 何时引入 SnoopFilter？v0 不引入？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: v0 不引入（单 CPU 域场景不必要）；Phase 7.D 引入（GPU 域）
- **依赖**: gem5 SnoopFilter 蓝图

### D4 CoherenceDomain 集成方式

- **Q**: CacheTLM 如何发现所属 CoherenceDomain？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: ModuleFactory 创建 CacheTLM 时通过 `CoherenceDomain::get_domain("apu_domain")` 查找
- **依赖**: ModuleFactory 改造

### D5 协议扩展性

- **Q**: 是否预留其他协议（CHI / TileLink / MESIF）？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 抽象基类支持（工厂模式），v0 仅实现 MOESI/MESI

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自 spec §3.3 + 调研 §2.6）
- **2026-06-11**: B3 批次设计 — 6×6 状态转换表 + 蓝图对齐 + R1-R8 风险
- **Phase 7.C (未来)**: 实施 MOESI/MESI + CacheTLM 改造
- **Phase 7.D (未来)**: TCC 协议集成
- **Phase 7.F+ (未来)**: CHI / TileLink / SnoopFilter 真实实现
