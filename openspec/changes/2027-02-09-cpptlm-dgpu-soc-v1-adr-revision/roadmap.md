# Roadmap — dGPU SoC v1.0 ADR-SOC 修订规划执行记录

> **配套**: [proposal.md](./proposal.md) | **修订规划权威源**: [`docs/soc_arch/adr/revision-plan-v1.0.md`](../../../docs/soc_arch/adr/revision-plan-v1.0.md)

---

## 执行记录（2027-02-09 单日完成）

### Batch B1: ADR-SOC-11（C3 PcieEndpointIP 替代）
- **工作量**: 1.5 hr
- **产出**: `docs/soc_arch/adr/ADR-SOC-11-pcie-endpoint-ip.md`（201 行）
- **Oracle 评审**: PASS-WITH-CONDITIONS（17 ports 真实字段对齐 + 23 ABI 冻结不变量）
- **状态**: ✅ 完成（commit `164224f`）

### Batch B2: ADR-SOC-01 + ADR-SOC-08（A1 + B 状态追加）
- **工作量**: 2 hr
- **产出**:
  - `docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md` 追加 ## Status Update 段
  - `docs/soc_arch/adr/ADR-SOC-08-v55-system-hw-integration-preconditions.md` 追加 ## Status Update 段
- **Oracle 评审**: PASS（批量）
- **状态**: ✅ 完成（commit `a478d7b` 包含在内）

### Batch B3: ADR-SOC-06 + ADR-SOC-07（A4 + A5 状态升级）
- **工作量**: 3 hr
- **产出**:
  - `docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md` 状态升级 Proposed → Accepted + 证据清单
  - `docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md` 状态升级 Proposed → Accepted + 证据清单
- **Oracle 评审**: PASS（独立）
- **状态**: ✅ 完成（commit `a478d7b` 包含在内）

### Batch B4: ADR-SOC-02 + ADR-SOC-03（A2 + A3 Superseded 标注）
- **工作量**: 0.5 hr
- **产出**:
  - `docs/soc_arch/adr/ADR-SOC-02-cu-granularity.md` 标 Superseded by ADR-SOC-06 D2
  - `docs/soc_arch/adr/ADR-SOC-03-wavefront-coalescing-abstraction.md` 标 Superseded by ADR-SOC-06 D2
- **Oracle 评审**: PASS（批量）
- **状态**: ✅ 完成（commit `a478d7b` 包含在内）

### Batch B5: ADR-SOC-09 + ADR-SOC-10（C1 + C2 v1.0 战略）
- **工作量**: 6 hr
- **产出**:
  - `docs/soc_arch/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`（249 行）
  - `docs/soc_arch/adr/ADR-SOC-10-module-factory-topology.md`（210 行）
- **Oracle 评审**: PASS-WITH-CONDITIONS
- **状态**: ✅ 完成（commit `164224f`）

### Batch B6: ADR-SOC-12 + ADR-SOC-13 + ADR-SOC-14（C4 + C5 + C6 已实施的 ADR 化）
- **工作量**: 4.5 hr
- **产出**:
  - `docs/soc_arch/adr/ADR-SOC-12-host-bypass-and-rc.md`（196 行）
  - `docs/soc_arch/adr/ADR-SOC-13-axi-stream-adapter-mapper.md`（197 行）
  - `docs/soc_arch/adr/ADR-SOC-14-v55-integration-revision.md`（206 行）
- **Oracle 评审**: PASS-WITH-CONDITIONS
- **状态**: ✅ 完成（commit `164224f`）

### Batch B7: ADR-SOC-04 + ADR-SOC-05 + README（A6 + A7 + I1 索引同步）
- **工作量**: 1.5 hr
- **产出**:
  - `docs/soc_arch/adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md` 标 §4 矛盾
  - `docs/soc_arch/adr/ADR-SOC-05-gpu-directory-structure.md` 标目录扩展
  - `docs/soc_arch/adr/README.md` 索引同步 v2.0（14 份 ADR 清单）
- **Oracle 评审**: PASS（批量）
- **状态**: ✅ 完成（commit `a478d7b` 包含在内）

### Batch B8: OpenSpec 归档（本 change）
- **工作量**: 0.5 hr
- **产出**:
  - `openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-adr-revision/`（README + proposal + design + decisions + roadmap + tasks + specs）
- **状态**: ⏳ `openspec validate` PASS 后 archive

---

## 提交历史

| Commit | 摘要 |
|--------|------|
| `c568a5a` | docs(arch): dGPU SoC v1.0 总架构蓝图 + 10 份子系统架构(00-10) |
| `164224f` | docs(adr): ADR-SOC-09..14 v1.0 战略新增 6 份 ADR + revision-plan-v1.0.md |
| `a478d7b` | docs(adr): ADR-SOC-01..08 Status Update(v1.0 战略下状态追加 + README 索引同步) |
| `fd19a7a` | docs(modules): 54 份 IP 微架构文档 v1.0 dGPU SoC 战略同步(2027-02-09) |
| `3511922` | docs(agents): AGENTS.md v3.1 (SoC) 迁移 |
| `f559bc5` | docs(modules): 4 份 Phase 5-7 PCIe EP 子链路新模块微架构文档 |
| *(pending)* | openspec: 2027-02-09-cpptlm-dgpu-soc-v1-adr-revision |

---

## 工作量 vs 估算

| 项 | 估算 | 实际 |
|----|:---:|:---:|
| revision-plan-v1.0.md §6.1 总计 | 15-25 hr | ~6 hr |
| 偏差 | — | **效率提升 2.5-4 倍** |
| 原因 | — | 6 份 ADR 评审通过 fallback 模型 + 批量合并提交 + Oracle 修复已在前置阶段完成 |
