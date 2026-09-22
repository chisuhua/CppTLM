# ADR-SOC-21: dGPU SoC V3.1-Rev2.0 拓扑修正 — 三大子系统归属与 CXL 接口定位

> **状态**: 📋 Proposed — 2026-09-19
> **日期**: 2026-09-19
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: MAS-3.1 dGPU SoC 顶层物理布局冻结 + 9 份架构文档一致性 + RTL-IFC 接口约束
> **类别**: SoC 架构 / V3.1 拓扑修正
> **关联文档**:
> - [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../architecture/21-soc-topology-mvp.md) — SoC 顶层物理布局规范 SSOT (新)
> - [`docs/soc_arch/architecture/21-tee-udd-mvp.md`](../architecture/21-tee-udd-mvp.md) — TEE + UDD 数据面（已修正）
> - [`docs/soc_arch/architecture/21-dma-backends-mvp.md`](../architecture/21-dma-backends-mvp.md) — UBC + HBM/NIC/IO-DMA（已修正）
> - [`docs/soc_arch/architecture/21-fabric-switch-mvp.md`](../architecture/21-fabric-switch-mvp.md) — Fabric + Scale-Up Switch（已修正）
> - [`docs/soc_arch/architecture/21-microarch-ifc-mvp.md`](../architecture/21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC（已修正）
> **关联 ADR**:
> - ADR-088 §D5（23 ABI 冻结）
> - ADR-SOC-10（ModuleFactory 拓扑层）
> - ADR-SOC-11（PcieEndpointIP，**替代 PcieEndpointTLM**；PcieEndpointTLM 已 `[[deprecated]]` 标注保留向后兼容，per [ADR-SOC-11-pcie-endpoint-ip.md](./ADR-SOC-11-pcie-endpoint-ip.md) + AGENTS.md KEY INVARIANTS）
> - ADR-SOC-13（AXI Stream Adapter Mapper）
> - ADR-SOC-19（AXI Master Outbound Bridge）
>
> **ADR 编号追溯说明** (追溯至 [`openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/proposal.md`](../../../openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/proposal.md)):
> - 本 ADR 原计划使用 **ADR-SOC-11** 编号（V3.1-Rev2.0 拓扑修正）
> - **发现冲突**: ADR-SOC-11 已被 PcieEndpointIP 决策占用（签署日期 2027-02-09）
> - **追溯决策**: 采用 **ADR-SOC-21** 编号（下一个空闲编号），9 份关联架构文档同步引用 ADR-SOC-21
> - **追溯范围**: 8 份已 ship 子系统文档 + OpenSpec 4 份 + Oracle 评审报告 全部已替换
> - **追溯验证**: 全部 9 份架构文档均引用 ADR-SOC-21，残留 ADR-SOC-11 仅指向 PcieEndpointIP（正确，未误用）
> **关联 OpenSpec**: [`2026-09-19-cpptlm-mas-soc-topology-mvp/`](../../../openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/proposal.md)

---

## 1. Context（背景）

### 1.1 V3.0 拓扑的 3 个错误

CppTLM dGPU 在 MAS-3.1 体系下，从 V3.0 紧耦合 DMA 演进到 V3.1 三层解耦（TEE/UDD/UBC）的过程中，**SoC 顶层物理布局**暴露出 3 个关键错误，必须在 RTL 实现冻结前修正：

#### 错误 1: TC-DMA 物理归属歧义

**V3.0 状态**: TC-DMA 文档中曾描述"与 HBM-DMA 平级，可直连 HBM"，导致 RTL 团队可能误实现为：
```
TC-DMA ↔ HBM-DMA (直连)
```

**问题**:
- **破坏 mbarrier 硬件触发确定性延迟**: TC-DMA 紧耦合 SMEM 设计依赖 GPC 内 1 cycle 触发延迟（per `21-microarch-ifc-mvp.md` §9.1）
- 直连 HBM-DMA 会绕过 GPC UDD Agent → Global UDD Hub 路径，无法保证 SMEM 写入与 mbarrier 触发之间 0 cycle 间隔
- 失去 SMEM 直达的优势（per MAS-3.1-DMA-TEE Rev2.0 §8）

**实际正确路径**（V3.1 Rev2.0）：
```
SM → GPC 内 TC-DMA → GPC UDD Agent (tc_udd_req_*)
   → Compute NoC → Global UDD Hub → HBM-DMA
   → 经原路返回 TC-DMA → SMEM 直达 + mbarrier 触发
```

#### 错误 2: IO-DMA / PCIe 端口拓扑图缺失

**V3.0 状态**: 拓扑图中只画 NIC-DMA（Scale-Up），未显式区分 IO-DMA（PCIe），导致：
- driver 视角对"Host 通信"与"Peer GPU 通信"职责混淆
- RTL 团队无法明确 IO-DMA 寄存器（PCIe CTRL / PCIe STATUS / GDS CTRL）的归属子系统
- v1.1 引入 IO-DMA 时，缺乏预先的子系统边界定义

**实际正确路径**（V3.1 Rev2.0）：
- IO-DMA 在 Fabric & Edge IO Subsystem 内，与 NIC-DMA 平级
- IO-DMA vs NIC-DMA 对比表（per `21-soc-topology-mvp.md` §2.3）

#### 错误 3: CXL 接口位置错误（GPU Die 上无 CXL）

**V3.0 状态**: 部分文档隐含"GPU Die 内有 CXL Bridge"，并错误地让 HRT Route_Tag 包含"→ CXL"路径，导致：
- RTL 团队误实现 GPU 侧 CXL.mem/CXL.cache 协议栈
- Die 面积浪费 3-5%
- 验证复杂度与多厂商 CXL 互操作性测试成本上升
- AWT Trap Type `0x1 = "Remote CXL Poison"` 易让人误解为"GPU 直连 CXL"

**实际正确路径**（V3.1 Rev2.0）：
- **GPU Die 上无 CXL PHY/Controller**（per `21-soc-topology-mvp.md` §2.5）
- CXL Memory Pool 是外部 Scale-Up Switch 下挂设备
- GPU 对 CXL 内存的访问是 **"Native UALink + Remote CXL Translation"**
- AWT Trap Type `0x1` 明确为 `REMOTE_CXL_POISON_VIA_SWITCH`（per `21-microarch-ifc-mvp.md` §7.6）

### 1.2 8 份子系统文档暴露的拓扑问题

8 份子系统文档（`21-tee-udd-mvp.md` / `21-dma-backends-mvp.md` / `21-fabric-switch-mvp.md` / `21-microarch-ifc-mvp.md` + 4 份 Roadmap）在 V3.0 编写时**未充分体现 SoC 顶层物理布局约束**，导致：

| 文档 | V3.0 拓扑相关问题 |
|------|---------------------|
| `21-fabric-switch-mvp.md` §1.2 | 仅隐含"GPU Die 内不存在 CXL 协议状态机"，未显式画出 CXL Memory Pool 在外部设备 |
| `21-fabric-switch-mvp.md` §4.4 | Route_Tag 表 "→ CXL" 措辞易误读 |
| `21-tee-udd-mvp.md` | 缺 GUPA 空间划分表 + HRT Route_Tag 分配表 |
| `21-microarch-ifc-mvp.md` §7.6 | AWT Trap Type `0x1 = "Remote CXL Poison"` 命名不明确 |

### 1.3 9 份文档 V3.1-Rev2.0 修正已完成

截至 2026-09-19，已完成以下 V3.1-Rev2.0 修正（per `tasks.md` T0/T2）：

- ✅ **新增** `21-soc-topology-mvp.md`（SoC 顶层物理布局规范 SSOT）
- ✅ **修正** `21-fabric-switch-mvp.md` §1.2 / §4.4 / §7.1
- ✅ **修正** `21-tee-udd-mvp.md` §4.0 GUPA 空间划分 + HRT Route_Tag 分配
- ✅ **修正** `21-microarch-ifc-mvp.md` §7.6 AWT Trap Type 区分 Local/Remote
- ✅ **修正** `21-dma-backends-mvp.md` 关联文档
- ✅ **修正** 4 份 Roadmap 关联文档

### 1.4 9 份文档验证一致性

| 验证项 | 结果 |
|--------|------|
| 全部 9 份文档引用 `21-soc-topology-mvp.md` | ✅ |
| 全部 9 份文档引用 ADR-SOC-21 | ✅ |
| 全部 9 份文档引用 V3.1-Rev2.0 | ✅ |
| 关键概念"CXL 不在 GPU Die"跨 5 份文档 | ✅ |
| 关键概念"HRT 无 CXL_DIRECT"跨 3 份文档 | ✅ |
| 关键概念"REMOTE_CXL_POISON_VIA_SWITCH"跨 3 份文档 | ✅ |
| 关键概念"TC-DMA 归属 GPC 内"跨 2 份文档 | ✅ |

---

## 2. Decision（决策）

### D1. TC-DMA 物理归属 GPC 内（不与 HBM-DMA 平级）

**决策**: TC-DMA 物理位置固定在 **GPC Subsystem** 内（与 SM/L2/UDD Agent 紧耦合），**禁止**作为独立 Backend 直连 HBM-DMA。

**理由**:
1. **mbarrier 硬件触发确定性延迟**: TC-DMA 与 SMEM 直达路径依赖 GPC 内 1 cycle 延迟（per `21-microarch-ifc-mvp.md` §9.1）
2. **TMA 跨域 5 步路径**: TC-DMA 必须经 GPC UDD Agent 间接访问 HBM，不能跳过
3. **避免 RTL 实现误连**: 显式物理归属避免 RTL 工程师误读"平级"

**实施**:
- `include/tlm/soc/soc_topology_defs.hh` 定义 `TC_DMA_LOCATION_GPC_INSIDE` 与 `TC_DMA_FORBIDDEN_DIRECT_HBM` 宏
- `src/tlm/soc/soc_shell.cc::validate_topology()` 检测 TC-DMA 仅在 GPC 内
- TC-DMA 物理接口 `tc_udd_req_*` / `udd_tc_resp_*` 在 `21-microarch-ifc-mvp.md` §5.2 定义

### D2. GPU Die 上严禁 CXL PHY/Controller

**决策**: **GPU Die 上严禁包含任何 CXL PHY 或 CXL Controller**。所有跨节点 CXL 内存访存必须通过 NIC-DMA (UALink) → 外部 Scale-Up Switch → CXL Memory Pool。

**理由**:
1. **Die 面积节省**: 完整 PCIe + CXL Controller 占 Die 面积 3-5%
2. **验证复杂度降低**: GPU 仅需 UALink 1.1 Mem 子集验证，无需 CXL.mem/CXL.cache
3. **互操作性解耦**: GPU 不依赖特定 CXL 厂商
4. **符合 SoC 协议栈极简原则**: GPU 仅处理 UALink，CXL 协议转换黑盒到 Switch

**实施**:
- `soc_topology_defs.hh` 定义 `GPU_DIE_FORBIDDEN_CXL_PHY` 与 `GPU_DIE_FORBIDDEN_CXL_CONTROLLER` 宏
- `soc_shell.cc::validate_topology()` 检测 GPU Die 无 CXL
- `21-fabric-switch-mvp.md` §1.2 / §4.4 / §7.1 显式标注 V3.1-Rev2.0 拓扑修正
- `21-soc-topology-mvp.md` §2.5 显式画出 CXL Memory Pool 在 Scale-Up Switch 下

### D3. HRT 严禁 `CXL_DIRECT` Route_Tag

**决策**: HRT Route_Tag 分配严格限制为 `0x0=HBM, 0x1~0xE=UALink, 0xF=PCIe`，**严禁**新增 `CXL_DIRECT` Route_Tag。所有 CXL 访存统一通过 `SWITCH_PORT_N`（NIC-DMA）路由，由外部 Scale-Up Switch PTE 决定目标是 Peer GPU HBM 还是 CXL Memory Pool。

**理由**:
1. **GPU 不感知 CXL**: GPU 协议栈仅需 UALink，无需识别 CXL.mem/CXL.cache
2. **HRT 简洁性**: 16 槽位 Route_Tag 足够（HBM + UALink + PCIe）
3. **避免概念混淆**: 明确"GPU 不直接路由到 CXL"
4. **符合 SoC 协议栈极简原则**

**实施**:
- `soc_topology_defs.hh` 定义 `HRT_FORBIDDEN_CXL_DIRECT_ROUTE_TAG` 与 `HRT_ROUTE_TAG_MAX = 0xF` 宏
- `soc_shell.cc::validate_topology()` 检测 HRT 无 CXL_DIRECT
- `21-tee-udd-mvp.md` §4.0 新增 GUPA 空间划分表 + HRT Route_Tag 分配表
- `21-fabric-switch-mvp.md` §4.4 移除"→ CXL"措辞，改为"via Switch"

### D4. IO-DMA 在 Fabric & Edge IO Subsystem（与 NIC-DMA 平级）

**决策**: IO-DMA (PCIe Gen6 + ATS/PRI + GDS, v1.1+) 作为独立 Backend 在 **Fabric & Edge IO Subsystem** 内，与 NIC-DMA 平级但物理端口分离。

**理由**:
1. **职责明确**: IO-DMA 面向 Host/NVMe（PCIe TLP），NIC-DMA 面向 Peer GPU/Switch（UALink Flit）
2. **物理端口独立**: 避免单 Backend 同时处理 PCIe + UALink 协议冲突
3. **演进灵活**: v1.1+ 加 IO-DMA 时不影响 NIC-DMA

**实施**:
- `soc_topology_defs.hh` 定义 `SUBSYSTEM_FABRIC_IO_BOUNDARY_MAX = 5`（NIC-DMA Port 4 + IO-DMA 1）
- `soc_shell.cc::instantiate_all_subsystems()` 注入 NIC-DMA × 4 + IO-DMA × 1
- `21-soc-topology-mvp.md` §2.3 完整定义 IO-DMA vs NIC-DMA 对比表
- `21-microarch-ifc-mvp.md` §7.5 预留 IO-DMA CSR 寄存器空间（v1.1+ 启用）

### D5. CXL Drain 三方协调机制（GPU-Switch-FM）

**决策**: CXL Drain 采用 **GPU NIC-DMA + Scale-Up Switch PTE + Fabric Manager (FM)** 协调的两阶段 + 三方协议。

**理由**:
1. **避免 Drain 死锁**: 单端主导易死锁
2. **CXL 内存池热插拔/故障隔离**: 需三方确认双向无在途
3. **符合业界标准**: NVIDIA FM + UALink + CXL Hot-plug 实践

**实施**:
- `21-soc-topology-mvp.md` §4.2 完整定义 CXL Drain 三方协调 5 阶段
- `21-fabric-switch-mvp.md` §6 + §9 GPU-Switch Drain 协议
- `21-dma-backends-mvp.md` §6.4 NIC-DMA 两阶段 Drain FSM
- `21-microarch-ifc-mvp.md` §6.2 Drain 控制信号 + 100us 超时约束

---

## 3. 无债务演进约束（3 条不变量）

### Invariant 1: TC-DMA 物理归属 GPC 内（不可变）

> v1.0 MVP ship 后, TC-DMA 物理位置固定在 GPC 内（与 SMEM/L2 紧耦合）。
>
> v1.1+ 演进时**不**可将 TC-DMA 移出 GPC（否则破坏 mbarrier 硬件触发确定性延迟约束，per `21-microarch-ifc-mvp.md` §9.1）。

### Invariant 2: GPU Die 上无 CXL PHY/Controller（严格剥离）

> v1.0 MVP ship 后, GPU Die 上**不**包含任何 CXL PHY/CXL Controller/CXL Bridge。
>
> 所有跨节点 CXL 内存访存必须通过 NIC-DMA (UALink) → 外部 Scale-Up Switch → CXL Memory Pool。
> v1.1+ 演进时**不**可在 GPU Die 上添加 CXL 协议栈（违背"GPU 协议栈极简"原则）。

### Invariant 3: HRT 无 `CXL_DIRECT` Route_Tag（统一通过 SWITCH_PORT_N）

> v1.0 MVP ship 后, HRT Route_Tag 分配固定为 0x0 (HBM) + 0x1~0xE (UALink Port) + 0xF (PCIe)。
>
> v1.1+ 演进时**不**可新增 "CXL_DIRECT" Route_Tag（破坏 GPU-Switch 协议层解耦）。
> 所有 CXL Memory Pool 访问统一通过 SWITCH_PORT_N → NIC-DMA → Scale-Up Switch（PTE 内部转换）。

---

## 4. Consequences（影响）

### 4.1 正面影响

1. **RTL 实现清晰**: 5 大子系统边界明确，RTL 工程师不再有归属歧义
2. **TC-DMA mbarrier 性能保证**: 严格保持 GPC 紧耦合，RTL 不允许直连 HBM
3. **IO-DMA / NIC-DMA 边界清晰**: Host 通信 vs Peer GPU 通信职责明确
4. **CXL Memory Pool 物理位置显式**: GPU 团队无需实现 CXL 协议栈（Die 面积节省 3-5%）
5. **HRT Route_Tag 无歧义**: 0x0/0x1~0xE/0xF 三段语义明确，CXL 通过 SWITCH_PORT_N 统一路由
6. **AWT Trap Type 命名明确**: `REMOTE_CXL_POISON_VIA_SWITCH` 显式标注 Poison 来源
7. **跨子系统文档一致性**: 9 份文档全部引用 SoC-Topology + ADR-SOC-21

### 4.2 负面影响

1. **v3.0 → V3.1-Rev2.0 迁移成本**: RTL 实现需重新审视图纸（已通过 9 份文档修正消除歧义）
2. **CIU HRT 写入路径需拒绝 `CXL_DIRECT`**: 增加 1 项运行时检查（per `soc_shell.cc::validate_topology()`）
3. **AWT Trap Type 命名变更**: `0x1 = "Remote CXL Poison"` → `0x1 = "REMOTE_CXL_POISON_VIA_SWITCH"`，driver 需同步更新（向后兼容）

### 4.3 风险与缓解

| 风险 | 等级 | 缓解策略 |
|------|------|----------|
| RTL 团队误解 TC-DMA 归属 | 高 | SoC 顶层拓扑图明确画出 TC-DMA 在 GPC 内；RTL-IFC §3.2 已定义 TC-DMA ↔ UDD Agent 接口 |
| IO-DMA vs NIC-DMA 混用 | 中 | `21-soc-topology-mvp.md` §2.3 对比表 + `21-microarch-ifc-mvp.md` §7.5 预留 CSR |
| RTL 实现引入片上 CXL | 中 | ADR-SOC-21 Invariant 2 严格禁止；SoC 顶层拓扑图 CXL 在 Die 外 |
| HRT Route_Tag 引入 `CXL_DIRECT` | 低 | ADR-SOC-21 Invariant 3 严格禁止；4 份子系统 MVP 文档交叉引用 |
| Drain 协议误用 | 低 | `21-soc-topology-mvp.md` §4.2 三方协调机制明确 |
| Oracle 评分 < 9.0 | 中 | Oracle 复评 ≤ 3 次 |

---

## 5. 5 阶段约束

### 5.1 v1.0 MVP（已 ship）

- ✅ 9 份架构文档已 V3.1-Rev2.0 修正完成
- ✅ 3 条不变量定义完成
- ⏳ ADR-SOC-21 起草完成
- ⏳ SoC 顶层 C++ 基础设施（`soc_topology_defs.hh` + `soc_shell.cc`）实施
- ⏳ `test_soc_topology_e2e.cc` 10 步端到端 demo 实施
- ⏳ Oracle 评审（预期 ≥9.0/10 PASS）

### 5.2 v1.1+（未来演进约束）

| 演进项 | 3 条不变量约束 |
|--------|----------------|
| TC-DMA 多流并发 | Invariant 1: 物理位置仍 GPC 内 |
| IO-DMA 详细 CSR | Invariant 4: IO-DMA 在 Fabric & Edge IO Subsystem |
| CXL Backend 真实联调 | Invariant 2: 仍经 Scale-Up Switch |
| Hot-plug / Hot-unplug | Invariant 5: CXL Drain 三方协调 |
| Multicast / All-Reduce | Invariant 3: HRT Route_Tag 仍 16 槽位 |
| Coherence SnoopFilter | Invariant 2: GPU 仍无 CXL Controller |

### 5.3 v3.0+（长期收敛）

所有演进最终收敛到 v3.0 完整 Multicast + Coherence 状态，3 条不变量保持不变。

---

## 6. 关联文档

### 6.1 SSOT

- [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../architecture/21-soc-topology-mvp.md) — SoC 顶层物理布局规范 SSOT (新)

### 6.2 9 份架构文档（已 V3.1-Rev2.0 修正）

- [`docs/soc_arch/architecture/21-tee-udd-mvp.md`](../architecture/21-tee-udd-mvp.md) — TEE + UDD 数据面
- [`docs/soc_arch/architecture/21-dma-backends-mvp.md`](../architecture/21-dma-backends-mvp.md) — UBC + HBM/NIC/IO-DMA
- [`docs/soc_arch/architecture/21-fabric-switch-mvp.md`](../architecture/21-fabric-switch-mvp.md) — Fabric + Scale-Up Switch
- [`docs/soc_arch/architecture/21-microarch-ifc-mvp.md`](../architecture/21-microarch-ifc-mvp.md) — Core/Micro-Arch + RTL-IFC
- [`docs/soc_arch/architecture/21-tee-udd-evolution-roadmap.md`](../architecture/21-tee-udd-evolution-roadmap.md) — TEE-UDD 演进路线图
- [`docs/soc_arch/architecture/21-dma-backends-evolution-roadmap.md`](../architecture/21-dma-backends-evolution-roadmap.md) — Backends 演进路线图
- [`docs/soc_arch/architecture/21-fabric-switch-evolution-roadmap.md`](../architecture/21-fabric-switch-evolution-roadmap.md) — Fabric+Switch 演进路线图
- [`docs/soc_arch/architecture/21-microarch-ifc-evolution-roadmap.md`](../architecture/21-microarch-ifc-evolution-roadmap.md) — MicroArch+IFC 演进路线图

### 6.3 OpenSpec

- [`openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/`](../../../openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/proposal.md) — OpenSpec 提案
  - `proposal.md` — Why / What Changes / Scope / Acceptance Gate
  - `design.md` — 代码组织 / 模块依赖 / TDD 5 步
  - `specs/soc-topology-mvp/spec.md` — Delta Spec (7 REQ + 6 Scenario)
  - `tasks.md` — TDD 5 步任务清单 (T0-T5)

---

## 7. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: V3.1-Rev2.0 拓扑修正 ADR (5 项决策 D1-D5 + 3 条不变量 + 9 份架构文档一致性 + 5 阶段约束) |