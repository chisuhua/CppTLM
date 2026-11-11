# cpptlm-cep-gate-strength-tooling: 实施计划（C1 · 闸强度与工具补齐）

> **配套**: `.rddf/improvements/cpptlm-cep-gate-strength-tooling.md`（7 段草案 + 9 条 AC）
> **台账**: `obj-cep-platform.md` §13.2（C1）+ Entry #340（排序裁决）+ Entry #342（本条授权 / 回执）
> **生成时间**: 2026-11-12 · **生成方式**: 角色内置流程（`rdd-builder` 的前置 ② `.rddf/state/.planner-handoff.json` 不存在 ⇒ ⛔ 不伪造前置，走 builder 内置 TDD 5 步 + 轻量执行）
> **Worktree**: 无（轻量执行 —— 全部为脚本 + 文档，零源码 / 零 ABI）
> **分支**: `main`
> **审查**: oracle 1 轮（需改后批准，5 MUST 已落盘草案）；验收由 verifier 按 9 条 AC 逐条取证

---

## Goal (目标)

把 CEP 的 5 处「**登记了，但没有牙齿**」变成「**拦截**」，且**零架构决策变更**：

| 子项 | 交付 |
|---|---|
| **R-a** | `scripts/cep/generate_doc_index.py`（索引生成器落仓）+ 索引闸**三合一** + `07 号` 文末索引段由生成器产出（自述 23/9 → **28/15**，行号列重算） |
| **R-e** | 删 `cep_b3b2_deferral_check.sh:57-61` 的恒真 grep 段 + 建「`obj §6.2` B3b-2 行 ↔ `test/cep_b3b2_deferrals.json`」**族名交叉校验** |
| **R-f** | `cep_b4_observation_keys_check.sh` 输出 `[synthetic-fixture]` 降级标注 + 闸头写明强度边界 |
| **R-g** | `scripts/README.md` 补 2 闸登记 + 计数改成**实测可核**；`AGENTS.md` **4 处**数字订正 |
| **R-h** | `scripts/README.md:55` 裸路径 `fabric.json` → `include/tlm/cep/fabric.json` |

**完成定义 (DoD)**：
1. 9 条 AC 全部有工具证据（`bash` rc + 输出片段）
2. 3 条**决定性变异**（索引计数 / 索引行号 / 族名真源）逐条「破坏 ⇒ 必红 ⇒ 还原 ⇒ 绿」，且**红因正确**
3. `ctest -j4` 全绿 + `[cep]` 全绿 + **35 个** `cep_*.sh` 闸 **rc + ✗ 计数双判**全绿
4. 冻结面五步授权链走完（`07 号` hash 变更，`artifact_count` 仍 **78**）
5. 台账 Entry #342 落盘（含 oracle 5 MUST 的处置 + 3 处实测订正）

---

## 关键约束（设计要点摘要）

| 约束 | 来源 | 影响 |
|------|------|------|
| **闸不得自证** | `ADR-CEP-06` + `obj §6.2` B2 | 新判据的期望值必须**独立于被测对象**（标题结构 / 台账行 / 集合），⛔ 禁止 `grep <被测文件>` 定期待值 |
| **红因必须正确** | `cep_gate_selftest.sh:49-50`（fixture 老化先例） | 变异必须让闸按**语义**变红，⛔ 不接受「文件被删光」式变红 |
| **`07 号` 在冻结集内** | `cep_freeze_check.sh` 的 glob | 改索引段 ⇒ hash 变 ⇒ **必须走五步授权链**（Entry → 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验） |
| **ADR 只允许两种编辑** | `docs/architecture/07 号` §8.5 + `ADR-CEP-00` SU §⑤ | 本条**不碰任何 ADR** |
| **`07 号` 只改文末索引段** | 本条 Out-of-Scope | 正文 / 判据 / 计数（B3b-2 仍 0/4）零改动 |
| **gen 是 tool** | `cep_freeze_check.sh:16-19`（checker 不冻结） | `scripts/cep/generate_doc_index.py` **不入冻结集** |
| **不占 KPI slot** | 先例 `cpptlm-cep-b3b2-d4-trigger-probe` | ⛔ 不新建 OpenSpec change |
| **`docs_sync_check.sh` 无 `--strict` 恒 rc=0** | 本节实测 | AC-6 必须用 `--strict` 且同时看 ✗ 计数 |

### 一处对草案的**事实性适配**（须在 Entry #342 登记）

草案 R-e 写「与 `07 号` SU·六 的族名单交叉校验」；实测**该前提已失效**：

- 收口批（Entry #339）把 10 则 SU 全量改成**归宿指针** ⇒ `07 号` 正文已**不再含** 4 族枚举（`grep 'CXL 3.0'` 在 SU 段 0 命中）；
- 4 族枚举的**现存活真源** = `.rddf/roadmap/objectives/obj-cep-platform.md:549`（`- **B3b-2. …** —— CXL 2.0 非对称(**显式冻结**) / CXL 3.0 对称 DCOH / NVLink / UALink(**受限-⏭**)`）。

⇒ 交叉校验的**期望值来源**改为 `obj §6.2` 的 B3b-2 行（**语义不变**：仍是「脚本内硬编码名单 ↔ 外部真源」的漂移检测）；同时订正闸头与 `scripts/README.md:47` 里「与 SU · 六 一致」的**过期表述**。

---

## Work Units

> **TDD 5 步 canonical markers**（`rdd-doctor` 的 `plan-tdd` 检查）：
> 1. **Write the failing test** 2. **Run test to verify it fails** 3. **Write minimal implementation**
> 4. **Run test to verify it passes** 5. **Defer commit**
>
> 本条全部为**闸脚本 / 文档**改动 ⇒ 「测试」= **决定性变异脚本**（在 `/tmp/opencode/` 跑，不落仓）。

### WU-1: R-h 路径漂移（AC-6）

1. **Write the failing test**: `bash scripts/test/docs_sync_check.sh --strict` ⇒ 当前 **rc=1**，`缺失路径数:1`，点名 `scripts/README.md` 的 `fabric.json` —— **已实测红**（这就是 failing test，无需新写）
2. **Run test to verify it fails**: 同上，记录 ✗ 计数 = 1
3. **Write minimal implementation**: `scripts/README.md:55` 的 `` `fabric.json` `` → `` `include/tlm/cep/fabric.json` ``
4. **Run test to verify it passes**: `--strict` ⇒ **rc=0** + `缺失路径数:0`
5. **Defer commit**: 与 WU-2 合并一次 commit（同 `scripts/README.md`）

### WU-2: R-g 登记处对齐（AC-7 的登记半）

1. **Write the failing test**: 三个断言脚本（未登记清单 / 计数一致性）
   - `for f in scripts/test/cep_*.sh; do grep -q $(basename $f) scripts/README.md || echo MISS; done` ⇒ 断言输出为空
   - `grep -c '25 个' AGENTS.md` ⇒ 断言「0」且磁盘实际数 == 文中数
2. **Run test to verify it fails**: MISS = **2**（`cep_index_position_check.sh` / `cep_e2e_reconcile_check.sh`）；`AGENTS.md` 的「25 个」= **4 处**，磁盘实际 = **35**
3. **Write minimal implementation**:
   - `scripts/README.md`：守门表**补 2 行**（29 → 31 行）；`:74` 的口径行改成可核算式 `31 + 2（季度复审）+ 2（实验运行器）= 35 = ls scripts/test/cep_*.sh`
   - `scripts/README.md:47`：「与 SU · 六 族名单一致」→ 「与 `obj §6.2` B3b-2 行的族枚举一致」（WU-4 的连带）
   - `AGENTS.md` **4 处**数字订正（`:7` / `:217` / `:231` / `:614`）
4. **Run test to verify it passes**: MISS = 0；`grep -c '25 个'` = 0；`### 守门表` 行数 31；`31+2+2 == 35`
5. **Defer commit**: 与 WU-1 合并

> ⚠️ **同句内另两处计数漂移（本 WU 一并订正，须在 Entry #342 列明）**：`AGENTS.md` 的「`15 个` `cep_*_probe.cc`」实测 = **17**；`:7` 的「`30 份` main spec」实测 `openspec/specs/cep-*` = **37**。二者与本条「闸清单可信（数对）」是同一 defect class，留在同一句里会继续误导读者。

### WU-3: R-f 强度诚实化（AC-5）

1. **Write the failing test**: 断言 `bash scripts/test/cep_b4_observation_keys_check.sh 2>&1 | grep -c '\[synthetic-fixture\]'` ≥ 1
2. **Run test to verify it fails**: 当前输出 **0 命中**（闸仍在静默自合成 `trace.h5`）
3. **Write minimal implementation**: 闸头注释写明「本闸 = **文件层协议镜像校验**（`metrics.json` 为真实产出，`trace.h5` 由 metrics 镜像合成）；真实 run 的 h5 层校验归实验 4 件套运行时」；在 `exec python3 …check.py` 之前 `echo "[synthetic-fixture] trace.h5 由 metrics.json 镜像合成（非真实 run 产物；retention 丙 ⇒ experimental/* 不入库）"`
4. **Run test to verify it passes**: 命中 ≥ 1 且 **rc=0**；⛔ 全文无「已验证 trace 内容」类表述
5. **Defer commit**: 单独 commit（闸脚本）

### WU-4: R-e 族名交叉校验（AC-4）

1. **Write the failing test**: 变异脚本 —— 把 `obj §6.2` B3b-2 行的 `CXL 3.0` 改成 `CXL 3.1` ⇒ 断言闸**必红**且输出含「真源」
2. **Run test to verify it fails**: 当前闸 **仍绿**（`:57-61` 只 grep `$DECL` **自身** ⇒ 恒真，真源侧零校验）
3. **Write minimal implementation**:
   - **删** `:56-62` 的恒真 grep 段（python 段 `:26-48` **已有**集合判据 ⇒ ⛔ 不重复造）
   - **新增** python 交叉校验：定位 `obj` 中**恰好 1 行** `^- \*\*B3b-2\.`，要求 4 个族 token（`CXL 2.0` / `CXL 3.0` / `NVLink` / `UALink`）齐备，且 JSON 的 `family` 集合 == 映射后的 4 元素集合
   - 闸头 `:10` / `:13` 的「`07 号` SU·六」改为「`obj §6.2` B3b-2 行」
4. **Run test to verify it passes**: 基线绿（`4 PASS / 0 FAIL`）；3 个变异（删族 / 同族重复 / 改真源族名）逐条按**正确原因**红
5. **Defer commit**: 单独 commit（闸脚本）

### WU-5: R-a 生成器 + 闸三合一 + 索引重生成（AC-1 / AC-2 / AC-3）

1. **Write the failing test**: 变异脚本 3 条 —— ① 把一个 `## ` 降级为 `### ` ⇒ 索引闸必红 ② 改索引 A 任一行的「行」列 ⇒ 必红 ③ 改索引 A 的行数 ⇒ 必红
2. **Run test to verify it fails**: 当前闸对 ① **不红**（只比自述 vs 表行数，不看文件真实 `## ` 数）；且索引 A 现 **10/23 行号错**
3. **Write minimal implementation**:
   - 新增 `scripts/cep/generate_doc_index.py`：读 `07 号` ⇒ 产出「自述计数行 + 索引 A（全量 `## `，含实测行号）+ 索引 B（全量 SU）」，索引 C **不动**；`--write` 就地重写，`--check` 比对，无参打印
   - `07 号`：删 `:571` 的「另有 5 个未列入」排除声明；`:577` 的「逐条对应 / 23·9」重写为生成器口径；索引 A/B 由生成器全量重生成（自述 **28 / 15**）
   - `cep_index_position_check.sh`：三合一 + 行号逐行校验 + 标题集合相等（防遗漏 / 防重复）
4. **Run test to verify it passes**: 生成器连跑 2 次逐字相同；`--check` rc=0；闸基线绿（`一级节 28/28 · SU 15/15`）；3 条变异逐条按正确原因红
5. **Defer commit**: 产物 commit（`07 号` + 生成器 + 闸）

### WU-6: 台账 + 冻结链 + 全量回归（AC-8）

1. **Write the failing test**: `bash scripts/test/cep_freeze_check.sh`（无参）⇒ 预期**红**（`07 号` hash 变但清单未更新）
2. **Run test to verify it fails**: 记录红因 = `07 号` hash mismatch（`artifact_count` 仍 78）
3. **Write minimal implementation**（五步链，⛔ 不合并提交）：
   1. 台账 **Entry #342**（授权 + 回执 + oracle 处置 + 3 处实测订正 + §13.1 补 R-g）
   2. 产物 commit
   3. `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #342"`
   4. 清单单独 commit
   5. `bash scripts/test/cep_freeze_check.sh`（无参）⇒ rc=0
4. **Run test to verify it passes**: `ctest -j4` 全绿 + `[cep]` 全绿 + 35 闸 rc/✗ 双判全绿 + `artifact_count` == 78
5. **Defer commit**: 提交由用户确认（⛔ 不主动 commit / push）

---

## 交付物清单

| # | 文件 | 类型 |
|:--:|---|---|
| 1 | `scripts/cep/generate_doc_index.py` | 新增（生成器，**不入冻结集**） |
| 2 | `scripts/test/cep_index_position_check.sh` | 改（三合一 + 行号校验） |
| 3 | `scripts/test/cep_b3b2_deferral_check.sh` | 改（删恒真段 + 真源交叉校验） |
| 4 | `scripts/test/cep_b4_observation_keys_check.sh` | 改（`[synthetic-fixture]` 标注） |
| 5 | `docs/architecture/07-coherence-exploration-platform.md` | 改（**仅文末索引段**，冻结件 ⇒ 五步链） |
| 6 | `scripts/README.md` | 改（裸路径 + 2 行登记 + 计数口径 + :47 表述） |
| 7 | `AGENTS.md` | 改（4 处数字） |
| 8 | `docs/guide/cep-usage.md` | 改（`:118` 的登记处缺口声明改为「已于 2026-11-12 闭合」；⛔ §12.8 表格不动，其自述「本表不是状态源」） |
| 9 | `.rddf/roadmap/objectives/obj-cep-platform.md` | 改（Entry #342 + §13.1 补 R-g） |
| 10 | `.rddf/reviews/cep-governance-goal-scope-review.md` | 改（`:76` 的「冻结产物 77」笔误 → 78） |
| 11 | `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json` | 改（五步链第 3-4 步，仅 `07 号` hash） |
