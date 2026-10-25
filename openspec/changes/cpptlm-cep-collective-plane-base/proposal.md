# cpptlm-cep-collective-plane-base: Fabric Collective Plane 基座(CollectivePlaneIf + Null + Local + 契约判据)

> **状态**: 🟡 **Proposed**(2026-10-26 建目录;planner 阶段产出 proposal + design + 2 个 delta capability)
> **队列**: `obj-cep-platform §4.0` 队列 **#5**(Stage 1 / 2027-Q4)· 依据 `ADR-CEP-03` + `ADR-CEP-01` · 前序 #1 `cpptlm-cep-umbrella` / #2 `cpptlm-cep-poc-spike` / #3 `cpptlm-cep-stub-cache-core` / #4 `cpptlm-cep-directory-ordering-plane` 均已 ✅ archived(完成归档语义)→ **KPI 并发 0/3 → 本 change 占 1 slot**
> **形态**: **单 change 单 Phase**;估时 8 周标称 → 实际 4-6 周当量(队列 #2/#3/#4 实测先例:标称 12 周实际 4-6 周)
> **裁决(2026-10-26,DRI 四问)**: **范围切小**(`CollectivePlaneIf` + `NullCollectivePlane` + `LocalCollectivePlane` 只做 group 生命周期 / region-epoch / **BARRIER rendezvous**;REDUCE·BROADCAST·MULTICAST 的**数据路径**推迟至 P3) / **Bundle 类型内嵌**(`ADR-CEP-03 §5` 假设的 `coherence_bundles_tlm.hh` 现实不存在 ⇒ 偏离登记,Bundle 化推迟) / **header-only 不进 ModuleFactory**(与队列 #3/#4 先例一致) / **独立声明源** `collective_planes.json`(不碰冻结的 `protocol_instance.schema.json`)
> **关联 ADR**: `ADR-CEP-01`(旁路平面 + 完成域判别规则)+ `ADR-CEP-03`(Collective 内部接口 + 跨平面契约)+ `ADR-CEP-07`(`INV-CEP-07-IC3` NullCollectivePlane / `INV-CEP-07-IC6` functional 零延迟)+ `ADR-CEP-04`(`INV-CEP-04-IC5` epoch invalid 窗口)+ `ADR-CEP-02`(canonical 10 流)+ `ADR-CEP-06`(实验产出)/ `ADR-CEP-09`(VC 死锁自由)/ `ADR-CEP-13`(一致性域)/ `ADR-CEP-14`(契约测试)
> **跨仓权威(只读)**: ArchForge `32 号` **无** collective plane 章节(实测 `grep -n "Collective\|collective\|SHARP\|group" 32-cep-based-coherence-architecture.md` 仅 1 处 `group_id` 提及)⇒ **本 change 的权威源是本仓 `ADR-CEP-01` / `ADR-CEP-03`**,不依赖跨仓
> **承接**: 队列 #4 **⏭(孤儿) M-4 行为侧** 已由队列 #3 落成(`cep-vc-mapping-contract`)· **⏭(孤儿) verdict 逐 `(network, vc)`** 已由队列 #4 落成(`cep-channel-dependency-gate`)⇒ **本 change 不再染指**;S1-1 / S1-2 / S1-3 / S1-4 / S1-7 滚动登记在 obj §4.0/§4.2 队列 #5 行
> **planning 输入**: `.rddf/improvements/cpptlm-cep-collective-plane-base.md`(5 段草稿)

---

## Why

Stage 1 的第 5 个 change。Fabric Collective Plane 是 CEP 6 层架构里的**旁路平面**(per `ADR-CEP-01 Decision 1`),与 Directory & Ordering Plane **横向并列、彼此零调用边**。它至今**仓内零实现** —— 而 `ADR-CEP-03 §6` 已把后续序列排死:本 change(P2)→ P3 SHARP → P4 UALink。**插件必须有被插件的平面**。

| 缺口 | 证据 |
|---|---|
| **平面本体零存在** | `grep -rn "CollectivePlane\|collective_region\|group_id\|CollectiveReq\|CollectiveEvt" include/ src/ test/ examples/ configs/` → **零命中**(仅 `compute_bundles_tlm.hh` 的 GPU `workgroup_id`,不相关) |
| **`INV-CEP-07-IC3` 只有文本断言** | `ADR-CEP-07:155` 定义「`NullCollectivePlane` 是 Collective Plane 默认实现,所有原语返回"不支持"」;`test/test_cep_canonical_invariants.cc:61` 只校验**编号存在**("Simulation + Seam" 组)⇒ **零行为载体** |
| **`INV-CEP-04-IC5` 的行为主体落在本平面** | `ADR-CEP-04:301` 定义「`fabric_ingress_classify` 中 "table 命中 + epoch invalid" 窗口 = 正常服务放行」;`fabric_ingress_classify` 属 `ADR-CEP-03 §4` ⇒ **owner ADR 是 CEP-04,行为主体在本平面** —— 队列 #4 未实现,是**遗留给本 change 的债** |
| **完成域判别规则零机器化** | `ADR-CEP-01 Decision 2` 的「requester-scoped → Coherence / group-scoped → Collective」是**全平台唯一的归属裁决规则**;`ADR-CEP-01 §3.3` 的 **7 条抓取点**(评审检查单)无一条可机器检查 |
| **H4 假设无载体** | `obj §9.1` H4:「完成域判别规则足以同时容纳 coherence + collective」,验证手段 = 「**同一实验跑 coherence 流 + collective 原语**」;只交 Null 实现 ⇒ H4 在 Stage 2(P3,队列 #10)之前**没有任何可跑载体** |

**为什么不能全推给 P3/P4**:REDUCE / BROADCAST / MULTICAST 的**数据路径**确实需要 fabric ingress 分类(per `ADR-CEP-03 §3.1` R2)⇒ 属 P3。但**接口 / 契约 / group 生命周期 / region-epoch / BARRIER rendezvous**不依赖任何硬件特定逻辑 —— 推迟等于让 P3 变成「从零起一个平面」,而不是「给平面加插件」。

**为什么在同一 change**:接口、类型、Null 实现、Local 实现、region/epoch 状态机、契约判据**互为对方的验证条件**(Local 的 group 生命周期要靠契约判据的零调用边闸证明没有反向依赖;契约判据的「未注册区域报错」要靠 Local 的 region 表才有意义)。

---

## What Changes

### 新增

| 文件/capability | 作用 |
|---|---|
| `openspec/specs/cep-collective-plane-base/spec.md`(delta) | **capability `cep-collective-plane-base`**(12 Scenario)—— `CollectivePlaneIf` 单接口 + 类型集 + `NullCollectivePlane`(**行为化 `INV-CEP-07-IC3`**)+ `LocalCollectivePlane`(group 生命周期 + region/epoch 表 + **BARRIER rendezvous**)+ **epoch invalid 窗口放行**(**行为化 `INV-CEP-04-IC5`**) |
| `openspec/specs/cep-collective-plane-contract/spec.md`(delta) | **capability `cep-collective-plane-contract`**(8 Scenario)—— 5 条**可机器检查**的契约判据:零调用边(include-graph 闸)/ 区域互斥(CEP-13)/ collective VC 预留与 coherence VC 不冲突 / 完成域判别(`group_id` 不进 Coherence Interface)/ canonical 10 流零 diff |
| `include/tlm/cep/collective_plane.hh` | **header-only** `CollectivePlaneIf` + 类型集 + `NullCollectivePlane` + `LocalCollectivePlane`;**testbench 定位**(不进 `ModuleFactory`、不注册 23 ABI、不改 frozen header) |
| `include/tlm/cep/collective_plane_contract.hh` | **header-only** 契约判据(5 条)+ 三态 SKIP/PASS/FAIL + verdict 逐条输出 |
| `include/tlm/cep/collective_planes.json` | 本仓机器可读的 collective plane 声明(plane 绑定默认 null / registered region 表 / collective VC 预留);**诞生即不冻结**,per 队列 #4 `protocol_instances.json` 先例 |
| `include/tlm/cep/collective_planes.schema.json` | 配套 schema(**诞生即不冻结**) |

### 修改

| 文件 | 改什么 |
|------|--------|
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 状态列 📋 → 🟡 · §4.2 承接列 · §5.2 KPI 0/3 → **1/3** · §11 Entry #66 |
| `.rddf/roadmap/phases/phase-10.md` | 队列 #5 对应子任务行状态 + KPI baseline 行 |
| `.rddf/roadmap.md` · `AGENTS.md` | KPI baseline 0/3 → 1/3 |
| **`docs/adr/ADR-CEP-00-platform-charter-and-governance.md`** | **Status Update 2026-10-26**:队列 #5 已启动(KPI 0/3 → 1/3)+ 上一段「当前态」→「当时态」 |
| **`docs/adr/ADR-CEP-03-fabric-collective-plane-internals.md`** | **Status Update**:**§5 Bundle 路径偏离声明**(现实无 `coherence_bundles_tlm.hh` ⇒ 类型内嵌、Bundle 化推迟至 P3) |
| **`docs/adr/ADR-CEP-01-fabric-collective-plane.md`** | **Status Update**:**§3.3 抓取点可测性裁决声明**(7 条中哪几条本 change 可机器检查、哪几条推迟) |

> `scripts/test/cep_gate_selftest.sh` 的锚点迁移(#5 → #6)是**归档日**动作(per `Entry #64` 模板第 3 步),planner 阶段**不动** —— 动了会让当前活队列锚点失配。

### 冻结面

**3 个成员改动**(均**仅追加** `## Status Update` 段;走 `cep_freeze_check.sh --update --authorized-by "Entry #66"` 两步授权):

| 成员 | 追加内容 |
|------|---------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | Status Update 2026-10-26 —— 队列 #5 已启动 + KPI 数值 |
| `docs/adr/ADR-CEP-03-fabric-collective-plane-internals.md` | §5 Bundle 路径偏离声明 |
| `docs/adr/ADR-CEP-01-fabric-collective-plane.md` | §3.3 抓取点可测性裁决声明 |

**零改动**:`ADR-CEP-07` · `ADR-CEP-04` · `ADR-CEP-02` · `ADR-CEP-09` · `ADR-CEP-13` · `protocol_instance.schema.json` · `fabric.{json,schema.json}` · 3 份既有 main spec · 其余 ADR-CEP 家族成员

---

## Capabilities

### capability 1 —— `cep-collective-plane-base`(12 Scenario)

**平面本体**:接口 + 类型 + 两个实现 + group/epoch 状态机。

| 组 | 内容 |
|---|---|
| 接口面 | `CollectivePlaneIf` 单接口的**方法集**与 ADR-CEP-03 §2 Decision 2 逐字对齐(group 生命周期 3 + `send` / `recv` / `collective_barrier` / `query_capability`);**原语最小集 = {REDUCE, BROADCAST, MULTICAST, BARRIER}**(AllReduce = REDUCE + BROADCAST 复合,**不列原语**) |
| 类型面 | `group_id_t` / `member_set_t` / `group_attrs_t` / `addr_range_t` / `access_mode_t` / `CollectiveReq` / `CollectiveEvt`(`Result` \| `Completion` \| `Fault`)/ `cap_t`;`CollectiveEvt` 必须含 **Fault 变体**(per `ADR-CEP-01 §3.3` 抓取点 6 partial failure 显式建模) |
| Null 面 | `NullCollectivePlane`:**全部**原语返回「不支持」—— **行为化 `INV-CEP-07-IC3`**(此前只有编号文本断言) |
| Local 面 | `LocalCollectivePlane`:group 生命周期 + region/epoch 表 + **BARRIER rendezvous**(N-1 到达不释放 / N 到达释放)+ REDUCE·BROADCAST·MULTICAST **到「未注册区域立即报错」为止**(数据路径不实现) |
| epoch 面 | **`table 命中 + epoch invalid` ⇒ 正常服务放行**(非 fail-stop)—— **行为化 `INV-CEP-04-IC5`**(代还队列 #4 遗留给本平面的债) |
| 区域面 | 区域互斥(CEP-13 语义):重叠注册 ⇒ `group_register_error`;drain 失败 ⇒ **不建 epoch**;未注册区域上的操作 ⇒ **立即报错**,不静默 |

### capability 2 —— `cep-collective-plane-contract`(8 Scenario)

**契约判据**:把 ADR 的「说辞」变成「机器可判」。

| 判据 | 机器化形态 | 依据 |
|---|---|---|
| **零调用边** | include-graph 闸(collective 头文件**不得** include directory / ordering / sharer 头)+ 反向 fixture | `ADR-CEP-01 §3.3` 抓取点 1(高 impact · 必查) |
| **canonical 10 流零 diff** | 与 `ADR-CEP-02 §2` 清单的 diff 断言 + 反向 fixture | 抓取点 4(中) |
| **完成域判别** | `group_id` 等 group-scoped 字段**不得**出现在 Coherence Interface 侧 | `ADR-CEP-01 Decision 2` + §3.2 |
| **collective VC 预留** | 预留 VC ∈ 0-3 且与 `fabric.json` 既有 `vc_binding` **不冲突**(crosscheck 消费 `collective_planes.json`) | 抓取点 3(高 impact)· 死锁通道隔离 |
| **区域互斥** | 注册路径的重叠检测 + epoch 状态机单测 | `ADR-CEP-03 §3.2/§3.4` + CEP-13 |

三态显式化(SKIP ≠ PASS ≠ FAIL)+ verdict 逐条可 grep —— 与队列 #4 的判据形态对齐。

### 与 B2(66 条 invariant 机器检查 100%)的关系 —— **本 change 新增 invariant 数 = 0**

`test/test_cep_canonical_invariants.cc` 的 `expected_groups()` 把 15 组 × 66 条编号**硬编码**;ADR-CEP-01/03 **在 15 组里无组** ⇒ **新增 `INV-` 编号即契约违反**。本 change 的贡献方式是:

1. **行为化既有条目**:`INV-CEP-07-IC3`(NullCollectivePlane 全原语返回不支持)+ `INV-CEP-04-IC5`(epoch invalid 窗口放行)—— 两条**已在 66 条计数内**,本 change 给它们**行为载体**;
2. **契约层加固**:零调用边 + canonical 10 流零 diff —— 对应「Simulation + Seam」与「Coherence Interface」两组的既有条目;
3. **delta spec 的 Scenario 断言**:归档时并入 main spec(per 队列 #4 先例)。

> **若上述清单为空则本 change 自我否决**(C-5 验收项)。

---

## Impact

- **B2**:新增 invariant 数 = **0**;行为化 **2** 条 + 契约层加固 **2** 组。
- **KPI**:0/3 → **1/3**(CEP 系列自身并发 ≤ 1,per `obj §6.3 D1`)。
- **冻结面**:37 → **37**(成员数不变;3 份 ADR 内容更新)。
- **对后续 change 的影响**:P3 SHARP / P4 UALink 有了被插件的平面基座;队列 #6 CHI RN-F 可消费 `NullCollectivePlane` 默认绑定;H4 假设获得可跑载体。
- **偏离登记**:`ADR-CEP-03 §5`(Bundle 路径)+ `ADR-CEP-01 §3.3`(抓取点可测性)—— 两条**已拍板项**的偏离,均以 ADR Status Update 显式登记(冻结面 ⇒ 两步授权),**禁静默偏离**。

---

## Acceptance

> **红绿分离**：「判据实现退化」= 用例红;「判定结论为红」= 用例**仍绿**且结论**登记为交付物**。
> **每条判据须配反向 fixture 或正控**(纯负向断言无正控 = 判据可能根本没生效)。

### capability 1 —— `cep-collective-plane-base`(12 Scenario)

- [ ] **A-1-1** `CollectivePlaneIf` 方法集与 `ADR-CEP-03 §2 Decision 2` **逐字对齐**(group 生命周期 3 + `send` / `recv` / `collective_barrier` / `query_capability`);原语集**恰好 4 个** = {REDUCE, BROADCAST, MULTICAST, BARRIER}
- [ ] **A-1-2** 类型集齐备:`CollectiveEvt` 含 **Fault 变体**(partial failure 显式建模,per 抓取点 6);`CollectiveReq` 的 group-scoped 字段齐备(`group_id` / `op` / `tag` / `src_member` / `reduce_op` / `dtype` / `multicast_mask`)
- [ ] **A-1-3** `NullCollectivePlane` **全部**原语返回「不支持」(**行为化 `INV-CEP-07-IC3`**)—— **反向 fixture**:注入「假装支持」的 plane ⇒ 红
- [ ] **A-1-4** `LocalCollectivePlane` group 生命周期状态机可执行(create → register_region → destroy),含 **group_destroy 对 in-flight 操作的 `Fault` 语义**(ADR 留白的本 change 拍板)
- [ ] **A-1-5** **BARRIER rendezvous**:N-1 到达**不**释放、N 到达释放 —— **反向 fixture**:阈值改 N-1 ⇒ 红
- [ ] **A-1-6** **epoch invalid 窗口放行**:`table 命中 + epoch invalid` ⇒ **正常服务放行**(非 fail-stop)—— **行为化 `INV-CEP-04-IC5`** —— **反向 fixture**:删掉 epoch 检查 ⇒ 红
- [ ] **A-1-7** 区域互斥(CEP-13 语义):重叠注册 ⇒ `group_register_error`;drain 失败 ⇒ **不建 epoch** —— **正控**:合法注册成功
- [ ] **A-1-8** 未注册区域上的 collective 操作 ⇒ **立即报错**,不静默 —— **反向 fixture**:静默返回 ⇒ 红
- [ ] **A-1-9** functional 模式下 collective **不产生 latency**(与 `INV-CEP-07-IC6` 兼容)
- [ ] **A-1-10** `query_capability` 的边界写死:未知 `op` 与 `max_bytes == 0` 的返回契约明确
- [ ] **A-1-11** `group_id` **不得**出现在 Coherence Interface 侧接口/契约(完成域判别的**反向**面)
- [ ] **A-1-12** **零调用边**:collective 头文件**不得** include directory / ordering / sharer 头 —— **反向 fixture**:注入 include ⇒ 红

### capability 2 —— `cep-collective-plane-contract`(8 Scenario)

- [ ] **A-2-1** 声明源 `collective_planes.json` 形态与 schema 一致(plane 绑定默认 null / registered region 表 / collective VC 预留);**诞生即不冻结**
- [ ] **A-2-2** 判据**三态显式化**:SKIP ≠ PASS ≠ FAIL,日志可 grep(`[verdict]` 格式,per 队列 #4 先例)
- [ ] **A-2-3** **零调用边**闸可执行(include-graph 扫描 + 编译期依赖)—— 与 A-1-12 互为正交实现(一个查 include,一个查类型)
- [ ] **A-2-4** **canonical 10 流零 diff**:本仓不新增/不改 canonical 流清单 —— **反向 fixture**:fixture 改流清单 ⇒ 红
- [ ] **A-2-5** **collective VC 预留** ∈ 0-3 且与 `fabric.json` 既有 `vc_binding` **不冲突** —— **反向 fixture**:fixture 改冲突 VC ⇒ 红
- [ ] **A-2-6** 区域互斥判据与 A-1-7 **同源**(共享同一判据实现,**禁双实现**,per 队列 #4 双挂先例)
- [ ] **A-2-7** 声明源缺失/畸形 ⇒ **判据 FAIL**(配置错误),与「无可检查对象 ⇒ SKIP」严格区分(per 队列 #4 `Entry #65` 的 `source_error` 语义)
- [ ] **A-2-8** **B2 贡献清单非空**且逐条对应到本 change 的机器可检查项

### 收尾

- [ ] **A-FIN-1** `openspec validate cpptlm-cep-collective-plane-base --strict` rc=0
- [ ] **A-FIN-2** 守门 7 项 rc=0;`cep_gate_selftest` 全绿;`openspec list --json` 实测 **1/3**
- [ ] **A-FIN-3** `ctest` 全绿;`[cep]` 的 case/断言数**逐项登记**
- [ ] **A-FIN-4** **归档治理 6 步**(per `Entry #64` 模板,design.md 已预登记):(1) `FROZEN_GLOBS` 扩集(2) manifest 重生成授权串 (3) selftest 锚点迁移 #5 → #6(4) `cep_doc_ref_check` VIRTUAL_PATHS 双态注册(5) 活文档数字一致性同步(6) `openspec archive --yes`
- [ ] **A-FIN-5** 反向验证自查:每条 fixture 变异必红 + GREEN 样本必绿
