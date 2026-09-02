# Design — dGPU SoC v1.0 ADR-SOC 修订规划

> **配套**: [proposal.md](./proposal.md) | **修订规划权威源**: [`docs/soc_arch/adr/revision-plan-v1.0.md`](../../../docs/soc_arch/adr/revision-plan-v1.0.md)

---

## 1. 修订原则（per revision-plan-v1.0.md §2.1）

### 1.1 尊重 ADR 不可变原则
- 已签发 ADR 正文**不改**，通过 `## Status Update` 段追加状态变化
- 决策需变更 → 创建新 ADR（SOC-NN），旧 ADR 标注 "Superseded by ADR-SOC-NN"

### 1.2 决策与状态严格分离
| 变更类型 | 落点 |
|----------|------|
| 决策变更（Decision 内容修订） | 新 ADR + 旧 ADR Status Update 标 "Superseded by ADR-SOC-NN" |
| 状态变化（实施落地/Phase 完成） | 原 ADR Status Update 段追加 |
| **禁止** | 在 Status Update 中塞入决策性内容（违反 ADR 不可变 + 制造矛盾 ADR） |

### 1.3 状态升级反映现实
- 长期 Proposed 但实际已实施的 ADR-SOC-06/07 → 升 Accepted + Status Update 段

### 1.4 每份 ADR 经 Oracle 评审
- 修订/新建每份 ADR 必须经 Oracle 独立评审 PASS
- 纯状态追加可批量合并 1 次 Oracle 评审

---

## 2. ADR 树状图 v1.0

```
ADR-SOC 系列 (14 份, v1.0)
│
├─── 现有 8 份（v0.5 时代，已签发）
│    ├── ADR-SOC-01 coherence-protocol-strategy         ✅ Accepted + Status Update
│    ├── ADR-SOC-02 cu-granularity                       ✅ Accepted + Status Update (Superseded by 06-D2)
│    ├── ADR-SOC-03 wavefront-coalescing-abstraction     ✅ Accepted + Status Update (Superseded by 06-D2)
│    ├── ADR-SOC-04 hsapp-cp-dispatcher-simplification   ✅ Accepted + Status Update (§4 矛盾标注)
│    ├── ADR-SOC-05 gpu-directory-structure              ✅ Accepted + Status Update (目录扩展)
│    ├── ADR-SOC-06 cpptlm-v05-mvp                       ✅ Accepted + Status Update (状态升级)
│    ├── ADR-SOC-07 dgpu-board-soc-layering              ✅ Accepted + Status Update (状态升级)
│    └── ADR-SOC-08 v55-system-hw-integration-preconditions  📋 Proposed + Status Update (5 测试+4 跨仓)
│
└─── 新增 6 份（v1.0 战略，2027-02-09 建立）
     ├── ADR-SOC-09 v1-nvidia-amd-dual-vendor            📋 Proposed (D1-D4 双 vendor)
     ├── ADR-SOC-10 module-factory-topology              📋 Proposed (D1-D4 拓扑层)
     ├── ADR-SOC-11 pcie-endpoint-ip                     📋 Proposed (D1-D4 17 ports 替代)
     ├── ADR-SOC-12 host-bypass-and-rc                   📋 Proposed (D1-D3 软件 bring-up)
     ├── ADR-SOC-13 axi-stream-adapter-mapper            📋 Proposed (D1-D5 三端口 + OOO)
     └── ADR-SOC-14 v55-integration-revision             📋 Proposed (D1-D4 23 ABI 冻结不变量)
```

---

## 3. 修订决策树（per revision-plan-v1.0.md §3-§5）

### 3.1 A 类：现有 ADR 修订清单（必须）
- **A1** ADR-SOC-01: Status Update 追加（Phase 7.C 部分落地 + 双 vendor）
- **A2** ADR-SOC-02: Status Update 标 Superseded by 06-D2
- **A3** ADR-SOC-03: Status Update 标 Superseded by 06-D2
- **A4** ADR-SOC-06: 状态升级 Proposed → Accepted + 证据清单
- **A5** ADR-SOC-07: 状态升级 Proposed → Accepted + 证据清单
- **A6** ADR-SOC-04: Status Update 标 §4 "永不做" 矛盾
- **A7** ADR-SOC-05: Status Update 标目录扩展

### 3.2 B 类：现有 ADR 状态追加（强烈建议）
- **B** ADR-SOC-08: 5 项前置测试当前状态 + 4 项跨仓提议反馈

### 3.3 C 类：新增 ADR 清单（强烈建议）
- **C1** ADR-SOC-09: v1.0 融合 NVIDIA + AMD 双 vendor
- **C2** ADR-SOC-10: ModuleFactory 拓扑层
- **C3** ADR-SOC-11: PcieEndpointIP 替代 PcieEndpointTLM
- **C4** ADR-SOC-12: Host Bypass + RC
- **C5** ADR-SOC-13: AXI Stream Adapter + AXI4Mapper
- **C6** ADR-SOC-14: v5.5+ 系统级硬件仿真集成修订

---

## 4. ADR ↔ 模块微架构 ↔ 子系统架构映射

| ADR | 对应模块微架构 | 对应子系统架构 |
|-----|----------------|----------------|
| ADR-SOC-01 | coherence-protocol / coherence-domain / coherence-bridge / coherent_xbar / snoop_filter | 09-coherence-protocol |
| ADR-SOC-02 | gpu-compute_unit / wavefront | 05-sm-compute-unit |
| ADR-SOC-03 | gpu-compute_unit / wavefront | 05-sm-compute-unit |
| ADR-SOC-04 | command-processor / submit-queue / doorbell | 02-command-processor + 04-work-distribution |
| ADR-SOC-05 | gpu.* + pcie.* + cluster.* | 全部（目录结构） |
| ADR-SOC-06 | dgpu-soc-pcie-slice / dgpu-board | 01-host-interface + 02-command-processor |
| ADR-SOC-07 | dgpu-board / dgpu-soc-pcie-slice | 01-host-interface |
| ADR-SOC-08 | (跨仓,无仓内模块) | (跨仓) |
| **ADR-SOC-09** | pm4-decoder / command-processor | 02-command-processor + 04-work-distribution |
| **ADR-SOC-10** | (9 类 SimModule 容器) | 全部（拓扑层） |
| **ADR-SOC-11** | dgpu-soc-pcie-slice | 01-host-interface |
| **ADR-SOC-12** | **host-bypass** + **pcie-root-complex** | 01-host-interface |
| **ADR-SOC-13** | **axi4-stream-adapter** + **axi4-mapper** | 01-host-interface |
| **ADR-SOC-14** | (跨仓, 23 ABI 状态) | (跨仓) |

**粗体**: 本 change 新建的 4 份模块微架构文档

---

## 5. v1.1+ 待办（不属本 change）

| 决策 | 触发条件 | 归属 |
|------|----------|------|
| ADR-SOC-09 D5 修订（KFD 路径） | UsrLinuxEmu ADR-088 KFD 反馈 | v1.1 |
| ADR-SOC-14 D5 修订（live migration 节奏） | UsrLinuxEmu ADR-089 v5.5+ 更新 | v1.1 |
| Phase 1-3 PCIe 子链路 3 项 ADR | Link Layer / 130b Encoding / PHY Digital | v1.1 |
| 9 类 SimModule 容器单独 ADR | ADR-SOC-10 实施后拆分 | v1.1 |
| ADR-SOC-04 §4 "永不做" 重新评估 | v1.0 双 vendor 落地后 | v1.1 |
