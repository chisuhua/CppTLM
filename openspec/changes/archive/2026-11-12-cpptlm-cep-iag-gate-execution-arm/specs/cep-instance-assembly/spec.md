# cep-instance-assembly Specification (delta)

## Purpose

CEP 的**实例级装配闸(IAG 族)**此前**只被测试侧调用** ⇒ 装配路径**不拦** ⇒ 结构违规的
拓扑 config 能装配成功。本 change 补上**执行臂**:① 装配路径臂(`ModuleFactory::instantiateAll`)
② 离线 CLI 臂(`validate_topology` 挂点),两臂**共用同一份**判据实现与加载规则。

本 capability **只约束「执行臂在位」这一形态**,⛔ **不改变任何 IAG 判据口径**
(`include/tlm/cep/instance_assembly_gate.hh` 零 diff);判据本体仍由既有 Requirement 承载。

> **演进方式**:本 spec 经由 OpenSpec delta(**MODIFIED**)演进,**不直接改写**本文件。
> **规范载体**:`include/tlm/cep/instance_assembly_gate.hh`(判据) + `src/core/module_factory.cc` Step 0.26(装配臂)
> **canonical 实现**:`examples/cep_instance_assembly_gate.cc` + `scripts/test/cep_iag_gate_reconcile_check.sh`

## MODIFIED Requirements

### Requirement: cep-instance-assembly

CEP 的实例级装配闸 SHALL **在装配路径上执行**,且其**离线挂点** SHALL 与装配路径臂
**共用同一份**判据实现与加载规则。

#### Scenario: IAG-01 —— 三层证据齐备 ⇒ 装配闭合

- **WHEN** 对含 `cep.bridges[]` 的拓扑跑装配闸
- **THEN** 每个被挂载的实例 SHALL 在声明层存在 · `family` SHALL **存在**且**可解析** · bridge 的 `modules` SHALL **非空**且逐个在拓扑中 · `bridge_id` SHALL **存在**且唯一
- **AND** 若 `plane_binding` 含该实例键且绑定**非空** ⇒ SHALL 已在 plane 注册表登记
- **AND** 若绑定为 **null / 空** ⇒ SHALL 视为「该实例不启用 collective」⇒ **不判红**;**仅当**该实例在 `required_plane_instances` 中 ⇒ SHALL 判 `[IAG-04]`

#### Scenario: IAG-02 —— 负控:未声明 / 未绑定 / 未登记 / 模块缺失 / id 重复 ⇒ 必红

- **WHEN** 分别构造:bridge 指向**未声明**实例 · 实例 `family` **缺失或不可解析** · bridge 的 `modules` **为空或引用不在拓扑**的模块 · `bridge_id` **缺失或重复** · `cep.bridges` 为空 · **required** 实例的 plane 绑定**为空或缺失** · plane 名**未登记**
- **THEN** 装配闸 SHALL 判 FAIL,且错误信息 SHALL 命中对应归因 tag(`[IAG-01]` ~ `[IAG-07]` **与 `[BSS-01]`**)
- **AND** **非 required** 实例的 plane 绑定为 null ⇒ SHALL **判 PASS**(「不启用 collective」是合法状态,⛔ 不得误判为装配失败)

#### Scenario: BSS-01 —— 跨 entry 重叠(同一模块属多个 bridge entry)⇒ 必红

- **WHEN** 构造 `cep.bridges[]` 中**两条不同 `bridge_id`** 的 entry,其 `modules` 均含同一模块名
- **THEN** 装配闸 SHALL 判红且诊断含归因 tag `[BSS-01]` 与该模块名的**出现计数**

#### Scenario: BSS-01 —— 单 entry 内重复列同一模块 ⇒ 必红

- **WHEN** 构造**单条** bridge entry,其 `modules` 列出同一模块名**两次**(如 `["w","chi","w"]`)
- **THEN** 装配闸 SHALL 判红且诊断含归因 tag `[BSS-01]`
- **AND** ⛔ 该子情形 SHALL **不得**被「跨 entry 重叠」的口径收窄排除

#### Scenario: BSS-01 —— 装配路径 SHALL 执行同一判据(执行臂)

- **WHEN** `ModuleFactory::instantiateAll()` 装配的拓扑中,同一模块名出现在多个 bridge entry 的 `modules` 展开序列中
- **THEN** 装配 SHALL **失败**(返回 false)且诊断含归因 tag `[BSS-01]`
- **AND** 装配路径与装配闸 SHALL **共用同一份判据实现**(`include/tlm/cep/bridge_module_membership.hh` —— **唯一实现**;⛔ **禁复制实现**)
- **AND** 装配路径 SHALL 在**桥挂载循环之前**执行该检查 ⇒ 挂载循环**不进入** ⇒ **零桥挂载**(桥挂载槽不被后挂的桥覆盖)
- ⚠️ **失败原子性边界**:该失败发生在对象已创建之后 ⇒ **不新增回滚机制**;⛔ **不得**表述为「零副作用」
- ⚠️ **诊断优先级**:该检查**短路** ⇒ 同一 config 若同时违反其它桥校验条件,**只报** `[BSS-01]` 一类原因 ⇒ **结构性关口先于逐条桥校验**

#### Scenario: IAG 族 —— 装配路径 SHALL 执行同一判据(执行臂)

- **WHEN** 某个 `cep.bridges[]` 指向**未声明**的实例、或引用**不在拓扑内**的模块
- **THEN** `ModuleFactory::instantiateAll()` SHALL **失败**(`return false`)
- **AND** 失败 SHALL 发生于**模块创建之前** ⇒ **零副作用**
- **AND** 诊断 SHALL 含**可精确匹配的归因 tag**(`[IAG-02]` / `[IAG-06]` 等)⇒ ⛔ 不得只以「非零退出」表达

#### Scenario: 两臂 SHALL 共用同一份判据实现与加载规则

- **WHEN** 校验装配路径臂与离线 CLI 臂
- **THEN** 两处 SHALL 均调用 `tlm::cep::run_instance_assembly_gate`
- **AND** 两处 SHALL 均调用 `tlm::cep::load_instance_assembly_inputs` ⇒ ⛔ **禁双实现**
- **AND** `include/tlm/cep/instance_assembly_gate.hh` SHALL **零 diff**(判据口径不变)

#### Scenario: 离线挂点 SHALL 真在命令面内

- **WHEN** 执行 `cmake --build build --target validate_topology`
- **THEN** 命令面 SHALL 含**全部 CEP config**(清单 == 磁盘实数)的 IAG CLI 调用
- **AND** 摘掉该段 SHALL **必红**(⛔ 不得是「挂点存在但命令面没调」)

#### Scenario: 判据 SHALL 声明其不可约残余

- **WHEN** 交付本能力
- **THEN** SHALL 显式记录:① IAG 族只覆盖**装配期结构**,不覆盖运行时一致性;
      ② 离线臂对**非 CEP config** = SKIP ⇒「离线闸全绿」不等于「所有 config 都过了 CEP 判据」;
      ③ 装配路径臂是**新增拦截点** ⇒ 可能拦住过去能装配(但结构违规)的 config
- **AND** ⛔ SHALL NOT 把「不可判 / 不空转」表述为「已覆盖 / 已证明」
