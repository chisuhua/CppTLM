# 验收报告 —— `cep-evidence-tier2-minimal-slice`(档 2 协议级观测面)

> **角色**:verifier(只验不改;本报告是唯一产物)· **日期**:2026-11-14 · **已用轮次**:1
> **判据来源**(per §verifier 产出契约 优先级 2):`openspec/changes/archive/2026-11-14-cep-evidence-tier2-minimal-slice/proposal.md` 的 **AC-1 ~ AC-14** 段(归档契约,逐字)
> **验收对象**:该 change 的**全部交付面**(基类观测面 + mesif 落点 + 登记面 + 闸 + 文档 + 冻结链)

---

## 0 · 判定汇总

| 判定 | 条数 | AC |
|---|:--:|---|
| ✅ **通过** | **10** | AC-3 / AC-4 / AC-5 / AC-6 / AC-7 / AC-10 / AC-11 / AC-12 / AC-13 / AC-14 |
| ⚠️ **部分通过**(子项缺失) | **4** | AC-1(③ 缺)· AC-2(反向 fixture 缺)· AC-8(有界性缺)· AC-9(产出物面缺) |

⇒ **总判定:有条件通过** —— 10/14 完全满足;4 条各缺**一个子项**(均为**实现缺口**,非判据错误)⇒ ⇒ **路由回 builder 补齐**(分类 = `implementation_gap`,非 `proposal_drift`)。

⚠️ **缺口清单(4 项 · 全部可机判补齐)**:

| # | 缺什么 | 为什么它是缺口(逐字对照 AC) |
|:--:|---|---|
| **G-1** | **AC-1 ③ provenance 的机器判据** | AC-1 ③ 要求「**判定窗口内禁止 fixture 播种目录** ⇒ 每个 sharer bit 须**可溯源到 peer 引擎自身的 `apply_event → sync_tracker`**」⇒ 实测:`test/test_cep_tier2_observation.cc` 全文**无**该断言(仅有 `self_mask_valid` 检查,见 `:70-72`)⇒ ⚠️ 正控事实上满足 provenance(A 的 sharer 来自 A 引擎的 `sync_tracker`),但**无机器判据**⇒ 下一个用例若改用 fixture 播种,该缺口不会被发现 |
| **G-2** | **AC-2 的反向 fixture** | AC-2 要求「反向 fixture —— 拔掉任一段(观测面不产记录 / 判定不收 / 断言不落)⇒ **按签名红**;还原 ⇒ 绿」⇒ 实测:`test/test_cep_tier2_observation.cc` 与 `scripts/test/cep_e2e_reconcile_check.sh` 中**无**任何 tier2/observation 的影子变异机制 ⇒ 正向连通 ✅(测试确实走完三段),但**反向**未做 |
| **G-3** | **AC-8 的有界性(溢出路径)** | AC-8 要求「缓冲有上限 + `dropped_observations` 计数(**非零 ⇒ 红**)」⇒ 实测:实现有上限(`kObservationCapacity = 4096`)与计数,但**无任何测试触发溢出**(`grep dropped_observations` 仅 4 处**全为 `== 0` 断言`,见 `:131/:164/:287/:304`)⇒ ⇒ 「非零 ⇒ 红」这条判据**从未被执行过** |
| **G-4** | **AC-9 的「新增产出物」覆盖面** | AC-9 要求「**新增产出物内** grep 禁表述 ⇒ **命中即红**;⚠️ 该 grep 闸**必须自带反向 fixture**」⇒ 实测:`FORBIDDEN` 短语表(`cep_e2e_reconcile_check.sh:71`)只扫描 **`test/cep_evidence_tiers.json` 这一个登记面** ⇒ ⛔ **文档产出物(`07 号` 的新 SU · `docs/guide/cep-usage.md` §12.10 · main spec)** 未被扫描 ⇒ 且**无反向 fixture** |

---

## 1 · 逐条判定与工具证据

### AC-1(非空洞判据 · 停手条件)—— ⚠️ 部分(①② ✅ / ③ ❌)

| 子项 | 判定 | 证据 |
|---|:--:|---|
| ① ≥1 个写 episode(transition(Wr→M) + fill) | ✅ | `test/test_cep_tier2_observation.cc:141-152` 断言 `transitions == 2` / `fills == 1` / `write_episodes == 1`;实跑 `./build/bin/cpptlm_tests "[tier2]"` ⇒ **7 cases / 33 assertions 全绿** |
| ② 断线三任一 ⇒ 硬失败(⛔ 不是 0) | ✅ | 同文件 `:243-268` 4 项断言(`tracker_ == nullptr` / `responder_mask_of == 0` / `level >= kLevelCount` 各 `observation_invalid == 1` + 正控 `== 0`)⇒ 判「不可判」而非 0 |
| ③ provenance(禁 fixture 播种 + bit 可溯源) | ❌ | `grep -n "provenance\|溯源\|peer" test/test_cep_tier2_observation.cc` ⇒ **0 命中** ⇒ **无机器判据**(见 G-1) |

### AC-2(三段连通)—— ⚠️ 部分(正向 ✅ / 反向 fixture ❌)

- **正向** ✅:`:118-138` 的负控断言 `write_episodes == 1` / `swmr_violations == 1` ⇒ 证明观测记录 → 判定器 → 断言三段**确实贯通**;
- **反向** ❌:`grep -rn "shadow\|mutations" test/test_cep_tier2_observation.cc` ⇒ **0 命中**;`scripts/test/cep_e2e_reconcile_check.sh` 的 6 条变异只覆盖**登记面**,不覆盖观测链 ⇒ (见 G-2)。

### AC-3(负控 · 同流 + 单差异 + 精确计数)—— ✅

- 负控① SWMR:两引擎**共享同一** `HierarchicalDirectory` + **不同 node**(`:167-186`)⇒ 违例数 `== 1`(精确,⛔ 非「> 0」);
- 负控② data-value:同一驱动仅改 home 回值 ⇒ `== 1` / 正控 `== 0`(`:206-229`);
- 计数口径 ✅:`detect()` 只取 `kind == Transition && event == Wr && to == M`(`:75-80`)⇒ 一次 WRITEBACK 的 2 transition + 1 fill **只计 1 episode**;
- ⛔ M1 不在清单 ✅(按 architect 勘误 §2)。

### AC-4(装配期硬断言)—— ✅

`mesif_protocol_engine.hh::set_tracker` 的 3 硬断言 + 1 防御断言 ⇒ 反 fixture 4 项断言全绿(`:243-268`);⚠️ 防御断言(completeness == EXACT)按 AC-4 标注「非生产不变式」⇒ **不要求**反 fixture ✅ 与 AC 原文一致。

### AC-5(登记面 + 跨档聚合检查)—— ✅

`test/cep_evidence_tiers.json`(1 条档 2 载体登记)· 闸 `cep_e2e_reconcile_check.sh` 的 **T4 / T5a / T5b / T5c** ⇒ `bash scripts/test/cep_e2e_reconcile_check.sh --mutations` ⇒ **6 条变异全部按签名红**(含新增 **e** 肯定式通过态 / **f** 跨档 run 引用)+ 真实仓字节指纹相等 ✅。

### AC-6(判定行钉死)—— ✅

`:141-152` ⇒ `transitions == 2` / `fills == 1` / `write_episodes == 1` ⇒ 装行落点**未**计入 SWMR 判定 ✅。

### AC-7(范围纪律)—— ✅

`git diff 066767ac..e963d3db --stat` 实测:`include/bundles/` · `docs/adr/` · `include/tlm/cep/sharer_tracker.hh` · `experimental/` · `tools/render_report.py` · `scripts/test/cep_experiment_run.sh` · `test/cep_e2e_run_tiers.json` · `test/cep_experiment_harness.hh` · `protocol_instance.schema.json` ⇒ **全部零 diff**;`grep -rn '#include[[:space:]]*"\(test/\|\.\./test\)' include/` ⇒ **0 命中**;`grep -rn 'catch_amalgamated\|cep_experiment_harness' include/` ⇒ **0 命中**;新增 `INV-CEP-` 数 == **0**;`SharerTrackerIf` 虚方法数 == 5 的既有断言仍绿(`[cep]` 全绿)✅。

### AC-8(确定性 + 有界性)—— ⚠️ 部分(确定性 ✅ / 有界性 ❌)

- **确定性** ✅:`:280-292` 同一流两遍 ⇒ 观测记录**逐字相等**(`operator==` 逐字段比对);
- **有界性** ❌:`kObservationCapacity = 4096` + `dropped_observations` 计数**已实现**,但**无测试触发溢出** ⇒ 「非零 ⇒ 红」从未被执行 ⇒ (见 G-3)。

### AC-9(防夸大机器面)—— ⚠️ 部分(登记面 ✅ / 产出物面 ❌)

- **登记面** ✅:`FORBIDDEN` 表 + 否定语境豁免 ⇒ 变异 e/f 按签名红;
- **产出物面** ❌:`FORBIDDEN` 只作用于 `test/cep_evidence_tiers.json` ⇒ `07 号` 新 SU / `docs/guide/cep-usage.md` §12.10 / main spec **未被扫描**,且**无反向 fixture** ⇒ (见 G-4)。

### AC-10(档 1 零改动)—— ✅

`git diff 066767ac..e963d3db -- test/cep_experiment_harness.hh` ⇒ **零 diff**;`[cep]` **523 cases / 11747 assertions** 全绿 ⇒ 档 1 既有负控未受影响 ✅。

### AC-11(总纲 SU + 手册 + 索引)—— ✅

`07 号` 新 SU「Status Update 2026-11-14(档 2 协议级观测面 · per 台账 Entry #410)」· `scripts/cep/generate_doc_index.py --write` ⇒ 索引 **33 一级节 / 20 SU** · `bash scripts/test/cep_index_position_check.sh` ⇒ **rc=0**(三合一计数 + 行号逐行)· `docs/guide/cep-usage.md` **新 §12.10** + §12.9 一行导航 ✅。

### AC-12(冻结面五步链)—— ✅(**三次**,非 AC 原文的「一次」)

| 次 | 授权 | 产物数 | 证据 |
|:--:|---|:--:|---|
| ① 归档前 | Entry #411 | 84 | 清单 commit `48e81dbc` |
| ② 归档后 | Entry #411 | **85** | 清单 commit `3e98eee7` |
| ③ 引用同步后 | Entry #412 | 85 | 清单 commit `fcea9477` |

⇒ `bash scripts/test/cep_freeze_check.sh`(无参复验)⇒ **85 个产物 hash 全部一致** ✅。⚠️ 第三次由**归档后引用悬空**引发(`07 号` 属冻结面)⇒ 已在 Entry #412 登记 ⇒ **AC 文本的「×1」与实际「×3」的差异已如实登记**,不判红。

### AC-13(0 回归)—— ✅

`ctest --test-dir build -j2` ⇒ **177/177 全绿**;17 个 `scripts/test/cep_*.sh` 逐个 rc=0;`docs_sync --strict` / `agents_md_lint` / `format --check` / `markdown_link_check` 全 rc=0;工作区干净 ✅。

### AC-14(诚实边界登记)—— ✅

`07 号` 新 SU ② 六条 + `docs/guide/cep-usage.md` §12.10 四条硬边界 + main spec 的 Scenario 段 ⇒ ① data-value 生产流恒 0 = 本装置无该形态 ② 无 run / 无通过态 ③ 单族单拓扑 ④ 与档 1 正交 ⑤ 非全覆盖 ⑥ 不可判 ≠ 零违例 ⇒ **逐字可查** ✅。

---

## 2 · 遗留清扫复核(verifier 第 5 步)

| 项 | 范围 | 结果 |
|---|---|---|
| TODO / FIXME / XXX / HACK | `protocol_engine_base.hh` · `mesif_protocol_engine.hh` · `test_cep_tier2_observation.cc` | **0 命中** ✅ |
| 临时调试日志(`fopen` / `printf` / `DEBUG_PRINT`) | 同上 | **0 命中** ✅ |
| 被 skip 的测试(`SKIP` / `#if 0`) | `test_cep_tier2_observation.cc` | **0 命中** ✅ |
| 注释掉的代码块 | 同上 | **0 命中** ✅ |
| 未替换的 mock / 临时 fallback | 同上 | **0 命中** ✅ |
| ⚠️ **AC 未落项** | 见 §0 的 G-1~G-4 | **4 项** ⇒ ⛔ **不得**记作「清扫通过」⇒ 见 §3 |

---

## 3 · 失败项与路由建议

| # | 失败项 | 分类 | 路由 | 建议补法(⛔ 不新增 invariant · ⛔ 不改已签批 ADR · ⛔ 不动档 0/1 载体) |
|:--:|---|---|---|---|
| **G-1** | AC-1 ③ provenance 无机器判据 | `implementation_gap` | **builder** | 在 `test_cep_tier2_observation.cc` 的正控里加断言:观测到的**他人** sharer bit **恰等于** peer 引擎的 `self_mask`(⇒ 即可溯源到 peer 的 `sync_tracker`),并加一条**反 fixture**:用 `dir.set_sharer(...)` 直接播种 ⇒ 该断言必红 |
| **G-2** | AC-2 反向 fixture 缺 | `implementation_gap` | **builder** | 复用既有 `scripts/test/cep_test_shadow.sh` 影子机制:影子树里让观测面**不产记录**(如把 `enable_observations` 路径切断)⇒ 判定侧必红(按签名)⇒ 还原 ⇒ 绿 |
| **G-3** | AC-8 有界性路径未测 | `implementation_gap` | **builder** | 造 > 4096 条记录 ⇒ `dropped_observations() > 0` ⇒ 断言「该次观测**不可判**」(⛔ 不得记零违例);⚠️ 建议**把上限做成可注入**(构造参数/静态 setter)以便测试,⛔ 不得为测试改生产语义 |
| **G-4** | AC-9 未覆盖文档产出物 | `implementation_gap` | **builder** | 把 `FORBIDDEN` 扫描面扩到**档 2 的文档产出物**(`07 号` 新 SU 段 · `cep-usage.md` §12.10 · main spec)⇒ 并**自带反向 fixture**(影子树里植入一条肯定式夸大表述 ⇒ 按签名红) |

⚠️ **补齐路径**:该 change **已归档** ⇒ 4 项补正**不改任何契约**(⛔ 不动 main spec 的 Requirement)⇒ 建议作为**归档后缺陷补正**(单一 commit + 台账 Entry 登记),⛔ **不新建 change**(新建会把 4 项纯实现缺口升级为一次完整立项流程,⛔ 且会让 KPI 再占 slot)。

⚠️ **复审口径**:4 项补正完成后,**只复核这 4 项**(⛔ 不重审已通过的 10 项)⇒ 复核轮次 = 本报告的第 2 轮(总 2 轮,⛔ 未超 verifier 的 3 轮上限)。

---

## 4 · 关联

change `cep-evidence-tier2-minimal-slice`(归档件 `openspec/changes/archive/2026-11-14-cep-evidence-tier2-minimal-slice/`)· 台账 **Entry #410**(实施回执)/ **#411**(五步链授权)/ **#412**(归档回执 + 第三次 `--update` 授权)/ **#413**(指针链 + 既有债登记)· architect 裁决 `.rddf/reviews/cep-tier2-prerequisite-rulings.md`(C′ / C′-bis / C′-bis-ter)· `.rddf/improvements/cep-evidence-tier2-minimal-slice.md`(v2)· `ADR-CEP-20` 决策 5 / 6

---

# 第 2 轮验收（2026-11-14）—— 只复核 G-1 ~ G-4

> **角色**:verifier(只验不改;本轮产出 = 本段)· **已用轮次**:**2 / 3**
> **复核范围**:⛔ **只复核 G-1 ~ G-4** —— 第 1 轮已通过的 **10 项(AC-3 / 4 / 5 / 6 / 7 / 10 / 11 / 12 / 13 / 14)不重审**。
> **被验对象**:台账 **Entry #414** 的实施授权 + **Entry #415** 的实施回执 ⇒ 实施增量 = `git diff 01769640..HEAD`(规划 commit → HEAD)。
> **判据来源**:`.rddf/improvements/cep-tier2-verification-closure.md` 的 **A-1 ~ A-12** + 第 1 轮报告的 G-1 ~ G-4 原文。

## 0 · 判定汇总

| 缺口 | 第 1 轮 | 第 2 轮 | 结论 |
|---|:--:|:--:|---|
| **G-1** provenance 机器判据 | ⚠️ | ✅ **通过** | 判据已可执行且**有牙**(见 §1.1 的两条决定性变异) |
| **G-2** 三段连通反向证据 | ⚠️ | ⚠️ **部分达成** | 三段中 **2 段**已可机判;「**断言不落**」一段**仍未可机判** ⇒ AC-2 **不能记 ✅** |
| **G-3** 有界性溢出路径 | ⚠️ | ✅ **通过** | 溢出路径已**真实触发**且「非零 ⇒ 不可判」已执行 |
| **G-4** AC-9 产出物覆盖面 | ⚠️ | ✅ **通过** | 扫描面 = 段级「新增产出物」;变异 g 按签名红;**边界已实测** |

⇒ **总判定:仍为「有条件通过」,但缺口由 4 项收敛为 1 项**(AC-2 的「断言不落」一段)。
⇒ **未达成项的唯一出口** = D-1 的升级路径(观测事实导出 JSON + 新 shell 闸 + 影子树变异,≈ 0.5 ~ 1 人日,**触发五步链**)⇒ 该决策属 **verifier + 用户**(⛔ builder 不得自行升级)。

---

## 1 · 逐条判定与工具证据

### 1.1 · G-1(AC-1 ③ provenance 机器判据)—— ✅ **通过**

| 子项 | 判定 | 工具证据 |
|---|:--:|---|
| 判据**可执行** | ✅ | `test/test_cep_tier2_observation.cc` 的 `detect()` 签名 = `detect(obs, home_last_write, dropped, known_engine_masks, observations_enabled)` ⇒ **5 个形参全部无默认值** ⇒ 调用方**无法**省略 `known_engine_masks` |
| ⛔ **无「传 `~0` 变死代码」** | ✅ | `grep -n "uint64_t{0}\|~0" test/test_cep_tier2_observation.cc` ⇒ 仅 **2 处命中且全在注释里**(`:73` / `:376` 的禁传说明)⇒ **零处实参传 `~0`** |
| **非默认值调用点存在** | ✅ | 9 处调用点(`:161 :197 :238 :262 :268 :379 :389 :418 :454 :478 :517`)**全部显式传值** ⇒ 满足 A-11 ① |
| **正控** | ✅ | `:379` 用例:他人位恰等于 peer 引擎 `self_mask` ⇒ `provenance_broken == false` 且 `untraceable_bits == 0` |
| **判别力(非恒真)** | ✅ | `:389` 子断言:把 peer 位**排除**出已知集合 ⇒ `provenance_broken == true` ⇒ 该断言**不是装饰** |
| **反 fixture** | ✅ | `:418` 用例:`dir.set_sharer(kAddr, kNodeIdGmmu, 0, SnoopState::S)` 播种 ⇒ `untraceable_bits != 0` ⇒ `provenance_broken == true` |
| ⚠️ **播种前置断言(防伪测试)** | ✅ | `:415` 读回 `dir.lookup_scoped(kAddr, 0)` 确认目录位**确实**置上 ⇒ 规避「未映射节点 / 越界 level 静默 no-op」造成的假红 |
| ⚠️ **决定性变异(有牙)** | ✅ | 实测:把 `if (untraceable != 0)` 短路为 `if (false)` ⇒ **2 个用例变红**(正控的判别力子断言 + 反 fixture)⇒ 判据**有牙**;已还原并复验 |

### 1.2 · G-2(AC-2 三段连通)—— ⚠️ **部分达成**(⭔ 不记 ✅)

| 段 | 判定 | 工具证据 |
|---|:--:|---|
| ① 观测面 → 判定 → 断言(正向) | ✅ 可机判 | `:517` `v_on.write_episodes == 1` / `v_on.swmr_violations == 1` |
| ② 「**判定不收**」 | ✅ 可机判 | `:530` 只喂 `Kind::Fill` 的记录 ⇒ `write_episodes == 0` / `swmr_violations == 0` / `fill_records == fills_only.size()` ⇒ 判定行的**选择**是决定性的 |
| ③ 「**观测面不产记录**」 | ✅ 可机判(且**强化**) | `:545` 同款驱动 + 仅「观测面关闭」一个差异 ⇒ `observation_count() == 0` ⇒ `detect(..., observations_enabled=false)` ⇒ `invalid == true` 且 `invalid_reason` 含「不可判」⇒ **那组 0 被标成「不可判」,⭔ 不是「零违例」**(直接封住 AC-14 边界 ⑥ 的禁区) |
| ④ 「**断言不落**」 | ❌ **未可机判** | `grep -rn "shadow\|SHADOW\|mutations\|--mutations" test/test_cep_tier2_observation.cc` ⇒ **0 命中**;`ls scripts/test/ \| grep -i "tier2\|observ"` ⇒ **无 tier2 专用影子闸**(仅 `cep_b4_observation_keys_check.*`,与本档位无关)⇒ **断言被删除时无任何机制会变红** |

⇒ **AC-2 的字面要求(「拔掉任一段 ⇒ 按签名红」)未闭合** ⇒ **⚠️ 部分达成,⛔ 不得记 ✅**。
⇒ 已在用例内逐字标注 + 台账 Entry #415 ① 逐字标注 ⇒ **诚实边界已登记**(A-3 ③ 满足)。

### 1.3 · G-3(AC-8 有界性)—— ✅ **通过**

| 子项 | 判定 | 工具证据 |
|---|:--:|---|
| 上限**可注入**(不改默认) | ✅ | `protocol_engine_base.hh` 新增 `set_observation_capacity()` / `observation_capacity()` + `observation_capacity_` 成员;**默认 `kObservationCapacity = 4096` 不变** ⇒ 生产路径零行为变化 |
| 溢出路径**真实触发** | ✅ | `:454` 用例:`set_observation_capacity(2)` ⇒ 一次写(2 transition + 1 fill)⇒ `observation_count() == 2` 且 `dropped_observations() == 1` |
| 「非零 ⇒ 判不可判」**已执行** | ✅ | `detect(..., dropped=1, ...)` ⇒ `invalid == true` 且 `invalid_reason` 含「丢弃」 |
| ⚠️ **最狠的情形也覆盖** | ✅ | `:478` 用例:`cap=1` ⇒ **判定行本身被丢弃** ⇒ `write_episodes == 0` / `swmr_violations == 0` ⇒ **必须**判 `invalid == true` ⇒ 这正是「零违例」假象的**最小复现** ⇒ 已被封住 |
| ⚠️ **决定性变异(有牙)** | ✅ | 实测:把 `record_observation` 的判据改回 `kObservationCapacity` ⇒ **W2 用例变红** ⇒ 有界性判据**有牙**;已还原并复验 |

### 1.4 · G-4(AC-9 产出物覆盖面)—— ✅ **通过**

| 子项 | 判定 | 工具证据 |
|---|:--:|---|
| 扫描面 = **段级「新增产出物」** | ✅ | `cep_e2e_reconcile_check.sh` 新增 **T6** 块,3 个单元:`07 号`「Status Update 2026-11-14」段 · 手册「## 12.10」段 · 新 main spec(整份)⇒ **按标题定位,⛔ 不写死行号** |
| ⚠️ **为何段级是对的** | ✅ | 实测:整文件扫 ⇒ **7 处假红**(`07号` 档 0 SU 标题 ×3 + 后置否定 ×2 + main spec 英文 `SHALL NOT` ×2)⇒ 根因 = `FORBIDDEN` 是**档位相关**词表(「通过态」在档 0 **合法**、在档 2 非法)⇒ **段级**才是 AC-9 原文「**新增产出物内** grep」的**字面读法**(文件级反而**超出**契约) |
| ⛔ **不放宽判据** | ✅ | `NEG_MARKERS` 仅补 **1 个** RFC 2119 否定标记 `NOT`(消 main spec 的英文 `SHALL NOT` 假红)⇒ **未扩窗口、未加词表项**;且**未改** `has_negation_context` 的「只看前文」方向 ⇒ 段级扫描后后置否定问题自动消失 |
| **反向 fixture(变异 g)** | ✅ | `bash scripts/test/cep_e2e_reconcile_check.sh --mutations` ⇒ **7 条变异全按签名红**(含 **g** = 文档植入肯定式夸大表述) |
| **影子树 plumbing** | ✅ | 新 env `CEP_RECONCILE_DOCS_ROOT` + 文档影子树复制 + **递归重入透传**(否则变异 g 读真实仓 ⇒ 永不红)+ 指纹集纳 **3 份文档** ⇒ 「真实仓字节指纹前后相等」✅ |
| ⚠️ **边界正确性(独立实测)** | ✅ | 影子树实测:把**档 0** 的 SU 标题(`07号:639`)改成含肯定式「检测力已证」⇒ 闸**仍绿**(rc=0)⇒ 扫描面**正确限定**在档 2 的新增产出物内 ⇒ **无假红** |
| ⚠️ **注释/计数同步** | ✅ | 脚本头「7 条决定性变异」+ `:430` echo「7 条」已同步(⛔ `docs_sync_check` 不抓此项) |

---

## 2 · 其余判据的独立核验(A-5 ~ A-12)

| 判据 | 判定 | 工具证据 |
|---|:--:|---|
| **A-5** verifier 第 2 轮报告 | ✅ | 本段(verifier 产出,⛔ builder 未代写判定) |
| **A-6** 台账 §10 订正 | ✅ | `obj` 内 4 行**行内替换**;**行数不变**(22 912 行)⇒ **零行号位移**;「DoD 12 条」= 「11/12 达成;唯一未达成 = `B3b-2` = `0/4` ⏭」⇒ **禁 token 5 条不命中**(`cep_dod_reading_check.sh` rc=0) |
| **A-7** push | ⏳ **待执行**(W7,下一步) | — |
| **A-8** 0 回归 | ✅ | `ctest --test-dir build -j2` ⇒ **177/177**;`scripts/test/cep_*.sh` **42** 个逐个 rc=0(**失败闸数 = 0**);`docs_sync --strict` / `agents_md_lint` / `format --check` / `markdown_link_check` 全 rc=0 |
| **A-9** 范围纪律(逐字抄 AC-7) | ✅ | `git diff 066767ac..HEAD --stat --` 对 AC-7 的 **9 条路径** ⇒ **输出为空**(全部零 diff);本轮增量 `01769640..HEAD` 的文件清单 = **5 个**(`obj` 台账 · `role.json` · `protocol_engine_base.hh` · `cep_e2e_reconcile_check.sh` · `test_cep_tier2_observation.cc`)⇒ **零 ADR / 零 main spec / 零 invariant / 零虚方法**;`git diff 01769640..HEAD -- openspec/specs/cep-evidence-tier2-observation/spec.md` ⇒ **空**(Requirement / Scenario 逐字未变);`SharerTrackerIf` 虚方法 = **5**(不含析构)且本轮**零改动**;本轮新增行含 `INV-CEP-` 的 = **0** |
| **A-10** 冻结面 | ✅ | 本轮**零文档改动**(3 份文档只被**读**)⇒ 冻结面 hash 不变 ⇒ 无需五步链;`cep_freeze_check.sh` 无参复验 rc=0(85 产物) |
| **A-11** 死判据免税 | ✅ | ① 新增形参非默认值调用点:9/9 显式传值 + 零 `~0`;② provenance 短路 ⇒ **2 例红**;③ `set_observation_capacity` 有非 4096 调用点(`:452` / `:476`);④ 无「机件在、用例无」残留 |

---

## 3 · 遗留清扫复核(verifier 第 5 步)

| 项 | 范围 | 结果 |
|---|---|---|
| TODO / FIXME / XXX / HACK | 本轮 3 个代码文件 | **0 命中** ✅ |
| 临时调试日志(`fopen` / `printf` / `DEBUG_PRINT`) | 同上 | **0 命中** ✅ |
| 被 skip 的测试(`SKIP` / `#if 0`) | 同上 | **0 命中** ✅ |
| 注释掉的代码块 | 同上 | **0 命中** ✅ |
| **变异残留** | 本轮 2 次决定性变异 | ✅ **已还原并复验**(`[tier2]` 11 cases / 67 assertions 全绿) |
| ⚠️ **AC 未落项** | G-2 的「断言不落」一段 | **1 项** ⇒ ⛔ **不记清扫通过** |

---

## 4 · 失败项与路由建议

| # | 未达成项 | 分类 | 路由 | 出口条件 |
|:--:|---|---|---|---|
| **G-2 ④** | AC-2 的「**断言不落**」一段未可机判(断言被删 ⇒ 无机制变红) | `implementation_gap`(判据正确、实现未完) | **verifier + 用户**(D-1 裁决) | 二选一:① **接受「部分达成」并结案**(登记 AC-2 为 ⚠️ 长期形态,⭔ 不得写 ✅);② 走升级路径 = 观测事实导出 JSON + 新 shell 闸 + 影子树变异(≈ 0.5 ~ 1 人日;⚠️ 新闸若入 `scripts/test/` ⇒ **触发五步链**)⇒ 届时**须回 planner 重排** |

---

## 5 · 范围边界(如实登记 · 非缺口)

**T6 只守档 2 的「新增产出物」** ⇒ **档 0 / 档 1 的产出物不受防夸大闸保护**。⚠️ 这是 **AC-9 原文的字面范围**(「新增产出物内 grep 禁表述」)⇒ **不是缺口**,但**读者须知**:若将来档 0/1 的产出物出现夸大表述,本闸**不会**拦。

---

## 6 · 关联

· 第 1 轮报告(同文件上文)· 台账 **Entry #414**(规划 + metis 13 条处置 + 实施授权)/ **#415**(W1~W4 实施回执)· `.rddf/improvements/cep-tier2-verification-closure.md`(A-1 ~ A-12)· 实施增量 `git diff 01769640..HEAD` · `ADR-CEP-20` 决策 5 / 6
