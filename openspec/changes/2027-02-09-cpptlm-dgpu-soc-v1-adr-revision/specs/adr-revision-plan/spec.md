# adr-revision-plan — dGPU SoC v1.0 ADR-SOC 修订规划

> **配套**: [proposal.md](../proposal.md)

## Purpose

定义 dGPU SoC v1.0 战略下 ADR-SOC 系列的修订规划，包括：
- 6 份新 ADR（ADR-SOC-09..14）的强制需求
- 8 份现有 ADR 的 Status Update 强制需求
- ADR README 索引同步
- 4 份新模块微架构文档（对应 ADR-SOC-12/13）

---

## ADDED Requirements

### Requirement: 新建 ADR-SOC-09 v1-nvidia-amd-dual-vendor

`docs/soc_arch/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md` 必须存在且包含：
- 主题：v1.0 战略 = 同时支持 CUDA + ROCm 双 driver stack
- 决策点 D1-D4：
  - D1 AMD 路径与 NVIDIA 路径共享 ComputeUnitTLM 蓝图（cu_template + nv_mode/amd_mode）
  - D2 两类 driver stack 的 ABI 隔离（NVIDIA PM4 method packet vs AMD PM4 TYPE3 opcode）
  - D3 UsrLinuxEmu IOCTL 分发（NVIDIA 0x27/0x29/0x01 已实施 + 0x28 永久 -ENOSYS + AMD KFD 需跨仓承诺）
  - D4 Coherence 兼容性（NVIDIA 不需 CPU↔GPU coherence；AMD Infinity Fabric 需要）
- 证据附录：所有数字断言必须附 `文件:行号` + grep 证据

#### Scenario: ADR-SOC-09 通过 Oracle 评审

- **WHEN** Oracle 评审 ADR-SOC-09
- **THEN** 评审结果必须为 PASS 或 PASS-WITH-CONDITIONS（所有 P0 修复已应用）

### Requirement: 新建 ADR-SOC-10 module-factory-topology

`docs/soc_arch/adr/ADR-SOC-10-module-factory-topology.md` 必须存在且包含：
- 主题：9 类 SimModule P2-P5 层级容器 + ApuSoC 顶层
- 决策点 D1-D4：
  - D1 单一入口 JSON 拓扑
  - D2 SimModule 多层容器（9 类 P2-P5）
  - D3 PcieEndpointIP 作为 17 ports ChStreamModuleBase
  - D4 JSON `axi4_mapper_inject: true` 可选注入

#### Scenario: ADR-SOC-10 通过 Oracle 评审

- **WHEN** Oracle 评审 ADR-SOC-10
- **THEN** 评审结果必须为 PASS 或 PASS-WITH-CONDITIONS

### Requirement: 新建 ADR-SOC-11 pcie-endpoint-ip

`docs/soc_arch/adr/ADR-SOC-11-pcie-endpoint-ip.md` 必须存在且包含：
- 主题：17 ports PcieEndpointIP 整合模块替代 4 端口 PcieEndpointTLM
- 决策点 D1-D4：
  - D1 PcieEndpointTLM 加 `[[deprecated("use PcieEndpointIP")]]`（已实施,429327d）
  - D2 chstream_register.hh 保留 PcieEndpointTLM 注册（既有测试依赖）
  - D3 新代码统一使用 PcieEndpointIP
  - D4 23 ABI 冻结（include/tlm/gpu/pcie_endpoint_tlm.h layout 不变）

#### Scenario: ADR-SOC-11 通过 Oracle 评审

- **WHEN** Oracle 评审 ADR-SOC-11
- **THEN** 评审结果必须为 PASS 或 PASS-WITH-CONDITIONS（17 ports 真实字段对齐 + 23 ABI 冻结不变量）

### Requirement: 新建 ADR-SOC-12 host-bypass-and-rc

`docs/soc_arch/adr/ADR-SOC-12-host-bypass-and-rc.md` 必须存在且包含：
- 主题：Phase 7 HostBypassTLM + PcieRootComplexTLM（PF0-only 简化）
- 决策点 D1-D3：
  - D1 软件 bring-up 跳过 PCIe RC BFM
  - D2 PcieRootComplexTLM 枚举 PF0-only（Oracle M2 标注）
  - D3 M1 修复 429327d 自动转发 4 方向 AXI 通道

#### Scenario: ADR-SOC-12 通过 Oracle 评审

- **WHEN** Oracle 评审 ADR-SOC-12
- **THEN** 评审结果必须为 PASS 或 PASS-WITH-CONDITIONS

### Requirement: 新建 ADR-SOC-13 axi-stream-adapter-mapper

`docs/soc_arch/adr/ADR-SOC-13-axi-stream-adapter-mapper.md` 必须存在且包含：
- 主题：Phase 5/6 Axi4Bundle + Axi4StreamAdapter + Axi4Mapper
- 决策点 D1-D5：
  - D1 Axi4Bundle 17 字段 + Axi4LiteBundle 9 字段
  - D2 Axi4StreamAdapter 三端口（master_out / slave_in / cfg_slave_in）
  - D3 Axi4Mapper 独立模块（与 PcieEndpointIP 解耦）
  - D4 512-bit 数据宽度已知限制（ch_uint<512> 实为 64-bit 存储）
  - D5 PCIe Cfg 地址编码简化（Phase 8 M1 用 awaddr 当 offset）

#### Scenario: ADR-SOC-13 通过 Oracle 评审

- **WHEN** Oracle 评审 ADR-SOC-13
- **THEN** 评审结果必须为 PASS 或 PASS-WITH-CONDITIONS

### Requirement: 新建 ADR-SOC-14 v55-integration-revision

`docs/soc_arch/adr/ADR-SOC-14-v55-integration-revision.md` 必须存在且包含：
- 主题：Phase 8 完成后 v5.5+ 集成修订
- 决策点 D1-D4：
  - D1 5 项前置测试当前实施状态（详见 ADR-SOC-08 状态追加）
  - D2 4 项 UsrLinuxEmu 跨仓提议反馈状态（待回应）
  - D3 23 ABI 冻结不变量（仓内 19/19 函数 + 4 回调 typedef 契约）
  - D4 live migration 集成点推迟时间表

#### Scenario: ADR-SOC-14 通过 Oracle 评审

- **WHEN** Oracle 评审 ADR-SOC-14
- **THEN** 评审结果必须为 PASS 或 PASS-WITH-CONDITIONS（19/19 函数 + 4 回调 typedef 契约）

### Requirement: 现有 ADR Status Update 段追加

8 份现有 ADR（ADR-SOC-01..08）必须包含 `## Status Update` 段：
- ADR-SOC-01: Phase 7.C 部分落地 + 双 vendor
- ADR-SOC-02: Superseded by ADR-SOC-06 D2
- ADR-SOC-03: Superseded by ADR-SOC-06 D2
- ADR-SOC-04: §4 矛盾标注
- ADR-SOC-05: 目录扩展（tlm/pcie/ + tlm/cluster/）
- ADR-SOC-06: 状态升级 Accepted + 证据清单
- ADR-SOC-07: 状态升级 Accepted + 证据清单
- ADR-SOC-08: 5 测试 + 4 跨仓 + 23 ABI 状态

**正文必须不变**（尊重 ADR 不可变原则）

#### Scenario: 8 份现有 ADR 均含 Status Update 段

- **WHEN** `grep -l "## Status Update" docs/soc_arch/adr/ADR-SOC-{01..08}*.md | wc -l`
- **THEN** 输出必须等于 8

### Requirement: ADR README 索引同步

`docs/soc_arch/adr/README.md` 必须包含：
- 状态图例（Proposed / Accepted / Superseded）
- 14 份 ADR 完整清单（ADR-SOC-01..14）
- 修订规划引用（revision-plan-v1.0.md）

#### Scenario: README 含 14 份 ADR 清单

- **WHEN** 阅读 `docs/soc_arch/adr/README.md`
- **THEN** 应能找到 ADR-SOC-01 至 ADR-SOC-14 全部 14 份条目

### Requirement: 修订规划权威源

`docs/soc_arch/adr/revision-plan-v1.0.md` 必须存在且：
- 包含 §1-§9 完整结构（背景/策略/修订清单/状态追加/新增/执行计划/风险/ADR 树/映射）
- 通过 Metis 评审（X1-X9 + M1-M2 修复已落地）
- 引用 `openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-adr-revision/`（本 change）

#### Scenario: revision-plan-v1.0.md 通过 Metis 评审

- **WHEN** Metis 评审 revision-plan-v1.0.md
- **THEN** 评审结果必须为 PASS，所有 P0 问题已修复

### Requirement: 4 份新模块微架构文档

4 份新模块微架构文档必须存在（对应 ADR-SOC-12/13）：
- `docs/soc_arch/modules/host-bypass.md`（HostBypassTLM 微架构）
- `docs/soc_arch/modules/pcie-root-complex.md`（PcieRootComplexTLM 微架构）
- `docs/soc_arch/modules/axi4-stream-adapter.md`（Axi4StreamAdapter 微架构）
- `docs/soc_arch/modules/axi4-mapper.md`（Axi4Mapper 微架构）

每份必须包含：
- 状态字段 v1.0 Implemented
- 关联 ADR（ADR-SOC-11/12/13）
- 关联子系统架构（01-host-interface.md）
- 测试覆盖
- 已知限制（512-bit 数据宽度 + PCIe Cfg 地址编码简化）

#### Scenario: 4 份新模块文档全部产出

- **WHEN** `ls docs/soc_arch/modules/{host-bypass,pcie-root-complex,axi4-stream-adapter,axi4-mapper}.md | wc -l`
- **THEN** 输出必须等于 4

---

## MODIFIED Requirements

（无）

---

## REMOVED Requirements

（无）

---

## Cross-References

- **父 change**: `2027-02-09-cpptlm-dgpu-soc-v1-architecture/`（v1.0 总架构蓝图）
- **执行规划**: [`docs/soc_arch/adr/revision-plan-v1.0.md`](../../../docs/soc_arch/adr/revision-plan-v1.0.md)
- **关联 ADR**: ADR-SOC-01..14 全部
- **关联模块**: docs/soc_arch/modules/ 54 份 IP 微架构 + 4 份本 change 新增
- **关联架构**: docs/soc_arch/architecture/ 11 份（00-overview + 10 子系统）
