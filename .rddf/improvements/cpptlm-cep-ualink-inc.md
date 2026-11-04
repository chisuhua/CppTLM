---
name: cpptlm-cep-ualink-inc
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-01
  - ADR-CEP-02
  - ADR-CEP-03
  - ADR-CEP-04
  - ADR-CEP-09
  - ADR-CEP-11
  - ADR-CEP-12
状态: 🟡 **进行中**(2026-11-04)—— 队列 #14;**两段式**(形态丙,per 用户裁决 2026-11-04);**开工前置 = `ADR-CEP-01` / `04` / `09` 的三则 SU 已签发**(per 台账 Entry #193)⇒ ✅ **已满足**;**规约层 change 目录已建**(`cpptlm-cep-ualink-inc-state-spec`,per Entry #195)
created: 2026-11-04
---

# improvement: cpptlm-cep-ualink-inc

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 🟡 **待启动**(2026-11-04)· **队列 #14**(`obj-cep-platform §4.0` · **Stage 3** · **2030-Q1** · 📋 Planned)
> · **两段式**(形态丙,per 用户裁决 2026-11-04):第 0 个 change = `cpptlm-cep-ualink-inc-state-spec`(**规约层**)+ 第 1 个 change = `cpptlm-cep-ualink-inc`(**实现层**)
> · **开工前置**:`ADR-CEP-01` / `04` / `09` 的 **2026-11-03/04 三则 SU** ⇒ ✅ **已满足**(台账 **Entry #193**)
> · **合规边界**:UALink Common 2.0 是**公开规范**(用户裁决 2026-11-03)⇒ **可投喂 AI** ⇒ Task 0 普查可引**条款级定位**
>   (⚠️ 与队列 #13 的 NVLink SHARP **不同** —— 后者受 NDA 约束 ⇒ 禁投喂规范正文)
> · **VC 路径**:**路径 D**(共享 VC + 可机器判定的证书)—— 用户裁决 2026-11-04;裁决载体 = `ADR-CEP-01` SU#4
> · **估时**:**10 周 → 14 周**(两段式;依据 = 队列 #13 的先例 8→13 周)
> · **KPI**:Proposed 并发 **当前实测 1/3**(规约层 change 已建 ⇒ 占 1 slot,per Entry #195;规约层归档后 → 0/3 ⇒ 启实现层 → 1/3 ⇒ 实现层归档 → 0/3)

---

## 架构依据

### ① Decision D 已重开 ⇒ 本队列是它的**第一个真实消费者**

`ADR-CEP-01` 的 SU 2026-11-03(#4)(台账 **Entry #193**)把抓取点 3 由「单一要求」改为**两种满足模式**:

| 模式 | 形态 | 本队列适用? |
|---|---|---|
| **M-1 隔离** | 集合流量走**独立** VC | ❌ 本拓扑**无空闲 VC**(`compute_noc` / `ualink_fabric` 的 0-3 均被 coherence 占满) |
| **M-2 共享 + 证书** | 集合流量**复用** coherence 已占用的 VC + **四条可机器判定的证书**(C-1′ / C-2 / C-3 / C-4) | ✅ **本队列走此模式** |

⇒ 本队列的 **VC 路径** = M-2 ⇒ 集合平面**继承**该 network 既有实例的 VC 空间(**不声明**自己的绑定)
⇒ 载体 = `collective_planes.json` + `collective_plane_contract.hh`(均**非** M1 冻结件)⇒ **零冻结件改动 / 零跨仓依赖**。

### ② 机制侧与决策侧已各归其位

- `ADR-CEP-09` 的 2026-11-03 段:**需求归 01 / 机制归 09**;集合平面**不进图**(它**不引入**新顶点 / 新边 ⇒ 无环性结论**直接继承**既有实例的图);
  并登记 **`IC1` 的鉴别力边界**(三项仍在:边端点良构 / `vc_id` 越界 / 声明边表自身无环;⛔ 对 `vc_binding` 的**语义分配零鉴别力**)。
- `ADR-CEP-04` 的 2026-11-03 段:tracker 选型 = **候选集(7 值)+ 语义层判别依据 + 逐实例理由登记**
  ⇒ ⛔ **不存在**可机械派生的全局规则(已被仓内 6/7 存量声明反证)。

### ③ 协议侧事实

| 面 | 事实 | 出处 |
|---|---|---|
| adapter 与通道 | `UalinkAdapter` · Stage 3 · Station / Port / Lane;TL Load / Store / Atomic | 07 号 §5.1.1 UALink 行 |
| **INC 原语** | **ReadReduce / WriteMulticast / Broadcast / AllReduce** | 同上 |
| 原语集合的定位 | 「UALink → **全集合族**」(对比「NVLink → SHARP op 集」) | `ADR-CEP-03:70` |
| 版本锁 | `ualink-2.0` / **UALink Common 2.0** | `ADR-CEP-11 §3` |
| fabric | `ualink_fabric` `vc_count = 4`;VC0=Fault Request / VC1=Atomic / VC2=Remote Query / VC3=Response(**已锁定**) | `fabric.json`;ArchForge `22-nsa:147` |
| `moesi_gpu` / `mesif` 在该网的映射 | `request→0 / atomic→1 / query→2 / response→3 / snoop_resp→3`(**两者逐类相等** ⇒ C-1′ 的继承良定义) | `protocol_instances.json` |

⇒ ⚠️ `compute_noc` 上 `tilelink_c` 与其余 6 实例**不一致** ⇒ **该网不得用 M-2**(per `ADR-CEP-01` E.3.3)。

---

## 范围

### In Scope

| 面 | 内容 |
|---|---|
| **规约层**(change 0) | INC 四原语的**可执行语义契约**(哪些进判据 / 哪些登记 pending)+ 四条证书的**规约面**(边表 + 逐边依据)+ Task 0 公开来源普查产物 |
| **实现层**(change 1) | `UalinkAdapter`(独立头 · 零自有表副本 · 无 `transition_*`)· `FabricCollectivePlane`(数据路径**真可达**)· `ualink_4` 实例声明(13 键)· 集体面 **M-2** 落地(`status` 第二取值 + `V6-d` 四条证书载体)· 计数链同步 |
| **判据** | 正向(每条负向断言配正控)+ **反向 fixture**(每条必红 + 归因 id **精确相等**)+ 归因 id 元测试 |
| **文档** | 07 号 §5.1.1 UALink 行 + §11 计数 · `scripts/README.md` 守门表 + 反向 fixture 登记 |

### Out of Scope

- 硬件 **opcode / 指令编码** · 真实 **带宽 / 延迟**结论 · UALink 的**一致性状态机**(它没有)
- **扩 `vc_count`**(路径 A)—— 本仓与 ArchForge 的 4 VC 是**硬件事实** ⇒ 不得为建模便利改写
- 真实 UALink **线格式 / 物理拓扑** · 新 canonical 流 · 新 reduce 算子族(唯一真源 = 既有 `ReduceOp` 恰 6 值)
- `ualink_4` 的 `flow_support` **取值** —— 由 Task 0 普查确定,⛔ **不得**推理填空

---

## Why

1. **平台最后一个队列**:Stage 3 = 收官(`07 号 §5.1`:「Stage 3 | UALink In-Network Compute | 1」⇒ 全部 14 个 change 的第 14 个)⇒ 本队列交付后 CEP 的协议族覆盖闭环。
2. **唯一还没覆盖的「跨协议 collective」对**:队列 #13 的 NVLink SHARP 只覆盖 **SHARP op 集**,而 UALink 是**全集合族** ⇒ 二者的 collective 语义面**不重叠**。
3. **INC 是「coherence 可见性 × 网内归约」交叉的唯一实例**:其余 6 个协议实例**全部**是 coherence-only ⇒ 平台此前答不了「网内归约时该区域的 coherence 可见性怎么办」。
4. **Decision D 的重访触发 ② 已命中**:不改 ADR 就**无法开工** ⇒ 本队列是 Decision D 的**第一个真实消费者** ⇒ 也是它的**第一个可证伪对象**。

---

## What Changes

### 形态:**两段式**(per 用户裁决 2026-11-04)

| # | change | 交付面 |
|---|---|---|
| **0** | `cpptlm-cep-ualink-inc-state-spec`(**规约层**) | ① INC 四原语(ReadReduce / WriteMulticast / Broadcast / AllReduce)的**可判定**语义;② **AllReduce = REDUCE + BROADCAST 的复合**(⛔ **不是**第 5 个原语,per `ADR-CEP-01:94` / `ADR-CEP-03:75`);③ reduce 算子**只引用**既有 `ReduceOp`(恰 6 值)⇒ **不新造**枚举;④ **partial failure** 挂既有 `CollectiveEvt::Fault`(per `ADR-CEP-01` V5);⑤ 四条证书的**规约面**(边表 + 逐边依据 + 参照域);⑥ Task 0 公开来源普查产物(UALink Common 2.0 **可投喂** ⇒ 条款级定位) |
| **1** | `cpptlm-cep-ualink-inc`(**实现层**) | ① `UalinkAdapter`(零副本 · 无转移表 · 委托规约载体);② `FabricCollectivePlane`(REDUCE / MULTICAST / BROADCAST 数据路径**真可达** ⇒ ⛔ **不得**像 `LocalCollectivePlane` 那样返 `DataPathNotImplemented` 却宣称 INC 已实现);③ `ualink_4` 实例(13 键 · `ualink-2.0`);④ 集体面 **M-2**(`status = "shared_no_isolation"` + `V6-d` 四条证书载体 + `evaluate_vc_reservation` 的 `status` 值域放宽为 2 值);⑤ 计数链同步(7 → 8)+ 07 号三处 |

### ⛔ 本层**不做**的(防范围蔓延)

- **不改** `vc_count` / `fabric.json` / `protocol_instance.schema.json` ⇒ **零跨仓、零硬件保真度倒退**
- **不改** `ADR-CEP-01` / `03` / `04` / `09` 的**正文**(只消费已签的 SU)
- **不**为 UALink 造**新的** collective plane 语义(复用既有 `CollectivePlaneIf` 7 方法面)
- **不**新建 reduce 算子族 · **不**新增 canonical 流

---

## Acceptance

- [ ] **Task 0 硬门**:产出 `docs/research/<date>-cep-ualink-inc-anchor-summary.md` —— UALink Common 2.0 **条款级**定位(公开规范 ⇒ 可投喂)· **sha256 钉版** · **零**「规范正文整段复制」· 逐条标注**哪些语义进判据 / 哪些 pending**
- [ ] **INC 四原语**的规约层判据就位:`AllReduce` **不得**被列为第 5 原语(有独立判据 `CP-PRIM-SET` 守着)· `ReduceOp` **恰 6 值**被断言且**不新增**枚举
- [ ] **partial failure** 走既有 `CollectiveEvt::Fault` ⇒ ⛔ **不新增** `CollectivePlaneIf` 方法(7 方法面已硬锁)
- [ ] **四条证书**(C-1′ / C-2 / C-3 / C-4)各有**可执行反向 fixture**且**因正确原因红** ⇒ 归因 id **精确相等**命中(`CP-03-VC-CERTIFICATE/<子标记>` / `CP-09-IC1`)
- [ ] ⛔ **不**把「重跑 `IC1`」当证书(实测**恒真**:边按 network 声明 ⇒ 诱导子图恒无环)⇒ 判据**必须**落在 C-1′(语义继承)· C-2(`fabric.json` 的 `vc_count`)· C-3(控制面互斥)· C-4(未检验项登记)
- [ ] **未检验项 U-1 / U-2** 显式登记且**不**含「已验证」字样(U-1 = H6 隔离假设;U-2 = 同 VC 内两类流量的相互阻塞**不被图模型表达**)
- [ ] `ualink_4` 的 `sharer_tracker.impl` 落在 `ADR-CEP-04` SU 的**候选集(7 值)**内 ∧ 有**理由登记**(载体 = `protocol_instances.json` 顶层 `sharer_tracker_rationales{}`)∧ 理由指向公开来源或既有裁决条目 ⇒ ⛔ **不**用 `flow_support` 派生(已被反证)
- [ ] **计数链同步**:实例 7 → 8 · `no_upgrade_instances` 3 → 4 · `implemented` 7 → 8 · 07 号 §5.1.1 状态列 + §11 三处 ⇒ 逐条保留来源注释
- [ ] **冻结面**闭环:① 台账 Entry(逐件列名)→ ② commit → ③ `--update --authorized-by` → commit → ④ `--update` → commit;本队列的**唯一**需两步授权的下游冻结产物 = `openspec/specs/cep-collective-plane-contract/spec.md`(范围 = `V6-c` 适用条件 + 新增 `V6-d` + `status` 第二取值语义;**不含**其 `:35` 的「恰含四组」)
- [ ] **收口**:`ctest` 全绿 + 门禁 rc=0 + `openspec validate --strict` rc=0 + `format.sh --check` rc=0 + KPI 回填

---

## Capabilities

| 能力 | 内容 | 载体 |
|---|---|---|
| **`cep-ualink-inc`**(新) | UALink INC 的**规约层 + 实现层**契约:四原语语义 · `UalinkAdapter` · `FabricCollectivePlane` · `ualink_4` 实例 · M-2 集体面 · 四条证书 | 本队列两段式 |
| **`cep-collective-plane-contract`**(delta) | `V6-d` 共享模式证书 + `status` 第二取值(`shared_no_isolation`)+ **`V6-a` 的 `status` 值域放宽为 2 值** | 本队列实现层 |

---

## Impact

| 面 | 影响 |
|---|---|
| **冻结面** | 已签 **3** 份 ADR 的 SU(`ADR-CEP-01` / `04` / `09`,台账 Entry #193)· 归档时 **1** 份 M1 main spec 同步(`cep-collective-plane-contract`) |
| **非冻结面** | `collective_planes.json` / `collective_planes.schema.json`(`status` 第二取值)· `collective_plane_contract.hh`(`V6-d` + `evaluate_vc_precondition` + `evaluate_vc_reservation` 的值域放宽)· `collective_plane.hh`(`FabricCollectivePlane`)· 新增 2 个头 |
| **跨仓** | **零** —— `fabric.json` / `fabric.schema.json` / `protocol_instance.schema.json` 全部**零 diff** ⇒ `cep_decl_alignment_check.sh` ⑥ 的 ArchForge 镜像闸**不触发** |
| **计数链** | 实例 7 → 8 · `no_upgrade_instances` 3 → 4(若 `ualink_4` 不声明静默升级)· `implemented` 7 → 8 · 07 号 §11 三处 |
| **平台能力** | 首次覆盖「coherence × collective × **网内归约**」;`FabricCollectivePlane` 首个**数据路径真可达**的 collective plane;`BroadcastNull` 之外第 2 个现役 tracker 载体被消费 |
| **残余风险 ①** | **U-1**(「VC 隔离确实避免协议级死锁」)在 M-2 下**永远无法检验** ⇒ 登记为未检验 ⇒ 该假设**不得**再被当作已满足的前提 |
| **残余风险 ②** | **U-2**(同 VC 内 coherence ↔ collective 的相互阻塞)**不被 VC 级依赖图表达** ⇒ 只能靠 C-3(控制面互斥)+ 显式的顺序声明 ⇒ 已登记 |
| **残余风险 ③** | Task 0 的公开来源若不足以钉死 `WriteMulticast` 的部分失败语义 ⇒ 该项登记 **pending** ⇒ **不得**推理填空 |
| **可逆性** | 中:新增 2 头 + 1 实例 + 1 枚举取值 ⇒ 回退 = 删 `ualink_4` 条目 + 撤 `status` 第二取值 + 撤 `V6-d`;⚠️ 已签 ADR 的 SU **不可回退**(只能再追加 SU) |
