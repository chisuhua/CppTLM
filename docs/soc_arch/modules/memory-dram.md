# memory-dram 微架构文档

> **类别**: memory > dram
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/memory/dram_ctrl_tlm.hh`
> **蓝图来源**: gem5 `src/mem/draming_ctrl.hh`（bank/rank/页策略/刷新）+ `DRAMSim2` 集成
> **首版 commit**: 蓝图（来自调研 §2.2）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.2
> - 邻接: [memory-memtlm.md](./memory-memtlm.md) (v0 简化) | [memory-simple.md](./memory-simple.md) | [memory-hbm.md](./memory-hbm.md) | [memory-qos.md](./memory-qos.md)

---

## 1. 设计目标（蓝图）

`tlm::DRAMCtrlTLM` 是 CppTLM Phase 7.D 规划的 **真实 DRAM 控制器模型**——具备 bank/rank/页策略/刷新周期等完整 DRAM 行为，对位 gem5 DRAMCtrl。**与 gem5 对位**: `gem5::DRAMCtrl`（~3000 行，bank/rank/页调度器 + 刷新 + QoS hooks）。

**核心特征**：
- **多 bank 模型**（典型 8 bank/rank，4-8 rank/channel）
- **页命中/冲突模型**（open page policy：page hit 加速，page miss + tRP/tRCD/tRAS 惩罚）
- **bank 调度器**（FR-FCFS / PAR-BS 简化版）
- **刷新周期**（tREFI 7.8μs 模拟）
- **写读转换**（read-to-write 切换 + write-to-read 切换延迟）
- **多 channel 支持**（可选：1/2/4 channel）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                  DRAMCtrlTLM 单体                            │
│                                                             │
│  ┌──────────────┐                                            │
│  │  ReqIn       │                                            │
│  │  (CacheReq)  │                                            │
│  └──────┬───────┘                                            │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  request_queue_ (per QoS class)                  │     │
│  │    - read_queue_ / write_queue_                  │     │
│  │    - QoS-aware priority (默认：read > write)    │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  address_mapper_                                  │     │
│  │    - addr → {channel, rank, bank, row, col}     │     │
│  │    - 典型：DDR4 地址映射                          │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  bank_scheduler_ (FR-FCFS 简化)                  │     │
│  │    - 选择最早就绪的 bank                          │     │
│  │    - 检查 page hit / miss                         │     │
│  │    - 应用 tRP/tRCD/tCAS 延迟                      │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  bank_state_table_                                │     │
│  │    - per bank: { row_open, last_access_cycle }   │     │
│  │    - page_table_: { bank → row }                 │     │
│  │    - 刷新跟踪器                                    │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐                                            │
│  │  RespOut     │                                            │
│  │  (CacheResp) │                                            │
│  └──────────────┘                                            │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 DRAM 时序参数（典型 DDR4）

| 参数 | 含义 | 默认值（cycle @ 1 GHz） |
|------|------|------------------------|
| `tCK` | 时钟周期 | 1 cycle |
| `tRCD` | ACTIVATE 到 READ/WRITE 延迟 | 14 cycle |
| `tRP` | PRECHARGE 到 ACTIVATE 延迟 | 14 cycle |
| `tRAS` | ACTIVATE 到 PRECHARGE 最小间隔 | 32 cycle |
| `tCAS` | READ 到数据可用延迟 | 14 cycle |
| `tREFI` | 刷新周期 | 7800 cycle (7.8 μs) |
| `tRFC` | 刷新时长 | 350 cycle |
| `tBurst` | 突发长度（4/8 beat） | 4 cycle |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `req_in_` | `InputStreamAdapter<CacheReqBundle>` | 接收请求 |
| `resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | 发送响应 |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  DRAMCtrlTLM 内部                           │
│                                                             │
│  配置:                                                      │
│    - num_channels_ / num_ranks_ / num_banks_               │
│    - tRCD_/tRP_/tRAS_/tCAS_/tREFI_/tRFC_                  │
│    - page_policy_: OPEN / CLOSE / ADAPTIVE                  │
│    - scheduler_: FR_FCFS / PAR_BS / FCFS                   │
│                                                             │
│  状态:                                                      │
│    - bank_state_table_[channel][rank][bank]: BankState      │
│    - page_table_[channel][rank][bank]: open_row            │
│    - last_refresh_cycle_: 刷新时间戳                         │
│                                                             │
│  队列:                                                      │
│    - read_queue_: std::deque<CacheReqBundle>                │
│    - write_queue_: std::deque<CacheReqBundle>               │
│    - pending_responses_: map<txn_id, depart_cycle>          │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class DRAMCtrlTLM : public ChStreamModuleBase {
public:
    enum class PagePolicy { OPEN, CLOSE, ADAPTIVE };
    enum class Scheduler { FR_FCFS, PAR_BS, FCFS };

    static constexpr uint32_t DEFAULT_NUM_CHANNELS = 1;
    static constexpr uint32_t DEFAULT_NUM_RANKS = 4;
    static constexpr uint32_t DEFAULT_NUM_BANKS = 8;
    static constexpr uint32_t DEFAULT_T_RCD = 14;
    static constexpr uint32_t DEFAULT_T_RP = 14;
    static constexpr uint32_t DEFAULT_T_RAS = 32;
    static constexpr uint32_t DEFAULT_T_CAS = 14;
    static constexpr uint32_t DEFAULT_T_REFI = 7800;
    static constexpr uint32_t DEFAULT_T_RFC = 350;
    static constexpr uint64_t DEFAULT_CAPACITY = 8ULL * 1024 * 1024 * 1024;  // 8 GB

    explicit DRAMCtrlTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "DRAMCtrlTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_num_channels(uint32_t n) { num_channels_ = n; }
    void set_num_ranks(uint32_t n) { num_ranks_ = n; }
    void set_num_banks(uint32_t n) { num_banks_ = n; }
    void set_timing(uint32_t t_rcd, uint32_t t_rp, uint32_t t_ras,
                    uint32_t t_cas, uint32_t t_refi, uint32_t t_rfc);
    void set_page_policy(PagePolicy p) { page_policy_ = p; }
    void set_scheduler(Scheduler s) { scheduler_ = s; }

    // === 端口 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    struct DRAMAddress {
        uint32_t channel, rank, bank, row, col;
    };
    DRAMAddress decode_address(uint64_t addr) const;

    uint64_t compute_access_latency(const DRAMAddress& addr, bool is_write,
                                    bool page_hit);
    bool needs_refresh(uint64_t now);
    void perform_refresh(uint64_t now);

    // 配置
    uint32_t num_channels_, num_ranks_, num_banks_;
    uint32_t t_rcd_, t_rp_, t_ras_, t_cas_, t_refi_, t_rfc_;
    PagePolicy page_policy_;
    Scheduler scheduler_;
    uint64_t capacity_bytes_;

    // 状态
    struct BankState {
        uint64_t open_row;
        uint64_t last_access_cycle;
        bool is_open;
    };
    std::vector<std::vector<std::vector<BankState>>> bank_state_table_;
    std::vector<std::vector<std::vector<uint64_t>>> page_table_;
    uint64_t last_refresh_cycle_;

    // 队列
    std::deque<CacheReqBundle> read_queue_;
    std::deque<CacheReqBundle> write_queue_;
    std::map<uint64_t, uint64_t> pending_responses_;
    std::map<uint64_t, uint64_t> data_store_;

    // 统计
    tlm_stats::Scalar reads_completed_;
    tlm_stats::Scalar writes_completed_;
    tlm_stats::Scalar page_hits_;
    tlm_stats::Scalar page_misses_;
    tlm_stats::Scalar refreshes_performed_;
    tlm_stats::Scalar bank_conflicts_;
    tlm_stats::Distribution access_latency_;
    tlm_stats::Distribution queueing_delay_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 5 阶段

```cpp
void DRAMCtrlTLM::tick() {
    uint64_t now = current_cycle();

    // 1. 派发 pending responses
    dispatch_pending_responses(now);

    // 2. 处理刷新（如果需要）
    if (needs_refresh(now)) {
        perform_refresh(now);
    }

    // 3. 接收新请求（分离 read/write 队列）
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        if (req.is_write.read()) {
            write_queue_.push_back(req);
        } else {
            read_queue_.push_back(req);
        }
        req_in_.consume();
    }

    // 4. bank 调度（按 scheduler 选择请求）
    schedule_and_dispatch(now);

    // 5. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 schedule_and_dispatch

```cpp
void DRAMCtrlTLM::schedule_and_dispatch(uint64_t now) {
    // 优先级：read > write（可配）
    for (auto& req_opt : {try_schedule(read_queue_, now),
                          try_schedule(write_queue_, now)}) {
        if (req_opt) {
            const auto& req = *req_opt;
            DRAMAddress addr = decode_address(req.address.read());
            bool page_hit = (page_table_[addr.channel][addr.rank][addr.bank] == addr.row);
            uint64_t latency = compute_access_latency(addr, req.is_write.read(), page_hit);

            if (page_hit) ++page_hits_;
            else ++page_misses_;

            // 更新 page table
            if (page_policy_ == PagePolicy::OPEN) {
                page_table_[addr.channel][addr.rank][addr.bank] = addr.row;
            }

            // 派发 response
            pending_responses_[req.transaction_id.read()] = now + latency;
        }
    }
}
```

### 4.3 compute_access_latency

```cpp
uint64_t DRAMCtrlTLM::compute_access_latency(const DRAMAddress& addr,
                                              bool is_write, bool page_hit) {
    auto& bank = bank_state_table_[addr.channel][addr.rank][addr.bank];

    if (page_hit) {
        // 页命中：仅 tCAS
        return t_cas_;
    } else {
        // 页缺失：tRP（关闭） + tRCD（开新页） + tCAS
        uint64_t latency = t_rp_ + t_rcd_ + t_cas_;
        if (bank.is_open) {
            // 需要先 PRECHARGE
            ++bank_conflicts_;
        }
        bank.is_open = true;
        bank.open_row = addr.row;
        bank.last_access_cycle = current_cycle();
        return latency;
    }
}
```

## 5. Bundle 字段使用

| 字段 | DRAMCtrlTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——pending_responses_ 键 |
| `address` | **关键**——DRAMAddress 解码 |
| `is_write` | **关键**——read/write 队列分离 + 调度优先级 |
| `size` | 决定 burst 长度（典型 64 B） |
| `data` | 写时存 data_store_ |

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/draming_ctrl.hh` DRAMCtrl | `tlm::DRAMCtrlTLM` | 简化：v0 仅 1 scheduler + 1 page policy |
| `DRAMCtrl::activateBank` | `compute_access_latency` (page miss 路径) | 同语义 |
| `DRAMCtrl::prechargeBank` | `compute_access_latency` (bank.close 路径) | 同语义 |
| `DRAMCtrl::burst` | (tCAS + burst 长度) | 同语义 |
| `DRAMCtrl::refresh` | `perform_refresh` | 同语义 |
| `DRAMCtrl::addToReadQueue` | `read_queue_.push_back` | 同语义 |
| `DRAMCtrl::addToWriteQueue` | `write_queue_.push_back` | 同语义 |
| `DRAMCtrl::decodeAddr` | `decode_address` | 同语义 |

## 7. 实施路径

### 7.1 Phase 7.D 步骤

1. 新建 `include/tlm/memory/dram_ctrl_tlm.hh`（~400 行）
2. 实现 `DRAMAddress` + `decode_address` (DDR4 地址映射)
3. 实现 `bank_state_table_` + `page_table_`
4. 实现 `compute_access_latency` (page hit/miss + tRP/tRCD/tCAS)
5. 实现 3 种 scheduler (FR_FCFS / PAR_BS / FCFS) — v0 简化版
6. 实现 3 种 page_policy (OPEN / CLOSE / ADAPTIVE)
7. 实现 `perform_refresh`
8. 加 Catch2 测试：`test/test_dram_ctrl.cc`
9. 新增 `configs/dram_ctrl_test.json`（CPU↔DRAM 端到端）

### 7.2 Phase 7.D+ 步骤（优化）

1. 真实 PAR-BS 调度器（parallel aware bank scheduling）
2. 真实 DRAMSim2 集成（v0 留接口）
3. 温度感知刷新（v0 留接口）

### 7.3 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[dram]"` 全部通过
- [ ] page hit/miss 行为正确
- [ ] bank 调度生效
- [ ] 刷新周期正确（tREFI 7.8μs）
- [ ] 端到端带宽/延迟合理

### 7.4 估计工作量

- 设计: 1 周
- 基础版实施: 3-4 周
- 测试: 1 周
- **总计: 5-6 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **DRAM 时序模型不真实**——参数不准 | 中 | 中 | 单元测试对齐 Micron DDR4 datasheet |
| R2 | **bank 调度器复杂度**——PAR-BS 难实现 | 高 | 中 | v0 简化 FR-FCFS，PAR-BS 留 Phase 7.D+ |
| R3 | **刷新开销**——频繁刷新降吞吐 | 中 | 中 | tREFI 准确；refresh 并行化（v0 简化） |
| R4 | **多 channel 一致性**——跨 channel 顺序保证 | 中 | 中 | v0 单 channel 优先；多 channel 留 Phase 7.D+ |
| R5 | **address decode 错误**——地址映射 bug | 中 | 高 | 单元测试覆盖（线性/随机/页对齐访问） |
| R6 | **data_store_ 大内存**——8 GB 默认 | 中 | 中 | 默认值改 1 GB，配置可调 |
| R7 | **调度死锁**——读/写队列互锁 | 低 | 高 | 加 watchdog + 强制消费 |

## 9. 设计决策点

### D1 默认 scheduler

- **Q**: 默认 FR-FCFS / PAR-BS / FCFS？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: FR-FCFS（gem5 默认）
- **依赖**: 性能优化目标

### D2 默认 page_policy

- **Q**: 默认 OPEN / CLOSE / ADAPTIVE？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: OPEN（典型 DDR4 控制器）
- **依赖**: 工作集特征

### D3 多 channel 支持

- **Q**: v0 支持多 channel 还是仅 1 channel？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 1 channel（v0 简化）；多 channel 留 Phase 7.D+
- **依赖**: 复杂度 vs 真实度

### D4 集成 DRAMSim2

- **Q**: v0 集成 DRAMSim2 还是纯 C++ 实现？
- **状态**: 留待 Phase 7.D+ 设计时确定
- **建议**: v0 纯 C++ 简化（避免外部依赖）
- **依赖**: 真实度 vs 集成复杂度

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.2）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7.D (未来)**: 基础版实施（bank/rank/page/refresh）
- **Phase 7.D+ (未来)**: 真实 PAR-BS + DRAMSim2 集成
