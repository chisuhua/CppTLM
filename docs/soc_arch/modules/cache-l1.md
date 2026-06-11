# cache-l1 微架构文档

> **类别**: Cache > L1
> **状态**: ✅ 已实施
> **Header**: `include/tlm/cache_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:30`）
> **蓝图来源**: gem5 `src/mem/cache/base_cache.hh`（BaseCache 抽象）— v0 简化版
> **首版 commit**: v2.1 路径同步（2026-06-08 附近）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.3
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §2（GPU 内部互联关键模板）

---

## 1. 设计目标

`CacheTLM` 是 CppTLM v2.1 的**单端口 L1 cache 简化模型**，用于验证 ChStream 协议下"读/写+命中判断"端到端。**与 gem5 对位**: `gem5::Cache`（经典写回写分配）— v0 仅保留 hit/miss 决策 + 5/50 周期延迟。

**核心特性**（来自 `cache_tlm.hh:32-137`）：
- 继承 `ChStreamModuleBase`，单端口 Initiator/Target
- `std::map<uint64_t, uint64_t>` 存储 cache 行（**简化：全关联**）
- 命中延迟 5 周期，未命中 50 周期（硬编码）
- 写：写分配（更新 map）
- ChStream 握手：`req_in_.valid() && req_in_.ready()` 时处理

## 2. 架构概览

### 2.1 内部结构

```
        req_in_  (InputStreamAdapter<CacheReqBundle>)
            │
            ▼
   ┌─────────────────────────┐
   │ tick():                 │
   │  if (valid && ready):   │
   │    addr = req.address   │
   │    hit = map.count(addr) │
   │    latency = hit ? 5 : 50│
   │    if write: map[addr]=data
   │    build resp, write     │
   │    resp_out_             │
   │    req_in_.consume()     │
   │  adapter_->tick()       │
   └─────────────────────────┘
            │
            ▼
   cache_lines_: std::map<uint64_t, uint64_t>
            │
            ▼
        resp_out_  (OutputStreamAdapter<CacheRespBundle>)
```

### 2.2 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|------|------|
| `req_in_` | `InputStreamAdapter<CacheReqBundle>` | 1 | 接收请求 |
| `resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | 1 | 发送响应 |
| `adapter_` | `StreamAdapter<CacheTLM, ...>*` | 1 | ChStream 桥梁 |

## 3. 接口（Public API）

```cpp
class CacheTLM : public ChStreamModuleBase {
public:
    CacheTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "CacheTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig&) override;

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();
    cpptlm::StreamAdapterBase* get_adapter() const;

    tlm_stats::StatGroup& stats();
    void dumpStats(std::ostream& os) const;
};
```

## 4. 行为流程

### 4.1 tick() 主循环

```cpp
void CacheTLM::tick() {
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        uint64_t addr = req.address.read();
        bool is_write = req.is_write.read();

        ++stats_requests_;
        bool hit = cache_lines_.count(addr) > 0;
        uint64_t access_latency = hit ? 5 : 50;

        if (is_write) {
            cache_lines_[addr] = req.data.read();
        }
        if (hit) ++stats_hits_;
        else ++stats_misses_;
        stats_latency_.sample(access_latency);

        bundles::CacheRespBundle resp;
        resp.transaction_id.write(req.transaction_id.read());
        resp.data.write(hit ? cache_lines_[addr] : 0);
        resp.is_hit.write(hit ? 1 : 0);
        resp.error_code.write(0);
        resp_out_.write(resp);
        req_in_.consume();
    }
    if (adapter_) adapter_->tick();
}
```

**关键简化**：
- `latency` 仅 `sample()` 调用，**未实际延迟响应**——v0 单拍即返回
- 写：写分配但**无 dirty 位**（无回写语义）
- **没有 MSHR、WriteBuffer、replacement policy**

## 5. Bundle 字段使用

| 字段 | CacheTLM 使用 |
|------|---------------|
| `transaction_id` | 透传到 resp（保持 ID） |
| `address` | **关键**——查表键 |
| `is_write` | 决定是否更新 map |
| `data` | 写时存入 map |
| 其他（`size` / `fragment_*`） | 透传（v0 忽略） |

## 6. 统计

| 指标 | 类型 | 含义 |
|------|------|------|
| `stats_requests_` | Scalar | 总请求数 |
| `stats_hits_` | Scalar | 命中数 |
| `stats_misses_` | Scalar | 未命中数 |
| `stats_latency_` | Distribution | 访问延迟（5 或 50 cycle） |

**路径**: `system.cache`
**典型数值**（来自 `test_cache_chstream_test.json`）：
- 单测 `CacheTLM basic read miss`: 1 miss, 1 request, latency 50
- `CacheTLM read hit after write`: 1 hit, latency 5
- 命中率统计：`hits / requests`

## 7. 蓝图（未来演进）

### 7.1 Phase 7.C 应用

调研 §2.3 + 7.C：CacheTLM 升级为 **protocol-aware**：
- `CacheLine = {data, CoherenceState, sharers_bitmask}`
- 6×6 状态转换表（`I→S`/`I→M`/`S→M`/`M→I`/`S→I`/等）
- `get_snoop_targets()` / `handle_snoop()` 接口
- 与 `CoherenceDomain` 集成（snoop callback + `lookup_home_node()`）

### 7.2 蓝图增强（gap）

- **多级 cache**: L1 + L2 + L3，递归实例化（`memtest.py` 风格 `-c caches[:level]`）
- **替换策略**: LRU/LFU/FIFO/RRIP（gem5 replacement policies）
- **NoncoherentCacheTLM**（位于一致域之下）
- **MSHR + WriteBuffer**（gem5 BaseCache 核心组件）
- **真实延迟**（`stats_latency_.sample()` 后**实际**等 N 周期再返回）
- **set 参数化**：`sets` / `ways` / `blocksize` / `level`

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **延迟仅统计不真延迟**——单拍即返回 | 高 | 中 | v0 简化；如需真实延迟需在 tick() 中维护 in-flight 队列（v2.2+） |
| R2 | 全关联（`std::map`）——非 set-associative | 高 | 低 | v0 简化；L1 通常 4/8-way associativity（v2.2+ 改 set index hash） |
| R3 | **写无 dirty 位**——无回写语义 | 高 | 中 | 简化（v0）；写回 cache 需 dirty bit + writeback 队列 |
| R4 | 命中/未命中延迟 5/50 硬编码——无配置 | 高 | 中 | 缺 setter；需 `set_hit_latency()` / `set_miss_latency()` |
| R5 | **协议不可知**（不知 MOESI/MESI） | 高 | 中 | Phase 7.C 升级蓝图已规划 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ✅ | `test/test_cache_*.cc` 系列 + `[chstream]` + `[phase6]` 集成测试 |
| 端到端 (Cache→Crossbar→Memory) | ✅ | `configs/single_cluster_soc.json` / `configs/crossbar_test.json` |
| 命中率统计 | ✅ | `hits / requests` 公式可计算 |
| ChStream 协议 | ✅ | `req_in_.valid() && req_in_.ready()` 握手 |
| Stats 输出 | ✅ | `dumpStats()` 含 4 指标 |

## 10. 修订历史

- **2026-04-12**: CacheTLM 初版（v2.1 新 ChStream 语义）
- **2026-04-13**: Crossbar/Memory 集成测试
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B1 批次）
