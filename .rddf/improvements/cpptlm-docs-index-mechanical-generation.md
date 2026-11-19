---
name: cpptlm-docs-index-mechanical-generation
priority: P2
phase: phase-1
依赖:
  - docs/adr/README.md §7（抽取契约，architect 已落盘）
  - scripts/cep/generate_doc_index.py（同类机制先例，Entry #342）
  - scripts/test/cep_gate_harness.py（fixture 拷贝根机制）
状态: ✅ **已实施**（2026-11-19；W1~W5 全部完成，per 台账 Entry #504；**不占 OpenSpec slot** —— 与 `cep-b4-evidence-device` 先例同型：无 change 但过机械闸 + 变异验证）
created: 2026-11-19
---

# improvement: cpptlm-docs-index-mechanical-generation

**主题**：把 `docs/adr/README.md`（真源 3 索引）的「议题 / 状态 / 份数」三列从**人工同步**改为**脚本机械生成 + 闸强制对账**，消除「文档计数与状态声明静默腐化」这一机制性缺陷。

> **决策源**：无新 ADR —— 机制设计经 **oracle 1 轮**（APPROVE + 4 条件，条件已全部采纳）；**判据面已钉死**在 [`docs/adr/README.md §7`](../../docs/adr/README.md)（architect 写集，已落盘 commit `0024a338`）
> **触发**：2026-11-19 docs 重组后的架构面漂移体检 —— 实测抓到 **3 处**同型漂移 + **1 处未兑现承诺**（详见 §Why）
> **先例**：`scripts/cep/generate_doc_index.py`（台账 Entry #342）—— **同类机制、同类解法**，且先例**未走 ADR**（台账 Entry 登记即可）
> **估时**：**1 ~ 1.5d**（生成器 ~0.5d + 闸与 4 变异 fixture ~0.5d + 首次重生成与注册 ~0.5d）

---

## 架构依据

| 依据 | 出处 | 本 improvement 用到的决策要点 |
|---|---|---|
| **抽取契约（判据面）** | `docs/adr/README.md §7`（本轮 architect 落盘） | 源行二形态（59 行内 `> **状态**: X` + 4 章首 `## Status`）· 封闭词表 9 项 · 取**最后一个**命中 · emoji 由词决定 · 词表外 **fail loud** + 显式豁免表（当前仅 `ADR-X-SUMMARY` 1 项）· 议题列正则须允许连字符 · 份数列**按系列分解** · 闸的三合一判据 + 4 变异 fixture 要求 |
| **同类机制先例** | `scripts/cep/generate_doc_index.py` + 台账 **Entry #342** | CLI 契约 `--doc` / `--write` / `--check`；**拒绝重写非生成块形态的行**（防误伤）；`--check` 与 `--write` 同口径；生成器落仓 + 闸扩展为「三合一」= 本 improvement 的**直接模板** |
| **fixture 拷贝根机制** | `scripts/test/cep_gate_harness.py` + `scripts/test/cep_gate_selftest.sh` | 闸脚本**自身不冻结**，但须能修 ⇒ 自测保证「修了没坏」；fixture **必须跑在拷贝根上** ⇒ ⛔ 不得改工作树真 ADR（`ADR-CEP-*` 全在 M1 冻结面内，中途崩溃会留下 red 工作区） |
| **闸纪律** | `docs/guide/cep-usage.md` §12.5 读闸纪律 | 「新增闸须附**决定性变异**」= fixture 必需项，非可选项 |
| **ADR 不可变** | `AGENTS.md` §DOC HYGIENE + `docs/adr/README.md` 顶部 | 本 improvement **只重生成索引**，⛔ **不改任何 ADR 正文** ⇒ 零冻结面改动 |

---

## Why

**缺陷 = 「人工同步 + 无对账闸」的机制性根因**，不是偶发笔误。四条独立证据链：

| # | 证据 | 实测 |
|:--:|---|---|
| W-1 | **07 号索引同型失败已发生过一次** | 台账 **Entry #342** 诊断：07 号文末索引「手工同步」+ 闸只校验「自述数 == 表行数」，**不校验「表行数 == 实际 `## ` 数」** ⇒ **5 个 `## ` 未列入仍绿**、索引 A 行号 **10/23 错** |
| W-2 | **`docs/adr/README.md` 重声明了「零手抄」却不可复现** | 2026-11-19 重建的索引声称「状态机械抽取自 ADR 本体、零手抄」，但生成器停在 `/tmp` **未入库** ⇒ 索引**当前不可重生成** ⇒ 该承诺**不可兑现**（已如实标注为「未治理项」，未作伪声明） |
| W-3 | **计数声明已多处漂移** | `docs/guide/cep-usage.md`：闸 **35 → 44**、冻结集 **69 → 89**（2026-11-19 实测，本轮已校正）；「63 份 ADR」手抄于 `AGENTS.md` + `docs/README.md` + `docs/adr/README.md` **3 处** |
| W-4 | **同一判据两个数** | `ADR-CEP-00` SU 写「22 份 / 22 键 / total=125」，`07 号` 写「23 份 / 23 键 / total=131」，实测快照 **23 / 131** ⇒ 一处对一处过期（根因 = Entry #431 的 W2 已授权更新快照与 delta spec，**唯 ADR 未同步声明**） |

**⛔ 为什么不再「人工校对一次」**：Entry #342 已实测此路必腐（07 号 5 个 `## ` 未列入仍绿）⇒ 本轮 W-3/W-4 就是「校对之后无人对账」的**复现**。

**⛔ 为什么不做「一次性全量人工校对」**：同上 —— 无机制 ⇒ 下一个 ADR / 下一个闸仍会复发。

---

## What Changes

| # | 内容 | 落点 | 性质 |
|:--:|---|---|---|
| **W1** | **生成器**：`--write` 重写 `docs/adr/README.md` 的「议题 / 状态 / 份数」段；`--check` 判漂移（rc=1）；**拒绝重写非生成块形态的行**（防误伤）；**幂等**（连跑 2 次逐字相同）；抽取按 `docs/adr/README.md §7` 实现 | `scripts/docs/gen_adr_index.py`（新） | 新增 |
| **W2** | **判据面** = `docs/adr/README.md §7`（源行形态 / 封闭词表 / 取词规则 / 归一化 / fail loud / 豁免表 / 议题正则 / 系列分解 / 三合一判据 / fixture 要求） | `docs/adr/README.md`（**已落盘**） | ✅ 已完成 |
| **W3** | **闸**：三合一判据（① 逐系列实际数 == 表行数 == 统计表 cell == 合计 ② 每份索引状态 == 按 §7 的确定性抽取 ③ 生成器 `--check` rc=0）+ **4 个变异 fixture**（改 1 份 ADR 状态 ⇒ ②红 / 加 1 份未登记 ADR ⇒ ①红 / 删 1 行索引 ⇒ ①红 / 改合计 cell 不改表 ⇒ ①红）+ **ctest 注册** | `scripts/test/docs_adr_index_check.sh`（新）· `test/CMakeLists.txt`（改） | 新增 + 注册 |
| **W4** | **首次重生成**：`--write` 落地，**一次性消除人工归一化层**（含 `ADR-CEP-20` 双状态 / `ADR-DGPU-15` emoji 错配 / `ADR-X-SUMMARY` 截断三处特例） | `docs/adr/README.md`（改） | 一次性大 diff |
| **W5** | **登记**：新增脚本登记进 `scripts/README.md`（DOC HYGIENE 规则：结构调整必同步脚本索引） | `scripts/README.md`（改） | 登记 |

---

## Acceptance

**全部机器可判**（无人工判据）：

| # | 判据 | 手段 |
|:--:|---|---|
| AC-1 | 生成器 `--check` rc=0 且**幂等** | 连跑 2 次 `--write`，输出**逐字相同** |
| AC-2 | **判据①**（逐系列）：实际 `ADR-*.md` 计数 == 索引表行数 == 统计表该行 cell == 合计 cell | 闸 `--check` + 独立复算（4 系列） |
| AC-3 | **判据②**：每份 ADR 的索引「状态」== 按 §7.1~§7.2 对其本体的**确定性抽取** | 闸逐份比对（63 份） |
| AC-4 | **fail loud**：词表外状态 ⇒ rc≠0 且**指名文件** | 注入 1 份伪状态 ADR ⇒ 必红并指名 |
| AC-5 | **4 个变异 fixture 全红 + 还原绿** | fixture **跑在拷贝根**（`cep_gate_harness.py` 模式）⇒ ⛔ 零工作树污染 |
| AC-6 | 首次 `--write` 后 `docs/adr/README.md` 与生成器输出**逐字相同** | diff 比对 |
| AC-7 | **零回归**：既有 11 个文档闸 + `ctest` 全绿 + **M1 冻结面 89 件 hash 一致** | 闸套件 + `cep_freeze_check.sh` |

⚠️ **AC-5 的 fixture 必须跑在拷贝根** —— `ADR-CEP-*` 全在 M1 冻结面内，直接改工作树真 ADR 会让 `cep_freeze_check` / `cep_docs_sync_check` 整组 rc=1（中途崩溃即留下 red 工作区）。

---

## Capabilities

| # | 能力 | 判别力（改什么 ⇒ 必红） |
|:--:|---|---|
| **C-1** | **索引自愈** | ADR 状态改 ⇒ `--write` 可重生成；不改 ⇒ ②红 |
| **C-2** | **新增 ADR 漏登记检出** | 新增 1 份 ADR 不登记 ⇒ ①红（**这是当前 07 号闸的最大盲区**） |
| **C-3** | **判据面可审计** | 抽取契约唯一（§7）⇒ 改它 = 改判据面 ⇒ 须走授权（与 `ADR-CEP-11` 协议版本锁定同型处置） |
| **C-4** | **可推广**（本期**不**推广） | 本期**仅** `docs/adr/README.md` 一个索引；推广到 `architecture/` `implementation/` `validation/` 子 README 列为**后续 improvement**（oracle 建议：先单点跑通验证，⛔ 不复制未验证的判据形态） |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增** | `scripts/docs/gen_adr_index.py` · `scripts/test/docs_adr_index_check.sh` |
| **改** | `docs/adr/README.md`（一次性大 diff：生成块接管）· `test/CMakeLists.txt`（ctest 注册）· `scripts/README.md`（登记） |
| **零改动** | **零** ADR 正文 · **零** 冻结面产物 · **零** 产品代码（`include/` `src/` `test/test_*.cc`）· **零** `openspec/specs/**` |
| **风险** | 低 —— 判据面已由 architect 钉死并落盘；生成器只重写索引的**生成块**；⚠️ **唯一需人审的是首次 `--write` 的大 diff**（逐字核对状态列是否符合 §7 契约） |
| **未覆盖（明确排除）** | ① `docs/architecture/README.md` 等**其他子索引**的计数不腐化（⇒ 后续 improvement）② 07 号 5 条伪链接（⇒ 归 Entry #502 的冻结面授权，本 improvement 不碰）③ `docs/guide/cep-usage.md` §12.5 的**分类标签**机器派生（⇒ 分类属语义判断，⛔ 不做；只做「具名集合 == 磁盘集合」的廉价对账闸，见后续 improvement） |

---

## 实施结果（2026-11-19，builder 回执；per 台账 Entry #504）

| # | 内容 | 落点 | commit | 判据 |
|:--:|---|---|---|---|
| **W1** | 生成器（`--write` / `--check` / `--json` + 外来内容护栏 + 幂等） | `scripts/docs/gen_adr_index.py` | `adb43981` | `--check` rc=0，连跑逐字相同 |
| **W2** | 判据面 = `docs/adr/README.md §7`（architect 落盘） | `docs/adr/README.md` | `0024a338` | — |
| **W3** | 闸（三合一）+ **4 变异 fixture** + ctest 注册 2 项 | `scripts/test/docs_adr_index_check.sh` · `test/CMakeLists.txt`（#176/#177） | `fd7ae0fe` | 闸 rc=0；`--selftest` 4/4 变异全红 |
| **W4** | 首次 `--write`（63 份，一次性消除人工归一化层） | `docs/adr/README.md` | `5eed6eeb` | 与生成器输出逐字相同 |
| **W5** | `scripts/README.md` 登记 + 子目录表 5→6 | `scripts/README.md` · `scripts/CMakeLists.txt` · `scripts/test/docs_sync_check.sh` | `adb43981` / `fd7ae0fe` | `docs_sync_check --strict` rc=0 |

**零回归**：13 个文档闸全绿 · `ctest` **186/186** · M1 冻结面 89 件 hash 一致（未触碰）。

**实现期发现的两条（已写进 commit，供后续追责）**：

| # | 发现 | 处置 |
|:--:|---|---|
| G-1 | **判据面词表不完备**：§7.2 封闭词表只有「提案」，而 `ADR-DGPU-10` 本体写「📋 **提议**」⇒ 生成器 fail loud 抓到 | 全量审计 63 份（61 直接命中 + 1 同义词 + 1 豁免）确认**只差这一个同义词** ⇒ 补 `提议 ≡ 提案`（**同义词入表，非新增状态类**，共用 `📋 提案`）。⚠️ 根因 = architect 钉契约时只验证了**形态**（59 行内 + 4 章首），未验证**词表覆盖** |
| G-2 | **变异设计差点假绿**：初版变异 1 是**前置插入**「起草中」，但契约「取**最后一个**命中」⇒ 行内后出现的「已签发」覆盖它 ⇒ 抽取不变 ⇒ 变异无效却显示绿 | 改为**行尾追加**后必红。**教训**：对「取最后一个」型契约做变异，必须让变异成为**最后一个 token** |
| G-3 | **新契约打红另一个闸**：Entry #502 的审计文字含**反例路径**（`docs/cep/` / `docs/architecture/adr/`），被 `cep_doc_ref_check` 当真实引用 | 用闸既有 `VIRTUAL_PATHS` 登记 2 条并写明理由（先例 `docs/definitely-not-exists-xyz.md`）；⛔ **未回改台账**（append-only） |

**遗留（二期，另立 improvement）：C-4 推广** —— 本 improvement 按 oracle 建议**先单点跑通**（仅 `docs/adr/README.md`）。二期实测数据与设计见 `.rddf/improvements/cpptlm-docs-index-generalization.md`。

**关联**：台账 **Entry #431**（W2 快照更新授权 —— 本 improvement 的 W-4 特例的根因）· 台账 **Entry #342**（07 号索引生成器先例）· 台账 **Entry #502**（冻结面文档同步授权）· 台账 **Entry #504**（本次回填）· [`docs/adr/README.md §7`](../../docs/adr/README.md) · `scripts/cep/generate_doc_index.py` · `scripts/test/cep_gate_harness.py` · `docs/guide/cep-usage.md` §12.5
