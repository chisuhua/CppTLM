# design —— `cep-evidence-tier-declaration`

> **定位**:实施契约。决策依据 = `ADR-CEP-20`(v1.0,决策 6 + §5 抓取点 6);本文记**实施面细节**与**判据设计**。
> ⚠️ **本 change ⛔ 不改 `ADR-CEP-20` 的任何决策** —— 只**落**它的义务。若实施中发现 ADR 需要修正 ⇒ **停并回报**(走 ADR 追加 SU 流程,⛔ 不在本 change 内改)。

## D-1 · 现状(实测 · 本 change 的事实基座)

| 事实 | 落点 |
|---|---|
| 喂料循环(生产) | `test/test_cep_stub_cache_emit.cc:120-131` —— `for (const auto& s : w->txn_stamps())` + `memory_ops_addrs.insert(s.addr)` + `detector.write/read(s.addr, s.data)` |
| **喂料通路对 SWMR 无通道** | `:126-128` 对写事件**只**调 `detector.write(...)`;`SwmrDataValueDetector::write()`(`test/cep_experiment_harness.hh:157-162`)无条件置 `written=true`、**不累加 `swmr`** ⇒ **任何** `txn_stamps` 序列都只能产出 `swmr == 0` |
| data-value 在真实流上万恒 0 | `include/tlm/traffic_gen_tlm.hh:233` `req.data.write(0)` + `cohif_responder.hh:164` 只回显 ⇒ 真实流 `observed == last-written` 恒成立 |
| 检测器**已有** 5 处负控 | `test/test_cep_experiment_harness.cc:87-130`(正控 + SWMR 负控 + data-value 负控 + 红绿分离)· `test_cep_stub_cache_e2e.cc:695/708` · `test_cep_simulation_mode_lock.cc:226` · `test_cep_b4_evidence_device.cc:97/104/130/171/174` |
| 对账闸的必填键**不含** `evidence_scope` | `scripts/test/cep_e2e_reconcile_check.sh:62-64` 的 `required` 9 键 |
| 对账闸**无影子覆盖开关** | 同脚本 `:25` 硬编码 `LEDGER="test/cep_e2e_runs.jsonl"`(对照 `cep_full_green_run_check.sh:28-34` 的 `CEP_FGR_*`) |
| jsonl 3 条的 `evidence_scope` | 第 1、2 条「可对账(…);⛔ 非 B4 证据;⛔ 非一致性安全断言(IC3=⚪ 不适用)」· 第 3 条「拓扑级 E2E run · …(含「拓扑级」= 档位词)」 |
| `07 号` 新 SU 的两处标题陷阱 | `cep_dod_reading_check` **S7**(`## Status Update 2026-11-13` 前缀须恰 1 命中)· `cep_full_green_run_check` **C5**(`target[-1]` 取含「拓扑级 IC3 通过态」的标题 ⇒ 会去新 SU 找 run 目录名) |

## D-2 · 实施面(3 处)

| # | 落点 | 形态 |
|:--:|---|---|
| **A** | `test/test_cep_stub_cache_emit.cc` | 把 `:120-131` 的循环抽成**文件内** `feed(cep_exp::SwmrDataValueDetector&, const std::vector<X::TxnStamp>&)` helper(**生产与见证共用**);新增 `TEST_CASE("[cep][exp][evidence-tier][synthetic-fixture] …")` |
| **B** | `test/cep_e2e_run_tiers.json`(新)· `scripts/test/cep_e2e_reconcile_check.sh` | 旁挂表 + 3 判据 + `CEP_RECONCILE_JSONL` / `CEP_RECONCILE_TIERS` + `evidence_scope` 入 `required` |
| **C** | `07 号` **新 SU** · `docs/guide/cep-usage.md` **新 §12.9** | 三档阶梯 + 声明义务 + 跨档聚合禁令;§12.8 一行导航 |

## D-3 · 判据设计(逐条 + 变异签名)

| # | 判据 | 变异 ⇒ **签名** |
|:--:|---|---|
| **T1** | 每条 `test/cep_e2e_runs.jsonl` 条目在 `test/cep_e2e_run_tiers.json` **有且仅有 1 档** | 删某 `run_id` 的档位 ⇒ `档位未登记` |
| **T2** | **档 0** 条目的 `evidence_scope` 含真源词(`档 0` / `拓扑级`) | 删档 0 条目的档位词 ⇒ `档位声明缺失` |
| **T3** | tiers 表的 `run_id` ⊆ jsonl(**反向零 orphan**) | tiers 表加 jsonl 不存在的 `run_id` ⇒ `僵尸档位登记` |
| **T4** | `evidence_scope` **必填**(入 `required`) | 删该键 ⇒ `必填键缺失:evidence_scope`(独立签名,⛔ 不得与其它键共用) |
| **W1** | data-value **喂料映射**见证:**同一 vector** 仅切 `read` 观测值 ⇒ 正控 0 / 负控 > 0 | 把对照组改成也注入 ⇒ 正控 ≠ 0 ⇒ 测试红 |
| **W2** | 喂料映射**被走过**(跑 `feed()` 而非直调检测器) | 把新用例改成只调 `write_without_invalidate` ⇒ 映射未被覆盖 ⇒ **人工评审**(⛔ 无机器判据) |
| **R1** | SWMR **结构不可达**登记 | 文档层(⛔ 禁当鉴别力判据) |

## D-4 · 冻结面(**2 次 `--update`**)

- **第 1 次**(归档前):`07 号` 新 SU ⇒ 改已有冻结产物 ⇒ 产物数**不变(83)**· 走五步链;
- **第 2 次**(归档合并后):`openspec/specs/cep-evidence-tier-registry/spec.md` 命中 `FROZEN_GLOBS` 的 `openspec/specs/cep-*/spec.md` ⇒ **自动入集** ⇒ 产物 **83 → 84**;
- ⚠️ 两次均用**同一**授权 Entry 号;`test/` / `scripts/` / `docs/guide/` **不在冻结集** ⇒ 其改动不触发清单变更。

## D-5 · 硬边界(逐字进 tasks)

1. ⛔ **不产出 SWMR 检测力见证**(喂料通路结构不可达)—— ⛔ 不得把「SWMR 登记」写成「SWMR 已见证」;
2. ⛔ **不把档 0 的 0 升格为语义判据**(`ADR-CEP-20` 决策 2);
3. ⛔ **不改 `ADR-CEP-20` 决策** / 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention;
4. ⛔ **不改 `test/cep_adr_structural_deferrals.json`**(V-20-6 留给档 2 change);
5. ⛔ **不改 `test/cep_e2e_runs.jsonl`**(append-only 台账;档位走旁挂表);
6. ⛔ 新 SU 标题**不得含「拓扑级 IC3 通过态」**(C5 陷阱),且⛔ 不以 `## Status Update 2026-11-13` 开头(S7)。

## D-6 · 不可约残余(随交付落盘)

1. **档 0 的语义空洞不变** —— 本 change 只补「档位登记 + 喂料映射见证」;
2. **SWMR 侧无见证**(结构不可达);
3. 档位词表是**真源单点**(脚本常量 + 注释指向 `ADR-CEP-20` 决策 6 表格)⇒ 文档侧同词但无机器一致性保证;
4. `cep_dod_reading_check` **S6 扫描面盲区**(只扫首个 `## Status Update 2026-11-13` 块)⇒ 新 SU 不在禁表述扫描内 ⇒ 登记不改;
5. **档 2 未排期** ⇒ 强度天花板不变。

## 铁律

1. **先读实测,再改代码**(D-1 的每条都有 `file:line`)。
2. **不重写存量的同义反复**(5 处负控已存在 ⇒ 本 change 的增量是**喂料映射**)。
3. **档位走旁挂表** ⇒ ⛔ 不碰 append-only 台账。
4. **变异必须可在影子树执行** ⇒ 加覆盖开关,⛔ 不靠手工改真实仓。
5. **口径随引用重复** ⇒ 「档 0 = 样本通路活性 / 非语义」「SWMR 结构不可达」两句必须跟着新 SU / §12.9 / 旁挂表注释一起走。
