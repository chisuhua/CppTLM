# 验收报告 · `cep-protocol-engine-capability-declaration`

> **验收者**：verifier（角色内建，非 subagent）· **日期**：2026-11-23
> **基线**：`fb13e8de`（branch `openspec/cep-protocol-engine-capability-declaration` 起点）
> **被测提交**：`0d3f637b`（实施）+ `4b409245`（测试补足）+ `4069306d`（索引补登）
> **工作树**：`.rddf/wt/cep-protocol-engine-capability-declaration/`（worktree 模式，P1.5 判定）
> **已用轮次**：**1 / 3**（`RDDF_VERIFIER_MAX_LOOPS`）

---

## 0. 判据来源（按优先级）

| 优先级 | 来源 | 采用情况 |
|:--:|---|---|
| 1 | 用户本次验收条件 | 未单独给出 ⇒ 下取 |
| 2 | `openspec/changes/cep-protocol-engine-capability-declaration/proposal.md` 的 `## 验收标准` 段 | ✅ **采用**（AC-1 ~ AC-13） |
| 3 | 本次变更自声明（3 个 commit message） | 与 AC 一致，无额外承诺 |
| 4 | ADR 的 Verification 章 | 本 change **不新增/不改** ADR ⇒ 不适用（AC-12 已证 66/66 不变） |

---

## 1. 分流判定（`rdd-verifier` skill 三前置）

| # | 条件 | 实测 | 结论 |
|:--:|---|---|:--:|
| 1 | change 存在且 proposal 含 `## 验收标准` | `grep -cE '^## (验收标准\|Acceptance Criteria)'` = **1** | ✅ |
| 2 | P3 archive gate 已触发（`iteration.json` 的 `status ∈ {in_worktree, completed}`） | `.rddf/state/iteration.json` **不存在**（P0 已报 `iteration.json not found`） | ❌ **未触发** |
| 3 | skill 可加载 | `rdd-skills.sh --json` 的 `available[]` 含 `rdd-verifier` | ✅ |

⇒ **条件 2 不满足 ⇒ 走本地验收**（本文档即本地验收报告）。
⚠️ 本地验收的额外门「`.rddf/state/iteration.json` 中 `tasks_done == tasks_total > 0`」**无法机检**（文件不存在）⇒ 以 `tasks.md` 人工核对替代：**8/8 已勾**（见 §3 AC-10）。

---

## 2. 逐条判定

| AC | 判定 | 证据（命令 → 输出摘要） |
|:--:|:--:|---|
| **AC-1** 通用层零协议名 / 零 include 协议头 | ✅ **PASS** | `bash scripts/test/cep_engine_capabilities_check.sh` → `✓ AC-1 零协议类名` + `✓ AC-1 零 include 协议头` |
| **AC-2** 实例集双向零 orphan | ✅ **PASS** | ① 闸 → `✓ AC-2 正向` + `✓ AC-2 反向`；② **独立复算两个 id 集，逐字相同**：`chi_rnf cxl_2_0 cxl_3_0_symmetric mesif moesi_gpu nvlink_4 tilelink_c ualink_4` == 同串（8 项） |
| **AC-3** 在飞建模一致（编译期 + probe） | ✅ **PASS** | 闸 → `✓ probe[AC-3-baseline] 基线编译通过` + `✓ probe[AC-3-mutated] 编译失败且命中签名「chi_rnf」`；另 `test_cep_engine_capabilities.cc` 含 5 条逐实例 `static_assert`（消息各含实例 id） |
| **AC-4** 瞬态态策略（编译期 + probe，仅 5 个持字段 adapter） | ✅ **PASS** | 闸 → `✓ probe[AC-4-baseline] 编译通过` + `✓ probe[AC-4-mutated] 编译失败且命中签名「mesif」`；测试含 5 条 `static_assert`（`chi_rnf`/`mesif`/`moesi_gpu`/`tilelink_c`/`cxl_2_0`）；实测 `ualink_adapter.hh` / `nvlink_adapter.hh` **无** `kTransientStateCount` ⇒ 已显式排除 |
| **AC-5** `basis` 非空 + `kEgressHasMachineCheck` 诚实登记 | ✅ **PASS** | 闸 → `✓ AC-5 粗检：初始化区段无空串`；测试 → 逐行 `REQUIRE(basis[0] != '\0')` + `static_assert(!kEgressHasMachineCheck)` |
| **AC-6** 谓词变异负控（限两项） | ✅ **PASS** | `./bin/cpptlm_tests "[cep][engine_capabilities]"` → `All tests passed (91 assertions in 6 test cases)`；其中负控 4 段：`basis` 置空 / `basis = nullptr` / `id` 重复 / `id` 置空 ⇒ 均 `CHECK_FALSE` |
| **AC-7** SKIP 与 FAIL 可区分 | ✅ **PASS** | 影子副本（`HDR` 指向空头）→ `[SKIP] 从 kEngineCapabilities 初始化区解析出 0 行（自检未就绪）` 且 **rc=0**（与 FAIL 的 rc=1 + `[FAIL]` 可区分） |
| **AC-8** 引擎头文件集反向存在性 | ✅ **PASS** | ① 正控：`✓ AC-8 引擎头文件集恰为已登记的 6 个（5 实例引擎 + 1 Null 外壳）`；② **反向 fixture 实跑**：`touch include/tlm/cep/fake_protocol_engine.hh` ⇒ 闸 `[FAIL] … engine-cap-new-engine-detected`；`rm` ⇒ 还原后 `[PASS]` |
| **AC-9** 单真源（struct/init 区段禁字段复制） | ✅ **PASS** | 闸 → `✓ AC-9 单真源：struct / init 区段无 transient / in_flight`；区段由 `// == AC-9 scan begin/end: … ==` 标记界定（非全文件扫描 ⇒ 不误伤 `HasInFlightApi`） |
| **AC-10** 零回归 | ⚠️ **部分通过（附 2 项环境条件）** | 见 §3 详述 |
| **AC-11** 既有冻结面零变更 | ✅ **PASS** | `git diff --name-only fb13e8de..HEAD` 对 `docs/adr/ADR-CEP-` / `docs/architecture/cep/` / `openspec/specs/cep-` **零命中**；新增的 `openspec/changes/<name>/` 属新 change（AC 已豁免） |
| **AC-12** 不新增 invariant 编号 | ✅ **PASS** | `python3 scripts/test/cep_invariant_map_check.py` → **rc=0**，`66/66 = 66 项有牙载体 + 0 ⏭-unmodelled + 0 ⏭-pending`；diff 中**零**新增 `INV-CEP-*-IC*` |
| **AC-13** 表述纪律（人工评审） | ✅ **PASS（人工）** | 仅扫本 change **新增行**：唯一命中是 AC-13 条文**自身**（`不得出现「协议语义已验证 / 无违例 / 已证明」`，属禁止性语境）⇒ **无肯定式违禁表述** |

---

## 3. AC-10 详述（唯一非全绿项）

### 3.1 新条目

`ctest -R "cep_engine_capabilities"` → **2/2 Passed**（`cep_engine_capabilities` 0.04s · `cep_engine_capabilities_check` 5.99s）。

### 3.2 全部 `scripts/test/cep_*.sh` 逐条跑

```
PASS=49  FAIL=1
FAILED: cep_full_green_run_check.sh
```

**唯一红闸根因（环境）**：`✗ IC3 非通过态: report.html 未渲染「✓ 一致性闸门通过」` ⇒ 依赖既有 experiment 产物。
**同一脚本在主仓**：`bash scripts/test/cep_full_green_run_check.sh` → **RC=0** ⇒ **非本 change 引入**。

### 3.3 `ctest -R cep`（worktree，111 项）

红 5 项，**逐项证明为 worktree 环境产物**：

| # | 测试 | 红因 | 反证 |
|:--:|---|---|---|
| 88 | `cep_stub_cache_core` | 读 `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/…`（worktree **缺失**） | 主仓 `./build/bin/cpptlm_tests "[cep][stub-cache-core]"` = **224 assertions 全过** |
| 89 | `cep_scenario_anchor_reconciliation` | 同一归档路径缺失（实测 `MISSING openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core`） | 同因 |
| 90 | `cep_all` | 聚合 `[cep]` ⇒ 含 88/89 | 传递 |
| 155 | `cep_registry_baseline_check` | **仅**归档计数：文档 `170/54` ≠ 实测 `69/30` | worktree 归档目录实测 **69**；主仓 **170** |
| 183 | `cep_full_green_run_check` | 缺 experiment `report.html` 产物 | 主仓 RC=0（§3.2） |

**根因**：worktree 为干净 checkout，`openspec/changes/archive/` 仅 **69** 个目录（主仓 **170**，其余不在库内）⇒ 读归档路径 / 依赖既有 run 产物的判据在 worktree 恒红。

### 3.4 条件（须在归档合并后于主仓复验）

| # | 条件 | 复验命令 |
|:--:|---|---|
| C-1 | `ctest -R cep` 在主仓全绿 | `ctest --test-dir build -R cep -j4` |
| C-2 | `cep_registry_baseline_check` 的归档计数同步到 **171 / 55**（本 change 归档后） | `bash scripts/test/cep_registry_baseline_check.py`（或 ctest 同名项） |

---

## 4. 遗留清扫复核（build 自查项的第二意见）

扫描范围 = 本 change 触碰的 10 个 `include/` `test/` `scripts/` 文件。

| 项 | 结果 |
|---|---|
| TODO / FIXME / HACK | ✅ 干净（唯一命中 `mktemp -d …XXXXXX` 是 mktemp 占位符，**非** TODO） |
| 注释掉的代码 | ✅ 干净 |
| 临时调试输出（`static FILE* diag` / `fopen("/tmp` / `std::cout <<` / `printf("DEBUG`） | ✅ 干净 |
| 被 skip 的测试（`[!mayfail]` / `!shouldfail` / `SKIP(` / `.disabled`） | ✅ 干净 |
| 半截实现 / 未替换 mock | ✅ 无（8 个 task 全勾，闸 + 测试 + probe 三路覆盖） |
| 新造 fallback 分支 | ✅ 无 |

**额外发现（非缺陷，已记录）**：`code-review-graph` 的 pre-commit hook 在两次 commit 都报 `Untested: egress_mode_name, in_flight_modeling_name, EngineCapability, engine_capabilities_are_consistent, EngineTypeTraits`。实测：后三者由 §2 的 AC-5/AC-6/AC-3 覆盖；前两者已在 commit `4b409245` 补断言（91 assertions）。**hook 的图谱快照未随该 commit 刷新** ⇒ 输出滞后，非真实缺口。

---

## 5. 失败项与路由建议

| 项 | 分类 | 路由 |
|---|---|---|
| AC-10 的 5 项 ctest 红 + 1 项闸红 | **环境受限**（非 `implementation_gap`，非 `proposal_drift`） | **不回 build**。改为**归档合并后在主仓复验**（条件 C-1 / C-2） |
| 其余全部 AC | — | 无需路由 |

> ⚠️ **不判 pass 的部分必须显式**：AC-10 在 **worktree** 内**未**达成「全绿」；本文档不把它读作通过，而是登记为**待主仓复验的条件项**。

---

## 6. 结论

**验收判定：PASS（附 2 项环境条件 C-1 / C-2，须在归档合并后于主仓复验）**

- AC-1 ~ AC-9、AC-11 ~ AC-13：**全部通过**，每条均有工具证据（闸输出 / 测试计数 / diff 对账 / 脚本 rc）。
- AC-10：**新条目 2/2 绿、49/50 闸绿**；残余 5 项 ctest 红与 1 项闸红均已**双线取证**证明为 worktree 环境产物（主仓对照 + 缺失路径实测）。
- 遗留清扫：**全项干净**。

**verifier 未修改任何实现文件**（角色边界）。本轮为**第 1 轮**（上限 3）。

---

## 7. 复验回执（2026-11-23 · 归档合并后于主仓复验）

> 本节由 **builder** 在归档合并后记录 —— 闭合本文 §3.4 的两个条件项；
> `§6` 的 PASS（附条件）据此**更新为 PASS（无条件）**。

| 条件 | 复验命令 | 结果 |
|:--:|---|---|
| **C-1** `ctest -R cep` 全绿 | `ctest --test-dir build -R cep -j4` | ✅ **110/110 Passed**（含本 change 的 `cep_engine_capabilities` + `cep_engine_capabilities_check` 两条新条目） |
| **C-2** 归档计数同步 | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ **5/5 == 实测**（spec 139 / cep-* 48 / 归档 **171** / CEP **55** / active **0** / 闸 50 / test 78） |

**额外的全量回归（主仓）**：

| 项 | 结果 |
|---|---|
| 全部 `scripts/test/cep_*.sh` | ✅ **50/50 全绿**（worktree 期唯一红的 `cep_full_green_run_check.sh` 在主仓通过，印证 §3.3 的环境归因） |
| `bash scripts/test/cep_freeze_check.sh`（无参） | ✅ RC=0 · `provenance 自检通过：91 产物 hash 与 commit 46c55728 一致` |
| `bash scripts/test/cep_docs_sync_check.sh --strict` | ✅ RC=0 |
| `bash scripts/test/cep_doc_ref_check.sh` | ✅ RC=0 |
| `bash scripts/test/cep_spec_strict_check.sh` | ✅ RC=0（6 项）—— ⚠️ 归档时 `openspec archive` 写入占位 Purpose 曾致 ① 零红失败，已在 commit `46c55728` 补写真实 Purpose 修复 |
| 主仓全量构建 | ✅ `MAIN_BUILD_RC=0`（91/91） |

**⇒ 判定更新：PASS（无条件）。** §6 的「附 2 项环境条件」已全部闭合。
