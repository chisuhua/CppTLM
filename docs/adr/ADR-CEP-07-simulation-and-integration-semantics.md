# ADR-CEP-07: Coherence Exploration Platform 仿真语义与集成 Seam

> **状态**: 🟡 起草中(v0.7)
> **日期**: 2026-10-21(v0.7 修订)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP 仿真语义(functional/timing 推进)+ 集成 seam(ModuleFactory 注册方式)+ NullProtocolEngine**,解 metis G4 战略盲点
> **关联真源**:
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4 §5.1 simulation_mode 字段(per oracle B3-5 实证 — 06 §4.7 不存在,真源在 06 §5.1 + 01-hybrid §4.7)
> - 真源 1:`docs/architecture/01-hybrid-architecture-v2.1.md` v2.1.10 §1.1(6 层)+ §4.7
> - 真源 1:`06-bis-coherence-as-network-extension.md` v0.3 §5(SnoopProbe / SnoopRespBundle 沿用)
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口,草案)
> - 真源 3:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射,本批起草)
> - 真源 3:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流清单)
> - 真源 3:`ADR-CEP-06-experiment-protocol.md`(实验输出,本批起草)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md`(编号预分配,本批起草)
> - 真源 3:`ADR-CEP-08-ordering-point-portability.md`(Ordering Point 可迁移性,本批起草)
> - 真源 3:`ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md`(多 VC 仲裁 + 死锁自由性论证,本批起草)
> - 真源 3:`ADR-CEP-12-protocol-engine-capability-map.md`(ProtocolEngine 能力映射表 schema,本批起草)
> - 真源 3:`ADR-CEP-13-coherence-domain-ownership.md`(一致性域归属,本批起草)
> - 真源 3:`ADR-DGPU-12-soc-internal-tlm-wiring-principle.md`(ModuleFactory 装配)
> - 真源 3:`ADR-DGPU-08-abi-freeze-policy.md`(23 ABI 冻结)
> - 真源 3:`ADR-DGPU-09-driver-visible-minimal-soc-scope.md`(driver-visible 入口)
> - 真源 1:`01-hybrid-architecture-v2.1.md` v2.1.10 §1.1(6 层)
> - 真源 1:`06-bis-coherence-as-network-extension.md` v0.3 §5(SnoopProbe / SnoopRespBundle 沿用)

---

## 1. 背景

### 1.1 metis G4 盲点 — 仿真语义 + 集成 seam 双缺位

CEP 系列 ADR 长期断言"CEP 与 simulation_mode 正交"(per `ADR-CEP-00 §6`),但从未定义:
- functional vs timing 模式下 group-scoped completion 如何推进仿真周期
- CEP 模块如何注册进 ModuleFactory(`REGISTER_CHSTREAM` / `REGISTER_MODULE`)
- 与 frozen headers / DGpuBoard 所有权如何共存
- 06-bis 已规划的 `coherence_bundles_tlm.hh` / `coherence_bridge.hh` / `coherence_domain.hh` / `coherent_xbar_tlm.hh` / `coherence_domains` JSON 字段如何处置
- `NullProtocolEngine`(per metis G4 盲点 — 只有 NullCollectivePlane,没有 NullProtocolEngine)

### 1.2 本 ADR 的定位

本 ADR 解 G4 **仿真语义 + 集成 seam**;06-bis 既有产物处置另起 ADR。

---

## 2. 决策

### 决策 1:`simulation_mode` 单一维度(per `06 v0.4 §5.1` + `01-hybrid §4.7` + `ADR-DGPU-11`;per oracle B3-5 实证 — 06 §4.7 不存在)

`simulation_mode ∈ {functional, timing}` 是 JSON 顶层字段,**不引入 network.mode 4 组合**(per 06 v0.4 §5.2):

| 模式 | 含义 | 何时用 |
|------|------|--------|
| **functional** | 业务/网络逻辑正确性验证,延迟 = 0 / 协议字段映射存在 | 前期性能建模期 |
| **timing** | 周期精确仿真,Gem5 Simple vs Garnet 类似精度 | 后期协议特化时 |

**不变量 INV-CEP-07-IC1**:`simulation_mode` 是顶层唯一维度,协议插件不得引入 `network.mode` 字段(per 06 v0.4 §5.2 拒绝 network.mode 的依据)。

### 决策 2:functional / timing 推进对 coherence plane 的影响

| 仿真语义 | coherence 流推进 | group-scoped completion 推进 |
|---------|------------------|-------------------------------|
| **functional** | 流推进 = 同步直调(per `SimplePort::send`);事务完成 = 单次 tick 即达 | group rendezvous 等待 = 占位(0 cycle),所有成员 ack 后即时 emit `Completion` 事件 |
| **timing** | 流推进 = cycle-accurate;事务完成 = 跨 cycle 累加(per `vc_id` 多 VC 仲裁) | group rendezvous 等待 = 累计 cycle;参与方未到齐时不推进仿真周期 |

**实现**:`ProtocolEngine` 基类的 `advance_cycle()` 虚函数(per `chstream_module.hh:73` 默认空实现)按 `simulation_mode` 分发:
- functional:no-op
- timing:cycle accumulation,flush 时钟

### 决策 3:ModuleFactory 集成 seam

**重要**:既有宏形式 `REGISTER_CHSTREAM(T)` / `REGISTER_OBJECT(T)` **不存在**(per oracle B7 + grep 实证:`include/chstream_register.hh:50` `REGISTER_CHSTREAM` 无参,`include/modules.hh:30` `REGISTER_OBJECT` 无参 no-op)。CEP 模块采用**真实注册机制**:

| 模块类型 | 基类 | 注册机制 | 路径 | 示例 |
|---------|------|----------|------|------|
| **ProtocolEngine** 派生类 | `ChStreamModuleBase` | `ModuleFactory::registerObject<T>`(per `include/core/module_factory.hh:122` Step 2) | `include/tlm/cep/` | `ChiRnfAdapter` / `CxlCacheAdapter` / `TileLinkCAdapter` / `NvlinkAdapter` / `UalinkAdapter` |
| **SharerTracker** 派生类 | `SimModule`(per `module_factory.hh:135` `static_assert std::is_base_of_v<SimModule, T>`)| `REGISTER_MODULE(T)`(per `include/modules_cluster.hh:41-105`,有 T 参数,合法) | `include/tlm/cep/` | `FullMapDirectory` / `SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull` |
| **FabricCollectivePlane** 派生类 | `SimModule` | `REGISTER_MODULE(T)` | `include/tlm/cep/` | `NullCollectivePlane` / `NvlinkSharpCollective` / `UalinkIncCollective` |
| **StubCacheCore** | `ChStreamModuleBase` | `ModuleFactory::registerObject<T>` + `ChStreamAdapterFactory::create` | `include/tlm/cep/` | `StubCacheCore` |

**约束**:
- CEP 模块**不**作为 23 ABI 入口(per `ADR-DGPU-08 §2.3` 0 新增约束)
- CEP 模块**不**进 `cpptlm_emulator.h`(per `ADR-DGPU-08` 冻结面)
- CEP 模块**不**改 `include/tlm/pcie/pcie_endpoint_tlm.h` `[[deprecated]]` 状态(per `ADR-DGPU-09` driver-visible 入口)
- **新增约束**:CEP 模块注册走**真实注册机制**,不发明宏形式

### 决策 4:`NullProtocolEngine` 作为 ProtocolEngine 默认实现

```cpp
// include/tlm/cep/null_protocol_engine.hh
class NullProtocolEngine : public ProtocolEngine {
public:
  // 10 流覆盖矩阵(per oracle v0.4 B6 修订 — 必须全 10 流)
  void request(addr, perm, txn_id) override { /* 流 1: no-op */ }
  void completion(txn_id, data, perm, error) override { /* 流 2: immediate-complete to stub */ }
  void snoop(addr, type, txn_id) override { /* 流 3: no-op */ }
  void snoop_resp(addr, hit, state, dirty, data, txn_id) override { /* 流 4: no-op */ }
  void writeback(addr, data, dirty) override { /* 流 5: no-op */ }
  void forward_data(src, dst, data, perm, txn_id) override { /* 流 6: no-op */ }
  void back_invalidate(addr, type, txn_id) override { /* 流 7: no-op */ }
  void address_resolve(pa, node_id, &home_node, &coherent) override { /* 流 8: immediate-resolve */ }
  void barrier(domain, direction, txn_id) override { /* 流 9: immediate-complete */ }
  void snoop_completion_ack(addr, node_id, txn_id) override { /* 流 10: no-op */ }
  void advance_cycle() noexcept override {}
  cap_t query_capability(op_t op, size_t) override { return {}; }
};
```

**用途**:
- Stub Cache Core 单独测试(无网络) — 用于 A12 stub 可验证性
- ProtocolEngine 接入对端(per A10 插件能力映射表) — 用于 stub ↔ ProtocolEngine 解耦测试
- 平台 boot/shutdown 路径测试

### 决策 5:`NullCollectivePlane` 已有(per `ADR-CEP-03 §2 Decision 3`),本 ADR 不重复定义

沿用 `ADR-CEP-03 §2 Decision 3` 已定义的 `NullCollectivePlane`;本 ADR 显式声明它是 Collective Plane 默认绑(per ADR-CEP-01 §2 Decision 1)。

### 决策 6:CEP 与 DGpuBoard / frozen headers 集成边界

| 边界 | CEP 行为 |
|------|---------|
| `cpptlm_emulator.h` 23 ABI | **不修改**(per ADR-DGPU-08 冻结) |
| `include/tlm/pcie/pcie_endpoint_tlm.h` `[[deprecated]]` | **不修改** |
| `include/tlm/cache_tlm.hh` 现有 CacheTLM | **不修改**(A13 议题 CEP-15 决议)|
| DGpuBoard 板级集成 | **不进入** dgpu_board_shell.cc;CEP 是独立探索平台,通过 JSON `extends` 或 `hierarchy` 引用(per 06 §10)|
| ModuleFactory Step 7 StreamAdapter 注入 | **沿用现有机制**;CEP 模块用 `REGISTER_CHSTREAM` / `REGISTER_MODULE` 接入 |

### 决策 7:`simulation_mode` 与协议插件能力的映射

`simulation_mode` 是 JSON 字段,ProtocolEngine 通过 `INIT_CONFIG` 读入后决策:
- functional:disable timing-specific counters,enable correctness checks

### 决策 8:06-bis 既有产物处置

**本 ADR 不处置**;独立 ADR `ADR-CEP-07A-06bis-artifact-disposition.md`(后续起草)处理:
- `include/bundles/coherence_bundles_tlm.hh`(不存在,待创建 — per ADR-CEP-00A §3.6)
- `include/tlm/coherence_bridge.hh` / `coherence_domain.hh` / `coherent_xbar_tlm.hh`
- `coherence_domains` JSON 字段(per 06-bis §4)
- 处置策略:采用 / 改名 / 废弃 三选一 + 决策理由

### 决策 9:ProtocolEngine 插件接口的**可组合性**要求(v0.7 新增,per 调研 F20 + Oracle 独立复审)

**问题**:本 ADR v0.6 定义了 ModuleFactory 集成 seam 与 `NullProtocolEngine`,但**未对插件接口的"可组合性"提任何要求**。而调研 F20 给出了一手教训:

> ProtoGen 的 OO 代码结构在适配 HieraGen 时"**the code quality significantly decreased making it difficult to maintain and extend the HieraGen tool**";HeteroGen 则"**largely from scratch**"重写,并引入**贯穿全工具的统一 IR 数据结构**。
> —— 博士论文 §9.2(一手,E1)

**问题实质**:协议自动生成工具是"**编译器**"(论文原话:"they are effectively compilers"),其"算法阶段"可以拼接;而 CEP 的组合维度是**协议实例的组合**,两者的耦合形态不同但**教训可迁移** —— 若插件接口在设计期没有为组合留接缝,组合能力落地时将面临**接口重写**。

**决策**:
1. **ProtocolEngine 插件接口必须从第一版就为组合维度预留接缝** —— 具体为:
   - 插件**声明**本协议的排序假设与消息→VC 绑定(per `ADR-CEP-12` 的 `ordering_assumption` / `vc_binding` 字段)
   - 插件**不实现**组合逻辑(组合由 Directory & Ordering Plane + Protocol Engine 之间的编排层承担)
2. **组合接缝点必须在 Stage 1 定义并被合同测试覆盖**(per `ADR-CEP-14` Stub 可验证性合同测试的扩展)
3. 本 ADR **不**规定具体的接缝 API 形态 —— 形态归 `ADR-CEP-12` 的 schema(字段层)与 `ADR-CEP-08` / `ADR-CEP-13` 的语义层

**不承诺**:本 ADR **不**承诺 Stage 1 就交付"分层 + 异构"组合能力(per `ADR-CEP-17 §2 Decision 9`:该组合**可做**但走人工声明 + 实验证伪路线,不承诺自动化)。本决策只承诺**接口不堵死组合**。

**理由**:接口重写是**最贵**的技术债;而组合能力的**范围**由 `ADR-CEP-13` 决定。两者必须分离:**接口预留(本决策,Stage 1 必做) ≠ 组合落地(ADR-CEP-13,后续 Stage)**。

---

## 3. 不变量

- **INV-CEP-07-IC1**:`simulation_mode` 顶层唯一维度,协议插件无 `network.mode` 字段
- **INV-CEP-07-IC2**:`NullProtocolEngine` 是 ProtocolEngine 默认实现,所有方法 no-op,用于解耦测试
- **INV-CEP-07-IC3**:`NullCollectivePlane` 是 Collective Plane 默认实现,所有原语返回"不支持"
- **INV-CEP-07-IC4**:CEP 模块注册走既有宏体系(`REGISTER_CHSTREAM` / `REGISTER_MODULE` / `REGISTER_OBJECT`)
- **INV-CEP-07-IC5**:CEP 不破 23 ABI / frozen headers / `[[deprecated]]` 状态
- **INV-CEP-07-IC6**:functional 模式下 `avg_latency_cycles == 0` 必须成立(per `ADR-CEP-06 §5`)
- **INV-CEP-07-IC7**(v0.7 新增):**ProtocolEngine 插件接口必须为组合维度预留接缝** —— 插件**声明**排序假设与消息→VC 绑定(per `ADR-CEP-12` `ordering_assumption` / `vc_binding`),但**不实现**组合逻辑;组合接缝点须被合同测试覆盖(per 决策 9 / 调研 F20)

---

## 4. 仿真时序推进图(per simulation_mode)

### 4.1 functional 模式

```
tick():
  ProtocolEngine::advance_cycle() = no-op
  StubCacheCore::tick() = 同步直调(per SimplePort::send)
  完成事件即时 emit,latency = 0
```

### 4.2 timing 模式

```
tick():
  ProtocolEngine::advance_cycle():
    for each pending_txn in transaction_table:
      if all conditions met:
        advance txn state by 1 cycle
        if state == COMPLETE:
          emit completion to stub
  StubCacheCore::tick():
    snoop response = current cycle + N(param)
```

---

## 5. 集成 seam 实施清单

| Seam | 实施 | 阻塞 |
|------|------|------|
| **模块注册** | `REGISTER_CHSTREAM(ChiRnfAdapter)` 等 5 个 + `REGISTER_MODULE` 等 3 类 + `REGISTER_OBJECT(StubCacheCore)` | P1 实施 |
| **配置文件** | `configs/dgpu_cep_minimal_v1.json` 新建(独立于 dgpu_soc_*_family)| P2 builder |
| **仿真入口** | `src/tlm/cep/cep_main.cc`(独立入口,不依赖 dgpu_board_shell.cc)| P2 builder |
| **Python 工具** | `tools/render_report.py` / `tools/run_sweep.py` / `tools/ab_compare.py`(per `ADR-CEP-06 §4`)| P2 builder |
| **KPI 集成** | CEP umbrella change `cpptlm-cep-umbrella` (per `ADR-CEP-00 §2 Decision 3` 单一真源,Stage 0 计入 KPI = 1 slot,per oracle v0.4 B8 修订:三处口径对齐)| Stage 0 doc-only 实施 |
| **实验目录** | `experimental/` 目录约定(per `ADR-CEP-06 §2`)| P1 doc-only |

---

## 6. 抓取点

1. **NullProtocolEngine 与 NullCollectivePlane 双 null 实现必须存在**
2. **`simulation_mode` 与 `network.mode` 不共存**(per 06 v0.4 §5.2)
3. **CEP 模块不破 23 ABI / frozen headers**(machine grep 检查)
4. **`simulation_mode` 字段在 CEP JSON 必含**(per ADR-CEP-06 §6 抓取点 1)
5. **functional 模式下 `avg_latency_cycles == 0` 强制**(per ADR-CEP-06 §5)
6. **插件接口可组合性**(v0.7 新增):ProtocolEngine 插件**必须**声明 `ordering_assumption` + `vc_binding`,且**不得**自行实现跨域/跨层组合逻辑(per `INV-CEP-07-IC7`)

---

## 7. 关联 ADR

- 基础:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
- 配套:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射)
- 配套:`ADR-CEP-06-experiment-protocol.md`(实验输出)
- 配套:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
- 后续:`ADR-CEP-07A-06bis-artifact-disposition.md`(06-bis 既有产物处置,后续起草)
- 配套:`ADR-CEP-08-ordering-point-portability.md`(Ordering Point 可迁移性)
- 配套:`ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md`(多 VC 仲裁 + 死锁自由性论证)
- 配套:`ADR-CEP-12-protocol-engine-capability-map.md`(插件能力映射表 schema)
- 配套:`ADR-CEP-13-coherence-domain-ownership.md`(一致性域归属)
- 关联:`ADR-DGPU-08-abi-freeze-policy.md`(23 ABI 冻结)
- 关联:`ADR-DGPU-09-driver-visible-minimal-soc-scope.md`(driver-visible 入口)
- 关联:`ADR-DGPU-12-soc-internal-tlm-wiring-principle.md`(ModuleFactory 装配)

---

## 8. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| `simulation_mode` 单一维度 | functional / timing 1 维度,无 network.mode | ✅ 已拍板 |
| functional / timing 推进 | functional = 同步直调;timing = cycle-accurate | ✅ 已拍板 |
| ModuleFactory 集成 | `REGISTER_CHSTREAM` / `REGISTER_MODULE` / `REGISTER_OBJECT` 沿用 | ✅ 已拍板 |
| NullProtocolEngine | 新建| ✅ 已拍板 |
| NullCollectivePlane | 沿用 `ADR-CEP-03 §2 Decision 3` | ✅ 已拍板 |
| CEP 不破 23 ABI / frozen headers | 严格沿用 ADR-DGPU-08/09 | ✅ 已拍板 |
| 06-bis 既有产物处置 | 独立 ADR `ADR-CEP-07A` 处理(本 ADR 不展开)| ✅ 已拍板 |
| **插件接口可组合性** | **接口从第一版为组合维度预留接缝**(声明 `ordering_assumption` + `vc_binding`,不实现组合逻辑)| ✅ **已拍板**(v0.7,per 调研 F20)|

---

## 9. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
