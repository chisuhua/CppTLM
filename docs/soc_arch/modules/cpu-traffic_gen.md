# cpu-traffic_gen 微架构文档

> **类别**: CPU > TrafficGen · **状态**: ✅ 已实施 + 📋 v1.0 dGPU SoC 战略补充
> **状态**: ✅ 已实施
> **Header**: `include/tlm/traffic_gen_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:34`）
> **蓝图来源**: gem5 `src/cpu/testers/traffic_gen/` (LinearGen / RandomGen / DrSimGen)
> **首版 commit**: v2.1 路径同步
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.3
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §3

---

## 1. 设计目标

`TrafficGenTLM` 是 **CPU 流量生成器**，支持 3 基础模式 + 4 压力模式，专门用于内存子系统压测。**与 gem5 对位**: `gem5::TrafficGen`（`BaseTrafficGen` 子类）。

**核心特性**（来自 `traffic_gen_tlm.hh:35-230`）：
- **3 基础模式**：`SEQUENTIAL` / `RANDOM` / `TRACE`
- **4 压力模式**（通过 `stress_patterns.hh`）：`HOTSPOT` / `STRIDED` / `NEIGHBOR` / `TORNADO`
- `num_requests_ = 20`（硬编码上限）
- 10% 概率/周期发包
- `inflight_txns_` 跟踪 + 延迟统计

## 2. 架构概览

```
   ┌────────────────────────┐
   │   TrafficGenTLM         │
   │                        │
   │  tick():               │
   │   if (resp valid):     │──►  latency_.sample()
   │     consume + erase    │
   │   if (completed < N):  │
   │     if (rand%10 == 0):  │
   │       issueRequest()   │──► req_out_
   │                        │
   │  issueRequest():       │
   │   switch (mode_):      │──► addr, is_write
   │   if (stress != SEQ):  │
   │     addr = strategy_->next()  (覆盖)
   └────────────────────────┘
```

## 3. 接口（Public API）

```cpp
class TrafficGenTLM : public ChStreamModuleBase {
public:
    explicit TrafficGenTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "TrafficGenTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;

    // 基础模式 + 数量
    void set_mode(GenMode_TLM m);       // SEQUENTIAL / RANDOM / TRACE
    void set_num_requests(int n);

    // 压力模式（4 种）
    void set_stress_pattern(StressPattern p);
    void set_hotspot_config(const std::vector<uint64_t>& addrs,
                             const std::vector<double>& weights);
    void set_stride(uint64_t s);
    void set_mesh_config(uint64_t w, uint64_t h);

    // 适配器访问器
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>&  resp_in();
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>&  req_out();
    cpptlm::StreamAdapterBase* get_adapter() const;
};
```

**硬编码常量**：

| 常量 | 默认值 | 含义 |
|------|--------|------|
| `start_addr_` | 0x1000 | 地址起点 |
| `end_addr_` | 0x2000 | 地址终点 |
| `num_requests_` | 20 | 总发包数 |
| `rng_` seed | 42 | 随机种子（固定） |

**注意**: `stress_pattern_ != SEQUENTIAL` 时**策略覆盖基础模式生成的地址**（`issueRequest()` 第 178 行）。

## 4. 行为流程

### 4.1 tick() 主循环

```cpp
void TrafficGenTLM::tick() {
    current_cycle_++;

    // Phase 1: 响应消费
    if (resp_in_.valid() && resp_in_.ready()) {
        auto& resp = resp_in_.data();
        uint64_t txn_id = resp.transaction_id.read();
        auto it = txn_issue_time_.find(txn_id);
        if (it != txn_issue_time_.end()) {
            uint64_t latency = current_cycle_ - it->second;
            stats_latency_.sample(latency);
            txn_issue_time_.erase(it);
        }
        completed_++;
        stats_requests_completed_++;
        resp_in_.consume();
    }

    // Phase 2: 节流发包（10% 概率/周期）
    if (completed_ < num_requests_ && issued_ < num_requests_) {
        if (std::uniform_int_distribution<int>{0, 9}(rng_) == 0) {
            issueRequest();
        }
    }

    if (adapter_) adapter_->tick();
}
```

### 4.2 issueRequest() 地址生成

```cpp
void TrafficGenTLM::issueRequest() {
    bundles::CacheReqBundle req;
    bool is_write = false;
    uint64_t addr = 0;

    switch (mode_) {
        case GenMode_TLM::SEQUENTIAL:
            addr = cur_addr_;
            is_write = (cur_addr_ % 8 == 0);
            cur_addr_ += 4;
            if (cur_addr_ >= end_addr_) cur_addr_ = start_addr_;
            break;
        case GenMode_TLM::RANDOM:
            addr = addr_dist_(rng_);  // [start, end)
            is_write = (rng_() % 2 == 0);
            break;
        case GenMode_TLM::TRACE:
            if (trace_pos_ >= trace_.size()) return;  // 用完即停
            addr = trace_[trace_pos_].addr;
            is_write = trace_[trace_pos_].is_write;
            trace_pos_++;
            break;
    }

    // 压力模式覆盖基础模式地址
    if (stress_pattern_ != StressPattern::SEQUENTIAL && strategy_) {
        addr = strategy_->next_address(start_addr_, end_addr_ - start_addr_);
    }

    req.transaction_id.write(issued_++);
    req.address.write(addr);
    req.is_write.write(is_write ? 1 : 0);
    req.data.write(0);
    req.size.write(4);
    req_out_.write(req);

    txn_issue_time_[req.transaction_id.read()] = current_cycle_;
    stats_requests_issued_++;
    if (is_write) stats_writes_++;
    else stats_reads_++;
    stats_addr_distribution_.sample(addr);
}
```

### 4.3 压力模式策略（`include/tlm/stress_patterns.hh`）

| 模式 | 行为 |
|------|------|
| `SEQUENTIAL` | 顺序地址 |
| `RANDOM` | 均匀分布 |
| `HOTSPOT` | `std::discrete_distribution` 权重分布（需 `set_hotspot_config()`） |
| `STRIDED` | 固定步长（默认 64，set_stride() 可改） |
| `NEIGHBOR` | 80% 局部性（`±1` 步长）+ 20% 随机 |
| `TORNADO` | Mesh 对角遍历（需 `set_mesh_config(w, h)`） |

## 5. Bundle 字段使用

| 字段 | TrafficGenTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——`inflight_txns_` 映射键 |
| `address` | 由 mode + strategy 联合生成 |
| `is_write` | 由 mode 决定（SEQ: 8 字节对齐为写；RAND: 50%；TRACE: 用户定义） |
| `data` | 硬编码 0 |
| `size` | 硬编码 4 |
| 其他 | 忽略 |

## 6. 统计

| 指标 | 类型 | 含义 |
|------|------|------|
| `stats_requests_issued_` | Scalar | 已发请求数 |
| `stats_requests_completed_` | Scalar | 已收响应数 |
| `stats_reads_` | Scalar | 读请求数 |
| `stats_writes_` | Scalar | 写请求数 |
| `stats_latency_` | Distribution | 请求-响应延迟 |
| `stats_addr_distribution_` | Distribution | 地址分布 |

**路径**: `system.traffic_gen`

## 7. 蓝图（未来演进）

### 7.1 Phase 7.B 共享基类

调研 §4 Phase 1：与 CPUTLM / GPUTLM v0 共享 `compute_unit_base`（去除 tick 循环 / inflight 跟踪重复代码）。

### 7.2 蓝图增强

- **真实 trace 加载**（v0 硬编码 4 条）：`set_trace_file(path)` / `load_trace(records)`
- **更多 gem5 模式**：DramGen / DramRotGen / NvmGen
- **on_config_loaded JSON 解析**（与 GPUTLM v0 同样的缺口，Phase 7.B 统一修复）
- **波形级 burst**（与 GPUTLM v0 的 `coalescing_factor` 对齐）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **TRACE 模式硬编码 4 条**（0x1000/r, 0x1004/w, 0x1008/r, 0x100c/w） | 高 | 中 | v2.2 `set_trace_file()` / `load_trace()` |
| R2 | **JSON params 不读**——`on_config_loaded()` 未重写 | 高 | 中 | Phase 7.B 统一修复 |
| R3 | **rng seed=42 硬编码**——不可复现控制 | 中 | 低 | v2.2 `set_seed(uint64_t)` |
| R4 | **压力模式覆盖基础模式 is_write**——写混合比不可控 | 中 | 中 | v2.2 调整覆盖策略 |
| R5 | **节流 10% 概率**——非确定性，与 `num_requests_=20` 共同决定总时长 | 中 | 低 | v2.2 暴露 `set_issue_rate()` |
| R6 | **策略 `is_write` 保留基础模式**——stress 覆盖 addr 但不覆盖 is_write | 中 | 中 | 显式标注；v2.2 视情况 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ✅ | `configs/traffic_gen_tlm_test.json` + `[chstream]` 标签 |
| 端到端 (TGen→Cache→Xbar→Memory) | ✅ | `stress_strided.json` / `stress_hotspot.json` / `stress_full_system.json` |
| 6 种模式 | ✅ | 3 基础 + 4 压力（含 1 重叠 SEQUENTIAL） |
| 10% 节流 | ✅ | `uniform_int_distribution<int>{0, 9}` |
| 延迟统计 | ✅ | `stats_latency_.sample()` |
| **外部 trace 加载** | ❌ 硬编码 4 条 | 见 R1 |

## 10. 修订历史

- **2026-04-16**: TrafficGenTLM 初版
- **2026-04-20**: `stress_patterns.hh` 集成（4 压力模式）
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B1 批次）
