# Tasks — cpptlm-cep-collective-plane-base

> **阶段**: builder(单 Phase)
> **执行顺序**: T0 → T1 → T2 → T3 → T4 → T5 → T6(每步独立可验证)
> **红绿分离**: 「判据实现退化」= 用例红;「判定结论为红」= 用例**仍绿**且结论作为交付物登记
> **本 change 的 B2 贡献**(per design D-ε):新增 invariant 数 = **0**;行为化 `INV-CEP-07-IC3` + `INV-CEP-04-IC5` + 契约层加固 2 组。
>   **若本表 T4 / T5 的行为化落不了地 ⇒ 本 change 自我否决**(design D-ε 条款)。

## T0 — 实施期的权威校准(前置)

- [x] **T0-1** ✅ 已校准(实施期):`ADR-CEP-03 §2 Decision 2` 的 7 方法签名逐字落地:`group_create` / `group_destroy` /
      `group_register_region` / `send` / `recv` / `collective_barrier` / `query_capability`
      ⇒ **恰好 7 个**,不多不少
- [x] **T0-2** 原语枚举**恰好 4 值** = `REDUCE` / `BROADCAST` / `MULTICAST` / `BARRIER`;
      AllReduce **不得**出现为原语(per `ADR-CEP-03 §2`:AllReduce = REDUCE + BROADCAST 复合)
- [x] **T0-3** `CollectiveEvt` **恰好 3 变体** = `Result(gid, tag, payload)` | `Completion(gid, tag)` |
      **`Fault(gid, culprit, cause)`** —— Fault 变体是 partial failure 的显式建模载体(抓取点 6)
- [x] **T0-4** **`vc_binding` 的真实位置 = `include/tlm/cep/protocol_instances.json` 的
      `instances[].vc_binding`**(**不是** `fabric.json` —— 后者只有 `networks[].vc_count`)。
      ⇒ **实施期偏差**:design / proposal 原文写的「与 `fabric.json` 既有 `vc_binding` 不冲突」**指错了对象**,
      按权威回填为「与 `protocol_instances.json` 既有 `vc_binding` 不冲突」(per Entry #66 惯例)
- [x] **T0-5** **VC 预留的实测现状**:`moesi_gpu` 与 `mesif` 在 `compute_noc` / `ualink_fabric` 上
      **VC 0-3 全部被 coherence 占用** ⇒ 本仓当前拓扑**没有** collective 可用的隔离 VC。
      ⇒ 声明源的 `collective_vc_reservation` 默认为**空**(本 change 不预留);「不冲突」判据以
      **反向 fixture**(注入冲突预留 ⇒ 判红)承载判别力,而非让默认状态恒红(per「红绿分离」)
- [x] **T0-6** canonical 10 流的**权威源与解析形态**:`docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md`
      §2 的表格行 `| N | \`flow_name\` | … |` ⇒ 判据解析该表取流名集合
- [x] **T0-7** 抓取点可测性登记的**权威源**:`docs/adr/ADR-CEP-01-fabric-collective-plane.md` 的
      「Status Update 2026-10-26 —— §3.3 抓取点的**可测性裁决**」段的表格(per design D-ι 裁决 1 引入的 S4 修)

## T1 — `include/tlm/cep/collective_plane.hh`(capability 1:平面本体)

- [x] **T1-1** 类型面:`group_id_t` / `member_set_t` / `group_attrs_t` / `addr_range_t` / `access_mode_t` /
      `op_t` / `phase_t` / `tag_t` / `reduce_op_t` / `dtype_t` / `multicast_mask_t` / `cap_t` / `status_t`
- [x] **T1-2** `CollectiveReq` 字段:`group_id` / `op` / `tag` / `src_member` / `payload_desc` /
      `reduce_op` / `dtype` / `multicast_mask`(per `ADR-CEP-03 §2` 的「plane 的通用词汇」)
- [x] **T1-3** `CollectiveEvt` 3 变体 + `CollectiveEvt::Fault`
- [x] **T1-4** `status_t` 值域(**「不支持」的返回通道**,per design D-ι 裁决 3):
      `kOk` / `kUnsupported` / `kDataPathNotImplemented` / `kGroupRegisterError` / `kInvalidGroup` /
      `kInvalidRange` / `kRegionOverlap` / `kDrainFailed` / `kUnknownOp` / `kEmptyMemberSet`
- [x] **T1-5** `CollectivePlaneIf` **恰好 7 个虚方法**(逐字对齐 T0-1)+ 虚析构
- [x] **T1-6** `NullCollectivePlane`:全原语 → `kUnsupported` + **无副作用**(不建 group / 不投递事件);
      `group_create` 返 `kInvalidGroupId` 哨兵;`query_capability` 返「不支持」cap
- [x] **T1-7** `LocalCollectivePlane`:group 生命周期(create / destroy)+ **region 表** + **epoch 两阶段**
      (register 后 epoch invalid,`complete_drain` 回告后生效 —— per design D-ι 裁决 2)
- [x] **T1-8** `LocalCollectivePlane` 的**归属判定**三态:`NoRegion` / `RegisteredEpochInvalid` / `RegisteredActive`
      (R2 **判读侧**,per design D-ι 裁决 1)—— 只读观测口
- [x] **T1-9** **区域互斥**:重叠注册 ⇒ `kGroupRegisterError`;drain 失败 ⇒ **不建 epoch**(CEP-13)
- [x] **T1-10** **BARRIER rendezvous**:N-1 到达不释放 / N 到达释放;同相位互不排斥;跨相位有序
- [x] **T1-11** **已注册 + epoch 生效**区域上的 `send(REDUCE/BROADCAST/MULTICAST)` ⇒ `kDataPathNotImplemented`
      且**不投递事件**(per design D-ι 裁决 4)
- [x] **T1-12** **未注册**区域上的 collective 操作 ⇒ `kGroupRegisterError`,**立即报错不静默**
- [x] **T1-13** `group_destroy` 对 in-flight 操作发 **`Fault`** 且**不阻塞**(design D-ζ)
- [x] **T1-14** `query_capability` 边界:未知 `op` / `max_bytes == 0` ⇒ 明确返回,**不抛异常**
- [x] **T1-15** **零调用边**:`collective_plane.hh` **不得** include directory / ordering / sharer 侧头
      (去注释扫描 + 反向 fixture)

## T2 — `include/tlm/cep/collective_planes.json` + schema(声明源)

- [x] **T2-1** 声明源 3 组字段:`plane_binding` / `registered_regions` / `collective_vc_reservation`
- [x] **T2-2** 配套 schema(`additionalProperties: false` + 必填校验),**诞生即不冻结**
- [x] **T2-3** 默认值:`collective_vc_reservation = []`(per T0-5 —— 本仓当前拓扑无 collective 可用 VC)
- [x] **T2-4** 声明源被**至少一个**测试消费(⇒ 不产生死文件)

## T3 — `include/tlm/cep/collective_plane_contract.hh`(capability 2:契约判据)

- [x] **T3-1** 三态显式化(SKIP ≠ PASS ≠ FAIL)+ **verdict 逐条可 grep**
      (格式 `[verdict] <rule-id> PASS|FAIL|SKIP (detail)`,per `ADR-CEP-09 §8.3` 同源)
- [x] **T3-2** **零调用边**闸:扫 `collective_plane.hh` 的直接 include + 编译期依赖
      —— 与 T1-15 **正交**(一个查 include 结构,一个查类型可达),**不得**同一份实现
- [x] **T3-3** **canonical 10 流零 diff**:解析 `ADR-CEP-02 §2` 表格取流名集合(per T0-6);
      断言 collective 侧不引入 canonical 流名 + 流清单条数 == 10
- [x] **T3-4** **collective VC 预留**判据:值域 ∈ 0-3 + `< 该网络 vc_count` + 与
      `protocol_instances.json` 既有 `vc_binding` **不冲突**(per T0-4 回填)
- [x] **T3-5** **区域互斥**判据与行为面**同源**(共享 `LocalCollectivePlane` 的判据实现,**禁双实现**)
- [x] **T3-6** **声明源缺失 / 畸形 ⇒ 判 FAIL**(配置错误)—— 与「无可检查对象 ⇒ SKIP」严格区分
      (per 队列 #4 `Entry #65` 的 `source_error` 语义)
- [x] **T3-7** **抓取点可测性登记**判据:解析 `ADR-CEP-01` 的 Status Update 表格(per T0-7),
      逐条取出 7 个抓取点的覆盖状态;**交叉核对**「声称可测的每条在判据清单里都有对应条目且实跑通过」
      + 「声称推迟的不在判据清单里」—— 禁「文档里有表就通过」与恒真写法
- [x] **T3-8** **完成域判别**:`group_id` 等 group-scoped 字段**不得**出现在 Coherence Interface 侧

## T4 — `test/test_cep_collective_plane_base.cc`(行为面,13 Scenario)

- [x] **T4-1** 接口面:7 方法逐字 + 原语恰好 4 值 + AllReduce 不出现
- [x] **T4-2** 类型面:`CollectiveEvt` 含 Fault 变体(缺 ⇒ 红)
- [x] **T4-3** `NullCollectivePlane` 全原语不支持 + **无副作用** —— **反向 fixture**(注入「假装支持」⇒ 红)
- [x] **T4-4** `LocalCollectivePlane` group 生命周期 + `group_destroy` 对 in-flight 发 `Fault` 且不阻塞
- [x] **T4-5** BARRIER rendezvous:N-1 不释放 / N 释放 —— **反向 fixture**(阈值改 N-1 ⇒ 红)
- [x] **T4-6** **归属判定三态**(R2 判读侧):`RegisteredEpochInvalid` ⇒ **不 divert** —— **反向 fixture**
- [x] **T4-7** 区域互斥:重叠 ⇒ `kGroupRegisterError`;drain 失败 ⇒ 不建 epoch —— 正控:合法注册成功
- [x] **T4-8** 未注册区域 ⇒ 立即报错 —— **反向 fixture**(静默 ⇒ 红)
- [x] **T4-9** 已注册 + epoch 生效区域上的数据原语 ⇒ `kDataPathNotImplemented` 且不投递事件 —— **反向 fixture**
- [x] **T4-10** functional 模式:collective 不产生 latency
- [x] **T4-11** `query_capability` 边界:未知 `op` / `max_bytes == 0`
- [x] **T4-12** `group_id` 不出现在 Coherence Interface 侧
- [x] **T4-13** **零调用边**(include 扫描)+ **反向 fixture**

## T5 — `test/test_cep_collective_plane_contract.cc`(契约判据面,8 Scenario)

- [x] **T5-1** 声明源形态与 schema 一致 + **被消费**(不产生死文件)
- [x] **T5-2** 判据三态可区分 + verdict 格式可 grep
- [x] **T5-3** 零调用边闸可执行 —— **反向 fixture**(注入 include ⇒ 红)
- [x] **T5-4** canonical 10 流零 diff —— **反向 fixture**(改流清单 ⇒ 红)
- [x] **T5-5** collective VC 预留判据 —— **反向 fixture**(注入冲突 VC ⇒ 红)
- [x] **T5-6** 区域互斥判据与行为面**同源**(结论一致)
- [x] **T5-7** 声明源缺失 / 畸形 ⇒ **FAIL**(不是 SKIP)
- [x] **T5-8** 抓取点可测性登记:7 条逐条核对 + **B2 贡献清单非空**

## T6 — 收尾与验收

- [x] **T6-1** `openspec validate cpptlm-cep-collective-plane-base --strict` rc=0
- [x] **T6-2** 守门 7 项 rc=0;`cep_gate_selftest` 全绿;`openspec list --json` 实测 **1/3**
- [x] **T6-3** `ctest` 全绿;`[cep]` 的 case / 断言数**逐项登记**
- [x] **T6-4** `design.md` **实施期裁决**段落盘(T0 的 5 项偏差 + 实施期发现)
- [x] **T6-5** **B2 贡献清单核对**:逐条指认「被行为化的既有编号」或「被加固的契约组」
- [x] **T6-6** 反向验证自查:每条 fixture 变异必红 + GREEN 样本必绿
- [ ] **T6-7** **归档治理 6 步**(per design D-η / `Entry #64` 模板)—— **归档阶段**执行
