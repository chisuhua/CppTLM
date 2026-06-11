# cache-replacement 微架构文档

> **类别**: cache > replacement
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/cache/replacement_policy.hh`
> **蓝图来源**: gem5 `src/mem/cache/replacement_policies/`（LRU / LFU / FIFO / RRIP / MRU / Random）
> **首版 commit**: 蓝图（来自调研 §2.3）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

---

## 1. 设计目标（蓝图）

`cache-replacement` 是 **替换策略族**（Replacement Policies）抽象——Phase 7.C 与 `BaseCacheTLM` 协同实施，提供 4 种主流策略实现。**与 gem5 对位**: `gem5::BaseReplacementPolicy` + 6+ 具体实现。

**核心抽象**：
- `ReplacementPolicy` 抽象基类：暴露 `victim_way(set_idx)` 接口
- 4 种实现：LRU / LFU / FIFO / RRIP
- 工厂模式：`make_policy(name)` 暴露 JSON 配置

## 2. 接口（规划）

```cpp
namespace tlm {

class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;

    // 初始化（Cache 配置时调用）
    virtual void init(uint32_t num_sets, uint32_t num_ways) = 0;

    // 访问通知（更新策略状态）
    virtual void on_access(uint32_t set_idx, uint32_t way_idx) = 0;

    // 失效通知（line 被 evict）
    virtual void on_invalidate(uint32_t set_idx, uint32_t way_idx) = 0;

    // 选择 victim way（lazy 策略：仅在需要淘汰时调用）
    virtual uint32_t victim_way(uint32_t set_idx) = 0;

    virtual const char* name() const = 0;
};

// === LRU (Least Recently Used) ===
class LRUPolicy : public ReplacementPolicy {
    // 内部状态：每 set 一个访问顺序数组
    std::vector<std::vector<uint32_t>> access_order_;  // [set][rank]
    uint32_t num_sets_, num_ways_;

public:
    void init(uint32_t num_sets, uint32_t num_ways) override {
        num_sets_ = num_sets; num_ways_ = num_ways;
        access_order_.assign(num_sets, std::vector<uint32_t>(num_ways));
        for (uint32_t s = 0; s < num_sets; ++s)
            for (uint32_t w = 0; w < num_ways; ++w)
                access_order_[s][w] = w;  // 初始化：way 0 最旧
    }

    void on_access(uint32_t set_idx, uint32_t way_idx) override {
        // 找到 way_idx 在 access_order_[set_idx] 中的位置，移到末尾（最新）
        auto& order = access_order_[set_idx];
        auto it = std::find(order.begin(), order.end(), way_idx);
        if (it != order.end()) {
            uint32_t w = *it;
            order.erase(it);
            order.push_back(w);
        }
    }

    void on_invalidate(uint32_t set_idx, uint32_t way_idx) override {
        auto& order = access_order_[set_idx];
        auto it = std::find(order.begin(), order.end(), way_idx);
        if (it != order.end()) order.erase(it);
    }

    uint32_t victim_way(uint32_t set_idx) override {
        // LRU = 最旧的 = access_order_[set_idx].front()
        return access_order_[set_idx].front();
    }

    const char* name() const override { return "LRU"; }
};

// === LFU (Least Frequently Used) ===
class LFUPolicy : public ReplacementPolicy {
    std::vector<std::vector<uint32_t>> freq_;  // [set][way] 访问计数
    // ...
    uint32_t victim_way(uint32_t set_idx) override {
        // 选 freq_ 最小的 way
        return std::distance(freq_[set_idx].begin(),
                             std::min_element(freq_[set_idx].begin(), freq_[set_idx].end()));
    }
    const char* name() const override { return "LFU"; }
};

// === FIFO (First In First Out) ===
class FIFOPolicy : public ReplacementPolicy {
    std::vector<std::queue<uint32_t>> fifo_queue_;  // [set] → way 插入顺序
    // ...
    uint32_t victim_way(uint32_t set_idx) override {
        return fifo_queue_[set_idx].front();  // 最先插入的
    }
    const char* name() const override { return "FIFO"; }
};

// === RRIP (Re-Reference Interval Prediction) ===
class RRIPPolicy : public ReplacementPolicy {
    // 每 way 维护一个 RRPV (Re-Reference Prediction Value)
    // 0 = 近期访问, 1-3 = 远期, max = 立即淘汰
    std::vector<std::vector<uint8_t>> rrpv_;  // [set][way]
    static constexpr uint8_t RRIP_MAX = 3;
    // ...
    uint32_t victim_way(uint32_t set_idx) override {
        // 找 rrpv_ == RRIP_MAX 的 way（若有多个 → 选第一个）
        for (uint32_t w = 0; w < num_ways_; ++w)
            if (rrpv_[set_idx][w] == RRIP_MAX) return w;
        // 否则全部 +1 直到找到 RRIP_MAX
        while (true) {
            for (uint32_t w = 0; w < num_ways_; ++w) ++rrpv_[set_idx][w];
            for (uint32_t w = 0; w < num_ways_; ++w)
                if (rrpv_[set_idx][w] == RRIP_MAX) return w;
        }
    }
    const char* name() const override { return "RRIP"; }
};

// === 工厂 ===
std::unique_ptr<ReplacementPolicy> make_policy(const std::string& name) {
    if (name == "LRU")  return std::make_unique<LRUPolicy>();
    if (name == "LFU")  return std::make_unique<LFUPolicy>();
    if (name == "FIFO") return std::make_unique<FIFOPolicy>();
    if (name == "RRIP") return std::make_unique<RRIPPolicy>();
    return nullptr;
}

} // namespace tlm
```

## 3. 行为流程（规划）

### 3.1 与 BaseCache 集成

```cpp
class BaseCacheTLM {
protected:
    std::unique_ptr<ReplacementPolicy> policy_;

public:
    void on_config_loaded() override {
        const json& cfg = get_config();
        std::string policy_name = cfg.value("replacement_policy", "LRU");
        policy_ = make_policy(policy_name);
        policy_->init(num_sets_, num_ways_);
    }

    uint32_t find_victim_way(uint32_t set_idx) {
        if (!policy_) return 0;  // fallback
        return policy_->victim_way(set_idx);
    }

    void on_cache_access(uint32_t set_idx, uint32_t way_idx) {
        if (policy_) policy_->on_access(set_idx, way_idx);
    }
};
```

## 4. Bundle 字段使用

**无 Bundle 字段**——ReplacementPolicy 是**纯算法**抽象，不传输事务数据。

## 5. 蓝图对齐

- gem5 `src/mem/cache/replacement_policies/base.hh`（抽象基类）
- gem5 `src/mem/cache/replacement_policies/lru_rp.hh`（LRU）
- gem5 `src/mem/cache/replacement_policies/fifo_rp.hh`（FIFO）
- gem5 `src/mem/cache/replacement_policies/lfu_rp.hh`（LFU）
- gem5 `src/mem/cache/replacement_policies/rrip_rp.hh`（RRIP）
- 调研 §2.3 替换策略

## 6. 实施路径

### 6.1 Phase 7.C 步骤

1. 新建 `include/tlm/cache/replacement_policy.hh`（~250 行）
2. 实现 `ReplacementPolicy` 抽象基类
3. 实现 4 种策略：LRU / LFU / FIFO / RRIP
4. 实现 `make_policy` 工厂
5. 集成到 `BaseCacheTLM`
6. 单测：每策略 5+ 测试
7. JSON `replacement_policy` 字段暴露

### 6.2 验收标准

- [ ] 编译通过
- [ ] 4 策略单测 100% 覆盖
- [ ] `cpptlm_tests "[phase7]"` 全部通过
- [ ] `docs_sync_check.sh --strict` 通过

### 6.3 估计工作量

- 设计: 0.5 周
- 实施: 1 周
- 测试: 0.5 周
- **总计: 2 周**

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **LRU 实现顺序数组**——O(N) 移动操作 | 中 | 低 | v0 可接受（num_ways ≤ 16） |
| R2 | **LFU 计数溢出**——uint32_t 在 32 位下足够 | 低 | 低 | v0 用 uint32_t |
| R3 | **FIFO 队列内 way 失效**——erase 后顺序错乱 | 中 | 中 | 单元测试覆盖 |
| R4 | **RRIP max 值选择**——太大接近 FIFO，太小接近 LRU | 中 | 中 | 默认 3（gem5 默认值） |
| R5 | **替换策略不匹配 Coherence**——例如 Owned 态被替换 | 中 | 中 | Owned 态优先保留（CoherentCache 协调） |
| R6 | **与 BaseCache 集成 bug** | 中 | 中 | 单元测试覆盖 on_access / on_invalidate / victim_way 路径 |
| R7 | **JSON 配置错误**——未识别策略名 | 中 | 低 | `make_policy` 返回 `nullptr` + 显式报错 |
| R8 | **set/ways 参数变化**——init() 必须重新调用 | 低 | 低 | v0 配置期一次性 init() |

## 8. 设计决策点

### D1 默认策略

- **Q**: 默认 LRU 还是 FIFO？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: LRU（与真实 CPU 一致）

### D2 策略可配置

- **Q**: JSON `replacement_policy` 暴露哪些策略？仅 4 种还是更多？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 4 种（v0 简化）；v2.2 加 BIP / SRRIP / DRRIP

### D3 命中后是否更新 LRU

- **Q**: 读命中是否更新 LRU？写命中呢？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 读命中 + 写命中均更新 LRU（gem5 默认）
- **依赖**: 与 BaseCache 协同

## 9. 修订历史

- **2026-06-11**: 蓝图初版
- **2026-06-11**: B3 批次设计 — 4 策略 + 蓝图对齐
- **Phase 7.C (未来)**: 实施 ReplacementPolicy + 4 策略
- **v2.2+ (未来)**: BIP / SRRIP / DRRIP 扩展
