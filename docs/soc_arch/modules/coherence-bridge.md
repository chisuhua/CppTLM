# coherence-bridge 微架构文档

> **类别**: Coherence > Bridge
> **状态**: 🟡 规划中
> **Header**: (规划) `include/core/coherence_bridge.hh`
> **蓝图来源**: gem5 `src/mem/bridge.hh`（跨域桥接特化）+ `ProtocolBridge`（Phase 5 备选）
> **首版 commit**: 蓝图（来自调研 §2.6 + Phase 5/Phase 7 备选 dGPU）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.6
> - 邻接: [coherence-protocol.md](./coherence-protocol.md) (协议抽象层) | [interconnect-bridge.md](./interconnect-bridge.md) (链路桥接) | [coherence-domain.md](./coherence-domain.md) (✅ 基础设施已实施)

---

## 1. 设计目标（蓝图）

`tlm::CoherenceBridge` 是 CppTLM Phase 7.D+ 规划的 **跨 CoherenceDomain 协议桥接器**——连接两个独立 CoherenceDomain（不同协议或同协议不同域），处理协议转换、地址映射、snoop 转发。**与 gem5 对位**: `gem5::Bridge`（特化处理 coherence 协议）+ `ProtocolBridge`（Phase 5 备选）。

**核心特征**：
- **跨 CoherenceDomain 桥接**（CPU 域 ↔ GPU 域，APU 域 ↔ dGPU 域）
- **协议转换**（MOESI_AMD ↔ MESI_GPU；透传同协议）
- **地址映射**（域内地址 ↔ 域间地址，如 GPU VRAM ↔ Host 物理地址）
- **snoop 转发**（跨域 snoop probe 转发 + response 收集）
- **链路延迟注入**（与 interconnect-bridge 协同）

> **与 [interconnect-bridge.md](./interconnect-bridge.md) 的关系**: `interconnect-bridge.md` 关注**链路层桥接**（延迟+带宽），本文档关注**协议层桥接**（协议转换+地址映射+snoop 转发）。两者协同工作：interconnect-bridge 处理物理链路，coherence-bridge 处理 coherence 协议。

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│              CoherenceBridge 单体                             │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  Domain A 侧                                       │     │
│  │    - input_adapter_: CacheReqBundle → InternalMsg │     │
│  │    - output_adapter_: InternalMsg → CacheRespBundle│     │
│  │    - protocol_a_: shared_ptr<CoherenceProtocol>   │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  protocol_converter_                               │     │
│  │    - translate(probe, protocol_a_, protocol_b_)   │     │
│  │    - translate(req, protocol_a_, protocol_b_)     │     │
│  │    - translate(resp, protocol_b_, protocol_a_)    │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  address_mapper_                                   │     │
│  │    - map_a_to_b(addr) → addr'                    │     │
│  │    - map_b_to_a(addr) → addr'                    │     │
│  │    - 静态映射（v0）/动态页表（Phase 7+）         │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  snoop_forwarder_                                  │     │
│  │    - on_snoop_probe_a: 翻译 + 转发到 B 域          │     │
│  │    - on_snoop_resp_b: 翻译 + 转发回 A 域          │     │
│  │    - snoop_inflight_: 跨域 snoop 状态追踪         │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  Domain B 侧                                       │     │
│  │    - input_adapter_: CacheReqBundle → InternalMsg │     │
│  │    - output_adapter_: InternalMsg → CacheRespBundle│     │
│  │    - protocol_b_: shared_ptr<CoherenceProtocol>   │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 应用场景

| 场景 | Domain A 协议 | Domain B 协议 | 桥接需求 |
|------|---------------|--------------|----------|
| **APU 内 CPU↔GPU** | MOESI_AMD | MOESI_AMD | 透传 + 共享 L2/TCC 物理 |
| **APU CPU↔dGPU GPU** | MOESI_AMD | MESI_GPU | 协议转换 + 地址映射 (Host↔VRAM) |
| **多 Cluster 互连** | MOESI_AMD (Cluster A) | MOESI_AMD (Cluster B) | 透传 + NoC 桥接 |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `domain_a_req_in_` | `InputStreamAdapter<CacheReqBundle>` | Domain A 侧接收请求 |
| `domain_a_resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | Domain A 侧发送响应 |
| `domain_a_snoop_out_` | `OutputStreamAdapter<SnoopProbe>` | Domain A 侧广播 snoop |
| `domain_a_snoop_in_` | `InputStreamAdapter<SnoopResp>` | Domain A 侧接收 snoop response |
| `domain_b_req_out_` | `OutputStreamAdapter<CacheReqBundle>` | Domain B 侧发送请求 |
| `domain_b_resp_in_` | `InputStreamAdapter<CacheRespBundle>` | Domain B 侧接收响应 |
| `domain_b_snoop_in_` | `InputStreamAdapter<SnoopProbe>` | Domain B 侧接收 snoop |
| `domain_b_snoop_out_` | `OutputStreamAdapter<SnoopResp>` | Domain B 侧发送 snoop response |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  CoherenceBridge 内部                         │
│                                                             │
│  Domain A CacheReq → [protocol_a_] → [addr_map] →          │
│                    [snoop_forward] → [protocol_b_] →        │
│                    Domain B CacheReq                        │
│                                                             │
│  Domain B SnoopProbe → [protocol_b_] → [snoop_forward] →  │
│                       [protocol_a_] → Domain A SnoopProbe  │
│                                                             │
│  共享:                                                      │
│    - protocol_a_: shared_ptr<CoherenceProtocol>            │
│    - protocol_b_: shared_ptr<CoherenceProtocol>            │
│    - addr_map_a_to_b_: function<uint64_t(uint64_t)>        │
│    - addr_map_b_to_a_: function<uint64_t(uint64_t)>        │
│    - snoop_inflight_: map<txn_id, CrossDomainSnoopState>   │
│    - delay_queue_: per-txn FIFO（与 interconnect-bridge 协同）│
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {

class CoherenceBridge : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_BRIDGE_LATENCY = 200;  // 200 cycle (PCIe)

    CoherenceBridge(const std::string& name, EventQueue* eq,
                    std::shared_ptr<CoherenceProtocol> protocol_a,
                    std::shared_ptr<CoherenceProtocol> protocol_b,
                    uint32_t bridge_latency = DEFAULT_BRIDGE_LATENCY);

    std::string get_module_type() const override { return "CoherenceBridge"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_address_mapper(
        std::function<uint64_t(uint64_t)> a_to_b,
        std::function<uint64_t(uint64_t)> b_to_a);

    void set_bridge_latency(uint32_t cycles) { bridge_latency_ = cycles; }
    void enable_snoop_forwarding(bool en) { snoop_forwarding_enabled_ = en; }

    // === 域关联 ===
    void set_domain_a(CoherenceDomain* dom) { domain_a_ = dom; }
    void set_domain_b(CoherenceDomain* dom) { domain_b_ = dom; }

    // === Domain A 侧端口 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& domain_a_req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& domain_a_resp_out();
    cpptlm::OutputStreamAdapter<bundles::SnoopProbe>& domain_a_snoop_out();
    cpptlm::InputStreamAdapter<bundles::SnoopResp>& domain_a_snoop_in();

    // === Domain B 侧端口 ===
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& domain_b_req_out();
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& domain_b_resp_in();
    cpptlm::InputStreamAdapter<bundles::SnoopProbe>& domain_b_snoop_in();
    cpptlm::OutputStreamAdapter<bundles::SnoopResp>& domain_b_snoop_out();

    // === Snoop 转发 ===
    void forward_snoop_to_b(const SnoopProbe& probe);
    void forward_snoop_resp_to_a(const SnoopRespBundle& resp);

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void handle_domain_a_request();
    void handle_domain_b_response();
    void handle_domain_a_snoop_response();
    void handle_domain_b_snoop_probe();

    CacheReqBundle translate_request(const CacheReqBundle& req,
                                     CoherenceProtocol* from,
                                     CoherenceProtocol* to);
    SnoopProbe translate_snoop(const SnoopProbe& probe,
                               CoherenceProtocol* from,
                               CoherenceProtocol* to);

    std::shared_ptr<CoherenceProtocol> protocol_a_;
    std::shared_ptr<CoherenceProtocol> protocol_b_;
    CoherenceDomain* domain_a_;
    CoherenceDomain* domain_b_;

    std::function<uint64_t(uint64_t)> addr_map_a_to_b_;
    std::function<uint64_t(uint64_t)> addr_map_b_to_a_;

    uint32_t bridge_latency_;
    bool snoop_forwarding_enabled_;

    struct CrossDomainSnoop {
        uint64_t arrival_time;
        uint64_t departure_time;
        std::string source_protocol;
        std::string target_protocol;
    };
    std::map<uint64_t, CrossDomainSnoop> snoop_inflight_;  // transaction_id

    // 端口（同上）

    // 统计
    tlm_stats::Scalar cross_domain_requests_;
    tlm_stats::Scalar cross_domain_responses_;
    tlm_stats::Scalar cross_domain_snoop_forwards_;
    tlm_stats::Scalar cross_domain_protocol_translations_;
    tlm_stats::Scalar address_mappings_;
    tlm_stats::Distribution cross_domain_latency_;
};
}  // namespace tlm
```

## 4. 行为流程（规划）

### 4.1 tick() 6 阶段

```cpp
void CoherenceBridge::tick() {
    uint64_t now = current_cycle();

    // 1. 处理到期的 inflight txns（dequeue 到对侧）
    for (auto it = snoop_inflight_.begin(); it != snoop_inflight_.end(); ) {
        if (it->second.departure_time <= now) {
            it = snoop_inflight_.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Domain A 请求处理
    handle_domain_a_request();

    // 3. Domain B 响应处理
    handle_domain_b_response();

    // 4. Domain A snoop response 处理
    handle_domain_a_snoop_response();

    // 5. Domain B snoop probe 处理
    if (snoop_forwarding_enabled_) {
        handle_domain_b_snoop_probe();
    }

    // 6. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 handle_domain_a_request（请求 A→B）

```cpp
void CoherenceBridge::handle_domain_a_request() {
    if (!domain_a_req_in_.valid() || !domain_a_req_in_.ready()) return;

    const auto& req = domain_a_req_in_.data();

    // 1. 协议转换（protocol_a_ → protocol_b_）
    CacheReqBundle translated = translate_request(req, protocol_a_.get(), protocol_b_.get());
    ++cross_domain_protocol_translations_;

    // 2. 地址映射（域 A → 域 B）
    uint64_t mapped_addr = addr_map_a_to_b_(req.address.read());
    translated.address.write(mapped_addr);
    ++address_mappings_;

    // 3. 延迟入队
    uint64_t now = current_cycle();
    uint64_t departure = now + bridge_latency_;

    // 4. 转发到 Domain B
    domain_b_req_out_.write(translated);
    domain_a_req_in_.consume();

    cross_domain_requests_++;
}
```

### 4.3 handle_domain_b_snoop_probe（snoop 跨域）

```cpp
void CoherenceBridge::handle_domain_b_snoop_probe() {
    if (!domain_b_snoop_in_.valid() || !domain_b_snoop_in_.ready()) return;

    const auto& probe = domain_b_snoop_in_.data();

    // 1. 协议转换（protocol_b_ → protocol_a_）
    SnoopProbe translated = translate_snoop(probe, protocol_b_.get(), protocol_a_.get());

    // 2. 地址映射
    uint64_t mapped_addr = addr_map_b_to_a_(probe.addr);
    translated.addr = mapped_addr;

    // 3. 延迟入队
    uint64_t now = current_cycle();
    uint64_t departure = now + bridge_latency_;
    snoop_inflight_[probe.transaction_id] = {
        now, departure,
        protocol_b_->get_protocol_name(),
        protocol_a_->get_protocol_name()
    };

    // 4. 转发到 Domain A
    domain_a_snoop_out_.write(translated);
    domain_b_snoop_in_.consume();

    ++cross_domain_snoop_forwards_;
}
```

### 4.4 关键设计取舍

- **同步协议转换**：v0 简化（无状态保存），每个 probe 独立翻译
- **地址映射函数化**：用户传入 std::function（v0 简化，支持任意映射）
- **snoop 转发可选**：v0 简化（默认开启，配置可关闭）
- **延迟与 interconnect-bridge 协同**：v0 简化（每个 Bridge 独立延迟，未来可串联）

## 5. Bundle 字段使用

| 字段 | CoherenceBridge 使用 |
|------|---------------|
| `transaction_id` | **关键**——snoop_inflight_ 映射键 + 跨域追踪 |
| `address` | **关键**——地址映射（a_to_b / b_to_a） |
| `data` | 透传（协议转换不修改 data） |
| `is_write` | 透传（snoop type 决策不依赖） |
| `kernel_id` | 透传（跨域 kernel 追踪） |
| `coherence_msg` | **关键**——协议转换输入 |

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/bridge.hh` Bridge | `tlm::CoherenceBridge` | 特化：增加协议转换层 |
| `Bridge::delay` | `bridge_latency_` | 同语义 |
| `src/mem/port.hh` MasterPort/SlavePort | `domain_a_*` / `domain_b_*` | 同名沿用 |
| `ProtocolBridge` (Phase 5 备选) | (v0 CoherenceBridge 含协议转换) | CoherenceBridge = Bridge + ProtocolBridge 合并 |
| `Bridge::recvTimingReq` | `handle_domain_a_request` | 同语义 |
| `Bridge::recvTimingSnoopReq` | `handle_domain_b_snoop_probe` | 同语义 |
| `gem5::SnoopFilter` (跨域) | 跨域 snoop 简化为 broadcast | 简化 |

## 7. 实施路径

### 7.1 Phase 7.D 步骤

1. 新建 `include/core/coherence_bridge.hh`（~350 行）
2. 实现 8 端口（4 个 A 侧 + 4 个 B 侧）
3. 实现 `translate_request` / `translate_snoop`
4. 实现 `set_address_mapper`（std::function 注入）
5. 实现 `forward_snoop_to_b` / `forward_snoop_resp_to_a`
6. 与 `CoherenceProtocol` 集成（coherence-protocol.md）
7. 加 Catch2 测试：`test/test_coherence_bridge.cc`
8. 新增 `configs/apu_with_dgpu.json`（APU 域 + dGPU 域通过 CoherenceBridge 互连）

### 7.2 Phase 7 备选 dGPU 步骤

1. 实现标准 Host↔VRAM 地址映射（默认 std::function）
2. 实现 MOESI_AMD ↔ MESI_GPU 协议转换
3. 集成到 `cpptlm` Python 库拓扑 DSL

### 7.3 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[coherence_bridge]"` 全部通过
- [ ] 端到端 2 域 + 协议转换运行
- [ ] 跨域 snoop 真实转发
- [ ] 地址映射正确性

### 7.4 估计工作量

- 设计: 1 周
- Phase 7.D 基础版（同协议桥接）: 2 周
- Phase 7 备选 dGPU（跨协议 + 地址映射）: 2-3 周
- 测试: 1 周
- **总计: 6-7 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **协议转换死锁**——A 域等 B 域 response，B 域等 A 域 snoop | 中 | 高 | snoop_inflight_ 超时（默认 1000 cycle） |
| R2 | **地址映射不一致**——a_to_b / b_to_a 函数不对称 | 中 | 高 | 单元测试验证：a_to_b(b_to_a(addr)) == addr |
| R3 | **跨域 snoop 风暴**——dGPU 频繁触发 APU snoop | 中 | 中 | 依赖 SnoopFilter（snoop_filter.md） |
| R4 | **延迟叠加失控**——多 Bridge 串联延迟过大 | 中 | 中 | 暴露 `set_max_total_latency()` 配置 |
| R5 | **协议转换语义丢失**——MOESI 的 O/T 态在 MESI 中无法表达 | 中 | 中 | 文档明确：MESI_GPU 模拟 O 为 M |
| R6 | **transaction_id 跨域冲突**——两域独立 ID 空间 | 中 | 中 | 高位拼接（如 A_ID 32bit + B_ID 32bit） |
| R7 | **配置复杂度**——用户需理解 8 端口 + 2 协议 + 2 映射 | 中 | 中 | 文档 + 示例 + Python 封装 |
| R8 | **dGPU 断电/热插拔**——bridge 失联 | 低 | 中 | 暴露 `set_link_status()` 模拟断链 |

## 9. 决策点

### D1 默认协议对

- **Q**: 默认 MoesiAmdProtocol + MesiGpuProtocol（同协议还是跨协议）？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 同协议（v0 简化，MOESI_AMD + MOESI_AMD）
- **依赖**: 用户配置

### D2 默认地址映射

- **Q**: 默认地址映射函数是什么？
- **状态**: 留待 Phase 7 备选 dGPU 设计时确定
- **建议**: identity（v0 简化，同地址空间）
- **依赖**: 地址空间拓扑

### D3 snoop 转发开关

- **Q**: snoop 转发默认开启还是关闭？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 默认开（与 gem5 Bridge 一致）
- **依赖**: SnoopFilter 集成

### D4 跨域 transaction_id 拼接

- **Q**: 跨域 txn_id 如何避免冲突？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 64-bit ID 拼接（A_ID 32bit + B_ID 32bit）
- **依赖**: 跨域 ID 空间设计

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.6）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7.D (未来)**: 基础版实施（同协议桥接）
- **Phase 7 备选 dGPU (未来)**: 跨协议桥接 + 地址映射
