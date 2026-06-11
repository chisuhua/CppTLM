# memory-hbm 微架构文档

> **类别**: memory > hbm
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/memory/hbm_tlm.hh`
> **蓝图来源**: gem5 `src/mem/hbm2_stack.hh`（HBM2/HBM3 3D-stacked 高带宽内存）
> **首版 commit**: 蓝图（来自调研 §2.2 + Phase 7 备选 dGPU）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.2
> - 邻接: [memory-memtlm.md](./memory-memtlm.md) (v0) | [memory-simple.md](./memory-simple.md) | [memory-dram.md](./memory-dram.md) | [memory-qos.md](./memory-qos.md) | [gpu-pcie_bridge.md](./gpu-pcie_bridge.md) (dGPU)

---

## 1. 设计目标（蓝图）

`tlm::HBMTLM` 是 CppTLM Phase 7 备选 dGPU 规划的 **HBM2/HBM3 3D-stacked 高带宽内存模型**——通过硅穿孔（TSV）实现多通道高带宽堆叠，典型用于 dGPU 显存。**与 gem5 对位**: `gem5::HBM2Stack`（~600 行，8 channel × 16 bank 高带宽 + TSV 时序）。

**核心特征**：
- **多通道堆叠**（典型 4-8 channel，伪通道 ×2 = 8-16 逻辑通道）
- **3D 堆叠时序**（TSV 延迟模型）
- **高带宽**（典型 256-512 GB/s）
- **更短突发**（HBM3 2-beat burst）
- **温度感知刷新**（v0 简化：恒定 tREFI）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                  HBMTLM 单体                                 │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  HBM Stack (3D structure)                         │     │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐       │     │
│  │  │ Die 0    │  │ Die 1    │  │ Die 2    │       │     │
│  │  │ Ch0  Ch1 │  │ Ch2  Ch3 │  │ Ch4  Ch5 │       │     │
│  │  │ 16 bank  │  │ 16 bank  │  │ 16 bank  │       │     │
│  │  └──────────┘  └──────────┘  └──────────┘       │     │
│  │       ▲             ▲             ▲              │     │
│  │       └─────────────┴─────────────┘              │     │
│  │              TSV (Through-Silicon Via)            │     │
│  │              ~3 cycle latency                     │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  channel_scheduler_                               │     │
│  │    - 跨 channel 分散请求（最大化并行）            │     │
│  │    - bank 调度（同 DRAMCtrlTLM）                  │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────┐                                            │
│  │  RespOut     │                                            │
│  └──────────────┘                                            │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 与 DRAMCtrlTLM 的关系

| 维度 | DRAMCtrlTLM | HBMTLM (Phase 7 备选 dGPU) |
|------|-------------|------------------------------|
| **带宽** | 25-50 GB/s (DDR4 单 channel) | 256-512 GB/s (HBM2 4 stack) |
| **通道数** | 1 (v0 简化) | 4-8 通道堆叠 |
| **bank 数** | 8-16 | 16-32 per 通道 |
| **tRP/tRCD** | 14 cycle (DDR4) | 7-10 cycle (HBM2 更快) |
| **TSV 延迟** | ❌ 无 | ✅ 3 cycle (TSV 路径) |
| **应用场景** | 通用 CPU 内存 | dGPU 显存 / HPC 加速器 |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `req_in_` | `InputStreamAdapter<CacheReqBundle>` | 接收请求 |
| `resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | 发送响应 |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  HBMTLM 内部                                │
│                                                             │
│  配置:                                                      │
│    - num_stacks_: 1-4 (3D 堆叠数)                           │
│    - num_channels_per_stack_: 4-8                            │
│    - num_banks_per_channel_: 16                              │
│    - t_tsv_: 3 cycle (TSV 延迟)                             │
│    - t_rcd_/t_rp_/t_cas_: HBM2 时序                        │
│    - bandwidth_per_stack_: 64-128 GB/s                       │
│                                                             │
│  状态:                                                      │
│    - bank_state_table_[stack][channel][bank]                │
│    - page_table_[stack][channel][bank]                       │
│                                                             │
│  队列:                                                      │
│    - per_channel read_queue_/write_queue_                   │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class HBMTLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_NUM_STACKS = 4;
    static constexpr uint32_t DEFAULT_NUM_CHANNELS_PER_STACK = 2;
    static constexpr uint32_t DEFAULT_NUM_BANKS_PER_CHANNEL = 16;
    static constexpr uint32_t DEFAULT_T_TSV = 3;
    static constexpr uint64_t DEFAULT_CAPACITY = 16ULL * 1024 * 1024 * 1024;  // 16 GB

    explicit HBMTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "HBMTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_num_stacks(uint32_t n) { num_stacks_ = n; }
    void set_num_channels_per_stack(uint32_t n) { num_channels_per_stack_ = n; }
    void set_num_banks_per_channel(uint32_t n) { num_banks_per_channel_ = n; }
    void set_t_tsv(uint32_t cycles) { t_tsv_ = cycles; }

    // === 端口 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    struct HBMAddress {
        uint32_t stack, channel, bank, row, col;
    };
    HBMAddress decode_address(uint64_t addr) const;

    // 配置
    uint32_t num_stacks_;
    uint32_t num_channels_per_stack_;
    uint32_t num_banks_per_channel_;
    uint32_t t_tsv_;
    uint32_t t_rcd_, t_rp_, t_cas_;
    uint64_t capacity_bytes_;

    // 状态（同 DRAMCtrlTLM 模式）
    std::vector<std::vector<std::vector<std::vector<BankState>>>> bank_state_table_;
    std::vector<std::vector<std::vector<std::vector<uint64_t>>>> page_table_;

    // 队列
    std::vector<std::deque<CacheReqBundle>> per_channel_read_queue_;
    std::vector<std::deque<CacheReqBundle>> per_channel_write_queue_;
    std::map<uint64_t, uint64_t> pending_responses_;

    // 统计
    tlm_stats::Scalar total_reads_;
    tlm_stats::Scalar total_writes_;
    tlm_stats::Scalar aggregate_bandwidth_gbps_;
    tlm_stats::Average channel_utilization_;
    tlm_stats::Scalar page_hits_;
    tlm_stats::Scalar page_misses_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 4 阶段

```cpp
void HBMTLM::tick() {
    uint64_t now = current_cycle();

    // 1. 派发 pending responses
    dispatch_pending_responses(now);

    // 2. 接收新请求 + 地址解码 + 分散到各 channel
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        HBMAddress addr = decode_address(req.address.read());
        uint32_t ch_idx = addr.stack * num_channels_per_stack_ + addr.channel;
        if (req.is_write.read()) {
            per_channel_write_queue_[ch_idx].push_back(req);
        } else {
            per_channel_read_queue_[ch_idx].push_back(req);
        }
        req_in_.consume();
    }

    // 3. 跨 channel 调度（最大化并行）
    for (uint32_t ch = 0; ch < num_stacks_ * num_channels_per_stack_; ++ch) {
        schedule_channel(ch, now);
    }

    // 4. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 schedule_channel（每 channel 一致 DRAMCtrl 逻辑）

```cpp
void HBMTLM::schedule_channel(uint32_t ch, uint64_t now) {
    auto& rq = per_channel_read_queue_[ch];
    auto& wq = per_channel_write_queue_[ch];

    // 优先级：read > write
    for (auto& queue : {&rq, &wq}) {
        if (queue->empty()) continue;
        const auto& req = queue->front();
        HBMAddress addr = decode_address(req.address.read());

        bool page_hit = (page_table_[addr.stack][addr.channel][addr.bank] == addr.row);
        uint64_t latency = t_tsv_ + (page_hit ? t_cas_ : (t_rp_ + t_rcd_ + t_cas_));

        pending_responses_[req.transaction_id.read()] = now + latency;
        queue->pop_front();
    }
}
```

## 5. Bundle 字段使用

| 字段 | HBMTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——pending_responses_ 键 |
| `address` | **关键**——HBMAddress 解码（stack/channel/bank/row/col） |
| `is_write` | **关键**——read/write 队列分离 |
| `size` | 决定 burst 长度（典型 32 B for HBM） |

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/hbm2_stack.hh` HBM2Stack | `tlm::HBMTLM` | 简化：v0 仅 1-4 stack |
| `HBM2Stack::numChannels` | `num_channels_per_stack_` | 同语义 |
| `HBM2Stack::numPseudoChannels` | (v0 简化：单 pseudo channel) | Phase 7+ 真实双 PC |
| `HBM2Stack::numBanks` | `num_banks_per_channel_` | 同语义 |
| `HBM2Stack::TSV_latency` | `t_tsv_` | 同语义 |
| `HBM2Stack::decodeAddr` | `decode_address` | 同语义 |
| `HBM2Stack::activateBank` | `schedule_channel` (内嵌) | 同语义 |

## 7. 实施路径

### 7.1 Phase 7 备选 dGPU 步骤

1. 新建 `include/tlm/memory/hbm_tlm.hh`（~350 行）
2. 实现 `HBMAddress` + `decode_address` (HBM2 映射)
3. 实现 bank_state_table_ + page_table_ (per stack/channel/bank)
4. 实现跨 channel 调度器
5. 实现 TSV 延迟注入
6. 加 Catch2 测试：`test/test_hbm.cc`
7. 新增 `configs/dgpu_with_hbm.json`（dGPU 域 + HBM 端到端）

### 7.2 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[hbm]"` 全部通过
- [ ] TSV 延迟真实生效
- [ ] 多 channel 并行生效
- [ ] aggregate bandwidth 接近理论值

### 7.3 估计工作量

- 设计: 0.5 周
- 基础版实施: 2-3 周
- 测试: 0.5 周
- **总计: 3-4 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **TSV 时序不准**——3 cycle 是平均值 | 中 | 中 | 文档明确；运行时可调 |
| R2 | **跨 channel 一致性**——多 channel 顺序保证 | 中 | 中 | 单元测试覆盖 |
| R3 | **address decode 错误**——HBM2 复杂映射 | 中 | 高 | 单元测试覆盖 |
| R4 | **大内存默认**——16 GB 默认过大 | 中 | 中 | 默认 1 GB，配置可调 |
| R5 | **温度感知缺失**——v0 简化刷新 | 中 | 低 | Phase 7+ 真实温度模型 |
| R6 | **与 DRAMCtrlTLM 代码重复**——可复用性 | 中 | 中 | 抽出基类 `BankedMemoryTLM` |

## 9. 设计决策点

### D1 HBM2 vs HBM3

- **Q**: 默认 HBM2 还是 HBM3？
- **状态**: 留待 Phase 7 备选 dGPU 设计时确定
- **建议**: HBM2（v0 简化，HBM3 时序复杂）
- **依赖**: dGPU 蓝图版本

### D2 Pseudo Channel

- **Q**: v0 支持 pseudo channel 吗？
- **状态**: 留待 Phase 7+ 设计时确定
- **建议**: v0 不支持（单 PC）；Phase 7+ 双 PC
- **依赖**: 复杂度权衡

### D3 与 DRAMCtrlTLM 关系

- **Q**: HBMTLM 独立还是继承 DRAMCtrlTLM？
- **状态**: 留待 Phase 7 备选 dGPU 设计时确定
- **建议**: 抽出基类 `BankedMemoryTLM`（DRAMCtrl + HBM 共享 bank 逻辑）
- **依赖**: 重构复杂度

### D4 默认 stack 数

- **Q**: 默认 num_stacks_ 多少？
- **状态**: 留待 Phase 7 备选 dGPU 设计时确定
- **建议**: 4（HBM2 典型）
- **依赖**: dGPU 显存容量

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.2）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7 备选 dGPU (未来)**: 基础版实施（多 channel + TSV）
