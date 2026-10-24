# cpptlm-cep-poc-spike: CEP PoC Spike —— 3 个判定场景的可执行回归契约

> **状态**: 🟡 **Proposed**(2026-10-24 建目录;落地形态按 DRI 裁决落地为「正式 change + delta capability」)
> **队列**: `obj-cep-platform §4.0` 队列 **#2**(Stage 1 / 2027-Q1)· 前序 #1 `cpptlm-cep-umbrella` 已 ✅ archived(2026-10-23)→ **KPI 并发 0/3 → 本 change 占用 1 slot**
> **关联 ADR**: `ADR-CEP-13`(判据 (b) 的**可执行**判定 = Q-3e)+ `ADR-CEP-09`(顶点升级)+ `ADR-CEP-12`(per-network VC)
> **用例规格**: `docs/implementation/2026-10-22-cep-poc-spike-spec.md`(S1 / S2 / S3 三场景的权威规格)
> **承接**: `umbrella` 移出的 **S1-1 ~ S1-5** 装配期前置(per `umbrella/tasks.md:406-414`)

---

## Why

Stage 1 的多个协议插件 change(队列 #3 ~ #8)的**准入判据至今只能靠纸面辩论**:`ADR-CEP-13` 的域可见写判据 (b) 是否成立?per-network 嵌套 VC 形态在真实路径上是否**可组合**?`ADR-CEP-09` 的无环判据在顶点升级后是否仍成立?

本 change 用**可执行证据**回答这 3 个问题,并把答案固化为**系统契约**(delta capability `cep-poc-spike-verification`)—— 而不是留在某次会话的日志里。

**它不是实现**:单文件可丢弃 spike,不进 `ModuleFactory` / 不注册 ABI / 无公共 API(per 规格 §1.2 硬边界)。

**它为什么值得单独占一个 change**:S2 实跑通过已**解除 A6 死锁**(规格 §1.2:「A6 需要 (b) 的评估 → (b) 可由 S2 观测 → S2 需要 PoC 实现 → 死锁」→ **S2 必须可先行**)。S2 的 (b) 结论已于 2026-10-22 交付(判据 (b) 成立),S1 / S3 因此**现在**可做 —— 这是**唯一**能加速其余 Stage 1 change 的项。

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `examples/cep_poc_spike_s1.cc` | **S1** 场景 spike(单域基线:`perm` 4 值投影 vs 6 态机推导的期望 `perm`) |
| `examples/cep_poc_spike_s3.cc` | **S3** 场景 spike(死锁自由:per-network 顶点下的无环判据 + **反向「有牙」**验证) |
| `openspec/specs/cep-poc-spike-verification/spec.md`(delta) | **capability `cep-poc-spike-verification`** —— 把 3 个场景的判定判据固化为**可回归的契约** |

### 已有(S2,本 change **继承**不再重做)

| 文件 | 状态 |
|------|------|
| `examples/cep_poc_spike_s2.cc` | ✅ 已实跑通过(2026-10-22,per 台账 Entry #16)· 已注册 ctest 回归门(现 ctest #97) |

### 修改

| 文件 | 改什么 |
|------|--------|
| `examples/CMakeLists.txt` | 注册 S1 / S3 两个 ctest 用例(与 S2 同形) |
| `openspec/specs/cep-poc-spike-verification/spec.md` | **新建** main spec(capability 落地) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 状态列 ⏸ → 🟡 · §11 台账补 Entry |

### 冻结面

**零改动** —— M1 冻结面 30 成员全为 `.md` / `.json` / `.py`;本 change 的代码产物是 `examples/*.cc`(不在冻结集)。

---

## Capabilities

### New Capabilities

- **`cep-poc-spike-verification`** —— CEP PoC Spike 的 3 个判定场景(S1 / S2 / S3)**必须是可执行的回归契约**:每个场景有唯一判据 + 至少一条**反向「有牙」**验证(S3 强制;S1 / S2 遵循「只跑正向等于没跑」纪律),且**注册为 ctest 用例**。

### Modified Capabilities

**无** —— 本 change 不修改任何既有契约(per 规格 §1.2:「不是 conformance test」「不是架构变更」);它**新增**一个能力,而不是修订 `cep-core-contracts`。

---

## Impact

### 影响范围

| 层 | 影响 |
|---|---|
| 生产代码 | **零** —— spike 是 `examples/` 下的可丢弃单文件 |
| 公共 API / ABI | **零** —— 不进 `ModuleFactory`、不注册 ABI(per §1.2) |
| 契约 | **新增 1 个 capability**(3 场景判据的可执行化) |
| CI | ctest 用例 **+2**(S1 / S3);S2 已存在 |
| 跨仓 | **零** —— ArchForge 侧无依赖(本仓内部可执行) |

### 受影响测试

| 测试 | 性质 |
|------|------|
| `cep_poc_spike_s2`(ctest #97) | **继承**(已绿) |
| `cep_poc_spike_s1`(新增) | 新增 |
| `cep_poc_spike_s3`(新增) | 新增(含反向验证) |

### 风险与缓解(per `obj-cep-platform §8.1`)

| 风险 | 缓解 |
|------|------|
| **spike 被误当成实现** | 硬边界写进 spec 的 Requirement + design 的 Non-Goals;spike 文件头注明「可丢弃」 |
| **只跑正向 → 恒绿**(S3 明确警告) | spec 的 Scenario **强制**每场景至少一条反向验证;反向断言失败即报红 |
| **S1-1~S1-5 永久挂账** | 每项带**消费时机 / 触发条件**;`umbrella/tasks.md:407` 的「未消费前不得下线」纪律延续到本 change 的 tasks.md |
| **KPI 并发超限** | 本 change 占用 1 slot(0/3 → 1/3);**其余 Stage 1 change 保持 📋 Planned** |

---

## Acceptance

- [ ] **AC-1** `examples/cep_poc_spike_s1.cc` 存在且实现 S1 判据(逐序列点比对「实际投影 `perm`」与「6 态机推导的期望 `perm`」)
- [ ] **AC-2** `examples/cep_poc_spike_s3.cc` 存在且实现 S3 判据(per-network 顶点 `(协议实例, network, vc_id)` 上的无环检测)
- [ ] **AC-3** **S3 的反向「有牙」验证存在**:把顶点**降级**回 `(协议实例, vc_id)` 重建依赖图 → 环检测器**必须报出环**(否则该用例恒绿 → 报红)
- [ ] **AC-4** S1 / S3 各注册为 ctest 用例(`examples/CMakeLists.txt`),`ctest -R cep_poc_spike` **全绿**
- [ ] **AC-5** delta capability `cep-poc-spike-verification` 通过 `openspec validate cpptlm-cep-poc-spike --strict`(rc=0)
- [ ] **AC-6** **S1-1 ~ S1-5** 全部在本 change 的 `tasks.md` 中**带消费时机 / 触发条件**登记(逐项可寻址;**不得**只写「见 umbrella」)
- [ ] **AC-7** `obj §4.0` 状态列 ⏸ → 🟡;`phase-10.md` 对应子任务行同步;§11 台账补 Entry
- [ ] **AC-8** 全量守门 rc=0:`ctest`(全量)· `[cep]` · 5 个 `cep_* --strict` · `docs_sync_check --strict` · `markdown_link_check` · `openspec validate --changes --strict` · 冻结面 diff=0
- [ ] **AC-9** **零行为变更**:本 change 的代码产物仅在 `examples/`;`cpptlm_core` 与 23 ABI **零改动**(以 `git diff --stat` 为证)

> **AC 口径说明**:AC-1 ~ AC-3 是**可执行判据**(不是文档检查);AC-5 ~ AC-9 是**门禁检查**。
> **AC-3 是本 change 的「有牙」保证** —— 缺它则 S3 恒绿 → 整个 change 失去判定力。
