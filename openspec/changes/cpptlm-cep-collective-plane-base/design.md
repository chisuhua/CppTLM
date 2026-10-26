# Design — cpptlm-cep-collective-plane-base v1.0

> **状态**: 🟡 Proposed(2026-10-26,planner 阶段)
> **队列**: `obj-cep-platform §4.0` 队列 **#5**(Stage 1 / 2027-Q4)
> **形态**: 单 change 单 Phase · 标称 8 周 → 实际 4-6 周当量(per 队列 #2/#3/#4 实测先例)
> **裁决日期**: 2026-10-26(DRI 四问,全部取推荐项)

---

## Context

### 现状(队列 #4 完成后)

| 项 | 状态 |
|---|---|
| **Fabric Collective Plane** | **零存在**。`grep -rn "CollectivePlane\|collective_region\|group_id\|CollectiveReq\|CollectiveEvt" include/ src/ test/ examples/ configs/` → 零命中 |
| `INV-CEP-07-IC3`(NullCollectivePlane 默认实现) | 只有**编号文本断言**(`test_cep_canonical_invariants.cc:61` 校验编号存在于 "Simulation + Seam" 组)⇒ 零行为载体 |
| `INV-CEP-04-IC5`(epoch invalid 窗口放行) | 同样只有编号断言;**行为主体 `fabric_ingress_classify` 属 `ADR-CEP-03 §4`** ⇒ 队列 #4 遗留给本平面的债 |
| 完成域判别规则 | 只存在于 `ADR-CEP-01 Decision 2` 文本;`§3.3` 的 7 条抓取点无一条可机器检查 |
| `include/bundles/coherence_bundles_tlm.hh` | **不存在**。`ADR-CEP-03 §5` 的「沿用该路径」基于 06-bis P2.0 实施的假设(`06-bis §5` 标注 "include/bundles/coherence_bundles_tlm.hh (P2 实施)"),而 06-bis 的 P2 **尚未实施** |
| 仓内 bundle 参照 | `noc_bundles_tlm.hh`(`vc_id` ch_uint<8> 值域 0-3;`flit_category` 3 类)/ `cache_bundles_tlm.hh` / `cpphdl_types.hh`(`BUNDLE_FIELD_WIDTH` 编译期锁) |
| 仓内 CEP header 参照 | `include/tlm/cep/`:`sharer_tracker.hh` / `address_resolver.hh` / `channel_dependency_graph.hh` / `ordering_point_checker.hh` / `stub_cache_core.hh` —— 全部 **header-only、testbench 定位、不进 `ModuleFactory`** |

### 目标态

```
ADR-CEP-01 §1.1 的 6 层架构里,有一层从「空」变成「可执行 + 可机器检查」

   Coherence Interface Layer
            │
   Protocol Engine
            │
   ┌────────┴────────┐
   │                 │
Directory &        Fabric Collective
Ordering Plane    / Sync Plane        ← 本 change 落地
   │                 │
   └────────┬────────┘
            ▼
   Fabric & Transport Plane
```

- `CollectivePlaneIf` = ADR-CEP-03 §2 Decision 2 的**逐字**落地(group 生命周期 / `send` / `recv` / `collective_barrier` / `query_capability`)。
- 两个实现:`NullCollectivePlane`(默认绑定,行为化 `INV-CEP-07-IC3`)+ `LocalCollectivePlane`(group/epoch 状态机 + BARRIER rendezvous)。
- 一层契约判据:把 `ADR-CEP-01 §3.3` 抓取点里**本 change 可测**的部分变成机器闸。

### 关键约束

| # | 约束 | 来源 |
|---|------|------|
| C-1 | **不新增 `INV-` 编号**。15 组 66 条编号是 `test_cep_canonical_invariants.cc::expected_groups()` 的**硬编码**;ADR-CEP-01/03 在 15 组里**无组** ⇒ 新增编号即契约违反 | 实测 `test/test_cep_canonical_invariants.cc:46-82` |
| C-2 | **不扩 `protocol_instances.json`**。`protocol_instance.schema.json` 是**冻结成员**且 `additionalProperties: false` ⇒ 加字段要走两步授权;独立声明源零授权成本 | `include/tlm/cep/protocol_instance.schema.json` |
| C-3 | **零调用边**:Collective Plane 与 Directory & Ordering Plane **横向并列、彼此零调用边** | `ADR-CEP-01 §3.3` 抓取点 1(高 impact · 必查) |
| C-4 | **R3 的 drain 由发起注册的 node 端经自身 Coherence Interface 端口发出**,plane 间零直接调用边 | `ADR-CEP-03 §3.4`(per oracle B2-8 修订) |
| C-5 | **不进 23 ABI / 不改 frozen headers / 不破 `include/tlm/cache_tlm.hh` / 不接管 `dgpu_board_shell.cc`** | `07 号 §6.3` 冻结面声明 |
| C-6 | **原语最小集 = {REDUCE, BROADCAST, MULTICAST, BARRIER}**;AllReduce = REDUCE + BROADCAST **复合**,不列原语 | `ADR-CEP-03 §2` |
| C-7 | **functional 模式零延迟**(`avg_latency_cycles == 0`) | `INV-CEP-07-IC6` |
| C-8 | **canonical 10 流零 diff** | `ADR-CEP-01 §3.3` 抓取点 4 + `ADR-CEP-02 §2` |

---

## Goals / Non-Goals

### Goals

- **G-1** `CollectivePlaneIf` 成为仓内**可编译、可执行**的契约面(逐字对齐 ADR-CEP-03 §2 Decision 2)。
- **G-2** `INV-CEP-07-IC3` 与 `INV-CEP-04-IC5` 获得**行为载体**(两条已在 66 条计数内,本 change 只行为化、**不新增编号**)。
- **G-3** `ADR-CEP-01 §3.3` 抓取点中**本 change 可测**的部分成为**机器闸**,且每条闸配**反向 fixture**。
- **G-4** group-scoped 完成纪律有**可跑载体**(BARRIER rendezvous),使 H4 假设获得载体而非空话。
- **G-5** 声明源 + 判据 + 测试三者**闭环**(不产生死文件)。
- **G-6** 为 P3 / P4 留下**明确的接缝**(而不只是「以后再说」)。

### Non-Goals

| 不做 | 理由 | 归属 |
|---|---|---|
| REDUCE / BROADCAST / MULTICAST **数据路径** | 需 fabric ingress 分类(R2) | **P3 SHARP** |
| **fabric ingress 的「接线」**(把 R2 判定结果接到 fabric 路由) | 旁路 plane 侧**无** coherence 路径可「走」;接线是 fabric 侧工作 | **P3** |
| ~~R1 / R2 的真实 fabric ingress 分类~~ | **已澄清(per D-ι)**:R2 谓词的**判读侧**归本 change,**接线侧**归 P3 | — |
| 跨平面 ordering 契约 **A / B / E** | Stage 1 物理不可测(无 coherent atomic 流量、无 SHARP 插件)⇒ **禁写恒真/SKIP 判据** | Stage 2+ |
| 抓取点 2「组播投递单实现」 | fabric 组播机制不存在 ⇒ 仅**声明层**断言 | P3 |
| `collective_stalls` metric 落地 | 属 `INV-CEP-06-IC2` 实验产出契约;本 change 无实验载体 ⇒ 落即空 metric | Stage 1 末(有实验载体时) |
| `ModuleFactory` / `REGISTER_MODULE` 注册 | `07 §6.3` = 最终形态口径;Stage 1 与队列 #3/#4 先例一致 | P3(有真实 adapter 时) |
| Bundle 化 | 见 D-β | P3(有真实传输通道时) |
| **新增 `INV-` 编号** | C-1 | **永不**(除非 `expected_groups()` 同步变更) |

---

## Decisions

### D-α:范围**切小**为「接口 + Null + Local(仅 BARRIER rendezvous)」

**问题**:`ADR-CEP-03` 拍板了接口与契约,但**从没拍板**「Stage 1 是否要有非 Null 行为实现」。

**裁决**:**要**,但只做到 `LocalCollectivePlane` 的 group 生命周期 + region/epoch 表 + **BARRIER rendezvous**;REDUCE / BROADCAST / MULTICAST **到「未注册区域立即报错」为止**(数据路径不实现)。

**理由**:
1. `INV-CEP-07-IC3` / `INV-CEP-04-IC5` 需要**行为载体** —— 纯接口版让这两条到 2029-Q2 永远停在契约层文本断言;
2. H4 假设的验证手段是「同一实验跑 coherence 流 + collective 原语」⇒ 需要**能跑**的东西;
3. **估时匹配**:纯接口版 8 周明显虚高(队列 #4 标称 12 周实际 4-6 周当量);含 Local 版的体量与 8 周相称;
4. BARRIER 是**唯一不需要硬件特定逻辑**的原语(不依赖 fabric ingress 分类)⇒ 是 Stage 1 内可实现的最大集合。

**风险与缓解**:BARRIER 的相位语义容易踩 `INV-CEP-07-IC6`(functional 模式零延迟)⇒ `LocalCollectivePlane` 做成**零延迟即时完成**(见 A-1-9),并在 spec 里显式声明 collective 在 functional 模式**不产生 latency 字段**。

### D-β:Bundle 路径 —— **类型内嵌,Bundle 化推迟**(偏离 `ADR-CEP-03 §5` 已拍板项)

**问题**:`ADR-CEP-03 §5` 拍板「新增 `CollectiveReqBundle` / `CollectiveEvtBundle`,**沿用** `bundles::` 命名空间 + `coherence_bundles_tlm.hh` 路径」。但:

- `include/bundles/coherence_bundles_tlm.hh` **不存在**(06-bis P2 未实施);
- 提前新建 `coherence_bundles_tlm.hh` 会**抢 06-bis P2 的文件** ⇒ 两个 change 改同一文件 ⇒ 冲突(与 `ADR-CEP-07A` 的 artifact disposition 冲突);
- Bundle 的价值在**有真实传输通道时**;本 change 的 plane 是纯内存 + rendezvous,无通道。

**裁决**:**类型内嵌** `include/tlm/cep/collective_plane.hh`(沿用 `tlm::cep` 命名空间 + `ch_uint` 宽度体系,与队列 #3/#4 的 `stub_cache_core.hh` / `sharer_tracker.hh` 风格一致),**不落 Bundle**。

**治理**:这是**偏离已拍板项** ⇒ 必须在 `ADR-CEP-03` 追加 **Status Update** 显式登记(该 ADR 在 37 冻结成员内 ⇒ 走两步授权,per 台账 Entry #66)。**禁静默偏离**。Status Update 同时写明「Bundle 化推迟至 P3(有真实传输通道时)届时新建 `include/bundles/collective_bundles_tlm.hh`」—— 给出未来的**具体路径**,避免「以后再说」。

> **未选方案记录**:新建 `include/bundles/collective_bundles_tlm.hh` 并立即落 Bundle —— 被否:无消费方 ⇒ 死文件 + 提前承担 `BUNDLE_FIELD_WIDTH` 锁的维护成本,且与 06-bis P2 的文件归属边界仍需额外澄清。

### D-γ:**header-only,不进 `ModuleFactory`**(对齐 `07 §6.3` 的「最终形态」口径)

**问题**:`07 号 §6.3` 的注册机制表写着「`FabricCollectivePlane` 派生类 → `SimModule` → `REGISTER_MODULE(T)`」,而同表的 SharerTracker 在队列 #4 **也没注册**(`grep CEP include/modules_cluster.hh` 零命中)。

**裁决**:**不注册**。`07 §6.3` 读作**最终形态**口径(该表描述平台终态的注册方式,不是 Stage 1 的中间形态)。Stage 1 与队列 #3/#4 先例一致:header-only、testbench 定位、由测试夹具装配。

**治理**:差异**只在 design.md 登记**,**不动 `07 号`**(它是冻结面成员,改它要走两步授权且收益极低)。将来 P3 有真实 adapter 时,再按 `07 §6.3` 注册。

### D-δ:独立声明源 `include/tlm/cep/collective_planes.json`

**问题**:collective plane 的声明(plane 绑定 / registered region / VC 预留)放哪?

**裁决**:**独立文件 + 配套 schema,两者都「诞生即不冻结」**。

**理由**:C-2 —— `protocol_instances.json` 的 schema 是冻结成员且 `additionalProperties: false`;扩它要走两步授权 + 台账 entry,而 collective 的声明与协议实例声明是**不同维度**(前者是 plane 能力与资源,后者是协议能力)。队列 #4 的 `protocol_instances.json` 已是「诞生即不冻结」的先例。

**声明最小集**(防膨胀):

| 字段 | 含义 |
|---|---|
| `plane_binding` | 协议实例 → 绑定哪个 collective plane(默认 `null`) |
| `registered_regions` | 已注册的 collective region 表(range → gid / mode) |
| `collective_vc_reservation` | collective 流量预留的 VC(∈ 0-3) |

**配套**:`test_cep_collective_plane_contract.cc` 消费它 ⇒ **不产生死文件**(AI slop 风险 #5 的直接对策)。

### D-ε:**B2 贡献方式 = 行为化既有条目**(新增 invariant 数 = 0)

**问题**:本 change 怎么贡献 B2(66 条 invariant 机器检查 100%)?

**裁决**:**不新增编号**;贡献方式三条:

| # | 方式 | 内容 | 归属组 |
|---|------|------|-------|
| 1 | **行为化** | `INV-CEP-07-IC3`:NullCollectivePlane 全原语返回「不支持」 | "Simulation + Seam" |
| 2 | **行为化** | `INV-CEP-04-IC5`:epoch invalid 窗口 = 正常服务放行(**判读侧**;**P3 实现真 ingress classify 时须在那里重新行为化** —— per D-ι 裁决 1) | "Sharer Tracker" |
| 3 | **契约层加固** | 零调用边(抓取点 1)+ canonical 10 流零 diff(抓取点 4) | "Coherence Interface" / "Simulation + Seam" |

> **为何 1/2 能算「行为化」而非「另立新条」**:两条编号**已在 66 条内**,本 change 给它们**可执行载体**;`test_cep_canonical_invariants.cc` 的编号校验不变,但**行为断言**新增 ⇒ 覆盖率提升而计数不变。
>
> **为何不新建「Collective Plane」组**:那要改 `expected_groups()` 的硬编码 ⇒ 违反 C-1(且属「66 条」的口径变更,超出本 change 授权)。

**自我否决条款**:若上述清单在 builder 阶段归零 ⇒ **本 change 自我否决**(C-5)。

### D-ζ:两个 ADR 留白的**本 change 拍板**

`ADR-CEP-03` 留了两处白,两边各长一份会让 P3 走偏。**本 change 拍死**:

| 留白 | 拍板 | 理由 |
|------|------|------|
| **AllReduce 复合归属**(ADR-CEP-03 §2 说「由 plane 内部或调用方组合」) | **调用方组合**;plane 守 **4 原语最小集**不变 | 避免 plane 内部长出第 5 个原语 ⇒ 违反 C-6;且调用方组合是「plane 只做必要的事」的一致性 |
| **`group_destroy` 对 in-flight 操作**(ADR-CEP-03 §3.1 R4 只说 stale copy 失效化) | **对 in-flight 操作发 `Fault(gid, culprit, cause)`**,`group_destroy` **不阻塞** | 阻塞会引入「destroy 等待 group 完成」的隐式依赖 ⇒ 违反 C-3 的解耦精神;`Fault` 已是 `CollectiveEvt` 的既定变体(抓取点 6) |

### D-η:**归档日治理 6 步预登记**(per `Entry #64` 模板)

上一轮的教训:planner 不预登记,builder 归档时必漏。**本 change 的 design 提前列出**:

| # | 步骤 | 本 change 的具体值 |
|---|------|-----------------|
| 1 | `FROZEN_GLOBS` 扩集 | +2 份 main spec(`cep-collective-plane-base` / `cep-collective-plane-contract`);**只冻 main spec**,归档件 delta 不入集(per `Entry #64` 第 1 步的口径) |
| 2 | manifest 重生成 | `cep_freeze_check.sh --update --authorized-by "Entry #N"`(N = 归档时的台账 entry 号) |
| 3 | selftest 锚点迁移 | `cep_gate_selftest.sh` 两处 fixture 的「活队列 change 名」锚点 **#5 → #6**(`cpptlm-cep-chi-rnf-subset`);**当前锚点已钉在 #5**(per `Entry #64` 第 3 步的迁移结果) |
| 4 | `cep_doc_ref_check` VIRTUAL_PATHS 双态注册 | ① 登记 `openspec/changes/cpptlm-cep-collective-plane-base`(归档前路径保留历史指向);② **移除** planner 阶段预登记的 4 条「设计示意」精确路径(`include/tlm/cep/collective_plane.hh` / `_contract.hh` / `collective_planes.json` / `collective_planes.schema.json` —— builder 阶段已真实创建);③ **保留** `include/bundles/collective_bundles_tlm.hh`(P3 才建,本 change 不创建) |
| 5 | 活文档数字一致性 | KPI 基线 1/3 → 0/3 的 **6 处**(obj §5.2/§4.1 · roadmap.md · phase-10 · AGENTS.md · umbrella improvement · ADR-CEP-00)+ 队列 #5 行 → ✅ archived + 孤儿登记行重指向**队列 #6**(§4.0 + §4.2 两处,标记与项名同行) |
| 6 | `openspec archive --yes` | 完成归档语义(delta → main spec) |

> **另**:归档前须把 `include/tlm/cep/collective_planes.json` 的「诞生即不冻结」状态**复核一次**(per 队列 #4 的 S1-4 复核口径)—— 若已有 implemented 实例消费它,可能需要入冻结集。

### D-θ:估时分解(对标队列 #4 体量)

| 项 | 周 | 产出 |
|---|---|---|
| 接口 + 类型 + Null | 1.5 | `collective_plane.hh` 前半 + `INV-CEP-07-IC3` 行为化 |
| Local:group 生命周期 + region/epoch | 2 | `collective_plane.hh` 后半 + `INV-CEP-04-IC5` 行为化 + D-ζ 两条留白 |
| BARRIER rendezvous | 1.5 | 同相位无序 / 跨相位有序 + N-1/N 阈值反向 fixture |
| 声明源 + schema + 契约判据 | 2 | `collective_planes.json` + `collective_plane_contract.hh` + 5 条判据 |
| 测试 + 反向 fixture | 2 | 3 个 test 文件(行为面 / 判据面 / include-graph 闸) |
| 评审 + buffer | 1 | critic 复审 + 归档治理 |
| **合计** | **10 人周 ≈ 8 周标称(单线程 1.5 人)** | 与队列 #4 的「4 header + 1 CLI + 3 test / 22 Scenario」同量级 |

### D-ι:**R2 的「判读 / 接线」分层** + epoch 窗口的可构造性 + 「不支持」的返回通道

> **来源**:critic 对本 change planner 产出的评审(APPROVE_WITH_CONDITIONS)—— 1 个 BLOCKER 级计划洞 + 6 SHOULD_FIX。
> 本条一次性处置其中与**契约面**相关的 4 项(BLOCKER-1 / S1 / S2 / S3)。

**BLOCKER-1 的原洞**:plan 里同时说「R1/R2 的真实 fabric ingress 分类 ⇒ 推迟至 P3」(Non-Goals)与
「`table 命中 + epoch invalid` ⇒ 放行」(Scenario 6)—— 而 R2 谓词 = `(dest_addr ∩ collective_region_table) && group_epoch_valid`,
**Scenario 6 正是 R2 的判读逻辑**。且 `CollectivePlaneIf` 的 7 方法里**无任何 classify/判定入口**
⇒ 「放行正常服务,走 coherence 路径」在旁路平面上**无可观察点**(plane 侧没有 coherence 路径可走)。

**裁决 1 —— R2 分层**:

| 层 | 内容 | 归属 |
|---|------|------|
| **判读侧** | 「某地址的 collective 归属状态」纯判定 = 区域表查找 + epoch 有效性 ⇒ 三态:`NoRegion` / `RegisteredEpochInvalid` / `RegisteredActive` | **本 change** |
| **接线侧** | 把判定结果接到 fabric 路由(真正的 `fabric_ingress_classify` + divert 计数 + R5 观测) | **P3** |

**「放行」在 plane 侧的可观察形态** = 判定结果为 `RegisteredEpochInvalid` ⇒ **不 divert**。
这样 Scenario 6 有可观察点,且**不违反** A-1-1 的 7 方法锁(判定查询是 `CollectivePlaneIf` 之外的**只读辅助口**)。

**裁决 2 —— epoch 窗口的可构造性(两阶段)**:epoch-invalid 窗口若在同步 `group_register_region` 内零宽度,则该 Scenario **不可稳定构造**
(只能靠竞态)。⇒ `group_register_region` 返回后 epoch 保持 invalid,直至 **drain 完成回告**
(`complete_drain` 类的**装配/驱动用**辅助口)。**先例**:`FullMapDirectory::set_sharer` / `clear_sharer` 同样是
`SharerTrackerIf` 4 方法之外的**目录安装面** —— plane 的「drain 回告」与目录的「sharer 安装」是同一种**装配缝**。
⇒ 窗口非零宽度、可稳定构造,**不依赖竞态**。

**裁决 3 —— 「不支持」的返回通道**(S1):`ADR-CEP-03 §2` 的伪码里 `group_create` 返回 `group_id_t`、
`group_register_region` 返回 `status_t`、而 `send`/`recv`/`collective_barrier` 是 `task`(**无返回值**)
⇒ 「所有原语返回不支持」在无返回值的原语上**无表达通道**。裁决:
- **状态查询口**(`last_status()` 类的**只读辅助口**,per `FullMapDirectory::event_count` 先例)承载「不支持」的可观测性;
- `group_create` 失败另用**哨兵 group id**表达;`group_register_region` 失败用 `status_t` 错误码;
- **判据形态**:调用原语 ⇒ 查状态查询口 ⇒ 断言「不支持」+ 断言**无副作用**(未建 group / 未投递事件)。
⇒ builder 不必当场发明返回通道。

**裁决 4 —— 已注册区域上的数据原语行为**(S2):已注册 + epoch 生效的区域上 `send(REDUCE/BROADCAST/MULTICAST)`
⇒ 明确的**「数据路径未实现」状态**,**与**「未注册区域 ⇒ `group_register_error`」**可区分**,且**不投递事件**。
⇒ 接口不「承诺了但行为缺失」。

**裁决 5 —— 删掉恒真子句**(S3):BARRIER 的「同相位内跨参与者无序」在零延迟 functional 实现中**无并发可观察**
⇒ 改为可观察形态「同相位内允许多个成员先后到达且互不排斥」;「跨相位有序」保留(可测)。

---

## 实施期裁决(builder 阶段,2026-10-26)

> 每条给出「权威来源 + 实测证据 + 偏差性质(授权表述 / 措辞修订 / 语义修正)」。
> 性质图例:**表述** = 措辞与权威原意一致;**措辞修订** = 权威本意不变但字面需改;**语义修正** = 权威结论变了。

### D-κ:`vc_binding` 的权威位置 —— plan 原文**指错了对象**(语义修正)

| 项 | 内容 |
|---|---|
| **权威** | `vc_binding` 在 `include/tlm/cep/protocol_instances.json` 的 `instances[].vc_binding`;`fabric.json` 只有 `networks[].vc_count` |
| **plan 原写** | 「与 `fabric.json` 既有 `vc_binding` 不冲突」—— **对象错** |
| **实测** | `moesi_gpu` 与 `mesif` 在 `compute_noc` / `ualink_fabric` 上均占满 VC 0-3;`fabric.json` 的 `networks` 只有 `vc_count` |
| **修正** | 判据 CP-03 改为「与 `protocol_instances.json` 既有 `vc_binding` 不冲突」;`fabric.json` 只用于取 `vc_count` 做越界检查 |

### D-λ:collective VC 预留的**实测现状** —— 本仓当前拓扑**无** collective 可用隔离 VC(语义补充)

| 项 | 内容 |
|---|---|
| **实测** | 2 个协议实例 × 2 个网络,coherence 占满 VC 0-3 ⇒ **任何** collective VC 预留都会与 coherence 冲突 |
| **风险** | 若判据形态是「预留不冲突 ⇒ 否则 FAIL」,则默认状态**恒红** ⇒ 违反「禁写恒真/恒红判据」 |
| **裁决** | 声明源默认 `collective_vc_reservation = []`(**本 change 不预留** —— 预留是**拓扑级决策**,归 P3);判据的判别力由**反向 fixture**(注入冲突预留 ⇒ 判红)承载;并把「本仓当前拓扑无 collective 可用 VC」作为**结论登记**在声明源的 `current_state` |

### D-μ:barrier 的**成员身份**需装配缝(措辞修订)

| 项 | 内容 |
|---|---|
| **权威** | `ADR-CEP-03 §2` 的 `collective_barrier(group_id_t gid, phase_t phase)` —— **签名里没有 member 参数** |
| **问题** | rendezvous 的 N-1/N 判定必须知道「谁到了」⇒ 纯 7 方法接口**无法**计数 |
| **裁决** | 加**装配/驱动用辅助口** `set_current_member(member)`(不在 7 方法接口面内)。**先例**:`FullMapDirectory::set_sharer` 同样是 `SharerTrackerIf` 4 方法之外的**目录安装面**;plane 的「成员身份」与目录的「sharer 安装」是同一种**装配缝** |
| **一致性** | 7 方法接口面**零变化** ⇒ A-1-1 的「逐字对齐」仍成立 |

### D-ν:`Complete_drain` 与 epoch 两阶段 —— 装配缝的第 2 个(语义补充)

`ADR-CEP-03 §2` 的 `group_register_region` 无「drain 完成回告」入口 ⇒ epoch 两阶段(per D-ι 裁决 2)需要第 2 个装配缝
`complete_drain(gid)`。它与 D-μ 的 `set_current_member` 同属**装配/驱动用辅助口**,均不在 7 方法接口面内。
**先例**同上(`set_sharer` / `clear_sharer` 的目录安装面)。

### D-ξ:canonical 流名匹配必须**词边界**(语义修正)

| 项 | 内容 |
|---|---|
| **实测** | 裸子串匹配 `barrier(` 会命中 collective 平面的**自有**标识符 `collective_barrier(`;`completion(` 会命中 `make_completion(` ⇒ **假阳性** |
| **修正** | CP-02 改为**词边界**匹配(前一字符不得是标识符字符)⇒ collective 自有标识符不误判;真正的 canonical 流名(如 `request(`)仍被检出 |
| **回归** | 判据面 test 的「词边界对照」用例固定该行为 |

### D-ο:`ContractRule` 需真正的**「无覆盖」哨兵**(语义修正)

| 项 | 内容 |
|---|---|
| **实测** | 起初用 `ZeroCallEdge` 兼作「无覆盖」哨兵 ⇒ 「未绑定」与「绑定到该规则」**无法区分** ⇒ 抓取点 1 标「已满足」时**不报红**(漏判) |
| **修正** | 新增 `ContractRule::None` 作哨兵;真实规则编号顺延;`kContractRuleCount` 仍为 6(真实规则数) |

### D-π:**R3 drain 的「零调用边」在本仓的落地形态(语义补充)

`ADR-CEP-03 §3.4` 要求 drain 由**发起注册的 node 端**经**自身** Coherence Interface 端口发出,plane 只输出 invalidate 清单。
本仓**无** Coherence Interface 实现(队列 #6 才引入 CHI RN-F)⇒ 本 change 只能落地**「plane 侧不发起 drain」这一半**:
plane 的 `group_register_region` **不**触碰任何 coherence 侧对象(由 include-graph 闸 + 完成域判据双向保证),
drain 清单的**发出方**留待队列 #6。**本 change 不假装实现 drain**。

### D-τ:声明源里的 `group` 编号 = **分组标签**,不是**句柄预约**(语义澄清)

`collective_planes.json` 的 `registered_regions[].group` 是**声明侧给 region 打的分组标签**;
`LocalCollectivePlane::group_create` **自己分配**内部 gid ⇒ 两者**不是同一个编号空间**。
⇒ 判据 CP-04 用声明 gid 作**映射键**(同 gid 的多个 region 走同一个内部 group),
**不**检查二者字面相等 —— 声明 gid 不承诺与内部句柄一致(per critic 三审 NIT 澄清)。

### D-ρ:实施期自查发现的 **5 处真缺陷**(已修,登记备查)

| # | 缺陷 | 后果 |
|---|------|------|
| 1 | 判据编排里 `add(rule, evaluate(d), std::move(d))` —— **函数实参求值顺序未指定** ⇒ `std::move(d)` 可能先于 `evaluate` 执行 ⇒ **detail 被移空**(实测 CP-02/CP-06 的 detail 全空) | 判据红但**说不出原因** |
| 2 | `ContractRule` 哨兵冲突(见 D-ο) | 抓取点覆盖核对**漏判** |
| 3 | canonical 流名裸子串匹配(见 D-ξ) | CP-02 **假阳性** |
| 4 | 抓取点解析:① 未限定在 Status Update 段 ② 表格行 gate 未容忍 `>` blockquote 前缀 ③ 列计数错位(取到内容列而非可测性列) | 7 条抓取点**一条都解析不出**(`parsed=0`) |
| 5 | `ContractPaths` 的 `at()` 无条件拼 root ⇒ **绝对路径**输入指向不存在的路径 | 声明源「畸形」被误报成「不可读」 |

> 5 处都在**实现期**被自查与单测抓到 —— 与队列 #4 的经验一致:**判据实现退化必须靠反向 fixture + 实跑发现**,不能靠读代码。

### 实施期与 plan 的偏差汇总

| # | 偏差 | 性质 |
|---|------|------|
| 1 | `vc_binding` 权威位置(plan 指错对象) | 语义修正 |
| 2 | collective VC 预留的默认形态(空 + 反向 fixture) | 语义补充 |
| 3 | 加 2 个装配缝辅助口(`set_current_member` / `complete_drain`) | 措辞修订 |
| 4 | canonical 流名词边界 | 语义修正 |
| 5 | `ContractRule::None` 哨兵 | 语义修正 |
| 6 | R3 drain 的落地边界(只落地「plane 侧不发起」这一半) | 语义补充 |

> **7 方法接口面零变化** ⇒ A-1-1 的「逐字对齐 `ADR-CEP-03 §2 Decision 2`」仍成立;
> **新增 invariant 数 = 0** ⇒ D-ε 的 B2 计数口径不变。

### D-σ:对抗式评审(critic)复审轮 —— **3 个 BLOCKER** + 10 个 SHOULD_FIX 落地

> critic 复审结论 **REJECT**;3 个 BLOCKER 全部经**独立 PoC 实测**坐实(评审者在仓外写 PoC 直接链接
> 被评审 header 跑通)。**核心教训**:第一轮实现「24 Scenario 全绿」却仍被判 REJECT ——
> **绿的是现有断言,现有断言本身与 spec 矛盾(B-2)、或根本没测(B-1/B-3)**。
> 这正是本仓最警惕的形态:「声称有自动化、实则锚错对象」。

#### 3 个 BLOCKER(必堵,已逐条用 PoC 复验)

| # | 缺陷 | 后果 | 修复 | PoC 复验 |
|---|------|------|------|----------|
| **B-1** | `group_destroy` 只把 epoch 置 false,**不移除 region** | 残留 ⇒ 重叠检查遍历全表 ⇒ 该地址段**永久报废**;且违反 spec「destroy 后 region 表移除」 | destroy 时**从 `regions_` 移除**该 group 的 region | `region_state_of` → `no-region`;重注册 → `ok` ✅ |
| **B-2** | `send` 在 NoRegion / EpochInvalid 分支**投递 Fault 事件** | 直接违反 spec「**不得**投递任何事件」;且**测试锚定的恰是被禁止的行为** | 错误**只经状态**传达;事件通道留给真正的 partial failure | `pending_events=0` ✅ |
| **B-3** | `collective_barrier` **不校验成员身份** | 非成员到达计入阈值 ⇒ 「N-1 不释放」被绕过 ⇒ 破掉本 plane **唯一的 ordering 承诺** | 到达前校验 `current_member_ ∈ group.members`,否则 `Status::NotMember` | 非成员 + 2 真成员 ⇒ `pending=0`(不提前释放)✅ |

#### 10 个 SHOULD_FIX(全部落地)

| # | 缺陷 | 修复 |
|---|------|------|
| S-1 | `recv` 空队列语义缺失(旧事件原样返回 + status=ok) | 空 ⇒ 清空出参 + `Status::NoEvent` |
| S-2 | `complete_drain` 粒度 = per-gid 全量(部分 drain 无法表达) | 改 **per-region**:`complete_drain(gid, range)` |
| S-3 | CP-06 分类落洞(无法分类的措辞**静默绿**) | 三类措辞全不命中 ⇒ **FAIL** |
| S-4 | CP-06 的 marker **硬编码日期** `## Status Update 2026-10-26` | 改取**最后一个**含「抓取点」关键词的 Status Update 段(本仓铁律:ADR 不可变 = 追加新段) |
| S-5 | CP-01 只扫 `collective_plane.hh` **一个** header | 扫**两个**(行为面 + 判据面) |
| S-6 | CP-01 实现与自述口径不符(整行子串) + **不去注释** ⇒ 注释行误报 | **去注释** + 按 **include 路径**判定(不按整行子串) |
| S-7 | CP-02 只匹配 `name(` 形态 ⇒ `struct request` / `void barrier (` 漏检 | 改为**词边界 + 任意形态**匹配 |
| S-8 | CP-04 忽略声明源的 group 字段(每条 region 新建 group) | 按**声明的 group 编号**建 group(同 gid 多 region 走同一 group) |
| S-9 | spec 区域互斥的状态字面与实现不一致 | spec 改为「明确的『区域重叠』状态」(`RegionOverlap`)—— 实现语义更优,spec 对齐实现 |
| S-10 | CP-05 不去注释(coherence 侧注释提及 `group_id` 即假阳性) | 去注释后再查 |

#### 3 个 NIT(落地)

| # | 处理 |
|---|------|
| `GroupAttrs::collective_enabled` 死字段(声称契约未实施) | **删除** —— 「未注册 region 的 group 不可操作」由 `send` 的 NoRegion 分支**直接**保证 |
| `Group::alive` / `BarrierState::released_phases` 死状态 | **删除** |
| `next_gid_` 绕回撞 `kInvalidGroupId` 哨兵 | 绕回时**跳过哨兵** |
| 另:`Scenario 13` 行为面扫描**从未**被注入验证 | 补 poisoned fixture(行为面侧) |

#### 一处「措辞订正」(对外口径)

critic 判定:`INV-CEP-04-IC5` 的**行为主体**是 `fabric_ingress_classify`(本仓不存在),
本 change 行为化的是 R2 谓词的**判读侧** ⇒ 对外口径**不得**声称「IC5 原文主体已被行为化」。
⇒ commit message / 台账措辞统一改为「**行为化 IC5 的 R2 判读侧**」,并保留
「P3 实现真 ingress classify 时须在那里**重新**行为化」的登记。

### D-υ:三审(critic 第三次评审)—— **APPROVE_WITH_CONDITIONS**,5 SHOULD_FIX + 6 NIT 落地

critic 三审结论:**无 BLOCKER**(3 个原 BLOCKER 全部由其**独立 PoC** 复验闭合),
但仍有 5 个 SHOULD_FIX —— 全部落地:

| # | 缺陷 | 修复 |
|---|------|------|
| **U-1** | `CatchpointBinding::declared_measurable` 是**只写不读的死字段** —— 与刚删除的 `collective_enabled` **同类复发**,且就发生在修该类问题的同一个 commit 里 | 让它参与**交叉核对**:「映射表声明的覆盖承诺」必须与「ADR 的措辞分类」**严格一致**,不一致 ⇒ FAIL(把死字段变成真加固) |
| **U-2** | group 级 `epoch_valid` 旗标在 per-region drain 后**说谎**(2-region group 只 drain 1 个 ⇒ 旗标为真而另一 region 仍在 invalid 窗口) | 删除该旗标;`epoch_valid(gid)` 改为「该 group 名下**所有** region 都已 drain 回告」 |
| **U-3** | `Scenario 13` 的「反向 fixture ②」是**同义反复**(拿刚构造的 poisoned 文本找自己的字面量)⇒ 它声称堵的洞仍开着;且行为面扫描与 CP-01 **实质分叉**(漏 `directory` 条目 / 漏尖括号形态 / 漏无前缀形态) | 行为面改为与判据面**共享** `extract_include_paths` + `include_path_is_forbidden`;反向 fixture 改为**真**注入(尖括号形态 / 注释形态 / 禁用目录形态) |
| **U-4** | CP-06 的段选择仍**关键词脆弱**:未来重判段若标题不含「抓取点」三字 ⇒ 读到旧段 ⇒ 重判不可见、闸仍绿 | 段选择改为「含**抓取点登记表的表格形态**(`| **N**(` 行)」⇒ 与标题措辞**解耦** |
| **U-5** | 6 个 NIT:死字段 / 死状态 / 断言数订正 / **台账活表格未同步**「R2 判读侧」限定 / CP-04 gid 语义未澄清 / `complete_drain` 注释新旧并存 | 全部落地(台账 3 处活表格 + 2 处 Entry 表述已加「R2 判读侧」;design 补 **D-τ**;`complete_drain` 旧注释段已删) |

> **U-1 的教训值得单记**:上一轮刚因「`collective_enabled` 是声称的契约未实施」被要求删除,
> 同一轮里**又新增**了一个同类死字段(`declared_measurable`)⇒ 「删除死字段」不等于「字段都有用」,
> 新增字段时必须问「谁读它」。

#### 评审暴露的**方法论**教训(登记备查)

第一轮实现 24 Scenario 全绿、守门 7 项 rc=0、CLI 6 判据全 PASS —— 仍被 REJECT。
原因不是「测试不够多」,而是**测试锚错了对象**:
- B-2:测试断言的恰是 spec 明文禁止的行为 ⇒ **测试与 spec 互相矛盾**,而两者都「绿」;
- B-1/B-3:spec 有条款,测试**零覆盖** ⇒ 「有 24 个 Scenario」不等于「条款被覆盖」。
⇒ 后续 change 的 Scenario 清单应**逐条对账 spec 的每个 AND 子句**,而非只对账 Scenario 标题。

---

## Risks / Trade-offs

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|---|---|---|
| R-1 | **接口无消费方** —— 造了 `CollectivePlaneIf` 但全仓没人用(AI slop #2) | 中 | 高 | 强制要求 ≥3 消费点:Null 默认绑定 + Local 行为测试 + 契约判据 crosscheck;且 `test_cep_canonical_invariants.cc` 侧的行为断言**真的调用**它 |
| R-2 | **spec 抄 ADR**(AI slop #1)—— 把 7 条抓取点 / R1~R5 / A~E 抄成 Scenario,判据全是「ADR 文本含字符串 X」 | 中 | 高 | **禁写文本断言型判据**;每条判据要么**执行**代码,要么**扫描源码结构**(include-graph),要么**消费声明源**;ADR 文本断言只允许作为**补充**且必须标注「文本层」 |
| R-3 | **恒真 / 恒 SKIP 判据**(AI slop #3)—— ordering A/B/E 写出来必然 SKIP | 中 | 中 | 明确列入 Non-Goals + **禁写**;`07 号 §3.3` 抓取点 6 的「partial failure 显式建模」通过 `CollectiveEvt::Fault` 变体的存在性来覆盖 |
| R-4 | **R3 drain 被实现成 plane 直接调 Coherence Interface**(AI slop #6) | 中 | **高**(违反 C-3/C-4) | design 明写 drain 由发起注册 node 端经自身端口发出;plane 只输出 invalidate 请求清单;**include-graph 闸**作反向 fixture |
| R-5 | **偏离 `ADR-CEP-03 §5` 未显式登记** ⇒ 治理上算「静默偏离」 | 低 | 中 | D-β 的 Status Update 列入冻结面 3 成员清单;**A-FIN-4** 归档治理时复核 |
| R-6 | **BARRIER 相位语义**踩 `INV-CEP-07-IC6`(functional 模式零延迟) | 中 | 中 | Local 零延迟实现 + A-1-9 显式断言 |
| R-7 | **估时虚高** —— 8 周标称但实际体量只有 4 周 ⇒ 复审被打回 | 中 | 低 | D-θ 已拆到 1.5 人周量级;实际交付按队列 #4 先例(标称 12 → 实际 4-6 周当量)登记 |
| R-8 | **声明源成为死文件**(AI slop #5) | 低 | 中 | D-δ 要求配套 crosscheck 测试消费;`A-2-1` 验收项绑定 |
| R-9 | **零调用边闸误报** —— 头文件间接 include 触发 | 低 | 中 | 闸按**直接 include** 扫描 + 编译期依赖;反向 fixture 校准阈值 |

---

## Open Questions

| # | 问题 | 归属 | 何时裁决 |
|---|------|------|---------|
| Q-1 | `LocalCollectivePlane` 的 REDUCE / BROADCAST / MULTICAST **何时**从「未注册区域报错」升级为真数据路径? | P3 | 队列 #10 planner |
| Q-2 | `collective_planes.json` 是否需要与 `protocol_instances.json` 建立**交叉引用**(如「plane 绑定只允许指向已声明的协议实例」)? | 本 change 或 #6 | 队列 #6 planner;若不做,须在 A-2-1 显式声明「不检查」 |
| Q-3 | `CollectiveReq` 的 `payload_desc` 在类型内嵌形态下如何表达「addr+len \| inline」二选一? | 本 change | builder 阶段(设计细节,ADR 未细化) |
| Q-4 | `cap_t` 的取值域是位图还是枚举?`query_capability(op, max_bytes)` 的 `max_bytes == 0` 语义? | 本 change | builder 阶段(A-1-10 要求写死) |
| Q-5 | D-ζ 的 `group_destroy` 语义(发 `Fault` 不阻塞)是否需要 `ADR-CEP-03` Status Update 同步? | 本 change | builder 阶段(与 D-β 合并进同一次授权) |

---

## 与队列 #3 / #4 的先例对照

| 维度 | 队列 #3(stub-cache-core) | 队列 #4(directory-ordering-plane) | **本 change** |
|---|---|---|---|
| 范围切分 | 单文件头可实现 | 4 header + 1 CLI + 3 test | 2 header + 2 声明源 + 3 test |
| 判据形态 | 折叠映射表静态扫描 | J1/J2/J3 三判据 + 装配闸双挂 | **5 条契约判据 + include-graph 闸** |
| 行为实现 | 1 个(Stub Cache) | 1 个(FullMapDirectory) | **2 个**(Null + Local) |
| B2 贡献 | 行为化 `INV-SC1~6` | 行为化 `INV-CEP-04-ICx` + `INV-CEP-08-ICx` | **行为化 2 条 + 契约层加固 2 组** |
| 新增 INV 编号 | 0 | 0 | **0** |
| 声明源 | 无 | `protocol_instances.json`(诞生即不冻结) | `collective_planes.json` + schema(诞生即不冻结) |
| 偏离已拍板项 | 无 | 5 项(`D-ι`,逐条登记) | **2 项**(D-β Bundle 路径 / D-γ 注册形态)⇒ Status Update |
| 归档治理 | — | 6 步(Entry #58 首次) | 6 步(**本 design 预登记**,D-η) |
| 估时 | 8 周标称 → 显著更小 | 12 周标称 → 4-6 周当量 | 8 周标称 → 预期 4-6 周当量 |
