# 2026-09-19-cpptlm-mas-soc-topology-mvp: MAS-3.1 dGPU SoC V3.1-Rev2.0 拓扑修正

> **状态**: 📋 Proposed — 2026-09-19
> **目标**: 在 CppTLM dGPU 仓内实施 **MAS-3.1 V3.1-Rev2.0 拓扑修正**，解决"模块归属错误 + 接口缺失"问题。具体包括 3 项关键修正：① TC-DMA 物理归属 GPC 内紧耦合（不再与 HBM-DMA 平级）；② IO-DMA / PCIe 端口补全到 Fabric & Edge IO Subsystem；③ 移除片上 CXL PHY/Controller，明确 CXL Memory Pool 是外部 Scale-Up Switch 下挂设备。同时**新增 SoC 顶层物理布局规范文档** + **修正 HRT Route_Tag 分配** + **细化 AWT Trap Type**。
>
> **关联设计**: `docs/soc_arch/architecture/21-soc-topology-mvp.md`（SoC 顶层物理布局规范 SSOT）
> **关联子系统文档**:
> - `docs/soc_arch/architecture/21-tee-udd-mvp.md`（数据面）
> - `docs/soc_arch/architecture/21-dma-backends-mvp.md`（后端物理实现）
> - `docs/soc_arch/architecture/21-fabric-switch-mvp.md`（协议层）
> - `docs/soc_arch/architecture/21-microarch-ifc-mvp.md`（实现层契约）
>
> **关联 ADR**: ADR-SOC-21（V3.1-Rev2.0 拓扑修正, 已签发 per `docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md`）
> **路线图位置**: MAS-3.1 V3.1-Rev2.0 拓扑冻结阶段（SoC 顶层契约冻结 + 子系统文档修订）

---

## Why

CppTLM dGPU SoC 在 MAS-3.1 体系下的**顶层物理布局**经过 8 份子系统文档梳理后，暴露出 3 个**模块归属错误 + 接口缺失**问题，必须在 RTL 实现冻结前修正：

### 问题 1: TC-DMA 物理归属错误（最关键）

**当前状态**: 8 份子系统文档虽在 §3.3 与 §9.1 明确表述"TC-DMA 是 GPC 紧耦合组件，不直接连接 HBM"，但 **没有 SoC 顶层文档统一定义这一拓扑**，且现有 §1.1 拓扑图未明确画出 TC-DMA 在 GPC 内。

**主因（按阻断优先级排序）**:
1. **缺 SoC 顶层物理布局规范**: 8 份子系统文档按"4 个子系统维度"组织（数据面 / 后端 / 协议层 / 实现层），但**缺少第 5 份"SoC 顶层物理布局"文档**统一展示五大子系统（GPC / Memory / Fabric-IO / Control / External Bridge）的物理归属。
2. **TC-DMA 模块归属歧义**: RTL 团队可能误读为"TC-DMA 与 HBM-DMA 平级"，导致错误的 RTL 直连接口（违背 mbarrier 硬件触发的确定性延迟约束）。
3. **TMA 跨域拷贝路径未完整定义**: 5 步路径（SM → TC-DMA → GPC UDD Agent → Compute NoC → Global UDD → HBM-DMA → TC-DMA → SMEM 直达）散落在多文档，未集中阐述。

### 问题 2: IO-DMA / PCIe 端口缺失

**当前状态**: 8 份子系统文档在 [`21-dma-backends-mvp.md` §3.4](21-dma-backends-mvp.md) 提及 IO-DMA（v1.1 引入），在 [`21-fabric-switch-evolution-roadmap.md` §4.1](21-fabric-switch-evolution-roadmap.md) 规划 v1.1 加 PCIe，但 **SoC 顶层拓扑图中 IO-DMA 端口未被显式画出**，与 NIC-DMA 平级关系不清晰。

**主因**:
1. **拓扑图简化**: V3.0 拓扑将 NIC-DMA / IO-DMA 简化为单一"Edge IO"，未区分两个平级 Backend
2. **缺 IO-DMA vs NIC-DMA 对比表**: driver 视角对两者职责（Host 通信 vs Peer GPU）模糊
3. **Host 通信 / NVMe / GDS 路径不显式**: 易被误读为"PCIe 仅用于控制面"

### 问题 3: CXL 接口位置错误

**当前状态**: 8 份子系统文档隐含"GPU 不实现 CXL.mem/CXL.cache 协议"原则，但 **HRT Route_Tag 表存在歧义**（Route_Tag=0x1~0xE 的描述包含"→ CXL"），且 **CXL Memory Pool 在 SoC 拓扑中的物理位置未显式标出**。

**主因**:
1. **V3.0 拓扑残留**: 早期拓扑图曾画出 GPU Die 内有 CXL Bridge，需明确"该接口已废弃"
2. **HRT 中"→ CXL"措辞误导**: 让人误以为 GPU 侧直接发出 CXL.mem 事务，实际是经 NIC-DMA → Switch PTE 转换
3. **AWT Trap Type `0x1` 命名不明确**: 旧名 "Remote CXL Poison" 易让人以为是 GPU 直连 CXL，需明确为 `REMOTE_CXL_POISON_VIA_SWITCH`

### 预期收益

- **RTL 团队拓扑接口冻结**: 五大子系统边界清晰，RTL 实现不再有归属歧义
- **TC-DMA mbarrier 硬件触发确定性延迟得到保护**: 严格保持 GPC 紧耦合，RTL 不允许直连 HBM
- **IO-DMA / NIC-DMA 边界清晰**: Host 通信 vs Peer GPU 通信职责明确，避免跨后端混用
- **CXL Memory Pool 物理位置显式**: GPU 团队无需实现 CXL 协议栈（Die 面积节省 3-5%），验证复杂度降低一个数量级
- **HRT Route_Tag 无歧义**: 0x0/0x1~0xE/0xF 三段语义明确，CXL 通过 SWITCH_PORT_N 统一路由
- **AWT Trap Type 命名明确**: `REMOTE_CXL_POISON_VIA_SWITCH` 显式标注 Poison 来源是外部 Switch 转换
- **0 个新 ABI 函数** (per ADR-088 §D5 严格遵守)

---

## What Changes

### 新增文件

| 文件 | 状态 | 步骤 |
|------|------|:----:|
| `docs/soc_arch/architecture/21-soc-topology-mvp.md` | **新** (SSOT for topology) | T1 |
| `openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/proposal.md` | **新** (本文件) | T0 |
| `openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/design.md` | **新** (详细设计) | T0 |
| `openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/specs/soc-topology-mvp/spec.md` | **新** (delta spec) | T0 |
| `openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/tasks.md` | **新** (TDD 5 步) | T0 |
| `docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md` | **新** (V3.1-Rev2.0 拓扑修正 ADR) | T1 |
| `src/tlm/soc/soc_topology_defs.hh` | **新** (五大子系统边界宏定义) | T3 |
| `src/tlm/soc/soc_shell.cc` | **新** (SoC 顶层注入拓扑) | T3 |
| `test/test_soc_topology_e2e.cc` | **新** (SoC 端到端 demo, TDD 5 步) | T4 |

### 修正文件（4 份子系统文档）

| 文件 | 状态 | 修正点 | 步骤 |
|------|------|--------|:----:|
| `docs/soc_arch/architecture/21-fabric-switch-mvp.md` | **改** | §1.2 新增 V3.1-Rev2.0 拓扑修正横幅; §1.3 表格新增 SoC-Topology 行; §4.4 Route_Tag 表显式标注 "via Switch"; §7.1 Poison 编码明确 `REMOTE_CXL_POISON_VIA_SWITCH` | T2 |
| `docs/soc_arch/architecture/21-tee-udd-mvp.md` | **改** | 关联文档新增 `21-soc-topology-mvp.md` + ADR-SOC-21; 新增 §4.0 GUPA 空间划分 + HRT Route_Tag 分配表; §4.1 MMIO 表新增 HRT Shadow Entry 32-bit 字段位分配说明 | T2 |
| `docs/soc_arch/architecture/21-microarch-ifc-mvp.md` | **改** | 关联文档新增 `21-soc-topology-mvp.md` + ADR-SOC-21; §7.6 AWT Trap Type 改为 4 列完整表（含名称 / 来源识别 / 物理介质归属 / 软件处理责任方），明确 `0x1 = REMOTE_CXL_POISON_VIA_SWITCH` | T2 |
| `docs/soc_arch/architecture/21-dma-backends-mvp.md` | **改** | 关联文档新增 `21-soc-topology-mvp.md` + ADR-SOC-21 | T2 |

### 修正文件（4 份演进路线图）

| 文件 | 状态 | 步骤 |
|------|------|:----:|
| `docs/soc_arch/architecture/21-tee-udd-evolution-roadmap.md` | **改** (关联文档新增 SoC-Topology + ADR-SOC-21) | T2 |
| `docs/soc_arch/architecture/21-dma-backends-evolution-roadmap.md` | **改** (关联文档新增 SoC-Topology + ADR-SOC-21) | T2 |
| `docs/soc_arch/architecture/21-fabric-switch-evolution-roadmap.md` | **改** (关联文档新增 SoC-Topology + ADR-SOC-21) | T2 |
| `docs/soc_arch/architecture/21-microarch-ifc-evolution-roadmap.md` | **改** (关联文档新增 SoC-Topology + ADR-SOC-21) | T2 |

---

## Scope

### In Scope（v1.0 MVP 范围）

1. **新增 `21-soc-topology-mvp.md`**: SoC 顶层物理布局规范
   - 五大子系统划分（GPC / Memory / Fabric-IO / Control / External）
   - 修正后的 SoC 顶层互联拓扑图（明确 TC-DMA / IO-DMA / CXL 位置）
   - TMA 跨域拷贝 5 步路径 / CXL 内存池访存 6 步路径 / IO-DMA 路径
   - HRT Route_Tag 分配（移除 CXL_DIRECT）
   - CXL Drain 三方协调机制（GPU-Switch-FM）
   - 3 条无债务演进不变量（TC-DMA 归属 / GPU 无 CXL / HRT 无 CXL_DIRECT）
2. **修正 4 份子系统 MVP 文档**: 同步拓扑修正
3. **修正 4 份演进路线图**: 关联文档新增 SoC-Topology
4. **新增 `ADR-SOC-21`**: V3.1-Rev2.0 拓扑修正决策记录
5. **新增 SoC 顶层 C++ 基础设施**: `soc_topology_defs.hh` + `soc_shell.cc`（注入五大子系统）
6. **新增 `test_soc_topology_e2e.cc`**: 10 步端到端 demo 测试

### Out of Scope（推迟到后续阶段）

1. ❌ **TC-DMA 多流并发** (v1.1+ 演进, per `21-tee-udd-evolution-roadmap.md` §4.1)
2. ❌ **IO-DMA 详细 CSR** (v1.1+ 演进, per `21-microarch-ifc-evolution-roadmap.md` §4.2)
3. ❌ **CXL Backend (真实 CXL Memory Pool 联调)** (v2.0+ 演进, per `21-dma-backends-evolution-roadmap.md` §4.2)
4. ❌ **Hot-plug / Hot-unplug** (v2.0+ 演进, per `21-fabric-switch-evolution-roadmap.md` §4.2)
5. ❌ **Multicast / All-Reduce** (v3.0+ 演进, per `21-fabric-switch-evolution-roadmap.md` §4.4)
6. ❌ **Coherence SnoopFilter** (v3.0+ 演进)
7. ❌ **真实 RTL 时序 (Static Timing Analysis)** (推迟到 RTL 实施阶段)

---

## Acceptance Gate (10 项)

> **Oracle 评审锚点**: 全部 10 项必须 ✅ 才能 ship v1.0 MVP。

- [ ] **AG1**: `21-soc-topology-mvp.md` 已创建并含 5 大子系统划分 + 修正后拓扑图 + 3 条不变量
- [ ] **AG2**: `21-fabric-switch-mvp.md` §1.2 / §4.4 / §7.1 三处均明确标注 V3.1-Rev2.0 拓扑修正
- [ ] **AG3**: `21-tee-udd-mvp.md` 新增 §4.0 GUPA 空间划分 + HRT Route_Tag 分配表
- [ ] **AG4**: `21-microarch-ifc-mvp.md` §7.6 AWT Trap Type 明确为 `REMOTE_CXL_POISON_VIA_SWITCH`
- [ ] **AG5**: 8 份子系统文档的关联文档部分均含 `21-soc-topology-mvp.md` + ADR-SOC-21
- [ ] **AG6**: `ADR-SOC-21` 已创建, 含 Context / Decision / Consequences / 5 阶段约束
- [ ] **AG7**: `soc_topology_defs.hh` 定义 5 大子系统边界宏 (SUBSYSTEM_GPC / SUBSYSTEM_MEMORY / SUBSYSTEM_FABRIC_IO / SUBSYSTEM_CONTROL / SUBSYSTEM_EXTERNAL)
- [ ] **AG8**: `soc_shell.cc` 注入 5 大子系统拓扑 (GPC × 8, HBM-DMA × 2, NIC-DMA × 4, IO-DMA × 1, CIU × 1)
- [ ] **AG9**: `test_soc_topology_e2e.cc` 10 步端到端 demo 测试通过 (TC-DMA 归属 + IO-DMA 在 Fabric & Edge IO + CXL 不在 GPU Die + HRT 无 CXL_DIRECT + AWT Trap Type 区分)
- [ ] **AG10**: 0 个新 ABI 函数 (per ADR-088 §D5 严格遵守)

---

## Capabilities

### CAP-1: SoC 五大子系统边界清晰（per `21-soc-topology-mvp.md` §2）

| 子系统 | 包含模块 | 物理位置 |
|--------|----------|----------|
| **GPC Subsystem** | SM × 16-32 + TC-DMA × 2-8 + L2 Slice + UDD Agent + TEE Frontend | GPU Die 内 |
| **Memory Subsystem** | HBM-DMA × N + HBM3e Stacks | GPU Die 内 |
| **Fabric & Edge IO Subsystem** | Global UDD Hub + Memory & IO NoC + NIC-DMA + IO-DMA | GPU Die 内 |
| **Control Subsystem** | CIU + AWT Controller + GMMU Hub + Boot ROM | GPU Die 内 |
| **External** | Scale-Up Switch + CXL Memory Pool + Host CPU | GPU Die 之外 |

### CAP-2: TC-DMA 物理归属 GPC 内（per `21-soc-topology-mvp.md` §2.1）

- TC-DMA 与 SMEM/L2 紧耦合
- TC-DMA 通过 GPC 内 UDD Agent 间接访问 HBM（**不**直连 HBM-DMA）
- 维护 mbarrier 硬件触发的确定性延迟约束（per `21-microarch-ifc-mvp.md` §9.1）

### CAP-3: IO-DMA / PCIe 端口补全（per `21-soc-topology-mvp.md` §2.3）

- IO-DMA 在 Fabric & Edge IO Subsystem 内独立 Backend
- 与 NIC-DMA 平级（但物理端口分离）
- IO-DMA vs NIC-DMA 对比表（Host 通信 vs Peer GPU）
- IO-DMA 寄存器（v1.1+ 引入，per `21-dma-backends-mvp.md` §3.4）

### CAP-4: CXL Memory Pool 物理位置明确（per `21-soc-topology-mvp.md` §2.5）

- GPU Die 上**无** CXL PHY/Controller
- CXL Memory Pool 是 Scale-Up Switch 下挂的独立设备
- GPU 对 CXL 内存的访问是"Native UALink + Remote CXL Translation"

### CAP-5: HRT Route_Tag 分配（per `21-soc-topology-mvp.md` §4.1 + `21-tee-udd-mvp.md` §4.0）

| Route_Tag | 目标 Backend | 物理介质归属 |
|-----------|--------------|--------------|
| `0x0` | HBM-DMA | GPU Die 内 |
| `0x1 ~ 0xE` | NIC-DMA (Port N) | 外部 Switch (Peer GPU HBM **或** CXL Memory Pool) |
| `0xF` | IO-DMA | 外部 PCIe 端点 |

**关键**: HRT 中**没有** `CXL_DIRECT` Route_Tag（per ADR-SOC-21 §D3 不变量 3）

### CAP-6: AWT Trap Type 区分 Local/Remote (via Switch)

- `0x0`: `LOCAL_HBM_POISON` (GPU Die 内 HBM3e Stack)
- `0x1`: `REMOTE_CXL_POISON_VIA_SWITCH` (外部 CXL Memory Pool 经 Scale-Up Switch)
- `0x2`: `SECURITY_VIOLATION` (RCT 越权)
- `0x3`: `PCIE_IO_ABORT` (v1.1+, 外部 PCIe 端点)

---

## Impact

### 影响范围

- **9 份架构文档**: 1 新增 + 8 修正 (4 份子系统 MVP + 4 份 Roadmap)
- **1 份 ADR**: ADR-SOC-21 (V3.1-Rev2.0 拓扑修正, 新增)
- **1 个 SoC 注入基础设施**: `soc_topology_defs.hh` + `soc_shell.cc` (新增)
- **1 个端到端 demo 测试**: `test_soc_topology_e2e.cc` (新增)

### 风险评估

| 风险 | 等级 | 缓解策略 |
|------|------|----------|
| RTL 团队误解 TC-DMA 归属 | 高 | SoC 顶层拓扑图明确画出 TC-DMA 在 GPC 内, RTL-IFC §3.2 已定义 TC-DMA ↔ UDD Agent 接口 |
| IO-DMA vs NIC-DMA 混用 | 中 | `21-soc-topology-mvp.md` §2.3 对比表 + `21-microarch-ifc-mvp.md` §7.5 预留 CSR |
| RTL 实现引入片上 CXL | 中 | ADR-SOC-21 不变量 2 严格禁止, SoC 顶层拓扑图 CXL 在 Die 外 |
| HRT Route_Tag 引入 `CXL_DIRECT` | 低 | ADR-SOC-21 不变量 3 严格禁止, 4 份子系统 MVP 文档交叉引用 |
| Drain 协议误用 | 低 | `21-soc-topology-mvp.md` §4.2 三方协调机制明确 |

### 跨仓影响

- **UsrLinuxEmu**: 不修改 23 ABI 函数, driver 仅需更新 HRT 初始化代码 (无 CXL_DIRECT)
- **Cpputlm 仓**: 9 份文档 + 1 份 ADR + 2 个新 C++ 文件 + 1 个新测试

---

## 关联文档

- [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../../../docs/soc_arch/architecture/21-soc-topology-mvp.md) — SoC 顶层物理布局规范 SSOT
- [`docs/soc_arch/architecture/21-tee-udd-mvp.md`](../../../docs/soc_arch/architecture/21-tee-udd-mvp.md) — TEE + UDD 数据面
- [`docs/soc_arch/architecture/21-dma-backends-mvp.md`](../../../docs/soc_arch/architecture/21-dma-backends-mvp.md) — UBC + HBM/NIC/IO-DMA
- [`docs/soc_arch/architecture/21-fabric-switch-mvp.md`](../../../docs/soc_arch/architecture/21-fabric-switch-mvp.md) — Fabric + Scale-Up Switch
- [`docs/soc_arch/architecture/21-microarch-ifc-mvp.md`](../../../docs/soc_arch/architecture/21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC
- [`docs/soc_arch/architecture/21-tee-udd-evolution-roadmap.md`](../../../docs/soc_arch/architecture/21-tee-udd-evolution-roadmap.md) — TEE-UDD 演进路线图
- [`docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md`](../../../docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md) — V3.1-Rev2.0 拓扑修正 ADR (已签发)
- [`docs/soc_arch/architecture/00-overview.md`](../../../docs/soc_arch/architecture/00-overview.md) — dGPU SoC 总览

---

## 关联 ADR

- **ADR-SOC-21** (已签发 2026-09-19, per [`docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md`](../../../docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md)) — **V3.1-Rev2.0 拓扑修正** (本提案决策基础)
- ADR-088 §D5 — 23 ABI 冻结 (本提案 0 个新 ABI 函数)
- ADR-SOC-09 — v1.0 NVIDIA+AMD dual vendor 战略
- ADR-SOC-10 — ModuleFactory 拓扑层
- ADR-SOC-11 — PcieEndpointIP (替代 PcieEndpointTLM 4 端口冻结; PcieEndpointTLM 已 `[[deprecated]]` 标注保留向后兼容, per `21-microarch-ifc-mvp.md` §1.3)
- ADR-SOC-13 — AXI Stream Adapter Mapper (Backends 协同)
- ADR-SOC-19 — AXI Master Outbound Bridge (Backends 协同)

---

## 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: V3.1-Rev2.0 拓扑修正提案 (3 项关键修正 + 1 新文档 + 8 修正 + 1 ADR + 10 步 Acceptance Gate) |