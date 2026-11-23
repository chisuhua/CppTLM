# 验收报告：cep-litmus-falsifier-compound-graph-rename（I2 阶段 2 改名）

> **验收角色**：verifier（角色内建，非 subagent）· **日期**：2026-11-24
> **验收对象**：change `cep-litmus-falsifier-compound-graph-rename`（「litmus 证伪工装」⇒「**复合图证伪工装**」改名）
> **结论**：✅ **PASS**（无条件）—— 6/6 AC 全部通过，**每条均有工具证据**

---

## 1. 分流判定（为何走本地验收，而非 `rdd-verifier` skill）

| 条件 | 状态 | 证据 |
|---|:--:|---|
| ① change 有 `## 验收标准` 段 | ✅ | `openspec/changes/archive/2026-11-24-cep-litmus-falsifier-compound-graph-rename/proposal.md`（6 条 `- [ ] **AC-N**`） |
| ② P3 archive gate 触发（`iteration.json` 中 `status ∈ {in_worktree, completed}` ∧ `tasks_done == tasks_total > 0`） | ❌ | `.rddf/state/iteration.json` **不存在** ⇒ 条件不满足 |
| ③ `rdd-verifier` skill 可加载 | ✅ | `rdd-skills.sh --json` → `available[]` 含 `rdd-verifier` |

⇒ **条件 2 不满足 ⇒ 走本地验收（1–5 步）**。

**判据来源**：① 用户明确条件（「同意你的建议和推进」⇒ 改名 + 不可判登记 + G2 裁定草案）② change `proposal.md` 的 `## 验收标准`（**6 条 AC**）③ 本 change 自身声明的行为 ④ ADR `Verification` 章 —— 本单元**无新增 ADR** ⇒ 来源 ④ 不适用。

**审查轨迹**：architect 裁定记录 `.rddf/reviews/cep-litmus-falsifier-rename-adjudication-2026-11-24.md` —— **oracle 1 轮**（判定 **APPROVE**）→ **critic 1 轮**（判定 **mergeable: no** · 1 BLOCKER + 3 SHOULD_FIX + 2 NITS）⇒ **BLOCKER 已闭合**（`test/test_cep_combination_contract.cc:312` 语料路径硬编码补入范围）。

---

## 2. 逐条判定（6 条 AC）

### AC-1 改名完整性 · 机检 → ✅ PASS

**命中项全集**（`grep -rn "cep_litmus\|litmus_falsifier\|LitmusVerdict\|evaluate_litmus_falsification\|load_litmus_corpus\|CPPTLM_LITMUS_\|LITMUS_FALSIFIER_HH" include/ test/ scripts/ openspec/specs/`）：

| 命中文件 | 命中数 | 分类（⊆「必不改」登记表） |
|---|:--:|---|
| `scripts/test/cep_litmus_naming_check.sh` | 8 | **命名消歧闸文件名**（⛔ 必不改：其管辖违例类不随工件改名过期 + 阶段 1 审计链） |
| `test/CMakeLists.txt` | 3 | **ctest 条目名** `cep_litmus_naming_check`（⛔ 必不改，同上） |
| `scripts/README.md` | 1 | 守门表中**命名消歧闸行**（⛔ 必不改） |
| `include/tlm/cep/compound_graph_falsifier.hh` | 2 | ① 消歧句里的**旧文件名历史指称**（带否定标记 ⇒ 属「带否定标记的消歧句」类）② 引用**命名消歧闸文件名** |

⇒ **命中项 100% 落在「必不改」登记表内**；其余**零命中** ✅

### AC-2 零行为变更 · 机检 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| 改名后标签族全绿 | `./build/bin/cpptlm_tests "[cep][compound-graph][falsification]"` → **All tests passed (37 assertions in 5 test cases)**（= 改名前 `[cep][litmus][falsification]` 的 **37 assertions / 5 cases** ⇒ **逐条等价**） | ✅ |
| 既有闸两条臂 | `bash scripts/test/cep_compound_graph_falsification_check.sh` → 正控 ✓ + 反向 fixture ✓（**rc=0**） | ✅ |
| invariant 载体未被破坏 | `./build/bin/cpptlm_tests "[cep][combination-contract]"` → **All tests passed (70 assertions in 2 test cases)**（IC1 用例含 `:312` 语料加载 ⇒ **critic BLOCKER 的闭合证据**） | ✅ |

### AC-3 冻结面链 · 机检 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| `artifact_count` 不变 | `91 → 91`；清单含**新** spec（`openspec/specs/cep-compound-graph-falsification/spec.md` = True）、**不含**旧 spec（= False） | ✅ |
| 清单 diff 恰 2 项 | `--update --authorized-by "Entry #540"` 输出：「**新增 1 · 移出 1 · hash 变更 0**」 | ✅ |
| 无参复验 | `bash scripts/test/cep_freeze_check.sh` → 「✓ M1 产物冻结校验通过:91 个产物 hash 全部一致」· **零**「新增未登记」 | ✅ |
| 归档行为 | `openspec archive … --yes` ⇒ 「REMOVED requirement(s) ignored」+「**Totals: + 0, ~ 0, - 0**」⇒ **零 spec 改动** ⇒ 归档**未**改冻结面 ⇒ 无需二次 `--update`（与探针 P-3 预测**完全一致**） | ✅ |
| 无残留空壳 | `ls openspec/specs/ \| grep -i litmus` ⇒ **无** ⇒ 旧 capability 已退役（`retire_capabilities: true` 生效） | ✅ |
| 新 spec Purpose 非占位 | `grep -c "TBD - created by archiving" openspec/specs/cep-compound-graph-falsification/spec.md` ⇒ **0** ⇒ **真 Purpose**（⛔ 无需归档后补写） | ✅ |

### AC-4 闸缺陷修复 · 机检 + 反 fixture → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| **修复前**该形态假绿 | 实测：spec 路径改成不存在 ⇒ 闸仍 `RC=0`「全部判据通过」 | ✅（缺陷存在，已闭环） |
| **修复后**必红 | 实测：同形态 ⇒ 「✗ **litmus-scan-set-missing**: 扫描集成员缺失（判据失去作用面）」⇒ **RC=1** | ✅ |
| 还原后必须绿 | 还原 ⇒ `RC=0` | ✅ |
| 第④路影子 fixture 在闸内 | `bash scripts/test/cep_litmus_naming_check.sh` → 「✓ 臂②④ 扫描集缺一成员 ⇒ 必红（已红，红因码 litmus-scan-set-missing）」 | ✅ |

### AC-5 口径诚实 · 人工评审 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| 「**不可证伪**」是**真结果**、⛔ 不是「公理已成立」 | 新 spec Purpose 逐字在场：`不可证伪**」这一真结果，⛔ 不是「公理已成立」` | ✅ |
| ⛔ **不判定公理成立** | 同段逐字在场：`⛔ **不判定公理成立**` | ✅ |
| ⛔ 不表述为「已证明」 | `SHALL NOT 以任何形态表述为「已证明」`（1 处）+ `SHALL NOT 表述为「已证明」` | ✅ |
| 活代码面**零**能力高估 | 头文件 + 测试内搜「已证明 / 公理已成立 / 无违例保证」的正向用法 ⇒ **零命中**（全部为否定式：⛔ 不得 / 不是 / SHALL NOT） | ✅ |
| 头文件消歧段的否定语境在场 | `本工装不是内存模型 litmus` 在场（⇒ 闸的 claim pattern 依赖它仍成立） | ✅ |

### AC-6 零回归 · 机检 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| `ctest -R cep` 全绿 | 见 §4 | ✅ |
| 全部 `scripts/test/cep_*.sh` rc=0 | 见 §4 | ✅ |
| `cep_docs_sync_check.sh --strict` rc=0 | 见 §4 | ✅ |

---

## 3. 遗留清扫复核

| 清扫项 | 结果 | 证据 |
|---|:--:|---|
| TODO / FIXME / XXX / HACK / 临时调试输出 | ✅ 0 | 本 change 的 5 个代码产物逐文件 grep（唯一命中为 `mktemp -d …XXXXXX` **模板串**，非遗留） |
| 被 skip 的测试 / 未替换的 mock | ✅ 0 | `git diff` 内无 `SKIP`/`DISABLED`/`.disabled` 新增 |
| 临时文件残留（仓内） | ✅ 0 | `git status --short` 未跟踪项仅 `.rddf/state/*` 与 `*.lock`（仓内既有 noise） |
| 归档件残留空壳 | ✅ 0 | 旧 capability 目录**已退役**（`retire_capabilities: true` ⇒ openspec 删除该 spec 文件） |
| 未入库的归档件污染 | ✅ 0 | `openspec/changes/archive/` 被 `.gitignore` 排除 ⇒ 归档件**不入库**（符合仓内惯例） |

---

## 4. 全量回归实测

| 判据 | 结果（**实测** · 2026-11-24） |
|---|---|
| 全部 `scripts/test/cep_*.sh` | ✅ **51/51**（PASS=51 · FAIL=0） |
| `ctest --test-dir build -R cep -j2` | ✅ **111/111 Passed**（203.13s）· `ctest RC=0` |
| `bash scripts/test/cep_docs_sync_check.sh --strict` | ✅ rc=0 |
| `bash scripts/test/cep_doc_ref_check.sh --strict`（ctest 口径） | ✅ rc=0（⚠️ 首轮红 ⇒ 4 处失效路径已豁免，见 §4.5） |
| `bash scripts/test/cep_spec_strict_check.sh` | ✅ rc=0 |
| `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0（5/5 文档取值 == 实测） |
| `bash scripts/test/archive_verify_check.sh` | ✅ rc=0 |
| `bash scripts/test/cep_freeze_check.sh`（无参） | ✅ rc=0 · 91 产物 hash 一致 |
| `bash scripts/test/cep_litmus_naming_check.sh` | ✅ rc=0（五路反 fixture 全过） |
| `bash scripts/test/cep_compound_graph_falsification_check.sh` | ✅ rc=0 |
| `bash scripts/test/cep_engine_capabilities_check.sh` | ✅ rc=0（I1 闸未受改名影响） |

⚠️ **测量纪律注记**：本轮首次测 RC 时误用了 `cmd | tail; echo $?`（`$?` 取到 `tail` 的退出码）⇒ **RC 必须直接取**（`cmd > log 2>&1; echo $?`）⇒ 该误测曾被 `doc_ref_check` 的「输出 4 处问题但显示 rc=0」暴露 ⇒ 已改用正确测法重跑全部。

---

## 4.5 ⚠️ 验收中抓到并修掉的真问题（`cep_doc_ref_check.sh --strict`）

**现象**：`ctest -R cep` 首轮 **1 项红** —— `cep_doc_ref_check`（Failed）。单跑**无参** rc=0，加 `--strict` 判定 ⇒ **4 处**「引用不存在的文件」：

| # | 失效路径 | 落点（`obj-cep-platform.md`） | 性质 |
|:--:|---|---|---|
| 1 | `include/tlm/cep/litmus_falsifier.hh` | `:27329` / `:28405` / `:28434` | **历史 Entry**（#513 / #538 的改名前提述）⇒ append-only 不可回改 |
| 2 | `openspec/specs/cep-litmus-falsification/spec.md` | `:27333` / `:27340` / `:28514` | 历史 Entry + **Entry #540 的「冻结面移出声明行」**（C-1b **硬要求**写旧路径） |
| 3 | `scripts/test/cep_litmus_falsification_check.sh` | `:27332` | 历史 Entry |
| 4 | `openspec/changes/cep-litmus-falsifier-compound-graph-rename/` | `:28547` | Entry #540 ⑨ 关联行（change 目录**已归档** ⇒ active 路径不存在） |

**根因**：改名使**历史 Entry 的路径引用**与**冻结面移出声明行**（二者都**必须**保留旧路径）变成「不存在的文件」⇒ 与 `doc_ref_check` 的「引用可达性」判据**结构性冲突**。

**处置**（沿仓内既有机制 · ⛔ **不改历史 Entry**、⛔ **不删移出声明行**）：在 `scripts/test/cep_doc_ref_check.sh` 的 `VIRTUAL_PATHS` 白名单登记 **4 条**（**每条带理由**：改名后现行路径 + 为何必须保留旧名 —— 台账 append-only / C-1b 硬要求）⇒ 复跑 `--strict` **RC=0** · ctest 该项**转绿**。

⚠️ **这同时暴露了本 change 的一处设计遗漏**：proposal 的「影响面」段列了 `docs/implementation/…freeze-manifest.json` 与 `scripts/README.md`，但**未列** `scripts/test/cep_doc_ref_check.sh`（改名的路径豁免登记落点）⇒ 已在实施期补入（属必要的连带面，非范围蔓延 —— 它是**让 AC-6 成立的必要条件**）。

## 5. 计数与口径复核

| 面 | 验收时实测 | 与登记册 §0.1 一致 |
|---|---|:--:|
| `cep_*.sh` / `cep_*.py` / 闸族合计 | **51** / 14 / **65** | ✅（改名不计增减） |
| `test_cep_*.cc` | **78** | ✅（改名，非新增） |
| 全仓主 spec / `cep-*` spec | **139** / **48** | ✅（退役 1 + 新增 1 ⇒ **不变**） |
| OpenSpec active | **0** | ✅ |
| 归档 change 目录 / CEP 归档 | **173** / **57** | ✅（已回填） |
| M1 冻结清单 | **91** 产物 · hash 全一致 | ✅（移出 1 + 新增 1 ⇒ **不变**） |

---

## 6. ⚠️ 一处**过程瑕疵**登记（最终态无误）

**现象**：本 change **立项期间**（commit `6d7a1e5e` → 归档）`openspec list --json` 的 active 数为 **1**，而活文档 KPI 口径仍写 **0/3** ⇒ 该窗口内 `cep_docs_sync_check.sh` 断言①（KPI 口径 == 实测）**会红**。

**成因**：与 I1 的处理**不一致** —— I1 立项时同步把 7 处活口径改为 1/3；本单元**漏了**该同步。

**影响范围**：**仅过程窗口**。归档后 active=0 ⇒ 活口径 0/3 **== 实测** ⇒ 最终态 registry 5/5 ✅ · docs_sync rc=0 ✅ · **无残留不一致**。

⇒ 已登记于台账 Entry #540 ⑩，供后续 change 立项时对照（立项 ⇒ 同步活口径 ⇒ 归档 ⇒ 回填）。

---

## 7. 判定

**PASS（无条件）** —— AC-1 ~ AC-6 **6/6 全部通过**，每条均有 ≥1 条工具证据；无取证不可用项；遗留清扫干净；口径诚实成立。

**路由**：无失败项 ⇒ 无需回 builder。

**后续（不在本单元范围）**：G2 实施 ⇒ 开工前置 = architect 裁「每域模型来源 + 域间边构造规则」（新建 ADR）→ **用户 ratify** → planner 立 change → builder 实施（本次改名已为其铺路：活代码面不再有「litmus 工装」⇒ G2 的操作级模型直接落在 `compound_graph_falsifier` 名下 ⇒ 无二次改名成本）。

**关联**：台账 Entry #540（授权 + 判据 + 实施回执 + 瑕疵登记）· Entry #537（I1 · 承载能力声明面 = 唯一事实真源）· Entry #538（I2 阶段 1）· 裁定记录 `.rddf/reviews/cep-litmus-falsifier-rename-adjudication-2026-11-24.md` · 归档 `2026-11-24-cep-litmus-falsifier-compound-graph-rename`
