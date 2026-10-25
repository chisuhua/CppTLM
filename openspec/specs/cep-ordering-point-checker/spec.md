# cep-ordering-point-checker Specification

## Purpose
TBD - created by archiving change cpptlm-cep-directory-ordering-plane. Update Purpose after archive.

## Requirements

### Requirement: cep-ordering-point-checker

CEP SHALL 将 **AddressResolverIf 单接口 + J1/J2/J3 装配期静态判据 + INV-CEP-08-IC2 E 态静默升级检查项 + S1-7 (β) 裁决登记**固化为**可执行契约**;每条 SHALL 有**机器可判定**的形态(或明确降级登记)。

> **定位**:per `ADR-CEP-08 §1.3` 三子平面分工表,**Address Resolver / Sharer Tracker / Ordering Point 三子平面正交但耦合**;本 capability 覆盖第 3 子平面。

#### Scenario: `AddressResolverIf` SHALL 存在且仅含单方法 `resolve(pa)→home_node`

- **WHEN** 读取 `include/tlm/cep/address_resolver.hh`
- **THEN** SHALL 存在**且仅存在** 1 个方法 `resolve(pa) → home_node_id`,**命名逐字对齐** canonical 流 8 的语义
- **AND** 断言 SHALL 针对**签名**(C++ decltype)
- **AND** SHALL **不**新增 `canonical 流 8`(只定义接口路径;**不**改变 canonical 10 流语义)

> **锚点**:`test_cep_ordering_point_checker.cc` 的接口签名 decltype 断言

#### Scenario: **J1 单点性** SHALL 机器可判(per `ADR-CEP-08 决策 3 J1`)

- **WHEN** 读取 `protocol_instances.json` 中声明的一个实例的 ordering_assumption
- **THEN** 判据引擎 SHALL 通过**静态路径覆盖**判定「冲突事务路径是否都经同一点」⇔ J1 PASS/FAIL **二值可判**
- **AND** 输出 SHALL 有**机器可 grep** 形态(per `scp` 规范,见 capability 3 A-3-5)

> **锚点**:`test_cep_ordering_point_checker.cc` 的 J1 静态路径覆盖断言

#### Scenario: **J2 全序可观测性** SHALL 与 caps 耦合(per `ADR-CEP-08 决策 3 J2`)

- **WHEN** 某 protocol instance 消费的 Sharer Tracker `completeness == PARTIAL` 且 `broadcast_fallback == false`
- **THEN** 对**该 instance** 的 J2 SHALL **机器判 fail**(per `ADR-CEP-08 决策 3`:PARTIAL 无 fallback ⇒ J2 fail)
- **AND** J2 PASS 分支需要 caps `completeness != PARTIAL` **或** `broadcast_fallback == true`

> **锚点**:`test_cep_ordering_point_checker.cc` 的 J2 与 caps 耦合断言

#### Scenario: **E 态静默升级检查项**(per `ADR-CEP-08 决策 4` + `INV-CEP-08-IC2`)

- **WHEN** 对 `moesi_gpu`(per ArchForge `32 号 §6.1`)做组合检查
- **THEN** 检查器 SHALL 实跑;若允许静默升级的组合未声明为「可组合」⇒ fail
- **AND** 该检查项 **SHALL 不可缺**(per `ADR-CEP-05 §13.1` 的同批约束)

> **锚点**:`test_cep_ordering_point_checker.cc` 的 IC2 检查项断言(moesi_gpu 实跑)

#### Scenario: **S1-7 (β) 裁决登记**(per `ADR-CEP-05 §13.1` 同批约束)

- **WHEN** 裁决本 change 后的 `perm` 判别位选项(第 5 位? / 4 值 `perm` 是否足够)
- **THEN** 裁决结论 SHALL **作为交付物**登记(**不反向改** `cep-stub-cache-core.spec.md` 冻结面;**不落** schema 演进;**不落** ADR-CEP-05)
- **AND** 判据为红 ⇒ 用例红;**判定结论为红** ⇒ 用例仍绿且**登记为交付物**(per 红绿分离)

> **锚点**:`test_cep_ordering_point_checker.cc` 的 S1-7 (β) 登记断言

#### Scenario: **J3 唯一性保持** SHALL **降级登记**(单点 vacuous 机器判据)

- **WHEN** 检查 J3 的通用形式「迁移后两点不定出冲突序」
- **THEN** 该通用形式 SHALL **降级为文档判据**(**NOT 机器可判**;per `ADR-CEP-08 决策 7` HeteroGen §VI-E E1/学术侧未解决)
- **AND** 本 change 仅执行**单点 vacuous** 机器判据(有单一 OP ⇒ vacuous PASS;无 OP ⇒ 无 J3 可判)
- **AND** 「**vacuous**」SHALL 显式出现在断言标记中,防被误读为已完整实现(per 队列 #2 先例:不可验证条款不进 spec / 显式标记 vacuous)

> **锚点**:`test_cep_ordering_point_checker.cc` 的 J3 vacuous 机器判据断言
