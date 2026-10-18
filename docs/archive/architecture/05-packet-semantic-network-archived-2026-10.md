> **⚠️ [已归档 2026-10-18]**
> 
> **归档原因**: 本文档提议改 Packet 字段 (增量修改 2 字段 + 新增 3 字段 + reset() 重置) + 引入 L4.5 `ProtocolAdapter` 抽象层. 立场与 [`docs/architecture/01-hybrid-architecture-v2.1.md`](01-hybrid-architecture-v2.1.md) §1.1 (v2.1.10 "通信框架层 vs 总线层" 锁定 L3+L4 已达 Gem5 MessageBuffer 水准, **不需新增抽象层**) + [`docs/architecture/06-inter-module-network.md`](06-inter-module-network.md) v0.4 (§2.5 + §4.2 显式 0 改 Packet) **根本对立**.
> 
> **调研价值保留**:
> - §2.5 Packet 字段分类调研 (7 个死字段) 被 06 §2.5 继承
> - §3 Gem5 MessageBuffer 锚点调研被 01 §1.1 继承
> - §0.2bis KPI 现状承认被 06 §11.5 引用
> 
> **被替代设计**: L4.5 `ProtocolAdapter<InB, OutB>` 抽象 → 06 §4.4 `ProtocolBridge<InB, OutB, Ordered>` (类型签名不同但抽象意图类似, 沿用 PNN 命名, P3+ 实施时按 06 §15.3 模板落地).
> 
> **关联 ADR superseded 标注**: [`docs/adr/ADR-DGPU-13-pnn-protocol-neutral-network.md`](../../adr/ADR-DGPU-13-pnn-protocol-neutral-network.md) + [`docs/adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`](../../adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md) 顶部已加 `## Status Update 2026-10: superseded by 06 v0.4` 段.
> 
> **归档执行**: 2026-10-18, per docs/architecture/ 重新规划 (architect 视角, 单一真源原则).

---

# CppTLM 基于 Packet 语义的通信网络架构

> **文档状态**: 🟡 v0.1 初稿,待架构评审 (已归档 2026-10-18, 见顶部归档说明)
> **作者**: CppTLM build agent
> **关联 OpenSpec**: 待起草(`cpptlm-packet-semantic-network`)
> **关联 ADR**: 修订既有 `ADR-DGPU-13-pnn-protocol-neutral-network.md` + `ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`(PNN 蓝图已起草,本方案作为其代码驱动版本,不新起编号,见 §14.2)
> **前置调研**: [`docs/research/gem5-ruby-protocol-bridge-survey.md`](../research/gem5-ruby-protocol-bridge-survey.md)
> **前置架构**: [`docs/architecture/01-hybrid-architecture-v2.1.md`](01-hybrid-architecture-v2.1.md) v2.1.10 + [`docs/designs/dgpu-soc/communication-network.md`](../designs/dgpu-soc/communication-network.md) v1.0 + [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md) v0.1 (后者已于 2026-10-18 同步归档)
> **目标存档**: `docs/archive/architecture/05-packet-semantic-network-archived-2026-10.md` (原 `docs/architecture/05-packet-semantic-network.md`)

---

## ⚠️ 评审请求(写给评审者)

本文档是 CppTLM **仿真框架层**的关键架构草案,基于已读代码(`include/core/packet.hh` 276 行 + `chstream_module.hh` 76 行 + `simple_port.hh` 61 行 + `framework/stream_adapter.hh` 等)与既有三份前置文档的**综合演进方案**。

**本文不承诺**:
- ❌ 不承诺"消除协议特化"(`Packet` 已承认"协议必然要特化")
- ❌ 不承诺"运行时协议切换"(编译期绑定,沿用既有 C++ 模板约束)
- ❌ 不承诺"零代码改动切协议"(`ProtocolAdapter` 提供 framework 但不 magic)

**本文承诺**:
- ✅ **代码驱动**: 基于 `include/core/packet.hh` 实地读到的 276 行字段(占 Gem5 MessageBuffer 70% 匹配度,非 90%),不是抽象推演
- ✅ **最小侵入**: 0 改 Packet ext API、0 改 frozen headers、0 改 23 ABI、0 改 Packet 9 个保持字段;P1 诚实增量修改 Packet 2 个既有字段 + 新增 3 字段 + 修改 reset(),见 §2.2
- ✅ **桥接式抽象**: 新增 L4.5 `ProtocolAdapter<InB, OutB>` 模板基类(经 `ProtocolAdapterEr` type-erased 桥接到 ChStreamModuleBase),承认 `DualPortStreamAdapter` 无字段映射、新增字段映射代码(不"复用"),见 §2.3.2
- ✅ **5 阶段 P0-P4**: 与 PNN 蓝图同步推进,不重复造轮子
- ✅ **Gem5 Ruby 锚点**: 基于 20+ 年业界经验,可信度高

**评审重点**:
1. Packet 字段升级方案(`flit_category`/`protocol_id`/`bridge_id` 3 个 uint8_t/uint16_t)是否过度?
2. L4.5 `ProtocolAdapter` 与 L4 `DualPortStreamAdapter` 桥接语义是否清晰?
3. P0 阶段(含 3 文件 build 改动,1-2 周估时是否现实)与 P1 阶段(实施细节)是否可执行?
4. 与 `pnn-protocol-neutral-network.md` v0.1 的关系是**互补**还是**替代**?
5. canonical wire 选择 `NoCFlitBundle` + Packet 元数据 vs 单一 Bundle,哪种更符合 CppTLM 现状?

---

## 0. 摘要

### 0.1 核心立场

**CppTLM 通信网络的现状是"70% Gem5 Ruby 资产但被 Packet 类承载"**,架构目标是:

- **增量修改 Packet 既有 2 字段**:`CmdType` enum 扩展 4 个 PCIe 语义值(CFG_READ/CFG_WRITE/MSG/CPL)+ `route_path` 类型从 `std::vector<std::string>` 改为固定大小(共 2 处既有字段修改,均 P1 显式列出,**不算"0 改"**——见 §2.2.1 + §2.2.4)
- **新增 Packet 字段作为语义元数据载体**: 新增 3 个 `uint8_t`/`uint16_t` 字段,标识 `flit_category` / `protocol_id` / `bridge_id`,把"协议语义"从模块代码下移到 Packet 元数据
- **桥接式抽象 L4.5 `ProtocolAdapter<InB, OutB>`**: 在 L4 StreamAdapter 与 L0 Packet 之间插入(统一 seam 位置,见 §2.3),由 `ProtocolAdapter::pack/unpack` 写 Packet 协议字段(不是 StreamAdapter::send),**不重写 5 子类 StreamAdapter**;桥接实例 `DualPortProtocolAdapter` **组合持有** `DualPortStreamAdapter*` + 新增字段映射代码(承认非复用,见 §2.3.2)
- **5 阶段可执行**: P0 含 Packet 字段审计(P0 任务包含 1 个 config + 2 个测试,共 3 文件 build 改动,**非纯 doc-only**) + P1 实施细节(Packet 字段增强 + ProtocolAdapter 模板基类 + ModuleFactory Step 7c)

### 0.2 设计原则

1. **代码驱动,不是理论驱动**: 从 `packet.hh` 276 行出发,挖掘其已有 70% Gem5 MessageBuffer 字段,**不绕过 Packet**
2. **协议必然要特化**: Packet 字段命名即字段语义(typed POD),改字段名 = 改模块代码,本方案不假装"运行时无感切换"
3. **编译期绑定**: 沿用 C++ 模板,不引入运行时反射/类型擦除(保持 `BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable` 断言)
4. **桥内可藏状态**: 协议适配层内可有 OOO rid 表 / state machine(per `axi4_mapper.cc` 已有 outstanding_rid_),**但切换协议必须 idle drain**——不可逆
5. **明确边界**: P0 含 build 改动(非 doc-only),P1 增量修改 Packet 既有 2 字段(`CmdType` enum 扩 4 值 + `route_path` 改型);P0-P4 全程 0 改 `pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h` / Packet ext API
6. **KPI 现状承认**: 当前 `openspec/changes/` 5 活跃 / 3 上限已**实际超限 2**(且 5 个均不可 archive,详见 §0.2bis),本方案 P1-P4 启动**没有可行的 KPI 路径**,需 Owner 决策或季度复审触发
7. **与 simulation_mode 正交**: `network.mode`(协议轴)与 `simulation_mode`(功能/时序轴)独立,2 维组合 = 4 模式
8. **职责边界清晰**: StreamAdapter 管 Port↔ch_stream 转换;ProtocolAdapter 管 Bundle↔Bundle 协议转换**+ 写 Packet 协议字段**;两者不互相反向引用

### 0.2bis KPI 现状说明(2026-10)

`openspec/changes/` 当前活跃 proposal:

| 名称 | 状态 | 是否可 archive |
|------|------|---------------|
| `cpptlm-dgpu-soc-control-hub` | Proposed Draft | ❌ 未实施,不可 archive |
| `cpptlm-dgpu-soc-physical-ip-port-boundary` | Proposed Draft | ❌ 未实施,不可 archive |
| `cpptlm-msix-generator-ip` | Proposed Draft | ❌ 未实施,不可 archive |
| `cpptlm-p2-integration-unblock` | Proposed | ❌ 未实施,不可 archive |
| `cpptlm-pcie-tlp-entry-layer` | Proposed | ❌ 未实施,不可 archive |

**总数:5 / 上限:3,超限 2 个 slot。** 但**所有 5 个 change 均未实施完毕**,**没有可以 archive 的对象**——意味着本方案 P1-P4 启动**没有可行的 KPI 路径**,除非:

**KPI 口径备注**(待 P0 阶段确认):
- 上表按 `openspec/changes/` 目录活跃数计算,实际 KPI 计数需复核各 proposal.md 的 §6 / §7 是否声明"不消耗 slot"
- 已知 `cpptlm-dgpu-soc-control-hub` / `cpptlm-dgpu-soc-physical-ip-port-boundary` 可能声明 doc-only 豁免(待 proposal.md 实地确认)
- 若部分 change 不消耗 slot,实际 KPI 数可能为 2-3 而非 5(本节估算为最坏情况)
1. 实施其中 2 个 change 后 archive 它们(估时 30-60d,远超本方案 P1 估时)
2. 季度复审触发 archive(不可控)
3. 上限规则由项目 Owner 临时调整(超 Owner 决策范围)

**诚实的结论**: **本方案 P1 启动**被 KPI 现状**事实阻塞**,不属于"必须立即启动"的项目。建议:
- P0 阶段消耗 1 个 KPI slot(实际含 3 文件 build 改动,不满足 doc-only 豁免,详见 §7.1)
- P1 启动需等 KPI 释放,或由项目 Owner 决策是否扩展上限
- 同步动作:在 OpenSpec 季度复审中申请把本方案列入"优先级高于其他 5 个 change"

### 0.3 与现有三份前置文档的关系

| 前置文档 | 与本方案关系 |
|---------|-------------|
| `01-hybrid-architecture-v2.1.md` v2.1.10 | **本方案基于其 6 层框架**——v2.1.10 §1.1 的"通信框架层 vs 具体总线层"分层是本方案的**顶层架构依据** |
| `communication-network.md` v1.0 | **本方案基于其代码实证**——§4 表"5 子类 StreamAdapter 全景"是 L4.5 桥接的事实基础,§7 架构债务清单是 Packet 升级的动机 |
| `pnn-protocol-neutral-network.md` v0.1 | **本方案是其代码驱动版本**——PNN 自顶向下从 Gem5 Ruby 推导出 L_PNN;本方案自底向上从 Packet 276 行代码推导出 L4.5 桥接 + Packet 元数据升级。两者**互补而非冲突** |

**三角关系**:

```
   v2.1.10 (理论: 6 层框架 + 设计原则)
        ↓ 顶层架构依据
   communication-network.md (实证: 5-hop 数据流 + 5 子类适配器 + 8 项架构债务)
        ↓ 代码事实基础
   本方案 (综合: Packet 升级 + L4.5 桥接 + 5 阶段实施)
        ↓ 互补关系
   pnn-protocol-neutral-network.md (演进: L_PNN + 5 阶段 P0-P4)
```

---

## 1. 动机与现状

### 1.1 真实痛点(读 Packet 代码得到)

读 `include/core/packet.hh`(276 行)发现,`Packet` 已经是**复合头**,不是"单一数据容器",但**承载语义不充分**:

| 字段 | 类型 | 现状 | 问题 |
|------|------|------|------|
| `stream_id` | `uint64_t` | ChStreamModule 设置 | ✅ 用 |
| `seq_num` | `uint64_t` | 偶用 | ⚠️ 用得不多 |
| `cmd` | `CmdType` enum | READ/WRITE/INVALID | ⚠️ 仅 3 值,缺 PCIe CFG/MSI 等 |
| `type` | `PacketType` enum | REQ/RESP/STREAM/CREDIT | ✅ 用 |
| `vc_id` | `int` | 几乎不用 | ⚠️ Router/BidirectionalPort 用 1-cycle credit 模拟 |
| `flow_id` | `uint64_t` | 几乎不用 | ⚠️ 应承担 AXI rid 映射 |
| `priority` | `uint8_t` | 几乎不用 | ⚠️ ArbiterTLM 仲裁输入 |
| `route_path` | `std::vector<std::string>` | Router 追加但太重(`coherent_xbar_tlm.cc:65` 等赋值点) | ⚠️ 应改为固定大小 |
| `hop_count` | `int` | 计数 | ✅ 用 |
| `dependents` | `std::vector<Packet*>` | 几乎不用 | ⚠️ 死字段 |
| **payload** | `tlm_generic_payload*` | **唯一数据载体** | ✅ 用 |

**根因**(读 `packet.hh:221-239` 注释):

> "Payload 扩容(D-AXI v1.5 B17, N1 框架修复)——AxiMemBundle (~4136B) > PacketPool kMinPayloadBytes (256B),StreamAdapter 序列化时 `serialize_bundle` 检查 `len < sizeof(BundleT)` 会失败。"

Bundle 大小通过 **payload** 传,**不通过 Packet 字段**——这导致 Packet 的流控/路由字段(`vc_id`/`flow_id`/`priority`/`dependents`)长期闲置。

### 1.2 Gem5 Ruby 的解法(20+ 年锚点)

`docs/research/gem5-ruby-protocol-bridge-survey.md` 调研表(§6)显示 CppTLM 已有 **70% Gem5 Ruby 资产**:

| Gem5 Ruby | CppTLM 现有 | 匹配度 |
|-----------|------------|-------|
| MessageBuffer | `InputStreamAdapter<BundleT>` + `OutputStreamAdapter` | 70% |
| **NetworkInterface** | **整个 L4.5 = `DualPortStreamAdapter` (L4 端, 80%) + `ProtocolAdapterEr`/`ProtocolAdapter` (L4.5 新增, 0%→100% per P1-P4)** | **80%**(P1 后达 95%) |
| GarnetNetwork | `ModuleFactory` 装配 | 90% |
| NetworkLink | `SimplePort + PortPair` | 80% |
| CreditLink | `BidirectionalPortAdapter::pending_credits_` | 85% |
| Topology (Python) | JSON `connections[]` + port_index | 85% |
| SLICC controller | 手写 C++ | 40% |
| CHI 协议 | 无 | 0% (PNN P2 候选) |

**关键洞察**(per `gem5-ruby-protocol-bridge-survey.md` §6 表):

- CppTLM 整体 Gem5 Ruby 资产匹配度 **70%**(全项目均值)
- `Packet` 与 Gem5 MessageBuffer 匹配度 **70%**(`stream_id`/`seq_num`/`vc_id`/`priority`/`flow_id`/`route_path`/`hop_count` 7 个字段直接对应,但缺 MessageBuffer 的 ordered/peek/pop 显式 API)
- `DualPortStreamAdapter` 与 Gem5 NetworkInterface 匹配度 **80%**(语义对得上,但实际无字段级 PE↔Net 映射代码,见 §2.3.2)
- **`Packet` 不是 Gem5 MessageBuffer 90%**——修正 §0.1 line 23/§2.1 line 230 的早期表述,与 §1.2 一致

**NetworkInterface 类比统一**(前轮评审修订):
- ❌ 旧表述: "DualPortStreamAdapter = NetworkInterface" 或 "ProtocolAdapter::pack/unpack = NetworkInterface"
- ✅ **新表述**:**整个 L4.5 (ProtocolAdapterEr + ProtocolAdapter<InB,OutB> 模板 + DualPortProtocolAdapter 组合持有 DualPortStreamAdapter*)** = Gem5 NetworkInterface 的协议转换职责
- 具体 `ProtocolAdapterEr::pack_with_packet(Packet*)` = Gem5 `NetworkInterface::wakeup()` 中"接收 message → 打包 flit → 写 vnet 字段"流程

本方案的目标是:
1. 让 `Packet` 从"被动壳"进化为"语义元数据载体"(P1 新增 3 字段 + reset() 重置)
2. 让 `DualPortStreamAdapter` 升级为 `DualPortProtocolAdapter`(P1 新增字段映射代码,**承认非复用**)
3. 引入 L4.5 双层抽象:`ProtocolAdapterEr` (type-erased 基类, 供 ChStreamModuleBase 持有) + `ProtocolAdapter<InB, OutB>` (模板, 子类实现)

### 1.3 三类工作流冲突

| 工作流 | 需求 | 当前困境 |
|--------|------|---------|
| **早期架构探索** | 跑参数扫描(topology / link latency / VC count) | 每次协议字段变化都要改 6+ 文件 + 重新编译 + 跑回归 |
| **后期协议实现** | 切具体协议(AXI4 / CHI / PCIe FC) | 当前无统一机制,依赖 `axi4_mapper.cc` 个例 + PCIe FC 手写在 `pcie_link_layer_tlm.cc` |
| **跨模块契约验证** | Packet 字段语义稳定性 | 当前 Packet 字段"渐进式遗忘",部分字段成死字段 |

**核心矛盾**: 性能评估阶段需要"协议无关"快速迭代;协议实现阶段需要"协议特化"高保真建模;契约验证阶段需要 Packet 元数据稳定。三类工作流的工具链不能互相影响。

---

## 2. 总体架构

### 2.1 升级后的 8 层架构

```
┌── HOST (UE 进程) ───────────────────────────────────────────────────────┐
│ L8 用户态 driver / 23 ABI (cpptlm_emulator.h) ──── 冻结边界 [ABI]       │
└──┬───────────────────────────────────────────────────────────────────────┘
   │ C ABI(跨进程/SO, 0 变更)
┌──▼────────── DGpuBoard (shell, src/tlm/gpu/dgpu_board_shell.cc) ────────┐
│ L7 板级门面: BAR0 MMIO / BAR1 FB fast-path / BAR2 VRAM aperture          │
│ simulation_mode → set_mode(Functional|Timing) [MODE SWITCH]              │
│ network.mode → set_protocol_adapter() [PROTOCOL SWITCH]                 │
└──┬──────────────────────────────────────────────────────────────────────┘
   │
┌──▼────────── L6 Bundle 类型 (协议特化 typed POD) ──────────────────────┐
│   PcieTlpBundle / CrBundle / DoorbellBundle / MsixBundle / NoCFlitBundle │
│   └─ [BUNDLE_FIELD_WIDTH 编译期锁 + is_trivially_copyable 断言]           │
└──┬──────────────────────────────────────────────────────────────────────┘
   │
┌──▼────────── L4 StreamAdapter 5 子类 (协议中立, 模板只绑 BundleT) ──────┐
│   ├─ StreamAdapter<Module, ReqB, RespB> (单端口)                         │
│   ├─ MultiPortStreamAdapter<M, Rq, Rp, N> (N 端口同构)                   │
│   ├─ DualPortStreamAdapter<M, PE_Rq, PE_Rp, Net_Rq, Net_Rp> (2 组非对称) │
│   ├─ BidirectionalPortAdapter<M, B, N> (N 端口 + Credit)                │
│   └─ Axi4StreamAdapter (3 通道) + Axi4Mapper (OOO)                       │
└──┬──────────────────────────────────────────────────────────────────────┘
   │ pack_with_packet / unpack_with_packet (Step 2.5 + Step 5.5 写 Packet 协议字段, 见 §3.2)
┌──▼────────── ★ L4.5 协议适配层 (本方案新增, 嵌入 L4 与 L0 Packet 之间) ──┐
│   ProtocolAdapterEr (type-erased 基类, ChStreamModuleBase 持有) +       │
│   ProtocolAdapter<InB, OutB> (模板基类, 继承 ProtocolAdapterEr)            │
│   ├─ AbstractProtocolAdapter<FlitT> (Identity pack_with_packet, 零开销)   │
│   ├─ AxiProtocolAdapter / ChiProtocolAdapter / PcieTlpProtocolAdapter   │
│   └─ DualPortProtocolAdapter (组合持有 DualPortStreamAdapter* + 新增字段 │
│                                映射, ~30-50 行 P1 新增代码, 不复用, 见 §2.3.2) │
└──┬──────────────────────────────────────────────────────────────────────┘
   │ valid/ready handshake (InputStreamAdapter::process 在 Step 5.5 触发)
┌──▼────────── L3 ChStream 抽象 (Gem5 MessageBuffer 级别) ────────────────┐
│   InputStreamAdapter<BundleT> + OutputStreamAdapter<BundleT>            │
│   + ChStreamInitiatorPort / ChStreamTargetPort                          │
└──┬──────────────────────────────────────────────────────────────────────┘
   │
┌──▼────────── L2 MasterPort / SlavePort (继承 SimplePort, +PortStats) ──┐
└──┬──────────────────────────────────────────────────────────────────────┘
   │
┌──▼────────── L1 SimplePort + PortPair (同步直调) ───────────────────────┐
└──┬──────────────────────────────────────────────────────────────────────┘
   │
┌──▼────────── ★ L0.5 Packet 元数据层 (本方案升级, 既有 276 行 Packet) ───┐
│   ├─ 既有字段: stream_id/seq_num/cmd/type/route_path/hop_count/vc_id/   │
│   │            flow_id/priority/dependents/src_cycle/dst_cycle/original_req│
│   └─ ★ 新增字段: flit_category (uint8_t) / protocol_id (uint8_t) /        │
│                  bridge_id (uint16_t)                                    │
│   └─ [static_assert(sizeof(Packet) <= 320) 编译期锁, 类外, 见 §2.2.2]    │
└──┬──────────────────────────────────────────────────────────────────────┘
   │
┌──▼────────── L0 Packet::payload (tlm_generic_payload — 唯一数据载体) ──┐
└─────────────────────────────────────────────────────────────────────────┘
```

**关键差异**(vs PNN 蓝图):
- PNN 在 L5 Bundle 与 L4 StreamAdapter 之间插入 L_PNN
- **本方案在 L4 StreamAdapter 与 L0 Packet 之间插入 L4.5 + 升级 L0.5 Packet 元数据**
- 理由:**`Packet` 已经是 Gem5 MessageBuffer 的 70%**(`stream_id`/`seq_num`/`vc_id`/`priority`/`flow_id`/`route_path`/`hop_count` 7 字段直接对应,但缺 MessageBuffer 的 ordered/peek/pop 显式 API,见 §1.2)——不应该绕过它;而是把它从"被动壳"变成"语义元数据载体"

### 2.2 Packet 字段升级详情

#### 2.2.1 既有字段语义重映射(2 处修改 + 9 处保持)

读 `packet.hh:36-66` 后,梳理既有字段的"名实相符"目标:

| 字段 | 重映射策略 | 实施阶段 | 类型/语义变化 |
|------|----------|---------|--------------|
| `stream_id` | **保持**——ChStream 标识 (L3 层) | 既有 | ❌ 无 |
| `seq_num` | **保持** | 既有 | ❌ 无 |
| `vc_id` | **保持** | 既有 | ❌ 无 |
| `flow_id` | **保持** | 既有 | ❌ 无 |
| `priority` | **保持** | 既有 | ❌ 无 |
| `route_path` | **改型**——`std::vector<std::string>` → `std::array<char, 32>`(固定 32 字节,足够 4 个 module name,**P1 实施时根据实际拓扑调研再决定容量**) | P1 | ⚠️ 类型改 + 消费者更新 |
| `hop_count` | **保持** | 既有 | ❌ 无 |
| `dependents` | **保持** | 既有 | ❌ 无 |
| `cmd` (CmdType enum) | **扩展**——加 CFG_READ/CFG_WRITE/MSG/CPL 4 个 PCIe 语义值 | P1 | ⚠️ enum 值增加 |
| `type` (PacketType enum) | **保持**——REQ/RESP/STREAM/CREDIT 4 态 | 既有 | ❌ 无 |
| `payload`/`src_cycle`/`dst_cycle`/`original_req` | **保持** | 既有 | ❌ 无 |

**P1 修改既有字段清单**(诚实承认,**不是"0 改"**):

1. **`route_path` 改型**: `std::vector<std::string>` → `std::array<char, 32>`(32 字节足够 4 个 module name,P1 实施时根据实际拓扑调研再决定容量)
   - 消费者更新: `src/tlm/coherent_xbar_tlm.cc:65` `copy->route_path = pkt->route_path;` → 改为 memcpy 或重写为 char 数组赋值
   - 全仓 grep `route_path` 量化回归面(预计 < 10 处,均局部赋值/拷贝)
2. **`CmdType` enum 扩 4 值**: 新增 `CMD_CFG_READ=3` / `CMD_CFG_WRITE=4` / `CMD_MSG=5` / `CMD_CPL=6`
   - 风险:enum 序列化(如 log/SQLite 持久化)需 `BUNDLE_FIELD_WIDTH` 锁,已有先例(per `noc_bundles_tlm.hh:26-50` 16 字段锁)

#### 2.2.2 新增 3 个字段(P1 末尾追加)

```cpp
// include/core/packet.hh 新增 (P1 实施)
enum CmdType {
    CMD_INVALID = 0,
    CMD_READ    = 1,
    CMD_WRITE   = 2,
    // 新增 (P1): 协议命令码抽象 (PCIe CFG/MSI/CPL)
    CMD_CFG_READ     = 3,   // PCIe config read
    CMD_CFG_WRITE    = 4,
    CMD_MSG          = 5,   // posted write (MSI-X / doorbell)
    CMD_CPL          = 6,   // completion
};

class Packet {
    // ... 既有字段 (276 行, P1 改 2 处 + 增 3 字段, 见 §2.2.1) ...

    // ========== 新增: 协议中立元数据 (P1, 放末尾避免影响 offset) ==========

    // flit_category (4 类: 与 NoCFlitBundle 一致, 避免命名冲突)
    // ⚠️ 命名协调: NoCFlitBundle::flit_category 既有 (per include/bundles/noc_bundles_tlm.hh:45)
    //    含义: 0=REQUEST, 1=RESPONSE, 2=CREDIT (3 态)
    // 本方案 Packet::flit_category 沿用同一枚举,避免新增独立枚举
    uint8_t flit_category = 0;  // 0=REQUEST, 1=RESPONSE, 2=CREDIT, 3=reserved (per NoCFlitBundle)

    // protocol_id (协议标识, 编译期锁)
    // 0 = abstract (canonical NoCFlitBundle)
    // 1 = AXI4
    // 2 = CHI
    // 3 = PCIe TLP
    // 4 = CXL
    uint8_t protocol_id = 0;

    // bridge_id (ProtocolAdapter 实例 id, 用于 Stats)
    uint16_t bridge_id = 0;

    // 编译期锁 — 必须放在类外! 类内 Packet 是不完整类型
    // (类内 Packet 仍是不完整类型, `sizeof` 非法, GCC/Clang 报 invalid application of 'sizeof')
    // 实际位置: packet.hh 末尾
}; // class Packet

// 类外: Packet 大小预算
static_assert(sizeof(Packet) <= 320, "Packet size budget exceeded");
```

**3 个新增字段总计**: 1 + 1 + 2 = **4 字节**(新增字段末尾追加)。
**`route_path` 改型**:`std::vector<std::string>` (24 字节 std::vector header + 动态堆分配,实际 sizeof = 24B) → `std::array<char, 32>` (固定 32 字节)。**实际节省**:从动态堆分配(每次 push_back 走 new/delete)改为固定大小,**sizeof 增 +8B**(32B - 24B),**但消除堆分配开销**。
**Packet 类大小估算**(per packet.hh 实际字段):基线 ~136 字节(7 个 uint64_t + 4 个 int + 1 个 uint8_t + 1 个 atomic + vector<string> + payload ptr);P1 后 ~144 字节(+ 3 新字段 + route_path +8B)。

#### 2.2.3 `Packet::reset()` 必须重置新字段(P1 必做)

**关键警告**(default member initializer 只在构造期生效):

**default member initializer 只在构造期生效**,`reset()`(`packet.hh:246-264`)是 PacketPool 复用的**唯一重置点**——必须显式清零新字段,否则:
- 同一 pool 中一个 `protocol_id=2` (CHI) 的包释放后,下次 `acquire()` 复用时**残留 CHI 标识** → 静默跨协议串包

**P1 必须修改** `Packet::reset()`:

```cpp
// include/core/packet.hh (P1 增量, +3 行: 3 条赋值)
void reset() {
    // ... 既有重置逻辑 (packet.hh:248-264, 0 改) ...

    // P1 增量: 重置新字段, 防串包
    flit_category = 0;
    protocol_id = 0;
    bridge_id = 0;
}
```

#### 2.2.4 既非"0 改"也非"大改"的诚实定位

**承诺**(修订版):
- ✅ **0 改 Packet ext API**(`get_transaction_id`/`set_transaction_id`/`get_error_code` 等)
- ✅ **0 改 frozen headers**
- ✅ **0 改 23 ABI**
- ⚠️ **P1 增量修改 Packet 既有 2 字段**(`CmdType` enum 扩 4 值 + `route_path` 改型)
- ✅ **P1 末尾新增 3 个字段**(`flit_category`/`protocol_id`/`bridge_id`)
- ✅ **P1 必须修改 `Packet::reset()` 重置新字段**(防串包)

**回归面评估**:
- `route_path` 改型回归面: 全仓 grep 估计 < 10 处局部赋值(`coherent_xbar_tlm.cc:65` 已知 1 处)
- `CmdType` enum 扩展: 仅影响 enum 值使用方,**无破坏性**(C++ enum 扩展是兼容操作)
- 新增 3 字段: 默认值 0,无初始消费者时零行为变化

### 2.3 L4.5 ProtocolAdapter 设计

#### 2.3.1 抽象基类(模板 + type-erased 双层)

```cpp
// include/framework/protocol_adapter_er.hh (P1 实施, type-erased 基类)
namespace cpptlm {

// 前向声明 Packet (per include/core/packet.hh)
class Packet;

/**
 * @brief ProtocolAdapterEr - type-erased 基类, 供 ChStreamModuleBase 持有
 *
 * 解决 3 个问题:
 *   1. ChStreamModuleBase 不能持有 ProtocolAdapter<InB, OutB> 模板实例
 *      (ChStreamModuleBase 不是模板类, 无法传递 InB/OutB)
 *   2. pack/unpack 需要写 Packet 字段, 必须接收 Packet* 参数
 *   3. 与 ADR-DGPU-14 §2.1 候选 C 的 "StreamAdapter 子类继承" 冲突
 *      → 改为 type-erased 基类, ChStreamModuleBase 持有 ProtocolAdapterEr*
 *
 * 设计原则:
 *   - pack_with_packet/unpack_with_packet 接收 Packet* 写协议字段
 *   - 不继承 StreamAdapterBase (per ADR-DGPU-14 修订)
 *   - 切换协议必须 idle drain (不可逆)
 */
class ProtocolAdapterEr {
public:
    virtual ~ProtocolAdapterEr() = default;

    // 核心: 协议转换 + 写 Packet 协议字段
    virtual void pack_with_packet(Packet* pkt) = 0;
    virtual void unpack_with_packet(Packet* pkt) = 0;

    // 配置 (可选)
    virtual void configure(const nlohmann::json& cfg) {}

    // bridge_id 标识 (ModuleFactory 注入, 写 pkt->bridge_id)
    uint16_t bridge_id() const { return bridge_id_; }
    void set_bridge_id(uint16_t id) { bridge_id_ = id; }

    // 协议标识 (写 pkt->protocol_id)
    uint8_t protocol_id() const { return protocol_id_; }
    void set_protocol_id(uint8_t id) { protocol_id_ = id; }

    // 性能指标
    struct Stats {
        uint64_t pack_cycles = 0;
        uint64_t unpack_cycles = 0;
        uint64_t unpack_wait_cycles = 0;
        uint64_t conversions = 0;
    };
    Stats stats_;
    const Stats& stats() const { return stats_; }

private:
    uint16_t bridge_id_ = 0;
    uint8_t protocol_id_ = 0;  // 0=abstract, 1=AXI4, 2=CHI, 3=PCIe TLP, 4=CXL
};

} // namespace cpptlm


// include/framework/protocol_adapter.hh (P1 实施, 模板基类 — 继承 ProtocolAdapterEr)
namespace cpptlm {

/**
 * @brief ProtocolAdapter - 协议中立层与具体总线层的转换器 (模板版)
 *
 * 类比 Gem5 Ruby NetworkInterface
 *
 * 注意: 本类**继承** ProtocolAdapterEr, 通过模板参数 InB/OutB 表达协议两端
 *       **不存在** `ProtocolAdapterBase` 非模板类型 (前轮评审发现,已修复为 ProtocolAdapterEr + ProtocolAdapter<InB,OutB> 双层)
 */
template <typename InBundleT, typename OutBundleT>
class ProtocolAdapter : public ProtocolAdapterEr {
public:
    // 模板版 pack/unpack 不直接用 (Packet* 由 type-erased 入口传入)
    // 子类重写 pack_with_packet / unpack_with_packet
    void pack_with_packet(Packet* pkt) override {
        // 默认实现: 不写 Packet 协议字段 (abstract mode)
        InBundleT in;
        OutBundleT out;
        // 实际子类 (AxiProtocolAdapter 等) 重写时:
        //   1) 从 pkt->payload deserialize 进 in
        //   2) pack(in, out)
        //   3) pkt->protocol_id = this->protocol_id()
        //   4) pkt->bridge_id = this->bridge_id()
        //   5) serialize(out, pkt->payload)
    }
    void unpack_with_packet(Packet* pkt) override {
        // 默认实现: 不写 Packet 协议字段 (abstract mode)
    }

    // 子类重写的核心协议转换 (无 Packet*)
    virtual void pack(const InBundleT& in, OutBundleT& out) = 0;
    virtual void unpack(const OutBundleT& in, InBundleT& out) = 0;
};

// 基线: Identity adapter (零开销, 用于性能评估阶段)
template <typename FlitT>
class AbstractProtocolAdapter : public ProtocolAdapter<FlitT, FlitT> {
public:
    void pack(const FlitT& in, FlitT& out) override { out = in; }
    void unpack(const FlitT& in, FlitT& out) override { out = in; }
    void pack_with_packet(Packet* pkt) override {
        // Abstract mode: 不修改 Packet 协议字段 (由 Packet::reset() 保证默认值 0)
    }
    void unpack_with_packet(Packet* pkt) override {
        // Abstract mode: 不修改 Packet 协议字段
    }
};

} // namespace cpptlm
```

**关键命名决策**(修订):
- **双层架构**:
  - `cpptlm::ProtocolAdapterEr` (type-erased 基类, 供 ChStreamModuleBase 持有, **接收 Packet***)
  - `cpptlm::ProtocolAdapter<InB, OutB>` (模板类, 继承 ProtocolAdapterEr, 子类重写 `pack_with_packet(Packet*)`)
- **不存在** `cpptlm::ProtocolAdapterBase` 非模板类型
- **解决 pack/unpack 写 Packet 字段问题**(前轮评审发现):通过 `pack_with_packet(Packet*)` 入口,**显式接收 Packet* 写协议字段**
- **解决与 ADR-DGPU-14 候选 C 冲突**: ProtocolAdapterEr **不继承** StreamAdapterBase,独立类型(组合 Pimpl / Strategy 模式)

#### 2.3.2 与 DualPortStreamAdapter 的桥接关系(诚实定位, 改为组合)

**事实纠正**(基于 `dual_port_stream_adapter.hh` 实地代码):

读 `include/framework/dual_port_stream_adapter.hh:193-199` 实际代码:

```cpp
// 实际实现 (line 193-199) — 仅做 Packet 透传, 无任何字段映射
void process_pe_request_input(Packet* pkt) {
    if (!pkt || !pkt->payload) return;
    auto& req_in = module_->pe_req_in();
    if (!req_in.valid()) {
        req_in.process(pkt);  // ← 无 PE↔Net 字段映射
    }
}
```

**`DualPortStreamAdapter` 实际只做 "Bundle 转 Packet, 同 Bundle 类型透传"**——**不做协议字段映射**。

**修正后的桥接路径**(P1 实施, **改为组合而非继承**):

```
现有: framework/dual_port_stream_adapter.hh
  └─ class DualPortStreamAdapter<M, PE_Rq, PE_Rp, Net_Rq, Net_Rp>
       └─ process_pe_request_input() 仅透传, 无字段映射

新增: framework/protocol_adapter_er.hh
  └─ class ProtocolAdapterEr (type-erased 基类)
       └─ pack_with_packet/unpack_with_packet 虚函数 (接收 Packet*)

新增: framework/protocol_adapter.hh
  └─ class ProtocolAdapter<InB, OutB> : public ProtocolAdapterEr
       └─ pack/unpack 纯虚函数 (无 Packet*)

桥接: framework/dual_port_protocol_adapter.hh
  └─ class DualPortProtocolAdapter<M, PE_Rq, PE_Rp, Net_Rq, Net_Rp>
       : public ProtocolAdapter<PE_Rq, Net_Rq>  // ← 继承 ProtocolAdapterEr, 不继承 StreamAdapter
       ├─ pack_with_packet() 内做 PE_Rq → Net_Rq 字段映射 (P1 新增 ~30-50 行)
       └─ 持有 DualPortStreamAdapter<M, PE_Rq, PE_Rp, Net_Rq, Net_Rp>* (组合, 不继承)
```

**关键架构决策**(前轮评审修订):
- ❌ **不继承** `DualPortStreamAdapter`(避免与 ADR-DGPU-14 §2.1 候选 C "拒绝双重继承"冲突)
- ✅ **组合**持有 `DualPortStreamAdapter*`(标准 PImpl / Strategy 模式)
- ✅ `DualPortProtocolAdapter` 通过 `ProtocolAdapterEr::pack_with_packet(Packet*)` 入口,接收 Packet 写协议字段
- ✅ 解决"pack/unpack 无 Packet* 无法写字段"问题

**关键约束**(诚实承认):
- `DualPortProtocolAdapter` 不**复用**既有字段映射逻辑(因为 DualPortStreamAdapter 没有)
- 实际**新增** PE_Rq → Net_Rq 字段映射代码,约 30-50 行
- NICTLM 等既有模块**零修改**——继续用 DualPortStreamAdapter
- 只有**新模块**用 DualPortProtocolAdapter

**好处**:
- NICTLM 等既有模块**零修改**——继续用 DualPortStreamAdapter
- 新模块可通过 `DualPortProtocolAdapter` 显式实现字段映射
- 与 Gem5 NetworkInterface 的类比变得**严密**——adapter 真的做字段映射
- **不破坏 ADR-DGPU-14 修订版**的"单一职责"约束(组合而非继承)

### 2.4 Packet 元数据参与(通过 pack_with_packet 入口)

**关键洞察**:`Packet::protocol_id` 与 `Packet::bridge_id` 字段**作为 ProtocolAdapter 的"上下文"** 参与 pack/unpack。

**修订**: §2.3.1 引入 `ProtocolAdapterEr::pack_with_packet(Packet*)` 后,Packet 字段写入路径**清晰**:

```cpp
// axi_protocol_adapter.cc (P2 实施示意, 完整 pack_with_packet 实现)
class AxiProtocolAdapter : public ProtocolAdapter<NoCFlitBundle, Axi4Bundle> {
private:
    std::unordered_map<uint8_t, std::queue<Axi4Bundle>> outstanding_rid_;
public:
    // pack 模板版 (无 Packet*) — 实际协议字段映射
    void pack(const NoCFlitBundle& in, Axi4Bundle& out) override {
        out.addr = in.address;
        out.id   = in.src_node;  // ← 用 NoCFlitBundle.src_node (per noc_bundles_tlm.hh:26-50) 作为 AXI rid
        // ... 完整映射 ...
        stats_.conversions++;
    }

    // pack_with_packet type-erased 版 (有 Packet*) — 写 Packet 协议字段
    void pack_with_packet(Packet* pkt) override {
        // 1) 反序列化 pkt->payload 进 in (使用真实 API: bundles::deserialize_bundle)
        NoCFlitBundle in;
        bundles::deserialize_bundle(pkt->payload->get_data_ptr(), pkt->payload->get_data_length(), in);

        // 3) 协议转换 in → out
        Axi4Bundle out;
        pack(in, out);

        // 4) 写 Packet 协议字段 (核心!)
        pkt->protocol_id = this->protocol_id();  // = 1 (AXI4)
        pkt->bridge_id = this->bridge_id();      // ModuleFactory 注入
        pkt->flow_id = out.id;  // AXI rid 写回 Packet
        pkt->flit_category = 0;  // REQUEST (per NoCFlitBundle 编码)

        // 5) 序列化 out 回 pkt->payload (使用真实 API: bundles::serialize_bundle)
        bundles::serialize_bundle(out, pkt->payload->get_data_ptr(), pkt->payload->get_data_length());
    }

    void unpack_with_packet(Packet* pkt) override {
        // 1) 反序列化 pkt->payload 进 out
        Axi4Bundle out;
        bundles::deserialize_bundle(pkt->payload->get_data_ptr(), pkt->payload->get_data_length(), out);

        // 2) OOO 关联 (复用 axi4_mapper.cc 逻辑)
        outstanding_rid_[out.id].push(out);

        // 3) 协议转换 out → in
        NoCFlitBundle in;
        unpack(out, in);

        // 4) 写 Packet 协议字段
        pkt->protocol_id = this->protocol_id();
        pkt->bridge_id = this->bridge_id();

        // 5) 序列化 in 回 pkt->payload
        bundles::serialize_bundle(in, pkt->payload->get_data_ptr(), pkt->payload->get_data_length());
    }
};
```

**Packet 字段的"协议中立身份证明"作用**:

| Packet 字段 | 层级 | 语义 | 写入路径 |
|-----------|------|------|---------|
| `stream_id` | L3 | ChStream id | StreamAdapter::send (既有) |
| `flow_id` | L4.5 | AXI rid / CHI txn id | ProtocolAdapter::pack_with_packet (P2+) |
| `vc_id` | L2 | virtual channel (Router 路由) | RouterTLM::tick (既有) |
| `protocol_id` | L4.5 | 协议标识 (bridge 注入) | ProtocolAdapterEr::pack_with_packet (P1+) |
| `bridge_id` | L4.5 | bridge 实例 id (Stats 用) | ProtocolAdapterEr::set_bridge_id + pack_with_packet (P1+) |
| `flit_category` | L4.5 | REQUEST/RESPONSE/CREDIT 分类 (与 NoCFlitBundle 一致) | ProtocolAdapter::pack_with_packet (P2+) |

---

## 3. 数据流的 8-hop 拆解(L4.5 注入后)

### 3.1 既有 5-hop 流程回顾(per communication-network.md §3.3)

以 `CacheTLM → CrossbarTLM → MemoryTLM` 路径的 **CacheTLM.req_out → CrossbarTLM.req_in[0]** 为例:

```
Step 1: CacheTLM::tick() — OutputStreamAdapter<CacheReqBundle>::write
Step 2: StreamAdapter::tick() — OutputStreamAdapter::send (serialize_bundle)
Step 3: MasterPort::send — SimplePort::send (同步直调)
Step 4: SimplePort::send — other->recv(pkt)
Step 5: ChStreamTargetPort::recvReq — adapter_->process_request_input
Step 6: StreamAdapter::process_request_input — 按 pkt->type 路由
Step 7: InputStreamAdapter::process — deserialize_bundle, stream_valid_=true
```

### 3.2 升级后 8-hop 流程(L4.5 ProtocolAdapter 注入)

**关键职责分工**(StreamAdapter 不写协议字段):
- **StreamAdapter**: 管 `Port ↔ ch_stream` 转换,**不写 Packet 协议字段**
- **ProtocolAdapterEr** (type-erased 基类): 管 **Packet 协议字段写入**,通过 `pack_with_packet(Packet*)` / `unpack_with_packet(Packet*)` 入口
- **ProtocolAdapter<InB, OutB>** (模板): 管 **Bundle 协议转换**(pack/unpack 不接 Packet*),由子类 (AxiProtocolAdapter 等) 重写,内部在 pack_with_packet 内调 pack

**关键时序修正**(use-after-free 风险修复):
- 步骤 4 后 `pkt` 立即归还 PacketPool,不可在 Step 6 写字段
- **修正方案**:**协议字段写入时机改为 Step 2 后**(发送侧)与 **Step 5 同步**(接收侧),均在 `pkt` 仍在 module 持有期

```
Step 1: CacheTLM::tick() — OutputStreamAdapter<CacheReqBundle>::write(bundle)
        └─ module 业务逻辑 (不感知 ProtocolAdapter)

Step 2: StreamAdapter::tick() — OutputStreamAdapter::send(req_out_port_, PKT_REQ)
        └─ Packet* pkt = PacketPool::get().acquire()
        └─ serialize_bundle(stream_data_, pkt->payload)
        └─ 此时 pkt->protocol_id=0/bridge_id=0/flit_category=0 (Packet::reset() 默认值)
        └─ ★ NEW Step 2.5: ch_mod->apply_protocol_adapter_on_send(pkt)
        │   └─ pkt->protocol_id = adapter_->protocol_id()
        │   └─ pkt->bridge_id = adapter_->bridge_id()
        │   └─ pkt->flit_category = 0 (REQUEST) / 1 (RESPONSE) / 2 (CREDIT) (per NoCFlitBundle)
        │   └─ pkt->flow_id = ... (per protocol 映射, AXI 用作 rid)
        └─ port->send(pkt) — MasterPort::send
        └─ 此时 pkt 仍在 module 持有期,字段写入安全

Step 3: MasterPort::send — SimplePort::send (同步直调)
        └─ other->recv(pkt)

Step 4: ChStreamTargetPort::recvReq — adapter_->process_request_input(pkt, port_idx_)
        └─ PacketPool::get().release(pkt)  // 既有同步 release

Step 5: StreamAdapter::process_request_input — 按 pkt->type 路由
        └─ module->req_in().process(pkt)  // deserialize_bundle, Bundle 已就位
        └─ ★ NEW Step 5.5: ch_mod->apply_protocol_adapter_on_recv(pkt)
        │   └─ 在 process(pkt) 同步路径上, pkt 仍由 InputStreamAdapter 持有引用
        │   └─ 写入 pkt 协议字段(反向: 对端 bundle 反序列化进 pkt)
        └─ module->req_in().reset_valid() / 类似 cleanup (既有)

Step 6: RouterTLM::tick() — 按 vc_id 路由 + 1-cycle credit 模拟

Step 7: 对端 InputStreamAdapter::process — Bundle 已就位, 等模块 tick() 消费
        └─ Bundle 字段已含完整协议信息(Step 2.5 + 5.5 已写入 pkt→bundle 序列化)
```

**关键时序修订**(Step 2.5 + Step 5.5 写入):
- ❌ **旧方案**: Step 6 在 module tick() 触发,写已 release 的 pkt (use-after-free)
- ✅ **新方案**: 写入时机前移到 Step 2.5 (发送侧) / Step 5.5 (接收侧),均在 `pkt` 持有期内
- **实现要求**: ChStreamModuleBase 提供 `apply_protocol_adapter_on_send(Packet*)` 与 `apply_protocol_adapter_on_recv(Packet*)` 两个钩子(per §4.3 修订),**由 StreamAdapter::send 与 InputStreamAdapter::process 同步调用**

**Packet 字段"协议中立身份证明"**: 即使没有 ProtocolAdapter,Packet 字段(stream_id/flow_id/vc_id)也能独立承担部分语义(per §2.4 字段表)

---

## 4. ModuleFactory 集成

### 4.1 当前 9 步流程回顾(per communication-network.md §5)

| Step | 行号 | 职责 |
|------|------|------|
| 1 | 247-257 | PluginLoader dlopen |
| 2 | 262-345 | 从 REGISTRY 创建实例 |
| 3 | 347-358 | 解析 groups |
| 4 | 361-389 | sim_mod->instantiate(cfg) |
| 4.5 | 391-415 | simulate_instantiate 递归 |
| 5 | 420-490 | ConnectionResolver.resolveConnections |
| 6 | 492-615 | 建立 PortPair |
| 7a | 620-735 | ChStreamAdapterFactory 创建 |
| 7b | 755-847 | ChStream 连接 |
| 8 | 737-753 | StatsManager.register_group |
| 9 | 849-859 | incorporate_parent |

### 4.2 新增 Step 7c:ProtocolAdapter 注入(P1 实施)

```cpp
// src/core/module_factory.cc (P1 增量, +30 行)
void ModuleFactory::instantiateAll(const json& config) {
    // ... 既有 Step 1-7b ...

    // === Step 7c (本方案增量): ProtocolAdapter 注入 ===
    if (config.contains("network") && config["network"].contains("mode")) {
        std::string mode = config["network"]["mode"].get<std::string>();
        ProtocolAdapterFactory& adapter_factory = ProtocolAdapterFactory::get();

        if (!adapter_factory.knows(mode)) {
            throw std::runtime_error("Unknown network.mode: " + mode);
        }

        // 遍历所有 ChStream 模块, 注入 ProtocolAdapter
        for (auto& ch_mod : chstream_modules) {
            // ProtocolAdapter<InB, OutB> 是模板类, 工厂返回 type-erased 基类指针
            auto* adapter = adapter_factory.create(mode);  // 返回 ProtocolAdapterEr*
            adapter->configure(config["network"]);

            // 分配 bridge_id (ModuleFactory 维护全局 counter)
            uint16_t bridge_id = next_bridge_id_++;
            adapter->set_bridge_id(bridge_id);

            // 注入到 ChStreamModuleBase
            ch_mod->set_protocol_adapter(adapter);

            // ★ 关键: Step 6 触发由 ChStreamModuleBase::tick() 调用 adapter->pack_with_packet/unpack_with_packet
            //    (不修改 StreamAdapter::send 签名, 通过 ChStreamModuleBase 钩子调用)
        }
    }
}
```

**重要修复**(Step 7c 不修改 StreamAdapter::send 签名):
- Step 7c **不修改** `OutputStreamAdapter::send()` 签名(`include/framework/stream_adapter.hh:136-162` 实际签名是 `send(MasterPort*, PacketType, uint64_t)`)
- ProtocolAdapter 通过 ChStreamModuleBase::tick() 调用,**ModuleFactory Step 7c 仅注入引用,触发由模块代码负责**
- 工厂返回 `ProtocolAdapterEr*` (type-erased 基类),**不返回模板实例指针**(否则需知道具体 InB/OutB)

### 4.3 `ChStreamModuleBase` 新增 API

```cpp
// include/core/chstream_module.hh (P1 增量, +25 行)
#include "framework/protocol_adapter_er.hh"  // P1 新增
class Packet;  // 前向声明

class ChStreamModuleBase : public SimObject {
public:
    // ... 既有 API (76 行, 0 改) ...

    // === 本方案增量 (P1) ===
    // 类型: cpptlm::ProtocolAdapterEr* (type-erased 基类)
    // 原因: ChStreamModuleBase 不是模板类, 无法持有 ProtocolAdapter<InB, OutB> 实例
    virtual void set_protocol_adapter(cpptlm::ProtocolAdapterEr* adapter) {
        adapter_ = adapter;
    }
    cpptlm::ProtocolAdapterEr* protocol_adapter() const { return adapter_; }

    // P1 增量: 两个时序钩子, 由 StreamAdapter::send / InputStreamAdapter::process 同步调用
    // 默认空实现 — abstract mode 不操作 (Packet::reset() 保证默认值 0)
    virtual void apply_protocol_adapter_on_send(Packet* pkt) {
        if (adapter_) adapter_->pack_with_packet(pkt);
    }
    virtual void apply_protocol_adapter_on_recv(Packet* pkt) {
        if (adapter_) adapter_->unpack_with_packet(pkt);
    }

private:
    cpptlm::ProtocolAdapterEr* adapter_ = nullptr;
};
```

**关键约束**(时序修复,§3.2 修订):
- `set_protocol_adapter` 是**虚函数但有默认实现**(与 `set_stream_adapter` 是 pure virtual 区分)
- 默认实现 `adapter_ = adapter` 不抛异常,**向后兼容**——既有 75+ 模块无需修改
- **两个时序钩子** `apply_protocol_adapter_on_send` 与 `apply_protocol_adapter_on_recv`:
  - 由 `OutputStreamAdapter::send()` 在 Step 2.5 同步调用 (P1 需在 `include/framework/stream_adapter.hh:136-162` 增量 +3 行)
  - 由 `InputStreamAdapter::process()` 在 Step 5.5 同步调用 (P1 需在 `include/framework/stream_adapter.hh:42-93` 增量 +3 行)
- **触发时序**: 两个钩子均在 `pkt` 持有期内调用,**避免 use-after-free**
- **回归面**: 既有 75+ 模块无需修改(钩子默认实现是 no-op,abstract mode 零行为变化)

**触发点示例**(per §3.2):

```cpp
// OutputStreamAdapter::send (P1 增量, +3 行)
void OutputStreamAdapter<BundleT>::send(MasterPort* port, PacketType type, uint64_t stream_id) {
    Packet* pkt = PacketPool::get().acquire();
    // ... 既有序列化逻辑 ...
    ch_mod_->apply_protocol_adapter_on_send(pkt);  // ← NEW Step 2.5
    port->send(pkt);
}

// InputStreamAdapter::process (P1 增量, +3 行)
void InputStreamAdapter<BundleT>::process(Packet* pkt) {
    deserialize_bundle(pkt->payload, stream_data_);
    ch_mod_->apply_protocol_adapter_on_recv(pkt);  // ← NEW Step 5.5
    stream_valid_ = true;
    // pkt 在这里可 release (既有逻辑)
}
```

---

## 5. JSON 配置契约

### 5.1 完整字段(对齐 `simulation_mode` 正交)

```json
{
  "simulation_mode": "functional",
  "network": {
    "mode": "abstract",
    "topology": "mesh",
    "dims": [2, 2],
    "link_latency_cycles": 10,
    "link_bw_bytes_per_cycle": 8,
    "vc_count": 4,
    "routing": "xy",

    "abstract": {},
    "axi4": { "outstanding": 16, "data_width": 64, "wstrb": true },
    "chi":  { "flit_types": ["REQ", "RSP", "DAT", "SNP"] },
    "pcie_tlp": { "credit_P": 32, "credit_NP": 16, "credit_Cpl": 16 },
    "cxl":  { "sub_protocol": "mem" }
  },
  "modules": [],
  "connections": []
}
```

**字段说明**:
- `simulation_mode`(L7): `"functional"` | `"timing"`,per ADR-DGPU-11(既有字段)
- `network.mode`(L4.5): `"abstract"` | `"axi4"` | `"chi"` | `"pcie_tlp"` | `"cxl"`(本方案新增)
- `network.topology`(L2): `"mesh"` | `"crossbar"` | `"butterfly"` | `"ring"`(per `dgpu_soc_timing_v1.json` 既有拓扑)
- `network.vc_count`(L2): virtual channel 数,默认 4
- `network.<mode>`: 各协议 adapter 特定配置(abstract 为空对象)

### 5.2 2 维组合:4 模式

| | functional | timing |
|---|-----------|--------|
| **abstract** | 性能评估 (基线) | 时序性能评估 |
| **axi4** | AXI4 功能验证 | AXI4 时序验证 |

---

## 6. 配置 Schema

### 6.1 `network.mode` 枚举

| Mode | 对应 adapter | 阶段 | 估时 |
|------|-------------|------|------|
| `"abstract"` | `AbstractProtocolAdapter<NoCFlitBundle>` | P1 | 0 (基线) |
| `"axi4"` | `AxiProtocolAdapter` | P2 | 2-3d |
| `"chi"` | `ChiProtocolAdapter` | P2 | 2-3d |
| `"pcie_tlp"` | `PcieTlpProtocolAdapter` | P3 | 1-2d |
| `"cxl"` | `CxlProtocolAdapter` | P4 | 1-2d |

**字符串 vs enum class 决策**: 选**字符串**——便于扩展新模式(不必重新编译枚举类),与 `simulation_mode: functional|timing` 风格一致(per `dgpu_soc_timing_v1.json:4`)。

### 6.2 `network` 完整字段

```json
{
  "network": {
    "mode": "abstract",
    "topology": "mesh",
    "dims": [2, 2],
    "link_latency_cycles": 10,
    "link_bw_bytes_per_cycle": 8,
    "vc_count": 4,
    "routing": "xy",

    "abstract": {},
    "axi4": {},
    "chi": {},
    "pcie_tlp": {},
    "cxl": {}
  }
}
```

**字段语义说明**(JSON 无注释,以下为文档说明):
- `mode`: 协议模式, `"abstract"` / `"axi4"` / `"chi"` / `"pcie_tlp"` / `"cxl"`
- `topology`: 物理拓扑, `"mesh"` / `"crossbar"` / `"ring"` / `"butterfly"`
- `dims`: array of int, topology 维度(仅 mesh/butterfly 需要)
- `link_latency_cycles`: int, 每条 link 延迟
- `link_bw_bytes_per_cycle`: int, 每条 link 带宽
- `vc_count`: int, virtual channel 数,默认 4
- `routing`: 路由算法, `"xy"` / `"west_first"` / `"table"` / `"custom"`
- `abstract`/`axi4`/`chi`/`pcie_tlp`/`cxl`: 各协议 adapter 特定配置(P2-P4 逐步填充)

**注意**: `network.topology` 字段是 P1 增量(拓扑参数化),P0 阶段仅消费 `network.mode`。

---

## 7. 实施分阶段

### 7.1 P0 (1-2 周, 含 3 文件 build 改动, **非纯 doc-only**)

**诚实定位**(含 3 文件 build 改动):
- P0 任务含 1 个 config + 2 个测试,共 **3 个文件 build 改动**
- 不符合 AGENTS.md 严格 doc-only 豁免(0 函数签名/逻辑/构建改动)
- **不存在** "doc-only 分类豁免"先例:per `cpptlm-dgpu-soc-arch-truth-cleanup` §6 实际要求 `(c) 0 个构建产物/CMakeLists.txt 改动; (d) 0 个测试用例新增/修订`——本 P0 含 2 个测试 + 1 个 config,**严格说不满足豁免**
- 实际 KPI 影响: **P0 消耗 1 个 KPI slot**(与 P1-P4 各 1 slot 累计 5 slot)
- **KPI 启动前提**: per §0.2bis 当前 5/3 已超限,**P0 也无法启动**,除非季度复审 / Owner 决策释放 slot

**目标**: 用现有能力组合,验证本方案概念,跑 2×2 mesh baseline + Packet 字段使用现状审计

**任务清单**(诚实列出 3 文件 build 改动):
1. 起草 `cpptlm-packet-semantic-network` OpenSpec proposal(引用本蓝图 + gem5 调研,**归类待定**)
2. **build 改动 1**: 写 `configs/packet_semantic_mesh_2x2_baseline.json`(顶层 `configs/` 目录)
   - 2×2 mesh: 4 个 RouterTLM + 2 个 TrafficGenTLM
   - 内部 connections: src→router0→router1→dst
   - 不含 `network.mode` 字段(用现有 NoCFlitBundle 走 ChStream)
3. **build 改动 2**: 跑 Packet 字段使用现状审计:
   - `test/test_packet_field_audit.cc`: 扫所有 Packet 字段(stream_id/seq_num/vc_id/flow_id/priority/dependents),统计**哪些被设置/实际消费**
   - `[packet-audit]` ≥5 cases / 20 assertions
   - 产出 baseline 报告(Packet 字段"名实相符"现状表)
4. **build 改动 3**: 跑 baseline 测试:
   - `[pnn-baseline]` ≥2 cases / 10 assertions(复用 `[chstream]` + `[noc]` 标签)
5. 产出 baseline 报告(latency/throughput 数字 + Packet 字段审计表)作为 P1/P2 验证锚点

**触发条件**: 用户批准本方案 + KPI slot 释放(per §0.2bis 当前阻塞)

**不动边界**:
- 0 改 `include/core/*.hh`(Packet / ChStreamModuleBase / SimplePort 等)
- 0 改 `include/framework/*.hh`
- 0 改 frozen headers / 23 ABI

**估时**: 5d

**测试指标**: 6 标签 0 回归 + `[packet-audit]` ≥5 cases 通过 + `[pnn-baseline]` ≥2 cases 通过

### 7.2 P1 (1-2 月, 1 KPI slot, `cpptlm-packet-semantic-network`)

**目标**: 实施 Packet 字段升级 + ProtocolAdapter 模板基类 + AbstractProtocolAdapter + network.mode 解析

**任务清单**(诚实承认 P1 含 Packet 既有 2 字段修改 + 新增 3 字段 + reset() 修改):

1. **Packet 字段升级**:
   - `include/core/packet.hh` 增量:
     - `CmdType` enum 扩 4 值(CFG_READ/CFG_WRITE/MSG/CPL,**+4 行**)
     - `route_path` 改型(`vector<string>` → `std::array<char, 32>`,**+1 行**)
     - 新增 3 个字段 `flit_category`/`protocol_id`/`bridge_id`(**+12 行**)
     - `static_assert(sizeof(Packet) <= 320)` 编译期锁(**+1 行,放类外**)
     - `Packet::reset()` 重置新字段(**+3 行**,per §2.2.3 防串包)
   - `route_path` 消费者更新:
     - `src/tlm/coherent_xbar_tlm.cc:65` `copy->route_path = pkt->route_path;` → 改写为 char 数组赋值或 memcpy
     - 全仓 grep `route_path` 量化回归面(预计 < 10 处)
   - PacketPool/StreamAdapter 兼容性测试

2. **L4.5 ProtocolAdapter 模板基类**:
   - 新建 `include/framework/protocol_adapter.hh`(纯虚基类 + Stats,**~50 行**)
   - 新建 `include/framework/protocol_adapter_er.hh`(type-erased 基类,供 ChStreamModuleBase 持有,**~30 行**)
   - 新建 `include/framework/abstract_protocol_adapter.hh`(Identity pack/unpack,**~30 行**)
   - 新建 `src/framework/protocol_adapter.cc`(Identity 实现,**~30 行**)
   - 新建 `include/framework/protocol_adapter_factory.hh`(模式名 → adapter 类注册表,**~50 行**)
   - 新建 `src/framework/protocol_adapter_factory.cc`(**~30 行**)
   - 新建 `include/framework/dual_port_protocol_adapter.hh`(桥接 + PE_Rq → Net_Rq 字段映射,**~40 行,P1 新增**)

3. **ChStreamModuleBase 扩展**:
   - `include/core/chstream_module.hh` 增量(`set_protocol_adapter()` 默认空 + ProtocolAdapterEr 字段,**+15 行**)

4. **ModuleFactory Step 7c**:
   - `src/core/module_factory.cc` 增量(Step 7c 注入 ProtocolAdapter + bridge_id 分配,**+30 行**)

5. **CMakeLists.txt 显式列举**:
   - `src/framework/CMakeLists.txt` 增量(添加 protocol_adapter.cc + protocol_adapter_factory.cc + dual_port_protocol_adapter.cc,**+3 行**)
   - `test/CMakeLists.txt` 增量(添加 3 个 test 文件,**+3 行**)
   - per AGENTS.md:禁 GLOB,必须显式列举

6. **注册宏**:
   - `include/chstream_register.hh`(注意:**根 include**,非 `include/framework/`,per SHOULD_FIX B)增量(`REGISTER_CHSTREAM_PROTOCOL_ADAPTER` 宏,**+10 行**)

7. **测试**:
   - `test/test_packet_field_semantic.cc`(Packet 字段升级测试,**~200 行, ≥15 cases / 80 assertions**)
   - `test/test_protocol_adapter_abstract.cc`(AbstractProtocolAdapter 测试,**~150 行, ≥15 cases / 100 assertions**)
   - `test/test_dual_port_protocol_adapter_bridge.cc`(桥接测试,**~100 行, ≥10 cases / 50 assertions**)

**触发条件**: P0 baseline 通过 + KPI slot 释放(per §0.2bis,需 Owner 决策或季度复审触发)

**不动边界**:
- 全部 frozen headers (`pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h`)
- 23 ABI 签名
- Packet ext API (`get_transaction_id`/`set_transaction_id`/`get_error_code` 等)
- Packet::reset() 既有字段(仅新增 3 字段重置)

**修改边界**(诚实承认):
- Packet::CmdType enum 扩 4 值
- Packet::route_path 类型改
- Packet 新增 3 字段(末尾)
- Packet::reset() 重置新字段

**估时**: 10-15d

**测试指标**: `[packet-semantic]` ≥15 cases + `[pnn]` ≥15 cases + `[dual-port-protocol]` ≥10 cases,6 标签 0 回归,P0 baseline 数字复现

### 7.3 P2 (3-6 月, 1 KPI slot, `cpptlm-axi-chi-protocol-adapter`)

**目标**: 实施 `AxiProtocolAdapter` + `ChiProtocolAdapter` + Gem5 Ruby 对照验证

**任务清单**:
1. 新建 `include/framework/axi_protocol_adapter.hh`(模板 + 字段映射声明,**~80 行**)
3. 新建 `src/framework/axi_protocol_adapter.cc`(**~200 行, 80% 复用 `axi4_mapper.cc` 逻辑**)
4. 新建 `include/framework/chi_protocol_adapter.hh`(4 flit_type 模板,**~80 行**)
5. 新建 `src/framework/chi_protocol_adapter.cc`(**~200 行, REQ/RSP/DAT/SNP 4 类**)
7. 写 `configs/packet_semantic_mesh_2x2_axi4.json`(同 P0 拓扑, 仅 `mode: "axi4"`)
8. 写 `configs/packet_semantic_mesh_2x2_chi.json`(同 P0 拓扑, 仅 `mode: "chi"`)
9. 写 `test/test_protocol_adapter_axi4.cc`(**~200 行, ≥20 cases / 150 assertions**)
10. 写 `test/test_protocol_adapter_chi.cc`(**~200 行, ≥20 cases / 150 assertions**)
11. Gem5 Ruby 对照验证报告: 同拓扑同负载,latency 误差 <20% 验收线
12. 本蓝图文档增量更新(v0.1 → v0.2 反映 P2 实施)

**触发条件**: P1 通过 + Gem5 Ruby 对照验证接受

**不动边界**: 全部 frozen headers / 23 ABI / P1 已注册的所有 adapter

**估时**: 20-30d

**测试指标**: `[pnn-axi4]` ≥20 cases + `[pnn-chi]` ≥20 cases,6 标签 0 回归,Gem5 Ruby 对照 <20% 误差

### 7.4 P3 (6-12 月, 1 KPI slot, `cpptlm-pcie-tlp-protocol-adapter`)

**目标**: 多协议共存 + PcieTlpProtocolAdapter + 指标 dashboard 集成

**任务清单**:
1. 新建 `include/framework/pcie_tlp_protocol_adapter.hh`(6 kind + credit,**~80 行**)
2. 新建 `src/framework/pcie_tlp_protocol_adapter.cc`(**~200 行**)
3. 增量 `ModuleFactory` Step 7c 支持 per-connection 异构 adapter(`connections[].bridge` 字段)
4. 写 `test/test_protocol_adapter_pcie_tlp.cc`(**~200 行, ≥15 cases**)
5. 写 `test/test_multi_protocol_coexistence.cc`(**~200 行, ≥10 cases**)
6. 指标 dashboard 集成(与 `include/metrics/reporter.hh` 集成)
7. 本蓝图文档增量更新(v0.2 → v0.3 反映 P3 实施)

**触发条件**: P2 通过 + 真实多协议消费场景出现

**不动边界**: 全部 frozen headers / 23 ABI

**估时**: 30-45d

**测试指标**: `[pnn-pcie]` ≥15 cases + `[pnn-multi]` ≥10 cases,6 标签 0 回归

### 7.5 P4 (12+ 月, 1 KPI slot, `cpptlm-cxl-protocol-adapter`)

**目标**: CxlProtocolAdapter + 跨仓 ArchForge 联合验证

**任务清单**:
1. 新建 `include/framework/cxl_protocol_adapter.hh`(CXL.mem/CXL.cache/CXL.io 3 子协议,**~100 行**)
2. 新建 `src/framework/cxl_protocol_adapter.cc`(**~300 行, 复用 PcieTlp adapter + 加 CXL 特有字段**)
3. 跨仓 ArchForge 联合验证(per `docs/soc_arch/README.md` 占位)
4. 写 `test/test_protocol_adapter_cxl.cc`(**~200 行, ≥15 cases**)
5. 本蓝图文档增量更新(v0.3 → v1.0 反映 P4 实施 + 跨仓验证)

**触发条件**: P3 通过 + 跨仓 ArchForge CXL 拓扑成熟

**不动边界**: 全部 frozen headers / 23 ABI

**估时**: 30-45d

**测试指标**: `[pnn-cxl]` ≥15 cases,6 标签 0 回归,ArchForge 联合验证通过

---

## 8. 不破硬边界自检(诚实修订版)

| 边界 | 本方案触碰? | 说明 |
|------|-----------|------|
| **`pcie_endpoint_tlm.h`** | ❌ | ProtocolAdapter 在 `include/framework/`, 与 frozen pcie_endpoint_tlm 独立 |
| **`pcie_display_device.hh`** | ❌ | 同上 |
| **`pcie_bundles_tlm.hh`** | ❌ | 本方案不修改既有 Bundle, 仅新增 `protocol_adapter.hh` |
| **`cpptlm_emulator.h`** | ❌ | 本方案 0 改 23 ABI 签名 |
| **23 ABI 冻结** | ❌ | `cpptlm_emulator_create/mmio_read/...` 零修改 |
| **Packet ext API** (get_transaction_id/set_transaction_id/get_error_code/...) | ❌ | 零修改 |
| **Packet 既有字段(12 个全部保持)** (stream_id/seq_num/vc_id/flow_id/priority/hop_count/dependents/payload/src_cycle/dst_cycle/original_req/type — 其中 §2.2.1 表列举 9 个"语义重映射"对象 + 3 个额外 payload 域元数据) | ❌ | 0 改 |
| **Packet 既有字段(2 个修改)** (CmdType enum 扩 4 值 + route_path 改型) | ⚠️ P1 修改 | 见 §2.2.1 + §2.2.4 |
| **Packet 新增字段** (flit_category/protocol_id/bridge_id 末尾 3 字段) | ⚠️ P1 新增 | 见 §2.2.2 |
| **Packet::reset()** | ⚠️ P1 新增 3 行 | 重置新字段, 防串包 |
| **PacketPool** | ❌ | Packet 大小变化已在 budget 范围内, 无需扩容 |
| **双注册表** | ❌ | 本方案不新增注册表, 用 `ProtocolAdapterFactory` 单独管理 |
| **KPI 上限** | ⚠️ 现状超限 | 当前 5/3 超限 2, P1-P4 启动需先 archive 2 个 change, 见 §0.2bis |
| **6 标签 0 回归** | ⚠️ 承诺 | P1 含 Packet 改型回归面 (route_path < 10 处), P2-P4 需回归测试验证 |

---

## 9. 风险与失败模式

### 9.1 高风险:Packet 字段新增的二进制兼容 + reset() 串包风险

**症状 1**: Packet 类大小变化可能导致 ABI 兼容问题(虽然 CppTLM 不是 SO ABI,但编译缓存/链接顺序可能敏感)

**症状 2**(更严重):`Packet::reset()` 不重置新字段 → PacketPool 复用串包

**缓解**:
- 新增字段放**末尾**(line 273+),避免影响既有 offset
- `static_assert(sizeof(Packet) <= 320, "Packet size budget exceeded")` 编译期锁(**类外**,见 §2.2.2)
- 增量字段都用 `uint8_t`/`uint16_t`(对齐友好,4 字节增量)
- **P1 必须修改 `Packet::reset()`** 重置新字段(`flit_category`/`protocol_id`/`bridge_id` 全清零),详见 §2.2.3

### 9.2 高风险:ProtocolAdapter 与 StreamAdapter 职责混淆

**症状**: 开发者在 StreamAdapter 内做协议转换,导致 L4.5 抽象失效

**缓解**:
- 明确职责分工:
  - **StreamAdapter** 管 `Port ↔ ch_stream` 转换
  - **ProtocolAdapter** 管 `Bundle ↔ Bundle` 协议转换
- 编译期断言(P1 实施时按真实类型实参化,本节仅作语义示意):
  ```cpp
  // ProtocolAdapter<InB, OutB> 与 StreamAdapterBase 无继承关系
  static_assert(!std::is_base_of_v<cpptlm::StreamAdapterBase,
                                cpptlm::ProtocolAdapter<MyInB, MyOutB>>,
                "ProtocolAdapter must not inherit from StreamAdapterBase");
  // StreamAdapter<ModuleT, ReqB, RespB> 不暴露 pack/unpack 虚函数
  static_assert(!std::is_same_v<decltype(&cpptlm::StreamAdapter<MyMod, MyInB, MyOutB>::pack),
                                decltype(&cpptlm::StreamAdapter<MyMod, MyInB, MyOutB>::unpack)>,
                "StreamAdapter must not expose pack/unpack (ProtocolAdapter responsibility)");
  ```
- Code review checklist: StreamAdapter 内**禁止**做协议字段映射

### 9.3 高风险:NoCFlitBundle 16 字段超集假设失败

**症状**: CHI snoop / PCIe posted-write 语义在 16 字段中无法表达 → P2 实施时发现需要扩展字段

**缓解**:
- P0 阶段先跑 abstract adapter baseline,验证 16 字段对 simple traffic 足够
- P2 实施 AXI adapter 时验证 AXI 协议字段能完整映射到 16 字段
- 若不满足:P2 增量扩 NoCFlitBundle 字段(**仍不动 frozen**,因为 NoCFlitBundle 不在 frozen 列表)

### 9.4 高风险:bridge pack/unpack 时延漏计

**症状**: abstract 模式 latency=0,但具体协议有 pack/unpack 开销,导致 latency 偏差 >20%

**缓解**:
- P2 立 Gem5 Ruby 对照 <20% 误差验收线
- adapter::Stats 暴露 `pack_cycles` / `unpack_cycles` 计数器
- 若偏差 >20%: 增加 adapter latency 参数化(`adapter_latency_cycles: 2` 等)

### 9.5 中风险:协议切换不可逆

**症状**: adapter 内藏 OOO rid 表 / state machine,切换 mode 后状态守恒失败

**缓解**:
- 契约规定:**协议切换仅在 idle drain 后**(所有 outstanding 事务完成后)
- `ModuleFactory` Step 7c 切换前检查:`if (system_has_pending_transactions()) throw_error`
- 文档明示:**本方案协议切换是离线操作,不是热切换**

### 9.6 中风险:测试矩阵爆炸

**症状**: N 协议 × 2 模式 → N×2 配置 + N×2 测试

**缓解**:
- 每协议只保 1 golden 配置(`packet_semantic_mesh_2x2_<mode>.json`)
- `[pnn-<mode>]` 标签隔离
- baseline 配置(P0)跨协议共享
- 实施期先做 protocol adapter 的单元测试(不依赖完整 topology)

### 9.7 中风险:Packet 字段"渐进式遗忘"

**症状**: P1 加了 `flit_category`/`protocol_id`/`bridge_id`,但后续没人用,变成新的"死字段"

**缓解**:
- P0 baseline 报告**先列出现状**(`flow_id` / `vc_id` / `priority` 哪些在用)
- P1 新增字段**必须有立即消费者**:
  - `bridge_id` 由 `ProtocolAdapterEr::set_bridge_id()` 注入(由 ModuleFactory Step 7c 在分配时立即写入,见 §4.2)
  - `protocol_id` 由 ProtocolAdapter 实例化时立即注入(在 ProtocolAdapterEr 构造期或 ModuleFactory Step 7c 注入期)
  - `flit_category` 由 ProtocolAdapter::pack/unpack 立即消费(Step 6 触发,见 §3.2)
- 每个 P 阶段的 commit message 必含"哪些 Packet 字段被强化 / 新增"
- **明确**: **StreamAdapter::send 不写协议字段**(per §3.2 修订)

### 9.8 低风险:KPI 失控

**症状**: P1-P4 各 1 slot,但 OpenSpec 季度复审可能 archive 现有 change,释放 slot 速度不均

**缓解**:
- P0 启动需 1 个 KPI slot(per §0.2bis + §7.1 诚实承认,**不存在** "doc-only 先例" 豁免)
- P1-P4 顺序消耗,单一 P 未完成时不启动下一 P
- 季度复审触发时优先 archive 已实施完毕的 change

### 9.9 低风险:DSL 编译链诱惑

**症状**: 实施 P2/P3 时,发现协议字段映射复杂,开发者提议"像 SLICC 一样写 DSL"

**缓解**:
- **明确拒绝 DSL 诱惑**(保留 C++ 直接性,参考 `cpptlm-dgpu-soc-arch-truth-cleanup` §6 不破坏 C++ 栈的先例)
- adapter 类用 C++ 模板实现,开发成本 < DSL 编译链维护成本
- 任何"协议切换魔术"的设计诱惑都拒绝(本方案是 framework,不是 magic)

---

## 10. 命名约定(最终版,含与 PNN 消解)

| 名称 | 路径 | 命名风格 | 理由 |
|------|------|---------|------|
| 方案代号 | **Packet-Semantic Network** (无缩写) | 全称 | 与 PNN 区分,本方案强调 Packet 升级 + L4.5 桥接 |
| 基类(模板) | `cpptlm::ProtocolAdapter<InB, OutB>` | CamelCase + 模板 | 与 PNN 一致(避免分裂) |
| 基类(type-erased) | `cpptlm::ProtocolAdapterEr` | CamelCase + Er 后缀 | 供 ChStreamModuleBase 持有,见 §4.3 |
| 抽象实例 | `cpptlm::AbstractProtocolAdapter<FlitT>` | CamelCase + 前缀 | 与 `AbstractMemory` (gem5) 对齐 |
| AXI 实例 | `cpptlm::AxiProtocolAdapter` | CamelCase + 前缀 | 与 `Axi4StreamAdapter` 一致 |
| CHI 实例 | `cpptlm::ChiProtocolAdapter` | CamelCase + 前缀 | 与 `AxiProtocolAdapter` 对称 |
| PCIe 实例 | `cpptlm::PcieTlpProtocolAdapter` | CamelCase + 前缀 | 完整名(避免与 `PcieEndpointIP` 混淆) |
| CXL 实例 | `cpptlm::CxlProtocolAdapter` | CamelCase + 前缀 | |
| 桥接实例 | `cpptlm::DualPortProtocolAdapter` | CamelCase + 前缀 | 组合持有 `DualPortStreamAdapter*` (Pimpl / Strategy 模式) |
| 工厂 | `cpptlm::ProtocolAdapterFactory` | CamelCase + 后缀 | 与 `ChStreamAdapterFactory` 一致 |
| Packet 字段 | `flit_category` / `protocol_id` / `bridge_id` | snake_case | 与既有 `stream_id`/`flow_id` 一致 |
| JSON 字段 | `network.mode / network.topology / ...` | snake_case + 嵌套 | 与 `simulation_mode` 一致 |
| 注册宏 | `REGISTER_CHSTREAM_PROTOCOL_ADAPTER(mode, AdapterT)` | UPPER_SNAKE | 与 `REGISTER_CHSTREAM` 一致 |
| 测试标签 | `[packet-semantic]`, `[pnn-baseline]`, `[pnn-axi4]`, `[pnn-chi]` | 标签 + 后缀 | 与 `[realip]`, `[noc]` 风格一致 |
| ADR 编号 | `ADR-DGPU-13-pnn-protocol-neutral-network.md` + `ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md` | 沿用既有 13/14 | **修订既有**(PNN 蓝图已起草),见 §14.2 |

**与 PNN 命名消解**(PNN 蓝图已用 ProtocolBridge,本方案用 ProtocolAdapter):
- 本方案与既有 PNN 蓝图(`pnn-protocol-neutral-network.md` v0.1)的命名差异:
  - 本方案 `ProtocolAdapter` vs PNN `ProtocolBridge` —— **统一为 `ProtocolAdapter`**(语义更准,NetworkInterface 类比)
  - 本方案 `DualPortProtocolAdapter` vs PNN `ProtocolBridge<InB, OutB>` —— **统一为 `DualPortProtocolAdapter`**(**组合持有** DualPortStreamAdapter*,与 §2.3.2 一致)
- 消解路径: P0 阶段起草 OpenSpec 时,与既有 PNN 蓝图**合并为单一 change**:`cpptlm-pnn-protocol-network`,由本方案作为代码驱动版本,PNN 蓝图作为设计意图附录

**关键命名决策**:
- **不复用 PNN 缩写**——本方案与 PNN 是**互补**而非替代,保留区分便于追溯
- **基类名 `ProtocolAdapter` 而非 `ProtocolBridge`**——本方案强调"适配"(类似 NetworkInterface),"桥接"(PNN 用 Bridge)是 SLICC controller 类比;两者语义略有差异
- **桥接实例名 `DualPortProtocolAdapter`**——明示**组合持有** `DualPortStreamAdapter*`(Pimpl / Strategy 模式,与 §2.3.2 一致)

---

## 11. 评审清单(13 条,含诚实修订)

| # | 自检项 | 结论 |
|---|--------|------|
| 1 | 0 改 frozen headers | ✅ ProtocolAdapter 在 `include/framework/`, 不动 frozen |
| 2 | 0 破 23 ABI | ✅ `cpptlm_emulator.h` 零修改 |
| 3 | 0 改 Packet ext API | ✅ `get_transaction_id`/`set_transaction_id`/`get_error_code` 零修改 |
| 4 | 0 改 Packet 既有 12 字段 | ✅ stream_id/seq_num/vc_id/flow_id/priority/hop_count/dependents/payload/src_cycle/dst_cycle/original_req/type 等保持(见 §2.2.1 + §8 表) |
| 5 | P1 增量修改 Packet 既有 2 字段 | ⚠️ CmdType enum 扩 4 值 + route_path 改型,见 §2.2.1 + §2.2.4 |
| 6 | P1 末尾新增 3 字段 | ✅ flit_category/protocol_id/bridge_id,见 §2.2.2 |
| 7 | P1 修改 Packet::reset() | ⚠️ 重置新字段防串包,见 §2.2.3 |
| 8 | L4.5 ProtocolAdapter type-erased | ✅ ProtocolAdapterEr 基类 + ProtocolAdapter<InB,OutB> 模板,见 §4.3 |
| 9 | 6 标签 0 回归 | ⚠️ P1 含 Packet 改型回归面 (route_path < 10 处), P2-P4 需回归测试验证 |
| 10 | KPI 启动前提 | ⚠️ 现状 5/3 超限, P1-P4 启动需先 archive 2 个 change, 见 §0.2bis |
| 11 | P0 含 3 文件 build 改动 | ⚠️ 非纯 doc-only, 消耗 1 个 KPI slot,见 §7.1 + §0.2bis |
| 12 | 命名消解 | ✅ ProtocolAdapter vs PNN ProtocolBridge 已统一,见 §10 |
| 13 | 文档完整 | ✅ 本蓝图 + gem5 调研 + 配套 2 ADR (共用既有 ADR-DGPU-13/14) + OpenSpec proposal |

---

## 12. 评审问题清单(给 Oracle/Metis/团队)

### 12.1 Packet 字段升级问题

1. **Packet 新增 3 个字段**(flit_category/protocol_id/bridge_id)**是否过度**? 是否只加 1 个 `protocol_id` 就够?
2. **`flit_category` 字段与 `vc_id`/`flow_id`/`priority` 字段职责是否重叠**? 是否会加剧"渐进式遗忘"?
3. **Packet `cmd` enum 扩展 4 个值**(CFG_READ/CFG_WRITE/MSG/CPL,**命名规范遵循 `CmdType` 既定风格**,`MSG` 而非 `MSI` 是因 `MSI` 是 MSI-X interrupt 缩写,语义重叠)**是否会破坏既有 `CmdType` 序列化**? 是否需要 `BUNDLE_FIELD_WIDTH` 锁?
4. **Packet 字段审计**(P0 任务)**应该用静态分析工具还是运行时 instrumentation**? 推荐哪种?

### 12.2 L4.5 ProtocolAdapter 设计问题

1. **`ProtocolAdapter` 命名**是否与 `NetworkInterface` 概念一致? 是否应该直接命名 `NetworkInterface`(更贴近 Gem5)?
2. **`DualPortProtocolAdapter` 组合持有 `DualPortStreamAdapter*`**(Pimpl / Strategy 模式)**生命周期是否清晰**? 推荐由 `ChStreamAdapterFactory` / `ModuleFactory::stream_adapters_` 持有原 `DualPortStreamAdapter*`,`DualPortProtocolAdapter` 仅持非拥有引用?
3. **L4.5 抽象层与 L4 StreamAdapter 的边界**是否清晰? Packet 字段写入(`pkt->protocol_id`)应该由 StreamAdapter::send 完成还是 ProtocolAdapter::pack 完成?
4. **ProtocolAdapter 工厂注册**与 `ChStreamAdapterFactory` 是否合并? 还是单独管理?

### 12.3 实施 / KPI 问题

1. **P0 阶段 1-2 周**含 3 文件 build 改动,**消耗 1 个 KPI slot**(per §0.2bis 当前 5/3 已超限,见 §7.1 阻塞现状)
2. **P1 阶段 1-2 月 + 1 KPI slot** 估时是否合理? 参考 `cpptlm-vram-controller-functional-integration` 实际实施速度
3. **Packet 字段升级**(~15 行 + 编译期锁 + 测试)**是否能在 P1 内 1-2 天完成**? 还是需要单独 P0.5?
4. **Gem5 Ruby 对照 <20% 误差**验收线是否合理? gem5 Garnet 2.0 vs Simple Network 实测误差是多少?
5. **跨仓 ArchForge 联合验证**(P4)是否必要? 还是 P3 阶段 PcieTlp 验证后即收尾?

### 12.4 关系 / 边界问题

1. **与 `cpptlm-pnn-protocol-neutral-network.md` 关系**: 本方案是其代码驱动版本? 两者最终是否合并为单一 OpenSpec change? 还是分别走 KPI slot?
2. **与 `cpptlm-dgpu-soc-physical-ip-port-boundary` 关系**: 前者推 SDMA 5→8 端口切型 + NoCFlit 切型, 本方案是否依赖前者? 或独立?
3. **与 `cpptlm-dgpu-soc-control-hub` 关系**: 前者新建 ControlHub 集中 BAR decode, 本方案是否影响 Chub 与外设通信?
4. **`simulation_mode` 与 `network.mode` 正交**: 2 维组合 = 4 模式(functional×abstract / functional×axi4 / timing×abstract / timing×axi4), 是否所有 4 模式都有意义?
5. **跨仓 ArchForge 拓扑联合验证**(P4)vs **`docs/soc_arch/README.md` 占位**: 本蓝图文档应放 `docs/architecture/`(已选)还是 `docs/soc_arch/architecture/`(已迁 ArchForge)?

---

## 13. 决策请求(给项目 Owner)

### 13.1 需要批准的 3 个关键决策

1. **方案代号是否接受**(Packet-Semantic Network vs PNN 合并 vs 其他名字)?
2. **Packet 字段升级范围**是否接受(3 个字段 vs 1 个 `protocol_id` 字段 vs 0 字段仅 ProtocolAdapter)?
3. **P0 阶段 1-2 周含 3 文件 build 改动是否接受**? 还是要求 P0 阶段产出 1 个具体 adapter(如 AxiAdapter)作为 proof of concept?

### 13.2 后续行动

**如果批准**:
- 立即起草 `cpptlm-packet-semantic-network` OpenSpec proposal(引用本蓝图 + gem5 调研,归类待 KPI 评审)
- 修订既有 ADR-DGPU-13/14(加 `## Status Update 2026-10` 段,反映本方案作为代码驱动版本)
- P0 阶段启动(1-2 周, 含 3 文件 build 改动,需先消解 KPI 超限,见 §0.2bis)

**如果不批准**:
- 收集具体反对意见(哪一条不通过)
- 修订本蓝图(v0.1 → v0.2)
- 重新评审

---

## 14. 关联文档

### 14.1 现有

- [`docs/architecture/01-hybrid-architecture-v2.1.md`](01-hybrid-architecture-v2.1.md) — v2.1.10 已落地(6 层架构 + Gem5 类比)
- [`docs/designs/dgpu-soc/communication-network.md`](../designs/dgpu-soc/communication-network.md) — v1.0 已落地(实地代码画像 + 5-hop 数据流)
- [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md) — v0.1 待评审(自顶向下 PNN 蓝图)
- [`docs/research/gem5-ruby-protocol-bridge-survey.md`](../research/gem5-ruby-protocol-bridge-survey.md) — Gem5 Ruby 完整调研(v0.1)

### 14.2 待起草

- `openspec/changes/cpptlm-packet-semantic-network/proposal.md` — P0 OpenSpec (与既有 PNN 蓝图合并为 `cpptlm-pnn-protocol-network`, per §10; 归类待 KPI 评审)
- **修订既有**: `docs/adr/ADR-DGPU-13-pnn-protocol-neutral-network.md` + `docs/adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`
  - 既有 ADR-DGPU-13/14 是 PNN 蓝图占位
  - 本方案作为 PNN 蓝图的代码驱动版本,**共用同一对 ADR**,不新起编号
  - 修订时在 ADR-DGPU-13/14 头部加 `## Status Update 2026-10: Packet-Semantic Network 代码驱动版本` 段

**重要修正**(ADR 编号共用 + 双重继承冲突消解):
- 修订前需核对 ADR-DGPU-14 §2.1 候选 C——该候选**明确拒绝** "ProtocolBridge 作为 StreamAdapter 子类"方案
- 本方案 §2.3.2 已采用**组合持有** `DualPortStreamAdapter*`(Pimpl / Strategy 模式),**不继承 StreamAdapterBase**,与 ADR-DGPU-14 候选 C 的"拒绝双重继承"立场一致
- **消解路径**: 修订 ADR-DGPU-14 时,候选 C 保持"拒绝双重继承",本方案"组合持有"路径作为**候选 D**新增
- 详见 §2.3.2 + §4.3 的修订(ProtocolAdapterEr type-erased 基类)

### 14.3 现有 ADR 关联

- `ADR-DGPU-06` chip-internal AXI vs board-level PCIe 边界(本方案兼容,不破)
- `ADR-DGPU-11` timing-mode SoC scope(本方案沿用 functional/timing 切换)
- `ADR-DGPU-12` SoC 内部 TLM 接线原则(本方案不动既有 JSON 拓扑)
- `ADR-DGPU-08` 23 ABI 冻结 + 0 新增(本方案 0 改 23 ABI)
- `ADR-DGPU-09` Driver-Visible Minimal SoC 范围(本方案不影响 driver-visible 入口)

---

**版本**: v0.1(初稿,待评审)
**最后更新**: 2026-10
**批准状态**: 🟡 待架构评审(预期 1-2 轮 Oracle/Metis 复审)
**维护者**: CppTLM build agent
**下次更新**: 评审反馈后 v0.2