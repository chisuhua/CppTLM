# AC-9 实现评审 —— H3 v2「一致性 × 完成域」复合装置

**评审人**:architect · **日期**:2026-11-05 · **性质**:实现评审（`ADR-CEP-06` **SU#4** §⑤ 末项 / improvement `cep-b4-evidence-device` **AC-9**）
**评审对象**:`test/cep_consistency_completion_model.hh` · `test/test_cep_experiment_emit.cc`（`run_h3` v2）· `test/cep_completion_domain_model.hh`（阻塞开关）· 产出 `experimental/run-*-h2h3h4h6/h3`
**判据来源**:`ADR-CEP-06` SU#4 §② 六条非空洞性硬约束 + §⑤ 末项「装置含真实阻塞机制,**非**『watchdog 恒假 + 默认 0』」
**结论**:**PASS**（B4 回填的 gate 已过）· **架构决策无需修订**（见 §6）

---

## 1. 核心判定：为什么「恒假 / 恒真 watchdog」都不可能通过本装置

这一条比逐行读代码更强 —— 它是**结构性**论证，且由产出层硬约束（`EmitResult.fatal`）强制：

| 反事实 | 传导链 | 结果 |
|---|---|---|
| **watchdog 恒假**（永不 stalled） | `mislabel` 负控 ⇒ `watch_dog_stalled()==false` ⇒ `qualify()` 的 `deadlock_detector_live=false` ⇒ `!q.qualified()` | ⇒ `EmitResult.fatal` ⇒ **拒绝产出**（⛔ 不登记结论） |
| **watchdog 恒真**（空闲也 stalled） | 正控 ⇒ `deadlocked()==true` ⇒ `deadlock_detected` 写 **1**（不是 0）且 `run_until` 提前返回 `false` | ⇒ `!reached` ⇒ `EmitResult.fatal` ⇒ **拒绝产出** |

⇒ 能产出 run 的装置**必然**同时满足「正控 `max_idle_run` < 阈值 200」**且**「负控 `max_idle_run` ≥ 阈值」——
这两个条件不能靠一个恒假或恒真的判定同时满足。**「默认 0」在结构上不可能**：`deadlock_detected` 的取值来自
`positive.deadlocked()`（真实运行态），且 `deadlock_violations` 在 `_measured` 集内、不在 `_not_collected`。

**工具证据**
- `test/test_cep_experiment_emit.cc:212` —— `r.metrics.deadlock_detected = positive.deadlocked() ? 1 : 0;`（读运行态，非常量）
- `test/test_cep_experiment_emit.cc:205-211` —— `if (!reached || !q.qualified()) { bad.fatal = true; ... return bad; }`
- `test/cep_experiment_harness.hh:227` —— `bool stalled() const noexcept { return max_idle_ >= threshold_; }`（记忆型 `max_idle_`，非每周期瞬时值）
- 产出实测 `_measured` 含 `deadlock_violations` + `memory_violations`；`_not_collected` 恰为 `coherence_stalls` / `collective_stalls` / `directory_misses`

---

## 2. 逐条判定（SU#4 §② 六约束）

| # | 约束 | 判定 | 工具证据 |
|---|---|---|---|
| 1 | **在途窗口真实存在** | ✅ PASS | 产出 `max_in_flight=3`（`service_per_cycle=1` × 每轮注入 3 条 coherence ⇒ 排队真实形成）、`max_idle_run=2 > 0`。`test/test_cep_b4_evidence_device.cc` 正控段 `REQUIRE(m.max_in_flight() > 1)` |
| 2 | **双检测器各自被负控证明** | ✅ PASS | `test/cep_completion_domain_model.hh:115` 的到达判定 + `test/cep_consistency_completion_model.hh:214` 的开关透传构成阻塞链；错标 ⇒ 死锁=1（`qualify()` 的 `deadlock_detector_live`）；SWMR 注入 ⇒ swmr=1197；过期读 ⇒ data_value=1199。`qualify()` 同时要求两桶（`swmr>0 && data_value>0`） |
| 3 | **0 是实测 0** | ✅ PASS | `_measured` 含两键 + `_not_collected` 不含两键；`deadlock_detected` 由运行态取值；`memory_violations` 由 `detector().counts()` 取值 |
| 4 | **不得以空检查充数** | ✅ PASS（附 S1） | 退化开关 `detach_memory_ops` ⇒ 覆盖率 0 ⇒ `same_stream=false` ⇒ `fatal`；`degenerate_expect_fatal` 变体断言「fatal 且未写任何文件」。**但 `superset_violations` 的判定不可证伪 —— 见 S1** |
| 5 | **同一事务流（实例级）** | ✅ PASS | `memory_checked_txn_ids` = `blocking_subject_txn_ids` 的 1200 元素子集 ⇒ 覆盖率 1.000000；反向 fixture（`detach_memory_ops`）能使其为 0 ⇒ **可证伪** |
| 6 | **耦合探针（阻塞开/关）** | ✅ PASS | `scripts/test/cep_b4_coupling_probe.sh`：Δ(swmr)=+0 / Δ(data_value)=+0，与预测锚 `experimental/predictions/h3_coupling_v2.json` 一致；锚前置校验（实现 commit 是 HEAD 祖先 + 锚 mtime ≤ run mtime）；灵敏度 = SWMR 注入在两配置均 > 0（`[cep][b4]` 耦合探针 TEST_CASE） |

**§⑤ 末项（实现评审）：装置含真实阻塞机制** —— ✅ PASS。
阻塞机制的载体是 `CompletionDomainModel::tick()` 的到达判定（`test/cep_completion_domain_model.hh:112-127`）：
collective 成员的**到达**依赖 coherence 全部退休；该到达又推进 group-scoped coherence 的成员计数
⇒ 错标时形成「coherence 等 collective 到达 / collective 等 coherence 退休」的**循环等待**。
这不是「挂一个计时器报死锁」，而是与 `ADR-CEP-01 §6.2` 代价 3（分类错误 = 静默死锁）同构的可判定机制。

---

## 3. 发现（不阻塞 B4 回填）

### S1（SHOULD_FIX）`superset_violations` 判定在当前流下**不可证伪**

**事实链**（代码级）：
1. `inject_round()` 用 `dcoh_.set_sharer(addr, node, S)` 构造 `truth`（`truth |= responder_mask_of(node)`）——
   **truth 的来源就是 set 操作本身**；
2. `execute_memory_ops()` 中 `home_.service_read(addr)` 在 `FORWARD_TO` / `BROADCAST` 分支只
   `drive_snoop(..., resp_state=S, ...)` ⇒ **不收缩** sharer 集（`test/cep_directory_experiment.hh:91-121`）；
3. 随后 `dcoh_.view_of(addr).sharers & truth` 判定 —— 该 view 与步骤 1 的 set 是**同一 tracker 的即时投影**；
4. `clear_sharer(t.addr, t.node)` 发生在判定**之后**，且 `t.node ∈ truth`。

⇒ `truth ⊆ sharers` 是**同义反复**（tautology），`superset_violations == 0` 在该流形态下**结构必然**。
⇒ 「0 漏探」**不得作为独立证据引用**；`AC-8` 的「漏探=0 重证」因此只证明「数值得以保留」，不证明「无漏 snoop」。

⚠️ **这是 v1 装置同款的既有形态**（`run_h3` v1 亦为「set → 立即 view」），**非本次引入**。
按「范围最小化」不在本轮修，登记为后续项：
- **修法 A（推荐）**：补一条反向 fixture —— 构造 `truth` 含一个**未被 set / 已被 clear** 的节点 ⇒ 须红；
- **修法 B**：§9.1 ① 表 H3 分支 3 的 `✅ 成立` 加限定「判定当前不可证伪 ⇒ 仅登记数值,不作独立证据」。

### S2（SHOULD_FIX）H4 装置的阻塞开关**缺默认值断言**

`CompletionDomainConfig::collective_requires_coherence_retirement = true`（`test/cep_completion_domain_model.hh:70`）
是 H4 证据链的行为前提，但 `test/test_cep_h4_completion_domain.cc` 只用默认值、**未显式断言**该默认。
⇒ 若后续有人改默认值，H4 的「正控无死锁」结论会静默失效。
**缓解现状**：h2/h4/h6 的 13 必纳 + 声明集实测**逐项不变**（本轮已比对）⇒ 当前无实际漂移。
**修法**：H4 单测加 `REQUIRE(cfg.collective_requires_coherence_retirement == true)`（1 行回归锁）。

### N1（NIT）`data_value` 的观测形态是 self-read-after-write

`detector_.read(t.addr, t.version)` 读的是**本事务自己刚写的版本**，不是跨节点可见性判定
⇒ 语义强度有限（注入可红 ⇒ 非空检查）。**与 v1 一致**，引用时须带「模型级 / 自身版本回读」限定（文档已有）。

---

## 4. 既有证据未被扰动的复核

| 项 | 判定 | 证据 |
|---|---|---|
| h2 / h4 / h6 指标 | ✅ 逐项不变 | 两次 `run-h2h3h4h6` 的 13 必纳 + 声明集 diff（排除 `observation_keys` / `verification_scope`）全部相等 |
| h3 的变更 = 设计意图 | ✅ | `deadlock_violations` 由 `not-collected` → 实测 0；`_not_collected` 由 4 项收缩为 3 项（与预登记收缩一致）；延迟语义改为在途窗口 |
| 13 必纳契约面 | ✅ 零变 | `experimental/metrics.schema.json` 的 `required` 仍为 13 键；`additionalProperties: true` ⇒ `observation_keys` 为**附加键**，不侵入契约面 |

---

## 5. 结论与放行条件

**AC-9 = PASS** ⇒ `§6.2 B4` 的**回填 gate 已过**，可交 planner 落表。

放行**不附带**阻塞条件；S1 / S2 为**后续项**（不阻塞 B4，登记入 planner 的后续任务清单）。
⛔ 引用纪律：`memory_violations` / `deadlock_violations` 的 0 是**模型级实测 0**，
不得表述为「已证明协议层无违例 / 无死锁」（per `docs/validation/2026-11-05-cep-b4-evidence-device.md §5`）。

---

## 6. 架构决策是否需要修订 —— 判定：**无需修订**

审查范围 = SU#4 及其落地引入的 4 处「可能触及决策面」的变更：

| # | 变更 | 判定 | 理由 |
|---|---|---|---|
| 1 | H3 的**延迟语义换版**（lookup 成本 avg 14 → 在途窗口 avg 2） | ⛔ **无需 SU** | `§9.1 ①` H3 行的依赖观测是 `memory_violations` / `deadlock_violations` / `superset_violations`，**不含延迟**；H2 的延迟来自独立装置且实测不变。⇒ 实现口径变更，非决策变更。落点 = **planner 表注**（「v1/v2 延迟不可比」） |
| 2 | **v1 装置形态不保留为可跑变体** | ⛔ **无需 SU** | SU#4 §① 的「升级」决策已隐含单一形态；可追溯性由 **v2 同参数重证**承担（已在 `2026-11-04` 文档 §10 勘误登记）。若为「不保留旧文件」再加一则 SU，只增冻结面重算成本、零决策收益 |
| 3 | **H4 装置新增阻塞开关**（默认 true） | ⛔ **无需 SU** | 测试件**可配置化**，默认行为不变且 h4 指标实测逐项不变；契约面零改动。**但要求补 S2 的默认值断言**（后续项） |
| 4 | `metrics.json` 顶层新增 `observation_keys` | ⛔ **无需 SU** | SU#4 §② 约束 5 / §⑤ 已**明确要求**观测键实际落盘；schema `additionalProperties: true` ⇒ 不侵入 13 必纳契约面 |

**⇒ SU#4 不追加 SU、不做勘误。**

**待办（交 planner，不属于决策修订）**：SU#4 §⑤ 的 11 条 Verification checkbox 的**勾选回填**——
需「台账 entry → 内容 commit → 冻结 `--update --authorized-by <entry>` → manifest commit」双步授权链；
建议与 B4 回填**同批**执行（证据齐了再勾）。

---

## 7. 关联

`docs/adr/ADR-CEP-06-experiment-protocol.md` **SU#4** · `.rddf/improvements/cep-b4-evidence-device.md` **AC-9**
· 台账 **Entry #226 / #227** · `docs/validation/2026-11-05-cep-b4-evidence-device.md`
· `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` §10（勘误）
