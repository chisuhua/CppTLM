# proposal: cpptlm-cep-flow-support-assertions —— repo_owned 实例 `flow_support` 值层的机器锁

> **状态**: 🟡 Proposed（2026-11-15 立项 · planner 起草 · 待 builder 补 design/tasks 并实施）
> **依据**: improvement `.rddf/improvements/cpptlm-cep-flow-support-assertions.md`（v2 · metis 第 1 轮 14 条 MUST/SHOULD-FIX 逐条处置）· `ADR-CEP-02 §2`（canonical 10 流）· `ADR-CEP-12 §13`（权威分区）· `ADR-CEP-04 §13`（第 6 种 SharerTracker 追加式修订）· 台账 **Entry #427**
> **KPI**: 立项后 Proposed **0/3 → 1/3**

## 为什么

`flow_support` 是「协议级能力声明」的机器可读面，且被**运行时判据实际消费**：`include/tlm/cep/mesif_protocol_engine.hh:690-695` 的 `gate(CanonicalFlow f)` 以「`flow_support = false` ⇒ 拒（不静默降级）」实现（`ualink_protocol_engine.hh` 同构）。

**⚠️ 如实分层（按权威归属，⛔ 不夸大）**：

| 权威 | 实例 | 值数 | 现有保护 | 缺口 |
|---|---|:--:|---|:--:|
| **ArchForge 镜像** | `moesi_gpu` / `mesif` | 20 | ⑥ `strict_equal` 逐字段（`cep_decl_alignment_check.sh:523-551`）+ 持久反向 fixture（`cep_decl_reverse_verify.sh:183` 穷举 12 键） | 0 —— **但 ArchForge 缺席时整闸 exit 0 ⇒ 条件性保护**（见「不做什么」①） |
| **本仓权威**<br>（`repo_owned_instance_ids`） | `chi_rnf` | 10 | — | **10** |
| | `cxl_2_0` | 10 | 仅 `back_invalidate=false`（`test_cep_cxl_dcoh_instance_decl.cc:117`） | **9** |
| | `tilelink_c` | 10 | — | **10** |
| | `nvlink_4` | 10 | 仅 **key 存在性**文本扫描（`test_cep_nvlink_sharp.cc:277-293`，扫的是 13 个顶层键） | **10** |
| | `cxl_3_0_symmetric` | 10 | 10/10（`test_cep_cxl_dcoh_instance_decl.cc:113` + `:121-129`） | 0 |
| | `ualink_4` | 10 | 10/10（`test_cep_ualink_engine.cc:349-357`） | 0 |
| | **小计** | **60** | 21 | **39** |

⇒ **把 `chi_rnf` 的 `address_resolve` 从 `true` 改成 `false`**（或反之）**不会被任何测试发现** —— 而该字段恰是 `gate()` 的「声明 false ⇒ 拒」的输入 ⇒ **声明漂移会静默收窄/放宽运行时能力，且 CI 全绿**。

### 第 2 个动因：同一 delta 落点上的冻结契约互斥

`openspec/specs/cep-core-contracts/spec.md` 是 M1 冻结产物（`cep_freeze_check.sh` 的 `openspec/specs/cep-*/spec.md` glob）。其 `cep-sharer-tracker-contract` Requirement 仍写「**5 种实现**」（`:83` / `:88` 的 5 项列表 / `:96` 的「Hazard_* 在 5 种实现间收敛」），而：

- 代码真源 `include/tlm/cep/sharer_tracker.hh:238-260` 的 `kTrackerTraits` = **6 行**（第 6 行 `HierarchicalDirectory` + `AggregationAt::HIERARCHICAL`，`:255-259`）；
- `ADR-CEP-04:391` **已**追加式修订「5 种实现 ⇒ 6 种实现」（正文不改）；
- `docs/architecture/cep/directory-ordering.md:22` 已写「6 行」；
- **兄弟 spec** `openspec/specs/cep-hierarchical-directory/spec.md:36-38,61` 明确以 `kTrackerTraits[5]`（第 6 行）为三方真源并要求「逐字段相等」。

⇒ **唯一陈旧载体 = 这份冻结 main spec**，且它与**同在冻结集内**的兄弟 spec **契约互斥**。本 change 的 delta 本就要写进同一文件 ⇒ **同一文件 + 同一冻结成员 + 同一条五步链** ⇒ 合并处置的边际成本 ≈ 0；拆开则需**第二条五步链**（台账要求两条链**物理串行**）⇒ **合并**（用户裁决 2026-11-15，采 metis 建议）。

### ⛔ 认识论边界（不得过度声称）

期望表是**变更检测器**，不是**独立 oracle**：值的权威仍是声明源 `include/tlm/cep/protocol_instances.json`。⇒ delta 的 Requirement 写**变更检测义务**，**不**写「值的权威归属」—— 后者需 `ADR-CEP-12` Status Update 授权（其 `## Verification` 六条判据**无**逐实例值判据），**本 change 不动已签批 ADR**。

## 变更内容

**范围 = 4 个产物 + 1 份台账 Entry**，详见 improvement 的 §What Changes。

| # | 内容 | 文件 |
|:--:|---|---|
| **W1** | **期望表**：6 个 `repo_owned` 实例 × 10 键 = **60 格**期望值（逐格字面量）+ 每实例 `source`（族级出处；无逐值出处者显式标注） | `test/cep_flow_support_expected.json`（新） |
| **W2** | **判据**：新增 1 个 TEST_CASE（per-instance `SECTION`）—— ① 期望表 ⟷ 声明源逐格比对（**双向集合相等**）② 形状联判（`kCanonicalFlowNames` 代码真源）③ `is_boolean()` + id 唯一性 ④ **内存副本反向 fixture**（逐实例轮转；形态照抄同文件 `ADR-CEP-04-SU4-O2` ③ 段）⑤ **域 = `repo_owned_instance_ids` 全集**（⛔ 不按 `status` 过滤）⑥ `load_json` 加 **env 覆盖** `CEP_FLOW_SUPPORT_DECL`（仅本文件内 helper，零外溢） | `test/test_cep_instance_declaration_contract.cc`（改） |
| **W3** | **持久反向 fixture**：复制声明源到**临时目录**并毒化 1 格（⛔ **不改仓内任何文件**）⇒ `CEP_FLOW_SUPPORT_DECL=<副本>` 跑 `build/bin/cpptlm_tests '[cep][instance-decl]'` ⇒ 必红 ⇒ 未毒化态必绿（形态满足「只写临时目录 ⇒ 并行安全」） | `scripts/test/cep_flow_support_reverse_verify.sh`（新） |
| **W4** | 注册 W3 进 ctest（仓内铁律 2：持久反向 fixture 必须在**脚本侧**且能独立存活 —— 删除 C++ 用例后 fixture 仍须红） | `test/CMakeLists.txt`（改） |
| **W5** | **delta spec**：`ADDED` Requirement「`flow_support` 值层的变更检测义务」+ 4 Scenario；`MODIFIED` Requirement「`cep-sharer-tracker-contract`」实现数 5 ⇒ 6（⚠️ **Scenario 标题保留**「5 实现 + caps 四属性齐备」+ 正文加**显式免责句** —— `openspec` 的 `MODIFIED` 按场景名精确匹配且 archive 拒绝丢弃既有场景名 ⇒ **含数量的标题一经落盘无法经 delta 改名**；`ADDED`+`REMOVED` 同名被拒 ⇒ 「标题 + 免责句」是机械可行的最優形态，且**严格优于拆出**（拆出则 main spec 仍写「恰好 5 个实现」= 明错） | `specs/cep-core-contracts/spec.md`（新） |
| **W6** | 台账 Entry（planner 域，append-only）+ 本轮残余登记（② 项） | `.rddf/roadmap/objectives/obj-cep-platform.md` |

**⛔ 边界**：`include/tlm/cep/**` **零 diff** · `protocol_instances.json` **零 diff** · `protocol_instance.schema.json` **零 diff** · 引擎 / adapter / tracker **零 diff** · 新增 invariant = **0** · **不断言镜像实例**（`moesi_gpu` / `mesif`）的 `flow_support` 值 · **不勾选 `ADR-CEP-12` 的任何 Verification 项** · **不改任何已签批 ADR**（只引用 `ADR-CEP-04 §13` 的追加式修订）。

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | 期望表 = **6 实例 × 10 键 = 60 格**；格数**可机器验收**（`REQUIRE(表实例数 == 6)` + per-instance `SECTION` + 期望值为**字面量常量**）；⚠️ 「字面量而非派生」是源码属性 ⇒ 用**编译期常量计数**把它变成可验收项 |
| **AC-02** | 每实例 `source` **可机器验收**：所引文件存在 + 锚点串存在（复用 `test/cep_adr_parser.hh` 的 `read_file`）；`cxl_2_0` / `tilelink_c` / `nvlink_4` 显式标「无逐值出处(冻结口径)」；⛔ **禁**「ADR-CEP-12 能力映射」笼统背书 |
| **AC-03** | **反向控制机器化四要件**：① 逐实例 `SECTION` ② `REQUIRE(被变异实例数 == 6)` ③ `REQUIRE(翻转值 != 原值)` ④ 失败消息含 id + key + 期望 + 实际；⛔ **禁**「找到才断言」形态（反例 `test_cep_ualink_engine.cc:345-362`） |
| **AC-04** | **形状联判**：每实例 `flow_support` 恰 10 键且与 `kCanonicalFlowNames` 逐名一致；⚠️ **事实订正**：`test_cep_schema.cc:265-275` 锁的是 **schema 的 `properties.flow_support`**（⛔ **不是**任何实例）⇒ **实例级** 10 键形状由本 change **新增** ⇒ 「禁双实现」在此**不适用** |
| **AC-05** | **持久反向 fixture 必红**（仓内铁律 2）：`bash scripts/test/cep_flow_support_reverse_verify.sh` ⇒ 毒化态**必红** + 未毒化态**必绿**；脚本**只写临时目录** ⇒ 可进 ctest 并行；且**不依赖** C++ 用例仍在 |
| **AC-06** | **实施期零改动**：`include/tlm/cep/**` · `protocol_instances.json` · `protocol_instance.schema.json` · 引擎 / adapter / tracker 全零 diff；新增 invariant = **0** |
| **AC-07** | **不勾选 `ADR-CEP-12` 任何 Verification 项**（其 6 条判据无「逐实例 `flow_support` 值」对应项；V-12-5 是 `NullProtocolEngine` 的 10 流） |
| **AC-08** | `openspec validate cpptlm-cep-flow-support-assertions --strict` **rc=0**（零 WARNING） |
| **AC-09** | `./build/bin/cpptlm_tests '[cep]'` **0 failures**（**不固化用例数/断言数** —— per AGENTS.md「实时计数用 `--list-tags` 取」）；本 change **用例数 +1** |
| **AC-10** | **归档期五步链**：① 授权 Entry 必须是**归档期**那条且 id 与台账 `### Entry #N` **精确相等**（`cep_freeze_check.sh:131-138`）② `--update` **全量重算** ⇒ 复核清单 diff **只有** `cep-core-contracts/spec.md` 的 hash 变化 ③ 归档**禁 `--skip-specs`**（本 change 是「✅ 已完成归档」语义）④ **时序**：归档前冻结闸**必绿**（delta 在 `openspec/changes/<name>/specs/...`，冻结 glob 刻意不匹配该路径）→ 合并后**必红** → 才走五步链 ⇒ **不得**拿「归档前 rc=0」当已合规 ⑤ `artifact_count` **不变**（现 86）⑥ 「`frozen_at_commit` ≠ HEAD」是**常态告警** ⇒ ⛔ **不得**重复 `--update` |

## 不做什么

| 排除项 | 理由 |
|---|---|
| **断言镜像实例**（`moesi_gpu` / `mesif`）的 `flow_support` 值 | 其权威在 ArchForge ⇒ 本仓断言自己已声明不拥有的权威 = 违反权威分区；且上游每次合法刷新都要改本仓表 |
| **镜像 20 值的 SKIP 条件性洞**（ArchForge 缺席 ⇒ `cep_decl_alignment_check.sh:40-44` 整闸 exit 0 ⇒ 零保护） | ⚠️ **登记为台账 Entry，待 architect 裁决**（是否把镜像值纳入仓内权威 / 或要求 ArchForge 仓在 CI 中常驻）—— 判权在 architect，⛔ planner 不代裁 |
| **声明值 ↔ 引擎行为**的一致性 | 那是档 2 协议级观测面（`ADR-CEP-20` 决策 5/6）⇒ 另立项（本 change 是其**前置**：声明值被锁后才有稳定基线） |
| 改 `protocol_instances.json` 的任何值 | 即使测试发现漂移，处置是**改声明源并走 ADR / 台账**，不是改期望表迁就 |
| `family` / `perm` / `vc_binding` / `ordering_assumption` 等其余 12 个顶层属性的断言面 | 另立项 |
| 改任何已签批 ADR | 只**引用** `ADR-CEP-04 §13` 的「6 种实现」追加式修订 |
| `cxl_2_0` / `tilelink_c` / `nvlink_4` 的**逐值出处**补齐 | 仓内确无逐值出处 ⇒ 期望表显式标「无逐值出处(冻结口径)」，⛔ **不伪造**出处 |
