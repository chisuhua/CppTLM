# Credit Flow Control 实施计划

**版本**: 1.0
**日期**: 2026-04-28
**状态**: 实施中
**预估工作量**: 1-2 天

---

## 一、问题分析

### 1.1 当前状态

RouterTLM 实现了 credit-based flow control 基础结构：

```cpp
// downstream_credits_[port][vc] - 每端口每 VC 独立的 credit 计数器
std::array<std::array<uint8_t, NUM_VCS>, NUM_PORTS> downstream_credits_;

// Credit 操作 API
bool has_credit(unsigned out_port, unsigned vc) const;
void consume_credit(unsigned out_port, unsigned vc);
void receive_credit(unsigned in_port, unsigned vc);  // ← 存在但从未被调用！
```

### 1.2 核心问题

| 问题 | 说明 |
|------|------|
| `receive_credit()` 从未被调用 | Credit 返回路径未实现 |
| `credit_safety_reset()` 作为安全网 | 周期性强制重置，非精确流控 |
| ChStream 无 credit 信号机制 | 反向通道（resp_out→resp_in）未用于 credit |

### 1.3 Credit 流程

**当前流程（有 bug）**：
```
Router A                          Router B
    │                                  │
    │ has_credit() = true              │
    │ consume_credit() → 7             │
    │ send flit ──────────────────────►│ BW: 收到 flit 入 input_buffer
    │                                  │ VA/SA: 分配 VC，转发
    │                                  │ ST: 消耗 flit from buffer
    │                                  │ LT: 发送 flit 到下游
    │                                  │
    │←─ NO CREDIT RETURN ──────────────│
    │ (credit 永远不返回)               │
    │ has_credit() = 0 (8 周期后 safety reset)
```

**正确流程**：
```
Router A                          Router B
    │                                  │
    │ has_credit() = true              │
    │ consume_credit() → 7             │
    │ send flit ──────────────────────►│ BW: 收到 flit 入 input_buffer
    │                                  │ VA/SA: 分配 VC，转发
    │                                  │ ST: 消耗 flit from buffer
    │                                  │   → 同时触发 return_credit(A, vc)
    │                                  │ LT: 发送 flit 到下游
    │                                  │
    │←─── credit return (vc) ←─────────│
    │ receive_credit(vc) → 8            │
    │ has_credit() = true              │
```

---

## 二、解决方案设计

### 2.1 Credit 返回路径

利用 ChStream 的双向通道：

```cpp
// 连接配置: router_A.resp_out[EAST] → router_B.req_in[WEST]
//            router_B.resp_out[WEST] → router_A.resp_in[EAST]  (未使用)

// Credit 返回方案:
// 1. Router B 的 ST 阶段调用 return_credit_to_upstream(in_port, vc)
// 2. 通过 BidirectionalPortAdapter 发送 credit 信号到 resp_out_[reverse_port]
// 3. Credit 信号通过 ChStream 的反向连接传到 Router A 的 resp_in
// 4. Router A 的 process_response_input() 接收 credit 并调用 receive_credit()
```

### 2.2 反向端口映射

| 方向 | 输入端口 | 反向输出端口 |
|------|----------|--------------|
| NORTH | 0 | SOUTH (2) |
| EAST | 1 | WEST (3) |
| SOUTH | 2 | NORTH (0) |
| WEST | 3 | EAST (1) |
| LOCAL | 4 | LOCAL (4) |

### 2.3 Credit 信号格式

使用简化的 credit 返回，不需要携带完整 flit 数据：

```cpp
// Credit 返回包（轻量级）
struct CreditReturn {
    unsigned port;      // 上游端口索引
    unsigned vc;        // VC ID
};

// 在 ChStream 中传递时，复用现有通道或创建专用 credit 通道
```

---

## 三、实施步骤

### Step 1: 添加 Credit 返回基础设施

**文件**: `include/tlm/router_tlm.hh`, `src/tlm/router_tlm.cc`

1. 添加反向端口映射函数：
```cpp
private:
    // 获取反向端口索引 (EAST→WEST, NORTH→SOUTH, etc.)
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

2. 添加 `return_credit_to_upstream()` 方法：
```cpp
// 在 ST 阶段调用，当 flit 被消耗时触发 credit 返回
void return_credit_to_upstream(unsigned in_port, unsigned vc);
```

3. 添加 credit 信号缓冲区（用于延迟返回）：
```cpp
struct PendingCreditReturn {
    unsigned port;      // 反向端口
    unsigned vc;        // VC ID
    unsigned remaining_cycles;  // 延迟周期数
};
std::queue<PendingCreditReturn> pending_credit_returns_;
```

### Step 2: 修改 stage_switch_traversal()

**文件**: `src/tlm/router_tlm.cc`

在 ST 阶段，当 flit 从 input_buffer 移出时，触发 credit 返回：

```cpp
void RouterTLM::stage_switch_traversal() {
    for (const auto& winner : sa_winners_) {
        // ... 处理 flit ...

        // ST 阶段：flit 从 input_buffer 移出，触发 credit 返回到上游
        return_credit_to_upstream(winner.in_port, winner.out_vc);

        // ... 其余逻辑 ...
    }
}
```

### Step 3: 实现 return_credit_to_upstream()

```cpp
void RouterTLM::return_credit_to_upstream(unsigned in_port, unsigned vc) {
    unsigned reverse_p = reverse_port(in_port);

    // 创建 credit 返回延迟事件
    PendingCreditReturn cr;
    cr.port = reverse_p;
    cr.vc = vc;
    cr.remaining_cycles = 1;  // 1 周期链路延迟
    pending_credit_returns_.push(cr);
}
```

### Step 4: 修改 stage_link_traversal()

添加 credit 返回处理：

```cpp
void RouterTLM::stage_link_traversal() {
    // 处理 flit 发送
    if (!pending_link_.empty()) {
        auto pf = pending_link_.front();
        resp_out_[pf.out_port].write(pf.bundle);
        pending_link_.pop();
    }

    // 处理 credit 返回
    std::queue<PendingCreditReturn> next_queue;
    while (!pending_credit_returns_.empty()) {
        auto cr = pending_credit_returns_.front();
        pending_credit_returns_.pop();

        cr.remaining_cycles--;
        if (cr.remaining_cycles == 0) {
            // Credit 返回到上游
            // 通过 resp_out_[cr.port] 发送 credit 信号
            // 注意：需要 ChStream 支持 credit 信号类型
            send_credit_return(cr.port, cr.vc);
        } else {
            next_queue.push(cr);
        }
    }
    pending_credit_returns_ = std::move(next_queue);
}
```

### Step 5: 扩展 ChStream 支持 Credit 信号

**方案 A**: 扩展 ChStream 协议添加 credit 包类型（推荐）

**文件**: `include/bundles/noc_bundles_tlm.hh`

```cpp
// 在 NoCFlitBundle 中添加 credit 信号
struct CreditSignalBundle {
    // 轻量级 credit 信号，不需要完整 flit 数据
    ChannelBundle header;  // type = CH_CREDIT
    uint8_t vc_id;
    uint8_t port_id;
    uint8_t reserved;
};
```

**文件**: `include/framework/stream_adapter.hh`

添加 credit 发送方法：

```cpp
// StreamAdapterBase 接口扩展
virtual void send_credit(unsigned port, unsigned vc) {
    (void)port; (void)vc;
    // 默认空实现
}
```

**文件**: `include/framework/bidirectional_port_adapter.hh`

实现 credit 发送：

```cpp
void send_credit(unsigned port, unsigned vc) override {
    if (port >= N) return;
    // 通过 resp_out_port_[port] 发送 credit 信号到反向通道
    // 需要 ChStream 协议支持
}
```

### Step 6: 修改 BidirectionalPortAdapter::tick()

添加 credit 接收处理：

```cpp
void tick() override {
    // 现有 flit 发送逻辑...

    // 新增：处理 credit 接收（从反向通道）
    if (resp_in_port_[i] && resp_in_port_[i]->has_credit_signal()) {
        auto credit = resp_in_port_[i]->receive_credit();
        module_->receive_credit(credit.port, credit.vc);
    }
}
```

### Step 7: 添加单元测试

**文件**: `test/test_router_credit_flow.cc`（新建）

测试场景：
1. 单跳 credit 循环：NIC→Router→NIC
2. 多跳 credit 流动：Router A → Router B → Router C
3. VC 独立 credit 计数
4. credit 耗尽反压验证
5. credit 返回延迟验证

---

## 四、文件清单

### 新增文件

| 文件 | 说明 | 行数 |
|------|------|------|
| `plans/credit-flow-control-impl.md` | 本文档 | ~200 |

### 修改文件

| 文件 | 修改内容 | 行数 |
|------|---------|------|
| `include/tlm/router_tlm.hh` | 添加反向端口映射、credit 返回方法 | ~30 |
| `src/tlm/router_tlm.cc` | 实现 credit 返回逻辑、修改 ST/LT 阶段 | ~80 |
| `include/bundles/noc_bundles_tlm.hh` | 添加 CreditSignalBundle | ~20 |
| `include/framework/bidirectional_port_adapter.hh` | 添加 credit 发送/接收 | ~30 |
| `src/framework/bidirectional_port_adapter.cc` | 实现 credit 逻辑 | ~20 |
| `test/test_router_credit_flow.cc` | 新建测试 | ~200 |

---

## 五、验证方法

### 5.1 编译验证
```bash
cmake --build build -j$(nproc)
```

### 5.2 单元测试
```bash
./build/bin/cpptlm_tests "[router]"
./build/bin/cpptlm_tests "[credit]"
```

### 5.3 手动验证场景
```bash
# 启动 2x2 Mesh 拓扑
./build/bin/cpptlm_main configs/mesh_2x2.json

# 验证 credit 值变化：
# - 初始: downstream_credits_[port][vc] = 8
# - 发送 flit: -1
# - 收到返回: +1
# - 零值后反压生效
```

---

## 六、风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| ChStream 协议修改影响现有模块 | 中 | 高 | 使用类型安全扩展，添加新的包类型而非修改现有 |
| credit 死锁 | 低 | 高 | 保留 `credit_safety_reset()` 作为可选安全网 |
| 反向通道与正常通道冲突 | 低 | 中 | credit 使用独立队列，与 flit 分离处理 |

---

## 七、版本历史

| 版本 | 日期 | 修改 |
|------|------|------|
| 1.0 | 2026-04-28 | 初始版本 |