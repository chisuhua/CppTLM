---
name: cpptlm-cep-gate-strength-tooling
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— CEP 闸强度与工具补齐
依赖:
  - ADR-CEP-06
  - ADR-CEP-00A
  - ADR-CEP-12
状态: 🟡 **待实施**（2026-11-12 起草;per 台账 Entry #340 §13.2 + oracle 审查「需改后批准」的 5 MUST 已全部落盘）
created: 2026-11-12
---

# cpptlm-cep-gate-strength-tooling: CEP 闸强度与工具补齐

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— CEP 闸强度与工具补齐

> **状态**: 🟡 **待实施**（2026-11-12 起草;per 台账 Entry #340 §13.2「C1 · 闸强度与工具补齐」）
> **关联路线图**: [`phase-10`](../roadmap/phases/phase-10.md)(in-progress)+ [`objective-cep-platform`](../roadmap/objectives/obj-cep-platform.md)(§13 下一阶段任务梳理 + **Entry #340**)
> **oracle 审查**: 1 轮,结论 = **需改后批准**（方向无错;5 MUST + 3 SHOULD + 2 NIT 全部落盘 —— 其中 **MUST-4 推翻了本设计初稿的 R-e 方案**(python 段**已有**集合判据 ⇒ 初稿的「新增集合判据」是重复造）、**MUST-5 删掉了 R-f 的「优先探测真实 trace」半死分支**）
> **统一判据**: 每个被改的闸都必须有**一条决定性变异** —— 破坏 ⇒ 按**正确原因**红 ⇒ 还原 ⇒ 绿。⛔ **不得靠「文件消失」才红**（先例:[`cep_gate_selftest.sh`](../../scripts/test/cep_gate_selftest.sh) **:49-50** 的 fixture 老化记录 —— 旧写法删段时**意外截断整个文件** ⇒ 断言是靠「文件没了」才 fire,不是靠「真段被删」⇒ 2026-10-26 修正;同登记见台账 **Entry #331**）
> **边界**: 仅改**闸脚本 + 索引生成器 + 文档路径**;⛔ **零** ADR / schema / 声明面 / ABI / 判据内容改动
> **不占 KPI slot**: 产物 = 脚本 + 文档 ⇒ ⛔ 不新建 OpenSpec change（先例:`cpptlm-cep-b3b2-d4-trigger-probe`）

---

## 架构依据

| 依据 | 出处 | 决策要点 |
|------|------|---------|
| 守门 invariant「**有牙**」原则 | `ADR-CEP-06` + `obj §6.2` **B2** | 载体必须**有牙**（反向 fixture,或结构校验 + 负控）⇒ 「恒真判据」= **假绿**,不得计入覆盖 |
| 索引生成器是**已登记 backlog** | `docs/guide/cep-usage.md` §12.8「工具 backlog」+ `07 号` **:575** 逐字「本索引**由脚本生成**(零手抄)…**生成器不在仓内**(登记为 backlog:落 `scripts/`)」 | 不是新需求,是**兑现已登记项** |
| 跨仓锚点治理不变量 | `ADR-CEP-00A` 的 `Status Update 2026-11-11` ① | 行号**必腐化** ⇒ 一律用**语义锚**;机器锚点保留行号 |
| 受限族声明真源 | `ADR-CEP-12` + `obj §6.2` **B3b-2** | 4 族受限（CXL 2.0 **冻结** + CXL 3.0 / NVLink / UALink）⇒ 族名单须**集合级**且与 `07 号` SU·六 **交叉**可判 |
| **retention = 丙** | `ADR-CEP-06` 的 `Status Update(2026-11-08,per 台账 Entry #295)` | 只入库 `metrics.json` + `simulation_config.json`;`.gitignore:117` 使 **`trace.h5` 永不入库** ⇒ **CI 拿不到真实 trace** |
| 收口批的实测教训 | 台账 **Entry #339** ⑦ | 收口批**两次**吞掉文件 trailer ⇒ **手工同步是已证实的失败面** |

---

## 范围

**In-Scope（5 子项）**:

1. **R-a · 索引生成器落仓 + 闸三合一**（`scripts/cep/generate_doc_index.py` 新增;`cep_index_position_check.sh` 扩展;`07 号` 文末索引段由生成器产出）
2. **R-e · 族名校验有牙化**（删恒真 grep 段 + 建 `07 号` SU·六 ↔ JSON 的**交叉校验**）
3. **R-f · 闸强度**诚实化**（显式降级标注;⛔ 不保留「优先探测真实」的半死分支）
4. **R-g · 闸清单登记处对齐**（`scripts/README.md` 补 2 闸 + 闸数 25→35;`AGENTS.md` **4 处**数字订正）
5. **R-h · 路径漂移**（`scripts/README.md` 的裸路径 `fabric.json` 补目录前缀）

**Out-of-Scope（明确不做）**:

- ⛔ **零** ADR 正文改动（22 份 CEP ADR 全不动）· ⛔ 零 schema / 声明面 / ABI 改动
- ⛔ **不改 `07 号` 的判据内容**（只改**索引段的生成方式**与自述计数）
- ⛔ **不动 B3b-2 的 `⭕` 登记与计数**（仍 **0/4**）⇒ 本条**不改**任何 DoD 结论
- ⛔ **不新增 run 产物入库**（retention 丙不动）· ⛔ **不为「优先探测真实 trace」写任何 AC**（见 Why 的 R-f）
- ⛔ 不含 **C2 / C3 / C4**（各自独立;C2 还等 architect 裁 `T-C2-⓪/①`）· ⛔ 不新建 OpenSpec change（**不占 KPI slot**）
- ⛔ 不动 `test/CMakeLists.txt`（3 个闸**已注册**,见 AC-7）

---

## Why

这 5 项的**共同形态** = 「**登记了,但没有牙齿**」⇒ 本条把「登记」变成「拦截」;**⛔ 不新增任何架构决策**。

| 子项 | 实测缺口（**非转述**） | 不修的后果 |
|---|---|---|
| **R-a** | `07 号` 实际 `^## ` = **28**,索引自述「23 个一级节 / 9 则 SU」⇒ **5 个未列入**;索引头 `:571`（「**另有 5 个未列入**」）与 `:577`（「与现存一级节 / SU **逐条对应**」）**自相矛盾**;索引 A 的「行」列 **10/23 错**（`:597-604` 有 8 行全指向 473 或旧值） | 「28 节却自称 23」= **索引在说谎**;行号错 ⇒ ArchForge 对齐清单按行号锚定时**指错位置** |
| **R-e** | `cep_b3b2_deferral_check.sh:57-61` 的 grep 段 = `grep -q 'cxl_2_0' "$DECL" && …` ⇒ **恒真**（任何含该字符串的文件都过）;且 python 段 `:26-48` 的期望族名单**硬编码**在脚本内,与 `07 号` SU·六 **零交叉校验** | 族名单与真源漂移**不可发现** ⇒ B3b-2 的 `⭕` 登记**不可信** ⇒ 外部条件满足时机器**无法正确发现** |
| **R-f** | `:29` 的 `h5py.File(f"{d}/trace.h5","w")` **自合成**;`.gitignore:117` ⇒ **`trace.h5` 永不入库** ⇒ **CI 拿不到真实 trace** | 守门强度**不可观测**（读者以为「trace 内容已验证」）;而「优先探测真实 run」在 CI **恒不命中** ⇒ 那是**半死代码**,⛔ 不写 AC、不保留 |
| **R-g** | `AGENTS.md` **4 处**（`:7` / `:217` / `:231` / `:614`）写「**25 个** `cep_*.sh`」实为 **35**;`cep_e2e_reconcile_check.sh` 与 `cep_index_position_check.sh` **未**在 `scripts/README.md` 闸表登记 | 读者按 AGENTS.md **找不到闸**;闸数错 10 个 |
| **R-h** | `scripts/README.md:55` 引用裸路径 `` `fabric.json` ``（实为 `include/tlm/cep/fabric.json`）⇒ `docs_sync_check.sh --strict` **预存红** | 读者无法区分「预存」与「本批引入」⇒ 该闸信号被**稀释** |

> ⚠️ **R-f 的诚实口径（oracle MUST-5）**:保留的只有「**显式降级标注**」——闸头注释须写明「本闸 = **文件层协议镜像校验**（metrics.json 为真实产出、trace.h5 由 metrics 镜像合成）;**真实 run 的 h5 层校验归实验 4 件套运行时**」。⛔ **不得**宣称本闸验证了 trace 内容。

---

## What Changes

| # | 变更 | 文件 | 形态 |
|:--:|---|---|---|
| 1 | **索引生成器**（新增） | `scripts/cep/generate_doc_index.py` | 读 `07 号` ⇒ 产出**自述计数行 + 索引 A（全量 28 个 `## `）+ 索引 B（全量 15 则 SU）**,**行号列为生成时刻实测值**;⛔ **索引 C 保留手工**（含「已迁出」等人工语义） |
| 2 | **索引闸三合一**（改） | `scripts/test/cep_index_position_check.sh` | 新增「实际 `^## ` 数 == 自述一级节数 == 索引 A 行数」+「实际 SU 标题数 == 自述 SU 数 == 索引 B 行数」+ **「索引 A/B 的『行』列 == 实际行号」** |
| 3 | **索引内容**（改） | `docs/architecture/07-…md` 文末索引段 | 生成器产出（自述 23/9 → **28/15**）;⛔ **同时删除 `:571` 的「另有 5 个未列入」排除声明** + **重写 `:577` 的「逐条对应」句**（否则索引头宣称排除、索引体全量列入 = **自相矛盾**） |
| 4 | **族名交叉校验**（改） | `scripts/test/cep_b3b2_deferral_check.sh` | **删 `:57-61` 的恒真 grep 段**（python 段 `:26-48` **已有**集合判据 ⇒ ⛔ 不重复造）+ **新增 `07 号` SU·六 ↔ JSON 族名单交叉校验**（防「脚本内硬编码 vs 外部真源」漂移） |
| 5 | **强度标注**（改） | `scripts/test/cep_b4_observation_keys_check.sh` | 输出 `[synthetic-fixture]` 标注（**可 grep**）+ 闸头注释写明强度边界;⛔ **删「优先探测真实 trace」分支**（CI 恒不命中） |
| 6 | **登记处对齐**（改） | `scripts/README.md` | `:55` 的 `` `fabric.json` `` → `` `include/tlm/cep/fabric.json` ``;**补 2 个闸的登记行**;闸数 25 → **35**;**顺手核对** `docs/guide/cep-usage.md:118` 的闸分类表口径（手册已列、README 未列 = 登记处仍不一致的一半） |
| 7 | **AGENTS.md 数字订正**（改） | `AGENTS.md` | **4 处**「25 个」→ **35**（`:7` / `:217` / `:231` / `:614`） |
| 8 | **台账**（追加） | `.rddf/roadmap/objectives/obj-cep-platform.md` | **Entry #342**（授权 + 回执）;**并同步把 `R-g` 补进 §13.1 的 C1 行子项列**（⛔ 防「登记处漂移」在同一 change 内复发 —— 见 `.rddf/reviews/cep-governance-goal-scope-review.md` 的 R-g 行） |

> ⚠️ **变更 3 的行号影响**:索引 A/B/C 的「行」列由生成器**每次重算** ⇒ 解决「行号腐化」根因（当前索引 A **10/23 错**）。
> ⚠️ **变更 1 的冻结面口径**:`scripts/cep/generate_doc_index.py` 是 **tool** ⇒ ⛔ **不入冻结集**（与 `cep_freeze_check.sh:16-19`「**checker 不冻结**」原则一致）。

---

## Acceptance

- [ ] **AC-1（生成器幂等）**:对 `07 号` 连续跑 2 次生成器 ⇒ 产出**逐字相同**（零手抄 ⇒ 幂等）;且生成后 `git diff` **仅索引 A/B + 自述计数段**为空（索引 C 手工 ⇒ 该段**允许**有 diff）
- [ ] **AC-2（决定性变异 · 索引完整性）**:`cep_index_position_check.sh` —— **删 `07 号` 一个 `## ` 段 ⇒ 闸必红**,且红因 = 「实际 `## ` 数 ≠ 自述数」（⛔ **不得**靠「文件消失」才红）;**改索引 A 行数 ⇒ 必红**;**改索引 B 行数 ⇒ 必红**
- [ ] **AC-3（决定性变异 · 行号正确性）**:索引 A/B 的「**行**」列**逐行 == 生成时刻实际行号**（**实测基线:索引 A 当前 10/23 错**）⇒ **手工改乱任一索引行号 ⇒ 闸必红**（⛔ 不接受「只修计数不修行号」）
- [ ] **AC-4（决定性变异 · 族名交叉校验）**:`cep_b3b2_deferral_check.sh` —— **删 1 族** / **同族加第 2 条** ⇒ 必红（**旧 python 段 `:26-48` 已覆盖,作回归基线**）;**新增靶子:改 `07 号` SU·六 名单中的一族名 ⇒ 必红**（这是**新**交叉校验的决定性变异 —— 旧判据对该变异**不红**,故能证明新判据有牙）;4 族齐且各 1 条 ⇒ 绿
- [ ] **AC-5（强度可观测 · 不欺）**:b4 闸**输出含 `[synthetic-fixture]` 标注**且 **exit 0** ⇒ 可 `grep` 断言;闸头注释写明「本闸 = 文件层协议镜像校验;真实 run 的 h5 层校验归实验 4 件套运行时」⇒ ⛔ **不得**出现「已验证 trace 内容」类表述
- [ ] **AC-6（路径漂移清零）**:`bash scripts/test/docs_sync_check.sh --strict` ⇒ **rc=0**（`scripts/README.md` 的 `fabric.json` 缺失路径归零）
- [ ] **AC-7（闸注册 · 保持）**:`cep_index_position_check` / `cep_b3b2_deferral_check` / `cep_b4_observation_keys_check` **保持**在 `test/CMakeLists.txt` 的 `add_test`（**实测已注册** `:597` / `:708` / `:753`;⛔ **本 change 不动 CMakeLists**）+ `scripts/README.md` 闸表**有登记行**（含**新增 2 行**）⇒ `ctest --test-dir build -N` 能列出
- [ ] **AC-8（0 回归）**:`ctest --test-dir build -j4` **全绿** + `[cep]` 全绿 + **35 个** `cep_*.sh` 闸 **rc + ✗ 计数双判全绿**
- [ ] **AC-9（无自证）**:3 条新判据（AC-2 / AC-3 / AC-4 的**新靶子**）**不得**依赖「文件缺失 / 字符串自搜」这类恒真式;每条须有**独立于被测对象**的期望值来源（真源文件名 / 集合 / 实测行号）

---

## Capabilities

- **索引不可能再漏**:生成器 + 三合一闸 ⇒ 「28 节却自称 23」与「行号 10/23 错」这类漂移**在提交前即红**
- **族登记不可静默漂移**:SU·六 ↔ JSON **交叉校验** ⇒ 脚本内硬编码名单与外部真源不一致 ⇒ 闸红 ⇒ B3b-2 的 `⭕` 登记**可信**
- **闸强度可观测**:读者一眼可辨「本闸验的是**文件层镜像协议**,不是 trace 内容」⇒ 不再把自证当守门
- **闸清单可信**:闸数**对**（35）、**找得到**（2 个漏登记的补齐）、`docs_sync --strict` **归零** ⇒ 信号不再被预存红稀释
- **治理机制自洽**:索引用**生成器**兑现 `07 号:575` 已声明的「脚本生成」意图 ⇒ ⛔ 不引入第三条重复机制

---

## Impact

| 面 | 影响 |
|---|---|
| **冻结面** | ⚠️ `07 号` **在冻结集内**（`cep_freeze_check.sh:32`）⇒ 变更 ③ 改其 hash ⇒ **本条走五步授权链**（Entry #342 → 产物 commit → `--update --authorized-by "Entry #342"` → 清单 commit → 无参复验）;**`artifact_count` 仍 78**（⛔ 不新增冻结产物;`generate_doc_index.py` 是 tool ⇒ 不入冻结集） |
| **ADR** | **零改动**（22 份 CEP ADR 全不动） |
| **`07 号`** | **仅文末索引段**由生成器产出（自述 23/9 → 28/15 + 行号列重算 + `:571`/`:577` 两处声明改写）⇒ **正文与判据零改动** |
| **闸** | 3 闸增强（2/4/5）+ 2 闸补登记（6）;1 个生成器新文件 |
| **测试** | **+3 条决定性变异**（AC-2 / AC-3 / AC-4 新靶子）⇒ `ctest` 用例数不变（变异在闸脚本内,不新增 ctest 项） |
| **KPI** | **不占 OpenSpec slot**（产物 = 脚本 + 文档） |
| **下游** | C2 / C3 / C4 的前置**不受影响**;本条完成后 `obj §13.2` 的 C1 判据可勾 |
| **一处订正** | `.rddf/reviews/cep-governance-goal-scope-review.md:76` 写「冻结产物 **77**」为**笔误**,实为 **78**（per manifest `artifact_count`）⇒ 在 Entry #342 登记 |

**关联**:台账 **Entry #340**(§13 任务梳理 + 排序裁决)· **#341**(14 号归位 —— 同批治理序列)· **#339**(收口批 + oracle 复核 —— 「手工同步吞内容」的实测教训)· **#331**(`gate_selftest` fixture 老化记录)· `docs/guide/cep-usage.md` §12.8 未结义务清单（R-a / R-e / R-f / R-g / R-h 五行）· `ADR-CEP-06`(4 件套 + retention 丙)· `ADR-CEP-00A` SU 2026-11-11 ①(跨仓锚点治理不变量)· `ADR-CEP-12`(受限族声明)· `.rddf/reviews/cep-governance-goal-scope-review.md`(architect 梳理报告)
