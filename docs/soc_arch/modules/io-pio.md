# io-pio 微架构文档

> **类别**: io > pio
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/pio_device_tlm.hh`
> **蓝图来源**: gem5 `src/dev/io_device.hh`（PioDevice 抽象 + 设备特定 PIO 实现）
> **首版 commit**: 蓝图（来自调研 §2.5）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [io-dma.md](./io-dma.md) | [io-pci.md](./io-pci.md) | [coherence-bridge.md](./coherence-bridge.md) (CPU↔dGPU 桥接)

---

## 1. 设计目标（蓝图）

`tlm::PioDeviceTLM` 是 CppTLM v2.2+ 规划的 **Programmed I/O (PIO) 设备抽象**——CPU 通过 load/store 指令直接访问设备寄存器，绕过 DMA。**与 gem5 对位**: `gem5::PioDevice`（~200 行，抽象基类）+ 设备特定实现（如 Uart、Terminal）。

**核心特征**：
- **PioDevice 抽象基类**（`read()` / `write()` 接口）
- **地址映射**（per device address range）
- **直接 load/store 协议**（CPU 端无需特殊指令）
- **可选延迟注入**（v0 简化：固定 1-10 cycle）
- **多设备共存**（同一 Bus 多个 PIO 设备通过地址路由）

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                  PioDeviceTLM 抽象                           │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  PioDeviceTLM (abstract)                         │     │
│  │    - virtual read(addr, size) → data            │     │
│  │    - virtual write(addr, data, size)            │     │
│  │    - address_range_: { base, size }              │     │
│  │    - latency_: 默认 1-10 cycle                   │     │
│  └──────────────────────────────────────────────────┘     │
│                          ↑ 继承                              │
│  ┌──────────────────────────────────────────────────┐     │
│  │  具体设备 (Uart8250, Terminal, PciHost, ...)      │     │
│  │    - 实现 read/write                              │     │
│  │    - 维护设备状态 (registers)                     │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `pio_req_in_` | `InputStreamAdapter<PioReqBundle>` | 接收 PIO 请求（read/write） |
| `pio_resp_out_` | `OutputStreamAdapter<PioRespBundle>` | 发送 PIO 响应（read data） |

### 2.2 Bundle 类型（需新增）

> PIO 使用 **专用 Bundle**（与 CacheReqBundle 不同）：
> - `PioReqBundle` { addr, write_data, size, is_write, transaction_id }
> - `PioRespBundle` { transaction_id, read_data, error }

## 3. 接口（规划）

```cpp
namespace tlm {

class PioDeviceTLM : public ChStreamModuleBase {
public:
    explicit PioDeviceTLM(const std::string& name, EventQueue* eq,
                          uint64_t base_addr, uint64_t size,
                          uint32_t latency = 1);

    std::string get_module_type() const override { return "PioDeviceTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_address_range(uint64_t base, uint64_t size) {
        base_addr_ = base; size_ = size;
    }
    void set_latency(uint32_t cycles) { latency_ = cycles; }

    // === 抽象接口（继承者实现） ===
    virtual uint64_t read(uint64_t offset, unsigned size_bytes) = 0;
    virtual void write(uint64_t offset, uint64_t data, unsigned size_bytes) = 0;

    // === 地址检查 ===
    bool contains_addr(uint64_t addr) const {
        return addr >= base_addr_ && addr < base_addr_ + size_;
    }

    // === 端口 ===
    cpptlm::InputStreamAdapter<bundles::PioReqBundle>& pio_req_in();
    cpptlm::OutputStreamAdapter<bundles::PioRespBundle>& pio_resp_out();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

protected:
    uint64_t base_addr_;
    uint64_t size_;
    uint32_t latency_;

    // 统计
    tlm_stats::Scalar reads_;
    tlm_stats::Scalar writes_;
    tlm_stats::Distribution latency_actual_;
};

}  // namespace tlm
```

## 4. 行为流程（规划）

```cpp
void PioDeviceTLM::tick() {
    if (!pio_req_in_.valid() || !pio_req_in_.ready()) return;

    const auto& req = pio_req_in_.data();

    if (!contains_addr(req.address.read())) {
        // 地址不匹配，丢弃（或转发到下一 PIO 设备）
        pio_req_in_.consume();
        return;
    }

    uint64_t offset = req.address.read() - base_addr_;
    uint64_t depart = current_cycle() + latency_;

    if (req.is_write.read()) {
        write(offset, req.write_data.read(), req.size.read());
        ++writes_;
    } else {
        uint64_t data = read(offset, req.size.read());
        // 生成 response
        PioRespBundle resp;
        resp.transaction_id.write(req.transaction_id.read());
        resp.read_data.write(data);
        // 延迟入队
        pending_responses_[req.transaction_id.read()] = depart;
        ++reads_;
    }
    pio_req_in_.consume();
}
```

## 5. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/dev/io_device.hh` PioDevice | `tlm::PioDeviceTLM` | 同语义（抽象基类） |
| `PioDevice::read` | `virtual read` | 同语义 |
| `PioDevice::write` | `virtual write` | 同语义 |
| `PioDevice::addressRanges` | `address_range_` | 同语义 |
| `PioDevice::pioDelay` | `latency_` | 同语义 |

## 6. 实施路径

### Phase 7 备选 dGPU 步骤

1. 新建 `include/tlm/io/pio_device_tlm.hh`（~150 行）
2. 新建 `PioReqBundle` / `PioRespBundle`（在 `include/bundles/pio_bundles_tlm.hh`）
3. 实现 Uart8250 / Terminal / PciHost 等具体设备
4. 加 Catch2 测试

**估计工作量**: 1-2 周（基础版）

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **地址路由错误**——多 PIO 设备重叠 | 中 | 高 | 启动时校验无重叠 |
| R2 | **大小不一致**——read 4B 但设备仅 1B 寄存器 | 中 | 中 | size 检查 + 对齐 |
| R3 | **延迟模型缺失**——v0 固定延迟 | 中 | 中 | Phase 7+ 真实延迟 |

## 8. 决策点

### D1 默认延迟
- **Q**: 默认 latency_ 多少？
- **建议**: 1 cycle（PIO 真实延迟）
- **依赖**: 设备特性

### D2 PIO Bundle 与 CacheReq 关系
- **Q**: 独立 Bundle 还是复用 CacheReqBundle？
- **建议**: 独立 PioReqBundle（v0 简化）
- **依赖**: 协议清晰度

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7 备选 dGPU (未来)**: 基础版实施
