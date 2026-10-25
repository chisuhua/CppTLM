---
name: cpptlm-cep-collective-plane-base
priority: P1
phase: phase-10
主题: CEP Fabric Collective Plane 基座 —— CollectivePlaneIf 单接口 + NullCollectivePlane + LocalCollectivePlane(group 生命周期 / region-epoch / BARRIER rendezvous)+ 契约判据层
依赖:
  - ADR-CEP-01
  - ADR-CEP-03
  - ADR-CEP-07
  - ADR-CEP-04
  - ADR-CEP-02
  - ADR-CEP-06
  - ADR-CEP-09
  - ADR-CEP-13
  - ADR-CEP-14
状态: Proposed
created: 2026-10-26
---

> **载体定位**:本文是 **5 段规划输入**;**执行契约**在 `openspec/changes/cpptlm-cep-collective-plane-base/`
> (proposal + design + 2 个 delta capability);**任务分解**在同目录 `tasks.md`(**builder 阶段产出**)。
> **队列**:`obj-cep-platform §4.0` 队列 **#5**(Stage 1 / 2027-Q4)· 依据 `ADR-CEP-03` + `ADR-CEP-01` · 估时 8 周标称。
> **前序**:#1 umbrella / #2 poc-spike / #3 stub-cache-core / #4 directory-ordering-plane 均已 ✅ archived(完成归档语义)
> ⇒ **KPI 并发 0/3 → 本 change 占 1 slot**(CEP 系列自身并发 ≤ 1,per `obj §6.3 D1`)。

---

## Why

Stage 1 的第 5 个 change。Fabric Collective Plane 是 CEP 6 层架构里的**旁路平面**(per `ADR-CEP-01 Decision 1`),
与 Directory & Ordering Plane **横向并列、彼此零调用边**。它至今**仓内零实现**:

| 缺口 | 证据 |
|---|---|
| **平面本体零存在** | `grep -rn "CollectivePlane\|collective_region\|group_id\|CollectiveReq\|CollectiveEvt" include/ src/ test/ examples/ configs/` → **零命中**。`ADR-CEP-01 Decision 1` 锁定的 6 层架构里有一层是空的 |
| **`INV-CEP-07-IC3` 只有文本断言** | `ADR-CEP-07:155` 定义「`NullCollectivePlane` 是 Collective Plane 默认实现,所有原语返回"不支持"」;`test/test_cep_canonical_invariants.cc:61` 只校验**编号存在**(`INV-CEP-07-IC3` 在 "Simulation + Seam" 组)⇒ **零行为载体** |
| **`INV-CEP-04-IC5` 的行为主体落在本平面** | `ADR-CEP-04:301` 定义「`fabric_ingress_classify` 中 "table 命中 + epoch invalid" 窗口 = 正常服务放行」;而 `fabric_ingress_classify` 属 `ADR-CEP-03 §4` ⇒ **owner ADR 是 CEP-04,行为主体在本平面** —— 队列 #4 未实现,是**遗留给本 change 的债** |
| **完成域判别规则零机器化** | `ADR-CEP-01 Decision 2` 的「requester-scoped → Coherence / group-scoped → Collective」是**全平台唯一的归属裁决规则**,却只存在于 ADR 文本;`ADR-CEP-01 §3.3` 的 7 条抓取点(评审检查单)无一条可机器检查 |
| **H4 假设无载体** | `obj §9.1` H4 假设「完成域判别规则足以同时容纳 coherence + collective」,验证手段 = 「**同一实验跑 coherence 流 + collective 原语**」;若本 change 只交 Null 实现,H4 在 Stage 2(P3 SHARP,队列 #10)之前**没有任何可跑载体**。⚠️ **本 change 交付后的实际形态**:同一**测试夹具**可同时实例化 coherence 侧与 collective 侧两个平面(可跑、可断言)—— 但**尚未**构成「同一实验跑两类流」(traffic generator 集成留待 Stage 1 末 / P3) |

**为什么必须落在本 change 而非 P3/P4**:`ADR-CEP-03 §6` 已把后续序列排死 ——
`cpptlm-cep-collective-plane-base`(P2, 本 change)→ `cpptlm-cep-nvlink-sharp-adapter`(P3)→ `cpptlm-cep-ualink-inc-adapter`(P4)。
P3/P4 是**插件**,插件必须有**被插件的平面**;而 REDUCE/BROADCAST/MULTICAST 的数据路径需要
fabric ingress 分类(per `ADR-CEP-03 §3.1` R2)—— 那属 P3,**不在本 change**。

---

## What Changes

### 新增

| 文件/capability | 作用 |
|---|---|
| `openspec/specs/cep-collective-plane-base/spec.md`(delta) | **capability `cep-collective-plane-base`** —— `CollectivePlaneIf` 单接口 + 类型集(`group_id_t` / `member_set_t` / `addr_range_t` / `CollectiveReq` / `CollectiveEvt` / `cap_t`)+ `NullCollectivePlane`(**行为化 `INV-CEP-07-IC3`**)+ `LocalCollectivePlane`(group 生命周期 + region/epoch 表 + **BARRIER rendezvous**)+ **epoch invalid 窗口放行**(**行为化 `INV-CEP-04-IC5`**) |
| `openspec/specs/cep-collective-plane-contract/spec.md`(delta) | **capability `cep-collective-plane-contract`** —— 5 条**可机器检查**的契约判据:零调用边(include-graph 闸)/ 区域互斥(CEP-13)/ collective VC 预留与 coherence VC 不冲突 / 完成域判别(`group_id` 不进 Coherence Interface)/ canonical 10 流零 diff |
| `include/tlm/cep/collective_plane.hh` | **header-only** `CollectivePlaneIf` + 类型集 + `NullCollectivePlane` + `LocalCollectivePlane`;**testbench 定位**(与队列 #3/#4 一致:不进 `ModuleFactory`、不注册 23 ABI、不改 frozen header) |
| `include/tlm/cep/collective_plane_contract.hh` | **header-only** 契约判据(5 条)+ 三态 SKIP/PASS/FAIL + verdict 逐条输出;**声明源**驱动的部分与**源码 include-graph** 驱动的部分分开 |
| `include/tlm/cep/collective_planes.json` | 本仓机器可读的 collective plane 声明(**plane 绑定默认 null / registered region 表 / collective VC 预留**);**诞生即不冻结**,per 队列 #4 `protocol_instances.json` 先例 |
| `include/tlm/cep/collective_planes.schema.json` | 配套 schema(**诞生即不冻结**) |

### 修改

| 文件 | 改什么 |
|------|--------|
| `scripts/test/cep_gate_selftest.sh` | **归档日**才改(锚点 #5 → #6);planner 阶段**不动** |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 状态列 📋 → 🟡 · §4.2 承接列 · §5.2 KPI 0/3 → **1/3** · §11 Entry #66 |
| `.rddf/roadmap/phases/phase-10.md` | 队列 #5 对应子任务行状态 + KPI baseline 行 |
| `.rddf/roadmap.md` · `AGENTS.md` | KPI baseline 0/3 → 1/3 |
| **`docs/adr/ADR-CEP-00-platform-charter-and-governance.md`** | **Status Update 2026-10-26**:队列 #5 已启动(KPI 0/3 → 1/3)+ 上一段「当前态」→「当时态」 |

### 冻结面

**3 个成员改动**(均**仅追加** `## Status Update` 段;走 `cep_freeze_check.sh --update --authorized-by "Entry #66"` 两步授权):

| 成员 | 追加内容 |
|------|---------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | Status Update 2026-10-26 —— 队列 #5 已启动 + KPI 数值 |
| `docs/adr/ADR-CEP-03-fabric-collective-plane-internals.md` | **§5 Bundle 路径偏离声明**(`coherence_bundles_tlm.hh` 现实不存在 ⇒ 本 change 类型内嵌、Bundle 化推迟至 P3)—— per `ADR-CEP-03 §6` 后续 change 序列 |
| `docs/adr/ADR-CEP-01-fabric-collective-plane.md` | **§3.3 抓取点可测性裁决声明**(7 条中哪几条本 change 可机器检查、哪几条推迟到 P3/Stage 2) |

**零改动**:`ADR-CEP-07` · `ADR-CEP-04` · `ADR-CEP-02` · `ADR-CEP-09` · `ADR-CEP-13` · `protocol_instance.schema.json` · `fabric.{json,schema.json}` · 3 份既有 main spec · 其余 ADR-CEP 家族成员

---

## Non-Goals

**明确不做(防 scope creep 反向发生)**:

| 不做项 | 推迟理由 |
|---|---|
| REDUCE / BROADCAST / MULTICAST 的**数据路径** | 需要 fabric ingress 分类(per `ADR-CEP-03 §3.1` R2)⇒ 属 **P3 SHARP**;本 change 只到「接口 + 未注册区域报错」 |
| R1/R2 的**真实 fabric ingress 分类** | 同上,P3 |
| 跨平面 ordering 契约 **A / B / E** 场景 | Stage 1 物理不可测(无 coherent atomic 流量、无 SHARP 插件)⇒ **禁写恒真/SKIP 判据** |
| 抓取点 2「组播投递单实现」 | fabric 组播机制不存在 ⇒ 只能做**声明层**断言 |
| `collective_stalls` metric 落地 | 属 `INV-CEP-06-IC2` 的实验产出契约;本 change **无实验载体** ⇒ 落了就**是**空 metric;最多在 `LocalCollectivePlane` 内部埋 counter |
| `ModuleFactory` / `REGISTER_MODULE` 注册 | `07 号 §6.3` 是**最终形态**口径;Stage 1 与队列 #3/#4 先例一致(header-only、testbench 定位)⇒ 注册推迟到 P3 有真实 adapter 时 |
| Bundle 化(`CollectiveReqBundle` / `CollectiveEvtBundle`) | `ADR-CEP-03 §5` 假设的 `coherence_bundles_tlm.hh` **现实不存在**(06-bis P2 未实施);Bundle 的价值在有真实传输通道时 ⇒ 类型内嵌接口头,Bundle 化推迟 |
| **新增任何 `INV-` 编号** | 15 组 66 条 invariant 的**分组是冻结测试的硬编码**(`test_cep_canonical_invariants.cc` 的 `expected_groups()`)⇒ 新增编号即契约违反;本 change 的 B2 贡献 = **行为化既有条目**,计数恒为 66 |
| AllReduce 作为独立原语 | `ADR-CEP-03 §2` 已拍板 AllReduce = REDUCE + BROADCAST **复合**,不列原语 |

---

## Acceptance

> **红绿分离**：「判据实现退化」= 用例红;「判定结论为红」= 用例**仍绿**且结论作为交付物登记。
> **每条判据须配反向 fixture 或正控**(纯负向断言无正控 = 判据可能根本没生效)。

### A. 行为面(`cep-collective-plane-base`)

| # | 验收项 | 形态 | 反向/正控 |
|---|---|---|---|
| A-1 | `NullCollectivePlane` **全部**原语(含 `group_create` / `group_destroy` / `group_register_region` / `send` / `recv` / `collective_barrier` / `query_capability`)返回「不支持」 | 单元测试(**行为化 `INV-CEP-07-IC3`**) | 反向:注入「假装支持」的 plane ⇒ 红 |
| A-2 | `LocalCollectivePlane` group 生命周期:`group_create` → `group_register_region` → `group_destroy` 状态机可执行 | 单元测试 | 正控:合法路径绿 |
| A-3 | **BARRIER rendezvous**:N-1 到达不释放、N 到达释放 | 单元测试 | 反向:阈值改 N-1 ⇒ 红 |
| A-4 | **epoch invalid 窗口放行**:`table 命中 + epoch invalid` ⇒ **正常服务放行**,非 fail-stop(**行为化 `INV-CEP-04-IC5`**) | 单元测试 | 反向:删掉 epoch 检查 ⇒ 红 |
| A-5 | 区域互斥(CEP-13 语义):重叠注册 ⇒ `group_register_error`;drain 失败 ⇒ **不建 epoch** | 单元测试 | 正控:合法注册成功 |
| A-6 | 未注册区域上的 collective 操作 ⇒ **立即报错**,不静默 | 单元测试 | 反向:静默返回 ⇒ 红 |
| A-7 | functional 模式下 collective **不产生 latency**(与 `INV-CEP-07-IC6` 兼容) | 单元测试 | — |

### B. 契约判据面(`cep-collective-plane-contract`)

| # | 验收项 | 形态 | 反向/正控 |
|---|---|---|---|
| B-1 | **零调用边**:collective 头文件**不得** include directory / ordering / sharer 头 | include-graph 闸(grep + 编译期) | 反向:注入 include ⇒ 红 |
| B-2 | **canonical 10 流零 diff**:本仓不新增/不改 canonical 10 流 | 与 `ADR-CEP-02 §2` 清单的 diff 断言 | 反向:fixture 改流清单 ⇒ 红 |
| B-3 | **完成域判别**:`group_id` 等 group-scoped 字段**不得**出现在 Coherence Interface 侧接口/契约 | 声明层 + include-graph 闸 | 反向:注入 ⇒ 红 |
| B-4 | collective VC 预留 ∈ 0-3 且与 `fabric.json` 既有 `vc_binding` **不冲突** | crosscheck 测试(消费 `collective_planes.json`) | 反向:fixture 改冲突 VC ⇒ 红 |
| B-5 | 判据三态显式化(SKIP ≠ PASS ≠ FAIL)+ verdict 逐条可 grep | 单元测试 | — |

### C. 流程与治理

| # | 验收项 |
|---|---|
| C-1 | `openspec validate cpptlm-cep-collective-plane-base --strict` rc=0 |
| C-2 | 守门 7 项 rc=0 · `cep_gate_selftest` 全绿 · `ctest` 全绿 |
| C-3 | 文档改动一律带断言「命中 1 次」 |
| C-4 | 归档日治理 6 步(per `Entry #64` 模板)—— **design.md 预登记**,tasks.md 直接继承 |
| C-5 | **B2 贡献清单非空**(见 design.md「对 B2 的贡献」一节);若为空则**自我否决** |

---

## Capabilities

| capability | 内容 | Scenario 数(标称) |
|---|---|---|
| `cep-collective-plane-base` | 平面本体:接口 + 类型 + Null + Local + group/epoch + barrier rendezvous | 13 |
| `cep-collective-plane-contract` | 契约判据:5 条机器可检查的契约 + 声明源 | 8 |

> **合计 21 Scenario**(与队列 #4 的 22 / 队列 #3 的 18 同一量级 ⇒ 4-6 周当量,vs 8 周标称)。

---

## Impact

- **B2(66 条 invariant 机器检查 100%)**:本 change **新增 invariant 数 = 0**(ADR-CEP-01/03 在 15 组里**无组**,不可新增编号);
  贡献 = **行为化既有条目** `INV-CEP-07-IC3` + `INV-CEP-04-IC5` + **契约层加固**(零调用边 / canonical 10 流零 diff)。
- **KPI**:0/3 → **1/3**(CEP 系列自身并发 ≤ 1)。
- **冻结面**:37 → **37**(成员数不变;3 份 ADR 内容更新)。
- **对后续 change 的影响**:P3 SHARP / P4 UALink 有了被插件的平面基座;队列 #6 CHI RN-F 可消费 `NullCollectivePlane` 默认绑定。
- **风险**:见 design.md「Risks / Trade-offs」—— 主要是「接口无消费方」的空转风险与「Bundle 路径偏离已拍板项」的治理风险。

---

**最后**: 2026-10-26 planner 阶段 —— 5 段草稿已写;待建 `openspec/changes/cpptlm-cep-collective-plane-base/`
(proposal + design + 2 个 delta capability)。
