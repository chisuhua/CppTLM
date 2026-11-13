# ADR-CEP-20: 拓扑级 IC3 证据的强度边界与能力阶梯

> **状态**: 🟡 **Proposed**✅ **已签发**(v1.0,2026-11-13 —— per 台账 **Entry #401** 授权;oracle 2 轮 + critic 2 轮双审闭合,详见文末 **Status Update**)`
> ~~🟡 Proposed(v0.4~~ ⇒ 状态转换记录见文末 Status Update
> **日期**: 2026-11-13
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP 一致性证据的档位与强度边界** —— 拓扑级 E2E 装置 / 模型级载体 / 协议级引擎 三者的证据强度阶梯,以及「违例类要有判别力」这一诉求的**可实现性裁决与路由**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.2 §4(66 条守门 invariant 总表)· §7.2.2(「全绿」两级判定 —— 实体见 `docs/architecture/cep/simulation-and-experiment.md` §7.2.2)
> - 真源 3:`ADR-CEP-06`(决策 1 = 4 件套 · SU#4 ② 六条非空洞性约束 · SU 2026-11-08·二 retention 丙 · SU 2026-11-09 ② 五句证据边界 · SU 2026-11-13 ③ 四条强度限定)· `ADR-CEP-19`(决策 9 = 端口面 wrapper 形态;**「home 简化模型」定位出自其 SU 2026-11-08 · 二 ③** · ASM-8 ① = 拓扑级 run 的 `hypothesis_id` 须绑定 H3 · SU 2026-11-13 口径段)· `ADR-CEP-00`(治理宪章:跨仓归口 + 准入门槛)
>
> **一手锚点**(全部实测,逐 `file:line` 核实):
> - `include/tlm/cep/stub_cache_core.hh:339` / `:359` —— `struct CachedLine` + `std::map<uint64_t, CachedLine> lines_` ⇒ **该类 sharer 命中 0 / version 命中 0**(实测 grep)
> - `include/tlm/cep/stub_cache_endpoint.hh` / `include/tlm/cep/cohif_responder.hh` —— 同上(sharer 0 / version 0,实测 grep)
> - `include/bundles/cache_bundles_tlm.hh:39-46` —— `CacheReqBundle` 的 9 个字段,**无 version**;`CacheRespBundle` 同
> - `include/tlm/cep/cohif_responder.hh:164` —— `b.data.write(p.data)` ⇒ home 回程**只回显数据**,不带版本
> - `include/tlm/traffic_gen_tlm.hh:233` —— `req.data.write(0)` ⇒ 业务侧**写数据恒 0**
> - `test/cep_experiment_harness.hh:136` / `:165` / `:176-177` / `:185` —— `ViolationCounts` / `write_without_invalidate()`(可构造 SWMR 违例形态)/ `read()` 在 `checks_enabled_=false` 时**早退** / `counts()`
> - `test/test_cep_stub_cache_emit.cc:120-133` —— 样本集合 `memory_ops_addrs.insert(s.addr)` 与**喂检测器相互独立** ⇒ C1' 证明的是「装置产生了非拒绝事务样本」,**不是**「检测力已见证」
> - `docs/adr/ADR-CEP-06-experiment-protocol.md:690` —— SU#4 ② 六条非空洞性约束
> - `scripts/test/cep_full_green_run_check.sh:81` —— **C1' 样本活性见证**(判样本通路,⛔ 非语义)
> - `scripts/test/cep_ic3_report_states_check.sh:10-12` —— `ic3_verdict` 三态词表(`✓ 通过` / `❌ 违例 fail` / `⚪ 不适用`),**不分档**
> - `test/cep_e2e_runs.jsonl:1-3` —— 3 条条目中**唯一**的 `"ic3_verdict": "✓ 通过"` 是**档 0** 的拓扑 run(`hypothesis_id = "h3"`);承载 B4 的**模型级** h3 v2 run **不在**该注册表内
> - `docs/architecture/07-coherence-exploration-platform.md:639` —— 拓扑级 IC3 通过态 SU(本 ADR 的登记对象之一)
>
> **关联 change**: `backlog: cep-evidence-tier-declaration`(落实决策 3b 的负控见证 + 决策 6(ii) 的跨档聚合禁令机器面 + 档位阶梯登记;立项后回填指针)
> **AC 映射**: ⏸ **待 change 立项后回填**(per「ADR 完成的标志」第 4 条:未立项时允许暂缺)
> ⚠️ **签发前置**:① 台账 **Entry #401** 授权(含冻结面 `--update --authorized-by "Entry #401"` 号)· ② **oracle** 审稿(MUST-FIX F1~F3 已闭,见 §8 修订记录)· ③ **critic** 审稿(与既有 ADR / 契约是否冲突)· ④ BLOCKER 全闭 · ⑤ `test/cep_adr_structural_deferrals.json` 的 `decision_adrs` 归类 + `invariant_attribution_dispositions` 的文档层登记 + `scripts/test/cep_docs_sync_check.sh` 的 `NO_INV` 白名单 · ⑥ 冻结面两步授权链

---

## 1. 背景与问题

### 1.1 问题

change `cpptlm-cep-full-green-run-evidence`(已归档,台账 Entry #398)达成:**拓扑级 E2E run 上 IC3 = `✓ 通过`**(两违例类**均实测**为 0)。但该 0 的**语义强度**存疑,实测依据三条:

| # | 事实 | 后果 |
|---|---|---|
| 1 | 拓扑级装置三件套(`stub_cache_core` / `stub_cache_endpoint` / `cohif_responder`)**sharer 命中 0 / version 命中 0**;bundle 无 version 字段;home 回程**只回显** `p.data` | **SWMR** 违例形态 =「未先失效**既有 sharer** 的写」⇒ 装置**无 sharer** ⇒ 该形态**不产生** ⇒ 恒 0 是**形态缺席**的 0 |
| 2 | 同上 + 业务侧写数据**恒 0** | **data-value** 违例形态 =「读**观察到的版本** ≠ 最后写入的版本」⇒ 装置**无版本** ⇒ **观察值在概念上不存在** ⇒ 该判据**无法被实例化**(不是「恒 0」) |
| 3 | 检测器本身**具备**检出能力(`write_without_invalidate` 路径存在,`test/cep_experiment_harness.hh:165`) | ⇒ 0 的「空洞」**不在检测器**,在**装置语义面缺这两个概念** |

⇒ 由此产生**诉求 L-1**(台账 Entry #399 登记):「拓扑级 IC3 的违例类要有**判别力**」。

### 1.2 本 ADR 的定位

- ⛔ **不改**任何判据口径 / 阈值 / 预登记 / schema / retention;⛔ **新增 invariant 数 = 0**;
- ⛔ **不裁决**「拓扑级 run 该不该有 IC3 通过态」(已由 Entry #396~#398 落定);
- ✅ 只裁决两件事:① 该诉求在**本平台是否可实现**;② 一致性证据的**档位阶梯**与**强度上限的路由**。

---

## 2. 决策

### 决策 1 —— 拓扑级 E2E 装置的定位 = **框架级观测面**(不承载一致性语义)

拓扑级链路(`TrafficGenTLM` → `StubCacheEndpointTLM` → `CohIfResponderTLM`)的职责是**框架级连通性 / 时延 / 样本可观测性**;⛔ **不承担**一致性语义。⇒ 其 IC3 违例类**永不**作语义判据。

### 决策 2 —— 违例类在拓扑级的**唯一合法语义** = 「**样本通路活性**」判据

拓扑级 run 的 `memory_violations` 只能回答「**本 run 的装置产生了被观测的非拒绝内存操作样本**」(载体 = `observation_keys.memory_ops_checked_addrs` 长度 > 0;机器判据 = `scripts/test/cep_full_green_run_check.sh` 的 **C1'**)⇒ ⛔ **不得**被读成「未观测到内存违例」。

⚠️ **口径边界(实测,不可含糊)**:C1' 的样本集合与检测器消费**相互独立** —— `test/test_cep_stub_cache_emit.cc:126` 先把 `s.addr` 收进集合,再逐条喂检测器;检测器 `checks_enabled_=false` 时 `read()` **早退**(`test/cep_experiment_harness.hh:176-177`)而集合照记 ⇒ ⇒ **C1' 证明的是「装置产生了非拒绝事务样本」,⛔ 不是「检测力已见证」** ⇒ 「检测力已见证」需**离线负控**(见决策 3b)。

### 决策 3 —— ⛔ **拒绝**「装置语义级局部升级」;✅ **采纳**「检测力见证级增量(B′)」进 backlog

**3a ⛔ 拒绝「装置语义级局部升级」** —— 让 data-value 有判别力需要**版本**,而版本在装置语义面**不存在**(§1.1)⇒ 两条替代路径都**不产出装置级语义**:① 改**公共面**(业务写数据 + bundle 版本字段)⇒ 波及 `TrafficGenTLM` 的 **11 个 SoC 配置**(实测)+ bundle 语义,却仍不产生**装置级**版本(版本是**加进去的**,不是**长出来的**)⇒ 该 0 仍是「自造语义下的 0」;② 在**测试侧**代理出版本 ⇒ 检测的是**测试侧自造语义**⇒ ⇒ **概念错误**,⛔ 非成本问题。
⚠️ **与 `ADR-CEP-06` SU 2026-11-13 ③ 末条的衔接**:该末条把「写侧非恒定数据 + 版本字段 + 注入式负控」登记为拓扑装置获得判别力的路径(**当时 = 能力新增,change 范围外**)⇒ 本 ADR **裁决该路径在拓扑装置上不充分**(版本与 sharer 在装置语义面不存在 ⇒ 三要素中前两项**必然**退化为测试侧自造语义)⇒ 判别力改由**协议引擎面**(档 2)承载 ⇒ ⛔ 不得读成「SU ③ 末条所列路径仍然开放」。

**3b ✅ 采纳 B′(检测力见证级增量 · 产出侧 · test-side only)进 backlog** —— 在 `test/test_cep_stub_cache_emit.cc` 增一个**负控变体**:注入**已知** SWMR / data-value 违例形态 ⇒ 断言 `detector.counts() > 0` ⇒ ⇒ 证明「**本装置的样本流经检测器时,检测器对已知违例形态会变红**」⇒ 与档 1 的 SU#4 ② 约束 2(检测力由负控证明)**同型**,但**只覆盖检测器面**,⛔ 不覆盖装置语义面。
⚠️ **形态更正(相对 oracle 首轮建议)**:负控见证**属产出侧**,⛔ **不是**新闸判据 —— 闸只读**产物**(`metrics.json` / `simulation_config.json` / `report.html`),**无法见证「检测力已见证」**(那是**过程属性**,产物只记结果);把它做成闸的第 7 条变异只会重复 C2(产物计数非零)⇒ ⇒ 落点 = `backlog: cep-evidence-tier-declaration`(与该 change 的 reconcile 闸档位词检查同批)。

**3c ⚠️ 登记为接受的弱点** —— 在 B′ 落地前,档 0 **无检测力见证** ⇒ 与档 1 相比,档 0 的「0」**不含**检测力语义 ⇒ 引用时须带该限定。

### 决策 4 —— ⛔ **拒绝**「让拓扑级承载真实一致性语义」

**主理由(两条各自独立成立 · ⚠️ v0.4 重建)**:
1. **`ADR-CEP-19` 决策 9 已把该装置签批为「端口面 wrapper」形态,其 SU 2026-11-08 · 二 ③ 进一步把 home 定为**简化模型** ⇒ 让它承载真实一致性语义 = **改决策 9**(须走 ADR 取代流程)⇒ ⛔ 不得以「加一个装置特性」的名义实施;
2. **协议语义的「设计权威」归 ArchForge**(`ADR-CEP-00` 决策 1 的**定性分层** + `:120` 的「设计权威 → 实施」**单向门**)⇒ 给拓扑装置**新造**一套一致性语义(sharer 可见性 + 版本)属**新协议语义设计** ⇒ 须先取 ArchForge 的设计权威。

⚠️ **明确纠正(v0.3 及以前的错误论据)**:⛔ **不是**因为「协议 IP 归 ArchForge 仓」—— `ADR-CEP-00` 决策 1 明写 **代码归口 = CppTLM**(全部 C++ 实现),决策 4 的边界对照表亦逐条判定「ProtocolEngine 插件机制 | **CppTLM**」「5 个具体协议语义建模 | ArchForge(**可选**)· **不是** CEP ADR 签发依赖」⇒ 结论「CEP 核心框架归属 CppTLM,**不触发**边界条款」。本仓**可以**也**已经**实施协议级语义(`include/tlm/cep/*_protocol_engine.hh` 6 个引擎 + `ADR-CEP-12`)⇒ ⇒ 「本仓不能承载协议级语义」**是错的**;正确的是「本仓**不新造**协议语义的**设计**,但**可以实施**既有设计」。

**次要理由(重叠)**:与**模型级载体**(`test/cep_consistency_completion_model.hh`,B4 证据装置,已具备 SU#4 ② 六条非空洞性约束)功能重叠 ⇒ 本仓将维护两套 ⇒ 双重验证成本。⚠️ 「同居一仓」**不构成**不可行的理由 ⇒ 故「重叠」**仅作次要理由**,不得当作主论据。

### 决策 5 —— 判别力的**强度上限在本仓的协议引擎面**(⛔ 非「跨仓专属」)

需要**协议语义级**判别力时,载体是**协议引擎的不变量观测面**,而协议引擎族**就在本仓**(`include/tlm/cep/chi_rnf_protocol_engine.hh` / `cohif_protocol_engine.hh` / `mesif_protocol_engine.hh` / `moesi_gpu_protocol_engine.hh` / `tilelink_protocol_engine.hh` / `ualink_protocol_engine.hh`;实例声明面见 `ADR-CEP-12`)⇒ ⇒ **强度上限本仓可达**,⛔ **不是外部阻塞**;仅当需要**新协议语义的设计**(而非实现既有设计)时,才按 `ADR-CEP-00` 的单向门取 ArchForge 的设计权威。
⇒ 本 ADR 只**登记该路由**,⛔ 不裁决工程量级与排期(另立 change 评估)。

### 决策 6 —— **能力阶梯**显式化 + **档位声明义务** + **跨档聚合禁令**

| 档 | 载体 | 违例类证据强度 | 机器载体 |
|---|---|---|---|
| **0** | 拓扑级 E2E 装置(框架级观测面) | **样本通路活性**(⛔ 非语义判据) | `scripts/test/cep_full_green_run_check.sh` 的 **C1'** + `observation_keys` |
| **1** | 模型级 `ConsistencyCompletionModel` | **语义判据**(三负控 + 耦合探针 + watchdog 非空洞) | `ADR-CEP-06` SU#4 ② 六条非空洞性约束 |
| **2** | 协议级引擎(**本仓** `include/tlm/cep/*_protocol_engine.hh`) | **协议语义**(强度天花板) | `ADR-CEP-12` 的实例面 ⇒ ⏭ 待建不变量观测面(工程量级另裁) |

**(i) 档位声明义务** —— 任何一致性断言必须声明其档位与载体;⛔ **禁止**跨档比较强度(档 0 的 0 **不得**与档 1 的 0 并列引用)。

**(ii) 跨档聚合禁令(机器面已知盲区 · ⚠️ 实测)** —— 对账注册表 `test/cep_e2e_runs.jsonl` 的 3 条条目中,**唯一**的 `"ic3_verdict": "✓ 通过"` 是**档 0** 的拓扑 run(承载 B4 的**模型级** h3 v2 run **不在**该注册表内)⇒ 且 `ic3_verdict` 的三态词表(`scripts/test/cep_ic3_report_states_check.sh:10-12`)**不分档**、产物内**无任何 tier 字段** ⇒ ⇒ **任何按 `hypothesis_id` 聚合 verdict 的消费方,必须同读同条目的 `evidence_scope` 字段,⛔ 不得仅凭 verdict 计数**;⇒ 机器面补救(注册表档位词检查)落 `backlog: cep-evidence-tier-declaration`。

**(iii) 定位的载体对账表**(逐条标「已有 / 本次新增」—— ⛔ 禁止只列本 ADR 的载体):

| # | 载体 | 现状 |
|---|---|---|
| 1 | `ADR-CEP-06` SU 2026-11-13 ③(四条强度限定) | **已有** |
| 2 | `07 号` 附录 SU 2026-11-13 ③ | **已有** |
| 3 | `obj §6.2` B4 证据行(拓扑级档位追加行) | **已有** |
| 4 | `ADR-CEP-19` SU 2026-11-13(口径段) | **已有** |
| 5 | `docs/guide/cep-usage.md` §12.8(工程师**最高频**消费面) | **已有** |
| 6 | `test/cep_e2e_runs.jsonl` 第 3 条的 `evidence_scope` / `ic3_reason` | **已有** |
| 7 | `test/test_cep_stub_cache_emit.cc` 文件头诚实边界段 | **已有** |
| 8 | **本 ADR** | **本次新增** |

---

## 3. 选项对比表

| 选项 | 内容 | 结论 | 理由 |
|---|---|---|---|
| **A** 维持现状(空洞的 0 + 逐字口径) | 拓扑级装置定位不变;IC3 通过态 = 「该档位首次有**可机判**的通过态实例」;口径逐字登记 | **✅ 采纳(作为终态定位,非「暂不升级」)** | 该 0 的空洞是**装置语义面缺 sharer / 版本**的**结构性事实**,不是实现缺陷 ⇒ 现状已是该档位的**正确形态**;继续标注「强度不足」会诱发反复重开 ⇒ 决策 6 的阶梯把强度诉求**显式化**而非悬置 |
| **B** 在拓扑级装置内**语义级**局部升级 | ① 业务写数据非恒定 + ② bundle 加版本字段(或测试侧代理版本) | ⛔ **不采纳** | 装置**无 sharer / 无版本** ⇒ 升级后仍**不产生**装置级语义(决策 3a)⇒ ① 波及 `TrafficGenTLM` 的 11 个 SoC 配置 + bundle 语义(公共面)却拿不到判别力;② 测试侧代理版本 ⇒ 检测**测试侧自造语义** ⇒ 「可证伪」是假的 ⇒ **概念错误**,非成本问题 |
| **B′** 产出侧**检测力见证**(负控变体) | `test/test_cep_stub_cache_emit.cc` 增负控变体:注入已知违例形态 ⇒ 断言 `detector.counts() > 0` | **✅ 采纳(方向 · 进 backlog)** | 只覆盖**检测器面**、⛔ 不覆盖装置语义面 ⇒ **不**构成「装置级判别力」(⇒ 决策 3a 的拒绝**不适用**于 B′)⇒ 但可把档 0 从「样本活性」升级为「**样本活性 + 检测力已见证**」⇒ **test-side only · 零公共面变更** ⇒ 落 `backlog: cep-evidence-tier-declaration` ⇒ ⚠️ 落地前档 0 **无检测力见证**(决策 3c 登记的接受弱点) |
| **C** 让拓扑级承载真实一致性语义 | 给拓扑级装置加 sharer 可见性 + 版本 + 完整不变量语义 | ⛔ **不采纳**(作为本平台下一步) | **主理由(两条)**:① `ADR-CEP-19` 决策 9 已把该装置签批为「home **简化模型**」⇒ 升级 = 改决策 9;② 协议语义「**设计**」权威归 ArchForge(`ADR-CEP-00` 决策 1 + `:120` 单向门)⇒ 新造语义须先取设计权威。⚠️ **⛔ 非「协议 IP 归 ArchForge」**(代码归本仓;**本仓可实施、不新造设计**);**次要理由 = 重叠**:与模型级载体功能重叠 ⇒ 双份维护 |
| **D** 判别力路由到协议级 | 在**本仓**协议引擎族加不变量观测面;新协议语义的**设计**经 `ADR-CEP-00` 单向门取 ArchForge 权威 | **✅ 采纳(方向)** | 判别力的**天花板**在协议语义层(档 2),而该层**在本仓**(6 个协议引擎 + `ADR-CEP-12`)⇒ **上限可达**,⛔ **非外部阻塞**;⚠️ 工程量级另裁(另立 change) |
| **E1** 降格档 0 为**非实验 run**(回到 `framework-verification`、不绑 H3、两档阶梯) | 回滚本次已归档 change 的绑定与登记 | ⛔ **不采纳**(considered-rejected) | 方向可行(档位阶梯变两档)⇒ 但**治理成本**高:须反转**已归档** change `cpptlm-cep-full-green-run-evidence` 的全部交付物 + 修 `ADR-CEP-19` ASM-8 ① + 重写 `test/cep_e2e_runs.jsonl` 第 3 条 + 让位 Entry #396~#398 的登记 ⇒ 且 Entry #396~#398 已把「拓扑级首次可机判通过态实例」作为**已交付能力**登记 ⇒ 反转 = **能力收回**,⛔ 非 architect 单方面可为 ⇒ 须**用户裁决** |

---

## 4. 后果 / 代价

### 4.1 正面

- **诉求 L-1 被正面回答**且答案可执行:不是「不做」,而是「该诉求在本平台**概念上**不成立(装置无 sharer / 无版本)⇒ 强度上限在档 1 / 档 2」;
- **一条低成本的真实增量被识别并排队**(B′:产出侧负控见证)⇒ 档 0 可从「样本活性」升级为「样本活性 + 检测力已见证」;
- **杜绝反复重开**:能力阶梯 + 档位声明义务 ⇒ 后续「拓扑级为什么没有判别力」有契约级答案(档 0 = 样本通路活性),不再逐次重开;
- **保住证据面诚实**:档 0 的 0 仍可作**回归守门器**(「拓扑级链路的内存操作样本可观测 + 计数器未动」),且**不**被升格为一致性证明 ⇒ 与 Entry #396~#399 已落的口径一致;
- **零实施成本(本 ADR 自身)**:不改任何判据 / 阈值 / 预登记 / schema / retention ⇒ 新增 invariant 数 = 0 ⇒ 冻结面仅新增本 ADR 一件。

### 4.2 负面(代价)

- **档 0 的 0 永远「空洞」** ⇒ 拓扑级 run **不能**作为任何一致性安全断言的证据 ⇒ 引用面必须持续带档位限定(⛔ 人工纪律,机器**连「限定语在场」也保证不了** —— 实测无任何闸 grep 限定句;`cep_doc_ref_check.sh` 只验引用锚点可达,删 prose 不变红);
- ⚠️ **对账注册表对档位盲**(实测):`test/cep_e2e_runs.jsonl` 与 `ic3_verdict` 三态词表**均不区分档位**,且档 0 条目是注册表内**唯一** `✓ 通过` 实例 ⇒ ⇒ 任何按 `hypothesis_id` 聚合 verdict 的消费方**必须**同读 `evidence_scope`,⛔ 不得仅凭 verdict 计数 ⇒ 机器面补救在 backlog(决策 6(ii));
- **档 0 在 B′ 落地前无检测力见证** ⇒ 档 0 的 0 不含检测力语义(决策 3c);
- **拓扑级与模型级的双载体继续并存** ⇒ 决策 4 已把「本仓引入第二套一致性语义装置」标为**不采纳**,但**不能**禁止用户新增 ⇒ 只靠契约约束;
- **档 2 需协议引擎的不变量观测面建设** ⇒ 本仓**可达**,但工程量级**未排期**(另立 change 评估)⇒ ⛔ **不是外部阻塞**,是**内部排期债**;
- 决策 6 的**档位声明义务**是**新增的人工纪律** ⇒ 靠 8 处载体的逐字登记承载,⛔ 无独立机器闸(⇒ §5 抓取点 1 的载体状态标注)。

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| R-1 | 「档位声明」被**漏写** ⇒ 档 0 的 0 被读成一致性证明 | 决策 6(iii) 的 8 处载体对账表 + V-20-1(⚠️ **文档层**,无机器闸 ⇒ 人工评审 + `invariant_attribution_dispositions` 登记);⚠️ **不得**声称「机器保证限定语在场」(实测不成立) |
| R-2 | 后续有人以「B 只需小改」为由重提局部升级 | 决策 3a 逐条写明 B 的**两条路径都不产出装置级语义** + 11 个 SoC 配置的波及面 ⇒ 评审可直接引本 ADR 驳回;⚠️ 但**不得**把 B′ 混入 B 一起驳回(B′ **已被采纳**) |
| R-3 | 档 1 / 档 2 的强度被**跨档比较** | 决策 6(i) 明文**禁止** + 能力阶梯表固定档位语义 + 决策 6(ii) 点名注册表盲区 |
| R-4 | 档 2 的工程投入未排期 ⇒ 强度天花板在「可预见期」内不可达 | ⚠️ **本仓可控**(⛔ 非外部依赖)⇒ 由 backlog change 评估工程量级;⛔ **不得**表述为「跨仓阻塞」(v0.3 及以前的错误表述已在本版纠正) |
| R-5 | 档 0 的「档位声明」只在**深文档**里 ⇒ 只读 `cep-usage.md` 的工程师看不到 | 决策 6(iii) 把 `cep-usage.md` §12.8 列入载体对账表(第 5 行)⇒ 最高频消费面**已被覆盖**(现状:已有) |

---

## 5. 边界与抓取点(新增 invariant 数 = 0)

| # | 抓取点 | 载体 | 载体状态 |
|---|---|---|---|
| 1 | 档位限定的**逐字登记**(8 处载体对账表,决策 6(iii)) | `ADR-CEP-06` SU 2026-11-13 ③ · `07 号` 附录 SU ③ · `obj §6.2` · `ADR-CEP-19` SU 2026-11-13 · `cep-usage.md` §12.8 · `cep_e2e_runs.jsonl` 第 3 条 · `test_cep_stub_cache_emit.cc` 文件头 · 本 ADR | ⚠️ **文档层**(实测**无任何闸 grep 限定句**)⇒ 人工评审;✅ 归因 id 已复用(见 V-20-1)⇒ **无 id 债务** |
| 2 | 样本通路活性(档 0 的唯一机器判据) | `scripts/test/cep_full_green_run_check.sh` **C1'** + `observation_keys.memory_ops_checked_addrs` | ✅ **可执行**(6 条决定性变异含「删样本计数键 ⇒ 必红」) |
| 3 | 「档 0 闸不含语义判据形态」的流程禁令 | `scripts/test/cep_full_green_run_check.sh` 的 6 条变异集 | ⚠️ **文档层**(⛔ 机器不判「闸里有没有语义判据」)⇒ 人工评审 |
| 4 | 档 1 的语义判据(六条非空洞性约束) | `ADR-CEP-06` SU#4 ② 六条 + `test/test_cep_b4_evidence_device.cc` 三负控 + 耦合探针 | ✅ 可执行(**本 ADR 不改**) |
| 5 | 「本 ADR 不改判据」的机器面 | `experimental/preregistration.json` · `experimental/metrics.schema.json` · `ADR-CEP-06` 决策 1~5 | ✅ 可执行(`cep_prereg_drift_check` + `cep_freeze_check` 零 diff) |
| 6 | 跨档聚合禁令的机器面补救 | `backlog: cep-evidence-tier-declaration`(reconcile 闸加 `evidence_scope` 档位词检查) | ⏭ **待落地**(本 ADR 只登记义务) |
| 7 | 档 2 的**本仓**落点 + 设计权威边界 | `include/tlm/cep/*_protocol_engine.hh` · `ADR-CEP-12` · `ADR-CEP-00` 决策 1 / `:120` | **文档层**(⚠️ 本仓落点**已存在**;「新语义设计须取 ArchForge 权威」为纪律) |

⇒ ⛔ **新增 invariant 数 = 0** ⇒ 本 ADR 归 `scripts/test/cep_docs_sync_check.sh` 的 **`NO_INV`** 白名单(⛔ **禁**新造平行 id 体系;Verification 判据一律**复用**既有归因 id)。

---

## 6. 关联 ADR / 后续

- `ADR-CEP-00`(治理宪章)—— 决策 4 的**主理由 ②**(协议语义「**设计**」权威归 ArchForge + `:120` 单向门;⚠️ **代码归口 = CppTLM**)与决策 5 的设计权威边界;
- `ADR-CEP-06`(实验协议)—— SU#4 ② 六条非空洞性约束(**档 1** 的强度定义)· SU 2026-11-09 ② 五句证据边界(**档 0** 的自陈边界)· SU 2026-11-13 ③ 四条强度限定 · SU 2026-11-08·二 retention 丙;
- `ADR-CEP-19`(CohIf 载体)—— 决策 9 = 端口面 wrapper + home 简化模型(**档 0** 的装置面来源)· ASM-8 ① = 拓扑级 run 须绑定 H3 · SU 2026-11-13 口径段;
- `ADR-CEP-12`(协议引擎能力映射)—— **档 2** 的协议引擎族落点(chi_rnf / cohif / mesif / moesi_gpu / tilelink / ualink);
- **后续**:① `backlog: cep-evidence-tier-declaration` —— 决策 3b 的**负控见证**(产出侧)+ 决策 6(ii) 的**注册表档位词检查** + 能力阶梯入 `07 号`;② **档 2 的协议引擎不变量观测面**(**本仓可达**;工程量级另裁 ⇒ 另立 change)—— 若涉及**新协议语义设计**,先取 ArchForge 设计权威(`ADR-CEP-00` 单向门);
- **不依赖**:本 ADR **不**改 B4 的达成结论(Entry #228 ③)、⛔ **不**改 `B3b-2 = 0/4`。

---

## 7. 决策汇总

| 决策 | 结论 |
|---|---|
| 1 | 拓扑级 E2E 装置 = **框架级观测面**(不承载一致性语义) |
| 2 | 拓扑级违例类的唯一合法语义 = **样本通路活性**判据(⛔ 非语义);⚠️ C1' **不**证明「检测力已见证」 |
| 3a | ⛔ **拒绝**装置**语义级**局部升级(概念错误) |
| 3b | ✅ **采纳**产出侧**检测力见证(B′)**进 backlog(test-side only) |
| 3c | ⚠️ 登记接受弱点:B′ 落地前档 0 无检测力见证 |
| 4 | ⛔ **拒绝**拓扑级承载真实一致性语义(主理由 = ① `ADR-CEP-19` 决策 9 已签批为「home 简化模型」 ② 协议语义**设计**权威归 ArchForge;次要 = 与模型级重叠;⚠️ **⛔ 非「协议 IP 归 ArchForge」**) |
| 5 | ✅ 判别力强度上限**在本仓的协议引擎面**(⛔ 非「跨仓专属」;新语义**设计**才需 ArchForge 权威)· 本 ADR 只登记路由 |
| 6 | ✅ **能力阶梯**(档 0 样本通路 / 档 1 语义 / 档 2 协议语义)+ **档位声明义务** + **跨档聚合禁令** + **8 处载体对账表** |

---

## Verification(前瞻判据 · 可勾选 —— **不是**已跑完的实测日志)

> **口径**:本章是**签发前置**(per 台账 Entry #401)。每条给**载体** + **反向 fixture(毒化 ⇒ 必红 + 归因 id)**;归因 id **一律复用**既有体系 ⇒ ⛔ **禁**新造平行 id 体系。**载体状态**逐条标注:✅ 可执行 / **文档层**(人工评审)· ⏭ 待落地。

| # | 判据 | 载体 | 反向 fixture(毒化 ⇒ 必红) | 归因 id |
|---|---|---|---|---|
| **V-20-1** | 档位限定的**逐字登记**覆盖 8 处载体(决策 6(iii) 对账表) | §5 抓取点 1 的 8 处 | ⚠️ **文档层**(实测**无任何闸 grep 限定句**)⇒ **人工评审**;反向 fixture **无** ⇒ ⛔ **禁**当作鉴别力判据;✅ 归因 id **已复用**既有体系 ⇒ **无 id 债务**(⛔ 不产生 `invariant_attribution_dispositions` 登记) | `INV-CEP-06-IC3`(IC3 判定口径 owner `ADR-CEP-06`;**贴合度**:判的是「档位限定语是否在载体面齐备」,归因落在 IC3 口径面) |
| **V-20-2** | **能力阶梯三档**与**档位声明义务**已登记 | `07 号` §4 / 新 SU · 本 ADR §2 决策 6 | ⚠️ **文档层** ⇒ 人工评审;反向 fixture **无** ⇒ ⛔ 禁当鉴别力判据;✅ 归因 id 已复用 ⇒ **无 id 债务** | `INV-CEP-06-IC3`(IC3 判定口径 owner `ADR-CEP-06`;**贴合度**:档位声明是 **IC3 结论的读法约束** ⇒ 归该口径 owner;⚠️ 原拟挂 `INV-CEP-12-IC1` **已更正** —— 该 id 语义是「协议实例记录必含 7 字段组 / 13 顶层属性」,与阶梯登记无关) |
| **V-20-3** | 档 0 闸**只**判样本通路活性,⛔ **不含**语义判据形态 | `scripts/test/cep_full_green_run_check.sh`(C1' + 6 条变异) | ⚠️ **文档层**(⛔ 机器不判「闸里有没有语义判据」)⇒ 人工评审;反向 fixture **无** ⇒ ⛔ 禁当鉴别力判据;✅ 归因 id 已复用 ⇒ **无 id 债务** | `INV-CEP-06-IC5`(SWMR + data-value 双不变量 owner `ADR-CEP-06`;**贴合度**:该 id 正是「违例类语义面」的 owner ⇒ 禁止闸越界到该面) |
| **V-20-4** | 档 1 的语义判据**未被削弱**(六条非空洞性约束原样) | `ADR-CEP-06` SU#4 ② · `test/test_cep_b4_evidence_device.cc` | 削弱任一条 ⇒ `test_cep_b4_evidence_device` 必红(注入违例 ⇒ 计数必 > 0) | `INV-CEP-06-IC5` |
| **V-20-5** | 本 ADR **零 diff** 判据面(`preregistration.json` / `metrics.schema.json` / `ADR-CEP-06` 决策 1~5) | `cep_prereg_drift_check` + `cep_freeze_check` | 改动其一 ⇒ 必红 | `INV-CEP-06-IC1`(4 件套) |
| **V-20-6** | **跨档聚合禁令**的机器面已落地(reconcile 闸校验 `evidence_scope` 档位词) | `backlog: cep-evidence-tier-declaration` | ⏭ **待落地** ⇒ 本 ADR 只登记义务 ⇒ 落地后方有反向 fixture(毒化:删 jsonl 第 3 条的 `evidence_scope` ⇒ 必红)⇒ ⛔ 在 backlog change 落地前**禁**当作鉴别力判据 | 归因 id **待建立** ⇒ 登记 `invariant_attribution_dispositions` |

**载体状态汇总**:V-20-4 / V-20-5 **载体已存在且逐条实证** ⇒ **可执行**;V-20-1 / V-20-2 / V-20-3 为**文档层判据**(实测无机器载体)⇒ 人工评审 ⇒ ⛔ **禁**当作鉴别力判据;V-20-6 **待落地**(backlog change)。

**签发前置自检**:① 本章位于**正文**(首个 `## Status Update` 之前)✅ ② 每条判据均有**载体 + 归因 id + 反向 fixture**(文档层 / 待落地项已**逐条如实标注**「无载体 / 禁当鉴别力判据」)✅ ③ 归因 id **全部复用**既有体系(⛔ 零新造;待建项走 `invariant_attribution_dispositions` 登记)✅ ④ 载体**逐条实证存在**(可执行项)✅ ⑤ 文档层项的 `test/cep_adr_structural_deferrals.json` 登记(签发前置第 ⑤ 项)⏭ **待办**(⛔ 签发前必须完成)

---

## 8. 修订记录

| 版本 | 日期 | 变更 | 依据 |
|---|---|---|---|
| v0.1 | 2026-11-13 | 初稿(🟡 Proposed):决策 1~6 + 选项 A~D + 能力阶梯三档 | 台账 **Entry #401** 授权 · 用户口令「让 architect 拍 P1」· 台账 **Entry #399** 遗留 L-1 |
| v0.4 | 2026-11-13 | 吸收 **critic 首轮 BLOCKER-1**(对 `ADR-CEP-00` 的引用不成立):**决策 4 主理由重建** —— 删「协议 IP 归 ArchForge ⇒ 越界」(错误:`ADR-CEP-00` 决策 1 = **代码归口 CppTLM**、决策 4 表「ProtocolEngine 插件机制 = CppTLM」、`:120` 是「**设计权威** → 实施」单向门)⇒ 改为两条独立理由(① `ADR-CEP-19` 决策 9 已签批为「home **简化模型**」⇒ 升级 = 改决策 9;② 协议语义「**设计**」权威归 ArchForge)并**显式纠正**「本仓不能承载协议级语义」这一错误说法;**决策 5 重建** —— 档 2 **本仓可达**(6 个协议引擎 + `ADR-CEP-12`),⛔ 非「跨仓专属/外部阻塞」⇒ 档 2 表格、选项 C/D、§4 代价、R-4、§6 关联与后续、抓取点 7 **同步重建**;另吸收 SHOULD_FIX 2 处(决策 3a 与 `ADR-CEP-06` SU 2026-11-13 ③ 末条的显式衔接 · V-20-2 归因 id `INV-CEP-12-IC1` → `INV-CEP-06-IC3` 并注明更正)+ NIT 1 处(header 状态版本号漂移) | critic 首轮 |
| v0.3 | 2026-11-13 | oracle 复审(PASS-WITH-NOTES · F1~F6 全闭 · 0 新 BLOCKER)的 2 处修正:① **签发前置自检 ⑤** 的虚报(`✅` → `⏭ 待办`;登记表登记尚未执行)· ② 「检测器在线」措辞收紧为「**检测力已见证(离线负控)**」(负控是**离线**见证,⛔ 非「每次 run 在线」) | oracle 复审 |
| v0.2 | 2026-11-13 | 吸收 oracle 首轮审稿 MUST-FIX **F1~F3** + SHOULD_FIX **F4~F6**:① 决策 2 的 C1' 语义降称(实测 `emit:126` 与 `harness:176-177` ⇒ C1' **不**证明「检测力已见证」);② §4.1 删「未观测到违例」⇒ 改「样本活性在 + 计数器未动」(与决策 2 禁令句自洽);③ 「新闸 C1'」正名为 `scripts/test/cep_full_green_run_check.sh` 的 C1'(闸已随 change 归档 ⇒ 签发后不可变 ADR 不得写「新闸」);④ 决策 6 增「跨档聚合禁令」(实测 `cep_e2e_runs.jsonl` 唯一 `✓ 通过` 是档 0)+ **8 处载体对账表**;⑤ 选项表增 **B′**(✅ 采纳 · 产出侧负控见证 · **形态经更正**:属产出侧,⛔ 非新闸判据)与 **E1**(⛔ considered-rejected · 理由 = 治理成本 + 须用户裁决);⑥ 决策 4 的**主理由改引宪章域界**,「重叠」降为次要理由(「同居一仓」不构成不可行);⑦ V-20-1 / V-20-2 / V-20-3 的反向 fixture 去「⇒ 判红」(实测无机器闸 grep 限定句)⇒ 改标**文档层 + 禁当鉴别力判据** + `doc_layer_manual_review` 登记;⑧ §4.2 增「注册表对档位盲」代价;⑨ §4.3 R-1 的缓解措辞修正(机器**连「限定语在场」也保证不了**) | oracle 首轮审稿(F1~F6)· 本 ADR §8 v0.2 行 |

> ⛔ **本 ADR 未签发**:oracle + critic 双审完成且 BLOCKER 全闭前,⛔ 不得进入「已签发」状态、⛔ 不得走冻结面授权链。

---

## Status Update 2026-11-13(per 台账 Entry #401 / #402)—— **签发**(v1.0)

> **性质**:本则 = **签发登记**(append-only;⛔ 不改 §1~§8 正文与既有内容)。

| 项 | 值 |
|---|---|
| 状态 | 🟡 Proposed(v0.1~v0.4)→ ✅ **已签发**(v1.0,2026-11-13) |
| 审稿 | **oracle 2 轮**:首轮 **REJECT**(3 MUST-FIX F1~F3)⇒ 复审 **PASS-WITH-NOTES**(F1~F6 6/6 闭合 · 0 新 BLOCKER)· **critic 2 轮**:首轮 **REJECT**(1 BLOCKER:`ADR-CEP-00` 宪章域界**误引**)⇒ 复审 **APPROVE**(0 BLOCKER · 0 SHOULD_FIX · 2 NITS,其一已吸收) |
| 审稿报告 | `.rddf/reviews/ADR-CEP-20-review.md` |
| 登记 | `test/cep_adr_structural_deferrals.json`(`decision_adrs` +1;**仅 V-20-6**(归因 id 待建立)的 `doc_layer_manual_review` 登记 —— V-20-1~V-20-5 的归因 id **均已复用**既有体系 ⇒ **无 id 债务**)· `scripts/test/cep_docs_sync_check.sh` 的 `NO_INV` 白名单 +1(新增 invariant 数 = **0**) |
| 冻结面 | `docs/adr/ADR-CEP-20-*.md` 入 M1 冻结集 ⇒ 冻结产物 82 → **83** ⇒ `cep_freeze_check.sh --update --authorized-by "Entry #401"` |
| 关联 change | `backlog: cep-evidence-tier-declaration`(**未立项**;立项后回填指针 + AC 映射) |
| 新增 invariant 数 | **0** |

### 签发后的边界(逐字)

- ⛔ **决策内容自此不可改**:此后任何修改**只**能以追加 `## Status Update` 段的形式进行,且走冻结面授权链;
- ⛔ 本 ADR **不改**任何判据 / 阈值 / 预登记 / `experimental/metrics.schema.json` / retention 条款;
- ⛔ 本 ADR **不改** B4 的达成结论(台账 Entry #228 ③)· ⛔ **不改** `B3b-2 = 0/4`;
- ⚠️ **两条遗留交叉引用(⛔ 非本 ADR 义务)**:① *可选* —— 在 `ADR-CEP-06` 追加一条 SU 交叉引用本 ADR(critic NIT;签发后由任意后续批次执行);② 决策 6(iii) 的 8 处载体中,`docs/guide/cep-usage.md` §12.8 与 `ADR-CEP-06` SU 2026-11-13 ③ 的**可达性**由 `cep_doc_ref_check` 兜底,⛔ 但它们**是否仍逐字携带口径**无机器闸(见 V-20-1 的文档层标注)。
