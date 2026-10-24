# Spec: CEP PoC Spike 判定场景的可执行回归契约

## ADDED Requirements

### Requirement: cep-poc-spike-verification

CEP SHALL 将 **PoC Spike 的 3 个判定场景**(`S1` / `S2` / `S3`)实现为**可执行的回归契约**:每场景 SHALL 有**唯一判据**、SHALL 注册为 **ctest 用例**,且 SHALL 至少含 **1 条反向验证**。

> **定位**:契约化的是**判定判据**,不是 spike 的实现(spike 是 `examples/` 下的可丢弃单文件)。
>
> **红绿语义**:「红」有两种、语义相反,不得混用 —— 见下方「判定结论为红 与 判据实现退化 语义分离」Scenario。
>
> **消费者**:全部 Scenario 由 `test/test_cep_poc_spike_verification.cc` + `examples/cep_poc_spike_s{1,3}.cc` 的 ctest 回归门断言。

#### Scenario: 判定结论为红 与 判据实现退化 语义分离

| 情形 | ctest 用例 | 语义 |
|------|:---:|------|
| **判据实现退化**(恒真 / 漏检 / 判据本身有 bug) | 🔴 **红** | **缺陷** → 必须修判据 |
| **判定结论为红**(S1 / S3 的「失败含义」成立) | 🟢 **仍绿** | **交付物** → 结论须被登记(这是本 change 的**产出**,不是失败) |

- **WHEN** S1 或 S3 的判定结论为红(即失败含义成立)
- **THEN** 对应用例 SHALL **仍为绿**,且该结论 SHALL 被**登记为交付物**
- **AND** 仅当「判据实现退化」时用例 SHALL 变红
- **AND** 理由 SHALL 记录:`ADR-CEP-05:296` 登记「判据 2 状态 ✅ **已触发**(2026-10-22)—— `moesi_gpu` 有静默 `E→M`,判据 (b) 成立 → **4 值 `perm` 确无法表达**」→ **R-2 已命中** ⇒ S1 的**设计结局本就是「投影表达力不足」**
- **AND** 若把「结论为红」与「用例失败」焊死 → 一个**正确工作**的 spike 会让本 change 不可归档 → KPI slot 无法释放 → **级联阻塞整个 Stage 1**

#### Scenario: 3 个场景各自有唯一判据且全部注册为 ctest

- **WHEN** 读取 `openspec/specs/cep-poc-spike-verification/spec.md` 的本 Requirement
- **THEN** 恰好有 **3** 个判定场景:`S1` / `S2` / `S3`
- **AND** 每个场景 SHALL 给出**唯一判据**(不得有并列的「或」判据)
- **AND** 每个场景 SHALL 有**至少 1 条 Scenario** 陈述其判据的可执行形态
- **AND** `ctest -R cep_poc_spike` SHALL **恰好命中 3** 个用例(`cep_poc_spike_s1` / `cep_poc_spike_s2` / `cep_poc_spike_s3`)

#### Scenario: S1 —— 单域基线(`perm` 投影 vs 6 态机推导)

- **WHEN** S1 场景在 D-GPU 域 1 个 L1 + `moesi_gpu` 插件(6 态 × 6 事件)上执行序列点比对
- **THEN** 判据 SHALL 为**逐序列点**比对「实际投影 `perm`」与「由 6 态机推导的期望 `perm`」
- **AND** `perm` 值域 SHALL 维持 **4 值**(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`)—— 判据 SHALL **不**涉及协议插件的私有瞬态态 `T`(per `INV-CEP-05-IC1`:对外只暴露 4 个稳定态)
- **AND** **若**实际投影与期望**不一致**(即 4 值 `perm` 无法表达 6 态机的转移)→ 该不一致 SHALL 被**记为 S1 的判定结论**(交付物),**用例本身 SHALL 仍为绿**;仅当「判据实现退化」(比对恒真 / 漏检)时用例 SHALL 变红
- **AND** 该结论 SHALL 关联到 `ADR-CEP-05 §7` 判据 2 的状态(per `ADR-CEP-05:296`:该判据 2 已于 2026-10-22 判定「已触发」)

#### Scenario: S1 的反向验证 —— 判据退化必须变红

- **WHEN** S1 的期望 `perm` 序列被**故意改错一项**
- **THEN** S1 SHALL 报出**不一致**(该用例变红)
- **AND** 若 S1 在期望序列被改错后**仍通过** → 判据实现**退化为恒真** → 本 Scenario 失败

#### Scenario: S2 —— 跨域可见写(判据 (b) 的可执行判定)

- **WHEN** S2 场景执行跨域可见写序列
- **THEN** 判据 SHALL 为「跨域可见写**实际发生**」的可观测判定(per `ADR-CEP-13` `INV-CEP-13-IC2` 判据 **(b)**)
- **AND** S2 SHALL **不**依赖 A6 的结论 —— 它**正是产出**该结论的场景(per 规格 §1.2:「A6 需要 (b) 的评估 → (b) 可由 S2 观测 → S2 需要 PoC 实现 → 死锁」→ S2 必须可先行)

#### Scenario: S3 —— 死锁自由(per-network 顶点下的无环判据)

- **WHEN** S3 在 **2 网络 × 4 VC**(`compute_noc` + `ualink_fabric`)、**per-network** 绑定下跑环检测
- **THEN** 通道依赖图的顶点 SHALL 为 **`(协议实例, network, vc_id)`**
- **AND** 依赖图 SHALL 按 **per-network** 绑定构建(不得把跨网依赖错误地折叠进单个图)
- **AND** 依赖图在该顶点集上 SHALL **无环**(即 `ADR-CEP-09` 的无环判据在顶点升级后**仍成立**,而非退化为全局图)
- **AND** 无环结论 SHALL **逐 `(network, vc)` 标注**,不得只给全局结论(per `ADR-CEP-09:223-224`「verdict 必须逐 (network, vc) 标注」)

#### Scenario: S3 的顶点集与依赖边集 SHALL **恰好**等于机器可导出的集合

- **WHEN** S3 构造通道依赖图
- **THEN** 顶点集 SHALL **恰好**为 `∪_{n ∈ fabric.json.networks} {(协议实例, n, vc) : vc ∈ vc_binding[n].values()}`(**8** 个:每网 4 VC × 2 网)—— 多一个或少一个 SHALL 判失败
- **AND** 依赖边集 SHALL **恰好**为 ArchForge `32 号 §6.0.3` 的**逐网络**边表并集(**8** 条),**跨网边数 SHALL 为 0**
- **AND** 判据 SHALL **不**以「与 ADR 散文逐字相等」作为比较对象(per 仓库纪律:散文契约不作机器判据,机器面才是)

#### Scenario: S3 降级后报环的**成因标注**(不得表述为「真实跨网依赖」)

- **WHEN** S3 把顶点**降级**回 `(协议实例, vc_id)` 重建依赖图并跑环检测
- **THEN** 环检测器 SHALL 检出 **2↔3** 环(由同一张逐网络边表扁平化推导)
- **AND** 该环的成因 SHALL 被标注为「**网络别名造成的假环**」—— 两网共享 `vc_id` 值域 0-3,扁平化把**网内**边误并为跨网边(per `ADR-CEP-09:223-224`「合并边既可能制造**假环**,也可能掩盖真环」)
- **AND** 该结论 SHALL **不**表述为「per-network 顶点下存在真实的跨网依赖」—— 按权威边表**跨网边为 0**,该表述**不成立**;沿用它会把**错误因果**归档进 main specs

#### Scenario: S3 反向断言的有效期止于第 3 张网出现之前

- **WHEN** 登记「降级后必须报环」这条反向断言的有效期
- **THEN** 该断言 SHALL 标注其**有效期边界**:一旦出现**第 3 张网**,扁平化可能**掩盖真环** → 「降级后必须报环」可能**不再成立**(届时 SHALL 重新设计该断言,否则恒红)

#### Scenario: S3 的反向验证 —— 顶点降级必须报出环(「有牙」保证)

- **WHEN** 把 S3 的顶点**降级**回 `(协议实例, vc_id)` 重建同一依赖图并跑环检测
- **THEN** 环检测器 SHALL **报出环**
- **AND** 若降级后**仍报无环** → 该检测器**退化为恒真** → 本 Scenario 失败
- **AND** **只跑正向的 S3 等于没跑** —— 反向验证 SHALL 与正向验证**同等必需**

#### Scenario: 每场景的反向验证不得被删除

- **WHEN** 契约测试枚举本 Requirement 的全部 Scenario
- **THEN** `S1` / `S2` / `S3` 三场景 SHALL 各至少对应 **1 条**含「退化 / 改错 / 降级 ⇒ 必须变红」语义的反向 Scenario
- **AND** 反向 Scenario 缺失 SHALL 判**失败**(不得以「实现简单」为由豁免)

#### Scenario: spike 的判定结论 SHALL **不得**用作协议实例的放行依据

- **WHEN** 消费本 capability 产出的任何结论
- **THEN** 该结论 SHALL **不**被用作「放行任何协议实例」的依据 —— `ADR-CEP-09:221` 的放行禁令要求的是「图顶点按 `(协议实例, network, vc_id)` **建立**」(装配期校验器),而 spike 是**可丢弃实验床**,不产出装配期校验器
- **AND** 该结论 SHALL **不**被当作 conformance 证据(per 规格 §1.2 硬边界 2)

#### Scenario: spike 内引用的协议实例声明 SHALL 保持 `status: "planned"`

- **WHEN** spike 复制 / 引用 `protocol_instance` 声明
- **THEN** 该副本的 `status` SHALL 为 **`planned`**(两份上游真实声明亦为 `planned`)
- **AND** 若写成 `implemented` → SHALL 判失败 —— 否则 `ADR-CEP-12` 的「仅 `implemented` 计入 adapter 实例数」计数口径被击穿,且 `S1-2` 的触发条件(首个 `implemented` 实例装配后)会被**虚假触发**

#### Scenario: spike 的硬边界(可丢弃 / 零公共影响)

- **WHEN** 契约测试检查本 capability 所对应实现的生产影响面
- **THEN** spike 代码 SHALL 仅存在于 `examples/`(单文件、可丢弃)
- **AND** `cpptlm_core` 与 **23 ABI** SHALL **零改动**
- **AND** spike SHALL **不**进 `ModuleFactory`、SHALL **不**注册 ABI、SHALL **无**公共 API
- **AND** spike SHALL **不**引入新依赖(仅用仓内既有头 + 标准库)
