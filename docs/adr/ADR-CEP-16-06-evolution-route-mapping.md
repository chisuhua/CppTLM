# ADR-CEP-16: 06 §15.4 Coherence 演进路线 → CEP Stage 映射

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **`06 §15.4` 的 P2.0~P5 演进路线与 CEP Stage 0~3 的逐项对应 + 显式排除 P6**
> **关联真源**:
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4 §15.4(Coherence 演进推进路径)
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §8.2(06-bis P2.0-P5 → CEP 承接映射)+ §8.3(演进 seam)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
>
> **一手锚点**:
> - `docs/architecture/06-inter-module-network.md` §15.4 表(逐行):P2.0 / P2.1 / P3 / P3.5 / P4 / P5 —— **无 P6**
> - `docs/architecture/06-inter-module-network.md` §11.4 的另一张表**含 P6** → **以 §15.4 为准**,**不引用** P6(per `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结)

---

## 1. 背景

### 1.1 问题

`06 v0.4 §15.4` 给出 Coherence 演进的 6 个阶段(P2.0 ~ P5,含估时),与 06-bis v0.3 §7 一致。CEP 承接了这部分 scope,但**从未逐项说明**:

| 待答 | 后果 |
|------|------|
| 每一项被 CEP **承接 / supersede / 改判**? | 无法判断 06-bis 的哪些设想仍然有效 |
| **P6** 为何不引入? | 06 §11.4 与 §15.4 口径冲突 → 引用歧义 |
| 跨 vendor coherence / IOMMU 归谁? | 边界不清 → 与 ArchForge 争抢 |

### 1.2 口径冻结(先决)

`ADR-CEP-NUMBER-MAP §2 Decision 3` 已冻结:
> **`06 §15.4` 编号口径(P2.0 / P2.1 / P3 / P3.5 / P4 / P5,无 P6)为 CEP 系列 ADR 的唯一演化口径**

本 ADR 是该冻结的**逐项落地**。

---

## 2. 决策

### 决策 1:逐项映射表

| 06 §15.4 | 内容(原文增量) | 估时 | CEP 处置 | 落点 |
|----------|---------------|:----:|---------|------|
| **P2.0** | `CoherenceProtocol` C++ 实现 + 类型改写(string→ch_uint<8>)+ 3 Bundle 字段定义 | 30-45d | **supersede** | CEP 用 **ProtocolEngine 插件** + **canonical 10 流** 的 Bundle(per `ADR-CEP-02` / `07 号 §5.4`)。**不做**单一 `CoherenceProtocol` 类 —— 那与"多协议可插拔"根本矛盾 |
| **P2.1** | `CoherenceBridge` C++ 实施(走 programmatic bridge 路径)+ parser stub 替换 + 集成测试 | 30-45d | **supersede** | 跨域组合由 `ADR-CEP-13` 承接,但走**人工声明 + 实验证伪**路线(per `ADR-CEP-13` 决策 5),**不做** programmatic bridge 的 C++ 实施。`coherence_domains` stub 的处置见 `ADR-CEP-07A` |
| **P3** | `SnoopFilter` directory-based + `MesiGpuProtocol` + `MesiThreeLevelProtocol` + `translate_snoop` 协议转换 | 30-45d | **部分承接** | **SharerTracker 5 实现**承接 SnoopFilter 的角色(per `ADR-CEP-04`)。但 `MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop` 属**协议语义建模** → **归 ArchForge**(per AGENTS.md:423 边界条款);CEP 只提供 **adapter 载体** + 能力声明 schema |
| **P3.5** | JSON 端口路径打通(扩 Step 7a registerCoherenceAdapter 候选 C)+ Python 工厂落地 | 30-45d | **改判** | JSON 路径 → CEP 的 capability schema(JSON,per `ADR-CEP-12`)+ `coherence_domains` 字段处置(per `ADR-CEP-07A`);Python 工厂 → CEP 的 **`tools/` 3 脚本**(per `ADR-CEP-06 §4`),**不做** `cpptlm.library` 侧的 bridge 工厂 |
| **P4** | CHI 完整协议支持(REQ/SNP/RSP/DAT 4 vnet 借鉴 Gem5 mainline) | 30-45d | **改判 + 拆分** | Stage 1.4 `cpptlm-cep-chi-rnf-subset` 只做 **RN-F subset**(6 通道的受限子集,per `07 号 §5.1.1`);**完整 CHI** → Stage 4+ 维护态 |
| **P5** | IOMMU 集成 + 跨 vendor coherence(NVIDIA USRI vs AMD Infinity Fabric) | 60-90d | **明确排除** | IOMMU → 已由 `ADR-CEP-00 §3` 的 **scope reduction** 排除(Stage 5 独立 ADR);跨 vendor coherence → **归 ArchForge**(协议语义建模) |
| **P6** | (仅 06 §11.4 有:CHI 完整协议 + `DtifCacheTLM` CHI-aware cache,90+d) | — | **不引入** | per `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结:**以 §15.4 为准**。§11.4 的 P6 口径**不得**被 CEP 文档引用。完整 CHI 的需求已由 **P4 的拆分**(Stage 1.4 subset + Stage 4+ 完整)覆盖,无需单列 P6 |

### 决策 2:处置词的严格含义

| 处置词 | 含义 | 是否需 KPI slot |
|--------|------|----------------|
| **supersede** | 06-bis 的该项设想**由 CEP 的另一机制取代**,该项**不再单独实施** | 否(被 CEP 的既有 Stage 吸收) |
| **部分承接** | CEP 承接其中**框架层**的部分,**协议语义部分归 ArchForge** | 否(框架层进既有 Stage) |
| **改判** | 目标保留,但**实现形态 / 归属**变了 | 否 |
| **改判 + 拆分** | 目标保留,但**拆成两阶段**(subset 先做,完整后做) | 是(完整部分进 Stage 4+) |
| **明确排除** | 不做 | 否 |

### 决策 3:协议语义建模统一归 ArchForge

**决策**:P3 的 `MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop` 三项**明确归 ArchForge**;CEP 侧**只**提供:
- adapter 载体(ProtocolEngine 插件)
- 能力声明 schema(per `ADR-CEP-12`)
- 实验床(per `ADR-CEP-06`)

**理由**:AGENTS.md:423 的边界条款"SoC 架构组件/拓扑/演进类 → ArchForge";且 `ADR-CEP-00 §2 Decision 4` 已就边界豁免做过论证 —— **框架层归 CppTLM,协议语义归 ArchForge**。

### 决策 4:`P6` 口径的显式排除

**决策**:CEP 系列的任何文档**不得**引用 `06 §11.4` 的 P6 口径。

**理由**:`06 §11.4` 的表与 `§15.4` 的表**不一致**(前者含 P6,后者不含),`ADR-CEP-NUMBER-MAP §2 Decision 3` 已冻结以 §15.4 为准。**引用歧义是漂移源,必须显式禁止。**

**落点**:`scripts/test/cep_docs_sync_check.sh` 的 P 编号检查(per `ADR-CEP-NUMBER-MAP §2` 抓取点 3)。

### 决策 5:显式排除

| 排除项 | 理由 |
|--------|------|
| **不**实现 06 §11.4 的 P6 口径 | 决策 4 |
| **不**实现单一 `CoherenceProtocol` C++ 类 | 决策 1 的 P2.0 行 |
| **不**实现 programmatic bridge 的 C++ 路径 | 决策 1 的 P2.1 行;CEP 走人工声明路线 |
| **不**在 `cpptlm.library` 侧加 bridge 工厂 | 决策 1 的 P3.5 行 |
| **不**在 CppTLM 仓实现跨 vendor coherence 语义 | 决策 3 |
| **不**做 IOMMU 集成 | 已由 `ADR-CEP-00 §3` scope reduction 排除 |

---

## 3. 不变量

- **INV-CEP-16-IC1**:CEP 文档引用的 06 P 编号**必须**落在 `06 §15.4` 的口径内(P2.0 / P2.1 / P3 / P3.5 / P4 / P5);**不得**出现 P6
- **INV-CEP-16-IC2**:CEP 的每个 Stage **必须**能指出其承接的 06 §15.4 条目(或显式声明"不承接")
- **INV-CEP-16-IC3**:`06 §15.4` 的**协议语义建模项**(P3 的 `MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop`)**必须**由 ArchForge 承接;CppTLM 侧**不得**出现其实现

> **编号位说明**:`INV-CEP-16-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **P 编号口径**:grep CEP 文档的 `P[0-9.]+`,命中**必须**在 §15.4 集合内;命中 P6 = fail(per `INV-CEP-16-IC1`)
2. **Stage 覆盖**:每个 Stage 至少有一条 §15.4 承接声明(per `INV-CEP-16-IC2`)
3. **语义归属**:`MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop` 在 CppTLM 仓**不得**有实现(per `INV-CEP-16-IC3`)

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-NUMBER-MAP` | ✅ v0.1 | Decision 3 的 P 口径冻结**由本 ADR 逐项落地** |
| `ADR-CEP-00` | ✅ v0.6 | §3 supersede 表 + IOMMU scope reduction |
| `ADR-CEP-04` | ✅ v0.6 | P3 的 SnoopFilter 角色 → SharerTracker 5 实现 |
| `ADR-CEP-06` | ✅ v0.3 | P3.5 的 Python 工厂 → `tools/` 3 脚本 |
| `ADR-CEP-07A` | ✅ v0.1 | P2.1 的 parser stub 替换 → 06-bis 产物处置 |
| `ADR-CEP-12` | ✅ v0.1 | P3.5 的 JSON 路径 → capability schema |
| `ADR-CEP-13` | ✅ v0.1 | P2.1 的跨域组合 → 承诺边界(人工声明 + 实验证伪) |
| `ADR-CEP-16` | ✅ v0.1 | 本 ADR |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 逐项映射 | P2.0/P2.1 supersede;P3 部分承接;P3.5/P4 改判;P5 排除;P6 不引入 | ✅ 已拍板 |
| 处置词语义 | supersede / 部分承接 / 改判 / 改判+拆分 / 明确排除 | ✅ 已拍板 |
| 协议语义归 ArchForge | P3 三项归 ArchForge;CEP 只提供载体 + schema + 实验床 | ✅ 已拍板 |
| P6 显式排除 | CEP 文档禁引 §11.4 口径 | ✅ 已拍板 |

---


## Status Update 2026-10-27 —— §2 决策 1 的 P2.0 行「canonical 10 流的 Bundle」是**悬空引用** + P2.1 行与 `ADR-CEP-19` 的边界澄清

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记状态变化与边界澄清。
> **触发**: 2026-10-27 architect 角色架构重审(per 用户指令「切 architect 重审队列 #6 是否对齐 `01 号` 核心立场」)⇒ 新立 `ADR-CEP-19`(CohIf 载体)+ 队列 #7 `cpptlm-cep-cohif-carrier`;台账 Entry #74。
> **登记 ① —— §2 决策 1 的 P2.0 行(本表第 3 行)**:该行写「CEP 用 **ProtocolEngine 插件** + **canonical 10 流** 的 Bundle(per `ADR-CEP-02` / `07 号 §5.4`)」。**该替代物在本仓不存在**:实测 `ADR-CEP-02` 全文 `Bundle` **零命中**;`07 号 §5.4` 的业务 Bundle 列表(`CacheReqBundle` / `CacheRespBundle` / `GmmuPteBundle` / `SdmaCmdBundle` / `DoorbellBundle` / `CrBundle` / `MsixBundle`,共 7 项)**无它**;`include/bundles/` 17 个 `.hh` grep `coherence` **零命中**。⇒ 本行是**悬空引用**;与 `ADR-CEP-07A` 决策 1 第 3 项的同名表述**同源**(两处)。**处置**:**不改本行正文**(append-only),改为在此登记 + 由 `ADR-CEP-19` 决策 6 给出**实体路径** `include/bundles/cep_coherence_bundles_tlm.hh`(回避 `ADR-CEP-07A` 明令「不创建」的 `coherence_bundles_tlm.hh`)。
> **登记 ② —— §2 决策 1 的 P2.1 行与 §5 决策 5 排除表**:二者写「**不**实现 programmatic bridge 的 C++ 路径」。**与 `ADR-CEP-19` 不冲突**,边界澄清如下(逐 `file:line` 实测):
> - 本文件排除的 programmatic bridge 指的是 `06 号 §6.3` 用法 **C**(`set_*_target` 装配,**跨域桥**用途)—— 该用法在 `06 号 §6.3` 的分类里与用法 A(flat + name_prefix)/ B(SimModule 嵌套 JSON)并列;
> - `ADR-CEP-19` 决策 5 选的是**用法 A/B 族**的 **JSON 端口注册路径**(与 `RouterTLM` 同款:`chstream_register.hh:119` 的 `registerBidirectionalPortAdapter` + `module_factory.cc:743-750` 的 `is_multi` 分支**泛型虚分派** `bind_port_pair`)⇒ **不是** `set_*_target` 式 programmatic bridge;
> - ⇒ **两者对象不同,无冲突**。登记此点是为避免后续读者把「JSON 端口注册」误读成「programmatic bridge」而误判冲突。
> **登记 ③ —— 装配能力的边界**:仓内**无**「programmatic 创建 ChStream 端口」的先例(实测 `grep -rn "new cpptlm::ChStreamInitiatorPort\|new cpptlm::ChStreamTargetPort" src/ include/ | grep -v module_factory` → **空**;ChStream 端口只在 `module_factory.cc` Step 7 内 new)⇒ 即便本文件的排除条款不存在,programmatic 路径也缺 API 充分性证据。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #74**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #74)。

## 7. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
