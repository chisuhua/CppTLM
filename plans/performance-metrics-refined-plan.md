# 性能指标收集框架 — 优化实施计划

> **文档状态**: ✅ 已批准  
> **创建日期**: 2026-04-15  
> **审查者**: Sisyphus-Junior  
> **优化版本**: 2.0（基于 v1.0 深度审查）  
> **架构文档**: `docs/architecture/08-metrics-collection-framework-v2.2.md`  
> **参考**: gem5 Stats / BookSim Metrics / Sniper Statistics

---

## 执行摘要

本计划优化了原始 6 阶段实施路线，识别出：
- **并行机会**: Phase 2 和 Phase 3 可与 Phase 1 并行开发
- **复杂度修正**: 总工时从 14h 调整为 **18-20h**
- **关键路径**: Phase 0 → Phase 1 → Phase 4 → Phase 5（串行 12h）
- **测试策略**: Phase 0-4 可独立测试，Phase 5 需完整系统

**关键变更**:
1. 增加 `StatGroup::findSubgroup()` 和 `reset()` 方法（Phase 0）
2. 强制 HdrHistogram fallback 方案（Phase 2）
3. 提前编写 Phase 5 测试框架（使用 Mock StatsManager）
4. 添加性能 benchmark 验证脚本

---

## 1. 阶段依赖关系（优化版）

```
关键路径（串行，12h）:
┌─────────────────────────────────────────────────────────────────┐
│ Phase 0 (4h) → Phase 1 (3h) → Phase 4 (3h) → Phase 5 (4h)      │
└─────────────────────────────────────────────────────────────────┘
                    ↓                    ↑
        ┌───────────┴──────┐    ┌────────┴────────┐
        │ Phase 2 (3-4h)   │    │ Phase 3 (2h)    │
        │ (并行路径 1)      │    │ (并行路径 2)     │
        └──────────────────┘    └─────────────────┘
```

**并行策略**:
- **Sprint 1** (Day 1 AM): Phase 0 + Phase 3 启动（2 agents 并行）
- **Sprint 2** (Day 1 PM): Phase 1 + Phase 2 启动（2 agents 并行）
- **Sprint 3** (Day 2 AM): Phase 4 启动（等待 Phase 1 完成）
- **Sprint 4** (Day 2 PM): Phase 5 启动（等待 Phase 4 完成）

**总工期**: 2 个工作日（4 sprints）

---

## 2. 详细阶段计划（原子任务级）

### Phase 0: 核心统计类型（4h）

**目标**: 实现 Scalar / Average / Distribution / StatGroup + 基础测试

#### 任务分解

| ID | 任务 | 文件 | 操作 | 复杂度 | 验收命令 |
|----|------|------|------|--------|---------|
| P0-T1 | Scalar 实现 | `include/metrics/stats.hh` | CREATE | S | `ctest -R stats_core` |
| P0-T2 | Average 实现 | `include/metrics/stats.hh` | CREATE | S | `ctest -R stats_core` |
| P0-T3 | Distribution 实现 | `include/metrics/stats.hh` | CREATE | M | `ctest -R stats_core` |
| P0-T4 | StatGroup 层次管理 | `include/metrics/stats.hh` | CREATE | L | `ctest -R stats_core` |
| P0-T5 | 单元测试 | `test/test_stats_core.cc` | CREATE | M | `ctest -R stats_core` |
| P0-T6 | CMake 集成 | `CMakeLists.txt` | MODIFY | S | `cmake --build build` |

#### 任务详情

**P0-T1: Scalar 实现**
```cpp
// 必须实现:
// - operator++(), operator+=(Counter)
// - value(), reset()
// - 原子操作支持多线程安全
```
- **验收标准**:
  - [ ] `Scalar` 自增 10000 次后 `value() == 10000`
  - [ ] `operator+=` 支持任意增量
  - [ ] 多线程并发自增无数据竞争（TSan 验证）
- **验证命令**: `ctest -R "stats_core.scalar" -V`

**P0-T2: Average 实现**
```cpp
// 必须实现:
// - time_weighted sample: sample(value, duration)
// - result(): 返回 sum / total_duration
// - reset()
```
- **验收标准**:
  - [ ] 时间加权平均计算正确（模拟 100 cycles，50 cycles 值为 10，50 cycles 值为 20 → avg=15）
  - [ ] `reset()` 后 sum 和 duration 清零
- **验证命令**: `ctest -R "stats_core.average" -V`

**P0-T3: Distribution 实现**
```cpp
// 必须实现:
// - sample(value): 更新 min/max/sum/sum_sq/count
// - mean(), stddev(), min(), max()
// - reset()
```
- **验收标准**:
  - [ ] 标准差计算正确（已知样本 [2,4,4,4,5,5,7,9] → stddev=2.0）
  - [ ] `min()` 和 `max()` 跟踪边界值
  - [ ] 零样本时 `mean()` 返回 0（非 NaN）
- **验证命令**: `ctest -R "stats_core.distribution" -V`

**P0-T4: StatGroup 层次管理**
```cpp
// 必须实现:
// - addScalar(), addDistribution()
// - addSubgroup(), findSubgroup(path)
// - dump(ostream), reset()
// - 递归层次遍历
```
- **验收标准**:
  - [ ] `findSubgroup("system.cache")` 返回正确嵌套组
  - [ ] `dump()` 输出 gem5 风格对齐格式
  - [ ] `reset()` 递归重置所有子组和统计项
  - [ ] 层次路径支持 `.` 分隔符语法
- **验证命令**: `ctest -R "stats_core.statgroup" -V`

**P0-T5: 单元测试**
- **测试覆盖**:
  - Scalar: 5 tests (构造，自增，累加，reset, 线程安全)
  - Average: 4 tests (均匀采样，时间加权，边界，reset)
  - Distribution: 6 tests (基础统计，标准差，min/max, 零样本，reset, 大数溢出)
  - StatGroup: 6 tests (构造，嵌套，查找，dump, reset, 层次遍历)
- **验收标准**:
  - [ ] 覆盖率 > 95%（`gcov` 验证）
  - [ ] 零编译警告（`-Wall -Wextra -Wpedantic`）
- **验证命令**: `ctest -R stats_core --output-on-failure`

**P0-T6: CMake 集成**
- **修改**:
  ```cmake
  # CMakeLists.txt
  target_include_directories(cpptlm_core PUBLIC ${CMAKE_SOURCE_DIR}/include/metrics)
  ```
- **验收标准**:
  - [ ] `cmake --build build` 零错误
  - [ ] `include/metrics/` 添加到 PUBLIC include dirs
- **验证命令**: `cmake --build build --target cpptlm_core`

#### 交付物

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/metrics/stats.hh` | ~250 | 核心统计类型（头文件实现） |
| `test/test_stats_core.cc` | ~200 | 单元测试 |
| `CMakeLists.txt` | +5 | include 路径 |

#### 依赖

- **无**（Phase 0 是基础，无前置依赖）

#### 风险

- **中**: `Distribution` 方差计算可能溢出（缓解：使用 `long double` 或 Kahan 求和）

---

### Phase 1: 模块 Stats 集成（3h）

**目标**: 为 CacheTLM / CrossbarTLM / MemoryTLM 添加 Stats 结构体 + 采集点

#### 任务分解

| ID | 任务 | 文件 | 操作 | 复杂度 | 验收命令 |
|----|------|------|------|--------|---------|
| P1-T1 | CacheTLM Stats | `include/tlm/cache_tlm.hh` | MODIFY | M | `ctest -R module_stats.cache` |
| P1-T2 | CrossbarTLM Stats | `include/tlm/crossbar_tlm.hh` | MODIFY | M | `ctest -R module_stats.crossbar` |
| P1-T3 | MemoryTLM Stats | `include/tlm/memory_tlm.hh` | MODIFY | M | `ctest -R module_stats.memory` |
| P1-T4 | ModuleFactory 集成 | `include/core/module_factory.hh` | MODIFY | S | `ctest -R module_stats` |
| P1-T5 | EventQueue 关联 | `include/core/event_queue.hh` | MODIFY | S | 编译验证 |
| P1-T6 | 集成测试 | `test/test_module_stats.cc` | CREATE | M | `ctest -R module_stats` |

#### 任务详情

**P1-T1: CacheTLM Stats**
```cpp
// 采集点:
// - tick() 入口：requests++
// - 命中判断：hits++ 或 misses++
// - 延迟采样：latency.sample(access_latency)
// - 带宽：bandwidth_read.sample(bytes, cycles)
```
- **验收标准**:
  - [ ] 100 次请求后 `stats.requests.value() == 100`
  - [ ] 命中率计算正确（85 hits / 100 requests = 0.85）
  - [ ] 延迟分布包含 min/avg/max/stddev
  - [ ] `#ifdef ENABLE_METRICS` 包裹所有 stats 代码
- **验证命令**: `ctest -R "module_stats.cache" -V`

**P1-T2: CrossbarTLM Stats**
```cpp
// 采集点:
// - req_in[i].valid(): flits_received++
// - resp_out[dst].write(): flits_sent++
// - tick 开始到结束：flit_latency.sample(delta_cycles)
// - buffer 占用：buffer_occupancy.sample(queue_size)
```
- **验收标准**:
  - [ ] 4 端口独立统计（每个端口 `flits_received` 独立）
  - [ ] 路由延迟正确采样
  - [ ] 反压周期计数正确
- **验证命令**: `ctest -R "module_stats.crossbar" -V`

**P1-T3: MemoryTLM Stats**
```cpp
// 采集点:
// - tick() 读请求：requests_read++
// - tick() 写请求：requests_write++
// - 行缓冲命中：row_hits++ 或 row_misses++
// - 读/写延迟：latency_read.sample(), latency_write.sample()
```
- **验收标准**:
  - [ ] 读/写请求分别统计
  - [ ] 行缓冲命中率计算正确
  - [ ] 读延迟和写延迟独立分布
- **验证命令**: `ctest -R "module_stats.memory" -V`

**P1-T4: ModuleFactory 集成**
```cpp
// 新增方法:
// - enable_metrics(bool enabled = true)
// - stats_root(): 返回 StatGroup*
// - dump_metrics(const std::string& path)
// - reset_metrics()
```
- **验收标准**:
  - [ ] `enable_metrics(false)` 时所有 stats 代码被编译期优化掉
  - [ ] `dump_metrics()` 输出 gem5 格式到文件
  - [ ] `reset_metrics()` 调用所有 StatGroup::reset()
- **验证命令**: `ctest -R "module_stats.factory" -V`

**P1-T5: EventQueue 关联**
```cpp
// EventQueue 新增:
// - getCurrentCycle() const: uint64_t
// - 用于 stats 时间戳
```
- **验收标准**:
  - [ ] `getCurrentCycle()` 返回当前仿真周期
  - [ ] 零开销（inline 函数）
- **验证命令**: `cmake --build build`

**P1-T6: 集成测试**
- **测试场景**:
  - CacheTLM: 100 次访问，验证命中率 85%
  - CrossbarTLM: 4 端口各 50 次请求，验证路由统计
  - MemoryTLM: 读/写各 50 次，验证行缓冲命中率
  - ModuleFactory: enable/disable metrics 切换
- **验收标准**:
  - [ ] 所有模块 stats 正确采集
  - [ ] `factory.dump_metrics()` 输出正确格式
  - [ ] 现有 233 个测试用例全部通过
- **验证命令**: `ctest -R module_stats --output-on-failure && ctest -R "[phase6]"`

#### 交付物

| 文件 | 修改量 | 说明 |
|------|--------|------|
| `include/tlm/cache_tlm.hh` | +40 行 | Stats 结构体 + 采集点 |
| `include/tlm/crossbar_tlm.hh` | +50 行 | Stats 结构体 + 采集点 |
| `include/tlm/memory_tlm.hh` | +50 行 | Stats 结构体 + 采集点 |
| `include/core/module_factory.hh` | +30 行 | metrics 管理方法 |
| `include/core/event_queue.hh` | +5 行 | getCurrentCycle() |
| `test/test_module_stats.cc` | ~150 行 | 集成测试 |

#### 依赖

- **Phase 0 完成**（需要 `StatGroup`, `Scalar`, `Distribution`）

#### 风险

- **高**: 编译期 `#ifdef ENABLE_METRICS` 可能遗漏某些采集点（缓解：添加编译期 assert）
- **中**: stats 代码可能影响现有逻辑时序（缓解：所有 stats 代码放在业务逻辑后）

---

### Phase 2: PercentileHistogram（3-4h）

**目标**: 实现百分位延迟分析（HdrHistogram 或 指数桶 fallback）

#### 任务分解

| ID | 任务 | 文件 | 操作 | 复杂度 | 验收命令 |
|----|------|------|------|--------|---------|
| P2-T1 | HdrHistogram 集成 | `external/hdr_histogram/` | CREATE | M | `ctest -R histogram` |
| P2-T2 | PercentileHistogram 封装 | `include/metrics/histogram.hh` | CREATE | L | `ctest -R histogram` |
| P2-T3 | 指数桶 fallback | `include/metrics/histogram.hh` | CREATE | M | `ctest -R histogram` |
| P2-T4 | 单元测试 | `test/test_percentile_histogram.cc` | CREATE | M | `ctest -R histogram` |
| P2-T5 | 模块集成 | `include/tlm/cache_tlm.hh` | MODIFY | S | 编译验证 |

#### 任务详情

**P2-T1: HdrHistogram 集成**
```bash
# 获取 header-only 版本
# https://github.com/HdrHistogram/HdrHistogram_c
# 放置在 external/hdr_histogram/hdr_histogram.h
```
- **验收标准**:
  - [ ] `external/hdr_histogram/hdr_histogram.h` 存在
  - [ ] CMake 能正确编译链接
  - [ ] 内存占用 < 5KB per histogram
- **验证命令**: `cmake --build build --target cpptlm_core`

**P2-T2: PercentileHistogram 封装**
```cpp
// 必须实现:
// - record(int64_t value)
// - percentile(double p): 返回 p 分位值
// - p50(), p95(), p99(), p99_9()
// - reset(), memory_usage()
```
- **验收标准**:
  - [ ] p50/p95/p99/p99.9 计算误差 < 1%
  - [ ] 支持值范围：1 cycle ~ 1 hour (3.6M cycles)
  - [ ] `memory_usage()` 返回实际占用字节数
- **验证命令**: `ctest -R "histogram.percentile" -V`

**P2-T3: 指数桶 fallback**
```cpp
// 当 HdrHistogram 不可用时:
// - 32 个指数桶：[1-2), [2-4), [4-8), ..., [2^31, 2^32)
// - 内存占用：256 bytes
// - 精度：桶边界附近误差 < 5%
```
- **验收标准**:
  - [ ] `#ifdef USE_HDR_HISTOGRAM` 条件编译
  - [ ] fallback 版本 API 兼容
  - [ ] 32 桶覆盖完整值范围
- **验证命令**: `cmake --build build -DUSE_HDR_HISTOGRAM=OFF`

**P2-T4: 单元测试**
- **测试覆盖**:
  - 基础记录：单值，多值，重复值
  - 百分位计算：已知分布验证 p50/p95/p99
  - 边界条件：零样本，单样本，溢出值
  - 内存验证：`memory_usage() < 5KB`
  - fallback 模式：禁用 HdrHistogram 时正常工作
- **验收标准**:
  - [ ] 覆盖率 > 90%
  - [ ] p99 误差 < 1%（对比理论值）
- **验证命令**: `ctest -R histogram --output-on-failure`

**P2-T5: 模块集成**
```cpp
// CacheTLM 新增:
// tlm_stats::PercentileHistogram& latency_hist;
// 在 tick() 中：latency_hist.record(access_latency)
```
- **验收标准**:
  - [ ] CacheTLM 能记录延迟直方图
  - [ ] `dump_metrics()` 包含 p50/p95/p99
- **验证命令**: `cmake --build build`

#### 交付物

| 文件 | 行数 | 说明 |
|------|------|------|
| `external/hdr_histogram/hdr_histogram.h` | ~500 | HdrHistogram header-only |
| `include/metrics/histogram.hh` | ~150 | PercentileHistogram 封装 |
| `test/test_percentile_histogram.cc` | ~120 | 单元测试 |
| `include/tlm/cache_tlm.hh` | +10 | 集成 latency_hist |

#### 依赖

- **Phase 0 完成**（需要 `StatGroup`）

#### 风险

- **高**: HdrHistogram 可能与现有 CMake 配置冲突（缓解：预置 fallback 方案）
- **低**: 指数桶精度不足（缓解：文档说明精度限制）

---

### Phase 3: TrafficGenTLM 压力模式（2h）

**目标**: 扩展 TrafficGenTLM 支持 6 种压力模式

#### 任务分解

| ID | 任务 | 文件 | 操作 | 复杂度 | 验收命令 |
|----|------|------|------|--------|---------|
| P3-T1 | 压力模式策略接口 | `include/tlm/stress_patterns.hh` | CREATE | M | `ctest -R stress_patterns` |
| P3-T2 | TrafficGenTLM 扩展 | `include/tlm/traffic_gen_tlm.hh` | MODIFY | M | `ctest -R stress_patterns` |
| P3-T3 | 6 种模式实现 | `src/tlm/stress_patterns.cc` | CREATE | L | `ctest -R stress_patterns` |
| P3-T4 | 单元测试 | `test/test_stress_patterns.cc` | CREATE | M | `ctest -R stress_patterns` |
| P3-T5 | JSON 配置示例 | `configs/stress_*.json` | CREATE | S | 编译验证 |

#### 任务详情

**P3-T1: 压力模式策略接口**
```cpp
// 必须实现:
// - enum class StressPattern { SEQUENTIAL, RANDOM, HOTSPOT, STRIDED, NEIGHBOR, TORNADO }
// - class StressPatternStrategy (抽象基类)
//   - next_address(uint64_t base, uint64_t range) = 0
```
- **验收标准**:
  - [ ] 6 种模式枚举定义
  - [ ] 策略接口支持多态
  - [ ] 工厂方法 `createStrategy(StressPattern)`
- **验证命令**: `cmake --build build`

**P3-T2: TrafficGenTLM 扩展**
```cpp
// 新增方法:
// - set_pattern(StressPattern)
// - set_hotspot_config(addrs, weights)
// - set_strided_config(stride)
// - set_tornado_config(mesh_width, mesh_height)
```
- **验收标准**:
  - [ ] `set_pattern()` 切换模式
  - [ ] HOTSPOT 配置支持动态权重
  - [ ] TORNADO 配置支持 mesh 拓扑参数
- **验证命令**: `cmake --build build`

**P3-T3: 6 种模式实现**

**SEQUENTIAL**:
```cpp
uint64_t current = base;
uint64_t next_address(uint64_t base, uint64_t range) override {
    uint64_t ret = current;
    current = base + ((current - base + 1) % range);
    return ret;
}
```

**RANDOM**:
```cpp
std::random_device rd;
std::mt19937_64 gen(rd());
std::uniform_int_distribution<uint64_t> dist(0, range-1);

uint64_t next_address(uint64_t base, uint64_t range) override {
    return base + dist(gen);
}
```

**HOTSPOT**:
```cpp
// 80% 流量集中 20% 地址
std::discrete_distribution<> hotspot_dist(weights);
std::vector<uint64_t> hotspot_addrs;

uint64_t next_address(uint64_t base, uint64_t range) override {
    return hotspot_addrs[hotspot_dist(gen)];
}
```

**STRIDED**:
```cpp
uint64_t stride = 64; // 缓存行大小
uint64_t offset = 0;

uint64_t next_address(uint64_t base, uint64_t range) override {
    uint64_t ret = base + offset;
    offset = (offset + stride) % range;
    return ret;
}
```

**NEIGHBOR**:
```cpp
// 80% 访问相邻地址，20% 随机跳转
uint64_t last_addr = base;

uint64_t next_address(uint64_t base, uint64_t range) override {
    if (gen() % 100 < 80) {
        return last_addr + 1; // 相邻
    } else {
        last_addr = base + (gen() % range);
        return last_addr;
    }
}
```

**TORNADO**:
```cpp
// 对角流量模式 (mesh NoC)
uint64_t mesh_width, mesh_height;

uint64_t next_address(uint64_t base, uint64_t range) override {
    // 从 (x, y) 到 (mesh_width-1-x, mesh_height-1-y)
    // 实现对角流量
}
```

- **验收标准**:
  - [ ] 6 种模式全部实现
  - [ ] HOTSPOT 模式通过卡方检验（p-value > 0.05）
  - [ ] STRIDED 模式支持可配置步长
  - [ ] TORNADO 模式支持 mesh 参数
- **验证命令**: `ctest -R "stress_patterns.strategy" -V`

**P3-T4: 单元测试**
- **测试覆盖**:
  - SEQUENTIAL: 验证线性递增
  - RANDOM: 验证均匀分布（卡方检验）
  - HOTSPOT: 验证 80/20 分布（卡方检验）
  - STRIDED: 验证固定步长
  - NEIGHBOR: 验证局部性（80% 相邻）
  - TORNADO: 验证对角模式
- **验收标准**:
  - [ ] 每种模式 5+ tests
  - [ ] 卡方检验 p-value > 0.05
- **验证命令**: `ctest -R stress_patterns --output-on-failure`

**P3-T5: JSON 配置示例**
```json
// configs/stress_hotspot.json
{
  "modules": [
    {
      "name": "tg_hot",
      "type": "TrafficGenTLM",
      "config": {
        "pattern": "HOTSPOT",
        "hotspot_addrs": ["0x2000", "0x3000", "0x4000"],
        "hotspot_weights": [50, 30, 20],
        "num_requests": 100000
      }
    }
  ]
}
```
- **验收标准**:
  - [ ] 配置文件可被 ModuleFactory 加载
  - [ ] 模式参数正确传递
- **验证命令**: `ctest -R "json_config"`

#### 交付物

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/tlm/stress_patterns.hh` | ~100 | 策略接口 + 枚举 |
| `src/tlm/stress_patterns.cc` | ~200 | 6 种模式实现 |
| `include/tlm/traffic_gen_tlm.hh` | +30 | set_pattern() 等方法 |
| `test/test_stress_patterns.cc` | ~150 | 单元测试 |
| `configs/stress_hotspot.json` | ~15 | HOTSPOT 配置 |
| `configs/stress_strided.json` | ~10 | STRIDED 配置 |

#### 依赖

- **无**（Phase 3 完全独立）

#### 风险

- **低**: 随机数生成器种子问题（缓解：使用 `std::random_device` 初始化）

---

### Phase 4: StatsManager + 报告生成（3h）

**目标**: 实现全局统计管理 + 多格式报告输出

#### 任务分解

| ID | 任务 | 文件 | 操作 | 复杂度 | 验收命令 |
|----|------|------|------|--------|---------|
| P4-T1 | StatsManager 单例 | `include/metrics/stats_manager.hh` | CREATE | M | `ctest -R stats_manager` |
| P4-T2 | MetricsReporter | `include/metrics/metrics_reporter.hh` | CREATE | L | `ctest -R reporter` |
| P4-T3 | Text 输出生成 | `src/metrics/metrics_reporter.cc` | CREATE | M | `ctest -R reporter.text` |
| P4-T4 | JSON 输出生成 | `src/metrics/metrics_reporter.cc` | CREATE | M | `ctest -R reporter.json` |
| P4-T5 | Markdown 输出生成 | `src/metrics/metrics_reporter.cc` | CREATE | M | `ctest -R reporter.markdown` |
| P4-T6 | 单元测试 | `test/test_metrics_reporter.cc` | CREATE | M | `ctest -R reporter` |

#### 任务详情

**P4-T1: StatsManager 单例**
```cpp
// 必须实现:
// - static StatsManager& instance()
// - register_group(StatGroup*, const std::string& path)
// - find_group(const std::string& path)
// - dump_all(ostream)
// - reset_all()
```
- **验收标准**:
  - [ ] 单例模式线程安全（C++11 magic static）
  - [ ] `register_group()` 支持层次路径
  - [ ] `find_group("system.cache")` 返回正确组
  - [ ] `dump_all()` 递归输出所有统计
- **验证命令**: `ctest -R "stats_manager.instance" -V`

**P4-T2: MetricsReporter**
```cpp
// 必须实现:
// - generate_text(path)
// - generate_json(path)
// - generate_markdown(path)
// - generate_all(output_dir)
```
- **验收标准**:
  - [ ] 3 种格式输出 API 统一
  - [ ] `generate_all()` 原子性（失败时回滚）
  - [ ] 输出目录不存在时自动创建
- **验证命令**: `ctest -R "reporter.api" -V`

**P4-T3: Text 输出生成**
```text
# gem5 风格输出
---------- Begin Simulation Statistics ----------
system.sim_cycles                               100000
system.cache.requests                            10000
system.cache.hits                                 8500
system.cache.miss_rate                          0.1500
---------- End Simulation Statistics ----------
```
- **验收标准**:
  - [ ] 列对齐（使用 `std::setw()`）
  - [ ] 包含注释说明（`# description`）
  - [ ] 单位标注（`Cycle`, `Count`, `Ratio`）
- **验证命令**: `ctest -R "reporter.text" -V`

**P4-T4: JSON 输出生成**
```json
{
  "simulation": { "sim_cycles": 100000 },
  "cache": {
    "requests": 10000,
    "hits": 8500,
    "miss_rate": 0.15,
    "latency": { "avg": 45.3, "p99": 380 }
  }
}
```
- **验收标准**:
  - [ ] 完整层次结构
  - [ ] 使用 nlohmann/json 库
  - [ ] 数值类型正确（integer vs float）
- **验证命令**: `ctest -R "reporter.json" -V`

**P4-T5: Markdown 输出生成**
```markdown
# Performance Metrics Report

**Simulation Duration:** 100,000 cycles

## Cache Performance

| Metric | Value |
|--------|-------|
| Hit Rate | 85.00% |
| Avg Latency | 45.3 cycles |
| P99 Latency | 380 cycles |

## Latency Distribution

```
  0-50    ████████████████████████████████ (65%)
 50-100   ████████████ (22%)
100-200   ████ (8%)
200-400   ██ (4%)
400+      █ (1%)
```
```
- **验收标准**:
  - [ ] Markdown 表格格式正确
  - [ ] ASCII 直方图比例准确
  - [ ] 支持中文注释（可选）
- **验证命令**: `ctest -R "reporter.markdown" -V`

**P4-T6: 单元测试**
- **测试覆盖**:
  - StatsManager: 单例，注册，查找，dump, reset
  - MetricsReporter: 3 种格式输出，原子性，目录创建
  - Text 格式: 对齐，注释，单位
  - JSON 格式: 层次，类型，转义
  - Markdown 格式: 表格，直方图
- **验收标准**:
  - [ ] 覆盖率 > 90%
  - [ ] 输出格式验证（字符串匹配）
- **验证命令**: `ctest -R reporter --output-on-failure`

#### 交付物

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/metrics/stats_manager.hh` | ~80 | StatsManager 单例 |
| `include/metrics/metrics_reporter.hh` | ~50 | 报告 API |
| `src/metrics/metrics_reporter.cc` | ~200 | 3 种格式实现 |
| `test/test_metrics_reporter.cc` | ~150 | 单元测试 |

#### 依赖

- **Phase 0 完成**（需要 `StatGroup`）
- **Phase 1 完成**（需要模块 stats 实例）

#### 风险

- **中**: Markdown ASCII 直方图格式化复杂（缓解：简化为固定宽度条形）

---

### Phase 5: 压力测试基础设施（4h）

**目标**: 完整的压力测试框架 + E2E 验证

#### 任务分解

| ID | 任务 | 文件 | 操作 | 复杂度 | 验收命令 |
|----|------|------|------|--------|---------|
| P5-T1 | 测试框架核心 | `test/test_stress_framework.cc` | CREATE | L | `ctest -R stress.framework` |
| P5-T2 | 场景化测试 | `test/test_stress_scenarios.cc` | CREATE | L | `ctest -R stress.scenarios` |
| P5-T3 | Mock StatsManager | `test/mock_stats_manager.hh` | CREATE | S | 编译验证 |
| P5-T4 | JSON 配置 | `configs/stress_full_system.json` | CREATE | S | 编译验证 |
| P5-T5 | 结果输出目录 | `test/stress_results/` | CREATE | S | 编译验证 |
| P5-T6 | CI 集成 | `.github/workflows/ci.yml` | MODIFY | S | GitHub Actions 验证 |

#### 任务详情

**P5-T1: 测试框架核心**
```cpp
// 必须实现:
// - StressTestRunner 类
//   - load_config(json)
//   - run_simulation(cycles)
//   - verify_metrics(expected)
//   - generate_report()
// - 自动报告生成
```
- **验收标准**:
  - [ ] 支持 JSON 配置加载
  - [ ] 自动运行仿真指定周期
  - [ ] 自动验证指标是否符合预期
  - [ ] 自动生成报告到 `test/stress_results/`
- **验证命令**: `ctest -R "stress.framework" -V`

**P5-T2: 场景化测试**

| 场景 | 模式 | 规模 | 验证目标 |
|------|------|------|---------|
| `stress_hotspot_2cpu` | HOTSPOT | 100K cycles | 热点延迟 > 均匀延迟 20% |
| `stress_strided_cache` | STRIDED (64B) | 100K cycles | 缓存未命中率 > 50% |
| `stress_random_full` | RANDOM | 500K cycles | 无丢包，延迟稳定 |
| `stress_mixed_4cpu` | MIXED (4 模式) | 1M cycles | 全系统稳定运行 |
| `stress_backpressure` | SEQUENTIAL burst | 100K cycles | 反压机制正确工作 |

- **验收标准**:
  - [ ] 5 个场景全部实现
  - [ ] 每个场景有明确 pass/fail 判定
  - [ ] 测试时间 < 5 分钟（CI 友好）
- **验证命令**: `ctest -R stress.scenarios --output-on-failure`

**P5-T3: Mock StatsManager**
```cpp
// 用于 Phase 5 早期开发（不依赖 Phase 4 完成）
// - 模拟 register_group(), dump_all()
// - 验证测试框架逻辑
```
- **验收标准**:
  - [ ] Mock 与真实 StatsManager API 兼容
  - [ ] 可独立测试框架逻辑
- **验证命令**: `cmake --build build`

**P5-T4: JSON 配置**
```json
// configs/stress_full_system.json
{
  "modules": [
    {"name": "cpu0", "type": "TrafficGenTLM", "config": {...}},
    {"name": "cache", "type": "CacheTLM"},
    {"name": "xbar", "type": "CrossbarTLM"},
    {"name": "mem", "type": "MemoryTLM"}
  ],
  "metrics": {
    "enabled": true,
    "output_dir": "./test/stress_results"
  }
}
```
- **验收标准**:
  - [ ] 配置可被 ModuleFactory 加载
  - [ ] metrics 配置正确传递
- **验证命令**: `ctest -R "json_config"`

**P5-T5: 结果输出目录**
```bash
test/stress_results/
├── stress_hotspot_2cpu/
│   ├── metrics.txt
│   ├── metrics.json
│   └── metrics.md
├── stress_strided_cache/
│   └── ...
└── README.md
```
- **验收标准**:
  - [ ] 目录结构正确
  - [ ] 每次测试生成独立子目录
  - [ ] README 说明结果解读
- **验证命令**: `ls -la test/stress_results/`

**P5-T6: CI 集成**
```yaml
# .github/workflows/ci.yml
- name: Run stress tests
  run: |
    ctest -R stress --output-on-failure
    timeout 300 ./build/bin/cpptlm_tests "[stress]"
```
- **验收标准**:
  - [ ] GitHub Actions 自动运行
  - [ ] 超时保护（5 分钟）
  - [ ] 失败时上传结果文件
- **验证命令**: `ctest -R stress`

#### 交付物

| 文件 | 行数 | 说明 |
|------|------|------|
| `test/test_stress_framework.cc` | ~200 | 测试框架核心 |
| `test/test_stress_scenarios.cc` | ~250 | 5 个场景测试 |
| `test/mock_stats_manager.hh` | ~50 | Mock StatsManager |
| `configs/stress_full_system.json` | ~30 | 全系统配置 |
| `test/stress_results/README.md` | ~20 | 结果说明 |
| `.github/workflows/ci.yml` | +15 | CI 集成 |

#### 依赖

- **Phase 4 完成**（需要 MetricsReporter）
- **Phase 0-3 完成**（需要完整 stats 系统）

#### 风险

- **高**: 5 分钟 CI 约束可能导致测试规模受限（缓解：优化测试规模，使用抽样）
- **中**: 结果文件过大（缓解：限制报告大小，压缩上传）

---

## 3. 验收标准总结

| 阶段 | 完成标志 | 验证命令 |
|------|---------|---------|
| **Phase 0** | Scalar/Average/Distribution/StatGroup 测试 100% 通过 | `ctest -R stats_core` |
| **Phase 1** | 3 个模块采集正确 + 233 现有测试通过 | `ctest -R module_stats && ctest -R "[phase6]"` |
| **Phase 2** | p50/p95/p99 误差 < 1% + HdrHistogram fallback | `ctest -R histogram` |
| **Phase 3** | 6 种压力模式 + 卡方检验通过 | `ctest -R stress_patterns` |
| **Phase 4** | 3 种格式报告有效输出 + 原子性保证 | `ctest -R reporter` |
| **Phase 5** | 5 个压力场景全部通过 + CI < 5 分钟 | `ctest -R stress && timeout 300` |

---

## 4. 提交策略

每个阶段作为独立原子提交：

| 提交 | 消息 | 文件数 | 验证命令 |
|------|------|--------|---------|
| 1 | `feat(P8-0): 核心统计类型 — Scalar/Average/Distribution/StatGroup` | 3 | `ctest -R stats_core` |
| 2 | `feat(P8-1): 模块 Stats 集成 — CacheTLM/CrossbarTLM/MemoryTLM` | 7 | `ctest -R module_stats` |
| 3 | `feat(P8-2): PercentileHistogram — HdrHistogram + 指数桶 fallback` | 4 | `ctest -R histogram` |
| 4 | `feat(P8-3): TrafficGenTLM 压力模式 — 6 种策略 + 卡方检验` | 6 | `ctest -R stress_patterns` |
| 5 | `feat(P8-4): 报告生成 — Text/JSON/Markdown 三格式` | 4 | `ctest -R reporter` |
| 6 | `feat(P8-5): 压力测试基础设施 — 5 场景 E2E + CI 集成` | 6 | `ctest -R stress` |

---

## 5. 并行执行计划

**Sprint 1 (Day 1 AM, 4h)**:
- **Agent A**: Phase 0 (P0-T1 ~ P0-T6)
- **Agent B**: Phase 3 (P3-T1 ~ P3-T5)

**Sprint 2 (Day 1 PM, 4h)**:
- **Agent A**: Phase 1 (P1-T1 ~ P1-T6)
- **Agent B**: Phase 2 (P2-T1 ~ P2-T5)

**Sprint 3 (Day 2 AM, 3h)**:
- **Agent A**: Phase 4 (P4-T1 ~ P4-T6)
- **Agent B**: Phase 5 提前准备 (P5-T3 Mock, P5-T4 JSON, P5-T5 目录)

**Sprint 4 (Day 2 PM, 4h)**:
- **Agent A**: Phase 5 (P5-T1 ~ P5-T6)
- **Agent B**: 端到端验证 + 文档整理

**总工期**: 2 工作日（16h 实际开发）

---

## 6. 风险矩阵

| 风险 | 影响 | 概率 | 缓解措施 | 负责人 |
|------|------|------|---------|--------|
| HdrHistogram 编译失败 | Phase 2 阻塞 | 中 | 预置指数桶 fallback | Agent B |
| Stats 集成影响性能 | 仿真变慢 > 2% | 中 | `#ifdef ENABLE_METRICS` + benchmark | Agent A |
| 内存溢出 (> 10KB/模块) | 仿真崩溃 | 低 | 在线聚合，不存样本 | Agent A |
| 现有测试破坏 | 回归失败 | 高 | Phase 1 后立即运行 233 测试 | Agent A |
| CI 超时 (> 5 分钟) | 集成失败 | 中 | 优化测试规模，抽样验证 | Agent B |
| Phase 4 阻塞 Phase 5 | 工期延误 | 中 | 提前准备 Mock StatsManager | Agent B |

---

## 7. 附录：文件清单

### 新增文件（20 个）

```
include/metrics/
├── stats.hh                    # Phase 0
├── histogram.hh                # Phase 2
├── stats_manager.hh            # Phase 4
└── metrics_reporter.hh         # Phase 4

src/metrics/
└── metrics_reporter.cc         # Phase 4

external/hdr_histogram/
└── hdr_histogram.h             # Phase 2

include/tlm/
├── stress_patterns.hh          # Phase 3
└── traffic_gen_tlm.hh          # Phase 3 (modify)

src/tlm/
└── stress_patterns.cc          # Phase 3

test/
├── test_stats_core.cc          # Phase 0
├── test_module_stats.cc        # Phase 1
├── test_percentile_histogram.cc # Phase 2
├── test_stress_patterns.cc     # Phase 3
├── test_metrics_reporter.cc    # Phase 4
├── test_stress_framework.cc    # Phase 5
├── test_stress_scenarios.cc    # Phase 5
└── mock_stats_manager.hh       # Phase 5

configs/
├── stress_hotspot.json         # Phase 3
├── stress_strided.json         # Phase 3
└── stress_full_system.json     # Phase 5

test/stress_results/
└── README.md                   # Phase 5
```

### 修改文件（8 个）

```
include/tlm/
├── cache_tlm.hh                # Phase 1, 2
├── crossbar_tlm.hh             # Phase 1
└── memory_tlm.hh               # Phase 1

include/core/
├── module_factory.hh           # Phase 1
└── event_queue.hh              # Phase 1

CMakeLists.txt                  # Phase 0
.github/workflows/ci.yml        # Phase 5
```

---

**维护**: CppTLM 开发团队  
**版本**: 2.0  
**最后更新**: 2026-04-15  
**审查者**: Sisyphus-Junior  
**批准状态**: ✅ 已批准（可立即执行）
