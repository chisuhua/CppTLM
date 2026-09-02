# cpptlm-dgpu-soc-v1-adr-revision: dGPU SoC v1.0 ADR-SOC 修订规划归档(基于 revision-plan-v1.0.md)

> **状态**: 📋 Proposed — 2027-02-09
> **父 change**: `2027-02-09-cpptlm-dgpu-soc-v1-architecture/`（v1.0 总架构蓝图,已建立）
> **执行规划**: [`docs/soc_arch/adr/revision-plan-v1.0.md`](../../../docs/soc_arch/adr/revision-plan-v1.0.md)（核心产出, Metis 评审 PASS,477 行）
> **关联**: ADR-SOC-01..08（既有,修订/状态追加）+ ADR-SOC-09..14（新建,v1.0 战略）

---

## Why

CppTLM dGPU SoC v1.0 战略（per [`2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../2027-02-09-cpptlm-dgpu-soc-v1-architecture/)）下，ADR-SOC 系列面临以下差距(per `revision-plan-v1.0.md` §1.2)：

| 差距 | 级别 | 摘要 |
|------|:---:|------|
| **G1** | P0 | ADR-SOC-02/03 与 ADR-SOC-06 D2 直接矛盾（CU 黑盒 vs 白盒 warp 级） |
| **G2** | P0 | 8 个 ADR 中 3 个仍 Proposed（ADR-SOC-06/07/08），长期未升 Accepted |
| **G3** | P1 | ADR-SOC-06/07 写于 4 端口 PcieEndpointTLM 时代，未反映 Phase 4+ 17 端口 PcieEndpointIP |
| **G4** | P1 | 无 ADR 反映 v1.0 融合 NVIDIA + AMD 双 driver stack 战略 |
| **G5** | P1 | Phase 1-3 PCIe 子链路 3 项决策点无对应 ADR |
| **G6** | P1 | ADR-SOC-04 §4 "永不做 ROCm/AQL/Doorbell" 与 v1.0 双 vendor 直接矛盾 |
| **G7** | P1 | ADR-SOC-05 目录结构过时（仅 gpu/，缺 pcie/ cluster/） |
| **G8** | P2 | 9 类 SimModule 容器无对应 ADR |
| **G9** | P2 | ADR-SOC-08 5 项前置测试当前状态未跟踪 |
| **G10** | P2 | 跨仓承诺（UsrLinuxEmu ADR-088 NVIDIA-only）与 v1.0 AMD KFD 路径依赖未声明 |

本 change 是 `revision-plan-v1.0.md`（Metis 评审通过的执行规划）的 **归档载体**，包含已落地的 6 份新建 ADR（ADR-SOC-09..14）+ 8 份现有 ADR Status Update + ADR-SOC-09..14 对应新模块微架构文档。

---

## What Changes

### 已落地（2027-02-09 HEAD，本 change 归档）

| 文件 | 变更类型 | 摘要 |
|------|----------|------|
| `docs/soc_arch/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md` | **新增** | v1.0 融合 NVIDIA Blackwell + AMD CDNA 3/3.5 双 vendor 战略（D1-D4） |
| `docs/soc_arch/adr/ADR-SOC-10-module-factory-topology.md` | **新增** | ModuleFactory 拓扑层 + 9 类 SimModule P2-P5 + ApuSoC 顶层（D1-D4） |
| `docs/soc_arch/adr/ADR-SOC-11-pcie-endpoint-ip.md` | **新增** | PcieEndpointIP 替代 PcieEndpointTLM（D1-D4） |
| `docs/soc_arch/adr/ADR-SOC-12-host-bypass-and-rc.md` | **新增** | HostBypassTLM + PcieRootComplexTLM 软件 bring-up 路径（D1-D3） |
| `docs/soc_arch/adr/ADR-SOC-13-axi-stream-adapter-mapper.md` | **新增** | Axi4StreamAdapter 三端口 + Axi4Mapper 独立模块（D1-D5） |
| `docs/soc_arch/adr/ADR-SOC-14-v55-integration-revision.md` | **新增** | v5.5+ 系统级硬件仿真集成修订（D1-D4） |
| `docs/soc_arch/adr/ADR-SOC-01..08` | **修订** | Status Update 段追加（v1.0 战略下状态升级/Superseded/矛盾标注） |
| `docs/soc_arch/adr/README.md` | **修订** | 索引同步 v2.0（状态图例 + 14 份 ADR 清单 + 修订规划） |
| `docs/soc_arch/adr/revision-plan-v1.0.md` | **新增** | 修订规划权威源（Metis PASS,Batch B1-B8 执行计划） |
| `docs/soc_arch/modules/host-bypass.md` | **新增** | HostBypassTLM 微架构（ADR-SOC-12 对应） |
| `docs/soc_arch/modules/pcie-root-complex.md` | **新增** | PcieRootComplexTLM 微架构（ADR-SOC-12 对应） |
| `docs/soc_arch/modules/axi4-stream-adapter.md` | **新增** | Axi4StreamAdapter 微架构（ADR-SOC-13 对应） |
| `docs/soc_arch/modules/axi4-mapper.md` | **新增** | Axi4Mapper 微架构（ADR-SOC-13 对应） |

### v1.1+ 待办（不属本 change 范围）

- ADR-SOC-09 D5 修订（基于 UsrLinuxEmu ADR-088 KFD 反馈）
- ADR-SOC-14 D5 修订（live migration 节奏定义）
- Phase 1-3 PCIe 子链路 Link Layer / 130b Encoding / PHY Digital 3 项 ADR（G5 差距）
- 9 类 SimModule 容器单独 ADR（G8 差距）
- ADR-SOC-04 §4 "永不做" 列表重新评估（v1.1 决策性变更）

---

## Scope

**In Scope（已落地）**:
- 6 份新 ADR（ADR-SOC-09..14）覆盖 v1.0 战略的关键决策
- 8 份现有 ADR 的 Status Update（仅追加段,正文不变,尊重 ADR 不可变原则）
- ADR README 索引同步
- ADR-SOC-12/13 对应 4 份新模块微架构文档（ADR-SOC-14 的 23 ABI 模块已存在,无需新增）
- 12 份新 ADR 文档 + 5 份修订 ADR + 1 份修订规划 + 4 份新模块文档 = 22 文件

**Out of Scope**:
- 实施代码变更（不属本 change,代码已在 Phase 1-8 + 各子 change 中落地）
- 现有模块微架构文档同步（已在 `2027-02-09-cpptlm-dgpu-soc-v1-architecture/` 父 change 中批量提交）
- ADR-SOC-09..14 中标注的 v1.1+ 待办项

---

## Oracle 评审记录

| ADR | 评审状态 | 关键修复 |
|------|:---:|------|
| ADR-SOC-09 | ✅ PASS-WITH-CONDITIONS | AMD 路径与 00-overview §4-bis R06/R09/R17 一致性 + 证据附录 + 23 ABI 状态对齐 |
| ADR-SOC-10 | ✅ PASS-WITH-CONDITIONS | 9 类 SimModule 引用对齐 + JSON 注入开关 |
| ADR-SOC-11 | ✅ PASS-WITH-CONDITIONS | 17 ports 真实字段对齐 + 23 ABI 冻结不变量 |
| ADR-SOC-12 | ✅ PASS-WITH-CONDITIONS | Phase 8 M1 修复 429327d + PF0-only 简化标注 |
| ADR-SOC-13 | ✅ PASS-WITH-CONDITIONS | 512-bit 数据宽度限制 + PCIe Cfg 地址编码简化 |
| ADR-SOC-14 | ✅ PASS-WITH-CONDITIONS | 19/19 函数 + 4 回调 typedef 契约 |
| ADR-SOC-01..08 | ✅ PASS（批量） | Status Update 段追加,正文不变 |

---

## Acceptance Gate

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **AR-G1** 6 份新 ADR 全部产出 | Sisyphus | ✅ | `ls docs/soc_arch/adr/ADR-SOC-{09..14}*.md \| wc -l` = 6 |
| **AR-G2** 8 份现有 ADR 全部追加 Status Update 段 | Sisyphus | ✅ | `grep -l "## Status Update" docs/soc_arch/adr/ADR-SOC-{01..08}*.md \| wc -l` = 8 |
| **AR-G3** README 索引同步 v2.0 | Sisyphus | ✅ | `docs/soc_arch/adr/README.md` 含 14 份 ADR 清单 |
| **AR-G4** revision-plan-v1.0.md 已 Metis PASS | Sisyphus | ✅ | 文件存在 + 477 行 + X1-X9/M1-M2 修复已落地 |
| **AR-G5** 4 份新模块微架构文档产出（ADR-SOC-12/13 对应） | Sisyphus | ✅ | `ls docs/soc_arch/modules/{host-bypass,pcie-root-complex,axi4-stream-adapter,axi4-mapper}.md \| wc -l` = 4 |
| **AR-G6** Oracle 评审全过（PASS-WITH-CONDITIONS） | Sisyphus | ✅ | 6 份新 ADR 评审记录 |
| **AR-G7** `openspec validate` PASS | Sisyphus | ⏳ | `openspec validate 2027-02-09-cpptlm-dgpu-soc-v1-adr-revision` |
| **AR-G8** 23 ABI 状态全仓统一（19/19 + 4 回调 typedef） | Sisyphus | ✅ | `grep -RIn "19/19 函数"` 无残留 "18/23" |

**最终验收**(本 change archive 时):
- [x] AR-G1 ~ AR-G6, AR-G8 ✅
- [ ] AR-G7 `openspec validate` PASS(待执行)
- [ ] archive 后 specs/main 同步（本 change 仅归档,不更新 main specs）

---

## 文件清单

```
openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-adr-revision/
├── README.md                       # 导航
├── proposal.md                     # 本文件
├── design.md                       # v1.0 ADR 修订原则 + 树状图 + 引用矩阵
├── decisions.md                    # revision-plan-v1.0.md 的 A1-A7 + B + C1-C6 决策列表
├── roadmap.md                      # Batch B1-B8 执行记录(全部完成)
├── specs/
│   └── adr-revision-plan/
│       └── spec.md                 # ADDED Requirements: 6 份新 ADR + Status Update + 索引同步
└── tasks.md                        # 22 文件任务清单 + 完成状态
```

---

**起草**: Sisyphus (2027-02-09, per Metis 评审 + revision-plan-v1.0.md 执行落地)
**Owner**: CppTLM Team
**状态**: 📋 Proposed — 待 `openspec validate` PASS 后 archive
