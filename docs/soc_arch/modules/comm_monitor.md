# comm_monitor 微架构文档

> **类别**: Interconnect > CommMonitor
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/interconnect/comm_monitor_tlm.hh`
> **蓝图来源**: gem5 `src/mem/comm_monitor.hh`（流量监控/带宽/延迟/outstanding）
> **首版 commit**: 蓝图（来自调研 §2.4）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4
> - 邻接: [interconnect-crossbar.md](./interconnect-crossbar.md) (v0 crossbar)

---

## 1. 设计目标（蓝图）

`tlm::CommMonitorTLM` 是 CppTLM v2.2+ 规划的 **流量监控器**（profiling pass-through module），用于在两个模块间插入透明探针，收集带宽/延迟/outstanding 计数等性能统计。**与 gem5 对位**: `gem5::CommMonitor`（~600 行，snooping 无干扰）。

**核心特征**：
- **透明探针**（不影响数据流，仅记录）
- **三层粒度**（system-wide / per-master / per-transaction）
- **关键指标**：bandwidth, latency, outstanding transactions, transaction count
- **可选压缩采样**（v0 简化：全量采集；Phase 7+ 加 sampling）
- **单端口 + 单出口**（与 NICTLM 类似但同 Bundle）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                 CommMonitorTLM 单体                           │
│                                                             │
│  ┌──────────────────┐         ┌──────────────────┐         │
│  │  ReqIn/RespOut   │         │  ReqOut/RespIn   │         │
│  │  (上游端口)       │         │  (下游端口)       │         │
│  └─────────┬────────┘         └──────────┬─────────┘         │
│            │                                │               │
│            ▼                                ▼               │
│  ┌──────────────────────────────────────────────────┐     │
│  │  monitor_core_                                     │     │
│  │    - per_txn_inflight_: map<txn_id, arrival_time> │     │
│  │    - bytes_in_flight_window_: sliding window       │     │
│  │    - req/resp 计数器                               │     │
│  │    - 延迟直方图                                    │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                │               │
│            ▼                                ▼               │
│  ┌──────────────────────────────────────────────────┐     │
│  │  output_report_                                    │     │
│  │    - PeriodicStat（每 N cycle 输出一次）            │     │
│  │    - Histogram（延迟分布）                          │     │
│  │    - TimeSeries（带宽曲线）                        │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 应用场景

| 场景 | 监控对象 | 关键指标 |
|------|----------|----------|
| **CPU 侧 L1↔L2 流量** | 4 个 L1 → 1 个 L2 | L1 hit rate, L1-to-L2 bandwidth |
| **APU 内 CPU↔GPU 流量** | CPU cluster → GPU cluster | cross-domain bandwidth, kernel launch latency |
| **dGPU 流量** | Host → dGPU | PCIe bandwidth, GPU utilization proxy |
| **NoC 拥塞检测** | RouterTLM 入口 | outstanding transactions, flit 注入率 |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `req_in_` | `InputStreamAdapter<CacheReqBundle>` | 接收请求（探针上游） |
| `resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | 发送响应（探针上游） |
| `req_out_` | `OutputStreamAdapter<CacheReqBundle>` | 转发请求（探针下游） |
| `resp_in_` | `InputStreamAdapter<CacheRespBundle>` | 接收响应（探针下游） |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  CommMonitorTLM 内部                          │
│                                                             │
│  ReqIn ─► [record_arrival(txn_id)] ─► ReqOut               │
│                                                             │
│  RespIn ─► [record_completion(txn_id)] ─► RespOut         │
│                                                             │
│  record_arrival:                                            │
│    - inflight_txns_[txn_id] = current_cycle()              │
│    - req_counter_++                                        │
│    - bytes_in_flight_ += req.size                          │
│                                                             │
│  record_completion:                                         │
│    - latency = current_cycle() - inflight_txns_[txn_id]    │
│    - latency_hist_.sample(latency)                         │
│    - inflight_txns_.erase(txn_id)                          │
│    - resp_counter_++                                       │
│    - bytes_in_flight_ -= req.size                          │
│                                                             │
│  periodic_report (每 report_interval_ cycles):            │
│    - 打印/记录 current bandwidth, outstanding, p99 latency │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class CommMonitorTLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_REPORT_INTERVAL = 1000;  // 1000 cycle

    explicit CommMonitorTLM(const std::string& name, EventQueue* eq,
                            uint32_t report_interval = DEFAULT_REPORT_INTERVAL);

    std::string get_module_type() const override { return "CommMonitorTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_report_interval(uint32_t cycles) { report_interval_ = cycles; }
    void enable_sampling(double sample_rate) { sample_rate_ = sample_rate; }
    void set_monitor_name(const std::string& name) { monitor_name_ = name; }

    // === 端口访问器 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out();
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in();

    // === 统计查询 ===
    double get_average_bandwidth() const;  // bytes/cycle
    uint64_t get_outstanding_count() const;
    const tlm_stats::Distribution& get_latency_histogram() const;

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void record_arrival(const CacheReqBundle& req);
    void record_completion(const CacheRespBundle& resp);
    void periodic_report();

    uint32_t report_interval_;
    double sample_rate_;  // 0.0-1.0, 1.0 = 全量
    std::string monitor_name_;

    std::map<uint64_t, uint64_t> inflight_txns_;  // txn_id → arrival cycle
    uint64_t bytes_in_flight_;
    uint64_t total_req_count_;
    uint64_t total_resp_count_;
    uint64_t total_bytes_;
    uint64_t last_report_cycle_;

    InputStreamAdapter<CacheReqBundle> req_in_;
    OutputStreamAdapter<CacheRespBundle> resp_out_;
    OutputStreamAdapter<CacheReqBundle> req_out_;
    InputStreamAdapter<CacheRespBundle> resp_in_;

    // 统计
    tlm_stats::Scalar total_requests_;
    tlm_stats::Scalar total_responses_;
    tlm_stats::Scalar total_bytes_transferred_;
    tlm_stats::Average outstanding_transactions_;
    tlm_stats::Average bandwidth_bytes_per_cycle_;
    tlm_stats::Distribution request_latency_;
    tlm_stats::Scalar dropped_samples_;  // 采样模式下
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 3 阶段

```cpp
void CommMonitorTLM::tick() {
    // 1. 响应路径（记录完成 + 转发）
    if (resp_in_.valid() && resp_in_.ready()) {
        const auto& resp = resp_in_.data();
        record_completion(resp);
        resp_out_.write(resp);
        resp_in_.consume();
    }

    // 2. 请求路径（记录到达 + 转发）
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();

        // 采样决策
        if (should_sample()) {
            record_arrival(req);
        } else {
            dropped_samples_++;
        }

        req_out_.write(req);
        req_in_.consume();
    }

    // 3. 周期报告
    if (current_cycle() - last_report_cycle_ >= report_interval_) {
        periodic_report();
        last_report_cycle_ = current_cycle();
    }

    // 4. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 record_arrival / record_completion

```cpp
void CommMonitorTLM::record_arrival(const CacheReqBundle& req) {
    uint64_t txn_id = req.transaction_id.read();
    inflight_txns_[txn_id] = current_cycle();
    bytes_in_flight_ += req.size.read();

    total_req_count_++;
    total_bytes_ += req.size.read();

    total_requests_++;
    total_bytes_transferred_ += req.size.read();
    outstanding_transactions_.sample(inflight_txns_.size());
}

void CommMonitorTLM::record_completion(const CacheRespBundle& resp) {
    uint64_t txn_id = resp.transaction_id.read();
    auto it = inflight_txns_.find(txn_id);
    if (it == inflight_txns_.end()) {
        // 异常：响应没有匹配的请求
        return;
    }

    uint64_t latency = current_cycle() - it->second;
    request_latency_.sample(latency);

    inflight_txns_.erase(it);
    bytes_in_flight_ -= ...;  // 需要记录 size
    total_resp_count_++;
    total_responses_++;
}

bool CommMonitorTLM::should_sample() const {
    if (sample_rate_ >= 1.0) return true;
    return uniform_random(0.0, 1.0) < sample_rate_;
}
```

### 4.3 periodic_report

```cpp
void CommMonitorTLM::periodic_report() {
    uint64_t now = current_cycle();
    uint64_t interval = now - last_report_cycle_;

    double bw = static_cast<double>(total_bytes_) / interval;
    bandwidth_bytes_per_cycle_.sample(bw);

    DPRINTF(MONITOR, "[%s @ %lu] bw=%.2f B/cyc outstanding=%lu req=%lu resp=%lu\n",
            monitor_name_.c_str(), now,
            bw, inflight_txns_.size(), total_req_count_, total_resp_count_);

    total_bytes_ = 0;
    total_req_count_ = 0;
    total_resp_count_ = 0;
}
```

### 4.4 关键设计取舍

- **透传无延迟**：v0 CommMonitor 不注入延迟（仅记录），与 gem5 一致
- **采样 v0 全量**：1.0 sample rate（性能可接受范围）
- **响应大小记录**：v0 简化（响应不带 size，需在 record_arrival 时存 size 到 inflight_txns_）

## 5. Bundle 字段使用（规划）

| 字段 | CommMonitorTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——inflight_txns_ 键 + 延迟计算 |
| `address` | 透传（可扩展做地址热力图） |
| `size` | **关键**——带宽累加 + 字节 in-flight |
| `is_write` | 透传（可扩展做读写比例统计） |
| `kernel_id` | 透传（可扩展做 kernel 流量分析） |

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/comm_monitor.hh` CommMonitor | `tlm::CommMonitorTLM` | 简化：v0 不做 sampling（1.0 全量） |
| `gem5::CommMonitor::recordRequest` | `record_arrival` | 同语义 |
| `gem5::CommMonitor::recordResponse` | `record_completion` | 同语义 |
| `gem5::CommMonitor::getAverageBandwidth` | `get_average_bandwidth` | 同名沿用 |
| `gem5::CommMonitor::getOutstanding` | `get_outstanding_count` | 同名沿用 |
| `gem5::CommMonitor::getLatency` | `get_latency_histogram` | 同语义 |
| `gem5::CommMonitor::disable()` | (v0 留空) | Phase 7+ 实施 |

## 7. 实施路径

### 7.1 Phase 7.B 步骤（基础版，全量采集）

1. 新建 `include/tlm/interconnect/comm_monitor_tlm.hh`（~250 行）
2. 实现 4 端口 + record_arrival/record_completion
3. 实现 periodic_report
4. 加 Catch2 测试：`test/test_comm_monitor.cc`
5. 新增 `configs/comm_monitor_test.json`（CPU↔Cache 间插入 monitor）
6. `chstream_register.hh` 注册

### 7.2 Phase 7+ 步骤（采样 + 多粒度）

1. 实现 `enable_sampling()` 采样模式
2. 实现 per-master 粒度（多 Monitor 实例 + 不同 monitor_name_）
3. 集成到 `cpptlm` Python 库 dashboard（plot 带宽曲线）

### 7.3 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[monitor]"` 全部通过
- [ ] 端到端运行 + 周期报告输出
- [ ] 延迟直方图采集
- [ ] 采样模式验证（不破坏数据流）

### 7.4 估计工作量

- 设计: 0.5 周
- 基础版实施: 1 周
- 采样+多粒度: 1 周
- 测试 + dashboard 集成: 0.5 周
- **总计: 3 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **开销大**——全量采集 + map 查找影响 tick 性能 | 中 | 中 | Phase 7+ 启用 sampling；用 fixed-size 数组代替 map |
| R2 | **响应 size 缺失**——CacheRespBundle 无 size 字段 | 中 | 中 | 在 record_arrival 时存 size 到 inflight_txns_ 的 value |
| R3 | **监控数据丢失**——periodic_report 输出 sink 未实现 | 中 | 中 | 接入 `tlm_stats::OutputReport` 全局管道 |
| R4 | **多 CommMonitor 串联开销**——同链路插入 N 个 monitor | 中 | 中 | 暴露 `set_passive_mode()` 跳过记录 |
| R5 | **transaction_id 复用**——16-bit ID wrap 后 inflight 失配 | 低 | 中 | 64-bit ID 或 wrap 计数器 |
| R6 | **延迟测量误差**——request/response 不在同一 CommMonitor | 中 | 中 | 强制成对使用（req 路径 + resp 路径） |
| R7 | **时钟同步**——多时钟域下时间戳不一致 | 中 | 中 | v0 仅支持单时钟域；多域需 `set_clock_domain()` |

## 9. 设计决策点

### D1 默认采样率

- **Q**: 默认 sample_rate_ 是 1.0（全量）还是 0.1（10%）？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 1.0（v0 简化，与 gem5 默认一致）
- **依赖**: 性能 vs 精度权衡

### D2 报告输出目标

- **Q**: periodic_report 输出到 stdout / log / Python dashboard？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 全量（stdout + log + Python pipe 三个都支持）
- **依赖**: `tlm_stats::OutputReport` 实施

### D3 多 Bundle 类型支持

- **Q**: CommMonitor 是单 Bundle 还是多 Bundle？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 单 Bundle（与 NICTLM 类似）；多 Bundle 通过模板特化
- **依赖**: Phase 7.D TCC + Compute Bundle 集成

### D4 监控数据保留策略

- **Q**: TimeSeries 保留多少 cycle 数据？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 滑动窗口（最近 10000 cycle）

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.4）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7.B (未来)**: 基础版实施（全量采集）
- **Phase 7+ (未来)**: 采样 + 多粒度 + dashboard 集成
