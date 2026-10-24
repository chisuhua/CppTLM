# Tasks — cpptlm-cep-poc-spike

> **状态**: 🟡 **Proposed**(2026-10-24 建目录)
> **队列**: `obj-cep-platform §4.0` 队列 **#2** · 前序 #1 已 ✅ archived → KPI 并发 0/3 → 本 change 占 1 slot
> **落地形态**: 正式 openspec change + delta capability `cep-poc-spike-verification`(per `design.md` D1)
> **权威规格**: `docs/implementation/2026-10-22-cep-poc-spike-spec.md`

---

## 0. 落地与登记

- [x] **T0.1** 建 `openspec/changes/cpptlm-cep-poc-spike/` 4 件套(proposal / design / specs / tasks)+ `.openspec.yaml` ✅ **本 commit 已完成**
- [x] **T0.2** `openspec validate cpptlm-cep-poc-spike --strict` rc=0
- [x] **T0.3** `obj-cep-platform §4.0` 状态列 ⏸ → 🟡;`phase-10.md` 对应子任务行同步;§11 台账补 Entry(登记落地形态裁决 + 承 S1-1~S1-5)
- [x] **T0.4** `openspec list --json` 实测 active 数 == 1(本 change 占用 1 slot,≤ 3)

---

## 1. S2 —— 跨域可见写(**已交付,继承**)

> ✅ **S2 已实跑通过**(2026-10-22,per 台账 Entry #16;初版 RUN 被 critic 3 BLOCKERS 证伪后修正重跑)· 已注册 ctest 回归门(现 ctest #97)· **交付「判据 (b) 成立」**

- [x] **T1.1** S2 spike 实现(`examples/cep_poc_spike_s2.cc`)✅ 2026-10-22
- [x] **T1.2** S2 注册 ctest 用例 ✅ 2026-10-22
- [x] **T1.3** **S2 的反向「有牙」验证** —— 规格 §3.3 的「只跑正向等于没跑」纪律**同样适用于 S2**(契约 Scenario「每场景的反向验证不得被删除」要求 3 场景各有 1 条)→ **补齐 S2 的反向验证**
- [x] **T1.4** S2 的判据结论**登记进 delta spec**(✅ 已在本 change 的 delta spec 中)→ 实现时核对 `examples/cep_poc_spike_s2.cc` 的实际判据与 spec 措辞**逐字一致**

---

## 2. S1 —— 单域基线(`perm` 投影不变量)

> 判据(per 规格 §3.1):**逐序列点**比对「实际投影 `perm`」与「由 6 态机推导的期望 `perm`」;只判 `perm` 投影,**不判**协议插件私有瞬态态 `T`(per `design.md` D5)

- [x] **T2.1** 实现 `examples/cep_poc_spike_s1.cc`:D-GPU 域 1 个 L1(4 值 `perm` 投影)+ `moesi_gpu` 插件(6 态 × 6 事件)
- [x] **T2.2** 实现 S1 的**正向**判据:逐序列点比对实际 `perm` 投影 vs 6 态机推导的期望 `perm`;任一不一致 → 失败退出
- [x] **T2.3** 实现 S1 的**反向「有牙」验证**(per `design.md` D3):把期望 `perm` 序列**故意改错一项** → 判据**必须**报出不一致(否则该实现恒真 → 报红)
- [x] **T2.4** 注册 ctest 用例 `cep_poc_spike_s1`(`examples/CMakeLists.txt`,与 S2 同形)
- [x] **T2.5** 核对 S1 判据与 delta spec 的 Scenario 措辞**逐字一致**
- [x] **T2.6** **S1 判定结论登记** —— 若 S1 结论为红(4 值 `perm` 无法表达 6 态机转移,即 R-2 命中),该结论 SHALL 作为**交付物**登记(用例**仍绿**),并关联 `ADR-CEP-05 §7` 判据 2 状态(per `design.md` D7-a)

---

## 3. S3 —— 死锁自由(per-network 顶点下的无环判据)

> 判据(per 规格 §3.3):2 网络 × 4 VC(`compute_noc` + `ualink_fabric`)· **per-network** 绑定 · 顶点 = **`(协议实例, network, vc_id)`** · 依赖图在该顶点集上**无环**

- [x] **T3.1** 实现 `examples/cep_poc_spike_s3.cc`:2 网络 × 4 VC + per-network 绑定 + 通道依赖图构建(顶点 = `(协议实例, network, vc_id)`)
- [x] **T3.2** 实现 S3 的**正向**判据:在**升级顶点**上跑环检测 → 期望**无环**
- [x] **T3.3** 实现 S3 的**反向「有牙」验证**(per 规格 §3.3 **强制**):把顶点**降级**回 `(协议实例, vc_id)` 重建同一图 → 环检测器**必须报出环**;若仍报无环 → 该检测器恒真 → **报红**
- [x] **T3.4** 注册 ctest 用例 `cep_poc_spike_s3`(`examples/CMakeLists.txt`)
- [x] **T3.5** 核对 S3 判据与 delta spec 的 Scenario 措辞**逐字一致**
- [x] **T3.6** **S3 verdict 逐 `(network, vc)` 标注** —— 不得只给全局结论(per `ADR-CEP-09:223-224`)
- [x] **T3.7** **S3 报环成因标注** —— 降级后检出的 2↔3 环 SHALL 标注为「**网络别名造成的假环**」,**不得**表述为「存在真实的跨网依赖」(按权威边表跨网边 = 0;per `design.md` D6-②)
- [x] **T3.8** **S3 反向断言的有效期标注** —— 止于**第 3 张网**出现之前(per `design.md` D6-④)

---

## 4. S1-1 ~ S1-5 —— 装配期前置(自 `umbrella/tasks.md:406-414` 承接)

> ⚠️ **纪律**(承 `umbrella/tasks.md:407`):**未消费前不得下线** —— provisional **不许变永久债**。
> ⚠️ **不得**为「表看起来干净」而勾选未到消费时机的事项(per `design.md` D4 反例)。
> ⚠️ **更不得**把「已判定不触发」写成「已消费」—— 二者语义不同(per `design.md` D4.5)。
> ⚠️ **不得**把umbrella 归档件当作**状态真源**去改写(append-only + 归档不可变);真源是本表 + `obj §4.0`。

### 4.1 逐项三层判定(2026-10-24 planner 审查,per `design.md` D4.1)

| 项 | 分层 | 状态 | 证据 |
|---|------|------|------|
| **S1-1** | ① 定义层(ADR 顶点定义) | ✅ **已消费** | `ADR-CEP-09 §8.2` · commit `964455e8`(2026-10-22)· `ADR-CEP-12:264` 已同步 |
| | ② 建图层(按新顶点实际建图并跑装配期无环判据) | ⛔ **未消费** | `ADR-CEP-09:221` 措辞为「**建立**之前」· `07 号:684` = 「📐 契约已定义,🛠️ 未实现」 |
| | ③ 放行闸(解除「不得用其无环判据放行任何协议实例」) | ⛔ **仍有效**(当前 vacuous:0 个 `implemented` 实例) | `ADR-CEP-09:221` |
| **S1-2** | N-1 / N-2 实现侧复核 | ⏸ **已判定不触发** | 触发条件 = 首个 `implemented` 实例**装配后**;spike 无 adapter / 不进 `ModuleFactory` → **不产出协议实例** |
| **S1-3** | SLICC 备选重评 | ⏸ **备选已登记 + 触发条件待命** | 备选登记在 `ADR-CEP-12 §8`(Entry #9) |
| **S1-4** | `fabric.json` 纳 M1 冻结集 | ⏸ **规则已登记 · 前提已破** | 规则:Entry #9(`fabric.json` 不冻结;`fabric.schema.json` 早已冻结)· **前提已破**:该文件**已被 S2 spike 的 ctest 读取**(`examples/cep_poc_spike_s2.cc:407`) |
| **S1-5** | adapter 消息类 → `CATEGORY_*` 显式映射表(非恒等) | 🟡 **规约已闭合 · 表体与 contract test 未做** | 规约:`ADR-CEP-12 §8.3` 分层纪律;表体**需 adapter**,而 spike 无 adapter |

### 4.2 本表的动作项(逐项)

- [x] **T4.0** 5 项**逐项**登记(条目 + 分层 + 状态 + 证据)—— ✅ **2026-10-24 planner 审查已完成登记**
- [x] **T4.1** **S1-1 ①(定义层)核对** —— S3 的**顶点集与依赖边集** SHALL **恰好**等于机器可导出集合(8 顶点 / 8 边 / 跨网边 = 0),并逐 `(network, vc)` 标注 verdict(per `design.md` D6 + delta spec 对应 Scenario)
- [x] **T4.2** **S1-2 / S1-3 / S1-4** 登记「**已判定不触发 / 待命**」+ **钉死度量口径** + **复核节奏**(per `design.md` D4.5 表)—— 落 `obj §4.0` / `§4.2` / `phase-10.md` 三处镜像
- [x] **T4.3** **S1-4 的真实问题**改写为**当下可裁**的决策:「`fabric.json` 已被 ctest 回归门消费 → 是否因此立即纳入 M1 冻结集?」→ **交 DRI 裁决**(per `design.md` D4.2)
- [x] **T4.4** **S1-5** 同形登记:规约已闭合(`ADR-CEP-12 §8.3`)/ 表体与 contract test 未做(需 adapter)/ **与 M-4 行为侧合并为同一决策**;owner 指向队列 #6(**待 DRI 拍板**)
- [x] **T4.5** **补登 3 项孤儿前置**(per `design.md` D4.4):**M-4 行为侧**(与 S1-5 合并)· **`moesi_gpu` / `mesif` 未排期**(S1/S3 的输入依赖,必须给 owner)· **verdict 逐 `(network, vc)` 标注义务**(归队列 #4)

### 4.3 owner 重指向(归档时不产生「指向已归档 change 的无主债」)

| 义务 | owner | 理由 |
|------|-------|------|
| S1-1 ②建图层 + ③放行闸 | **队列 #4** `cpptlm-cep-directory-ordering-plane` | 该队列已关联 `ADR-CEP-09 Decision A`(装配期无环检查)· 队列 #2 是可丢弃 spike,**不产出**装配期校验器 |
| S1-2 / S1-3 / S1-4 | **队列 #3+** | 触发条件本身指向未来实例;spike 不产出实例 |
| S1-5(+ M-4 行为侧) | **队列 #6** `cpptlm-cep-chi-rnf-subset`(**待 DRI 拍板**) | 「首个真实使用 schema 的协议 change」;归属属**排期决策**,不由本 change 推定 |

## 4.6 移交 builder 的 3 项(属**实施类**,planner 只出规格)

> **角色边界**:planner 不改 `src/**`、不实施门禁脚本改动、不做实施类 commit —— 以下 3 项**移交 builder**。

- [x] **T4.6** **承接链的机器化(第 7 条断言)** ✅ 2026-10-24 —— `scripts/test/cep_docs_sync_check.sh` 第 11 组新增**断言 ⑦**,4 条子判定:
  ① **可达** —— 归档件(umbrella 归档 `tasks.md`)里每个 `⏭(移交)` 标记 SHALL 在 `obj §4.0` 排队表出现(§4.0 = 权威表,per `obj:231`);
  ② **复述一致** —— 同一集合 SHALL 在 `obj §4.2` 出现(§4.2 是 §4.0 的同义复述);
  ③ **owner 活性** —— §4.0 / §4.2 中**凡行内提及移交标记**的行,其 change 名 SHALL **非已归档 change**(防「指向已归档 change 的无主债」);
  ④ **反向不凭空** —— 活文档出现的 `S1-N` SHALL 在归档件中**存在**(归档件含 S1-1 ~ S1-7)。
  **标记展开**:`S1-1 ~ S1-5` 这类**区间写法**须展开(phase-10.md 用的是区间写法)。
  **判定载体**:`obj §4.0` + `obj §4.2` 两张表;`phase-10.md` **仅作反向检查的来源**,不作判定载体(它是依赖图,不是承接列真源)。
  **为何要**:Entry #22 S8 自陈「6 项移交后**无任何机器闸**保证队列 #2/#4/#6 承接」—— 本次修订所治的正是这类漂移。
  **验收**:fixture **4 个**(1 绿 + **3 变异必红**:移除 S1-3 提及 / change 名改成已归档的 / 凭空造 S1-9)→ `cep_gate_selftest.sh` 场景数 **14 → 18**。
- [x] **T4.7** **测试头注释的旧路径** ✅ 2026-10-24 —— 归档前路径 `openspec/changes/cpptlm-cep-umbrella/...` 在**出处引用注释**里共 **9 处**(planner 审查只识别出 2 处 → **全仓扫描后补齐**),已全部改为 `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/...`:`test/test_cep_{canonical_invariants,contracts_crosscheck,dummy_signals,module_factory_seam,schema,simulation_mode_lock,tools_contract}.cc` + `test/cep_adr_parser.hh` + `scripts/test/cep_docs_sync_check.sh`。
  **不改**:`docs/implementation/*`(历史快照,append-only;`cep_doc_ref_check.sh:113` 的 VIRTUAL_PATHS 已把旧路径登记为**允许的历史引用**)· 归档件自身(不可变)· `cep_doc_ref_check.sh:113`(VIRTUAL_PATHS 条目)· `cep_docs_sync_check.sh:526`(glob 已同时接受新旧形态)。
- [x] **T4.8** **断言② 的绕过面复查** —— 本次在 `obj §4.0` 写入了大量状态措辞(如「已判定不触发」「未消费」);已实测 `cep_docs_sync_check.sh` rc=0,故**当前**无冲突;但若后续继续在该行加措辞,须重跑断言②(实体锚点 `umbrella` / `Stage 0` / `R10` + 旧状态词**同句**即报红)。

---

## 5. 验收

- [x] **T5.1** `ctest -R cep_poc_spike` **恰好 3 个用例全绿**(S1 / S2 / S3)—— 且**判定结论(可能为红)已按 §3.1 / §3.3 的失败含义登记为交付物**(per `design.md` D7-a:**「结论为红」≠「用例失败」**)
- [x] **T5.2** `openspec validate cpptlm-cep-poc-spike --strict` rc=0
- [x] **T5.3** 全量 `ctest` rc=0;`[cep]` 专项 rc=0;5 个 `cep_* --strict` rc=0
- [x] **T5.4** `docs_sync_check --strict` rc=0;`markdown_link_check` rc=0;`openspec validate --changes --strict` rc=0
- [x] **T5.5** **冻结面 diff = 0**(M1 冻结面成员数以 `scripts/test/cep_freeze_check.sh` 清单为准;本 change 预期**零**改动)
- [x] **T5.6** **零行为变更**:`git diff --stat` 显示生产代码改动仅限 `examples/`;`cpptlm_core` 与 23 ABI 零改动
- [x] **T5.7** **反向验证自查**:S1 / S2 / S3 各**故意破坏一次**判据 → 对应用例**必须变红**(逐个验证并记录)
- [x] **T5.8** **「结论为红」路径自查**:S1 的 4 值 `perm` 投影**预期会失败**(R-2 已命中)→ 验证该情形下用例**仍绿**且结论被登记(per `design.md` D7-a;若用例变红 → 说明红绿语义未分离,须修)
- [x] **T5.9** **delta spec 的每个 Scenario 有可执行断言**(防「声称可自动化却无自动化」)—— 新增 `test/test_cep_poc_spike_verification.cc` 逐条断言

> **实施结果(2026-10-24,per 台账 Entry #48 + #49)**:
>
> | 场景 | 产物 | 实测结论 |
> |------|------|---------|
> | **S1** | `examples/cep_poc_spike_s1.cc` | `S1_VERDICT=FAIL` · `S1_R2=TRIGGERED(state_transition[2][1]=3)` · `S1_MISMATCH_STEPS=1`(仅写路径)· `S1_WRITEBACK_OBSERVED=1` · `S1_VALIDITY_UNTIL=queue6-perm-schema-evolution` · `S1_SELFCHECK=OK` · **exit 0** |
> | **S2** | `examples/cep_poc_spike_s2.cc`(补自检) | `S2_SELFCHECK=OK` · `S2_NEG_REAL_FALLBACK=1` · 断言 **22/22** 全绿 · 结论「判据 (b) 声明层裁定被实测确认」**不变** · exit 0 |
> | **S3** | `examples/cep_poc_spike_s3.cc` | `S3_VERDICT=ACYCLIC` · 顶点 **8** / 边 **8** / 跨网边 **0** · `S3_DOWNGRADED_CYCLE=vc2<->vc3`(**从实际环格式化**,非硬编码)· `S3_CAUSE=alias-false-cycle` · `S3_REVERSE_VALIDITY=until-3rd-network-appears` · `S3_RELEASE_USE=forbidden` · `S3_SELFCHECK=OK` · exit 0 |
>
> **红绿分离已实证**:S1 的**判定结论为红**(R-2 命中)但 **exit 0** ⇒ ctest 绿;而「判据退化」(自检失败)→ **exit 1** ⇒ 回归门能区分两者。
> **T2.5 / T3.5 的「逐字一致」核对**由 `test/test_cep_poc_spike_verification.cc` **机器执行**(Scenario → 证据映射表逐条断言)⇒ 不再靠人眼。
> **ctest**:98 → **101**(新增 `cep_poc_spike_s1` / `cep_poc_spike_s3` / `cep_spike_delta_verification`)。
> ⚠️ **验证用例名刻意不含 `cep_poc_spike` 前缀**(否则 `ctest -R cep_poc_spike` 命中 4 个,违反本 change 的 spec「恰好命中 3」—— critic B3)。

> **critic 复审修复(2026-10-24,per 台账 Entry #49)**:首轮 critic 复审 **REJECT / 4 BLOCKERS**,全部修复并**逐条反向验证有牙**:
> ① **B1** S2 反向验证结构性无判别力(`b_holds_neg` 构造性恒 false → 删整段 NEG 实验自检仍 OK)→ 加 `neg_is_real_fallback`(须真拿到旧值 + 来源内存 + 不可达)→ mutant `s2_nonegexp` 现被检出。
> ② **B2** S3 `S3_DOWNGRADED_CYCLE` **硬编码** + `has2&&has3` 对错路径恒真 → 改为**从实际环格式化** + 断言「环**恰好** {vc2,vc3}」→ mutant(`return stack`)现被 needle 检出(spike exit 0 也对账红)。
> ③ **B3** `ctest -R cep_poc_spike` 实为 **4** 个(验证用例撞前缀)→ 改名 `cep_spike_delta_verification` + 对账测试加**注册数断言**。
> ④ **B4** `status: "planned"` 证据空转(三份 spike 源本无 `status`)+ 带空格可绕过 → Scenario 改写为「spike **SHALL NOT** 引入协议实例声明」+ **结构化键位**判据(`{status:` / `,status:`;不用裸串 —— 裸串会误报注释/标识符)。
> 另修 5 项 SHOULD_FIX(`mode` 缺失改判失败 / 枚举循环 `CHECK` 累积 / ctest `DEPENDS` / `<functional>` 显式包含 / 归档路径动态发现)+ 3 项 NITS(行数 / 断言数 / `== 15` Scenario 计数)。

---

## 6. 归档

- [ ] **T6.0** **critic 复审完成且 BLOCKERS = 0**(归档前置门,per 决策 C1)—— 首轮 REJECT(4 BLOCKERS)已全部修复 + 逐条反向验证有牙;**待复审 PASS 后勾选**
- [ ] **T6.1** 归档前:本 change 的 delta spec 合并进 `openspec/specs/cep-poc-spike-verification/spec.md`(**完成归档**语义 —— 非 ⏸ 暂存)
- [ ] **T6.2** `obj §4.0` 状态列 🟡 → ✅ archived;`phase-10.md` 同步;§11 台账补归档 Entry
- [ ] **T6.2b** **owner 重指向**(per 2026-10-24 四裁决):**S1-1 ②** → 队列 #4(附「装配路径调用 + 逐 (network,vc) verdict」验收);**S1-1 ③** → **改判为全局不变量**(owner = 每个协议实例 change 的放行 Scenario,**不挂**单队列行);**M-4 决策** → 队列 #3(附冻结面耦合预登记:`fabric.schema.json vc_count.maximum=4`);**S1-5 表体** → 队列 #6(依赖方向:#3 决策为其输入前置);S1-2 / S1-3 / S1-4 → 活队列行 —— 否则成为**指向已归档 change 的无主债**(per `design.md` D4.3 + `obj §4.7.3` A1)
- [ ] **T6.3** KPI slot 释放 → 并发回到 **0/3** → 队列 **#3 `cpptlm-cep-stub-cache-core`** 可启动
