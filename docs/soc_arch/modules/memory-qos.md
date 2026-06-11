# memory-qos 微架构文档

> **类别**: memory > qos
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/memory/qos_mem_ctrl_tlm.hh`
> **蓝图来源**: gem5 `src/mem/qos/`（QoS-aware memory controller + policy plugins）
> **首版 commit**: 蓝图（来自调研 §2.2 + Phase 7.D+）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.2
> - 邻接: [memory-dram.md](./memory-dram.md) (DRAMCtrl) | [memory-hbm.md](./memory-hbm.md) (HBM) | [memory-simple.md](./memory-simple.md) | [memory-memtlm.md](./memory-memtlm.md) (v0)

---

## 1. 设计目标（蓝图）

`tlm::QoSMemCtrl` 是 CppTLM Phase 7.D+ 规划的 **QoS 感知内存调度器**——在 SimpleMemory/DRAMCtrl/HBM 之上增加 QoS 优先级、带宽分配、配额管理。**与 gem5 对位**: `gem5::QoSMemCtrl`（~800 行，policy 插件 + 公平队列 + 优先级）。

**核心特征**：
- **多 QoS class**（典型 4-8 class，high/mid/low/best-effort）
- **优先级调度**（strict priority / weighted fair queuing）
- **带宽配额**（per-class max bandwidth 限制）
- **公平队列**（DRR 简化版）
- **策略插件**（v0 简化：编译期策略选择）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                  QoSMemCtrl 单体                             │
│                                                             │
│  ┌──────────────┐                                            │
│  │  ReqIn       │                                            │
│  │  (CacheReq + QoS class)                                  │
│  └──────┬───────┘                                            │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  per_class_request_queue_[N_CLASSES]              │     │
│  │    - high_priority_queue_                         │     │
│  │    - mid_priority_queue_                          │     │
│  │    - low_priority_queue_                          │     │
│  │    - best_effort_queue_                           │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  qos_scheduler_                                   │     │
│  │    - 选择策略：StrictPriority / WFQ / DRR         │     │
│  │    - 带宽配额检查                                 │     │
│  │    - 公平队列（DRR quantum）                       │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  underlying_memory_ (Simple/DRAM/HBM)             │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐                                            │
│  │  RespOut     │                                            │
│  └──────────────┘                                            │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 QoS 调度策略对比

| 策略 | 优点 | 缺点 | v0 支持 |
|------|------|------|---------|
| **Strict Priority** | 高优先级确定性延迟 | 低优先级可能饥饿 | ✅ |
| **Weighted Fair Queuing (WFQ)** | 按权重公平 | 实现复杂 | ✅（简化版） |
| **Deficit Round Robin (DRR)** | O(1) 调度，低开销 | 短包可能不公平 | ✅（简化版） |
| **Token Bucket** | 流量整形 | 配置复杂 | ❌ Phase 7+ |
| **Earliest Deadline First (EDF)** | 实时调度 | 需要 deadline 信息 | ❌ Phase 7+ |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `req_in_` | `InputStreamAdapter<CacheReqBundle>` | 接收请求（含 `qos_class` 字段） |
| `resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | 发送响应 |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  QoSMemCtrl 内部                            │
│                                                             │
│  队列（per QoS class）:                                     │
│    - per_class_queue_[NUM_QOS_CLASSES]                     │
│    - per_class_deficit_[NUM_QOS_CLASSES] (DRR)             │
│    - per_class_bytes_served_[NUM_QOS_CLASSES]              │
│                                                             │
│  调度器:                                                    │
│    - current_policy_: StrictPriority / WFQ / DRR           │
│    - weights_[NUM_QOS_CLASSES]                              │
│    - max_bandwidth_pct_[NUM_QOS_CLASSES]                   │
│                                                             │
│  底层内存:                                                  │
│    - underlying_memory_: Simple / DRAM / HBM 实例          │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {

enum class QoSClass : uint8_t {
    HIGH = 0,      // 实时、kernel launch
    MID = 1,       // 高优先级 cache miss
    LOW = 2,       // 普通访存
    BEST_EFFORT = 3,  // background / prefetch
};

enum class QoSPolicy {
    STRICT_PRIORITY,
    WEIGHTED_FAIR_QUEUING,
    DEFICIT_ROUND_ROBIN,
};

class QoSMemCtrl : public ChStreamModuleBase {
public:
    static constexpr uint32_t NUM_QOS_CLASSES = 4;

    explicit QoSMemCtrl(const std::string& name, EventQueue* eq,
                        std::shared_ptr<ChStreamModuleBase> underlying_memory);

    std::string get_module_type() const override { return "QoSMemCtrl"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_policy(QoSPolicy p) { policy_ = p; }
    void set_weight(QoSClass cls, uint32_t weight) { weights_[cls] = weight; }
    void set_max_bandwidth_pct(QoSClass cls, uint32_t pct) {
        max_bandwidth_pct_[cls] = pct;
    }
    void set_drr_quantum(uint32_t bytes) { drr_quantum_ = bytes; }

    // === 端口 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    CacheReqBundle select_next();
    void enqueue_request(const CacheReqBundle& req);

    // 配置
    QoSPolicy policy_;
    std::array<uint32_t, NUM_QOS_CLASSES> weights_;
    std::array<uint32_t, NUM_QOS_CLASSES> max_bandwidth_pct_;
    uint32_t drr_quantum_;

    // 队列
    std::array<std::deque<CacheReqBundle>, NUM_QOS_CLASSES> per_class_queue_;
    std::array<uint32_t, NUM_QOS_CLASSES> per_class_deficit_;
    std::array<uint64_t, NUM_QOS_CLASSES> per_class_bytes_served_;
    uint64_t total_bytes_served_;
    uint64_t last_window_cycle_;

    // 底层内存
    std::shared_ptr<ChStreamModuleBase> underlying_memory_;
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> mem_req_out_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> mem_resp_in_;

    // 统计
    tlm_stats::Scalar per_class_requests_[NUM_QOS_CLASSES];
    tlm_stats::Scalar per_class_throttled_[NUM_QOS_CLASSES];
    tlm_stats::Average per_class_queue_depth_[NUM_QOS_CLASSES];
    tlm_stats::Average per_class_bandwidth_share_[NUM_QOS_CLASSES];
};

}  // namespace tlm
```

## 4. 行为流程（规划）

### 4.1 tick() 4 阶段

```cpp
void QoSMemCtrl::tick() {
    uint64_t now = current_cycle();

    // 1. 接收新请求 + 按 QoS class 入队
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        QoSClass cls = extract_qos_class(req);  // 从 req.qos_class 字段
        per_class_queue_[cls].push_back(req);
        req_in_.consume();
    }

    // 2. 接收底层内存响应 + 转发
    if (mem_resp_in_.valid() && mem_resp_in_.ready()) {
        const auto& resp = mem_resp_in_.data();
        resp_out_.write(resp);
        mem_resp_in_.consume();
    }

    // 3. QoS 调度（select_next + 推到底层内存）
    CacheReqBundle selected = select_next();
    if (mem_req_out_.ready()) {
        mem_req_out_.write(selected);
    }

    // 4. 带宽窗口采样
    if (now - last_window_cycle_ >= WINDOW_SIZE) {
        compute_bandwidth_share(now);
        last_window_cycle_ = now;
    }

    // 5. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 select_next（按策略选择）

```cpp
CacheReqBundle QoSMemCtrl::select_next() {
    switch (policy_) {
        case QoSPolicy::STRICT_PRIORITY:
            return select_strict_priority();

        case QoSPolicy::WEIGHTED_FAIR_QUEUING:
            return select_wfq();

        case QoSPolicy::DEFICIT_ROUND_ROBIN:
            return select_drr();
    }
    return {};
}

CacheReqBundle QoSMemCtrl::select_strict_priority() {
    for (uint32_t c = 0; c < NUM_QOS_CLASSES; ++c) {
        if (!per_class_queue_[c].empty()) {
            return per_class_queue_[c].front();
        }
    }
    return {};
}

CacheReqBundle QoSMemCtrl::select_drr() {
    for (uint32_t c = 0; c < NUM_QOS_CLASSES; ++c) {
        if (per_class_deficit_[c] > 0 && !per_class_queue_[c].empty()) {
            const auto& req = per_class_queue_[c].front();
            if (req.size.read() <= per_class_deficit_[c]) {
                per_class_deficit_[c] -= req.size.read();
                return req;
            }
        }
    }
    // 重置 deficit
    for (uint32_t c = 0; c < NUM_QOS_CLASSES; ++c) {
        per_class_deficit_[c] += drr_quantum_ * weights_[c];
    }
    return select_drr();
}
```

## 5. Bundle 字段使用

| 字段 | QoSMemCtrl 使用 |
|------|---------------|
| `transaction_id` | 透传 |
| `address` | 透传 |
| `is_write` | 透传 |
| `size` | **关键**——DRR deficit 累计 |
| `qos_class` | **关键**——per_class_queue_ 路由 |
| `kernel_id` | 透传 |

> **注**: `qos_class` 字段需扩展 `CacheReqBundle`（Phase 7.D+）。

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/qos/QoSMemCtrl.hh` QoSMemCtrl | `tlm::QoSMemCtrl` | 同语义 |
| `src/mem/qos/policy_*.hh` Policy plugins | `QoSPolicy` enum | 简化：编译期选择 |
| `src/mem/qos/QoSMemSinkCtrl.hh` Sink | (v0 留空) | Phase 7+ 实施 |
| `QoSMemCtrl::schedule` | `select_next` | 同语义 |
| `QoSMemCtrl::addToGroup` | `enqueue_request` | 同语义 |
| `QoSMemCtrl::getCurrPktSize` | (v0 简化) | Phase 7+ |

## 7. 实施路径

### 7.1 Phase 7.D+ 步骤

1. 新建 `include/tlm/memory/qos_mem_ctrl_tlm.hh`（~300 行）
2. 扩展 `CacheReqBundle` 加 `qos_class` 字段
3. 实现 3 种策略 (StrictPriority / WFQ / DRR)
4. 实现 per-class 队列 + 带宽配额
5. 加 Catch2 测试：`test/test_qos_mem_ctrl.cc`
6. 新增 `configs/qos_mem_test.json`（4 QoS class 端到端）

### 7.2 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[qos]"` 全部通过
- [ ] 3 种策略可切换
- [ ] 带宽配额真实生效
- [ ] high class 严格优先（strict mode）

### 7.3 估计工作量

- 设计: 0.5 周
- 基础版实施: 2 周
- 测试: 0.5 周
- **总计: 3 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **DRR 公平性偏差**——短包可能不公平 | 中 | 中 | 单元测试覆盖；quantum 可调 |
| R2 | **strict priority 饥饿**——low class 永久得不到服务 | 高 | 中 | 加 watchdog：low 至少 N% 带宽 |
| R3 | **qos_class 字段扩展影响兼容**——旧 config 不带该字段 | 中 | 高 | 默认 BEST_EFFORT，兼容旧 config |
| R4 | **多策略配置冲突**——policy + weights 同时设置 | 中 | 中 | on_config_loaded 校验 |
| R5 | **窗口采样误差**——短窗口抖动 | 中 | 低 | 滑动窗口（最近 N 个 WINDOW_SIZE） |
| R6 | **底层内存不感知 QoS**——仅 QoSMemCtrl 层调度 | 中 | 中 | 文档明确：v0 仅 QoS 层调度 |

## 9. 设计决策点

### D1 默认策略

- **Q**: 默认 StrictPriority / WFQ / DRR？
- **状态**: 留待 Phase 7.D+ 设计时确定
- **建议**: DRR（O(1) 调度 + 公平）
- **依赖**: 性能 vs 公平权衡

### D2 QoS class 数量

- **Q**: 默认 NUM_QOS_CLASSES = 4？
- **状态**: 留待 Phase 7.D+ 设计时确定
- **建议**: 4（典型：high/mid/low/best-effort）
- **依赖**: 应用场景

### D3 带宽配额 vs 优先级

- **Q**: 用带宽配额还是优先级？
- **状态**: 留待 Phase 7.D+ 设计时确定
- **建议**: 两者都支持（组合：先按配额，再按优先级）
- **依赖**: QoS 模型

### D4 与底层内存耦合

- **Q**: QoSMemCtrl 是否感知底层内存状态（page hit/miss）？
- **状态**: 留待 Phase 7+ 设计时确定
- **建议**: v0 不感知（QoS 层独立）；Phase 7+ 集成
- **依赖**: 跨层集成复杂度

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.2）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7.D+ (未来)**: 基础版实施（3 策略 + 4 class）
