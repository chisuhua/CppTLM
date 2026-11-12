# ADR-CEP-00: Coherence Exploration Platform 跨仓治理与归口

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: 跨仓治理决策(签发仓待定 — 候选 ArchForge / CppTLM,见 §5)
> **关联真源**:
> - 真源 1:[`../architecture/06-inter-module-network.md`](../architecture/06-inter-module-network.md) v0.4(锚点 per `ADR-CEP-00A §3.1`)
> - 真源 1:[`../architecture/06-bis-coherence-as-network-extension.md`](../architecture/06-bis-coherence-as-network-extension.md) v0.3(将被 supersede;锚点 per `ADR-CEP-00A §3.2`)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节

---

## 1. 背景

Coherence Exploration Platform(CEP)是一个支持 CHI / CXL 3.0 / TileLink-C / NVLink / UALink 五种协议作为可插拔插件的一致性互联探索平台。架构探索包含:

- **6 层框架**(per `ADR-CEP-01` 首次定义;非 06 v0.4 既有):
  Stub Cache / Coherence Interface / Protocol Engine / Directory & Ordering Plane / Fabric Collective Plane / Fabric & Transport Plane
- **19 个 CEP-NN ADR 编号 + 1 份 NUMBER-MAP = 20 份文件**(per `ADR-CEP-NUMBER-MAP §2 Decision 1` 预分配:CEP-00/00A/01/02/03/04/05/06/07/07A/08/09/10/11/12/13/14/15/16 + NUM;CEP-00 ~ CEP-05 在第 1 回合起草,其余为本批 / 第 3 回合起草)

核心矛盾:CEP 的范围与 `06-bis-coherence-as-network-extension.md` v0.3 §7.3-§7.7 阶段范围(P2.0-P5,per `ADR-CEP-00A §3.2` 实证修订;§7.8 实为 KPI 串行约束)严重重叠。**双计划并存 = 漂移风险**。

### 1.1 关于 06/06-bis 章节引用的修订

06 v0.4 §2.1 锁定的是 L1-L6 payload/port 层模型(`tlm_generic_payload`/`MasterPort`/`ChStream`/`StreamAdapter`/`Bundle`/协议字段),与 CEP 6 层是完全不同的层模型。**CEP 6 层是 CEP 首次定义**(per `ADR-CEP-01`),非 06 v0.4 锁定。

---

## 2. 决策(锁定 3 件事)

### 决策 1:代码归口 = CppTLM,设计权威按定性分层

- **CEP 所有可执行代码(C++)** 归 CppTLM 仓(`src/` / `include/` / `test/`)
- **CEP 设计真源** 按架构师定性分层:
  - **归 CppTLM 真源 1**(`docs/architecture/`):框架级能力 = **6 层骨架**(Stub Cache + Coherence Interface + Protocol Engine + Directory & Ordering Plane + Fabric Collective Plane + Fabric & Transport Plane,per `ADR-CEP-01 §1.1`) + Stub Cache 接口 + Coherence Interface 抽象事务流 + 插件机制
  - **归 ArchForge 真源 2**(跨仓镜像):协议语义建模 = 5 个具体协议(CHI / CXL 3.0 / TileLink-C / NVLink / UALink)的设计文档 + 协议取舍类 ADR + topology 探索研究报告

### 决策 2:CEP ↔ 06-bis P2-P5 supersede 路径

- 06-bis v0.3 §7.3-§7.7 阶段范围(P2.0 = CoherenceProtocol / P2.1 = CoherenceBridge / P3 = SnoopFilter + MesiGpuProtocol / P3.5 = JSON 端口路径 / P4 = CHI 完整协议 / P5 = IOMMU + 跨 vendor,per `ADR-CEP-00A §3.2` 实证)被 CEP 完整继承
- 在 `06-bis-coherence-as-network-extension.md` 顶部加 `## Status Update 2026-10-19: superseded by CEP ADR-CEP-00 + CEP-01 ~ CEP-05(范围继承,详见 ADR-CEP-00 §3)` 段(per AGENTS.md "ADR 不可变 + Status Update" 先例)
- 06-bis §2.1 跨仓分工契约保留(ArchForge = 设计真源,CppTLM = 实施责任),作为 CEP 跨仓同步机制的基础

### 决策 3:单 DRI + 准入门 + KPI 排队(per oracle B2-1 修订:Stage 0 计入 KPI + 算术闭环)

- **CEP DRI** = 架构师本人(本仓当前架构师代理)
- **准入门**:ArchForge 协议接口 PR 未合入前,CppTLM 对应 OpenSpec change 不得从 Proposed Draft 转 Proposed
  - **执行机制**:DRI 人工 gate + tasks.md checklist 引用 `openspec/changes/archive/2026-10-20-cpptlm-p2-integration-unblock/tasks.md:189-192` 先例
  - **审计**:每季度跨仓审计(per `06-bis §7.8` KPI 串行约束)由 DRI 执行,产出对齐报告
- **KPI 排队**(per AGENTS.md "OpenSpec Proposed ≤ 3" 政策 + 当前基线 3/3 上限触发,**v0.3 算术闭环**):
  - **Stage 0**: 1 个 umbrella change `cpptlm-cep-umbrella`(1 KPI slot,30 人日,doc-only + 测试契约骨架 — **计入 KPI**,per oracle B2-1 修订:umbrella 不符 doc-only 豁免"0 函数签名/逻辑/构建改动"先例)
  - **Stage 1**: 10 个串行 `cpptlm-cep-poc-spike` / `cpptlm-cep-stub-cache-core` / `-directory-ordering-plane` / `-collective-plane-base` / `-moesi-gpu-adapter` / `-cohif-carrier` / `-chi-rnf-subset` / `-mesif-adapter` / `-cxl-nonsymmetric` / `-tilelink-tree-manager`(每次 1 KPI slot,KPI ≤3 → 排 10 季度)
  - **Stage 2**: 2 个串行 `-cxl-dcoh-symmetric` / `-nvlink-sharp`(KPI 排队同上)
  - **Stage 3**: 1 个 `-ualink-inc`
  - **总计**:14 个 change(1 + 10 + 2 + 1 = 14),串行 14 季度;**任一时刻 Proposed 并发 ≤ 1**(守 KPI 上限)

> **2026-10-22 增补(台账 Entry #12 [adr-amendment])** —— `cpptlm-cep-poc-spike`:
> - **性质**:**可丢弃的 spike**(单文件 / 不进 `ModuleFactory` / 不注册 ABI / 无公共 API)——
>   **不是** Stage 1 的实现,只验证「可组合性 + 契约自洽」,**不是** conformance test
> - **用例规格**:`docs/implementation/2026-10-22-cep-poc-spike-spec.md`(S1/S2/S3 的输入 / 期望 / 判定)
> - **置首位的理由**:其 **S2 场景**(D-GPU 的 E 态静默升级 → D-FABRIC 读同一行 → 读到旧值还是新值)
>   是 `ADR-CEP-13` `INV-CEP-13-IC2` 判据 **(b)** 的**可执行**判定 —— 而 (b) 是 **A6** 的唯一输入,
>   A6 又是**全部协议插件 change** 的前置(`perm` 表达力未定)→ **它是唯一能加速其余 Stage 1 change 的项**;
>   若排在末位,则「解阻 Q-3」的用途失效(见 PoC 规格 §1.2 item 3 的循环依赖澄清)
> - **不改变**「任一时刻 Proposed 并发 ≤ 1」的 KPI 约束 —— 排队**长度** +1,并发不变
> - **登记 ≠ 立刻实现**:其**实现**排在 **A4**(本批已完成)+ **A6** 的相关输入齐备之后;规格已先行
>   (per P-2,`docs/implementation/2026-10-22-cep-poc-spike-spec.md`)

### 决策 4:归"建模框架(CppTLM)"豁免论证(per S9 + 2026-10-20 gem5 Ruby 类比强化)

**定性依据**(用户决策 2026-10-20):**"支持做互相协议探索的"归入框架级,类比 gem5 Ruby network 做一致性探索**。

**gem5 Ruby 实证**(per `docs/research/gem5-coherence-protocols-survey.md:16-23` + `gem5-ruby-protocol-bridge-survey.md:43-48`):
- gem5 Ruby 位于 **gem5 仓内** `src/mem/ruby/`,是 gem5 框架的 memory subsystem 建模子系统
- 其协议通过 SLICC 可插拔(MESI_Two_Level / MESI_Three_Level / MOESI_CMP_directory / MOESI_hammer / MOESI_AMD_Base / GPU_VIPER)
- **gem5 从未把"协议一致性探索"做成 gem5 之外的项目** — 它是 gem5 框架能力

**CEP 同构** → CEP 亦为 **CppTLM 框架能力**,不是 dGPU 业务扩展,不是 ArchForge 项目。

AGENTS.md:423 边界条款("SoC 架构**组件/拓扑/演进**类提案" → ArchForge)逐条对照:

| 范围判定项 | CEP 归属 | 理由 |
|----------|---------|------|
| **6 层**框架骨架 | CppTLM(framework-level) | 类比 gem5 Ruby — 跨协议一致性探索 = 框架能力 |
| Stub Cache / Coherence Interface 接口 | CppTLM | 仿真层抽象,不绑定 SoC 业务 |
| ProtocolEngine 插件机制 | CppTLM | 插件加载/注册/lifecycle 与 06/06-bis ModuleFactory 复用 |
| 5 个具体协议语义建模 | ArchForge(可选) | 协议语义属于"SoC 架构组件"范畴 — **但不是 CEP ADR 签发依赖**(per §5.1 推论)|
| topology 探索研究 | ArchForge(可选) | 与 dGPU SOC 设计同根 |

**结论**:CEP 核心框架归属 CppTLM,**不触发 AGENTS.md:423 边界条款**。CEP ADR 签发仓 = CppTLM(per §5.1)。

---

## 3. 06-bis supersede 范围映射表

**校准**(per `ADR-CEP-00A §3.2` 行号实证 + oracle v0.4 B2 修订 P5 语义):

| 06-bis v0.3 阶段(行号,per `ADR-CEP-00A §3.2`)| CEP 承接位置 | 备注 |
|----------------------|-------------|------|
| **P2.0** CoherenceProtocol C++ 实现(`06-bis §7.3` 含 P2.0/P2.1 合并节,行 ~620-630)| **CEP 首次定义**(per `ADR-CEP-04` §6 + `ADR-CEP-03 §3.2` R3)| 06-bis §5.2 已定义 SnoopProbe(`06-bis:411`)/SnoopRespBundle 沿用 |
| **P2.1** CoherenceBridge C++ 实施(`06-bis §7.3` 合并节 P2.1)| **CEP 首次定义**(per `ADR-CEP-01` §2 Decision 1 programmatic bridge 路径)| CHI / CXL / TileLink 各自适配 |
| **P3** SnoopFilter directory-based(`06-bis §7.4`)| **CEP 首次定义**(per `ADR-CEP-04` §2 SharerTracker 5 种实现)| DCOH 对称模式完整继承 |
| **P3.5** JSON 端口路径打通(`06-bis §7.5`)| 沿用 06 §7 Python 拓扑 + ProtocolEngine 插件描述符(`ADR-CEP-NUMBER-MAP §2 Decision 1` 预分配)| 与 programmatic bridge 路径一致 |
| **P4** CHI 完整协议支持(`06-bis §7.6`)| CEP ProtocolEngine `ChiRnfAdapter` 完整实现(从 subset 到 full)| 当前 CHI RN-F subset 在 CEP ProtocolEngine 雏形 |
| **P5** IOMMU 集成 + 跨 vendor coherence(`06-bis §7.7`,**scope reduction 待签发决策 ADR**,per oracle B2 注:NVLink/UALink 非历史 IOMMU)| **🟡 待签发独立 ADR**(非 CEP ADR 涵盖,需 scope reduction 决策)| 当前:仅文档化,无实施承接 |

**注 1**:`06-bis §7.8`(行 653) = "v0.3 总 KPI 调整 — 串行激活,并发 ≤ 1",**非阶段范围**。

**注 2**:`06 §15.4` 演化阶段编号(P2.0/P2.1/P3/P3.5/P4/P5,**无 P6**)是 CEP 系列 ADR 的唯一口径;`06 §11.4` 主 roadmap 表 P6 不引入(per `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结)。

**supersede 实施**:在 `06-bis-coherence-as-network-extension.md` 顶部加 `## Status Update 2026-10-19: superseded by CEP ADR-CEP-00 + CEP-01 ~ CEP-05` 段(per AGENTS.md "ADR 不可变 + Status Update" 先例,不动 06-bis 正文)。

---

## 4. 跨仓同步机制

沿用既有先例,不新造机制:

1. **设计权威 → 实施的单向门**(per 06-bis §2.2):ArchForge 接口变更 → 先 ArchForge 仓合并 → 再同步 → 再实施
2. **STATUS UPDATE 同步契约**(per 06-bis §5.5/§6.4):CppTLM 实施时对 ArchForge 接口做类型改写、命名空间镜像后,必须在 ArchForge markdown 加 `## STATUS UPDATE` 段
3. **跨仓 PR 作为 OpenSpec gate**(per `openspec/changes/archive/2026-10-20-cpptlm-p2-integration-unblock/tasks.md:189-192`):tasks.md 显式列 "ArchForge 仓 PR 已开" 作为 gate
4. **VIRTUAL_PATHS 防路径漂移**(per `scripts/test/docs_sync_check.sh:115-116`):CEP 文档引用 ArchForge 路径时注册新 VIRTUAL_PATHS 条目
5. **编译期漂移绊线**:协议 Bundle 用 `BUNDLE_FIELD_WIDTH` static_assert 锁宽度(per `include/bundles/cpphdl_types.hh`)
6. **季度跨仓审计**(per 06-bis §2.2 第 4 条):接口签名 vs 实现比对

---

## 5. 待决项与决策汇总(合并 per S3)

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 代码归口 | CppTLM | ✅ 已拍板 |
| 设计权威分层 | CppTLM 真源 1(框架级) + ArchForge 真源 2(协议语义)| ✅ 已拍板 |
| 06-bis supersede | 顶部加 Status Update 段,内容继承 | ✅ 已拍板 |
| DRI | 架构师本人 | ✅ 已拍板 |
| 准入门 | ArchForge PR 合入前 CppTLM change 不升 Proposed | ✅ 已拍板 |
| KPI 排队 | 14 个 change 串行 14 季度(per §2 决策 3;2026-10-22 per Entry #12 由 10 → 11;**2026-10-26 per Entry #71 由 11 → 13** —— `moesi_gpu` / `mesif` 排入 Stage 1;**2026-10-27 per Entry #74 由 13 → 14** —— 新增 `-cohif-carrier` 插在 `-moesi-gpu-adapter` 与 `-chi-rnf-subset` 之间) | ✅ 已拍板 |
| AGENTS.md:423 边界 | 不触发(CEP 框架级不属 SoC 业务架构) | ✅ 已拍板 |
| **ADR 签发仓** | ✅ **已拍板 = CppTLM 仓** — 定性:"支持做互相协议探索的"归入框架级,类比 gem5 Ruby network(per §5.1)| ✅ 已拍板 |
| **真源文档路径** | ✅ **已拍板 α** = `docs/architecture/07-coherence-exploration-platform.md`(2026-10-20 创建 v0.1 草案) — 见 §5.2 | ✅ 已拍板 |

### 5.1 ADR 签发仓 ✅ 已拍板(2026-10-20)

**决策:签发仓 = CppTLM 仓**(`docs/adr/ADR-CEP-*.md` 物理留在本仓)。

**定性依据**(用户决策):**"支持做互相协议探索的"归入框架级,类比 gem5 Ruby network 做一致性探索**。

**gem5 Ruby 类比锚点**(per `docs/research/gem5-ruby-protocol-bridge-survey.md` + `gem5-coherence-protocols-survey.md` 实证):
- gem5 Ruby 是 **gem5 框架内的 memory subsystem 建模子系统**(Sequencer / CacheMemory / SLICC Controller / MessageBuffer / NetworkInterface / Garnet),**不是** gem5 之外的项目
- gem5 Ruby 的**协议可插拔性**通过 SLICC 实现(MESI_Two_Level / MESI_Three_Level / MESI_Three_Level_HTM / MOESI_CMP_directory / MOESI_CMP_token / MOESI_hammer / MOESI_AMD_Base / GPU_VIPER 等,per `gem5-coherence-protocols-survey.md:16-23` 实证表)
- gem5 Ruby 的**目录 / 探测结构多样性**通过**不同协议各自实现不同的 `.sm` 组件**达成 — `MOESI_CMP_directory-dir.sm`(中心目录)vs `MOESI_AMD_Base-probeFilter.sm`(保守探测过滤)vs `MOESI_CMP_token` 的 token 机制(副本携带权限,无中心 sharer set);**不是**同一 `.slicc` 内切换变体(per oracle v0.5 复审 MEDIUM 修订 + `gem5-slicc-chi-survey.md:193` 实证)
- → **gem5 把"跨协议一致性探索"做成框架能力,而不是每个协议一个项目**

**CEP 同构定位**:

| 维度 | gem5 Ruby | CppTLM CEP |
|------|-----------|-------------|
| 宿主框架 | gem5 | CppTLM |
| 协议可插拔机制 | SLICC `.slicc` 文件 | ProtocolEngine 插件 + 能力映射表 |
| 协议实现数 | 6+ 协议族 | **2 来源族 / 16 adapter 实例**(工业 6 族 8 实例 + Ruby-SLICC 1 族 8 实例)+ **5 个一致性模型语义索引**(交叉视图,per `07 号 §5.1`)|
| 目录策略可插拔 | `.slicc` 变体 | SharerTracker 5 实现 + caps descriptor |
| 探索产出 | 协议 / 拓扑 / 目录对比实验 | 同左(per `ADR-CEP-06` 4 件套)|
| **仓归属** | **gem5 仓内(`gem5/src/mem/ruby/`)** | **CppTLM 仓内(`docs/adr/` + `docs/architecture/` + `src/tlm/cep/`)** |

**推论**(消除跨仓归属冲突):
- 10 份 CEP ADR **全部留在 CppTLM 仓**(真源 3)
- `docs/architecture/07-coherence-exploration-platform.md` 留在 CppTLM 仓(真源 1)—— 与 ADR 同仓,引用不断裂
- **协议语义建模文档(5 个具体协议的设计文档)仍可归 ArchForge**(真源 2),但**不构成 CEP ADR 的签发依赖** — 走跨仓引用而非迁移
- 跨仓准入门(§2 Decision 3)保持有效:ArchForge 侧协议接口 PR 未合入前,CppTLM change 不升 Proposed

**与 v0.1 候选 α 的差异**:初版建议"签发仓 = ArchForge(跨仓治理)",本决策改为 **CppTLM**,理由即上述 gem5 Ruby 类比 — 框架级探索能力属于宿主框架,协议语义文档才是跨仓对象。

**能力覆盖类比(2026-10-20 调研补充)**:既然 CEP 定性为框架级一致性探索能力,它**应该**能覆盖 gem5 Ruby 的协议空间 — 调研结论是**能**(6/8 直接支持 + 1 需扩展 + 1 排除,canonical 10 流 100% 覆盖零新增流),详见 [`ADR-CEP-17`](ADR-CEP-17-ruby-slicc-protocol-family.md) + [`docs/research/cep-coherence-protocol-space-survey.md`](../research/cep-coherence-protocol-space-survey.md)。

**生态类比(2026-10-20 调研 v0.2 补充)** —— gem5 Ruby 类比的**第二重含义**:

| 层 | 含义 | 证据 |
|----|------|------|
| **归口类比** | gem5 Ruby 是 gem5 的**框架能力**(不是 gem5 之外的项目);CEP 同理是 CppTLM 的框架能力 | 本节 v0.1 论证 |
| **生态类比** | gem5 **已经是学术协议生成研究线的标准评测床** —— HeteroGen(HPCA 2022)在 gem5 上以 **64 核**(60 tiny + 4 big)对比手工异构协议 HCC,平均性能相当、网络流量差异 < 5% | `HeteroGen_HPCA2022-draft.md:1382-1400` |
| **生态类比(源码级,v0.3 补)** | 更强:学术工具**直接产出 gem5 Ruby 协议源文件** —— HeteroGen 仓库含 `gem5_HeteroGen_protocols/`(`HETERO_RCCO_GEN_*.slicc` wrapper + 5 个 `.sm` 实体 + `configs/ruby/*.py` + `setup.sh` / `run_*.sh`)。⚠️ **非开箱即用**:需 `alloy-gem5`(定制 fork)+ 外部 docker 参考系统 | 源码 `HeteroGen/gem5_HeteroGen_protocols/`(MIT License,最后提交 2022-01-07) |

**推论**:CEP 若要真正成为"框架级一致性探索平台",就不仅要**类比** gem5 Ruby,还要**能承载**这类在 gem5 上做过的研究(如把 *Gen 族的生成协议*接进 CEP 探索)。这为 `ADR-CEP-17 Decision 7`(学术协议族纳入)提供了除"覆盖广度"之外的**第二重理由**。

**v0.3 边界澄清**:源码核实显示 *Gen 族当前**不能**从 SSP 生成 SLICC(后端只有 Murphi,论文 §9.2 规划的 SLICC/Rumur 后端**未落地**),但**已经能直接产出 gem5 `.slicc`**。→ 二者关系是**互补**:*Gen 族**生成 gem5 协议**,CEP **在统一实验床上运行 / 对比协议**。CEP 的价值随 gem5 协议空间扩张而**上升**,不下降。

### 5.2 真源文档路径 ✅ 已拍板(2026-10-20)

CEP 框架级文档(**6 层**骨架 + Stub Cache 接口 + Coherence Interface 抽象事务流 + 插件机制):
- **(α) `docs/architecture/07-coherence-exploration-platform.md`** — ✅ **已拍板**,2026-10-20 创建 v0.1 草案,与 06 / 06-bis 平级
- (β) 在 `06-bis-coherence-as-network-extension.md` 内扩写 — ❌ 未采纳(supersede 后该文档冻结)

**落地**:`docs/architecture/07-coherence-exploration-platform.md` 已创建(525 行),并注册到 `docs/architecture/README.md` Doc Map(per ADR-0057 theme doc sync)。

### 5.3 循环依赖解除

CEP-01/03/04/05 头部引用本 ADR 作为"真源 3",但本 ADR v0.1 未签发 → 形成循环依赖。**解除方法**:

- v0.2 阶段:在 CEP-01/03/04/05 头部对 CEP-00 引用处加注"(草案,循环引用待签发解除)"
- v0.3 阶段(本 ADR 签发后):删去此注,正式形成真源 3 引用

---

## 6. 关联 ADR

- 真源 3:`ADR-CEP-00A-real-source-anchor-map.md`(本批起草)
- 真源 3:`ADR-CEP-06-experiment-protocol.md`(本批起草)
- 真源 3:`ADR-CEP-07-simulation-and-integration-semantics.md`(本批起草)
- 真源 3:`ADR-CEP-NUMBER-MAP.md`(本批起草)
- 真源 3:`ADR-DGPU-13-pnn-protocol-neutral-network.md`(已 superseded by 06 v0.4)
- 真源 3:`ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`(已 superseded by 06 v0.4)
- 真源 3:`ADR-DGPU-06-axi-mem-bundle-boundary.md`(沿用,chip-internal AXI vs board-level PCIe)
- 真源 3:`ADR-DGPU-08-abi-freeze-policy.md`(沿用,23 ABI 冻结)
- 真源 3:`ADR-DGPU-09-driver-visible-minimal-soc-scope.md`(沿用,driver-visible 入口)
- 真源 3:`ADR-DGPU-11-timing-mode-soc-scope.md`(沿用,simulation_mode 正交)
- 真源 3:`ADR-DGPU-12-soc-internal-tlm-wiring-principle.md`(沿用,ModuleFactory 装配)
- 真源 3:`ADR-CEP-17-ruby-slicc-protocol-family.md`(Ruby-SLICC 协议族接入 + 协议私有状态边界,2026-10-20)
- 真源 3:`ADR-X.1-transaction-id.md`(沿用,txn_id 命名空间)

---

## 7. v0.3 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 8. Status Update(2026-10-21)—— §2 决策 3 准入门措辞修订(接口 PR → 接口规约 PR)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)——
> §2 决策 3 的**正文不改**,仅登记准入门判据 **C-1** 的措辞变更。
> **回执载体**: [`docs/implementation/2026-10-21-cep-archforge-response-receipt.md`](../implementation/2026-10-21-cep-archforge-response-receipt.md)
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` Entry #8(`adr-amendment`)

### 8.1 修订内容

§2 决策 3 的**准入门条款**中,判据 **C-1** 的措辞:

| | 措辞 | 载体 |
|---|------|------|
| **修订前** | ArchForge 侧存在**已合入**的 coherence 接口 **PR**(有 commit hash) | 本清单 §6 C-1 / `ADR-CEP-00 §2 决策 3` |
| **修订后** | ArchForge 侧存在**已合入**的 coherence **接口规约文档**(有 commit hash)—— 即**纯设计仓**形态的规约交付物<br>(形态 = `ADR-SOC-25` + `32 号` + roadmap 答复表) | 同上 |

**实现型 PR 归 CppTLM 仓**(per §2 决策 1 的代码归口:全部 C++ 实现归 CppTLM)——
准入门**只管「接口形状已定稿」**,不管「实现已落地」。

### 8.2 理由

ArchForge 是**纯设计仓**(`git log` 全为 `docs(...)` 提交,无 `src/`、无 `include/`)→
**结构上无法**产出「接口 PR」→ 修订前措辞下 C-1 **永远无法闭合** → M2 准入门**永久阻塞**。
这是本轮唯一的**治理级**阻塞,优先于一切技术结论。

### 8.3 效力范围与边界(重要 —— 措辞变更**不放行** M2)

1. **效力载体**: 判据 C-1 的**定义载体**是本仓
   `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md §6`(工作清单,非冻结集)——
   §6 的 C-1 行已按本段修订。§2 决策 3 的**正文句子**按不可变约定**保持原文**
   (「ArchForge 协议接口 PR 未合入前…」),**其判据口径以本段为准**;
2. 本次修订**只解决「判据的可闭合性」**,**不解决「判据的闭合」**;
3. 截至本 Status Update,C-1 **仍为红** —— ArchForge 侧三份答复文档
   (`roadmap/cep-interface-alignment.md` v0.2 / `ADR-SOC-25` / `32 号`)**尚未提交进 git**
   (2026-10-21 实测:`M` + `??` × 2)→ **无 commit hash 可查**;
   > ✅ **Status Update 2026-10-21(提交后追记)**:ArchForge 已于 commit **`6020f42`**
   > 合入上述三份规约文档(8 文件 / +1415 行)→ **C-1 已闭合**。
   > 本段正文保留 2026-10-21 首次登记时的状态快照,不回改。
4. M2 放行仍需 **C-1 ~ C-5 全部满足** + DRI 人工 gate(per §2 决策 3 执行机制);
5. `cpptlm-cep-umbrella` 维持 **🟡 Proposed Draft**,Stage 0 **不 archive**。

### 8.4 连带影响

| 项 | 影响 |
|----|------|
| `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md` §6 | C-1 措辞同步 + 状态列更新 |
| `docs/implementation/2026-10-21-cep-archforge-response-receipt.md` | 新增(本轮回执) |
| M1 冻结集 | 本文件属冻结集 → 冻结清单**已重生成**(`cep_freeze_check.sh --update`) |

## Status Update 2026-10-23 —— §8 第 5 条失效登记(`cpptlm-cep-umbrella` 归档)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§8 正文不改**,仅登记状态变化。
> **§8 第 5 条**「`cpptlm-cep-umbrella` 维持 **🟡 Proposed Draft**,Stage 0 **不 archive**」—— **已失效**:
> - 该 change 现为 **✅ archived**(归档目录 `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/`);
> - **KPI slot 已释放** —— 本段登记时点(2026-10-23)Proposed 并发 **0/3**(实测 `openspec list --json`),下一个可启动 change = 队列 #2 `cpptlm-cep-poc-spike`(per `obj §4.0`)。**该状态已于 2026-10-24 变化 → 见下段 Status Update**。
> **不受影响的部分**:§8 其余条款(准入门措辞修订 **C-1** 及其理由)**继续有效** —— C-1 已于 2026-10-21 闭合(ArchForge `6020f42`)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #33**
## Status Update 2026-10-24 —— 队列 #2 `cpptlm-cep-poc-spike` 立项

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文一律不改**,仅登记状态变化。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #42**
## Status Update 2026-10-24 —— 队列 #2 `cpptlm-cep-poc-spike` 归档:delta capability 并入 main spec

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **前置门**:归档前经 **critic 7 轮复审** —— 前 6 轮 REJECT(逐轮揭示「有牙」的六类缺口:判别力 / 真实性 / 敏感度 / 等价性 / 夹具自守 / 覆盖承诺+正控),每轮均修复并以 mutant 逐条反证;**第 7 轮 APPROVE**(per `obj §11` Entry #49~#54)。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #55**
> **owner 重指向(per Entry #55)**:S1-1 ①定义层 = ✅已消费;**②建图层 → 队列 #4**;**③放行闸 → 全局不变量**(不挂队列行);**M-4 决策 → 队列 #3**;**S1-5 表体 → 队列 #6**;S1-2/3/4 → 队列 #3+。
## Status Update 2026-10-25 —— 队列 #3 `cpptlm-cep-stub-cache-core` 立项:M-4 行为侧决策落盘

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **M-4 行为侧决策已落**(承接自队列 #2 归档时的 owner 重指向):协议层 credit **不独占** VC(该问题**已**被 ArchForge `32 号 §6.0.1` 排除表关闭,本仓**不重新裁决**)· 传输层**保持索引对齐**、**不建**显式映射表(显式映射表归队列 #6)· `VC_DAT` 语义正名(**预留 DAT 桶**,当前恒 0)· 冲突输入(`vc_id` vs `flit_category`)**传输层不仲裁**;**`include/tlm/cep/fabric.schema.json` 的 `vc_count.maximum=4` 不需要动** —— 本次冻结面改动仅**本文件**与 `ADR-CEP-05` 的 Status Update 追加段。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived);**§2 决策 3 的排队算术不变** —— 队列 #3 为**单一** change(`ADR-CEP-05 §10` 的 `-base`/`-impl` 二分作废,见该 ADR §14.1)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #58**
## Status Update 2026-10-25 —— 队列 #3 `cpptlm-cep-stub-cache-core` 归档:M-4 行为侧决策的最终落点

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **M-4 行为侧决策的最终落点**:协议层 credit **不独占** VC(该问题**已**被 ArchForge `32 号 §6.0.1` 排除表关闭,本仓**不重新裁决**)· 传输层**保持索引对齐**、**不建**显式映射表(显式映射表归队列 #6)· `VC_DAT` 语义正名(**预留 DAT 桶**,当前恒 0)· 冲突输入(`vc_id` vs `flit_category`)**传输层不仲裁**;**`include/tlm/cep/fabric.schema.json` 的 `vc_count.maximum=4` 不需要动** —— 立项时「须同步」的假定经实测**不成立**。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived);**§2 决策 3 的排队算术不变** —— 队列 #3 为**单一** change(`ADR-CEP-05 §10` 的 `-base`/`-impl` 二分作废,见该 ADR §14.1)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #60**
## Status Update 2026-10-25 —— 队列 #4 `cpptlm-cep-directory-ordering-plane` 立项:建图层前置承接

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **裁决(DRI 四问,2026-10-25)**:① **范围切小** —— SharerTrackerIf 单接口 + caps descriptor(第一天就位) + D-5 全量契约 + **FullMapDirectory 唯一行为实现**,其余 4 种实现(SparseSF/DcohSymmetric/TreeManager/BroadcastNull)跟随队列 #6~#9 各自的协议族(per `ADR-CEP-04 §8` 拆分先例 + obj §4.0 排队表);② **装配闸输入源** = `include/tlm/cep/protocol_instances.json`(本仓机器可读实例声明,诞生即不冻结,per Entry #56 先例);③ **双挂**(`instantiateAll` + `validate_topology` 共享同一判据实现,禁双实现);④ **S1-7 (β) 同批补入** `INV-CEP-08-IC2`(E 态静默升级组合检查项)。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);**§2 决策 3 的排队算术不变** —— 队列 #4 为**单一** change(oracle 复审确认不拆 `-base`/`-impls` 二分;`ADR-CEP-04 §8` 的 4 个 change 名**失指已在该 ADR 的 Status Update 处置**)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #62**
## Status Update 2026-10-25 —— 队列 #4 `cpptlm-cep-directory-ordering-plane` 归档:准入前置余项重指向

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **交付内容**: `SharerTrackerIf` 单接口(4 方法)+ caps descriptor(4 属性)+ behavior(2 属性)+ `FullMapDirectory` 唯一行为实现 · `AddressResolverIf` + J1/J2/J3 + `INV-CEP-08-IC2` · 装配期通道依赖图(顶点 = `(实例, network, vc)`,verdict 逐 `(network, vc)`,双挂共享同一判据)`。**S1-1 ②建图层 + S1-7 (β) 重评估两项准入前置同时消费**。
> **S1-7 (β) 裁决**: 不新增第 5 判别位 —— `UNIQUE` 已编码 E 态可写,静默升级的**可组合性**由 `combination_capabilities.hierarchical` 承载;扩 `perm` 会破 `ADR-CEP-02 CEN-Ib11` 的 4 值锁且不增判别力。
> **实施期偏差(5 项)**: caps 4 属性按 `ADR-CEP-04 §2 决策 3`(而非 planner 原写的 hazard/broadcast_fallback)· `PARTIAL` 不可由合法 schema 声明表达(J2 判别力由合成声明承担)· 通道依赖边表作为**声明数据**(`32 号 §6.0.3` 派生副本)· 第 4 个 header(`ordering_point_checker.hh`)· 镜像补齐 2 个实例(`moesi_gpu` + `mesif`)。逐条见 design.md `D-ι`。
> **Stage 1 准入前置的余项**: S1-2 ⛔ 未触发(需首个 `implemented` 实例)· S1-3 ⛔ 未触发(需第 3 张网 / 第 3 个协议实例)· S1-4 ✅ 已消费(Entry #56);登记已重指向队列 #5 行(per Entry #64)。
> **遗留(不属本 change)**: `validate_topology` target 首条命令引用不存在的 `build/configs/dgpu_board_v1_mvp.json`(`scripts/CMakeLists.txt:48`)—— 该 target 事前已不可跑完,修复需单独决策。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #64**
## Status Update 2026-10-26 —— 队列 #5 `cpptlm-cep-collective-plane-base` 立项:准入前置余项承接

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **交付范围(裁决:DRI 四问)**: 范围切小(`CollectivePlaneIf` 单接口 + `NullCollectivePlane` + `LocalCollectivePlane` 只做 group 生命周期 / region-epoch / **BARRIER rendezvous**;REDUCE·BROADCAST·MULTICAST 的**数据路径**推迟至 P3)/ **Bundle 类型内嵌**(偏离 `ADR-CEP-03 §5`,见该 ADR 的 Status Update)/ **header-only 不进 `ModuleFactory`**(对齐 `07 号 §6.3` 最终形态口径)/ **独立声明源** `collective_planes.json`(不碰冻结的 `protocol_instance.schema.json`)。
> **B2 贡献方式**: **新增 invariant 数 = 0**(`ADR-CEP-01` / `ADR-CEP-03` 在 15 组 66 条里**无组**,新增编号即契约违反)⇒ 贡献 = **行为化既有条目**(`INV-CEP-07-IC3` NullCollectivePlane 默认实现 / `INV-CEP-04-IC5` epoch invalid 窗口放行)+ **契约层加固**(零调用边 / canonical 10 流零 diff)。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #66**
## Status Update 2026-10-26 —— 队列 #5 `cpptlm-cep-collective-plane-base` 归档

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **交付内容**: `CollectivePlaneIf` **恰好 7 个虚方法**(逐字对齐 `ADR-CEP-03 §2 Decision 2`)+ 类型集 + `NullCollectivePlane`(**行为化 `INV-CEP-07-IC3`**)+ `LocalCollectivePlane`(group 生命周期 / region-epoch 两阶段 / BARRIER rendezvous / **R2 判读侧三态**)+ **6 条契约判据**(零调用边 / canonical 10 流零 diff / collective VC 预留 / 区域互斥(与行为面同源)/ 完成域判别 / 抓取点可测性登记)+ 独立声明源 `collective_planes.json` + 离线 CLI。
> **B2 贡献口径**: **新增 invariant 数 = 0**(`ADR-CEP-01` / `ADR-CEP-03` 在 15 组 66 条里**无组**,新增编号即契约违反)⇒ 贡献 = **行为化既有条目**(`INV-CEP-07-IC3` NullCollectivePlane 默认实现 / `INV-CEP-04-IC5` 的 **R2 判读侧** —— 其行为主体 `fabric_ingress_classify` 本仓不存在,**P3 实现真 ingress 时须在那里重新行为化**)+ 契约层加固(零调用边 / canonical 10 流零 diff)。
> **三段评审闭环(本仓首次)**: planner 产出评审 APPROVE_WITH_CONDITIONS → builder 实施评审 **REJECT**(3 BLOCKER + 10 SHOULD_FIX,全部经独立 PoC 复验)→ 三审 APPROVE_WITH_CONDITIONS(5 SHOULD_FIX + 6 NIT)⇒ 全部落地(台账 Entry #66~#70)。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #70**

---
## Status Update 2026-10-26 —— 排队算术 11 → 13:新增 `-moesi-gpu-adapter` / `-mesif-adapter`(DRI 三问裁决)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **排队算术变更(本 entry 的实质)**:§2 决策 3 的 **Stage 1 由 7 个串行扩至 9 个** —— 新增 `-moesi-gpu-adapter`(插在 `-chi-rnf-subset` **之前**)与 `-mesif-adapter`(排在 `-chi-rnf-subset` **之后**);`-chi-rnf-subset` 及后续 change 的顺序号顺延。**总计 11 → 13 change,串行 11 → 13 季度**。
> **DRI 三问裁决(2026-10-26)**:① `moesi_gpu` / `mesif` **排入** Stage 1 前段(而非追加到全局末尾)② **S1-7 ①②**(`perm` 形态 schema 演进)**跟 `moesi_gpu` 走** —— 其**唯一真实消费者**是 `moesi_gpu`(`ADR-CEP-05 §13.1` 判据 2 **已触发**:`moesi_gpu` 静默 `E→M` ⇒ 4 值 `perm` 无法表达);若留在 `-chi-rnf-subset`(显式升级的工业协议)⇒ schema 在**无消费者**的真空里演进 = `Entry #58` 已拒绝的形态 ③ **S1-5**(6 类 → `CATEGORY_*` 显式映射表)**跟首个 adapter 走**。
> **准入前置 owner 重指向(per Entry #71)**:`S1-2`(首个 `implemented` 实例装配后复核)由 ⛔ 未触发 → **🟡 由队列 #6 触发**;`S1-5` / `S1-7 ①②` 由队列 #6 **交付**;`S1-3`(第 3 个协议实例)触发点由「未排期」前移至 **队列 #8**(`moesi_gpu` + `chi-rnf` + `mesif` = 第 3 个)。
> **`moesi_gpu` / `mesif` 未排期** 孤儿项 **已于本 entry 消解** —— `moesi_gpu` → 队列 #6,`mesif` → 队列 #8(**显式标注 R-1 `HierarchicalDirectory` 阻塞**)。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived);**§2 决策 3 的「任一时刻 Proposed 并发 ≤ 1」约束不变**;**「登记 ≠ 排期」口径不变** —— 本 entry 登记的是**排队位置**,不是**就绪承诺**。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #71**
## Status Update 2026-10-27 —— 排队算术 13 → 14:新增 `-cohif-carrier`(架构缺口 C1/C2/C3)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。**唯一例外**:`§2 决策 3` 的 **KPI 排队算术块**与 `§5` 决策汇总的 **KPI 排队行**属**活口径**(per `Entry #71` 先例:排队总数变动时必须同步,否则本 ADR 与 `obj §4.0` 永久矛盾)—— 已在下方逐处标注。
> **架构缺口(本 entry 的实质依据,逐 `file:line` 实测)**:
> - **C1** **CEP 零 ChStream 承载体** —— `grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/` → **空**(7 个 `.hh`);`stub_cache_core.hh:154` 实为普通 `class StubCacheCore` —— 而**本文件 §6.3 的等价规定**(`ADR-CEP-07:64/:67` + `07 号 §6.3`)明写 ProtocolEngine / StubCacheCore **都应是** `ChStreamModuleBase`。
> - **C2** **「CEP 用 canonical 10 流的 Bundle」是悬空引用,且同源两处** —— `ADR-CEP-07A:49` + `ADR-CEP-16:45` 均如此写;而 `ADR-CEP-02` 全文 `Bundle` **零命中**、`07 号 §5.4` 业务 Bundle 列表(7 项)**无它**、`include/bundles/` 17 个 `.hh` grep `coherence` **零命中**。
> - **C3** **唯一的 Bundle 化承诺绑在集合通信队列** —— `ADR-CEP-03:225` 推迟至 P3(`cpptlm-cep-nvlink-sharp-adapter`)⇒ **主干流无人认领**。
> **排队算术变更(本 entry 的实质)**:§2 决策 3 的 **Stage 1 由 9 个串行扩至 10 个** —— 新增 `-cohif-carrier`(插在 `-moesi-gpu-adapter` **之后**、`-chi-rnf-subset` **之前**);其后 `-chi-rnf-subset` / `-mesif-adapter` / `-cxl-nonsymmetric` / `-tilelink-tree-manager` 顺延为 #8 / #9 / #10 / #11;Stage 2 顺延为 #12 / #13;Stage 3 顺延为 #14。**总计 13 → 14 个 change(1 + 10 + 2 + 1 = 14),串行 13 → 14 季度**;Stage 1 跨度 2027-Q1 ~ 2029-Q2;**终态 2029-Q4 → 2030-Q1**。
> **为什么插在 #6 之后、#7 之前**:队列 #6 `cpptlm-cep-moesi-gpu-adapter` 交付首个真实协议语义 + **6→3 特化字典**;载体队列**消费**该字典;`chi-rnf-subset` 则**首次在真实载体上验证**。若载体排在 `chi-rnf-subset` 之后 ⇒ 后者继承队列 #6 的**逻辑级**装配验证(`stub_cache_core.hh:42` 明写 `net_side` 由 **mock ProtocolEngine** 驱动)⇒ **载体债复制一遍**。
> **载体交付面(per `ADR-CEP-19` 决策 1~9)**:① `include/bundles/cep_coherence_bundles_tlm.hh`(`bundles::CohIfBundle`,**只覆盖 5 条 RN-side 流**,`flow_id` 区分;无网络层字段)② `include/tlm/cep/protocol_engine_base.hh`(`ChStreamModuleBase` 派生 + `Input/OutputStreamAdapter<CohIfBundle>`)③ `include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>`,**首次落地**;`CohIfBundle ↔ NoCFlitBundle`)④ `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏(**`registerObject` + `registerBidirectionalPortAdapter` 两件都做**;`static_assert(n>1)`)+ `module_factory.hh:313-315` 钩子扩展(死钩子 → 活钩子)。**装配走 JSON 链**(`ADR-CEP-16:46/:86` 明确**不**实现 programmatic bridge 的 C++ 路径;`module_factory.cc:743-750` 的 `is_multi` 分支是**泛型虚分派**)。**显式延期**:`StubCacheCore` 的 ChStream 端口面 → `cpptlm-cep-stub-cache-chstream-surface`(需 MODIFIES 冻结 main spec)。
> **B2 贡献口径**:**新增 invariant 数 = 0**(本 entry 与 `ADR-CEP-19` 均**不定义** invariant ⇒ `ADR-CEP-19` 登记进 `cep_docs_sync_check.sh` 的 `NO_INV` 白名单)⇒ 15 组 66 条编号**未被改动**。
> **准入前置 owner**:本 entry **不新增** Stage 1 准入前置项 —— 载体队列**服务**已有的 `S1-2`(首个 `implemented` 实例装配后复核,由队列 #6 触发)与队列 #8 的装配验证;`S1-3`(第 3 张网 / 第 3 个协议实例)⛔ 仍**未触发**(于 #9 达成)。
> **不受影响的部分**:`§2 决策 3` 的「任一时刻 Proposed 并发 ≤ 1」与 §8 准入门措辞修订 **C-1** 及其理由**继续有效**;`§5` 其余决策行**零改动**。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #74**
## Status Update 2026-10-27 —— 队列 #6 `cpptlm-cep-moesi-gpu-adapter` 归档:2 份 delta capability 并入 main spec

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **不受影响的部分**:§2 决策 3 的**排队算术** · §8 的准入门措辞修订 **C-1** 及其理由 · 冻结成员数 —— 一律**不变**(算术数字的**唯一真源**为 §2 正文与台账 `obj §4.0`)。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #80**
## Status Update 2026-10-27 —— 队列 #7 `cpptlm-cep-cohif-carrier` 立项:4 件套落位

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **判据面**:`ADR-CEP-19` 的 2 处判据增补(CC-05 ④ / CC-06 ②)已由本 ADR 的**上一段**之外的另一份 ADR 追加段追认(`ADR-CEP-19` Status Update 2026-10-27,正文零改动,per 台账 Entry #78)⇒ 本段只登记**状态**,不改判据。
> **不受影响的部分**:§2 决策 3 的**排队算术** · §8 的准入门措辞修订 **C-1** 及其理由 · 冻结成员数 —— 一律**不变**(算术数字的**唯一真源**为 §2 正文与台账 `obj §4.0`)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #81**

---

**版本**: v0.2 草案 + Status Update 2026-10-21 + **Status Update 2026-10-23** + **Status Update 2026-10-24** + **Status Update 2026-10-25**(队列 #3 归档) + **Status Update 2026-10-27**(队列 #6 归档) + **Status Update 2026-10-27**(队列 #7 启动)+ **Status Update 2026-10-25**(队列 #4 启动)+ **Status Update 2026-10-25**(队列 #4 归档)+ **Status Update 2026-10-26**(队列 #5 启动)+ **Status Update 2026-10-26**(队列 #5 归档)+ **Status Update 2026-10-26**(队列 #6 启动 + 排队 11 → 13)
**最后更新**: 2026-10-28
**批准状态**: 🟡 待 Oracle 复审(§5.1 签发仓 + §5.2 真源路径已拍板;§8 Status Update 经 DRI 明示措辞拍板)
**维护者**: CppTLM build agent(architect mode)
## Status Update 2026-10-28 —— 队列 #8 `cpptlm-cep-chi-rnf-subset` 立项(决策见 `ADR-CEP-12 §11`)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **本段登记的决策在别处**:队列 #8 引入的**实例声明权威分区**机制(镜像域 vs 本仓域)属 `ADR-CEP-12` 的管辖范围 ⇒ 决策正文见 **`ADR-CEP-12 §11`**,本 ADR **不复述**。
> **不受影响的部分**:§2 决策 3 的**排队算术** · §8 的准入门措辞修订 **C-1** 及其理由 · 冻结成员数 —— 一律**不变**(算术数字的**唯一真源**为 §2 正文与台账 `obj §4.0`)。
> **关联**:台账 `obj-cep-platform §11` **Entry #91**。
## Status Update 2026-10-28 —— 队列 #7 载体交付面进度 + `module_factory.hh` 行号漂移登记

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **正文一律不改**,仅登记状态变化。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #86**

> **登记 ① —— 载体交付面 ③ / ④ 的进度**(per 本文件 2026-10-27 那则登记的「载体交付面」四项):
> ① `CohIfBundle` ✅(Entry #84)· ② `ProtocolEngineBase` ✅(Entry #85)·
> ③ `include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>`)**✅ 已落地**(Entry #86)·
> ④ `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏 ⬜ **未开始**(归队列 #7 Phase 4);`module_factory.hh` 钩子扩展 ✅(Entry #86)。

> **登记 ② —— 行号漂移**:本文件 2026-10-27 那则引用的 `module_factory.hh:313-315`(钩子扩展点)**按「ADR 不可变」不改**;Phase 3 之后该符号在 **336-343** 行(完整对照表见 `06 号` 文末「附录 · 行号对照表」,2026-10-28)。

## Status Update 2026-11-11(per 台账 Entry #338)—— 「一决策一文档化」的**已知残留** + 3 条可判定触发条件(⛔ 不是豁免)

> **性质**:末尾追加 SU(append-only)。⛔ **既有正文与既有 Status Update 段逐字未变**;⛔ **新增 invariant 数 = 0**。本 ADR 是**治理宪章**(跨仓归口 + 准入门槛的 owner)⇒ 治理面残留与触发条件由本 SU 承载(⛔ **不新建 ADR** —— 「为承载一条治理残留而新立 ADR」本身即与本条残余的治理方向相悖)。

### ① 残余登记(`ADR-CEP-01` / `-02` / `-03` 的**一决策一文档化**)

**裁决**:登记为「**已知残留**」(契约给出的第二条出路),⛔ **今日不拆**。
> ⚠️ **这不是豁免,是带触发条件的延期** —— 台账 Entry #139 的「不排期」曾因**无边界**而被补触发条件;本 SU 不重复该缺陷。

### ② 3 条触发条件(**均可客观判定**)

| # | 触发条件 | 判定方式 |
|---|---|---|
| **T1** | 某次修订 / supersede 的**取代范围**只覆盖 §2 的**单一决策块**(取代范围声明 ∩ 决策块清单 = 单元素)⇒ 触发该块的拆出 | 取代范围声明 + 决策块清单做**交集**,单元素即命中(先例:`ADR-CEP-01` D.3/D.4 的「取代范围」范式) |
| **T2** | 台账裁决(Entry 级)的**事实前提只涉及** `-01`/`-02`/`-03` 中的**某一个决策块**、且要求修改该块语义 ⇒ 触发 | Entry 的事实前提 ∩ 决策块清单 = 单元素且含「修改语义」(先例:`ADR-CEP-01` Decision D 重开) |
| **T3** | 某决策块定义的语义被新决策取代、其 **invariant 需迁移 owner** ⇒ 触发 | `test/cep_adr_parser.hh` 的 `owner_specs()` 需变更 ⇒ 即命中(实测:`ADR-CEP-02` 定义 11 条 `CEN-Ib*`,owner 前缀 `{"CEN-Ib"}` → 文件名映射) |

### ③ 触发 ⇒ 预定义执行路径 D1(**不在今日执行,但路径已定** ⇒ 触发时不需重新设计**)

新建 ADR 承载拆出决策 + 原 3 份**逐字保留**(机器行号锚零破坏)+ 各追加 SU 声明「决策 N 已由新件承接」+ `ADR-CEP-00 §3` supersede 表更新(该表在 **M1 冻结集**内 ⇒ 此步**需冻结面授权**)。

⚠️ **先例的准确边界**:**有先例**的是「SU 承接 / supersede」的**形态**(①「原文冻结 + 顶部 SU 声明 superseded + 继任真源」;②「SU 携带取代范围 + 取代后文本」);⛔ **无先例**的是「**拆出 + invariant owner 迁移**」这个**组合** —— 而 D1 可行性里**最难的恰是 owner 重映射**(`ADR-CEP-02` 的 11 条 `CEN-Ib*` 由 `owner_specs()` 绑**文件名**)。
⇒ **D1 的第一步 = 先跑通 `owner_specs()` 变更的 parser 回归**,再动 ADR 拆分 —— 顺序颠倒 ⇒ invariant owner 歧义。

### ④ ⛔ 明确不采的替代路径

- **只在架构文档建「ADR → 决策块」索引表就算达成** —— 无触发条件的豁免登记 = 已付过的学费再付一次;
- **`ADR-CEP-01`/`-03` 重签 v1.0、放弃不可变** —— 违 AGENTS.md 核心不变量;大量引用语义集体失效;
- **只拆 `-01`/`-03` 而 `-02` 不动** —— `-02` 恰是最重的一块(4 决策 + 11 invariants + 跨仓行号锚),拆容易的留最难的 = 真问题原地不动。

### ⑤ 决策块计数口径(治理不变量 —— **本段是唯一规范载体**)

**决策块** = ADR 的 §2 章下的**独立决策载体**(「主决策段」+ 各 `### 决策 N` 段);§2.0 的选项对比表是决策的**支撑物**,**不计块**。

> ⚠️ **落点说明**:本段是**规范载体**,并**取代** `07 号` 的 `Status Update 2026-11-04(#2) §②` 的载点声明(该段原写「落本文件」⇒ 现已失效,保留为历史登记);`07 号` §8.5 ③ 是**镜像指针**。⛔ **不另立 ADR**(`ADR-CEP-NUMBER-MAP` 的职权域 = 编号空间;为一个计数口径破其冻结基线 + 耦合 `test/cep_adr_parser.hh` 的 owner 解析器,收益不成比例)。
> ⚠️ **已知不一致(诚实边界)**:本口径把「主决策段」计入决策块,而 `test/cep_adr_parser.hh` 的判定正则**只数 `### 决策 N:` 标题** ⇒ 两者在 `ADR-CEP-00` 自身即给出不同结果(本口径 = 5 块(1 主决策段 + 4 个 `### 决策 N`) / **标题口径实测 = 4 条 `### 决策 N:`** / **`test/cep_adr_parser.hh` 的 `decision_head` 要求标题后跟 invariant id ⇒ 对该 ADR 计 0** ⇒ **三者互不相等**)⇒ **该差是已登记的待办**(⛔ 未修;口径的规范载体是本段,parser 的口径待单独对齐)。

> ⚠️ **本段刻意不落在 `ADR-CEP-01`/`-02`/`-03` 三份上** —— per `07 号` SU#2 §③ D0 堵法 ③:「3 份 ADR 自身的 SU **推迟到首次触发时再落**」⇒ 单读 -01/-02/-03 的读者**看不到**本残留标记。
> ⚠️ **D0 堵法 ②(机器检查)尚未落地**:断言「架构真源含 3 份 ADR 的残留登记 + 3 条触发条件」的检查**当前不存在**(`scripts/test/` 与 `test/` 内无对应载体)⇒ **残留仍可被静默删除** ⇒ 登记为**待办**(独立 change,落非冻结面)。

**关联**:台账 **Entry #338**(授权)· **#169**(拆分登记 + 计数口径)· **#139**(「不排期无边界」的先例与被补先例)· `ADR-CEP-01` D.3/D.4(取代范围范式)· `ADR-CEP-02`(4 决策 + 11 invariants)· `test/cep_adr_parser.hh`(`owner_specs()` —— owner 重映射的载体)· `07 号` §8.5(计数口径的镜像面)


## Status Update 2026-11-12(per 台账 Entry #363)—— **决策块计数口径归一**(取代 §⑤ 的「5」)+ **R-c 机器检查落地** + 三份 ADR 全名重述

> **性质**:末尾追加 SU(append-only)。⛔ **既有正文与既有 Status Update 段逐字未变**;⛔ **新增 invariant 数 = 0**;⛔ 本 SU 内**不得**出现 `### 决策 ` 标题(否则本 ADR 的 `^### 决策` 计数将由 **4** 变 **5** ⇒ 破坏 R-d 快照自洽)。
> **上游**:台账 **Entry #359**(D-1 ~ D-5)+ **Entry #361**(D-6 ~ D-9 + C3a / C3b 切片)+ `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md`(22 条 AC)· change `cpptlm-cep-invariant-gate-teeth`(design D-1 ~ D-11)。

### ① 取代声明(§⑤ 的规范地位与数值)

本 SU **继承** `§⑤`(本文件 `:426` 起)的「**唯一规范载体**」地位;`§⑤` 文本**保留为历史**,⛔ **不回改**。

⛔ **自本 SU 起作废的两处**:

1. `§⑤` 中的「本口径 = **5** 块(1 主决策段 + 4 个 `### 决策 N`)」⇒ **作废**;现行口径 = **4**(见 ②)。
2. `§⑤` 的「**已知不一致**」段(本文件 `:431`)⇒ **作废**;该差已由本 SU 归一(见 ② / ③),⛔ 不再作为「待办」挂着。

⇒ **canonical 实现宿主** = `scripts/test/cep_decision_block_check.py`(本 change 新闸)的 Python 实现;本文件 `:431` 的「标题口径 4」升格为该实现的**文档镜像**;`test/cep_adr_parser.hh` 的 `decision_head` = **invariant-id 提取器、不计数**(⛔ 不改);`scripts/test/cep_docs_sync_check.sh` 的正则 = **历史形态**(⛔ 不追认、不改)。

### ② 口径值 + 三值换算表

**口径值**:`ADR-CEP-00` 的决策块数 = **4**。

| ADR | §⑤ 原口径 | 现行口径(D-1 四步 · 标题口径) | 差异理由 |
|---|:--:|:--:|---|
| `ADR-CEP-01` | 3 | 3 | 一致 |
| `ADR-CEP-02` | **4** | **3** | 其**章级 thesis**(`ADR-CEP-02:36`)已被 `### 决策 1(精简)`(`:88`)**完整重述** ⇒ 章级标题**非独立载体**(⛔ **不是**「流表 = 支撑物」—— `§2.0 选项对比表` 才是支撑物) |
| `ADR-CEP-03` | 3 | 3 | 一致 |

**第三个值(⛔ 不构成口径)**:本文件 `:30` 的 H2 标题「决策(锁定 **3** 件事)」= **历史措辞** ⇒ ⛔ **不得**作为任一计数来源。

**取数口径(D-1 · 顺序不可换)**:① **先剥** ` ``` ` 围栏代码块 → ② **再取** §2 章范围 → ③ **仅** `###` 级 → ④ 匹配 `^###\s*决策\s*\d+(?:\.\d+)?`。**块 ID = 正则全匹配的编号串**(⛔ 禁前缀 / 子串 / 整数归一)。

### ③ 机器判据(本 SU 的落地载体)

| 段 | 闸 | 判据 | 期望值来源 |
|---|---|---|---|
| **R-d** | `scripts/test/cep_decision_block_check.py` | 22 份 ADR 的 §2 决策块计数**逐份** == 快照(`test/cep_decision_block_snapshot.json` · 22 键 + `total`=125)+ 两条显式红 + 围栏危险标记 | **闸外快照**(⛔ 禁从被检对象自身派生) |
| **R-c** | 同上 | 双向对账:正向(三份精确 basename + **无同号拆出件**)+ 反向(本 SU 的 `### ⑥` 重述块)+ 触发条件表的可解析性 | **闸内源码常量**(⛔ 禁从 `NUMBER-MAP` 派生) |

⇒ `§⑤` `:434` 自认的「**D0 堵法 ②(机器检查)尚未落地**…**残留仍可被静默删除**」⇒ **自本 SU 起已落地**(落地载体 = 上表两段 + `scripts/test/cep_decision_block_check.sh` 的决定性变异)。

### ④ D1 路径的第 5 步(合法拆分 ⇒ 闸红 ⇒ 更新期望值 = 强制函数)

`§⑤` `:415` 的 D1 执行路径原列 **4** 个动作;**第 5 步 = 同步更新 R-c 闸的期望值常量**(`ADR-CEP-01` / `ADR-CEP-02` / `ADR-CEP-03` 三份精确 basename + `### ⑥` 重述块的 3 个完整 token)。
⇒ 触发 T1 / T2 / T3 时的红是**正确的红**(同 `cep_adr_structural_check.sh` 的 registry 先例)⇒ **更新期望值 = 强制函数**,⛔ 不得以「weaken 判据」消红。

### ⑤ 触发条件表(3 条 · 逐条可解析)

⚠️ 本段**只引用**,⛔ **不改** SU 2026-11-11 `### ②` 的原表(append-only):**T1 / T2 / T3** 三条触发条件与其「判定方式」列**原样有效**;其可解析性由 R-c 闸按 **4 类**(仓内路径 / `Entry #N` / ADR 短名 / 符号)校验。

⚠️ **下一个 `### ⑥` 块的格式已冻结**(per change `cpptlm-cep-invariant-gate-teeth` 的 design **D-5.3**):**恰 3 条 bullet**、每条**唯一一个完整编号 token** ⇒ R-c 闸的**反向对账**按此定界取块 ⇒ ⛔ 改本块格式 = 改闸的判据面 ⇒ 须同批改闸 + 走授权。

### ⑥ 三份 ADR 全名重述

- `ADR-CEP-01` —— 承载 Coherence Interface 抽象事务流清单的「一决策一文档化」残余登记
- `ADR-CEP-02` —— 承载 barrier 流语义澄清的「一决策一文档化」残余登记
- `ADR-CEP-03` —— 承载 fabric collective plane 内部件的「一决策一文档化」残余登记

**关联**:台账 **Entry #363**(C3 实施开工授权 + `--authorized-by` 依据)· **#364**(C3a:R-d 闸 + 快照 + helper)· **#361**(D-6 ~ D-9 + 切片)· **#359**(D-1 ~ D-5)· **#338**(R-c 授权 + SU 2026-11-11)· **#169**(拆分登记 + 计数口径)· change `cpptlm-cep-invariant-gate-teeth`(design D-1 ~ D-11 + 契约 delta `specs/cep-invariant-gate/spec.md`)· `scripts/test/cep_decision_block_check.{sh,py}` · `test/cep_decision_block_snapshot.json` · `scripts/test/cep_test_shadow.sh`
