# cep-collective-plane-contract Specification (delta)

## MODIFIED Requirements

### Requirement: cep-collective-plane-contract-shared-mode-certificate

CEP SHALL 为 **M-2(共享 VC + 证书)**提供**可执行判据面**:`status` 第二取值 + **V6-d** +
**六条证书**的**逐条**可判形式(`C-1′` / `C-2` / `C-3` / `C-4` / `R1-b` / `R1-c`)。

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

> **定位**:per `ADR-CEP-01` SU#4 **E.6**(取代 D.4 的 V6-a 行)· **E.3.1**(判据 + 归因 id 纪律)。
> ⚠️ **2026-11-12 修订**(per 台账 **Entry #344** 的 C2 立项 · `cpptlm-cep-transport-vc-consistency`):
> **六条判据** —— 既有四条(C-1′ / C-2 / C-3 / C-4)+ **新增两条**(`R1-b` 排序假设逐 VC 一致 · `R1-c` scope 完备性);
> ⛔ **不新增** `ContractRule`(`kContractRuleCount == 6` 是硬断言)⇒ 两条作为 `CP-03` 行的**子判据**。
> ⚠️ **适用域口径**:**条件化** —— 仅 `certificate_scope` **内**的 network 参与逐 VC 一致性判定;
> `compute_noc` 按 `ADR-CEP-01 §E.3.3` **不属 M-2** ⇒ **允许**不一致(per `U-3` 裁决)⇒ ⛔ **不得**把 `compute_noc` 加入 `certificate_scope`。
> ⚠️ **归因前缀**:新增两条用 `R1-b` / `R1-c` 子标记(**⛔ 不用** `C-1′-b` —— 借 `C-1′` 身份);既有 `C-1′` 前缀**逐字不动**。
> **载体**:`collective_planes.json`(声明面)+ `collective_plane_contract.hh`(判定面)—— 与行为面**共享**同一判据实现,**禁双实现**。
> **作用域**:证书是**逐 `(plane, network)`** 的;`plane_binding` 为 `null` 的实例**不在**范围内;
> **不产生 fabric 集合流量**的 plane(如 `LocalCollectivePlane`)**不在**范围内。
> ⚠️ **声明最小集仍为四组** —— 证书适用的 `(plane, network)` 清单登记在**既有** `vc_reservation_declaration` 组内
> (字段 `certificate_scope`),⛔ **不新增**第 5 组。

- **WHEN** `status == "shared_no_isolation"` 且 `certificate_scope` 非空
- **THEN** SHALL 判定**六条**证书 —— **循环内**(逐 `(plane, network)`):**C-1′ / C-2 / R1-b / C-3**;**循环后**(声明级):**R1-c / C-4**
      —— **任一条** FAIL ⇒ `CP-03` 行 SHALL 判 **FAIL**;六条全绿 ⇒ `CP-03` 行 SHALL 判 **PASS**
- **AND** `R1-c`(scope 完备性)SHALL 在 `certificate_scope` 项**循环之后、`C-4` 之前**判定
      (它是**声明级**判据 —— 遍历 `decl.plane_binding`,⛔ 不是遍历 scope 项)
      > ⚠️ **⛔ 不放循环之前**:既有 `C-3` 的反向 fixture 采用「改写 `certificate_scope[0].plane`」的变异形态 ⇒
      > 循环前会让 `R1-c` **抢归因** ⇒ 既有 `C-3` 归因断言失效 ⇒ 「循环后」是**既有 6 条反向 fixture 零改动**的硬约束
- **AND** V6-d SHALL **折进 `CP-03` 行**(⛔ **不新增** `ContractRule`;`rule=` 字段**仍为** `CP-03`)
- **AND** 失败时的 `detail` SHALL 以**精确前缀**表达是哪一条:
      `[CP-03-VC-CERTIFICATE/C-1′]` / `[CP-09-IC1]` / `[CP-03-VC-CERTIFICATE/C-3]` / `[CP-03-VC-CERTIFICATE/C-4]` /
      `[CP-03-VC-CERTIFICATE/R1-b]` / `[CP-03-VC-CERTIFICATE/R1-c]`
      ⇒ 归因匹配按**完整前缀精确相等**(⛔ 不用子串前缀匹配 —— 否则 `CP-03` 会命中它)
- **AND** ⛔ SHALL **不**把「重跑 `evaluate_channel_dependency_gate`」当作证书
      —— 该闸对 `vc_binding` 的**语义分配零鉴别力**(边按 network 声明 ⇒ 诱导子图恒无环 ⇒ 恒真,per `ADR-CEP-01` **E.4**)
- **AND** **反向 fixture**:逐条毒化 —— 每条证书**至少一条**,且
      **C-1′** 两条(**语义面** + **结构面**,同归因)· **C-3** 两条(**声明级** + **运行级**,分别归因)
      ⇒ 断言必红且**精确**命中对应前缀

> ⚠️ **标题的「历史措辞」说明**:本 Scenario 标题中的「C-1′ / C-2 / C-3 / C-4」是本 Requirement **引入时**的四条证书;本 change 后 V6-d 实际判定**六条**(四条 + `R1-b` + `R1-c`)⇒ 标题**逐字保留**(MODIFIED delta 的**场景名守恒**要求 —— 改名会被判为「丢弃既有场景」)⇒ **完整六条清单以本段正文为准**。
> **锚点**:`test_cep_ualink_inc.cc` 的 V6-d 六条证书断言 + 反向 fixture
> (C-1′ 2 · C-2 1 · C-3 2 · C-4 1 · **R1-b 5** · **R1-c 2**)

#### Scenario: **C-1′ 槽位语义继承** —— 继承 SHALL **良定义**

- **WHEN** 判定某 `(plane, network)` 的 **C-1′**
- **THEN** 声明源 SHALL **不**含 `collective_plane_vc_binding` 块
      (集合平面**自带**独立 VC 绑定 ⇒ ⛔ 载体必须是**继承**;per `ADR-CEP-01` SU#4 **E.9** 的反 fixture)
- **AND** 该 network 上**全部有 `vc_binding` 的**既有实例的映射 SHALL **逐类相等**
      (类集合相同 ∧ 每类映射到同一 `vc_id`)⇒ 不等 ⇒ 必红
- **AND** 该 network SHALL **至少有一个**有绑定的既有实例 ⇒ 否则继承**无主体** ⇒ SHALL 判 **FAIL**(⛔ 禁 vacuous PASS)
- **AND** **正控**:该网全部有绑定的实例映射逐类相等且声明源无自带绑定块 ⇒ PASS
- **AND** **反向 fixture**(两条,**同归因** `[CP-03-VC-CERTIFICATE/C-1′]`,**精确相等**匹配):
      ① **语义面**:令其中两个实例的映射不一致 ⇒ 必红(影子 `protocol_instances.json`)
      ② **结构面**:在声明源注入 `collective_plane_vc_binding` 块 ⇒ 必红(影子 `collective_planes.json`)

> **锚点**:`test_cep_ualink_inc.cc` 的 C-1′ 断言 + 反向 fixture
> (影子 `protocol_instances.json` + 影子 `collective_planes.json`)

#### Scenario: **C-2 VC 覆盖完整** —— 继承映射 SHALL **覆盖**该网 `vc_count` 全部取值

- **WHEN** 判定某 `(plane, network)` 的 **C-2**
- **THEN** 集合平面在该网上**绑定**的 `vc_id`(= 继承映射的 **vc 镜像**)SHALL **覆盖**
      `fabric.json` 该 network 的 `vc_count` **全部取值** ⇒ 只覆盖部分 ⇒ 必红
- **AND** 该判据 SHALL **直接**对 `fabric.json` 的 `vc_count` 判定(⛔ **不**依赖「边表是否引用某 VC」)
- **AND** **正控**:覆盖完整 ⇒ PASS;**反向 fixture**:把该网 `vc_count` 调大 ⇒ 必红
      (归因 `[CP-09-IC1]`,**精确相等**匹配)

> **锚点**:`test_cep_ualink_inc.cc` 的 C-2 断言 + 反向 fixture(影子 `fabric.json`)

#### Scenario: **C-3 控制面与数据面互斥** —— 声明级与运行级 SHALL **分别归因**

> **为何「分别」**:声明级是**证书判定**(该 plane 是否**声明**承担 C-3)⇒ 归 `CP-03-VC-CERTIFICATE/C-3`;
> 运行级是**行为义务**(in-flight 时**真**拒绝)⇒ 归 `UIA-04` ⇒ 两级归属不同面 ⇒ ⛔ **不共用** id
> (per `ADR-CEP-01` SU#5 ②)。

- **WHEN** 判定某 `(plane, network)` 的 **C-3**
- **THEN** 该 plane SHALL **已登记**在 plane 注册表(单源在**定义点**)∧ SHALL 声明为**非本地模型**
      (承载真实 fabric 集合流量)∧ SHALL 声明承担 **C-3** ⇒ 任一不满足 ⇒ 必红
- **AND** **运行级**:该 plane 的**行为** SHALL 在 group 有 op **in-flight** 时**拒绝**
      `group_register_region` 与 `group_destroy` ⇒ 由调用序列 fixture 判定
- **AND** 两级 SHALL **分别**归因:声明级 ⇒ `[CP-03-VC-CERTIFICATE/C-3]`(本 Requirement 的证书 id);
      运行级 ⇒ `UIA-04`(本仓**新建**的行为义务 id,per `cep-ualink-inc` 的 UIA-04 场景)
      ⇒ **反向 fixture**(声明级 / 运行级各一条)必红且**各自**精确命中
      (⛔ 两级**不**共用一个 id —— 声明级是**证书判定**、运行级是**行为义务**,归属不同面)

> **锚点**:`test_cep_ualink_inc.cc` 的 C-3 声明级断言 + `FabricCollectivePlane` 的调用序列断言 + 反向 fixture

#### Scenario: **C-4 未检验项显式登记** —— 登记项 SHALL **存在、非空、且未被冒充**

- **WHEN** 判定某 `(plane, network)` 的 **C-4**
- **THEN** 未检验项登记 SHALL **存在** ∧ **非空** ∧ 每项 `ref` **非空**
- **AND** 每项 `statement` SHALL **不含**「已验证 / 已满足」字样 ⇒ 把任一未检验项标成「已验证」⇒ 必红
- **AND** 登记项 SHALL 由**规约载体**提供(⛔ **不得**在判据面**复制**一份未检验项表 —— 禁双实现)
- **AND** **正控**:U-1 / U-2 登记完整 ⇒ PASS;**反向 fixture**:把某项 `statement` 改成含「已验证」⇒ 必红
      (归因 `[CP-03-VC-CERTIFICATE/C-4]`,**精确相等**匹配)

> **锚点**:`test_cep_ualink_inc.cc` 的 C-4 断言 + 反向 fixture(影子规约载体)

#### Scenario: **R1-b 排序假设逐 VC 一致** —— 同 network 上混排实例的 `ordering_assumption` SHALL **全等**

> **定位**:per `ADR-CEP-12` 决策 5(`ordering_assumption` 逐网络 / 逐 `vc_id`)+ 台账 **Entry #344** 的 `T-C2-⓪`(**条件化口径**)。
> **为何不是「新增决策」**:`include/tlm/cep/protocol_instance.schema.json` 的 `ordering_assumption` description 早宣称
> 「一致性由**装配期校验**承担(per `INV-CEP-12-IC2`)」,而 **`INV-CEP-12-IC2` 全文只保证 key 可解析**
> ⇒ **该承诺此前无承担者** ⇒ 本 Scenario 是**兑现已有文本**。
> **载体**:`include/tlm/cep/collective_plane_contract.hh` 的 `evaluate_shared_mode_certificate`(V6-d 段)——
> 与 C-1′ **共享**同一判据实现(⛔ **禁双实现**),⛔ **不新增** `ContractRule`。
> **归因**:`[CP-03-VC-CERTIFICATE/R1-b]`(**⛔ 不用** `C-1′-b` —— `C-1′` 在 `ADR-CEP-01 §E.3.1` 只定义槽位继承,借其名会掩盖这是新决策)。

- **WHEN** 判定某 `(plane, network)` 的 **`R1-b`**
- **THEN** 该 network 上**全部有 `vc_binding` 的**实例 SHALL 声明 `ordering_assumption[network]`
      ⇒ 任一此类实例**缺**该声明 ⇒ SHALL 判 **FAIL**
      (⛔ **不得**以 `ordering_assumption` 为驱动 —— 那会把缺失实例**静默当子集排除**)
      > ⚠️ **「有 `vc_binding`」的精确定义** = 该 network 的 `vc_binding` **至少含一个整数值**
      > (⇒ 与 `C-1′` 的实例集合**同口径** —— 二者 MUST **逐字复刻**同一 loader push 条件)
- **AND** 每个实例的 `ordering_assumption[network]` 的 **key 集合** SHALL 恰为 `0 .. vc_count-1`
      (⇒ 缺项 ⇒ FAIL;越界项 ⇒ FAIL)
      > ⚠️ **判定次序 SHALL 为「越界 → 缺失」** —— 否则 `"3"` → `"4"` 的变异会误报为「完备性不足」⇒ 归因不稳
- **AND** 每个实例的 `ordering_assumption[network]` 的 **key 形态** SHALL 为**规范十进制**
      ⇒ 非数字 / 前导零(如 `"04"`)⇒ SHALL 判 **FAIL**
      (⛔ schema 的 `propertyNames.pattern = ^[0-9]+$` **允许**前导零 ⇒ 判据须自带该检查)
- **AND** **不同实例**之间,**同一 `vc`** 的 ordering 值 SHALL **全等**
      ⇒ 任一 `vc` 上取值不同 ⇒ SHALL 判 **FAIL**
- **AND** **正控**:同 network 上各实例的逐 VC ordering 全等 ⇒ PASS
- **AND** **反向 fixture**(至少 **5** 条,归因 `[CP-03-VC-CERTIFICATE/R1-b]`,**精确相等**匹配):
      ① **值不一致**:改某实例某 vc 的 ordering(`p2p_ordered` → `unordered`)⇒ 必红
      ② **条目缺失**:删某实例某 vc 的 ordering ⇒ 必红
      ③ **key 非数字** ⇒ 必红
      ④ **key 前导零** ⇒ 必红
      ⑤ **key 越界**(`>= vc_count`)⇒ 必红
- **AND** ⛔ SHALL **NOT** 判「排序假设的**正确性**」—— 本判据只证**跨实例一致**;
      ⛔ 同一 VC 承载多类消息时(实测 `ualink_fabric` 的 vc3 同时承载 `response` + `snoop_resp`),
      `ordering_assumption[vc]` 是**每 VC 一个值** ⇒ **不得**把本判据的绿引申为「逐类正确」
- **AND** ⛔ SHALL **NOT** 判 `ordering_assumption` 的**取值域**(既有 `scripts/test/cep_decl_alignment_check.sh`
      的 `jsonschema` 全量校验已锁 `enum`)⇒ **禁双实现**

> **锚点**:`test/test_cep_ualink_inc.cc` 的 `R1-b` 断言 + 5 条反向 fixture
> (+ 「旧实现必绿 / 新实现必红」并排取证:旧实现 `grep -c ordering_assumption` = **0** ⇒ 同一变异在旧实现上 PASS)

#### Scenario: **R1-c scope 完备性** —— 声明了 plane 绑定的实例 SHALL 落在 `certificate_scope` 内

> **定位**:per `ADR-CEP-01` **E.3.3**(参照集 = 两域并集 + 不一致 ⇒ 该网不得用 M-2)+ 台账 **Entry #344** 的 `K-2`。
> **为何需要**:`certificate_scope` 是**人手写的清单** ⇒ 写漏一个 network ⇒ 判据**静默不覆盖**该网,
> 且**没有任何闸会抱怨** ⇒ 本判据把作用域从「自声明」变为「**自校验**」。
> **方向**:**声明 ⇒ scope 完备性**(⛔ 不是「scope 项 ⇒ 声明支持」方向 —— 后者检不出「scope 漏登记」)。
> **归因**:`[CP-03-VC-CERTIFICATE/R1-c]`。

- **WHEN** 判定 **`R1-c`**(scope 完备性)
- **THEN** 对 `decl.plane_binding` 中**每一个**实例 SHALL 依次判定:
      ① `plane_binding` 值为空串(显式 `null`)⇒ **跳过**(该实例**不启用** collective)
      ② 该 plane **未在** plane 注册表登记 ⇒ SHALL 判 **FAIL**(红因「plane 未登记」)
      ③ 该 plane 是**本地模型**(`is_local_model == true`)⇒ **跳过**(不承载真实 fabric 集合流量)
      ④ 该实例**无任何** `vc_binding` ⇒ SHALL 判 **FAIL**(M-2 无主体;⛔ 禁 vacuous PASS)
      ⑤ 该实例 `vc_binding` 中的**每个** network N ⇒ **`(plane, N)` 对** SHALL 出现在 `certificate_scope` 内
      ⇒ 不在 ⇒ SHALL 判 **FAIL**,红因 SHALL 为「`(plane, network)` 不在 `certificate_scope` 内」
      (⛔ **不是**「plane 未登记」—— 那是 ②,两者**归因文本必须可区分**)
- **AND** 本判据 SHALL 在 `certificate_scope` 项**循环之前**判定(⛔ 非循环内 —— 缺失项**根本不会被遍历到**)
- **AND** **正控**:各绑定实例的 `(plane, network)` 对均在 `certificate_scope` 内 ⇒ PASS
- **AND** **反向 fixture**(至少 **2** 条,归因 `[CP-03-VC-CERTIFICATE/R1-c]`,**精确相等**匹配):
      ① **scope 漏登记**:给某 `compute_noc` 上有 `vc_binding` 的实例**插** `plane_binding` 键
      (⚠️ 是**插键**不是改值)⇒ 必红且红因含「不在 `certificate_scope`」
      ② **无 VC 绑定**:令绑定 `FabricCollectivePlane` 的实例**无任何** `vc_binding` ⇒ 必红
- **AND** ⛔ SHALL **NOT** 把 `compute_noc` 加入 `certificate_scope` 以求「全网一致」
      —— 它按 `ADR-CEP-01 §E.3.3` **不属 M-2** ⇒ 允许不一致(per `U-3` 裁决)
- **AND** `plane_binding` 为**零键**时本判据 SHALL 判 **vacuous PASS**(无主体 ⇒ 无缺口可查)
      ⇒ ⛔ **语义区别于** V6-c 的「零键 ⇒ SKIP」(后者 = 不适用)⇒ 二者**不得**混为一谈

> **锚点**:`test/test_cep_ualink_inc.cc` 的 `R1-c` 断言 + 2 条反向 fixture
> (+ 「旧实现必绿 / 新实现必红」并排取证:旧实现**不读** `plane_binding` ⇒ 旧绿是**平凡**的,取证时须逐字写明)

#### Scenario: **既有证书的归因前缀 SHALL 逐一不变**

> **为何单列**:新增子判据**可能**抢在既有判据之前触发 ⇒ 既有反向 fixture 的**归因前缀**被改写
> ⇒ ⛔ 「仍绿」**不等于**「归因未变」(断言写错也可能仍绿)⇒ 必须**逐条比对前缀字符串**。

- **WHEN** 既有 **6 条**反向 fixture 各自触发
- **THEN** 其归因前缀 SHALL **逐字不变**:`[CP-03-VC-CERTIFICATE/C-1′]`(2 条)· `[CP-09-IC1]`(1 条)· `[CP-03-VC-CERTIFICATE/C-3]`(2 条)· `[CP-03-VC-CERTIFICATE/C-4]`(1 条)
- **AND** 新 tag `[CP-03-VC-CERTIFICATE/R1-b]` / `[CP-03-VC-CERTIFICATE/R1-c]` SHALL **NOT** 被
      `[CP-03-VC-CERTIFICATE]` 或 `[CP-03-VC-CERTIFICATE/C-1′]` 的**精确前缀**匹配命中
      (⛔ 既有 fixture 用**带 `]`** 的 `rfind` ⇒ 结构上不可能互命中 ⇒ 仍须回归钉住)

> **锚点**:`test/test_cep_ualink_inc.cc` 的 6 条反向 fixture 的前缀断言 + 新 tag 的隔离断言

#### Scenario: **本 Requirement 的两条新判据 SHALL 在真实 gate 路径可触达**

> **为何单列**:防「判据只存在于测试里手搓 `decl` 直调」⇒ 实现与 fixture 同源 ⇒ **无法证明判据有牙**。

- **WHEN** 经 `run_collective_plane_contract` 的**真实调用链**执行契约判定
- **THEN** `R1-b` / `R1-c` 的 FAIL SHALL **折进 `CP-03` 行**(`rule=` 字段**仍为** `CP-03`)
- **AND** `ContractRule` 总数 SHALL **保持 6**(⛔ **不新增**第 7 条)
- **AND** `detail` SHALL 含**精确前缀** `[CP-03-VC-CERTIFICATE/R1-b]` / `[CP-03-VC-CERTIFICATE/R1-c]`
- **AND** **反向 fixture**:经真实 gate 路径毒化 ⇒ 必红

> **锚点**:`test/test_cep_ualink_inc.cc` 的真实 gate 路径用例
