# cpptlm-dgpu-soc-v1-architecture: dGPU SoC v1.0 总架构蓝图 (NVIDIA Blackwell + AMD CDNA 3 融合)

> **状态**: Proposed — 2027-02-09
> **父 change**: `2026-09-01-cpptlm-dgpu-pcie-ip-microarch`（PCIe IP 微架构 umbrella）+ `2026-06-24-gpu-soc-phase8a-infra`（GPU SoC Phase 8.A）
> **前置**: Phase 8 PCIe EP 整合完成（`2027-02-09-cpptlm-dgpu-pcie-ip-integration` HEAD 429327d）+ v0.5 MVP 完成（`2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3` archive）
> **关联 OpenSpec**（已存在,作为子文档归口）：
>   - PCIe IP 子链路：`2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc` / `2026-09-29-cpptlm-dgpu-pcie-130b-encoding` / `2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl` / `2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool` / `2026-11-03-cpptlm-dgpu-axi-stream-adapter` / `2026-12-22-cpptlm-dgpu-axi4-mapper` / `2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc` / `2027-02-09-cpptlm-dgpu-pcie-ip-integration`

## Why

CppTLM 在 2026-06..2027-02 完成 v0.5 MVP + Phase 1-7 PCIe EP 7 阶段 + Phase 8 整合交付,
从单 driver stack + 单 PCIe EP + 黑盒 CU 升级到 **dGPU SoC v1.0** 战略形态:同时支持
CUDA + ROCm 双 driver stack、17 端口 PcieEndpointIP、HBM3e + L2 + DSMEM/TMEM 多层
memory hierarchy、wgmma/tcgen05 + MFMA 双 vendor 张量核心、完整 MOESI/GPU coherence。

总架构蓝图 (`docs/soc_arch/architecture/00-overview.md`) 是 v1.0 战略的**导航文档**,为
后续 10 份子系统架构 (01-10)、ADR-SOC 修订规划、模块微架构修订提供战略参考。

## What Changes

| 文件 | 用途 |
|---|---|
| `docs/soc_arch/architecture/00-overview.md` | **新**: v1.0 总架构蓝图（7 层架构 + D1-D8 + 4 数据流视图 + 兼容性 + 风险） |
| `docs/soc_arch/architecture/01-host-interface.md` | **规划**: PcieEndpointIP + HostBypassTLM + PcieRootComplexTLM（待 Oracle 评审） |
| `docs/soc_arch/architecture/02-command-processor.md` | **规划**: CommandProcessor + Pm4Decoder 双 vendor（待 Oracle 评审） |
| `docs/soc_arch/architecture/03-task-management-unit.md` | **规划**: TMU + TMD + 依赖预取 + PDL（待 Oracle 评审） |
| `docs/soc_arch/architecture/04-work-distribution.md` | **规划**: WDU + Crossbar DDS + CGA Cluster + AMD SPI/SQ（待 Oracle 评审） |
| `docs/soc_arch/architecture/05-sm-compute-unit.md` | **规划**: CU/SM + Warp调度 + 寄存器文件 + DSMEM/TMEM（待 Oracle 评审） |
| `docs/soc_arch/architecture/06-tensor-core.md` | **规划**: wgmma + tcgen05 + 5th Tensor Core + MFMA（待 Oracle 评审） |
| `docs/soc_arch/architecture/07-memory-system.md` | **规划**: HBM3e + L1/L2 + Shared Memory + DSMEM + TMEM（待 Oracle 评审） |
| `docs/soc_arch/architecture/08-noc-interconnect.md` | **规划**: Mesh NoC + Router + NIC + 跨 cluster 通信（待 Oracle 评审） |
| `docs/soc_arch/architecture/09-coherence-protocol.md` | **规划**: MOESI/GPU + CoherenceDomain + Bridge + SnoopFilter（待 Oracle 评审） |
| `docs/soc_arch/architecture/10-completion-notify.md` | **规划**: Doorbell + CompletionRing + MSI-X（待 Oracle 评审） |

**注**: 当前阶段 (2027-02-09) 仅 `00-overview.md` 已写并 Oracle 评审 FAIL 待修复;
01-10 子架构文档**规划中**,需在后续 OpenSpec task 中逐份实施 + Oracle 评审。

## Scope

**In Scope**:
- 7 层架构 (L1..L7, 统一编号) 导航与决策矩阵
- v1.0 MVP / v1.1 完整版范围矩阵 (单一权威表)
- D1..D8 关键设计决策与备选方案
- 4 数据流视图 (命令流 / 数据流 / 配置流 / 中断流)
- 与 v0.5 / Phase 8 兼容性分析 + 扩展点
- 8 份 ADR-SOC 引用矩阵 + 25+ 模块微架构引用矩阵
- R1..R7 风险与缓解
- 反模式 (明确不做)

**Out of Scope**:
- 具体子系统内部实现细节（由 01-10 子架构文档负责）
- ADR-SOC 01-08 修订/新建（由后续 OpenSpec change `cpptlm-dgpu-soc-v1-adr-revision` 负责,待 Metis 审查后启动）
- 模块微架构修订（由后续 OpenSpec change `cpptlm-dgpu-soc-v1-modules-update` 负责）

## Acceptance Gate

- `openspec validate --strict` PASS（当前 proposal.md 满足最小 schema）
- 00-overview.md 通过 Oracle 评审 PASS（含 P0/P1 修复后复审）
- 01-10 子架构文档**各自**通过 Oracle 评审（每份独立评审）
- ADR-SOC 修订规划通过 Metis 评审,后续修订/新建由独立 OpenSpec change 实施并 Oracle 评审
- 模块微架构修订由独立 OpenSpec change 实施并 Oracle 评审
- 与 Phase 8 整合交付（`2027-02-09-cpptlm-dgpu-pcie-ip-integration`）无冲突

## Risks

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| R1 | 双 vendor 内核实现工作量过大 | 🟡中 | 蓝图模式共享 CU,运行时 config 切换 |
| R2 | AMD ROCm 生态成熟度不足 | 🟡中 | NVIDIA 路径优先,AMD 备选 |
| R3 | Tensor Core (tcgen05 + TMEM) 实施复杂度高 | 🟡中 | v1.0 MVP 仅 wgmma,v1.1 完整版追加 |
| R4 | DSMEM/TMEM 集群通信延迟建模不准 | 🟡中 | v1.0 推迟,先用 L2 共享近似 |
| R5 | 23 ABI 头冻结但实现未交付（planned contract） | 🟡中 | R7 已在 00-overview.md 中如实反映 |
| R6 | 跨层编号与范围矩阵的内部一致性 | 🔴高 | Oracle 评审已暴露 P0/P1,本 change 后续任务逐步修复 |
| R7 | 10 份子架构文档工作量（每份 800-1000 行） | 🟡中 | 分批实施,每份独立 Oracle 评审 |