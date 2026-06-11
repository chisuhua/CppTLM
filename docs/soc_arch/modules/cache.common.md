# cache.common 微架构文档

> **类别**: cache > (common)
> **状态**: 🟡 规划中（跨 Phase 7.C-E 共享概念）
> **Header**: (无独立文件 — 概念文档)
> **蓝图来源**: gem5 `src/mem/cache/base_cache.hh`（BaseCache 抽象）
> **首版 commit**: 蓝图（来自调研 §2.3）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

---

## 1. 设计目标（蓝图）

本文件是 **Cache 路径的跨 phase 概念文档**——记录 v0 简化版 + Phase 7.C+ 真实实现的 Cache 通用术语、设计决策、字段语义对位。**与 gem5 对位**: `gem5::BaseCache`（~3000 行 slicc + C++ 混合实现，CppTLM 用 C++ `switch` 表简化）。

**目标读者**: `cache-l1.md` / `cache-l2.md` / `cache-protocol.md` / `cache-replacement.md` / `cache-noncoherent.md` 的实施者。

## 2. 通用概念（规划）

### 2.1 Cache 抽象层次

```
┌─────────────────────────────────────────────────────────────┐
│                  Cache 抽象层次                                │
├─────────────────────────────────────────────────────────────┤
│ Application: JSON params { sets, ways, blocksize, level }   │
│   ↓ 构造                                                         │
│ BaseCache (v2.2 引入)                                          │
│   - cache_lines_: map<addr, CacheLine>                       │
│   - MSHR: Miss Status Handling Register                       │
│   - WriteBuffer: 写缓冲（write-back 用）                     │
│   - Tags: index/tag 计算                                       │
│   ├─ CoherentCache（Phase 7.C 引入，CacheTLM 升级）         │
│   │   - CoherenceState (I/S/E/M/O/T)                        │
│   │   - sharers_: bitmask                                    │
│   │   - snoop_callback_                                     │
│   │   - 6×6 状态转换表                                      │
│   ├─ NoncoherentCache（cache-noncoherent.md 蓝图）           │
│   │   - assert on snoop request                             │
│   ├─ L1Cache (v0 CacheTLM) - 私有，L1 粒度                  │
│   ├─ L2Cache (cache-l2.md 蓝图) - 共享/包含/排他             │
│   └─ TCC (gpu-tcc.md 蓝图) - GPU L2，Phase 7.D              │
│   ↓ 替换策略                                                     │
│ ReplacementPolicy (cache-replacement.md 蓝图)                │
│   - LRU / LFU / FIFO / RRIP                                  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 关键术语

| 术语 | 含义 | v0 现状 | Phase 7.C+ |
|------|------|---------|-----------|
| **CacheLine** | cache 一行（block） | `std::pair<data, state>` | 完整 `CacheLine = {data, state, sharers, dirty, tag}` |
| **Set** | cache 中按地址索引的组 | 抽象（`std::map<addr, data>` 全关联） | 真实 N-way set-associative |
| **Way** | set 内的一路 | 抽象 | 真实 `ways_` |
| **Index** | set 索引（地址中段） | 抽象 | 真实 `(addr >> idx_shift) & idx_mask` |
| **Tag** | set 内唯一标识（地址中高段） | 抽象 | 真实 `tag_` |
| **MSHR** | Miss Status Handling Register（未完成请求队列） | 抽象 | 真实 `std::queue<MissReq>` |
| **WriteBuffer** | 写缓冲（write-back 写脏行） | 抽象 | 真实 `std::deque<DirtyLine>` |
| **Write Policy** | write-through / write-back | **v0 写分配但无 dirty 位** | 真实 dirty + write-back 队列 |
| **Allocation** | read-allocate / write-allocate / no-write-allocate | 全部 write-allocate | 暴露 setter |
| **Inclusion** | L1 ⊂ L2（inclusive）/ exclusive | 抽象 | Phase 7.E 真实 inclusion property |
| **Coherence State** | I/S/E/M/O/T (6 states) | 抽象 | 真实 6×6 状态转换 |
| **Sharer** | 拥有该 cache 行有效副本的节点 | 抽象 | 真实 `std::bitset` |
| **Tag Latency** | tag 比较延迟 | 0（同步） | 真实 `tag_latency_` |
| **Data Latency** | 数据访存延迟 | 5/50 硬编码 | 真实 `data_latency_` |
| **Response Latency** | 响应延迟 | 0（同步） | 真实 `response_latency_` |

## 3. 设计决策

| # | 决策 | 采纳方案 |
|---|------|----------|
| **D1** | CacheLine 数据结构 | 阶段演进：v0 = `std::pair` → v2.2 = 完整 `CacheLine` |
| **D2** | set/ways 参数化 | v2.2 引入（`{sets, ways, blocksize, level}` JSON params） |
| **D3** | 替换策略抽象 | 引入 `ReplacementPolicy` 抽象基类（4 种实现） |
| **D4** | Coherence 状态 | 简化 6×6 状态机（C++ `switch` 表，不复制 slicc） |
| **D5** | 写策略 | 暴露 `set_write_policy(through/back)` |
| **D6** | MSHR | 真实 `std::queue<MissReq>` + 容量限制 |
| **D7** | WriteBuffer | 真实 `std::deque<DirtyLine>`（仅 write-back） |
| **D8** | CoherenceDomain 集成 | Phase 7.C 引入 `snoop callback` |

## 4. 字段语义对位

| gem5 字段 | CppTLM v0 | CppTLM Phase 7.C+ |
|------------|-----------|-------------------|
| `BaseCache::cpuSidePort` | 1 个 `req_in_` + `resp_out_` | 同 v0 |
| `BaseCache::memSidePort` | 1 个 `req_out_` + `resp_in_`（多端口未实现） | 4 端口：`req_in_/resp_out_/req_out_/resp_in_` |
| `BaseCache::mshr` | 抽象 | `std::queue<MissReq>` |
| `BaseCache::writeBuffer` | 抽象 | `std::deque<DirtyLine>` |
| `BaseCache::tags` | `std::map<addr, data>`（全关联简化） | `tags_[set_idx][way_idx]` |
| `BaseCache::replacement_policy` | 硬编码（无替换） | `std::unique_ptr<ReplacementPolicy>` |
| `BaseCache::coherence_protocol` | 抽象 | `std::unique_ptr<CoherenceProtocol>` |

## 5. 蓝图对齐

- gem5 `src/mem/cache/base_cache.hh`（~1500 行）
- gem5 `src/mem/cache/cache_blk.hh`（CacheLine 抽象）
- gem5 `src/mem/cache/mshr.hh`（MSHR 实现）
- gem5 `src/mem/cache/cache_impl/associative_cache_impl.hh`（set-associative 实现）
- 调研 §2.3 Cache 路径

## 6. 实施路径

### 6.1 Phase 7.C 实施步骤

1. 新建 `include/tlm/cache/base_cache_tlm.hh`（~500 行）
2. 引入 `CacheLine = {data, CoherenceState, sharers_bitmask, dirty, tag}` 结构
3. 引入 `ReplacementPolicy` 抽象基类（4 个实现见 cache-replacement.md）
4. 引入 `CoherenceProtocol` 抽象基类（MOESI 状态机见 cache-protocol.md）
5. 改造 `CacheTLM` 为 `class CoherentCache : public BaseCache`
6. ModuleFactory 集成：创建 CacheTLM 时查找 CoherenceDomain → 注册 snoop callback
7. 5 个新 doc（cache.common / cache-l1 / cache-l2 / cache-protocol / cache-replacement / cache-noncoherent）协同实施

### 6.2 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[phase7]"` 全部通过
- [ ] 6×6 状态转换表通过单元测试
- [ ] `docs_sync_check.sh --strict` 通过

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **v0 全关联 `std::map` 性能瓶颈** | 高 | 中 | Phase 7.C 引入真实 set-associative |
| R2 | **替换策略无抽象**——硬编码 | 高 | 中 | Phase 7.C 引入 `ReplacementPolicy` 基类 |
| R3 | **MSHR 不存在**——单拍即返回 | 中 | 中 | Phase 7.C 真实 MSHR 队列 |
| R4 | **WriteBuffer 不存在**——写无脏位 | 中 | 中 | Phase 7.C dirty + write-back 队列 |
| R5 | **CoherenceDomain 与 CacheTLM 集成** | 高 | 中 | Phase 7.C 真实实现 + snoop callback |
| R6 | **跨 Cache 一致性**（L1 ⊂ L2 包含） | 中 | 中 | Phase 7.E 真实 inclusion property |
| R7 | **6×6 状态机易错**（deadlock / livelock） | 中 | 高 | 单元测试 + 触发升级条件（5+ 回归） |
| R8 | **snoop filter 一致性** | 中 | 中 | Phase 7.C 引入 SnoopFilterTLM |

## 8. 设计决策点

### D1 CacheLine 最小集

- **Q**: `CacheLine` 应包含哪些字段？`data/state/dirty/tag` 还是更细？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: `data/state/dirty/tag/sharers_bitmask` 5 字段（最小集）

### D2 set 索引算法

- **Q**: set 索引是 `(addr >> idx_shift) & idx_mask` 还是 hash 函数？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 位掩码（与 gem5 `AssociativeCache` 对齐）
- **依赖**: 与 blocksize / num_sets JSON params 配对

### D3 WriteBuffer 容量

- **Q**: WriteBuffer 默认容量？back-pressure 触发？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 默认 16（与真实 CPU L1 write buffer 容量一致）

### D4 ReplacementPolicy 抽象边界

- **Q**: `ReplacementPolicy` 应暴露什么接口？`victim_way(set_idx)` 还是 `on_access(set_idx, way_idx)`？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: `victim_way(set_idx)`（lazy 策略：仅在需要淘汰时调用）
- **依赖**: gem5 `BaseReplacementPolicy` API

## 9. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.3）
- **2026-06-11**: B3 批次设计 — 提取 D1-D8 + 字段语义对位
- **Phase 7.C (未来)**: 实施 `BaseCache` 抽象 + 6×6 状态机
- **Phase 7.E (未来)**: L1 ⊂ L2 inclusion 真实实现
- **Phase 7.D+ (未来)**: TCC 真实 cache 行状态
