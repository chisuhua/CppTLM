# interconnect-crossbar 微架构文档

> **类别**: Interconnect > Crossbar
> **状态**: ✅ 已实施
> **Header**: `include/tlm/crossbar_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:32`）
> **蓝图来源**: gem5 `src/mem/xbar.hh` + `src/mem/noncoherent_xbar.hh`（非一致 crossbar）
> **首版 commit**: v2.1 路径同步 · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4

---

## 1. 设计目标

`CrossbarTLM` 是 **4 端口地址路由 crossbar**，用于连接多个 CPU/Cache 端到多个 Memory 端。**与 gem5 对位**: `NonCoherentXBar`（v0 简化版，无 snoop 广播）。

**核心特性**（来自 `crossbar_tlm.hh:28-138`）：
- 4 请求端口 + 4 响应端口
- 地址位提取路由：`dst = (addr >> 12) & 0x3`
- 单周期冲突检测（同一目标端口仅 1 个 flit 转发）
- v0 简化：所有请求都"成功"（无 back-pressure 反馈）

## 2. 架构概览

### 2.1 端口布局

```
  req_in[0]   ─►┐
  req_in[1]   ─►┤
  req_in[2]   ─►┤──► [CrossbarTLM] ──► resp_out[dst]
  req_in[3]   ─►┘            │
                              ▼
                       port_busy_[0..3]
```

### 2.2 地址路由表

| 端口 | 地址范围 |
|------|----------|
| 0 | 0x0000-0x0FFF |
| 1 | 0x1000-0x1FFF |
| 2 | 0x2000-0x2FFF |
| 3 | 0x3000-0x3FFF |

路由公式：`route_address(addr) = (addr >> PORT_SHIFT) & PORT_MASK`，`PORT_SHIFT=12`, `PORT_MASK=0x3`。

## 3. 接口（Public API）

```cpp
class CrossbarTLM : public ChStreamModuleBase {
public:
    // 请求方向端口（public 以便 StreamAdapter 访问）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out[NUM_PORTS];

    CrossbarTLM(const std::string& name, EventQueue* eq);
    unsigned route_address(uint64_t addr) const;

    void set_stream_adapter(cpptlm::StreamAdapterBase*) override {}
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;

    void tick() override;
    void do_reset(const ResetConfig&) override;
    unsigned num_ports() const override { return NUM_PORTS; }
};
```

**常量**：
| 常量 | 值 | 含义 |
|------|----|------|
| `NUM_PORTS` | 4 | 端口数（不可配置） |
| `PORT_SHIFT` | 12 | 地址右移位数 |
| `PORT_MASK` | 0x3 | 端口掩码 |

## 4. 行为流程

### 4.1 tick() 主循环

```cpp
void CrossbarTLM::tick() {
    bool conflicted = false;
    unsigned conflict_dst = NUM_PORTS;

    for (unsigned i = 0; i < NUM_PORTS; i++) {
        port_busy_[i] = false;
    }

    for (unsigned i = 0; i < NUM_PORTS; i++) {
        if (req_in[i].valid() && req_in[i].ready()) {
            const auto& req = req_in[i].data();
            unsigned dst = route_address(req.address.read());

            if (port_busy_[dst]) {
                conflicted = true;
                conflict_dst = dst;
            } else {
                port_busy_[dst] = true;
            }

            bundles::CacheRespBundle resp;
            resp.transaction_id.write(req.transaction_id.read());
            resp.data.write(req.data.read());
            resp.is_hit.write(1);
            resp.error_code.write(0);
            resp_out[dst].write(resp);

            ++stats_flits_sent_;
            stats_flit_latency_.sample(3);
            req_in[i].consume();
        }
    }
    for (unsigned i = 0; i < NUM_PORTS; i++) {
        if (adapter[i]) adapter[i]->tick();
    }
}
```

### 4.2 关键设计取舍

- **响应简化为透传**：`is_hit=1`, `data=req.data`（**不是从 memory 真读**——v0 仅验证 crossbar 路由）
- **冲突仅记录**：`conflicted` 标志在 v0 实际**未使用**（仅赋值）——单周期冲突时第 2 个 flit 仍可写 `resp_out[dst]`（**v0 bug 风险**：v2.2 应丢弃冲突 flit）
- **flit_latency 恒为 3**（硬编码）

## 5. Bundle 字段使用

| 字段 | CrossbarTLM 使用 |
|------|---------------|
| `transaction_id` | 透传到 resp（保持 ID） |
| `address` | **关键**——`route_address()` 决策 |
| `data` | 透传到 resp（v0 简化） |
| 其他 | 忽略 |

## 6. 统计

| 指标 | 类型 | 含义 |
|------|------|------|
| `stats_flits_received_` | Scalar | **未在 v0 tick 中递增**（接收计数缺失，仅 sent 计数） |
| `stats_flits_sent_` | Scalar | 已转发的 flit 数 |
| `stats_flit_latency_` | Distribution | 转发延迟（v0 恒为 3 cycle） |
| `stats_buffer_occupancy_` | Average | 缓冲区占用率（v0 需补采样） |

**路径**: `system.crossbar`

## 7. 蓝图（未来演进）

### 7.1 Phase 7.C 应用

调研 §2.4：`CoherentXBar`（含 snoop 广播）—— v0 `NonCoherentXBar` 升级路径：
- 增加 `SnoopFilter` 子模块
- snoop request 泛洪到同域其他 cache
- 接收 snoop response 维护 cache 一致性

### 7.2 蓝图增强

- **真实冲突处理**：v0 冲突 flit 仍被写（bug），应丢弃或 back-pressure
- **可配端口数**：`NUM_PORTS` 改为模板参数（与 `ArbiterTLM<N>` 一致）
- **可配地址路由**：JSON 中 `routing_mask` + `routing_shift` 字段
- **bridge 子模块**：`BridgeTLM`（延迟+带宽限流）
- **CommMonitorTLM**（流量监控）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **v0 冲突处理 bug**——冲突 flit 仍被转发 | 高 | 中 | v2.2 修复：冲突时 `req_in[i].consume()` 后丢弃（不写 `resp_out[dst]`） |
| R2 | 4 端口硬编码 | 中 | 中 | 模板化（与 `ArbiterTLM<N>` 对齐） |
| R3 | 路由公式硬编码（`>>12 & 0x3`） | 中 | 低 | JSON 参数化（`routing_mask`） |
| R4 | `stats_flits_received_` 未在 tick 中递增（**实现 bug**） | 高 | 低 | v2.2 修复：在 `if (req_in[i].valid())` 块内 `++stats_flits_received_` |
| R5 | **无 back-pressure 反馈**——v0 总是 ready | 高 | 中 | v2.2 加 `buffer_occupancy_` 阈值，超过则 `ready()=false` |
| R6 | 协议不可知（无 snoop 广播） | 高 | 中 | Phase 7.C `CoherentXBarTLM` 蓝图 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ✅ | `test/crossbar_test.json` + `[crossbar]` 标签测试 |
| 4 端口地址路由 | ✅ | `(addr >> 12) & 0x3` 公式 |
| 冲突检测 | ⚠️ 标记但未生效 | 需 v2.2 修复（见 R1） |
| 端到端 (CPU→Cache→Xbar→Memory) | ✅ | `configs/single_cluster_soc.json` |
| Stats 输出 | ⚠️ 1 项未采样 | 见 R4 |

## 10. 修订历史

- **2026-04-13**: CrossbarTLM 初版
- **2026-04-15**: `MultiPortStreamAdapter` 集成
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B1 批次）
