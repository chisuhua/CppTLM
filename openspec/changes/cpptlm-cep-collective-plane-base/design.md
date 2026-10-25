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
| R1 / R2 的真实 fabric ingress 分类 | 同上 | **P3** |
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
| 2 | **行为化** | `INV-CEP-04-IC5`:epoch invalid 窗口 = 正常服务放行 | "Sharer Tracker" |
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
| 4 | `cep_doc_ref_check` VIRTUAL_PATHS 双态注册 | 登记 `openspec/changes/cpptlm-cep-collective-plane-base`(归档前路径保留历史指向);**并移除** planner 阶段预登记的 2 条「设计示意」条目(`include/tlm/cep/collective_plane` 前缀式 / `include/bundles/collective_bundles_tlm.hh` —— 后者 P3 才建,故**保留**)|
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

---

## 实施期裁决(builder 阶段,待补)

> **占位**:builder 阶段对**权威面**的实测校准 + 与本 design 的偏差,逐条记入本节(per 队列 #4 的 `D-ι` 先例)。**每条须给**「权威来源 + 实测证据 + 偏差性质(授权表述 / 措辞修订 / 语义修正)」。

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
