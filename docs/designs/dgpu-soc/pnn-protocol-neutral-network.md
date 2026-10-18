# PNN — Protocol-Neutral Network 协议中立通信网络架构蓝图

> **版本**: v0.1 (初稿，待架构评审)
> **日期**: 2026-10
> **状态**: 🟡 **设计阶段，待评审**（v0.1 由 build agent 起草，预期 1-2 轮 Oracle/Metis 复审）
> **作者**: CppTLM build agent
> **前置调研**: [`docs/research/gem5-ruby-protocol-bridge-survey.md`](../../research/gem5-ruby-protocol-bridge-survey.md)
> **前置架构**: [`docs/designs/dgpu-soc/communication-network.md`](communication-network.md)（v1.0 已落地，L1-L6 分层 + Gem5 类比）
> **配套 ADR（待起草）**: ADR-DGPU-13（PNN 范围定义）+ ADR-DGPU-14（StreamAdapter↔ProtocolBridge seam 决策）
> **目标存档**: `docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`

---

## ⚠️ 评审请求（写给评审者）

本文档是 CppTLM **通信网络架构层面**的关键决策草案，目的是解决用户多轮提出的诉求：

> "在性能评估阶段设计一个通信网络，在后期可以基于这个通信网络再转换成不同的通信协议（比如 AXI/CHI 等等）。**所有我要的是 CppTLM 仿真架构的方案**。"

**本文不承诺**：
- ❌ 不承诺"消除所有协议特化"（PNN 接受"协议必然要特化"的事实）
- ❌ 不承诺"运行时协议切换"（Carbon 文档明示 LT↔CA 不可逆，PNN 沿用编译期）
- ❌ 不承诺"零代码改动切协议"（协议字段映射是真实工程，PNN 提供 framework 但不 magic）

**本文承诺**：
- ✅ **架构形式化**：把 CppTLM 现有的 70% 资产（ChStream + StreamAdapter 5 子类 + NoCFlitBundle + DualPortStreamAdapter）形式化为可演进的 PNN 框架
- ✅ **5 阶段可执行路径**：P0 doc-only / P1 minimal bridge / P2 AXI+CHI / P3 multi-protocol / P4 CXL
- ✅ **零硬边界冲突**：不破 frozen headers / 不破 23 ABI / 不破 6 标签 0 回归
- ✅ **Gem5 Ruby 工作流对照**：基于 20+ 年业界经验，可信度高

**评审重点**：
1. 命名约定（`ProtocolBridge` / `AbstractProtocolBridge` / `AxiProtocolBridge`）是否与现有 API 风格一致？
2. canonical wire 选 `NoCFlitBundle` 16 字段是否够超集？
3. `network.mode` 字符串枚举 vs `enum class` 哪种更好？
4. P0 阶段（doc-only）是否真的能在 1-2 周内完成？
5. 与 `cpptlm-dgpu-soc-physical-ip-port-boundary` / `cpptlm-dgpu-soc-control-hub` 的关系是正交还是冲突？

---

## 0. 摘要

### 0.1 核心立场

**CppTLM 通信网络 = Protocol-Neutral Network (PNN)**，架构目标是：
- **前期（性能评估阶段）**：模块只与 L3 ChStream 抽象交互，使用 canonical wire `NoCFlitBundle`，由 `AbstractProtocolBridge` 做 Identity 转换，零协议特化
- **后期（协议具体化阶段）**：通过 `network.mode` JSON 字段切换，注入对应的 `AxiProtocolBridge` / `ChiProtocolBridge` / `PcieTlpProtocolBridge` / `CxlProtocolBridge`，完成从抽象到具体协议的无缝过渡

### 0.2 设计原则

1. **不重发明 Gem5 Ruby**: CppTLM 已有 70% Gem5 Ruby 资产（ChStream + StreamAdapter 5 子类 + RouterTLM + NoCFlitBundle），PNN 是**形式化 + 补齐 1 个抽象层**而非新建
2. **协议必然要特化**: Bundle 字段命名即字段语义（typed POD），改字段名 = 改模块代码。PNN 不假装"运行时无感切换"
3. **编译期绑定**: 沿用 C++ 模板，不引入运行时反射/类型擦除（保持 `BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable` 断言）
4. **桥内可藏状态**: 协议桥内可有 OOO rid 表 / 状态机（per `axi4_mapper.cc`），**但切换协议必须 idle drain**——不可逆
5. **不破 frozen 边界**: P0-P4 全程 0 改 `pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h`
6. **不破 KPI 上限**: P0 走 doc-only 先例，P1-P4 各 1 slot 顺序消耗
7. **与 simulation_mode 正交**: `network.mode`（协议轴）与 `simulation_mode`（功能/时序轴）独立，2 维组合 = 4 模式

### 0.3 与现有 OpenSpec 状态的关系

| 现有 change | 与 PNN 关系 |
|---|---|
| `cpptlm-dgpu-soc-physical-ip-port-boundary` (Proposed Draft) | **正交**——前者推 SDMA 5→8 端口切型 + NoCFlit 切型，PNN 提供跨协议切型的 bridge 框架 |
| `cpptlm-dgpu-soc-control-hub` (Proposed Draft) | **正交**——前者新建 ControlHub 集中 BAR decode，PNN 不影响 BAR decode 路径 |
| `cpptlm-p2-integration-unblock` (Proposed) | **正交**——P2 集成 4 断点，PNN 提供 L4 接入能力 |
| `cpptlm-gpu-fused-soc-survey` | **正交**——v0.5 redo 已 archive |
| `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 | **依赖**——本方案是 doc-only 类变更，**不消耗 KPI slot**（参考 §6 先例） |

---

## 1. 动机与目标

### 1.1 真实痛点

当前 dGPU SoC 仿真场景下，**两类工作流冲突**：

| 工作流 | 需求 | 当前困境 |
|---|---|---|
| **早期架构探索** | 跑参数扫描（topology / link latency / VC count） | 每次协议字段变化（如 SDMA 5 端口 PcieTlp → NoCFlit）都要改 6+ 文件 + 重新编译 + 跑回归 |
| **后期协议实现** | 切具体协议（AXI4 / CHI / PCIe FC） | 当前无统一机制，依赖 `axi4_mapper.cc` 个例 + PCIe FC 手写在 `pcie_link_layer_tlm.cc` |

**核心矛盾**: 性能评估阶段需要"协议无关"快速迭代；协议实现阶段需要"协议特化"高保真建模。两类工作流的工具链不能互相影响。

### 1.2 Gem5 Ruby 的解法（业界锚点）

Gem5 Ruby 20+ 年经验证明**协议中立网络层是可行的**：
- **MessageBuffer** 是协议中立的消息队列（FIFO），coherence controller 只与它 push/pull
- **SLICC** DSL 编译期烧协议（改 `.slicc` 文件，不动 controller 外壳）
- **Garnet** 是详细 network model，**Simple** network 是简化版——`--network=garnet2.0` / `--network=simple` 切换性能/精度
- **Topology.py** 文件 + 命令行 `--topology=Mesh_XY --mesh-rows=4` 描述任意拓扑
- **CHI 协议**（AMBA 5）已落地，证明从 MESI/MOESI 切到 CHI 是真实工作流

### 1.3 CppTLM 的目标（与 Ruby 1-1 对应）

| Gem5 Ruby | CppTLM PNN 目标 |
|---|---|
| MessageBuffer | `InputStreamAdapter<Bridge::InB>`（既有 60%） |
| SLICC controller | `ProtocolBridge<InB, OutB>` 模板（PNN 新增） |
| NetworkInterface | `ProtocolBridge::pack/unpack` 虚函数（PNN 新增） |
| Simple vs Garnet | `network.mode: abstract` 切换（PNN 新增） |
| Topology.py | `network.topology: mesh` + 既有 JSON `connections[]`（PNN 增量） |
| CHI 协议 | `ChiProtocolBridge`（PNN P2 实施） |
| CXL 协议 | `CxlProtocolBridge`（PNN P4 实施，gem5 仍在 WIP） |

---

## 2. 总体架构

### 2.1 PNN 在 CppTLM 6 层架构中的位置

```
┌── HOST (UE 进程) ───────────────────────────────────────────────────────┐
│ 23 ABI (cpptlm_emulator.h) ── 冻结边界 [ABI BOUNDARY]                    │
└──┬───────────────────────────────────────────────────────────────────────┘
   │ C ABI
┌──▼──────── DGpuBoard (shell) ───────────────────────────────────────────┐
│ 板级门面: BAR0/1/2 + framebuffer + VRAM 真源                            │
│ simulation_mode + network.mode + bind_memory_backings  [MODE SWITCH]   │
└──┬───────────────────────────────────────────────────────────────────────┘
   ▼ [SoC INTERNAL BOUNDARY]
┌──▼──────── SoC 内部 ───────────────────────────────────────────────────┐
│  L_PNN 协议中立通信网络 (本方案)                                          │
│                                                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  抽象层: ProtocolBridge<InB, OutB> 模板                          │   │
│  │  ├─ AbstractProtocolBridge (Pack/Unpack = Identity, 零开销)      │   │
│  │  ├─ AxiProtocolBridge    (Pack: flit→axi, Unpack: axi→flit)    │   │
│  │  ├─ ChiProtocolBridge    (类似, 4 flit_type REQ/RSP/DAT/SNP)   │   │
│  │  ├─ PcieTlpProtocolBridge (类似, 6 kind + credit)               │   │
│  │  └─ CxlProtocolBridge    (复用 PCIe, P4)                        │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│  ↑↓ pack/unpack                                                          │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  canonical wire: NoCFlitBundle (16 字段)                          │   │
│  │  src/dst_node + address + size + vc_id + flit_type/idx/count     │   │
│  │  + hops + ...                                                    │   │
│  │  → PNN 唯一规范线格式,任何协议字段映射以此为中转                 │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│  ↑↓ bundle 序列化                                                        │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  拓扑层: NICTLM + RouterTLM + BidirectionalPortAdapter (5 端口)   │   │
│  │  + GpuInternalFabric + CrossbarTLM                                │   │
│  │  → 复用既有 (L4 StreamAdapter 5 子类)                             │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│  ↑↓ valid/ready handshake                                                │
│  L3 ChStream 抽象 (InputStreamAdapter<Bridge::InB> / OutputStreamAdapter)│
│  L2 MasterPort/SlavePort/SimplePort/Packet                              │
│  L1 tlm::tlm_generic_payload                                             │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 `ProtocolBridge` 核心设计

```cpp
// include/framework/protocol_bridge.hh (P1 实施, 模板基类)
namespace cpptlm {

template <typename InBundleT, typename OutBundleT>
class ProtocolBridge {
public:
    virtual ~ProtocolBridge() = default;

    // === 核心: 协议转换 ===

    // 从 PNN 抽象线格式 (NoCFlitBundle) 打包到目标协议线格式
    virtual void pack(const InBundleT& in, OutBundleT& out) = 0;

    // 从目标协议线格式解包到 PNN 抽象线格式
    virtual void unpack(const OutBundleT& in, InBundleT& out) = 0;

    // === 配置: 每个 bridge 实例化时调用一次 ===
    virtual void configure(const json& cfg) {}

    // === 性能指标: bridge 内部计数器 (P1 引入) ===
    struct Stats {
        uint64_t pack_cycles = 0;
        uint64_t unpack_cycles = 0;
        uint64_t unpack_wait_cycles = 0;  // OOO 等 outstanding 时
        uint64_t conversions = 0;
    };
    Stats stats_;
    const Stats& stats() const { return stats_; }
};

}  // namespace cpptlm
```

### 2.3 `AbstractProtocolBridge`（P1 实施，基线）

```cpp
// include/framework/abstract_protocol_bridge.hh (P1 实施)
namespace cpptlm {

// 性能评估阶段用: Pack = Identity, Unpack = Identity
// 零协议特化, 零开销
template <typename FlitT>
class AbstractProtocolBridge : public ProtocolBridge<FlitT, FlitT> {
public:
    void pack(const FlitT& in, FlitT& out) override {
        out = in;  // Identity
    }
    void unpack(const FlitT& in, FlitT& out) override {
        out = in;  // Identity
    }
};

}  // namespace cpptlm
```

### 2.4 `AxiProtocolBridge`（P2 实施，具体协议示例）

```cpp
// include/framework/axi_protocol_bridge.hh (P2 实施, ~80 行)
namespace cpptlm {

// Pack: NoCFlitBundle → Axi4Bundle (AXI4 wire-format)
// Unpack: Axi4Bundle → NoCFlitBundle (含 OOO rid 关联, 复用 axi4_mapper.cc 逻辑)
class AxiProtocolBridge : public ProtocolBridge<NoCFlitBundle, Axi4Bundle> {
private:
    // 复用 axi4_mapper.cc 已有 OOO rid 关联
    std::unordered_map<uint8_t, std::queue<Axi4Bundle>> outstanding_rid_;
public:
    void pack(const NoCFlitBundle& in, Axi4Bundle& out) override {
        // flit → AXI 字段映射
        out.addr = in.address;
        out.data = /*... 32-byte data slice ...*/;
        out.id   = in.src_node;  // 用 src_node 作为 AXI rid
        // ... 完整映射见 P2 design.md
    }
    void unpack(const Axi4Bundle& in, NoCFlitBundle& out) override {
        // OOO 关联 (复用 axi4_mapper.cc)
        outstanding_rid_[in.id].push(in);
        if (outstanding_rid_[in.id].size() >= expected_count) {
            // 组装回 NoCFlitBundle
        }
    }
};

}  // namespace cpptlm
```

### 2.5 注册机制

```cpp
// include/framework/chstream_register.hh (P1 增量, +10 行)
namespace cpptlm {

// PNN 新增: 协议桥注册
template <typename BridgeT>
void register_protocol_bridge(const std::string& mode_name) {
    ProtocolBridgeFactory::get().registerBridge<BridgeT>(mode_name);
}

#define REGISTER_CHSTREAM_PROTOCOL_BRIDGE(mode_name, BridgeT) \
    const bool _reg_pbridge_##BridgeT = \
        (cpptlm::register_protocol_bridge<BridgeT>(mode_name), true)

}  // namespace cpptlm
```

### 2.6 ModuleFactory Step 7 消费 `network.mode`

```cpp
// src/core/module_factory.cc (P1 增量, +15 行)
void ModuleFactory::instantiateAll(const json& config) {
    // ... 既有 Step 1-6 ...

    // === Step 7a: ChStreamModuleBase 注入 StreamAdapter (既有) ===
    // ...

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
            ch_mod->set_protocol_bridge(bridge);  // PNN 新增 ChStreamModuleBase API
        }
    }
}
```

### 2.7 `ChStreamModuleBase` PNN 扩展

```cpp
// include/core/chstream_module.hh (P1 增量, +20 行)
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

---

## 3. 接口契约

### 3.1 模块接入（不变）

```cpp
class MyModule : public ChStreamModuleBase {
    InputStreamAdapter<MyReqBundle>  req_in_;
    OutputStreamAdapter<MyRespBundle> resp_out_;
public:
    void tick() override {
        if (req_in_.valid() && req_in_.ready()) {
            // ... 业务逻辑 (不感知 PNN 桥) ...
            resp_out_.write(resp);
            req_in_.consume();
        }
    }
};
REGISTER_CHSTREAM(MyModule)
```

**关键不变量**: 模块代码**完全不知道** PNN 桥的存在——桥由 `ModuleFactory` Step 7b 自动注入。

### 3.2 JSON 配置契约

**P0 性能评估**（pure abstract）:
```json
{
  "network": {
    "mode": "abstract",
    "topology": "mesh",
    "dims": [2, 2],
    "link_latency_cycles": 10,
    "link_bw_bytes_per_cycle": 8,
    "vc_count": 4,
    "routing": "xy"
  },
  "modules": [...],
  "connections": [...]
}
```

**P2 切 AXI**（仅改 `mode`，modules/connections 零 diff）:
```json
{
  "network": {
    "mode": "axi4",
    "topology": "mesh",
    "dims": [2, 2],
    "link_latency_cycles": 10,
    "link_bw_bytes_per_cycle": 8,
    "vc_count": 4,
    "routing": "xy",
    "axi4": { "outstanding": 16, "data_width": 64, "wstrb": true }
  },
  "modules": [...同 abstract...],
  "connections": [...同 abstract...]
}
```

### 3.3 协议桥契约（伪代码）

```cpp
class MyAxiBridge : public cpptlm::ProtocolBridge<NoCFlitBundle, MyAxiBundle> {
    // 必实现 4 个虚函数
    void pack(const NoCFlitBundle& in, MyAxiBundle& out) override;
    void unpack(const MyAxiBundle& in, NoCFlitBundle& out) override;
    void configure(const json& cfg) override;  // 可选
    Stats stats() const override;  // 性能指标 (可选)
};
```

---

## 4. 配置 Schema

### 4.1 `network.mode` 枚举

| Mode | 对应 bridge | 阶段 | 估时 |
|---|---|---|---|
| `"abstract"` | `AbstractProtocolBridge<NoCFlitBundle>` | P1 | 0 (基线) |
| `"axi4"` | `AxiProtocolBridge` | P2 | 2-3d |
| `"chi"` | `ChiProtocolBridge` | P2 | 2-3d |
| `"pcie_tlp"` | `PcieTlpProtocolBridge` | P3 | 1-2d |
| `"cxl"` | `CxlProtocolBridge` | P4 | 1-2d |

**字符串 vs enum class 决策**: 选**字符串**——便于扩展新模式（不必重新编译枚举类），与 `simulation_mode: functional|timing` 风格一致（`dgpu_soc_timing_v1.json:4`）。

### 4.2 `network` 完整字段

```json
{
  "network": {
    "mode": "abstract",
    "topology": "mesh",                    // mesh | crossbar | ring | butterfly
    "dims": [2, 2],                        // topology 维度
    "link_latency_cycles": 10,             // 每条 link 延迟
    "link_bw_bytes_per_cycle": 8,          // 每条 link 带宽
    "vc_count": 4,                         // virtual channel 数
    "routing": "xy",                       // xy | west_first | table | custom

    "axi4": { /* P2: AxiProtocolBridge 特定配置 */ },
    "chi":  { /* P2: ChiProtocolBridge 特定配置 */ },
    "pcie_tlp": { /* P3: PcieTlpProtocolBridge 特定配置 */ },
    "cxl":  { /* P4: CxlProtocolBridge 特定配置 */ }
  }
}
```

**注意**: `network.topology` 字段是 P1 增量（拓扑参数化），P0 阶段仅消费 `network.mode`。

---

## 5. 实施分阶段（详细）

### 5.1 P0 (1-2 周, 0 KPI, doc-only)

**目标**: 用现有能力组合，验证 PNN 概念，跑 2×2 mesh baseline

**任务清单**:
1. 起草 `cpptlm-pnn-protocol-network` OpenSpec proposal（doc-only, 引用本蓝图 + gem5 调研）
2. 写 `configs/examples/pnn_mesh_2x2_baseline.json`:
   - 2×2 mesh: 4 个 RouterTLM + 2 个 TrafficGenTLM
   - 内部 connections: src→router0→router1→dst
   - 不含 `network.mode` 字段（用现有 NoCFlitBundle 走 ChStream）
3. 跑 baseline 测试:
   - `[pnn-baseline]` ≥2 cases / 10 assertions
   - 复用 `[chstream]` + `[noc]` 标签
4. 产出 baseline 报告（latency/throughput 数字）作为 P1/P2 验证锚点

**触发条件**: 用户批准 PNN 架构方案

**不动边界**: 全部

**估时**: 5d

**测试指标**: 6 标签 0 回归 + `[pnn-baseline]` ≥2 cases 通过

### 5.2 P1 (1-2 月, 1 KPI slot, `cpptlm-pnn-protocol-bridge`)

**目标**: 实施 `ProtocolBridge` 模板 + `AbstractProtocolBridge` + `network.mode` 解析

**任务清单**:
1. 新建 `include/framework/protocol_bridge.hh`（~50 行, 模板基类 + Stats）
2. 新建 `include/framework/abstract_protocol_bridge.hh`（~30 行, Identity pack/unpack）
3. 新建 `src/framework/abstract_protocol_bridge.cc`（~30 行）
4. 增量 `include/framework/chstream_register.hh`（+10 行, `REGISTER_CHSTREAM_PROTOCOL_BRIDGE` 宏）
5. 增量 `include/core/chstream_module.hh`（+20 行, `set_protocol_bridge()` API）
6. 增量 `src/core/module_factory.cc`（+15 行, Step 7b 注入逻辑）
7. 新建 `include/framework/protocol_bridge_factory.hh`（~50 行, 模式名 → bridge 类注册表）
8. 新建 `src/framework/protocol_bridge_factory.cc`（~30 行）
9. 写测试 `test/test_protocol_bridge_abstract.cc`（~150 行, ≥15 cases / 100 assertions）

**触发条件**: P0 baseline 通过 + KPI slot 释放

**不动边界**: 全部 frozen headers / 23 ABI

**估时**: 10-15d

**测试指标**: `[pnn]` ≥15 cases, 6 标签 0 回归, P0 baseline 数字复现

### 5.3 P2 (3-6 月, 1 KPI slot, `cpptlm-pnn-axi-chi`)

**目标**: 实施 `AxiProtocolBridge` + `ChiProtocolBridge` + Ruby 对照验证

**任务清单**:
1. 新建 `include/framework/axi_protocol_bridge.hh`（~80 行, 模板 + 字段映射声明）
2. 新建 `src/framework/axi_protocol_bridge.cc`（~200 行, **80% 复用 `axi4_mapper.cc` 逻辑**）
3. 新建 `include/framework/chi_protocol_bridge.hh`（~80 行, 4 flit_type 模板）
4. 新建 `src/framework/chi_protocol_bridge.cc`（~200 行, REQ/RSP/DAT/SNP 4 类）
5. 写 `configs/examples/pnn_mesh_2x2_axi4.json`（同 P0 拓扑, 仅 `mode: "axi4"`）
6. 写 `configs/examples/pnn_mesh_2x2_chi.json`（同 P0 拓扑, 仅 `mode: "chi"`）
7. 写测试 `test/test_protocol_bridge_axi4.cc`（~200 行, ≥20 cases / 150 assertions）
8. 写测试 `test/test_protocol_bridge_chi.cc`（~200 行, ≥20 cases / 150 assertions）
9. Ruby 对照验证报告: 同拓扑同负载，latency 误差 <20% 验收线
10. PNN 蓝图文档增量更新（v0.1 → v0.2 反映 P2 实施）

**触发条件**: P1 通过 + Ruby 对照验证接受

**不动边界**: 全部 frozen headers / 23 ABI / P1 已注册的所有 bridge

**估时**: 20-30d

**测试指标**: `[pnn-axi4]` ≥20 cases + `[pnn-chi]` ≥20 cases, 6 标签 0 回归, Ruby 对照 <20% 误差

### 5.4 P3 (6-12 月, 1 KPI slot, `cpptlm-pnn-multi-protocol`)

**目标**: 多协议共存 + PcieTlpProtocolBridge + 指标 dashboard 集成

**任务清单**:
1. 新建 `include/framework/pcie_tlp_protocol_bridge.hh`（~80 行, 6 kind + credit）
2. 新建 `src/framework/pcie_tlp_protocol_bridge.cc`（~200 行）
3. 增量 `ModuleFactory` Step 7b 支持 per-connection 异构 bridge（`connections[].bridge` 字段）
4. 写测试 `test/test_protocol_bridge_pcie_tlp.cc`（~200 行, ≥15 cases）
5. 写测试 `test/test_multi_protocol_coexistence.cc`（~200 行, ≥10 cases）
6. 指标 dashboard 集成（与 `include/metrics/reporter.hh` 集成）
7. PNN 蓝图文档增量更新（v0.2 → v0.3 反映 P3 实施）

**触发条件**: P2 通过 + 真实多协议消费场景出现

**不动边界**: 全部 frozen headers / 23 ABI

**估时**: 30-45d

**测试指标**: `[pnn-pcie]` ≥15 cases + `[pnn-multi]` ≥10 cases, 6 标签 0 回归

### 5.5 P4 (12+ 月, 1 KPI slot, `cpptlm-pnn-cxl`)

**目标**: CxlProtocolBridge + 跨仓 ArchForge 联合验证

**任务清单**:
1. 新建 `include/framework/cxl_protocol_bridge.hh`（~100 行, CXL.mem/CXL.cache/CXL.io 3 子协议）
2. 新建 `src/framework/cxl_protocol_bridge.cc`（~300 行, 复用 PcieTlp bridge + 加 CXL 特有字段）
3. 跨仓 ArchForge 联合验证（per `docs/soc_arch/README.md` 占位）
4. 写测试 `test/test_protocol_bridge_cxl.cc`（~200 行, ≥15 cases）
5. PNN 蓝图文档增量更新（v0.3 → v1.0 反映 P4 实施 + 跨仓验证）

**触发条件**: P3 通过 + 跨仓 ArchForge CXL 拓扑成熟

**不动边界**: 全部 frozen headers / 23 ABI

**估时**: 30-45d

**测试指标**: `[pnn-cxl]` ≥15 cases, 6 标签 0 回归, ArchForge 联合验证通过

---

## 6. 不破硬边界自检

| 边界 | PNN 触碰? | 说明 |
|---|---|---|
| **`pcie_endpoint_tlm.h`** | ❌ | ProtocolBridge 在 `include/framework/`, 与 frozen pcie_endpoint_tlm 独立 |
| **`pcie_display_device.hh`** | ❌ | 同上 |
| **`pcie_bundles_tlm.hh`** | ❌ | PNN 不修改既有 Bundle, 仅新增 `protocol_bridge.hh` |
| **`cpptlm_emulator.h`** | ❌ | PNN 0 改 23 ABI 签名 |
| **23 ABI 冻结** | ❌ | `cpptlm_emulator_create/mmio_read/...` 零修改 |
| **双注册表** | ❌ | PNN 不新增注册表, 用 `ProtocolBridgeFactory` 单独管理 |
| **KPI 上限** | ✅ 合规 | P0 doc-only (per `cpptlm-dgpu-soc-arch-truth-cleanup` §6 先例), P1-P4 各 1 slot 顺序消耗 |
| **6 标签 0 回归** | ✅ 承诺 | 新增类, 桥接仅在 Step 7b 注入, 不影响既有 Step 1-7 路径 |

---

## 7. 风险与失败模式

### 7.1 高风险：NoCFlitBundle 16 字段超集假设失败

**症状**: CHI snoop / PCIe posted-write 语义在 16 字段中无法表达 → P2 实施时发现需要扩展字段

**缓解**:
- P1 阶段先跑 abstract bridge baseline，验证 16 字段对 simple traffic 足够
- P2 实施 AXI bridge 时验证 AXI 协议字段能完整映射到 16 字段
- 若不满足：P2 增量扩 NoCFlitBundle 字段（**仍不动 frozen**，因为 NoCFlitBundle 不在 frozen 列表）

### 7.2 高风险：bridge pack/unpack 时延漏计

**症状**: abstract 模式 latency=0，但具体协议有 pack/unpack 开销，导致 latency 偏差 >20%

**缓解**:
- P2 立 Ruby 对照 <20% 误差验收线
- bridge::Stats 暴露 `pack_cycles` / `unpack_cycles` 计数器
- 若偏差 >20%: 增加 bridge latency 参数化（`bridge_latency_cycles: 2` 等）

### 7.3 中风险：协议切换不可逆

**症状**: bridge 内藏 OOO rid 表 / state machine，切换 mode 后状态守恒失败

**缓解**:
- 契约规定：**协议切换仅在 idle drain 后**（所有 outstanding 事务完成后）
- `ModuleFactory` Step 7b 切换前检查：`if (system_has_pending_transactions()) throw_error`
- 文档明示：**PNN 协议切换是离线操作，不是热切换**

### 7.4 中风险：测试矩阵爆炸

**症状**: N 协议 × 2 模式 → N×2 配置 + N×2 测试

**缓解**:
- 每协议只保 1 golden 配置（`pnn_mesh_2x2_<mode>.json`）
- `[pnn-<mode>]` 标签隔离
- baseline 配置（P0）跨协议共享
- 实施期先做 protocol 桥的单元测试（不依赖完整 topology）

### 7.5 低风险：KPI 失控

**症状**: P1-P4 各 1 slot，但 OpenSpec 季度复审可能 archive 现有 change，释放 slot 速度不均

**缓解**:
- P0 走 doc-only 先例（per `cpptlm-dgpu-soc-arch-truth-cleanup` §6）
- P1-P4 顺序消耗，单一 P 未完成时不启动下一 P
- 季度复审触发时优先 archive 已实施完毕的 change

### 7.6 低风险：DSL 编译链诱惑

**症状**: 实施 P2/P3 时，发现协议字段映射复杂，开发者提议"像 SLICC 一样写 DSL"

**缓解**:
- **明确拒绝 DSL 诱惑**（保留 C++ 直接性，参考 `cpptlm-dgpu-soc-arch-truth-cleanup` §6 不破坏 C++ 栈的先例）
- 桥类用 C++ 模板实现，开发成本 < DSL 编译链维护成本
- 任何"协议切换魔术"的设计诱惑都拒绝（PNN 是 framework, 不是 magic）

---

## 8. 命名约定（最终版）

| 名称 | 路径 | 命名风格 | 理由 |
|---|---|---|---|
| 方案代号 | **PNN** | 全大写缩写 | 业界惯例（Ruby / CHI / MSI） |
| 基类 | `cpptlm::ProtocolBridge<InB, OutB>` | CamelCase + 模板 | 与 `StreamAdapter<M, Req, Resp>` 一致 |
| 抽象实例 | `cpptlm::AbstractProtocolBridge<FlitT>` | CamelCase + 前缀 | 与 `AbstractMemory` (gem5) 对齐 |
| AXI 实例 | `cpptlm::AxiProtocolBridge` | CamelCase + 前缀 | 与 `Axi4StreamAdapter` 一致 |
| CHI 实例 | `cpptlm::ChiProtocolBridge` | CamelCase + 前缀 | 与 `AxiProtocolBridge` 对称 |
| PCIe 实例 | `cpptlm::PcieTlpProtocolBridge` | CamelCase + 前缀 | 完整名（避免与 `PcieEndpointIP` 混淆） |
| CXL 实例 | `cpptlm::CxlProtocolBridge` | CamelCase + 前缀 | |
| 工厂 | `cpptlm::ProtocolBridgeFactory` | CamelCase + 后缀 | 与 `ChStreamAdapterFactory` 一致 |
| canonical Bundle | `bundles::NoCFlitBundle` | **不重命名** | 既有, 0 回归 |
| JSON 字段 | `network.mode / network.topology / ...` | snake_case + 嵌套 | 与 `simulation_mode` 一致 |
| 注册宏 | `REGISTER_CHSTREAM_PROTOCOL_BRIDGE(mode, BridgeT)` | UPPER_SNAKE | 与 `REGISTER_CHSTREAM` 一致 |
| 测试标签 | `[pnn]`, `[pnn-baseline]`, `[pnn-axi4]`, `[pnn-chi]` | 标签 + 后缀 | 与 `[realip]`, `[noc]` 风格一致 |
| ADR 编号 | `ADR-DGPU-13`, `ADR-DGPU-14` | 数字递增 | 与 `ADR-DGPU-01~12` 一致 |

---

## 9. 评审清单（10 条）

| # | 自检项 | 结论 |
|---|---|---|
| 1 | 0 改 frozen headers | ✅ ProtocolBridge 在 `include/framework/`，不动 frozen |
| 2 | 0 破 23 ABI | ✅ `cpptlm_emulator.h` 零修改 |
| 3 | 6 标签 0 回归 | ✅ 新增类, Step 7b 增量, 既有 Step 1-7 路径不动 |
| 4 | KPI P0=0 / P1-P4 各 1 顺序消耗 | ✅ 走 doc-only 先例 + 顺序消耗 |
| 5 | 复用既有测试套件 | ✅ P0 复用 `[chstream]` + `[noc]`, P1+ 新增 `[pnn]` |
| 6 | 避免过度设计 | ✅ P1 只做 abstract bridge, AXI 推 P2, CHI 推 P2 |
| 7 | 无外部依赖 | ✅ 纯 C++ 模板 + JSON |
| 8 | P0 可 1 周启动 | ✅ 仅 doc + 现有能力组合（无新代码） |
| 9 | 文档完整 | ✅ 本蓝图 + gem5 调研 + 配套 2 ADR + OpenSpec proposal (P0 起草) |
| 10 | 可逆 | ✅ 每 P 独立 OpenSpec change, archive 即回滚 |

---

## 10. 评审问题清单（给 Oracle/Metis/团队）

### 10.1 命名 / API 问题

1. **`ProtocolBridge<InB, OutB>` 模板签名是否够泛？** 考虑加 `StateT` 模板参数（OOO 状态机）？
2. **`AbstractProtocolBridge<FlitT>` 是否需要显式 `NoCFlitBridge` 派生？** 还是直接 `ProtocolBridge<NoCFlitBundle, NoCFlitBundle>` 够用？
3. **`network.mode` 字符串 vs `enum class` 哪种更好？** 字符串便于扩展，enum 类型安全
4. **per-connection 异构 bridge 是否过早设计？** P3 引入是否合理？
5. **`set_protocol_bridge()` API 是 `ChStreamModuleBase` 新增方法**——是否破坏既有 8 步装配？

### 10.2 协议映射问题

1. **NoCFlitBundle 16 字段**（src/dst_node + address + size + vc_id + flit_type/idx/count + hops + ...）**是否覆盖 AXI 必填字段？**
2. **CHI 4 flit_type (REQ/RSP/DAT/SNP) 映射到 `flit_type` 字段是否清晰？**
3. **PCIe FC credit 字段**（`credit_P/NP/Cpl`）**如何在 NoCFlitBundle 表达？** 是新增字段还是 PCI bridge 内部维护？
4. **CXL 3 子协议**（CXL.mem / CXL.cache / CXL.io）**是否需要 3 个 bridge 类，还是 1 个 bridge 多 mode？**

### 10.3 实施 / KPI 问题

1. **P0 阶段 1-2 周**是否能在不消耗 KPI 情况下完成？参考 `cpptlm-dgpu-soc-arch-truth-cleanup` §6 先例
2. **P1 阶段 1-2 月 + 1 KPI slot** 估时是否合理？参考 `cpptlm-vram-controller-functional-integration` 实际实施速度
3. **Ruby 对照 <20% 误差**验收线是否合理？gem5 Garnet 2.0 vs Simple Network 实测误差是多少？
4. **跨仓 ArchForge 联合验证**（P4）是否必要？ 还是 P3 阶段 PcieTlp 验证后即收尾？

### 10.4 关系 / 边界问题

1. **与 `cpptlm-dgpu-soc-physical-ip-port-boundary` 关系**：前者推 SDMA 5→8 端口切型 + NoCFlit 切型，PNN 是否依赖前者？或独立？
2. **与 `cpptlm-dgpu-soc-control-hub` 关系**：前者新建 ControlHub 集中 BAR decode，PNN 是否影响 Chub 与外设通信？
3. **`simulation_mode` 与 `network.mode` 正交**：2 维组合 = 4 模式（functional×abstract / functional×axi4 / timing×abstract / timing×axi4），是否所有 4 模式都有意义？
4. **跨仓 ArchForge 拓扑联合验证**（P4）vs **`docs/soc_arch/README.md` 占位**：PNN 蓝图文档应放 `docs/designs/dgpu-soc/` 还是 `docs/soc_arch/architecture/`（已迁 ArchForge）？

---

## 11. 决策请求（给项目 Owner）

### 11.1 需要批准的 3 个关键决策

1. **方案代号 PNN 是否接受？** 还是用其他名字（如 `BPF` Bridge Protocol Framework / `PIP` Protocol-Independent Plane）
2. **`NoCFlitBundle` 16 字段作为 canonical wire 是否接受？** 还是新建 `TransportBundle`（含 AXI 5 通道 + CHI 4 类 + PCIe 6 kind 超集）
3. **P0 阶段 1-2 周 doc-only 是否接受？** 还是要求 P0 阶段产出 1 个具体 bridge（如 AxiBridge）作为 proof of concept

### 11.2 后续行动

**如果批准**:
- 立即起草 `cpptlm-pnn-protocol-network` OpenSpec proposal（doc-only, 引用本蓝图 + gem5 调研）
- 起草配套 ADR-DGPU-13（PNN 范围定义）+ ADR-DGPU-14（StreamAdapter↔ProtocolBridge seam 决策）
- P0 阶段启动（1-2 周, 0 KPI）

**如果不批准**:
- 收集具体反对意见（哪一条不通过）
- 修订本蓝图（v0.1 → v0.2）
- 重新评审

---

## 12. 关联文档

### 12.1 现有

- `docs/designs/dgpu-soc/communication-network.md` — v1.0 已落地（6 层架构 + Gem5 MessageBuffer 类比）
- `docs/designs/dgpu-soc/architecture.md` — Minimal DGpu SoC 业务架构（v0.4 + Lock Note v0.6）
- `docs/designs/dgpu-soc/data-flow-and-topology-comparison.md` — 3 config 对比
- `docs/research/gem5-ruby-protocol-bridge-survey.md` — Gem5 Ruby 完整调研（v0.1, 本次同步起草）

### 12.2 待起草

- `openspec/changes/cpptlm-pnn-protocol-network/proposal.md` — P0 doc-only OpenSpec
- `docs/adr/ADR-DGPU-13-pnn-protocol-neutral-network.md` — PNN 范围定义 ADR
- `docs/adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md` — StreamAdapter↔ProtocolBridge seam 决策 ADR

### 12.3 现有 ADR 关联

- `ADR-DGPU-06` chip-internal AXI vs board-level PCIe 边界（PNN 兼容，不破）
- `ADR-DGPU-11` timing-mode SoC scope（PNN 沿用 functional/timing 切换）
- `ADR-DGPU-12` SoC 内部 TLM 接线原则（PNN 不动既有 JSON 拓扑）
- `ADR-DGPU-08` 23 ABI 冻结 + 0 新增（PNN 0 改 23 ABI）
- `ADR-DGPU-09` Driver-Visible Minimal SoC 范围（PNN 不影响 driver-visible 入口）

---

**版本**: v0.1 (初稿，待评审)
**最后更新**: 2026-10
**批准状态**: 🟡 待架构评审（预期 1-2 轮 Oracle/Metis 复审）
**维护者**: CppTLM build agent
**下次更新**: 评审反馈后 v0.2
