# Design — cpptlm-cep-directory-ordering-plane v1.0

> **状态**: 🟡 Proposed(2026-10-25,planner 阶段)
> **上游**: `.rddf/improvements/cpptlm-cep-directory-ordering-plane.md` · `obj §4.0` 队列 #4 · `obj §11` **Entry #62**
> **跨仓权威(只读)**: ArchForge `32-cep-based-coherence-architecture.md §6.0.1` / `§6.0.2` · `22-nsa-fabric-address-spec.md §0.5.4.1`(NS-R2)
> **三轮裁决(2026-10-25,DRI 四问)**:范围**切小**(1 个 tracker 全实现)· **装配闸输入源 = `protocol_instances.json`** · **双挂**(instantiateAll + validate_topology) · **S1-7 (β) 同批补入** INV-CEP-08-IC2

---

## Context

### 现状(队列 #3 完成后)

**① Sharer Tracker 全家族**:`ADR-CEP-04 §3.1/§3.2/§3.3/§3.4/§4/§2 Decision 3` + `openspec/specs/cep-core-contracts/spec.md`(Stage 0)已定义**契约语义词表**(`SharerTrackerIf` API 形态 / caps 4 属性 / D-5 重构 / IC1~IC3),但**仓内 0 实现**。47 个 configs 全部无 CEP section。**无消费者实现**(FullMap 之外的 4 种)已经 ADR-CEP-04 §8 明文拆分到「以协议族为实现载体」的后续 change(obj §4.0 的队列 #6~#9)。

**② Channel Dependency Graph(装配期)**:per `ADR-CEP-09 Decision A` + `INV-CEP-09-IC1`「**不通过 = 配置 fail**」;顶点 = `(协议实例, network, vc_id)` 三元组(§8.2);**S1-1 ②建图层义务未消费**。S3 spike 已实证 8-顶点/8-边/0-跨网边的**判定可计算性**,但仅是**单文件脚本**,不连 `instantiateAll` 装配管线、herent 分离 verdict 未输出(**Entry #55**:「镜像↔权威抗漂移未实证」).

**③ J1/J2/J3**:`ADR-CEP-08 §2 决策 3` 已定义三判据,无仓内实现。决策 4(`INV-CEP-08-IC2` E 态静默升级)是**组合必查项**。决策 7 显式排除「自动判定组合可行性」。

**④ AddressResolver 无 owner ADR**:`ADR-CEP-08 §1.3` 三子平面分工表只写概念;canonical 流 8 的总结点由 `ADR-CEP-02 §CEN-Ib8`(「在 ProtocolEngine 内终结」)给出。

### 目标态

| 层 | 目标 |
|---|------|
| **Sharer Tracker** | `SharerTrackerIf` 单接口(lookup / snoop_begin / snoop_fold / observe_snoop)+ caps descriptor 4 属性 + D-5 全量契约 + **FullMapDirectory 唯一行为实现**成为**可执行契约**(bitmask 64 bit,bit→node_id 显式映射) |
| **Address Resolver** | `AddressResolverIf` **单方法** `resolve(pa)→home_node`(,**spec 首次定义**);与 SharerTrackerIf **正交** |
| **Channel Dependency Gate** | `(实例,net,vc)` 三元组顶点 + 边由声明导出 + **SKIP/PASS/FAIL 三态** + verdict 逐 (network, vc) 输出 + **`protocol_instances.json` 输入源契约** |
| **Ordering Point 三判据** | J1/J2/J3 **装配期静态判据**(机器化)+ **S1-7 (β) 裁决登记** + INV-CEP-08-IC2 实跑 |
| **跨仓** | **零改动** —— 本 change 服从上游权威,不要求上游回改 |
| **冻结面** | **3 个成员**改动(`ADR-CEP-00` / `ADR-CEP-04` / `07号`,均仅追加 Status Update),其余 0 diff |

### 关键约束

| # | 约束 | 来源 |
|---|---|----|
| K1 | 全家族 ADR(CEP-04/08/09/12)都是冻结面,**只能**追加 Status Update | `cep_freeze_check.sh` 34 成员 |
| K2 | **无消费者实现禁止**:5 种 tracker 只做 FullMapDirectory(其余 4 种跟随各自消费队列) | D-α 裁决 |
| K3 | 冻结面改动走**两步授权** | `cep_freeze_check.sh:48-62` + Entry #58 #61 |
| K4 | **断言 ⑦③c 硬编码孤儿行**:M-4 行为侧 / 未排期 / verdict 逐 三项**必须在 §4.0 与 §4.2 两处**都有带 `⏭(孤儿)` 标记的登记行 | `cep_docs_sync_check.sh:1188-1218`硬编码 |
| K5 | delta spec 只保留 `## ADDED Requirements` 段内(锚点**内嵌** Scenario 末尾)+ **Requirement < 500 字符** | `openspec --strict` 门槛 |
| K6 | 断言「输入改变时输出必须改变」+ 纯负向断言必须有正控 | 队列 #2/#3 六类判据 |
| K7 | 机检器锚点走**双态**(归档前 change 目录 + 归档后 main spec),**不复制** | 队列 #3 先例(Entry #61 教训 3) |

---

## Goals / Non-Goals

### Goals

- **G1** `SharerTrackerIf` + caps + D-5 全量契约 + **FullMapDirectory 唯一行为实现**变**可执行契约** ≤ **4-6 周**当量(队列 #4 估时的显著缩小版)
- **G2** `AddressResolverIf`(单方法)与 J1/J2/J3 + `INV-CEP-08-IC2` 静态判据成为**可执行契约**
- **G3** Channel Dependency Graph + 装配期闸 + `protocol_instances.json` 三态契约落地
- **G4** 冻结面 3 个成员仅 Status Update 追加
- **G5** **镜像 ↔ 权威抗漂移**闭合:第 3 个能力中的 fixture 与 S3 spike **同轴化**

### Non-Goals

见 `proposal.md` 的 Non-Goals 8 条(4 种 tracker 实现 / runtime enforcement / `snoop_complete` / 平铺 lookup / `subscribe_remote_snoop` / AddressResolver 并入 tracker / `cep-stub-cache-core.spec.md` 反向改 / `ADR-CEP-08/09/12` 正文改)。

---

## Decisions

### D-α:范围**切小**为「FullMapDirectory 唯一行为实现」

**仲裁** —— 其余 4 种实现跟随**各自消费队列**(队列 #6~#9,per `ADR-CEP-04 §8` 拆分 + `obj §4.0` 排队表):
- 3 个无消费者实现提前一年落码 = 队列 #2/#3 双实证的失败模式(obj §11 Entry #55 ③ 警示 + Entry #60 的教训沉淀)
- `ADR-CEP-04 §8` 自己就把实现拆成 4 个后续 change(`-impls` / `-cxl-dcoh` / `-tilelink-tree`),但不与 `obj §4.0` 11-change 算术融合 → **失指**(见下 D-ε)

**FullMapDirectory 的选型理由**:
- 是 `completeness=EXACT` **无** broadcast fallback 分支(per `ADR-CEP-04 §2 Decision 2`,`PARTIAL→EXACT` 的最简形态)
- 是 07 号 §5.1.2 表 3 个 Ruby 实例的**共同选型**
- IC2 幂等 / IC3 生命周期(5 事件)**必须有 ≥1 个真实实现才可执行断言**(per 队列 #3 G3 先例)
- 覆盖 **`5×4 mapping 表**(`completeness`/`hazard`/`aggregation_at`/`broadcast_fallback`)** 里的 FullMap 行 + DcohSymmetric 行

**备选与排除**:

| 备选 | 排除 |
|---|---|
| 12 周全量(5 种实现全做) | 4 个无消费者实现 → 发布即堆死代码;R9 风险(`obj:535`) |
| `FullMap + BroadcastNull`(双实现) | BroadcastNull 无消费者(自失效类未排期),不进 AC;builder 阶段若估时余量可做(不写进 Acceptance) |

---

### D-β:**三子平面分工的裁决**

per `ADR-CEP-08 §1.3` 三子平面分工表,裁决:Address Resolver / Sharer Tracker / Ordering Point 三者**正交但耦合**:
- **Address Resolver** = 「拓扑归属判定」,单方法 `resolve(pa) → home_node`,**不感知协议形态**
- **Sharer Tracker** = 「per-node 状态维护 + cap/hazard」,lookup 承受 **`want_read/want_write/want_own/evict`** 4 种规范 intent
- **Ordering Point 三判据** = 「装配期静态检查 J1/J2/J3」,**组合必查项**含「**E 态静默升级**」(per `INV-CEP-08-IC2`)
- 三者**共享** `ResponderSet` 形态(change 内 spec **首次定义** bitmask 64 bit + bit→node_id 显式映射,per ADR-CEP-04 先例措辞)

**备选**:AddressResolverIf 并进 `SharerTrackerIf::lookup`(❌ _通过 per `ADR-CEP-04 §1.3._lookup 承诺降级」的教训:TileLink 的 `ESCALATE_UP` 下 lookup = 树遍历;若把 Address Resolver 塞回,tracker 强制感知拓扑,把「per-node 状态维护」与「全局地址归属解析」耦合)

---

### D-γ(裁决:装配期架构:**双挂共享判据,禁双实现**)

**双挂形态**:
| 挂点 | 触发时机 | 输入 | 动作 |
|---|---|---|---|
| **`src/core/module_factory.cc::instantiateAll()`** | `validateConfig` 之后 / 实例创建之前;仅当 **`final_config` 含 CEP section**(如 `cep_protocol_instances` 字段)才触发 | `include/tlm/cep/protocol_instances.json` | 无环 fail ⇒ 返回 false(**`INV-CEP-09-IC1`**)  |
| **`scripts/CMakeLists.txt::validate_topology`** target | 人工 `cmake --build build --target validate_topology` | 同上(共享判据文件) | 有环详报(离线验证用) |

**输入源**:`include/tlm/cep/protocol_instances.json`(本仓机器可读声明,按 `protocol_instance.schema.json` 校验)
- **诞生即不冻结**(per `Entry #56` 先例);首个 implemented 实例跑过后再纳入(S1-4 复核口径)
- fixture 内容:ArchForge `32 号 §6.1`(moesi_gpu)的**派生副本**;`cep_decl_alignment_check.sh` **共享同一判据实现**(禁双实现)

**三态**:["SKIP", "PASS", "FAIL"](per metis)
- **SKIP** = `protocol_instances.json` 不存在 / 为空 ⇒ 与 PASS **可区分**(显式打印);per `cep_decl_alignment_check.sh` `[SKIP]` 先例
- **PASS** = 全部声明通过
- **FAIL** = 有环 / IC1 违例 / J1/J2 违例 / IC2 违例

**verdict 输出**: *[verdict]* net=<net> vc=<id> … **机器可 grep 格式**(per metis)。

**备选**:
- 单挂(❌ per `ADR-CEP-09 抓取点 1` 要求「拓扑校验 target 必须执行无环性检查」字面 —— 现有 `validate_topology` target 的 5 个 config 全是 **无 CEP section**,SKIP 化并用 Status Update 说明不是推荐方案)
- 嵌入 config schema(❌ `fabric.schema.json`/`protocol_instance.schema.json` 冻结面,改动权威面大;且 53 个 configs 中绝大多数不涉及 CEP)
- `cep_decl_alignment_check.sh` 单挂(❌ 该脚本在 ArchForge 缺席时 `[SKIP]` 退出 0,不能承担装配期 fail 义务)

---

### D-δ:**S1-7 (β) 的裁决本 change 内决策层落实**

per `ADR-CEP-05 §13.1` 与 `ADR-CEP-08 决策 4` **同批**约束:
- **裁决对象**:Stub 的 `perm` 4 值锁(per `ADR-CEP-02 CEN-Ib11`)在**组合**语义下是否需要**第 5 种判别位**(per 队列 #3 的 Q-3b:新字段必须表达「`UNIQUE` 可静默升 `WRITEBACK`」)
- **裁决形态**:决策层(非实现层);裁决结论**作为交付物**登记(红绿分离)
- **同批**:决策 4 compositional check 的输入字段形态取决于该裁决;**INV-CEP-08-IC2** E 态静默升级检查项**一并落地**(对 `moesi_gpu` 实跑检查)
- **不反向改** `cep-stub-cache-core.spec.md`(冻结面);**不落** schema 演进(归队列 #6)

---

### D-ε:**`ADR-CEP-04 §8` 四个 change 名的失指处置**

`ADR-CEP-04 §8` 列的 4 个后续 change(`cpptlm-cep-sharer-tracker-base/-impls/-cxl-dcoh/-tilelink-tree`)与 `obj §4.0` 11-change 算术**冲突**(如队列 #3 的 D11 同构),处置:
- **给 `ADR-CEP-04`** 追加 `## Status Update`,声明这 4 个 change 名已被 `obj §4.0` 重排取代;Sharer Tracker 的**实现拆分**改由「**以协议族为实现载体的队列 #6~#9**」承接(即「单接口 + N 实现」的 N 个实现**跟随各协议族**落地)
- **`ADR-CEP-04` 的 public surface**(`SharerTrackerIf` / caps / IC1~IC3)**零改动**(per **本 ADR 冻结面条款**:K1 ADR 不可变)
- **`ABP-CEP-04 §8` 的四 change 名永久失指** ⇒ 给 `07号` 同步一行**「SharerTracker 行 change 引用」**的修正是必要性(见 D-ζ)

---

### D-ζ:**`07号 :674` 的 SharerTracker 行 change 引用修正**

07号(=`docs/architecture/07-coherence-exploration-platform.md`)是**冻结面成员**,per `obj §4.7.3 D4` 落点,D4 说「相关 ADR 的 Status Update 只追加不改正文」。**裁决**:对 `07:674` 的`SharerTracker 行`,`“Change` 引用”**不改内容本身**(per `K1` 只追加行为) —— 我们在**Status Update 段落中**声明「该行的引用已修正」,但**该行正文**需要被**修正吗**(冻结面允许「追加 Status Update」不等于「允许改`inline`内容」)?
- **裁决**:给 `07号` **追加一个 Status Update 段**(不改该行本身);并在**该 Status Update**段落中**并列出**「SharerTracker 行的 change 引用」**的**已批准修正。若弹出 business 语义refactor 或 audit 报红再以 **`VIRTUAL_PATHS`** 类推登记。

---

### D-η:**S1-1 ②建图层的「装配期钩子落点」的对抗反论证**

per metis blocker:「本仓目前**没有任何机器可读的协议实例声明文件** ⇒ 装配闸读什么?」

**裁决**:新建 `include/tlm/cep/protocol_instances.json`(本仓机器可读声明),**不冻结**(per Entry #56 先例),并**明确**:
- `include/tlm/cep/protocol_instances.json` 是 **ArchForge `32 号 §6.1/§6.2` 的派生副本**(per `fabric.json` 同型先例)
- moesi_gpu / mesif 两个 fixture 声明**显式标注 `status: planned`** ⇒ 与 S3 spike 一样,这些不触发 `S1-2` 的「装配后实现侧复核」(per `ADR-CEP-12 INV-CEP-12-IC3`)
- **`S1-2` 的复核动作**(首个 `implemented` 实例装配后实现侧复核)仍然 **⛔ 未触发** —— 需要一个**真实的** `status: implemented` 实例;本 change 不引入
- fixture 的内容**不硬编码**(per Entry #55 抗漂移前提):它**读取** `protocol_instances.json` 而**非硬编码**

---

### D-θ:16 处主要内嵌锚点 / **对单文件头 vs 多文件的权衡**

per 队列 #3(队列 #2/#3 也发生过「单文件头即可实现」),裁决:
- **本 change 使用三个独立的头文件**:`sharer_tracker.hh` / `address_resolver.hh` / `channel_dependency_graph.hh`(**比队列 #3 的 `stub_cache_core.hh` 单文件头扩得多**)
- 推断:**Sharer Tracker 接口面更大**(4 方法 × 4 caps × 5 实现 × 生命周期)≈ 300+ 行;**AddressResolver** 是小接口;**Graph** 是 builder+DFS
- **全部 header-only,不进 `ModuleFactory`**(per 队列 #3 的喜好:测试用例**由夹具装配**)
- **`protocol_instances.json`** 独立(initiate schema 需独立文件);`cep_decl_alignment_check.sh` **共享同一判据实现**(禁双实现,per metis)

---

## Risks / Trade-offs

| 风险 | 影响 | 缓解 | 关联 AC |
|---|---|---|---|
| 22 Scenario 过度承诺 | 不可验证的 AC 进入 spec ⇒ 队列 #2 失败模式重演 | A-1-9 / A-2-6 显式降级登记;对账测试断言 Scenario 锚点均真实存在 | A-1-9 / A-2-6 |
| S1-1 ②「禁离线脚本」义务 | 必须真实 hook 到 `instantiateAll`,否则为「声称装配的**伪 hook**」 | A-3-4/S 走真 `instantiateAll` + 正控注入(有环 fail) + **SKIP/PASS/FAIL 三态** | A-3-4 / A-3-7 |
| FullMap 之外 caps 行为不可断言 | 5×4 表的数据级断言有**="//**" 迷失风险 | A-1-3 显式「数据级契约」 + A-1-9 「仅 schema 枚举」 | A-1-3 / A-1-9 |
| 装配闸双挂复制守卫 | 违 K6(误报与漏报同为缺陷)/ 纪律 ⑤ 等价 | 双挂 **共享** 同一判据实现(**禁双实现**) | D-γ |
| bitmask 形态无权威 | 4 实现 caps 表可能 mismatch | A-1-2 显式 **spec 首次定义** | A-1-2 |
| GPU config 46 个无 CEP section | 闸会** erroneously **报红 | A-3-7 **SKIP** 显式打印,与 PASS 可区分 | A-3-7 |
| 单人仓的独立性缺口 | 评审结论的独立性受限 | 本仓的「评审」由 oracle / metis / critic subagent 承担 —— **不等价**于真人外部评审,缺口已在 `obj §8.1` R10 登记 | — |

---

## Open Questions

| # | 问题 | 当前处置 | 归属 |
|---|---|---|---|
| **Q1** | **`AddressResolverIf` 的三种实现形态**(per-node 树遍历 / home_node 参数注入 / uniform(self const 判断)…)是否本 change 内给出指定实现? | **不做**(推到队列 #6+ 的各协议族;本 change 只落 **`resolve 接口` + spec 首次定义**) | Phase 1 决定 |
| **Q2** | `validate_topology` 的 **128 configs** 目标集是否本 change 同步扩到含 CEP section 的 config? | **不扩**(仅**现有** 5 个 target);若扩,落 Phase 1(builder 阶段判断) | Phase 1 |
| **Q3** | 是否本 change 内落 `INV-CEP-04-IC1` 的机检形态(字面值扫描)即**不做** schemas 层显式枚举? | 以 **INV-CEP-04-IC1 字面值扫描**(per `ADR-CEP-04 §4.0 oracle SHOULD-FIX #7 字面值收敛先例`)× **A-1-8**;schema `SharerTrackerCaps.type` 交给 Phase 1 | Phase 1 |
| **Q4** | `FullMapDirectory` **是否提供**「`completeness=EXACT` ⇒ `broadcast_fallback=false`」这样的**断言**(per `ADR-CEP-04 §2 Decision 2`)? | **是**;per A-1-3 数据级 mapping 断言 | Phase 1 |

---

## 与队列 #3 的先例对照

| 维度 | 队列 #3(2017-Q2,单 change 双 Phase) | **队列 #4(本 change)** |
|------|---|---|
| 建目录时 | planner + proposal + design + 2 delta | planner + proposal + design + **3 delta** |
| 触发问题 | M-4 决策(旧场景,跨仓权威已闭) | 4 个 Stage 1 准入前置项(owner 重指向 / S1-7 (β) / S1-1 ②建图层 / S1-2~4) |
| **估时** | 12 周 → **4-6 周当量** | 12 周 → **4-6 周** 当量 |
| 复用契约 | Stub Cache **不进 ModuleFactory** / testbench 定位 | Sharer Tracker **不进 ModuleFactory** / testbench 定位 |
| anchor 锚点形态 | 内嵌 Scenario 末尾 / 9+8 = 17 anchors 全等 | **内嵌 Scenario 末尾 / 22 anchors 全等** |
| 机检器 | **共享同一判据实现**(禁双实现) | 同样(**双挂共享判据**,禁双实现) |
