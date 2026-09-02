# cache-l2 微架构文档

> **类别**: cache > l2 · **状态**: 🟡 规划中 → 🔵 Implemented (v1.0 dGPU SoC 战略补充)
> **Header**: (规划) `include/tlm/cache/l2_cache_tlm.hh`
> **蓝图来源**: gem5 `src/mem/cache/cache.cc`（L2 实例化 + `memtest.py -c "2:2:1"` 多级树）+ NVIDIA B200 ~60MB/die + AMD MI300X ~8MB/XCD
> **首版 commit**: 蓝图（来自调研 §2.3）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team (Sisyphus)

---

## 1. 设计目标（蓝图）

`tlm::L2CacheTLM` 是 CppTLM v2.2+ 引入的 **Level-2 Cache 抽象**——支持多级 cache hierarchy（L1 私有 + L2 共享）。**与 gem5 对位**: `gem5::Cache`（在 `memtest.py` 中以 L2 实例化）。

**核心特征**：
- **多级树**（`memtest.py -c "2:2:1"` 风格：每级 N 个 L1 共享一个 L2）
- **Inclusion property**（L1 ⊂ L2 = inclusive / exclusive）
- **多级 snoop fanout**（L1 失效时 snoop 其他 L1）
- **大写缓冲**（L2 通常 write-back + WriteBuffer）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                    2 级 Cache Hierarchy                        │
│                                                             │
│  CPU0 ──► L1_0 (私有) ─┐                                  │
│                         │                                  │
│  CPU1 ──► L1_1 (私有) ─┤──► L2 (共享) ──► Memory         │
│                         │                                  │
│  CPU2 ──► L1_2 (私有) ─┤                                  │
│                         │                                  │
│  CPU3 ──► L1_3 (私有) ─┘                                  │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|------|------|
| `req_in_[N]` | `InputStreamAdapter<CacheReqBundle>` | N (L1 数) | 接收 L1 上行请求 |
| `resp_out_[N]` | `OutputStreamAdapter<CacheRespBundle>` | N (L1 数) | 响应 L1 |
| `mem_req_out_` | `OutputStreamAdapter<CacheReqBundle>` | 1 | 转发到 MemoryTLM |
| `mem_resp_in_` | `InputStreamAdapter<CacheRespBundle>` | 1 | 接收 MemoryTLM 响应 |
| `snoop_in_` | `InputStreamAdapter<SnoopProbe>` | 1 | 接收 CoherenceDomain snoop（invalidation / downgrade） |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  L2CacheTLM                                 │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │ BaseCache (共享基类，Phase 7.C 引入)            │     │
│  │   - cache_lines_: set_idx × way_idx → CacheLine │     │
│  │   - tags_/data_/state_ 数组                    │     │
│  │   - MSHR: std::queue<MissReq>                  │     │
│  │   - WriteBuffer: std::deque<DirtyLine>         │     │
│  │   - sharers_: bitmask<NUM_L1>                  │     │
│  └──────────────────────────────────────────────────┘     │
│                          ↑ 继承                              │
│  ┌──────────────────────────────────────────────────┐     │
│  │ L2CacheTLM 特有                                  │     │
│  │   - l1_inclusion_: bool (L1 ⊂ L2 包含)        │     │
│  │   - snoop_fanout_                               │     │
│  │   - 多 L1 端口的地址路由                        │     │
│  └──────────────────────────────────────────────────┘     │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class L2CacheTLM : public BaseCacheTLM {  // 继承 BaseCache
public:
    static constexpr uint32_t MAX_L1_CLIENTS = 16;

    explicit L2CacheTLM(const std::string& name, EventQueue* eq,
                         uint32_t num_l1_clients = 4);

    std::string get_module_type() const override { return "L2CacheTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_inclusion(bool inclusive) { l1_inclusion_ = inclusive; }
    void set_num_l1_clients(uint32_t n) { num_l1_clients_ = n; }
    void set_snoop_fanout(uint32_t n) { snoop_fanout_ = n; }

    // === 多 L1 端口访问器 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& l1_req_in(uint32_t idx);
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& l1_resp_out(uint32_t idx);

    // === Memory 端口 ===
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& mem_req_out();
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& mem_resp_in();

    // === Snoop 接口 ===
    void receive_snoop_probe(uint64_t addr, const std::string& probe_type);

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    bool check_l1_inclusion(uint64_t addr);  // L1 ⊂ L2 检查
    void broadcast_snoop(uint64_t addr);       // snoop fanout

    uint32_t num_l1_clients_;
    bool l1_inclusion_;
    uint32_t snoop_fanout_;

    std::vector<std::unique_ptr<InputStreamAdapter<CacheReqBundle>>> l1_req_ins_;
    std::vector<std::unique_ptr<OutputStreamAdapter<CacheRespBundle>>> l1_resp_outs_;

    OutputStreamAdapter<CacheReqBundle> mem_req_out_;
    InputStreamAdapter<CacheRespBundle> mem_resp_in_;

    // 统计
    tlm_stats::Scalar l1_to_l2_requests_;
    tlm_stats::Scalar l2_to_memory_requests_;
    tlm_stats::Scalar inclusion_violations_;
    tlm_stats::Scalar snoop_fanout_count_;
    tlm_stats::Distribution l2_access_latency_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 5 阶段

```cpp
void L2CacheTLM::tick() {
    // 1. 响应消费 (Memory 侧响应)
    if (mem_resp_in_.valid() && mem_resp_in_.ready()) {
        // 消费 + 响应转发到对应 L1
    }

    // 2. L1 请求处理（轮询所有 L1 端口）
    for (uint32_t i = 0; i < num_l1_clients_; ++i) {
        if (l1_req_ins_[i]->valid() && l1_req_ins_[i]->ready()) {
            const auto& req = l1_req_ins_[i]->data();
            handle_l1_request(i, req);
            l1_req_ins_[i]->consume();
        }
    }

    // 3. Snoop 处理（来自 CoherenceDomain）
    if (snoop_in_.valid() && snoop_in_.ready()) {
        const auto& probe = snoop_in_->data();
        receive_snoop_probe(probe.addr, probe.type);
        snoop_in_->consume();
    }

    // 4. MSHR 推进（v0 简化：不阻塞）
    // 5. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 handle_l1_request

```cpp
void L2CacheTLM::handle_l1_request(uint32_t l1_idx, const CacheReqBundle& req) {
    uint64_t addr = req.address.read();
    bool is_write = req.is_write.read();
    bool is_hit = false;
    bool is_dirty = false;

    // 1. tag 查找
    auto* line = find_line(addr);
    if (line && line->tag == extract_tag(addr)) {
        is_hit = true;
        is_dirty = line->state == MOESI_M;
    }

    // 2. L1 inclusion 检查
    if (l1_inclusion_ && is_hit && line->sharers[i] && !line->sharers[l1_idx]) {
        // 异常：L1 有副本但未在 sharers 中
        inclusion_violations_++;
    }

    // 3. 状态更新
    if (is_hit) {
        if (is_write) line->state = MOESI_M;  // 写：M 态
        line->sharers.set(l1_idx);
    } else {
        // 4. miss：MSHR + 转发到 Memory
        MissReq miss{addr, is_write, l1_idx};
        mshr_.push(miss);
        forward_to_memory(req);
    }

    // 5. 响应回 L1
    CacheRespBundle resp;
    resp.transaction_id.write(req.transaction_id.read());
    resp.is_hit.write(is_hit ? 1 : 0);
    l1_resp_outs_[l1_idx]->write(resp);
}
```

## 5. Bundle 字段使用（规划）

| 字段 | L2CacheTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——`inflight_txns_` 映射键 |
| `address` | **关键**——tag 查找 |
| `is_write` | 决定 hit/miss 后的状态转换 |
| `data` | 写时更新 cache 行 |
| `size` | 透传 |
| `kernel_id` | 透传（Phase 7.D 跨域一致性） |

## 6. 蓝图对齐

- gem5 `src/mem/cache/cache.cc`（L2 实例化）
- gem5 `configs/example/memtest.py -c "2:2:1"`（多级 cache 树配置）
- gem5 `src/mem/snoop_filter.cc`（snoop 过滤）
- 调研 §2.3 Cache 路径

## 7. 实施路径

### 7.1 Phase 7.E 步骤

1. 新建 `include/tlm/cache/l2_cache_tlm.hh`（~350 行）
2. 继承 `BaseCacheTLM`（Phase 7.C 引入）
3. 写多 L1 端口 `MultiPortStreamAdapter`
4. 写 inclusion 验证
5. 写 snoop fanout（与 `CoherenceDomain::register_bridge` 集成）
6. 加 Catch2 测试：`test/test_l2_cache.cc`
7. 新增 `configs/l2_cache_test.json`（2 L1 共享 1 L2 拓扑）

### 7.2 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[phase7]"` 全部通过
- [ ] 2 L1 共享 1 L2 端到端运行
- [ ] inclusion violation 检测生效
- [ ] snoop fanout 真实触发

### 7.3 估计工作量

- 设计: 1 周
- 实施: 1-2 周
- 测试: 0.5-1 周
- **总计: 2.5-4 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **inclusion 验证**——真实多 L1 场景难测 | 中 | 中 | 单元测试覆盖；inclusion_violations_ 统计监控 |
| R2 | **snoop fanout 复杂**——多 L1 顺序保证 | 中 | 中 | v0 简化（按 num_l1_clients_ 顺序 fanout） |
| R3 | **MSHR 容量**——背压场景可能溢出 | 中 | 中 | 暴露 `set_max_mshr_entries()` |
| R4 | **写策略**——write-back 与 L1 的 view 一致性 | 高 | 中 | v0 简化（write-through）；Phase 7.D 真实 write-back |
| R5 | **L1 ⊂ L2 包含 vs 排他**——v0 仅 inclusive | 中 | 中 | on_config_loaded 暴露 `set_inclusion(false)` |
| R6 | **多 L1 snoop 风暴** | 中 | 中 | Phase 7.C SnoopFilter 减负 |
| R7 | **跨域 CoherenceDomain**——L2 在 CPU 域但需要 snoop GPU 域的 TCC | 中 | 中 | Phase 7.D+ 跨域桥接 |
| R8 | **大写缓冲**——WriteBuffer 容量与 L1 一致 | 中 | 中 | on_config_loaded 暴露 setter |

## 9. 设计决策点

### D1 L1 ⊂ L2 inclusion 默认值

- **Q**: 默认 inclusive 还是 exclusive？
- **状态**: 留待 Phase 7.E 设计时确定
- **建议**: inclusive（与 gem5 默认一致）
- **依赖**: gem5 `Cache::inclusive` 默认

### D2 snoop fanout 顺序

- **Q**: snoop 按 L1 编号顺序还是按最近使用顺序？
- **状态**: 留待 Phase 7.E 设计时确定
- **建议**: 按编号顺序（确定性）
- **依赖**: 与 L1 LRU 集成（Phase 7.E+）

### D3 MSHR 容量

- **Q**: MSHR 默认容量？
- **状态**: 留待 Phase 7.E 设计时确定
- **建议**: 16（与真实 CPU L2 一致）

### D4 多 L1 port adapter

- **Q**: 多 L1 端口用 MultiPortStreamAdapter 还是 MultiPortIndependent？
- **状态**: 留待 Phase 7.E 设计时确定
- **建议**: MultiPortStreamAdapter（沿用 CrossbarTLM 模式）
- **依赖**: `framework/multi_port_stream_adapter.hh`

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.3 + 调研 §2.4）
- **2026-06-11**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐
- **Phase 7.C (未来)**: BaseCache 抽象 + CoherenceDomain 集成
- **Phase 7.E (未来)**: L2CacheTLM 实施 + multi-CU mesh 集成
- **Phase 7.F+ (未来)**: 真实 inclusion property + 大写缓冲
