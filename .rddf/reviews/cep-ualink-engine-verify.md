# 验收报告 · `cpptlm-cep-ualink-coherence-engine`

- **验收对象**:change `cpptlm-cep-ualink-coherence-engine`(第 2 项:UALink **记账级专用承载**引擎)
- **角色**:verifier(2026-11-07)
- **判据来源**:change `proposal.md` **AC-1 ~ AC-8** + delta spec(`openspec/specs/cep-ualink-coherence-engine/spec.md` 的 UGE-A ~ UGE-E)
- **结论**:**PASS**(0 失败项 · 1 条范围外遗留登记 · 1 轮)
- **分流说明**:`.rddf/state/iteration.json` **不存在** ⇒ **未触发** `rdd-verifier` 的 P3 archive gate;`proposal.md` 的 AC 段标题为 `## Acceptance`(⛔ 非 `## Acceptance Criteria`)⇒ 按分流三条件走**本地 1–5 步验收**

## 一、AC 逐条判定

| AC | 判定 | 证据(工具调用) |
|---|---|---|
| **AC-1** 三分法观测面 + **4 条机器可判不变量** | ✅ | `test/test_cep_ualink_engine.cc` **UGE-02** ⇒ `unsupported_rejected[Request]==0 ∧ [Completion]==0`;7 条拒绝流各 `flow_events[f]==unsupported_rejected[f]==1`;`snoop_resp` 恰好 1 次 pending 且拒绝计数 0;`flow_events` 全 10 流各 1 ⇒ `[cep][ualink][engine]` 6 cases / 112 assertions 全 PASS |
| **AC-2** completion 记账**不依赖有序性**(有牙) | ✅ | **UGE-03** 三段:① **中途探针**(request t1,t2,t3 → 仅 completion **t2**)⇒ 在途集合 **== `{t1,t3}`}** 逐格相等(`count(2)==0`);② **端态**(反序 t3,t1,t2)⇒ 集合空 + `completion_unmatched==0` + **全部**计数器与正序**逐字段相等**;③ **决定性变异**见下表 ⇒ **判据非恒真** |
| **AC-3** `snoop_resp` **pending 终态** | ✅ | **UGE-04**:恰好一次登记 + 零「拒绝」计数 + 在途集合**逐格不变**(`count(42)==1`)+ 对 request/completion 零影响 + 零发射 |
| **AC-4** 装配闭环 + 负控按**正确原因**红 | ✅ | **UGE-05**:闸 **PASS**(`ualink_4` **required**)+ 负控 ① plane 绑定置空 ⇒ `[IAG-04]` 归因红;② plane 名未登记 ⇒ `[IAG-05]` 归因红(断言命中**归因 tag** 而非仅判红) |
| **AC-5** **零发射** | ✅ | **UGE-02 / UGE-04**:`egress_emitted == 0` + 全部 2 端口 `resp_out()[p].valid() == false` |
| **AC-6** **B3b-2 仍 0/4** | ✅ | **UGE-06**:4 族 deferral 登记完整且 `ualink` disposition 仍 `deferred`;`ualink_4` 的 `flow_support` 8 false + 2 true + `perm==["NONE"]` 逐字断言;`ualink-pending` 档位仍 `public_nonnormative`(⛔ 不升格为 `spec_clause`)⇒ **零计数变化** |
| **AC-7** 0 回归 + 冻结面授权 | ✅ | `ctest --test-dir build -j4` ⇒ **159/159**;`[cep]` **473 cases / 11089 assertions**;4 闸全绿;`cep_freeze_check.sh` 无参复验 ⇒ provenance 通过(41 产物 hash 全部一致,冻结于 commit `09bf5e14`) |
| **AC-8** `07 号` §5.1.1 UALink 行**精确化** | ✅ | `docs/architecture/07-coherence-exploration-platform.md` §5.1.1 UALink 行现含「**流语义已闭环(记账级弱档建模声明)**」+「⛔ **不得**读成『UALink 协议语义已实现』」+「**B3b 的 UALink 行维持 ⏭ 受限(仍 0/4)**」⇒ 与 `ADR-CEP-12` SU 2026-11-07 ③ 的字面禁令一致 |

## 二、**决定性变异**证据(判据有牙,非恒真)

| # | 变异 | 期望 | 实测 |
|---|---|---|---|
| **M-1** | 恒等匹配 → **插入序回退且不比对 `txn_id`**(FIFO-pop 无比对) | 必红 | ✅ **红** —— **中途探针**(line 158)+ **端态**(line 192)**两处同时命中**(`2 failed / 66 passed`) |
| **M-2** | 三分法 → **「10 流一律记账」**(删 7 条拒绝计数) | 必红 | ✅ **红** —— 不变量 2 命中(line 148) |
| **M-3** | 去掉 fail-closed ⇒ `completion_unmatched` 不计数 | 必红 | ✅ **红** —— line 158 命中 |

⇒ 三次变异后**均已还原**(`git diff --stat include/tlm/cep/ualink_protocol_engine.hh` 为空 ⇒ 与 HEAD 一致)。

## 三、遗留清扫复核(本次范围)

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK / 待补 / 待实现 | **0 命中** |
| 调试残留(`printf` / `std::cerr` / `std::cout` / `fopen` / `diag`) | **0 命中** |
| 被 skip 的测试(`SKIP(` / `#if 0`) | **0 命中** |
| 注释掉的代码行 | **0 命中** |
| 临时 fallback / 临时代码 | **0 命中** |

⚠️ **范围外遗留登记(不趁机清理)**:`clang-format --check` 报 **14 个文件**需格式化 ⇒ 本轮**只**格式化本次新增的 `test/test_cep_ualink_engine.cc`;`include/tlm/cep/ualink_protocol_engine.hh` 与 `include/chstream_register.hh` **本就合规**;其余 **13 个为历史遗留**(含 `test_cep_refkind_coverage.cc` = 上一轮 Entry #266 新增)⇒ 登记为**独立清理项**,⛔ 不在本 change 范围(per 台账 Entry #271 §6)。

## 四、⚠️ 验收确认的**诚实边界**

1. 本 change 交付的是**记账级专用承载**,⛔ **不是** UALink 协议实现 ⇒ **依赖 UALink 协议语义的实验仍不可做**(per `ADR-CEP-12` SU 2026-11-07 ⑦ 代价 ①);
2. **AC-2 的鉴别力有上限**:fixture 抓 **order-coupling 类** mutant(恒等/FIFO/LIFO 三者互异 ⇒ 逐格相等断言可分);⛔ **抓不到**「按序扫描匹配」(语义仍恒等,仅低效)与「以地址作键」(⛔ `on_completion` 签名**不含地址** ⇒ **结构性不可达**)⇒ 契约面已**诚实登记**该边界,⛔ **不得**声称覆盖键型变异;
3. **B3b 计数零变化** ⇒ 终态 DoD Go **维持挂起**(per `07 号` SU · 六 / Entry #264)⇒ 本 change **不**构成对该挂起的解除。

## 五、失败项与路由

**无失败项** ⇒ ⛔ 无需回 builder / planner。

## 六、已用轮次

第 **1** 轮(2026-11-07)⇒ **PASS**。
