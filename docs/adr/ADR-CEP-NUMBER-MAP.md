# ADR-CEP-NUMBER-MAP: Coherence Exploration Platform 编号预分配

> **状态**: 🟡 起草中(v0.7)
> **日期**: 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP-08 ~ CEP-16 编号预分配 + 议题映射**,解 metis G5 隐式依赖环
> **关联真源**:
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节

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

### 1.2 编号冲突风险

- `CEP-13` 既是 ADR 编号,**也是**不变量 id(`ADR-CEP-03 §3.2` 不变量 CEP-13)
- `CEP-NN` 前缀双语义,后续引用易混淆

### 1.3 本 ADR 的定位

本 ADR **冻结** CEP-08 ~ CEP-16 编号 + 议题映射,**打破隐式依赖环**;同时**显式**约束"不变量 id ≠ ADR 编号"以解 G3 编号冲突。

---

## 2. 决策

### 决策 1:CEP-NN 编号预分配表(解 G5)

> ⚠️ **本表第 5 列的语义**(per 台账 **Entry #321 / #322**,续 **Entry #307 C-3**):
> ① 第 5 列 = **ADR 文件存在性**(`✅ 已起草` / `🟡 待起草`)⇒ **不承载生命周期状态**;
> ② **生命周期**状态的真源 = 各 `ADR-CEP-NN-*.md` 的 header `> **状态**:` 元数据行 + 其 Status Update 链;**本 ADR 自身亦同此例**;
> ③ **派生视图**(编号 → 生命周期)= `python3 scripts/test/cep_adr_structural_check.py --list`(**现算现打**,派生自各 ADR header,**非权威**);
> ④ 本口径**自足成文** —— 台账 Entry #321 / #307 C-3 仅作**授权 / 决策凭据**(⛔ 台账不作状态声明);
> ⑤ **例外**:§6 决策汇总表的 `✅ 已拍板` 属「**决策已定**」维度,**不**受本口径覆盖。

| 编号 | 类型 | 议题 | 依赖 | ADR 文件 |
|------|------|------|------|----------|
| **CEP-00** | ADR | 跨仓治理 + KPI 排队 + 边界条款豁免 | — | ✅ 已起草 |
| **CEP-00A** | ADR | 真源章节映射 | CEP-00 | ✅ 已起草 |
| **CEP-01** | ADR | Fabric Collective 旁路平面 | — | ✅ 已起草 |
| **CEP-02** | ADR | canonical 10 流清单 + barrier | CEP-00A | ✅ 已起草 |
| **CEP-03** | ADR | Fabric Collective 内部 + non-cacheable | CEP-01, CEP-02 | ✅ 已起草 |
| **CEP-04** | ADR | Sharer Tracker D-5 重构 | CEP-02 | ✅ 已起草 |
| **CEP-05** | ADR | Stub Cache RN 投影 + 折叠表 | CEP-02 | ✅ 已起草 |
| **CEP-06** | ADR | 实验输出协议 | — | ✅ 已起草 |
| **CEP-07** | ADR | 仿真语义 + 集成 seam | — | ✅ 已起草 |
| **CEP-07A** | ADR | 06-bis 既有产物处置(7 项逐项 + 协议泄漏通用禁令) | CEP-12, CEP-13, CEP-14 | ✅ 已起草 |
| **CEP-08** | ADR | Ordering Point 可迁移性规则(A3)| CEP-04 | ✅ 已起草 |
| **CEP-09** | ADR | TileLink-C Tree Tip 映射 + 多 VC 仲裁 **+ VC 分配的死锁自由性论证**(A5,含 Decision A 无环判据 / Decision B 仲裁策略)| CEP-01, CEP-04, **CEP-12** | ✅ 已起草|
| **CEP-10** | ADR | 可观测性归属(A8)| CEP-06, CEP-07 | 🟡 待起草 |
| **CEP-11** | ADR | 协议版本锁定(规范版本锁定表 = 唯一真源)(A9)| CEP-12, CEP-00 | ✅ 已起草 |
| **CEP-12** | ADR | ProtocolEngine 插件能力映射表 schema(A10)| CEP-04, CEP-08, CEP-09, CEP-11 | ✅ 已起草|
| **CEP-13** | ADR | 一致性域归属(A11)| CEP-02, CEP-04 | ✅ 已起草 |
| **CEP-14** | ADR | Stub 可验证性合同测试(C1~C10 + 负向断言 + 组合接缝)(A12)| CEP-05, CEP-07, CEP-08 | ✅ 已起草 |
| **CEP-15** | ADR | CppTLM CacheTLM 对齐边界(共存不替换)(A13)| CEP-14, CEP-05, CEP-07A | ✅ 已起草 |
| **CEP-16** | ADR | `06 §15.4` P2.0-P5 演进对应(P6 排除)(A14)| CEP-00, CEP-04, CEP-13 | ✅ 已起草 |
| **CEP-17** | ADR | Ruby-SLICC 协议族接入 + 协议私有状态边界 + 学术协议族 + 组合承诺边界 | CEP-00, CEP-02, CEP-04, CEP-05 | ✅ 已起草|
| **CEP-18** | ADR | 协议生成路线范围(SSP 描述 → 插件生成 vs 纯手写插件)| CEP-12, CEP-17 | ✅ 已起草 |
| **CEP-19** | ADR | **CohIf 载体形态**(业务 Bundle `CohIfBundle` + ProtocolEngine 的 ChStream 端口面 + `ProtocolBridge` 特化桥 + JSON 装配)| CEP-07, CEP-02, CEP-16 | ✅ 已起草 |
| **CEP-20** | ADR | **拓扑级 IC3 证据的强度边界与能力阶梯**（证据档位阶梯 + 跨档聚合禁令）| CEP-06 | `ADR-CEP-20-topology-ic3-evidence-strength.md` |
| **CEP-21** | ADR | **Sharer 位宽 —— responder bit 映射的扩容机制与触发判据**（两阶段：机制化 + 触发式扩容到 W）| CEP-04, CEP-12 | `ADR-CEP-21-sharer-bitmap-expansion.md` |

**CEP-NUM(本 ADR)**:本表本身,作为"编号映射单一真源"

**说明**:
- **CEP-07A** 是 metis G6.8 建议独立起草的 ADR(06-bis 既有产物处置),本表显式纳入
- **CEP-09** 是 metis G3 提到的"多 VC 仲裁"承诺,合并 A5(TileLink 映射)以减少 ADR 数
- **CEP-13** 命名冲突见 Decision 2
- **去重(2026-10-21)**:本表曾对 CEP-07A / 11 / 14 / 15 / 16 各登记 **2 行**(旧的「🟡 待起草」预分配行 + 后续起草时补的「✅ 起草」行,后者未删前者)。已折叠为每编号**恰好 1 行**:保留新行(描述更完整),把旧行独有的 gap 编号(A9/A12/A13/A14)并入描述,依赖列取**并集**。机器检查见 `test/test_cep_contracts_crosscheck.cc`「编号表是唯一真源且无重号」。

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
  - Stage 1 = **10** 串行 × 1 slot
  - Stage 2 = 2 串行 × 1 slot
  - Stage 3 = 1 slot
  - 总 **14** 个 OpenSpec change,串行 **14** 季度,任一时刻 Proposed 并发 ≤ 1

> **2026-10-23 增补(台账 Entry #27 [adr-amendment])** —— 本块原写「Stage 1 = 6 串行 /
> 总 **10** 个 change / 串行 **10** 季度」,与 `ADR-CEP-00 §2 Decision 3` 的 **11**
> (Stage 0 = 1 + Stage 1 = **7** + Stage 2 = 2 + Stage 3 = 1)冲突 —— **真源 3 内部矛盾**。
> **已按权威口径更正为 11**(旧值从未是权威值)。
> **写法沿用** `ADR-CEP-00 §2 决策 3` 的「2026-10-22 增补」同款:原地更正 + 标注增补块 + 台账 entry。

> **2026-10-26 增补(台账 Entry #71 [scope-change])** —— 本块原写「Stage 1 = **7** 串行 /
> 总 **11** 个 change / 串行 **11** 季度」。`ADR-CEP-00 §2 Decision 3` 的排队已由
> **11 → 13**(`moesi_gpu` / `mesif` 排入 Stage 1;Stage 1 变 **9** 个串行)⇒ **同批机械同步**为
> **9 / 13 / 13**(真源仍 = `ADR-CEP-00 §2 Decision 3`,本块只是其**投影**)。
> **写作纪律**:「表内计数的一致性陈述」随表机械变化 ⇒ 原地更正(同 Entry #12 的「表可增,论断不改」原则);
> 而**历史论据引述**(如 `ADR-CEP-04` / `ADR-CEP-05` 的 Status Update 里引述「11 个 change」作论据)**不改** ——
> 其逻辑不因算术扩增而失效(拆分会破算术这一点仍成立)。
> **2026-10-27 增补(台账 Entry #74 [scope-change])** —— 本块原写「Stage 1 = **9** 串行 /
> 总 **13** 个 change / 串行 **13** 季度」。`ADR-CEP-00 §2 Decision 3` 的排队已由
> **13 → 14**(新增队列 #7 `-cohif-carrier`,插在 `-moesi-gpu-adapter` 与 `-chi-rnf-subset`
> 之间;Stage 1 变 **10** 个串行)⇒ **同批机械同步**为 **10 / 14 / 14**(真源仍 =
> `ADR-CEP-00 §2 Decision 3`,本块只是其**投影**)。**同批**在 §2 决策 1 的编号预分配表
> **新增 `CEP-19` 行**(CohIf 载体形态,✅ v1.0 已签发 2026-10-27)⇒ **ADR 清单 21 → 22**。
> **写作纪律**同 Entry #71:表内计数随表机械变化 ⇒ 原地更正;**历史论据引述不改**。
> **冻结面**:本文件属 M1 冻结面(成员数以 `scripts/test/cep_freeze_check.sh` 清单为准),本次改动已走授权闸
> `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #74"`。

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
| 编号预分配 | CEP-08 ~ CEP-16 全部预分配 + 议题 + 依赖 | ✅ 已拍板 |
| 不变量 id 与 ADR 编号命名空间分离 | 跨 ADR 引用加 ADR 前缀(INV-CEP-NN-XX)| ✅ 已拍板 |
| A14 编号映射冻结 | 06 §15.4 P2.0/P2.1/P3/P3.5/P4/P5(无 P6) | ✅ 已拍板 |
| 起草顺序 | 4 批串行,批内并行 | ✅ 已拍板 |

---


## Status Update 2026-10-27 —— §2 决策 1 预分配表**新增 `CEP-19`** + KPI 预算块 13 → 14

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**;唯一例外是 §2 决策 1 的**表内计数**与 §2 决策 4 的 **KPI 预算块**(均为**活口径**,per Entry #12 / #27 / #71 同款先例:随表机械变化 ⇒ 原地更正 + 标注增补块 + 台账 entry)。
> **登记 ① —— 新增 `CEP-19`**:编号预分配表新增一行 —— `CEP-19` = **CohIf 载体形态**(业务 Bundle `CohIfBundle` + ProtocolEngine 的 ChStream 端口面 + `ProtocolBridge` 特化桥 + JSON 装配),依赖 `CEP-07` / `CEP-02` / `CEP-16`,状态 ✅ **v1.0 已签发 2026-10-27**。⇒ **ADR 清单 21 → 22**。
> **登记 ② —— KPI 预算块同步**:`ADR-CEP-00 §2 Decision 3` 的排队已由 **13 → 14**(新增队列 #7 `-cohif-carrier`;Stage 1 变 **10** 个串行)⇒ 本块**同批机械同步**为 **10 / 14 / 14**。
> **登记 ③ —— `CEP-19` 不定义 invariant**:**新增 invariant 数 = 0**(`ADR-CEP-19` 决策 2 / 8 明确「不引入新抽象层」,其 9 条决策**均不含** invariant 定义)⇒ 已登记进 `scripts/test/cep_docs_sync_check.sh` 的 `NO_INV` 白名单(否则「目录内 ADR 未纳入 OWNERS 或 NO_INV ⇒ 其 invariant 不会被计数」会报红)⇒ **15 组 66 条编号未被改动**。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #74**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #74)。

## Status Update 2026-11-10 —— 第 5 列语义**归位**为「ADR 文件存在性」+ §2 决策 1 表上方**补口径行**(per 台账 Entry #321 / #322)

> **性质**:本 ADR **尚未签发**(header `> **状态**: 🟡 起草中(v0.7)`)⇒ ⛔ **不受 append-only 约束** ⇒ 本次**直接改正文**(header 版本 v0.6 → v0.7)。
> **触发**:台账 **Entry #321**(R-8 裁决)经 **oracle**(1 轮,PASS-WITH-NOTES)· **critic**(1 轮,REJECT,1 BLOCKER + 11 SHOULD_FIX + 4 NIT)⇒ 形态由「D-1 删列」修订为「**D-1′ 保留列 + 换语义**」。

### ① 本次修订(逐条)

| 项 | 内容 |
|---|---|
| **第 5 列语义** | 由「`v0.3 状态`」(承载**生命周期**且符号 **5 种形态** ⇒ 不可解析)改为「**ADR 文件**」⇒ 承载**文件存在性**(`✅ 已起草` / `🟡 待起草`)**2 值** ⇒ 生命周期维度上的镜像面**归零**;`CEP-10`(无 ADR 文件)保留 `🟡 待起草` ⇒ **占位信号保住** |
| **表头** | `v0.3 状态` ⇒ `ADR 文件`(⛔ 旧表头**自称权威**,是失实陈述) |
| **口径行** | 补于 §2 决策 1 表**紧上方**(五要点 + 例外声明):第 5 列 = 文件存在性 · **不承载**生命周期 · 生命周期真源 = 各 ADR header + SU 链 · **本 ADR 自身亦同此例** · 派生视图获取命令 · 凭据与真源分离 · §6 `✅ 已拍板` 属「决策已定」维度不受本口径覆盖 |
| **连带** | §6 决策汇总表第 1 行删「+ 状态」(否则与 §2 决策 1 的表**自相矛盾**)· `:79` 去重说明删「状态更准确」 |

### ② ⚠️ 为何**保留**该列而非删列(critic BLOCKER B1)

`openspec/specs/cep-core-contracts/spec.md:184` 的 Scenario「编号表是唯一真源(IC1 / IC2)」明文要求:`CEP-00` ~ `CEP-18` 每个编号**恰好一条**表项,**含议题 / 依赖 / 状态** ⇒ 该 main spec 是 **M1 冻结集成员**(`cep_freeze_check.sh:71`)⇒ **删列 = 静默改写冻结契约**;且该条款**无机器载体**(`test_cep_contracts_crosscheck.cc:57-87` 只判「无重号 + 行数 ≥18 + 特定 ID 存在」,**不检列存在性**)⇒ **不会自动报红**。
⇒ 采纳**路线甲**:保留列 + 换语义 ⇒ **冻结主契约零 diff**、**零额外五步链**、**不占 KPI slot**。

### ③ 派生视图(替代「文档内总览」)

生命周期信息**不在本表** ⇒ 由 `scripts/test/cep_adr_structural_check.py` **现算现打**「编号 → 生命周期 / 文件存在性」(左表 = **本表 §2 决策 1 的编号集合** ⇒ `CEP-10` 显式打印「🟡 未起草(无 ADR 文件)」;生命周期抽取**锚定首个** `> **状态**:` 行;打印覆盖率 `已解析 N/21` + 未识别者清单)。
⚠️ 该视图**派生自各 ADR header,非权威** ⇒ ⛔ **不得被任何文档引用为状态依据**;⚠️ **纯导航 · 不判红** ⇒ 不进 invariant map、不加 `INV-CARRIER`。

### ④ 边界

- ⛔ **不改** `openspec/specs/cep-core-contracts/spec.md`(路线甲 ⇒ **零 diff**)· ⛔ **不改** `07 号` · ⛔ 不新建 invariant id(`07 号 §4` / `ADR-CEP-00A §4` / `cep_invariant_map_check.py` 零 diff)· ⛔ 不建判红闸 · ⛔ 本 ADR 的 **M1 冻结集成员资格**单独裁决(本轮**不做**)· ⛔ `B3b-2` 维持 0/4。
- ⚠️ **实例级闭合**:本次消除了**本表**的镜像面并补上**本文件**的自洽口径 ⇒「复发根因闭合」**仅就本表成立**;**类级口径**(「CEP 文档中任何 ADR 的生命周期/版本状态一律以其自身 header 为准」)是**另一条缺口**,已登记 backlog(含 `07 号 §8.3` / `ADR-CEP-16 §5` / `ADR-CEP-17:337` 三处同型载体)⇒ **本轮不做**。

> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #321 / #322**(裁决 + 形态修订 + 冻结面授权)
> **冻结面**: 本文件属 M1 冻结面,本次修订已走授权闸(见 Entry #321 / #322)。


## 7. v0.1 修订记录(本 ADR 初版)

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
