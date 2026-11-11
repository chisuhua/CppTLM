# C1 · 闸强度与工具补齐 —— verifier 验收报告

> **对象**: `cpptlm-cep-gate-strength-tooling`（台账 `.rddf/roadmap/objectives/obj-cep-platform.md` §13.2 的 **C1**）
> **实施区间**: `b3f0226e..a08d6a4c`（5 个 commit，**未 push**）
> **验收时间**: 2026-11-12 · **验收角色**: `verifier`（用户口令切换）
> **结论**: ✅ **通过（PASS with conditions）** —— 9/9 AC 全部有工具证据；**0 BLOCKER**；4 项非阻断 finding（2 SHOULD_FIX / 2 NIT）
> **第 2 轮（2026-11-12，§八）**: ✅ **F-1 / F-2 / F-4 全部闭合**（各带工具证据 + 3 条决定性变异）;**F-3** 按建议维持 backlog（已登记 **O-6**）⇒ **C1 连同验收修补整体判通过（PASS）**
> **已用轮次**: **1 / 3**（`RDDF_VERIFIER_MAX_LOOPS=3` 对齐）

---

## 〇、判据来源

本 change **不新建 OpenSpec change**（产物 = 脚本 + 文档 ⇒ 不占 KPI slot）⇒ **不满足** `rdd-verifier` skill 的分流前置（`openspec/changes/<name>/proposal.md` 的 `## 验收标准` 段不存在 + P3 archive gate 未触发）⇒ 走 `verifier 产出契约` 的 **1–5 步本地验收**。

| 优先级 | 判据来源 | 本次取值 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 无（用户只给「切 verifier 做独立验收」） |
| 2 | `openspec/changes/<name>/proposal.md` 的 AC 段 | ⛔ 不存在（无 openspec change） |
| 3 | **本次变更自己声明了什么就验什么** | ✅ **本报告的主判据**：`.rddf/improvements/cpptlm-cep-gate-strength-tooling.md` 的 **9 条 AC** + `.rddf/plans/cpptlm-cep-gate-strength-tooling.md` 的 DoD 5 条 |
| 4 | ADR 的「Verification」章 | 不适用（本 change ⛔ 零 ADR 改动） |

**额外门**：`.rddf/state/iteration.json` **不存在**（无 openspec change ⇒ 该门不适用，非失败）。

---

## 一、逐条判定（9 条 AC）

### AC-1 生成器幂等 —— ✅ PASS

| 证据 | 命令 | 结果 |
|---|---|---|
| 副本连跑 2 次 | `python3 scripts/cep/generate_doc_index.py --doc <copy> --write` ×2 | 第 2 次「✓ 无变化(幂等)」；两版**逐字相同** |
| 仓内即不动点 | 同上 vs 仓内文件 | `diff -q` **无差异** |
| `--check` on 真实文件 | `python3 scripts/cep/generate_doc_index.py --check` | `✓ 索引段已同步`，rc=0 |
| 区间外零扰动 | 逐行比对 `旧 b3f0226e` vs 现文件 | 区间**前**（正文 + 索引标题）逐字相同 ✓；区间**后**（索引 C 及之后）逐字相同 ✓ |
| **索引 C 人工段保留** | `索引 C` 段逐字比对 | **True**（旧 29 行 / 新 29 行，未被生成器触碰）✓ |

### AC-2 决定性变异 · 索引完整性 —— ✅ PASS（6/6 红，红因正确）

全部以**独立 fixture**（`/tmp/opencode/v1/*.md` + 改路径的闸副本）执行，非改动仓内文件。

| 变异 | rc | 闸的红因（逐字） |
|---|:--:|---|
| 删掉 `## 10. References` 整段（索引不动） | 1 | `实际 `## ` 数 27 ≠ 自述「28 个一级节」` |
| 把 `## 7. …` 降级为 `### ` | 1 | `实际 `## ` 数 27 ≠ 自述「28 个一级节」` |
| 删索引 A 末行 | 1 | `索引 A 数据行数 27 ≠ 自述「28 个一级节」` |
| 删索引 B 一行 | 1 | `索引 B 数据行数 14 ≠ 自述「15 则 Status Update」` |
| 索引**之后**追加 `## 13.` | 1 | `## 13. 越界段`（违反「索引永久置于文末」纪律） |
| 复制一个 `## ` 标题（重复） | 1 | `文件存在重复的 `## ` 标题 1 个(行号无法唯一定位)` |

> ⛔ **无一条**靠「文件消失」才红 —— 全部按**语义**变红，满足本 change 的统一判据。

### AC-3 决定性变异 · 行号正确性 —— ✅ PASS

| 项 | 证据 |
|---|---|
| 变异 | 索引 A 第 3 行 `181` → `182` ⇒ rc=1，红因 `索引 A 第 3 行:行号 182 ≠ 实际 181('3. canonical 10 条 Coherence Interface 流')` |
| **正向全量**（独立复算） | 索引 A **28/28 行号全对，0 不对**；索引 B **15/15 行号全对，0 不对** |
| 相对基线 | 原实测「索引 A 行号 **10/23** 错」⇒ 现 **0 错**（生成器逐次重算） |

### AC-4 决定性变异 · 族名交叉校验 —— ✅ PASS（5/5 红）

| 变异 | rc | 红因 |
|---|:--:|---|
| 删 1 族（`ualink`） | 1 | `family set mismatch: got [...] expected [...]` |
| 同族加第 2 条 | 1 | `family set mismatch: got [... 'ualink', 'ualink']` |
| **改真源族名**（台账 `CXL 3.0` → `CXL 3.1`） | 1 | `真源缺族 token 'CXL 3.0'(cxl_3_0_symmetric)` |
| **改真源分母**（`0/4` → `0/5`） | 1 | `真源分母 5 ≠ 登记族数 4` |
| 台账缺 `B3b-2` 行 | 1 | `真源定位失败: `obj §6.2` 的 B3b-2 行命中 0 条(应恰好 1)` |

> **新判据独有**：后 3 条**旧判据全部不红**（见 AC-9 的并排证明）⇒ 证明「真源交叉校验」是真增量，非重复造。

### AC-5 强度可观测 · 不欺 —— ✅ PASS

| 项 | 证据 |
|---|---|
| 输出标注 | `bash scripts/test/cep_b4_observation_keys_check.sh` ⇒ 首行 `[synthetic-fixture] trace.h5 由 metrics.json 镜像合成(非真实 run 产物;retention 丙 ⇒ experimental/* 不入库)…`；`grep -c '\[synthetic-fixture\]'` = **1**；rc=0 |
| 闸头强度边界 | 注释含「本闸 = **文件层协议镜像校验**」+「**真实 run 的 h5 层校验归实验 4 件套运行时**」 ✓ |
| ⛔ 无「已验证 trace 内容」类表述 | 全文件 grep：`唯一命中是第 9 行的**禁令本身**`（「⛔ 不得宣称本闸验证了 trace 内容」）⇒ 无违规声明 |
| ⛔ 无「优先真实 run」半死分支 | 旧版与新版 grep `glob|real|existing|Path(`：**活代码 0 处**；新版仅 2 行**说明性注释**（解释为何不做）⇒ MUST-5 落实正确 |

### AC-6 路径漂移清零 —— ✅ PASS

`bash scripts/test/docs_sync_check.sh --strict` ⇒ **rc=0**，输出 `缺失路径数：0` + `✅ 所有路径引用都有效`（修复前 rc=1、缺失 1，点名 `scripts/README.md` 的裸 `include/tlm/cep/fabric.json`）。

### AC-7 闸注册 · 保持 —— ✅ PASS

| 项 | 证据 |
|---|---|
| ctest 列出 3 闸 | `ctest --test-dir build -N` ⇒ `#137 cep_b4_observation_keys_check` / `#152 cep_b3b2_deferral_check` / `#160 cep_index_position_check` |
| ⛔ 未动 CMakeLists | `git diff b3f0226e..HEAD -- test/CMakeLists.txt` **为空** ✓（符合 Out-of-Scope） |
| `scripts/README.md` 登记 | 3 闸均有登记行（`:48` / `:50` / `:58`） |
| **登记面计数自洽**（独立按 section 边界复算） | 守门表 **31** + 季度复审 **2** + 实验运行器 **2** = **35** = `ls scripts/test/cep_*.sh` = **35**，**一一对应 True、无重复登记** |

### AC-8 0 回归 —— ✅ PASS

| 面 | 独立复跑结果 |
|---|---|
| `ctest --test-dir build -j4` | **168 / 168 passed, 0 failed**（136.93s） |
| `[cep]` Catch2 | **All tests passed (11516 assertions in 508 test cases)** |
| **35 闸 rc 双判** | **35 / 35 rc=0**（逐个 `bash` 跑，✗ 计数双判） |

> ⚠️ 唯一 1 条 `✗` 出现在 `scripts/test/cep_freeze_reverse_verify.sh` 的**「故意破坏」步内**（破坏 `include/tlm/cep/fabric.json` ⇒ freeze check 报红并点名）—— 那是**反向验证的预期证据**，该闸 rc=0 ⇒ 反向验证成立。`scripts/test/cep_freeze_check.sh` 的 `frozen_at_commit ≠ HEAD` 提示是**产物 commit 之后另有 2 个非产物 commit** 的固有噪声，**非本 change 引入**。

### AC-9 无自证 —— ✅ PASS（**旧闸 vs 新闸并排证明**）

把两个闸的**旧版**（`git show b3f0226e:…`）与**新版**指向同一破坏 fixture 对跑：

| 破坏 | **旧闸** | **新闸** | 结论 |
|---|---|---|---|
| 删掉 `07 号` 一个 body `## ` 段（索引不动） | **rc=0 绿**（假绿：`✓ CEP 索引位置闸通过(索引在文末 + 计数自洽)`） | rc=1 红：`实际 `## ` 数 27 ≠ 自述「28 个一级节」` | 新判据**有牙**，红因正确 |
| 台账真源族名漂移（`CXL 3.0`→`CXL 3.1`） | **rc=0 绿**（假绿：`✅ 族名单: 4 族齐全`） | rc=1 红：`真源缺族 token 'CXL 3.0'` | 新判据**有牙**，红因正确 |

**期望值来源独立性核查**：
- 索引闸 = **文件自身的 `## ` 标题结构**（⛔ 不是索引自述 —— 索引自述正是被测对象）
- b3b2 闸 = **台账 `obj §6.2` 的 B3b-2 行族枚举 + 分母**（外部真源，⛔ 不是被测 JSON）
- 生成器 = 从标题结构**机械产出**（⛔ 无任何期望值常量写死）
⇒ 三者均**不依赖「文件缺失 / 字符串自搜」**式恒真判据 ✓

---

## 二、边界与错误路径（构建 agent 未做这层）

| # | 场景 | 期望 | 实测 |
|:--:|---|---|---|
| E1 | 文档缺 `## §末 文档索引` 标题 | 拒绝写盘 + 明确文案 | rc=1，`✗ 期望恰好 1 个含「§末 文档索引」的 `## ` 标题,实得 0 个` ✓ |
| E2 | 文档缺 `### 索引 C` 小节 | 拒绝写盘 | rc=1，`✗ 未找到 `### 索引 C` 小节(索引 C 必须存在且位于索引 A/B 之后)` ✓ |
| E3 | 标题含 `|`（会破表格） | 拒绝写盘 | rc=1，`✗ 标题含 `|`,无法安全写入 Markdown 表格` ✓ |
| E4 | 目标文件不存在 | 明确失败 | rc=2，`✗ 未找到 …` ✓ |
| E5 | 冻结面 `--update` **无** `--authorized-by` | 拒绝写盘 | rc=1，`✗ 缺少 --authorized-by <台账 entry 标题> —— **拒绝写盘**` ✓ |
| E6 | 冻结面 `--update --authorized-by "Entry #999"`（不存在的 entry） | 拒绝写盘 | rc=1，`✗ 授权校验失败:台账 … 中没有 id 为 'Entry #999' 的 entry`；**清单 0 处改动** ✓ |
| E7 | 冻结面 manifest 的 artifact 是否全在磁盘 | 全部存在 | `artifact_count=78` / 实际条目 **78** / 缺失 **0** ✓ |

---

## 三、Out-of-Scope 核对（逐条实测）

| 声明 | 实测 |
|---|---|
| ⛔ 零 ADR 改动 | `git diff --name-only b3f0226e..HEAD -- docs/adr/` = **0 个** |
| ⛔ 零 schema / 声明面 / ABI 改动 | diff 内无 `openspec/specs/`、无 `include/abi/`、无 `.json`（除冻结清单）✓ |
| ⛔ 不动 `test/CMakeLists.txt` | diff **为空** ✓ |
| ⛔ 不改 `07 号` 的判据内容 | `07 号` 全部 diff hunk 起始行 = **571 / 575 / 577 / 579 / 593 / 611** ⇒ **全部落在索引标题（569）之后**；正文与判据区（1–568）**零改动** ✓ |
| ⛔ 不动 B3b-2 计数 | 台账 `- **B3b-2. …**` 仍为 **`**0/4**`** ✓ |
| ⛔ 不新增入库 run 产物 | diff 内 `experimental/` = **0 个** ✓ |
| ⛔ `artifact_count` 仍 78 | manifest `artifact_count=78`、`authorized_by=Entry #342`、`frozen_at_commit=0704b7ca` ✓ |
| ⛔ 不占 KPI slot | `openspec/changes/` 下 **0 个 active** ✓ |

---

## 四、冻结面五步授权链（独立复验）

| 步 | 实测 |
|:--:|---|
| 1 台账授权 | `### Entry #342` 存在（台账 Entry 总数 **342**）✓ |
| 2 产物 commit | `0704b7ca` ✓ |
| 3 `--update --authorized-by "Entry #342"` | `✓ 授权校验通过:台账 entry 'Entry #342'` ✓ |
| 4 清单单独 commit | `7e67d8ff` ✓ |
| 5 无参复验 | **rc=0**；`✓ provenance 自检通过:78 个产物的 hash 与 commit 0704b7ca4063 的内容一致` ✓ |

---

## 五、发现（4 项，**均非 BLOCKER**）

### F-1 · [SHOULD_FIX] `scripts/test/cep_decl_reverse_verify.sh` 在守门表内，但**从未进入 CI**

**事实**（实测）：
- 它**在** `scripts/README.md` 的守门表（31 行）内，状态列写「✅ 生效」；
- `test/CMakeLists.txt` **无** `add_test(NAME cep_decl_reverse_verify …)`；`ctest -N` 的 **14 个** `*_reverse_verify` 项里**没有它**；
- 全仓 grep：**没有任何闸或 ctest 项执行它**（`cep_decl_alignment_check.sh:545` 只是**注释里提及**）；
- 而 `scripts/test/cep_decl_alignment_check.sh` 的输出却声称「4 条反向 fixture: ✓」⇒ **读者会误以为反向验证已被 CI 覆盖**。

**为何不是 BLOCKER**：本 change 的 R-g 目标是「**闸清单的登记与计数**与磁盘对齐」，该项的**计数与登记**已对齐（31+2+2=35，逐条一一对应）✔；且该脚本**不在** C1 声明的 5 个子项内 ⇒ **不违反任何一条 AC**。
**但**：它与 C1 要消灭的「**登记了，但没有牙齿**」是**同一缺陷类**，且 `scripts/README.md:77` 的字面表述「**本表 29 个脚本**全部已注册进 `ctest`」对它**不成立**（且「29」在表变成 31 行后已过期 —— C1 新增的 `:80` 注记解释了 29→31 的来龙去脉，但没改 `:77` 的字面数）。
**建议**：单独一条小修 —— 把 `:77` 的「29」订正为「31」并注明 `scripts/test/cep_decl_reverse_verify.sh` 是**间接/手工覆盖**（或把它注册进 ctest）。

### F-2 · [SHOULD_FIX · 潜伏] 生成器 `--write` 对「索引标题 → 索引 C」区间内的 **body 内容是静默删除**

**事实**（实测）：在 `## §末 文档索引` 标题**之后**插入一个 `## 13. 插在索引里的新段`（含正文「内容」），跑 `--write` ⇒ 该段的**正文被删除**，但**索引 A 仍留下一行** `| 29 | 13. 插在索引里的新段 | 571 |` ⇒ **索引声称它存在、正文却没了**。

**当前风险面**：现 `07 号` 的该区间（569–632）内**只有生成内容**（非表格非引用行仅 2 行 = `### 索引 A` / `### 索引 B` 两个小标题）⇒ **今天没有实际内容会被删**。
**为什么仍要记**：① 闸能**在 `--write` 之前**红（`实际 `## ` 数 29 ≠ 自述 28`）⇒ 但 `07 号` 索引头的文档序是「每次追加后重生成」，即 **`--write` 在前**；② `--write` 是**破坏性**动作，却输出「✅ 已重写索引段」，**无任何告警**。
**建议**：`--write` 检测到区间内存在**非生成内容**时**拒绝写盘并打印该内容的位置**（与 E1/E2 的错误路径同风格）。

### F-3 · [NIT · 潜伏] 代码围栏内的 `## ` 会被生成器与闸**同口径**误计

**事实**（实测）：在 `07 号` 里放一个含 `## 这是一个示例标题（在围栏内）` 的围栏 ⇒ 生成器自述「**29 个一级节**」，闸也数到 **29** ⇒ **两者一致 ⇒ 闸不红**，但索引多出一条不存在的章节、后续行号全体偏移。
**当前状态**：现 `07 号` 围栏内 `## ` 命中数 = **0** ⇒ **今天没有实际错误**，且「排除围栏后真实一级节数 = 28」与生成器口径一致。
**性质**：生成器与闸**共用同一解析启发式** ⇒ 闸无法发现生成器的**误判**（属 AC-9「期望值独立」精神的盲区，但**不违反** AC-9 的字面要求 —— 判据相对**索引**是独立的）。
**建议**：记 backlog —— 未来若 `07 号` 需要嵌入 Markdown 示例，生成器与闸应改为**围栏感知**解析（两侧同时改，否则口径分裂更糟）。

### F-4 · [NIT] improvement 草案的 9 条 AC 仍是 **0/9 未勾**

**事实**：`.rddf/improvements/cpptlm-cep-gate-strength-tooling.md` 的 9 条 `- [ ]` 全部未勾；而仓库有已勾先例（`.rddf/improvements/cpptlm-cep-umbrella.md` 26/26、`.rddf/improvements/cpptlm-cep-nvlink-sharp.md` 9/9）。
**为何不是问题**：台账 **Entry #342** 已是实施回执的**在册载体**，本报告是验收载体 ⇒ 勾选态**非状态源**。
**建议**：回 builder 顺手勾上（避免「实现已闭合但勾选态是 0/9」这种读法歧义）。

---

## 六、遗留清扫复核（本次范围内）

| 检查项 | 结果 |
|---|---|
| TODO / FIXME / XXX / 调试开关（`set -x` / `print("DEBUG`） | **0**（15 个改动文件全扫） |
| 被 skip 的测试（`skip` / `pytest.skip` / `@unittest.skip`） | **0** |
| 注释掉的代码块（`#` 开头的 `if/for/while/def/return/import/…`） | **0** |
| 未替换的 mock / 临时 fallback / 「for future use」死代码 | **0** |
| 语法检查 | `bash -n` × 3 闸 ✓；`python3 -m py_compile scripts/cep/generate_doc_index.py` ✓；`__pycache__` 已清 |
| 3 处清扫命中（`AGENTS.md` 的「TODO 残留」规则文本、`obj:529` 登记形态、`cep-usage.md:168` 的 `Entry #XXX`） | **均为既有文本，逐条查 diff 确认不在本 change 的 diff 内** ⇒ 非本 change 引入 |
| 登记处可达性 | 新工具 `scripts/cep/generate_doc_index.py` 在 `scripts/README.md:21` 可查到；`AGENTS.md` STRUCTURE 的 scripts 行**已含** `cep/` ✓ |

**范围外的历史遗留**（只记录、不在本 change 顺手清理）：`scripts/README.md:77` 的「29 个」字面数与「全部已注册 ctest」表述（= F-1）。

---

## 七、路由建议

| 项 | 判定 | 路由 |
|---|---|---|
| **9 条 AC** | ✅ 全 PASS | **C1 可判通过（PASS with conditions）** |
| **F-1** | SHOULD_FIX（登记面残留 + 同缺陷类） | 建议**回 builder**：单独小修（改 `:77` 字面数 + 标注 `scripts/test/cep_decl_reverse_verify.sh` 的覆盖口径，或注册进 ctest） |
| **F-2** | SHOULD_FIX（潜伏破坏性写） | 建议**回 builder**：`--write` 加「区间内存在非生成内容 ⇒ 拒绝写盘」的护栏 |
| **F-3** | NIT（潜伏，当前无实际错误） | 记 **backlog**（围栏感知解析），⛔ 不在本 change 修 |
| **F-4** | NIT（勾选态） | 建议**回 builder** 顺手勾 9 条 AC |

**无 BLOCKER** ⇒ 本 change **不需要**打回重做，也**不需要**第 2 轮验收。
**建议顺序**：F-1 + F-2 + F-4 可合成**一笔小修**（同属 `scripts/README.md` + 闸脚本 + improvement 文件）⇒ 修完只需**复跑**：`scripts/test/cep_index_position_check.sh` + `scripts/test/cep_b3b2_deferral_check.sh` + `docs_sync_check.sh --strict` + `ctest -j4`（35 闸全绿），**无需**再走冻结面五步链（不动 `07 号`）。

**已用轮次：1 / 3** —— 本报告为第 1 轮；若 F-1/F-2/F-4 修完复验，为第 2 轮（仍在预算内）。

---

## 八、第 2 轮复验（F-1 / F-2 / F-4 闭合确认）· 2026-11-12

> **触发**:per 用户口令「执行建议了下一步内容」⇒ 回 `builder` 实施 F-1 / F-2 / F-4（台账 **Entry #343**）⇒ 本节为修完后的**第 2 轮独立复验**。
> **判据**:本报告 §七 的路由建议 + Entry #343 的实施回执。**已用轮次：2 / 3**。

### 8.1 F-1 闭合 ✅

| 判据 | 实测 |
|---|---|
| `scripts/test/cep_decl_reverse_verify.sh` **真进 CI** | `test/CMakeLists.txt` 补 `add_test(NAME cep_decl_reverse_verify …)`;`ctest --test-dir build -N` ⇒ **Test #135**;**Total Tests 168 → 169** |
| **守门表 31 个脚本名逐条出现在 `ctest -N`** | **缺失 0 个** ⇒ `scripts/README.md` 「本表 31 个脚本**全部**已注册进 `ctest`」自此**机器可核为真**（不再是自述） |
| 该闸稳定性 | **3 连跑 rc=0** · ~5.7s · **不写仓内文件**（`git status` 干净）⇒ 可安全并行 |
| `:77` 字面「29 个」 | 已订正为「**31 个**」;`:80` 的引用同步订正;并追加 2026-11-12 补注册注记 |
| **计数口径未破** | 守门表仍 **31** + 季度复审 **2** + 实验 **2** = **35** = 磁盘 **35**（补注册**不新增磁盘脚本**）⇒ `AGENTS.md` 4 处「35 个」**无需再改** |

### 8.2 F-2 闭合 ✅

| 判据 | 实测 |
|---|---|
| 护栏存在 | `scripts/cep/generate_doc_index.py` 新增 `is_generatable()` / `find_foreign()` / `assert_block_generatable()`（白名单 = 空行 / 表格行 / 引用行 / `### ` 小节标题） |
| **变异①** `--write` 遇外来内容 | **rc=1** 且**文件逐字零改动**（`diff -q` 通过）、外来正文仍在 ⇒ 证「拒绝」而非「静默删」 |
| **变异②** `--check` 遇外来内容 | **rc=1**（**不只挡写盘**，同口径拦） |
| **变异③** 真实文件 | `--check` **rc=0**;直接调用 `find_foreign()` ⇒ 外来内容数 **0**（区间 62 行）⇒ **不误报** |
| 双向自洽 | 生成块自身也须落在白名单内（`assert_block_generatable`）⇒ 防「闸与生成器口径分裂」 |

### 8.3 F-4 闭合 ✅

- 9 条 AC `- [ ]` → `- [x]`（**9/9**）;状态行 `🟡 待实施` → `✅ 已实施并验收`。
- 勾选同时**就地订正 2 处会误导读者的文本**:① **AC-4** 的靶点（`07 号` SU·六 → `obj §6.2` 的 `B3b-2` 行，per Entry #342 §③）② **AC-7** 的范围（CMakeLists 的改动属 **Entry #343** 独立一笔，非 C1）。

### 8.4 F-3 维持 backlog ✅（明确不做）

- **未修**，已登记为台账 **O-6**（§「可选」行）⇒ 与本报告 §五 F-3 的建议一致。
- 当前 `07 号` 围栏内 `## ` 命中 **0** ⇒ **今天无实际错误**;修它须**生成器与闸同时**改（只改一侧会让口径分裂，比现状更糟）⇒ 独立一笔。

### 8.5 修完后的全量回归（独立复跑）

| 面 | 实测 |
|---|---|
| `ctest --test-dir build -j4` | **169 / 169 passed, 0 failed**（135.57s）—— 含新项 `cep_decl_reverse_verify`（#135 · 6.63s） |
| `[cep]` Catch2 | **All tests passed (11516 assertions in 508 test cases)** |
| **35 闸 rc + ✗ 双判** | **35 / 35 rc=0**（唯一 1 条 `✗` 在 `scripts/test/cep_freeze_reverse_verify.sh` 的**「故意破坏」步内** ⇒ 反向验证的**预期证据**） |
| 3 闸 + 冻结面 + 文档面 | 4 个被触闸 rc=0 · `cep_freeze_check` rc=0 · `docs_sync_check --strict` rc=0 · `agents_md_lint` rc=0 · `generate_doc_index --check` rc=0 · `py_compile` ok |
| 冻结面 | `07 号` **未改** ⇒ `artifact_count` 仍 **78**、hash 未变 ⇒ **未走五步链**（正确 —— 不需要） |
| 遗留清扫（6 个改动文件） | **0** TODO / FIXME / 调试开关 / skip / 死代码;2 处初筛命中经查 diff 确认**均为既有文本** |

### 8.6 第 2 轮结论

> ✅ **F-1 / F-2 / F-4 全部闭合**（各有工具证据，含 3 条决定性变异）;**F-3** 按建议维持 backlog 并已登记（**O-6**）。
> **无新增 BLOCKER / SHOULD_FIX** ⇒ **C1 连同其验收修补整体判通过（PASS）**。
> **已用轮次：2 / 3**（余 1 轮，用于任何后续修补的复验）。
