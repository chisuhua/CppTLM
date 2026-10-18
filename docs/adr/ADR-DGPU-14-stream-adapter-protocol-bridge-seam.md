## Status Update 2026-10-18: superseded by 06 v0.4 (未签发 ADR, 永不签发)

**本 ADR 自始未签发** (🟡 起草中 v0.1), 2026-10-18 经 [`docs/architecture/06-inter-module-network.md` v0.4 §4 + §9.2](../architecture/06-inter-module-network.md) 修订后, seam 位置决策已被完全收纳, **永不签发**.

**差异** (vs ADR-DGPU-14 v0.1 起草内容):
- ADR §2.1 "seam 位置 L4↔L0 Packet" → 06 §4.1 锁定 seam 在 L5↔L4 (per 06 §0.1 核心立场第 3 条, 与 PNN §2.2 一致)
- ADR §2.2 "改 Packet 字段" → 06 §2.5 显式不动 Packet (0 改 0 增 0 reset 改)
- ADR §3 "新增 L4.5 ProtocolAdapter" → 06 §4.2 显式不引入新抽象层 (per 06 §0.1 核心立场第 3 条)

**不动 ADR-DGPU-14 正文** (per AGENTS.md "ADR 不可变 + Status Update" 先例), 正文保留作为调研历史.

**关联归档文档**:
- [`../designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md) (评审对象, 已于 2026-10-18 归档至 [`../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md`](../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md))
- [`../archive/architecture/05-packet-semantic-network-archived-2026-10.md`](../architecture/05-packet-semantic-network-archived-2026-10.md) (立场对立, 早期方案已归档)

---

# ADR-DGPU-14: StreamAdapter ↔ ProtocolBridge Seam 决策

> **状态**: 🟡 **起草中** (v0.1 草案, 待 Oracle/Metis 复审, 预计 1-2 轮 Revise 后签发)
> **日期**: 2026-10-12 起草
> **关联架构文档**: [`../designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md) (v0.1, 评审对象)
> **关联调研**: [`../../research/gem5-ruby-protocol-bridge-survey.md`](../../research/gem5-ruby-protocol-bridge-survey.md) (v0.1, 评审对象)
> **配套 ADR**: [ADR-DGPU-13](ADR-DGPU-13-pnn-protocol-neutral-network.md) (PNN 范围定义, 同 cycle 起草)
> **关联现有 ADR**: [ADR-DGPU-12](ADR-DGPU-12-soc-internal-tlm-wiring-principle.md) (R-WIRE-* 原则, 沿用) / [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md) (23 ABI 冻结, 沿用)

---

## 1. 背景

### 1.1 问题

PNN (Protocol-Neutral Network) 方案的核心是引入 `ProtocolBridge<InB, OutB>` 抽象层 (per [ADR-DGPU-13](ADR-DGPU-13-pnn-protocol-neutral-network.md))。但与既有 `StreamAdapter<ModuleT, ReqBundleT, RespBundleT>` 抽象的**关系**需要明确决策:

| 问题 | 候选方案 | 关键判断点 |
|---|---|---|
| **seam 位置** | 候选 A: `Module` ↔ `ProtocolBridge` ↔ `StreamAdapter` ↔ `MasterPort`; 候选 B: `Module` ↔ `StreamAdapter` ↔ `ProtocolBridge` ↔ `MasterPort`; 候选 C: `ProtocolBridge` 作为 `StreamAdapter` 子类 | 哪个 seam 引入开销最小? 哪个对模块代码 0 侵入? |
| **bridge 注入时机** | 候选 1: ModuleFactory Step 7a (与 StreamAdapter 同期); 候选 2: ModuleFactory Step 7b (新 step, 既有后); 候选 3: DGpuBoard::bind_memory_backings() (programmatic) | 哪个能保持 `network.mode` JSON 驱动? 哪个不破 frozen headers? |
| **bridge 状态作用域** | 候选 α: per-module (每个 module 持 1 bridge); 候选 β: per-connection (每条连接持 1 bridge); 候选 γ: per-topology (整个 SoC 持 1 bridge) | 哪个支持 per-connection 异构协议? 哪个最简? |
| **bridge 性能建模粒度** | 候选 i: 零开销 (Identity, 编译期 if 移除); 候选 ii: 1-cycle 固定延迟; 候选 iii: 参数化延迟 (`bridge_latency_cycles`) | 哪个满足 Ruby 对照 <20% 误差验收线? |

### 1.2 现状约束

- **StreamAdapter 5 子类已实现** (`include/framework/`):
  - `StreamAdapter<ModuleT, ReqB, RespB>` (单端口)
  - `MultiPortStreamAdapter<ModuleT, Rq, Rp, N>` (N 端口)
  - `DualPortStreamAdapter<ModuleT, PE_Rq, PE_Rp, Net_Rq, Net_Rp>` (2 组非对称)
  - `BidirectionalPortAdapter<ModuleT, B, N>` (N 端口 + Credit)
  - `Axi4StreamAdapter` (3 通道, + Axi4Mapper)
- **ModuleFactory Step 7a 已注入 StreamAdapter** (`src/core/module_factory.cc:620-735`)
- **ChStreamModuleBase API 已有 `set_stream_adapter()`** (P0 既有, per `include/core/chstream_module.hh:30-50`)
- **ModuleFactory Step 7 mirror 机制** (per `src/core/module_factory.cc:691-701`)

### 1.3 业界锚点 (Gem5 Ruby)

| Gem5 Ruby | CppTLM 现状 | PNN 决策需求 |
|---|---|---|
| NetworkInterface (per controller) | `DualPortStreamAdapter` (per module) | 决定 bridge 作用域 (per-module / per-connection) |
| NI ↔ Garnet Router | `StreamAdapter` ↔ `SimplePort` | 决定 seam 位置 (Module↔Bridge↔Adapter vs Module↔Adapter↔Bridge) |
| NI::wakeup() 调度 | `StreamAdapter::tick()` | 决定 bridge 注入时机 (Step 7a vs 7b) |
| NI 内部状态 (credits, outstanding) | `BidirectionalPortAdapter::pending_credits_` | 决定 bridge 状态作用域 |

---

## 2. 决策

### 2.1 决策 1: Seam 位置 — 候选 A (`Module ↔ ProtocolBridge ↔ StreamAdapter ↔ MasterPort`)

**接受候选 A 方案**, Seam 在 Module 与既有 StreamAdapter 之间插入 ProtocolBridge:

```
┌──────────────────────────────────────────────────────────┐
│ Module (CacheTLM / CrossbarTLM / RouterTLM / ...)        │
│   ├─ InputStreamAdapter<MyReqBundle> req_in_            │
│   └─ OutputStreamAdapter<MyRespBundle> resp_out_        │
└─────────────────────┬────────────────────────────────────┘
                      │ (Module 与 Bridge 接口, **新增 seam**)
┌─────────────────────▼────────────────────────────────────┐
│ ProtocolBridge<InB, OutB>                                 │
│   ├─ pack(InB) → OutB                                    │
│   ├─ unpack(OutB) → InB                                  │
│   ├─ configure(json)                                     │
│   └─ stats_ (pack_cycles, unpack_cycles, conversions)    │
└─────────────────────┬────────────────────────────────────┘
                      │ (Bridge 与 StreamAdapter 接口, **既有 seam**)
┌─────────────────────▼────────────────────────────────────┐
│ StreamAdapter<ModuleT, Bridge<>::InB, Bridge<>::OutB>    │
│   ├─ tick() → bridge->pack/inject → MasterPort::send     │
│   └─ process_request_input → bridge->unpack → req_in_    │
└─────────────────────┬────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────┐
│ MasterPort / SlavePort / SimplePort / Packet             │
└──────────────────────────────────────────────────────────┘
```

**为什么候选 A**:
- **模块代码 0 侵入**: 模块仍只与 `InputStreamAdapter<Bridge::InB>` / `OutputStreamAdapter<Bridge::OutB>` 交互, 不感知 ProtocolBridge 存在
- **既有 StreamAdapter 不改**: StreamAdapter 模板签名从 `StreamAdapter<ModuleT, ReqBundleT, RespBundleT>` 变为 `StreamAdapter<ModuleT, BridgeInB, BridgeOutB>`, 但 BridgeInB 编译期可推断为 `NoCFlitBundle` (P0 abstract) 或 `Axi4Bundle` (P2) 等
- **桥可独立测试**: ProtocolBridge 类可独立 mock 测试, 不依赖 StreamAdapter
- **性能可观测**: bridge 内 `pack_cycles` / `unpack_cycles` 计数器独立于 StreamAdapter::tick()

**为什么拒绝候选 B** (Module ↔ StreamAdapter ↔ ProtocolBridge ↔ MasterPort):
- bridge 介入 MasterPort 层, 需修改 SimplePort 序列化 (违背 `pcie_endpoint_tlm.h` 等 frozen)
- bridge 状态 (OOO rid) 跨 StreamAdapter 边界, 调试复杂

**为什么拒绝候选 C** (ProtocolBridge 作为 StreamAdapter 子类):
- 混合两个独立抽象, 编译期类型擦除
- `StreamAdapter<M, Req, Resp>` 与 `ProtocolBridge<InB, OutB>` 是**两个正交轴** (StreamAdapter 关注 Module↔Port 转换, ProtocolBridge 关注 InB↔OutB 转换), 组合成 1 类违反单一职责

### 2.2 决策 2: 注入时机 — Step 7b (与 Step 7a 既有后, 增量)

**接受 Step 7b 方案** (ModuleFactory 新增 Step, 既有 Step 7a 之后):

```cpp
// src/core/module_factory.cc (P1 增量, +15 行)
void ModuleFactory::instantiateAll(const json& config) {
    // ... 既有 Step 1-6 ...

    // === Step 7a (既有): ChStreamModuleBase 注入 StreamAdapter ===
    for (auto& ch_mod : chstream_modules) {
        // ... 既有 StreamAdapter 注入 ...
    }

    // === Step 7b (PNN 增量): 协议桥注入 ===
    if (config.contains("network") && config["network"].contains("mode")) {
        std::string mode = config["network"]["mode"].get<std::string>();
        ProtocolBridgeFactory& bridge_factory = ProtocolBridgeFactory::get();
        if (!bridge_factory.knows(mode)) {
            throw std::runtime_error("Unknown network.mode: " + mode);
        }
        for (auto& ch_mod : chstream_modules) {
            auto bridge = bridge_factory.create(mode);
            bridge->configure(config["network"]);
            ch_mod->set_protocol_bridge(bridge);  // P1 新增 API
        }
    }

    // ... 既有 Step 8-9 ...
}
```

**为什么 Step 7b**:
- **既有 Step 7a 路径不动**: 0 改 `module_factory.cc:620-735` 既有代码
- **network.mode JSON 驱动**: P1 启用后, `network.mode` 字段缺失时 fallback 到既有 (per `simulation_mode` 缺省 `"functional"` 的 `dgpu_soc_timing_v1.json:4` 先例)
- **错误处理明确**: mode 未知时抛 `std::runtime_error`, 与既有 `ModuleFactory::instantiateAll` 错误处理风格一致

**为什么拒绝 Step 7a** (与 StreamAdapter 同期):
- 既有 Step 7a 代码 (620 行) 改动风险高
- StreamAdapter 与 ProtocolBridge 关注点不同, 同期注入混淆职责

**为什么拒绝 Step 7b / bind_memory_backings** (programmatic):
- 违背 JSON 驱动原则 (R-WIRE-1, per ADR-DGPU-12)
- programmatic 桥接是当前架构债务, PNN 不增加

### 2.3 决策 3: 桥状态作用域 — 候选 β (per-connection, P3 引入)

**接受 per-connection 作用域 (候选 β)**, 但**P1 阶段先用 per-module 简化**:

| 阶段 | 作用域 | 实现 | 触发 |
|---|---|---|---|
| **P1 (minimal)** | per-module (候选 α 简化) | `ChStreamModuleBase::bridge_` (1 字段, 1 桥 per module) | P1 实施 |
| **P3 (full)** | per-connection (候选 β) | `ConnectionResolver::bridge_per_connection_` (N 字段, 1 桥 per connection) | P3 实施 |

**为什么 P1 简化**:
- P1 阶段 `network.mode` 是 SoC-wide (整个 SoC 1 个 mode), per-module 足够
- P1 实施复杂度最小 (新增 1 字段 + Step 7b 1 步)
- P3 阶段 per-connection 异构 bridge 是真实需求 (per `cpptlm-dgpu-soc-physical-ip-port-boundary` design.md 5 wire 分层), 但**不在 P1 范围**

**为什么 P3 per-connection**:
- Gem5 NetworkInterface 是 per-controller (per-module) — 对应 P1 per-module
- 真实 SoC 异构场景: SDMA 用 NoCFlit, PCIe EP 用 PcieTlp, **同 module 不同 connection 不同 bridge** — 对应 P3 per-connection
- per-topology (候选 γ) 粒度太粗, 无法支持异构

**Seam 演进路径** (P1 → P3):
```
P1: ChStreamModuleBase::bridge_  (1 字段)
        ↓
P3: ChStreamModuleBase::bridges_  (vector<N>, per-connection)
        ↓
P3: ConnectionResolver::bridge_per_connection_  (全局索引, 支持 JSON `connections[].bridge` 字段)
```

**契约**: P1 → P3 阶段, **每个 P 独立可逆** (per ADR-DGPU-13 决策 6)。

### 2.4 决策 4: 性能建模 — 候选 iii (参数化延迟)

**接受候选 iii 方案** (P1 引入 `bridge_latency_cycles` 参数化):

```cpp
// include/framework/protocol_bridge.hh (P1 模板基类)
template <typename InB, typename OutB>
class ProtocolBridge {
public:
    // ... pack/unpack 接口 ...

    // === PNN 性能建模参数 (P1 引入) ===
    void set_bridge_latency(uint32_t cycles) { bridge_latency_cycles_ = cycles; }
    uint32_t bridge_latency() const { return bridge_latency_cycles_; }

    struct Stats {
        uint64_t pack_cycles = 0;
        uint64_t unpack_cycles = 0;
        uint64_t unpack_wait_cycles = 0;
        uint64_t conversions = 0;
    };
    const Stats& stats() const { return stats_; }

private:
    uint32_t bridge_latency_cycles_ = 0;  // 0 = 零开销 (Identity)
    Stats stats_;
};
```

**为什么候选 iii**:
- **P0 abstract 模式**: `bridge_latency_cycles_ = 0` 编译期 if 移除, 零开销
- **P2 具体协议模式**: `bridge_latency_cycles_ = 2` (典型 pack/unpack 2 cycles), 可调
- **Ruby 对照 <20% 误差**: 参数化延迟是误差校正关键
- **JSON 注入**: `"network": { "axi4": { "bridge_latency_cycles": 2 } }` (P1 接受, P2 启用)

**为什么拒绝候选 i** (零开销 hard-coded):
- abstract 模式零开销 OK, 具体协议模式需要延迟建模
- 不可扩展

**为什么拒绝候选 ii** (1-cycle 固定):
- 不可调, 误差校正无手段

### 2.5 决策 5: 桥注册机制 — `ProtocolBridgeFactory` (单注册表)

**接受单注册表方案** (per `ProtocolBridgeFactory`, 不破既有双注册表):

```cpp
// include/framework/protocol_bridge_factory.hh (P1 新增, ~50 行)
namespace cpptlm {

class ProtocolBridgeFactory {
public:
    static ProtocolBridgeFactory& get();

    template <typename BridgeT>
    void registerBridge(const std::string& mode_name) {
        creators_[mode_name] = []() {
            return std::make_unique<BridgeT>();
        };
    }

    bool knows(const std::string& mode_name) const;
    std::unique_ptr<ProtocolBridgeBase> create(const std::string& mode_name);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<ProtocolBridgeBase>()>> creators_;
};

}  // namespace cpptlm
```

**为什么独立注册表**:
- **不破双注册表**: SimObject (object) vs SimModule (module) 双注册表保护类型安全, PNN 桥独立
- **关注点分离**: bridge 不需要 SimObject/SimModule 派生, 无 tick(), 无 EQ 关联
- **P1 注册宏**: `REGISTER_CHSTREAM_PROTOCOL_BRIDGE(mode, BridgeT)` (per 蓝图 §2.5)

### 2.6 决策 6: 桥与 ModuleFactory 错误处理

**接受 `network.mode` 未知时抛异常方案**:

```cpp
// src/core/module_factory.cc (Step 7b)
if (!bridge_factory.knows(mode)) {
    throw std::runtime_error("Unknown network.mode: " + mode +
                             " (registered modes: " + ... + ")");
}
```

**为什么抛异常**:
- 与既有 `ModuleFactory::instantiateAll` 错误处理风格一致 (per `module_factory.cc:142-866`)
- 早期失败: 仿真启动前明确错误, 不是运行时静默 fallback
- 调试友好: 异常消息包含已知 modes 列表

**为什么拒绝静默 fallback**:
- 用户期望 `network.mode` 切换生效, 静默 fallback 致困惑
- 仿真结果不可预测 (protocol 不同 → latency 不同)

### 2.7 决策 7: ChStreamModuleBase 新增 API (P1 增量)

**接受 ChStreamModuleBase 新增 2 方法** (P1 增量, +20 行):

```cpp
// include/core/chstream_module.hh (P1 增量)
class ChStreamModuleBase : public SimObject {
public:
    // ... 既有 API ...

    // === PNN 增量 ===
    virtual void set_protocol_bridge(cpptlm::ProtocolBridgeBase* bridge) {
        bridge_ = bridge;
    }
    cpptlm::ProtocolBridgeBase* protocol_bridge() const { return bridge_; }

private:
    cpptlm::ProtocolBridgeBase* bridge_ = nullptr;
};
```

**为什么仅 2 方法**:
- 最小侵入: 既有 8 步装配 (per blueprint §1) 不动
- 与 `set_stream_adapter()` 风格一致 (P0 既有, `include/core/chstream_module.hh:30-50`)
- per-connection 演进 (P3) 时, 扩展为 `set_protocol_bridges(vector)` 而非 1 字段

### 2.8 决策 8: 测试隔离 — `[pnn]` 标签 + golden 配置 per 协议

**接受 PNN 测试隔离方案**:

| 测试标签 | 含义 | 数量 |
|---|---|---|
| `[pnn]` | 通用 PNN 桥接测试 | ≥15 cases (P1) |
| `[pnn-baseline]` | P0 baseline 2×2 mesh 性能评估 | ≥2 cases |
| `[pnn-abstract]` | P1 abstract bridge 单元测试 | ≥10 cases |
| `[pnn-axi4]` | P2 AXI bridge 单元 + 集成 | ≥20 cases |
| `[pnn-chi]` | P2 CHI bridge 单元 + 集成 | ≥20 cases |
| `[pnn-pcie]` | P3 PCIe TLP bridge | ≥15 cases |
| `[pnn-multi]` | P3 多协议共存 | ≥10 cases |
| `[pnn-cxl]` | P4 CXL bridge | ≥15 cases |

**golden 配置约定**:
- 每协议 1 个 golden 配置: `configs/examples/pnn_mesh_2x2_<mode>.json`
- baseline 配置 (P0) 跨协议共享
- `[pnn-<mode>]` 标签隔离

**6 标签 0 回归**: 新增类 + Step 7b 增量, 既有 Step 1-7 路径不动, `[pcie]` / `[realip]` / `[pcie-memory]` / `[minimal_dgpu_soc]` / `[dgpu_soc_timing]` / `[chstream]` 不破

---

## 3. PNN 实施状态表 (跨决策 1-8)

| 阶段 | Seam 位置 | 注入时机 | 作用域 | 性能建模 | 触发 |
|---|---|---|---|---|---|
| P0 | (既有) | (既有) | (无 bridge) | (无) | 起草本 ADR + 蓝图评审 |
| P1 | Module↔Bridge↔Adapter | Step 7b | per-module | 参数化延迟 | 1 KPI slot 释放 |
| P2 | (不变) | (不变) | (不变) | (启用) | P1 通过 |
| P3 | (不变) | (不变) | per-connection | (启用) | P2 通过 |
| P4 | (不变) | (不变) | (不变) | (启用) | P3 通过 |

---

## 4. 不在范围 (Non-Goals)

1. **修改 SimplePort 序列化** — 违背 frozen headers, PNN 仅在 Module↔Adapter 之间插入
2. **修改 StreamAdapter 模板签名** — 既有 `StreamAdapter<M, Req, Resp>` 改 `StreamAdapter<M, BridgeIn, BridgeOut>`, 但 `BridgeIn` 编译期可推断, 调用方零改动
3. **运行时协议热切换** — Carbon 文档明示不可逆, PNN 仅支持离线切换
4. **DSL 协议编译链** — 拒绝引入, 保留 C++ 直接性
5. **cycle-accurate 5-stage router** — P3 候选, 不在 P1 范围
6. **新注册表分类** — PNN 用 `ProtocolBridgeFactory` 单独管理, 不破双注册表
7. **修改 ChStreamTargetPort / ChStreamInitiatorPort** — 既有 Port 路径不动, PNN 仅在 Module 侧新增 `set_protocol_bridge()`

---

## 5. 风险与失败模式

### 5.1 高风险: Seam 位置候选 A 与既有 StreamAdapter 模板签名冲突

**症状**: `StreamAdapter<M, Req, Resp>` 中 `Req` 是 `CacheReqBundle` 等具体 Bundle, PNN 改为 `Bridge::InB = NoCFlitBundle` 后, 编译错误

**缓解**:
- **P1 模板参数重命名**: `StreamAdapter<M, BridgeInB, BridgeOutB>` (而非 `Req`, `Resp`), 语义清晰
- **P0 baseline 验证**: P0 阶段用现有 NICTLM + RouterTLM + NoCFlitBundle 跑 baseline, 验证 NoCFlitBundle 编译期可作为 BundleT 参数
- **P1 编译期 if 移除**: Identity pack/unpack 用 `if constexpr (std::is_same_v<InB, OutB>)` 优化

### 5.2 中风险: per-module 简化限制 P3 异构协议

**症状**: P1 per-module bridge 假设整个 module 1 个 protocol, P3 真实异构场景 (SDMA NoCFlit + PCIe EP PcieTlp) 需要 per-connection

**缓解**:
- **P1 实施时, `bridge_` 字段预留 `vector<BridgeBase*>` 接口**: P1 用 1 元素, P3 扩 N 元素
- **P3 显式扩展**: `ChStreamModuleBase::bridges_` (vector) 替代 `bridge_` (1 字段)
- **API 演进路径**: `set_protocol_bridge(bridge)` (P1) → `set_protocol_bridges(vector)` (P3) — 接口兼容

### 5.3 中风险: bridge 状态泄漏 (OOO rid 表)

**症状**: `AxiProtocolBridge::outstanding_rid_` 在 idle drain 前切换 protocol 致状态守恒失败

**缓解**:
- **契约规定**: 协议切换仅在 idle drain 后 (per ADR-DGPU-13 决策 7)
- **`ModuleFactory` Step 7b 检查**: `if (system_has_pending_transactions()) throw_error`
- **bridge::reset() API**: P1 引入 `virtual void reset() { stats_ = {}; }`, protocol 切换前调用

### 5.4 低风险: bridge_latency_cycles 误用

**症状**: 用户设 `bridge_latency_cycles = 0` 期望"零开销"但实际 AXI bridge 仍有时延

**缓解**:
- **文档明示**: `bridge_latency_cycles = 0` 仅对 abstract mode 有效
- **P2 单元测试**: AXI bridge `bridge_latency_cycles = 2` 是固定值, 不可覆盖
- **JSON schema 校验**: `bridge_latency_cycles` 范围 0-100, 异常时抛错

### 5.5 低风险: ProtocolBridgeFactory 命名冲突

**症状**: `ProtocolBridgeFactory` 与既有 `ChStreamAdapterFactory` 命名不一致

**缓解**:
- 命名空间 `cpptlm::ProtocolBridgeFactory` 显式区分
- 既有 `cpptlm::ChStreamAdapterFactory` 不动
- P1 实施时, 跨注册表测试覆盖

---

## 6. 自检 10 条

| # | 自检项 | 结论 |
|---|---|---|
| 1 | 0 改 frozen headers | ✅ Seam 在 Module↔Adapter 之间, 不触 frozen |
| 2 | 0 破 23 ABI | ✅ `cpptlm_emulator.h` 零修改 |
| 3 | 6 标签 0 回归 | ✅ Step 7b 增量, 既有 Step 1-7 路径不动 |
| 4 | KPI P0=0 / P1-P4 各 1 顺序消耗 | ✅ 走 doc-only 先例 + 顺序消耗 |
| 5 | 复用既有测试套件 | ✅ `[chstream]` + `[noc]` 既有 + 新增 `[pnn]` |
| 6 | 避免过度设计 | ✅ P1 per-module 简化, P3 扩 per-connection |
| 7 | 无外部依赖 | ✅ 纯 C++ 模板 + JSON |
| 8 | P0 可 1 周启动 | ✅ 仅 doc + 现有能力组合 (无新代码) |
| 9 | 文档完整 | ✅ 本 ADR + 蓝图 + gem5 调研 + 配套 ADR-13 + OpenSpec proposal (P0 起草) |
| 10 | 可逆 | ✅ 每 P 独立 OpenSpec change, archive 即回滚 |

---

## 7. 关联文档

### 7.1 关联 (起草对象)

- [`../designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md) — PNN 架构蓝图 v0.1
- [`../../research/gem5-ruby-protocol-bridge-survey.md`](../../research/gem5-ruby-protocol-bridge-survey.md) — Gem5 Ruby 调研 v0.1
- [ADR-DGPU-13](ADR-DGPU-13-pnn-protocol-neutral-network.md) — PNN 范围定义 (同 cycle 起草)

### 7.2 现有 ADR 关联 (沿用)

- [ADR-DGPU-06](ADR-DGPU-06-axi-mem-bundle-boundary.md) — chip-internal AXI vs board-level PCIe 边界
- [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md) — 23 ABI 冻结 + 0 新增
- [ADR-DGPU-11](ADR-DGPU-11-timing-mode-soc-scope.md) — timing-mode SoC scope
- [ADR-DGPU-12](ADR-DGPU-12-soc-internal-tlm-wiring-principle.md) — SoC 内部 TLM 接线原则 (R-WIRE-1/2/3/4/5)

### 7.3 待起草 OpenSpec

- `openspec/changes/cpptlm-pnn-protocol-network/proposal.md` — P0 doc-only (P0 启动时起草)
- `openspec/changes/cpptlm-pnn-protocol-bridge/proposal.md` — P1 (待 KPI slot 释放)

---

## 8. 状态更新

> **2026-10-12 起草 (v0.1)**: build agent 起草, 待 Oracle/Metis 复审
> 预期 1-2 轮 Revise 后签发

---

**版本**: v0.1 草案
**最后更新**: 2026-10-12
**签发状态**: 🟡 待评审
**维护者**: CppTLM build agent
