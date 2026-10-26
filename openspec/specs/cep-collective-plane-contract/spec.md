# cep-collective-plane-contract Specification

## Purpose
定义 Fabric Collective Plane 的**跨平面契约判据**:6 条可机器检查的规则
(零调用边 / canonical 10 流零 diff / collective VC 预留不冲突 / 区域互斥(与行为面**同源**)/
完成域判别(`group_id` 不进 Coherence Interface)/ 抓取点可测性登记)+ 独立声明源
`collective_planes.json` + **SKIP / PASS / FAIL 三态**显式化 + verdict 逐条可 grep。

本 capability 把 `ADR-CEP-01 §3.3` 的 **7 条抓取点**里本 Stage 可测的部分固化为机器闸;
**本 Stage 物理不可测的条款(跨平面 ordering A/B/E、fabric 组播投递单实现)不写进判据清单**
(per「禁写恒真 / 恒 SKIP 判据」纪律)⇒ 判据退化 = 用例红,判定结论为红 = 用例仍绿 + 结论登记为交付物。

## Requirements

### Requirement: cep-collective-plane-contract

CEP SHALL 将 **Fabric Collective Plane 的跨平面契约**固化成**可机器检查的判据**:5 条契约判据 + 独立声明源 + 三态显式化判定。

> **定位**:per `ADR-CEP-01 §3.3` 的 7 条抓取点 —— 覆盖本 Stage 可测部分,其余 P3 / Stage 2+。
> **判据实现**:`collective_plane_contract.hh` —— 与行为面**共享**同一判据实现,**禁双实现**。
> **声明源**:`collective_planes.json` + schema —— **诞生即不冻结**;**不扩** `protocol_instances.json`。
> **禁写恒真 / 恒 SKIP 判据**:本 Stage 不可测的条款 SHALL **不**进判据清单。

#### Scenario: 声明源形态与 schema 一致(plane 绑定 / region 表 / VC 预留)

- **WHEN** 加载 `include/tlm/cep/collective_planes.json`
- **THEN** SHALL 与配套 schema **逐字段一致**,且声明最小集 SHALL 恰含三组:`plane_binding`(协议实例 → 绑定的 plane,默认 `null`)/ `registered_regions`(range → gid + mode)/ `collective_vc_reservation`(collective 流量预留的 VC)
- **AND** 声明源 SHALL 被**至少一个**测试消费(⇒ 不产生死文件)
- **AND** 声明源缺失 / 畸形 SHALL 判 **FAIL**(配置错误)—— 与「无可检查对象 ⇒ SKIP」**严格区分**(per 队列 #4 `Entry #65` 的 `source_error` 语义)

> **锚点**:`test_cep_collective_plane_contract.cc` 的声明源 schema 一致性断言 + crosscheck 消费断言

#### Scenario: 判据 SHALL **三态显式化**(SKIP ≠ PASS ≠ FAIL)

- **WHEN** 契约判据给出结论
- **THEN** SHALL 显式区分 **`[SKIP]`** / **`[PASS]`** / **`[FAIL]`** 三态(SKIP 与 PASS **可区分**,per 队列 #4 `cep-channel-dependency-gate` 先例)
- **AND** 每条判据 SHALL 输出**逐条可 grep** 的 verdict(格式 `[verdict] <rule-id> PASS|FAIL|SKIP (detail)`,per `ADR-CEP-09 §8.3` 同源格式)
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
