# cep-collective-plane-contract Specification(delta)

## Purpose

**新增** Requirement `cep-collective-plane-contract-shared-mode-certificate`:**M-2(共享 VC + 证书)** 的
可执行判据面 —— **V6-d** 与其**四条证书**(C-1′ / C-2 / C-3 / C-4)的**逐条**可判形式。

> **口径**:`ADR-CEP-01` SU 2026-11-03(#4)的 **E.3**(四条证书 + 归因 id 纪律)· **E.4**(⛔ 不得以「重跑 DAG」当证书)
> · **E.6**(取代 D.2 / D.4 的规范句:`status` 值域 = **2 值**;`shared_no_isolation` ⇒ V6-c **不适用**)
> · **E.7**(代价登记)—— 台账 Entry **#193**(三则 ADR SU + 冻结面授权)· **#203**(规约层归档)。
>
> **既有 Requirement 逐字不变**:本 delta **只** ADDED ⇒ 冻结面的实质 diff = **新增 1 个 Requirement**。
> **归因 id 不新增规则族**:V6-d **折进 `CP-03` 行**(以 `detail` **精确前缀**表达)⇒ `kContractRuleCount == 6` **不变**。

---

## ADDED Requirements

### Requirement: cep-collective-plane-contract-shared-mode-certificate

CEP SHALL 为 **M-2(共享 VC + 证书)**提供**可执行判据面**:`status` 第二取值 + **V6-d** +
四条证书的**逐条**可判形式。

#### Scenario: **`status` 取值域 SHALL 为 2 值**,空预留的声明 SHALL 支持**共享模式**

- **WHEN** 加载 `collective_planes.json` 的 `vc_reservation_declaration`
- **THEN** `status` SHALL 恰为 **2 值**之一:`"unavailable"` \| `"shared_no_isolation"`
      ⇒ 其余取值 SHALL 判 **FAIL**(⛔ 不得「见招拆招」地接受任意字符串)
- **AND** `status == "shared_no_isolation"` 时 SHALL **要求** `certificate_scope` **非空**
      (每项含 `plane` + `network`;`network` SHALL 在 `fabric.json` 的 `networks` 内登记)
      ⇒ 缺失 / 为空 / network 未登记 ⇒ SHALL 判 **FAIL**
- **AND** `status == "unavailable"` 时 SHALL **不适用** V6-d(改由 V6-c 守门)⇒ 判定 SHALL 为 **SKIP**(⛔ 禁 vacuous PASS)
- **AND** **正控**:空预留 + `unavailable` 四字段齐全 ⇒ PASS;**反向 fixture**:`status` 改成第三个字符串 ⇒ 必红(归因 `UIA-05`)

> **锚点**:`test_cep_ualink_inc.cc` 的 `status` 值域断言 + `certificate_scope` 校验断言 + 反向 fixture

#### Scenario: **V6-d 共享模式证书 SHALL 可执行**(C-1′ / C-2 / C-3 / C-4 全绿)

> **定位**:per `ADR-CEP-01` SU#4 **E.6**(取代 D.4 的 V6-a 行)· **E.3.1**(四条判据 + 归因 id 纪律)。
> **载体**:`collective_planes.json`(声明面)+ `collective_plane_contract.hh`(判定面)—— 与行为面**共享**同一判据实现,**禁双实现**。
> **作用域**:证书是**逐 `(plane, network)`** 的;`plane_binding` 为 `null` 的实例**不在**范围内;
> **不产生 fabric 集合流量**的 plane(如 `LocalCollectivePlane`)**不在**范围内。
> ⚠️ **声明最小集仍为四组** —— 证书适用的 `(plane, network)` 清单登记在**既有** `vc_reservation_declaration` 组内
> (字段 `certificate_scope`),⛔ **不新增**第 5 组。

- **WHEN** `status == "shared_no_isolation"` 且 `certificate_scope` 非空
- **THEN** SHALL 对 `certificate_scope` 的**每个** `(plane, network)` 逐条判定 **C-1′ / C-2 / C-3 / C-4**
      —— **任一条** FAIL ⇒ `CP-03` 行 SHALL 判 **FAIL**;四条全绿 ⇒ `CP-03` 行 SHALL 判 **PASS**
- **AND** V6-d SHALL **折进 `CP-03` 行**(⛔ **不新增** `ContractRule`;`rule=` 字段**仍为** `CP-03`)
- **AND** 失败时的 `detail` SHALL 以**精确前缀**表达是哪一条:
      `[CP-03-VC-CERTIFICATE/C-1′]` / `[CP-09-IC1]` / `[CP-03-VC-CERTIFICATE/C-3]` / `[CP-03-VC-CERTIFICATE/C-4]`
      ⇒ 归因匹配按**完整前缀精确相等**(⛔ 不用子串前缀匹配 —— 否则 `CP-03` 会命中它)
- **AND** ⛔ SHALL **不**把「重跑 `evaluate_channel_dependency_gate`」当作证书
      —— 该闸对 `vc_binding` 的**语义分配零鉴别力**(边按 network 声明 ⇒ 诱导子图恒无环 ⇒ 恒真,per `ADR-CEP-01` **E.4**)
- **AND** **反向 fixture**:逐条毒化(C-1′ / C-2 / C-3 / C-4 各一条)⇒ 断言必红且**精确**命中对应前缀

> **锚点**:`test_cep_ualink_inc.cc` 的 V6-d 四条证书断言 + 4 条反向 fixture

#### Scenario: **C-1′ 槽位语义继承** —— 继承 SHALL **良定义**

- **WHEN** 判定某 `(plane, network)` 的 **C-1′**
- **THEN** 集合平面 SHALL **不**作为协议实例出现在 `protocol_instances.json`
      (⇒ 它**没有**自己的 VC 绑定 —— 载体 = **继承**)
- **AND** 该 network 上**全部有 `vc_binding` 的**既有实例的映射 SHALL **逐类相等**
      (类集合相同 ∧ 每类映射到同一 `vc_id`)⇒ 不等 ⇒ 必红
- **AND** 该 network SHALL **至少有一个**有绑定的既有实例 ⇒ 否则继承**无主体** ⇒ SHALL 判 **FAIL**(⛔ 禁 vacuous PASS)
- **AND** **正控**:该网全部有绑定的实例映射逐类相等 ⇒ PASS;**反向 fixture**:令其中两个实例的映射不一致 ⇒ 必红
      (归因 `[CP-03-VC-CERTIFICATE/C-1′]`,**精确相等**匹配)

> **锚点**:`test_cep_ualink_inc.cc` 的 C-1′ 断言 + 反向 fixture(影子 `protocol_instances.json`)

#### Scenario: **C-2 VC 覆盖完整** —— 继承映射 SHALL **覆盖**该网 `vc_count` 全部取值

- **WHEN** 判定某 `(plane, network)` 的 **C-2**
- **THEN** 集合平面在该网上**绑定**的 `vc_id`(= 继承映射的 **vc 镜像**)SHALL **覆盖**
      `fabric.json` 该 network 的 `vc_count` **全部取值** ⇒ 只覆盖部分 ⇒ 必红
- **AND** 该判据 SHALL **直接**对 `fabric.json` 的 `vc_count` 判定(⛔ **不**依赖「边表是否引用某 VC」)
- **AND** **正控**:覆盖完整 ⇒ PASS;**反向 fixture**:把该网 `vc_count` 调大 ⇒ 必红
      (归因 `[CP-09-IC1]`,**精确相等**匹配)

> **锚点**:`test_cep_ualink_inc.cc` 的 C-2 断言 + 反向 fixture(影子 `fabric.json`)

#### Scenario: **C-3 控制面与数据面互斥** —— 声明级与运行级 SHALL **同归因**

- **WHEN** 判定某 `(plane, network)` 的 **C-3**
- **THEN** 该 plane SHALL **已登记**在 plane 注册表(单源在**定义点**)∧ SHALL 声明为**非本地模型**
      (承载真实 fabric 集合流量)∧ SHALL 声明承担 **C-3** ⇒ 任一不满足 ⇒ 必红
- **AND** **运行级**:该 plane 的**行为** SHALL 在 group 有 op **in-flight** 时**拒绝**
      `group_register_region` 与 `group_destroy` ⇒ 由调用序列 fixture 判定
- **AND** 两级 SHALL **同归因**(`[CP-03-VC-CERTIFICATE/C-3]`)⇒ **反向 fixture**(声明级 / 运行级各一条)必红且精确命中

> **锚点**:`test_cep_ualink_inc.cc` 的 C-3 声明级断言 + `FabricCollectivePlane` 的调用序列断言 + 反向 fixture

#### Scenario: **C-4 未检验项显式登记** —— 登记项 SHALL **存在、非空、且未被冒充**

- **WHEN** 判定某 `(plane, network)` 的 **C-4**
- **THEN** 未检验项登记 SHALL **存在** ∧ **非空** ∧ 每项 `ref` **非空**
- **AND** 每项 `statement` SHALL **不含**「已验证 / 已满足」字样 ⇒ 把任一未检验项标成「已验证」⇒ 必红
- **AND** 登记项 SHALL 由**规约载体**提供(⛔ **不得**在判据面**复制**一份未检验项表 —— 禁双实现)
- **AND** **正控**:U-1 / U-2 登记完整 ⇒ PASS;**反向 fixture**:把某项 `statement` 改成含「已验证」⇒ 必红
      (归因 `[CP-03-VC-CERTIFICATE/C-4]`,**精确相等**匹配)

> **锚点**:`test_cep_ualink_inc.cc` 的 C-4 断言 + 反向 fixture(影子规约载体)
