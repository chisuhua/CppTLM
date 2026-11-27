## ADDED Requirements

### Requirement: cep-ssp-interpreter

CEP SHALL 为用户自定义的 SSP 声明面提供 **functional-only 解释器**：把声明表跑成**状态迁移 + perm 投影**，⛔ **零时序**（不含 hop / 消息级延迟 / 链路时延）。

#### Scenario: 声明表 → 状态迁移 + perm 投影（零时序）

- **WHEN** 解释器读 `include/tlm/cep/ssps.json` 的某条 SSP 并对其事件序列求值
- **THEN** 每步 SHALL 产出该 SSP 声明的**目标状态**与**状态投影**（per `transition` 的 `basis` 语义）
- **AND** 每步 SHALL 产出该步的 **perm 投影**（4 值锁 `none` / `read` / `unique` / `writeback` · `CEN-Ib11`）
- **AND** ⛔ 解释器 SHALL **不**产出任何**时长 / 延迟 / hop 数**（⛔ 时序面属 S2 `ADR-CEP-25` · ⛔ 本档**不得**被表述为「协议语义已真实链路验证」）

#### Scenario: 洞命中 ⇒ 显式违例计数 + 该次实验判「不可判」

- **WHEN** 解释器求值时事件的**目标状态**落在声明表的 `0xFF` 空洞（运行期事件落洞）
- **THEN** 解释器 SHALL **显式计数**该次违例（⛔ **不得**静默跳过 · ⛔ **不得** panic）
- **AND** 承载该次运行的实验 SHALL 判「**不可判**」（⛔ 不得把该次实验记为「一致」或「通过」）
- **AND** ⛔ 「不可判 / 不空转」SHALL NOT 被表述为「已覆盖 / 已证明」

#### Scenario: 瞬态态零泄漏到 Stub 面

- **WHEN** 解释器求值过程中持有瞬态态 / 协议私有状态
- **THEN** 该状态 SHALL **只**住在插件内部（`INV-CEP-05-IC1` 回归）
- **AND** ⛔ 该状态 SHALL NOT 出现在 Stub 暴露面（⛔ 不泄漏到 `perm` / 状态投影 / 任何对外可见字段）

### Requirement: cep-ssp-experiment

用户自定义协议 SHALL 能产出 `ADR-CEP-06` 的**实验 4 件套**，且其证据**声明档位**。

#### Scenario: 4 件套在 SSP 协议上齐备

- **WHEN** 对一条用户自定义 SSP 协议跑 `ADR-CEP-06` 的实验流程
- **THEN** SHALL 产出 `simulation_config.json` + `trace.h5` + `metrics.json` + `report.html` **四件齐备**
- **AND** 四件 SHALL 描述**同一次**运行（⛔ 不得跨运行拼接）

#### Scenario: 实验证据 SHALL 声明档位

- **WHEN** 产出该次实验的证据
- **THEN** 证据 SHALL 显式声明 `verification_mode` + `verification_scope`（`ADR-CEP-20` 决策 6）
- **AND** ⛔ **档 0**（仅形态 / 引用闭环）的活性证据 SHALL **不得**冒充满档
- **AND** ⛔ **跨档聚合** SHALL 被禁止（不同档的观测 SHALL NOT 合并为单一结论）

#### Scenario: SSP 协议 SHALL 过合同测试体系

- **WHEN** 校验一条用户自定义 SSP 协议的可验证性
- **THEN** 它 SHALL 满足 `ADR-CEP-14` 的 **C1~C10** 合同测试体系（per Stub 可验证性合同）
- **AND** ⛔ 「形态 + 引用闭环」的通过 SHALL NOT 被表述为「语义已验证」（活性 / 无死锁 / 一致性 / 兼容性 **均未**由本档覆盖）
