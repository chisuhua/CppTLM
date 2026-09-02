# Decisions — dGPU SoC v1.0 ADR-SOC 修订规划

> **配套**: [proposal.md](./proposal.md) | **修订规划权威源**: [`docs/soc_arch/adr/revision-plan-v1.0.md`](../../../docs/soc_arch/adr/revision-plan-v1.0.md)

---

## 决策清单

### A 类：现有 ADR 修订（必须，P0-P1）

| ID | ADR | 决策 | 工作量 | 状态 |
|----|-----|------|:---:|:---:|
| **A1** | ADR-SOC-01 | Status Update 追加（Phase 7.C 部分落地 + 双 vendor MOESI/GPU 6x6） | 0.5 hr | ✅ |
| **A2** | ADR-SOC-02 | Status Update 标 Superseded by ADR-SOC-06 D2（CU 黑盒 → 白盒 warp 级） | 0.25 hr | ✅ |
| **A3** | ADR-SOC-03 | Status Update 标 Superseded by ADR-SOC-06 D2（wavefront 抽象失效） | 0.25 hr | ✅ |
| **A4** | ADR-SOC-06 | 状态升级 Proposed → Accepted + 证据清单（6 commits + dgpu_soc_with_pcie_ip.json） | 1.5 hr | ✅ |
| **A5** | ADR-SOC-07 | 状态升级 Proposed → Accepted + 证据清单（3 commits + 全链路 E2E） | 1.5 hr | ✅ |
| **A6** | ADR-SOC-04 | Status Update 标 §4 "永不做" 与 v1.0 双 vendor 矛盾 | 0.5 hr | ✅ |
| **A7** | ADR-SOC-05 | Status Update 标目录扩展（tlm/pcie/ + tlm/cluster/） | 0.5 hr | ✅ |

### B 类：现有 ADR 状态追加（强烈建议，P1）

| ID | ADR | 决策 | 工作量 | 状态 |
|----|-----|------|:---:|:---:|
| **B** | ADR-SOC-08 | 5 项前置测试当前状态 + 4 项 UsrLinuxEmu 跨仓提议反馈 + 23 ABI 仓内 19/19 函数 + 4 回调 typedef 契约 | 1.5 hr | ✅ |

### C 类：新增 ADR（强烈建议，P1）

| ID | ADR | 决策 | 工作量 | 状态 |
|----|-----|------|:---:|:---:|
| **C1** | ADR-SOC-09 | v1.0 融合 NVIDIA Blackwell + AMD CDNA 3/3.5 双 vendor 战略（D1 AMD ComputeUnit 蓝图 / D2 PM4 method packet vs TYPE3 opcode / D3 UsrLinuxEmu 跨仓依赖 / D4 MOESI/GPU 跨 vendor 复用） | 4 hr | ✅ |
| **C2** | ADR-SOC-10 | ModuleFactory 拓扑层（D1 JSON 单一入口 / D2 SimModule 9 类 P2-P5 / D3 PcieEndpointIP 17 ports / D4 AXI4Mapper 可选注入） | 2 hr | ✅ |
| **C3** | ADR-SOC-11 | PcieEndpointIP 替代 PcieEndpointTLM（D1 4 端口 [[deprecated]] / D2 注册保留 / D3 17 ports 新代码统一 / D4 23 ABI 冻结） | 1.5 hr | ✅ |
| **C4** | ADR-SOC-12 | Host Bypass + RC（D1 软件 bring-up 跳过 RC BFM / D2 PF0-only 简化 / D3 M1 修复 429327d 自动转发 4 方向 AXI） | 1.5 hr | ✅ |
| **C5** | ADR-SOC-13 | AXI Stream Adapter + AXI4Mapper（D1 Axi4Bundle 17 字段 / D2 三端口 StreamAdapter / D3 Axi4Mapper 解耦 / D4 512-bit 限制 / D5 PCIe Cfg 地址编码简化） | 1.5 hr | ✅ |
| **C6** | ADR-SOC-14 | v5.5+ 系统级硬件仿真集成修订（D1 前置测试指针 / D2 UsrLinuxEmu 反馈 / D3 23 ABI 冻结不变量 / D4 live migration 推迟） | 1.5 hr | ✅ |

### 索引同步（必须）

| ID | 决策 | 工作量 | 状态 |
|----|------|:---:|:---:|
| **I1** | ADR README 同步 v2.0（状态图例 + 14 份 ADR 清单 + 修订规划） | 0.5 hr | ✅ |

### 模块微架构（必须，对应 C4/C5）

| ID | 文件 | 工作量 | 状态 |
|----|------|:---:|:---:|
| **M1** | docs/soc_arch/modules/host-bypass.md | 0.5 hr | ✅ |
| **M2** | docs/soc_arch/modules/pcie-root-complex.md | 0.5 hr | ✅ |
| **M3** | docs/soc_arch/modules/axi4-stream-adapter.md | 0.5 hr | ✅ |
| **M4** | docs/soc_arch/modules/axi4-mapper.md | 0.5 hr | ✅ |

---

## 决策统计

| 类型 | 数量 | 总工作量 |
|------|:---:|:---:|
| A 类（现有 ADR 修订） | 7 | 5.0 hr |
| B 类（现有 ADR 状态追加） | 1 | 1.5 hr |
| C 类（新增 ADR） | 6 | 12.0 hr |
| 索引同步 | 1 | 0.5 hr |
| 模块微架构（4 份） | 4 | 2.0 hr |
| **合计** | **19** | **21.0 hr** |

实际落地时间（含 Oracle 评审、修复、Batch 拆分提交）: **约 6 小时**（2027-02-09 单日完成）

---

## 跨决策引用

- A1 引用 ADR-SOC-09 D4（双 vendor Coherence 共享 MOESI/GPU）
- A2/A3 引用 ADR-SOC-06 D2（CU 白盒 warp 级 + WavefrontState PC/cycle 观测）
- A4 引用 ADR-SOC-11（17 ports 替代）/ ADR-SOC-09（双 vendor）/ ADR-SOC-10（拓扑层）
- A5 引用 ADR-SOC-11（替代）/ ADR-SOC-14（UsrLinuxEmu 集成）
- A6 引用 ADR-SOC-09 D3（KFD 跨仓依赖）
- C1-C6 互引：双 vendor / 拓扑层 / 替代 / Host Bypass / AXI / v5.5+ 集成形成 v1.0 决策网
