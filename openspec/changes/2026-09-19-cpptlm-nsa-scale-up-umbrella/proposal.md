# 2026-09-19-cpptlm-nsa-scale-up-umbrella: MAS-3.1 NSA-aware Scale-Up 演进 (Umbrella Proposal)

> **状态**: 📋 Proposed — 2026-09-19
> **目的**: 在 CppTLM dGPU 仓内建立 **NSA-aware Scale-Up 演进体系**, 作为 9 份 NSA 草案 + 3 份 ADR 的 **umbrella 提案**。NSA-aware Scale-Up 是 V3.1-Rev2.0 的下一阶段 (v1.x / v3.x), 借鉴 NVIDIA NVL72 / AMD MI300X / Intel Xe-HPC + CXL 3.0 业界方向。
>
> **关联设计**: 
> - [`docs/soc_arch/architecture/22-nsa-fabric-address-spec.md`](../../../../docs/soc_arch/architecture/22-nsa-fabric-address-spec.md) NSA-aware 64-bit Fabric Address 规范
> - [`docs/soc_arch/architecture/23-dist-scale-up-topology.md`](../../../../docs/soc_arch/architecture/23-dist-scale-up-topology.md) NSA-aware SoC 拓扑
> - [`docs/soc_arch/architecture/24-host-gpu-pcie-ifc.md`](../../../../docs/soc_arch/architecture/24-host-gpu-pcie-ifc.md) Host-GPU PCIe 详细
> - [`docs/soc_arch/architecture/25-nsa-hardware.md`](../../../../docs/soc_arch/architecture/25-nsa-hardware.md) NSA-aware MMU + Remote Atomic + Directory
> - [`docs/soc_arch/architecture/26-gsp-rm-firmware.md`](../../../../docs/soc_arch/architecture/26-gsp-rm-firmware.md) GSP-RM 固件
> - [`docs/soc_arch/architecture/27-nsa-capability.md`](../../../../docs/soc_arch/architecture/27-nsa-capability.md) Capability 多租户
> - [`docs/soc_arch/architecture/28-cxl-3-fabric.md`](../../../../docs/soc_arch/architecture/28-cxl-3-fabric.md) CXL 3.0 Fabric 兼容
> - [`docs/soc_arch/architecture/29-nsa-evolution-roadmap.md`](../../../../docs/soc_arch/architecture/29-nsa-evolution-roadmap.md) 5 阶段演进
> - [`docs/soc_arch/architecture/21-dist-scale-up-topology-b.md`](../../../../docs/soc_arch/architecture/21-dist-scale-up-topology-b.md) 方案 B 降级替代

> **NSA 命名含义澄清** (适用于所有 NSA 草案, 2026-09-19 决策):
>
> 本草案中 **NSA** 含义为 **"Network-System Architecture"** (多 Linux 节点 + 跨 Fabric 寻址架构), 与美国 **National Security Agency (国家安全局)** **无关**, 仅 CppTLM 内部使用。
>
> NSA 衍生术语对应关系:
>
> | NSA 衍生术语 | 含义 | 与 CXL 3.0 / NVLink / Infinity Fabric 对应 |
> |---|---|---|
> | NSA Switch | NSA Switch (跨 Fabric 路由器) | ≈ CXL Fabric Switch Tier 1 |
> | NSA Fabric Address | 64-bit NSA-aware 地址 (16+48 bits) | ≈ CXL Fabric Address |
> | NSA-aware MMU | 含 Fabric ID + Capability 的 MMU | ≈ CXL-aware IOMMU |
> | NSA Stage 1/2/3 | 5 阶段演进阶段 | ≈ CXL 3.0 Fabric 量产节奏 |
> | NSA-aware SoC | NSA-aware 分布式 SoC 终态 | ≈ CXL 3.0 Fabric-aware SoC |
>
> **替代命名参考** (若未来需替换): GFS (Global Fabric System) / FAS (Fabric Address Space) / UFA (Unified Fabric Address), **当前决策保持 NSA + 备注澄清**。
>

> **关联 ADR**: 
> - **ADR-SOC-22**: NSA-aware 64-bit Fabric Address 格式决策
> - **ADR-SOC-23**: NSA-aware vs 方案 B 选型决策
> - **ADR-SOC-24**: Capability-Based 多租户隔离选型决策
> - **ADR-SOC-21**: V3.1-Rev2.0 拓扑修正决策基础

---

## Why

### 1. V3.1-Rev2.0 的局限

V3.1-Rev2.0 (`21-soc-topology-mvp.md` v3.1) 是 **1 个 Linux 节点 + 1 个 PCIe Hierarchy** 架构, 已提交 9 份架构草案 (Proposed 状态, 评审中) + 1 份 ADR + 4 份 Roadmap, Oracle 评审 9.62/10 PASS。

但 V3.1-Rev2.0 存在 3 大局限:

1. **单 Linux 节点**: 故障域 = 1 个, Host OS 崩溃 = 8 GPU 不可用
2. **单 PCIe Hierarchy**: 跨机柜扩展受限, 无法支持超大规模 (128+ GPU)
3. **GUPA 仅本地寻址**: 64-bit 路由域划分仅覆盖单 Compute Tray, 无法跨 Fabric

### 2. 业界方向

NVIDIA NVL72 / AMD MI300X CXD / Intel Xe-HPC + CXL 3.0 都向**分布式 Scale-Up + Capability 隔离 + 跨 Fabric 寻址**演进。NSA-aware Scale-Up 是 CppTLM 必须跟随的**业界方向**。

### 3. 主因 (按阻断优先级排序)

1. **缺 NSA-aware 升级路径** (最关键): V3.1-Rev2.0 无 v1.x / v3.x Roadmap
2. **缺 Capability 多租户隔离**: 仅 RCT 软件维护, 硬件强制隔离缺失
3. **缺 GSP-RM 控制面下沉**: Host KMD 5x 厚 (100K LOC), 控制面介入热路径 20-35 μs

### 4. 预期收益

- **NSA-aware 64-bit Fabric Address**: 跨 Fabric 寻址 + CXL 3.0 兼容
- **NSA-aware MMU + Remote Atomic + Hardware Directory**: 跨 tray 延时 ~500 ns (vs 方案 B 1.8 μs)
- **Capability-Based 多租户硬件强隔离**: 借鉴 CHERI + seL4, HW 强制 + 形式化可验证
- **GSP-RM 控制面下沉**: 借鉴 NVIDIA Hopper H100 GSP-RM, 控制面介入 5-10x 加速
- **0 个新 ABI 函数** (per ADR-088 §D5 严格遵守)
- **降级路径完整**: 方案 B 作为 NSA 硬件不就绪时的 fallback

---

## What Changes

### 新增文件

| 文件 | 状态 | 步骤 |
|------|------|:----:|
| `openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/proposal.md` | **新** (umbrella scope + 9 草案引用) | T0 |
| `openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/design.md` | **新** (数据通路 + 控制面契约引用) | T0 |
| `docs/soc_arch/architecture/22-nsa-fabric-address-spec.md` | **新** (草案 1, 已提交 Proposed) | T0 |
| `docs/soc_arch/architecture/23-dist-scale-up-topology.md` | **新** (草案 2, 已提交 Proposed) | T0 |
| `docs/soc_arch/architecture/24-host-gpu-pcie-ifc.md` | **新** (草案 3, 已提交 Proposed) | T0 |
| `docs/soc_arch/architecture/25-nsa-hardware.md` | **新** (草案 4, 已提交 Proposed + Oracle P0 修正) | T0 |
| `docs/soc_arch/architecture/26-gsp-rm-firmware.md` | **新** (草案 5, 已提交 Proposed + Oracle P0 修正) | T0 |
| `docs/soc_arch/architecture/27-nsa-capability.md` | **新** (草案 6, 已提交 Proposed) | T0 |
| `docs/soc_arch/architecture/28-cxl-3-fabric.md` | **新** (草案 7, 已提交 Proposed) | T0 |
| `docs/soc_arch/architecture/29-nsa-evolution-roadmap.md` | **新** (草案 8, 已提交 Proposed + Oracle P1 修正) | T0 |
| `docs/soc_arch/architecture/21-dist-scale-up-topology-b.md` | **新** (方案 B 降级) | T0 |
| `docs/soc_arch/adr/ADR-SOC-22-nsa-fabric-address-format.md` | **新** (NSA Fabric Address 决策) | T0 |
| `docs/soc_arch/adr/ADR-SOC-23-nsa-vs-plan-b-selection.md` | **新** (NSA vs 方案 B 选型决策) | T0 |
| `docs/soc_arch/adr/ADR-SOC-24-capability-based-multitenant-isolation.md` | **新** (Capability 选型决策) | T0 |
| `docs/validation/nsa-scale-up-oracle-pass.md` | **新** (Oracle 评审报告) | T5 |
| `test/test_nsa_scale_up_e2e.cc` | **新** (端到端 demo, TDD 5 步) | T4 |

**注意**: NSA 草案 9 份 + 方案 B + 3 份 ADR 已在本会话提交 (Proposed 状态), umbrella 提案仅整合。

### 修改文件 (后续 NSA 阶段实施时)

| 文件 | 状态 | 阶段 |
|------|------|------|
| V3.1-Rev2.0 9 份架构文档 | **不改** (NSA 草案兼容路径完整, per `22-nsa-fabric-address-spec.md` §4) | NSA Stage 1 |
| V3.1-Rev2.0 4 份 Roadmap | **加 Status Update 段** (v4.0 NSA-aware 演进路径) | NSA Stage 1 |
| `21-microarch-ifc-mvp.md` §4.2 | **已加** (Host-GPU PCIe 详细, per `24-host-gpu-pcie-ifc.md`) | 已提交 Proposed |

---

## Scope

### In Scope (NSA Stage 1, v1.x, 2026-2027, **TLM 功能模型层 per ADR-SOC-23 D3**)

> **载体声明 (per Oracle 评审 2026-09-20)**: CppTLM 是 TLM 2.0 **仿真框架**（非 RTL 项目）。NSA Stage 1 范围严格限定为 **TLM 功能模型 + 延时注入参数**，不涉及物理 RTL 实现。所有"硬件/RTL"措辞在本阶段理解为"TLM 模型内对应的功能模块"。

- [x] **NSA-aware 64-bit Fabric Address 格式**: 16-bit Fabric ID + 48-bit Local Addr (per ADR-SOC-22, TLM 字段定义)
- [x] **NSA-aware MMU / TLB TLM 模型**: 8-bit Fabric ID 翻译支持 (256 节点), Capability **TLM 校验路径** 与 HW 校验路径 TLM 复现并行 (per `25-nsa-hardware.md` §2, HW 真实电路推迟到 Stage 2)
- [x] **GSP-RM 微控制器 TLM 模型 + 固件行为仿真**: RISC-V 200-400 MHz 行为级 TLM 模拟, 4 大服务 (Memory/Fault/Fabric/Tenant) 固件功能仿真, per `26-gsp-rm-firmware.md`（**注**: Stage 1 阶段 CppTLM 仓不交付真实 RTL；如需 RTL 实施, 需另起 RTL 仓 change）
- [x] **Capability-Based 多租户 TLM 模型**: 128 bits Token + TLM 校验路径 + 5 阶段生命周期 (per `27-nsa-capability.md`, HW 强制校验推迟到 Stage 2)
- [x] **HRT Entry 32-bit 字段扩展 (TLM 数据结构)**: 含 Fabric ID, 与 V3.1-Rev2.0 兼容 (per ADR-SOC-22 D3)
- [x] **薄 Host driver**: 5x LOC 减少 (100K → 20K LOC) — 仅约束口径, 非本仓交付
- [x] **NSA-aware CIU 注入 Fabric ID 路径 (TLM 接口)**: per `25-nsa-hardware.md` §5
- [x] **FM↔GSP-RM 控制面契约 (TLM 数据交换)**: 决策/执行分工 + Owner Directory 同步 + Drain 时序 (per `26-gsp-rm-firmware.md` §0.5)
- [x] **NSA Switch Linux FM (方案 B 降级路径, TLM)**: 降级路径完整 (per `21-dist-scale-up-topology-b.md`)

### Out of Scope (推迟到 NSA Stage 2/3, v3.x)

> Stage 2/3 涉及物理硬件（Remote Atomic 电路、Directory L1/L2/L3、NSA Switch CXL PHY），由 **RTL 仓**承接。本仓仅提供 TLM 行为模型与时序参数注解。

- ❌ **Remote Atomic Unit 物理硬件**: 推迟到 NSA Stage 2 (v3.x, per `25-nsa-hardware.md` §3, 由 RTL 仓实施)
- ❌ **Hardware Directory L1/L2/L3 物理硬件**: 推迟到 NSA Stage 2 (per `25-nsa-hardware.md` §4, 由 RTL 仓实施)
- ❌ **NSA Switch 物理 CXL 3.0 Fabric Switch Tier 1**: 推迟到 NSA Stage 2 (per `28-cxl-3-fabric.md` §3, 由 RTL 仓实施)
- ❌ **16-bit Fabric ID 完整启用 (TLM 仿真仅占位)**: 推迟到 NSA Stage 3 (per ADR-SOC-22)
- ❌ **CXL 3.0 Fabric 完整兼容 (物理层)**: 推迟到 NSA Stage 3 (由 RTL 仓实施)
- ❌ **跨数据中心 (Multi-Fabric)**: 推迟到 NSA Stage 3 (per `28-cxl-3-fabric.md` §5.2)

---

## Acceptance Gate (10 项, NSA Stage 1 TLM 功能模型层 验证)

> **载体声明 (per Oracle 评审 2026-09-20)**: CppTLM 是 TLM 2.0 **仿真框架**（非 RTL 项目）。AG3/AG4/AG5 涉及 HW/RTL 的部分，在 Stage 1 改为"TLM 功能模型 + 延时注入参数"。物理 RTL 验收移交给 RTL 仓。AG8/AG9 涉及物理指标（μs / GB/s）改为"TLM 仿真模型注入参数 ≤ X"。

- [ ] **AG1**: NSA-aware 64-bit Fabric Address 格式 TLM 字段定义 (per `22-nsa-fabric-address-spec.md` §2)
- [ ] **AG2**: HRT Entry 32-bit NSA-aware 字段位分配 (TLM 数据结构) (per ADR-SOC-22 D3)
- [ ] **AG3**: NSA-aware MMU / TLB **TLM 模型** 支持 8-bit Fabric ID 翻译 (per `25-nsa-hardware.md` §2 TLM 部分)
- [ ] **AG4**: Capability Token 128 bits TLM 模型 + **TLM 校验路径** (per `27-nsa-capability.md` §2 + §4, HW 强制校验推迟到 Stage 2)
- [ ] **AG5**: GSP-RM **TLM 功能模型** + 4 大服务固件行为仿真 (per `26-gsp-rm-firmware.md` §2 + §4, RTL 物理实施由 RTL 仓承接)
- [ ] **AG6**: FM↔GSP-RM 控制面契约 TLM 实施 (per `26-gsp-rm-firmware.md` §0.5)
- [ ] **AG7**: 数据通路流程图 (VA → Fabric Addr → Data 返回) (per `25-nsa-hardware.md` §0.5)
- [ ] **AG8**: Host↔GPU 4 KB Write **TLM 延时注入参数** ≤ 1.2 μs (95 分位) (per `24-host-gpu-pcie-ifc.md` §6.1, 仅作为 TLM 仿真模型参数基线, 物理验收由 RTL 仓)
- [ ] **AG9**: 8 GPU 总 Host↔GPU 带宽 **TLM 仿真模型参数** ≥ 200 GB/s (per `24-host-gpu-pcie-ifc.md` §4, 仅作为 TLM 仿真模型参数基线, 物理验收由 RTL 仓)
- [ ] **AG10**: 0 个新 ABI 函数 (per ADR-088 §D5)

---

## Capabilities (6 项)

### CAP-1: NSA-aware 64-bit Fabric Address (per ADR-SOC-22)

- 16-bit Fabric ID + 48-bit Local Addr
- 阶段 1 (v1.x): 8-bit Fabric ID 启用 (256 节点)
- 阶段 3 (v3.x): 16-bit Fabric ID 完整启用 (65K 节点)
- 与 CXL 3.0 Fabric Address 字段对齐

### CAP-2: NSA-aware SoC 拓扑 (per 草案 2)

- 5 大子系统 NSA-aware 升级 (GPC/Memory/Fabric-IO/Control/External)
- NSA-aware HRT Entry 32-bit 字段扩展
- Compute Tray 独立 CPU + Linux OS (N 个)
- NSA Switch (双角色, UALink Fabric + CXL Fabric)

### CAP-3: NSA-aware 硬件 (per 草案 4)

- NSA-aware MMU / TLB (Fabric ID + Capability HW 校验)
- Remote Atomic Unit 硬件 (per IEEE 750 / TSO 一致性)
- Hardware Directory L1/L2/L3 (MESIF 变体)
- 跨 tray 延时 ~500 ns (vs 方案 B 1.8 μs, 加速 3.6×)

### CAP-4: GSP-RM 固件架构 (per 草案 5)

- RISC-V 微控制器 + NV-RTOS (控制面高频)
- 4 大服务: Memory/Fault/Fabric/Tenant
- VirtIO Host Comm 接口
- FM↔GSP-RM 决策/执行分工
- Capability 签发 <1 μs (vs V3.1-Rev2.0 ~100 μs)

### CAP-5: Capability-Based 多租户 (per ADR-SOC-24 + 草案 6)

- 128 bits Capability Token
- HW 校验 (NSA-aware MMU 并行)
- 与 V3.1-Rev2.0 RCT 双层校验 (Stage 1) → 完全替代 (Stage 3)
- 借鉴 CHERI + seL4 Capability 思想
- 与 MIG 正交互补 (资源空间切分 + 地址访问权)

### CAP-6: CXL 3.0 Fabric 兼容 (per 草案 7)

- NSA Switch = CXL Fabric Switch Tier 1 + UALink Fabric
- NSA Fabric ID = CXL Fabric Port ID (16 bits 字段对齐)
- Port-based Routing (PBR) 引擎
- CXL 3.0 Coherency Engine

---

## Impact

### 影响范围

> **载体声明 (per Oracle 评审 2026-09-20)**: 本 change 影响范围严格限定为 CppTLM 仓（TLM 2.0 仿真框架）；物理硬件（NSA Switch CXL PHY / Remote Atomic Unit 电路 / Hardware Directory L1/L2/L3）由独立的 RTL 仓承接，本仓不交付 RTL。

- **8 份 NSA 草案 + 1 份方案 B**: 全部已提交 Proposed 状态, 含数据通路流程图 (Oracle P0 修正) + 控制面契约 + 依赖图 + Normative Glossary (Oracle P1 修正)
- **3 份 ADR (ADR-SOC-22/23/24)**: 已提交 Proposed, NSA 草案决策基础
- **V3.1-Rev2.0 兼容**: NSA Stage 1 期间 (Fabric ID = 0 默认), 无破坏性变更
- **方案 B 降级替代**: NSA 硬件不就绪时, 仍可出货 (per ADR-SOC-23 D2)
- **0 个新 ABI 函数**: 23 ABI 函数签名零修改 (per ADR-088 §D5 + ADR-SOC-20 ABI 二级精简基线)

### 风险评估

| 风险 | 等级 | 缓解策略 |
|------|------|----------|
| CXL 3.0 量产延期 | 中 | 降级到方案 B (per ADR-SOC-23 D2) |
| NSA Switch 自研失败 | 中 | 复用商业 UALink Switch 芯片 |
| 多 Linux 节点 KMD 协调失败 | 低 | 跨节点 PCIe over UALink 协议 + 分布式 KMD |
| 商业化与 NVIDIA 竞争劣势 | 中 | 开放标准 (CXL 3.0 + UALink) vs NVIDIA 闭源 NVLink |
| 客户不愿从 V3.1-Rev2.0 升级 | 低 | 兼容路径: V3.1-Rev2.0 → 方案 B → 方案 C 平滑升级 |

### 跨仓影响

- **UsrLinuxEmu**: 不修改 23 ABI 函数, driver 仅需更新 HRT/RCT 注入路径 (Stage 1 仅注入 8-bit Fabric ID)
- **Cpputlm 仓**: 8 份 NSA 草案 + 3 份 ADR + umbrella 提案 + 测试

---

## 关联文档

### 9 份 NSA 草案 + 1 份方案 B + 3 份 ADR

- 22-nsa-fabric-address-spec.md (草案 1, NSA Fabric Address)
- 23-dist-scale-up-topology.md (草案 2, NSA-aware SoC 拓扑)
- 24-host-gpu-pcie-ifc.md (草案 3, Host-GPU PCIe 详细)
- 25-nsa-hardware.md (草案 4, NSA-aware 硬件)
- 26-gsp-rm-firmware.md (草案 5, GSP-RM 固件)
- 27-nsa-capability.md (草案 6, Capability 多租户)
- 28-cxl-3-fabric.md (草案 7, CXL 3.0 Fabric 兼容)
- 29-nsa-evolution-roadmap.md (草案 8, 5 阶段演进)
- 21-dist-scale-up-topology-b.md (方案 B 降级)
- ADR-SOC-22 (NSA Fabric Address)
- ADR-SOC-23 (NSA vs 方案 B)
- ADR-SOC-24 (Capability 选型)

### V3.1-Rev2.0 已提交 Proposed 文档

- 21-soc-topology-mvp.md
- 21-tee-udd-{mvp,evolution-roadmap}.md
- 21-dma-backends-{mvp,evolution-roadmap}.md
- 21-fabric-switch-{mvp,evolution-roadmap}.md
- 21-microarch-ifc-{mvp,evolution-roadmap}.md

---

## 关联 ADR

- **ADR-SOC-22**: NSA-aware 64-bit Fabric Address 格式决策 ✅
- **ADR-SOC-23**: NSA-aware vs 方案 B 选型决策 ✅
- **ADR-SOC-24**: Capability-Based 多租户隔离选型决策 ✅
- **ADR-SOC-21**: V3.1-Rev2.0 拓扑修正决策基础

---

## 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: NSA-aware Scale-Up 演进 umbrella 提案 (整合 9 份草案 + 3 份 ADR + 10 项 Acceptance Gate + 6 项 Capabilities) |
