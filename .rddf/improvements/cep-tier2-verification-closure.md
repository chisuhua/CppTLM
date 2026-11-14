---
name: cep-tier2-verification-closure
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)—— 档 2 change 的**验收闭环**(4 项实现缺口补正 + verifier 第 2 轮 + 台账现状订正 + push)
依赖:
  - ADR-CEP-20
  - ADR-CEP-06
  - ADR-CEP-12
状态: "🟢 已实施并验收(2026-11-14 · **v3 · 实施 + verifier 第 2 轮**)—— W1 ~ W7 **全部完成**;4 项缺口 ⇒ **G-1 / G-3 / G-4 闭合** + **G-2 部分达成**(AC-2 ⭔ 不得记 ✅);⛔ 唯一未达成项 = D-1 待裁(verifier + 用户);已 push ⇒ **本 improvement 归档态**"
created: 2026-11-14
---

# cep-tier2-verification-closure: 档 2 验收闭环

> **范围纪律**:⛔ **不改任何已签批 ADR** · ⛔ **新增 invariant 数 = 0** · ⛔ **不改 main spec 的 Requirement / Scenario** · ⛔ **不产 4 件套 run** · ⛔ **不新建 OpenSpec change**(理由见 §What Changes ①,含对 `AGENTS.md`「公共 API ⇒ 必须走 openspec」条款的正面 rebuttal)· ⛔ **不占 KPI slot**(保持 0/3)
> **产出形态**:**归档后缺陷补正**(实施 commit)+ 台账订正 commit + verifier 第 2 轮报告 + push(三个提交面分开,见 §Impact)
> **估时**:≈ **1.0 ~ 1.5 人日**(W4 走「段级扫描面」分支 ≈ 1.0;若改走「五步链改文档」分支 ≈ 1.5 —— per metis P2-11)
> **上游**:`.rddf/reviews/cep-evidence-tier2-minimal-slice-verify.md`(verifier · 第 1 轮「有条件通过」10 ✅ + 4 ⚠️ · **第 2 轮**「3 项闭合 + G-2 部分达成」)· 台账 **Entry #410 ~ #413** · **#414**(规划 + metis 处置 + 授权)· **#415**(实施回执)· **#416**(第 2 轮验收)· **#417**(push 回执)

## ⚠️ 工作区现态(实施前必读 · per metis P0-1)

**本计划起草时,仓内已有 2 个文件被改动但未提交,且无台账 Entry 登记**:

```
 M include/tlm/cep/protocol_engine_base.hh   (+14 行)
 M test/test_cep_tier2_observation.cc          (+24 行)
```

| 文件 | 已存在的改动 | W1 / W2 的**剩余**工作 |
|---|---|---|
| `include/tlm/cep/protocol_engine_base.hh` | `set_observation_capacity()` / `observation_capacity()` + `observation_capacity_` 成员(默认 **4096** 不变)+ `record_observation` 改用成员 | **零**(已就绪) |
| `test/test_cep_tier2_observation.cc` | `Tier2Verdict` 加 `provenance_broken` / `untraceable_bits`;`detect()` 加 `dropped` / `known_engine_masks` 形参 | **只补用例**(W1 / W2 / W3) |

⇒ **实施者不得重复添加上述机件**(按本计划照做 = 重复声明 ⇒ 编译错误或 API 重复)。
⇒ ⚠️ **这批 WIP 当前处于「机件在、用例无」状态 ⇒ 正是 G-1 / G-3 的原始失败模式** ⇒ 见 **A-11(死判据免税)**。

## Why

档 2 change `cep-evidence-tier2-minimal-slice` 已于 2026-11-14 **✅ 已完成归档**(带 delta · `openspec/specs/cep-evidence-tier2-observation/spec.md` · 6 Scenario · 冻结面 85 产物)。**但归档 ≠ 验收通过** —— verifier 的独立验收判定 = **有条件通过(10 ✅ / 4 ⚠️)**,4 项缺口全部分类 `implementation_gap`(判据正确、实现未完)⇒ 路由回 builder。

**四条动机(按紧迫性)**:

| # | 动机 | 不做的代价 |
|:--:|---|---|
| **M1** | **两条判据从未被执行过** —— G-3 的「`dropped_observations` 非零 ⇒ 红」与 G-4 的「新增产出物内 grep 禁表述 ⇒ 命中即红」在验收时**只存在于文档**,**无任何代码路径触发** ⇒ 档 2 对外声称的「可机判」在这两个方向上是**空洞的** | 档 2 的登记面(`test/cep_evidence_tiers.json` 的 `can_claim`)继续声称「不可判形态可与零违例严格区分」而该路径**从未跑过** ⇒ 下一次有人依赖它时会踩坑 |
| **M2** | **台账正在对外陈述一个内部自相矛盾的状态** —— `obj:1369`「DoD 12 条 = **实质全达成**」与 `obj:1370`「终态 DoD Go = **⏳ 挂起**(唯一未达成 = `B3b-2` = **0/4 ⏭**)」相邻 ⇒ 任何读台账的人都会得到错误结论 | 治理台账是本仓对外陈述的唯一权威 ⇒ 矛盾陈述会让后续裁决(B3b-2 / objective 归档)建立在错误前提上 |
| **M3** | **25 个 commit 未推送** —— 归档件 / 台账 Entry #404~#413 / 冻结清单**全部只在本地** ⇒ 协作面与冻结面不一致(冻结面 hash 指向的 commit 在远端不存在) | 协作方看不到本次全部交付;`frozen_at_commit` 指向的 commit 无法被他人复验 |
| **M4** | **G-1 provenance 判据缺席 ⇒ 下一个用例会静默劣化** —— 验收时实测「正控事实上满足 provenance」(A 的 sharer 来自 A 引擎自身的 `sync_tracker`),但**无机器判据** ⇒ 若下一个人改用 fixture 播种目录,缺口不会被发现 | provenance 破口 = 「观测到的他人 sharer 位不可溯源」⇒ 观测不可信 ⇒ **在无判据的情况下会被当成零违例** |

**⚠️ 反向动机(诚实登记)**:「补完验收让台账好看」**不是**本计划的目的 —— 若 W1~W4 出现「为了让闸变绿而弱化判据」的写法,本计划即**失败**。⇒ 判据边界:**允许「证明判据能变红」(变异),不允许「把判据放松到能通过」**。

**⚠️ 未说出口的第四动机(per metis 隐藏意图分析 · 须登记)**:本计划同时在**为「归档后缺陷补正」这条非标准路径立先例** —— 该路径若无本轮先例与边界登记,将来会被随意用于任何「归档后想改点东西」的场景 ⇒ ⇒ 本先例的边界必须**明确且窄**(见 §What Changes ① 的 4 条理由 + §④ 排除项)。

## What Changes

### ① 产出形态:⭔ **不新建 OpenSpec change**(决策 + 对 repo 硬规则的 rebuttal)

| 项 | 值 |
|---|---|
| 形态 | **归档后缺陷补正**(实施 commit)· 台账订正 commit · verifier 报告第 2 轮 · push |
| 理由 ① | 4 项**全部**是 `implementation_gap` —— **判据本身已在归档契约里**(AC-1③ / AC-2 / AC-8 / AC-9),**无需新判据** |
| 理由 ② | 归档件**不可回改** ⇒ 新建 change 会让「补 4 个测试」升级为一次完整立项(Proposal + Design + Specs + Tasks + 冻结链 + 审批)⇒ **生命周期成本与收益严重失衡** |
| 理由 ③ | 真正的不对称点是**时间窗**:本轮改动**只碰实现 + 闸脚本**,而**不碰任何契约面** ⇒ 若改的是契约,才需要 change 的完整流程(⛔ 见下方 rebuttal) |
| ⛔ rebuttal(`AGENTS.md` ANTI-PATTERNS「架构性变更未走 OpenSpec」:影响 ≥2 模块 / **公共 API** / 配置 schema 必须先提案) | 本轮唯一的**新增公共 API 面** = `set_observation_capacity()` / `observation_capacity()`(基类 2 个方法)⇒ 判定为「**测试注入 seam,非契约面**」—— 依据:**main spec `openspec/specs/cep-evidence-tier2-observation/spec.md` 的 Requirement / Scenario 未定义任何「可注入性」语义** ⇒ 注入点**不在契约面** ⇒ 不触发该条款。⚠️ **豁免须双重登记**(台账 Entry + code comment 标注「仅测试注入,非契约面」)⇒ 若 verifier 第 2 轮判其为契约面 ⇒ ⛔ **升级为 change**(不得在补正 commit 里夹带) |
| ⚠️ 何时才该新建 change | 若实施中**必须改动判据本身**(而非补实现)⇒ 属 `proposal_drift` ⇒ 路由回 planner 重写 proposal,⛔ 不得在补正 commit 里夹带判据变更 |

### ② 工作包(W1 ~ W7 · 各自带退出条件)

| # | 工作包 | 改哪些文件 | 退出条件(可机判) | 触冻结面? |
|:--:|---|---|---|:--:|
| **W1** | **G-1 provenance 机器判据** | `test/test_cep_tier2_observation.cc` | ① 正控:他人 sharer bit **恰等于** peer 引擎 `self_mask` ⇒ `provenance_broken == false`;② **反 fixture**:用 `HierarchicalDirectory::set_sharer` 的**4 参重载** `(addr, node, level, state)` 播种 `kNodeIdGmmu` ⇒ `untraceable_bits != 0` ⇒ `provenance_broken == true`;③ ⚠️ **播种前置断言**:播种后必须确认目录位**确实置上**(读回验证)⇒ 否则未映射节点/越界 level 会**静默 no-op**,反 fixture 会以「判据不灵」的方式假红 | ✗ |
| **W2** | **G-3 有界性溢出路径** | `test/test_cep_tier2_observation.cc` | `set_observation_capacity(2)` ⇒ 造 >2 条记录 ⇒ `dropped_observations() > 0` ⇒ `detect()` 判 `invalid == true`;⚠️ 该用例**只触发丢弃、不触发 `self_mask` 失效** ⇒ `invalid_reason` 断言含「丢弃」(**否则会被 `self_mask` 分支覆盖**,见 `test_cep_tier2_observation.cc` 的 reason 覆盖顺序) | ✗ |
| **W3** | **G-2 三段连通行为级负控** | `test/test_cep_tier2_observation.cc` | ① 判定器不收 ⇒ 违例 0;② 观测面关闭 ⇒ 无记录;⇒ ⚠️ **这是「行为级负控」,⭔ 非 AC-2 原文要求的「按签名红」** ⇒ **判定结果只能是 ⚠️ 部分达成,⛔ 不得记 ✅**(见 A-3 + D-1) | ✗ |
| **W4** | **G-4 AC-9 产出物覆盖面** | `scripts/test/cep_e2e_reconcile_check.sh` | 见下方 **W4 详规** | ✗ **只读不改** |
| **W5** | **verifier 第 2 轮复核** | `.rddf/reviews/cep-evidence-tier2-minimal-slice-verify.md` | **只复核 G-1~G-4**(⛔ 不重审已通过的 10 项)⇒ 逐条 ✅/⚠️ + 工具证据 + 「已用轮次 = 2/3」+ ⛔ 声明未重审其余 10 项;⚠️ **必须由 verifier 角色产出**,builder 不得代写判定(见 A-5) | ✗ |
| **W6** | **台账 §10 现状表订正** | `.rddf/roadmap/objectives/obj-cep-platform.md` | 见下方 **W6 详规** | ✗ |
| **W7** | **push** | — | `git push origin main` ⇒ 远端含归档件 + Entry #404~#414 + 冻结清单 ⇒ `git rev-list --left-right --count origin/main...HEAD` = `0 0`(⛔ 须先 `git fetch origin` 再计数,否则 `origin/main` 可能是陈旧引用) | ✗ |

### ②-1 · W4 详规(per metis P0-2 · 原「文件级扫描」形态**不可执行**,已改)

**⛔ 原形态为何不可执行(实测)**:把 4 个产出物**按整文件**扫 ⇒ 基线**不绿** —— 实测 **7 处假红**(均因短语**前 16 字符窗口内无否定标记**):

| 位置 | 命中短语 | 前窗 | 假红成因 |
|---|---|---|---|
| `07号:639` / `:808` / `:834` | `通过态` | `6-11-13(拓扑级 IC3 ` | 这是**档 0** 的 SU 标题(「拓扑级 IC3 通过态」)⇒ **档 0 产 run ⇒ 合法有通过态** ⇒ 用「档 2 专属禁词」扫**档 0 语境**是**范畴错误** |
| `07号:673` | `通过态` | `据**等同**;\n4. **一次` | 后置否定(`**一次通过态实例 ≠**` ⇒ `≠` 在短语**之后** ⇒ 只看前文的 `has_negation_context` 判不出) |
| `spec.md:64` | `通过态` | `ALL NOT 存在档 2 的「` | 英文 RFC 2119 否定(`SHALL NOT`)⇒ `NEG_MARKERS` **不含英文否定** |
| `spec.md:65` | `强于档 1` | `HALL NOT 表述为档 2 ` | 同上 |
| `cep-usage:230` | `通过态` | `检测力已证」;\n- ⇒ **一次` | 后置否定 + 位于 **§12.9** 段(非档 2 段) |

**⇒ 根因**:`FORBIDDEN` 是**档位相关**的词表,却用**不分档位**的方式扫 ⇒ 必然假红。

**✅ 修正形态:段级「新增产出物」扫描面(per AC-9 原文「**新增产出物内** grep」的**字面读法** —— 文件级扫描反而**超出** AC-9)**:

| 产出物单元 | 定位方式(⛔ 不写死行号) | 为什么是这个范围 |
|---|---|---|
| `test/cep_evidence_tiers.json` | 整份 | 档 2 登记面(**现状已绿**,零改动) |
| `07号` 的 **档 2 SU 段** | 从标题 `## Status Update 2026-11-14(档 2 协议级观测面 …)` 到**下一个 `## ` 之前** | 档 2 的总纲 SU ⇒ **新增产出物**;⛔ **不含**档 0 的 SU 2026-11-13 附录段 ⇒ 消 3 处假红 |
| `docs/guide/cep-usage.md` 的 **§12.10 段** | 从 `## 12.10` 到下一个 `## ` 之前 | 档 2 的手册段 ⇒ **新增产出物**;⛔ **不含** §12.9 ⇒ 消 1 处假红 |
| `openspec/specs/cep-evidence-tier2-observation/spec.md` | **整份** | 该 main spec **整体**即档 2 的产出物(74 行,Requirement 与 Purpose 混排 ⇒ **不可按档位分段**)⇒ 整扫 ⇒ 消「段落切分」歧义 |

**⇒ 必做的 2 处配套改动**:

1. **`NEG_MARKERS` 补英文否定标记**(`NOT`)⇒ 消 `spec.md:64/65` 的 2 处假红。⚠️ **必须附「加标记后仍能红」的变异证明**(植入肯定式夸大表述且其前窗无 `NOT` ⇒ 仍须红)⇒ 否则违反 R-1。
2. **⛔ 不改 `has_negation_context` 的「只看前文」方向** —— 段级扫描后,后置否定问题**自动消失**(那 2 处均在非档 2 段内)⇒ 无需扩大方向 ⇒ **不引入新误判面**。

**W4 的 plumbing(per metis P0-2 · 必须一并做,否则变异 g 永不红)**:

| 项 | 要求 |
|---|---|
| 新 env | `CEP_RECONCILE_DOCS_ROOT`(影子树文档根)⇒ 变异 g 靠它把扫描面指向影子副本 |
| 指纹集 | `FP_BEFORE` / `FP_AFTER` 的 `sha256sum` **必须纳入被扫的 3 份文档**(现只 hash 3 个 JSON)⇒ 否则「变异污染真实仓」检测不到 |
| 递归重入 | 变异调用点(`:405-407`)必须把新 env 透传下去(现只透传 3 个 JSON 的 env)⇒ 否则变异 g 读到**真实仓文档** ⇒ **永不红** |
| 注释与计数 | 脚本头注释「4 条决定性变异」与 `:430` 的「6 条」**同批**改为 7 条(⛔ `docs_sync_check` 不会抓这个,须人工同步) |
| 变异 g | 影子树里把 `07号` 档 2 SU 段(或手册 §12.10 段)的某行改成**肯定式**夸大表述 ⇒ 按签名红 |

### ②-2 · W6 详规(per metis P0-5 · 原「6 处陈旧数字」不可枚举)

**⛔ 问题**:`§10` 存在**两代现状快照**(`obj:1369`~`obj:1375` = 2026-10 段 · `obj:1443`~`obj:1448` = 2026-11-12 段)⇒ 陈旧面**不是 6 处,是「两代 + 当前值」三层** ⇒ 且两代的值彼此冲突(`未推送` = `17` vs `0`)。

**⇒ 改法裁定**:**就地改** `obj:1369` 那张表(依据 = 台账既有纪律「**§10 可就地同步**」,per `obj:1499` 纪律归属表)+ **追加**一段「2026-11-14 现状(实测)」声明「⛔ 上方 2026-10 段与 2026-11-12 段的现状快照**均已被本段取代**」⇒ 与 `Entry #355 ⑧ 第 5 项`「未推送 20 已过时,以本段为准」的**既有先例同形**。

**⇒ 真源命令表(A-6 的可执行依据)**:

| 行 | 现值(陈旧) | 应改为 | 真源命令 |
|---|---|---|---|
| `obj:1369` DoD 12 条 | 「**实质全达成**」 | 「**11/12 达成**;唯一未达成 = `B3b-2` = **0/4 ⏭**」 | ⛔ **不得**改写为「全达成」(禁 token:`✅(⏭)` / `全 ⏭ 计入达成` / `提前进入 Stage 4+` / `口径 ④ 已 ratify` / `Stage 4+ 已开 ⇒ 可归档`) |
| `obj:1371` 测试基线 | `ctest` **167/167** | 实测值 | `ctest --test-dir build -N \| grep -c 'Test *#'` |
| `obj:1372` ADR 签发 | 「19 份决策 ADR ⇒ 17 已签发」 | ⚠️ **保留原口径 + 另起一行**写文件数 | ⚠️ **三个分母不可互换**:**决策 ADR**(`19 份决策 ADR`)= 与文件数**不是同一集合**;**文件数 22** = `ls docs/adr/ADR-CEP-*.md`(**23 个**)**减** `ADR-CEP-NUMBER-MAP.md`;`obj:28` 台账头部写「21 份」⇒ **三处需一起对齐** |
| `obj:1374` 未 push | **17 commit** | 实测值(push 前 = 25) | `git rev-list --count origin/main..HEAD` |
| `obj:1445` 基线(2026-11-12 段) | `ctest` **169/169** | ⚠️ **不回改**(append-only 段)⇒ 由新段声明取代 | — |
| `obj:1447` 未推送(2026-11-12 段) | **0** | ⚠️ **不回改** ⇒ 由新段声明取代 | — |

### ③ ⚠️ 冻结面处置(实测依据)

W4 会**读** 2 个 M1 冻结集产物(`07号` · 新 main spec)⇒ 但只读不改 ⇒ **冻结面 hash 不变** ⇒ ⛔ **不需要五步链**。

⚠️ **例外(实施时当场判断)**:若实施中发现必须**修改**这两份文档的表述才能让扫描通过 ⇒ ⇒ **触发五步链**(台账 Entry 先落 → 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单单独 commit → 无参复验)⇒ ⛔ 不得为省事而改判据。

### ④ ⛔ 明确排除(out-of-scope · 本轮不做)

| 排除项 | 为什么排除 |
|---|---|
| **B3b-2 裁决 / objective 归档** | ⚠️ **不是本仓可单方面推进的事** —— ① 口径 ④ **已由 architect 裁决**(台账 Entry #332 · 2026-11-10 · oracle PASS-WITH-NOTES + critic REJECT)⇒ **critic 的 2 条 BLOCKER 已于 `Entry #332 ⑯` 闭合**;**未决的只是 MUST-FIX-3(归档语义)/ MUST-FIX-4(回滚路径)/ 用户 ratify** ⇒ ② 台账 Entry #334 ① 明确「⛔ **不引用** Entry #332 的『口径 ④』作为依据(该口径**未 ratify**)」;③ Entry #332 ⑨ 定义**三个终局分支**(Ⅰ 达成归档需 N ≥ 1 ⇒ 不可达 / Ⅱ 维持挂起 ⇒ 需 ≥4 次季度复审才到 Ⅳ 暂存归档 / Ⅲ 用户改判)⇒ 当前 = **终局 Ⅱ** ⇒ **归档不是「四条判据满足即可」**,而是**用户裁决**或**等季度复审** ⇒ ⇒ **规划域无权推进** |
| **`responder_mask_of` 4-bit 扩容 ADR** | 属 **architect 域**(需新 ADR / SU)· 虽实测 `sharer_tracker.hh` **不在** M1 冻结集 ⇒ 扩容不需冻结授权,但**属契约面** ⇒ 本轮只登记依赖 |
| **档 2 全族推广(其余 5 族)** | 依赖上一项 ⇒ 且 `kResponderBitToNode` 仅映射 **4 个节点**(bit0=SDMA / bit1=VRAM / bit2=GMMU / bit3=PCIe)⇒ 未映射节点 `responder_mask_of` 返回 0 ⇒ 多节点族 sharer 维度**数据表示上不可承载** ⇒ **能力缺口**,非工时问题 |
| **权限维 ADR** | 会**新增一条 invariant** ⇒ 破坏历次「新增 invariant 数 = 0」纪律 ⇒ 建议排最后(先广度后维度) |
| **顺手清 73 个历史归档件 tasks 未全勾** | 跨 change 的工具级历史遗留 ⇒ 单独立项(⛔ 已归档件内不改) |
| **改新 main spec 的 Purpose** | 「档 2 尚无载体」是 delta spec 的**引入前描述** ⇒ 归档契约不改 ⇒ 现行载体 cell 的指针已在 `07号` SU 2026-11-14 §①(Entry #413 登记) |
| **给 `FORBIDDEN` 表第 4 项改回契约原文** | AC-9 原文是「**档 2 强于档 1**」,闸现写「**强于档 1**」⇒ 闸**更严** ⇒ ⛔ **不放宽**(放宽 = 弱化判据)⇒ 仅登记措辞差异 |

### ⑤ ⚠️ 已知风险(本计划自带的失败模式)

| # | 风险 | 防线 |
|:--:|---|---|
| **R-1** | **为让闸变绿而弱化判据** —— W4 扩面后,`07号` 新 SU ② 与手册 §12.10 里本就含「通过态」「检测力已证」「所有族」等词(全部在**否定语境**)⇒ 实现者可能**扩大否定语境豁免窗口**来让基线变绿 | ⛔ 豁免窗口**固定 16 字符**且**只加 `NOT` 一个英文标记**(不加其他);任何扩大 ⇒ 须在台账登记 + **变异 g 必红** + 补「加标记后仍能红」的变异证明 |
| **R-2** | **G-2 的「部分达成」被粉饰成「已达成」** | ⛔ A-3 **锁死为 ⚠️**(⛔ 不得 ✅)· verifier 报告与台账 Entry **两处**逐字写明「行为级负控,⭔ 非 AC-2 原文要求的『按签名红』」 |
| **R-3** | **W6 顺手把「DoD 12 条」改成「全达成」** | ⛔ 禁 token 5 条(`✅(⏭)` / `全 ⏭ 计入达成` / `提前进入 Stage 4+` / `口径 ④ 已 ratify` / `Stage 4+ 已开 ⇒ 可归档`)⇒ **替代表述只允许「11/12 达成 + 唯一未达成 = B3b-2 = 0/4 ⏭」** |
| **R-4** | **把「补缺口」偷换成「改判据」** | ⛔ W1~W4 **不改任何 Requirement / Scenario** ⇒ 任何判据改动 ⇒ 路由回 planner |
| **R-5** | **死判据复发** —— 新增形参(`known_engine_masks` / `dropped`)若只被默认值调用 ⇒ 判据恒绿 | ⛔ **A-11**(死判据免税)3 条硬判据 |
| **R-6** | **反 fixture 做成伪测试** —— `set_sharer` 对未映射节点 / 越界 level **静默 no-op** ⇒ 播种失败 ⇒ 断言以「判据不灵」的方式红 | ⛔ W1 ③ **播种前置断言**(读回验证目录位已置) |

## Acceptance

> **判据来源**:verifier 报告 `.rddf/reviews/cep-evidence-tier2-minimal-slice-verify.md` 的 **G-1 ~ G-4**(逐字)+ 归档 proposal 的 **AC-7 / AC-9** 原文 + 本计划 W1~W7 的退出条件。
> ⚠️ **本轮验收只复核这 4 项 + W6/W7** ⇒ ⛔ **不重审**已通过的 10 项。

| # | 判据 | 可机判的验证动作 |
|:--:|---|---|
| **A-1** | **G-1 provenance 机器判据到位** | ① 正控:`detect()` **传入真实 mask 集合**(⛔ 禁用默认 `~0` —— 否则恒绿)⇒ 他人 bit 恰等于 peer `self_mask` ⇒ `provenance_broken == false`;② 反 fixture:播种 `kNodeIdGmmu` ⇒ `untraceable_bits != 0` ⇒ `provenance_broken == true`;③ 播种前置断言通过;⇒ 实跑 `./build/bin/cpptlm_tests "[tier2]"` 全绿 |
| **A-2** | **G-3 溢出路径被真实触发** | `set_observation_capacity(2)` ⇒ `dropped_observations() > 0` ⇒ `detect()` 的 `invalid == true` ⇒ 实跑绿 |
| **A-3** | **G-2 负控到位 + 差距被如实标注** | ① 两个行为级负控用例实跑绿;② ⚠️ **verifier 第 2 轮的判定只能是 ⚠️「部分达成」,⛔ 不得记 ✅**;③ verifier 报告与台账 Entry **各有一处**逐字写明「⭔ 非 AC-2 原文的『按签名红』」 |
| **A-4** | **G-4 段级扫描面 + 变异 g** | ① 扫描面 = **4 个「新增产出物」单元**(登记面整份 + `07号` 档 2 SU 段 + 手册 §12.10 段 + main spec 整份),**按标题定位不写死行号**;② **变异 g**(影子树 + `CEP_RECONCILE_DOCS_ROOT`)按签名红;③ `--mutations` ⇒ **7 条变异**(a~g)全按签名红 + 基线正控绿 + **真实仓字节指纹前后相等**(含 3 份文档);④ **「加 `NOT` 后仍能红」的变异证明**存在且通过 |
| **A-5** | **verifier 第 2 轮报告落盘(独立性)** | 报告由 **verifier 角色**产出(⛔ builder 不得代写判定)⇒ 逐条 G-1~G-4 判定 + **可复制的命令与输出** + 「已用轮次 = 2/3」+ ⛔ 声明未重审其余 10 项 |
| **A-6** | **台账 §10 现状表订正到位** | ① 按 **§②-2 的真源命令表**逐项实测后回填;② ⚠️ **三个 ADR 分母不互换**(`决策 ADR` / `文件数 22` / 台账头部 `21 份`)⇒ 三处一起对齐;③ `obj:1369` 的「DoD 12 条」= **11/12 达成 + 唯一未达成 = B3b-2 = 0/4 ⏭**;④ 追加「2026-11-14 现状(实测)」段声明取代两代旧快照;⑤ `cep_docs_sync_check.sh` rc=0 + `cep_dod_reading_check.sh` rc=0(禁 token 5 条不命中) |
| **A-7** | **push 完成且远端一致** | `git fetch origin` 后 `git rev-list --left-right --count origin/main...HEAD` = `0 0` |
| **A-8** | **0 回归(闸 allowlist)** | `ctest --test-dir build -j2` ⇒ **全绿**(⛔ 不写死数字)· **必须 rc=0 的闸**:17 个判据闸(与 verifier 报告 AC-13 同口径)· `cep_test_shadow.sh` 是**被 source 的 helper**(无独立入口)⇒ **排除**;`cep_experiment_run.sh` / `cep_h8_ab_runs.sh` / `cep_b4_coupling_probe.sh` / `cep_b3b2_d4_quarterly_review.sh` 是**实验 / 巡视 runner** ⇒ **排除**(其触达 `experimental/` ⇒ 与 A-9 的零 diff 冲突);· `docs_sync --strict` / `agents_md_lint` / `format --check` / `markdown_link_check` 全 rc=0 · 工作区干净 |
| **A-9** | **范围纪律(逐字抄归档契约 AC-7)** | `git diff` 对 `include/bundles/` · `docs/adr/` · `include/tlm/cep/sharer_tracker.hh` · `experimental/**` · `tools/render_report.py` · `scripts/test/cep_experiment_run.sh` · `test/cep_e2e_run_tiers.json` · `test/cep_experiment_harness.hh` · `protocol_instance.schema.json` ⇒ **全部零 diff**;⚠️ **W1 的反 fixture 只用 `sharer_tracker.hh` 的现有 4 参重载,⛔ 不得改该文件**;· `grep -rn '#include[[:space:]]*"\(test/\|\.\./test\)' include/` ⇒ 0 命中;· 新增 `INV-CEP-` 数 == **0**;· `SharerTrackerIf` 虚方法数不变;· `openspec/specs/cep-evidence-tier2-observation/spec.md` 的 **Requirement / Scenario 逐字未变** |
| **A-10** | **冻结面** | 若 W1~W4 **零文档改动** ⇒ 无参 `cep_freeze_check.sh` rc=0(85 产物 hash 全一致);若**触发了文档改动** ⇒ 五步链完整走完 + 授权号 = 台账真实 Entry + 清单单独 commit + 无参复验 |
| **A-11** | **死判据免税(per metis P0-4 · 本轮唯一防 G-1/G-3 复发的判据)** | ① 每个新增形参(`known_engine_masks` / `dropped`)≥ **1 处非默认值调用点**(grep 可判)⇒ ⛔ `known_engine_masks` 的正控**禁传默认 `~0`**;② **决定性变异**:把 `provenance_broken` 恒置 `false` ⇒ **必有例变红**;③ `set_observation_capacity` ≥ **1 处非 4096 调用点**;④ ⚠️ 若实施结束时 WIP 仍「机件在、用例无」⇒ ⛔ **回滚该批 WIP**(禁留死判据) |
| **A-12** | **提交面与回滚粒度** | 三个提交面**分开**:① 实施 commit(W1~W4 + 判据)· ② 台账订正 commit(W6 + 回执 Entry)· ③ push(W7)⇒ 回滚粒度 = 单 commit 可 `git revert`;· `.rddf/improvements/cep-tier2-verification-closure.md`(未跟踪)与 `.rddf/state/role.json`(已改)必须**显式归属**(提交或还原)⇒ A-8 的「工作区干净」才成立 |

## Capabilities

**新增能力(本轮交付后成立)**:

| # | 能力 | 之前 | 之后 |
|:--:|---|---|---|
| **C-1** | **provenance 可机判** | 无判据(靠人读) | 「他人 sharer 位不可溯源」⇒ `provenance_broken` ⇒ 判不可信 |
| **C-2** | **有界性可机判** | 溢出路径**从未执行** | `dropped > 0` ⇒ 判「不可判」⇒ ⛔ 不记零违例 |
| **C-3** | **防夸大扫描面 = 「新增产出物」段级** | 只扫 1 个登记面(整文件) | 登记面 + `07号` 档 2 SU 段 + 手册 §12.10 段 + main spec(整份)⇒ **AC-9 的字面读法** |
| **C-4** | **台账现状陈述自洽** | 内部自相矛盾 + 两代快照冲突 | 11/12 + 唯一未达成项明确 + 两代旧快照被显式取代 |
| **C-5** | **「归档后缺陷补正」路径的先例与边界** | 无先例(每次靠临场判断) | 4 条理由 + 4 条排除项 + rebuttal 条款 ⇒ 边界明确且窄 |

**⛔ 不新增**(保持本轮纪律):invariant 数 **0** · ADR 数 **0** · DoD 条目数 **12**(不改文本)· B3b-2 计数 **0/4**(不动)· KPI slot **0**(不新建 change)· 冻结产物数 **85**(零文档改动时不变)· `FORBIDDEN` 词表**不放宽**

**⚠️ 边界(本轮能力不覆盖)**:

| # | 不覆盖 | 归属 |
|:--:|---|---|
| **B-1** | **协议语义全覆盖** | 权限维 / 标签维不在档 2 范围内(见 `07号` SU 2026-11-14 ②) |
| **B-2** | **多节点族 sharer 维度** | 4-bit 映射 ⇒ **能力缺口** ⇒ 4-bit 扩容 ADR 之后才谈 |
| **B-3** | **档 2 的「通过态」** | ⭔ 本档位不产 run ⇒ 无 verdict ⇒ **永远不会有** |
| **B-4** | **B3b-2 的 4 个受限族** | 规约真源仓外 / 受限 ⇒ 本仓推不动 |
| **B-5** | **AC-2 的「按签名红」** | 本轮只到**行为级负控** ⇒ ⚠️ **部分达成**(见 D-1) |

## Impact

**文件改动面(预期 · 全部可核)**:

| 文件 | 改动 | 状态 | 冻结面 |
|---|---|---|:--:|
| `include/tlm/cep/protocol_engine_base.hh` | `set_observation_capacity` / `observation_capacity` / `observation_capacity_` | **已在工作区(未提交)**⇒ W1/W2 **零改动** | ✗ |
| `test/test_cep_tier2_observation.cc` | `Tier2Verdict` provenance 字段 · `detect()` 新形参 | **已在工作区(未提交)**⇒ W1/W2/W3 **只补用例** | ✗ |
| `scripts/test/cep_e2e_reconcile_check.sh` | W4 段级扫描面 + `NEG_MARKERS` 补 `NOT` + 新 env + 指纹集扩 + 变异 g + 注释/计数同步 | **将改** | ✗ |
| `.rddf/reviews/cep-evidence-tier2-minimal-slice-verify.md` | W5 第 2 轮段 | **将改**(verifier 域) | ✗ |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | W6 §10 订正 + 追加现状段 + 回执 Entry | **将改** | ✗ |
| `.rddf/improvements/cep-tier2-verification-closure.md` | 本文件(planner 域产物) | **未跟踪 ⇒ 须显式提交** | ✗ |
| `.rddf/state/role.json` | 角色切换记录 | **已改 ⇒ 须显式提交或还原** | ✗ |
| `docs/architecture/07-coherence-exploration-platform.md` | **计划零改动**(仅被 W4 读) | 只读 | ✓ IN |
| `openspec/specs/cep-evidence-tier2-observation/spec.md` | **计划零改动**(仅被 W4 读) | 只读 | ✓ IN |

**KPI / 计数影响**:Proposed **0/3 → 0/3**(不新建 change)· 归档 change **143**(不新增)· cep-* main spec **43**(不新增)· invariant **66**(不新增)· 冻结产物 **85**(零文档改动时不变)

**风险与回滚**:

| 项 | 内容 |
|---|---|
| **最大风险** | R-1(弱化判据让闸变绿)⇒ 防线 = 变异 g + 豁免窗口固定 + 变异证明 |
| **次风险** | R-5(死判据复发)⇒ 防线 = A-11 |
| **回滚路径** | 三个提交面分开 ⇒ 单 commit `git revert` 即可;若已 push ⇒ 追加修复 commit(⛔ 禁改历史) |
| **不可逆项** | ⛔ **push 不可撤**(但 `git revert` 可补偿)⇒ W7 排在最后 |

## 待裁

| # | 待裁项 | 谁能裁 | 不裁的后果 | 可执行分支 |
|:--:|---|---|---|---|
| **D-1** | **W3 的「部分达成」是否可接受** —— AC-2 原文要求「按签名红」,本轮只能给行为级负控 | **verifier**(W5 判定时) | 若不可接受 ⇒ W3 需升级 | ⛔ **不升级为 change**;升级路径 = **观测事实导出 JSON + 新 shell 闸 + 影子树变异**(≈ 0.5~1 人日;⚠️ 新闸若入 `scripts/test/` ⇒ **触发五步链**)⇒ 届时**须回 planner 重排** |
| **D-2** | **是否在本轮顺带做 4-bit 扩容** | architect(需新 ADR) | ⛔ 本轮**明确排除** | 若要并入 ⇒ 另立 improvement + metis 新轮 |
| **D-3** | **B3b-2 / objective 归档的用户门** | **用户** | ⛔ 终局 Ⅱ 持续挂起 | 三选项:① ratify 口径 ④(需先闭合 MUST-FIX-3/4 + 新 SU + obj §12 判定规则段)② 走终局 Ⅲ(用户另行裁决 DoD 结构)③ 维持 Ⅱ ⇒ **本轮均不推进**,只登记 |

## metis 审查(第 1 轮 · 2026-11-14)处置(逐条 · 13 条)

> **审查者**:metis(planner 域固定审查者 · 本 improvement **总计 1 轮**)· **返回 6 节**:隐藏意图 / 歧义 / AI slop 风险 / 缺失的验收标准 / 边界情况 / 建议修改
> **处置原则**:metis 的**每条事实主张均经 planner 独立复核**;复核不成立的**据实驳回并登记**(不盲从)。

| # | metis 建议 | 复核结论 | 处置 |
|:--:|---|---|---|
| **P0-1** | 加「工作区现态」段;Impact 表把已存在的改动列为「将改」⇒ 重复添加 | ✅ **成立**(实测 `git diff` = 2 文件 +38 行) | **已采纳** —— 新增「⚠️ 工作区现态」段 + Impact 表改「状态」列 + **A-11 ④**(WIP 无用例 ⇒ 回滚) |
| **P0-2** | W4 不可执行(8 处假红)⇒ 改锚行集合 或 逐条裁定假红 | ✅ **成立且实测复核**(planner 实测 **7 处假红**,与 metis 列表一致)⇒ 根因 = FORBIDDEN 是**档位相关**词表却扫**不分档位**范围 | **已采纳且改用第三种形态(优于 metis 的两个选项)**:**段级「新增产出物」扫描面** —— 依据是 AC-9 原文「**新增产出物内** grep」的**字面读法**(文件级扫描反而超出契约)⇒ 配套:`NEG_MARKERS` 补 `NOT` + 不改 `has_negation_context` 方向 + plumbing(env / 指纹 / 递归重入 / 注释计数)+ 变异 g |
| **P0-3** | A-3 锁死为 ⚠️ 不得 ✅;D-1 写成可执行分支 | ✅ **成立** | **已采纳** —— A-3 ② 明写「⛔ 不得记 ✅」;D-1 给出「观测事实导出 JSON + 新 shell 闸 + 影子树变异(≈0.5~1 人日·触发五步链)」的可执行分支 + 「须回 planner 重排」 |
| **P0-4** | 新增「死判据免税」AC(新增形参非默认值调用点 / provenance 恒 false 变异 / cap 非 4096 调用点) | ✅ **成立** —— `known_engine_masks` 默认 `~0` ⇒ `untraceable` 恒 0 ⇒ **正是 G-1/G-3 的原始失败模式在新一层复发** | **已采纳** —— A-11 4 条 |
| **P0-5** | A-6 去空判据 + 给真源命令表 + 裁定「改旧段 vs 追加段」 | ✅ **成立**(metis 的 `obj:1454` 行号引用**有偏差** —— 该行是 Q2 行;真正的纪律分类在 `obj:1499`) | **已采纳** —— §②-2 真源命令表 5 行;改法裁定 = **就地改 `obj:1369`(依据「§10 可就地同步」per `obj:1499`)+ 追加 2026-11-14 现状段声明取代两代旧快照**(与 `Entry #355 ⑧` 同形先例) |
| **P1-6** | §④ 事实订正:critic 2 条 BLOCKER **已闭合**;未决 = MUST-FIX-3/4 + ratify | ✅ **成立**(实测 `obj:18402` + `obj:18530` ⑯ 闭合记录) | **已采纳** —— §④ 排除项第 1 行已改写;**结论(不可单方面推进)不变**,只换论据 |
| **P1-7** | 补 rebut `AGENTS.md`「公共 API ⇒ 必须走 openspec」 | ✅ **成立** —— 原草稿**无一条正面 rebuttal** | **已采纳** —— §① 新增 rebuttal 行(判定 `set_observation_capacity` 为「测试注入 seam,非契约面」,依据 = main spec 未定义可注入性)+ 双重登记要求 + verifier 判为契约面则升级为 change |
| **P1-8** | A-9 逐字抄 AC-7(补 `sharer_tracker.hh` / `cep_experiment_run.sh` / `protocol_instance.schema.json`) | ✅ **成立** | **已采纳** —— A-9 补齐 9 项 + 加「W1 的反 fixture 只用现有 4 参重载,⛔ 不得改该文件」 |
| **P1-9** | A-8 改 allowlist(42 个闸里有 helper 与 4 个 runner) | ✅ **成立** —— `cep_test_shadow.sh` 是 helper;4 个是 runner ⇒ 与 A-9 的 `experimental/` 零 diff **互斥** | **已采纳** —— A-8 改为「17 个判据闸(与 verifier AC-13 同口径)」+ 显式排除 5 个 + 排除理由 |
| **P1-10** | 统一 ADR 计数口径(三个分母不可互换) | ✅ **成立** —— `19 份决策 ADR` / `22 份文件` / `obj:28` 的 `21 份` 是**三个不同集合** | **已采纳** —— §②-2 真源命令表 + A-6 ② 明写三处一起对齐 |
| **P2-11** | 估时改区间(W4 是 1 人日量级) | ✅ **成立** | **已采纳** —— 估时改 **1.0 ~ 1.5 人日**,按 W4 两分支分列 |
| **P2-12** | 补提交面(未跟踪的 improvement + 已改的 role.json) | ✅ **成立** | **已采纳** —— Impact 表加两行 + **A-12** 三个提交面分开 + 回滚粒度 |
| **P2-13** | W5 加独立性条款(verifier 产出,builder 不得代写) | ✅ **成立** | **已采纳** —— A-5 ①「⛔ builder 不得代写判定」+ 「可复制的命令与输出」 |

### ⚠️ metis 提法中**未被采纳**的部分(如实登记)

| metis 提法 | 不采纳理由 |
|---|---|
| W4「或保留文件级扫描 + 把 8 处假红逐条裁定为历史行白名单」 | 白名单 = **逐条豁免** ⇒ 与 R-1 冲突(等于把判据放松到能通过);且段级扫描面已**字面符合** AC-9 ⇒ 无需白名单 |
| W4「或照抄 `cep_dod_reading_check.py` 的 A1/A2/A3 锚常量 + `--root` 影子树模式」 | 该范式是**行级锚点**(适合「某一句必须逐字存在」),而 AC-9 要的是「**新增产出物内 grep 禁表述 ⇒ 命中即红**」⇒ **需要的是范围(段)不是锚点(行)** ⇒ 采用**段级 + 影子树 `--root` 的思路**,但不采用行级锚常量 |
| 「A-8 要求 42 个闸全 rc=0 与 A-9 冲突」 | ✅ **采纳该结论**,但**不采纳**「改 allowlist 到 17 个」的唯一解 —— 改为**显式分类**(17 判据闸必过 + 5 排除 + 其余为可选)⇒ 保留覆盖面且不自相矛盾 |
| 「§④ ③ KPI 占 slot 是伪理由」 | ✅ **采纳**(已删该条作为独立理由)⇒ 但**理由 ③ 已改写**为「时间窗不对称」(只碰实现不碰契约面)⇒ 结论保留、论据更准 |

## 关联

· verifier 报告 `.rddf/reviews/cep-evidence-tier2-minimal-slice-verify.md`(G-1~G-4 缺口清单 · 10 ✅ + 4 ⚠️)
· architect 裁决 `.rddf/reviews/cep-tier2-prerequisite-rulings.md`(C′ / C′-bis R1~R8 / C′-bis-ter 勘误)
· 归档件 `openspec/changes/archive/2026-11-14-cep-evidence-tier2-minimal-slice/` · 新 main spec `openspec/specs/cep-evidence-tier2-observation/spec.md`
· 台账 **Entry #410**(实施回执)/ **#411**(五步链授权)/ **#412**(归档回执)/ **#413**(指针链 + 既有债登记)/ **#332**(口径 ④ 裁决 · critic 2 BLOCKER 已闭合 · **未 ratify**)/ **#334**(⛔ 不引用未 ratify 口径)/ **#355 ⑧**(「以本段为准」先例)/ **#356**(二次盘点)
· `ADR-CEP-20` 决策 5 / 6 · `ADR-CEP-06` SU#4 ② · `docs/architecture/07-coherence-exploration-platform.md` SU 2026-11-13 ① / SU 2026-11-14 · `docs/guide/cep-usage.md` §12.7 / §12.9 / §12.10
· 闸 `scripts/test/cep_e2e_reconcile_check.sh`(`NEG_MARKERS` / `has_negation_context` / `FORBIDDEN` / env / 指纹集)· `scripts/test/cep_dod_reading_check.py`(锚行范式先例 · S6 禁 token 5 条)
