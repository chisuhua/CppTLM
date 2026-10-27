# CppTLM `docs/research/` — 框架级调研（CEP 一致性 + gem5 + CppTLM 专用）

> **版本**: v1.0 · **日期**: 2026-10-27 · **状态**: 📋 Research Index
> **维护**: CppTLM Team (Sisyphus)
> **变更**: 2026-10-27 **拆分迁移** —— 硬件微架构调研（74 份）迁往 **ArchForge 仓** `docs/research/`；本目录保留 **框架级**调研（11 份）。详见 §4。

---

## 1. 范围

本目录收纳 **CppTLM 框架级能力**的调研材料：**一致性互联探索平台（CEP）** 的协议空间调研与架构评审、**gem5 Ruby / Garnet** 一致性协议族调研、以及 **CppTLM 专用**的 GPU SoC 融合调研。

**这些文件是本仓已签发 ADR 与 active change 的"判据定义真源"或"证据锚点"**，因此**不能随硬件调研一并迁出**（判据一旦离开被引用的 ADR，ADR 就不再自足）。

**使用原则**：

- ✅ 引用调研结论作为**框架级**设计参考
- ❌ **不直接复制第三方内容到源码**（避免版权风险）
- ❌ **不声称与第三方实现等价**（仅 reference，非 conformance）
- ⚠ **架构真源在 `docs/architecture/`（真源 1）+ `docs/designs/`（真源 2）+ `docs/adr/`（真源 3）**；本目录是**附属证据层**，不承载"现状真相"，不单独作为设计裁决依据

---

## 2. 目录索引（11 份）

### 2.1 CEP 一致性探索平台（2 份）★

| 文件 | 角色 | 主要引用方 |
|------|------|-----------|
| `cep-coherence-protocol-space-survey.md` | **ADR-CEP-08/09/12/13/14 的判据定义真源** | `docs/adr/ADR-CEP-08/09/12/13/14`、`docs/architecture/07-coherence-exploration-platform.md` |
| `cep-cohif-carrier-architecture-review-2026-10-26.md` | active change `cpptlm-cep-cohif-carrier` 的**架构依据** | 该 change 的 proposal/design/tasks + `.rddf/` |

> ⚠ **已知治理缺陷（登记中）**：`ADR-CEP-19` 全文对 `CC-0x` 只有编号引用、**不含定义**，判据定义真源落在本目录的附属层 → **真源 3（ADR）不自足**。见 `.rddf/roadmap/objectives/obj-cep-platform.md` 登记项。**迁移前必须先消解该缺陷**（把判据定义回写进 ADR），否则等于把缺陷固化。

### 2.2 gem5 一致性协议族调研（7 份）

| 文件 | 主题 | 主要引用方 |
|------|------|-----------|
| `gem5-soc-survey.md` | gem5 标准库 + AMD MI300X dGPU + ComputeUnit patterns | `docs/architecture/06-inter-module-network.md` / `06-bis-*` / `07-*` |
| `gem5-coherence-protocols-survey.md` | 11 类 coherence 协议族（SLICC） | `ADR-CEP-00` / `ADR-CEP-17` / `06-bis-*` / `07-*` |
| `gem5-ruby-protocol-bridge-survey.md` | Ruby/Garnet 协议中立通信网络（PNN 借鉴主文档） | `ADR-CEP-00` / `ADR-DGPU-13` / `ADR-DGPU-14` / `06-bis-*` |
| `gem5-garnet-network-survey.md` | Garnet 3.0 router pipeline + credit + NI | `06-inter-module-network.md` / `06-bis-*` / `07-*` |
| `gem5-messagebuffer-network-interface.md` | MessageBuffer + NetworkInterface 事务流转 | `06-inter-module-network.md` / `06-bis-*` / `07-*` |
| `gem5-coherence-network-evolution.md` | gem5 一致性网络 20+ 年演进史 | `06-bis-*` |
| `gem5-slicc-chi-survey.md` | SLICC 编译链 + AMBA CHI 协议族 | `ADR-CEP-00` / `06-bis-*` |

### 2.3 CppTLM 专用（1 份）

| 文件 | 主题 | 主要引用方 |
|------|------|-----------|
| `research-cpptlm-gpu-fused-soc-survey.md` | CppTLM GPU SoC 融合调研 | `.rddf/` 规划依据 |

---

## 3. 跨仓指针：已迁往 **ArchForge** 的调研

以下硬件微架构调研于 **2026-10-27** 迁往 **ArchForge 仓**（SoC 架构建模真源仓）：

| 原 CppTLM 路径 | ArchForge 路径 | 对应 ArchForge 架构文档 |
|---------------|---------------|---------------------|
| `docs/research/SM/`（25 份） | `docs/research/SM/` | `05-sm-compute-unit.md` / `15-sm-microarchitecture-design.md` |
| `docs/research/TMU/`（16 份） | `docs/research/TMU/` | `03-task-management-unit.md` |
| `docs/research/WDU/` + `WDUtoSM/`（14 份） | `docs/research/WDU/` + `WDUtoSM/` | `04-work-distribution.md` |
| `docs/research/CP/`（13 份） | `docs/research/CP/` | `02-command-processor.md` |
| `docs/research/PCIe/`（1 份） | `docs/research/PCIe/` | `16-pcie-endpoint-architecture.md` |
| `docs/research/` 顶层 5 份硬件专利解析 | `docs/research/` 顶层 | — |
| `docs/architecture/14-sm-operand-supply-design-target.md` | `docs/architecture/33-sm-operand-supply-design-target.md` | SM 操作数供给设计目标 OSF |

> **ArchForge 索引**：`/workspace/main/ArchForge/docs/research/AGENTS.md`
>
> **注意**：ArchForge 原先以相对路径 `../../research/…` 引用这批文件（迁移前**全部悬空**），迁移后 **9/12 自动解悬空**；剩余 3 个为历史遗留（`2026-09-04-*.md` 三份从未存在 + `WDUtoSM/nvidia/US8112614B2_...md` 原文省略号占位）。

### 3.1 跨仓引用写法

**不要**用相对路径跨仓引用（必然失效）。统一写法：

```
【ArchForge 仓】docs/research/SM/CudaCore/SYNTHESIS.md
【CppTLM 仓】docs/research/gem5-soc-survey.md
```

---

## 4. 迁移记录（2026-10-27）

采用**按消费方拆分**，而非整目录搬迁：

| 分类 | 处置 | 依据 |
|------|------|------|
| `CP/` `WDU/` `WDUtoSM/` `TMU/` `PCIe/` `SM/` + 顶层 5 份专利（74 份） | → **ArchForge** | ArchForge 的 `00-overview.md` / ADR-SOC-02/06/09/11/15 以相对路径引用它们（迁移前全悬空），且主题属 **SoC 架构建模** |
| `cep-*.md` `gem5-*.md` `research-cpptlm-*.md`（11 份） | → **留 CppTLM** | 是本仓**已签发 ADR / active change** 的判据真源与证据锚点；主题属**框架级能力**（CEP） |
| `docs/architecture/14-sm-operand-supply-design-target.md` | → **ArchForge `docs/architecture/33-…`** | silicon 侧真源在 ArchForge；编号 `14` 在 ArchForge 已被 `14-stage-a-architecture-audit.md` 占用 |

**保留在本仓的副作用（已知）**：`docs/architecture/06-inter-module-network.md` §14 等处对 `docs/research/PCIe/…` 的引用改为**跨仓指针**。

---

## 5. 命名约定

- **综述文档**：`overview.md` / `SYNTHESIS.md`
- **专利解析文件**：`US<专利号>_<中文主题>_解析.md`（**已全部迁出**，本仓不再有）
- **论文解析**：`arXiv<编号>_<中文主题>_解析.md`（**已全部迁出**）
- **第三方原文不存放**（仅分析）— 避免版权 + 体积问题

---

## 6. 相关文档

- 本仓真源 1：[`docs/architecture/`](../architecture/)（含 [README](../architecture/README.md) 索引）
- 本仓真源 2：[`docs/designs/`](../designs/)
- 本仓真源 3（ADR）：[`docs/adr/`](../adr/)
- ArchForge 仓（设计真源，SoC 架构建模）：`/workspace/main/ArchForge` → 其 `docs/research/AGENTS.md`（调研索引）· `docs/architecture/INDEX.md`（架构索引）· `docs/adr/README.md`（ADR 索引）

---

*本索引基于 2026-10-27 的一手材料与跨仓引用扫描整理。硬件微架构调研的迁移是跨仓动作，需两仓同步提交。*
