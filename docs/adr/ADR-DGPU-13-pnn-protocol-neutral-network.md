## Status Update 2026-10-18: superseded by 06 v0.4 (未签发 ADR, 永不签发)

**本 ADR 自始未签发** (🟡 起草中 v0.1), 2026-10-18 经 [`docs/architecture/06-inter-module-network.md` v0.4 §15](../architecture/06-inter-module-network.md) 演进 seam + [`docs/architecture/06-bis-coherence-as-network-extension.md`](../architecture/06-bis-coherence-as-network-extension.md) v0.3 详化后, 内容已被完全收纳, **永不签发**.

**差异** (vs ADR-DGPU-13 v0.1 起草内容):
- ADR §2 "P1 引入 ProtocolBridge 模板" → 06 §11.2 v0.4 收窄方案 A 推迟到 P3+ (per 06 §15.3 演进 seam)
- ADR §3 "Step 7c 注入" → 06 §9.2 显式不引入
- ADR §4 "改 Packet 字段" → 06 §2.5 + §4.2 显式拒绝 (与 [`docs/archive/architecture/05-packet-semantic-network-archived-2026-10.md`](../archive/architecture/05-packet-semantic-network-archived-2026-10.md) 早期方案同款证据被驳)

**不动 ADR-DGPU-13 正文** (per AGENTS.md "ADR 不可变 + Status Update" 先例), 正文保留作为调研历史.

**关联归档文档**:
- [`../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md`](../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md) (评审对象, 已于 2026-10-18 归档至 [`../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md`](../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md))
- [`../archive/architecture/05-packet-semantic-network-archived-2026-10.md`](../archive/architecture/05-packet-semantic-network-archived-2026-10.md) (立场对立, 早期方案已归档)

---

# ADR-DGPU-13: PNN 协议中立通信网络范围定义

> **状态**: 🟡 **起草中** (v0.1 草案, 待 Oracle/Metis 复审, 预计 1-2 轮 Revise 后签发)
> **日期**: 2026-10-12 起草
> **关联架构文档**: [`../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md`](../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md) (v0.1, 评审对象)
> **关联调研**: [`../research/gem5-ruby-protocol-bridge-survey.md`](../research/gem5-ruby-protocol-bridge-survey.md) (v0.1, 评审对象)
> **配套 ADR**: [ADR-DGPU-14](ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md) (StreamAdapter↔ProtocolBridge seam 决策, 同 cycle 起草)
> **关联 OpenSpec**: 待起草 `cpptlm-pnn-protocol-network` (P0 doc-only) / `cpptlm-pnn-protocol-bridge` (P1) / `cpptlm-pnn-axi-chi` (P2) / `cpptlm-pnn-multi-protocol` (P3) / `cpptlm-pnn-cxl` (P4)
> **关联现有 ADR**: [ADR-DGPU-06](ADR-DGPU-06-axi-mem-bundle-boundary.md) (沿用, PNN 兼容) / [ADR-DGPU-11](ADR-DGPU-11-timing-mode-soc-scope.md) (沿用, PNN 与 simulation_mode 正交) / [ADR-DGPU-12](ADR-DGPU-12-soc-internal-tlm-wiring-principle.md) (沿用, PNN 不破既有 JSON 拓扑) / [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md) (沿用, PNN 0 改 23 ABI)

---

## 1. 背景

### 1.1 问题

dGPU SoC 仿真场景下, 性能评估阶段(早期架构探索)与协议实现阶段(后期具体化)使用**同一组工具链**, 互相影响:

| 阶段 | 需求 | 当前困境 |
|---|---|---|
| **性能评估** (架构探索) | 跑参数扫描 (topology / link latency / VC count) | 每次协议字段变化 (如 SDMA 5 端口 PcieTlp → NoCFlit) 都要改 6+ 文件 + 重新编译 + 跑回归 |
| **协议实现** (后期具体化) | 切具体协议 (AXI4 / CHI / PCIe FC) | 当前无统一机制, 依赖 `axi4_mapper.cc` 个例 + PCIe FC 手写在 `pcie_link_layer_tlm.cc` |

**核心矛盾**: 性能评估阶段需要"协议无关"快速迭代; 协议实现阶段需要"协议特化"高保真建模。**两类工作流的工具链不能互相影响**。

### 1.2 业界锚点 (Gem5 Ruby 20+ 年经验)

> 调研证据见 [`../research/gem5-ruby-protocol-bridge-survey.md`](../research/gem5-ruby-protocol-bridge-survey.md)

**Gem5 Ruby 通过 3 个核心机制解耦"协议中立网络层"与"具体协议"**:

1. **MessageBuffer** 是协议中立的消息队列 (FIFO) — coherence controller 只与它 push/pull
2. **SLICC** DSL 编译期烧协议 — 改 `.slicc` 文件, 不动 controller 外壳
3. **Simple vs Garnet 2.0** 网络模型切换 — `--network=simple` 性能/精度 trade-off

**CHI 协议** (Arm AMBA 5) 已在 Gem5 主线落地, 证明从 MESI/MOESI 切到 CHI 是真实工作流 (per gem5.org/ruby/CHI)。

### 1.3 CppTLM 现状 (per `../designs/dgpu-soc/communication-network.md` v1.0)

CppTLM 已有 70% Gem5 Ruby 资产:

| Gem5 Ruby | CppTLM 现有 | 匹配度 |
|---|---|---|
| Sequencer | `ChStreamModuleBase` | 60% |
| CacheMemory | `InputStreamAdapter<BundleT>` + `OutputStreamAdapter<BundleT>` | 70% |
| MessageBuffer | `InputStreamAdapter<BundleT>::stream_data_/valid_` | 70% |
| NetworkInterface | `DualPortStreamAdapter` | 80% (PE↔Net 协议转换已实现) |
| Router | `RouterTLM` | 50% (缺 cycle-accurate 5-stage) |
| GarnetNetwork | `ModuleFactory` 装配 | 90% (Step 1-9) |
| NetworkLink | `SimplePort + PortPair` | 80% |
| CreditLink | `BidirectionalPortAdapter::pending_credits_` | 85% |
| Topology (Python) | JSON `connections[]` + port_index | 85% |
| **SLICC 协议控制器** | **手写 C++ (无 DSL)** | **40%** (缺协议控制器抽象) |
| Simple vs Garnet | `simulation_mode` JSON 字段 | 90% (已落地 functional/timing) |
| **CHI 协议** | **无** | **0%** (PNN P2 候选) |

**关键缺口**:
1. **协议控制器抽象** (SLICC 替代) — PNN `ProtocolBridge<InB, OutB>` 模板
2. **CHI/CXL 等具体协议** — PNN P2/P4 候选
3. **canonical wire 形式化** — 选 `NoCFlitBundle` 16 字段作为 PNN 唯一规范线

### 1.4 现状约束

- **23 ABI 签名字节级冻结** (per ADR-DGPU-08): 任何 wiring 改动**不**改变 ABI 含义
- **frozen headers** (`pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h`): 0 修改
- **D-AXI v1.4 已交付** (commit `429327d`): `[pcie]` 36672 / `[realip]` 193 / `[pcie-memory]` 48 / `[minimal_dgpu_soc]` 66 / `[dgpu_soc_timing]` 12024 assertions 全绿
- **KPI 3/3 已满** (2026-10-09 修订后): e1 v1.1 + flr-rebar v1.1 archived, p2-integration-unblock 仍 Proposed
- **6 标签 0 回归**: PNN 任何阶段不破
- **`cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 §6 先例**: doc-only 类变更**不消耗 KPI slot**

---

## 2. 决策

### 2.1 决策 1: PNN 正式立项, 方案代号接受

**接受 PNN (Protocol-Neutral Network) 方案代号**, 范围定义如下:

| 维度 | PNN 范围 | 不在范围 |
|---|---|---|
| **协议中立层** | L_PNN 协议中立通信网络 (本方案) | 协议控制器内部状态机 (各 bridge 类内部实现) |
| **canonical wire** | `NoCFlitBundle` 16 字段超集 (既有, 不重命名) | 协议特定字段 (在 bridge 内部) |
| **协议桥抽象** | `ProtocolBridge<InB, OutB>` 模板 (P1 实施) | DSL 编译链 (像 SLICC 一样) — 保留 C++ 直接性 |
| **配置驱动** | `network.mode / network.topology` JSON 字段 (P1 增量) | 运行时协议热切换 (Carbon 文档明示不可逆) |
| **拓扑层** | 既有 RouterTLM / NICTLM / CrossbarTLM / GpuInternalFabric (复用) | cycle-accurate 5-stage router (P3 候选) |
| **协议实现** | abstract (P1) / axi4 (P2) / chi (P2) / pcie_tlp (P3) / cxl (P4) | 物理 IP serdes / PHY 层 (Non-Goal) |

### 2.2 决策 2: PNN 与 simulation_mode 正交

**`network.mode` (协议轴) 与 `simulation_mode` (功能/时序轴) 独立**, 2 维组合 = 4 模式:

| 模式 | `simulation_mode` | `network.mode` | 用途 |
|---|---|---|---|
| **LT × Abstract** | `functional` | `abstract` | P0 性能评估 (Identity bridge, 零开销) |
| **LT × AXI4** | `functional` | `axi4` | P2 早期功能验证 (AXI wire-format, 协议真实) |
| **AT × Abstract** | `timing` | `abstract` | P0 时序评估 (含 latency, 协议中立) |
| **AT × AXI4** | `timing` | `axi4` | P2 周期精确协议验证 (完整 model) |
| AT × CHI | `timing` | `chi` | P2 CHI 周期精确 |
| AT × PCIe TLP | `timing` | `pcie_tlp` | P3 PCIe FC + TLP 完整 |
| AT × CXL | `timing` | `cxl` | P4 CXL.mem/cache/io |

**为什么正交**: 现有 `simulation_mode` 切换是 functional (零延迟协议真实) ↔ timing (latency 注入)。`network.mode` 切换是协议中立 ↔ 协议特化。两者**关注点不同**, 合并会致配置爆炸。

### 2.3 决策 3: canonical wire 选 `NoCFlitBundle` 16 字段

**接受 `NoCFlitBundle` 作为 PNN 唯一规范线格式**, 16 字段定义见 `include/bundles/noc_bundles_tlm.hh:26-137`。

**为什么不新建 `TransportBundle`**: 
- NoCFlitBundle 已存在, 0 改 frozen headers
- 16 字段是任何协议公共超集 (src/dst_node + address + size + vc_id + flit_type/idx/count + hops)
- P0 阶段先用 abstract bridge 验证 16 字段对 simple traffic 足够; P2 实施 AXI bridge 时再验证 AXI 必填字段能完整映射

**触发重评估条件** (任一满足则重写 canonical wire):
- P2 实施 AXI bridge 时发现 16 字段不够 (新加 `aw/w/b/ar/r` 通道字段)
- P2 实施 CHI bridge 时发现 4 flit_type 无法在 `flit_type` 表达 (新加 snoop 类)
- P3 实施 PcieTlp bridge 时发现 credit 字段缺失 (新加 credit_P/NP/Cpl 字段)

### 2.4 决策 4: 协议桥用 C++ 模板, 不用 DSL 编译链

**拒绝 SLICC 式协议 DSL**, 接受 C++ 模板 + 虚函数方案:

```cpp
template <typename InB, typename OutB>
class ProtocolBridge {
public:
    virtual void pack(const InB& in, OutB& out) = 0;   // 协议特定映射
    virtual void unpack(const OutB& in, InB& out) = 0;
    virtual void configure(const json& cfg) {}          // 配置注入
    Stats stats_;                                        // 性能指标
};
```

**为什么不用 DSL**:
1. 引入 SLICC 编译链违背 CppTLM 纯 C++/JSON 栈 (per `cpptlm-dgpu-soc-arch-truth-cleanup` §6 不破坏 C++ 栈的先例)
2. DSL 编译链维护成本 > 协议桥类直接维护成本
3. 桥类用 C++ 模板实现, 编译期类型安全 (与 `BUNDLE_FIELD_WIDTH` 兼容)
4. **可读的 C++ > DSL 抽象** (per `pcie_link_layer_tlm.cc` 手写协议字段先例)

### 2.5 决策 5: P0 阶段 doc-only, 走 cleanup §6 先例

**P0 阶段 1-2 周, 0 KPI 消耗, 走 doc-only 路径**:

依据: `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 §6 显式声明 "doc-only cleanup 类 change 不计入 Proposed KPI 上限"。

**P0 阶段任务**:
1. 起草 `cpptlm-pnn-protocol-network` OpenSpec proposal (引用本 ADR + 蓝图 + gem5 调研)
2. 写 `configs/examples/pnn_mesh_2x2_baseline.json` (用现有 NICTLM + RouterTLM + NoCFlitBundle)
3. 跑 baseline 测试: `[pnn-baseline]` ≥2 cases / 10 assertions, 复用 `[chstream]` + `[noc]` 标签
4. 产出 baseline 报告 (latency/throughput 数字) 作为 P1/P2 验证锚点

### 2.6 决策 6: P1-P4 各 1 KPI slot, 顺序消耗, 季度归档兜底

**P1-P4 阶段每个消耗 1 KPI slot**, 顺序激活:

| 阶段 | OpenSpec change | 估时 | 触发条件 |
|---|---|---|---|
| P0 | `cpptlm-pnn-protocol-network` (doc-only) | 5d | 批准本 ADR + 蓝图 |
| P1 | `cpptlm-pnn-protocol-bridge` | 10-15d | P0 通过 + 1 slot 释放 |
| P2 | `cpptlm-pnn-axi-chi` | 20-30d | P1 通过 + 1 slot 释放 |
| P3 | `cpptlm-pnn-multi-protocol` | 30-45d | P2 通过 + 1 slot 释放 |
| P4 | `cpptlm-pnn-cxl` | 30-45d | P3 通过 + 1 slot 释放 |

**slot 释放机制**: 任何 active change 实施完成后 archive, 释放 slot。

**季度归档兜底**: 超 3/3 KPI 上限触发季度复审, 优先 archive 已实施完毕的 change (per AGENTS.md KPI 规则)。

### 2.7 决策 7: 协议切换是离线操作, 不是热切换

**明确协议切换是离线操作 (offline), 不是热切换 (hot-swap)**:

- bridge 内可藏 OOO rid 表 / state machine (per `axi4_mapper.cc` 既有)
- 切换 mode 后状态守恒失败 (drain in-flight + outstanding 处理)
- **契约**: 协议切换仅在 `idle_drain()` 后 (所有 outstanding 事务完成)
- **不承诺**: 运行时无感切换

依据: Carbon 白皮书明示 "Swapping is a one-way process… swapping back is much more difficult"。

### 2.8 决策 8: 0 破硬边界 (frozen headers / 23 ABI / 6 标签 0 回归)

**PNN 全程 0 触碰以下边界**:

| 边界 | PNN 触碰? | 说明 |
|---|---|---|
| `pcie_endpoint_tlm.h` (4 端口冻结) | ❌ | ProtocolBridge 在 `include/framework/`, 独立 |
| `pcie_display_device.hh` | ❌ | 同上 |
| `pcie_bundles_tlm.hh` | ❌ | PNN 不修改既有 Bundle, 仅新增 `protocol_bridge.hh` |
| `cpptlm_emulator.h` (23 ABI 签名) | ❌ | PNN 0 改 ABI 签名 |
| 双注册表 (SimObject vs SimModule) | ❌ | PNN 用 `ProtocolBridgeFactory` 单独管理, 不新增注册表 |
| 6 标签 0 回归 | ✅ 承诺 | 新增类, Step 7b 增量, 既有 Step 1-7 路径不动 |

---

## 3. PNN 实施状态表

| 阶段 | 内容 | 估时 | KPI | 状态 |
|---|---|---|---|---|
| P0 | doc-only OpenSpec + baseline JSON + 报告 | 5d | 0 | 🟡 起草 (本 ADR 评审后启动) |
| P1 | `ProtocolBridge` + `AbstractProtocolBridge` + `network.mode` 解析 | 10-15d | 1 slot | ⚪ 未启动 |
| P2 | `AxiProtocolBridge` + `ChiProtocolBridge` + Ruby 对照 | 20-30d | 1 slot | ⚪ 未启动 |
| P3 | `PcieTlpProtocolBridge` + per-connection 异构 + dashboard | 30-45d | 1 slot | ⚪ 未启动 |
| P4 | `CxlProtocolBridge` + ArchForge 跨仓验证 | 30-45d | 1 slot | ⚪ 未启动 |

---

## 4. 不在范围 (Non-Goals)

1. **运行时协议热切换** — Carbon 文档明示不可逆, PNN 仅支持离线切换
2. **DSL 协议编译链** — 拒绝引入, 保留 C++ 直接性
3. **cycle-accurate 5-stage router** — P3 候选, 不在 P1 范围
4. **物理 IP serdes/PHY 层** — 真实硬件建模, 不在 PNN 范围
5. **Coherence 协议控制器抽象** — Gem5 SLICC 替代品, PNN 仅做通信网络层
6. **Bundle 字段重命名** — `NoCFlitBundle` 16 字段接受, 不重塑通用基类
7. **新注册表** — PNN 用 `ProtocolBridgeFactory` 单独管理, 不破双注册表

---

## 5. 风险与失败模式

### 5.1 高风险: NoCFlitBundle 16 字段超集假设失败

**症状**: CHI snoop / PCIe posted-write 语义在 16 字段中无法表达

**缓解**:
- P1 阶段先跑 abstract bridge baseline, 验证 16 字段对 simple traffic 足够
- P2 实施 AXI bridge 时验证 AXI 协议字段能完整映射到 16 字段
- 若不满足: P2 增量扩 NoCFlitBundle 字段 (**仍不动 frozen**, NoCFlitBundle 不在 frozen 列表)

### 5.2 高风险: bridge pack/unpack 时延漏计

**症状**: abstract 模式 latency=0, 但具体协议有 pack/unpack 开销, 导致 latency 偏差 >20%

**缓解**:
- P2 立 Ruby 对照 <20% 误差验收线
- bridge::Stats 暴露 `pack_cycles` / `unpack_cycles` 计数器
- 若偏差 >20%: 增加 bridge latency 参数化 (`bridge_latency_cycles: 2` 等)

### 5.3 中风险: 协议切换不可逆

**症状**: bridge 内藏 OOO rid 表 / state machine, 切换 mode 后状态守恒失败

**缓解**:
- 契约规定: 协议切换仅在 idle drain 后
- `ModuleFactory` Step 7b 切换前检查: `if (system_has_pending_transactions()) throw_error`
- 文档明示: PNN 协议切换是离线操作

### 5.4 中风险: 测试矩阵爆炸

**症状**: N 协议 × 2 模式 → N×2 配置 + N×2 测试

**缓解**:
- 每协议只保 1 golden 配置 (`pnn_mesh_2x2_<mode>.json`)
- `[pnn-<mode>]` 标签隔离
- baseline 配置 (P0) 跨协议共享

### 5.5 低风险: KPI 失控

**症状**: P1-P4 各 1 slot, 但 OpenSpec 季度复审可能 archive 现有 change, 释放 slot 速度不均

**缓解**:
- P0 走 doc-only 先例 (per `cpptlm-dgpu-soc-arch-truth-cleanup` §6)
- P1-P4 顺序消耗, 单一 P 未完成时不启动下一 P
- 季度复审触发时优先 archive 已实施完毕的 change

### 5.6 低风险: DSL 编译链诱惑

**症状**: 实施 P2/P3 时, 发现协议字段映射复杂, 开发者提议"像 SLICC 一样写 DSL"

**缓解**:
- 明确拒绝 DSL 诱惑 (保留 C++ 直接性)
- 桥类用 C++ 模板实现, 开发成本 < DSL 编译链维护成本
- 任何"协议切换魔术"的设计诱惑都拒绝 (PNN 是 framework, 不是 magic)

---

## 6. 自检 10 条

| # | 自检项 | 结论 |
|---|---|---|
| 1 | 0 改 frozen headers | ✅ ProtocolBridge 在 `include/framework/`, 不动 frozen |
| 2 | 0 破 23 ABI | ✅ `cpptlm_emulator.h` 零修改 |
| 3 | 6 标签 0 回归 | ✅ 新增类, Step 7b 增量, 既有 Step 1-7 路径不动 |
| 4 | KPI P0=0 / P1-P4 各 1 顺序消耗 | ✅ 走 doc-only 先例 + 顺序消耗 |
| 5 | 复用既有测试套件 | ✅ P0 复用 `[chstream]` + `[noc]`, P1+ 新增 `[pnn]` |
| 6 | 避免过度设计 | ✅ P1 只做 abstract bridge, AXI 推 P2, CHI 推 P2 |
| 7 | 无外部依赖 | ✅ 纯 C++ 模板 + JSON |
| 8 | P0 可 1 周启动 | ✅ 仅 doc + 现有能力组合 (无新代码) |
| 9 | 文档完整 | ✅ 本 ADR + 蓝图 + gem5 调研 + 配套 ADR-14 + OpenSpec proposal (P0 起草) |
| 10 | 可逆 | ✅ 每 P 独立 OpenSpec change, archive 即回滚 |

---

## 7. 关联文档

### 7.1 关联 (起草对象)

- [`../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md`](../archive/designs/dgpu-soc/pnn-protocol-neutral-network-archived-2026-10.md) — PNN 架构蓝图 v0.1 (评审对象)
- [`../research/gem5-ruby-protocol-bridge-survey.md`](../research/gem5-ruby-protocol-bridge-survey.md) — Gem5 Ruby 调研 v0.1 (评审对象)
- [`ADR-DGPU-14`](ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md) — StreamAdapter↔ProtocolBridge seam 决策 (同 cycle 起草)

### 7.2 现有 ADR 关联 (沿用)

- [ADR-DGPU-06](ADR-DGPU-06-axi-mem-bundle-boundary.md) — chip-internal AXI vs board-level PCIe 边界 (PNN 兼容, 不破)
- [ADR-DGPU-11](ADR-DGPU-11-timing-mode-soc-scope.md) — timing-mode SoC scope (PNN 沿用 functional/timing 切换)
- [ADR-DGPU-12](ADR-DGPU-12-soc-internal-tlm-wiring-principle.md) — SoC 内部 TLM 接线原则 (PNN 不动既有 JSON 拓扑)
- [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md) — 23 ABI 冻结 + 0 新增 (PNN 0 改 23 ABI)
- [ADR-DGPU-09](ADR-DGPU-09-driver-visible-minimal-soc-scope.md) — Driver-Visible Minimal SoC 范围 (PNN 不影响 driver-visible 入口)

### 7.3 待起草 OpenSpec

- `openspec/changes/cpptlm-pnn-protocol-network/proposal.md` — P0 doc-only (P0 启动时起草)
- `openspec/changes/cpptlm-pnn-protocol-bridge/proposal.md` — P1 (待 KPI slot 释放)
- `openspec/changes/cpptlm-pnn-axi-chi/proposal.md` — P2 (待 P1 完成)
- `openspec/changes/cpptlm-pnn-multi-protocol/proposal.md` — P3 (待 P2 完成)
- `openspec/changes/cpptlm-pnn-cxl/proposal.md` — P4 (待 P3 完成)

---

## 8. 状态更新

> **2026-10-12 起草 (v0.1)**: build agent 起草, 待 Oracle/Metis 复审
> 预期 1-2 轮 Revise 后签发
>
> **2026-10-20 (KPI 现状变更,正文不改)**: §1 正文记载的 "KPI 3/3 已满 (2026-10-09 修订后)" 已过期。2026-10-20,3 个 active change(`cpptlm-msix-generator-ip` 0/44 / `cpptlm-pcie-tlp-entry-layer` 0/56 / `cpptlm-p2-integration-unblock` 0/45,均 0% 未实施)全部 **⏸ 暂存归档**(`openspec archive --skip-specs`,delta spec 未合并进 `openspec/specs/`)→ **当前 baseline 0/3**。per `AGENTS.md`「⏸ 两种归档语义必须区分」。本 ADR 正文按不可变原则保留为调研历史。

---

**版本**: v0.1 草案
**最后更新**: 2026-10-12
**签发状态**: 🟡 待评审
**维护者**: CppTLM build agent
