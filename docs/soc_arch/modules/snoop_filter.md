# snoop_filter 微架构文档

> **类别**: Interconnect > SnoopFilter
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/interconnect/snoop_filter_tlm.hh`
> **蓝图来源**: gem5 `src/mem/snoop_filter.hh`（cache 一致性追踪 + snoop 减负）
> **首版 commit**: 蓝图（来自调研 §2.4 + Phase 7.C）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4
> - 邻接: [coherent_xbar.md](./coherent_xbar.md) (CoherentXBarTLM 调用方) | [cache-protocol.md](./cache-protocol.md) (MOESI 协议)

---

## 1. 设计目标（蓝图）

`tlm::SnoopFilterTLM` 是 CppTLM Phase 7.C 规划的 **snoop 过滤器**——记录哪些 cache 持有某个地址的副本，从而避免不必要的 snoop 广播。**与 gem5 对位**: `gem5::SnoopFilter`（~700 行，SnoopFilter 抽象 + 多实现）。

**核心特征**：
- **追踪 sharer 列表**（per-address set of cache_ids）
- **响应 `get_snoop_targets(addr)` 查询**
- **维护 cache line 状态**（I/S/E/M/O/T + sharers bitmask）
- **支持 2 种实现**（`SnoopFilterCache` 全追踪 + `SnoopFilterInvalidator` 失效追踪）
- **与 CoherentXBarTLM 联动**（作为 shared_ptr 注入）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                SnoopFilterTLM 单体                            │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  filter_table_                                     │     │
│  │    - entry: { addr, state, sharers_bitmask }     │     │
│  │    - hash(addr) → entry index                    │     │
│  │    - 默认 4-way set associative                  │     │
│  │    - LRU 替换（与 cache-replacement 共享策略）    │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  snoop_response_decision_                         │     │
│  │    - on snoop probe: lookup(addr) → set<sharers> │     │
│  │    - 输出: 哪些 cache_id 需要收到 snoop           │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 与 gem5 SnoopFilter 关系

| gem5 类 | CppTLM 对应 | 差异 |
|---------|------------|------|
| `SnoopFilter` (基类) | `tlm::SnoopFilterTLM` (基类) | 简化：v0 仅 2 种实现 |
| `SnoopFilterCache` | `tlm::SnoopFilterCacheTLM` | 完整追踪 sharers |
| `SnoopFilterInvalidator` | `tlm::SnoopFilterInvalidatorTLM` | 仅追踪 invalidator |

### 2.2 端口表

> SnoopFilter **无外部端口**——它是被动查询表，由 `CoherentXBarTLM` 通过 `get_snoop_targets()` 同步查询。

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  SnoopFilterTLM 内部                         │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  SnoopFilterTLM (基类，abstract)                  │     │
│  │    - virtual get_snoop_targets(addr)             │     │
│  │    - virtual update_sharer(addr, cache_id, op)   │     │
│  │    - virtual invalidate(addr)                    │     │
│  └──────────────────────────────────────────────────┘     │
│                          ↑ 继承                              │
│  ┌──────────────────────────────────────────────────┐     │
│  │  SnoopFilterCacheTLM (完整追踪版)                  │     │
│  │    - filter_table_: vector<FilterEntry>           │     │
│  │    - num_sets_/num_ways_                          │     │
│  │    - LRU eviction                                 │     │
│  └──────────────────────────────────────────────────┘     │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  SnoopFilterInvalidatorTLM (失效追踪版)            │     │
│  │    - 仅追踪被 invalidator 标记的地址              │     │
│  │    - 更小 footprint，更高 eviction 概率            │     │
│  └──────────────────────────────────────────────────┘     │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {

// === 抽象基类 ===
class SnoopFilterTLM {
public:
    virtual ~SnoopFilterTLM() = default;

    // 核心查询：哪些 cache_id 需要收到 addr 的 snoop？
    virtual std::set<uint32_t> get_snoop_targets(uint64_t addr) const = 0;

    // 更新 sharer 列表
    virtual void update_sharer(uint64_t addr, uint32_t cache_id,
                               SnoopOp op) = 0;  // ADD/REMOVE

    // 失效（地址被踢出 cache 层级）
    virtual void invalidate(uint64_t addr) = 0;

    // 统计查询
    virtual size_t get_table_size() const = 0;
};

// === 完整追踪版 ===
class SnoopFilterCacheTLM : public SnoopFilterTLM {
public:
    static constexpr uint32_t DEFAULT_NUM_SETS = 1024;
    static constexpr uint32_t DEFAULT_NUM_WAYS = 4;

    SnoopFilterCacheTLM(uint32_t num_sets = DEFAULT_NUM_SETS,
                        uint32_t num_ways = DEFAULT_NUM_WAYS,
                        uint32_t max_caches = 16);

    std::set<uint32_t> get_snoop_targets(uint64_t addr) const override;
    void update_sharer(uint64_t addr, uint32_t cache_id, SnoopOp op) override;
    void invalidate(uint64_t addr) override;
    size_t get_table_size() const override { return filter_table_.size(); }

private:
    struct FilterEntry {
        uint64_t addr;
        std::bitset<16> sharers;  // max 16 caches
        CoherenceState state;
        uint64_t lru_counter;
    };

    std::vector<std::vector<FilterEntry>> filter_table_;
    uint32_t num_sets_;
    uint32_t num_ways_;
    uint64_t global_lru_counter_;
};

// === 失效追踪版 ===
class SnoopFilterInvalidatorTLM : public SnoopFilterTLM {
public:
    explicit SnoopFilterInvalidatorTLM(uint32_t max_entries = 256);

    std::set<uint32_t> get_snoop_targets(uint64_t addr) const override;
    void update_sharer(uint64_t addr, uint32_t cache_id, SnoopOp op) override;
    void invalidate(uint64_t addr) override;
    size_t get_table_size() const override { return invalidator_set_.size(); }

private:
    std::unordered_map<uint64_t, uint32_t> invalidator_set_;  // addr → invalidator cache_id
    uint32_t max_entries_;
};

}  // namespace tlm
```

## 4. 行为流程（规划）

### 4.1 SnoopFilterCacheTLM::get_snoop_targets

```cpp
std::set<uint32_t> SnoopFilterCacheTLM::get_snoop_targets(uint64_t addr) const {
    auto it = lookup(addr);
    if (it == filter_table_.end()) {
        return {};  // 未追踪，无 sharer
    }

    std::set<uint32_t> targets;
    for (uint32_t i = 0; i < max_caches_; ++i) {
        if (it->sharers.test(i)) {
            targets.insert(i);
        }
    }
    return targets;
}
```

### 4.2 SnoopFilterCacheTLM::update_sharer

```cpp
void SnoopFilterCacheTLM::update_sharer(uint64_t addr, uint32_t cache_id,
                                        SnoopOp op) {
    auto& set = filter_table_[hash(addr) % num_sets_];

    // 查找现有 entry
    auto it = std::find_if(set.begin(), set.end(),
        [&](const FilterEntry& e) { return e.addr == addr; });

    if (it == set.end()) {
        // 新建 entry
        if (set.size() >= num_ways_) {
            // LRU 替换
            auto lru_it = std::min_element(set.begin(), set.end(),
                [](const FilterEntry& a, const FilterEntry& b) {
                    return a.lru_counter < b.lru_counter;
                });
            set.erase(lru_it);
        }
        FilterEntry new_entry;
        new_entry.addr = addr;
        new_entry.sharers.reset();
        new_entry.state = MOESI_I;
        new_entry.lru_counter = ++global_lru_counter_;
        set.push_back(new_entry);
        it = set.end() - 1;
    }

    // 更新 sharers
    if (op == SnoopOp::ADD) {
        it->sharers.set(cache_id);
        // 状态更新（与 cache-protocol 对齐）
        if (it->sharers.count() == 1) {
            it->state = MOESI_E;  // 唯一持有者 → E
        } else {
            it->state = MOESI_S;  // 多 sharer → S
        }
    } else if (op == SnoopOp::REMOVE) {
        it->sharers.reset(cache_id);
        if (it->sharers.none()) {
            // 无 sharer → 可回收
            set.erase(it);
        }
    }
    it->lru_counter = ++global_lru_counter_;
}
```

### 4.3 关键设计取舍

- **简化协议**：v0 仅追踪 6 状态（I/S/E/M/O/T），不复制 gem5 slicc 完整协议
- **2 种实现**：完整追踪（精确但 footprint 大）+ 失效追踪（粗略但 footprint 小）
- **同步查询**：CoherentXBar 在 broadcast 前同步查询（无端口、无 delay）
- **max_caches_ = 16**：硬编码上限（v0 简化）

## 5. Bundle 字段使用

> SnoopFilter **不消费 Bundle**——它通过 CoherentXBar 调用 `get_snoop_targets()` 同步查询。

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/snoop_filter.hh` SnoopFilter | `tlm::SnoopFilterTLM` (基类) | 简化：v0 仅 2 种实现 |
| `SnoopFilterCache` | `tlm::SnoopFilterCacheTLM` | 同语义 |
| `SnoopFilterInvalidator` | `tlm::SnoopFilterInvalidatorTLM` | 同语义 |
| `SnoopFilter::lookup` | `get_snoop_targets` | 同语义 |
| `SnoopFilter::update` | `update_sharer` | 同语义 |
| `SnoopFilter::invalidate` | `invalidate` | 同语义 |
| `SnoopFilter::evict` | (LRU 内部) | 同语义 |

## 7. 实施路径

### 7.1 Phase 7.C 步骤

1. 新建 `include/tlm/interconnect/snoop_filter_tlm.hh`（~250 行）
2. 实现 `SnoopFilterTLM` 抽象基类
3. 实现 `SnoopFilterCacheTLM`（完整追踪版）
4. 实现 `SnoopFilterInvalidatorTLM`（失效追踪版）
5. 与 `CoherentXBarTLM` 集成（shared_ptr 注入）
6. 加 Catch2 测试：`test/test_snoop_filter.cc`

### 7.2 Phase 7.D 步骤（联动 TCC）

1. TCC 通过 CoherentXBar 查询 SnoopFilter
2. GPU 写 CPU cache line → SnoopFilter 触发 CPU snoop
3. 验证 GPU 写后 CPU L1 收到 invalidation

### 7.3 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[snoop_filter]"` 全部通过
- [ ] get_snoop_targets 正确性（与 gem5 reference 对照）
- [ ] LRU 替换真实生效
- [ ] 与 CoherentXBar 集成测试通过

### 7.4 估计工作量

- 设计: 0.5 周
- 基础版实施（2 种实现）: 1-2 周
- 集成到 CoherentXBar: 0.5 周
- 测试: 0.5 周
- **总计: 2.5-3.5 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **SnoopFilter 容量限制**——大工作集下频繁 LRU 替换 | 中 | 中 | 暴露 `set_num_sets()` 给用户配置 |
| R2 | **SnoopFilter 状态不一致**——与 cache 实际状态脱钩 | 中 | 高 | 仅作 hint（CoherentXBar 必须 fallback 到 broadcast 全部） |
| R3 | **SnoopFilterInvalidator 误判**——仅记录 invalidator 不够精确 | 中 | 中 | 文档明确：精度 vs footprint 权衡 |
| R4 | **max_caches_ 硬编码**——> 16 cache 时溢出 | 低 | 中 | 模板化 `MAX_CACHES` 参数 |
| R5 | **LRU 锁步问题**——global_lru_counter_ 频繁增加溢出 | 低 | 低 | 64-bit counter（>100 年才溢出） |
| R6 | **多线程 race**——并发 update_sharer 数据竞争 | 低 | 高 | 加锁（v0 单线程仿真，无需考虑） |
| R7 | **与 cache-protocol 状态脱节**——filter 状态 ≠ cache 状态 | 中 | 中 | filter 仅作 hint；真实状态由 cache-protocol 维护 |

## 9. 设计决策点

### D1 默认实现

- **Q**: 默认 `SnoopFilterCacheTLM` 还是 `SnoopFilterInvalidatorTLM`？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: `SnoopFilterCacheTLM`（精确优先）
- **依赖**: 用户配置

### D2 容量参数默认值

- **Q**: DEFAULT_NUM_SETS / DEFAULT_NUM_WAYS？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 1024 sets × 4 ways = 4096 entries（与 L2 同量级）
- **依赖**: gem5 SnoopFilterCache 默认值

### D3 状态机简化

- **Q**: 完整 MOESI 6 态还是简化 4 态？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 简化 4 态（I/S/M/E），不追踪 O/T（v0 简化）
- **依赖**: cache-protocol.md 的 MOESI 实施深度

### D4 多 CoherenceDomain 支持

- **Q**: 一个 SnoopFilter 跨多 domain 还是 per-domain？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: per-domain（每 CoherenceDomain 持有一个 SnoopFilter 实例）
- **依赖**: Phase 7.D 跨域桥接

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.4）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7.C (未来)**: 基础版实施（2 种实现 + CoherentXBar 集成）
- **Phase 7.D (未来)**: TCC + GPU 域 snoop 集成
