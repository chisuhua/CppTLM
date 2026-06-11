# coherent_xbar 微架构文档

> **类别**: Interconnect > CoherentXBar
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/interconnect/coherent_xbar_tlm.hh`
> **蓝图来源**: gem5 `src/mem/coherent_xbar.hh`（含 snoop 广播 crossbar）
> **首版 commit**: 蓝图（来自调研 §2.4 + Phase 7.C）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4
> - 邻接: [interconnect-crossbar.md](./interconnect-crossbar.md) (v0 NonCoherentXBar) | [snoop_filter.md](./snoop_filter.md) (Phase 7.C 联动)

---

## 1. 设计目标（蓝图）

`tlm::CoherentXBarTLM` 是 CppTLM Phase 7.C 规划的 **含 snoop 广播的 crossbar**——在 v0 `CrossbarTLM`（无 snoop）基础上升级为 CoherentXBar 协议。**与 gem5 对位**: `gem5::CoherentXBar`（~800 行，snoop fanout + 跨域）。

**核心特征**：
- **继承 v0 CrossbarTLM 4 端口架构**（避免重新发明）
- **snoop 广播**（同一 coherence 域内所有 cache 收到 snoop request）
- **snoop response 收集**（acks/invalidation 确认）
- **SnoopFilter 集成**（减少冗余 snoop 流量）
- **可选跨域转发**（与 `CoherenceDomain::register_bridge()` 联动）

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│                CoherentXBarTLM 单体                          │
│                                                             │
│  ┌──────────────┐         ┌──────────────┐                  │
│  │  CPU port 0  │         │  CPU port 3  │                  │
│  │  Req/Resp    │         │  Req/Resp    │                  │
│  └──────┬───────┘         └──────┬───────┘                  │
│         │                        │                          │
│         ▼                        ▼                          │
│  ┌──────────────────────────────────────────────────┐     │
│  │  address_route_ (4 → N routing)                 │     │
│  │    - 路由到 memory port (forward path)           │     │
│  │    - 路由到 snoop targets (snoop path)           │     │
│  └──────────────────────────────────────────────────┘     │
│         │                        │                          │
│         ▼                        ▼                          │
│  ┌──────────────────────────────────────────────────┐     │
│  │  snoop_broadcastor_                              │     │
│  │    - get_snoop_targets(addr)  ← SnoopFilter     │     │
│  │    - 发 snoop request 到所有 targets             │     │
│  │    - 收 snoop response 维护 cache line 一致性    │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                   │
│         ▼                                                   │
│  ┌──────────────┐         ┌──────────────┐                  │
│  │  Memory port │         │  GPU TCC port│                  │
│  │  (forward)   │         │  (forward)   │                  │
│  └──────────────┘         └──────────────┘                  │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 与 v0 CrossbarTLM 的关系

| 维度 | v0 CrossbarTLM | CoherentXBarTLM (Phase 7.C) |
|------|----------------|------------------------------|
| **snoop 广播** | ❌ 无 | ✅ 有 |
| **SnoopFilter** | ❌ 无 | ✅ 集成 |
| **跨域** | ❌ 单域 | ✅ 跨 CoherenceDomain 桥接 |
| **端口数** | 4 CPU 端口 | 4 CPU + N memory 端口（可配） |
| **协议** | 透传 Bundle | 透传 Bundle + snoop 协议 |

### 2.2 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|------|------|
| `cpu_req_in_[N]` | `InputStreamAdapter<CacheReqBundle>` | N (默认 4) | 接收 CPU/L1 上行请求 |
| `cpu_resp_out_[N]` | `OutputStreamAdapter<CacheRespBundle>` | N | 响应 CPU/L1 |
| `mem_req_out_[M]` | `OutputStreamAdapter<CacheReqBundle>` | M (默认 2: DRAM + GPU TCC) | 转发到 Memory/TCC |
| `mem_resp_in_[M]` | `InputStreamAdapter<CacheRespBundle>` | M | 接收 Memory/TCC 响应 |
| `snoop_out_[K]` | `OutputStreamAdapter<SnoopProbe>` | K (per cache) | 广播 snoop 到 cache |
| `snoop_in_[K]` | `InputStreamAdapter<SnoopResp>` | K | 收 snoop response |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  CoherentXBarTLM 内部                        │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  v0 CrossbarTLM 基类（继承）                       │     │
│  │    - address_route_, port_busy_                  │     │
│  │    - route_address(addr)                         │     │
│  └──────────────────────────────────────────────────┘     │
│                          ↑ 继承                              │
│  ┌──────────────────────────────────────────────────┐     │
│  │  CoherentXBar 特有                                │     │
│  │    - snoop_targets_: addr → set<port_id>         │     │
│  │    - snoop_filter_: shared_ptr<SnoopFilterTLM>   │     │
│  │    - snoop_inflight_: map<txn_id, snoop_state>   │     │
│  │    - cross_domain_bridge_: optional<BridgeTLM>   │     │
│  └──────────────────────────────────────────────────┘     │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
template <uint32_t NUM_CPU_PORTS = 4, uint32_t NUM_MEM_PORTS = 2>
class CoherentXBarTLM : public CrossbarTLM {
public:
    explicit CoherentXBarTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "CoherentXBarTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_snoop_filter(std::shared_ptr<SnoopFilterTLM> filter) {
        snoop_filter_ = filter;
    }
    void set_coherence_domain(CoherenceDomain* dom) { domain_ = dom; }
    void set_cross_domain_bridge(BridgeTLM* bridge) {
        cross_domain_bridge_ = bridge;
    }

    // === CPU 端口访问器（继承自 CrossbarTLM） ===
    // req_in[i], resp_out[i] (i in [0, NUM_CPU_PORTS))

    // === Memory 端口访问器 ===
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& mem_req_out(uint32_t idx);
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& mem_resp_in(uint32_t idx);

    // === Snoop 端口访问器 ===
    cpptlm::OutputStreamAdapter<bundles::SnoopProbe>& snoop_out(uint32_t cache_id);
    cpptlm::InputStreamAdapter<bundles::SnoopResp>& snoop_in(uint32_t cache_id);

    // === Snoop 处理 ===
    void broadcast_snoop(uint64_t addr, SnoopType type);  // INV / DOWNGRADE / SHARED
    void handle_snoop_response(uint64_t addr, SnoopResp resp);

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    // 继承自 CrossbarTLM: address_route_, route_address()
    std::shared_ptr<SnoopFilterTLM> snoop_filter_;
    CoherenceDomain* domain_;
    BridgeTLM* cross_domain_bridge_;  // 跨域桥接

    std::map<uint64_t, std::set<uint32_t>> snoop_targets_;  // addr → cache_ids
    std::map<uint64_t, SnoopInflightState> snoop_inflight_;  // txn_id → snoop 状态

    // 统计
    tlm_stats::Scalar snoop_requests_sent_;
    tlm_stats::Scalar snoop_responses_received_;
    tlm_stats::Scalar snoop_filtered_;  // 被 SnoopFilter 减负的数量
    tlm_stats::Scalar cross_domain_forwards_;
    tlm_stats::Distribution snoop_completion_latency_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 6 阶段

```cpp
void CoherentXBarTLM::tick() {
    // 1. Memory 侧响应消费
    for (uint32_t m = 0; m < NUM_MEM_PORTS; ++m) {
        if (mem_resp_in(m).valid() && mem_resp_in(m).ready()) {
            const auto& resp = mem_resp_in(m).data();
            uint32_t cpu_port = extract_cpu_port_from_resp(resp);
            cpu_resp_out(cpu_port).write(resp);
            mem_resp_in(m).consume();
        }
    }

    // 2. CPU 请求处理（继承 v0 路由 + snoop 广播）
    for (uint32_t i = 0; i < NUM_CPU_PORTS; ++i) {
        if (cpu_req_in(i).valid() && cpu_req_in(i).ready()) {
            const auto& req = cpu_req_in(i).data();
            handle_coherent_request(i, req);
            cpu_req_in(i).consume();
        }
    }

    // 3. Snoop response 收集
    for (uint32_t k = 0; k < num_caches_; ++k) {
        if (snoop_in(k).valid() && snoop_in(k).ready()) {
            const auto& sresp = snoop_in(k).data();
            handle_snoop_response(sresp.addr, sresp.resp);
            snoop_in(k).consume();
        }
    }

    // 4. 跨域转发（如有）
    if (cross_domain_bridge_ && has_cross_domain_pending_) {
        forward_to_cross_domain();
    }

    // 5. snoop inflight 推进（v0 简化：不阻塞）
    advance_snoop_inflight();

    // 6. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 handle_coherent_request

```cpp
void CoherentXBarTLM::handle_coherent_request(uint32_t cpu_port,
                                              const CacheReqBundle& req) {
    uint64_t addr = req.address.read();

    // 1. 路由到 memory port（继承 v0 逻辑）
    uint32_t mem_port = route_to_mem_port(addr);
    mem_req_out(mem_port).write(req);

    // 2. snoop 广播（CoherentXBar 特有）
    if (is_snoop_required(req)) {  // write / upgrade / read-shared
        std::set<uint32_t> targets;
        if (snoop_filter_) {
            targets = snoop_filter_->get_snoop_targets(addr);
        } else {
            targets = get_all_caches_in_domain();
        }

        for (uint32_t cache_id : targets) {
            if (cache_id == cpu_port) continue;  // 跳过自己
            SnoopProbe probe{addr, req.transaction_id.read(),
                             derive_snoop_type(req)};
            snoop_out(cache_id).write(probe);
            ++snoop_requests_sent_;
        }

        snoop_inflight_[req.transaction_id.read()] = {
            addr, targets, current_cycle()
        };
    }
}
```

### 4.3 关键设计取舍

- **继承 v0 路由**：避免重新发明 4 端口 + 地址位提取逻辑
- **SnoopFilter 可选**：v0 默认无 SnoopFilter，Phase 7.C 中期集成
- **跨域桥接 v0 留接口**：v0 单域实现，Phase 7.D+ 启用跨域
- **snoop 协议简化**：3 种 snoop type（INV / DOWNGRADE / SHARED），覆盖 90% 用例

## 5. Bundle 字段使用（规划）

| 字段 | CoherentXBarTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——snoop_inflight_ 映射键 |
| `address` | **关键**——snoop targets 查找 + memory 路由 |
| `is_write` | **关键**——决定是否需要 snoop（写必 snoop，读可选） |
| `coherence_msg` | 透传（Phase 7.C+ 显式 coherence 消息） |
| `data` | 透传 |
| `kernel_id` | 透传（Phase 7.D 跨域一致性追踪） |

## 6. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/mem/coherent_xbar.hh` CoherentXBar | `tlm::CoherentXBarTLM` | 模板化 NUM_CPU_PORTS/NUM_MEM_PORTS |
| `CoherentXBar::snoopSend` | `broadcast_snoop` | 同语义 |
| `CoherentXBar::snoopFilter` | `snoop_filter_` (shared_ptr) | 共享同一 SnoopFilter 实例 |
| `CoherentXBar::getPort` | `snoop_out/in` 模板化访问器 | 模板化 |
| `src/mem/snoop_filter.hh` SnoopFilter | `tlm::SnoopFilterTLM` | 见 snoop_filter.md |
| `CoherentXBar::recvTimingSnoopReq` | `handle_snoop_response` | 同语义 |

## 7. 实施路径

### 7.1 Phase 7.C 步骤

1. 新建 `include/tlm/interconnect/coherent_xbar_tlm.hh`（~300 行）
2. 继承 `CrossbarTLM`（v0 路由 + port_busy 复用）
3. 实现 snoop_broadcastor_ + 6 端口（4 CPU + 2 mem + K snoop）
4. 实现 `broadcast_snoop` + `handle_snoop_response`
5. 加 SnoopFilter 集成（可选 shared_ptr）
6. 加跨域桥接接口（v0 留空）
7. 加 Catch2 测试：`test/test_coherent_xbar.cc`
8. `chstream_register.hh` 注册

### 7.2 Phase 7.D 步骤（跨域）

1. 实现 `forward_to_cross_domain()` 真实逻辑
2. 与 `CoherenceDomain::register_bridge()` 集成
3. 协议转换占位（v0 透传）

### 7.3 验收标准

- [ ] 编译通过
- [ ] `cpptlm_tests "[coherent_xbar]"` 全部通过
- [ ] 4 CPU + 2 mem 端到端运行
- [ ] snoop 广播真实生效（cache line 状态正确转换）
- [ ] SnoopFilter 减负生效（filtered 计数 > 0）
- [ ] 跨域 snoop 转发（Phase 7.D）

### 7.4 估计工作量

- 设计: 1 周
- 基础版实施（无 SnoopFilter/跨域）: 2 周
- SnoopFilter 集成: 1 周
- 跨域桥接: 1 周
- 测试: 1 周
- **总计: 6 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **snoop 风暴**——多 cache 写同一地址 | 高 | 高 | 强制 SnoopFilter 集成（v0 简化：可关闭但默认开） |
| R2 | **snoop 死锁**——snoop response 永远收不齐 | 中 | 高 | snoop_inflight_ 加超时（默认 1000 cycle）；超时强制完成 |
| R3 | **跨域 snoop 死锁**——dGPU 不响应 | 中 | 高 | BridgeTLM timeout + 强制 invalidate |
| R4 | **NUM_CPU_PORTS 模板膨胀**——4/8/16 各自一份代码 | 中 | 低 | 显式特化 [4, 8, 16] |
| R5 | **SnoopFilter 误过滤**——filter 说无 target 但实际有 | 中 | 中 | filter 输出 `optional<set>`；空时 broadcast 全域 |
| R6 | **v0 CrossbarTLM 改造不彻底**——继承后路由 bug | 中 | 中 | 完整跑 v0 CrossbarTLM 回归测试 |
| R7 | **顺序保证**——同一地址 snoop 必须按序 | 中 | 中 | per-addr snoop queue（v0 简化：单 in-flight 即可） |
| R8 | **大延迟场景内存膨胀**——snoop_inflight_ 累积 | 中 | 中 | `MAX_SNOOP_INFLIGHT = 1024` 限制 |

## 9. 设计决策点

### D1 继承 v0 CrossbarTLM

- **Q**: 继承 v0 还是重写？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 继承 v0（节省 200 行地址路由代码）
- **依赖**: v0 CrossbarTLM 当前状态

### D2 SnoopFilter 默认开/关

- **Q**: 默认启用 SnoopFilter？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 默认开（避免 snoop 风暴）
- **依赖**: SnoopFilterTLM 实施

### D3 跨域桥接触发条件

- **Q**: 何时启用 cross_domain_bridge_？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 配置 `cross_domain=true` 触发（默认 false）
- **依赖**: `CoherenceDomain::register_bridge()` API

### D4 snoop 协议消息类型

- **Q**: SnoopProbe 用什么消息类型？
- **状态**: 留待 Phase 7.C 设计时确定
- **建议**: 3 种基础类型（INVALIDATE / DOWNGRADE_TO_S / READ_SHARED），覆盖 90%
- **依赖**: gem5 `SnoopReqType` 枚举

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §2.4）
- **2026-06-12**: B3 批次设计 — 提取 D1-D4 + 蓝图对齐 + 风险列表
- **Phase 7.C (未来)**: 基础版实施（单域 + snoop 广播）
- **Phase 7.D (未来)**: SnoopFilter 集成 + 跨域桥接
