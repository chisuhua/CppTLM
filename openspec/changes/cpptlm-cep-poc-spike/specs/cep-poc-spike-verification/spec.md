# Spec: CEP PoC Spike 判定场景的可执行回归契约

## ADDED Requirements

### Requirement: cep-poc-spike-verification

CEP SHALL 将 **PoC Spike 的 3 个判定场景**(`S1` 单域基线 / `S2` 跨域可见写 / `S3` 死锁自由)实现为**可执行的回归契约**:每个场景 SHALL 有**唯一判据**、SHALL 注册为 **ctest 用例**,且 SHALL 至少含 **1 条反向「有牙」验证** —— 即「若判据实现退化为恒真,则该用例必须变红」。

> **定位**:本 capability 契约化的是**判定判据**,**不是** spike 的实现(spike 是 `examples/` 下的可丢弃单文件,per `docs/implementation/2026-10-22-cep-poc-spike-spec.md` §1.2)。契约化的理由:Stage 1 多个协议插件 change 的准入判据不得停留在纸面辩论或单次会话日志里。

#### Scenario: 3 个场景各自有唯一判据且全部注册为 ctest

- **WHEN** 读取 `openspec/specs/cep-poc-spike-verification/spec.md` 的本 Requirement
- **THEN** 恰好有 **3** 个判定场景:`S1` / `S2` / `S3`
- **AND** 每个场景 SHALL 给出**唯一判据**(不得有并列的「或」判据)
- **AND** 每个场景 SHALL 有**至少 1 条 Scenario** 陈述其判据的可执行形态
- **AND** `ctest -R cep_poc_spike` SHALL **恰好命中 3** 个用例(`cep_poc_spike_s1` / `cep_poc_spike_s2` / `cep_poc_spike_s3`)

#### Scenario: S1 —— 单域基线(`perm` 投影 vs 6 态机推导)

- **WHEN** S1 场景在 D-GPU 域 1 个 L1 + `moesi_gpu` 插件(6 态 × 6 事件)上执行序列点比对
- **THEN** 判据 SHALL 为**逐序列点**比对「实际投影 `perm`」与「由 6 态机推导的期望 `perm`」
- **AND** 任一序列点不一致 SHALL 判 **S1 失败**
- **AND** `perm` 值域 SHALL 维持 **4 值**(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`)—— 判据 SHALL **不**涉及协议插件的私有瞬态态 `T`(per `INV-CEP-05-IC1`:对外只暴露 4 个稳定态)

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
- **AND** 依赖图在该顶点集上 SHALL **无环**(即 `ADR-CEP-09` 的无环判据在顶点升级后**仍成立**,而非退化为全局图)
- **AND** 依赖图 SHALL 按 **per-network** 绑定构建(不得把跨网依赖错误地折叠进单个图)

#### Scenario: S3 的反向验证 —— 顶点降级必须报出环(「有牙」保证)

- **WHEN** 把 S3 的顶点**降级**回 `(协议实例, vc_id)` 重建同一依赖图并跑环检测
- **THEN** 环检测器 SHALL **报出环**
- **AND** 若降级后**仍报无环** → 该检测器**退化为恒真** → 本 Scenario 失败
- **AND** **只跑正向的 S3 等于没跑** —— 反向验证 SHALL 与正向验证**同等必需**

#### Scenario: 每场景的反向验证不得被删除

- **WHEN** 契约测试枚举本 Requirement 的全部 Scenario
- **THEN** `S1` / `S2` / `S3` 三场景 SHALL 各至少对应 **1 条**含「退化 / 改错 / 降级 ⇒ 必须变红」语义的反向 Scenario
- **AND** 反向 Scenario 缺失 SHALL 判**失败**(不得以「实现简单」为由豁免)

#### Scenario: spike 的硬边界(可丢弃 / 零公共影响)

- **WHEN** 契约测试检查本 capability 所对应实现的生产影响面
- **THEN** spike 代码 SHALL 仅存在于 `examples/`(单文件、可丢弃)
- **AND** `cpptlm_core` 与 **23 ABI** SHALL **零改动**
- **AND** spike SHALL **不**进 `ModuleFactory`、SHALL **不**注册 ABI、SHALL **无**公共 API
- **AND** spike SHALL **不**引入新依赖(仅用仓内既有头 + 标准库)
