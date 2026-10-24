# Tasks — cpptlm-cep-poc-spike

> **状态**: 🟡 **Proposed**(2026-10-24 建目录)
> **队列**: `obj-cep-platform §4.0` 队列 **#2** · 前序 #1 已 ✅ archived → KPI 并发 0/3 → 本 change 占 1 slot
> **落地形态**: 正式 openspec change + delta capability `cep-poc-spike-verification`(per `design.md` D1)
> **权威规格**: `docs/implementation/2026-10-22-cep-poc-spike-spec.md`

---

## 0. 落地与登记

- [ ] **T0.1** 建 `openspec/changes/cpptlm-cep-poc-spike/` 4 件套(proposal / design / specs / tasks)+ `.openspec.yaml` ✅ **本 commit 已完成**
- [ ] **T0.2** `openspec validate cpptlm-cep-poc-spike --strict` rc=0
- [ ] **T0.3** `obj-cep-platform §4.0` 状态列 ⏸ → 🟡;`phase-10.md` 对应子任务行同步;§11 台账补 Entry(登记落地形态裁决 + 承 S1-1~S1-5)
- [ ] **T0.4** `openspec list --json` 实测 active 数 == 1(本 change 占用 1 slot,≤ 3)

---

## 1. S2 —— 跨域可见写(**已交付,继承**)

> ✅ **S2 已实跑通过**(2026-10-22,per 台账 Entry #16;初版 RUN 被 critic 3 BLOCKERS 证伪后修正重跑)· 已注册 ctest 回归门(现 ctest #97)· **交付「判据 (b) 成立」**

- [x] **T1.1** S2 spike 实现(`examples/cep_poc_spike_s2.cc`)✅ 2026-10-22
- [x] **T1.2** S2 注册 ctest 用例 ✅ 2026-10-22
- [ ] **T1.3** **S2 的反向「有牙」验证** —— 规格 §3.3 的「只跑正向等于没跑」纪律**同样适用于 S2**(契约 Scenario「每场景的反向验证不得被删除」要求 3 场景各有 1 条)→ **补齐 S2 的反向验证**
- [ ] **T1.4** S2 的判据结论**登记进 delta spec**(✅ 已在本 change 的 delta spec 中)→ 实现时核对 `examples/cep_poc_spike_s2.cc` 的实际判据与 spec 措辞**逐字一致**

---

## 2. S1 —— 单域基线(`perm` 投影不变量)

> 判据(per 规格 §3.1):**逐序列点**比对「实际投影 `perm`」与「由 6 态机推导的期望 `perm`」;只判 `perm` 投影,**不判**协议插件私有瞬态态 `T`(per `design.md` D5)

- [ ] **T2.1** 实现 `examples/cep_poc_spike_s1.cc`:D-GPU 域 1 个 L1(4 值 `perm` 投影)+ `moesi_gpu` 插件(6 态 × 6 事件)
- [ ] **T2.2** 实现 S1 的**正向**判据:逐序列点比对实际 `perm` 投影 vs 6 态机推导的期望 `perm`;任一不一致 → 失败退出
- [ ] **T2.3** 实现 S1 的**反向「有牙」验证**(per `design.md` D3):把期望 `perm` 序列**故意改错一项** → 判据**必须**报出不一致(否则该实现恒真 → 报红)
- [ ] **T2.4** 注册 ctest 用例 `cep_poc_spike_s1`(`examples/CMakeLists.txt`,与 S2 同形)
- [ ] **T2.5** 核对 S1 判据与 delta spec 的 Scenario 措辞**逐字一致**

---

## 3. S3 —— 死锁自由(per-network 顶点下的无环判据)

> 判据(per 规格 §3.3):2 网络 × 4 VC(`compute_noc` + `ualink_fabric`)· **per-network** 绑定 · 顶点 = **`(协议实例, network, vc_id)`** · 依赖图在该顶点集上**无环**

- [ ] **T3.1** 实现 `examples/cep_poc_spike_s3.cc`:2 网络 × 4 VC + per-network 绑定 + 通道依赖图构建(顶点 = `(协议实例, network, vc_id)`)
- [ ] **T3.2** 实现 S3 的**正向**判据:在**升级顶点**上跑环检测 → 期望**无环**
- [ ] **T3.3** 实现 S3 的**反向「有牙」验证**(per 规格 §3.3 **强制**):把顶点**降级**回 `(协议实例, vc_id)` 重建同一图 → 环检测器**必须报出环**;若仍报无环 → 该检测器恒真 → **报红**
- [ ] **T3.4** 注册 ctest 用例 `cep_poc_spike_s3`(`examples/CMakeLists.txt`)
- [ ] **T3.5** 核对 S3 判据与 delta spec 的 Scenario 措辞**逐字一致**

---

## 4. S1-1 ~ S1-5 —— 装配期前置(自 `umbrella/tasks.md:406-414` 承接)

> ⚠️ **纪律**(承 `umbrella/tasks.md:407`):**未消费前不得下线** —— provisional **不许变永久债**。
> ⚠️ **不得为「表看起来干净」而勾选未到消费时机的事项**(per `design.md` D4 反例)。

| # | 条目 | 消费时机 / 触发条件 | 本 change 内 |
|---|------|--------------------|:---:|
| **S1-1** | `ADR-CEP-09` 的通道依赖图顶点按 **`(协议实例, network, vc_id)`** 建立 —— 在此之前不得用其无环判据放行任何协议实例 | **可立即**,但属 **`ADR-CEP-09` 修订(冻结面)** → 需走 `cep_freeze_check.sh --update --authorized-by "Entry #N"` 两步流程 → **不在本 change 内改 ADR** | ⛔(登记 + 挂授权) |
| **S1-2** | N-1 / N-2 的**实现侧复核** —— 复核 per-network 嵌套形态 | 首个 `implemented` 实例**装配后** | ⛔ |
| **S1-3** | SLICC 式「协议本地 vnet + 装配期绑定」备选的重评 | 出现**第 3 张网**或**第 3 个协议实例** | ⛔ |
| **S1-4** | `include/tlm/cep/fabric.{schema.json,json}` 在首个 `implemented` 实例跑过后**再纳 M1 冻结集**(零实例新产物不诞生即冻结) | 首个 `implemented` 实例**跑过后** | ⛔ |
| **S1-5** | **adapter 消息类 → `CATEGORY_*` 的显式映射表(非恒等)** —— 6 类一致性消息类落在 Coherence Interface 层,`CATEGORY_*` 保持**链路层 3 类**不变 | **可立即**(契约层设计) | ✅ |

- [ ] **T4.0** 5 项**逐项**在本表登记(条目 + 消费时机)—— ✅ **本 commit 已完成登记**
- [ ] **T4.1** **S1-5** —— 落 `adapter 消息类 → CATEGORY_*` 的**显式映射表**(非恒等)到契约层(Coherence Interface 层);`CATEGORY_*` 维持**链路层 3 类**
- [ ] **T4.2** **S1-1** —— 登记为**待授权的 ADR 修订项**(不改正文):`ADR-CEP-09` 顶点定义变更走冻结面授权闸;本 change 内只**登记**(`design.md` Q1)
- [ ] **T4.3** **S1-2 / S1-3 / S1-4** —— 保持 ⛔ 未勾选,并在 `obj §4.0` 队列表末列持续显示「承接 S1-1~S1-5」→ 每次 Stage 1 排期可见

---

## 5. 验收

- [ ] **T5.1** `ctest -R cep_poc_spike` **恰好 3 个用例全绿**(S1 / S2 / S3)
- [ ] **T5.2** `openspec validate cpptlm-cep-poc-spike --strict` rc=0
- [ ] **T5.3** 全量 `ctest` rc=0;`[cep]` 专项 rc=0;5 个 `cep_* --strict` rc=0
- [ ] **T5.4** `docs_sync_check --strict` rc=0;`markdown_link_check` rc=0;`openspec validate --changes --strict` rc=0
- [ ] **T5.5** **冻结面 diff = 0**(M1 冻结面 30 成员;本 change 预期**零**改动)
- [ ] **T5.6** **零行为变更**:`git diff --stat` 显示生产代码改动仅限 `examples/`;`cpptlm_core` 与 23 ABI 零改动
- [ ] **T5.7** **反向验证自查**:S1 / S2 / S3 各**故意破坏一次**判据 → 对应用例**必须变红**(逐个验证并记录)

---

## 6. 归档

- [ ] **T6.1** 归档前:本 change 的 delta spec 合并进 `openspec/specs/cep-poc-spike-verification/spec.md`(**完成归档**语义 —— 非 ⏸ 暂存)
- [ ] **T6.2** `obj §4.0` 状态列 🟡 → ✅ archived;`phase-10.md` 同步;§11 台账补归档 Entry
- [ ] **T6.3** KPI slot 释放 → 并发回到 **0/3** → 队列 **#3 `cpptlm-cep-stub-cache-core`** 可启动
