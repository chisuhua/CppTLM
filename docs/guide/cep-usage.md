# CEP 使用手册(Stage 4+ 维护态交接)

> **本文件原为** [`docs/architecture/07-coherence-exploration-platform.md`](../architecture/07-coherence-exploration-platform.md) 的 **§12**(per 台账 Entry #333 架构文档治理 + 用户裁决 B:架构文档只放蓝图/方向/终态方案,实施状态与操作步骤迁出)。
> **子节编号保留 `12.x`** ⇒ 手册内 10 处 `§12.x` 自引用**零改写**;⛔ **不要**把子节重编号为 `1.x`。
> **定位**:Stage 4+ 维护态下的**操作手册**(只导航 + 操作步骤,⛔ 不复述 `07 号` 的 Status Update 内容、不产生状态判定)。
> **状态判定真源**:`objective-cep-platform §4.0`(队列位状态)+ `§4.0-a`(实例级状态)+ `§11 决策台账`(残余缺口)。

> **交付物定位**:本节是 `objective-cep-platform §8.3` ④「交接文档完成(CEP 使用手册 → `07 号` 扩充)」的**唯一交付物** —— Stage 4+ 维护态下的**操作手册**。
>
> **范围纪律(per 台账 Entry #332 ⑫ SF-5,逐字)**:本节**只做导航 + 操作步骤**,⛔ **不复述**`07 号` 其余 **N** 则 Status Update(以 `07 号` §末 索引 B 为准) 的内容(否则违反单源纪律 + 真源原则,且会无谓膨胀 M1 冻结件)。
>
> **本节不产生状态判定**:状态权威在 **`obj §4.0` / `obj §4.0-a`(原 `07 号` §11 已迁出)**(落地)/ `docs/adr/ADR-CEP-*`(决策)/ `objective-cep-platform §6.2`(DoD)/ **`07 号` §7**(实验契约);本节只给**读法与导航**。**本节只回答三件事:「去哪找 / 怎么操作 / 什么不能写」。**
>
> ⚠️ **本节不含台账 Entry #332 ⑪ 要求的「新 SU」**(口径 ④ 本体 —— ⑤ 裁决句 + MUST-FIX-2 三处重裁 + 终局三分支 —— **尚未落进 `07 号`**)⇒ **§12.7 的禁令按现行绝对禁令执行**,且 §12.7 与 §12.8 之间存在**待重裁的张力**(已在该两节逐条标注)。

## 12.1 10 分钟上手

| 步 | 动作 | 命令 / 入口 | 通过判据 |
|----|------|-------------|---------|
| 1 | 构建(OFF 路径) | `./build.sh` | 产出 `build/bin/cpptlm_tests` |
| 2 | 跑 CEP 测试 | `./build/bin/cpptlm_tests "[cep]"` | 全绿(用例数随版本增长 ⇒ **以当前输出为准**;2026-11-10 实测 **508 cases / 11516 assertions**,取数命令同上) |
| 3 | 跑 CEP 守门(**只读巡检**) | `for f in scripts/test/cep_*.sh; do case "$f" in *experiment_run.sh\|*h8_ab_runs.sh) continue;; esac; bash "$f"; done` | 逐个看 `rc`;⚠️ **三条例外**(cadence 闸会自然超期 / 跨仓缺失时跳过 / 部分闸会真跑实验并写盘)见 **§12.5** ⇒ ⛔ **`rc=0` 不等于「验证过」** |
| 4 | 看最小装配拓扑 | `configs/cep_chi_rnf_pe_minimal.json` | `cep.bridges[].protocol_instance` 指向**已声明**实例(`chi_rnf`) |
| 5 | 理解装配路径 | `src/core/module_factory.cc` | `instantiateAll` → **Step 0.25** 装配期判据(`run_channel_gate` / `run_ordering_gate`;FAIL ⇒ **模块创建前**即 `return false`)→ Step 7 注入 adapter → **Step 7.5** 桥成员互斥 `[BSS-01]`(命中 ⇒ **零桥挂载**)→ 桥挂载循环 |

**三条入口指针**(读不懂时按此顺序):

1. **这是什么** → `07 号` **§2**(6 层)/ **§3**(canonical 10 流)/ **§5.1.1**(协议族 → 实例 → adapter)
2. **为什么这么设计** → [`ADR-CEP-00`](../adr/ADR-CEP-00-platform-charter-and-governance.md)(治理宪章)+ `ADR-CEP-01` ~ `ADR-CEP-19`(各域决策)
3. **做到哪一步了** → **`obj §4.0` / `obj §4.0-a`(原 `07 号` §11 已迁出)**(活落地状态)+ `objective-cep-platform §4.0`(排队总表)

> ⚠️ **9 份 `configs/cep_*.json` = 7 份最小装配(`*_pe_minimal.json`)+ 2 份 E2E 双拓扑(`cep_chi_rnf_e2e_{a,b}.json`)**,⛔ **不含**通用 NoC router/link 形态(实测模块类型只有 `TrafficGenTLM` / `StubCacheEndpointTLM` / `CohIfResponderTLM` / `*ProtocolEngine`)⇒ **第 6 层(传输)在 CEP 侧无运行时实现**,详见 **§12.2**。

## 12.2 载体地图

> **本表回答「去哪找」**;⛔ **状态判定不在本表**(状态真源 = **`obj §4.0` / `obj §4.0-a`(原 `07 号` §11 已迁出)**)⇒ 表内括注仅作**读法提示**,**不是状态源**。逐文件职责见各文件头注释。

| 层(`07 号` §2) | 代码载体 | 声明源 / 配置 | 判据载体 |
|---------|---------|--------------|---------|
| **1 Stub Cache** | `include/tlm/cep/stub_cache_core.hh`(纯类,5 态 + 4 值 `Perm`)+ `stub_cache_endpoint.hh`(端口面 wrapper) | — | `test/test_cep_stub_cache_core.cc` · `test_cep_stub_cache_e2e.cc` |
| **2 Coherence Interface** | `include/bundles/cep_coherence_bundles_tlm.hh`(`CohIfBundle`:**只覆盖 10 流中的 5 条 RN 侧流**) | `include/tlm/cep/fabric.json`(2 网络 × 4 VC) | `test/test_cep_canonical_flow_contract.cc`(13 条载体) |
| **3 Protocol Engine** | `include/tlm/cep/protocol_engine_base.hh`(`ProtocolEngineBase<N>`,**ChStream 端口面**) + 6 个 `*_protocol_engine.hh` | `include/tlm/cep/protocol_instances.json`(**实例声明源**;⚠️ 实例数以该文件为准 —— `obj §4.0` / `obj §4.0-a`(原 `07 号` §11 已迁出) 的计数行可能滞后) | `test/test_cep_engine_and_numbering_contract.cc` |
| **4 Directory & Ordering** | `sharer_tracker.hh` / `address_resolver.hh` / `ordering_point_checker.hh` / `cxl_dcoh_tracker.hh` / `tilelink_tree_manager.hh` / `broadcast_null_tracker.hh` | `protocol_instances.json` 的 `sharer_tracker` + `sharer_tracker_rationales` | `test/test_cep_sharer_tracker_base.cc` · `test_cep_ordering_point_checker.cc` |
| **5 Fabric Collective Plane** | `collective_plane.hh`(`CollectivePlaneIf` 7 方法 + `Null` / `Local`)+ `fabric_collective_plane.hh`(**唯一数据路径真实现**) | `include/tlm/cep/collective_planes.json`(4 binding) | `test/test_cep_collective_plane_base.cc` · `_contract.cc` |
| **6 Fabric & Transport** | ⛔ **CEP 侧无运行时传输实现**(无 router / credit 模型);⚠️ **VC 分配是声明式**(`vc_binding` + `fabric.json` 的 `vc_count`)⇒ **无运行时 VC 分配器** | `fabric.json` + `collective_planes.json` 的 `collective_vc_reservation`(当前 **空**) | `channel_dependency_graph.hh` 的装配期无环检查(死锁自由性) |
| **跨层 · 装配** | `include/tlm/cep/instance_assembly_gate.hh`(`[IAG-01]`~`[IAG-07]`)+ `bridge_module_membership.hh`(唯一实现,禁双实现) | JSON 的 `cep.bridges` | `test/test_cep_instance_assembly.cc` · `test_cep_bridge_reattach_guard.cc` |
| **跨层 · 桥** | `include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` / `ProtocolBridge<InB,OutB,Ordered>`)+ `include/tlm/cep/cohif_noc_bridge.hh` | JSON 的 `cep.bridges[]` | `test/test_cep_protocol_bridge_contracts.cc` |
| **跨层 · 规约** | 6 个 `*_state_spec.hh`(表驱动 `transition`;**无 `chi_rnf_state_spec`** —— CHI 状态内联在 adapter) | — | `test/test_cep_*_state_spec.cc`(6 份) |

**8 adapter / 6 engine 的分工**:`adapter` = 状态**规约**的消费者(转移表 + `perm` 投影);`engine` = 运行时插件(`ChStreamModuleBase` 派生)。⚠️ **两者强度不等**,且**强度分级随实现演进** ⇒ ⛔ **本节不给分级清单**(会腐化)⇒ **分级读法见 `obj §4.0` / `obj §4.0-a`(原 `07 号` §11 已迁出) 与`07 号` 的分级读法(该段已迁出 → Entry #246)**;⛔ **不得**由「有引擎文件」推出「协议语义已实现」。

## 12.3 新增协议族的标准流程(add-feature + 单 change)

> **适用范围**:Stage 4+ 维护态(**不再串行排队**,per `objective-cep-platform §4.5`「维护模式:新协议接入走 add-feature + 单 change」)。

| 步 | 动作 | 落点 / 载体 | 硬约束 |
|----|------|------------|-------|
| 0 | **先判「转移表真源」是否成立** | 判定式 T1–T4 = [`cep/protocol-engine.md`](../architecture/cep/protocol-engine.md) §5.1.5;**逐族当前判定 = 台账 `Entry #249` ① 的 `D-3` + `objective-cep-platform §6.2` 的 B3b-1 / B3b-2**（原文出处 = `07 号` 的 `Status Update(2026-11-06)` §②/§③，per Entry #339 已收敛为归宿指针） | ⛔ **T1–T4 未全满足 ⇒ 不补引擎**(补 = 推理填空,per 台账 Entry #238) |
| 1 | 建 feature fragment(**条件步**) | `rddf roadmap add-feature cep --phase-refs phase-10,phase-11` | **触发条件** = 「phase-11 注册 **且** ≥2 CEP changes 跨 phase-10/11 分布」(`.rddf/roadmap.md` Features 段);⚠️ **今日 phase-11 未注册 ⇒ 本步暂不执行**;⛔ 单 phase 退化形态(零信息量)不建 |
| 2 | 建**单** change(两段式) | 规约层 + 实现层(先例:队列 #10 ~ #14) | **KPI Proposed 并发 ≤ 1**(`objective-cep-platform §5.4`) |
| 3 | 落**状态规约载体** | `<family>_state_spec.hh`(置于 `include/tlm/cep/`,表驱动 `transition` + 逐格 `basis`) | ⛔ **零自有表副本**(adapter 委托规约载体);⚠️ **例外**:若族的状态机**内联在 adapter**(CHI RN-F 先例,per 同段 §③)⇒ **不**新建载体,改走 adapter 内联表 + 对应契约面 |
| 4 | 落 **adapter** | `<family>_adapter.hh`(置于 `include/tlm/cep/`) | 转移 API 与 `is_silent_upgrade` 的语义边界 |
| 5 | 落 **engine** | `<family>_protocol_engine.hh`(置于 `include/tlm/cep/`) | 继承 `ProtocolEngineBase<N>` + `set_stream_adapter()` |
| 6 | 登记**实例声明**(13 键) | `include/tlm/cep/protocol_instances.json`(schema = `protocol_instance.schema.json`) | `id` / `family` / `adapter` / `status` / `protocol_version` / `flow_support` / `perm` / `atomic` / `consistency_labels` / `ordering_assumption` / `vc_binding` / `sharer_tracker` / `combination_capabilities` |
| 7 | 登记**静默升级**(若有) | `include/tlm/cep/perm_upgrades.json` | `UNIQUE → WRITEBACK` 形态;⛔ 4 值 `perm` 表达不了的升级**必须**在此登记 |
| 8 | 落**判据 + 反向 fixture** | `test/test_cep_<family>_*.cc` + `cep_<family>_reverse_verify.sh`(置于 `scripts/test/`) | ⛔ **只测不拦不算载体**(须「破坏输入必红」) |
| 9 | 装配期校验 | `instance_assembly_gate.hh` + `adapter_schema_contracts.hh` | 声明 ↔ schema **双向**映射(闸 `cep_decl_alignment_check.sh` + `cep_decl_reverse_verify.sh`) |
| 10 | 冻结面五步授权 | 见 **§12.6** | ⛔ **台账与清单不得合并提交** |

> ⚠️ **先例指针**:队列 #6(`moesi_gpu`)· #8(`chi_rnf`)· #9(`mesif`)· #10(`cxl_2_0`)· #11(`tilelink_c`)· #12(`cxl_3_0_symmetric`)· #13(`nvlink_4`)· #14(`ualink_4`)—— 逐队列的交付面与偏离登记见 `objective-cep-platform §4.0`。

## 12.4 实验 4 件套跑法

**产物形态**(per **`07 号` §7.1** 契约):每个 run 一个目录(目录名 = `run_id`,格式 `{protocol}-{topology}-{directory}-{timestamp}`),置于 `experimental/`,下含 `simulation_config.json` + `trace.h5` + `metrics.json` + `report.html`。

> ⚠️ **仓内实际布局与契约有三处差异**:① 实际目录**多一层**(`experimental/run-<ts>-<tag>/<hN>/`)⇒ 4 件套在**子目录**里;② 目录命名**不含 `07 号` §7.1 的 `{protocol}-{topology}-{directory}` 段**(实测 `run-2026-11-09-cep-e2e` / `run-<ts>-<tag>`,per SU · 十五 D-9 已登记「无拓扑后缀」);③ stub-cache E2E 类 run **只入库 2 件**(`metrics.json` + `simulation_config.json`,per SU · 十五 D-9)⇒ ⛔ **不是** 4 件套齐全 ⇒ 该类 run **不得**作 B4 证据(见本节「三条硬约束」②)。

| 步 | 命令 | 闸 |
|----|------|-----|
| 1 | 跑实验 | `bash scripts/test/cep_experiment_run.sh` |
| 2 | A/B 对比 | `bash scripts/test/cep_h8_ab_runs.sh` |
| 3 | 观测键齐备性 | `bash scripts/test/cep_b4_observation_keys_check.sh` |
| 4 | IC3 三态 fixture | `bash scripts/test/cep_ic3_report_states_check.sh` |
| 5 | 预登记漂移 | `bash scripts/test/cep_prereg_drift_check.sh` |
| 6 | 台账 ↔ 仓内 run 对账 | `bash scripts/test/cep_e2e_reconcile_check.sh` |
| 7 | 渲染报表 | `python3 tools/render_report.py <exp_dir>`(`exp_dir` 是**必填位置参数**;其余工具见 **`07 号` §7.3**) |

**三条硬约束**(⛔ 违反即达成无效):

1. **兜底契约**:不可观测 metric 标 `not-collected` + per-metric `_not_collected_reason`;⛔ **禁止**省略 / 写 `0` / 写 `-1`(per **`07 号` §7.2**)
2. **达成不得仅依赖 run-validity**:违例类(`deadlock_violations` / `memory_violations`)若为 `not-collected` ⇒ IC3 判「**⚪ 不适用**」且该 run **不得**作任何一致性安全断言的证据(per `objective-cep-platform §6.2` B4)
3. **验证深度须声明**:每个实验须声明 `verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} + `verification_scope`;`bounded_random` / `sampled` 的结果 **不得**表述为「已证明无死锁」(per **`07 号` §7.2.1**)

> ⚠️ **已知软点**:`cep_b4_observation_keys_check.sh` 会**自行用 h5py 从 `metrics` 反向合成 `trace.h5`** ⇒ 对「`trace.h5` 由仿真器真实写出」这一环节**无判别力**(只覆盖 metrics 侧观测键)。已登记为独立 backlog。

## 12.5 闸清单

**35 个 `scripts/test/cep_*.sh`**,按职责 7 类:

| 类 | 数量 | 代表 |
|----|:----:|------|
| 冻结面校验 | 1(其反向脚本**已计入**下方 17) | `cep_freeze_check.sh` · `cep_freeze_reverse_verify.sh` |
| 文档同步 / ADR 结构 | 3 | `cep_docs_sync_check.sh` · `cep_doc_ref_check.sh` · `cep_adr_structural_check.sh` |
| 漂移 / 声明对齐 | 5 | `cep_decl_alignment_check.sh` · `cep_archforge_alignment_check.sh` · `cep_prereg_drift_check.sh` · `cep_e2e_reconcile_check.sh` · `cep_index_position_check.sh` |
| 实验运行器 | 3 | `cep_experiment_run.sh` · `cep_h8_ab_runs.sh` · `cep_b4_observation_keys_check.sh` |
| 登记 / 状态核查 | 5 | `cep_b3b2_deferral_check.sh` · `cep_b3b2_d4_probe_check.sh` · `cep_b3b2_d4_quarterly_review.sh` · `cep_ic3_report_states_check.sh` · `cep_b4_coupling_probe.sh` |
| **变异证据(reverse verify / fail-closed)** | **17** | 15 个 `*_reverse_verify.sh` + `cep_source_effect_check.sh` + `cep_tilelink_engine_failclosed.sh` |
| 闸自检 | 1 | `cep_gate_selftest.sh`(对**闸自身**跑回归 fixture) |

**读闸纪律**:

- ⛔ **「跳过」与「跑过」必须可区分** —— 退出码都是 `0` ⇒ **不可**把跳过读成通过。⚠️ **token 不统一**:`cep_decl_alignment_check.sh` 打印 `[SKIP]` / `[RUN]`,而 `cep_archforge_alignment_check.sh` 只打印「· 跳过:」(**无 `[SKIP]` token**)⇒ **以闸的实际输出为准**
- ⛔ **cadence 闸会自然超期**:`cep_b3b2_d4_quarterly_review.sh` 内含「≤100 天未执行即红」断言(最近真实执行 2026-11-09)⇒ **该闸转红与代码无关**(该脚本**刻意不注册 ctest**,以免无关 PR 批量变红)⇒ ⛔ 不作回归信号
- ⛔ **部分闸会真跑实验并写盘**:`cep_experiment_run.sh` / `cep_h8_ab_runs.sh` ⇒ **只读巡检请排除这两个**
- ⛔ **无退出码语义的 `echo PASS` 不算闸** —— 每个闸的 PASS 必须带计数
- ✅ **近半数闸自带反向 fixture**(17 / 35)⇒ 「破坏输入必红」是默认强度要求,⛔ **新增闸须附决定性变异**

> ⚠️ **名单的权威来源 = `ls scripts/test/cep_*.sh`**(35)⇒ ⛔ **不得**以文档里的数字为准。**登记处现状(2026-11-12 起,per 台账 Entry #342)**:`scripts/README.md` §CEP 契约守门脚本 **31 行 + 季度复审 2 + 实验运行器 2 = 35**(已补登 `cep_index_position_check.sh` / `cep_e2e_reconcile_check.sh`)· `AGENTS.md` **4 处已订正为 35**(⛔ 原写「25 个」)· `docs/ONBOARDING.md §5.5` **只列部分**(导航性质,⛔ 不作计数源)。
>
> ⚠️ **另有 5 个 CEP `.py` 闸**(`cep_adr_structural_check.py` / `cep_b3b2_d4_probe_validate.py` / `cep_b4_observation_keys_check.py` / `cep_invariant_map_check.py` / `cep_gate_harness.py`)—— 由 `.sh` 闸**调用**,⛔ **不单独计入 35**。

## 12.6 冻结面五步授权链

**M1 冻结集**:**69 个产物**,清单载体 = `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(含 `globs` 22 条 + `exclusions` **0** 条)。

> **范围口径(per 台账 Entry #324 C-2′)**:冻结面 = **glob 全量 + 显式排除表(当前为空)** ⇒ **新文件自动入集** ⇒ ⛔ **不需要**逐条登记「未冻结」(原枚举式登记表 `test/cep_freeze_scope_registry.json` **已于 2026-11-10 退役**)。

**改冻结件的五步授权链**(⛔ **不得合并提交**):

```
1. 台账 Entry 先落(授权记录)          ← planner 写 .rddf/roadmap/objectives/obj-cep-platform.md §11
2. 产物 commit                        ← builder
3. bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"
4. 清单单独 commit                    ← builder
5. 无参复验(bash scripts/test/cep_freeze_check.sh)rc=0 且 provenance 一致
```

**四条硬约束**:

- ⛔ **缺 `--authorized-by` ⇒ 拒绝写盘**(闸会校验台账 `§11` 真实存在该 id 的 Entry)
- ⛔ `--update` 必须在**产物 commit 之后**跑 ⇒ 否则 provenance 不成立(清单描述的不是该 commit 的内容)⇒ 闸 **rc=1 阻断**
- ⚠️ **清单 commit 之后**,`frozen_at_commit` 落后 HEAD 一个 commit 是**常态且不阻断**(闸仅告警)⇒ ⛔ **不要为消除该告警重复 `--update`**
- ⛔ **台账与清单不得合并提交** —— 合并会使 `frozen_at_commit` 指向错误 commit

**先例**:台账 Entry #319 → 产物 commit `25f7aade` → `--update`(清单 diff **恰 4 处**)→ 清单 commit `6f01a8cd` → 无参复验 rc=0。

## 12.7 DoD 读法(B3b-1 vs B3b-2 + ⏭ 登记语义 + 禁写 `✅(⏭)`)

**终局 DoD 12 条**真源 = `objective-cep-platform §6.2`。本节只给**读法**,⛔ 不复制条款。⚠️ **禁令的权威文本 = 台账 `objective-cep-platform §6.2`(B3a / B3b 拆两行 + **防自伪条款**)**—— 原文在 `07 号` 的 `Status Update 2026-11-04` ① 段,per 台账 Entry #339 收口已**收敛为归宿指针**;本节只给读法,⛔ 不另立禁令。

**两条必须分开读的 DoD 行**(per 台账 `objective-cep-platform §6.2` 的「一词两义」拆两行裁决 —— 原文出处 `07 号` 的 `Status Update 2026-11-04` ① 段):

| 行 | 含义 | 当前值 | 读法 |
|----|------|:-----:|------|
| **B3b-1** | 运行时 ProtocolEngine(**规范可读族**) | ✅ **2/2** | CHI RN-F + TileLink-C —— 仅指「规范可读且四要件全满足的 2 族」 |
| **B3b-2** | 运行时 ProtocolEngine(**受限族**) | ⏭ **0/4** | CXL 2.0 非对称 / CXL 3.0 对称 DCOH / NVLink / UALink |

**⛔ 四条禁写**:

1. ⛔ **不得**把 B3b-1 的 ✅ 扩写为「B3b 整体达成」
2. ⛔ **不得**写 `B3b-2 ✅(⏭)` 形态(⛔ **单一状态 token**;N = 0 时一律 `⏭`,**永不 ✅**)
3. ⛔ **不得**把 B3b-2 整行改写为「已达成」—— 「DoD 降格后仍宣称达成」是明确禁止的失败模式。📌 **本条作用域已重裁**(per 台账 Entry #373):作用域与例外**见 `07 号` 的 `Status Update 2026-11-13` ②**(⛔ 本行不复制作用域定义)。⚠️ 现行登记口径仍 = 口径 ②(挂起);口径 ④ **未 ratify**。⛔ **不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文**(per 台账 Entry #264 / Entry #284 的先例形态)
4. ⛔ **不得**用「**永久冻结**」措辞 —— 4 族的阻塞是「**外部条件 + 人力资源受限**」(逐族触发条件见 `test/cep_b3b2_deferrals.json`)

**⭕ 登记形态**(B2 同型;分母恒定,⛔ **永不可缩**):

```
B2 ✅ 66/66(N 项有牙载体 + M 项授权⏭,映射表全绿,Entry #XXX)
```

- **分母 = 66**(编号位 67)· **N ≥ 1**(全 ⏭ ⇒ **不得**登记为达成)· **M = 66 − N** 为授权 ⏭ 项,须**逐条机器可查登记**
- **当前实测**:**61 项有牙载体 + 5 项授权 ⏭** ⇒ `test/cep_invariant_assertion_map.json`(61 条,派生视图)+ `test/cep_invariant_deferrals.json`(5 条 `⏭-unmodelled`)
- ⛔ **映射表是生成物**(`// INV-CARRIER: <id>` 刚性标记)⇒ **不是**手写口径源;**重新生成** = `python3 scripts/test/cep_invariant_map_check.py --generate`

**终态 Go 的当前状态**:**⏳ 挂起**(口径 ②,per 台账 Entry #264)。口径 ④「条件化终局 + B3b-2 判定规则预登记」已由台账 Entry #332 裁决,⛔ **其生效以用户 ratify 为前置** ⇒ **未 ratify 前不自动开终态门**。⚠️ 口径 ④ 的**本体 SU 尚未落进 `07 号`**(Entry #332 ⑪ 的落点含新 SU)⇒ **本段只登记当前生效的口径 ②**。

## 12.8 未结义务清单

> **本清单只列「机器可查载体已存在」的未结项**;其余未结讨论散落在台账 §11 与各 SU,⛔ **不在此复述**。
>
> ⚠️ **状态判定不在本表** —— 各行的「当前是否仍成立」以**载体文件与台账**为准(本表是**导航**,不是状态源)⇒ 各项闭合后本表即过期 ⇒ ⛔ **不得**把本表读作「实时状态」。
> ⚠️ **定位精化**(2026-11-12,per 台账 **Entry #377** 的 T9)——本表 = **导航 + 历史登记**;表内的删除线行 = **历史闭合标记**(per 台账 Entry #342),⛔ **同样不是状态源**。
> ⇒ **唯一状态源** = OpenSpec change 的 `tasks.md` 勾选 + `proposal.md` 状态行(per `obj-cep-platform §4.7` 硬规则 1);⛔ 本表**不引入状态列**(加状态列 = 造出第二状态源)。

| 类别 | 项 | 载体(机器可查) | 阻塞类型 | 解除触发 |
|------|-----|---------------|---------|---------|
| **受限族**(B3b-2 = 0/4) | CXL 2.0 非对称 | `test/cep_b3b2_deferrals.json` | **external** | ECA 解除 / 联盟发布可读规范 / 人类成员条款摘要 |
| | CXL 3.0 对称 DCOH | 同上 | **external** | 同上 |
| | NVLink | 同上 | **external** | NVIDIA 发布**条款级**规范(白皮书 / 编程指南**不算**) |
| | UALink | 同上 | **external** | Evaluation Copy 约束解除 / 联盟发布公开规范 |
| **invariant ⏭**(5 条) | `CEN-Ib8` / `INV-CEP-08-IC1` / `INV-CEP-09-IC2` / `INV-CEP-13-IC1` / `INV-CEP-13-IC3` | `test/cep_invariant_deferrals.json` | 能力未建模 | 逐条 `trigger_condition` |
| **ADR 归因**(18 条) | `condition_unmet` 2 · `carrier_buildable_id_blocked` 10 · `carrier_present_id_absent` 2 · `doc_layer_manual_review` 4 | `test/cep_adr_structural_deferrals.json` | 见逐条 `disposition` | **建 id 架构性不可行** ⇒ 归类收敛(per Entry #326/#331);⛔ 不新建 id |
| **口径 ④ 实施轮** | MUST-FIX-1 / MUST-FIX-2 | 台账 Entry #332 ⑧ | ✅ **已完成**(2026-11-12,change `cpptlm-cep-dod-reading-consistency` 已归档;⛔ 未 ratify 的 MUST-FIX-3/4 **未并入** —— 回执 per 台账 **Entry #375** 归档回执 + **Entry #376** 验收 15/15 AC PASS) | —(已闭合) |新 SU 落地时)执行;⚠️ **重裁落地前 §12.7 禁写 #3 按绝对禁令执行** |
| | MUST-FIX-3 / MUST-FIX-4 | 台账 Entry #332 ⑧ | 待 **用户 ratify** / 归档形态定义 | ratify 后方可执行 |
| ~~**闸强度缺口**~~ | ✅ **已闭合 2026-11-12**(per 台账 Entry #342)—— `cep_b3b2_deferral_check.sh` 的恒真 grep 段(grep `$DECL` **自身**)已删,改为**集合覆盖 + 逐族锚校验**;⛔ 本表状态判定以**载体文件与台账**为准 | 同脚本 | ✅ 已闭合 | 决定性变异:删 1 族 / 同族加第 2 条 ⇒ 必红 |
| ~~**守门软点**~~ | ✅ **已闭合 2026-11-12**(per 台账 Entry #342)—— `cep_b4_observation_keys_check.sh` 的自合成 `trace.h5` 已加 `[synthetic-fixture]` **显式标注**且 exit 0 ⇒ 强度可被 grep 观测,⛔ 不欺 | 同脚本 + 本节 **§12.4** 尾注 | ✅ 已闭合 | 闸头注释写明「本闸 = 文件级」 |
| ~~**登记处不完整**~~ | ✅ **已闭合 2026-11-12**(per 台账 Entry #342)—— `cep_index_position_check.sh` / `cep_e2e_reconcile_check.sh` 已登记进 `scripts/README.md` 守门表;`AGENTS.md` 的闸数已改为「以 `ls scripts/test/cep_*.sh` 为准」;索引位置闸已强化为**三合一**(实际 `## ` 数 == 自述 == 表行数 + 行号逐行 + 标题覆盖) | 三处登记文档 + `scripts/test/cep_index_position_check.sh` | backlog | 独立 change 补齐登记 + 给索引闸加「标题计数 == 行数」断言 |
| ~~**工具 backlog**~~ | ✅ **已闭合 2026-11-12**(per 台账 Entry #342)—— 索引生成器已落仓 `scripts/cep/generate_doc_index.py`(连跑 2 次逐字相同 ⇒ 幂等);索引 A 28 行 + 索引 B 15 行全量 + 行号实测 | `07 号` §末 文档索引 | ✅ 已闭合 | 对账闸 `cep_index_position_check.sh`(ctest 注册) |
| **范围外既有状态** | 非 CEP 线归档件 delta 无对应 main spec | `openspec/changes/archive/` | — | ⛔ **不属 CEP 线** ⇒ 不影响 `§8.3` ①;**明细见台账 Entry #332 ⑫ SF-4**(⛔ 计数随归档增长 ⇒ 本节不复述数字) |

> ⚠️ **本清单不含**下列两项 —— 二者的**状态判据不在本清单**,处置见各自出处:
> - **「全绿 run」缺口**(SU · 十五 ①)—— ⚠️ **至今未闭合**:SU · 十六 ① 只闭合了「回程腿在拓扑级不可达」这个**子项**,并逐字声明「**仍不承载全绿 run**」(IC3 仍 ⚪ 不适用)⇒ 「全绿未达成」结论不变;
> - **装配闸未挂 `validate_topology` CLI 的不对称**(SU · 十七 ② 末条)—— ✅ **已闭合 2026-11-13**(per 台账 **Entry #381** 归档 + **Entry #387** 验收):IAG 族**两臂同补** —— ① 装配路径臂(`src/core/module_factory.cc` **Step 0.26**,FAIL ⇒ 装配失败且**零副作用**)② 离线 CLI 臂(`examples/cep_instance_assembly_gate.cc` + `validate_topology` 的**单列第二段**,9 份 CEP config)⇒ 两臂**共用同一份**判据实现与加载规则 ⇒ `instantiateAll` 与 CLI 对同一 config 的结论**必然一致**。
>   ⚠️ **口径订正(2026-11-13,per 台账 Entry #379 ②)**:原登记(Entry #358)称「CLI 未挂 IAG 闸族 ⇒ 两侧结论**可能不一致**」—— **实测偏窄**:IAG 族在 `instantiateAll` 侧**原本也没有**执行臂(`module_factory.cc` 全文 0 命中 `run_instance_assembly_gate`)⇒ 实为「**两侧一致地都不挂**」⇒ 缺陷面更宽 ⇒ 修法必须**两臂同补**(已按此实施)。
>   ⛔ **勿与本轮 Q1 修复的事项混淆**:Q1 修的是「**组合 target 引用已删除文件 ⇒ 整条 target
>   跑不完 ⇒ 其后 4 条拓扑校验 + 装配闸 CLI 挂点全部从未执行**」(静默 20 天)⇒ 该项**已闭合**;
>   两者是**不同**的缺陷(前者 = 挂点未执行;后者 = 挂点已执行但判据族不全)。

---

