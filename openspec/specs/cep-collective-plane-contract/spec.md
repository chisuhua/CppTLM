# cep-collective-plane-contract Specification

## Purpose
定义 Fabric Collective Plane 的**跨平面契约判据**:6 条可机器检查的规则
(零调用边 / canonical 10 流零 diff / collective VC 预留不冲突 / 区域互斥(与行为面**同源**)/
完成域判别(`group_id` 不进 Coherence Interface)/ 抓取点可测性登记)+ 独立声明源
`collective_planes.json` + **SKIP / PASS / FAIL 三态**显式化 + verdict 逐条可 grep。

⚠️ **2026-11-03 修订**(per 队列 #13 实现层 · 台账 **Entry #189** 授权):
声明最小集由**三组**扩为**四组**(加 `vc_reservation_declaration` —— Decision D 的**显式声明门**);
「空预留且缺声明 ⇒ FAIL」;并新增 **Requirement** `cep-collective-plane-contract-vc-precondition`
承载 **`V6-c` 前提约束**(归因 `CP-03-VC-PRECONDITION`)与 **`CP-05` 标识清单 / 扫描面扩展**
(归因 `CP-05`)⇒ **规则族数仍为 6**(`CP-01`..`CP-06`;`kContractRuleCount` 不变)。

本 capability 把 `ADR-CEP-01 §3.3` 的 **7 条抓取点**里本 Stage 可测的部分固化为机器闸;
**本 Stage 物理不可测的条款(跨平面 ordering A/B/E、fabric 组播投递单实现)不写进判据清单**
(per「禁写恒真 / 恒 SKIP 判据」纪律)⇒ 判据退化 = 用例红,判定结论为红 = 用例仍绿 + 结论登记为交付物。

## Requirements

### Requirement: cep-collective-plane-contract

CEP SHALL 将 **Fabric Collective Plane 的跨平面契约**固化成**可机器检查的判据**:6 条契约判据(`CP-01`..`CP-06`)+ 独立声明源 + 三态显式化判定。

> **定位**:per `ADR-CEP-01 §3.3` 的 7 条抓取点 —— 覆盖本 Stage 可测部分,其余 P3 / Stage 2+。
> **判据实现**:`collective_plane_contract.hh` —— 与行为面**共享**同一判据实现,**禁双实现**。
> **声明源**:`collective_planes.json` + schema —— **诞生即不冻结**;**不扩** `protocol_instances.json`。
> **禁写恒真 / 恒 SKIP 判据**:本 Stage 不可测的条款 SHALL **不**进判据清单。
> ⚠️ **本次修订的实质变化 = 1 个 Scenario**(「声明源形态与 schema 一致」:三组 → 四组 + 空预留门)
> + 「5 条契约判据」计数订正为 **6**;其余 Scenario **逐字不变**。

#### Scenario: 声明源形态与 schema 一致(plane 绑定 / region 表 / VC 预留)

- **WHEN** 加载 `include/tlm/cep/collective_planes.json`
- **THEN** SHALL 与配套 schema **逐字段一致**,且声明最小集 SHALL 恰含**四组**:`plane_binding`(协议实例 → 绑定的 plane,默认 `null`)/ `registered_regions`(range → gid + mode)/ `collective_vc_reservation`(collective 流量预留的 VC)/ **`vc_reservation_declaration`**(空预留时的显式声明)
- **AND** `collective_vc_reservation` 为**空**时 SHALL **必须**有 `vc_reservation_declaration`,且 `status` / `reason` / `owner` / `revisit_trigger` **四字段齐全** —— 缺任一 ⇒ 判 **FAIL**(per `ADR-CEP-01` SU 2026-11-03 Decision D);**不得**把「空预留」判成 **PASS**(否则该判据对不同拓扑**恒真**、不具鉴别力)
- **AND** 声明源 SHALL 被**至少一个**测试消费(⇒ 不产生死文件)
- **AND** 声明源缺失 / 畸形 SHALL 判 **FAIL**(配置错误)—— 与「无可检查对象 ⇒ SKIP」**严格区分**(per 队列 #4 `Entry #65` 的 `source_error` 语义)

> **锚点**:`test_cep_collective_plane_contract.cc` 的声明源 schema 一致性断言 + crosscheck 消费断言
> + 空预留缺声明 ⇒ FAIL 的断言 + 反向 fixture

#### Scenario: 判据 SHALL **三态显式化**(SKIP ≠ PASS ≠ FAIL)

- **WHEN** 契约判据给出结论
- **THEN** SHALL 显式区分 **`[SKIP]`** / **`[PASS]`** / **`[FAIL]`** 三态(SKIP 与 PASS **可区分**,per 队列 #4 `cep-channel-dependency-gate` 先例)
- **AND** 每条判据 SHALL 输出**逐条可 grep** 的 verdict(格式 `[verdict] rule=<CP-0x> PASS|FAIL|SKIP (detail)`,per `ADR-CEP-09 §8.3` 同源格式)
- **AND** **不得**把「无可检查对象」装作 **PASS**

> **锚点**:`test_cep_collective_plane_contract.cc` 的三态可区分断言 + verdict 格式断言

#### Scenario: 零调用边闸 SHALL 可执行(include-graph 扫描 + 编译期依赖)

- **WHEN** 契约判据跑「零调用边」检查(per `ADR-CEP-01 §3.3` 抓取点 1)
- **THEN** SHALL 扫描 collective 头文件的**直接 include** 与编译期依赖,并在出现 directory / ordering / sharer 侧依赖时判 **FAIL**
- **AND** 该闸 SHALL 与行为面的零调用边断言**正交**(一个查 include 结构,一个查类型可达)—— 两者**不得**是同一份实现
- **AND** **反向 fixture**:注入一条 directory include ⇒ 断言必红

> **锚点**:`test_cep_collective_plane_contract.cc` 的 include-graph 闸断言 + 反向 fixture

#### Scenario: canonical 10 流 SHALL **零 diff**

- **WHEN** 契约判据核对 Coherence Interface 侧的流清单
- **THEN** SHALL 与 `ADR-CEP-02 §2` 的 canonical 10 条流**逐条相等**(不新增、不删除、不改语义;per `ADR-CEP-01 §3.3` 抓取点 4)
- **AND** **反向 fixture**:fixture 改流清单 ⇒ 断言必红

> **锚点**:`test_cep_collective_plane_contract.cc` 的 10 流 diff 断言 + 反向 fixture

#### Scenario: collective VC 预留 SHALL 与 coherence VC 绑定**不冲突**

- **WHEN** 契约判据核对 `collective_planes.json` 的 `collective_vc_reservation` 与 `include/tlm/cep/fabric.json` 的既有 `vc_binding`
- **THEN** 预留 VC SHALL ∈ **0-3**(per `noc_bundles_tlm.hh` 的 `vc_id` 值域 / `fabric.schema.json` maximum=4)
- **AND** 预留的 VC SHALL **不与**任何已声明协议的 `vc_binding` **冲突**(死锁通道隔离:collective barrier 等待期间不得占用 coherence 事务依赖的 fabric 资源,per `ADR-CEP-01 §3.3` 抓取点 3)
- **AND** 预留为**空**时 SHALL 按「声明源形态」Scenario 的**空预留门**判定(缺声明 ⇒ FAIL)
- **AND** **反向 fixture**:fixture 改成冲突 VC ⇒ 断言必红

> **锚点**:`test_cep_collective_plane_contract.cc` 的 VC 冲突断言 + 反向 fixture

#### Scenario: 区域互斥判据 SHALL 与行为面**同源**(禁双实现)

- **WHEN** 契约判据检查 registered region 表的重叠
- **THEN** SHALL 与行为面 `LocalCollectivePlane` 的区域互斥检查**共享同一判据实现** —— **禁双实现**(per 队列 #4 的「双挂共享判据」纪律:守卫须与它守护的命令等价)
- **AND** 判据的结论 SHALL 与行为面的实际行为**一致**(同一输入 ⇒ 同一结论)

> **锚点**:`test_cep_collective_plane_contract.cc` 的同源一致性断言(判据结论 vs 行为结论)

#### Scenario: 抓取点可测性 SHALL **显式登记**(哪几条本 Stage 可测、哪几条推迟)

- **WHEN** 核对本 capability 覆盖的抓取点范围
- **THEN** SHALL **显式**声明:`ADR-CEP-01 §3.3` 的 7 条抓取点中,哪几条**本 Stage 可机器检查**、哪几条**推迟到 P3 / Stage 2+**、推迟的**理由**
- **AND** 推迟项 SHALL **不得**以「恒 PASS」「恒 SKIP」的形式出现在判据清单中
- **AND** **权威源 = `docs/adr/ADR-CEP-01-fabric-collective-plane.md` 的「Status Update 2026-10-26 —— §3.3 抓取点的可测性裁决」段**;
  断言 SHALL **解析该段**并逐条取出 7 个抓取点的覆盖状态与理由
  (per `test/cep_adr_parser.hh` 的 ADR 解析先例 —— **不是**「文档里有这张表就通过」,
  也不是测试自建一份期望集与自建一份实际集对比的**恒真**写法)
- **AND** 解析 SHALL 覆盖**全部 7 条**抓取点编号;缺一条 / 多一条 ⇒ 判红

> **锚点**:`test_cep_collective_plane_contract.cc` 的抓取点覆盖登记断言(编号集合逐条比对)

#### Scenario: 本 capability 对 B2 的贡献 SHALL **非空**且逐条可查

- **WHEN** 核对本 change 对「66 条守门 invariant 机器检查 100% 覆盖」的贡献
- **THEN** 贡献清单 SHALL **非空**,且每一条 SHALL 指明:**被行为化的既有编号** 或 **被加固的契约组**
- **AND** SHALL **不新增**任何 `INV-` 编号(15 组 66 条编号是 `test_cep_canonical_invariants.cc::expected_groups()` 的硬编码;ADR-CEP-01/03 在 15 组里**无组** ⇒ 新增编号即契约违反)
- **AND** 若该清单为空 ⇒ 本 change **自我否决**

> **锚点**:`test_cep_collective_plane_contract.cc` 的贡献清单非空断言

---

### Requirement: cep-collective-plane-contract-vc-precondition

CEP SHALL 为 **`CP-03` 族**补充 **`V6-c` 前提约束**,并为 **`CP-05`** 补充
**标识清单扩展**与**可注入的扫描面扩展**。

> **为什么独立成 Requirement**(而非并入既有 Requirement):既有 Requirement 的 8 个 Scenario
> **逐字不变** ⇒ 冻结面的**实质 diff 仅 1 个 Scenario** ⇒ review / 授权时逐行可比。
> **归因 id 不新增规则族**:`V6-c` 折进 `CP-03` 行(以 **detail 前缀** 表达)· `CP-05` 扩展仍在 `CP-05` 行
> ⇒ `kContractRuleCount == 6` **不变**。

#### Scenario: **`V6-c` 前提约束 SHALL 可执行**(`unavailable` ⇒ 仅本地模型)

- **WHEN** `vc_reservation_declaration.status == "unavailable"`
- **THEN** `plane_binding` 的**每个非空**绑定值 SHALL 落在**本地模型白名单**内 ⇒ 否则 SHALL 判 **FAIL**
  (拓扑中**不得**存在真实 fabric 集合流量载体)
- **AND** 白名单 SHALL **从 plane 注册表派生**(每个 plane 实现在**定义点**声明其局部性)⇒ **未注册**的绑定名 ⇒ SHALL 判 **FAIL**
- **AND** ⚠️ `null` 绑定(loader 物化为空串)SHALL **不**判 FAIL;`plane_binding` 为**零键** ⇒ SHALL 判 **SKIP**
  (⛔ **禁** vacuous PASS)
- **AND** ⛔ SHALL **不**新增第 7 条 `ContractRule` ⇒ V6-c SHALL 作为 `CP-03` 行的 **detail 前缀**
  `[CP-03-VC-PRECONDITION]` 表达;`rule=` 字段**仍为** `CP-03`
- **AND** **正控**:当前绑定 ⇒ PASS;`null` 绑定 ⇒ SKIP;**反向 fixture**:绑定一个**未注册**的 plane 名 ⇒ 必红
  (归因 `CP-03-VC-PRECONDITION`;**精确相等**匹配 —— ⛔ 不用前缀匹配,否则 `CP-03` 会命中它)

> **锚点**:`test_cep_collective_plane_contract.cc` 的 V6-c 断言 + 反向 fixture

#### Scenario: **CP-05** 标识清单 SHALL 含 `collective_region_table` 且扫描面 SHALL 可注入扩展

- **WHEN** 运行完成域判别(coherence 侧扫描)
- **THEN** 标识清单 SHALL 含 `collective_region_table`(coherence 侧**不得**出现区域表类型)
- **AND** 扫描面 SHALL 支持**注入扩展**(由 `ContractPaths` 提供扩展文件列表)⇒ 扩展面 SHALL 是**真正的 coherence / 源端侧**文件
  —— ⛔ SHALL **不**把 collective 侧的 adapter 头塞进 coherence 侧扫描(那只会「证明没扫错文件」= 恒真)
- **AND** **正控**:同一注入对既有 coherence 侧文件 **必红** ⇒ 证明扫描器有牙;
  **反向 fixture**:注入 `collective_region_table` ⇒ 必红(归因 `CP-05`)

> **锚点**:`test_cep_collective_plane_contract.cc` 的 CP-05 清单断言 + 反向 fixture

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

#### Scenario: **C-3 控制面与数据面互斥** —— 声明级与运行级 SHALL **同归因**

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
