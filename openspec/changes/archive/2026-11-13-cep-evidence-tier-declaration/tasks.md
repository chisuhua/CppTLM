# tasks —— `cep-evidence-tier-declaration`

> 顺序 = **A0(授权门) → A1(实测复核) → A2(喂料映射见证) → A3(档位登记机器面) → A4(影子变异) → A5(阶梯入总纲 + 手册) → A6(五步链 + 两次 `--update`) → A7(0 回归) → A8(归档 + KPI)**。
> 门:**G1 = A0** · **G2 = A2**(喂料映射见证正负配对)· **G3 = A4**(3 条变异按签名红)· **G4 = A6**(冻结 provenance)· **G5 = A7**(0 回归)· **G6 = A8**(归档 + KPI)。
> ℹ️ 编号区分:本文件的 **G1~G6 = 阶段门**;design §D-3 的 **T1~T4 / W1~W2 / R1 = 判据**。

## A0 · 授权门(G1)
- [x] A0.1 台账 Entry 授权实施(占 1 slot,KPI **0/3 → 1/3**) ⇒ ✅ Entry #403(C8 立项,占 1 slot)
- [x] A0.2 improvement(v2)+ `ADR-CEP-20` 已落盘 ⇒ ✅ improvement v2 + ADR-CEP-20 v1.0
- [x] A0.3 四件套已建 + `openspec validate --strict` rc=0 ⇒ ✅ `openspec validate --strict` rc=0
- [x] A0.4 基线实测:jsonl 3 条的 `evidence_scope` / `hypothesis_id` / `ic3_verdict`;`required` 9 键;喂料循环行号(复核 design §D-1) ⇒ ✅ jsonl 3 条 / required 9 键 / 喂料循环行号 均已复核(design §D-1)

## A1 · 实测复核(⛔ 只读 · 不改代码)
- [x] A1.1 复核「喂料通路对 SWMR 无通道」(`:126-128` 只调 `write()`;`write()` 不累加 `swmr`) ⇒ ✅ `:126-128` 只调 `write()`;`write()` 不累加 `swmr`
- [x] A1.2 复核 5 处存量负控的覆盖面(⇒ 确认本 change 的增量是**喂料映射**而非负控本身) ⇒ ✅ 5 处存量负控已核(harness:87-130 / e2e:695,708 / sim_mode_lock:226 / b4:97,104,130,171,174)
- [x] A1.3 复核 `07 号` 新 SU 的两处标题陷阱(S7 / C5) ⇒ ✅ S7 + C5 两处陷阱已核并写入 design §D-1
- [x] A1.4 ⛔ 若任一复核推翻 design §D-1 的事实 ⇒ **停**并回报(⛔ 不许硬做) ⇒ ✅ 无需停(复核未推翻 design §D-1)

## A2 · 喂料映射见证(G2)
- [x] A2.1 把 `test/test_cep_stub_cache_emit.cc:120-131` 的喂料循环抽成文件内 `feed(...)` helper(生产与见证**共用**) ⇒ ✅ `feed()` 单实现源(生产与见证共用)
- [x] A2.2 新增 `TEST_CASE("[cep][exp][evidence-tier][synthetic-fixture] …")`:构造 `vector<TxnStamp>`(同地址两次写 + `read` 观测旧 `data`)⇒ 断言 `counts().data_value > 0` ⇒ ✅ 新 TEST_CASE 负控 `data_value==1`
- [x] A2.3 **正负配对**:对照组 = **同一 vector** 仅把观测值改为最新 ⇒ `counts().data_value == 0`(⛔ 不得用两段不同代码充当注入) ⇒ ✅ 正控 = 同一 vector 仅切观测值 ⇒ `==0`
- [x] A2.4 ⛔ **不设 `CEP_EXP_DIR`**(新 case 无产物副作用);打 `[synthetic-fixture]` 标注 ⇒ ✅ 新 case 不设 `CEP_EXP_DIR`;打 `[synthetic-fixture]`
- [x] A2.5 逐字登记「SWMR **结构不可达**」⇒ 文档层,⛔ 禁当鉴别力判据 ⇒ ✅ SWMR 结构不可达已登记(代码注释 + 新 SU ③ + §12.9 + design §D-6)
- [x] A2.6 ⛔ 不改任何现有断言(`:132-133` 的 `counts().swmr == 0u` 等保持) ⇒ ✅ 现有断言未改(E2E-01 22 断言仍绿,产物 20 地址 / mv=0 不变)

## A3 · 档位登记机器面
- [x] A3.1 新增 `test/cep_e2e_run_tiers.json`:3 条 `run_id` → 档位(全档 0) ⇒ ✅ `test/cep_e2e_run_tiers.json`(v2:run_tiers / declaration_required / grandfathered)
- [x] A3.2 `cep_e2e_reconcile_check.sh` 加 T1 / T2 / T3 三判据 ⇒ ✅ T1/T2/T2b/T3 已落
- [x] A3.3 加 `CEP_RECONCILE_JSONL` / `CEP_RECONCILE_TIERS` 覆盖(影子变异可执行) ⇒ ✅ `CEP_RECONCILE_JSONL` / `CEP_RECONCILE_TIERS`
- [x] A3.4 `evidence_scope` 入 `required` + 缺键**独立签名** ⇒ ✅ `evidence_scope` 入 required(签名 `必填键缺失:`)
- [x] A3.5 档位词**真源单点**(脚本内常量 + 注释指向 `ADR-CEP-20` 决策 6 表格);文档侧同词 ⇒ ✅ `TIER_WORDS` 真源单点(注释指向 ADR-CEP-20 决策 6)

## A4 · 影子变异(G3)
- [x] A4.1 (a) 删档 0 条目的档位词 ⇒ `档位声明缺失` ⇒ ✅ 变异 a ⇒ `档位声明缺失`
- [x] A4.2 (b) 删 tiers 表某 `run_id` ⇒ `档位未登记` ⇒ ✅ 变异 b ⇒ `档位未登记`
- [x] A4.3 (c) tiers 表加 jsonl 不存在的 `run_id` ⇒ `僵尸档位登记` ⇒ ✅ 变异 c ⇒ `僵尸档位登记`
- [x] A4.4 (d) 删 `evidence_scope` 键 ⇒ `必填键缺失:evidence_scope` ⇒ ✅ 变异 d ⇒ `必填键缺失:evidence_scope`
- [x] A4.5 四条**按签名红** + 还原绿;真实仓字节指纹前后相等 ⇒ ✅ 4/4 按签名红 + 指纹相等

## A5 · 阶梯入总纲 + 手册(G4 前置)
- [x] A5.1 `07 号` **新 SU**(插在 `## §末 文档索引` 之前;⛔ 不改正文)⇒ **5 条标题约束**逐条满足(形态 `## 附录 · Status Update 2026-11-13(证据档位阶梯 · per 台账 Entry #NNN)—— …`;**⛔ 不含「拓扑级 IC3 通过态」**;⛔ 不与既有表题逐字相同;索引重生成) ⇒ ✅ 新 SU「附录 · Status Update 2026-11-13(证据档位阶梯)」5 条标题约束全满足
- [x] A5.2 三档阶梯 + 档位声明义务 + 跨档聚合禁令(指针 `ADR-CEP-20` 决策 6) ⇒ ✅ 三档 + 两条义务 + 指针
- [x] A5.3 `cep-usage.md` **新 §12.9**;§12.8 **一行导航**(⛔ 不加状态列) ⇒ ✅ §12.9 + §12.8 一行导航
- [x] A5.4 `cep_index_position_check` / `cep_docs_sync_check` / `cep_doc_ref_check` / `markdown_link_check` 全绿 ⇒ ✅ 四闸全绿

## A6 · 五步链 + 两次 `--update`
- [x] A6.1 五步链:Entry 先落 → 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验(**第 1 次**:`07 号` 新 SU,产物数不变 = 83) ⇒ ✅ 第 1 次(Entry #404 → 产物 → --update → 清单 → 复验)· 产物数 83 不变
- [x] A6.2 归档合并 `openspec/specs/cep-evidence-tier-registry/spec.md` 后**第 2 次** `--update`(产物 **83 → 84**)+ 清单 commit + 无参复验 ⇒ ✅ 第 2 次(归档合并后 --update ⇒ **84**)+ 清单 commit + 复验

## A7 · 0 回归(G5)
- [x] A7.1 `ctest` **全绿**(⛔ 不写死总数;以 `ctest -N` 实测为准,数量变化须逐条说明新增/删除的测试名) ⇒ ✅ ctest **177/177**(新增 `cep_e2e_reconcile_mutations`)
- [x] A7.2 全部 `scripts/test/cep_*.sh` 逐个 rc=0 ⇒ ✅ 全 `cep_*.sh` 逐个 rc=0
- [x] A7.3 format / docs_sync / doc_ref / agents_md_lint / freeze / reconcile / adr_structural 全 rc=0 ⇒ ✅ 全绿
- [x] A7.4 `openspec validate <name> --strict` rc=0 ⇒ ✅ 归档前 rc=0

## A8 · 归档 + KPI(G6)
- [x] A8.1 `openspec archive`(✅ 已完成归档语义)⇒ delta **ADDED** 合并进 `openspec/specs/cep-evidence-tier-registry/` ⇒ ✅ `openspec archive` rc=0(specsUpdated · added 1)
- [x] A8.2 归档后**第 2 次** `--update` + 清单 commit(A6.2) ⇒ ✅ 见 A6.2
- [x] A8.3 KPI 活口径 **1/3 → 0/3**(7 处)+ 体量数字(main spec / 归档数 / 闸数)由闸实测回填 ⇒ ✅ KPI **0/3**(7 处)+ 体量数字(归档 142 / spec 42 / 闸 42)
- [x] A8.4 台账归档回执 ⇒ ✅ Entry #405

## ⛔ 明确不做
- ⛔ 产出 SWMR 检测力见证(结构不可达)· ⛔ 把档 0 的 0 升格为语义判据 · ⛔ 改 `ADR-CEP-20` 任何决策 · ⛔ 改 `test/cep_adr_structural_deferrals.json` · ⛔ 改 `test/cep_e2e_runs.jsonl` · ⛔ 改判据/阈值/预登记/schema/retention · ⛔ 新增 invariant · ⛔ 改 B4 结论 · ⛔ 动 `B3b-2` · ⛔ 实施档 2
