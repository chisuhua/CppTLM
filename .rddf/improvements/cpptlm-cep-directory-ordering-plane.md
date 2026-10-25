---
name: cpptlm-cep-directory-ordering-plane
priority: P1
phase: phase-10
主题: CEP Directory & Ordering Plane —— SharerTrackerIf 单接口 + caps(D-5) + FullMapDirectory 最小实现 + ChannelDependencyGraph 装配闸 + AddressResolverIf 小接口
依赖:
  - ADR-CEP-04
  - ADR-CEP-08
  - ADR-CEP-09
  - ADR-CEP-12
  - ADR-CEP-02
  - ADR-CEP-15
状态: 已归档(2026-10-25)
created: 2026-10-25
last_revised: 2026-10-25
---

# cpptlm-cep-directory-ordering-plane: Directory & Ordering Plane(SharerTrackerIf 单接口 + caps(D-5) + FullMapDirectory 最小实现 + 装配期判据 + S1-7 (β) 裁决)

> **状态**: 🟡 **Proposed**(2026-10-25 planner 阶段建目录;单 Phase 形态,与队列 #3 的「双 Phase」不同 —— 方案矩阵本身简单)
> **队列**: `obj-cep-platform §4.0` 队列 **#4** · 前序 #1/#2/#3 均 ✅ archived → **KPI 并发 0/3 → 本 change 占 1 slot**
> **裁决(2026-10-25,DRI 三问)**:范围**切小**(单接口 + caps + D-5 + **FullMapDirectory 唯一行为实现**);装配闸输入源 = **`include/tlm/cep/protocol_instances.json`**;**双挂**(instantiateAll + validate_topology,共享判据);**S1-7 (β) 决策落本 change**(同批含 INV-CEP-08-IC2)
> **载体定位**:本文是 **5 段规划输入**;**执行契约**在 `openspec/changes/cpptlm-cep-directory-ordering-plane/`;**排队与状态**在 `obj §4.0`
> **冻结面**:改动 **3** 个成员(ADR-CEP-00 / ADR-CEP-04 / 07号),均**仅追加** Status Update 段;走 `cep_freeze_check.sh --update --authorized-by "Entry #62"` 两步授权

---

## Why

### ① **Sharer Tracker 是 Stage 1 后续 5 个队列的「公共地基」**,它至今没有仓内形态

`ADR-CEP-04` 已把接口族(决策 1)、D-5 重构(§3)、**同地址并发 lookup hazard** 行为契约(§4)、**被动观察**(§3.4)、**CXL 对称 aggregation_at 二元**(§3.5)、**DCOH 完备性契约**(决策 2)全部定义完毕 —— 但**仓内 0 实现**。`ADR-CEP-04 §1.2`「真实解决的两条独立原则」依赖一个**可调用的单接口**才能被 validation。

旁证:队列 #3 已把 Stub Cache Core(6 接口 / 5 态 / INV-SC)做成了**可执行**契约并合并进 `openspec/specs/cep-stub-cache-core/spec.md`。但 Stub 的 lookup 语义**只承诺 net_side 投影,不承诺 SharerTrackerIf 的语义**(look → result/view/hazard)。CEP 后续队列 #6~#9 各自的协议族(CXL/TileLink/CHI NF/NVLink SHARP)都要消费 Sharer Tracker 接口 —— **多种实现不必仓内全做,但接口面必须全做**(否则队列 #6 无法导入)。

### ② **3 条 Stage 1 准入前置必须落地**

- **S1-1 ② 建图层**(装配期校验器,**须在装配路径调用** + 输出逐 `(network, vc)` verdict;**禁**离线脚本形态;判据可计算性已由 S3 spike 实证,但**镜像↔权威抗漂移未实证** per Entry #55) → 义务在队列 #4
- **S1-7 (β) 重评估**(per `ADR-CEP-05 §13.1` 与 `ADR-CEP-08 决策 4` **同批**约束)+ INV-CEP-08-IC2 E 态静默升级检查项 → 需要裁决 D-δ
- **S1-2 / S1-3 / S1-4**(owner 重指向队列 #4,per Entry #61):S1-2 ⛔ 未触发,S1-3 ⛔ 未触发,S1-4 ✅ 已消费

### ③ 结论:本 change 是「**把 Sharer Tracker / Channel Dependency Graph 变成可执行落地**」的一次最小充分交付

**裁决点(**oracle + metis 两路独立同意**)**:
- **切小**(单接口 + caps + D-5 + **FullMapDirectory 唯一行为实现**),其余 4 种(SparseSF/DcohSymmetric/TreeManager/BroadcastNull)跟随**各自消费队列**(队列 #6~#9 各自的协议族);避免「4 个无消费者实现提前一年堆代码」(队列 #2/#3 双实证的失败模式,`obj §11 Entry #55 ③` 警示过)
- **装配闸输入源 = `include/tlm/cep/protocol_instances.json`**(本仓机器可读实例声明),**诞生即不冻结**,首个 implemented 实例跑过后再按 S1-4 复核口径纳入;不动冻结的 fabric.* 双件/CFG-08 schema
- **双挂**(instantiateAll + validate_topology 共享判据,不复制)
- **M-4 行为侧孤儿行在外部队列暂留**(Entry #61 判据 ④:活文档不得失主),队列 #4 不承接 M-4

---

## What Changes

### 新增(**3 个 NEW capability** + **4 个源文件**)

| 文件/capability | Snapshot 引用 |
|---|---|
| `include/tlm/cep/sharer_tracker.hh` | `cephubtype: SharerTrackerIf` 单接口,lookup `(addr,intent)→result/view/hazard`,caps descriptor 4 属性 struct,**begin/fold 生命周期**,observe_snoop 被动记账,**FullMapDirectory 唯一行为实现** |
| `include/tlm/cep/address_resolver.hh` | AddressResolverIf 单接口:`resolve(pa) → home_node_id`(**spec 首次定义**沿用 ADR-CEP-04 先例措辞) |
| `include/tlm/cep/channel_dependency_graph.hh` | builder + per-`(network,vc)` verdict + DFS 三色环检测(算法迁 `cep_poc_spike_s3.cc:125-151`) |
| `include/tlm/cep/protocol_instances.json` | 本仓机器可读协议实例声明(按 schema 校验) |
| `test/test_cep_sharer_tracker_base.cc` | capability **1** 锚点 |
| `test/test_cep_ordering_point_checker.cc` | capability **2** 锚点 |
| `test/test_cep_channel_dependency_gate.cc` | capability **3** 锚点 |
| delta capability `cep-sharer-tracker-base` | Interface signature / caps / FullMap 行为 / hazard / BEGIN-FOLD 生命周期 / 禁用 API 静扫 |
| delta capability `cep-ordering-point-checker` | J1~J3 判据执行 + AddressResolverIf 存在 + E 态静默升级检查项 + S1-7 (β) 裁决登记 |
| delta capability `cep-channel-dependency-gate` | 顶部 `(实例,net,vc)` 顶点 / 边由声明导出 / 有环 fail / verdict 逐 `(network,vc)` / SKIP-FP 三态 / moesi_gpu fixture 对账 |

### 修改

| 文件 | 改什么 |
|------|--------|
| `src/core/module_factory.cc` | `instantiateAll()` 里的守卫钩子(validateConfig 之后 / 实例创建之前;final_config 内含 CEP 协议实例 section 才触发) |
| `src/core/module_factory_validate.cc` | **不动**(module_factory_validate `:199-215` 是字段平铺检查,无 schema 严检) |
| `scripts/CMakeLists.txt` `validate_topology` | target 追加一次 channel_dependency_graph 调用(共享同一判据文件,**不复制**实现) |
| `docs/architecture/07-coherence-exploration-platform.md` | `:674` SharerTracker 行 change 引用(队列 #3 → #4;冻结面成员,走两步授权) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 状态列 📋 → 🟡 · §11 Entry #62 · §5.2 KPI 0/3 → 1/3 |
| `.rddf/roadmap.md` · `.rddf/roadmap/phases/phase-10.md` | queue #4 状态 + KPI baseline + phase-10 依赖图 |
| `AGENTS.md` | KPI 基线行(0/3 → 1/3) |
| `docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` | **Status Update**:§8 四个 change 名失指(ADR-CEP-04 §8 列的 `cpptlm-cep-sharer-tracker-base/-impls/-cxl-dcoh/-tilelink-tree` 与 obj §4.0 的 11 change 算术冲突,per 队列 #3 的 D11 同构处置) |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | **Status Update 2026-10-25**:队列 #4 已启动(KPI 0/3 → 1/3)+ 上一段「当前态」→「当时态」 |
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | **不动**(§7 判据 2 已触发状态由队列 #3 落成 `cep-vc-mapping-contract`;S1-7 (β) 的裁决**登记**进本 change 的 design.md,不反向改 ADR-CEP-05) |

### 冻结面

**3 个成员改动**(均**仅追加** `## Status Update` 段): `ADR-CEP-00` · `ADR-CEP-04` · `07号`
**零改动**: `ADR-CEP-05` · `ADR-CEP-08` · `ADR-CEP-09` · `protocol_instance.schema.json` · `fabric.{json,schema.json}` · 2 份既有 main spec

---

## Non-Goals

1. **不实现tejo/CXL/DcohSymmetric`"(CXL对称) / `TreeManager`(TileLink) / `BroadcastNull` / `SparseSnoopFilter` 四种 tracker 实现** —— 4 实现的首个消费者在队列 #6~#9 的**各自协议族**(per `ADR-CEP-04 §8` 拆分先例 + `obj §4.0` 排队表 #6~#9);提前落 = 无消费者代码
2. **不做 runtime enforcement** —— Ordering Point 三判据(J1/J2/J3)是**装配期**静态检查;runtime 死锁检测仅在「装配 fail 后」才有意义,不在本 change(R-2)
3. **不做 `snoop_complete`** / **不做 `lookup()→sharer_bitmap` 平铺返回** —— per `ADR-CEP-04 §3.6` 禁止旧接口(本 ADR 首次定义)
4. **不做 `subscribe_remote_snoop()`** —— 传输通道概念泄进 tracker 扁平,per `ADR-CEP-04 §3.4`
5. **不执行**「自动判定组合可行性」 —— per `ADR-CEP-08 决策 7` 显式排除;J3 的**通用形式**(迁移后两点不定出冲突序)**不可静态判定**,降级登记为「单点 vacuous 机器判据 + 文档判据」(per ADR-CEP-08 决策 7 HeteroGen E1)
6. **不做 Steering/Arbiter**(Decision B) —— per `ADR-CEP-09 Decision A` 必须**先于** Decision B;且「仲裁一致性」≠「仲裁策略本身」(不是本 ADR 重点)
7. **不写 ADR-CEP-05 的 Status Update**,也不反向改 `cep-stub-cache-core.spec.md`(已冻结) —— S1-7 (β) 的裁决**只登记进本 change 的 design.md**(Stub 的 4 值 perm 不在本 change 改;schema 演进归队列 #6)
8. **不跨切** 7 号 §5.1.2 表中的 3 个 Ruby 实例.FullMapDirectory 由 moesi/split-protocol 共享,它就是 07 号表中「FullMap」一行对应的最小充分实现
9. **不编译问题吐槽**:FullMapDirectory 是 `completeness=EXACT` (无 broadcast fallback 分支),per `ADR-CEP-04 决策 2`;DCOH 的 CONSERVATIVE 契约 / PARTIAL+broadcast fallback / TreeManager 的 CHAIN 等**行为断言**(per oracle/metis 引证都落 Spark 行)——本 change 只落 FullMap 行 + `Hazard_STALL`,那些行为断言有 anchor)
10. **Clocking/网络接入**: Sharer Tracker 不接 json `connections[]`(per `include/tlm/cluster/AGENTS.md` 的 SimModule 分类,SharerTracker 不是网络拓扑组件 —— 单接口调用即可)

---

## Acceptance

> **红绿分离**(per 队列 #2/#3 确立):「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为交付物登记。本节条款**不**因结论为红而失败。

### capability 1 —— `cep-sharer-tracker-base`(9 Scenario)

- [ ] **A-1-1** `SharerTrackerIf` 声明面完整:C++ 侧接口签名(**lookup / snoop_begin / snoop_fold / observe_snoop**)与 caps descriptor 字段(super fuente / completeness / aggregation_at / broadcast_fallback),per `ADR-CEP-04 §3.1/§4/§2 Decision 3` + `INV-CEP-04-IC1` 字面值机检
- [ ] **A-1-2** caps descriptor **4 属性** struct 的**形态**静态断言 + 编译期宽度锁(**bitmask 64 bit**,bit→node_id 显式映射表;spec **首次定义** 沿用 `ADR-CEP-04` 先例措辞)
- [ ] **A-1-3** **5 实现 × 4 caps** 的 mapping 表(`completeness/aggregation_at/hazard/broadcast_fallback`)与 caps 契约**数据级**自洽(per `ADR-CEP-04 §2/§4`,FullMap 行 + DcohSymmetric 行的契约机检)
- [ ] **A-1-4** **FullMapDirectory 行为实现**:`lookup` ANSWER_HERE + 精确 sharer 集;`completeness=EXACT`;极简 `Hazard_NONE`(per `ADR-CEP-04 §4 表` FullMapDirectory 行 `Hazard_STALL`,**实测确认**;断言「与 caps 表一致」)
- [ ] **A-1-5** **snoop_begin / snoop_fold / remaining_mask / MergedState 派生事件**:`大名标记 RetireOnZeroPending` 需 ≥ 1 组可执行断言(队列 #3 的「单文件头」教训:Sharer Tracker 是**独立类能力**——该断言走 mock ResponderSet)
- [ ] **A-1-6** **同地址并发 hazard**:同 addr 两次 lookup 时,第二次必须按 caps 表返回 STALL(非 NONE)(per `ADR-CEP-04 §4`)
- [ ] **A-1-7** **INV-CEP-04-IC2(DcohSymmetric)幂等性断言** + **IC3(5 事件名+时序)**的行为断言(per `ADR-CEP-04 §2`)
- [ ] **A-1-8** **禁用 API 静态扫描**:去注释后 `snoop_complete` / `subscribe_remote_snoop` / `is_distributed` / `lookup()→sharer_bitmap` **零出现**(per `ADR-CEP-04 §3.6`)
- [ ] **A-1-9** **fullMap 之外的 4 种实现 → 声明层断言**(schema 枚举值校验),**NOT** 行为断言(⚠️ per metis)

### capability 2 —— `cep-ordering-point-checker`(6 Scenario)

- [ ] **A-2-1** `AddressResolverIf` **存在且单方法** `resolve(pa) → home_node`(**spec 首次定义**,per `ADR-CEP-08 §1.3` 正交分工;与 canonical 流 8 的 `address_resolve` **在 ProtocolEngine 内部做 DI seam 形态**,不跨引擎边界)
- [ ] **A-2-2** **J1 单点性**的静态路径覆盖检查:对 protocol_instances.json 的每个实例,检查「冲突事务路径全部经由同一 OP」声明机械判定 (**3 规格,per `ADR-CEP-08 决策 3`**)
- [ ] **A-2-3** **J2 全序可观测性**与 Sharer Tracker caps 的**耦合**:`completeness==PARTIAL` 无 fallback ⇒ J2 fail(per `ADR-CEP-08 决策 3`)
- [ ] **A-2-4** **E 态静默升级检查项**(per `ADR-CEP-08 决策 4` + `INV-CEP-08-IC2`):对 `moesi_gpu` **实跑**检查(它是已知允许静默升级的实例,组合检查器**硬要求**通过/缺失即报 fail)
- [ ] **A-2-5** **S1-7 (β) 裁决登记**(per `ADR-CEP-05 §13.1`):裁决结论**作为交付物**登记(红绿分离——判据为红即用例红;结论为红即登记)
- [ ] **A-2-6** **J3 唯一性保持: 降级登记形态**(单点 vacuous 机器判据;通用形式为**文档判据** + 「触发条件 = 队列 #6」降级条款,per 队列 #2 先例)

### capability 3 —— `cep-channel-dependency-gate`(7 Scenario)

- [ ] **A-3-1** 顶点 = `(协议实例, network, vc_id)` **三元组**(per `ADR-CEP-09 §8.2`),且 `vc_id` ∈ 0-3
- [ ] **A-3-2** 边由 `vc_binding` + `ordering_assumption` 导出(不硬编码! per Entry #55 抗漂移)
- [ ] **A-3-3** 无环 = **PASS**:moesi_gpu fixture 由 `protocol_instances.json` 驱动,断言 8 顶点 / 8 边 / **0 跨网边**(per `cep-poc-spike-verification/spec.md` S3 结论)
- [ ] **A-3-4** **注入环 ⇒ 配置 fail**(反向验证,必需):fixture 注入一个会成环的 binding,断言 `instantiateAll` 整条管线必须**显式返回 false**(per `INV-CEP-09-IC1`)且 verdict 逐 `(network, vc)` 标注
- [ ] **A-3-5** **verdict 输出通道**:`instantiateAll(bool)` 之外的日志通道必须机器可 grep(per 检查器 fixture Q:Windows on stdout;采用 `[verdict] net=<net> vc=<id>` 格式)
- [ ] **A-3-6** **顶点/边与 S3 spike 对账**:**同轴化**检查确保与 `cep_poc_spike_s3.cc` 镜像一致 —— **Entry #55 的「镜像↔权威抗漂移未实证」升级为已闭合**
- [ ] **A-3-7** **SKIP/PASS/FAIL 三态显式化**:protocol_instances.json 缺失或空 ⇒ **SKIP**(与 PASS 可区分),不得与 PASS 同判;per `docs_sync_check.sh` SKIP/RUN 先例 + **[SKIP] 显式打印**

### 收尾

- [ ] **A-FIN-1** `openspec validate cpptlm-cep-directory-ordering-plane --strict` rc=0
- [ ] **A-FIN-2** 守门 7 项 rc=0;`cep_gate_selftest` 全绿;`openspec list --json` 实测 1/3(= 1 active)
- [ ] **A-FIN-3** `ctest` 全绿;`[cep]` 标签的 case 数与断言数**逐项登记**
- [ ] **A-FIN-4** **归档治理清单** per metis 的「归档日治理 6 步」:(1) `FROZEN_GLOBS +3` (2) manifest 重生成授权串 (3) selftest 锚点迁移 (4) `cep_doc_ref_check` VIRTUAL_PATHS 双态注册 (5) 活文档数字一致性同步 (6) `openspec archive --yes`
- [ ] **A-FIN-5** 反向验证自查:每条 fixture 变异必红 + **GREEN 样本**必绿

---

## Capabilities

### New Capabilities

- **`cep-sharer-tracker-base`** —— Sharer Tracker 的**接口签名 + caps descriptor + FullMapDirectory 唯一行为实现**为可执行契约;**5 实现 × 4 caps** mapping 表的数据级自洽契约;IC1~IC3 不变量的 3 条行为断言
- **`cep-ordering-point-checker`** —— Phase 0「**AddressResolverIf 单接口** + **J1~J3**装配期静态判据 + **INV-CEP-08-IC2** E 态静默升级检查项 + **S1-7 (β) 裁决登记**(含 J3 vacuous 双形态验证)
- **`cep-channel-dependency-gate`** —— 「**装配期通道依赖图** + 顶点 `(协议实例, network, vc_id)` 三元组」为可执行契约;**verdict 逐 (network, vc)**;SKIP/PASS/FAIL 三态;`protocol_instances.json` 为输入源

### Modified Capabilities

**无** —— 队列 #3 落成 `cep-vc-mapping-contract`(8 Scenario) 与 `cep-stub-cache-core`(9 Scenario) 的**冻结 spec** 零改动。

---

## Impact

### 影响范围

| 层 | 影响 |
|---|---|
| **新增类** | `include/tlm/cep/{sharer_tracker,address_resolver,channel_dependency_graph}.hh` —— header-only |
| **新增 JSON** | `include/tlm/cep/protocol_instances.json`(**不冻结**);fixture: `configs/dgpu_cep_baseline.json` 含 CEP 声明段`
| **钩子** | `src/core/module_factory.cc` 守卫钩子;`scripts/CMakeLists.txt` `validate_topology` 追加 |
| **测试** | 3 个新 test 文件(9+6+7 = **22 Scenario**,锚点均指向本 3 个文件) |
| **冻结面** | **3 个成员**(仅追加 Status Update) : ADR-CEP-00 / ADR-CEP-04 / 07号 |
| **KPI** | **0/3 → 1/3** |
| **23 ABI / frozen headers** | **零改动** |

### 风险与缓解

| 风险 | 缓解 |
|------|------|
| 22 Scenario 过度承诺(非 FullMap 行为断言无从实证) | A-1-9 / A-2-6 / A-2-5 显式降级登记(队列 #2 先例) |
| 装配闸「离线脚本化」 | A-3-4 用**真 `instantiateAll()`** 驱动 + A-3-7 三态显式化(**SKIP 显式打印**) |
| 镜像 vs 权威漂移 | A-3-6 同轴化(fixture 从 `protocol_instances.json` 读,与 S3 spike 对账) |
| `responder_set` 位宽/映射形态无权威 | A-1-2 **spec 首次定义**(bitmask 64 bit,编译期锁,bit→node_id 显式映射) |
| 单人仓的独立性缺口 | 本仓的「评审」由 oracle / metis / critic subagent 承担,**不等价于**真人外部评审(`obj §8.1` R10) |

### 复评节奏

- 建 change 目录后 commission `openspec validate --strict` + 守门 7 项 + `cephub gate_selftest` 全绿即视为 **planner exit**
- 每 Scenario 点名锚点 per 队列 #3 的先例;**对账测试**断言 Scenario 数 == 锚点数
- 实施阶段(Phase 1)单独计工件,估时显著低于 12 周标称(队列 #3 同型先例:8 周标称 vs 单文件头实现)

---

**最后**: 2026-10-25 planner 阶段 —— 5 段草稿已写;**change 目录已建**(openspec/changes/cpptlm-cep-directory-ordering-plane:proposal + design + 3 个 delta capability);`openspec validate --strict` rc=0;KPI 0/3 → **1/3**;per 台账 **Entry #62**。

---

**最后**: 2026-10-25 planner 阶段 —— 5 段草稿已写;**change 目录已建**(`openspec/changes/cpptlm-cep-directory-ordering-plane/`:proposal + design + 3 个 delta capability`cep-sharer-tracker-base`(9 Scenario) / `cep-ordering-point-checker`(6) / `cep-channel-dependency-gate`(7));裁决四问落地(范围切小 / `protocol_instances.json` / 双挂 / S1-7 β 同批);`openspec validate --strict` rc=0;KPI 0/3 → **1/3**;per 台账 Entry #62。**下一个 change = 队列 #5 `cpptlm-cep-collective-plane-base`**。

---

**最后(归档)** : 2026-10-25 —— builder 实施完成(Entry #63:4 个 header + 1 个 CLI + 3 个 test,22 Scenario 全落地)+ **完成归档**(Entry #64:`openspec archive --yes`,archive 名 `2026-10-25-cpptlm-cep-directory-ordering-plane`;22 Scenario 并入 3 份 main spec;冻结集 34 → **37**;KPI 1/3 → **0/3**)。**下一个 change = 队列 #5 `cpptlm-cep-collective-plane-base`**。
