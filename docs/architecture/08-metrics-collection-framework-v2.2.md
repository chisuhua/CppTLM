# CppTLM 性能指标收集框架 v2.2 — 设计架构

> **文档状态**: 📋 待实施  
> **v2.2 版本**: 2.2.0-draft  
> **创建日期**: 2026-04-15  
> **变更摘要**: 基于 gem5/BookSim 最佳实践，设计轻量级性能指标收集框架

---

## 1. 架构愿景

**核心目标**: 为非侵入式性能指标收集提供标准化基础设施，支持压力测试和系统级性能分析。

**核心策略**: 模块内嵌 Stats 结构体 + 全局 StatsManager 统一管理，遵循 gem5 统计框架模式。

**设计原则**:
1. **非侵入式**: 指标收集不改变模块核心逻辑，仅添加 stats 成员变量
2. **层次化**: 指标按模块层级组织 (`system.cache.hits`, `system.xbar.latency.avg`)
3. **在线聚合**: min/max/avg/stddev 实时计算，不存储全量样本（零额外内存增长）
4. **标准化输出**: 兼容 gem5 文本格式 + JSON/Markdown 报告
5. **零开销开关**: 编译期 `#ifdef ENABLE_METRICS` + 运行期条件判断
6. **可重置**: 仿真中途可重置统计，支持 warmup/measurement/drain 三阶段

---

## 2. 业界最佳实践分析

### 2.1 gem5 统计框架

| 维度 | gem5 实现 | CppTLM 采纳 |
|------|-----------|-------------|
| **统计类型** | `Stats::Scalar`, `Stats::Average`, `Stats::Distribution`, `Stats::Histogram` | ✅ 全部采用 |
| **层次命名** | `system.cpu.dcache.demandHits` | ✅ `system.cache.hits` |
| **聚合方式** | Online（`Stats::Average` 实时除 tick） | ✅ Online 聚合 |
| **输出格式** | `stats.txt` (对齐列) + JSON | ✅ Text + JSON |
| **单位系统** | `(Cycle)`, `(Count)`, `(Byte/Second)` | ✅ 每指标带单位 |
| **重置机制** | `resetStats()` 回调 | ✅ 支持 |

### 2.2 BookSim NoC 指标

| 维度 | BookSim 实现 | CppTLM 采纳 |
|------|--------------|-------------|
| **延迟分析** | min/avg/max + `plat_hist[]` 数组 | ✅ Distribution + HdrHistogram |
| **流量指标** | injection/accepted packet rate | ✅ `requests.(generated/completed)` |
| **链路指标** | `sent_flits`, `buffer_occupancy` | ✅ `flits.sent`, `bufferOccupancy` |
| **三阶段仿真** | warmup/measurement/drain | ✅ 支持 |

### 2.3 关键洞察

```
专业 vs 业余指标框架对比:
┌─────────────────┬─────────────────────────┬──────────────────────┐
│ 维度            │ 专业 (gem5/BookSim)     │ 业余                 │
├─────────────────┼─────────────────────────┼──────────────────────┤
│ 层次结构        │ 按 SimObject 层级组织    │ 扁平全局计数器        │
│ 单位            │ 每个指标都有单位          │ 纯数字                │
│ 采样阶段        │ warmup/measurement/drain │ 无阶段分离            │
│ 聚合            │ Online (min/max/avg)    │ 仅事后处理            │
│ 百分位          │ 直方图 (p50/p95/p99)    │ 仅平均值              │
│ 时间序列        │ 周期性快照               │ 仅仿真结束输出        │
└─────────────────┴─────────────────────────┴──────────────────────┘
```

---

## 3. 整体架构

### 3.1 分层设计

```
┌─────────────────────────────────────────────────────────────────┐
│  应用层：压力测试场景                                              │
│  HOTSPOT / STRIDED / RANDOM / SEQUENTIAL / MIXED                 │
├─────────────────────────────────────────────────────────────────┤
│  报告层：MetricsReporter                                           │
│  Text (gem5 风格) │ JSON (自动化) │ Markdown (人工可读)           │
├─────────────────────────────────────────────────────────────────┤
│  管理层：StatsManager (全局单例)                                    │
│  register_group() │ dump() │ reset()                              │
├─────────────────────────────────────────────────────────────────┤
│  统计类型层：Scalar / Average / Distribution / PercentileHistogram │
├─────────────────────────────────────────────────────────────────┤
│  模块层：CacheTLM.stats │ CrossbarTLM.stats │ MemoryTLM.stats     │
│  TrafficGenTLM.stats │ ArbiterTLM.stats │ CPUTLM.stats           │
├─────────────────────────────────────────────────────────────────┤
│  现有架构：EventQueue │ StreamAdapter │ ModuleFactory │ ch_stream  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 统计类型设计

```cpp
namespace tlm_stats {

// 1. Scalar — 简单计数器 (如请求总数)
class Scalar {
    std::atomic<Counter> _value{0};
    void operator++();           // 自增
    void operator+=(Counter);    // 累加
    Counter value() const;       // 读取
};

// 2. Average — 周期平均值 (如缓冲区占用率)
class Average {
    Counter _current_value{0};   // 当前周期值
    Counter _sum{0};             // 累积值 × 周期数
    Counter _last_update_tick{0};
    Result result();             // 返回 sum / total_ticks
};

// 3. Distribution — 分布统计 (如延迟 min/avg/max/stddev)
class Distribution {
    Counter _min{UINT64_MAX};
    Counter _max{0};
    Counter _sum{0};
    Counter _sum_sq{0};          // 方差计算
    Counter _count{0};
    void sample(Counter val);    // 采样
    Result mean();
    Result stddev();
};

// 4. PercentileHistogram — 百分位直方图 (p50/p95/p99)
class PercentileHistogram {
    // 基于 HdrHistogram 或指数桶实现
    void record(int64_t value);
    int64_t percentile(double p);
};

// 5. Formula — 计算型指标 (如 missRate = misses / requests)
class Formula {
    std::function<Result()> _calc;
    Result result();
};

} // namespace tlm_stats
```

### 3.3 层次化组设计

```cpp
namespace tlm_stats {

class StatGroup {
    std::string _name;
    StatGroup* _parent;
    std::vector<std::unique_ptr<StatBase>> _stats;
    std::map<std::string, std::unique_ptr<StatGroup>> _subgroups;

public:
    // 工厂方法
    Scalar& addScalar(const std::string& name, const std::string& desc,
                      const std::string& unit = "count");
    Distribution& addDistribution(const std::string& name, const std::string& desc);
    StatGroup& addSubgroup(const std::string& name);

    // 输出
    void dump(std::ostream& os) const;
    void reset();
};

} // namespace tlm_stats
```

---

## 4. 模块指标定义

### 4.1 CacheTLM 指标

```cpp
struct CacheTLM::Stats : public tlm_stats::StatGroup {
    // 请求指标
    tlm_stats::Scalar& requests;        // 总请求数
    tlm_stats::Scalar& requests_read;   // 读请求数
    tlm_stats::Scalar& requests_write;  // 写请求数
    
    // 命中/未命中
    tlm_stats::Scalar& hits;            // 命中数
    tlm_stats::Scalar& misses;          // 未命中数
    tlm_stats::Formula& miss_rate;      // 未命中率 = misses/requests
    
    // 延迟分布
    tlm_stats::Distribution& latency;   // 访问延迟 (min/avg/max/stddev)
    tlm_stats::PercentileHistogram& latency_hist;  // 延迟百分位 (p50/p95/p99)
    
    // 带宽
    tlm_stats::Average& bandwidth_read;  // 读带宽 (bytes/cycle)
    tlm_stats::Average& bandwidth_write; // 写带宽 (bytes/cycle)
};
```

| 指标路径 | 类型 | 单位 | 说明 |
|---------|------|------|------|
| `system.cache.requests` | Scalar | Count | 总请求数 |
| `system.cache.hits` | Scalar | Count | 命中数 |
| `system.cache.misses` | Scalar | Count | 未命中数 |
| `system.cache.miss_rate` | Formula | Ratio | 未命中率 |
| `system.cache.latency.avg` | Distribution | Cycle | 平均访问延迟 |
| `system.cache.latency.p99` | PercentileHistogram | Cycle | 99 分位延迟 |

### 4.2 CrossbarTLM 指标

```cpp
struct CrossbarTLM::Stats : public tlm_stats::StatGroup {
    // 流量指标
    tlm_stats::Scalar& flits_received;  // 接收 flit 数
    tlm_stats::Scalar& flits_sent;      // 发送 flit 数
    
    // 延迟
    tlm_stats::Distribution& flit_latency;  // flit 穿越延迟
    
    // 拥塞
    tlm_stats::Scalar& backpressure_cycles; // 反压周期数
    tlm_stats::Average& buffer_occupancy;   // 平均缓冲区占用 (flits)
    tlm_stats::Average& utilization;        // 利用率 (active/total cycles)
};
```

### 4.3 MemoryTLM 指标

```cpp
struct MemoryTLM::Stats : public tlm_stats::StatGroup {
    // 请求指标
    tlm_stats::Scalar& requests_read;   // 读请求
    tlm_stats::Scalar& requests_write;  // 写请求
    tlm_stats::Scalar& row_hits;        // 行缓冲命中
    tlm_stats::Scalar& row_misses;      // 行缓冲未命中
    
    // 延迟
    tlm_stats::Distribution& latency_read;   // 读延迟
    tlm_stats::Distribution& latency_write;  // 写延迟
    
    // 带宽
    tlm_stats::Average& bandwidth_read;  // 读带宽
    tlm_stats::Average& bandwidth_write; // 写带宽
    
    // 命中率
    tlm_stats::Formula& row_buffer_hit_rate; // 行缓冲命中率
};
```

### 4.4 TrafficGenTLM 指标

```cpp
struct TrafficGenTLM::Stats : public tlm_stats::StatGroup {
    // 生成指标
    tlm_stats::Scalar& requests_generated;  // 生成请求数
    tlm_stats::Scalar& responses_received;  // 收到响应数
    tlm_stats::Average& injection_rate;     // 注入率 (requests/cycle)
    
    // 地址分布 (用于验证压力模式)
    tlm_stats::PercentileHistogram& address_hist; // 地址访问分布
};
```

### 4.5 ArbiterTLM 指标

```cpp
struct ArbiterTLM::Stats : public tlm_stats::StatGroup {
    // 仲裁指标
    tlm_stats::Scalar& requests_arbitrated;  // 仲裁请求数
    tlm_stats::Scalar& port_requests[N];     // 每端口请求数
    tlm_stats::Distribution& queue_depth;    // 队列深度分布
    tlm_stats::Distribution& wait_time;      // 等待时间分布
};
```

### 4.6 CPUTLM 指标

```cpp
struct CPUTLM::Stats : public tlm_stats::StatGroup {
    tlm_stats::Scalar& requests_issued;     // 发出请求数
    tlm_stats::Scalar& responses_completed; // 完成响应数
    tlm_stats::Distribution& inflight;      // 在途请求数分布
    tlm_stats::Average& ipc;                // 每周期指令数 (模拟)
};
```

---

## 5. 输出格式

### 5.1 Text 格式 (gem5 风格)

```text
---------- Begin Simulation Statistics ----------
system.sim_cycles                                 100000                       # Simulation cycles (Cycle)
system.cache.requests                             10000                       # Total cache requests (Count)
system.cache.hits                                  8500                       # Cache hits (Count)
system.cache.misses                                1500                       # Cache misses (Count)
system.cache.miss_rate                          0.150000                       # Cache miss rate (Ratio)
system.cache.latency.min                             12                       # Cache access latency minimum (Cycle)
system.cache.latency.avg                          45.300                       # Cache access latency average (Cycle)
system.cache.latency.max                           450                       # Cache access latency maximum (Cycle)
system.cache.latency.stddev                       28.150                       # Cache access latency std deviation (Cycle)
system.cache.latency.p50                             32                       # Cache access latency 50th percentile (Cycle)
system.cache.latency.p95                            120                       # Cache access latency 95th percentile (Cycle)
system.cache.latency.p99                            380                       # Cache access latency 99th percentile (Cycle)
system.xbar.flits_received                        20000                       # Total flits received by crossbar (Count)
system.xbar.flits_sent                            19800                       # Total flits sent by crossbar (Count)
system.xbar.flit_latency.avg                       3.200                       # Flit traversal latency average (Cycle)
system.xbar.buffer_occupancy                       2.450                       # Average buffer occupancy (Flits)
system.memory.requests_read                       10000                       # Memory read requests (Count)
system.memory.latency_read.avg                    120.500                       # Memory read latency average (Cycle)
---------- End Simulation Statistics ----------
```

### 5.2 JSON 格式

```json
{
  "simulation": {
    "sim_cycles": 100000,
    "total_transactions": 10000
  },
  "cache": {
    "requests": 10000,
    "hits": 8500,
    "misses": 1500,
    "miss_rate": 0.15,
    "latency": {
      "min": 12,
      "avg": 45.3,
      "max": 450,
      "stddev": 28.15,
      "p50": 32,
      "p95": 120,
      "p99": 380
    }
  },
  "xbar": {
    "flits_received": 20000,
    "flits_sent": 19800,
    "flit_latency": { "avg": 3.2 },
    "buffer_occupancy": 2.45
  }
}
```

### 5.3 Markdown 格式

```markdown
# Performance Metrics Report

**Simulation Duration:** 100,000 cycles  
**Total Transactions:** 10,000  
**Dropped Packets:** 0 (0.00%)

## Cache Performance

| Metric | Value |
|--------|-------|
| Hit Rate | 85.00% |
| Miss Rate | 15.00% |
| Avg Latency | 45.3 cycles |
| P99 Latency | 380 cycles |

## Latency Distribution

```
  0-50    ████████████████████████████████████████ (65%)
 50-100   ████████████████ (22%)
100-200   ██████ (8%)
200-400   ███ (4%)
400+      █ (1%)
```
```

---

## 6. 集成方式

### 6.1 模块内嵌 Stats

```cpp
// include/tlm/cache_tlm.hh
#include "metrics/stats.hh"

class CacheTLM : public ChStreamModuleBase {
public:
    struct Stats : public tlm_stats::StatGroup {
        tlm_stats::Scalar& requests;
        tlm_stats::Scalar& hits;
        tlm_stats::Scalar& misses;
        tlm_stats::Distribution& latency;
        
        Stats(tlm_stats::StatGroup* parent)
            : tlm_stats::StatGroup("cache", parent),
              requests(addScalar("requests", "Total cache requests")),
              hits(addScalar("hits", "Cache hits")),
              misses(addScalar("misses", "Cache misses")),
              latency(addDistribution("latency", "Cache access latency"))
        {}
    } stats;

    CacheTLM(const std::string& name, EventQueue* eq, tlm_stats::StatGroup* parent)
        : ChStreamModuleBase(name, eq), stats(parent) {}

    void tick() override {
        // ... existing logic ...
        stats.requests++;
        if (hit) {
            stats.hits++;
        } else {
            stats.misses++;
        }
        stats.latency.sample(access_latency);
    }
};
```

### 6.2 ModuleFactory 集成

```cpp
// include/core/module_factory.hh
class ModuleFactory {
    std::unique_ptr<tlm_stats::StatGroup> _stats_root;
    
public:
    void enable_metrics();
    tlm_stats::StatGroup& stats_root();
    void dump_metrics(const std::string& path);
    void reset_metrics();
};

// 使用示例
ModuleFactory factory(&eq);
factory.enable_metrics();
factory.instantiateAll(config);  // 自动传递 stats parent
factory.dump_metrics("output/stats.txt");
```

### 6.3 JSON 配置扩展

```json
{
  "modules": [
    {
      "name": "traffic_gen_0",
      "type": "TrafficGenTLM",
      "config": {
        "pattern": "HOTSPOT",
        "hotspot_ratio": 0.8,
        "num_requests": 100000,
        "start_addr": "0x1000",
        "end_addr": "0x10000"
      }
    }
  ],
  "metrics": {
    "enabled": true,
    "dump_interval": 10000,
    "output_dir": "./metrics_output",
    "formats": ["text", "json", "markdown"]
  }
}
```

---

## 7. 三阶段仿真支持

```cpp
enum class SimPhase {
    WARMUP,      // 预热期 — 不记录统计
    MEASUREMENT, // 测量期 — 记录统计
    DRAIN        // 排空期 — 仅处理在途请求
};

class MetricsController {
    SimPhase phase_ = SimPhase::WARMUP;
    uint64_t warmup_cycles_;
    uint64_t measurement_cycles_;
    
public:
    bool should_record() const {
        return phase_ == SimPhase::MEASUREMENT;
    }
    
    void advance_phase(uint64_t current_cycle);
};

// 在模块 tick 中使用
void CacheTLM::tick() {
    if (metrics.should_record()) {
        stats.latency.sample(access_latency);
    }
    // 核心逻辑始终执行
    process_request();
}
```

---

## 8. 依赖分析

| 依赖 | 来源 | 用途 | 是否必需 |
|------|------|------|---------|
| 标准库 (`<atomic>`, `<vector>`, `<map>`) | C++ STL | 基础数据结构 | ✅ 必需 |
| HdrHistogram_c | 第三方 C 库 | 百分位直方图 | ⚠️ 可选（可用指数桶替代） |
| nlohmann/json | 已有依赖 | JSON 输出 | ✅ 已有 |

---

## 9. 性能开销评估

| 指标类型 | 内存开销 | CPU 开销 | 说明 |
|---------|---------|---------|------|
| Scalar | 8 bytes | 原子操作 | 可忽略 |
| Average | 24 bytes | 乘法 + 加法 | 可忽略 |
| Distribution | 40 bytes | min/max/sum 更新 | 可忽略 |
| PercentileHistogram | 5KB (HdrHistogram) | 记录值哈希 | < 1% |
| **总计** | **~5KB per module** | **< 2% overhead** | 专业级 |

---

## 10. 与现有架构的兼容性

| 现有组件 | 兼容性 | 说明 |
|---------|--------|------|
| EventQueue | ✅ 兼容 | 使用 `getCurrentCycle()` 作为时间基准 |
| StreamAdapter | ✅ 兼容 | Stats 在模块 tick() 中记录，不影响 Adapter |
| ModuleFactory | ✅ 兼容 | 新增 `enable_metrics()` 方法 |
| ch_stream | ✅ 兼容 | Stats 不修改握手协议 |
| JSON 配置 | ✅ 兼容 | 新增可选 `metrics` 字段 |
| 现有测试 | ✅ 兼容 | 默认 metrics 关闭，不影响现有测试 |

---

**维护**: CppTLM 开发团队  
**版本**: 2.2.0-draft  
**最后更新**: 2026-04-15