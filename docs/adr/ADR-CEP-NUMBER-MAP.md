# ADR-CEP-NUMBER-MAP: Coherence Exploration Platform 编号预分配

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP-08 ~ CEP-16 编号预分配 + 议题映射**,解 metis G5 隐式依赖环
> **关联真源**:
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
> - 真源 3:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射)
> - 真源 3:`ADR-CEP-06-experiment-protocol.md`(实验输出)
> - 真源 3:`ADR-CEP-07-simulation-and-integration-semantics.md`(仿真语义)
> - 真源 3:`ADR-CEP-07A-06bis-artifact-disposition.md`(06-bis 产物处置,后续起草)

---

## 1. 背景

### 1.1 metis G5 隐式依赖环

6 份 v0.2 ADR + 第 3 回合 9 议题中存在隐式依赖环:

```
A14 → 06 §15.4 P2.0-P5(per `ADR-CEP-00A §3.1` 实证 — **无 P6**)→ CEP-00 §3 supersede 表 → CEP-04 / CEP-02 → A5/A11
↑                                                            ↓
└────────────────────────────────────────────────────────────┘
```

**后果**:任何一方调整,环内其他方被迫反复修订。

### 1.2 编号冲突风险(per metis G3)

- `CEP-13` 既是 ADR 编号,**也是**不变量 id(`ADR-CEP-03 §3.2` 不变量 CEP-13)
- `CEP-NN` 前缀双语义,后续引用易混淆

### 1.3 本 ADR 的定位

本 ADR **冻结** CEP-08 ~ CEP-16 编号 + 议题映射,**打破隐式依赖环**;同时**显式**约束"不变量 id ≠ ADR 编号"以解 G3 编号冲突。

---

## 2. 决策

### 决策 1:CEP-NN 编号预分配表(冻结,解 G5)

| 编号 | 类型 | 议题 | 依赖 | v0.3 状态 |
|------|------|------|------|----------|
| **CEP-00** | ADR | 跨仓治理 + KPI 排队 + 边界条款豁免 | — | ✅ v0.3 |
| **CEP-00A** | ADR | 真源章节映射 | CEP-00 | ✅ v0.1 |
| **CEP-01** | ADR | Fabric Collective 旁路平面 | — | ✅ v0.3 |
| **CEP-02** | ADR | canonical 10 流清单 + barrier | CEP-00A | ✅ v0.3 |
| **CEP-03** | ADR | Fabric Collective 内部 + non-cacheable | CEP-01, CEP-02 | ✅ v0.3 |
| **CEP-04** | ADR | Sharer Tracker D-5 重构 | CEP-02 | ✅ v0.3 |
| **CEP-05** | ADR | Stub Cache RN 投影 + 折叠表 | CEP-02 | ✅ v0.3 |
| **CEP-06** | ADR | 实验输出协议 | — | ✅ v0.1 |
| **CEP-07** | ADR | 仿真语义 + 集成 seam | — | ✅ v0.1 |
| **CEP-07A** | ADR | 06-bis 既有产物处置(7 项逐项 + 协议泄漏通用禁令) | CEP-12, CEP-13, CEP-14 | ✅ **v0.1 起草 2026-10-21** |
| **CEP-08** | ADR | Ordering Point 可迁移性规则(A3)| CEP-04 | ✅ **v0.1 起草 2026-10-21** |
| **CEP-09** | ADR | TileLink-C Tree Tip 映射 + 多 VC 仲裁 **+ VC 分配的死锁自由性论证**(A5,含 Decision A 无环判据 / Decision B 仲裁策略)| CEP-01, CEP-04, **CEP-12** | ✅ **v0.1 起草 2026-10-21**(依赖表已补 CEP-12)|
| **CEP-10** | ADR | 可观测性归属(A8)| CEP-06, CEP-07 | 🟡 待起草 |
| **CEP-11** | ADR | 协议版本锁定(规范版本锁定表 = 唯一真源)(A9)| CEP-12, CEP-00 | ✅ **v0.1 起草 2026-10-21** |
| **CEP-12** | ADR | ProtocolEngine 插件能力映射表 schema(A10)| CEP-04, CEP-08, CEP-09, CEP-11 | ✅ **v0.1 起草 2026-10-21**(已采纳 `ADR-CEP-17 §3.4` 草案)|
| **CEP-13** | ADR | 一致性域归属(A11)| CEP-02, CEP-04 | ✅ **v0.1 起草 2026-10-21** |
| **CEP-14** | ADR | Stub 可验证性合同测试(C1~C10 + 负向断言 + 组合接缝)(A12)| CEP-05, CEP-07, CEP-08 | ✅ **v0.1 起草 2026-10-21** |
| **CEP-15** | ADR | CppTLM CacheTLM 对齐边界(共存不替换)(A13)| CEP-14, CEP-05, CEP-07A | ✅ **v0.1 起草 2026-10-21** |
| **CEP-16** | ADR | `06 §15.4` P2.0-P5 演进对应(P6 排除)(A14)| CEP-00, CEP-04, CEP-13 | ✅ **v0.1 起草 2026-10-21** |
| **CEP-17** | ADR | Ruby-SLICC 协议族接入 + 协议私有状态边界 + 学术协议族 + 组合承诺边界 | CEP-00, CEP-02, CEP-04, CEP-05 | ✅ **v0.3 起草 2026-10-21**(超出预分配;§3.4 schema 草案已转由 `ADR-CEP-12` 承接)|
| **CEP-18** | ADR | 协议生成路线范围(SSP 描述 → 插件生成 vs 纯手写插件)| CEP-12, CEP-17 | ✅ **v0.1 起草 2026-10-21** |

**CEP-NUM(本 ADR)**:本表本身,作为"编号映射单一真源"

**说明**:
- **CEP-07A** 是 metis G6.8 建议独立起草的 ADR(06-bis 既有产物处置),本表显式纳入
- **CEP-09** 是 metis G3 提到的"多 VC 仲裁"承诺,合并 A5(TileLink 映射)以减少 ADR 数
- **CEP-13** 命名冲突见 Decision 2
- **去重(2026-10-21)**:本表曾对 CEP-07A / 11 / 14 / 15 / 16 各登记 **2 行**(旧的「🟡 待起草」预分配行 + 后续起草时补的「✅ 起草」行,后者未删前者)。已折叠为每编号**恰好 1 行**:保留新行(描述更完整、状态更准确),把旧行独有的 gap 编号(A9/A12/A13/A14)并入描述,依赖列取**并集**。机器检查见 `test/test_cep_contracts_crosscheck.cc`「编号表是唯一真源且无重号」。

### 决策 2:不变量 id 与 ADR 编号命名空间分离(解 G3)

**冲突**:`CEP-13` 既是 ADR 编号(本表),又是不变量 id(`ADR-CEP-03 §3.2` 不变量 `CEP-13` — non-cacheable 降级契约)。

**解决**:
- **ADR 编号**:`CEP-NN`(per Decision 1)
- **不变量 id**:沿用 ADR 内部命名(`CEN-Ib` / `INV-SC` / `CEP-04-IC1` / `CEP-13` / `CEP-06-IC1` / `CEP-07-IC1` 等),**不强制全局唯一**
- **跨 ADR 不变量引用**:统一加 ADR 前缀(如 `INV-CEP-04-IC1`, `INV-CEP-06-IC1`),**避免 `CEP-13-NC`** 之类拼接命名

**实施**:
- 后续 ADR 不变量 id **必须** 加 ADR 前缀(如 `INV-CEP-NN-XX` 或 `CEP-NN-ICX`)
- 现有 `CEP-13` 不变量 id 保留(向后兼容),在 ADR-CEP-13 起草时显式重命名为 `INV-CEP-13-NC`
- 修订记录(后续 ADR)显式标注"不变量 id 沿用/重命名"

### 决策 3:打破 G5 隐式依赖环 — A14 编号映射先冻结为常量

**关键**:A14(CEP-16)依赖 06 §15.4 P2.0-P5 编号,**无 P6**(per oracle v0.3 B3-7 实证 — 06 §15.4 实为 P2.0/P2.1/P3/P3.5/P4/P5;06 §11.4 主 roadmap 表的 P6 与 §15.4 编号不一致,以 §15.4 为准)。

**冻结**:`06 §15.4` 编号口径(P2.0/P2.1/P3/P3.5/P4/P5,**无 P6**)为 CEP 系列 ADR 的**唯一演化口径**:
- `ADR-CEP-00 §3 supersede 表`已用此口径
- `ADR-CEP-04 §1.1 / §6` 已用此口径
- 后续 ADR 一律用此口径
- 06 §11.4 P6(CHI 完整协议)不引入,**避免歧义**

**实施**:本表 Decision 1 的依赖列已基于此冻结口径;A14(CEP-16)起草时直接沿用,不需要回头修 CEP-00 §3。

### 决策 4:起草顺序(打破依赖环)

按依赖拓扑序(per Decision 1 表的"依赖"列):

```
第一批(无依赖或弱依赖):CEP-06 / CEP-07 / CEP-00 / CEP-00A
第二批(强依赖第一批):CEP-01 / CEP-02 / CEP-03 / CEP-04 / CEP-05
第三批(强依赖第一批+第二批):CEP-07A / CEP-08 / CEP-09 / CEP-10 / CEP-11
第四批(综合):CEP-12 / CEP-13 / CEP-14 / CEP-15 / CEP-16
```

**每批内部可并行起草**(互不依赖),批与批串行(强依赖)。

**KPI 预算**(per `ADR-CEP-00 §2 Decision 3`):
- **ADR KPI 预算以 `ADR-CEP-00 §2 Decision 3` 为单一真源**(per oracle v0.4 B8 修订:三处口径对齐):
  - Stage 0 umbrella = 1 KPI slot(计入)
  - Stage 1 = 6 串行 × 1 slot
  - Stage 2 = 2 串行 × 1 slot
  - Stage 3 = 1 slot
  - 总 10 个 OpenSpec change,串行 10 季度,任一时刻 Proposed 并发 ≤ 1

---

## 3. 不变量

- **INV-CEP-NUM-IC1**:CEP-NN 编号预分配表(本 ADR §2 Decision 1)是编号单一真源;新增 ADR 必须先查本表
- **INV-CEP-NUM-IC2**:不变量 id 与 ADR 编号命名空间分离,跨 ADR 不变量引用必须加 ADR 前缀(per Decision 2)
- **INV-CEP-NUM-IC3**:A14 编号口径(CEP-16)冻结为 06 §15.4,无需回头修 CEP-00 §3

---

## 4. 抓取点

1. **CEP-NN 编号查重**:新增 ADR 必须先 grep `docs/adr/ADR-CEP-NN-*.md` 确认编号未占用
2. **不变量 id 命名合规**:跨 ADR 引用 grep `INV-CEP-[0-9]+` 模式;不匹配 `_CEP-NN-NC` 等拼接命名
3. **06 §15.4 口径强制**:CEP 系列 ADR 引用 06 P 编号时,grep `P[0-9.]+` 必须命中 §15.4 章节,不命中 §11.4

---

## 5. 关联 ADR

- 基础:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
- 配套:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射)
- 配套:`ADR-CEP-06-experiment-protocol.md`(实验输出)
- 配套:`ADR-CEP-07-simulation-and-integration-semantics.md`(仿真语义)
- 后续:CEP-07A / CEP-08 ~ CEP-16(本表分配)

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 编号预分配 | CEP-08 ~ CEP-16 全部预分配 + 议题 + 依赖 + 状态 | ✅ 已拍板 |
| 不变量 id 与 ADR 编号命名空间分离 | 跨 ADR 引用加 ADR 前缀(INV-CEP-NN-XX)| ✅ 已拍板 |
| A14 编号映射冻结 | 06 §15.4 P2.0/P2.1/P3/P3.5/P4/P5(无 P6) | ✅ 已拍板 |
| 起草顺序 | 4 批串行,批内并行 | ✅ 已拍板 |

---

## 7. v0.1 修订记录(本 ADR 初版)

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | metis G5 + G3 | 解隐式依赖环 + 编号冲突 |
| §2 Decision 2 命名空间分离 | metis G3 | INV-CEP-NN-XX 命名约定 |
| §2 Decision 3 A14 冻结 | oracle S5 | 06 §15.4 唯一口径 |
| §2 Decision 4 起草顺序 | metis G5 | 4 批串行 + KPI 预算 |
| §3 3 条不变量 | 综合 | 与 §2 一一对应 |
| §2 Decision 1 表去重 | 代码复审(explore) + 自测「编号重复登记: CEP-11」 | CEP-07A/11/14/15/16 各有 2 行 → 折叠为每编号 1 行(gap 编号并入描述,依赖取并集) |

---

**版本**: v0.2 草案
**最后更新**: 2026-10-21(v0.2:§2 Decision 1 编号表去重)
**批准状态**: 🟡 待 Oracle v0.5 二次复审
**维护者**: CppTLM build agent(architect mode)