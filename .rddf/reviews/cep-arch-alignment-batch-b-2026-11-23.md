# Batch B 草案 · CEP 架构文档对齐（冻结面）—— ⛔ 未落盘

> **本文件是草案，不是真源**；目标文件为 M1 冻结面成员，⛔ **执行前必须走授权链**（见 §0）。
> 起草日期：2026-11-23 · 起草者：architect · 依据：`docs/adr/CEP-DECISION-DIGEST.md` + 全仓实测
> 关联审查：`.rddf/reviews/`（本次审查结论见对话正文）；oracle 已对「方向甲」出具否决（前提错误）

---

## 0. 授权链（执行前必读）

| 步骤 | 动作 | 责任方 |
|:--:|---|---|
| 1 | 台账落 Entry（**append-only**）—— 写明「架构文档对齐批次」+ 逐项清单 + 授权范围 | **rdd-planner**（台账唯一写入方） |
| 2 | 按本草案逐项编辑冻结面文件 | architect |
| 3 | `python3 scripts/cep/generate_doc_index.py --write`（07 号索引重生成） | architect |
| 4 | `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` | architect |
| 5 | **冻结清单单独 commit**（⛔ 不与正文混提） | builder |
| 6 | 无参复验 `bash scripts/test/cep_freeze_check.sh` + 全部相关闸 | builder |

⛔ **本草案不含台账 Entry 号** ⇒ 步骤 1 未完成前 ⛔ **不得落盘**。

### 涉及文件（全部在 M1 冻结集）

| 文件 | 冻结依据 | 本批改动项 |
|---|---|:--:|
| `docs/architecture/07-coherence-exploration-platform.md` | glob `docs/architecture/07-*.md` | ①⑤⑥⑦⑨ |
| `docs/architecture/cep/fabric-transport.md` | glob `docs/architecture/cep/*.md` | ② |
| `docs/architecture/cep/parity-baseline.md` | 同上 | ③④⑦⑧ |
| `docs/architecture/06-bis-coherence-as-network-extension.md` | ⚠️ **不在冻结集**（manifest grep = 0）⇒ 仅受「superseded 正文不可变」纪律约束 | ⑨（登记，不改） |

---

## 1. 方向乙 · 07 号 新增「附录 A · 证据档位 × 6 层架构 归属映射」

### 1.1 落位裁决（关键）

| 判据 | 实测 | 影响 |
|---|---|---|
| 机器锚点解析方式 | `cep_doc_ref_check.sh::section_line()` = **按节号**解析（登记表示例：`5`、`5.1.1`、`####`） | ⇒ 新增 `## ` 节**不破**机器锚点 |
| 07 号 的 `path:line` 活锚点 | 最高 `:743`（`obj-cep-platform-remaining.md:333`）；另有 `:138` / `:316` / `:590` / `:582-600` / `:696` | ⇒ **正文段内插入会位移** ⇒ 只能追加在 `:896` 之后 |
| 文末索引纪律 | `07:901`「本索引永久置于文末；此后**新增段落 SHALL 插在本索引之前** ⇒ 新增内容永不使既有 `path:line` 锚点位移」 | ⇒ **追加位置 = 最后一条 SU 之后、文末索引之前**（本草案遵守） |
| 索引生成 | `scripts/cep/generate_doc_index.py` 全量生成「自述计数行 + 索引 A/B」 | ⇒ 一级节 36→37 **自动同步**，`cep_index_position_check.sh` 保持绿 |
| SU 计数 | 索引 B 只收 `## Status Update` 标题 | ⇒ 本附录标题**不含**「Status Update」⇒ SU 仍 23 则 |

**落点**：`docs/architecture/07-coherence-exploration-platform.md` 第 896 行之后、第 897 行（`## §末 文档索引`）之前。

### 1.2 正文全文（可直接落盘）

```markdown
## 附录 A · 证据档位 × 6 层架构 归属映射(per `ADR-CEP-20` 决策 6)

> **性质**:**归属登记**(attribution)—— ⛔ **不是强度比较**。行 = 档(封闭词表 0/1/2)· 列 = 归属面 + 载体 + 机器锚点;⛔ 本表内**任意两行之间不存在强度比较**,⛔ 无聚合行,⛔ 无比较维度(per `ADR-CEP-20` 决策 6(i) · `openspec/specs/cep-evidence-tier2-observation/spec.md:72-78`「排序轴 = 装置归属层级 · 跨档强度 SHALL NOT 可比」)。
> **为什么需要本附录**:三档阶梯此前只登记在**附录 SU**(2026-11-13 阶梯 / 2026-11-14 档 2 载体 / 2026-11-21 档 2 第二族)与 `docs/guide/cep-usage.md` §12.9~§12.10 ⇒ 本文件**正文 §1~§11 零提及** ⇒ 只读正文的读者拿不到「档位声明义务 + 跨档聚合禁令」⇒ 本附录把**归属**补进结构层(per `ADR-CEP-20`「能力阶梯入 `07 号`」的后续项)。
> **落位说明**:本附录**不占 `§N` 节号**、且物理位置在既有 SU 之后、文末索引之前 —— 因为正文段内插入会位移既有 `path:line` 锚点(实测活锚点 ≤ `:743`)⇒ 遵守本文件文末索引纪律。

| 档 | 归属面(在 §2 架构中的位置) | 载体 | 机器锚点 |
|:--:|---|---|---|
| **0** | **横跨「Core / Traffic Generator」与「Coherence Interface」两层的 CohIf 边界装置** | `StubCacheEndpointTLM` + `CohIfResponderTLM`(拓扑级 E2E 装置) | `scripts/test/cep_full_green_run_check.sh` 的 **C1'** + `observation_keys.memory_ops_checked_addrs`;登记面 = `test/cep_e2e_run_tiers.json`(T1~T3 / T2b) |
| **1** | **§2 运行时架构之外** = 实验 harness 面(⚠️ 不在 §2.1 分层图内) | `ConsistencyCompletionModel`(`test/cep_consistency_completion_model.hh`;调用方仅 `test_cep_b4_evidence_device.cc` / `test_cep_experiment_emit.cc`) | `ADR-CEP-06` SU#4 ② 六条非空洞性约束 |
| **2** | **「Protocol Engine」层**(引擎侧载体)+ **「Directory & Ordering Plane」层**(tracker 侧载体) | ① 引擎侧:`protocol_engine_base.hh` 的 `EventObservation` + `mesif_protocol_engine.hh` 两处落点 · ② tracker 侧:`tilelink_home_state_driver.hh` | `test/test_cep_tier2_observation.cc`;登记面 = `test/cep_evidence_tiers.json`(T4 / T5 / T8 / T9) |

**两处「无 CEP 自有载体」显式标注**(⛔ 不静默省略):

| 层 | 现状 | 证据 |
|---|---|---|
| **Core / Traffic Generator** | 无 CEP 自有载体(通用 `TrafficGenTLM` 复用) | 本文件附录 SU(2026-11-14)② 读法边界 ⑤ |
| **Fabric & Transport Plane** | 无 CEP 自有运行时传输实现(仅 `fabric.json` 声明 + 装配期无环闸);⚠️ **但已有通用 `RouterTLM` 构成的 XY mesh 路径**(`CohIfNICTLM` ↔ `RouterTLM`;判据载体 `test/test_cep_mesh_e2e.cc`;容器 `include/tlm/cep/cep_mesh_interconnect.hh`)⇒ ⛔ **协议引擎不参与该业务事务路径** | `docs/architecture/cep/fabric-transport.md` §6.3.1 · `docs/guide/cep-usage.md` §12.2 第 6 行 |

**读法边界(⛔ 随引用重复)**:

1. ⛔ 本表**只登记归属** —— ⛔ **禁止**由本表推出「某档强于/弱于某档」;⛔ 禁止把两行的载体并列为同一结论的支撑;
2. ⛔ 档 2 与档 1 **正交**(协议引擎状态机 + 目录可见性 ↔ 完成域模型)⇒ ⛔ 不得表述为「档 2 强于档 1」(per `spec:72-78`);
3. ⛔ 档 0 的违例类 0 读法(「本装置无该形态」/ SWMR 结构不可达 / data-value 无判别力 / ⛔ 不得称「检测力已证」)⇒ **完整口径见本文件附录 SU(2026-11-13)③ + 手册 §12.9** ⇒ 本附录**不复述**;
4. ✅ **档位声明义务 + 跨档聚合禁令**的权威仍为 `ADR-CEP-20` 决策 6 + 手册 §12.9 ⇒ 本附录**不替代**该读法,只补**归属**;
5. ✅ 「不可判」≠「零违例」;
6. ⚠️ **「层」一律用层名,⛔ 不用层号** —— 手册 §12.2 的编号(1 Stub ~ 6 Transport,**Core 未编号**)与本文件 §2.2 的层序**不是同一套枚举** ⇒ 跨文档引用须写层名。

**配套机判(判据规格 · 交 builder 实现)** —— 建议在 `scripts/test/cep_e2e_reconcile_check.sh` 增 **T10**(部分闭合 `ADR-CEP-20` 登记的「闸盲区」):

| # | 判据 | 机判形态 |
|:--:|---|---|
| 1 | 本附录恰 **3 个档位行** | 档词封闭集复用 `TIER_WORDS` |
| 2 | 每行 ≥1 归属 cell + ≥1 机器锚点 | 形态计数 |
| 3 | ⛔ 无比较级措辞 | 禁词正则:`强于\|不弱于\|优于\|等同\|并列引用\|跨档.*(比较\|聚合)\|合计\|总判定` ⇒ 命中必红 |
| 4 | 层名齐全(6 层各恰现一次,或在「无 CEP 自有载体」清单内) | 计数 |
| 5 | 每行携带「强度不可跨档比较」限定句 | 形态计数 |
```

### 1.3 ⛔ 措辞红线（critic 硬条件，逐条对照）

| ⛔ 禁写 | 本稿实际措辞 |
|---|---|
| 「B′ 已落地」整体化 | 本稿不提 B′ |
| 「弱点已闭合」(撞 `registry spec:48`) | 本稿无 |
| 「检测力已证」(撞 `spec:48` / `07:742`) | 本稿仅**引用**该禁令，不作断言 |
| 「档 0 承载真实一致性语义」 | 本稿明确档 0 = 观测面 / 样本通路活性 |
| 跨档聚合 | 本稿只登记归属，无聚合行 |
| 档 2「强于档 1」 | 本稿明确「正交」 |

---

## 2. B8（本次新增发现）· `docs/architecture/cep/fabric-transport.md` 文件头订正

### 2.1 现状（自相矛盾）

| 位置 | 原文 | 实测 |
|---|---|---|
| `fabric-transport.md:5` | 「⚠️ 本层当前**无 CEP 自有运行时实现**(仅 `fabric.json` 声明 + 装配期死锁自由性闸)」 | ❌ 与同文件 `:43` 矛盾 |
| `fabric-transport.md:43` | 「仓内**现已存在** Router + flit 级路径 —— `CohIfNICTLM` ↔ `RouterTLM` 构成的 XY mesh(判据载体 `test/test_cep_mesh_e2e.cc`)」 | ✅ 实测：`include/tlm/cep/cohif_nic.hh`(class `CohIfNICTLM`)· `include/tlm/cep/cep_mesh_interconnect.hh`(class `CepMeshInterconnect`)· `test/test_cep_mesh_e2e.cc` · `configs/cep_mesh_e2e.json` · `configs/interconnect/cep_mesh_2x2.json` **全部存在** |
| `docs/guide/cep-usage.md:45` | 「✅ **但 coherence 流量已可穿过通用 `RouterTLM` 构成的 XY mesh**」 | ✅ 与 `:43` 一致 |

### 2.2 建议文本（替换 `:5` 一行，**零行数变化** ⇒ 不位移任何锚点）

```markdown
> **传输层**:装配 seam(ModuleFactory)、「经网络」的定义与拓扑分档判据。⚠️ 本层**无 CEP 自有运行时传输实现**(仅 `fabric.json` 声明 + 装配期死锁自由性闸);✅ **但 coherence 流量已可穿过通用 `RouterTLM` 构成的 XY mesh**(`CohIfNICTLM` ↔ `RouterTLM`;容器 `include/tlm/cep/cep_mesh_interconnect.hh`;判据载体 `test/test_cep_mesh_e2e.cc`)⇒ ⛔ **协议引擎不参与该业务事务路径**(per 本文件 §6.3.1)。
```

**零行数变化**是关键：`fabric-transport.md` 的锚点（`07 号` 索引 B + 跨仓对齐清单）不受影响。

---

## 3. C1 · 07 号 追加 SU（档 0 空洞性的**双根因**登记）

### 3.1 问题

`07:705`/`:706` 只登记了档 0 的**装置语义面**根因（无 sharer 语义 ⇒ SWMR 结构不可达；写数据恒 0 ⇒ data-value 无判别力）。**未登记**第二个根因：**拓扑分档**——全绿 run 只能在拓扑 A（直连零延迟）达成 ⇒ 背压 / VC / 排序从不被穿过。⇒ 读者无法自行推出「为什么档 0 的 0 必然空洞」。

⛔ **不得**据此声称「档 0 判别力提升」或「档 0 更接近档 1」——那会撞 `ADR-CEP-20` 决策 6(i)。本 SU 只做**根因补全**。

### 3.2 建议新增 SU（追加在既有 SU 之后、附录 A 之前或之后，⛔ 不改既有 SU）

```markdown
## Status Update 2026-11-23(档 0 空洞性的根因补全 —— per 台账 Entry #N)—— 装置语义面 + 拓扑分档

> **性质**:纯**追加** SU(append-only)。⛔ 不改本文件正文与任何既有 SU —— **逐字保留不回改**;新增 invariant 数 = **0**;⛔ 不改任何已签批 ADR。
> **依据**:`ADR-CEP-20` 决策 1 / 2 / 6 · `ADR-CEP-19` 决策 9 · 本文件 §8.3 与附录 A · `docs/architecture/cep/fabric-transport.md` §6.3.1 · `docs/guide/cep-usage.md` §12.2

### ① 事实(实测)

- **根因 A(已登记,本则不重复)** —— 装置语义面:档 0 装置**无 sharer 语义** ⇒ SWMR 违例**结构上不可达**(喂料通路只调合法写 `write()`);写数据**恒 0** + home 只回显 ⇒ data-value 检查**无判别力** ⇒ 该 0 = 「**本装置无该形态**」的 0(per 本文件附录 SU 2026-11-13 ③)。
- **根因 B(本则补全)** —— **拓扑分档**:现行全绿 run 落在**拓扑 A(直连 · 零延迟)**(`docs/architecture/cep/core-traffic-gen.md` §本层当前状态)⇒ **Fabric & Transport Plane 未被穿过** ⇒ 背压 / VC 分配 / 排序点**从不在该 run 内被行使**;⚠️ 仓内**已有** mesh 路径(`CohIfNICTLM` ↔ `RouterTLM` 的 XY mesh)但 ⛔ **协议引擎不参与该业务事务路径**(per `fabric-transport.md` §6.3.1)⇒ 拓扑 B **不承载**全绿。

### ② 口径限定(⛔ 随引用重复)

1. ⛔ 本则**不改**档 0 的任何读法 —— 违例类 0 仍读作「**本装置无该形态**」的 0;⛔ **不得**表述为「已证明协议层无违例」;⛔ **不得**称「检测力已证」;
2. ⛔ 本则**不主张**档 0 与档 1 的强度关系(⛔ 跨档比较禁令,per `ADR-CEP-20` 决策 6(i));本则只登记**两个独立根因的存在**;
3. ⛔ 根因 B **不蕴含**「档 0 承载一致性语义」—— 拓扑分档是**观测覆盖面**问题,⛔ 不是语义强度问题;⛔ 不得被读作「档 0 已具备语义判据」;
4. ⛔ 拓扑 B 产 run 属**另一批次**议题(观测面覆盖扩充,⛔ 未经决策不实施);本则**只登记现状**;
5. ✅ 「不可判」≠「零违例」。

### ③ Verification(可勾选 · 前瞻)

- [x] 本则已**追加**,本文件既有正文与**全部**既有 SU **逐字未变**;新增 invariant 数 = **0**
- [x] ① 的两个根因各带一手证据(装置语义面 / 拓扑分档 + mesh 载体实测路径)
- [x] ② 的五条口径齐备(尤其「不改档 0 读法」「不主张跨档关系」「不蕴含语义升级」)
- [x] 索引已重生成(`python3 scripts/cep/generate_doc_index.py --write`),`cep_index_position_check.sh` rc=0

**关联**:`ADR-CEP-20` 决策 1 / 2 / 6 · `ADR-CEP-19` 决策 9 · `docs/architecture/cep/fabric-transport.md` §6.3.1 · `docs/architecture/cep/core-traffic-gen.md` · `docs/guide/cep-usage.md` §12.2 · 台账 Entry #N
```

---

## 4. B1 · 07 号 §8.3 表订正（重复行 + 列数 + 缺 19/20/21）

### 4.1 三重缺陷（实测）

| 缺陷 | 实测 |
|---|---|
| **列数不符** | 表头 3 列(`编号\|议题\|阶段`)，但 **17 个数据行中有 10 行是 4 格** ⇒ Markdown 渲染时「议题」被拆成两列(第二列变成「已起草」状态列) |
| **重复行** | `CEP-11` / `CEP-14` / `CEP-15` / `CEP-16` / `CEP-07A` 各 **2 行**，其中 3 对**阶段值冲突**：`CEP-14` =「Stage 1」vs「Stage 0~1」· `CEP-15` =「Stage 1」vs「Stage 1 前」· `CEP-07A` =「Stage 1 前」vs「Stage 0~2」 |
| **缺 3 份** | 无 `CEP-19` / `CEP-20` / `CEP-21` |

### 4.2 建议替换文本（3 列 · 15 行 · 每 ID 一行）

**裁决说明（冲突消解）**：冲突的 3 对取**后出现**行的值（`CEP-14` = Stage 0~1 · `CEP-15` = Stage 1 前 · `CEP-07A` = Stage 0~2）—— 理由：后出现行带「已起草 2026-10-21」状态与更细的议题描述，属**后补登记**；⛔ 但**阶段列真源 = `ADR-CEP-00` 决策 3**（本表 §8.1 自述「单一时钟真源」），本裁决仅为**文档层**消解，⛔ 不改任何 ADR。

```markdown
| 编号 | 议题 | 阶段 |
|------|------|------|
| **CEP-07A** | 06-bis 既有产物处置(7 项逐项:`coherence_bundles_tlm.hh` 创建时机 + `coherence_domains` JSON 字段) | Stage 0~2 |
| **CEP-08** | Ordering Point 可迁移性规则 | Stage 1~2 |
| **CEP-09** | TileLink-C Tree Tip 映射 + **多 VC 路由器仲裁 + VC 分配的**死锁自由性论证**义务**(per 调研 F18) | Stage 1~4 |
| **CEP-10** | 可观测性归属(横切 vs 顶层独立) | Stage 1 |
| **CEP-11** | 协议版本锁定(规范版本锁定表 = 唯一真源;CHI / CXL / TileLink / NVLink / UALink 全族) | **Stage 1 前签发**(R2 缓解) |
| **CEP-12** | ProtocolEngine 插件能力映射表 schema | Stage 2 |
| **CEP-13** | 一致性域归属 | Stage 2 |
| **CEP-14** | Stub 可验证性合同测试(C1~C10 + 负向断言) | Stage 0~1 |
| **CEP-15** | 与 CppTLM 现有 CacheTLM 对齐(A13;`CacheTLM` 0 diff) | Stage 1 前 |
| **CEP-16** | `06 §15.4` P2.0-P5 演进路线对应 | Stage 1 |
| **CEP-17** | Ruby-SLICC 协议族接入 + 协议私有状态边界 | Stage 1 |
| **CEP-18** | 协议生成路线范围(SSP 描述 → 插件生成 vs 纯手写插件) | Stage 3 |
| **CEP-19** | CohIf 载体 + ProtocolBridge(5 条 RN-side 流;⛔ 决策 9 不动 `StubCacheCore`) | Stage 1 |
| **CEP-20** | 拓扑级 IC3 证据强度三档阶梯(档位声明义务 + 跨档聚合禁令) | Stage 4 之后 |
| **CEP-21** | Sharer responder 位宽扩容机制(阶段 1 机制化 / 阶段 2 触发式扩到 `W`,默认 8) | Stage 4 之后 |

> ⚠️ **状态不在本表**(⛔ 避免与 §9 双真源):各 ADR 的**签发 / 起草状态**以 `docs/adr/README.md` §2 + `docs/adr/ADR-CEP-*.md` 头部为准;本表只承载「**编号 + 议题 + 阶段归属**」。
> ⚠️ **阶段列真源** = `ADR-CEP-00` 决策 3(本节 §8.1 自述「单一时钟真源」)⇒ 本表为**镜像**,⛔ 冲突时以 ADR 为准。
```

### 4.3 零位移约束（**可行** —— 行数算平）

| 段 | 原 | 新 | 位移 |
|---|---|---|:--:|
| 表头 + 分隔行 | 2 | 2 | 0 |
| 数据行 | **17**(12 个唯一 ID + 5 个重复行) | **15**(补齐 19/20/21 后的唯一 ID 数) | **-2** |
| 注记行 | 0 | **2**(状态不在本表 / 阶段列真源) | **+2** |
| **合计** | **19** | **19** | **0** ✅ |

⇒ **等长替换 ⇒ `:590` / `:696` / `:743` / `:582-600` 等活锚点零位移** ⇒ **不需要**改动台账 / `.rddf/improvements/` / `.rddf/reviews/` 里的历史行号引用。

⛔ 前提：两条注记行**必须占满 2 行**（不得折行导致行数变化）⇒ 落盘后须复核 `:349`~`:367` 的行号与内容。

---

## 5. B2 · 07 号 §9「Primary ADRs」表补 3 行

### 5.1 现状

`:400`~`:425` 共 21 行，**缺 `ADR-CEP-19` / `ADR-CEP-20` / `ADR-CEP-21`** ⇒ 总纲的「Primary ADR」清单落后于决策面（其中 `19`/`20` 均已签发）。

### 5.2 建议新增 3 行（**追加在表末** ⇒ 零位移）

```markdown
| [ADR-CEP-19](../adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md) | CohIf 载体队列 + `CohIfBundle`(5 条 RN-side 流)+ `ProtocolBridge` 双层钩子 + JSON 装配链;⛔ **决策 9 = 不动 `StubCacheCore`(显式延期)** |
| [ADR-CEP-20](../adr/ADR-CEP-20-topology-ic3-evidence-strength.md) | 拓扑级 IC3 证据强度**三档阶梯** + **档位声明义务** + **跨档聚合禁令**;档 0 = 框架级观测面(⛔ 不承载一致性语义) |
| [ADR-CEP-21](../adr/ADR-CEP-21-sharer-bitmap-expansion.md) | Sharer responder 位宽**扩容机制**:阶段 1 机制化(成立即生效)/ 阶段 2 触发式扩到 `W`(默认 8) |
```

---

## 6. B3 · 计数订正（3 + 1 处）

| # | 位置 | 现状 | 建议 |
|:--:|---|---|---|
| 1 | `07:10` | 「**关联真源 3**(10 份 ADR v0.6):」 | 「**关联真源 3**(24 份 ADR · ✅ 已签发 20 / 🟢 Proposed 1 / 🟡 起草中 3):」 |
| 2 | `07:113` | 「10 份 CEP ADR(真源 3)」 | 「24 份 CEP ADR(真源 3)」 |
| 3 | `07:434` | 「`docs/adr/ADR-CEP-*.md` 10 份」 | 「`docs/adr/ADR-CEP-*.md` 24 份」 |
| 4 | `06-bis:16` | 「`docs/adr/ADR-CEP-*.md` **共 21 份**」 | 「**共 24 份**」 |

⛔ **第 4 项不落盘**：`06-bis` 是 superseded 历史快照（per `ADR-CEP-16`/`ADR-CEP-17` 逐项映射）⇒ 与 ADR 正文同等对待（只追加不改）⇒ **仅登记**（见 §9）。

⛔ **不在本批**：`07:590`（SU 区的「23 份 ADR … 23 键 + `total`=131」）—— 快照实测 `count=24` / `files` **24 键** / `total=131` ⇒ 该陈述**当时为真、现已陈旧** ⇒ 属 SU 区 ⇒ **⛔ 不可回改** ⇒ 由 §3 的新 SU 顺带订正（在该 SU 的「关联」里加一行快照键数实测）。

---

## 7. B5 / B6 / 丙 · `docs/architecture/cep/parity-baseline.md` 三项

### 7.1 B5 —— §4.7 产物表路径陈旧（2 处）

| 现状（`:304` / `:306`） | 实测 | 建议 |
|---|---|---|
| `scripts/cep/parity_trace_gen.py` | ⛔ 不存在 | `scripts/cep/parity/parity_trace_gen.py` |
| `scripts/cep/parity_compare.py` | ⛔ 不存在 | `scripts/cep/parity/parity_compare.py` |

（`scripts/cep/parity/` 实存 21 个文件，含 `build_gem5.sh` / `parity_config.json` / `gem5_*_probe.py` / `run_parity*.sh` / `selftest_parity_compare.py` 等。）

### 7.2 B6 —— §8 表编号乱序 + 重复

**实测**：§8 表 18 个数据行，编号序列 = `1,2,3,4,5,6,7,8,9,12,11,10,11,13,14,15,16,17` ⇒ **11 出现两次**（内容不同：「主对拍档(口径 A)已实测」vs「P-2 的判据口径不足」）· **12 错位**（排在 11/10 之前）· 其余 1~17 齐全、无缺号。

**两种改法**（⛔ 内容逐字保留，只动编号 / 行序）：

| 改法 | 动作 | 行位移 |
|---|---|:--:|
| **甲** | **只改编号**：`:368` 的第二个「11」→ **18**，其余 13→14 · 14→15 · 15→16 · 16→17 · 17→18 顺延 ⇒ 编号集合变为 **1..18 唯一齐全**（行序仍非升序） | 0 |
| **乙** | **按编号重排行序**（1..18 升序）⇒ 可读性最好 | 0（表在文件末尾，且**外部无行号锚点** —— 实测 `grep -rn "parity-baseline\.md:[0-9]"` = **空**） |

**建议**：**乙**（表内编号是读者唯一的索引手段，升序才有意义；零位移已验证）。

### 7.3 丙 —— 「档位」术语消歧（同名异义）

**问题**：`parity-baseline.md:122`/`:128`/`:148`/`:364` 的「档位」指**对拍判据分档**（辅助档 / 主对拍档），与**证据档位**（档 0/1/2）同名异义 ⇒ 且它是 `docs/architecture/cep/` 下**唯一**出现「档位」的文件 ⇒ 读者在该目录内读到「档位」会误解。

**建议（改 4 处称谓，零行数变化）**：

| 位置 | 现状 | 建议 |
|---|---|---|
| `:122` | 「**判据按「档位」分列**」 | 「**判据按「判据档」分列**(⛔ 此处「档」指**对拍判据档**(辅助 / 主对拍),⛔ **非**证据档位(档 0/1/2))」 |
| `:128` | 表头「适用档位」 | 「适用**判据档**」 |
| `:148` | 表头「driver / 档位」 | 「driver / **判据档**」 |
| `:364` | 「引入『档位分列』」 | 「引入『**判据档分列**』」 |

---

## 8. B7 · `parity-baseline.md` 裁决段与实测的分歧（注记，不改裁决）

### 8.1 问题

§2/§3/§6 的**裁决文本**仍以「**gem5 Ruby = 主基准**」呈现（`:334` 表内「✅ **主基准**」），但同文件 `:362`（§8 #7）实测登记：

> 「实测:**gem5 v24 无 `build/RUBY` 变体** ⇒ 改用 `build/NULL`;且 **Ruby 的 `Sequencer.in_ports` 是 `VectorResponsePort`** ⇒ **CPU-less 驱动不了 Ruby coherence 栈** ⇒ 首轮实测用 **classic `Cache`**(无 coherence 协议开销)⇒ 首轮结论**只覆盖口径与建模语义**,**不覆盖** Ruby 协议栈基准」

⇒ 读者读裁决段会得到**过强**印象（以为已建立 Ruby coherence 栈对拍基准）。

### 8.2 建议（注记式追加，⛔ 不改裁决 —— 裁决变更属 ADR 域）

在 `§3.1 主基准 = gem5 Ruby` 之后追加一行注记（零行数变化：并入该段末行）：

```markdown
> ⚠️ **可达性限定(2026-11-14 补记)**:本裁决的**方法学**(M3 独立性 + 可复核 + 与 H7 判据类型匹配)**不变**;但**实测可达性**有窄化 —— gem5 v24 **无 `build/RUBY` 变体**(改用 `build/NULL`),且 Ruby `Sequencer.in_ports` 为 `VectorResponsePort` ⇒ **CPU-less 驱动不了 Ruby coherence 栈** ⇒ 首轮实测走 classic `Cache`(无 coherence 协议开销)⇒ **首轮结论只覆盖口径与建模语义,不覆盖 Ruby 协议栈基准**(per 本文件 §8 #7)。
```

⛔ **不改**「gem5 Ruby = 主基准」这个裁决本身（裁决变更需走 ADR / 台账）；本项只补**可达性限定**。

---

## 9. 附带订正 · `07:838` 的「冻结面」标签不准（⛔ 只能追加）

### 9.1 问题

`07:838`（附录 SU 2026-11-14 的 ① 表第 11 行）把 4 处「Sharer Tracker **5** 实现」残留并称「**冻结面**」：

> 「…**冻结面**:`docs/architecture/06-bis-coherence-as-network-extension.md` 与 `ADR-CEP-00` / `ADR-CEP-16` / `ADR-CEP-17` | 同型「Sharer Tracker **5** 实现」残留 | **本批不改** —— …冻结面 4 处按 **ADR / superseded 正文不可变**纪律**仅登记不追改**…」

**实测**：`docs/architecture/06-bis-coherence-as-network-extension.md` **不在** `FROZEN_GLOBS`（24 条 glob 全量核对）**也不在** manifest（`grep -c 06-bis` = **0**）⇒ 「冻结面」标签**不准确**。

### 9.2 处置建议（⛔ 不回改该 SU —— 追加新 SU 订正）

新 SU 中记一行：

```markdown
> ⚠️ **订正上一则 SU 的标签**(per 台账 Entry #N):该则把 `docs/architecture/06-bis-coherence-as-network-extension.md` 与 3 份 ADR 并称「冻结面」—— 实测 **06-bis 不在 `FROZEN_GLOBS`(24 条)也不在 manifest(90 产物)** ⇒ 该标签不准确。**结论不变**(`06-bis:26` 的「SharerTracker 5 实现」**仍不追改**),但**理由须更正** = `06-bis` 是 **superseded 历史快照**(per `ADR-CEP-16`/`ADR-CEP-17` 逐项映射)⇒ 与 ADR 正文同等对待(只追加不改),⛔ **不是**冻结面约束。
```

---

## 10. 本批**不**包含（另立）

| 项 | 归属 | 处置 |
|---|---|---|
| `openspec/specs/cep-evidence-tier2-observation/spec.md:4` Purpose 仍写「档 2 尚无载体」 | **契约面** | planner 域（openspec 变更）⇒ architect **不写** ⇒ 登记为提案 |
| `tier2_applicable` 推导规则入契约层 | **契约面** | 同上 |
| `T10` 机判脚本实现 | **实施面** | builder 域 ⇒ 本批只给**判据规格**(§1.2) |
| `ADR-CEP-21` 阶段 1 机制化 / `kResponderMappedBits`(现值 **4**) | 决策 + 实施 | ADR-CEP-21 = 🟢 **Proposed**(未签发)⇒ 阶段 1「成立即生效」**尚未生效** ⇒ 本批不动 |
| 「拓扑 B 产档 0 run」(甲′) | 决策 | ⛔ oracle 已否决原表述;改写版**未起草** ⇒ 建议先做 B′ 负控见证(`ADR-CEP-20:71`,已在 backlog) |
| `06 号`(`06-inter-module-network.md`)**不在** M1 冻结集 | — | 本批未涉及 |

---

## 11. 执行顺序与验证

1. §2（`fabric-transport.md` 单行替换 · 零位移）→ §7.3（术语消歧 4 处 · 零位移）→ §6 的 07 号 3 处计数（**等行数替换**）→ §5（§9 表追加 3 行）→ §4（§8.3 表 · **零位移方案**）→ §1（附录 A 新增）→ §3（新 SU）→ §9（订正 SU）
2. `python3 scripts/cep/generate_doc_index.py --write`
3. 闸：`cep_freeze_check.sh` · `cep_index_position_check.sh` · `cep_e2e_reconcile_check.sh` · `cep_doc_ref_check.sh` · `cep_docs_sync_check.sh --strict` · `docs_sync_check.sh --strict` · `cep_dod_reading_check.sh` · `cep_adr_structural_check.py` · `cep_decision_block_check.py` · `docs_adr_index_check.sh` · `gen_adr_index.py --check`
4. `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单单独 commit → 无参复验

### 落盘决策点（**已锁定 · 2026-11-23 用户确认「按建议推进」**）

| # | 决策点 | 裁定 | 依据 |
|:--:|---|---|---|
| 1 | §8.3 表改法 | **等长替换（19 行 → 19 行）** ⇒ 活锚点零位移 | §4.3 行数算平 |
| 2 | 附录 A 的物理位置 | **最后一条 SU 之后、文末索引之前** | `07:901` 索引纪律 + 活锚点最高 `:743` |
| 3 | §3 新 SU 相对附录 A 的位置 | **之后**（附录 A = 正文节，SU 在其后更贴近既有形态） | 形态一致性 |
| 4 | 台账 Entry 号 | **#537**（台账最新 = #536） | `obj-cep-platform.md` |
| 5 | E2E 修订甲′ | **不在本批**（需 ADR SU + spec delta 的独立授权链） | 范围隔离 |
