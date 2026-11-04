# cep-ualink-inc Specification

## Purpose
定义 **UALink INC** 在 CEP 平台上的**实现层可执行契约**:`UalinkAdapter`(零自有表副本 · 无转移面)·
`FabricCollectivePlane`(数据路径**真可达** + 控制面互斥)· **M-2 共享模式**的声明面接线 ·
**实例声明的收窄形态**(`ualink_4` **不**声明)。

> **口径**:`ADR-CEP-01` SU#4(**E.3** 四条证书 / **E.6** 取代文本)· `ADR-CEP-03 §2`(7 方法面 + 类型集)·
> `ADR-CEP-11 §3`(版本锁)· 台账 Entry **#203**(规约层归档)· **#194**(improvement)· **#193**(三则 ADR SU)。
>
> **边界**:⛔ **不**改 `CollectivePlaneIf` 的 **7 方法**面 · ⛔ **不新增**原语(仍恰 4 值)·
> ⛔ **不新增** reduce 算子族(唯一真源 = 既有 `ReduceOp` 恰 6 值)· ⛔ **0 新增** ABI ·
> ⛔ **0 新增** `INV-` 编号 · ⛔ **不产出任何**带宽 / 延迟结论。
>
> **合规边界**:UALink Common 2.0 的**规范正文**受 **Evaluation Copy Agreement** 约束 ⇒ **禁止**投喂
> ⇒ 锚点摘要 `N-1` ~ `N-9` 维持 `pending` ⇒ 本能力**不得**推理填空 ⇒ **不声明** `ualink_4` 实例
> (其 `flow_support` / `perm` / `sharer_tracker.impl` 取值**不可判定**,per `ADR-CEP-04` SU §③ 残余)。

---

## Requirements

### Requirement: cep-ualink-inc

CEP SHALL 为 **UALink INC** 提供一份**可执行实现层契约**:零副本的协议 adapter ·
承载真实 fabric 集合流量的 collective plane · **M-2 共享模式**的声明面接线 ·
**实例声明的收窄形态**。

#### Scenario: UIA-01 —— `UalinkAdapter` SHALL **零自有表副本**

- **WHEN** 读取 `include/tlm/cep/ualink_adapter.hh`
- **THEN** 其**去注释后**文本 SHALL **不**含规约载体 `ualink_inc_state_spec.hh` 的任何
      **成员字面量 / 表名**(类型**别名**允许 —— 别名只出现**类型**路径,不含成员访问)
- **AND** 全部判定 SHALL **委托**规约载体 ⇒ 规约改一格、本 adapter **不**必红(本 adapter **不**依赖表)
- **AND** **反向 fixture**:在 adapter 中加一行**载体成员字面量** ⇒ 断言必红(归因 `UIA-01`)

> **锚点**:`test_cep_ualink_inc.cc` 的禁项文本扫描断言 + 反向 fixture

#### Scenario: UIA-02 —— `UalinkAdapter` SHALL **不暴露** INC 原语的转移面

- **WHEN** 读取 `include/tlm/cep/ualink_adapter.hh`
- **THEN** 其文本 SHALL **不**含任何 `transition_` 标识符
- **AND** 理由:INC 四原语的**规范定义**是 **pending**(锚点摘要 `N-1`)⇒ 暴露转移面即等于**自造转移表**
- **AND** **反向 fixture**:加一个 `transition_*` 函数 ⇒ 断言必红(归因 `UIA-02`)

> **锚点**:`test_cep_ualink_inc.cc` 的 `transition_` 禁项扫描断言 + 反向 fixture

#### Scenario: UIA-03 —— `FabricCollectivePlane` 的数据路径 SHALL **真可达**

- **WHEN** 对**已注册**且 **epoch 已生效**的区域发起数据原语
- **THEN** SHALL **真执行**(⛔ **不得**返回「数据路径未实现」状态)
- **AND** ⛔ SHALL **不**以「数据路径未实现」却宣称 INC 已实现 —— 二者**必须可区分**
- **AND** **反向 fixture**:把真可达分支改回「数据路径未实现」⇒ 断言必红(归因 `UIA-03`)

> **详细行为**见 `cep-collective-plane-base` 的新增 Requirement `cep-collective-plane-base-fabric-plane`
> **锚点**:`test_cep_ualink_inc.cc` 的数据路径断言 + 反向 fixture

#### Scenario: UIA-04 —— **C-3 控制面与数据面互斥**(运行级)

- **WHEN** 某 group 有 op **in-flight**
- **THEN** `group_register_region` 与 `group_destroy` SHALL **被拒绝**且拒绝**可观测**
- **AND** **反向 fixture**:去掉 in-flight 检查 ⇒ 断言必红(归因 `UIA-04`)

> **锚点**:`test_cep_ualink_inc.cc` 的调用序列断言 + 反向 fixture

#### Scenario: UIA-05 —— 声明面的 `status` SHALL 支持 **2 值**

- **WHEN** 加载 `collective_planes.json`
- **THEN** `vc_reservation_declaration.status` SHALL ∈ {`unavailable`, `shared_no_isolation`}
- **AND** 本仓当前值 SHALL 为 **`shared_no_isolation`**(M-2)⇒ 其 `reason` SHALL **显式写明**
      「`ualink_4` 实例**尚未**声明」这一事实(⛔ 不得隐藏)
- **AND** **反向 fixture**:`status` 改成第三个字符串 ⇒ 断言必红(归因 `UIA-05`)

> **锚点**:`test_cep_ualink_inc.cc` 的 `status` 值域断言 + 反向 fixture

#### Scenario: UIA-06 —— 证书作用域 SHALL **显式登记**

- **WHEN** `status == "shared_no_isolation"`
- **THEN** 声明源 SHALL 记录证书适用的 `(plane, network)` 清单(字段 `certificate_scope`)
      ⇒ **未列入**清单者 SHALL 视为**不在** M-2 覆盖内
- **AND** 该清单 SHALL **非空**且每项 `network` 在 `fabric.json` 登记 ⇒ 否则判 **FAIL**
- **AND** **反向 fixture**:清空该清单 ⇒ 断言必红(归因 `UIA-06`)

> **锚点**:`test_cep_ualink_inc.cc` 的 `certificate_scope` 断言 + 反向 fixture

#### Scenario: UIA-07 —— `ualink_4` 实例 SHALL **不**声明**(收窄的可执行形态)

- **WHEN** 读取 `include/tlm/cep/protocol_instances.json`
- **THEN** SHALL **不**含 `id == "ualink_4"` 的实例 ⇒ 实例数保持 **7**
- **AND** 理由:`ualink_4` 的 `flow_support` / `perm` / `sharer_tracker.impl` 取值**不可判定**
      (锚点摘要 `N-8` / `N-9` **pending**;`ADR-CEP-04` SU §③ 残余「⛔ 不得预先钉死」)
      ⇒ 声明它**必然**要求给出具体值 = **推理填空**
- **AND** **反向 fixture**:加一条 `ualink_4` 实例 ⇒ 断言必红(归因 `UIA-07`)
- **AND** 该实例的声明 SHALL 作为 **backlog** 登记,触发条件 = 用户**自行**取得 Evaluation Copy
      并**明确**授权 + 走**新**的 Task 0 轮次

> **锚点**:`test_cep_ualink_inc.cc` 的实例集断言 + 反向 fixture
