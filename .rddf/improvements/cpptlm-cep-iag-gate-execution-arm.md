---
name: cpptlm-cep-iag-gate-execution-arm
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— 装配闸族(IAG)的执行臂补齐
依赖:
  - ADR-CEP-09
  - ADR-CEP-05
状态: "🟡 待实施（2026-11-12 起草;per 台账 **Entry #377** 的批 2(D-2)+ **Entry #378** 的批 1 收口;architect 方案 `.rddf/reviews/cep-cli-iag-gate-alignment.md`(4 条决策 + 影响面实测 + 验收判据);⚠️ 起草期实测**纠正**既有登记的窄化 —— 不是「两侧不一致」而是「两侧一致地都不挂 IAG 族」,故修法必须**两臂同补**）"
created: 2026-11-12
---

# cpptlm-cep-iag-gate-execution-arm: 装配闸族(IAG)的执行臂补齐 —— 让「只测不拦」变成「装配路径就拦」

> **范围纪律**:只做 **IAG 族(`[IAG-01]~[IAG-08]`)的执行臂补齐**;⛔ 不改任何 ADR 正文 · ⛔ 不新建 ADR · ⛔ 不改 `B3b-2 = 0/4` · ⛔ 不顺手修 §4.0 预存 grep 漂移 · ⛔ 不在本 change 内加「体量数字」机器守(已登记为 backlog,per Entry #378 ④)。
> **估时**:≈1-2 人日(离线 CLI ≈0.5d · 装配路径臂 ≈0.3d · target 第二段 ≈0.2d · 决定性变异 + 回归 ≈0.5d)

## 架构依据

| 依据 | 内容 |
|---|---|
| `ADR-CEP-09` 抓取点 1 | 装配期判据必须**在装配路径上**执行(⛔ 不得以运行时检查替代)—— 与 `module_factory.cc:172-174` 的既有注释同源 |
| `ADR-CEP-05` V-05-6 | 「三明治挂载形态」的配置契约 ⇒ bridge 必须指向**已声明**实例且模块**在拓扑内** |
| 台账 **Entry #377** 批 2 | D-2 的立项依据 + 三批路径 |
| 台账 **Entry #378** ④ | 「体量数字无机器守」缺口**不在本 change**(记 backlog) |
| architect 方案 | `.rddf/reviews/cep-cli-iag-gate-alignment.md` —— **4 条决策 + 9 份 config 影响面实测 + 6 条验收判据** |
| 先例 | `Entry #319`(BSS-01 补执行臂)· `module_factory.cc:827`(`find_bridge_module_conflicts` 已有装配路径执行臂)⇒ **补执行臂是本仓既有模式** |

## 范围

| 面 | 内容 |
|---|---|
| ✅ **在范围内** | ① 离线 CLI `cep_instance_assembly_gate`(对 config 跑 IAG 族)② 装配路径臂(`module_factory.cc` Step 0.75)③ `validate_topology` 组合 target 的**第二段**(CEP config 清单)④ 决定性变异(≥4 条,含「两臂同签名」与「挂点真跑」)⑤ ctest 注册 + 守门表登记 |
| ⛔ **不在范围内** | ① 改 IAG 判据**本身**的口径(`instance_assembly_gate.hh` ⛔ 只读引用,判据文本一字不改)② 改 `protocol_instances.json` / `collective_planes.json` 的**内容**(只读)③ `nvlink_4` 的 IAG-05 覆盖缺口(仓内无对应 config ⇒ 登记不修)④ 任何 ADR ④ `B3b-2` 计数 ⑤ 「体量数字」机器守(backlog) |

## Why

**一个「bridge 指向未声明实例」的 config,在真实仿真与离线 CLI 上都不会被拦** —— 唯一有执行臂的地方是 test。

| 缺口 | 契约依据(逐字可核) | 实测 | 不修的后果 |
|---|---|---|---|
| **装配路径臂缺失** | `instance_assembly_gate.hh` 头注 + `test_cep_bridge_reattach_guard.cc:22-27`「契约侧判据 `[BSS-01]` 早已存在…… 但它只被**测试侧**调用 ⇒「只测不拦」⇒ 装配路径**不拦**」 | `grep 'run_instance_assembly_gate' src/core/module_factory.cc` ⇒ **0 命中** | 结构违规的 config **能装配成功** ⇒ 仿真跑到一半才炸 ⇒ 错误现场离根因最远 |
| **离线 CLI 臂缺失** | `ADR-CEP-09` 抓取点 1 的离线挂点先例(`cep_channel_dependency_gate`) | `scripts/CMakeLists.txt` 的组合 target 命令面 = 4 个**非 CEP** config + 声明层 CLI ⇒ 对 CEP config **无 IAG 判据** | `validate_topology` 一键跑 ⇒ **不覆盖**装配结构 ⇒ 「一键校验」的承诺落空 |
| **既有登记窄化(起草期实测纠正)** | `cep-usage.md §12.8` 表外第 2 项 + Entry #358 口径澄清「CLI 未挂 IAG 闸族 ⇒ `instantiateAll` 与 CLI 对同一 config 的结论**可能不一致**」 | **实测不成立**:IAG 族在 `instantiateAll` 侧**也没有**执行臂 ⇒ 是「**两侧一致地都不挂**」 | ⛔ 若按原登记只补 CLI 臂 ⇒ 制造**反向不一致**(CLI 报红、真实装配放行)⇒ 必须两臂同补 |

**为什么是现在**:C1~C4 已把治理线收口(闸强度 / 传输层 VC / 治理不变式 / DoD 读法)⇒ 剩下最后一处「**闸存在但没装在路径上**」的同族缺陷;且 `module_factory.cc` 的 Step 0.25 已有一条**同族执行臂**(channel/ordering)⇒ 补第二条的边际成本最低。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **离线 CLI** `cep_instance_assembly_gate` | `examples/cep_instance_assembly_gate.cc`(新增) | 对 **config** 跑 `run_instance_assembly_gate(config, instances, planes)`;退出码 `0=PASS/SKIP · 1=FAIL · 2=用法错`;⛔ **不扩**声明层 CLI(`cep_channel_dependency_gate`) |
| 2 | **装配路径臂** | `src/core/module_factory.cc` **Step 0.75** | 调**同一份** `run_instance_assembly_gate` ⇒ FAIL ⇒ `return false`(**零副作用**,此时模块尚未创建) |
| 3 | **target 第二段** | `scripts/CMakeLists.txt` | `CPPTLM_CEP_VALIDATION_CONFIGS` = 9 份 `configs/cep_*.json`;每条**只**跑 IAG CLI ⇒ ⛔ 不与 `${TOPOLOGY_VALIDATOR}` 混跑 |
| 4 | **决定性变异 + 注册** | `scripts/test/cep_iag_gate_reconcile_check.sh` + `test/CMakeLists.txt` + `scripts/README.md` | ≥4 条变异,**按红因签名**断言;ctest **172 → 173**;守门表 **34 → 35** |

## Acceptance

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ **「rc≠0」不构成红因**。

- [ ] **AC-1（装配路径臂有牙）**:把某 CEP config 的 bridge `protocol_instance` 改成**未声明 id** ⇒ `instantiateAll` ⇒ **必红**,红因签名含 **`[IAG-02]`**,且**断言模块未创建**(零副作用)⇒ 还原 ⇒ 绿
- [ ] **AC-2（离线臂有牙 + 两臂同签名）**:同一破坏 ⇒ IAG CLI ⇒ **必红**,红因签名**同为 `[IAG-02]`** ⇒ 证明两臂共享同一判据实现 ⇒ 还原 ⇒ 绿
- [ ] **AC-3（两臂同绿 · 9 份 config 逐个）**:9 份 `configs/cep_*.json` 逐个 ⇒ 装配路径绿 + CLI 绿(`cep_chi_rnf_e2e_a.json` = **SKIP** ⇒ 绿)⇒ 无一被新执行臂判红
- [ ] **AC-4（挂点真跑 · Q1 教训）**:`cmake --build build --target validate_topology` ⇒ 命令面**含 9 次 IAG CLI 调用** ⇒ 变异「从命令面摘掉 IAG 调用」⇒ **必红** ⇒ 还原 ⇒ 绿(⛔ 不得是「挂点存在但命令面没调」)
- [ ] **AC-5（禁双实现 + 判据只读）**:两处均调 `run_instance_assembly_gate` ⇒ `instance_assembly_gate.hh` 的**内容零 diff** ⇒ 机器可验(`git diff` + grep)
- [ ] **AC-6（0 回归）**:`ctest` **173/173** · `cep_*.sh` **40/40** rc=0 · `format.sh --check` rc=0 · `docs_sync_check.sh --strict` rc=0 · `cep_doc_ref_check.sh --strict` rc=0 · `agents_md_lint.py` rc=0
- [ ] **AC-7（范围纪律）**:⛔ 未改 `instance_assembly_gate.hh` 内容 · ⛔ 未改 `protocol_instances.json` / `collective_planes.json` · ⛔ 未改任何 ADR · ⛔ 未改 `B3b-2` · ⛔ 未新增「体量数字」断言 ⇒ `git diff --stat` 逐条可核

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-instance-assembly` / `cep-adapter-schema-contracts` | **强化**(非新增) | IAG 判据**已存在**;本 change 只补**执行臂** ⇒ ⛔ 不新增 capability、⛔ 不改判据文本 |
| `cep-core-contracts`(装配面) | **强化** | 「装配期判据不得以运行时检查替代」(`ADR-CEP-09` 抓取点 1)的落地补全 |

## Impact

| 项 | 内容 |
|---|---|
| **新增文件** | `examples/cep_instance_assembly_gate.cc` · `scripts/test/cep_iag_gate_reconcile_check.sh` |
| **修改文件** | `src/core/module_factory.cc`(Step 0.75 加闸)· `scripts/CMakeLists.txt`(第二段)· `test/CMakeLists.txt`(+1 `add_test`)· `scripts/README.md`(守门表 +1 行)· `examples/CMakeLists.txt`(+1 可执行 + ctest 注册) |
| **冻结面** | ❌ **零命中** —— 改动全在 `src/` / `examples/` / `scripts/` / `test/` ⇒ ⛔ **不触发五步链**、清单零变更 |
| **KPI slot** | ✅ 占 **1 个** Proposed(当前 **0/3**)⇒ 归档后回填 0/3 |
| **风险** | ① **新增拦截点** ⇒ 可能拦住过去能装配(但结构违规)的 config ⇒ 属期望行为,实施期逐条记录「由绿转红」;② `nvlink_4` 无对应 config ⇒ IAG-05 路径暂无真实 config 可跑(**已知覆盖缺口**,登记不修) |
| **不可约残余** | ① IAG 族只覆盖**装配期结构**,⛔ 不覆盖运行时一致性;② CLI 臂对非 CEP config = SKIP ⇒「离线闸全绿」**不等于**「所有 config 都过了 CEP 判据」 |

## 已裁项（本草案不重新讨论）

| # | 裁决 | 落点 |
|:--:|---|---|
| 1 | 装配路径臂挂 **Step 0.75**(topology 就绪 + 零副作用) | architect 方案 §二 决策 1 |
| 2 | **新建独立 CLI**(⛔ 不扩声明层 CLI) | 决策 2 |
| 3 | target **单列第二段**(⛔ 不与通用 validator 混跑) | 决策 3 |
| 4 | `required_plane_instances` **传空集合**(⛔ 取 plane_binding 全集会误伤 `moesi_gpu`/`mesif` 的 null 绑定) | 决策 4 |

## metis 处置对照

> ⛔ **本轮未跑 metis** —— 用户已批准 architect 方案(4 条决策 + 影响面实测 + 验收判据已落盘),且草案**不引入新裁决** ⇒ 按 §审查配对「新建 improvement ⇒ metis 审」的最小适用边界:本草案 = 方案转录,⛔ 不重开已裁项。**若实施期发现与 architect 方案冲突 ⇒ 停下回 planner,不临场改口径。**

## 关联

台账 **#378**(批 1 + 体量数字缺口 backlog)· **#377**(批 2 分解)· **#358**(Q1 修复 + 口径澄清)· **#319**(BSS-01 执行臂先例)· **#301** · architect 方案 `.rddf/reviews/cep-cli-iag-gate-alignment.md` · `include/tlm/cep/instance_assembly_gate.hh` · `src/core/module_factory.cc` · `scripts/CMakeLists.txt` · `examples/CMakeLists.txt`
