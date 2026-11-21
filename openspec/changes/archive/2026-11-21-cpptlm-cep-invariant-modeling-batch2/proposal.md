# proposal: cpptlm-cep-invariant-modeling-batch2 —— 2 条守门 invariant 的建模（批 2）

> **来源**：`.rddf/improvements/cep-invariant-modeling-batch2.md`（登记册 §6.4 队列 2 单元 **I-6**）
> **形态来源**：architect 裁决 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二（D-1 ~ D-5）· oracle **APPROVE** + critic **APPROVE_WITH_CONDITIONS**（无 BLOCKER）· **用户 ratify R-2a ~ R-2f**（2026-11-21）
> **前置**：Q-4 裁决已落 ✅ · litmus change 已归档（= **I-6a**）✅ · Q-2 已裁 ✅ · `obj §0 DoD 7` 的「100% 覆盖」措辞已订正 ✅
> **⛔ 实施侧不得自决字段名 / 取值域**（形态已由裁决给定并 ratify）。

## Why

批 1 之后，66 条守门 invariant 里**最后**的 2 条仍是 `⏭-unmodelled`（`test/cep_invariant_deferrals.json` 现恰有这 2 条）：

| id | 缺口 | 触发条件 |
|---|---|---|
| `INV-CEP-13-IC1` | 「『采用该公理为判据』**无机器可检声明项**」 | `ADR-CEP-13` 决策 5 ① 的剩余转写（组合判据成为结构化声明项）+ litmus change 立项 |
| `INV-CEP-13-IC3` | 「adapter 提前 ack 行为成为实例声明项」 | 触发 = adapter 提前 ack 行为成为声明项；代价 = **schema 13→14** + `INV-CEP-12-IC1` 联动修订 + `ADR-CEP-12` **追加式**修订 |

⇒ **不做的代价**：`⏭-unmodelled` 恒 > 0 ⇒ 「66 条全部有判定级载体」永远不成立 ⇒ 平台在 invariant 维度永久停在 64/66。

⚠️ **本单元真正的交付物 = 处置记录的「可机检性」**（schema 14 化只是载体）⇒ 目标函数是「**不引入任何新的不可判面**」⇒ 每条判据必须能**按正确原因红**（⛔ 「加了断言就算有牙」是最需防的失败模式）。

## What Changes（**9 项 · 形态已 ratify**）

| # | 变更 | 约束 |
|:--:|---|---|
| 1 | **schema `properties` 13→14** | 新增**可选**顶层属性 `combination_contract`（`bridging.early_ack` + `compound_consistency.{adopted, edge_classes, verification_scope}`）；⛔ **不进 `required`** ⇒ `INV-CEP-12-IC1` 载体零改动 · 镜像域零跨仓 · 4 处 `required.size()==13u` 断言零改动 |
| 2 | **实例声明项补齐** | **本仓权威域** = `repo_owned_instance_ids` ∪ `authority_migrations[].instance_id` ⇒ 逐个声明 `combination_contract.bridging.early_ack`；值域 3 值：`allow-early-write-ack` / `deny-early-completion` / `unknown`（⛔ 不得编值）|
| 3 | **条件判据** | `compound_consistency.adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空（schema 不写 `if/then` ⇒ 落闸判据）|
| 4 | **载体（咬合）** | `// INV-CARRIER:` 刚性标记 + 邻域 ±8 行含**针对该 id 语义**的断言（⛔ `REQUIRE(true)` 凑数不算）|
| 5 | **deferrals 移除 2 条** | `deferrals` 长度 2 → **0**（⛔ 文件**不得删除**）+ 重生成映射表；⛔ **并集闸硬约束**：`载体 ∪ ⭭ == 66` ∧ **交集为空** |
| 6 | **台账锚独立闸** | 新建 `scripts/test/cep_invariant_batch2_ledger_check.py` + `test/CMakeLists.txt` 注册（理由：映射表闸的台账锚判据**只对仍登记的 `⭭` 条目执行** ⇒ 本单元把 2 条删光后一次都不跑）|
| 7 | **两份 ADR 追加 SU** | `ADR-CEP-12`（新增声明项 + 口径澄清 + 边类语义**不定义** + 禁派生填语料）· `ADR-CEP-13`（`V-13-4`/`V-13-5` 的「机器载体不可得」判读被取代；⛔ 判据内容逐字不变）⇒ ⛔ 两份正文**只增行**；⚠️ 追加段落在**文末** |
| 8 | **litmus 语料保持为空** | `compound_edges` 仍为 **[]**；`_fill_when` 文案改为「**待操作级模型落地**」+「通道图 ≠ 操作图」说明（⛔ 禁止由声明项派生填充 ⇒ 范畴错误 ⇒ 假绿）|
| 9 | **冻结面五步链** | 台账 Entry（**以落库时实测最大值 + 1** 为准，⛔ 不预留编号）→ 改产物 → commit → `--update --authorized-by "Entry #5xx"` → **清单单独 commit** → 无参复验 rc=0 |

## Out of-Scope

- ⛔ `INV-CEP-13-IC1` 的 **litmus change 本身**（⇒ 独立 change = **I-6a**，已归档）
- ⛔ `INV-CEP-12-IC1` 的**语义**改动（**只联动，不改语义**）
- ⛔ **边类语义定义**（`rfe`/`fr`/`ws`）⇒ 归 `.rddf/improvements/cep-memory-model-operational-trace.md`
- ⛔ **镜像域实例**（`moesi_gpu` / `mesif`，权威在 ArchForge）加 `combination_contract` ⇒ 禁（golden 方向① + 镜像闸 ⑥）
- ⛔ **新增实例**（会连带红 `n == 8u` 与 `owned.size() == 6u` 断言）
- ⛔ `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/architecture/**` · `test/cep_instance_golden.json`（**含其 `_note` / `_scope_rationale` 注释**）

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **码位纪律**：deferrals / 脚本的 disposition 码位用 `⏭`(U+23ED)，⛔ **不得**用 `⭕`(U+2B55)（按字面 grep 会恒 0 命中）。

- [ ] **AC-1（处置落地 · 四小项）**：① `test/cep_invariant_deferrals.json` **文件仍存在** ∧ `deferrals == []` ② `python3 scripts/test/cep_invariant_map_check.py --check` rc=0 ∧ 输出含「**66 项有牙载体**」∧ `⏭-unmodelled` == **0** ∧ `⏭-pending` == **0** ③ `--generate` 后 `--check` 绿 ∧ `git diff --name-only` **含** `test/cep_invariant_assertion_map.json` ④ **台账锚独立闸绿**：`python3 scripts/test/cep_invariant_batch2_ledger_check.py` rc=0（判据 = **同一 Entry 段**内含 `INV-CEP-13-IC1` 与 `INV-CEP-13-IC3` **且**含本单元的载体文件路径 ∧ 该段不含「已建模 / 可判」类禁述；⚠️ **反 fixture**：把 Entry 段内任一 id 改掉 ⇒ **按签名红** ⇒ 还原绿；⚠️ **不得**用「台账全文含该 id」作判据 —— 台账现存 **13 / 9 处**含这两个 id ⇒ 全文判据**恒绿**）
- [ ] **AC-2（66/66 全有**载体** —— ⛔ 不是「终态达成」）**：`len(test/cep_invariant_assertion_map.json` 的 `carriers`) == **66**（按 **dict 键数**计，⛔ 不是 `"file"` 条目数 —— 当前后者为 68）；⚠️ `INV-CEP-04-IC4` **不在**其中。⛔ **禁止用负断言关闭 66/66** ⇒ **「66/66」只能读作「66 条全部有载体」，⛔ 不得读作「66 条全部可判 / 全部达成」**
- [ ] **AC-3（schema 形态变更的机器面）**：`properties` 由 13 增至 **14**（新增 `combination_contract`）· `required` **仍 13**（⛔ 第 14 项**不进** `required`）· `scripts/test/cep_decl_alignment_check.sh` + `test/test_cep_schema.cc` 全绿；⚠️ **反 fixture 两条**：① 把第 14 个属性加进 `required` ⇒ **按签名红**（落点 = `test/test_cep_instance_declaration_contract.cc:104-105` 的 `required.size()==13u` + `:114` 的 `from_schema == from_groups`）② 把 `properties` 改回 13 ⇒ **按签名红**（`test/test_cep_schema.cc:423`）⇒ 还原绿；⚠️ ⛔ **禁改** `test/test_cep_instance_declaration_contract.cc:105` / `:114` 与 `test/test_cep_schema.cc:257` 三处
- [ ] **AC-4（派生断言改形 · 4 个断言点 / 2 个用例标题）**：`test/test_cep_schema.cc` ① `:234` 用例（标题改「必含 13 个顶层属性 + 1 个可选声明项」· `:252` 与 `:257` **拆成两个列表**）② `:418` 用例（标题改「7 组 / 13 个必含属性 + 1 个可选属性」· `:423` ⇒ 14）③ `:447` **改写为三条**（`REQUIRE(union_ids == set(required))` ∧ `REQUIRE(props.size() - union_ids.size() == 1)` ∧ `REQUIRE(props.contains("combination_contract"))`）—— ⚠️ 单改成「分组并集 == `required` 集合」是**净损失**（原 `union_ids.size() == props.size()` 还证明了「7 组覆盖 schema 全部属性」，弱化后新属性漏登记将不再被发现）④ `:401`（`combination_capabilities` 恰 3 维）· `:454`（ADR grep）· `:455`（ADR 全文**不得**含「6 个字段组」串）**零改动 / 保持绿**；⚠️ 4 处 per-family `required.size()==13u` 与 `test/test_cep_instance_declaration_contract.cc:135` 的 `n == 8u` **零改动**
- [ ] **AC-5（冻结面五步链 ×2 + manifest 核对）**：① `cep_freeze_check.sh` 的 `artifact_count` **逐字节不变**（== **90**；`protocol_instance.schema.json` 已是冻结成员，本单元**不新增**冻结成员 ⇒ 计数不变）② `--update --authorized-by "Entry #5xx"` **执行一次** ③ 清单**单独 commit** ④ 无参复验 rc=0 ⑤ `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json` 的 diff **只改 hash / provenance 字段**（⚠️ **本单元 = 4 个 hash**：schema + 两份 `ADR-CEP-1{2,3}` + **`openspec/specs/cep-core-contracts/spec.md`** —— 末者因本 change 的 delta spec 归档合并而变 ⇒ 它**也是**冻结成员（清单 glob #17 + 显式成员 #19）⇒ **AC-5⑤ 的「只改 3 个 hash」与「schema 变更须入 delta spec」不可兼得**，本单元取**后者**并显式登记该偏离；4 个 hash **全部相关** ⇒ AC-5⑤ 的**意图**（防 `--update` 顺手吸收**无关**漂移）仍满足）⑥ 台账 Entry 编号**唯一性** grep 计数 == 1；⚠️ **台账与清单不得合并提交**
- [ ] **AC-6（ADR 追加式 · 原文零改动）**：`git diff` 对 `docs/adr/ADR-CEP-12-*.md` **与** `docs/adr/ADR-CEP-13-*.md` 的改动**只增行**（既有行 diff 行数 == **0**）；⚠️ 反 fixture：改任一既有行 ⇒ 判红；⚠️ `ADR-CEP-12:158` 的口径修订括注「(= schema 实际属性数)」属**正文**（⛔ 不可改）⇒ 只能由 SU 记为「被取代表述」；⚠️ 追加段**文末**
- [ ] **AC-7（`INV-CEP-12-IC1` 只联动不改语义）**：该 invariant 的**载体** `test/test_cep_instance_declaration_contract.cc:98-115` 的 `git diff` 行数 == **0** · `docs/adr/ADR-CEP-12-*.md:155` 的 IC1 行 `git diff` 行数 == **0**；⚠️ ⛔ **不适用**「`reason` / `trigger_condition` / `ledger_entry` 逐字未变」—— `INV-CEP-12-IC1` 是**载体型** invariant，`test/cep_invariant_deferrals.json` 里**从来没有**它的这三字段
- [ ] **AC-8（范围纪律 · 零越权 + 镜像禁改）**：`git diff --name-only` **不含** `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/architecture/**`；⚠️ **禁给镜像域实例**（`moesi_gpu` / `mesif`）或 `test/cep_instance_golden.json` 加 `combination_contract` ⇒ **golden 零改动**（连 `_note` / `_scope_rationale` 注释也不得改）；⚠️ `include/tlm/cep/protocol_instance.schema.json`（冻结面）**允许**改但须走 AC-5 五步链
- [ ] **AC-9（`early_ack` 逐实例声明 + `unknown` 计数机器化）**：**本仓权威域**的每个实例都声明 `combination_contract.bridging.early_ack`（值可为 `unknown`）⇒ 判据 = **专用 expected fixture**（新增 `test/cep_combination_contract_expected.json`，形如 `schema: cep-combination-contract-expected/v1`）· 域**双向集合相等**（`owned == fixture ids`）· 逐实例期望值 · `unknown` **精确计数**；⚠️ **反 fixture 三条**：① 删某实例的 `bridging.early_ack` ⇒ 红 ② 改 fixture 某实例期望值 ⇒ 红 ③ 域侧加一个假 id ⇒ 双向集合相等红；⚠️ `unknown_count` 字段是**变更检测器**，⛔ **不是**「未编值」的证据 ⇒ 若 6 个实例全为 `unknown`，⛔ **不得**在报告/台账里表述为「声明面已有信息量」
- [ ] **AC-10（条件判据 + 反 fixture · 防空跑）**：`adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空 ⇒ 判据 = **合成实例**（不需入库）的正反三例：`valid({adopted:true, edge_classes:[]})` 假 · `valid({adopted:true, verification_scope:""})` 假 · `valid({adopted:false})` 真（`false` 允许空）；⚠️ 现状**无任何实例必须声明 `compound_consistency`** ⇒ ⛔ **不**用仓内实例做这条反 fixture（该分支永远空跑）⇒ 只用**合成实例**
- [ ] **AC-11（咬合 · 每条载体断言**针对该 id 语义**）**：`INV-CEP-13-IC3` 的载体须断言三件：schema 的 `early_ack` 枚举**恰 3 值且不含 `not-applicable`** · `bridging.required == ["early_ack"]` · 本仓权威域实例逐个含 `combination_contract.bridging.early_ack`；`INV-CEP-13-IC1` 的载体须断言三件：`adopted==true ⇒ edge_classes 非空 ∧ verification_scope 非空` · `edge_classes ⊆ {rfe, fr, ws}` · `compound_consistency.required == ["adopted"]`；⛔ **不接受**只有 `INV-CARRIER` 标记 + 邻域宏名的形态；⚠️ 载体必须是 `.cc`（`test/` 或 `scripts/test/` 下 —— `scan_carriers` 只扫这两处），⛔ **不放** `include/`
- [ ] **AC-12（litmus 语料保持为空）**：`test/cep_litmus_corpus.json` 的 `compound_edges` 仍为 **[]**（既有机器载体：`test/test_cep_litmus_falsification.cc:118`）· `bash scripts/test/cep_litmus_falsification_check.sh` 两臂 rc=0；⛔ **不得**由 `combination_contract` 派生填充；⚠️ `litmus_falsifier.hh` 的 `reachable` **只由语料决定** ⇒ 加声明项**不会**翻转闸臂①
- [ ] **AC-13（诚实边界 · 禁夸大）**：⛔ 只 grep **机器产物**（判定产物 `report.json` / 新载体源码）与台账 Entry 的**断言句**；⛔ **不 grep** ADR SU / improvement 自身（禁述串必现 ⇒ 自指红）；禁述集合 = 「公理已成立」/「已被检验」/「已证明」；载体 = `test/test_cep_tools_contract.cc`；⚠️ **声明 ≠ 判定** —— 声明面成立 **不等于** 复合一致性公理已被检验（其判定载体归 `cep-memory-model-operational-trace`）
- [ ] **AC-14（0 回归 · 禁 SKIP 冒充绿）**：ctest 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准）逐个 rc=0 **且输出不含 `[SKIP]`**（⚠️ `scripts/test/cep_litmus_falsification_check.sh` 在缺 `build/bin/cpptlm_tests` 时打印 `[SKIP]` 并 `exit 0` ⇒ **必须先确认 `$BUILD_DIR/bin/cpptlm_tests` 存在**）· `bash scripts/test/cep_adr_structural_check.py` rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**
