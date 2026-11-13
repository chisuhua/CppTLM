---
name: cep-evidence-tier-declaration
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 证据档位**登记**与 data-value 喂料映射见证(落 ADR-CEP-20 决策 6;⚠️ **不含** SWMR 见证 —— 结构不可达)
依赖:
  - ADR-CEP-20
  - ADR-CEP-06
  - ADR-CEP-19
状态: "🟡 待实施(2026-11-13 起草 · **v2 已按 metis 首轮 4 条 P0 重写**;per **ADR-CEP-20** 决策 6(ii)+ §5 抓取点 6 + 台账 **Entry #402** 遗留 ①)"
created: 2026-11-13
---

# cep-evidence-tier-declaration: 证据档位**登记**与 data-value 喂料映射见证

> **范围纪律**:⛔ 不改任何判据口径 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款 · ⛔ **不改 `ADR-CEP-20` 的任何决策**(本 change 只**落**它的义务)· ⛔ **新增 invariant 数 = 0** · ⛔ 不改 B4 结论 · ⛔ 不动 `B3b-2` · ⛔ **不改 `test/cep_adr_structural_deferrals.json`**(V-20-6 的机器载体留给**档 2 change** 一并处理)· ⛔ **不实施档 2**。
> **估时**:≈**0.5-1 人日**(metis 首轮后**下调**:原稿的 item 1 被证为**零增量** ⇒ 已重写;item 4 移出)。

## Why

**`ADR-CEP-20`(已签发 v1.0)登记了 2 项**未被机器面覆盖的**义务**(**不是 3 项** —— 原稿的第 3 项「阶梯入总纲」保留,但「检测力见证」已按下述**降级**):

| # | 义务(ADR-CEP-20 出处) | 现状(实测) | 本 change 的处置 |
|:--:|---|---|---|
| 1 | **决策 6(ii)** 跨档聚合禁令须有**机器面** | ⛔ **缺** —— 实测 `test/cep_e2e_runs.jsonl` 3 条中唯一 `"ic3_verdict": "✓ 通过"` 是**档 0**;`ic3_verdict` 三态词表不分档;**且 `evidence_scope` 甚至不在 `cep_e2e_reconcile_check.sh` 的 `required` 9 键内** ⇒ 缺该字段时会 `KeyError`(**无签名** ⇒ 违反「rc≠0 不构成红因」) | ✅ **补机器面**(旁挂档位表 + 三判据 + 影子覆盖开关 + `evidence_scope` 入 required) |
| 2 | **决策 6** 能力阶梯(档 0/1/2)+ 档位声明义务**显式化** | ⛔ **只在 ADR 内** —— `07 号`(总纲)与 `docs/guide/cep-usage.md` **均无**档位阶梯 | ✅ 总纲新 SU + 手册**新 §12.9**(⛔ 不塞进 §12.8) |
| 3 | **决策 3b** 档 0 的**检测力见证(B′)** | ⚠️ **原稿误判** —— metis 实测:① `test/test_cep_stub_cache_emit.cc:126-128` 对写事件**只**调 `detector.write(...)`,**从不**调 `write_without_invalidate(...)`;`SwmrDataValueDetector::write()`(`test/cep_experiment_harness.hh:157-162`)无条件置 `written=true`、**不累加 `swmr`** ⇒ ⇒ **喂料通路结构上无法表达 SWMR 违例**;② 真实流 `req.data` 恒 0(`traffic_gen_tlm.hh:233`)⇒ data-value 恒不触发 | ⚠️ **降级为两项**:(a) **data-value 的喂料映射见证**(synthetic fixture,**新增真实信息**)(b) **SWMR 结构不可达的登记**(⛔ **不产出 SWMR 见证** —— 那是做不到的,不是没做) |

**⚠️ 存量已被覆盖、⛔ 本 change 不重写(metis 实测,5 个文件)**

| 已有覆盖 | 落点 |
|---|---|
| 检测器**正控**(不注入 ⇒ 0)+ 红绿分离(`enabled` vs `disabled`) | `test/test_cep_experiment_harness.cc:87-130` |
| **SWMR 负控** | `test/test_cep_experiment_harness.cc:99-106` · `test_cep_stub_cache_e2e.cc:695` · `test_cep_simulation_mode_lock.cc:226` · `test_cep_b4_evidence_device.cc:97/130/171` |
| **data-value 负控** | `test/test_cep_experiment_harness.cc:108-115` · `test_cep_stub_cache_e2e.cc:708` · `test_cep_b4_evidence_device.cc:104/174` |

⇒ ⇒ **所以本 change 的第 3 项**⛔ **不是**「再写一遍负控」(那是同义反复),而是:① 证明**生产喂料映射**(`s.is_write` / `s.data` / `s.addr` 三字段解释)能把 data-value 形态**送达**检测器(存量负控**不经过**该映射);② 把「SWMR 在喂料通路**结构不可达**」这一事实**登记**下来(替 ADR-CEP-20 决策 3c 的弱点补上**可复核的成因**)。

**⚠️ 上游矛盾登记(metis 新发现 · ⛔ 不改 ADR 正文,只在此与本 change 的台账 Entry 记录)**

`ADR-CEP-20` 决策 6(iii) 载体对账表**第 5 行**把 `docs/guide/cep-usage.md §12.8` 标为「**已有**」档位口径 ⇒ **实测该节 `grep 档` 0 命中档位语**(它只有「无 sharer / 无版本」的**口径句**)⇒ ⇒ **V-20-1 的「8 处已齐」前提不成立**;本 change 补的是**阶梯句**而非**口径句**,该缺口自此**有人认领**。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **data-value 喂料映射见证 + SWMR 结构不可达登记** | `test/test_cep_stub_cache_emit.cc`(**新 TEST_CASE**,`[cep][exp][evidence-tier][synthetic-fixture]`) | ① 构造 `std::vector<TxnStamp>`(同地址两次写、`data` 不同 ⇒ 模拟版本变化;随后 `read` 观测**旧** `data`)⇒ **调用与生产逐字相同的喂料循环**(把 `:120-131` 的循环抽成文件内 `feed(detector, stamps)` helper,**两处共用**)⇒ 断言 `counts().data_value > 0`;② 对照组 = **同一 vector**,仅把 `read` 的观测值改为**最新** ⇒ `== 0`(**同一输入、唯一差异** ⇒ 可归因);③ **明写**「SWMR 在喂料通路**结构不可达**(`:126-128` 只调 `write()`)⇒ 本项**不**产出 SWMR 见证」;④ ⛔ **不设 `CEP_EXP_DIR`**(新 case 必须无产物副作用);⑤ 打 `[synthetic-fixture]` 显式标注(先例:`cep_b4_observation_keys_check.sh` 的自我合成标注) |
| 2 | **档位登记机器面** | `test/cep_e2e_run_tiers.json`(**新增旁挂**)· `scripts/test/cep_e2e_reconcile_check.sh` | **⛔ 不改** `test/cep_e2e_runs.jsonl`(append-only);旁挂表 `{run_id → tier}`(真假 run 3 条全登);闸加 3 判据:① 每条 jsonl 条目在 tiers 表**有且仅有 1 个**档位 ② **档 0** 条目的 `evidence_scope` 含**真源词表**之一(`档 0` / `拓扑级`)③ tiers 表的 `run_id` **必须都在** jsonl(反向零 orphan,防僵尸登记);**加 `CEP_RECONCILE_JSONL` / `CEP_RECONCILE_TIERS` 覆盖**(对照 `cep_full_green_run_check.sh:28-34` 的 `CEP_FGR_*`)⇒ 毒化可在**影子树**执行;`evidence_scope` **加入 `required`**(3 条现存条目均有 ⇒ 零回归),缺键给**独立签名** |
| 3 | **阶梯入总纲 + 手册** | `docs/architecture/07-coherence-exploration-platform.md` **新 SU**(冻结件 ⇒ **五步链**)· `docs/guide/cep-usage.md` **新 §12.9**(⛔ **不塞进 §12.8**;§12.8 只加**一行导航**) | 三档阶梯 + 档位声明义务 + 跨档聚合禁令(指针 `ADR-CEP-20` 决策 6);⛔ 不改 `07 号` 正文 |

**⚠️ `07 号` 新 SU 的标题约束(逐条写死 · metis 实测 5 条)**

| # | 约束 | 依据 |
|:--:|---|---|
| 1 | 形态 = `## 附录 · Status Update 2026-11-13(证据档位阶梯 · per 台账 Entry #NNN)—— …` | `cep_dod_reading_check` **S7**:`## Status Update 2026-11-13` 前缀须**恰 1 命中**(既有先例) |
| 2 | ⛔ **不得**含「**拓扑级 IC3 通过态**」 | `cep_full_green_run_check.sh:140-147` 的 **C5** 取含该短语的标题 `target[-1]` ⇒ 会去新 SU 找 run 目录名并**必红** |
| 3 | ⛔ 不得与 `:639` 的既有表题**逐字相同** | `cep_index_position_check` 的重复标题判据 |
| 4 | 插在 `## §末 文档索引` **之前**;重跑 `scripts/cep/generate_doc_index.py` | 索引 A/B + 自述数 |
| 5 | 若复述「B3b-2 禁令」类 token,**同行须带作用域 token** | `cep_dod_reading_check` **S5** |

**⚠️ `cep_dod_reading_check` 的已知盲区(本 change 顺带记录,⛔ 不在本 change 修)**

`S6` 的禁表述扫描面 = **仅**「首个 `## Status Update 2026-11-13` 开头」的块 ⇒ 新 SU(「附录 ·」开头)**不在扫描面内** ⇒ 新 SU 里若写「档 0 已证检测力」**不会被任何闸抓到** ⇒ 登记为残余(**⛔ 不放宽,改成「全部 SU 块」属独立闸改动,另立**)。

## Acceptance

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1(data-value 喂料映射见证 · 正负配对)**:同一 `std::vector<TxnStamp>`,仅切换 `read` 的观测值(旧 vs 最新)⇒ **两次** `counts().data_value` 差 = 违例数(**正控 == 0** / **负控 > 0**)⇒ ⛔ 不得用两段不同代码充当「注入」
- [ ] **AC-2(喂料映射被真正走过)**:新用例调用**与生产逐字相同**的 `feed(detector, stamps)` helper(`:120-131` 抽出后两处共用)⇒ ⛔ 不得只调 `write_without_invalidate` 绕过映射
- [ ] **AC-3(SWMR **结构不可达** 的登记)**:逐字写明「`:126-128` 只调 `write()`;`write()` 不累加 `swmr` ⇒ **任何** `txn_stamps` 序列都只能产出 `swmr == 0` ⇒ 本 change ⛔ **不**产出 SWMR 见证」⇒ 该条为**登记项**(文档层),⛔ **禁当鉴别力判据**
- [ ] **AC-4(档位登记三判据)**:① 每条 jsonl 条目在 tiers 表**有且仅有 1 档** ② 档 0 条目 `evidence_scope` 含真源词 ③ tiers 表 `run_id` ⊆ jsonl(**反向零 orphan**)⇒ 三判据全绿;**且** `evidence_scope` 已入 `required`,缺键按**独立签名**红
- [ ] **AC-5(影子变异 · 3 条真变异 + 签名)**:`CEP_RECONCILE_*` 影子树上 —— (a) 删档 0 条目的「拓扑级」⇒ 签名「档位声明缺失」;(b) 删 tiers 表某 `run_id` ⇒ 签名「档位未登记」;(c) 给 tiers 表加一条 jsonl 不存在的 `run_id` ⇒ 签名「僵尸档位登记(反向 orphan)」⇒ **三条按签名红 + 还原绿**
- [ ] **AC-6(阶梯入总纲 + 手册)**:`07 号` 新 SU 落盘(⛔ 不改正文;**5 条标题约束逐条满足**)⇒ 索引重生成;`cep-usage.md` **新 §12.9** 落盘 + §12.8 **一行导航**(⛔ 不加状态列)⇒ `cep_index_position_check` / `cep_docs_sync_check` / `cep_doc_ref_check` / `markdown_link_check` 全绿
  ⚠️ **载体状态如实标注**:本 AC 的三闸全绿只证明**文档能解析 / 锚点在 / 索引一致**,**⛔ 不证明阶梯内容正确** ⇒ 语义正确性 = **人工评审**(per `ADR-CEP-20` V-20-2 的 `doc_layer_manual_review`),⛔ **禁当鉴别力判据**
- [ ] **AC-7(冻结面 + 五步链 · 两次 `--update`)**:Capabilities = **ADDED** ⇒ 归档合并出 `openspec/specs/cep-evidence-tier-declaration/spec.md` ⇒ 冻结产物 **83 → 84** ⇒ ① 归档前 `--update`(`07 号` 新 SU)② **归档合并 spec 后第二次** `--update`(新 main spec 自动入集)⇒ 两次均用同一授权 Entry 号 ⇒ provenance 成立
- [ ] **AC-8(0 回归)**:`ctest` **全绿**(⛔ **不写死 176**;以 `ctest -N` 实测总数为准,若数量变化须**逐条说明**新增/删除的测试名)· 全部 `scripts/test/cep_*.sh` 逐个 rc=0 · format / docs_sync / doc_ref / agents_md_lint / freeze / reconcile / adr_structural 全 rc=0
- [ ] **AC-9(范围纪律)**:⛔ 未改任何判据 / 阈值 / 预登记(`preregistration.json` **零 diff**)/ `metrics.schema.json` / retention · ⛔ 未改 `ADR-CEP-20` 决策 · ⛔ **新增 invariant 数 = 0** · ⛔ 未改 `test/cep_adr_structural_deferrals.json`(V-20-6 留给档 2 change)· ⛔ 未动 `B3b-2` · ⛔ 未改 B4 结论

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-evidence-tier-registry` | **ADDED** | **证据档位登记面**:jsonl 条目 ↔ 档位表的双向零 orphan + 档 0 条目的档位词义务 + 生产喂料映射的 data-value 可达性见证。⚠️ **名字刻意不含「检测力见证」** —— 档 0 的语义空洞**不因本 change 改变**(per `ADR-CEP-20` 决策 1/2/3c) |

## Impact

| 项 | 内容 |
|---|---|
| **冻结面** | ✅ **2 次**(`07 号` 新 SU ⇒ 归档前 `--update`;新 main spec 自动入集 ⇒ 归档后**第二次** `--update`)⇒ 产物 **83 → 84** |
| **KPI slot** | ✅ 占 **1 个**(当前 0/3)⇒ 归档后回填 0/3 |
| **风险** | ① B′ 仍可能被读成「档 0 已证检测力」⇒ **capability 名刻意避开** + AC-3 逐字登记 ② 档位词表双写(脚本 + 文档)⇒ 真源单点(脚本内常量 + 注释指向 `ADR-CEP-20` 决策 6 表格)③ `07 号` 新 SU 的 **C5 陷阱**(标题不得含「拓扑级 IC3 通过态」)⇒ AC-6 逐条约束 |
| **不可约残余** | ① 档 0 的**语义空洞**不因本 change 改变 ② **SWMR 侧无见证**(喂料通路结构不可达)⇒ 档 0 的 SWMR 0 连「组件见证」都没有 ③ 档 2(协议引擎不变量观测面)仍**未排期** ④ `cep_dod_reading_check` S6 的扫描面盲区(新 SU 不在禁表述扫描内)⇒ 登记不改 |

## metis 首轮处置(2026-11-13 · 逐条)

| metis finding | 处置 |
|---|---|
| **P0-1** item 1 是**零增量**(5 处存量负控已覆盖;且 SWMR 在喂料通路**结构不可达**) | ✅ **重写** —— 改为 data-value **喂料映射**见证(正负配对、唯一差异)+ AC-2 引用存量覆盖**不重写** + AC-3 把 SWMR 降为**登记** |
| **P0-2** `hypothesis_id == "h3"` **不是档位键**(漏检 2/3 + 上线即绿 + 会误伤档 1) | ✅ **重设计** —— 旁挂 `test/cep_e2e_run_tiers.json`(3 条全登)+ 三判据 + 反向零 orphan + 影子覆盖开关 + `evidence_scope` 入 `required` |
| **P0-3** 冻结面数字**错**(ADDED ⇒ 83 → 84,且需**两次** `--update`) | ✅ 已改(AC-7) |
| **P0-4** item 4(V-20-6 的 disposition)是**条件句/空头** | ✅ **移出** —— 改为范围纪律「⛔ 不改 `cep_adr_structural_deferrals.json`」,留给档 2 change |
| P1-5 `07 号` 标题约束**漏 C5** | ✅ 已补为 5 条逐条约束 |
| P1-6 AC-8 与范围纪律重复 / AC-9 凑数 | ✅ AC-8 删除(并入 AC-9)· AC-9 重构为 **3 条真变异 + 签名**(AC-5) |
| P1-7 `ctest 176` 硬编码 | ✅ 已去(AC-8 改为「以 `ctest -N` 实测为准」) |
| P2-8 `cep-usage.md §12.8` **不是教学位**(自述「不引入状态列」) | ✅ 已改为**新 §12.9** + §12.8 **一行导航** |
| P2-9 「闭合它」/「流经检测器」措辞 + capability 名 | ✅ L「登记边界 + 补组件见证」· capability 改名 `cep-evidence-tier-registry` |
| P2-10 **上游矛盾**(ADR 6(iii) 第 5 行标 §12.8「已有」而实测 0 处档位语) | ✅ 已登记(Why 末段) |

## 关联

**ADR-CEP-20**(v1.0 · 决策 6 + §5 抓取点 6)· **Entry #402**(签发回执 · 遗留 ①)· **#401**(ADR 授权)· **#399**(遗留 L-1)· `ADR-CEP-06` SU#4 ② · `test/test_cep_stub_cache_emit.cc` · `test/cep_experiment_harness.hh` · `scripts/test/cep_e2e_reconcile_check.sh` · `scripts/test/cep_full_green_run_check.sh`(C5 约束)· `docs/architecture/07-coherence-exploration-platform.md` · `docs/guide/cep-usage.md`
