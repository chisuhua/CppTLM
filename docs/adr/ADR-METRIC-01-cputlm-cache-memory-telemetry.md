# ADR-METRIC-01: CPUTLM/CacheTLM/MemoryTLM Telemetry 统计收集框架

> **版本**: 1.0
> **日期**: 2026-06-23 (追溯补 — 实施早于 ADR)
> **状态**: ✅ 已实施 (commit `66d9674`, F10 阶段)
> **影响**: TLM 模块 stats 字段标准化 + MetricsReporter 集成验证点 + 后续 percentile/histogram 扩展基础
> **前置依赖**: `metrics/stats.hh` 框架 (已存在)

---

## 1. Context (背景)

### 1.1 Phase 7 之前的 telemetry 现状

| 模块 | stats 字段 | increment 点 | 缺失项 |
|------|------------|-------------|--------|
| **CacheTLM** | `stats_requests_` / `stats_hits_` / `stats_misses_` / `stats_requests_read_` / `stats_requests_write_` | `tick()` L85/97/99 | ❌ 无 latency 分布 |
| **MemoryTLM** | `stats_requests_` / `stats_requests_read_` / `stats_requests_write_` / `stats_latency_read_` / `stats_latency_write_` | `tick()` L77/80 | ❌ 无 bandwidth utilization |
| **CPUTLM** | **无** | **无** | ❌ 整个 stats group 缺失 |
| **TrafficGenTLM** | (待 Phase 7+) | — | — |

**后果**:
- `MetricsReporter::summary()` 输出 JSON 中 `system.cpu` group 不存在
- CPU 端 `transactions_issued` / `avg_response_latency` 无法被 telemetry 收集
- 用户无法回答"CPU 发了多少 request" / "端到端 latency 分布"等基础问题
- F1 P2.5 "性能 metric 断言" 因 F10 缺失无法验证 hit rate / latency 数值

### 1.2 F10 范围界定

F10 限于 **telemetry infrastructure** (stats 字段 + tick increment), **不**验证:
- ❌ `ModuleFactory::instantiateAll` Step 7 wiring 完整路径 (F11 P2.2 strengthen 已记录架构约束: CPUTLM/CacheTLM→MemoryTLM 直连在 current Step 7 下数据不流, 属 F12+ 范畴)
- ❌ CacheTLM hit rate 具体值断言 (F1.P2.5 范围已收窄为 "summary structure exists")
- ❌ MemoryTLM bandwidth utilization 计算 (F10.2 后续扩展)

### 1.3 与 F1.P2.5 的依赖关系

F1.P2.5 "性能 metric 断言" 在 F10 之前:
- ❌ `metrics_reporter.get_summary()` 无 `system.cpu` group → P2.5 必失败
- ❌ 无法断言 `stats_requests_issued_` ≥ 1

F10 之后:
- ✅ `metrics_reporter.get_summary()` 含 `system.cpu` group (3 个字段: requests_issued / requests_completed / latency)
- ✅ F1.P2.5 可断言 group 存在 + 字段结构, 不需具体数值 (F1 已 commit `d927fff`)

---

## 2. Decision (决策)

### 2.1 决策表

| 决策 | 选择 | 备选 | 理由 |
|------|------|------|------|
| **CPUTLM stats group 命名** | **`"cpu"`** | `"cpu_tlm"` / `"cpputlm"` | 与 CacheTLM (`"cache"`) / MemoryTLM (`"memory"`) 一致; 短且语义清晰 |
| **`get_stats_path()` 返回值** | **`"system.cpu"`** | `"cpu"` / `"system.cputlm"` | 与 CacheTLM(`system.cache`)/MemoryTLM(`system.memory`)/TrafficGenTLM(`system.traffic_gen`) 一致, 统一 `system.{type}` 命名空间 |
| **latency 类型** | **`tlm_stats::Distribution` (保留全部分布样本)** | `tlm_stats::Average` (仅均值) | 后续 percentile/histogram 扩展需全分布; Average 信息损失不可逆 |
| **CPUTLM 唯一补 stats** | **CPUTLM 缺 stats group, 仅补这一个** | CacheTLM/MemoryTLM 一起加新字段 | CacheTLM(`cache_tlm.hh:85/97/99`)/MemoryTLM(`memory_tlm.hh:77/80`) 已正确 increment, 重复加会双计数 |
| **不验证 Step 7 wiring** | **3 个 TEST_CASE 全部绕开 ModuleFactory** | 完整 Step 7 wiring 测试 | Step 7 当前不允许 CPUTLM→CacheTLM→MemoryTLM 直连数据流 (F11 风险); F12+ 架构级工作 |
| **`inflight_issue_cycles_` 跟踪** | **`std::unordered_map<uint64_t, uint64_t>` (txn_id → issue_cycle)** | 单个 `last_issue_cycle_` (仅最近 1 个) | CPUTLM MAX_INFLIGHT=4, 需跟踪多个 in-flight txn 的 issue cycle; txn_id 是天然 key |
| **`do_reset()` 清空 in-flight** | **清空 `inflight_issue_cycles_`** | 保留旧值 | reset 语义要求状态归零; 旧值会让后续 latency 计算错位 |

### 2.2 CPUTLM 实施细节

**新代码** (commit `66d9674`, `include/tlm/cpu_tlm.hh` +31 LOC, -3 LOC):

#### 2.2.1 stats group 定义

```cpp
private:
    // F10 telemetry: transactions_issued + latency distribution
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_requests_issued_;
    tlm_stats::Scalar& stats_requests_completed_;
    tlm_stats::Distribution& stats_latency_;
```

3 个字段语义:
- `stats_requests_issued_` (Scalar, unit=count) — CPU 发起的请求总数
- `stats_requests_completed_` (Scalar, unit=count) — CPU 收到的响应总数
- `stats_latency_` (Distribution, unit=cycle) — request-to-response 周期延迟完整分布

#### 2.2.2 构造函数初始化

```cpp
explicit CPUTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq),
      ...
      stats_("cpu", nullptr),
      stats_requests_issued_(stats_.addScalar("requests_issued", "Total CPU requests issued", "count")),
      stats_requests_completed_(stats_.addScalar("requests_completed", "Total CPU responses received", "count")),
      stats_latency_(stats_.addDistribution("latency", "CPU request-to-response latency", "cycle")) {}
```

#### 2.2.3 Override 接口

```cpp
std::string get_stats_path() const override { return "system.cpu"; }
tlm_stats::StatGroup* get_stats_group() override { return &stats_; }
```

#### 2.2.4 `tick()` increment 点

- **写 req_out 时**: `++stats_requests_issued_` + `inflight_issue_cycles_[txn_id] = current_cycle`
- **收 resp_in 时**: 计算 `latency = current_cycle - inflight_issue_cycles_[txn_id]` + `stats_latency_.sample(latency)` + `++stats_requests_completed_` + erase from map
- **`do_reset()`**: 清空 `inflight_issue_cycles_`

### 2.3 测试设计 (3 个 TEST_CASE, `test/test_f10_telemetry.cc`, +173 LOC)

| # | 名称 | 验证 | 绕开 |
|---|------|------|------|
| **F10.1** | `CPUTLM issues requests and stats increment` | CPUTLM 200 周期后 `stats_requests_issued_ >= 1`, `completed <= issued`, latency samples 可用 | Step 7 wiring (CPUTLM 直发 + 立即回 resp) |
| **F10.2** | `MemoryTLM stats infrastructure exists` | 直接构造 MemoryTLM 验证 stats group 注册 + 4 字段 (`requests_read/write` + `latency_read/write`) 存在 | Step 7 wiring |
| **F10.3** | `MetricsReporter JSONReporter summary contains TLM groups` | 验证 `system.cpu` / `system.memory` group 在 JSON output 中 | 端到端 E2E 流量 |

### 2.4 CacheTLM / MemoryTLM 不重复补

`git show 66d9674 -- include/tlm/cache_tlm.hh include/tlm/memory_tlm.hh` 验证:
- CacheTLM `tick()` L85/97/99 已 `++stats_requests_` / `++stats_hits_` / `++stats_misses_` / `++stats_requests_read_` / `++stats_requests_write_`
- MemoryTLM `tick()` L77/80 已 `++stats_requests_` / `++stats_requests_read_` / `++stats_requests_write_` / `stats_latency_read_` / `stats_latency_write_`

F10 仅补 CPUTLM 缺漏, 不动其他两类 (避免双计数 + 改动爆炸)。

---

## 3. Consequences (后果)

### 3.1 解锁的能力

- ✅ **CPUTLM stats 完整**: 3 字段 (issued / completed / latency) 覆盖 CPU 端 telemetry 全场景
- ✅ **`system.cpu` group 在 `MetricsReporter::summary()` JSON 中可见**: 用户能直接读 `summary()['system.cpu']`
- ✅ **F1.P2.5 "metric 结构断言" 可验证**: `get_summary()['system.cpu']` 含 3 字段
- ✅ **F10.2 percentile/histogram 扩展基础**: `Distribution` 类型保留全分布样本, 后续可加 `.percentile(0.99)` / `.histogram(bins=10)`
- ✅ **`test_f10_telemetry.cc` 3 个 TEST_CASE pass**: 203/203 → 703/703 (本 F10 +3)
- ✅ **命名空间统一**: `system.{cpu,cache,memory,traffic_gen}` 全 TLM 一致

### 3.2 已知技术债

- ⚠️ **Step 7 wiring 数据不流**: F10 测试绕开 ModuleFactory, 实际 `instantiateAll` Step 7 当前不允许 CPUTLM→CacheTLM→MemoryTLM 直连数据流 (F11 P2.2 strengthen 记录, F12+ 架构级修复)
- ⚠️ **MemoryTLM bandwidth utilization 未实现**: `stats_bandwidth_utilization_` 字段缺失, F10.2 后续扩展
- ⚠️ **CacheTLM 无 latency 分布**: 仅 hit/miss counts, 无 `stats_hit_latency_` / `stats_miss_latency_` 区分 (F10.2 扩展)
- ⚠️ **`do_reset()` 不重置 stats group**: 当前只清 `inflight_issue_cycles_`, 不 `stats_.reset()`. 多次 reset 仿真累积 stats. F10.2 决策点
- ⚠️ **TrafficGenTLM telemetry 缺失**: Phase 7+ 待加 (F10.3)

### 3.3 Phase 7+ 扩展点

- **F12 (Phase 7.B GpuComputeUnitTLM)**: GpuComputeUnitTLM/VectorRegFileTLM/WavefrontTLM 各加 stats group (`system.gpu.compute_unit` / `system.gpu.vector_regfile` / `system.gpu.wavefront`)
- **F13 (Phase 7.D TCC Bridge)**: TccTLM 加 snoop broadcast stats (`stats_snoops_issued_` / `stats_snoops_received_`)
- **F14 (Phase 7.E Multi-CU)**: `system.gpu.compute_unit.*` 数组命名 (per-CU)
- **F15 (Phase 7.F Demo)**: `test_apu_soc.py` 端到端 telemetry 验证 (CPU + GPU + Memory 综合 metric dashboard)
- **F10.2**: percentile / histogram / bandwidth utilization
- **F10.3**: TrafficGenTLM telemetry

---

## 4. References (参考)

### 4.1 设计来源

- **`docs/superpowers/plans/2026-06-20-future-work-roadmap.md` F10** — CPUTLM/CacheTLM/MemoryTLM 性能 metric 收集 (本 ADR 实施目标)
- **`include/metrics/stats.hh`** — `tlm_stats::StatGroup` / `Scalar` / `Distribution` 框架 (前置存在)
- **`include/metrics/metrics_reporter.hh`** — `summary()` JSON 输出 (F10.3 验证点)

### 4.2 关联 ADR

- **`ADR-LIB-01-cpptlm-library-python-higher-cluster-factories.md`** — F5 cpptlm.library 高级工厂 (工厂生成的 JSON 可立即被本 F10 telemetry 收集)
- **`ADR-INC-01-incorporate-parent-late-binding.md`** — P1 late-binding (本 F10 stats 在 wiring 之前增量, 是 wiring 验证前提)
- **`ADR-X.13-stub-multi-extension.md`** — 多 TLM 扩展 stub 标记 (snoop broadcast 依赖 multi-extension reset, 影响 latency 计算正确性)

### 4.3 实施追溯

- **F10 commit `66d9674`** — CPUTLM telemetry + F10 integration test (本 ADR 实施, 2026-06-23)
- **P1 commit `04399c8`** — ApuSoC late-binding (F10 测试绕开 Step 7, 后续 F12 集成)
- **P0 commit `fb56cc3`** — D.1 PortManager Mirror (stats group path 与 PortManager 命名空间对齐)

### 4.4 关联任务

- `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` F10 (本 ADR) + F1.P2.5 (依赖 F10) + F12/F13/F14 (扩展点)

---

## 5. Status Update

无（首次签发即已实施完成, 追溯补 ADR 仅为文档化, 设计决策与实施一致）。
