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
  - **Stage 1**: 7 个串行 `cpptlm-cep-poc-spike` / `cpptlm-cep-stub-cache-core` / `-directory-ordering-plane` / `-collective-plane-base` / `-chi-rnf-subset` / `-cxl-nonsymmetric` / `-tilelink-tree-manager`(每次 1 KPI slot,KPI ≤3 → 排 7 季度)
  - **Stage 2**: 2 个串行 `-cxl-dcoh-symmetric` / `-nvlink-sharp`(KPI 排队同上)
  - **Stage 3**: 1 个 `-ualink-inc`
  - **总计**:11 个 change(1 + 7 + 2 + 1 = 11),串行 11 季度;**任一时刻 Proposed 并发 ≤ 1**(守 KPI 上限)

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
| KPI 排队 | 11 个 change 串行 11 季度(per §2 决策 3;2026-10-22 per Entry #12 由 10 → 11) | ✅ 已拍板 |
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
> **触发**: ArchForge 侧答复 `ADR-SOC-25` D11(治理级协商请求)+ `32 号 §2 F8`。
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

## Status Update 2026-10-23 —— `cpptlm-cep-umbrella` 已完成归档(§8 第 5 条失效)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§8 正文不改**,仅登记状态变化。
> **触发**: 2026-10-23 `cpptlm-cep-umbrella`(Stage 0 umbrella)**完成归档** —— tasks **99/99** 全勾;delta spec 已合并进 `openspec/specs/cep-core-contracts/spec.md`;R10 已按 `obj-cep-platform §9.3` 的闭合判据处置(残余风险已登记 + DRI 书面接受,per 台账 Entry #28)。归档与路径迁移见台账 Entry #29。
> **§8 第 5 条**「`cpptlm-cep-umbrella` 维持 **🟡 Proposed Draft**,Stage 0 **不 archive**」—— **已失效**:
> - 该 change 现为 **✅ archived**(归档目录 `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/`);
> - **KPI slot 已释放** —— 本段登记时点(2026-10-23)Proposed 并发 **0/3**(实测 `openspec list --json`),下一个可启动 change = 队列 #2 `cpptlm-cep-poc-spike`(per `obj §4.0`)。**该状态已于 2026-10-24 变化 → 见下段 Status Update**。
> **不受影响的部分**:§8 其余条款(准入门措辞修订 **C-1** 及其理由)**继续有效** —— C-1 已于 2026-10-21 闭合(ArchForge `6020f42`)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #33**
> **冻结面**:本文件属 M1 冻结面 **30 成员**,本次改动已走授权闸 `cep_freeze_check.sh --update --authorized-by "Entry #33"`。

## Status Update 2026-10-24 —— 队列 #2 `cpptlm-cep-poc-spike` 已启动(KPI 并发 0/3 → 1/3)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文一律不改**,仅登记状态变化。
> **触发**: 2026-10-24 队列 #2 `cpptlm-cep-poc-spike` **已建 change 目录**(`openspec/changes/cpptlm-cep-poc-spike/`,4 件套齐全 + delta capability `cep-poc-spike-verification`),并占用 1 个 KPI slot → **并发 0/3 → 1/3**。
> **对上一段(2026-10-23)的衔接**:该段登记「KPI slot 已释放 → 并发 0/3 → 下一个可启动 = 队列 #2」—— **队列 #2 现已启动**,该预测已兑现;其**历史快照语义保留不回改**。
> **当时态(历史快照,已被下一段取代)**:Proposed 并发 **1/3**(实测 `openspec list --json`);下一个可启动 change = 队列 #3 `cpptlm-cep-stub-cache-core`(per `obj §4.0`)。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #42**
> **冻结面**:本文件属 M1 冻结面 **30 成员**,本次改动已走授权闸 `cep_freeze_check.sh --update --authorized-by "Entry #42"`。

## Status Update 2026-10-24 —— 队列 #2 `cpptlm-cep-poc-spike` 已完成归档(KPI 并发 1/3 → 0/3)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **触发**: 2026-10-24 队列 #2 `cpptlm-cep-poc-spike` **已完成归档**(`openspec archive`,归档名 `2026-10-25-cpptlm-cep-poc-spike`)—— delta capability `cep-poc-spike-verification`(**15 Scenario**)已合并进 `openspec/specs/cep-poc-spike-verification/spec.md`,KPI slot 释放 → **并发 1/3 → 0/3**。
> **对上一段(2026-10-24)的衔接**:该段登记「队列 #2 已启动,并发 0/3 → 1/3」—— **现该 change 已归档**,slot 释放;其**历史快照语义保留不回改**。
> **前置门**:归档前经 **critic 7 轮复审** —— 前 6 轮 REJECT(逐轮揭示「有牙」的六类缺口:判别力 / 真实性 / 敏感度 / 等价性 / 夹具自守 / 覆盖承诺+正控),每轮均修复并以 mutant 逐条反证;**第 7 轮 APPROVE**(per `obj §11` Entry #49~#54)。
> **当时态(历史快照,已被下一段取代)**:Proposed 并发 **0/3**(实测 `openspec list --json`);下一个可启动 change = 队列 #3 `cpptlm-cep-stub-cache-core`(per `obj §4.0`;并承接 **M-4 行为侧决策**)。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #55**
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #55)。
> **owner 重指向(per Entry #55)**:S1-1 ①定义层 = ✅已消费;**②建图层 → 队列 #4**;**③放行闸 → 全局不变量**(不挂队列行);**M-4 决策 → 队列 #3**;**S1-5 表体 → 队列 #6**;S1-2/3/4 → 队列 #3+。

## Status Update 2026-10-25 —— 队列 #3 `cpptlm-cep-stub-cache-core` 已启动(KPI 并发 0/3 → 1/3)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **触发**: 2026-10-25 队列 #3 `cpptlm-cep-stub-cache-core` **已建 change 目录**(`openspec/changes/cpptlm-cep-stub-cache-core/`)—— planner 阶段产出 `proposal.md` + `design.md` + **2 个** delta capability(`cep-vc-mapping-contract` 8 Scenario / `cep-stub-cache-core` 9 Scenario);**双 Phase 单 change**(Phase 0 = M-4 决策落盘 + 契约化 / Phase 1 = stub cache core 实现);并占用 1 个 KPI slot。
> **对上一段(2026-10-24)的衔接**:该段登记「队列 #2 已完成归档,并发 1/3 → 0/3」—— **队列 #3 现已启动**,该预测已兑现;其**历史快照语义保留不回改**。
> **M-4 行为侧决策已落**(承接自队列 #2 归档时的 owner 重指向):协议层 credit **不独占** VC(该问题**已**被 ArchForge `32 号 §6.0.1` 排除表关闭,本仓**不重新裁决**)· 传输层**保持索引对齐**、**不建**显式映射表(显式映射表归队列 #6)· `VC_DAT` 语义正名(**预留 DAT 桶**,当前恒 0)· 冲突输入(`vc_id` vs `flit_category`)**传输层不仲裁**;**`include/tlm/cep/fabric.schema.json` 的 `vc_count.maximum=4` 不需要动** —— 本次冻结面改动仅**本文件**与 `ADR-CEP-05` 的 Status Update 追加段。
> **当时态(历史快照,已被下一段取代)**:Proposed 并发 **1/3**(实测 `openspec list --json`);下一个可启动 change = 队列 #4 `cpptlm-cep-directory-ordering-plane`(per `obj §4.0`)。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived);**§2 Decision 3 的「11 个 change,串行 11 季度」算术不变** —— 队列 #3 为**单一** change(`ADR-CEP-05 §10` 的 `-base`/`-impl` 二分作废,见该 ADR §14.1)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #58**
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸 `cep_freeze_check.sh --update --authorized-by "Entry #58"`。

## Status Update 2026-10-25 —— 队列 #3 `cpptlm-cep-stub-cache-core` 已完成归档(KPI 并发 1/3 → 0/3)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文与上方各段一律不改**,仅登记状态变化。
> **触发**: 2026-10-25 队列 #3 `cpptlm-cep-stub-cache-core` **已完成归档**(`openspec archive`,归档名 `2026-10-25-cpptlm-cep-stub-cache-core`)—— **完成归档**语义:tasks **36/36** 全勾;delta capability `cep-vc-mapping-contract`(**8 Scenario**)与 `cep-stub-cache-core`(**9 Scenario**)**已合并**进 `openspec/specs/`(两个新 main spec);归档后 KPI slot 释放 → 并发 **1/3 → 0/3**。
> **对上一段(2026-10-25)的衔接**:该段登记「队列 #3 已启动,并发 0/3 → 1/3」—— **现该 change 已归档**,slot 释放;其**历史快照语义保留不回改**(措辞改为「**当时态**」)。
> **M-4 行为侧决策的最终落点**:协议层 credit **不独占** VC(该问题**已**被 ArchForge `32 号 §6.0.1` 排除表关闭,本仓**不重新裁决**)· 传输层**保持索引对齐**、**不建**显式映射表(显式映射表归队列 #6)· `VC_DAT` 语义正名(**预留 DAT 桶**,当前恒 0)· 冲突输入(`vc_id` vs `flit_category`)**传输层不仲裁**;**`include/tlm/cep/fabric.schema.json` 的 `vc_count.maximum=4` 不需要动** —— 立项时「须同步」的假定经实测**不成立**。
> **当前态**:Proposed 并发 **0/3**(实测 `openspec list --json`);下一个可启动 change = 队列 #4 `cpptlm-cep-directory-ordering-plane`(per `obj §4.0`)。
> **不受影响的部分**:§8 准入门措辞修订 **C-1** 及其理由**继续有效**(2026-10-21 已闭合);§8 第 5 条**仍为失效**(umbrella 保持 ✅ archived);**§2 Decision 3 的「11 个 change,串行 11 季度」算术不变** —— 队列 #3 为**单一** change(`ADR-CEP-05 §10` 的 `-base`/`-impl` 二分作废,见该 ADR §14.1)。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #60**
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #60)。

---

**版本**: v0.2 草案 + Status Update 2026-10-21 + **Status Update 2026-10-23** + **Status Update 2026-10-24** + **Status Update 2026-10-25**(队列 #3 启动)+ **Status Update 2026-10-25**(队列 #3 归档)
**最后更新**: 2026-10-25
**批准状态**: 🟡 待 Oracle 复审(§5.1 签发仓 + §5.2 真源路径已拍板;§8 Status Update 经 DRI 明示措辞拍板)
**维护者**: CppTLM build agent(architect mode)
