# interconnect-bridge 微架构文档

> **类别**: Interconnect > Bridge
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/interconnect/bridge_tlm.hh`
> **蓝图来源**: gem5 `src/mem/bridge.hh` + `src/mem/port.hh`（延迟/带宽限流桥接）
> **首版 commit**: 蓝图（来自调研 §2.4 + Phase 5 备选）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4
> - 邻接: [interconnect-crossbar.md](./interconnect-crossbar.md) (v0 crossbar 已实施)

---

## 1. 设计目标（蓝图）

`tlm::BridgeTLM` 是 CppTLM v2.2+ 规划的 **跨域桥接器**（cross-domain bridge），用于将两个独立 CoherenceDomain 互连（如 CPU 域 ↔ GPU 域，APU 域 ↔ dGPU 域）。**与 gem5 对位**: `gem5::Bridge`（端口对 + 延迟/带宽限流 + 协议转换）。

**核心特征**：
- **跨 CoherenceDomain 桥接**（CPU 域 ↔ GPU 域，APU 域 ↔ dGPU 域）
- **延迟注入**（end-to-end 50-500 cycle，对位 PCIe 物理层）
- **带宽限流**（bytes/cycle 上限，模拟 PCIe 链路）
- **协议转换占位**（Phase 7 备选 dGPU 阶段实施；v0 仅做透传）
- **单端口 + 单出口**（in/out 不对称，像 NICTLM 但同 Bundle）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                   BridgeTLM 单体                              │
│                                                             │
│  ┌────────────────────┐         ┌────────────────────┐    │
│  │  Master Port       │         │  Slave Port        │    │
│  │  (Domain A 侧)     │         │  (Domain B 侧)     │    │
│  │  ReqIn/RespOut     │         │  ReqIn/RespOut     │    │
│  └─────────┬──────────┘         └──────────┬─────────┘    │
│            │                                │               │
│            ▼                                ▼               │
│  ┌──────────────────────────────────────────────────┐     │
│  │  bridge_delay_queue_                              │     │
│  │    - per-txn FIFO（按 transaction_id 索引）       │     │
│  │    - 出队时间 = 入队时间 + bridge_latency_        │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                │               │
│            ▼                                ▼               │
│  ┌──────────────────────────────────────────────────┐     │
│  │  bandwidth_limiter_                               │     │
│  │    - bytes_in_flight_ < max_bytes_in_flight_     │     │
│  │    - 超限则 back-pressure（ready=false）          │     │
│  └──────────────────────────────────────────────────┘     │
│                                                             │
│  协议转换占位（Phase 5/7 备选 dGPU 启用）                   │
│    - Bundle A 类型 ↔ Bundle B 类型                          │
│    - coherence protocol 转换（MESI ↔ MOESI）              │
│    - address mapping（域内地址 ↔ 域间地址）                │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 应用场景

| 场景 | Master Domain | Slave Domain | 桥接延迟 |
|------|---------------|--------------|----------|
| **APU 内 CPU↔GPU** | CPU 域（MOESI） | GPU 域（MOESI_AMD） | 5-10 cycle（共享 L2/TCC 物理链路） |
| **dGPU 离散卡** | Host APU 域 | dGPU 域 | 100-500 cycle（PCIe 物理层） |
| **多 Cluster 互连** | Cluster A 域 | Cluster B 域 | 20-50 cycle（NoC 跨域链路） |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `master_req_in_` | `InputStreamAdapter<CacheReqBundle>` | Master 域侧接收请求 |
| `master_resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | Master 域侧发送响应 |
| `slave_req_out_` | `OutputStreamAdapter<CacheReqBundle>` | Slave 域侧发送请求 |
| `slave_resp_in_` | `InputStreamAdapter<CacheRespBundle>` | Slave 域侧接收响应 |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                    BridgeTLM 内部                            │
│                                                             │
│  MasterReq → [bridge_delay_queue_] → [bandwidth_limiter_]  │
│                                              │              │
│                                              ▼              │
│                                          SlaveReq           │
│                                                             │
│  SlaveResp → [bridge_delay_queue_] → [bandwidth_limiter_]  │
│                                              │              │
│                                              ▼              │
│                                          MasterResp         │
│                                                             │
│  共享:                                                      │
│    - inflight_txns_: map<txn_id, in_out_time>              │
│    - bytes_in_flight_: atomic<uint32>                      │
│    - snoop_forwarder_: 跨域 snoop 转发占位                  │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class BridgeTLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_BRIDGE_LATENCY = 100;  // 100 cycle
    static constexpr uint32_t DEFAULT_BANDWIDTH_LIMIT = 256; // bytes/cycle (PCIe x16 ~)

    explicit BridgeTLM(const std::string& name, EventQueue* eq,
                       uint32_t bridge_latency = DEFAULT_BRIDGE_LATENCY,
                       uint32_t bandwidth_bytes_per_cycle = DEFAULT_BANDWIDTH_LIMIT);

    std::string get_module_type() const override { return "BridgeTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_bridge_latency(uint32_t cycles) { bridge_latency_ = cycles; }
    void set_bandwidth_limit(uint32_t bytes_per_cycle) {
        bandwidth_bytes_per_cycle_ = bytes_per_cycle;
    }

    // === 域关联 ===
    void set_master_domain(CoherenceDomain* dom) { master_domain_ = dom; }
    void set_slave_domain(CoherenceDomain* dom) { slave_domain_ = dom; }

    // === Master 侧端口 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& master_req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& master_resp_out();

    // === Slave 侧端口 ===
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& slave_req_out();
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& slave_resp_in();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void forward_master_to_slave();
    void forward_slave_to_master();
    bool check_bandwidth(const CacheReqBundle& req) const;

    uint32_t bridge_latency_;
    uint32_t bandwidth_bytes_per_cycle_;
    uint32_t bytes_in_flight_;

    CoherenceDomain* master_domain_;
    CoherenceDomain* slave_domain_;

    InputStreamAdapter<CacheReqBundle> master_req_in_;
    OutputStreamAdapter<CacheRespBundle> master_resp_out_;
    OutputStreamAdapter<CacheReqBundle> slave_req_out_;
    InputStreamAdapter<CacheRespBundle> slave_resp_in_;

    struct PendingTxn {
        uint64_t arrival_time;
        uint64_t departure_time;
        bool is_write;
    };
    std::map<uint64_t, PendingTxn> inflight_txns_;  // transaction_id → 状态

    // 统计
    tlm_stats::Scalar bridge_flits_forwarded_;
    tlm_stats::Scalar bridge_bytes_forwarded_;
    tlm_stats::Distribution bridge_latency_actual_;
    tlm_stats::Scalar bridge_bandwidth_throttled_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 4 阶段

```cpp
void BridgeTLM::tick() {
    uint64_t now = current_cycle();

    // 1. 处理到期的 inflight txns（dequeue 到对侧）
    for (auto it = inflight_txns_.begin(); it != inflight_txns_.end(); ) {
        if (it->second.departure_time <= now) {
            // 释放带宽
            bytes_in_flight_ -= ...;
            ++it;
        } else {
            ++it;
        }
    }

    // 2. Master → Slave 转发
    forward_master_to_slave();

    // 3. Slave → Master 转发
    forward_slave_to_master();

    // 4. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 forward_master_to_slave

```cpp
void BridgeTLM::forward_master_to_slave() {
    if (!master_req_in_.valid() || !master_req_in_.ready()) return;

    const auto& req = master_req_in_.data();

    // 带宽检查
    if (!check_bandwidth(req)) {
        bridge_bandwidth_throttled_++;
        return;  // back-pressure
    }

    // 延迟入队
    uint64_t now = current_cycle();
    uint64_t departure = now + bridge_latency_;
    inflight_txns_[req.transaction_id.read()] = {
        now, departure, req.is_write.read() == 1
    };
    bytes_in_flight_ += req.size.read();

    // 转发到 Slave 侧
    slave_req_out_.write(req);
    master_req_in_.consume();

    bridge_flits_forwarded_++;
    bridge_bytes_forwarded_ += req.size.read();
}

bool BridgeTLM::check_bandwidth(const CacheReqBundle& req) const {
    uint32_t req_bytes = req.size.read();
    return (bytes_in_flight_ + req_bytes) <= bandwidth_bytes_per_cycle_;
}
```

### 4.3 关键设计取舍

- **延迟按 transaction 计数**：v0 简化（不按 flit 拆延迟）
- **带宽按 cycle 限流**：v0 简化（不做 burst 累积）
- **协议转换 v0 不做**：透传 Bundle；Phase 5/7 备选 dGPU 启用 Bundle 转换
- **跨域 snoop 转发 v0 占位**：仅注册 callback，不实际转发

## 5. Bundle 字段使用（规划）

| 字段 | BridgeTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——`inflight_txns_` 映射键 |
| `address` | 透传（v0 不做地址转换） |
| `size` | **关键**——带宽限流计算 |
| `is_write` | 统计 |
| `kernel_id` | 透传（Phase 7.D 跨域一致性追踪） |
| `coherence_msg` | 透传（Phase 7.C+ 跨域 snoop） |

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/bridge.hh` Bridge | `tlm::BridgeTLM` | 简化：v0 不做协议转换 |
| `src/mem/port.hh` MasterPort/SlavePort | `master_req_in_/slave_req_out_` | 同名沿用 |
| `src/mem/bridge.hh` `delay` 字段 | `bridge_latency_` | 同样按 cycle 计数 |
| `src/mem/bridge.hh` `bandwidth` 字段 | `bandwidth_bytes_per_cycle_` | 同样按 bytes/cycle |
| `src/mem/bridge.hh` 协议转换 | (v0 留空) | Phase 5/7 备选 dGPU 实施 |

## 7. 实施路径

### 7.1 Phase 5 步骤（基础版，仅延迟+带宽）

1. 新建 `include/tlm/interconnect/bridge_tlm.hh`（~250 行）
2. 实现 Master/Slave 4 端口 + bridge_delay_queue_
3. 写带宽限流逻辑（v0 简化版：按 cycle 计数）
4. 加 Catch2 测试：`test/test_bridge_tlm.cc`
5. 新增 `configs/bridge_test.json`（CPU 域 + GPU 域通过 BridgeTLM 互连）
6. `chstream_register.hh` 注册

### 7.2 Phase 7 备选 dGPU 步骤（协议转换版）

1. 扩展 `BridgeTLM` 模板化 `BundleA, BundleB` 类型
2. 实现 Bundle 转换函数
3. 实现 MESI ↔ MOESI 协议转换
4. 实现 address mapping（域内 ↔ 域间）
5. 集成 `CoherenceDomain::register_bridge()`

### 7.3 验收标准（Phase 5 基础版）

- [ ] 编译通过
- [ ] `cpptlm_tests "[bridge]"` 全部通过
- [ ] 端到端 2 域通过 Bridge 通信
- [ ] 延迟注入真实生效（实测 latency = bridge_latency_）
- [ ] 带宽限流真实生效（超限 back-pressure）

### 7.4 估计工作量

- 设计: 0.5 周
- Phase 5 基础版实施: 1 周
- Phase 7 备选 dGPU 协议转换: 2-3 周
- 测试: 0.5 周
- **总计: 4-5 周**（含 dGPU 协议转换）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **延迟测量误差**——inflight_txns_ 维护 overhead | 中 | 中 | 用 fixed-size 数组代替 std::map（O(1) 查找） |
| R2 | **带宽限流死锁**——反向响应被限流 | 中 | 高 | 响应走独立 bandwidth 池（`bandwidth_resp_pool_`） |
| R3 | **协议转换复杂度**——Phase 5 v0 不做 | 中 | 中 | 占位 + TODO 注释；Phase 7 备选 dGPU 实施 |
| R4 | **跨域 snoop 风暴**——多 Bridge × 多 Cache 组合 | 中 | 中 | Phase 7.C SnoopFilter 减负 |
| R5 | **死锁风险**——bridge 形成环路 | 低 | 高 | 配置时检测 Bridge 拓扑环路（DFS） |
| R6 | **大延迟下 inflight_txns_ 内存膨胀** | 中 | 中 | 设置 `MAX_INFLIGHT_TXNS` 阈值（默认 1024） |
| R7 | **带宽 burst 模型缺失**——v0 简化限流不真实 | 中 | 中 | 暴露 `set_burst_credit()` 接口（v0.1 实施） |

## 9. 设计决策点

### D1 默认 bridge latency

- **Q**: 默认 bridge_latency_ 多少 cycle？
- **状态**: 留待 Phase 5 设计时确定
- **建议**: APU 内 = 5 cycle，dGPU (PCIe) = 200 cycle
- **依赖**: gem5 `Bridge::delay` 默认值

### D2 带宽模型

- **Q**: 带宽按 cycle 限流 还是按时间窗口平均？
- **状态**: 留待 Phase 5 设计时确定
- **建议**: 按 cycle 限流（v0 简化），Phase 7 备选 dGPU 加 burst model
- **依赖**: PCIe 物理层规范

### D3 协议转换触发条件

- **Q**: 何时启用协议转换？
- **状态**: 留待 Phase 5 设计时确定
- **建议**: 同一 coherence protocol（MOESI ↔ MOESI）→ 不转换；不同（MOESI ↔ MESI）→ 转换
- **依赖**: Phase 7.C CoherenceDomain 实施

### D4 跨域 snoop 转发

- **Q**: Bridge 是否转发 snoop？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: v0 占位（注册 callback），Phase 7.C 真实转发

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.4）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 5 (未来)**: 基础版实施（仅延迟+带宽）
- **Phase 7 备选 dGPU (未来)**: 协议转换实施（Bundle 类型化 + address mapping）
