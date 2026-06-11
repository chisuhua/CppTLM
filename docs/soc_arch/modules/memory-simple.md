# memory-simple 微架构文档

> **类别**: memory > simple
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/memory/simple_memory_tlm.hh`
> **蓝图来源**: gem5 `src/mem/simple_mem.hh`（吞吐/带宽/带宽限制队列模型）
> **首版 commit**: 蓝图（来自调研 §2.2）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.2
> - 邻接: [memory-memtlm.md](./memory-memtlm.md) (v0 简化 DRAM) | [memory-dram.md](./memory-dram.md) | [memory-hbm.md](./memory-hbm.md) | [memory-qos.md](./memory-qos.md)

---

## 1. 设计目标（蓝图）

`tlm::SimpleMemoryTLM` 是 CppTLM v2.2+ 规划的 **吞吐/带宽/带宽限制队列模型** memory——比 v0 `MemoryTLM` 多了真实带宽限制和队列深度模型，但比 `DRAMCtrlTLM` 简单（无 bank/rank/页策略）。**与 gem5 对位**: `gem5::SimpleMemory`（~400 行，bandwidth + queue depth + latency）。

**核心特征**：
- **带宽限制**（bytes/cycle 上限，模拟总线/接口带宽）
- **请求队列**（max_inflight_ 限制 outstanding 数）
- **固定延迟**（v0 简化：read/write 各一个固定 latency）
- **back-pressure 真实模型**（队列满则 ready=false）
- **无 bank 模型**（与 DRAMCtrlTLM 区别）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                SimpleMemoryTLM 单体                          │
│                                                             │
│  ┌──────────────┐                                            │
│  │  ReqIn       │                                            │
│  │  (CacheReq)  │                                            │
│  └──────┬───────┘                                            │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  request_queue_ (FIFO, max_inflight_ 容量限制)     │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  bandwidth_limiter_                              │     │
│  │    - bytes_in_flight_ < bandwidth_ * cycle_window │     │
│  │    - 超限则 back-pressure                        │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐         ┌──────────────┐                │
│  │  Read 路径   │         │  Write 路径  │                │
│  │  latency_r_  │         │  latency_w_  │                │
│  └──────┬───────┘         └──────┬───────┘                │
│         │                        │                          │
│         └────────────┬───────────┘                          │
│                      ▼                                       │
│              ┌──────────────┐                               │
│              │  RespOut     │                               │
│              │  (CacheResp) │                               │
│              └──────────────┘                               │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 与 v0 MemoryTLM 的关系

| 维度 | v0 MemoryTLM | SimpleMemoryTLM (Phase 7.D) |
|------|---------------|------------------------------|
| **带宽限制** | ❌ 无（固定延迟） | ✅ bytes/cycle |
| **请求队列** | ❌ 无限制 | ✅ max_inflight_ |
| **back-pressure** | ❌ 总是 ready | ✅ 队列满则 ready=false |
| **延迟** | rd=100, wr=120 cyc | 可配（默认 100/120） |
| **bank 模型** | ❌ 无 | ❌ 无（DRAMCtrlTLM 有） |
| **行缓冲** | ✅ 简化版 | ❌ 无 |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `req_in_` | `InputStreamAdapter<CacheReqBundle>` | 接收请求 |
| `resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | 发送响应 |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  SimpleMemoryTLM 内部                        │
│                                                             │
│  request_queue_: std::deque<CacheReqBundle>  (max_inflight_)│
│  bytes_in_flight_: uint32_t  (bandwidth 累计)              │
│  pending_responses_: std::map<txn_id, depart_cycle>        │
│  data_store_: std::map<uint64_t, uint64_t>  (读写存储)     │
│                                                             │
│  配置:                                                      │
│    - bandwidth_bytes_per_cycle_: 默认 64 B/cyc (DDR4 1ch)  │
│    - max_inflight_: 默认 16                                  │
│    - read_latency_: 默认 100 cycle                          │
│    - write_latency_: 默认 120 cycle                         │
│    - data_size_bytes_: 默认 4 GB                            │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class SimpleMemoryTLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_BANDWIDTH = 64;  // B/cyc (DDR4 single channel)
    static constexpr uint32_t DEFAULT_MAX_INFLIGHT = 16;
    static constexpr uint32_t DEFAULT_READ_LATENCY = 100;
    static constexpr uint32_t DEFAULT_WRITE_LATENCY = 120;
    static constexpr uint64_t DEFAULT_DATA_SIZE = 4ULL * 1024 * 1024 * 1024;  // 4 GB

    explicit SimpleMemoryTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "SimpleMemoryTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_bandwidth(uint32_t bytes_per_cycle) { bandwidth_ = bytes_per_cycle; }
    void set_max_inflight(uint32_t n) { max_inflight_ = n; }
    void set_read_latency(uint32_t cycles) { read_latency_ = cycles; }
    void set_write_latency(uint32_t cycles) { write_latency_ = cycles; }
    void set_data_size(uint64_t bytes) { data_size_bytes_ = bytes; }

    // === 端口 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    bool check_bandwidth(const CacheReqBundle& req) const;
    void enqueue_read(uint64_t txn_id, uint64_t arrival);
    void enqueue_write(const CacheReqBundle& req, uint64_t arrival);
    void dispatch_pending_responses(uint64_t now);
    uint64_t do_read(uint64_t addr);
    void do_write(uint64_t addr, uint64_t data);

    uint32_t bandwidth_;
    uint32_t max_inflight_;
    uint32_t read_latency_;
    uint32_t write_latency_;
    uint64_t data_size_bytes_;

    std::deque<CacheReqBundle> request_queue_;
    uint32_t bytes_in_flight_;
    std::map<uint64_t, uint64_t> pending_responses_;  // txn_id → depart_cycle
    std::map<uint64_t, uint64_t> data_store_;        // addr → data

    // 统计
    tlm_stats::Scalar reads_completed_;
    tlm_stats::Scalar writes_completed_;
    tlm_stats::Scalar bytes_read_;
    tlm_stats::Scalar bytes_written_;
    tlm_stats::Average queue_depth_;
    tlm_stats::Average bandwidth_utilization_;
    tlm_stats::Scalar backpressure_events_;
    tlm_stats::Distribution read_latency_actual_;
    tlm_stats::Distribution write_latency_actual_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 3 阶段

```cpp
void SimpleMemoryTLM::tick() {
    uint64_t now = current_cycle();

    // 1. 派发到期的 pending responses
    dispatch_pending_responses(now);

    // 2. 处理新请求（enqueue + bandwidth 检查）
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        if (check_bandwidth(req) && request_queue_.size() < max_inflight_) {
            request_queue_.push_back(req);
            bytes_in_flight_ += req.size.read();
            req_in_.consume();
        }
        // else: back-pressure
    }

    // 3. 处理队列头部请求（模拟延迟）
    while (!request_queue_.empty()) {
        const auto& req = request_queue_.front();
        uint64_t latency = req.is_write.read() ? write_latency_ : read_latency_;
        uint64_t depart = req.arrival_cycle_ + latency;
        if (depart <= now) {
            if (req.is_write.read()) {
                do_write(req.address.read(), req.data.read());
            } else {
                uint64_t data = do_read(req.address.read());
                // 生成 response
            }
            request_queue_.pop_front();
            bytes_in_flight_ -= req.size.read();
        } else {
            break;
        }
    }

    // 4. 统计采样
    queue_depth_.sample(request_queue_.size());
    bandwidth_utilization_.sample(static_cast<double>(bytes_in_flight_) / bandwidth_);

    // 5. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 关键设计取舍

- **每 cycle 处理 1 个请求**（队列头部弹出）
- **back-pressure 真实**：req_in_.ready() 反映 queue 状态
- **写后立即释放带宽**（不阻塞 read）
- **无 bank 冲突**（与 DRAMCtrlTLM 区别）

## 5. Bundle 字段使用

| 字段 | SimpleMemoryTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——pending_responses_ 键 |
| `address` | **关键**——do_read/do_write 索引 |
| `is_write` | **关键**——延迟路径 + 写处理 |
| `size` | **关键**——带宽计算 |
| `data` | 写时存 data_store_ |

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/simple_mem.hh` SimpleMemory | `tlm::SimpleMemoryTLM` | 同语义 |
| `SimpleMemory::bandwidth` | `bandwidth_bytes_per_cycle_` | 同语义 |
| `SimpleMemory::latency` | `read_latency_/write_latency_` | 区分读/写 |
| `SimpleMemory::queue_size` | `max_inflight_` | 同语义 |
| `SimpleMemory::getQueuePriority` | (v0 无优先级) | v0 简化（按到达顺序） |
| `SimpleMemory::access` | `do_read/do_write` | 同语义 |

## 7. 实施路径

### 7.1 Phase 7.D 步骤

1. 新建 `include/tlm/memory/simple_memory_tlm.hh`（~250 行）
2. 实现 2 端口 + request_queue_ + bandwidth_limiter_
3. 实现 `do_read` / `do_write` (data_store_)
4. 加 Catch2 测试：`test/test_simple_memory.cc`
5. 新增 `configs/simple_memory_test.json`（CPU↔SimpleMemory 端到端）

### 7.2 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[simple_memory]"` 全部通过
- [ ] 带宽限制真实生效
- [ ] back-pressure 真实生效
- [ ] 读/写延迟独立配置

### 7.3 估计工作量

- 设计: 0.5 周
- 基础版实施: 1-2 周
- 测试: 0.5 周
- **总计: 2-3 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **back-pressure 死锁**——队列满且下游也阻塞 | 中 | 高 | 加超时强制消费 |
| R2 | **带宽计算误差**——bytes_in_flight_ 维护错误 | 中 | 中 | 单元测试覆盖 |
| R3 | **写穿透了写缓冲**——v0 无 WriteBuffer 写穿透 | 中 | 中 | 暴露 `set_write_buffer_size()` |
| R4 | **大延迟下响应累积**——pending_responses_ 内存膨胀 | 中 | 中 | 容量限制 + 警告 |
| R5 | **data_store_ 内存占用**——4 GB 默认过大 | 中 | 中 | 默认值改 1 GB，配置可调 |
| R6 | **优先级缺失**——v0 FIFO，无法 QoS | 中 | 中 | Phase 7.D+ 集成 QoS 调度器 |

## 9. 设计决策点

### D1 默认带宽

- **Q**: 默认 bandwidth_bytes_per_cycle_ 多少？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 64 B/cyc (DDR4 single channel)
- **依赖**: gem5 SimpleMemory 默认

### D2 默认 max_inflight

- **Q**: 默认 max_inflight_ 多少？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 16（与真实内存控制器一致）
- **依赖**: queue 深度权衡

### D3 与 v0 MemoryTLM 共存

- **Q**: SimpleMemoryTLM 替换 v0 MemoryTLM 还是并存？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 并存（v0 简单版保留用于快速验证）
- **依赖**: 用户配置选择

### D4 data_store_ 实现

- **Q**: data_store_ 用 std::map 还是预分配数组？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: std::map（v0 简化）；Phase 7+ 优化为预分配数组
- **依赖**: 性能 vs 内存

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.2）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7.D (未来)**: 基础版实施（带宽+队列+back-pressure）
