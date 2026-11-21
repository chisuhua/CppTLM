---
name: cep-memory-model-operational-trace
priority: P2
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-13
  - ADR-CEP-12
状态: "🟡 **待立项**(2026-11-21 · planner 起草 · 架构裁决已落 = architect 裁决记录 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §三(登记册 §6.4 队列 2 的 **I-6 决策 2**)· 经 **metis 1 轮**(P0/P1/P2 逐条处置见 §审查轨迹)· ⏳ **待用户 ratify**(本单元的「每域模型来源」ratify 项 + I-6 的 R-2)· ⏳ 待立项)"
created: 2026-11-21
---

# cep-memory-model-operational-trace: 内存模型操作级轨迹 + 边类语义定义

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 的正文**(新建 ADR 是**新文件**,不触碰既有正文 ⇒ 两件事)· ⛔ **不产 run** · ⛔ **新增 invariant 数 = 0** · ⛔ **不改** `combination_contract` 的字段类型 / 取值域(已由 architect 裁决给定,⛔ 待用户 ratify)

## 架构依据

`ADR-CEP-13` 决策 2 采用的复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 是**操作级**判据:
`rfe` / `fr` / `ws` 是**内存操作之间的关系**(read-from external / from-read / write-serialization),
`ppo_com(t) ≡ ppo_i` 是**程序序**——判据的输入必须是**执行迹(operations + reads/writes + values)**。

⛔ **当前缺口**:本仓**无操作级模型** ⇒
  ① 边类语义**无处承载**(写在文档里的定义将无任何断言能检验 ⇒ 事实上的第二真源);
  ② `include/tlm/cep/litmus_falsifier.hh` 的图顶点 = `(instance, network, vc)` = **通道依赖图**
     ⇒ 与操作图**不同型** ⇒ 由声明项派生的边**不能**证伪该公理(假绿)。

**架构裁决已定**(per `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §三):
本仓**不定义** `rfe/fr/ws` 语义 ⇒ I-6 的 `combination_contract.compound_consistency.edge_classes`
只登记**取值域**(`rfe` / `fr` / `ws`)+ 语义锚点引用(`ADR-CEP-13` 决策 2)⇒ **定义工作归本单元**。

## 范围

- **In-Scope**:操作级轨迹模型(线程 / 操作 / 读写事件 / 值 / 序关系)· `rfe/fr/ws` 的**语义定义**(落到操作模型上)· 操作图与 litmus 工装的**接缝**(取代当前「通道图 + 语料」混图)· **每域模型来源 + 域间边构造规则**的裁决输入(见 What Changes #1)· **跨域桥接参与判定面**(=`early_ack` 的 `not-applicable` 语义缺口,见 What Changes #6)· `verification_scope` 的**输入**可枚举化(⛔ 不改字段类型 —— 见 What Changes #5)
- **Out-Scope**:⛔ 通道依赖图(既有 `channel_dependency_graph.hh`;其语料文件保留给既有闸)· ⛔ 4 件套 run · ⛔ 各协议族的协议语义 · ⛔ `combination_contract` 的**字段类型 / 取值域**(⇒ 须重开 R-2 + 用户 ratify,本单元**不做**)· ⛔ **运行期轨迹采集**(v1 采**手写 litmus 迹**,运行期采集另立 backlog)· ⛔ I-6 的实施面(schema / ADR 追加 SU / deferrals 移除)

## Why

**不做的代价**:`INV-CEP-13-IC1` 的「结构化声明项」已由 I-6 落地(声明**采用**该判据),但**判定**仍无载体
⇒ 档 2/档 3 体系里「复合一致性」永远只是**声明**而非**判定** ⇒ 跨域组合的正确性维度对平台永久不可用。

**为什么必须独立立项**:选 memory model 家族(ARM / POWER / C++)本身是**架构决策**;
操作级轨迹是**新载体**(与通道依赖图正交)⇒ 规模与审批链都与 I-6 不同级 ⇒ 混进 I-6 会把一个载体面变更绑到规格面修订上。

**为什么不能由 I-6 顺手做掉**:I-6 的完成判据是「`⭭-unmodelled` 2 → 0」+ schema 形态落定 ⇒ 它的载体是**声明面**;
把「定义 + 新操作级模型」塞进 I-6 会让该单元同时承担**规格修订**与**新载体建模**两件事 ⇒ 审批链失真。

**为什么「家族选型」不能是「选一个全局家族」**:`ADR-CEP-13` 决策 2 采用复合模型的动机原文是「后者(所有域模型相同)**过严** —— 异构系统的现实就是各域模型不同(如 CPU 簇 SC + GPU 簇 RC)」⇒ **单一全局家族与该动机直接相悖** ⇒ 本单元裁的是「**每域模型来源 + 域间边构造规则**」(见 What Changes #1)。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **每域模型来源 + 域间边构造规则**(⚠️ **不是**「选一个全局家族」——见 Why)| 架构决策(需**独立裁决 + 用户 ratify**,与 I-6 的 R-2 同型):每个一致性域声明自己的模型来源(SC / RC / TSO / …)+ 域间边(`rfe` / `fr` / `ws`)的构造规则;⇒ **不得**统一为一个家族 |
| 2 | **操作级轨迹模型** | 线程 → 操作(读/写 + 地址 + 值)→ 事件图(节点 = 操作,边 = `ppo`/`rf`/`fr`/`ws`/`rfe`);⚠️ `fr` / `ws` 是**派生边**(由 `rf` + 写序导出)⇒ 语料**不得**直接给出 ⇒ 域内 `rf` 是否入图 = **家族相关**的未定项(⇒ 随 #1 裁决) |
| 3 | **`rfe/fr/ws` 定义落点** | 定义写在**操作模型**上(可被断言检验)⇒ ⛔ 不在 ADR 正文外另立第二真源 ⇒ 若需 ADR ⇒ **新建** ADR(新文件,⛔ 不触碰既有正文) |
| 4 | **litmus 工装接缝** | `litmus_falsifier.hh` 的图**改为操作图** ⇒ ⛔ 不得混图;**物理分离**:通道语料留在 `test/cep_litmus_corpus.json`(既有闸继续用)· 操作迹另置 `test/cep_memory_model_traces.json` ⇒ 「分流计入」成为**结构性事实**而非断言 |
| 5 | **`verification_scope` 的输入可枚举化** | ⛔ **不改** `combination_contract.compound_consistency.verification_scope` 的字段类型 / 取值域(那是 I-6 已裁的 R-2e)⇒ 本单元只提供「散文 → **可枚举范围**」的**外部登记**并在文档层对齐;⚠️ 若要改字段类型 ⇒ 须**重开 R-2** 并取得用户 ratify |
| 6 | **跨域桥接参与判定面** | 「本实例是否参与跨域桥接」的判定面(architect 裁决 §三 的**连带依赖**)⇒ 使 `early_ack` 的 `not-applicable` 取值**可判**(本仓当前**无**该判定面 ⇒ architect 已把 `early_ack` 取值域收敛为 3 值) |
| 7 | **迹的来源 = 手写 litmus 迹** | v1 的轨迹是**数据**(具名 litmus);⚠️ **运行期采集**(从仿真流采操作迹)另立 backlog —— 否则与「不产 run」的纪律互相打脸,且工作量差一个数量级 |

## Acceptance

> **判据纪律**:每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。
> **术语(逐字复用,勿混用)**:操作 / 执行迹 / 见证(witness)/ **未证伪** / **不可判** —— 「未证伪」≠「无违例」≠「已证明」(per `ADR-CEP-13` 抓取点 5)。

- [ ] **AC-1(每域模型来源有据 · 文档层)**:新建 ADR 逐条列出「哪条公理依赖哪个域模型语义」+ **域间边构造规则** ⇒ ⛔ 不得只写结论;⚠️ 本条 = **人工评审**(architect/oracle 签核)—— 归档要求「引用一手锚点」属不可机检项,**不写成 checkbox**
- [ ] **AC-2(操作图 ≠ 通道图 · **行为**判据)**:喂一份**只含通道环**的语料 ⇒ 工装**不得**报 `falsified` ⇒ 复用既有反向 fixture(`scripts/test/cep_litmus_falsification_check.sh` 的通道环臂)即可,⛔ 不新造;⚠️ 类型层「顶点结构不同」**不算**判据
- [ ] **AC-3(输入边 vs 派生边分离)**:① 迹里带**显式** `fr` / `ws` 字段 ⇒ 拒收(rc≠0,原因码 `derived-edge-as-input`)② 给定 `rf` 映射 + 写序 ⇒ 断言**派生**的 `fr` / `ws` 集合逐条等于期望集 ③ **物理分离**:通道语料只在 `test/cep_litmus_corpus.json` · 操作迹只在 `test/cep_memory_model_traces.json` ⇒ 断言两文件的记录**互不出现对方的字段**
- [ ] **AC-4(判据可判别 · 双迹)**:两个**具名** litmus 迹(来源写入迹文件的 `source_litmus` / `anchor_id` 字段)—— ① 一个**已知违例**迹 ⇒ 报 `falsified = true` 且给出见证 ② companion 迹(**唯一差异**)⇒ 报**未证伪**;⛔ 「无环迹 ⇒ 未证伪」**不是**判据(任何单线程迹都无环 ⇒ 恒绿)
- [ ] **AC-5(锚点可审计 · 机检部分)**:每条迹含非空 `family` / `source_litmus` / `anchor_id`,且 `anchor_id` ∈ 预定义 allowlist;反 fixture:空锚点 ⇒ 红。⚠️ 「锚点是否**真的**对应该边类」属**人工评审**(并入 AC-1),⛔ 不写成 checkbox
- [ ] **AC-6(域绑定不可判)**:迹里每个线程须解析到一个一致性域;反 fixture:线程未声明域 ⇒ 判定为 `invalid` + 原因码 `unbound-domain` 且 rc≠0(⛔ 不得静默降级为「未证伪」)
- [ ] **AC-7(写序全量量化)**:写序非唯一时须对**所有候选写序**判定;三写迹 ⇒ `co1` 成环 / `co2` 不成环 ⇒ 判定分别为 `falsified=true`(见证含 `co_order == co1`)/ `falsified=false`(唯一差异纪律)
- [ ] **AC-8(`rf` vs `rfe` 分类边界)**:同一份迹仅改 writer 所属域 ⇒ 同域 ⇒ 边类集合 == `{rf}` · 跨域 ⇒ == `{rfe}`;并断言跨域边类集合**精确等于** schema `edge_classes` 枚举(从 `protocol_instance.schema.json` 读,⛔ 不写死字面量)
- [ ] **AC-9(判定三档 + 截断标记)**:判定产物含 `search_truncated: bool` ⇒ `true` 时判定字段**不得**为「未证伪」,须为 `undecided` + 原因码(⛔ 大迹不得静默变假阴性)
- [ ] **AC-10(畸形输入必红)**:语料解析失败 / `rf` 指向不存在的写 / 重复 op id / 自反 `ppo` 边 ⇒ 各一份反 fixture,rc≠0 且原因码唯一;⚠️ 现状 `litmus_falsifier.hh` 对坏输入**静默跳过** ⇒ ⛔ 不得继承该行为
- [ ] **AC-11(闸臂「替换」而非「删除」)**:既有闸的通道环臂(旧语义 = 通道环 ⇒ `falsified`)改写为新语义(通道环 ⇒ **必须不** `falsified`,原因码 `channel-edge-not-op-edge`),并在脚本注释写明「取代关系 + 取代理由」;旧→新语义映射**逐条列出**(⛔ 判据不得被静默删除)
- [ ] **AC-12(主 spec 的 REMOVED requirement)**:delta spec 显式删除「顶点 SHALL 为 `(instance, network, vc)`」;归档后 `grep -c '(instance, network, vc)' openspec/specs/cep-litmus-falsification/spec.md` == 0 ∧ `openspec validate --changes --strict` rc=0
- [ ] **AC-13(新 ADR 的登记与冻结 · bookkeeping 三件套)**:① 新 ADR 登记进 `test/cep_adr_structural_deferrals.json` 的 `decision_adrs` ⇒ `bash scripts/test/cep_adr_structural_check.sh` rc=0 ② 新 ADR 命中 `FROZEN_GLOBS` ⇒ 台账 Entry + `--update --authorized-by "Entry #N"` + **清单单独 commit** + 无参复验 rc=0 ③ `git diff --name-only HEAD~..HEAD -- docs/adr` 只出现 **1 个新增**路径(既有 `docs/adr/ADR-CEP-*.md` **零改动**)
- [ ] **AC-14(不新增 invariant 编号)**:66 号集合逐字不变(`cep_invariant_map_check.py` rc=0);新 ADR 的 Verification 行**复用** `INV-CEP-13-IC1`(per `ADR-CEP-13` §Verification「归因 id 一律复用」)
- [ ] **AC-15(V-13-4 的退场链路)**:二选一并写清 —— (a) 本单元提供运行期载体 ⇒ 追加 SU 声明 V-13-4 判读被取代(**正文不动**),且 `INV-CEP-13-IC1` 的 deferral 登记由 **I-6** 删除(本单元**不重复删**)· (b) 本单元**不**提供 ⇒ 显式 Out-of-Scope 并指向 I-6 后续单元;⚠️ 当前两者皆空 ⇒ 默认谁都以为对方做
- [ ] **AC-16(前置可机判)**:开工门 = `grep -c '"combination_contract"' include/tlm/cep/protocol_instance.schema.json` 输出 **1**(当前 **0**)⇒ 把「I-6 已落地」从隐含假设变成**单命令判据**
- [ ] **AC-17(零回归 · 白名单外)**:ctest 全绿(⛔ 不写死数字)· 全部 `scripts/test/cep_*.sh` rc=0 · `rddf_stage_gate.sh` rc=0;⚠️ **例外白名单** = AC-11 列出的既有判据改写清单(⛔ 其余用例**逐字未变**)

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-litmus-falsification` | **MODIFIED** | 工装从「通道图 + 跨域边」改为「操作图 + 操作迹」⇒ 判定对象与公理**同型**;⚠️ delta **必须含 REMOVED**(旧顶点形态的 requirement)—— 否则归档后主 spec 双真源 |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | 操作级轨迹模型(header-only)· `test/cep_memory_model_traces.json`(操作迹)· 新建 ADR(模型来源与域间边构造规则)· 轨迹测试 |
| **修改文件** | `include/tlm/cep/litmus_falsifier.hh` · `test/cep_litmus_corpus.json`(⛔ 仅改 `_fill_when` 文案 —— architect 裁决 §2.8 P-2 已**明令禁止**由声明项派生填充)· `scripts/test/cep_litmus_falsification_check.sh`(闸臂替换)· `test/cep_adr_structural_deferrals.json` · `docs/adr/README.md` 索引(**人工同步** —— 索引生成器未落仓)· `openspec/specs/cep-litmus-falsification/spec.md`(**冻结面** ⇒ 五步链) |
| **零改动(硬查)** | 既有 `docs/adr/ADR-CEP-*.md` 正文 · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `include/tlm/cep/channel_dependency_graph.hh` · `include/tlm/cep/protocol_instance.schema.json`(R-2e 已裁 ⇒ 本单元不改) |
| **冻结面** | `openspec/specs/cep-litmus-falsification/spec.md` 已在 `FROZEN_GLOBS`;新建 `docs/adr/ADR-CEP-*.md` 亦命中该 glob ⇒ **两处**都须走 `--update --authorized-by` + 清单单独 commit |
| **闸** | ctest +N(操作迹判定;⚠️ 数量**不是**鉴别力指标)· `cep_litmus_falsification_check.sh` 闸臂替换 · `cep_adr_structural_check.sh` 纳入新 ADR |
| **KPI** | 本单元占 1 slot(⇒ 峰值可能 **2/3**,与 I-6 同时在窗口内时)⇒ 归档后释放;⛔ 不写死 `0/3 → 1/3 → 0/3`(会腐化) |
| **风险** | ① 操作图与通道图混用 ⇒ 假绿(**首要风险**;⇒ AC-2/AC-3 物理分离)② 模型来源选型被低估 ⇒ 规模失控 ③ 写序全量枚举的组合爆炸(⇒ AC-7 的迹规模须有上界)④ `verification_scope` 越界改字段 ⇒ 须重开 R-2 |

## 审查轨迹

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|:--:|---|
| 1 | **architect** | **形态已裁**(2026-11-21) | 决策 2 = 本仓不定义边类语义 ⇒ 独立立项;草案随裁决记录 §附录 A 落盘 |
| — | **oracle**(裁决记录内) | APPROVE | 处置已并入 §三 与附录 A |
| — | **critic**(裁决记录内) | APPROVE_WITH_CONDITIONS | 处置已并入 §三 与附录 A |
| 1 | **metis** | **提出 P0 × 7 + P1 × 7 + P2 × 3** | ✅ **全部处置**:P0-1 家族选型改「每域模型来源 + 域间边构造规则」并入 AC-1/AC-7/AC-8 · P0-2 `verification_scope` 移出字段改写(What Changes #5)· P0-3 AC-2/3/4 重写为行为判据 · P0-4 依赖加 ADR-CEP-12 + 前置机检(AC-16)· P0-5 双形态并存改**物理分离**(AC-3③)· P0-6 「不改既有 ADR 正文 vs 新建 ADR」判据区分(AC-13③)· P0-7 `not-applicable` 连带依赖显式入 In-Scope(What Changes #6 + AC-6)· P1-8 AC-5 拆机检/人工(AC-1/AC-5)· P1-9 AC-6 拆分(AC-17 + AC-11)· P1-10 迹来源 = 手写 litmus(What Changes #7)· P1-11 `undecided` + `search_truncated` + 畸形必红(AC-9/AC-10)· P1-12 Capabilities 含 REMOVED(AC-12)· P2-13/14/15 KPI 改写 + 术语表 + ADR 取号与索引同步 |
