# io-dma 微架构文档

> **类别**: io > dma
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/dma_device_tlm.hh`
> **蓝图来源**: gem5 `src/dev/dma_device.hh`（DmaDevice 抽象 + DmaPort）
> **首版 commit**: 蓝图（来自调研 §2.5）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [io-pio.md](./io-pio.md) | [io-disk.md](./io-disk.md) | [memory-memtlm.md](./memory-memtlm.md)

---

## 1. 设计目标（蓝图）

`tlm::DmaDeviceTLM` 是 CppTLM v2.2+ 规划的 **DMA 设备抽象**——设备主动发起内存读写，绕过 CPU 干预。**与 gem5 对位**: `gem5::DmaDevice`（~200 行，DmaPort + dmaRead/dmaWrite）。

**核心特征**：
- **DmaDevice 抽象基类**
- **双向端口**（device port + memory port）
- **dmaRead / dmaWrite** 设备主动发起
- **完成回调**（callback 通知 DMA 完成）
- **支持多个并发 DMA 请求**

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                  DmaDeviceTLM 抽象                          │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  DmaDeviceTLM (abstract)                         │     │
│  │    - dma_port_: DmaPort (双向)                    │     │
│  │    - dmaRead(addr, size, cb)                     │     │
│  │    - dmaWrite(addr, data, size, cb)              │     │
│  │    - on_dma_complete(): virtual 回调              │     │
│  └──────────────────────────────────────────────────┘     │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  具体设备 (SimpleDisk, EtherDevice, ...)           │     │
│  │    - 实现 on_dma_complete 处理设备特定逻辑         │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 与 PIO 设备的对比

| 维度 | PioDeviceTLM | DmaDeviceTLM |
|------|--------------|--------------|
| **发起方** | CPU | 设备 |
| **CPU 参与** | 必须（load/store） | 无（设备独立） |
| **适用场景** | 配置寄存器 / 状态查询 | 大块数据传输 |
| **Bundle** | PioReqBundle | CacheReqBundle（复用） |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `dma_port_master_` | `OutputStreamAdapter<CacheReqBundle>` | 设备发起 read/write |
| `dma_port_slave_` | `InputStreamAdapter<CacheRespBundle>` | 接收内存响应 |

## 3. 接口（规划）

```cpp
namespace tlm {

using DmaCallback = std::function<void(uint64_t txn_id, bool success)>;

class DmaDeviceTLM : public ChStreamModuleBase {
public:
    explicit DmaDeviceTLM(const std::string& name, EventQueue* eq,
                          uint32_t max_outstanding_dma = 4);

    std::string get_module_type() const override { return "DmaDeviceTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_max_outstanding_dma(uint32_t n) { max_outstanding_dma_ = n; }

    // === DMA 发起（设备主动） ===
    bool dmaRead(uint64_t addr, uint64_t size, DmaCallback cb);
    bool dmaWrite(uint64_t addr, uint64_t data, uint64_t size, DmaCallback cb);

    // === 抽象回调（继承者实现） ===
    virtual void on_dma_complete(uint64_t txn_id, bool success) = 0;

    // === 端口 ===
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& dma_port_master();
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& dma_port_slave();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    struct DmaRequest {
        uint64_t txn_id;
        uint64_t addr;
        uint64_t size;
        bool is_write;
        DmaCallback callback;
    };
    std::deque<DmaRequest> pending_dmas_;
    std::map<uint64_t, DmaCallback> inflight_callbacks_;

    uint32_t max_outstanding_dma_;

    // 统计
    tlm_stats::Scalar dmas_started_;
    tlm_stats::Scalar dmas_completed_;
    tlm_stats::Scalar dmas_failed_;
    tlm_stats::Average outstanding_dmas_;
};

}  // namespace tlm
```

## 4. 行为流程

```cpp
bool DmaDeviceTLM::dmaRead(uint64_t addr, uint64_t size, DmaCallback cb) {
    if (inflight_callbacks_.size() >= max_outstanding_dma_) {
        return false;  // 队列满
    }

    uint64_t txn_id = next_txn_id_++;
    inflight_callbacks_[txn_id] = cb;

    CacheReqBundle req;
    req.transaction_id.write(txn_id);
    req.address.write(addr);
    req.size.write(size);
    req.is_write.write(0);
    dma_port_master_.write(req);

    ++dmas_started_;
    outstanding_dmas_.sample(inflight_callbacks_.size());
    return true;
}

void DmaDeviceTLM::tick() {
    // 接收 DMA 完成响应
    if (dma_port_slave_.valid() && dma_port_slave_.ready()) {
        const auto& resp = dma_port_slave_.data();
        uint64_t txn_id = resp.transaction_id.read();
        bool success = (resp.error_code.read() == 0);

        auto it = inflight_callbacks_.find(txn_id);
        if (it != inflight_callbacks_.end()) {
            on_dma_complete(txn_id, success);
            it->second(txn_id, success);
            inflight_callbacks_.erase(it);
            ++dmas_completed_;
        }
        dma_port_slave_.consume();
    }
    if (adapter_) adapter_->tick();
}
```

## 5. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/dev/dma_device.hh` DmaDevice | `tlm::DmaDeviceTLM` | 同语义 |
| `DmaDevice::dmaPort` | `dma_port_master_/slave_` | 同语义 |
| `DmaDevice::dmaRead` | `dmaRead` | 同语义 |
| `DmaDevice::dmaWrite` | `dmaWrite` | 同语义 |
| `DmaPort::recvAtomic` | (v0 简化：仅 timing) | v0 简化 |

## 6. 实施路径

### Phase 7 备选 dGPU 步骤

1. 新建 `include/tlm/io/dma_device_tlm.hh`（~150 行）
2. 实现 2 端口（master + slave）
3. 实现 `dmaRead/dmaWrite` + callback 机制
4. 加 Catch2 测试

**估计工作量**: 1-2 周

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **callback 嵌套调用**——callback 中再调 dmaRead | 中 | 中 | 异步化：callback 推到事件队列 |
| R2 | **max_outstanding 错误**——队列满丢弃 DMA | 中 | 中 | 返回 false，调用方重试 |
| R3 | **memory port 兼容**——不同 memory controller 协议 | 中 | 中 | 用 CacheReqBundle（与现有兼容） |

## 8. 决策点

### D1 Callback 同步/异步
- **Q**: 同步 callback（立即调用）还是异步（下一 cycle）？
- **建议**: 异步（避免 tick 嵌套）
- **依赖**: 仿真语义

### D2 max_outstanding 默认
- **Q**: 默认 max_outstanding_dma 多少？
- **建议**: 4（与真实 DMA 控制器一致）
- **依赖**: 设备特性

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7 备选 dGPU (未来)**: 基础版实施
