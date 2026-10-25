# Tasks — cpptlm-cep-directory-ordering-plane

> **阶段**: builder(单 Phase;与队列 #3 的「双 Phase」不同 —— 本 change 无「声誉先于实现」的时效性问题)
> **执行顺序**: T1 → T2 → T3 → T4 → T5 → T6 → T7 → T8 → T9 → T10(每步独立可验证)
> **红绿分离**: 「判据实现退化」= 用例红;「判定结论为红」= 用例**仍绿**且结论作为交付物登记

## T0 — 实施期的权威校准(前置,已完成的调研结论)

- [x] **T0-1** 复核 `protocol_instance.schema.json`(冻结,34 成员之一)的 13 个 required 顶层属性 ⇒
      `sharer_tracker.caps` 的 **4 属性 = {scope, completeness, authority, aggregation_at}**(per `ADR-CEP-04 §2 决策 3`),
      **不是** delta spec A-1-2 原写的 `{completeness, aggregation_at, hazard, broadcast_fallback}`
- [x] **T0-2** `completeness` 的 **schema enum 只含 `EXACT` / `CONSERVATIVE`**(无 `PARTIAL`)⇒ J2 的 PARTIAL 分支
      **不可由合法声明触达**,只能用**合成声明**驱动负向用例(该事实须在被测报告里显式登记)
- [x] **T0-3** `ordering_assumption` 的**取值域** = `{p2p_ordered, unordered}`(schema enum)⇒ 不是 `strong`
- [x] **T0-4** 通道依赖**边表**由 ArchForge `32 号 §6.0.3` 给出(`compute_noc` 5 条 + `ualink_fabric` 3 条 = 8 条),
      **不是**从 `vc_binding` 单靠 C++ 规则可推导的 ⇒ 边表必须作为**声明数据**随 `protocol_instances.json` 落地
      (禁止在 `.hh` 里硬编码)
- [x] **T0-5** ArchForge `32 号 §6.1` 的 `moesi_gpu` 完整 13 字段声明已取到(用于派生副本 + J1/J2/IC2 实跑)

## T1 — `include/tlm/cep/sharer_tracker.hh`(capability 1)

- [x] **T1-1** `node_id_t` / `responder_set_t`(64-bit,`static_assert`) / `snoop_handle_t`(16-bit) 类型面
- [x] **T1-2** `kResponderBitToNode` **bit→node_id 显式映射表**(bit 0 = `kNodeIdSdma`) + 双向 helper
- [x] **T1-3** 枚举族:`LookupResult`(4) / `LookupHazard`(3) / `Intent`(4 规范意图) / `SnoopState`(4)
- [x] **T1-4** `SharerTrackerCaps`{scope, completeness, authority, aggregation_at}(ADR 决策 3 逐字)
      + `SharerTrackerBehavior`{hazard_under_contention, broadcast_fallback}(ADR §4 表 + 决策 2)
- [x] **T1-5** `kTrackerTraits[5]` —— 5 实现 × (4 caps + 2 behavior) 数据级表(DcohSymmetric 的 `aggregation_at` 是**二元**)
- [x] **T1-6** `LifecycleEvent` 5 事件名(`IC3` 逐字:`allocate-on-first-forward` / `merge-on-snoop-begin` /
      `decrement-on-snoop-fold` / `retire-on-zero-sharers` / `retire-on-zero-pending`)
- [x] **T1-7** `SharerTrackerIf` **仅 4 个虚方法**(lookup / snoop_begin / snoop_fold / observe_snoop)+ 虚析构
- [x] **T1-8** `FullMapDirectory` —— 唯一行为实现:`lookup` 精确 sharer 集 / `FORWARD_TO`(dirty 且 want_read)/
      hazard STALL / snoop begin-fold-retire / `observe_snoop` 幂等(**IC2** 同参数同返回)
- [x] **T1-9** 禁 API 零出现(去注释后):`snoop_complete` / `subscribe_remote_snoop` / `is_distributed` / `sharer_bitmap`

## T2 — `include/tlm/cep/address_resolver.hh`(capability 2 的地址子平面)

- [x] **T2-1** `AddressResolverIf` **仅 1 个虚方法** `resolve(pa) const → node_id_t`
- [x] **T2-2** `WindowAddressResolver` 最小实现(窗口内 → home,窗口外 → `kInvalidNode`)

## T3 — `include/tlm/cep/channel_dependency_graph.hh`(capability 3)

- [x] **T3-1** `Vertex{instance, network, vc}` + `vertex_key` / `vertex_text`(三元组,`vc ∈ 0..3`)
- [x] **T3-2** `ChannelDependencyGraph`(顶点/边/统计/DFS 三色环检测 —— 算法迁 `cep_poc_spike_s3.cc:125-151`)
- [x] **T3-3** 声明模型 `DeclaredInstance` / `ChannelDeclaration` + `parse_channel_declaration(json)`
      (`vc_binding` / `ordering_assumption` / `sharer_tracker` / `perm` / `combination_capabilities` 投影)
- [x] **T3-4** `evaluate_channel_dependency_gate(decl)` —— 顶点由 `vc_binding` 导出、边由**声明边表**导出、
      端点未绑定 ⇒ FAIL、有环 ⇒ FAIL、verdict **逐 `(network, vc)`**
- [x] **T3-5** `GateReport::to_log()` —— `[SKIP]` / `[PASS]` / `[FAIL]` 三态 + `[verdict] net=… vc=… PASS|FAIL …` 行
- [x] **T3-6** `run_channel_gate(config)` —— 从 `config["cep"]` 取声明;**无 `cep` 段 ⇒ SKIP**(module_factory 挂点用)

## T4 — `include/tlm/cep/ordering_point_checker.hh`(capability 2 的判据子平面)

- [x] **T4-1** `evaluate_ordering_points(decl)` —— J1 静态路径覆盖(响应向 VC 必须 `p2p_ordered`)
- [x] **T4-2** J2 与 caps 耦合:`PARTIAL && !broadcast_fallback ⇒ FAIL`;合法声明恒 PASS 且报告显式登记该 vacuous 事实
- [x] **T4-3** **IC2**(`INV-CEP-08-IC2` E 态静默升级):`perm` 含 `UNIQUE` ⇒ 需 `combination_capabilities.hierarchical`;
      检查项 `ic2_item_present` 恒真,判据总数恒 4(删除任何判据 ⇒ 用例红)
- [x] **T4-4** J3 **降级登记**:0 OP ⇒ SKIP / 1 OP ⇒ **vacuous PASS** / ≥2 OP ⇒ SKIP(通用形式不可静态判定,队列 #6 承接)
- [x] **T4-5** `kS1_7BetaVerdict` —— S1-7 (β) 裁决结论作为**交付物**登记(`needs_fifth_perm_bit = false`)

## T5 — `include/tlm/cep/protocol_instances.json`(装配闸输入源)

- [x] **T5-1** 替换 planner 期的薄骨架为 **完整 13 字段** `moesi_gpu` 声明(ArchForge `32 号 §6.1` 的**逐字派生副本**)
- [x] **T5-2** 顶层 `channel_dependency_edges`(8 条,派生自 `32 号 §6.0.3`)+ `derived_from` / `mirror_note`
- [x] **T5-3** 该文件**诞生即不冻结**(per Entry #56 先例);不改冻结的 `fabric.*` / `protocol_instance.schema.json`

## T6 — `test/test_cep_sharer_tracker_base.cc`(9 Scenario / 9 锚点)

- [x] **T6-1** 接口 4 方法 decltype + 虚方法计数 == 5(4 + 析构)
- [x] **T6-2** caps 结构 + `sizeof(responder_set_t) == 8` + bit↔node 双射
- [x] **T6-3** 5×6 表数据级自洽(含 DcohSymmetric `aggregation_at` 二元)
- [x] **T6-4** FullMapDirectory lookup 语义(ANSWER_HERE / 精确集 / EXACT / broadcast_fallback=false)
- [x] **T6-5** begin→fold→retire 生命周期(mask 单调递减,≥2 组序列;`remaining==0 ⇔ RetireOnZeroPending`)
- [x] **T6-6** 同地址并发 hazard = STALL 且 view 无效(与首次 lookup 的 NONE **对比** —— 输入改变 ⇒ 输出改变)
- [x] **T6-7** IC2 幂等(同参同返)+ IC3 5 事件名与顺序
- [x] **T6-8** 禁 API 去注释扫描 + **正控**(同机制必须能命中真实存在的标识符)
- [x] **T6-9** 其余 4 实现 **deferred 登记**(只有声明层断言;断言 header 里不存在它们的类定义)

## T7 — `test/test_cep_ordering_point_checker.cc`(6 Scenario / 6 锚点)

- [x] **T7-1** `AddressResolverIf` 单方法 decltype + 虚方法计数 == 2
- [x] **T7-2** J1:仓库声明 PASS;合成「响应向 VC 未声明」⇒ FAIL(正反双向)
- [x] **T7-3** J2:合成 PARTIAL 无 fallback ⇒ FAIL;PARTIAL + fallback ⇒ PASS;合法声明 ⇒ PASS(含 vacuous 登记)
- [x] **T7-4** IC2:`moesi_gpu` 实跑 PASS;合成 `hierarchical=false` ⇒ FAIL;判据总数 == 4
- [x] **T7-5** S1-7 (β):裁决交付物 + `registered_in` 文件真实存在且含该裁决文字
- [x] **T7-6** J3 vacuous:1 OP ⇒ PASS(含 `vacuous` 字样)/ 0 OP ⇒ SKIP / 2 OP ⇒ SKIP

## T8 — `test/test_cep_channel_dependency_gate.cc`(7 Scenario / 7 锚点)

- [x] **T8-1** 顶点三元组(结构断言 + vc 值域)
- [x] **T8-2** 边由声明导出:**改声明 ⇒ 统计改变**;并断言 `.hh` 里无网络名字面量边表(正控)
- [x] **T8-3** 仓库声明 ⇒ `(8, 8, 0)` + 与 S3 spike **同轴对账**(跑 `cep_poc_spike_s3` 解析计数)
- [x] **T8-4** 注入环 ⇒ **真 `ModuleFactory::instantiateAll` 返回 false** + verdict 逐 `(network, vc)`
- [x] **T8-5** verdict 行格式 `[verdict] net=<n> vc=<id> PASS|FAIL …`
- [x] **T8-6** 空/缺失 ⇒ `[SKIP]`(与 `[PASS]` 可区分)
- [x] **T8-7** 双挂共享判据:module_factory 挂点与 CLI 挂点对同一输入产出**同一报告**

## T9 — 双挂装配

- [x] **T9-1** `src/core/module_factory.cc::instantiateAll()` —— `validateConfig` 之后 / 实例创建之前挂钩(仅 `cep` 段存在时触发)
- [x] **T9-2** `examples/cep_channel_dependency_gate.cc` CLI(读声明 → 打印两段报告 → exit 0/1)
- [x] **T9-3** `examples/CMakeLists.txt` 注册 + ctest;`scripts/CMakeLists.txt::validate_topology` 追加该 CLI

## T10 — 收尾

- [x] **T10-1** `scripts/test/cep_decl_alignment_check.sh` 追加第 ⑥ 项:本仓 `protocol_instances.json` ↔ ArchForge §6.1 **镜像逐字段相等**(A-3-2 的「镜像不符 ⇒ 判红」)
- [x] **T10-2** delta spec 回填 T0-1 / T0-4 的权威校准(A-1-2 caps 4 属性;A-3-2 边来源)+ design.md 记 D-ι
- [x] **T10-3** `openspec validate --strict` rc=0;守门 7 项 rc=0;`cep_gate_selftest` 全绿;`ctest` 全绿

## T11 — 实施期偏差与遗留(如实登记)

### 偏差(已在 `design.md §实施期裁决 D-ι` 逐条登记)

| # | 偏差 | 依据 | 处置 |
|---|------|------|------|
| **1** | caps descriptor 的 4 属性是 `{scope, completeness, authority, aggregation_at}`,**不是** planner 写的 `{completeness, aggregation_at, hazard, broadcast_fallback}` | `ADR-CEP-04 §2 决策 3` + 冻结 schema `sharer_tracker.caps.required` | `hazard` / `broadcast_fallback` 落 `SharerTrackerBehavior`;delta spec A-1-2 已回填 |
| **2** | `completeness == PARTIAL` 不可由合法声明表达(schema enum 只有 EXACT/CONSERVATIVE) | 冻结 schema + 决策 2 | J2 的 PARTIAL 分支由**合成声明**驱动,报告逐实例标 `vacuous PASS` |
| **3** | 通道依赖**边表**不可从 `vc_binding` 推导 | ArchForge `32 号 §6.0.3` | 边表作为声明数据落 `protocol_instances.json::channel_dependency_edges`;delta spec A-3-2 已回填 |
| **4** | 新增第 4 个 header `ordering_point_checker.hh`(planner 列 3 个) | `ADR-CEP-08 §1.3` 三子平面正交分工 | 依赖方向单向(`checker → graph`),无环 |
| **5** | 镜像必须含 **2 个**实例(`moesi_gpu` + `mesif`),planner 只写了 1 个 | ArchForge `32 号 §6.1/§6.2`(声明 2 份) | 补齐后逐字段比对通过;`cep_decl_alignment_check.sh` 新增第 ⑥ 项守漂移 |

### 遗留发现(不在本 change 范围)

- **`validate_topology` target 的首条命令引用了不存在的文件** `build/configs/dgpu_board_v1_mvp.json`
  (`scripts/CMakeLists.txt:48`;该 config 在源仓也不存在)。⇒ 该 target 在本 change **之前**就已无法跑完,
  本 change 追加的 CLI 命令因此跑不到。**验证替代**:① CLI 单独运行 PASS;
  ② `ninja -C build -t commands validate_topology` 确认本 CLI 已被接线(3 处引用)。
  修复该 target 属另一子系统(拓扑校验/配置分发),需单独决策。
