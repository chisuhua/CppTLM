# 验收报告：cep-ssp-declaration（批 1）

> **验收者**: verifier（角色内建 · 非 subagent）
> **判定**: ✅ **PASS**（轮 2 闭合后；轮 1 曾 FAIL = `implementation_gap`，见 §2）
> **已用轮次**: **2 / 3**（`RDDF_VERIFIER_MAX_LOOPS=3`）
> **分流判定**: ⛔ **未走 `rdd-verifier` skill** —— 条件②不满足（`.rddf/state/iteration.json` **缺失**）⇒ 按契约走**本地验收**
> **判据来源**（优先级降序）：① 用户本次指令「先验收归档」② `proposal.md` §验收标准（**A-1 ~ A-14**，含 `## 验收标准` 段 ✅）③ `tasks.md` 的 Task 0~8 ④ `ADR-CEP-24` §6 的 V-24-1~13 + §4 边界情况 E1~E8

---

## 1. 五步验收（逐条证据）

### 步 1 · 编译 / 静态检查 —— ✅ PASS

| 检查 | 结果 | 证据 |
|---|---|---|
| `./build.sh` | rc=0 · **0 个 `error:`** | `/tmp/opencode/v_build.log` |
| `openspec validate --type change --strict` | ✅ valid | 本轮实跑 |
| `openspec validate --type spec --strict` | ✅ valid | 本轮实跑 |
| `agents_md_lint.py --all` · `docs_sync_check --strict` · `cep_docs_sync_check` · `docs_adr_index_check` · `cep_registry_baseline_check` · `rddf_stage_gate` | 全 ✅ | 本轮实跑 |

### 步 2 · 单元测试 —— ✅ PASS（本 change 新增面）

| 项 | 结果 | 证据 |
|---|---|---|
| `[cep][ssp]` | **235 assertions / 5 test cases 全过** | `./build/bin/cpptlm_tests "[cep][ssp]"` |
| V-24-2 killer（逐格对账 + 反向见证） | ✅ | 同上（test case 2 / 3）|
| loader fail-closed 抽样 | 13 个 SECTION（E1~E12）| 同上（test case 5）|
| `tasks.md` 完成度 | **30 `[x]` / 4 `[ ]`** —— 4 项未勾 = Task 8 的验证项（本报告即其证据）| `grep -c "^- \[ \]"` |

### 步 3 · 集成 / 端到端 —— ✅ PASS

| 项 | 结果 | 证据 |
|---|---|---|
| ctest 注册生效（`cep_ssp_declaration` + `cep_ssp_check`）| ✅ 2/2 | `ctest -R cep_ssp` |
| 本 change 的闸 | ✅ **12 项全过** | `bash scripts/test/cep_ssp_check.sh` |
| **全量回归** | ✅ **211/211** | `ctest -j4`（⛔ 无 `-R` 过滤）|
| 全部 CEP 闸 | ✅ **56/56** | 逐闸实跑 |

### 步 4 · 边界与错误路径 —— ❌ **FAIL**（3 处缺口）

| # | 边界（来源）| 判定 | 证据 |
|:--:|---|:--:|---|
| 4a | loader `E1`~`E12` 抽样（13 SECTION）| ✅ | 测试实跑 |
| 4b | 反向 fixture（3 条变异按签名红）| ✅ | `--selftest` 全绿 |
| **4c** | ⭐ **`E1`「**全 `0xFF` 洞表** ⇒ loader fail-closed」（`ADR-CEP-24` §4 边界 E1）| ✅ **轮 2 已闭** | 轮 1 发现实现缺口（`cell.is_null()` 无条件放行）⇒ builder 增 **`E13`**：`non_hole == 0 ⇒ fail`（`ssp_loader.hh:160-164`）；测试 SECTION「E13 transition 全为空洞（退化 SSP）」断言 `E13` |
| **4d** | 空 `ssps` 数组 ⇒ `E2`（loader 有此判据）| ✅ **轮 2 已闭** | builder 增 SECTION「E2 空 ssps 数组（退化）」⇒ 断言 `E2` |
| **4e** | 空 `states` 数组 ⇒ `E3`（loader 有此判据）| ✅ **轮 2 已闭** | builder 增 SECTION「E3 空 states 数组（退化）」⇒ 断言 `E3` |

> ⚠️ **口径纪律**：4c 是**实现缺口**（代码不满足 ADR 的 E1）；4d / 4e 是**未测缺口**（代码有判据但无测试 ⇒ 「未测 ≠ 通过」）。
> ⚠️ **4c 的复现**：构造 `ssps.json` 使 `transition` 全为 `null` ⇒ `check_ssp_document` 返回 `{ok: true}` ⇒ **静默通过**（违反 `ADR-CEP-24` §4 E1「⛔ 不得静默通过」）。

### 步 5 · 遗留清扫复核 —— ✅ PASS

| 项 | 结果 | 证据 |
|---|---|---|
| TODO / FIXME / XXX（本批新增行）| **0** | `git diff cd94b125^..HEAD \| grep -E "^\+.*(TODO\|FIXME\|XXX)"` = 空 |
| 被 skip / disabled 的测试 | **0** | 同上（`.skip` / `.disabled` 零命中）|
| 临时调试日志 / 静默 fallback | **0** | 本批无 `printf(` 调试、无 fallback 分支 |
| 工作树（排除会话态）| **干净** | `git status --short` |

---

## 2. 轮 1 失败项与处置（✅ 全部已闭）

| # | 失败项 | 分类 | 路由 | 复现命令 |
|:--:|---|:--:|---|---|
| **F-1** | `E1`（全 `0xFF` 洞表 ⇒ loader fail-closed）**未实现** ⇒ 全洞表静默通过 | **`implementation_gap`** | **回 builder**：在 `ssp_loader.hh` 增 **`E13`**（`transition` 全为空洞 ⇒ fail，红因码 `E13`）+ 对应测试 SECTION | `grep -c "cell.is_null()) continue" include/tlm/cep/ssp_loader.hh` == 1 ⇒ 无条件放行 |
| **F-2** | 空 `ssps` 数组 ⇒ `E2` **无测试** | 未测（`implementation_gap` 的测试补齐）| **回 builder**：E2 SECTION 增「空 `ssps` ⇒ `E2`」| `grep -c "空 ssps" test/test_cep_ssp_declaration.cc` == 0 |
| **F-3** | 空 `states` 数组 ⇒ `E3` **无测试** | 同上 | **回 builder**：E3 SECTION 增「空 `states` ⇒ `E3`」| `grep -c "空 states" test/test_cep_ssp_declaration.cc` == 0 |

**轮 1 判定结论**：❌ **FAIL**（三项均 `implementation_gap` —— `proposal.md` A-2 与 `ADR-CEP-24` §4 E1 **明确要求**全洞表 fail-closed，是实现未达，⛔ 不需改 proposal）。

**轮 2 闭合证据（逐条）**：

| # | 闭合证据 | 命令 |
|:--:|---|---|
| F-1 | `grep -c 'fail("E13"' include/tlm/cep/ssp_loader.hh` == **1** · `grep -c 'expect_code(d, "E13")' test/...` == **1** · 测试「E13 transition 全为空洞」断言 `E13` | 本轮实跑 |
| F-2 | SECTION「E2 空 ssps 数组（退化）」⇒ 断言 `E2` | 本轮实跑 |
| F-3 | SECTION「E3 空 states 数组（退化）」⇒ 断言 `E3` | 本轮实跑 |

⇒ **门禁不变**：`[cep][ssp]` **244 assertions / 5 cases** 全过（轮 1 为 235，+9 = 3 个新 SECTION）· 56/56 闸 · 211/211 ctest · 闸 selftest 全绿 · freeze rc=0（C++ 改动不触冻结面）。

**⛔ verifier 不修改实现**（角色边界）⇒ 上表交回 builder 修复后重新验收（轮次 2/3）。

---

## 3. 未覆盖判据面（诚实登记）

| 项 | 说明 |
|---|---|
| **A-5 的「architect 批准记录」载体** | 当前 `ssps.json` **无私有事件**（6 个事件全有核心锚）⇒ A-5 仅由 loader 的 E7 分支覆盖（有测试），但**「批准记录存在性」**这一侧无真实样本 ⇒ 该判据面**结构性不可达**（无私有事件时无可验证对象）|
| **A-9「占比 > 半数」的触发侧** | 闸判据 5 只验证「占比 ≤ 半数」（当前 0/6）；**> 半数的红分支**靠闸的代码路径，未用真实样本激发 ⇒ 与上同类的**结构性不可达**（诚实登记，⛔ 不声称已验证）|
| **A-13 的「既有 8 实例行为逐字不变」** | 由 211/211 ctest + 56/56 闸间接证据；**无逐实例 diff 证据** ⇒ 强度为「全量回归绿」，⛔ 不声称「逐字」 |

---

## 4. 已用轮次

**2 / 3**。轮 1 = FAIL（F-1~F-3）→ builder 修复 → **轮 2 = PASS**（只验 F-1~F-3 闭合 + 新 diff 是否引入新问题，⛔ 未全量重审）。

**轮 2 的新 diff 核查**：`ssp_loader.hh` 仅增 `non_hole` 计数器 + E13 分支（+ 头注释一行）；`test_cep_ssp_declaration.cc` 仅增 3 个 SECTION ⇒ ⛔ 无对既有判据的修改、无新增依赖、无行为变更（除新增 E13 拒绝路径）⇒ **未引入新问题**。
