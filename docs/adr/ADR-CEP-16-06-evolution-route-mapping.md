# ADR-CEP-16: 06 §15.4 Coherence 演进路线 → CEP Stage 映射

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **`06 §15.4` 的 P2.0~P5 演进路线与 CEP Stage 0~3 的逐项对应 + 显式排除 P6**
> **关联真源**:
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4 §15.4(Coherence 演进推进路径)
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §8.2(06-bis P2.0-P5 → CEP 承接映射)+ §8.3(演进 seam)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md` §2 Decision 3(P 编号口径冻结)
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md` §3(supersede 表 + IOMMU scope reduction)
> - 真源 3:`ADR-CEP-13-coherence-domain-ownership.md`(跨域组合承诺边界)
>
> **一手锚点**:
> - `docs/architecture/06-inter-module-network.md` §15.4 表(逐行):P2.0 / P2.1 / P3 / P3.5 / P4 / P5 —— **无 P6**
> - `docs/architecture/06-inter-module-network.md` §11.4 的另一张表**含 P6** → **以 §15.4 为准**(per `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结)

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

## 7. 修订记录

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | `06 v0.4 §15.4` 逐行 + `ADR-CEP-NUMBER-MAP Decision 3` 冻结口径 + `ADR-CEP-00 §3` | 逐项映射 + 处置词 + 归属 + P6 排除 |

---

**版本**: v0.1 草案
**最后更新**: 2026-10-21
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
