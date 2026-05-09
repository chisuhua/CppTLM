# Phase 7 NoC/NIC TLM 完整实施计划

**版本**: 1.1
**日期**: 2026-05-09
**状态**: ✅ 核心代码已完成 / 📝 专项测试待补全
**预估工作量**: 3-4 天（并行执行）→ 实际核心代码已完成，剩余专项测试约 1 天

---

## 一、执行摘要

> **文档同步更新 (2026-05-09)**: 本计划基于 2026-04-27 状态分析，但实际代码在计划编写后已实现大部分内容。本次审计确认以下项目**已完成**。

Phase 7 核心代码**实际已完成约 95%**：

### ✅ 已完成项目（代码审计确认，2026-05-09）

| 项目 | 状态 | 证据位置 |
|------|:---:|---------|
| Credit-based Flow Control | ✅ | `router_tlm.cc:395-414` — `receive_credit()`, `return_credit_to_upstream()`, `pending_credit_returns_` |
| LinkTLM 链路模块 | ✅ | `include/tlm/link_tlm.hh` (111 行), `src/tlm/link_tlm.cc` (125 行), 已注册 |
| RouterTLM StatGroup | ✅ | `router_tlm.hh:279-284` — 已使用 `tlm_stats::StatGroup` |
| NICTLM adapter | ✅ | `nic_tlm.cc:49-50` — 已保存 adapter 指针 |
| mesh_4x4.json | ✅ | 48 modules (16 Router + 16 NIC + 16 CPU), 88+ connections |
| nic_router_nic.json | ✅ | 2 NIC + 1 Router，最小测试拓扑 |
| NoCStatistics | ✅ | `include/tlm/noc_statistics.hh` (148 行, 仅头文件) |

### 📝 待补全项目

1. **专项测试文件**（4 个文件尚未创建）：
   - `test_router_vc_arbiter.cc` — VC 分配 + 仲裁测试
   - `test_router_six_stage.cc` — 六阶段流水线测试
   - `test_nic_packetization.cc` — 包化/重组测试
   - `test_link_tlm.cc` — LinkTLM 单元测试
2. **test_phase7_integration.cc** — 当前存在 `test_phase7_benchmark.cc` 和 `test_phase7_transaction_lifecycle.cc`，但缺少 NIC→Router→NIC 集成测试
3. **流量模式** — `TrafficPattern` enum 和 `select_destination()` 尚未实现

---

## 二、优先级排序与依赖关系

### 依赖图

```
Task 1.1 (Credit Flow 修复)
    │
    ▼
Task 1.2 (LinkTLM) ───────┐
    │                      │
    ▼                      ▼
Task 1.3 (StatGroup)    Task 2.2 (VC Arbiter Test)
    │                      │
    ▼                      ▼
Task 1.4 (NIC adapter)  Task 2.3 (Six Stage Test)
    │                      │
    └──────────┬───────────┘
               ▼
        Task 2.1 (Phase7 Integration Test) ──→ 最终验收
               │
               ▼
    ┌─────────────────────┐
    │  Task 3.x (配置/统计)  │  (可并行)
    └─────────────────────┘
```

### 任务分组

| 组 | 任务 | 优先级 | 依赖 | 并行 |
|---|---|:---:|:---|:---|
| **A1** | 1.1 ~~精确~~ Credit Flow Control | ✅ | 无 | 已实现完整机制 |
| **A2** | 1.2 LinkTLM 模块 | ✅ | A1 | 已实现并注册 |
| **A3** | 1.3 RouterTLM StatGroup 统一 | ✅ | A1 | 已与 A2 并行完成 |
| **A4** | 1.4 NICTLM adapter 修复 | ✅ | 无 | 已实现 |
| **B1** | 2.1 test_phase7_integration.cc 重写 | 🔴 | A1,A2,A3,A4 | |
| **B2** | 2.2 test_router_vc_arbiter.cc | 🟡 | A1 | 可与 B3,B4 并行 |
| **B3** | 2.3 test_router_six_stage.cc | 🟢 | A1 | 可与 B2,B4 并行 |
| **B4** | 2.4 test_nic_packetization.cc | 🟢 | A4 | 可与 B2,B3 并行 |
| **C1** | 3.1 mesh_4x4.json | ✅ | 无 | 已实现 (48 modules) |
| **C2** | 3.2 nic_router_nic.json | ✅ | 无 | 已实现 |
| **C3** | 3.3 NoCStatistics 统计类 | ✅ | A3 | 已实现 (仅头文件) |
| **D1** | 4.1 流量模式 | 🟢 | 无 | 最后 |

---

## 三、详细实施步骤

---

### ✅ Task 1.1: ~~精确~~ Credit-based Flow Control（已完成）

**文件**: `src/tlm/router_tlm.cc`, `include/tlm/router_tlm.hh`

**问题**: 当前 `credit_safety_reset()` 每 8 周期强制重置所有 credits，注释明确说明"非精确流控"：
```cpp
// Credit 安全网：每 BUFFER_DEPTH 周期重置所有 credit，防止永久阻塞
// 注意：这是安全网而非精确的 credit-based flow control
```

**实施步骤**:

1. **修改 `stage_link_traversal()`** — 发送 flit 到下游的同时，向上游返回 credit
   ```cpp
   // 在 LT 阶段发送 flit 到 resp_out 的同时
   // 需要建模：下游 Router 消费 flit 后返回 credit
   // 方案 A: 通过 LinkTLM 传递 credit 返回信号
   // 方案 B: 在 BidirectionalPortAdapter::tick() 中处理
   ```

2. **添加 Credit 返回接口** — RouterTLM 需要接收来自下游的 credit 返回：
   ```cpp
   void receive_credit(unsigned in_port, unsigned vc);  // 已有实现但未被调用
   ```

3. **修改 `credit_safety_reset()`** — 改为真正的 credit 返回机制：
   - 移除周期性强制重置
   - 保留为可选的调试/恢复机制（仅在 deadlock 检测时触发）
   - 添加 `credit_timeout_` 计数器，超过阈值才触发安全网

4. **在 BidirectionalPortAdapter 中传递 credit**:
   - 当 process_request_input() 接收 flit 时，消费本地 input_buffer 空间
   - 当 input_buffer 有空间释放时（flit 被转发走），通过反向链路返回 credit

**验收标准**:
- `./build/bin/cpptlm_tests "[router]"` 全部通过
- Credit 值从 BUFFER_DEPTH 开始，发送 flit 后递减，收到返回后递增
- 不会出现 credit 负数或超过 BUFFER_DEPTH

**预估**: 0.5 天

---

### ✅ Task 1.2: LinkTLM 模块（已完成）

**文件**: 
- 新增 `include/tlm/link_tlm.hh` (~80 行)
- 新增 `src/tlm/link_tlm.cc` (~60 行)
- 修改 `include/chstream_register.hh`
- 修改 `src/CMakeLists.txt`（添加 link_tlm.cc）

**设计**:

LinkTLM 建模两个 Router 之间的物理链路：
- 链路延迟（可配置，默认 1 周期）
- 带宽限制（每周期 1 flit）
- Credit 返回传递（从下游 Router 返回到上游 Router）

```cpp
class LinkTLM : public ChStreamModuleBase {
public:
    static constexpr unsigned DEFAULT_LATENCY = 1;
    
    LinkTLM(const std::string& name, EventQueue* eq,
            unsigned latency = DEFAULT_LATENCY);
    
    // 端口: 双向，每侧有 req_in + resp_out
    // 实际使用两个端口: src_port (连接上游 Router) 和 dst_port (连接下游 Router)
    
    void tick() override;
    
    // 配置
    void set_latency(unsigned latency) { latency_ = latency; }
    unsigned latency() const { return latency_; }
    
private:
    unsigned latency_ = DEFAULT_LATENCY;
    
    // 延迟队列: 建模链路传输延迟
    struct DelayedFlit {
        bundles::NoCFlitBundle flit;
        unsigned remaining_cycles;
    };
    std::queue<DelayedFlit> delay_queue_;
    
    // Credit 返回队列
    struct CreditReturn {
        unsigned port;
        unsigned vc;
        unsigned remaining_cycles;
    };
    std::queue<CreditReturn> credit_queue_;
};
```

**注册**:
```cpp
// chstream_register.hh 中添加
ModuleFactory::registerObject<tlm::LinkTLM>("LinkTLM");
ChStreamAdapterFactory::get().registerAdapter<tlm::LinkTLM,
    bundles::NoCFlitBundle, bundles::NoCFlitBundle>("LinkTLM");
```

**验收标准**:
- LinkTLM 可独立实例化并通过编译
- `test_link_tlm.cc` 验证延迟队列和 credit 传递
- Mesh 配置中 Router 之间可通过 LinkTLM 连接

**预估**: 0.75 天

---

### ✅ Task 1.3: RouterTLM 统一使用 tlm_stats::StatGroup（已完成）

**文件**: `include/tlm/router_tlm.hh`, `src/tlm/router_tlm.cc`

**问题**: RouterTLM 使用原始 struct RouterStats：
```cpp
struct RouterStats {
    uint64_t flits_forwarded = 0;
    uint64_t packets_forwarded = 0;
    // ...
};
RouterStats stats_;
```

而 NICTLM 已正确使用 StatGroup：
```cpp
tlm_stats::StatGroup stat_group_;
tlm_stats::Scalar& stats_flits_sent_;
// ...
```

**实施步骤**:

1. **替换 RouterStats 为 StatGroup**:
   ```cpp
   // router_tlm.hh
   private:
       tlm_stats::StatGroup stat_group_;
       tlm_stats::Scalar& stats_flits_forwarded_;
       tlm_stats::Scalar& stats_packets_forwarded_;
       tlm_stats::Scalar& stats_total_hops_;
       tlm_stats::Distribution& stats_latency_;
       tlm_stats::Distribution& stats_buffer_occupancy_;
       
   public:
       tlm_stats::StatGroup& stats() { return stat_group_; }
   ```

2. **修改构造函数初始化列表**:
   ```cpp
   RouterTLM::RouterTLM(...)
       : ChStreamModuleBase(name, eq),
         // ...
         stat_group_("router"),
         stats_flits_forwarded_(stat_group_.addScalar("flits_forwarded", ...)),
         // ...
   ```

3. **修改统计更新点**:
   ```cpp
   // stage_switch_traversal() 中
   ++stats_flits_forwarded_;
   stats_total_hops_ += flit.bundle.hops.read();
   stats_latency_.sample(current_cycle_ - flit.cycle_received);
   ```

4. **更新测试**:
   - `test_router_tlm.cc` 中修改 stats 访问方式

**验收标准**:
- `./build/bin/cpptlm_tests "[router]"` 全部通过
- StatGroup dump() 输出包含 router 统计项

**预估**: 0.5 天

---

### ✅ Task 1.4: NICTLM set_stream_adapter() 空实现修复（已完成）

**文件**: `src/tlm/nic_tlm.cc`, `include/tlm/nic_tlm.hh`

**问题**: 当前实现为空：
```cpp
void NICTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    // DualPortStreamAdapter 通过内部指针存储，不需要额外处理
    (void)adapter;
}
```

**实施步骤**:

1. **添加 adapter 成员变量**（如果 DualPortStreamAdapter 需要显式保存）:
   ```cpp
   // nic_tlm.hh
   private:
       cpptlm::DualPortStreamAdapter<NICTLM, ...>* adapter_ = nullptr;
   ```

2. **修改 set_stream_adapter()**:
   ```cpp
   void NICTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
       adapter_ = static_cast<cpptlm::DualPortStreamAdapter<NICTLM,
           bundles::CacheReqBundle, bundles::CacheRespBundle,
           bundles::NoCFlitBundle, bundles::NoCFlitBundle>*>(adapter);
   }
   ```

3. **验证 ModuleFactory 中 DualPortAdapter 是否正确绑定**:
   - 检查 `src/core/module_factory.cc` 中 Step 7 对 NICTLM 的处理
   - 确保 `registerDualPortAdapter` 的 adapter 被正确创建和注入

**验收标准**:
- NICTLM 可通过 ModuleFactory + JSON 配置正确实例化
- `test_nic_tlm.cc` 测试通过

**预估**: 0.25 天

---

### 🔴 Task 2.1: test_phase7_integration.cc 重写

**文件**: `test/test_phase7_integration.cc`（完全重写，~300 行）

**问题**: 当前内容测试 CPUTLM/TrafficGenTLM/ArbiterTLM，与 Phase 7 NoC 无关。

**参考模式**: `test/test_phase6_integration.cc`

**测试场景**:

1. **最小拓扑: NIC → Router → NIC**（2 节点直连）
   ```cpp
   TEST_CASE("Phase 7: NIC→Router→NIC single hop", "[phase7][integration]") {
       EventQueue eq;
       REGISTER_CHSTREAM;
       ModuleFactory factory(&eq);
       
       json config = R"({
           "modules": [
               {"name": "nic0", "type": "NICTLM", "node_id": 0, "mesh_x": 2, "mesh_y": 1},
               {"name": "nic1", "type": "NICTLM", "node_id": 1, "mesh_x": 2, "mesh_y": 1},
               {"name": "router0", "type": "RouterTLM", "node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 1}
           ],
           "connections": [
               {"src": "nic0", "dst": "router0.4", "latency": 1},
               {"src": "router0.4", "dst": "nic1", "latency": 1}
           ]
       })"_json;
       
       factory.instantiateAll(config);
       // ... 注入请求，验证端到端传输
   }
   ```

2. **2×2 Mesh: 4 NIC + 4 Router 多跳**
   - 验证 XY 路由多跳转发
   - 验证 flit 经过多个 Router 后 hops 计数正确

3. **NIC packetize → Router 转发 → NIC reassemble**
   - 注入 CacheReqBundle 到 NIC0
   - 经过 Router 转发到 NIC1
   - 验证 NIC1 输出 CacheRespBundle

4. **Credit Flow Control 端到端验证**
   - 高速注入 flits，验证 credit 反压生效
   - 验证不会出现丢包或死锁

**验收标准**:
- 至少 4 个 TEST_CASE，覆盖上述场景
- 全部通过 `./build/bin/cpptlm_tests "[phase7]"`
- 零回归（现有 `[phase6]` 测试不受影响）

**预估**: 1 天

---

### 🟡 Task 2.2: test_router_vc_arbiter.cc

**文件**: 新增 `test/test_router_vc_arbiter.cc`（~180 行）

**测试内容**:

1. **VC 分配测试**:
   - HEAD flit 请求 VC，验证分配成功
   - 4 个 VC 全部分配后，第 5 个请求失败
   - TAIL flit 通过后释放 VC，新请求可分配

2. **Switch Allocation 仲裁测试**:
   - 多个输入端口竞争同一输出端口
   - Round-Robin 公平性验证
   - 不同输出端口可同时被分配（多 winner）

3. **Credit Flow 集成测试**:
   - credit 为 0 时，SA 阶段跳过该 VC
   - credit 返回后，下一周期可继续发送

**验收标准**:
- 至少 6 个 TEST_CASE
- `./build/bin/cpptlm_tests "[router][vc]"` 通过

**预估**: 0.5 天

---

### 🟢 Task 2.3: test_router_six_stage.cc

**文件**: 新增 `test/test_router_six_stage.cc`（~120 行）

**测试内容**:

1. **BW 阶段**: flit 从 req_in 进入 input_buffer
2. **RC 阶段**: HEAD flit 路由计算正确（查表验证 routing_table_）
3. **VA 阶段**: VC 分配后 vc_state_ 更新正确
4. **SA 阶段**: 仲裁 winner 选择正确
5. **ST 阶段**: flit 从 input_buffer 移除并进入 pending_link_
6. **LT 阶段**: 1 周期后 flit 出现在 resp_out

**验收标准**:
- 每个阶段独立验证
- 流水线延迟 = 6 周期（从注入到输出）

**预估**: 0.5 天

---

### 🟢 Task 2.4: test_nic_packetization.cc

**文件**: 新增 `test/test_nic_packetization.cc`（~140 行）

**测试内容**:

1. **单 flit 请求**: size=8 → 1 个 HEAD_TAIL flit
2. **多 flit 请求**: size=32 → 4 个 flits (HEAD+BODY+BODY+TAIL)
3. **Reassembly 完整**: 4 个 flits 按序到达 → 输出完整 CacheRespBundle
4. **Reassembly 乱序处理**: flits 乱序到达仍能正确重组
5. **AddressMap 映射**: 不同地址范围映射到不同 dst_node

**验收标准**:
- 至少 5 个 TEST_CASE
- `./build/bin/cpptlm_tests "[nic][packetize]"` 通过

**预估**: 0.5 天

---

### ✅ Task 3.1: configs/mesh_4x4.json（已完成）

**文件**: 新增 `configs/mesh_4x4.json`（~200 行）

**内容**: 16 节点 Mesh 拓扑（16 NIC + 16 Router）

```json
{
  "modules": [
    {"name": "nic_0_0", "type": "NICTLM", "node_id": 0, "mesh_x": 4, "mesh_y": 4},
    // ... 16 个 NIC
    {"name": "router_0_0", "type": "RouterTLM", "node_x": 0, "node_y": 0, "mesh_x": 4, "mesh_y": 4},
    // ... 16 个 Router
  ],
  "connections": [
    // NIC → Router (Local 端口)
    {"src": "nic_0_0", "dst": "router_0_0.4", "latency": 1},
    // ... 
    // Router → Router (网格连接)
    {"src": "router_0_0.1", "dst": "router_1_0.3", "latency": 1},  // East
    {"src": "router_0_0.0", "dst": "router_0_1.2", "latency": 1},  // North
    // ...
  ]
}
```

**预估**: 0.25 天

---

### ✅ Task 3.2: configs/test/nic_router_nic.json（已完成）

**文件**: 新增 `configs/test/nic_router_nic.json`（~30 行）

**内容**: 最小测试拓扑（2 NIC + 1 Router）

**预估**: 0.1 天

---

### ✅ Task 3.3: NoCStatistics 统计类（已完成）

**文件**: 新增 `include/tlm/noc_statistics.hh`（~100 行）

**设计**:
```cpp
class NoCStatistics {
public:
    void record_packet_sent(uint32_t src, uint32_t dst, uint64_t latency, uint64_t hops);
    void record_flit_forwarded(uint32_t router_id);
    void record_congestion(uint32_t router_id, unsigned port);
    
    // 聚合结果
    double avg_latency() const;
    double avg_hops() const;
    double throughput_flits_per_cycle() const;
    void dump(std::ostream& os) const;
    
private:
    tlm_stats::StatGroup root_;
    // 各子统计组...
};
```

**预估**: 0.5 天

---

### 🟢 Task 4.1: Uniform Random / Hotspot 流量模式

**文件**: `include/tlm/nic_tlm.hh`, `src/tlm/nic_tlm.cc`

**设计**:
```cpp
enum class TrafficPattern {
    UNIFORM_RANDOM,  // 均匀随机
    HOTSPOT,         // 热点
    TRANSPOSE,       // 转置
    BIT_COMPLEMENT   // 位补码
};

// NICTLM 中添加
void set_traffic_pattern(TrafficPattern pattern);
uint32_t select_destination(TrafficPattern pattern, uint32_t src_node);
```

**预估**: 0.5 天

---

## 四、并行实施机会

### 并行组 1（第 1 天，可全部并行）
- A1: Credit Flow 修复
- A4: NICTLM adapter 修复
- B3: test_router_six_stage.cc
- B4: test_nic_packetization.cc
- C1: mesh_4x4.json
- C2: nic_router_nic.json

### 并行组 2（第 2 天，A1 完成后可并行）
- A2: LinkTLM（依赖 A1）
- A3: StatGroup 统一（依赖 A1，可与 A2 并行）
- B2: test_router_vc_arbiter.cc（依赖 A1）
- C3: NoCStatistics（依赖 A3，可与 A3 并行）

### 并行组 3（第 3 天，全部前置完成后）
- B1: test_phase7_integration.cc 重写（依赖 A1-A4）
- D1: 流量模式（可独立进行）

### 第 4 天
- 全量回归测试
- 性能基准测试
- 文档更新

---

## 五、验证方法

### 5.1 每任务验证

每个 Task 完成后必须执行：

```bash
# 1. 编译验证
cmake --build build -j$(nproc)

# 2. 相关单元测试
./build/bin/cpptlm_tests "[router]"     # Router 相关
./build/bin/cpptlm_tests "[nic]"        # NIC 相关
./build/bin/cpptlm_tests "[noc]"        # NoC 通用
./build/bin/cpptlm_tests "[phase7]"     # Phase 7 集成

# 3. 格式检查
./scripts/format.sh --check
```

### 5.2 阶段验收验证

#### 阶段 1 完成验证（核心代码修复）
```bash
# 编译零警告
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF
cmake --build build -j$(nproc)

# Router + NIC 测试全部通过
./build/bin/cpptlm_tests "[router]"
./build/bin/cpptlm_tests "[nic]"
./build/bin/cpptlm_tests "[noc]"

# 已知失败保持 12 个（零回归）
```

#### 阶段 2 完成验证（测试补全）
```bash
# Phase 7 集成测试通过
./build/bin/cpptlm_tests "[phase7]"
# 预期: 15+ 用例, 100+ 断言, 全部通过

# 新测试标签
./build/bin/cpptlm_tests "[router][vc]"
./build/bin/cpptlm_tests "[router][stage]"
./build/bin/cpptlm_tests "[nic][packetize]"
```

#### 最终验收验证
```bash
# 全量回归测试
./build/bin/cpptlm_tests
# 预期: 总用例数 > 450, 零回归

# CTest 方式
ctest --test-dir build --output-on-failure

# 格式检查通过
./scripts/format.sh --check

# 4x4 Mesh 配置可加载
./build/bin/cpptlm_main configs/mesh_4x4.json
```

### 5.3 QA 验收场景

| 场景 | 工具 | 具体步骤 | 预期结果 |
|------|------|---------|---------|
| 编译零警告 | cmake + ninja | `cmake --build build` | 0 errors, 0 warnings |
| Router 单元测试 | cpptlm_tests | `./build/bin/cpptlm_tests "[router]"` | 全部通过 |
| NIC 单元测试 | cpptlm_tests | `./build/bin/cpptlm_tests "[nic]"` | 全部通过 |
| Phase 7 集成 | cpptlm_tests | `./build/bin/cpptlm_tests "[phase7]"` | 全部通过 |
| 全量回归 | cpptlm_tests | `./build/bin/cpptlm_tests` | 零回归 |
| 格式合规 | format.sh | `./scripts/format.sh --check` | 通过 |
| 4x4 Mesh 加载 | cpptlm_main | `./build/bin/cpptlm_main configs/mesh_4x4.json` | 成功加载并运行 100+ 周期 |

---

## 六、风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| Credit Flow 精确实现导致死锁 | 中 | 高 | 保留 credit_safety_reset 作为 deadlock 恢复机制；添加超时检测 |
| LinkTLM 与 BidirectionalPortAdapter 端口绑定冲突 | 中 | 高 | 参考 RouterTLM 和 CrossbarTLM 的 adapter 绑定模式；先在测试中验证 |
| test_phase7_integration 需要大量 Mock | 中 | 中 | 使用 ModuleFactory + JSON 配置直接实例化真实模块；参考 Phase 6 模式 |
| StatGroup 替换导致测试编译失败 | 低 | 中 | 使用 lsp_rename 批量替换 stats() 访问点；编译后逐一修复 |
| 4x4 Mesh 配置规模导致测试超时 | 低 | 低 | 集成测试使用 2×2 Mesh；4×4 仅用于手动基准测试 |

---

## 七、修改文件清单

### 新增文件（~12 个）

| 文件 | 类型 | 预估行数 | 说明 |
|------|------|:------:|------|
| `include/tlm/link_tlm.hh` | 头文件 | 80 | LinkTLM 类定义 |
| `src/tlm/link_tlm.cc` | 源文件 | 60 | LinkTLM 实现 |
| `test/test_router_vc_arbiter.cc` | 测试 | 180 | VC 分配 + 仲裁测试 |
| `test/test_router_six_stage.cc` | 测试 | 120 | 六阶段流水线测试 |
| `test/test_nic_packetization.cc` | 测试 | 140 | 包化/重组测试 |
| `test/test_phase7_integration.cc` | 测试 | 300 | NIC→Router→NIC 集成（重写） |
| `test/test_link_tlm.cc` | 测试 | 80 | LinkTLM 单元测试 |
| `include/tlm/noc_statistics.hh` | 头文件 | 100 | NoC 统计类 |
| `configs/mesh_4x4.json` | 配置 | 200 | 16 节点 Mesh |
| `configs/test/nic_router_nic.json` | 配置 | 30 | 最小测试拓扑 |

### 修改文件（~5 个）

| 文件 | 修改内容 | 预估修改量 |
|------|---------|:---------:|
| `src/tlm/router_tlm.cc` | Credit Flow 精确化 + StatGroup 替换 | ~120 行 |
| `include/tlm/router_tlm.hh` | RouterStats → StatGroup | ~40 行 |
| `src/tlm/nic_tlm.cc` | set_stream_adapter 修复 + 流量模式 | ~30 行 |
| `include/chstream_register.hh` | 添加 LinkTLM 注册 | ~5 行 |
| `src/CMakeLists.txt` | 添加 link_tlm.cc | ~1 行 |

---

## 八、关键设计决策

### 决策 1: Credit 返回路径选择
- **选项 A**: 通过 LinkTLM 显式传递 credit 返回信号
- **选项 B**: 在 BidirectionalPortAdapter 中隐式处理（假设下游总有空间）
- **推荐**: 选项 A（精确建模），但选项 B 可作为快速 fallback

### 决策 2: LinkTLM 是否必需
- **选项 A**: 创建 LinkTLM 作为独立模块
- **选项 B**: 将链路延迟建模在 RouterTLM 的 LT 阶段（当前 pending_link_ 队列）
- **推荐**: 选项 A（架构更清晰，支持不同链路参数），但选项 B 可满足基本需求
- **决策**: 先实现选项 B 的增强版（在 RouterTLM 内完成 credit 返回），Phase 7.x 再实现 LinkTLM
- **修正**: 鉴于 LinkTLM 是"高优先级缺失项"，实施选项 A

### 决策 3: test_phase7_integration.cc 范围
- **最小版本**: 仅测试 NIC→Router→NIC 单跳
- **完整版本**: 4 NIC + 4 Router 2×2 Mesh 多跳
- **推荐**: 完整版本，包含两个 TEST_CASE（单跳 + 多跳 Mesh）

---

## 九、版本历史

| 版本 | 日期 | 修改 |
|------|------|------|
| 1.1 | 2026-05-09 | 文档同步审计：标记已完成项目（LinkTLM、Credit Flow、StatGroup、配置等） |
| 1.0 | 2026-04-27 | 初始版本，基于 Oracle 状态分析 |

