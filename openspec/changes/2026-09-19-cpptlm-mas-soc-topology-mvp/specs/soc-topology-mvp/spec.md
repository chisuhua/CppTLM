# SOC-TOPOLOGY-MVP: dGPU SoC V3.1-Rev2.0 拓扑修正 — Delta Spec

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`tasks.md`](../tasks.md)
> **SSOT**: [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../../../../docs/soc_arch/architecture/21-soc-topology-mvp.md)
> **Spec 类型**: Delta Spec（修改 `openspec/specs/main-spec.md` 的 9 份架构文档）

---

## §1 Purpose

本 delta spec 定义 **MAS-3.1 dGPU SoC V3.1-Rev2.0 拓扑修正**的**功能性需求**与**验收场景**。本提案冻结 9 份架构文档 + 1 份 ADR + 2 个 C++ 文件 + 1 个端到端测试，以确保：

1. **TC-DMA 物理归属 GPC 内紧耦合**（不与 HBM-DMA 平级，不直连 HBM）
2. **IO-DMA / PCIe 端口补全到 Fabric & Edge IO Subsystem**（与 NIC-DMA 平级）
3. **CXL Memory Pool 物理位置明确**（不在 GPU Die 上，是 Scale-Up Switch 下挂设备）
4. **HRT Route_Tag 分配清晰**（0x0=HBM, 0x1~0xE=UALink, 0xF=PCIe，**没有 CXL_DIRECT**）
5. **AWT Trap Type 区分 Local/Remote**（`0x1 = REMOTE_CXL_POISON_VIA_SWITCH`）

---

## ADDED Requirements

### Requirement: req-1-SoC 顶层物理布局规范文档存在

The system MUST `docs/soc_arch/architecture/21-soc-topology-mvp.md` 必须存在并包含 5 大子系统划分。

#### Scenario: SoC 顶层文档存在
- **WHEN** 用户查阅 dGPU SoC 顶层架构
- **THEN** 应找到 `21-soc-topology-mvp.md`，包含：
  - 5 大子系统划分（GPC / Memory / Fabric-IO / Control / External）
  - 修正后的 SoC 顶层互联拓扑图
  - TMA 跨域拷贝 5 步路径
  - CXL 内存池访存 6 步路径
  - IO-DMA / PCIe 路径
  - HRT Route_Tag 分配表（**没有 CXL_DIRECT**）
  - 3 条无债务演进不变量

#### Scenario: 5 大子系统边界清晰
- **WHEN** RTL 工程师查阅物理模块归属
- **THEN** 应能明确：
  - GPC Subsystem 内含：SM × 16-32, TC-DMA × 2-8, L2 Slice, UDD Agent, TEE Frontend
  - Memory Subsystem 内含：HBM-DMA × N + HBM3e Stacks（**不含** TC-DMA）
  - Fabric & Edge IO Subsystem 内含：Global UDD Hub + NIC-DMA × 4 + IO-DMA × 1
  - Control Subsystem 内含：CIU + AWT Controller + GMMU Hub + Boot ROM
  - External 包含：Scale-Up Switch + CXL Memory Pool + Host CPU + NVMe（**不在 GPU Die**）

---

### Requirement: req-2-TC-DMA 物理归属 GPC 内

The system MUST TC-DMA 必须物理归属 GPC Subsystem 内，且**禁止**直连 HBM-DMA。

#### Scenario: TC-DMA 不在 Memory Subsystem
- **WHEN** `soc_shell.cc::validate_topology()` 执行
- **THEN** 应检测 TC-DMA 仅在 GPC 内（per D1 不变量 1）
- **AND** 应检测 `Memory Subsystem`（HBM-DMA）**没有** TC-DMA 直连接口

#### Scenario: TC-DMA 通过 GPC UDD Agent 访问 HBM
- **WHEN** SM 发起 TMA 描述符
- **THEN** 数据流路径必须是 5 步：
  1. SM → GPC 内 TC-DMA
  2. TC-DMA → GPC 内 UDD Agent（tc_udd_req_*）
  3. UDD Agent → Compute NoC → Global UDD Hub → HBM-DMA
  4. HBM-DMA 返回数据 → 经原路回到 TC-DMA
  5. TC-DMA → SMEM 直达（旁路 L2）+ 硬件触发 mbarrier

---

### Requirement: req-3-IO-DMA 在 Fabric & Edge IO Subsystem 内

The system MUST IO-DMA 必须与 NIC-DMA 平级，共同构成 Fabric & Edge IO Subsystem。

#### Scenario: IO-DMA 与 NIC-DMA 平级
- **WHEN** 查阅 `21-soc-topology-mvp.md` §2.3
- **THEN** 应明确：
  - IO-DMA 在 Fabric & Edge IO Subsystem 内独立 Backend
  - IO-DMA vs NIC-DMA 对比表（Host 通信 vs Peer GPU）
  - IO-DMA 与 NIC-DMA 物理端口分离

#### Scenario: IO-DMA 路径（v1.1+）
- **WHEN** Host CPU 发起 DMA / PCIe Peer-to-Peer
- **THEN** 应走 IO-DMA 路径：
  - Host → PCIe x16 PHY → IO-DMA → Memory & IO NoC → Global UDD Hub → GPC/HBM

---

### Requirement: req-4-GPU Die 上无 CXL PHY/Controller

The system MUST GPU Die 上**禁止**包含任何 CXL PHY 或 CXL Controller（CXL Memory Pool 是外部设备）。

#### Scenario: validate_topology 检测无 CXL PHY
- **WHEN** `soc_shell.cc::validate_topology()` 执行
- **THEN** 应检测 GPU Die 内**无** CXL PHY（per D2 不变量 2）
- **AND** 应检测 GPU Die 内**无** CXL Controller

#### Scenario: CXL Memory Pool 通过 Scale-Up Switch 访问
- **WHEN** GPU 访问 CXL Memory Pool
- **THEN** 数据流路径必须是 6 步：
  1. SM → TEE → GMMU → GUPA（高位 0x1500 落在 Scale-Up 域）
  2. UDD Agent → HRT 查表 → Route_Tag=0x1（NIC-DMA Port 0）
  3. UDD → NIC-DMA → 封装 UALink Mem-Read Flit
  4. NIC-DMA → UALink → 外部 Scale-Up Switch
  5. Switch PTE 内部转换：UALink → CXL.mem → 发往 CXL Memory Pool
  6. CXL 设备返回 → Switch 转换回 UALink Mem-Response → 返回 GPU

---

### Requirement: req-5-HRT Route_Tag 分配清晰（无 CXL_DIRECT）

The system MUST HRT Route_Tag 分配为 0x0=HBM, 0x1~0xE=UALink, 0xF=PCIe，**没有** CXL_DIRECT。

#### Scenario: HRT Route_Tag 表完整
- **WHEN** 查阅 `21-tee-udd-mvp.md` §4.0 GUPA 空间划分
- **THEN** 应明确：
  | Route_Tag | 目标 Backend | 物理介质归属 |
  |-----------|--------------|--------------|
  | `0x0` | HBM-DMA | GPU Die 内 (HBM3e Stack) |
  | `0x1 ~ 0xE` | NIC-DMA (Port N) | 外部 Scale-Up Switch (Peer GPU HBM **或** CXL Memory Pool) |
  | `0xF` | IO-DMA | 外部 PCIe 端点 |

#### Scenario: HRT 严禁 CXL_DIRECT
- **WHEN** `soc_shell.cc::validate_topology()` 执行
- **THEN** 应检测 HRT Shadow Bank 中**无** `CXL_DIRECT` Route_Tag 配置（per D3 不变量 3）
- **AND** CIU HRT 写入路径应拒绝任何 `0xF0~0xFF` 的 Route_Tag 值

---

### Requirement: req-6-AWT Trap Type 区分 Local/Remote (via Switch)

The system MUST AWT `AWT_PAYLOAD_2[7:0]` Trap Type 必须区分 Local HBM Poison 与 Remote CXL Poison (via Switch)。

#### Scenario: AWT Trap Type 4 列完整表
- **WHEN** 查阅 `21-microarch-ifc-mvp.md` §7.6
- **THEN** 应明确：
  | Trap Type | 名称 (V3.1-Rev2.0) | 来源识别 | 物理介质归属 | 软件处理 |
  |-----------|---------------------|----------|--------------|----------|
  | `0x0` | `LOCAL_HBM_POISON` | HBM-DMA 内 SECDED DED / Page Retire | GPU Die 内 (HBM3e Stack) | KMD 隔离物理页 |
  | `0x1` | `REMOTE_CXL_POISON_VIA_SWITCH` | NIC-DMA 收到 UALink Remote_Error Flit | 外部 CXL Memory Pool (经 Scale-Up Switch) | FM 隔离远端 CXL 节点 |
  | `0x2` | `SECURITY_VIOLATION` | RCT 校验失败 | GPU Die 内 | KMD + Hypervisor |
  | `0x3` | `PCIE_IO_ABORT` (v1.1+) | PCIe Link Down / UR | 外部 PCIe 端点 | KMD Host 驱动 |

#### Scenario: AWT Payload 名称显式标注 "via Switch"
- **WHEN** Trap Type 0x1 触发
- **THEN** 应明确 Poison 来源是外部 Scale-Up Switch 转换的 CXL Memory Pool（**非** GPU 直连 CXL）

---

### Requirement: req-7-跨子系统文档一致性

The system MUST 8 份子系统文档（4 份 MVP + 4 份 Roadmap）的关联文档部分均含 `21-soc-topology-mvp.md` + ADR-SOC-21。

#### Scenario: 9 份文档 SoC-Topology 引用一致
- **WHEN** 验证 `21-soc-topology-mvp.md` 引用
- **THEN** 全部 9 份子系统文档（4 MVP + 4 Roadmap + 1 新增 SOC-Topology）均应引用

#### Scenario: ADR-SOC-21 引用一致
- **WHEN** 验证 ADR-SOC-21 引用
- **THEN** 全部 8 份子系统文档（4 MVP + 4 Roadmap）均应引用 ADR-SOC-21

---

## §3 Scenarios（基于 User Story 格式）

### US-1: RTL 工程师查阅物理拓扑

**作为** RTL 工程师

**我想要** 查阅 dGPU SoC 顶层物理拓扑

**以便于** 实现 RTL 时正确归属模块物理位置

#### Scenario: RTL 工程师查阅 GPC 内 TC-DMA
- **WHEN** RTL 工程师查找 TC-DMA 物理位置
- **THEN** 应能在 `21-soc-topology-mvp.md` §2.1 找到 TC-DMA 在 GPC Subsystem 内
- **AND** 应明确 TC-DMA **不**直连 HBM-DMA，必须经 GPC UDD Agent

#### Scenario: RTL 工程师查阅 IO-DMA 物理位置
- **WHEN** RTL 工程师查找 IO-DMA 物理位置
- **THEN** 应能在 `21-soc-topology-mvp.md` §2.3 找到 IO-DMA 在 Fabric & Edge IO Subsystem 内
- **AND** 应明确 IO-DMA 与 NIC-DMA 平级（物理端口分离）

### US-2: driver 工程师配置 HRT

**作为** driver 工程师

**我想要** 通过 PCIe MMIO 配置 HRT（Hardware Routing Table）

**以便于** 告诉 UDD Agent "GUPA 前缀 X 应该路由到 Backend Y"

#### Scenario: driver 配置 Local HBM HRT 条目
- **WHEN** driver 写 `CIU_REG_HRT_SHADOW[0]` = `{Route_Tag=0x0, VC=0, QoS=Normal}`
- **THEN** GUPA 高位 0x0000~0x0FFF 自动路由至 HBM-DMA

#### Scenario: driver 配置 CXL Memory Pool HRT 条目
- **WHEN** driver 写 `CIU_REG_HRT_SHADOW[1]` = `{Route_Tag=0x1 (NIC-DMA Port 0), VC=0, QoS=Normal}`
- **THEN** GUPA 高位 0x1000~0x1FFF 自动路由至 NIC-DMA Port 0 → 外部 Scale-Up Switch
- **AND** Switch PTE 决定目标（CXL Memory Pool 或 Peer GPU HBM）
- **AND** driver **不能**配置 Route_Tag = `0xF0~0xFF`（CIU 拒绝）

### US-3: KMD 接收 AWT Trap

**作为** KMD (Kernel Mode Driver)

**我想要** 接收 SM AWT Trap 并区分故障源

**以便于** 决定恢复策略（重启 Kernel / 通知 FM / 隔离远端节点）

#### Scenario: KMD 接收 LOCAL_HBM_POISON Trap
- **WHEN** SM Warp 触发 AWT，Trap Type = 0x0
- **THEN** KMD 隔离物理页, 迁移数据, 重启 Kernel

#### Scenario: KMD 接收 REMOTE_CXL_POISON_VIA_SWITCH Trap
- **WHEN** SM Warp 触发 AWT，Trap Type = 0x1
- **THEN** KMD 通知 FM 隔离远端 CXL 节点, 触发 Checkpoint 恢复

---

## §4 Acceptance Criteria

> **Oracle 评审锚点**: 全部 7 个 REQ + 6 个 Scenario 必须 ✅ 才能 ship v1.0 MVP。

| 编号 | REQ | Scenario | Oracle 检查 |
|------|-----|----------|------------|
| REQ-1 | SoC 顶层文档存在 | 2 | AG1 |
| REQ-2 | TC-DMA 归属 GPC 内 | 2 | AG9 |
| REQ-3 | IO-DMA 在 Fabric & Edge IO | 2 | AG9 |
| REQ-4 | GPU Die 无 CXL | 2 | AG9 |
| REQ-5 | HRT 无 CXL_DIRECT | 2 | AG9 |
| REQ-6 | AWT Trap Type 区分 | 2 | AG4 |
| REQ-7 | 跨子系统文档一致性 | 2 | AG5 |

---

## §5 Out of Scope（推迟到后续阶段）

- ❌ TC-DMA 多流并发（v1.1+）
- ❌ IO-DMA 详细 CSR（v1.1+）
- ❌ CXL Backend 真实联调（v2.0+）
- ❌ Hot-plug / Hot-unplug（v2.0+）
- ❌ Multicast / All-Reduce（v3.0+）
- ❌ Coherence SnoopFilter（v3.0+）
- ❌ 真实 RTL 时序（推迟到 RTL 实施阶段）

---

## §6 关联文档

- [`proposal.md`](../proposal.md) — 提案总览
- [`design.md`](../design.md) — 设计文档（实施期）
- [`tasks.md`](../tasks.md) — TDD 5 步任务
- [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../../../../docs/soc_arch/architecture/21-soc-topology-mvp.md) — SoC 顶层物理布局规范 SSOT

---

## §7 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: V3.1-Rev2.0 拓扑修正 delta spec（7 REQ + 6 Scenario + 8 Acceptance Criteria） |