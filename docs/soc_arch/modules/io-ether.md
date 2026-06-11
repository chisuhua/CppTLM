# io-ether 微架构文档

> **类别**: io > ether
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/ether_link_tlm.hh` + `ether_device_tlm.hh`
> **蓝图来源**: gem5 `src/dev/net/etherlink.hh`（链路层）+ `src/dev/net/i82563.hh`（设备）
> **首版 commit**: 蓝图（来自调研 §2.5）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [io-dma.md](./io-dma.md) (DMA 帧传输) | [io-pci.md](./io-pci.md) | [noc.common.md](./noc.common.md)

---

## 1. 设计目标（蓝图）

`tlm::EtherLinkTLM` + `tlm::EtherDeviceTLM` 是 CppTLM v2.2+ 规划的 **以太网链路层 + 网络设备**——两个端点通过共享 channel 通信。**与 gem5 对位**: `gem5::EtherLink`（~200 行，连接两端 `EtherBus` + 延迟/丢包）+ `i82563` 等 NIC 设备。

**核心特征**：
- **EtherLink：双端点 + 共享 channel**（两端 `EtherDevice` 互连）
- **延迟注入**（典型 0.1-10 μs）
- **丢包率**（可配，模拟拥塞）
- **EtherDevice：NIC 抽象**（继承 DmaDeviceTLM）
- **TX/RX 帧**（典型 1500 B MTU）
- **DMA 帧缓冲**（与 memory 交换）

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│              EtherLinkTLM (双端点)                           │
│                                                             │
│  ┌────────────────────┐         ┌────────────────────┐    │
│  │  endpoint[0]       │         │  endpoint[1]       │    │
│  │  (EtherDevice A)   │         │  (EtherDevice B)   │    │
│  └─────────┬──────────┘         └──────────┬─────────┘    │
│            │                                │               │
│            ▼                                ▼               │
│  ┌──────────────────────────────────────────────────┐     │
│  │  shared_channel_                                  │     │
│  │    - link_latency_: 100-10000 cycle              │     │
│  │    - drop_rate_: 0.0-1.0 (可配)                  │     │
│  │    - in_flight_frames_                           │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│              EtherDeviceTLM (单 NIC)                         │
│                                                             │
│  继承 DmaDeviceTLM:                                          │
│    - 接收 EtherLink endpoint                                 │
│    - 通过 DMA 读帧到内存                                    │
│    - 通过 DMA 从内存写帧到 endpoint                         │
│                                                             │
│  帧格式:                                                    │
│    { dst_mac[6], src_mac[6], ethertype[2], payload[N], crc[4]} │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 EtherLink 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `endpoint_a_frame_in_` | `InputStreamAdapter<EtherFrameBundle>` | 从 A 接收帧 |
| `endpoint_a_frame_out_` | `OutputStreamAdapter<EtherFrameBundle>` | 向 A 发送帧 |
| `endpoint_b_frame_in_` | `InputStreamAdapter<EtherFrameBundle>` | 从 B 接收帧 |
| `endpoint_b_frame_out_` | `OutputStreamAdapter<EtherFrameBundle>` | 向 B 发送帧 |

### 2.2 EtherDevice 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `link_frame_in_` | `InputStreamAdapter<EtherFrameBundle>` | 从链路接收帧 |
| `link_frame_out_` | `OutputStreamAdapter<EtherFrameBundle>` | 向链路发送帧 |
| `dma_port_master_` | `OutputStreamAdapter<CacheReqBundle>` | DMA 帧到内存（继承） |
| `dma_port_slave_` | `InputStreamAdapter<CacheRespBundle>` | 接收内存响应（继承） |

## 3. 接口（规划）

```cpp
namespace tlm {

// === EtherLink ===
class EtherLinkTLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_LINK_LATENCY = 1000;  // 1 μs
    static constexpr double DEFAULT_DROP_RATE = 0.0;       // 0%

    explicit EtherLinkTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "EtherLinkTLM"; }

    void on_config_loaded() override;
    void set_link_latency(uint32_t cycles) { link_latency_ = cycles; }
    void set_drop_rate(double rate) { drop_rate_ = rate; }
    void set_link_speed_bps(uint64_t bps) { link_speed_bps_ = bps; }

    cpptlm::InputStreamAdapter<bundles::EtherFrameBundle>& endpoint_a_frame_in();
    cpptlm::OutputStreamAdapter<bundles::EtherFrameBundle>& endpoint_a_frame_out();
    cpptlm::InputStreamAdapter<bundles::EtherFrameBundle>& endpoint_b_frame_in();
    cpptlm::OutputStreamAdapter<bundles::EtherFrameBundle>& endpoint_b_frame_out();

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void forward_frame(bool from_a, const EtherFrameBundle& frame);

    uint32_t link_latency_;
    double drop_rate_;
    uint64_t link_speed_bps_;

    // 端口
    InputStreamAdapter<EtherFrameBundle> endpoint_a_frame_in_;
    OutputStreamAdapter<EtherFrameBundle> endpoint_a_frame_out_;
    InputStreamAdapter<EtherFrameBundle> endpoint_b_frame_in_;
    OutputStreamAdapter<EtherFrameBundle> endpoint_b_frame_out_;

    struct PendingFrame {
        uint64_t arrival_time;
        uint64_t departure_time;
        bool from_a;
        EtherFrameBundle frame;
    };
    std::deque<PendingFrame> pending_frames_;

    // 统计
    tlm_stats::Scalar frames_forwarded_;
    tlm_stats::Scalar frames_dropped_;
    tlm_stats::Scalar bytes_forwarded_;
    tlm_stats::Distribution frame_latency_;
};

// === EtherDevice ===
class EtherDeviceTLM : public DmaDeviceTLM {
public:
    static constexpr uint64_t DEFAULT_MAC_ADDR = 0x000102030405ULL;
    static constexpr uint16_t DEFAULT_MTU = 1500;

    explicit EtherDeviceTLM(const std::string& name, EventQueue* eq,
                            uint64_t mac_addr = DEFAULT_MAC_ADDR);

    std::string get_module_type() const override { return "EtherDeviceTLM"; }

    void on_config_loaded() override;
    void set_mac_addr(uint64_t mac) { mac_addr_ = mac; }
    void set_mtu(uint16_t mtu) { mtu_ = mtu; }

    // === 帧发送 ===
    void send_frame(const EtherFrameBundle& frame);

    // === 帧接收（on_dma_complete 注入内存后回调） ===
    void on_dma_complete(uint64_t txn_id, bool success) override;

    // 端口
    cpptlm::InputStreamAdapter<bundles::EtherFrameBundle>& link_frame_in();
    cpptlm::OutputStreamAdapter<bundles::EtherFrameBundle>& link_frame_out();

    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    uint64_t mac_addr_;
    uint16_t mtu_;
    InputStreamAdapter<EtherFrameBundle> link_frame_in_;
    OutputStreamAdapter<EtherFrameBundle> link_frame_out_;

    // 统计
    tlm_stats::Scalar frames_sent_;
    tlm_stats::Scalar frames_received_;
    tlm_stats::Scalar bytes_sent_;
    tlm_stats::Scalar bytes_received_;
    tlm_stats::Scalar frames_dropped_mtu_;
};

// === Frame Bundle ===
struct EtherFrameBundle {
    uint64_t dst_mac;
    uint64_t src_mac;
    uint16_t ethertype;
    std::vector<uint8_t> payload;
    uint32_t crc;
    uint64_t transaction_id;
};

}  // namespace tlm
```

## 4. 行为流程

```cpp
// EtherLink tick
void EtherLinkTLM::tick() {
    uint64_t now = current_cycle();

    // 1. 派发到期的帧
    while (!pending_frames_.empty() &&
           pending_frames_.front().departure_time <= now) {
        auto pf = pending_frames_.front();
        pending_frames_.pop_front();
        if (pf.from_a) {
            endpoint_b_frame_out_.write(pf.frame);
        } else {
            endpoint_a_frame_out_.write(pf.frame);
        }
        ++frames_forwarded_;
    }

    // 2. 从 A 接收
    if (endpoint_a_frame_in_.valid() && endpoint_a_frame_in_.ready()) {
        const auto& frame = endpoint_a_frame_in_.data();
        forward_frame(true, frame);
        endpoint_a_frame_in_.consume();
    }

    // 3. 从 B 接收
    if (endpoint_b_frame_in_.valid() && endpoint_b_frame_in_.ready()) {
        const auto& frame = endpoint_b_frame_in_.data();
        forward_frame(false, frame);
        endpoint_b_frame_in_.consume();
    }

    if (adapter_) adapter_->tick();
}

void EtherLinkTLM::forward_frame(bool from_a, const EtherFrameBundle& frame) {
    // 丢包决策
    if (uniform_random(0.0, 1.0) < drop_rate_) {
        ++frames_dropped_;
        return;
    }

    uint64_t now = current_cycle();
    uint64_t depart = now + link_latency_;
    pending_frames_.push_back({now, depart, from_a, frame});
}
```

## 5. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/dev/net/etherlink.hh` EtherLink | `tlm::EtherLinkTLM` | 同语义 |
| `EtherLink::link` | `link_latency_` | 同语义 |
| `EtherLink::drop` | `drop_rate_` | 同语义 |
| `src/dev/net/i82563.hh` i82563 | `tlm::EtherDeviceTLM` | 抽象版（无 82563 特定寄存器） |
| `i82563::Packet` | `EtherFrameBundle` | 简化（无完整 TCP/IP） |
| `i82563::Intr` | (v0 留空) | Phase 7+ |

## 6. 实施路径

### Phase 7+ 步骤

1. 新建 `EtherFrameBundle` 在 `include/bundles/`
2. 新建 `EtherLinkTLM`（~200 行）
3. 新建 `EtherDeviceTLM`（~250 行）继承 DmaDeviceTLM
4. 加 Catch2 测试：`test/test_ether_link.cc`

**估计工作量**: 2-3 周（基础版）

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **丢包模型简化**——v0 随机丢包 | 中 | 中 | 暴露 drop 模式（uniform / burst） |
| R2 | **带宽模型缺失**——v0 无 link_speed 真实限制 | 中 | 中 | 暴露 link_speed_bps_（v0 简化仅记录） |
| R3 | **MTU 违反处理**——超长帧丢弃 | 中 | 中 | 显式统计 frames_dropped_mtu_ |
| R4 | **TCP/IP 协议栈**——v0 仅 Ethernet，不含 IP/TCP | 中 | 中 | Phase 7+ 集成 lwIP 或自实现 |

## 8. 决策点

### D1 链路速度
- **Q**: 默认 link_speed_bps_ 多少？
- **建议**: 1 Gbps（典型 1G 以太网）
- **依赖**: 应用场景

### D2 丢包默认
- **Q**: 默认 drop_rate_ 多少？
- **建议**: 0.0（v0 无丢包）
- **依赖**: 仿真目的

### D3 EtherFrameBundle vs NoCFlitBundle
- **Q**: 帧与 flit 关系？
- **建议**: 独立（Ethernet 帧 vs NoC flit，v0 不互通）
- **依赖**: 协议层级

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7+ (未来)**: 基础版实施（EtherLink + EtherDevice）
