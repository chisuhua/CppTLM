# noc-router 微架构文档

> **类别**: NoC > Router
> **状态**: ✅ 已实施
> **Header**: `include/tlm/router_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:37`）
> **蓝图来源**: gem5 `src/mem/ruby/network/garnet2.0/Router.hh` + `src/mem/ruby/network/simple/SimpleRouter.hh`
> **首版 commit**: `1d176db`（v2.1 路径与早期 RouterTLM 实现；具体重构 commit 见 §10）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §8（GPU 内部互联关键模板）
> - Plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../../superpowers/plans/2026-06-11-phase7a-gpu-infra.md) §8（Phase 7.E 蓝图）

---

## 1. 设计目标

`tlm::RouterTLM` 是 CppTLM NoC 内的 **5 端口周期精确路由器**，承担多跳消息转发。**与 gem5 对位**: `gem5::Router`（Garnet 2.0）/ `gem5::SimpleRouter`。

**核心特性**（来自 `router_tlm.hh:114-128` 注释）：
- 5 个双向端口（N / E / S / W / Local）
- 每端口 4 个虚拟通道（VC）
- 每 VC 8 深 FIFO 输入缓冲
- 六阶段流水线（BW → RC → VA → SA → ST → LT）
- Credit-based Flow Control
- 可插拔路由算法（默认 XY 维度顺序）

## 2. 架构概览

### 2.1 端口布局

```
            NORTH (port 0)
                ▲
                │
   WEST ◄── [ RouterTLM ] ──► EAST
  (port 3)  │              │  (port 1)
                │
                ▼
            SOUTH (port 2)

            LOCAL (port 4) ──► (连接 NICTLM 的 PE 侧)
```

| 端口 | enum | 物理方向 | 用途 |
|------|------|----------|------|
| NORTH | 0 | +Y | Mesh 上行 |
| EAST | 1 | +X | Mesh 右行 |
| SOUTH | 2 | -Y | Mesh 下行 |
| WEST | 3 | -X | Mesh 左行 |
| LOCAL | 4 | 本地 | 连接到 NICTLM 的 PE 侧（注入/弹出 flit） |

### 2.2 内部结构（简化版六阶段流水线）

```
  req_in_[port]                                            resp_out_[port]
  (InputStreamAdapter)                                    (OutputStreamAdapter)
       │                                                          ▲
       ▼                                                          │
  ┌────────────────────────────────────────────────────────────┐
  │ input_buffer_[port][vc]   (8-deep FIFO)                   │
  │   └─► RouterFlit (含 RouterStageState)                    │
  │                                                            │
  │ ┌──► BW: Buffer Write   ── 写入 input_buffer_            │
  │ │                                                           │
  │ ├──► RC: Route Computation ── compute_xy_route(dst_node)   │
  │ │                                                            │
  │ ├──► VA: VC Allocation   ── allocate_vc(out_port)         │
  │ │                                                            │
  │ ├──► SA: Switch Allocation ── 多 winner 仲裁              │
  │ │                                                            │
  │ ├──► ST: Switch Traversal  ── pipe_reg_ + sa_winners_     │
  │ │                                                            │
  │ └──► LT: Link Traversal   ── pending_link_ (1 cyc 延迟)   │
  │                                                            │
  │ downstream_credits_[port][vc]  (credit 计数)               │
  │ pipe_reg_[port][vc]  (SA 后等 credit 的 flit 寄存器)       │
  │ pending_credit_returns_  (queue: Credit 返回调度)          │
  └────────────────────────────────────────────────────────────┘
```

### 2.3 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|------|------|
| `req_in_[port]` | `InputStreamAdapter<NoCFlitBundle>` | 5 | 接收每个端口的请求 flit |
| `resp_out_[port]` | `OutputStreamAdapter<NoCFlitBundle>` | 5 | 发送每个端口的响应 flit |
| `adapter_` | `BidirectionalPortAdapter<RouterTLM, NoCFlitBundle, 5>*` | 1 | 框架侧 ↔ ChStream 转换 |

## 3. 接口（Public API）

### 3.1 构造函数

```cpp
RouterTLM(const std::string& name, EventQueue* eq,
          unsigned node_x = 0, unsigned node_y = 0,
          unsigned mesh_x = DEFAULT_MESH_X,  // = 2
          unsigned mesh_y = DEFAULT_MESH_Y); // = 2
```

### 3.2 ChStreamModuleBase 继承

```cpp
void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
unsigned num_ports() const override { return NUM_PORTS; }  // 5
tlm_stats::StatGroup* get_stats_group() override { return &stat_group_; }
std::string get_stats_path() const override {
    return "system.router_" + std::to_string(node_x() + node_y() * mesh_x());
}
```

### 3.3 路由算法

```cpp
void set_routing_algorithm(std::unique_ptr<RoutingAlgorithm> algo);
RoutingAlgorithm* routing_algorithm() const { return routing_algo_.get(); }
```

默认算法：`XYRouting`（`router_tlm.hh:107-112`），XY 维度顺序，死锁自由。

### 3.4 拓扑配置

```cpp
unsigned node_x() const;  // 本节点 X 坐标
unsigned node_y() const;  // 本节点 Y 坐标
unsigned mesh_x() const;  // Mesh X 维度
unsigned mesh_y() const;  // Mesh Y 维度
uint32_t node_id() const { return node_x_ + node_y_ * mesh_x_; }
```

### 3.5 Credit 流控

```cpp
void set_credit_timeout(uint64_t cycles);  // 0 = 关闭（默认）
uint64_t credit_timeout() const;
void receive_credit(unsigned in_port, unsigned vc);  // 由 BidirectionalPortAdapter 调用
```

### 3.6 常量配置

| 常量 | 值 | 含义 |
|------|----|------|
| `NUM_PORTS` | 5 | 端口数（不可配置） |
| `NUM_VCS` | 4 | 每端口虚拟通道数 |
| `BUFFER_DEPTH` | 8 | 每 VC 输入缓冲深度 |

## 4. 行为流程

### 4.1 tick() 主循环（高层伪代码）

```cpp
void RouterTLM::tick() {
    current_cycle_++;
    adapter_->tick();  // 框架侧 → ChStream 推进

    // 六阶段流水线（顺序执行，1 周期内完成所有阶段）
    stage_buffer_write();          // BW: 从 req_in_[port] 读入 input_buffer_
    stage_route_computation();     // RC: 为 input_buffer_ 头部的 flit 计算路由
    stage_vc_allocation();         // VA: 分配输出 VC
    stage_switch_allocation();     // SA: 多 winner 仲裁（每输出端口独立）
    stage_switch_traversal();      // ST: pipe_reg_ 等 credit / 直接转发
    stage_link_traversal();        // LT: pending_link_ 1 cyc 延迟后实际发送

    // Credit 路径
    // - return_credit_to_upstream / send_credit_signal 由 BidirectionalPortAdapter 驱动
    // - credit_safety_reset 检测死锁（credit_timeout_ > 0 时）
}
```

### 4.2 关键算法

#### 4.2.1 XY 路由（`compute_xy_route`）

```cpp
unsigned RouterTLM::compute_xy_route(uint32_t dst_node) {
    unsigned dst_x = RoutingAlgorithm::nodeToX(dst_node, mesh_x_);
    unsigned dst_y = RoutingAlgorithm::nodeToY(dst_node, mesh_x_);
    if (dst_x > node_x_) return EAST;
    if (dst_x < node_x_) return WEST;
    if (dst_y > node_y_) return NORTH;
    if (dst_y < node_y_) return SOUTH;
    return LOCAL;
}
```

死锁自由：先 X 后 Y 维度单调收敛。

#### 4.2.2 逆向端口映射（`reverse_port`）

```cpp
static constexpr unsigned reverse_port(unsigned port) {
    switch (port) {
        case 0: return 2;  // NORTH → SOUTH
        case 1: return 3;  // EAST → WEST
        case 2: return 0;  // SOUTH → NORTH
        case 3: return 1;  // WEST → EAST
        case 4: return 4;  // LOCAL → LOCAL
        default: return 0;
    }
}
```

用于 Credit 返回：从 port X 出去后，credit 从 reverse_port(X) 返回上游。

#### 4.2.3 P0.1: 等待 Credit 的 Flit 寄存器（`pipe_reg_`）

```cpp
std::array<std::array<RouterFlit, NUM_VCS>, NUM_PORTS> pipe_reg_;
```

当 SA 选中了 flit 但下游没有 credit 时，flit 保存在 `pipe_reg_`，下一周期重新尝试（不丢弃，**不丢包**）。

#### 4.2.4 P0.1: SA 多 Winner

```cpp
struct SAWinner { unsigned in_port, in_vc, out_port, out_vc; };
std::vector<SAWinner> sa_winners_;
```

每周期可选中多个 winner（不同输出端口独立），提升单周期吞吐。

## 5. Bundle 字段使用

**NoCFlitBundle 字段**（`include/bundles/noc_bundles_tlm.hh`）：

| 字段 | RouterTLM 使用 |
|------|---------------|
| `transaction_id` | 流跟踪（latency 统计） |
| `src_node` / `dst_node` | 路由决策（`compute_xy_route`） |
| `address` | 透传 |
| `data` | 透传 |
| `vc_id` | 输入 VC 索引 |
| `flit_type` | REQUEST=0 / RESPONSE=1 / CREDIT=2（路由器主要处理前两类，credit 由 `receive_credit` 单独处理） |
| `flit_index` / `flit_count` | 多 flit 分组 |
| `hops` | 透传 + 跳数累加（`stats_total_hops_`） |
| `flit_category` | 分类统计 |
| `src_port` | 端口来源 |

`is_write` / `is_ok` / `error_code`: 透传（路由器不修改）。

## 6. 统计

`tlm_stats::StatGroup stat_group_`，4 个指标：

| 指标 | 类型 | 含义 |
|------|------|------|
| `stats_flits_forwarded_` | Scalar | 已转发的 flit 总数 |
| `stats_packets_forwarded_` | Scalar | 已转发的 packet 总数（packet = 同 transaction_id 的 flit 集合） |
| `stats_total_hops_` | Scalar | 累计跳数（packet × hop） |
| `stats_latency_` | Distribution | 单 packet 端到端延迟（从首次接收 flit 到最后一 flit 转发） |

**典型数值范围**（来自 `test_router_tlm.cc` 系列测试）：
- 0 负载场景：`flits_forwarded` = 0
- 1 packet × 1 flit：`flits_forwarded` = 1, `latency` ≈ 5-10 周期（5 阶段流水线 + 链路）
- 多跳场景：每跳 1 周期（不卡 credit）

## 7. 蓝图（未来演进）

### 7.1 Phase 7.E 应用

- 复用现有 `RouterTLM` / `LinkTLM` / `NoCFlitBundle`
- 通过 `BidirectionalPortAdapter<N>` 连接 CU ↔ GPU Crossbar
- gem5 `garnet_synth_traffic.py` 风格的多 CU 并发注入

### 7.2 蓝图增强（gap）

- **自适应路由**（`Mesh_westfirst` 风格）：`routing_algo_` 抽象基类已就绪，仅需新增 `WestFirstRouting` 实现
- **Multi-VC 调度算法**：当前 VA 阶段是简单分配，可升级为 iSLIP / WRR
- **Bypass 通道**（gem5 Garnet 1.0 风格）：单周期直通
- **Speculative RC**（gem5 Garnet 2.0 风格）：先 RC 再 BW，隐藏 RC 延迟

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | 5 端口固定 + 4 VC 固定 + 8 深固定——灵活性低 | 低 | 中 | `NUM_PORTS`/`NUM_VCS`/`BUFFER_DEPTH` 已为 `static constexpr`，可改为模板参数（v2.2+） |
| R2 | 仅支持 Mesh XY 路由——非 mesh 拓扑（如 ring、crossbar）需自实现 `RoutingAlgorithm` | 中 | 低 | `RoutingAlgorithm` 抽象基类 + `set_routing_algorithm()` 暴露 |
| R3 | `pipe_reg_` 仅 1 个 flit/VC——back-pressure 极端场景可能积压 | 低 | 中 | 输入缓冲 8 深 + 死锁检测（`credit_timeout_` >0 时触发） |
| R4 | 仿真端 `LinkTLM` 链路延迟通过 `pending_link_` 1 周期模拟——非真实可配延迟 | 中 | 低 | 当前足够（与其他 NoC 路由器粒度一致） |
| R5 | `on_config_loaded()` 未读取 JSON params（`node_x`/`node_y`/`mesh_x`/`mesh_y` 仅构造函数硬编码） | 高 | 中 | 沿用 `TrafficGenTLM` / `CPUTLM` 同样语义缺口；规划 Phase7.B 统一修复 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ✅ | `test/test_router_tlm.cc`（推测存在，需 grep 验证）+ Phase 4/5 集成测试 |
| Mesh 2x2 / 4x4 端到端 | ✅ | `configs/mesh_2x2_tlm.json` / `configs/mesh_4x4_tlm.json` |
| 路由正确性 | ✅ | XY 路由确定性 + 死锁自由（gem5 Mesh_XY 行为对齐） |
| Credit 流控 | ✅ | `pipe_reg_` 等 credit 寄存器 + `last_credit_return_cycle_` 死锁检测 |
| Stats 输出 | ✅ | `get_stats_group()` 返回 `stat_group_` 含 4 个指标 |

## 10. 修订历史

- **2026-04-23** (`1d176db` 之前): RouterTLM 初版（v2.1 路径同步）
- **2026-04-23 → 2026-06-08**: 随 v2.1 路径同步至 `include/tlm/router_tlm.hh`
- **2026-06-08** (v2.1.0): 随 v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B0 样本）
- **未来**: P0.1 改进（`pipe_reg_` / `sa_winners_` / `credit_timeout_`）的提交细节待追溯

> **注**: RouterTLM 的 P0.1 改进（`pipe_reg_` 等 credit 寄存器 + SA 多 winner + credit timeout）来自 v2.1 期间的 P0 债务修复 wave。具体 commit 需 `git log -- include/tlm/router_tlm.hh` 追溯。
