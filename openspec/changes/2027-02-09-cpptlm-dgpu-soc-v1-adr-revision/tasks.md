# Tasks — dGPU SoC v1.0 ADR-SOC 修订规划

> **配套**: [proposal.md](./proposal.md)

---

## 任务清单（22 文件）

### T1: 6 份新 ADR 文档

- [x] T1.1 ADR-SOC-09 v1-nvidia-amd-dual-vendor.md（249 行,Oracle PASS-WITH-CONDITIONS）
- [x] T1.2 ADR-SOC-10 module-factory-topology.md（210 行,Oracle PASS-WITH-CONDITIONS）
- [x] T1.3 ADR-SOC-11 pcie-endpoint-ip.md（201 行,Oracle PASS-WITH-CONDITIONS）
- [x] T1.4 ADR-SOC-12 host-bypass-and-rc.md（196 行,Oracle PASS-WITH-CONDITIONS）
- [x] T1.5 ADR-SOC-13 axi-stream-adapter-mapper.md（197 行,Oracle PASS-WITH-CONDITIONS）
- [x] T1.6 ADR-SOC-14 v55-integration-revision.md（206 行,Oracle PASS-WITH-CONDITIONS）

### T2: 8 份现有 ADR Status Update 段追加

- [x] T2.1 ADR-SOC-01 coherence-protocol-strategy（Phase 7.C + 双 vendor）
- [x] T2.2 ADR-SOC-02 cu-granularity（Superseded by 06-D2）
- [x] T2.3 ADR-SOC-03 wavefront-coalescing-abstraction（Superseded by 06-D2）
- [x] T2.4 ADR-SOC-04 hsapp-cp-dispatcher-simplification（§4 矛盾标注）
- [x] T2.5 ADR-SOC-05 gpu-directory-structure（目录扩展）
- [x] T2.6 ADR-SOC-06 cpptlm-v05-mvp（状态升级 Accepted + 证据清单）
- [x] T2.7 ADR-SOC-07 dgpu-board-soc-layering（状态升级 Accepted + 证据清单）
- [x] T2.8 ADR-SOC-08 v55-system-hw-integration-preconditions（5 测试 + 4 跨仓）

### T3: 索引同步

- [x] T3.1 ADR README 同步 v2.0（14 份 ADR 清单 + 修订规划）

### T4: 修订规划权威源

- [x] T4.1 revision-plan-v1.0.md（477 行,Metis PASS）

### T5: 4 份新模块微架构文档（ADR-SOC-12/13 对应）

- [x] T5.1 host-bypass.md（HostBypassTLM 微架构,103 行）
- [x] T5.2 pcie-root-complex.md（PcieRootComplexTLM 微架构,109 行）
- [x] T5.3 axi4-stream-adapter.md（Axi4StreamAdapter 微架构,121 行）
- [x] T5.4 axi4-mapper.md（Axi4Mapper 微架构,110 行）

### T6: OpenSpec change 归档（本 change）

- [x] T6.1 proposal.md 产出
- [ ] T6.2 `openspec validate 2027-02-09-cpptlm-dgpu-soc-v1-adr-revision` PASS
- [ ] T6.3 archive change + 同步 main specs
- [x] T6.4 README.md + design.md + decisions.md + roadmap.md + tasks.md + spec.md 产出

---

## 验证清单

- [x] `git log --oneline | grep "ADR-SOC"` 至少 5 条 commit 引用
- [x] `git show --stat 164224f` 含 7 文件（6 ADR + revision-plan）
- [x] `git show --stat a478d7b` 含 9 文件（8 ADR Status Update + README）
- [x] `git show --stat f559bc5` 含 4 文件（4 新模块文档）
- [x] `ls openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-adr-revision/` 含 6 文件
- [ ] `openspec validate 2027-02-09-cpptlm-dgpu-soc-v1-adr-revision` PASS
- [ ] `grep -RIn "18/23 函数" docs/soc_arch/` 无残留（23 ABI 状态全仓统一）

---

## 状态

**22 任务已完成 21（95.5%）**，待：
1. `openspec validate` PASS
2. archive 操作

均不阻塞代码或文档使用。
