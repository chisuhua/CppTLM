---
name: cpptlm-cep-nvlink-sharp
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-01
  - ADR-CEP-02
  - ADR-CEP-03
  - ADR-CEP-04
  - ADR-CEP-11
  - ADR-CEP-12
状态: ✅ **已归档**(2026-11-03)—— 队列 #13;**两段式均已完成归档**(规约层 Entry #186 · 实现层 Entry #190);**两段式**(形态丙,per 用户裁决 2026-11-03);**开工前置 = `ADR-CEP-01`/`02`/`03` 定稿**;`sharer_tracker.impl` = **`BroadcastNull`**(本队列补行为类,per 用户裁决 2026-11-03)
created: 2026-11-03
---

# improvement: cpptlm-cep-nvlink-sharp

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 🟡 待启动(2026-11-03)· **队列 #13**(`obj-cep-platform §4.0` · Stage 2 · **2029-Q4**)
> · **两段式**(形态丙,per 用户裁决 2026-11-03):第 0 个 change = `cpptlm-cep-nvlink-sharp-state-spec`(**规约层**)+ 第 1 个 change = `cpptlm-cep-nvlink-sharp`(**实现层**)
> · **开工前置**:`ADR-CEP-01` / `ADR-CEP-02` / `ADR-CEP-03` **定稿**(用户裁决 2026-11-03)
>   ⇒ ✅ **已满足**(三则 **2026-11-03 定稿签发 v1.0**,per 台账 Entry #169 / #170)
>   ⚠️ **2026-11-03 订正**:原文写「三则当前仍是「🟡 起草中(v0.6)」⇒ **前置尚未满足**」⇒ 已被签发取代
> · **VC 空间**:✅ **已裁**(2026-11-03,台账 Entry #171 —— **Decision D「显式声明门」**)
>   ⇒ 本拓扑**无隔离 VC**(coherence 占满 `compute_noc` / `ualink_fabric` 的 0-3)⇒ **不得占用承载 coherence 的 `compute_noc` / `ualink_fabric` 的 VC 与传输通道**(仅本地模型);⚠️ **能力域 = 节点内不受此限**(per `ADR-CEP-01` SU 2026-11-03(#2) G.1)
>   ⇒ `collective_planes.json` 须给 `vc_reservation_declaration` 四字段(`status`/`reason`/`owner`/`revisit_trigger`)
>   ⚠️ **行为面待 builder**(判据 `CP-03` 的 `V6-a` / `V6-c`;见 `obj-cep-platform §10` 的登记)
> · **载体归属**:形态丙 = **甲(第 7 个 coherence 实例 `nvlink_4`)+ 乙(collective plane 的 SHARP 扩展)合流**
> · **`sharer_tracker.impl` = `BroadcastNull`**(用户裁决 2026-11-03)⇒ 本队列**补其行为实现**(traits 行已登记 · 行为类**从未落地**)⇒ 顺带闭合 07 号 §11 的「`BroadcastNull` 仅声明层 traits」缺口
> · **KPI**: Stage 2 = **2 slot 串行**(规约层 → 实现层);当前实测 **1/3**(唯一 active = `cpptlm-cep-dual-port-inbound-dispatch`,per 台账 Entry #308;⚠️ 全局 baseline = **1/3**（**活口径**:逐次 KPI 变更的**归档流水**见 objective `obj-cep-platform.md` §11 台账(Entry #28 ~ #308 的 KPI 条目)⇒ **本行不堆历史快照**;⚠️ `docs_sync_check` 的KPI 断言只抓「当前/现行/真实」限定的当前态 ⇒ 历史叙述不影响校验）)
> · **估时**: 规约层 **5 周** + 实现层 **8 周** = **13 周**(标称 10 周 ⇒ 两段式 + 2 个载体面 + 行为类补齐 + VC 表 ⇒ 上修;per 用户裁决「估时以质量优先」)
> · **⚠️ 合规**:NVIDIA NVLink SHARP 正式文档**不可投喂 AI**(照 CXL 的 Evaluation Copy Agreement + AI AUP 同款处理)⇒ 普查只能走**公开非规范来源**(NVSHMEM 开源实现 / 公开白皮书 / 学术论文)

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `ADR-CEP-01` | Fabric Collective Plane 作为**旁路平面** · §3.1 barrier 分类 litmus · §3.3 抓取点 | SHARP 属 **collective 平面**(非 coherence 目录);抓取点 1「Collective Plane → Directory 调用边**必须为零**」(机器检查)⇒ `BroadcastNull`(`scope=NONE`)是唯一零目录边的选择;抓取点 3「集合流量走**独立 VC**」⇒ ⚠️ **本拓扑无隔离 VC**(**Decision D**,Entry #171)⇒ 须交付「`unavailable` + `vc_reservation_declaration` 四字段」,**不得**分配独立 VC |
| `ADR-CEP-02` | canonical **10 流**清单 + barrier 重钉为 Ordering Point | SHARP 的 TL atomic **不得**新造第 11 条流 ⇒ 必须归入既有 10 流 ⇒ 清单**零 diff** |
| `ADR-CEP-03` | §3.1 **R1~R5** + §3.2 non-cacheable 降级(CEP-13 首次定义)+ §3.3 ordering **A/B/C/D** | **规约层载体的主干**:分类判据 / 区域互斥 / epoch 纪律 / 可观测性 / 跨平面 ordering |
| `ADR-CEP-04` | §2 caps 表(6 实现)· §13「克隆 + 纸面 caps」禁令 | `BroadcastNull` traits 行已登记 ⇒ 本队列**首次兑现**其行为面(⇒ 非纸面 caps);**禁止**用「克隆 + 抄一份 caps」交差 |
| `ADR-CEP-11` | §3 版本锁表 | NVLink 版本锁 = **`nvlink-4.0`**(在锁表内 ⇒ 装配期不 fail) |
| `ADR-CEP-12` | §3 schema(13 键)· §11 实例级权威分区 · §13 声明权威 | 实例声明面(13 键齐备性**机器驱动**:`required` 实读) |
| 07 号 §5.1.1 | NVLink 行(`NvlinkAdapter` · Stage 2 · 工业 **6 族 / 8 实例**)· §5.4 canonical 10 流 · §11 状态表 | 本队列的**载体落点**与计数口径(⇒ 落地后需同步 §5.1.1 状态列与 §11 两行计数) |
| 队列 #6 先例 | `cpptlm-cep-collective-plane-base` 已交付 `CollectivePlaneIf`(**7 方法**,含 `group_register_region`)+ `NullCollectivePlane` + `LocalCollectivePlane` + `collective_planes.json`(**本仓自持**载体) | collective 面**载体已在** ⇒ 本队列**复用**而非重建(⇒ 只扩 `collective_planes.json` 的**三块 + `vc_reservation_declaration`**(Decision D)) |
| 台账 Entry #166 / #167 | 队列 #12 两段式归档 + 遗留订正 | 移交链 owner 已转指 **#13**;队列 #12 的**两段式 + 5 项载体形态决定项 + Task 0 普查 + 反向 fixture** 全流程为本队列的**可复用先例** |
| 用户裁决 | 2026-11-03 | 形态**丙**(两段式)· 开工前置含「ADR 定稿」· `sharer_tracker.impl` = `BroadcastNull` + 本队列补行为类 |

---

## 范围

### In Scope

- **第 0 个 change(规约层)** `cpptlm-cep-nvlink-sharp-state-spec`:
  - `include/tlm/cep/nvlink_sharp_state_spec.hh`(新 · header-only)
  - **SHARP 分类判据(R1 / R2)**:opcode **永不作分类器**(源端只发一种 ATOMIC);**唯一分类点 = fabric ingress**(`dest_addr ∩ collective_region_table` && `group_epoch_valid`)
  - **区域互斥与 epoch 纪律(R3 / R4)**:`group_register_region` 触发 coherence drain(**non-cacheable 降级**,CEP-13)⇒ drain 失败 ⇒ 注册失败且**不创建 epoch**;`group_destroy` 触发 stale copy 失效化 ⇒ 区域回归 coherence 域;epoch 未 commit 前该区域的 coherence 请求**正常服务(放行)**(⚠️ 2026-11-03 订正 —— 原写「被拒」与 `ADR-CEP-03 §3.1` R4 相反;per `ADR-CEP-03` SU 2026-11-03(#2) 的 S2 / `ADR-CEP-03 §4` 的 S1)
  - **可观测性(R5)**:divert 计数器 + fail-stop 断言(「epoch commit 后仍有 coherence atomic 命中已注册区域 = error」)
  - **跨平面 ordering A / B / C / D** 的规约表述(coherent atomic ↔ SHARP reduce 的可见性次序 / 同 group 内 reduce 排序约束 / SHARP barrier ↔ reduce)
  - **group epoch** 的**逐格 `basis{kind, pending, ref→锚点摘要条目号}`**(公开来源受限 ⇒ 多数 `pending=true`);**reduce 逐算子支持**只登记 `pending`(锚点 `N-1`)—— ⚠️ 载体**不定义**算子族(唯一真源 `tlm::cep::ReduceOp`,per `ADR-CEP-03` SU 2026-11-03(#2) F.1-b)
  - **集合 VC 预留单一状态**(**Decision D**:载体**仅**表达 `Unavailable` **单一取值**,类型**无** `vc_id`、**无** `reason` / `owner` / `revisit_trigger`;四字段权威在 `collective_planes.json` + `CP-03`;⚠️ **不得**分配独立 VC,**不得占用承载 coherence 的 `compute_noc` / `ualink_fabric` 的 VC 与传输通道**(仅本地模型);⚠️ **能力域 = 节点内不受此限**(per `ADR-CEP-01` SU 2026-11-03(#2) G.1))
- **第 1 个 change(实现层)** `cpptlm-cep-nvlink-sharp`:
  - `include/tlm/cep/nvlink_adapter.hh`(新 · **独立头** · 零自有表副本 · **无 `transition_*`**)
  - **`BroadcastNull` 行为实现**(新 · traits 取 `kTrackerTraits[4]` ⇒ **不另写 caps 字面量**)
  - `nvlink_4` 实例声明(13 键 · `protocol_version = nvlink-4.0`)+ `repo_owned_instance_ids` **+1**
  - `include/tlm/cep/collective_planes.json` **三块 + 声明**:`plane_binding` / `registered_regions` / `collective_vc_reservation`(空)+ **`vc_reservation_declaration`**(Decision D 的四字段)
  - 装配期校验 + 判据(正向 + **反向 fixture**)

### Out Scope

- NVSwitch **硬件模型**(不在本仓;SHARP 只作为**语义**建模)
- SHARP 的**硬件 opcode / 指令编码**(只写**枚举名**)
- **UALink INC**(队列 #14;本队列的 collective 规约层是其**前置先例**)
- `ADR-CEP-01` / `ADR-CEP-02` / `ADR-CEP-03` 的**正文改写**(只消费;定稿是**前置**而非本队列的工作面)
- canonical **10 流清单**零 diff · `protocol_instance.schema.json` **零改动**(值级枚举已含所需取值)
- 真实硬件的**性能 / 带宽**结论(本队列只出**功能正确性**结论)

---

## Why

### ① 平台缺的是**第三种平面组合**,不是又一个 adapter

已落地的 **6** 个协议实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c`)**全部**落在
「coherence 平面 × 中心化目录 / 设备侧代理 / 树形」这几种拓扑里 ⇒ **零个**覆盖
「**coherence × collective × 网内归约**」的组合 ⇒ 平台答不了「SHARP 在网内做归约时,
该区域的 coherence 可见性怎么办」—— 而这正是 dGPU SoC「NVSwitch 内嵌 SHARP」这一现实方案的直接对应
(per 07 号 §5.1.1 的 NVLink 行:「NVSwitch 内嵌 SHARP/atomic/barrier;排序点可落在 Switch/SPU」)。

### ② collective 面的**载体已在、零使用** ⇒ 与 R11 同型的「声明超前于实现」

队列 #6 已交付 `CollectivePlaneIf`(7 方法,含 `group_register_region` ⇒ SHARP 式注册的**跨平面契约入口**)
+ `NullCollectivePlane` + `LocalCollectivePlane` + `collective_planes.json`;但该载体当前
`plane_binding` **全为 null**、`registered_regions` **空**、`collective_vc_reservation` **空**
⇒ **零使用** ⇒ 本队列是它的**首个消费者**,与队列 #10 的 `SparseSnoopFilter`、队列 #12 的
`DcohSymmetric` **同型先例**(traits 有行 / 载体已备 / 行为面零兑现)。

### ③ `ADR-CEP-03 §3` 的 R1~R5 + A/B/C/D **已成文、零行为面** ⇒ 纸面契约

R1~R5(分类 / 互斥 / epoch / 可观测性)与 A~D(跨平面 ordering)是本仓**唯一**一份把
「in-network reduction 与 coherence 共存」的规则写下来的地方 —— 但它**只存在于 ADR 正文**,
**没有任何可执行载体** ⇒ 与队列 #12 落地前的 `DcohSymmetric` 完全同型。

### ④ 开工前置(用户裁决 2026-11-03)—— ✅ **已满足**

`ADR-CEP-03` 决策 1 明写「**任何 SHARP / UALink 插件落地前必成**」3 条生效条件 ⇒ 契约本体未定稿就先写载体
⇒ 会把**未定稿的措辞**固化成**可执行判据** ⇒ **开工前置 = 三则 ADR 定稿**(前置**不含**在估时内)。

**✅ 2026-11-03 状态**:三则已**定稿签发 v1.0**(正文 `ADR-CEP-01` **219** / `ADR-CEP-02` **248** / `ADR-CEP-03` **249** 行),
其中 `ADR-CEP-03` 决策 1 的 3 条生效条件**逐条标注为已满足**,且其 §4 由「状态机雏形伪码」**降级**为
「判据摘要 **S1~S6** + 实现指针」(S5 / S6 = per `ADR-CEP-03` SU 2026-11-03(#2) F.2)⇒ 本队列规约层是**唯一可执行真源**(无双真源)。
⚠️ 本节的历史措辞「三则当前仍是「🟡 起草中(v0.6)」」**已失效**,以本段为准。
台账:Entry #169(定稿授权 + 9 处「签发即债」)· #170(critic B1/B2 + oracle 复核修复)。

**✅ VC 空间(同日新增并已裁)**:见 `ADR-CEP-01` 的 SU 2026-11-03(**Decision D**)与台账 Entry #171。
⇒ 本拓扑 `unavailable` ⇒ **不得占用承载 coherence 的 `compute_noc` / `ualink_fabric` 的 VC 与传输通道**(仅本地模型);⚠️ **能力域 = 节点内不受此限**(per `ADR-CEP-01` SU 2026-11-03(#2) G.1);⚠️ **四字段权威恒在** `collective_planes.json` + `CP-03`(**载体只登记 `Unavailable` 单一状态**)。

### ⑤ 公开来源受限 ⇒ 载体语义**不得自推**

NVIDIA NVLink SHARP 的正式文档不可投喂 AI ⇒ 载体能钉死的只有「**分类点唯一**」「**区域互斥**」
「**epoch 生命周期**」这类**结构性**事实;reduce 函数族、epoch 精细语义、VC 编号等**多半要 `pending`**
⇒ 必须**逐格 basis 登记**而不是自推(照队列 #12 的 10 项 pending 先例)。

### ⑥ `BroadcastNull` 是**唯一**零目录边的选择(且顺带闭合一条已登记缺口)

`ADR-CEP-01 §3.3` 抓取点 1 要求 Collective Plane → Directory 的调用边**必须为零**(机器检查);
NVLink SHARP 不做目录 ⇒ tracker 只能是**退化基线** ⇒ `BroadcastNull`(`scope=NONE` /
`completeness=EXACT(degenerate)` / `authority=NONE` / `aggregation_at=NONE`)是语义上唯一贴切者
⇒ 本队列**补其行为类**(traits 行早已登记 ⇒ **首次兑现**)⇒ 顺带闭合 07 号 §11 的
「`BroadcastNull` 仅声明层 traits」缺口(该实现此前被登记为「无消费者 ⇒ 不排期」,本队列**成为其消费者**)。

---

## What Changes

| 面 | 第 0 个 change(规约层) | 第 1 个 change(实现层) |
|---|---|---|
| **载体** | `nvlink_sharp_state_spec.hh`:R1/R2 分类判据 + R3/R4 区域互斥与 epoch + R5 计数器/断言 + ordering A/B/C/D + **集合 VC 预留单一状态**(`Unavailable` 单一取值)+ 逐格 `basis` (⚠️ **不定义** reduce 算子族 · **不暴露** 区域表类型) | `nvlink_adapter.hh`:**独立 adapter**,**零自有表副本**,**无 `transition_*`**(NVLink 状态空间薄 ⇒ 无转移表可暴露) |
| **tracker** | — | **`BroadcastNull` 行为实现**(traits 取 `kTrackerTraits[4]` ⇒ 不另写 caps 字面量)⇒ 闭合 07 号 §11 的「仅声明层 traits」缺口 |
| **实例** | — | `nvlink_4`(13 键 · `protocol_version = nvlink-4.0`)+ `repo_owned_instance_ids` **+1** |
| **collective 面** | VC 预留**状态**(Decision D:空预留 + 四字段声明) | `collective_planes.json`:`plane_binding` + `registered_regions` + `collective_vc_reservation`(空)+ `vc_reservation_declaration` |
| **判据** | 编译期结构断言 + **逐格 basis 核对表**(每格 `pending` 须有 `ref`)+ **反向 fixture**(毒化 ⇒ **必红** + 归因命中) | 正向判据(每条**负向断言配正控**)+ **反向 fixture** + 装配期校验 + `ctest` 注册 |
| **零改动** | `ADR-CEP-01` / `02` / `03` **正文** · canonical 10 流清单 · `protocol_instance.schema.json` | 同左 + `collective_plane.hh` / `collective_plane_contract.hh`(**除已授权追加段**)· `sharer_tracker.hh`(接口面**零改动**) |

> ⚠️ **反向 fixture 的形态** = 「毒化 ⇒ 判据**必红**」(**鉴别力**)⇒ 与队列 #11 修**误报**的那一组
> (「毒化 ⇒ 判据**仍绿**」= 回归防护)**形态相反** ⇒ **两组都须登记**(delta spec + 台账),避免后人混用。

### 5 项载体形态决定项(Task 0 普查 + architect 裁决)

| # | 决定项 | 当前倾向 | 待裁 |
|---|---|---|---|
| ① | `NvlinkAdapter` 的**载体归属与零副本形态** | ✅ **已裁**(2026-11-03,Entry #176 + `ADR-CEP-03` SU):**独立头** `nvlink_adapter.hh`(与既有 6 个 adapter 的**一协议一头**惯例一致)+ **委托** `nvlink_sharp_state_spec.hh`(先例 `cxl_dcoh_adapter`);零自有表副本;**无 `transition_*`**。⚠️ **前提**:state spec 载体 SHALL **自足**(只 include 标准库)且 SHALL **NOT** 暴露 `collective_region_table` 类型 ⇒ 区域表的**运行时状态归实现层** ⇒ **委托 ≠ 源端获得区域表可见性** ⇒ 契约面**零改动**;仍缺的判据(coherence 侧扫描集合扩展 + 标识补项)归**实现层**并**复用既有 id**(不新造) | ~~普查确认「NVLink 状态空间是否真的薄到无需转移表」~~ **已裁** |
| ② | `sharer_tracker.impl` | ✅ **`BroadcastNull`**(用户裁决 2026-11-03)+ 本队列**补行为类** | 仅需 architect 确认 caps 逐字段取值 |
| ③ | `perm` 值域 | ✅ **已裁**(2026-11-03,Entry #177):**维持 4 值锁**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`)—— 既有裁决(`CEN-Ib11` + Entry #141 值域锁 + schema 的 `perm.enum`)⇒ **不新增 perm 值**;**NVLink 实例的具体子集 = pending**(锚点 `N-8`)⇒ 不得照抄 `tilelink_c` 的 3 值 | ~~architect 确认~~ **已裁** |
| ④ | atomic 归 canonical 10 流**哪一条** | ✅ **已裁**(2026-11-03,Entry #177):**`request` 流的独立属性**(`atomic.supported` + `atomic.intent_field`)—— 既有裁决(`CEN-Ib11` + `ADR-CEP-17` 决策 6 + schema)⇒ **不新裁、不新增第 11 条流、不占 `perm` 位**;**协议侧的具体 completion 配对 = pending**(锚点 `N-9`)⇒ 原倾向「`request` + `completion`(RMW 语义)」**无公开来源支撑** ⇒ 不得作为载体断言 | ~~architect 确认~~ **已裁** |
| ⑤ | group epoch / reduce 函数族 / **VC 编号** 的载体位置与取值 | **VC 编号 ✅ 已裁**(2026-11-03,Entry #171 —— **Decision D**:本拓扑 `unavailable` + 四字段声明,**不臆造编号**;`ADR-CEP-01` SU 的 `V6-a` / `V6-c`);**epoch / reduce 族 ✅ 已裁**(2026-11-03,Entry #176 + `ADR-CEP-03` SU E.2):**reduce 族 = 复用既有 `ReduceOp` 恰 6 值**(`Sum`/`Min`/`Max`/`BitAnd`/`BitOr`/`BitXor`)⇒ **不新增枚举**;**SHARP 逐算子支持 = pending**(锚点 `N-1`);**epoch 平台侧 = R3/R4**(已定稿)/ **协议侧 = pending**(锚点 `N-3`);**平台原语集维持恰 4**(ReduceScatter / AllGather **不在**本平台原语集内) | ~~普查 + architect 裁决~~ **已裁** |

### ⚠️ 判据纪律(4 铁律,自队列 #12 继承)

1. **纯负向断言须配正控** ⇒ 「无 class 定义」「不含某 token」类断言必须配一条**期望通过**的正向用例;
2. **每条判据须有可执行反向 fixture 且因正确原因红** ⇒ 探针须输出 `PROBE:FAIL <判据 id>` ⇒ 脚本按 **id 归因命中**判定,不给「反正红了」放行;
3. **编译期锁不得只以「编译红」充当 ctest 判据** ⇒ 编译期 `static_assert` 须**另有一条运行期判据**兜住;
4. **fixture 状态变迁后须重新基线化** ⇒ fixture 的锚点必须**动态定位**(不得硬编码会迁移的 change 名 / 队列号)⇒ 队列 #12 的 `cep_gate_selftest.sh` 第 4 次同类失效已给出反面先例。

---

## Acceptance

- [x] **开工前置**:`ADR-CEP-01` / `ADR-CEP-02` / `ADR-CEP-03` **定稿** —— ✅ **已完成**(2026-11-03 签发 **v1.0**,per Entry #169 / #170;本项即原「状态由未签发 ⇒ 已签发」的验收条款)
- [x] **Task 0 硬门**:✅ **已产出**(2026-11-03,Entry #173)⇒ 锚点摘要产物(`docs/research/<date>-cep-nvlink-sharp-anchor-summary.md`):**sha256 钉版** · **零 NDA / 规范正文暴露** · 记**条款级定位 + 断言摘要**;公开来源候选类 = NVSHMEM(开源实现)/ 公开白皮书 / in-network reduction 学术论文
- [x] **architect 裁决**:上述 **5 项载体形态决定项** 落盘(**5 项已全部裁**:② = `BroadcastNull`;③ / ④ = Entry #177;① / ⑤ = Entry #176;⑤ 的 VC 编号 = Entry #171)⇒ 裁决结论**同步** `ADR-CEP-01` / `02` / `03` 与 07 号 —— ✅ **已完成**(2026-11-03:三则 ADR **SU** 落盘,per Entry #169 / #170 / #171;07 号 §5.1.1 NVLink 行 + §11 三处计数已随队列 #13 归档同步,per Entry #190)⇒ ⚠️ **残留 2 项 architect 域**(不属 builder):`ADR-CEP-01` 附录 V3 行的 `fabric_ingress_classify` 别名/撞名、`ADR-CEP-03` SU F.4 缺 tracker 行 ⇒ 已登记 §10
- [x] **第 0 个 change(规约层)**:R1/R2 分类判据 + R3/R4 区域互斥与 epoch 生命周期 + R5 计数器/断言 + ordering A/B/C/D + **集合 VC 预留单一状态**(**Decision D**:载体仅 `Unavailable` 单一取值;四字段权威在 `collective_planes.json` + `CP-03`)+ **逐格 `basis`**(每格 `pending` 须有 `ref` ⇒ 无 `ref` 的 `pending` 即红)
- [x] **第 0 个 change 的反向 fixture**:毒化任一格 ⇒ 编译期锁**必红** + 归因命中(铁律 ②③)⇒ **正控**(干净载体必绿)
- [x] **第 1 个 change(实现层)**:`NvlinkAdapter` **零自有表副本**(静态扫描 + **鉴别力自测**)+ **无 `transition_*`** + `BroadcastNull` **行为实现**(caps 逐字段取 `kTrackerTraits[4]`)+ `nvlink_4` **13 键齐备**(schema `required` **实读**)+ `repo_owned_instance_ids` **+1** + `collective_planes.json` **三块 + `vc_reservation_declaration`** + 装配期校验
- [x] **第 1 个 change 的反向 fixture**:**9 组**毒化 ⇒ 必红 + 归因 id **精确相等**命中 + 基线正控 ⇒ **全过** —— ⚠️ **2026-11-03 订正**(per 台账 Entry #187 · oracle + metis 双审):原写 **5 组** ⇒ 实现层义务共 **9 条**(NVA-01 ~ NVA-09,各有独立归因)⇒ 铁律 ② 要求**逐条**「因正确原因红」
- [x] **零改动核验**:`ADR-CEP-01/02/03` 正文 · canonical 10 流清单 · `protocol_instance.schema.json` · `sharer_tracker.hh` 接口面 ⇒ **零 diff**(冻结面除**已授权**追加段外)
- [x] **收口**:`ctest` 全绿 + 门禁 6 脚本全 rc=0 + `openspec validate --strict` rc=0 + `format.sh --check` rc=0 + KPI **2 slot 串行**(规约层归档后 slot 释放再启实现层)+ 台账 Entry(归档记录)+ 07 号 §5.1.1 状态列与 §11 计数同步 —— ✅ **已完成**(2026-11-03):`ctest` **145/145** ·
门禁 **7/7** rc=0 · `openspec validate --strict` rc=0 · `format.sh --check` rc=0 ·
KPI **2 slot 串行**已按「规约层归档 ⇒ 释放 slot ⇒ 启实现层 ⇒ 实现层归档 ⇒ 释放 slot」走完
(1/3 → 0/3 → 1/3 → **0/3**)· 台账 Entry #186 / #187 / #188 / **#190** · 07 号 §5.1.1 + §11 同步
(⚠️ 冻结面 ⇒ 两步授权,per Entry #189)

---

## Capabilities

| 能力 | 内容 | 载体 |
|---|---|---|
| **`NvlinkSharpAdapter`** | NVLink TL 语义的协议族适配(零自有表副本 · 无转移表) | 第 1 个 change |
| **SHARP 分类判据** | R1(opcode 非分类器)+ R2(唯一分类点 = fabric ingress) | 第 0 + 第 1 个 change |
| **区域互斥 + epoch 生命周期** | R3(注册触发 drain / non-cacheable 降级 · drain 失败不建 epoch)+ R4(销毁触发失效化 · 区域回归 coherence 域) | 第 0 + 第 1 个 change |
| **可观测性面** | R5(divert 计数器 + fail-stop 断言) | 第 0 + 第 1 个 change |
| **跨平面 ordering 行为面** | A / B / C / D 四类可见性次序的**可判定**实现 | 第 0 + 第 1 个 change |
| **集合 VC 预留单一状态** | `ADR-CEP-01` **Decision D**:本拓扑 `unavailable`(coherence 占满 0-3)⇒ **不得占用承载 coherence 的 `compute_noc` / `ualink_fabric` 的 VC 与传输通道**(仅本地模型);⚠️ **能力域 = 节点内不受此限**(per `ADR-CEP-01` SU 2026-11-03(#2) G.1);载体只登记 `Unavailable` 单一取值(四字段权威在 `collective_planes.json` + `CP-03`)⇒ 判据 = `CP-03` 的 `V6-a` / `V6-c` | 第 0 + 第 1 个 change |
| **reduce 逐算子支持** | ⚠️ **不在载体面**(唯一真源 `tlm::cep::ReduceOp`,恰 6 值;载体 SHALL NOT 定义/引用)⇒ 逐算子支持多数 `pending`(锚点 `N-1`)⇒ **「恰 6 值」断言归实现层** | **实现层**(载体 §5 由 #13 实现层交付) |
| **`BroadcastNull` 行为实现** | 退化基线 tracker(无目录边 ⇒ 满足 `ADR-CEP-01 §3.3` 抓取点 1) | 第 1 个 change |

---

## Impact

| 面 | 影响 |
|---|---|
| **本仓实现** | 新增 2 个头 + 1 个行为类 + 1 个实例声明 + `collective_planes.json` **三块 + `vc_reservation_declaration`**;**冻结面零 diff**(除已授权追加段) |
| **平台能力** | 首次覆盖「coherence × collective × 网内归约」组合;**`BroadcastNull` 从「无消费者」变为「有消费者」** ⇒ 07 号 §11 该行状态须同步 |
| **文档** | 07 号 §5.1.1 的 NVLink 行状态列(Stage 2 ⇒ 已落地)+ §11 两行计数(SharerTracker `5/6` ⇒ `6/6`;adapter `6 个` ⇒ `7 个`)+ §5.1.1「6 族 / 8 实例」计数口径核对;`ADR-CEP-01/02/03` 定稿后追平 |
| **KPI** | **2 slot 串行**(规约层 → 实现层;当前实测 **1/3**(唯一 active = `cpptlm-cep-dual-port-inbound-dispatch`,per 台账 Entry #308;⚠️ 全局 baseline = **1/3**（**活口径**:逐次 KPI 变更的**归档流水**见 objective `obj-cep-platform.md` §11 台账(Entry #28 ~ #308 的 KPI 条目)⇒ **本行不堆历史快照**;⚠️ `docs_sync_check` 的KPI 断言只抓「当前/现行/真实」限定的当前态 ⇒ 历史叙述不影响校验）) ⇒ 规约层归档释放 slot 后方可启实现层 |
| **估时** | 规约层 **5 周** + 实现层 **8 周** = **13 周**(标称 10 周 ⇒ 上修;per 用户裁决「估时以质量优先」);**ADR 定稿前置不含在内** |
| **残余风险** | 公开来源受限 ⇒ `pending` 语义较多 ⇒ 与 R11(🟡 中)**同型** ⇒ 靠**逐格 basis** 兜住,不靠自推 |
| **与后续队列** | 队列 #14(UALink INC)以本队列的 **collective 规约层**为前置先例(同一 collective plane) |
| **可逆性** | 全部新增面 ⇒ 无既有行为变更 ⇒ 可整体回退而不破坏既有 6 个实例 |
