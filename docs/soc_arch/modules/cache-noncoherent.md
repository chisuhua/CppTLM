# cache-noncoherent 微架构文档

> **类别**: cache > noncoherent
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/cache/noncoherent_cache_tlm.hh`
> **注册**: (规划) `REGISTER_CHSTREAM` 扩展
> **蓝图来源**: gem5 `src/mem/cache/noncoherent_cache.cc`
> **首版 commit**: 蓝图（来自调研 §2.3） · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

---

## 1. 设计目标（蓝图）

`tlm::NoncoherentCacheTLM` 是**位于一致性域之下的非一致 cache**——**不实现 snooping**（在 snoop 请求到达时 `assert` 或 panic）。**与 gem5 对位**: `gem5::NoncoherentCache`。

**核心特征**：
- **不实现任何 coherence 协议**（与 CoherentCache 相反）
- **收到 snoop 请求时 panic**——v0 简化为 assert
- **用于混合系统**：CPU 域的 L1/L2 可能在某模块是非一致的（如 DMA / GPU 内私有 cache）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                  NoncoherentCacheTLM                        │
│                                                             │
│  ┌──────────────────────────────────────────────────┐      │
│  │ BaseCache (共享基类)                              │      │
│  │   - cache_lines_: set × way → CacheLine         │      │
│  │   - tags_/data_ (无 state 字段)                │      │
│  │   - MSHR: std::queue<MissReq>                  │      │
│  │   - 替换策略: LRU 默认                          │      │
│  └──────────────────────────────────────────────────┘      │
│                          ↑ 继承                              │
│  ┌──────────────────────────────────────────────────┐      │
│  │ NoncoherentCacheTLM 特有                        │      │
│  │   - receive_snoop_probe() 中 assert(0)         │      │
│  │   - 不挂载 CoherenceProtocol                     │      │
│  │   - 简化的 hit/miss 行为                         │      │
│  └──────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class NoncoherentCacheTLM : public BaseCacheTLM {
public:
    explicit NoncoherentCacheTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "NoncoherentCacheTLM"; }

    void on_config_loaded() override;  // 读 JSON params（不含 protocol）

    // === Snoop 接口（v0 panic）===
    void receive_snoop_probe(uint64_t addr, const std::string& probe_type) override {
        // v0: 不应到达
        assert(false && "NoncoherentCacheTLM received snoop probe (inconsistent topology)");
    }

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 简化版

```cpp
void NoncoherentCacheTLM::tick() {
    // 1. 响应消费（同 BaseCache）
    if (req_in_.valid() && req_in_.ready()) {
        // 消费 + 状态更新（无 state 字段，仅 tag/data 数组）
    }

    // 2. 请求处理（简化：tag 查 + 直接 hit/miss 决策）
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        uint64_t addr = req.address.read();
        bool is_hit = check_tag(addr);
        bool is_write = req.is_write.read();

        if (is_hit) {
            // 读 / 写直接命中（无状态机）
            if (is_write) update_data(addr, req.data.read());
        } else {
            // miss：MSHR + 转发到 Memory
            MissReq miss{addr, is_write};
            mshr_.push(miss);
            forward_to_memory(req);
        }
    }

    // 3. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 关键设计取舍

- **v0 Snoop panic**——收到 snoop probe 时 `assert(false)`
- **无 CoherenceProtocol 集成**——直接继承 BaseCache
- **无 Sharers bitmask**——单节点私有 cache，不需要追踪多副本
- **简化 hit/miss**——无 state machine，仅 tag 查

## 5. Bundle 字段使用

| 字段 | NoncoherentCacheTLM 使用 |
|------|---------------|
| `transaction_id` | inflight 跟踪 |
| `address` | tag 查找 |
| `is_write` | hit 时更新 data |
| `data` | hit 时更新 |
| `size` | 透传 |
| 其他 | 忽略 |

## 6. 蓝图对齐

- gem5 `src/mem/cache/noncoherent_cache.hh`（~200 行）
- gem5 `src/mem/cache/noncoherent_cache.cc`
- gem5 `noncoherent_cache.cc:115: panic(...)`（snoop 触发 panic）
- 调研 §2.3

## 7. 实施路径

### 7.1 Phase 7.C 步骤

1. 新建 `include/tlm/cache/noncoherent_cache_tlm.hh`（~100 行）
2. 继承 `BaseCacheTLM`（Phase 7.C 引入）
3. 写 `receive_snoop_probe` 中 `assert(false)` 实现
4. 加 Catch2 测试：
   - 正常 hit/miss
   - **snoop probe 触发 assert**（用 `REQUIRE_THROWS_AS` 验证）
5. JSON 配置示例（`noncoherent_l1_test.json`）

### 7.2 验收标准

- [ ] 编译通过
- [ ] hit/miss 测试通过
- [ ] snoop assert 测试通过
- [ ] `docs_sync_check.sh --strict` 通过

### 7.3 估计工作量

- 设计: 0.25 周
- 实施: 0.5 周
- 测试: 0.25 周
- **总计: 1 周**（最简单 cache 类型）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **误用**——用户配置 snoop probe 到 NoncoherentCache | 中 | 中 | assert 立即暴露错误拓扑配置 |
| R2 | **混合系统**——非一致与一致 cache 共存易错 | 中 | 中 | `CoherenceDomain` 注册时显式声明 |
| R3 | **DMA bypass**——DMA 可能绕过 coherence 写入 | 中 | 中 | v0 简化；Phase 7.D+ 真实 DMA 通道 |
| R4 | **GPU 内私有 cache**——TCC 不应 noncoherent | 低 | 中 | GPU L1/L2 应是 Coherent；仅 CPU 私有 cache 选 noncoherent |
| R5 | **替换策略未配置**——默认 LRU | 低 | 低 | v0 默认 LRU；JSON 暴露 |
| R6 | **stat 命名冲突**——与非一致 cache stat 混淆 | 低 | 低 | 显式 `get_stats_path()` |
| R7 | **v0 assert 太严格**——应 panic 改为丢弃 | 中 | 低 | v0 保持 assert（debug 期有效） |
| R8 | **混合配置 JSON**——单 `params.protocol` 字段 | 中 | 中 | JSON schema 显式校验 |

## 9. 设计决策点

### D1 snoop probe 行为

- **Q**: `receive_snoop_probe` 行为：assert / panic / log warn？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: assert（debug 期立即暴露错误）
- **替代**: v0 log warn + 丢弃（release 友好）

### D2 与 CoherenceDomain 关系

- **Q**: NoncoherentCacheTLM 是否注册到 CoherenceDomain？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: **不注册**——v0 assert 防止误用
- **依赖**: `CoherenceDomain::is_member` 检查

### D3 替换策略与 CoherentCache 复用

- **Q**: 替换策略实现是否在 CoherentCache 和 NoncoherentCache 间共享？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 共享（`BaseCacheTLM` 持有 `policy_`）
- **依赖**: cache-replacement.md 蓝图

## 10. 修订历史

- **2026-06-11**: 蓝图初版
- **2026-06-11**: B3 批次设计 — D1-D3 + 蓝图对齐
- **Phase 7.C (未来)**: 实施 NoncoherentCacheTLM（~100 行）
- **v2.2+ (未来)**: assert 改为 log warn（release 友好）
