# verify: cep-ssp-cp-q1b-landing —— 实施验收（AC 全量）

> **验收对象**：`cep-ssp-cp-q1b-landing` 的**实现**（contract 面：`family` 4 值域 + `protocol_version` 两分支）
> **验收时间**：2026-11-27
> **执行角色**：builder（角色内建 verifier 契约 · **本地验收路径**）
> **实施 commit**：分支 `openspec/cep-ssp-cp-q1b-landing` @ `4ef6f68f` · 已合并 main @ `d2a6997f`
> **⚠️ 路径判定**：`rdd-verifier` skill 的**canonical 前置条件②不满足**（`iteration.json` 不存在 ⇒ `rddf rdd-verify --dry-run` 返 `No eligible changes to verify`）⇒ 按 build.md §verifier 产出契约走**本地验收**（判据来源 1→4 · 逐条工具取证）
> **前序**：本报告取代同名文件的**立项产物**验收版（2026-11-26 · 9/9 PASS）；立项结论仍然有效

---

## 判据来源（优先级 1 → 4）

| # | 来源 | 本次采用 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 「切 builder 走 P2 实施」⇒ 实现落地 + 全量回归绿 |
| 2 | `openspec/changes/<name>/proposal.md` 的 `## 验收标准` | ✅ **7 条**（AC-1 ~ AC-7，本报告主判据）|
| 3 | 本次变更自己声明的行为 | `design.md` D-1~D-8 + plan 的 Done Definition（8 条）|
| 4 | ADR 的 Verification 章 | `ADR-CEP-11` SU(2026-11-26) ③ 第 1~7 面落地清单（**范围对账**）|

---

## 逐条判定（AC-1 ~ AC-7）

### AC-1 —— `family` enum 含 **4 值**；`protocol_family_from_name("user-defined")` 为 `true`，`protocol_family_id(UserDefined) == 4`

- **证据 1**：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` ⇒ `All tests passed (77 assertions in 6 test cases)`（含本 AC 的用例）
- **证据 2**：用例 `[cep][schema] family 域必须含 4 值且 user-defined 可解析` —— 断言 `kProtocolFamilyCount == 4u` + `protocol_family_from_name("user-defined", f)` ⇒ `f == ProtocolFamily::UserDefined` + `protocol_family_id(UserDefined) == 4u` + `protocol_family_name(UserDefined) == "user-defined"` + 既有 3 值逐字不变 + 未知字符串仍返 false
- **证据 3**：实现落点 `include/tlm/cep/protocol_family.hh:47`（`UserDefined = 4`）· `:54`（`kProtocolFamilyCount = 4`）· `:61`（数组条目）· `:89`（switch case）· `:116`（新增 `static_assert`）
- **判定**：✅ **PASS**（confidence 1.0）

### AC-2 —— `protocol_version` **接受** `user:<已登记 ssp-id>`；**拒绝** `user:<未登记>` / `chi-r3` / `user:`（空 id）—— 4 条反向 fixture 各自按**正确原因**红

- **证据 1**：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` ⇒ 6 cases 全绿（同一次运行）
- **证据 2**：用例 `[cep][schema] 两分支谓词必须接受 user:<已登记 ssp-id> 且拒绝其余` —— 正控 `chi-r2` / `mesif-1.0` / `user:mesif_ssp` ⇒ 真；反控 4 条 `user:no_such_ssp` / `chi-r3` / `user:` / `""` ⇒ 假（**各自红因不同**：分支② id 未登记 / 分支① 拼错官方版本 / 分支② 空 id / 空串）
- **证据 3**：用例 `[cep][schema] protocol_version pattern 必须容纳 user:<ssp-id> 两分支` —— pattern `^([a-z0-9]+(-[a-z0-9.]+)*|user:[a-z][a-z0-9_]*)$` 容纳 `user:mesif_ssp` / `user:a1`，拒绝 `user:Mesif` / `user:1abc` / `user:a-b` / `user:a.b` / `user:`；锁表 3 个官方版本仍被容纳（零回归）
- **证据 4**：schema 落点 `include/tlm/cep/protocol_instance.schema.json`（`family` enum + `user-defined` · `protocol_version` pattern 两分支）
- **判定**：✅ **PASS**（confidence 1.0）

### AC-3 —— **既有 8 实例全绿**（`protocol_version` 全在锁表 ⇒ 零回归）；批 1 的 `mesif_ssp` 不受影响

- **证据 1**：`./build/bin/cpptlm_tests "[cep]"` ⇒ `All tests passed (13719 assertions in 585 test cases)` —— **零回归**（全量 CEP 套件）
- **证据 2**：`./build/bin/cpptlm_tests "[cep][schema]"` ⇒ `All tests passed (310 assertions in 24 test cases)`
- **证据 3**：IC1 通用闸逐实例判定（`protocol_version_allowed` 两分支）实跑 8 个声明实例全绿；既有 3 族的 `family` 取值与编号（1/2/3）逐字未变（用例内断言）
- **证据 4**：`ADR-CEP-11` §3 锁定表**零改动**（`git diff main..openspec/cep-ssp-cp-q1b-landing -- docs/adr/` 为空）⇒ `INV-CEP-11-IC2` 保持
- **判定**：✅ **PASS**（confidence 1.0）

### AC-4 —— `openspec/specs/cep-core-contracts/spec.md` 的 IC1 Scenario 已两分支化（`grep "user:" <spec>` ≥ 1）

- **证据 1**：`grep -c "user:" openspec/specs/cep-core-contracts/spec.md` ⇒ **2**（≥ 1）
- **证据 2**：与 change delta 的 requirement 块**逐字一致**（脚本比对 `IDENTICAL: True`）⇒ 归档时 `openspec archive` 的 MODIFIED 应用为幂等（early-sync，不churn main spec）
- **证据 3**：IC2 / IC3 逐字零改动（diff 脚本显示差异**恰为 IC1 的 THEN 段**）
- **证据 4**：`openspec validate cep-ssp-cp-q1b-landing --strict` ⇒ rc=0；`openspec validate --changes --strict` ⇒ `1 passed, 0 failed`
- **判定**：✅ **PASS**（confidence 1.0）

### AC-5 —— 两分支谓词是**唯一**实现（共享 helper；⛔ 无第二份拷贝）

- **证据 1**：用例 `[cep][schema] 两分支谓词只有唯一实现(禁第二份拷贝)` 通过 —— 扫描 `include/tlm/cep` + `src` 的全部 `.hh/.cc/.h/.hpp`，凡含 `user:` 判定的文件即红；实测**零命中**
- **证据 2**：实现落点 `test/cep_adr_parser.hh`（`parse_registered_ssp_ids` + `protocol_version_allowed`）—— 与既有 `parse_protocol_version_lock_table` / `protocol_version_in_lock_table` **同处**（⛔ 零新文件），承 Entry #109 **G-2**
- **证据 3**：`include/tlm/cep/instance_assembly_gate.hh` **零 diff**（IAG 不查 `protocol_version` ⇒ 零 diff 是正向结果，per `ADR-CEP-11` SU ③ 第 3 面）
- **判定**：✅ **PASS**（confidence 1.0）

### AC-6 —— 五步链完成：schema + spec **两处**冻结面 ⇒ `cep_freeze_check.sh` 无参 **rc=0**

- **证据 1**：`bash scripts/test/cep_freeze_check.sh` ⇒ **rc=0**（`✓ M1 产物冻结校验通过:97 个产物 hash 全部一致(冻结于 2026-11-26, commit b83c5531a2e7)`）
- **证据 2**：五步链逐环留痕 —— 步1 台账 **Entry #556**（`06f82d30`）· 步2 产物 commit（`b83c5531`）· 步3 `--update --authorized-by "Entry #556"` 输出 `清单差分:新增 0 · 移出 0 · hash 变更 2`（与 Entry #556 ④ 预期**完全一致**）· 步4 清单**单独** commit（`6743e7c3`）· 步5 无参复验 rc=0
- **证据 3**：冻结集成员确认 —— `include/tlm/cep/protocol_instance.schema.json`（`cep_freeze_check.sh:56`）与 `openspec/specs/cep-core-contracts/spec.md`（`:113`）**均在集内**，且**恰这 2 处** hash 变更（其余 95 产物零变化）
- **判定**：✅ **PASS**（confidence 1.0）

### AC-7 —— `test_cep_schema.cc` 的 `[cep][schema][ic1]` 用例全绿 + 新增 `user:` 用例

- **证据 1**：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` ⇒ `All tests passed (77 assertions in 6 test cases)`
- **证据 2**：用例清单（`--list-tests`）—— 既有 IC1 通用闸 + 本次新增 5 例（`family 域 4 值` · `family enum 4 值 + required 13` · `pattern 两分支` · `两分支谓词` · `唯一实现守门`）
- **证据 3**：RED 证据链（构建日志）—— 首次 RED 精确报 `'UserDefined' is not a member of 'tlm::cep::ProtocolFamily'`（功能未实现），非脚手架误报
- **判定**：✅ **PASS**（confidence 1.0）

---

## 范围对账 —— `ADR-CEP-11` SU ③ 的 7 面落地清单

| SU ③ 面 | 落点 | 判定 |
|:--:|---|:--:|
| ① 本 SU（决策 3 / IC1 / 决策 5 三处）| ✅ 已落盘（`ADR-CEP-11` SU(2026-11-26)）· 本 change 零改动 | ✅ |
| ② schema `pattern` 必改 | `protocol_instance.schema.json` | ✅ |
| ③ 共享 helper（单一实现）| `test/cep_adr_parser.hh` + 唯一实现守门用例 | ✅ |
| ④ `test_cep_schema.cc:573` IC1 通用闸 | 两分支化 + 4 条反向 fixture | ✅ |
| ⑤ `test_cep_schema.cc:512` pattern 容纳性用例 | 纳入 `user:` 形态 | ✅ |
| ⑥ `cep-core-contracts/spec.md:335-339` IC1 Scenario | 两分支化（IC2/IC3 零改动）| ✅ |
| ⑦ 完备性口径（1~6 同一批）| 同一 commit `b83c5531`（⛔ 未拆批）| ✅ |

⇒ **7/7 覆盖，无遗漏面**。

---

## 全量回归（Done Definition）

| # | 命令 | 结果 |
|:--:|---|---|
| 1 | `./build/bin/cpptlm_tests "[cep][schema][ic1]"` | ✅ 6 cases / 77 assertions |
| 2 | `./build/bin/cpptlm_tests "[cep][schema]"` | ✅ 24 cases / 310 assertions |
| 3 | `./build/bin/cpptlm_tests "[cep]"` | ✅ **585 cases / 13719 assertions** |
| 4 | `ctest --test-dir build --output-on-failure -j4` | ✅ **211/211 通过**（3 项为既有 skip：`cep_parity_{linear,trace}_e2e` / `cep_parity_e2e`）|
| 5 | `bash scripts/test/cep_*.sh`（**56 个**）| ✅ **0 失败** |
| 6 | `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0 |
| 7 | `openspec validate cep-ssp-cp-q1b-landing --strict` / `--changes --strict` | ✅ rc=0 / 1 passed |
| 8 | `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |
| 9 | `python3 scripts/test/agents_md_lint.py` | ✅ rc=0（AGENTS.md 结构 / 体量 / 行长全合规）|
| 10 | `bash scripts/test/rddf_stage_gate.sh` | ✅ rc=0（P-5 活口径 7/7 同步）|
| 11 | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0（5 项文档取值 == 实测）|

---

## 遗留清扫（本次范围内）

| 项 | 取证 | 判定 |
|---|---|:--:|
| TODO / FIXME / XXX 残留 | `grep -rnE "TODO|FIXME|XXX"` 于全部改动文件 ⇒ **空** | ✅ |
| 临时调试日志（`static FILE* diag` / `printf` / `std::cerr`）| 同上 ⇒ **空** | ✅ |
| 注释掉的代码 / 被 skip 的测试 | 本次**零**注释掉的代码；新增 0 个 skip；`.cc.disabled` 新增 **0** | ✅ |
| 未替换的 mock / 临时 fallback | 无 | ✅ |
| 半截实现 | 6 项改动面全部完成 + 7/7 AC 通过 + tasks 39/40 | ✅ |
| 新建 `.disabled` 测试 | **0**（per AGENTS.md ANTI-PATTERNS）| ✅ |

---

## ⛔ 未闭合项与存量问题（**如实登记**）

| # | 项 | 性质 | 证据 | 处置 |
|:--:|---|---|---|---|
| 1 | **T7.6** `format.sh --check` rc=0 **未达** | **存量**（非本次引入）| 5 个**非本次**文件在 clang-format-22 下需重排（`test_cep_compound_graph_falsification.cc` / `test_cep_engine_capabilities.cc` / `test_cep_fence_effects.cc` / `test_cep_hop_timing_capabilities.cc` / `test_cep_ssp_declaration.cc`）；**本次**改动的 `test_cep_schema.cc` 已清（`06419682`）| ⛔ 不顺手重排存量（避免海量无关 diff）· tasks.md T7.6 保留未勾并注明 · 登记待独立批次 |
| 2 | worktree 缺 gitignored 件导致若干测试假红 | **环境缺口**（非本次引入）| `.gitignore:64` 忽略 `openspec/changes/archive/`（worktree 69 vs 主仓 178）+ `experimental/`（7 vs 1441）| **主仓基线对照**确认同一用例主仓全绿 ⇒ 排除本次引入；本地补齐后复验全绿（补齐件 gitignored ⇒ 不入提交）|
| 3 | `.rddf/state/iteration.json` 不存在 | **工具面缺口** | P0/P1 登记均为 non-blocking 跳过；`rddf rdd-verify --dry-run` 返 `No eligible changes to verify` | ⇒ `rdd-verifier` skill canonical 路径不可用 ⇒ 走**本地验收**（本报告）+ 写 verdict cache 作为归档门输入 |
| 4 | 表述纪律 | — | 本次全部 commit 与本报告**未**宣称「CEP 已支持用户自定义协议」；该能力仍在 `cep-ssp-batch2` 范围 | ✅ 遵守（D-8）|

---

## 结论

**实施验收：PASS**（AC-1 ~ AC-7 **7/7** 通过 · 全量回归 11 项绿 · 遗留清扫零残留 · ADR SU ③ 落地清单 7/7 覆盖）

- ⛔ **唯一未闭合项**为 T7.6（**存量** format 问题，非本次引入，已登记）
- **范围纪律**：未碰 `ADR-CEP-11` §3 锁表 · 未碰 `INV-CEP-12-IC1` 的 13 必含个数 · `instance_assembly_gate.hh` 零 diff · IC2/IC3 逐字零改动 · 零新文件 · 零新增 invariant

**路由**：AC 归档门输入 = 本报告 + `.rddf/state/verifier/.ac-verdict-cep-ssp-cp-q1b-landing.json`（`codebase_commit = 4ef6f68f` = 实现分支 tip）⇒ 准予进入 **P3 归档**（P2.5 评审 = merge · 用户已批准）。
