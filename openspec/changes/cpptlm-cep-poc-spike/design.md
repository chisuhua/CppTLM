# Design — cpptlm-cep-poc-spike v1.0

> **日期**: 2026-10-24 · **作者**: CppTLM build agent(builder mode)
> **关联**: `proposal.md`(同目录)· 用例规格 `docs/implementation/2026-10-22-cep-poc-spike-spec.md`

---

## Context

### 现状

| 项 | 事实 |
|---|---|
| **S2** | ✅ 已实跑通过(2026-10-22,per 台账 Entry #16)· 已注册 ctest 回归门(现 #97)· 交付「判据 (b) 成立」 |
| **S1 / S3** | ⏳ 待实现(per 规格头部状态行) |
| **change 目录** | 2026-10-23 前**不存在**(per 台账 Entry #22)→ **落地形态待 DRI 裁决** |
| **S1-1 ~ S1-5** | 从 `umbrella/tasks.md` 移出的 **5 项装配期前置**,归属本 change(per `obj §4.0` 队列 #2 末列) |
| **A6 死锁** | 已破 —— 规格 §1.2:「A6 需要 (b) 的评估 → (b) 可由 S2 观测 → S2 需要 PoC 实现 → 死锁」→ **S2 必须可先行**;S2 已交付 → **S1 / S3 现在可做** |

### 目标态

3 个判定场景(S1 / S2 / S3)全部**可执行 + 已注册回归门**,其判据固化为 delta capability `cep-poc-spike-verification` → 「Stage 1 的准入判据」从此是**系统契约**,不是某次会话的结论。

### 关键约束

| # | 约束 | 来源 |
|---|------|------|
| C1 | **单文件可丢弃 spike** —— 不进 `ModuleFactory` / 不注册 ABI / 无公共 API / 不是实现 / 不是 conformance test / 不是架构变更 | 规格 §1.2 硬边界(4 条,不可省略) |
| C2 | **每场景至少一条反向「有牙」验证** —— 只跑正向等于没跑 | 规格 §3.3 明文警告 + 本仓反向验证文化(`cep_decl_reverse_verify.sh` 的 12 条反向断言) |
| C3 | **串行 KPI**:任一时刻 Proposed ≤ 1;本 change 占用队列 #2 的 slot | `ADR-CEP-00 §2 Decision 3` + `obj §5.4` |
| C4 | **S1-1 ~ S1-5 未消费前不得下线**(provisional 不许变永久债) | `umbrella/tasks.md:407` |
| C5 | **零冻结面改动** | M1 冻结面 30 成员全为 `.md`/`.json`/`.py` |

---

## Goals / Non-Goals

### Goals

- **G1** S1 / S3 两个场景 spike 落地,**判据可执行**且**注册为 ctest 回归门**
- **G2** 3 个场景的判据固化为 **delta capability**(`cep-poc-spike-verification`)→ 归档后进 main specs,成为系统契约
- **G3** **S1-1 ~ S1-5** 在本 change 内**可寻址**(逐项带消费时机 / 触发条件)
- **G4** 消除「Stage 1 准入判据只能靠纸面辩论」的状态

### Non-Goals

- **NG1** ❌ 不实现 Stage 1 的任何协议插件 / stub cache / fabric(那是队列 #3 ~ #8)
- **NG2** ❌ 不改任何既有契约(per C1:「不是架构变更」)—— 只**新增**一个能力
- **NG3** ❌ 不进 `ModuleFactory` / 不注册 ABI / 不暴露公共 API
- **NG4** ❌ 不在本 change 内**消费完** S1-2 / S1-3 / S1-4(依赖「首个 `implemented` 实例」,见 D4)
- **NG5** ❌ 不引入新依赖

---

## Decisions

### D1:落地形态 = **正式 openspec change + 1 个 delta capability**

**决策**:建立 `openspec/changes/cpptlm-cep-poc-spike/`(4 件套),其 delta spec 新增 capability **`cep-poc-spike-verification`**。

**理由**(裁决「落地形态待 DRI 裁决」):

| 备选 | 评价 |
|------|------|
| **(a) 只留 `examples/` 里的 spike,不开 change** | ❌ 判据仍是「某次会话的产物」—— 无契约、无 tasks、无归档语义;S1-1~S1-5 无处安放;队列 #2 的「承接」承诺落空 |
| **(b) 正式 change,但**无 delta spec**(纯实现型 change) | ❌ 归档时**无 delta 可合并** → 判定判据留在 change 目录里,归档即失联;与 umbrella 的做法不一致 |
| **(c) 正式 change + delta capability** ✅ | ✅ 判据进 **main specs** → 归档后仍是**系统契约**;S1-1~S1-5 有 tasks.md 承载;与 `umbrella` 的「契约层 / 实现层分离」风格一致 |

**关键洞察**:spike 本身「零生产影响」,但它**产出的判据**有长期价值 —— 那就把**判据**而不是**代码**做成契约。代码留在 `examples/`(可丢弃),契约进 main specs(不可丢)。

**代价**:需要维护一份 delta spec(3 场景 × 判据 × 反向验证)。**接受** —— 否则「Stage 1 准入判据靠纸面辩论」的现状原样保留。

### D2:S1 / S3 各自**独立单文件**,与 S2 同形

**决策**:`examples/cep_poc_spike_s1.cc` / `cep_poc_spike_s3.cc`,各自 `main()` + 退出码即判定。

**理由**:S2 已确立先例(`examples/cep_poc_spike_s2.cc` + `examples/CMakeLists.txt` 的 `add_test`)。**三场景同形** → 读任一个都能立刻明白「spike 是什么形态」;退出码即判定 → ctest 直接用。

**排除**:合成一个 `cep_poc_spike` 可执行文件跑 3 场景 —— ❌ 会让「某场景失败」变成「整个文件失败」,定位成本上升,且偏离 S2 已验证的形态。

### D3:判据的**反向「有牙」**验证是**硬要求**(每场景 ≥ 1 条)

**决策**:S1 / S3 各**必须**含至少一条「若判据实现退化 → 该用例必须变红」的反向断言。

**S3 的强制形态**(per 规格 §3.3):把顶点**降级**回 `(协议实例, vc_id)` 重建依赖图 → 环检测器**必须报出环**。只跑正向的 S3 = 没跑。

**S1 的形态**:把期望 `perm` 序列**故意改错一项** → 逐序列点比对**必须报出不一致**。

**理由**:本仓已有「反向验证」文化(`scripts/test/cep_decl_reverse_verify.sh` 的 12 条反向断言)。**没有反向验证的判定器 = 恒绿** → 等于没跑。

### D4:S1-1 ~ S1-5 **携带 + 显式消费时机**,不强行本 change 内消费完

**决策**:5 项**全部**在本 change 的 `tasks.md` 登记,**逐项**写明「消费时机 / 触发条件」;其中 **S1-2 / S1-3 / S1-4** 明确标注为**待后续实例触发**(不在本 change 勾选)。

| 项 | 消费时机 | 本 change 内可勾选? |
|---|---------|:---:|
| **S1-1** `ADR-CEP-09` 通道依赖图顶点按 `(协议实例, network, vc_id)` 建立 | **可立即** —— 但它是对 `ADR-CEP-09` 的**修订**(属冻结面)→ 本 change 标记「需 ADR 修订授权」,**不在本 change 内改 ADR** | ⛔(需另行走 ADR 修订) |
| **S1-2** N-1 / N-2 的实现侧复核 | 首个 `implemented` 实例装配**后** | ⛔ |
| **S1-3** SLICC 式「协议本地 vnet + 装配期绑定」备选重评 | 出现**第 3 张网**或**第 3 个协议实例**时 | ⛔ |
| **S1-4** `fabric.{schema.json,json}` 在首个 `implemented` 实例跑过后再纳 M1 冻结集 | 首个 `implemented` 实例跑过**后** | ⛔ |
| **S1-5** adapter 消息类 → `CATEGORY_*` 显式映射表(非恒等) | **可立即**(契约层设计,落在 Coherence Interface 层) | ✅(可勾选) |

**理由**:per `umbrella/tasks.md:407`「**未消费前不得下线**」—— 纪律是「**携带 + 不遗忘**」,不是「**必须在本 change 内做完**」。**强行在本 change 内消费 S1-2/S1-3/S1-4 会造假**(它们客观上还没到消费时机)。

**反例(必须避免)**:在 tasks.md 里写「S1-2 ✅ 已完成」以让表看起来干净 → **把 provisional 变成永久债的假完成**,正是 umbrella 明令禁止的。

### D5:S1 判定**只比 `perm` 投影**,不判定协议插件内部瞬态

**决策**:S1 的比对对象是「实际投影 `perm`」与「由 6 态机推导的期望 `perm`」,**逐序列点**比对。

**理由**:per 规格 §3.1 —— Stub Cache 对外**只暴露 4 个稳定态**,协议插件私有瞬态态 `T` **不泄漏**(`INV-CEP-05-IC1`)。判 `perm` 投影即覆盖该不变量;判内部瞬态则是越界(且会因实现细节漂移而误报)。

### D6:不引入新依赖;spike 只用仓内既有头

**决策**:S1 / S3 的 spike **只用** `cpptlm_core` + 标准库(与 S2 同)。

**理由**:per umbrella 的 `D4`(3 个 tools 用纯标准库,零项目内依赖)与 spike 的「可丢弃」定位 —— 引入依赖会让「可丢弃」名不副实。

---

## Risks / Trade-offs

| # | 风险 / 取舍 | 缓解 / 状态 |
|---|------------|-----------|
| R1 | **delta spec 的维护成本**(3 场景 × 判据 × 反向验证) | **接受** —— 不固化则判据随归档失联(D1 已论证) |
| R2 | **S1 / S3 判据可能过严** → spike 长期红 | 判据来自**已归档的规格文档** §3.1 / §3.3,不是新发明;若过严 → 改规格并记 Entry,不改判据迁就实现 |
| R3 | **S1-2/S1-3/S1-4 长期未消费** → 永久债 | 每项在 tasks.md 带**触发条件**;`obj §4.0` 队列表末列持续显示「承接 S1-1~S1-5」→ 每次 Stage 1 排期都被看见 |
| R4 | **spike 被误当成 Stage 1 实现** | C1 硬边界 + Non-Goals + 文件头「可丢弃」+ capability 措辞「判定场景」而非「实现」 |
| R5 | **KPI 并发**:本 change 占 1 slot | 0/3 → 1/3;队列 #3 ~ #8 保持 📋 Planned(串行纪律) |

---

## Open Questions

| # | 问题 | 归属 / 触发 |
|---|------|------------|
| Q1 | **S1-1** 需修订 `ADR-CEP-09`(冻结面)—— 何时走 ADR 修订授权? | **DRI 裁决**:本 change 内标记 + 登记,实际修订走 `cep_freeze_check.sh --update --authorized-by "Entry #N"` 两步流程 |
| Q2 | S1 / S3 完成后,是否需要**独立评审**? | 承 R10 残余风险:`ADR-CEP-00 §8.1` 的「结论性独立」在单人仓结构性缺失 → 按 `obj §9.3` 的闭合判据处置(落地证据 + 残余风险登记 + DRI 书面接受) |
