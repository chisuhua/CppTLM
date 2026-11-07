# cpptlm-cep-refkind-carrier-whitelist-contract: 载体自足性的**唯一白名单**登记进契约面

> **状态**: 🟡 **Proposed**(2026-11-07 立项 —— per 台账 Entry #266 的**验收 NIT**;KPI 并发 **0/3 → 1/3**)
> **立项依据**:Entry #266 验收报告 §三 的 NIT ——「`nvlink_sharp` 载体的自足性探针(CP-05)与主测试需加『唯一白名单 `source_registry.hh`』⇒ **建议在契约面同步登记该白名单**(与 `cep-instance-assembly` 的 required 机制同一治理模式)」
> **类型**: **doc-only**(0 源码改动 / 0 契约逻辑改动 / 0 声明改动)
> **估时**: ~0.5d

## Why

白名单机制已在**两处载体探针**里各自硬编码 —— `ualink_inc` 的 UIS-10(per Entry #252)与 `nvlink_sharp` 的 CP-05(per Entry #266)⇒ **机制已存在但契约面无登记**:

- **漂移面**:两处白名单**各写一遍**(实现形态还不同:前者「只允许这一个」列表 / 后者「跳过这一个」)⇒ 未来载体接入 `RefKind` 时,第三个探针会**各自发明**一份白名单 ⇒ 白名单集合**无单一真源**;
- **无据可依**:契约面(`cep-refkind-coverage`)只规定了「接入 `RefKind`」的**行为**要求,⛔ 未规定「接入时**允许**引用什么」⇒ 载体自足性判据的**豁免面**是**未登记**的空白;
- **同款先例**:`cep-instance-assembly` 的 `required_plane_instances` 机制已入契约面(per Entry #261/#262)⇒ **同一治理模式**。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | **契约面**:在 `cep-refkind-coverage` Requirement 中追加「载体自足性的**唯一白名单**」子段 + 新 Scenario **RKC-04** | `openspec/specs/cep-refkind-coverage/spec.md`(MODIFIED) |
| 2 | **判据**:白名单**唯一且三处一致**(契约面 + 2 处探针) | `test/test_cep_refkind_whitelist_contract.cc`(新) |

**Out of Scope**:
- ⛔ **不改**任何载体源码 / 探针实现 / 主测试实现(本条只登记**已有**机制);
- ⛔ **不改** `source_registry.hh` / `sources.json`(白名单对象本身不动);
- ⛔ **不改** M1 冻结面(`cep-refkind-coverage` **不在** M1 冻结集 ⇒ 本条**不触发** `--update --authorized-by`);
- ⛔ **不改**任何计数(B3b-2 仍 0/4)。

## Acceptance

- [ ] **AC-1** 契约面 `cep-refkind-coverage` Requirement 含「载体自足性的**唯一白名单**」子段 + **RKC-04** Scenario
- [ ] **AC-2** 白名单**唯一**:两处探针源码中形如 `"tlm/cep/*.hh"` 的引号路径集合 **== `{source_registry.hh}`**(多一个必红)
- [ ] **AC-3** **三处一致**:契约面 + `ualink_inc` 探针(UIS-10)+ `nvlink_sharp` 探针(CP-05)指向**同一**白名单路径
- [ ] **AC-4** 0 回归:`ctest` 全绿 + 4 闸 + 冻结面零 diff(**本 change 不触发** M1 授权)
- [ ] **AC-5** 台账 Entry + 归档 + KPI 回填 0/3
