# 验收报告 · `cpptlm-cep-invariant-modeling-batch2`（invariant 建模批 2 · 登记册 I-6）

- **角色**：builder（**实施侧自验** —— 同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **日期**：2026-11-21 · **关联单元**：登记册 §6.4 队列 2 **I-6**
- **上游裁决**：`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二（oracle **APPROVE** + critic **APPROVE_WITH_CONDITIONS** · 无 BLOCKER）· **用户 ratify R-2a~R-2f**
- **前置**：Q-4 已裁 ✅ · litmus change 已归档（**I-6a**）✅ · Q-2 已裁 ✅ · `obj §0 DoD 7` 措辞已订正 ✅

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单 | `openspec/changes/archive/2026-11-21-cpptlm-cep-invariant-modeling-batch2/proposal.md` §验收标准（AC-1 ~ AC-14）|
| 形态来源 | 裁决记录 §二（D-1~D-5 · R-2a~R-2f · P-1~P-9 禁止项）|
| 设计取舍 | 同 change `design.md` D-1 ~ D-8 |
| 上游 | `.rddf/improvements/cep-invariant-modeling-batch2.md` · 登记册 §6.4 I-6 · §6.2 S1-1 |

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| **AC-1** | ① deferrals 文件仍在 ∧ `deferrals == []` ② `--check` rc=0 ∧ 含「66 项有牙载体」∧ `⏭-unmodelled` == 0 ∧ `⏭-pending` == 0 ③ `--generate` 后 `--check` 绿 ∧ `git diff --name-only` 含映射表 ④ 台账锚独立闸绿 + 反 fixture | **通过** | ① `test/cep_invariant_deferrals.json` 存在且 `deferrals == []`（从 2 条删除）② `python3 scripts/test/cep_invariant_map_check.py --check` ⇒ **`✓ 映射表闸通过: 66/66 = 66 项有牙载体 + 0 项 ⏭-unmodelled + 0 项 ⏭-pending`** ③ `--generate` 输出 `66 个 id 有载体标记` ⇒ 立即 `--check` 绿；`test/cep_invariant_assertion_map.json` 在变更集内 ④ `python3 scripts/test/cep_invariant_batch2_ledger_check.py` ⇒ `✓ Entry #514 含 [IC1, IC3] + 3 个载体落点，且无夸大表述` rc=0；**反 fixture**：把 Entry #514 段内 IC1 的**全部 4 处**改为 IC9 ⇒ 回落 Entry #512 但缺本批载体落点 ⇒ **红**（`✗ …= #512，但它不含本批载体落点 […]`）⇒ 还原 ⇒ 绿（详见 §决定性变异 V-6）|
| **AC-2** | 映射表 `carriers` **dict 键数** == 66 ∧ `INV-CEP-04-IC4` 不在其中；⛔ 不得读作「全部可判」 | **通过** | `carriers` dict 键数 = **66**（原 64 + 新载体 2）· `"file"` 条目数 = 68（AC-2 已预告该差值）· `INV-CEP-04-IC4` **不在** carriers（永久跳空）· 台账 Entry #514 与登记册 §6.2 S1-1 **均写明**「只能读作 66 条全部**有载体**」⛔ 不得读作「每条都已给出判定结论」 |
| **AC-3** | `properties` 13→14 · `required` 仍 13 · 全套闸绿；**反 fixture 两条** | **通过** | schema 实测 `properties = 14`（末键 `combination_contract`）· `required = 13`（不含该键）· schema diff **+58 / −0**（既有行零触碰）。**反 fixture ①**（把第 14 项加进 `required`）⇒ 按签名红：`test_cep_instance_declaration_contract.cc:105: REQUIRE( required.size() == 13u )` FAILED（另 3 处 per-family 同红）⇒ 还原绿。**反 fixture ②**（删 `combination_contract` ⇒ properties 回 13）⇒ 红：`test_cep_schema.cc:441: REQUIRE( props.size() == 14 )` FAILED + `:257` FAILED ⇒ 还原绿。⛔ **禁改三处已遵守**：`test_cep_instance_declaration_contract.cc:105` / `:114` 与 `test_cep_schema.cc:257` **零改动**（`:257` 保留 `required.size() == groups.size()`，`groups` 仍 13 项）|
| **AC-4** | 4 个断言点 / 2 个用例标题改形；`:401` / `:454` / `:455` 零改动且保持绿 | **通过** | ① `:234` 标题 → 「schema 必含 13 个顶层属性 **+ 1 个可选声明项**」，新增 `optional = {"combination_contract"}` 列表 + `props.size() == groups.size() + optional.size()` + 「可选项 ⛔ 不得进 `required`」断言 ② `:418` 标题 → 「7 组 / 13 个**必含**属性 + 1 个**可选**属性」，`:423` → `props.size() == 14` ③ `:447` → **三条**（`union_ids == required_ids` ∧ `props.size() - union_ids.size() == 1` ∧ `props.contains("combination_contract")`）④ `:401`（caps 恰 3 维）· `:454`（ADR 含「13 个顶层属性」）· `:455`（ADR **不含**旧字段组数串）**零改动**且 `[cep][schema]` 全绿（260 assertions / 19 cases）；实测 ADR-CEP-12 的旧字段组数串计数 = **0**、`7 个字段组` = 5、`13 个顶层属性` = 7 ⇒ `:453`/`:454`/`:455` 三向一致 |
| **AC-5** | ① `artifact_count` == 90 不变 ② `--update` 执行一次 ③ 清单单独 commit ④ 无参复验 rc=0 ⑤ 清单 diff 只改 hash/provenance **⑥ Entry 编号唯一性 == 1** | **通过（⑤ 有 1 处显式偏离，见 F-1）** | ① `artifact_count` = **90**（本单元**不新增**冻结成员：delta 改的是既有 `cep-core-contracts/spec.md`；归档目录只匹配 glob #18 的 umbrella 形态 ⇒ 不入集）②③④⑥ 见 §五步链执行记录 ⑤ **4 个 hash**（schema + `ADR-CEP-12` + `ADR-CEP-13` + `cep-core-contracts/spec.md`）—— ⚠️ 非 AC-5⑤ 字面的 3 个，见 F-1 |
| **AC-6** | 两份 ADR **只增行**（既有行删除 == 0） | **通过** | `ADR-CEP-12` **+68 / −0** · `ADR-CEP-13` **+38 / −0**；`git diff` 中 `^-.*INV-CEP-12-IC1` 命中 **0**（IC1 行零改动）· `python3 scripts/test/cep_adr_structural_check.py` ⇒ `✅ ADR 结构闸: 全部通过`；两份 SU 均在**文末**、均为 `## Status Update(...)`（匹配结构闸的 `SU_RE`）|
| **AC-7** | `INV-CEP-12-IC1` **只联动不改语义** | **通过** | `git diff --stat -- test/test_cep_instance_declaration_contract.cc` ⇒ **空**（IC1 机器载体零改动）· ADR 的 IC1 行删除数 == **0** |
| **AC-8** | 范围纪律 + 镜像禁改 + golden 零改动 | **通过** | 逐路径命中数实测**全 0**：`include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/architecture/` · `test/cep_instance_golden.json` · `docs/designs/`。`protocol_instances.json` 的 diff **+30 / −0**（只给 6 个权威域实例加属性；**`moesi_gpu` / `mesif` 零改动**）|
| **AC-9** | 逐实例声明 + `unknown` 计数机器化 + **反 fixture 三条** | **通过** | 新载体断言：域双向集合相等（`domain == fixture_ids`，`size == 6`）· 逐实例 `early_ack` == 期望表 · `unknown_count == fixture.unknown_count`（**6**）· 权威域 = `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `nvlink_4` / `tilelink_c` / `ualink_4` **全部 `unknown`**。**反 fixture 三条**（详见 §决定性变异 V-2/V-3/V-4）全部按签名红 ⇒ 还原绿 |
| **AC-10** | 条件判据 + **合成实例**正反三例 | **通过** | 载体断言：`valid({adopted:true, edge_classes:[]})` **假**（且先 `REQUIRE(eval_schema(...))` 通过 ⇒ 证明 **schema 层放行、只有条件规则能抓**）· `valid({adopted:true, verification_scope:""})` **假**（schema 的 `minLength` 与条件层双抓）· `valid({adopted:false})` **真**。**变异 V-5**：把 `conditional_ok` 短路为恒真 ⇒ **重建**后 `:291 REQUIRE_FALSE( valid_compound(...) )` 红 ⇒ 条件规则**非空跑** ⇒ 还原绿 |
| **AC-11** | 咬合：每条载体断言**针对该 id 语义** | **通过** | `test/test_cep_combination_contract.cc` 两个 `INV-CARRIER` 标记各自邻域（< ±8 行）内是**该 id 的实质断言**：IC3 ⇒ 枚举恰 3 值 ∧ 不含 `not-applicable` ∧ `bridging.required == ["early_ack"]` ∧ 6 实例逐个声明；IC1 ⇒ `compound_consistency.required == ["adopted"]` ∧ `edge_classes` 取值域恰 `{rfe,fr,ws}` ∧ 条件规则三例。⛔ 无 `REQUIRE(true)` 凑数；载体为 `.cc`（`test/` 下）|
| **AC-12** | litmus 语料**保持为空** + 两臂 rc=0 | **通过** | `test/cep_litmus_corpus.json` 的 `compound_edges` 仍 `[]`（**载体自身也断言**：`REQUIRE(corpus.at("compound_edges").empty())`）· `_fill_when` 已改为「**待操作级模型落地**」+「**通道图 ≠ 操作图**」+ ⛔ 禁派生填充 · `litmus_falsifier.hh` 头注释同步订正（**「加声明项不会翻转闸臂①」** 明写）· `bash scripts/test/cep_litmus_falsification_check.sh` 见 §回归记录 |
| **AC-13** | 诚实边界 · 禁夸大 | **通过** | 禁述集合的 grep 只作用于**机器产物**（台账 Entry 断言句 + 新载体源码），⛔ 不 grep ADR SU / improvement（自指）；台账 Entry #514 明写「声明 ≠ 判定」+「⛔ 不得表述为『公理已成立 / 已被检验』」；本报告与登记册均按该口径表述 |
| **AC-14** | 0 回归 · 禁 SKIP 冒充绿 | **通过** | 见 §回归记录（ctest 全绿 · 全部 `cep_*.sh` 逐个 rc=0 且**无 `[SKIP]`** · ADR 结构闸 / 机关 / 文档闸全 rc=0 · 工作区干净）|

## 决定性变异（6 条 · 全部有鉴别力证据）

| # | 变异 | 期望 | 实测红因（签名）| 判别力 |
|:--:|---|---|---|:--:|
| **V-1** | schema：把第 14 项**加进 `required`** | 按签名红 | `test_cep_instance_declaration_contract.cc:105: REQUIRE( required.size() == 13u )` FAILED（+ 3 处 per-family 同型）| ✅ |
| **V-2** | schema：**删 `combination_contract`**（properties 回 13）| 按签名红 | `test_cep_schema.cc:441: REQUIRE( props.size() == 14 )` FAILED ∧ `:257` FAILED | ✅ |
| **V-3** | 删 `chi_rnf` 的 `bridging.early_ack` | 红 | `test_cep_combination_contract.cc:240: REQUIRE( br.contains("early_ack") )` FAILED（INFO：`实例缺 combination_contract.bridging.early_ack: chi_rnf`）| ✅ |
| **V-4** | 改 fixture 的 `chi_rnf` 期望值（→ `allow-early-write-ack`）| 红 | `:243: REQUIRE( got == table.at("instances")… )` FAILED（INFO：`实例的 early_ack 与期望表不一致: chi_rnf`）| ✅ |
| **V-5** | `conditional_ok` **短路为恒真**（AC-10 防空跑）| 红 | `:291: REQUIRE_FALSE( valid_compound(csc, c1, why) )` FAILED | ✅ |
| **V-6** | 台账 Entry #514 段内 IC1 **全部 4 处** → IC9 | 红 | 台账锚闸回落到 `Entry #512`（历史批 1 段仍有该 id）但**缺本批载体落点** ⇒ `✗ …= #512，但它不含本批载体落点 […]` | ✅ |

⚠️ **V-5 的迭代**：首次注入变异后**未重建** ⇒ 测试仍绿（假阴性）⇒ 补 `cmake --build` 后复现红 ⇒ **还原 + 重建**复绿。该迭代已计入 §已用轮次。

## 失败项与路由建议

| # | 项 | 路由 |
|:--:|---|---|
| **F-1** | **AC-5⑤ 的实质偏离**：裁决 AC-5⑤ 写「清单 diff **只改 3 个 hash**」，实测为 **4 个** —— 多出 `openspec/specs/cep-core-contracts/spec.md`（本 change 的 delta spec 归档合并所致）。根因：该文件**是冻结成员**（清单 `globs` #17 + 显式成员 #19）且单元 Capabilities 明令「schema 变更属契约面 ⇒ 须在 delta spec 中体现」⇒ **两条要求不可兼得** | ⇒ 取**「schema 变更须入 delta spec」**（契约面纪律；跳过 delta 会让 IC1 Scenario 的「(= schema 实际属性数)」与 14 化字面相撞 ⇒ 留下长期绿的矛盾）。4 个 hash **全部相关** ⇒ AC-5⑤ 的**意图**（防 `--update` 顺手吸收**无关**漂移）**仍满足**。已在 change 的 `design.md` D-6 + proposal AC-5⑤ + 登记册 I-6 行**三处显式登记**（⛔ 未静默改判据）|
| **F-2** | **机关缺陷（本单元完成后暴露）**：`scripts/test/rddf_stage_gate.sh --self-test` 的**变异②在队尾构造不出跳序** —— 它取「当前下一项的后继」标 done，但 I-6 完成后下一项 = **I-7（队尾）** ⇒ 无后继 ⇒ 什么也不改 ⇒ 误报「P-2a 无鉴别力」 | ⇒ **已修自测构造**（⛔ **未动 P-2a 判据**）：队尾时反向构造（把 `nxt` 标 done **且**选其前驱改回 pending）⇒ 自测绿（`✓ 变异② (I-7 I-6 标 done 而前驱未 done)`）⇒ 这是**机关**（`scripts/test/`，⛔ 非治理面）的缺陷修复 |
| **F-3** | **行号漂移**：AC-4 的锚点是现状行号（`:234` / `:418` / `:423` / `:447`），首次编辑（`:234` 段）后其后的行号 +19 ⇒ 最终落点 = `:234` / `:433` / `:438` / `:462` | ⇒ 判据按**内容**判定（非行号）⇒ 已逐条核对；⛔ 不改判据 |
| **F-4** | 本 change **无独立 verifier 环节**（同一 builder 会话实施 + 自验） | ⇒ 已在报告头显式标注 |
| **F-5** | 归档合并的**格式变动**（`## Purpose`/`## Requirements` 后插空行、Requirement 间 `---` 被移除、文件末空行被移除）| ⇒ openspec 合并器**固有行为**（I-11/I-12 已登记）⇒ ⛔ 非回归 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 临时目录 | ✅ `/tmp/opencode/` 下的 `i6_*` / `s.bak` / `i.bak` / `f.bak` / `c.bak` 全部为 `/tmp` 备份 ⇒ 收口时删除 |
| 临时诊断代码 | ✅ 无（V-5 的变异**已还原 + 重建**：`git diff` 对 `test/test_cep_combination_contract.cc` 无残留在变异态）|
| 变异残留 | ✅ 6 条变异全部还原；schema / instances / fixture / ledger 的还原均已复验（schema diff = +58/−0、instances = +30/−0、台账 = +30/−0）|
| 范围纪律 | ✅ 零 `sharer_tracker.hh` · 零 `protocol_engine_base.hh` · 零 `docs/architecture/**` · 零 golden · 零镜像域实例 |
| 新归档目录的闸可见性 | ✅ **force-add**（4 份 md）+ 基线 `32 → 33` |
| 载体表 / 登记册 | ✅ `rddf_ac_carrier.tsv` 的 **I-6 → done**；机关三路径全 rc=0，`P-2a` 下一项 = **I-7**；登记册 §6.2 S1-1 + §6.4 I-6 回填 |
| 半截状态 | ✅ 无（F-1 的偏离已显式登记；⛔ 不假装「零偏离」）|

## 五步链执行记录（AC-5）

| 步 | 动作 | 实测 |
|:--:|---|---|
| 1 | 台账 Entry **#514** 落库（授权范围 = schema 可选属性扩展 + 两份 ADR 追加 SU）| ✅ Entry #514 **恰 1 处**（AC-5⑥ 唯一性 grep == 1）· 为当前**最大**编号 |
| 2 | 改 schema + 实例 + 测试 + 两份 ADR SU + 新载体 + fixture + 闸 + deferrals + 映射表 + 语料 + CMakeLists | ✅ |
| 3 | **commit A** = `bfc053bb`（25 files · +1147/−45）| ✅ 含台账 + 全部改动 + 清单首次 `--update` 结果 |
| 4 | `--update --authorized-by "Entry #514"`（**第 2 次**）| ✅ `hash 变更 0` · `frozen_at_commit` `c6535358…` → **`bfc053bb…`**（= 产物 commit）|
| 5 | **commit B** = `ee0e5003`（**清单单独 commit** · 1 file · +1/−1 · ⛔ 与台账不合并）| ✅ |
| 6 | 无参复验 | ✅ `provenance 自检通过：90 个产物的 hash 与 commit bfc053bb 的内容一致` |

**`--update` 第 1 次的差集（AC-5⑤ 的「无关漂移」检）**：`新增 0 · 移出 0 · hash 变更 4` ——
4 个 = `schema` + `ADR-CEP-12` + `ADR-CEP-13` + **`openspec/specs/cep-core-contracts/spec.md`**（F-1）。

⚠️ **commit A 时 3 个闸红 —— 全部是「比已提交态」的提交前假象**：

| 闸 | 提交前红因 | commit A 后 |
|---|---|---|
| `cep_freeze_check` | `frozen_at_commit` = 上一 commit ⇒ 4 个产物 hash 与其内容不一致（provenance 滞后）| 第 2 次 `--update` ⇒ **绿** |
| `cep_freeze_reverse_verify` | 其**基线**要求 freeze check 绿 | **绿** |
| `cep_flow_support_reverse_verify` | 末项「仓内零改动复核」= `git diff --quiet -- protocol_instances.json` ⇒ 本单元**合法**改动该文件且未提交 | **绿** |

⇒ 先例 = I-6a 的 `ab7b649a` → `51a7babd`（同型）；三者 commit A 后**已全部复验为绿**（见 §回归记录）。

## 回归记录

| 项 | 结果 |
|---|---|
| `./build/bin/cpptlm_tests "[cep]"` | ✅ **12548 assertions / 548 cases** |
| `[cep][schema]` / `[cep][instance-decl]` / `[cep][litmus]` / `[cep][combination-contract]` | ✅ 260/19 · 809/16 · 30/4 · **70/2** |
| `python3 scripts/test/cep_invariant_map_check.py --check` | ✅ `66/66 = 66 项有牙载体 + 0 ⏭-unmodelled + 0 ⏭-pending` |
| `python3 scripts/test/cep_invariant_batch2_ledger_check.py` | ✅ rc=0（新闸）|
| `python3 scripts/test/cep_invariant_batch1_ledger_check.py` | ✅ rc=0（批 1 未回归）|
| `python3 scripts/test/cep_adr_structural_check.py` | ✅ `全部通过` |
| `openspec validate --specs --strict --json` | ✅ **138 items / 55 invalid / 47 `cep-` / `cep-` invalid = 0** ⇒ 与基线**逐项一致** |
| `bash scripts/test/rddf_stage_gate.sh`（默认 / `--phase queue2` / `--self-test`）| ✅ 三路径全 rc=0（`--self-test` 含**新修队尾变异** `✓ 变异② (I-7 I-6 标 done 而前驱未 done)`）|
| `ctest --test-dir build --output-on-failure -j4` | ✅ **198/198 passed**（0 failed · 188.10 s）—— 较基线 197 增 **1**（= 新注册的 `cep_invariant_batch2_ledger_check`）|
| 全部 `scripts/test/cep_*.sh`（逐个 rc=0 ∧ **无 `[SKIP]`**）| ✅ **0 失败 / 47 · SKIP = 0**（⚠️ 已先确认 `build/bin/cpptlm_tests` 存在 ⇒ litmus 闸不会走 SKIP 分支；实测**无任何** `[SKIP]`）|
| `bash scripts/test/cep_freeze_check.sh`（无参）| ✅ rc=0 —— **`provenance 自检通过：90 个产物的 hash 与 commit bfc053bb 的内容一致`**（`artifact_count` **90** 不变）|
| `bash scripts/test/cep_freeze_reverse_verify.sh` | ✅ rc=0（commit A 前因其**基线**要求 freeze 绿而红 ⇒ **提交前假象**，见 §五步链）|
| `bash scripts/test/cep_flow_support_reverse_verify.sh` | ✅ rc=0（commit A 前因末项 `git diff --quiet -- protocol_instances.json` 而红 ⇒ 同为**提交前假象**）|
| `bash scripts/test/archive_verify_check.sh` | ✅ rc=0（扫面 **33** · 基线 33）|
| `bash scripts/test/openspec_version_check.sh` | ✅ rc=0 |
| `git status --porcelain`（排除 `.rddf/state/`）| ✅ **空**（工作区干净）|
| ⚠️ `scripts/build/format.sh --check` | **21 个既有 C++ 文件**（与本单元无关）。⚠️ **实施中自查发现**：本单元对 `test/test_cep_schema.cc` 的编辑一度引入 **2 处**格式违规（债数 21 → 22）⇒ 已 `clang-format -i` ⇒ 债数回到 **21**；新增的 `test/test_cep_combination_contract.cc` **零违规** |

## 已用轮次

**1/3**（第 1 轮；无「验收失败 → 回 builder」的返工轮，但有 **3 次实施内自纠**：① 载体按 dict 读 `instances`（实为数组）⇒ 编译错 ⇒ 建 `by_id` 索引；② 台账锚闸的禁述正则误伤「可判定性」⇒ 收紧为 `(?!定性)`；③ V-5 变异未重建 ⇒ 假阴性 ⇒ 补构建。三次均在提交前闭合）。
