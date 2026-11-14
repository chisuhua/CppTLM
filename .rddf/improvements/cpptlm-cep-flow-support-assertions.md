---
name: cpptlm-cep-flow-support-assertions
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-02
  - ADR-CEP-12
状态: 🟡 **已立项**(2026-11-15 · metis 第 1 轮 14 条全处置 + 2 项用户裁决 · change `cpptlm-cep-flow-support-assertions` 已建(proposal + delta spec)· `openspec validate --strict` **rc=0** · KPI **0/3 → 1/3** · **待 builder 补 `design.md` / `tasks.md` 并实施**)
created: 2026-11-15
---

# improvement: cpptlm-cep-flow-support-assertions

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**:⛔ 不改 `include/tlm/cep/protocol_instances.json`(声明源)· ⛔ 不改任何引擎 / adapter / tracker 实现 · ⛔ 不改 `protocol_instance.schema.json` · ⛔ `include/tlm/cep/**` **零改动** · ⛔ 新增 invariant 数 = **0** · ⛔ 本仓**只锁本仓权威的实例**,不断言镜像权威。
> **估时**:**实施 0.5 人日 + 归档链 0.3 人日**(归档链 = 五步授权 + 冻结清单复核)
> **KPI**:占 OpenSpec slot **1/3**(用户裁决 2026-11-15)
> **审查**:metis 第 1 轮(14 条 MUST/SHOULD-FIX 已逐条处置,per 本 improvement §架构依据「先例核对」)

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `protocol_instance.schema.json` | `include/tlm/cep/protocol_instance.schema.json:56-71` | `flow_support` 是 **10 键 required** + `additionalProperties:false` ⇒ 声明**形状**是契约 |
| `ADR-CEP-02 §2` | 已签发 · canonical 10 流清单 | 10 流的**名字**的唯一真源 = `include/tlm/cep/protocol_engine_base.hh:71-100` 的 `CanonicalFlow` + `kCanonicalFlowNames`(与 schema `required` 逐字一致) |
| `ADR-CEP-12` | 已签发 · 协议实例能力 / 前提假设 / 组合能力的结构化声明 schema | 「协议**有什么能力**」由**声明侧**权威给出 ⇒ 声明值漂移 = 能力面漂移 |
| `ADR-CEP-12 §13` + `ADR-CEP-04 §13` | 两份 ADR 的 Status Update | **权威分区**:`protocol_instances.json` 的 `repo_owned_instance_ids` = 6 个实例的权威**在本仓**;其余 2 个是 **ArchForge 镜像**(权威不在本仓)⇒ 本 change **只锁 repo_owned** |
| `cep-core-contracts` `cep-capability-map-contract` | `openspec/specs/cep-core-contracts/spec.md:246-254` | 现有 Scenario 只锁**形状**(「恰好 10 项 + 与 canonical 10 流逐名对齐」)⇒ **值**层无契约 |
| **先例核对(metis Q7)** | 同仓 3 种既有做法 | ① **镜像闸 + 持久反向 fixture**(`cep_decl_alignment_check.sh:523-551` 的 `strict_equal` 逐字段 + `cep_decl_reverse_verify.sh:174-217` 穷举 12 键)—— **期望值来自外部权威**,更强,但**只在 ArchForge 在位时生效**;② **单族字面断言 + 显式正控**(`test_cep_cxl_dcoh_instance_decl.cc:111-129`:`cxl_3_0`=true 与 `cxl_2_0`=false 互为正控)—— **本 change 是它的批量化**;③ **spec 侧「逐字未变」义务 + 测试**(`openspec/specs/cep-ualink-coherence-engine/spec.md:72` ↔ `test_cep_ualink_engine.cc:343-362`)。⇒ 本 change 走 **② 的批量化形态** + **① 的持久 fixture 纪律**;**不引入新做法** ⇒ **无需 architect 裁决** |
| **反向 fixture 形态先例** | `test/test_cep_instance_declaration_contract.cc:490-543`(`ADR-CEP-04-SU4-O2`) | **同一文件内**已有「值锁 + 内存副本翻转一格 ⇒ 必须判否」的成型先例(③ 段)⇒ 本 change 照抄该形态 |
| **持久 fixture 纪律** | 仓内铁律 2(台账 Entry #342 复审 B-NEW-1) | 「新断言**无持久反向 fixture** ⇒ 整用例被删时仓内无任何自动化会红」⇒ **持久** fixture 必须落在**脚本侧**(且形态须满足「只写临时目录 ⇒ 可进 ctest 并行」) |
| 台账本轮实测 | 2026-11-15 · 文档对齐订正的副产物核对 | repo_owned 6 实例的 60 个 `flow_support` 值中 **39 个无断言**(详见 §Why) |

## 范围

**In Scope**:

- **期望表**(新增 `test/cep_flow_support_expected.json`):**6 个 `repo_owned` 实例 × 10 键 = 60 格**的期望值,逐格显式写死;每实例带 `source` 字段(族级出处;**无逐值出处者显式标注**)。
- **判据**(新增 1 个 TEST_CASE,落 `test/test_cep_instance_declaration_contract.cc`,标签 `[cep][instance-decl]`):期望表 ⟷ 声明源**逐格比对** + 形状联判 + 类型 + id 唯一性 + **内存副本反向 fixture**;`load_json` 加 **env 覆盖**(`CEP_FLOW_SUPPORT_DECL`)供持久 fixture 指向副本。
- **持久反向 fixture**(新增 `scripts/test/cep_flow_support_reverse_verify.sh`):毒化**临时副本**(⛔ 不改仓内文件)⇒ 跑 `build/bin/cpptlm_tests '[cep][instance-decl]'` ⇒ 必红 ⇒ 恢复态必绿 ⇒ **可注册进 ctest**(满足「只写临时目录」的并行安全条件)。
- **1 份 delta spec**:`MODIFIES openspec/specs/cep-core-contracts/spec.md` —— ADDED `cep-flow-support-value-lock`(**变更检测义务**措辞)+ **MODIFIED `cep-sharer-tracker-contract`**(5 种 ⇒ 6 种;见 §Why 第 2 条)。
- **1 份台账 Entry**(planner 域,append-only)。

**Out of Scope**:

- ⛔ **不断言镜像实例**(`moesi_gpu` / `mesif`)的 `flow_support` 值 —— 其权威在 ArchForge,本仓断言自己已声明不拥有的权威 = 违反权威分区;其 SKIP 条件性洞单独登记(见 §Impact 残余登记)。
- ⛔ **不断言引擎行为与声明是否一致** —— 那是档 2 协议级观测面(per `ADR-CEP-20` 决策 5/6),另有立项。本 change 是它的**前置**(声明值被锁后才有稳定基线)。
- ⛔ 不改声明源 / schema / 引擎 / adapter / tracker;不改 `family` / `perm` / `vc_binding` / `ordering_assumption` 等其他 12 个顶层属性的断言面。
- ⛔ 不新建 ADR · 不改任何已签批 ADR(只**引用** `ADR-CEP-04 §13` 的「6 种实现」追加式修订)。
- ⛔ 不动 M1 冻结面(除归档时 delta 合并进 `cep-core-contracts` 这一处 ⇒ 归档步走五步链,per AC-08)。

## Why

**第 1 条 —— 真实缺口是 39 个「本仓权威却无守」的声明值。**

`flow_support` 是「协议级能力声明」的机器可读面,且被**运行时判据实际消费**:`include/tlm/cep/mesif_protocol_engine.hh:690-695` 的 `gate(CanonicalFlow f)` 以「`flow_support = false` ⇒ 拒(不静默降级)」实现(`ualink_protocol_engine.hh` 同构)。

但断言面与声明面严重不对称(2026-11-15 实测,按**权威归属**分层):

| 权威 | 实例 | 值数 | 现有保护 | 缺口 |
|---|---|:--:|---|:--:|
| **ArchForge 镜像** | `moesi_gpu` / `mesif` | 20 | ⑥ `strict_equal` 逐字段(`cep_decl_alignment_check.sh:523-551`,含 `flow_support`)+ 持久反向 fixture(`cep_decl_reverse_verify.sh:183` 穷举 12 键) | 0(但**条件性**,见残余登记) |
| **本仓权威**(`repo_owned`) | `chi_rnf` | 10 | — | **10** |
| | `cxl_2_0` | 10 | 仅 `back_invalidate=false`(`test_cep_cxl_dcoh_instance_decl.cc:117`) | **9** |
| | `tilelink_c` | 10 | — | **10** |
| | `nvlink_4` | 10 | 仅 **key 存在性**文本扫描(`test_cep_nvlink_sharp.cc:277-293`,扫的是 13 个顶层键) | **10** |
| | `cxl_3_0_symmetric` | 10 | 10/10(`test_cep_cxl_dcoh_instance_decl.cc:113` + `:121-129`) | 0 |
| | `ualink_4` | 10 | 10/10(`test_cep_ualink_engine.cc:349-357`) | 0 |
| | **小计** | **60** | 21 | **39** |

⇒ **把 `chi_rnf` 的 `address_resolve` 从 `true` 改成 `false`**(或反之)**不会被任何测试发现** —— 而该字段恰是 `gate()` 的「声明 false ⇒ 拒」的输入 ⇒ **声明漂移会静默收窄/放宽运行时能力,且 CI 全绿**。

**第 2 条 —— 同一个 delta 落点上的冻结契约互斥(合并处置的理由)。**

`openspec/specs/cep-core-contracts/spec.md` 是 M1 冻结产物(`cep_freeze_check.sh` 的 `openspec/specs/cep-*/spec.md` glob)。其 `cep-sharer-tracker-contract` Requirement 仍写「**5 种实现**」(`:83` / `:88` 的 5 项列表 / `:96` 的「Hazard_* 在 **5 种实现**间收敛」),而:
- 代码真源 `include/tlm/cep/sharer_tracker.hh:238-260` 的 `kTrackerTraits` = **6 行**(第 6 行 `HierarchicalDirectory` + `AggregationAt::HIERARCHICAL`,`:255-259`);
- `ADR-CEP-04:391` **已**追加式修订「5 种实现 ⇒ 6 种实现」(正文不改);
- `docs/architecture/cep/directory-ordering.md:22` 已写「6 行」;
- **兄弟 spec** `openspec/specs/cep-hierarchical-directory/spec.md:36-38,61` 明确以 `kTrackerTraits[5]`(第 6 行)为三方真源并要求「逐字段相等」。

⇒ **唯一陈旧载体 = 这份冻结 main spec**,且它与**同在冻结集内**的兄弟 spec **契约互斥**。本 change 的 delta 本就要写进同一文件 ⇒ **同一文件 + 同一冻结成员 + 同一条五步链** ⇒ 合并处置的边际成本 ≈ 0;拆开则需**第二条五步链**(台账要求两条链**物理串行**,per Entry #148 纪律)⇒ **合并**(用户裁决 2026-11-15,采 metis 建议)。

**第 3 条 —— 认识论边界(⛔ 不得过度声称)。**

期望表是**变更检测器**,不是**独立 oracle**:值的权威仍是声明源 `protocol_instances.json`;本表的契约力在于「**声明值变更必须同步本表 + 附台账 Entry,否则红**」⇒ 强制任何能力面变更走**显式评审**,而非静默漂移。**不得**把本表读作「本仓获得了这些值的独立权威」。⇒ delta spec 的 Requirement 因此写**变更检测义务**,**不**写「值的权威归属」—— 后者需要 `ADR-CEP-12` Status Update 授权(其 `## Verification` 六条判据**无**逐实例值判据),**本 change 不动已签批 ADR**。

## What Changes

1. **`test/cep_flow_support_expected.json`**(新增):期望表 —— 6 个 repo_owned 实例 × 10 键 = **60 格**;每实例带 `source`(族级出处;`cxl_2_0` / `tilelink_c` / `nvlink_4` **仓内无逐值出处** ⇒ 显式标注「无逐值出处(冻结口径)」,⛔ 禁用笼统背书)。
2. **`test/test_cep_instance_declaration_contract.cc`**(改):新增 1 个 TEST_CASE(per-instance `SECTION`,避免首个 `REQUIRE` 中止后看不见其余格):
   - ① **期望表 ⟷ 声明源逐格比对**,失败消息含 `id` / `key` / 期望 / 实际;**双向集合相等**(表 ids == `repo_owned_instance_ids` ⇒ 新增实例不会「无覆盖且绿」)。
   - ② **形状联判**:每实例 `flow_support` 键集**恰好 10** 且与 `kCanonicalFlowNames`(代码真源)逐名一致。
   - ③ `is_boolean()` 类型断言(避免 `get<bool>()` 对整数抛 `type_error` ⇒ 崩而非干净失败)+ **id 唯一性**断言。
   - ④ **内存副本反向 fixture**:逐实例轮转翻转 1 格 ⇒ `REQUIRE(flipped != original)` + `REQUIRE_FALSE(比对通过)` + 失败消息含 id/key(形态照抄同文件 `ADR-CEP-04-SU4-O2` ③ 段)。
   - ⑤ **域 = `repo_owned_instance_ids` 全集**(⛔ 不按 `status` 过滤)。
   - ⑥ `load_json` 加 **env 覆盖** `CEP_FLOW_SUPPORT_DECL`(仅本文件内 helper,零外溢)⇒ 供持久 fixture 指向副本。
3. **`scripts/test/cep_flow_support_reverse_verify.sh`**(新增):**持久**反向 fixture —— 复制声明源到**临时目录**并毒化 1 格(⛔ **不改仓内任何文件**)⇒ `CEP_FLOW_SUPPORT_DECL=<副本>` 跑 `build/bin/cpptlm_tests '[cep][instance-decl]'` ⇒ 必红 ⇒ 未毒化态必绿 ⇒ **注册进 ctest**(形态满足「只写临时目录 ⇒ 并行安全」)。
4. **delta spec**(新增):`MODIFIES openspec/specs/cep-core-contracts/spec.md` —— ADDED `cep-flow-support-value-lock`(变更检测义务;3 个 Scenario)+ MODIFIED `cep-sharer-tracker-contract`(5 ⇒ 6 种实现)。
5. **台账 Entry**(新增,planner 域,append-only)。

**载体选择理由**:落既有 `test_cep_instance_declaration_contract.cc`(标签 `[cep][instance-decl]`)⇒ 零新增 CMake 条目(`test/` 用 `file(GLOB)`)、零 ctest 注册改动(仅新增 1 个 shell fixture 需注册)。

## Acceptance

- [ ] **AC-01** 期望表 = **6 实例 × 10 键 = 60 格**,且**格数可机器验收**(`REQUIRE(表实例数 == 6)` + per-instance `SECTION` + 期望值以**字面量常量**表达(⛔ 不从 JSON 推导)⇒ 编译期可数);⚠️ 「字面量而非派生」是**源码属性**,故用**编译期常量计数**把它变成可验收项
- [ ] **AC-02** 每实例的 `source` **可机器验收**:所引文件**存在** + 锚点串**存在**(复用 `test/cep_adr_parser.hh` 的 `read_file`);`cxl_2_0` / `tilelink_c` / `nvlink_4` 显式标「无逐值出处(冻结口径)」;⛔ **禁**用「ADR-CEP-12 能力映射」做笼统背书(其 `## Verification` 无值判据)
- [ ] **AC-03** **反向控制机器化四要件**:① 逐实例 `SECTION` ② `REQUIRE(被变异实例数 == 6)` ③ `REQUIRE(翻转值 != 原值)`(防空转)④ 失败消息含 `id` + `key` + 期望 + 实际;⛔ **禁**「找到才断言」形态(反例:`test_cep_ualink_engine.cc:345-362` 的循环**从不断言自己找到了 `ualink_4`** ⇒ 实例被改名即整段静默空转、测试仍绿)
- [ ] **AC-04** **形状联判**:每实例 `flow_support` 键集恰好 10 且与 `kCanonicalFlowNames`(代码真源)逐名一致;⚠️ **事实订正**:`test_cep_schema.cc:265-275` 锁的是 **schema 的 `properties.flow_support`**(⛔ **不是**任何实例)⇒ **实例级** 10 键形状由本 change **新增**;唯一既有实例级判据是 `test_cep_moesi_gpu_adapter.cc:312-315`(仅 `moesi_gpu`)⇒ **「禁双实现」在此不适用**,流名唯一来源取 `kCanonicalFlowNames`
- [ ] **AC-05** **持久反向 fixture 必红**(仓内铁律 2):`bash scripts/test/cep_flow_support_reverse_verify.sh` ⇒ 毒化态**必红** + 未毒化态**必绿**;脚本**只写临时目录** ⇒ **可注册进 ctest** 并行安全;且该 fixture **不依赖** C++ 用例仍在(删掉用例后 fixture 仍红)
- [ ] **AC-06** **实施期零改动**:`include/tlm/cep/**` 零 diff · `protocol_instances.json` 零 diff · `protocol_instance.schema.json` 零 diff · 引擎 / adapter / tracker 零 diff;新增 invariant 数 = **0**
- [ ] **AC-07** **不勾选 `ADR-CEP-12` 的任何 Verification 项**(其 `## Verification` 六条判据**无**「逐实例 `flow_support` 值」对应项;V-12-5 是 `NullProtocolEngine` 的 10 流,⛔ 不得顺手勾掉并宣称有牙)
- [ ] **AC-08** `openspec validate cpptlm-cep-flow-support-assertions --strict` **rc=0**(零 WARNING)
- [ ] **AC-09** `./build/bin/cpptlm_tests '[cep]'` **0 failures**(**不固化用例数/断言数** —— per AGENTS.md「实时计数用 `--list-tags` 取」);本 change **用例数 +1**
- [ ] **AC-10** **归档期五步链**(delta 合并进 `openspec/specs/cep-core-contracts/spec.md` ⇒ 该文件在 M1 冻结面):
  - ① 授权 Entry 必须是**归档期**那条,且 id 与台账 `### Entry #N` 标题**精确相等**(`cep_freeze_check.sh:131-138`;`--authorized-by "a"` 伪造已被堵死)
  - ② `--update` 是**全量重算** ⇒ 更新后必须 `git diff` 清单、确认**只有** `cep-core-contracts/spec.md` 的 hash 变化(否则静默吸收无关漂移)
  - ③ 归档**禁止 `--skip-specs`**(本 change 是「✅ 已完成归档」语义 ⇒ 必须合并 delta)
  - ④ **时序**:归档前冻结闸**必绿**(delta 在 `openspec/changes/<name>/specs/...`,冻结 glob **刻意不匹配**该路径,per `cep_freeze_check.sh:73-77`)→ 归档合并后**必红** → 才走五步链 ⇒ **不得**拿「归档前 rc=0」当已合规
  - ⑤ `artifact_count` **不变**(现 86;本 change 不新增冻结文件,只改 1 个 hash)
  - ⑥ 「`frozen_at_commit` ≠ HEAD」是**常态告警**(清单 commit 落后 HEAD 一个 commit)⇒ ⛔ **不得**为消除该告警重复 `--update`

## Capabilities

**无新增 capability。** 本 change 只给**既有声明契约**补值层机器锁 —— 契约文本从「`flow_support` 形状已锁」扩到「**形状 + repo_owned 值层的变更检测义务已锁**」;**不引入新能力面**、**不新增 invariant**、**不赋予本仓对镜像值的权威**。

## Impact

| 面 | 影响 |
|---|---|
| 代码 | `test/test_cep_instance_declaration_contract.cc`(改 1 处)+ `test/cep_flow_support_expected.json`(新)+ `scripts/test/cep_flow_support_reverse_verify.sh`(新) |
| 声明源 / schema / 引擎 / adapter / tracker | **零改动** |
| 契约面 | 归档时 `openspec/specs/cep-core-contracts/spec.md` 增 1 个 Requirement + 改 1 个 Requirement(**冻结面** ⇒ 归档步走五步链,per AC-10) |
| 台账 | 1 条 Entry(planner 域,append-only) |
| KPI | 占 **1/3**(Proposed 并发 0/3 → 1/3) |
| 风险 | **低** —— 期望表与声明源不符会**立即红**(这正是设计目的);持久 fixture 证明判据有牙 |
| 回归 | `[cep]` 全量 0 failures;`ctest -R cep` 全量 0 failures |

> ⚠️ **残余登记(不在本 change 范围,仅登记 ⇒ 走**台账 §11 Entry**,⛔ **不使用 `backlog:` 伪机制**—— 仓内**无**该机制,台账 Entry 与 `.rddf/improvements/` 才是可检索载体)**:
> ① **镜像 20 值的 SKIP 条件性洞** —— `moesi_gpu` / `mesif` 的 `flow_support` 由 ⑥ `strict_equal` 逐字段覆盖 + 持久反向 fixture 覆盖,**但 ArchForge 仓缺席时 `cep_decl_alignment_check.sh:40-44` 整闸 exit 0** ⇒ **无 ArchForge 环境里那 20 个值零保护**。⛔ 本 change **不覆盖**它们(权威不在本仓;覆盖 = 本仓断言自己已声明不拥有的权威)。⚠️ 处置需 **architect 裁决**(是否把镜像值也纳入仓内权威、或要求 ArchForge 仓在 CI 中常驻)⇒ **登记为台账 Entry,待 architect**。
> ② **`cxl_2_0` / `tilelink_c` / `nvlink_4` 无逐值出处** —— 期望表中显式标注「无逐值出处(冻结口径)」;⛔ **不伪造**出处,⛔ 不用笼统背书。
> ③ **Scenario 标题债(工具约束,本 change 产生)** —— `openspec` 的 `MODIFIED` 块按**场景名精确匹配**且 archive **拒绝丢弃**既有场景名(`requirement-blocks.js:453-480` + `validator.js:671-680`);`ADDED`+`REMOVED` 同名被拒(`specs-apply.js:200-203`)⇒ **含数量的 Scenario 标题一经落盘即无法经 delta 改名**。⇒ 本 change 的 delta **保留**标题「5 实现 + caps 四属性齐备」并在正文加**显式免责句**(实现数 = 6,以正文为准)。⚠️ 根因是「标题内嵌数量」这一写法 ⇒ **后续新增 Scenario SHALL NOT 在标题内嵌数量**(已写入 delta 的 Requirement 说明)。

## Non-Goals

- ⛔ 不把「声明值 ↔ 引擎行为」的一致性纳入本 change(档 2 观测面,另立项)
- ⛔ 不为 `family` 族标签、`perm`、`vc_binding` 等补断言(另立项)
- ⛔ 不改 `protocol_instances.json` 的任何值 —— 即使测试发现漂移,处置是**改声明源并走 ADR / 台账**,不是改期望表迁就
- ⛔ 不改本仓对镜像实例(`moesi_gpu` / `mesif`)的权威分区
