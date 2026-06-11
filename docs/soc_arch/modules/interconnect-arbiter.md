# interconnect-arbiter 微架构文档

> **类别**: Interconnect > Arbiter
> **状态**: ✅ 已实施
> **Header**: `include/tlm/arbiter_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:35-36`，特化 N=2,4）
> **蓝图来源**: gem5 `src/mem/port_arbiter.hh`
> **首版 commit**: v2.1 路径同步
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4

---

## 1. 设计目标

`ArbiterTLM<N>` 是 **N 端口输入 + 1 端口输出的 Round-Robin 仲裁器**，通过 `txn_to_port_` 映射表自动把响应路由回源端口。**与 gem5 对位**: `gem5::PortArbiter`（简化版）。

**核心特性**（来自 `arbiter_tlm.hh:64-151`）：
- 模板化 N_PORTS（编译期确定）
- N 入 + 1 出（请求方向）
- N 出 + 1 入（响应方向）
- Round-Robin 仲裁（`last_served_`）
- 事务追踪（`txn_to_port_[txn_id] = src_port`）

## 2. 架构概览

```
  req_in[0]   ─►┐
  req_in[1]   ─►┤   ArbiterTLM<N>       req_out   ──► (下游模块)
  ...          ─►├──────────────►
  req_in[N-1] ─►┘      │  Round-Robin
                        ▼
                  req_queue_
                        │
                        ▼
                  txn_to_port_  (txn_id → src_port)

  resp_in    ◄── (下游响应) ──► resp_out[src_port(txn_id)]
```

## 3. 接口（Public API）

```cpp
template<unsigned N_PORTS>
class ArbiterTLM : public ChStreamModuleBase {
public:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>   req_in[N_PORTS];
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out[N_PORTS];

    explicit ArbiterTLM(const std::string& name, EventQueue* eq);
    unsigned num_ports() const override { return N_PORTS; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;

    void tick() override;
    void do_reset(const ResetConfig& config) override;

    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out();
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in();
};
```

**注册实例**（`chstream_register.hh:35-36`）：
- `ArbiterTLM2` = `template=2`
- `ArbiterTLM4` = `template=4`

## 4. 行为流程

### 4.1 tick() 三阶段

```cpp
void ArbiterTLM::tick() {
    // Phase 1: 接收请求（所有端口）
    for (unsigned i = 0; i < N_PORTS; i++) {
        if (req_in[i].valid() && req_in[i].ready()) {
            QueuedReq qr;
            qr.bundle = req_in[i].data();
            qr.src_port = i;
            req_queue_.push(qr);
            txn_to_port_[qr.bundle.transaction_id.read()] = i;
            req_in[i].consume();
        }
    }

    // Phase 2: Round-Robin 仲裁 + 发送
    if (!req_queue_.empty() && req_out_.valid() == false) {
        auto& req = req_queue_.front().bundle;
        req_out_.write(req);
        req_queue_.pop();
        last_served_ = (last_served_ + 1) % N_PORTS;
    }

    // Phase 3: 响应路由（txn_id → src_port）
    if (resp_in_.valid() && resp_in_.ready()) {
        auto& resp = resp_in_.data();
        uint64_t txn_id = resp.transaction_id.read();
        auto it = txn_to_port_.find(txn_id);
        if (it != txn_to_port_.end()) {
            resp_out[it->second].write(resp);
            txn_to_port_.erase(it);
        }
        resp_in_.consume();
    }

    // 适配器 tick
    for (unsigned i = 0; i < N_PORTS; i++) {
        if (adapters_[i]) adapters_[i]->tick();
    }
    if (single_adapter_) single_adapter_->tick();
}
```

### 4.2 关键设计取舍

- **Round-Robin 简化**：`last_served_` 仅在 `req_queue_` 出队时 `+1`，**未真正"跳过空队列"**——v0 是"按入队顺序轮转出队"而非"轮询端口"
- **`txn_to_port_` 自动清理**：响应到达时 `erase(it)`，无内存泄漏
- **响应无 buffer**：直接写 `resp_out[port]`，**依赖下游 ready 信号**——若下游 `resp_out[i].ready()=false` 则丢失响应（v0 简化）

## 5. Bundle 字段使用

| 字段 | ArbiterTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——`txn_to_port_` 映射键 |
| 其他 | 透传（v0 不修改） |

## 6. 统计

**无 StatGroup**——ArbiterTLM 是纯路由模块，**未挂载任何统计**（与 `CPUTLM` 类似）。

## 7. 蓝图（未来演进）

- **真 Round-Robin**（按端口轮询而非按队列顺序）
- **优先级仲裁**（gem5 `SimplePriority`）
- **iSLIP 调度**（k-ary 划分）
- **响应队列**（避免 `resp_out[port].ready()=false` 时丢响应）
- **统计**：平均延迟 / 队列长度 / 端口利用率 / 公平性指标

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **响应丢失**（下游 not ready 时直接覆盖） | 高 | 中 | v2.2 加响应 FIFO 缓冲 |
| R2 | Round-Robin 简化（按队列顺序而非按端口轮询） | 中 | 中 | v2.2 真按端口优先级轮询 |
| R3 | **无 back-pressure**——`req_out_.valid()=false` 时不写入，但 `req_queue_` 无限增长 | 中 | 中 | v2.2 加 `max_queue_size_` + 丢弃策略 |
| R4 | 模板参数 N 编译期确定，**JSON 不能动态改变 N** | 中 | 低 | 通过多个注册名（`ArbiterTLM2` / `ArbiterTLM4` / `ArbiterTLM8`）覆盖常用 N 值 |
| R5 | 无 StatGroup | 中 | 中 | 加 `req_queue_size_` / `resp_latency_` / `port_utilization_` |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ✅ | `configs/arbiter_tlm_test.json` + `[arbiter]` 标签 |
| N=2, N=4 模板特化 | ✅ | `ArbiterTLM2` / `ArbiterTLM4` 已注册 |
| Round-Robin 仲裁 | ⚠️ 简化 | 见 R2 |
| 事务追踪 | ✅ | `txn_to_port_` 自动维护 |
| 响应回源端口 | ✅ | 真实代码已实现 |
| 统计 | ❌ 无 | 见 R5 |

## 10. 修订历史

- **2024-05**: ArbiterTLM 初版（N_PORTS 模板）
- **2026-04**: `MultiPortStreamAdapter` 集成
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B1 批次）
