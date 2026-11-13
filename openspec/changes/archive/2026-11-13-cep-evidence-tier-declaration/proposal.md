# cep-evidence-tier-registry: 证据档位**登记**与 data-value 喂料映射见证

> **状态**: 🟡 **Proposed**(2026-11-13 立项 —— per 用户口令「按你建议的顺序推进」;KPI 并发 **0/3 → 1/3**)
> **立项依据**: **`ADR-CEP-20`**(已签发 v1.0 · 决策 6(ii) + §5 抓取点 6)· 台账 **Entry #402**(签发回执 · 遗留 ①)· **Entry #399**(遗留 L-1)· improvement `.rddf/improvements/cep-evidence-tier-declaration.md`(**v2 · 已按 metis 首轮 4 条 P0 重写**)
> **⛔ 范围纪律**:本 change **只做**「档位登记机器面 + 阶梯入总纲/手册 + data-value 喂料映射见证」;**⛔ 不改 `ADR-CEP-20` 的任何决策** · ⛔ 不改任何判据 / 阈值 / 预登记 / `metrics.schema.json` / retention · ⛔ **新增 invariant 数 = 0** · ⛔ 不改 `test/cep_adr_structural_deferrals.json`(V-20-6 留给**档 2 change**)· ⛔ 不实施档 2
> **估时**:≈0.5-1 人日(metis 首轮后**下调**;全为 test-side + 闸 + 文档)

## Why

**`ADR-CEP-20` 已签发,但其 2 项义务**没有机器面覆盖**;另有 1 项被**误判为可做**(metis 实测推翻):

| # | 义务 | 实测缺口 | 后果 |
|:--:|---|---|---|
| 1 | 决策 6(ii) **跨档聚合禁令** | `test/cep_e2e_runs.jsonl` 3 条中**唯一** `"ic3_verdict": "✓ 通过"` 是**档 0**;三态词表不分档产量无 tier 字段;**且 `evidence_scope` 不在 `cep_e2e_reconcile_check.sh` 的 `required` 9 键内** ⇒ 缺字段即 `KeyError`(**无签名** ⇒ 违反「rc≠0 不构成红因」) | 跨档并列**以机器可读形式**发生;而新判据若直接读该键会崩 |
| 2 | 决策 6 **能力阶梯**显式化 | 只在 ADR 内 —— `07 号`(总纲)与 `docs/guide/cep-usage.md` **均无**档位阶梯 | 只读总纲/手册的工程师看不到档位义务 ⇒ 档 0 的 0 仍会被读成一致性证明 |
| 3 | 决策 3b **档 0 检测力见证(B′)** | ⚠️ **metis 实测:做不到** —— `test/test_cep_stub_cache_emit.cc:126-128` 对写事件**只**调 `detector.write(...)`,`SwmrDataValueDetector::write()`(`test/cep_experiment_harness.hh:157-162`)无条件置 `written=true`、**不累加 `swmr`** ⇒ ⇒ **喂料通路结构上无法表达 SWMR 违例**;真实流 `req.data` 恒 0(`traffic_gen_tlm.hh:233`)⇒ data-value 恒不触发 | 本 change **降级**为:① data-value **喂料映射**见证(synthetic fixture)② **SWMR 结构不可达**的**登记**(⛔ 不产出 SWMR 见证) |

**⚠️ 存量已覆盖、⛔ 本 change 不重写**(metis 实测 5 个文件):检测器正控 + 红绿分离(`test/test_cep_experiment_harness.cc:87-130`)· SWMR 负控(`:99-106` · `test_cep_stub_cache_e2e.cc:695` · `test_cep_simulation_mode_lock.cc:226` · `test_cep_b4_evidence_device.cc:97/130/171`)· data-value 负控(`:108-115` · `test_cep_stub_cache_e2e.cc:708` · `test_cep_b4_evidence_device.cc:104/174`)⇒ ⇒ 本 change 的增量 = **喂料映射被走过**(存量负控**不经过**它)。

**⚠️ 上游矛盾(登记 · ⛔ 不改 ADR 正文)**:`ADR-CEP-20` 决策 6(iii) 表第 5 行把 `docs/guide/cep-usage.md §12.8` 标「**已有**」档位口径 ⇒ **实测该节 0 处档位语**(只有「无 sharer / 无版本」的口径句)⇒ **V-20-1 的「8 处已齐」前提不成立**;本 change 补的是**阶梯句**。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **data-value 喂料映射见证 + SWMR 结构不可达登记** | `test/test_cep_stub_cache_emit.cc`(**新 TEST_CASE** `[cep][exp][evidence-tier][synthetic-fixture]`) | ① 把 `:120-131` 的喂料循环抽成 `feed(detector, stamps)` helper(**与生产共用**);② 构造 `vector<TxnStamp>`(同地址两次写、`data` 不同;`read` 观测**旧** `data`)⇒ `counts().data_value > 0`;③ 对照组 = **同一 vector**,仅把观测值改为**最新** ⇒ `== 0`;④ **明写** SWMR 结构不可达;⑤ ⛔ **不设 `CEP_EXP_DIR`**;⑥ 打 `[synthetic-fixture]` 标注 |
| 2 | **档位登记机器面** | `test/cep_e2e_run_tiers.json`(**新增旁挂**)· `scripts/test/cep_e2e_reconcile_check.sh` | ⛔ **不改** `test/cep_e2e_runs.jsonl`;旁挂表 `{run_id → tier}`(3 条全登);闸加 3 判据 + `CEP_RECONCILE_JSONL` / `CEP_RECONCILE_TIERS` 覆盖(影子变异可执行);`evidence_scope` **入 `required`**(缺键独立签名) |
| 3 | **阶梯入总纲 + 手册** | `07 号` **新 SU**(冻结件)· `docs/guide/cep-usage.md` **新 §12.9** | 三档 + 声明义务 + 跨档聚合禁令(指针 ADR-CEP-20);§12.8 只加**一行导航**(⛔ 不加状态列) |

**Out of Scope(⛔ 硬边界)**

- ⛔ **不改 `ADR-CEP-20` 的任何决策**(本 change 只**落**它的义务)
- ⛔ **不产出 SWMR 检测力见证**(喂料通路结构不可达 —— metis 实测)⇒ ⛔ 不得把「SWMR 登记」写成「SWMR 已见证」
- ⛔ **不把档 0 的 0 升格为语义判据**(`ADR-CEP-20` 决策 2)
- ⛔ 不改 `test/cep_adr_structural_deferrals.json`(V-20-6 留给档 2 change)
- ⛔ 不改任何判据 / 阈值 / 预登记 / `metrics.schema.json` / retention · ⛔ 新增 invariant 数 = 0 · ⛔ 不改 B4 结论 · ⛔ 不动 `B3b-2`

## 验收标准(Acceptance Criteria)

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1(data-value 喂料映射见证 · 正负配对)**:同一 `vector<TxnStamp>`,**仅切** `read` 的观测值(旧 vs 最新)⇒ 两次 `counts().data_value` 差 = 违例数(**正控 == 0** / **负控 > 0**)⇒ ⛔ 不得用两段不同代码充当「注入」
- [ ] **AC-2(喂料映射被真正走过)**:新用例调用**与生产逐字相同**的 `feed(detector, stamps)`(`:120-131` 抽出后两处共用)⇒ ⛔ 不得只调 `write_without_invalidate` 绕过映射
- [ ] **AC-3(SWMR **结构不可达** 登记)**:逐字写明「`:126-128` 只调 `write()`;`write()` 不累加 `swmr` ⇒ 任何 `txn_stamps` 序列都只能产出 `swmr == 0` ⇒ 本 change ⛔ 不产出 SWMR 见证」⇒ **登记项**(文档层),⛔ 禁当鉴别力判据
- [ ] **AC-4(档位登记三判据)**:① 每条 jsonl 条目在 tiers 表**有且仅有 1 档** ② 档 0 条目 `evidence_scope` 含**真源词**之一 ③ tiers 表 `run_id` ⊆ jsonl(**反向零 orphan**)⇒ 三判据全绿;**且** `evidence_scope` 已入 `required`,缺键按**独立签名**红
- [ ] **AC-5(影子变异 · 3 条真变异 + 签名)**:`CEP_RECONCILE_*` 影子树上 —— (a) 删档 0 条目的档位词 ⇒ 「**档位声明缺失**」;(b) 删 tiers 表某 `run_id` ⇒ 「**档位未登记**」;(c) 给 tiers 表加 jsonl 不存在的 `run_id` ⇒ 「**僵尸档位登记**」⇒ 三条按签名红 + 还原绿
- [ ] **AC-6(阶梯入总纲 + 手册)**:`07 号` 新 SU(⛔ 不改正文;**5 条标题约束逐条满足** ⇒ 尤其 **⛔ 标题不得含「拓扑级 IC3 通过态」**(`cep_full_green_run_check` C5 陷阱))+ 索引重生成;`cep-usage.md` **新 §12.9** + §12.8 一行导航 ⇒ `cep_index_position_check` / `cep_docs_sync_check` / `cep_doc_ref_check` / `markdown_link_check` 全绿。⚠️ **载体状态**:四闸全绿只证明「可解析 / 锚点在 / 索引一致」,**⛔ 不证明阶梯内容正确** ⇒ 语义 = **人工评审**
- [ ] **AC-7(冻结面 · 两次 `--update`)**:Capabilities = **ADDED** ⇒ 归档合并 `openspec/specs/cep-evidence-tier-registry/spec.md` ⇒ 冻结 **83 → 84** ⇒ ① 归档前 `--update`(`07 号` 新 SU)② **归档合并后第二次** `--update`(新 main spec 自动入集)⇒ provenance 成立
- [ ] **AC-8(0 回归)**:`ctest` **全绿**(⛔ 不写死总数;以 `ctest -N` 实测为准,数量变化须**逐条说明**新增/删除的测试名)· 全部 `scripts/test/cep_*.sh` 逐个 rc=0 · format / docs_sync / doc_ref / agents_md_lint / freeze / reconcile / adr_structural 全 rc=0
- [ ] **AC-9(范围纪律)**:⛔ 未改任何判据 / 阈值 / 预登记(`preregistration.json` **零 diff**)/ `metrics.schema.json` / retention · ⛔ 未改 `ADR-CEP-20` 决策 · ⛔ **新增 invariant 数 = 0** · ⛔ 未改 `test/cep_adr_structural_deferrals.json` · ⛔ 未动 `B3b-2` · ⛔ 未改 B4 结论

## 成本收益

**消「跨档聚合可从机器面误读 + 档位阶梯不在总纲/手册 + 喂料映射未被见证」vs 1 个旁挂档位表 + 3 条闸判据 + 1 个新 TEST_CASE + 1 则 SU + 1 节手册**。

## 诚实边界(⛔ 不可约残余)

1. **档 0 的语义空洞不因本 change 改变** —— 本 change 只补「档位登记 + 喂料映射见证」,**⛔ 不产生语义判别力**(`ADR-CEP-20` 决策 1/2/3c);
2. **SWMR 侧无见证** —— 喂料通路**结构不可达**(`:126-128` 只调 `write()`)⇒ 档 0 的 SWMR 0 连「组件见证」都没有 ⇒ ⛔ 不得称「SWMR 检测力已见证」;
3. **档位词表**是真源单点(脚本内常量 + 注释指向 `ADR-CEP-20` 决策 6 表格)⇒ 文档侧**同词**但无机器一致性保证;
4. **`cep_dod_reading_check` S6 的扫描面盲区**(仅扫「首个 `## Status Update 2026-11-13` 块」)⇒ 新 SU(「附录 ·」开头)**不在禁表述扫描面内** ⇒ 登记不改(属独立闸改动);
5. **档 2**(协议引擎不变量观测面)仍**未排期** ⇒ 强度天花板不变。

## 关联

**ADR-CEP-20**(决策 6 · §5 抓取点 6)· 台账 **Entry #402** / **#401** / **#399** · improvement `.rddf/improvements/cep-evidence-tier-declaration.md` · `test/test_cep_stub_cache_emit.cc` · `test/cep_experiment_harness.hh` · `scripts/test/cep_e2e_reconcile_check.sh` · `scripts/test/cep_full_green_run_check.sh`(C5)· `docs/architecture/07-coherence-exploration-platform.md` · `docs/guide/cep-usage.md`
