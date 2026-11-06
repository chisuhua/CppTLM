# cpptlm-cep-basis-source-kind-and-cell-free: 来源类型可机器判 + `CellKind::Free`

> **状态**: 🟡 **Proposed**(2026-11-06 立项 —— 用户裁决「P1-M1 + P1-M2 全量」,per 台账 Entry #251;KPI 并发 **0/3 → 1/3**)
> **立项依据**: `07 号` **SU 2026-11-06 · 四**(D-1 来源效力裁决,台账 Entry #249)自陈「效力**不可机器自动判**」+ 台账 Entry #248 登记的两项 backlog(来源类型机制 / `CellKind::Free`)
> **improvement**: `.rddf/improvements/cpptlm-cep-basis-source-kind-and-cell-free.md`(7 段 + AC-1~AC-6)
> **关联 ADR**: `ADR-CEP-12`(认识论三档)· `ADR-CEP-04`(`ualink_4` 取值裁决)· `ADR-CEP-11`(规范版本锁定)
> **冻结面**: **零 diff** —— 新增 `sources.json`(不在 41 产物内);⛔ 不改 `fabric.json` / `protocol_instance.schema.json`
> **估时**: 3-5d(M1 2d / M2 1.5-2d)

## Why

`07 号` SU · 四 D-1 裁定「公开非规范来源不构成 `Direct`」,但同一 SU 诚实声明：**载体没有来源类型字段 ⇒ 该裁定只能人工抽样**。这使 B3b-2 的 4 族 ⏭ 缺乏可审计强度,也使任何未来族的「是否满足 `Direct`」不可复算。

同时 4 个状态载体各有**互不兼容的 basis 枚举**(4 套)、`CellKind` 有 2 套(2 值 / 3 值),`ref` 全是自由字符串 ⇒ 同一「证据强度」概念有 4 个 token 名,改个枚举名即可形式上合规。

`CellKind::Free` 缺失则「规范**明确**实现自由」被迫塞进 `Unknown`(规范未覆盖),导致 T4 完备性**低估**。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | `RefKind{SpecClause, PublicNonnormative, RepoDerived}` + `SourceEntry` + `kSources[]` + `resolve_ref()` | `include/tlm/cep/source_registry.hh`(新) |
| 2 | 来源注册表真源(id / kind / title / locator / sha256 / retrieved / license) | `include/tlm/cep/sources.json`(新) |
| 3 | 逐载体 `resolve_ref` 特化 + `basis_ref_kind_consistent()` | `tilelink_state_spec.hh` / `cxl_state_spec.hh` / `mesif_state_spec.hh` / `ualink_inc_state_spec.hh` |
| 4 | `CellKind::Free`(tilelink 2→3 值 / cxl 3→4 值) + adapter `transition_is_free` | 2 个 state_spec + 2 个 adapter |
| 5 | 判据测试(逐格可解析 / 映射一致 / JSON↔头文件一致 / 四态互斥) | `test/test_cep_source_registry.cc`(新) |
| 6 | 决定性变异闸(三态:clean / dropped / downgraded) | `scripts/test/cep_source_effect_check.sh` + probe(新) |

**Out of Scope**: 不改引擎转移逻辑;不重写既有格 basis;不改 M1 冻结面;不改 `07 号`/ADR(D-1 诚实声明的更新交 architect 追加 SU)。

## Impact

- **公共 API**: 新增 1 头 + 2 载体枚举扩展 ⇒ 走本 change;
- **引擎**: 零逻辑变更(仅 `Free` 分支 + adapter 新方法);
- **测试**: 新增 1 测试文件 + 1 闸;4 载体现有断言零改动(新入口是**增量**);
- **KPI**: 占 1 slot;
- **遗留**: architect 追加 SU 更新 D-1「不可机器判」声明(台账登记)。

## Acceptance

- [ ] **AC-1** 4 载体每格 `ref` 可解析到已登记来源(未登记 = 0);`sources.json` ↔ `kSources[]` 逐条一致
- [ ] **AC-2** 按载体映射 0 违规(tilelink `Direct`⇒`spec_clause`;cxl `PublicDirect`⇒`public_nonnormative`;mesif `Primary`⇒`repo_derived` 且 ref 非空;ualink `SelfDerived`⇒`repo_derived` / `Pending`⇒`public_nonnormative`)
- [ ] **AC-3** `CellKind::Free` 两载体落地 + 四态互斥 + `transition_is_free` 显式分支(当前真实 Free 格 = 0)
- [ ] **AC-4** 决定性变异 3 条按正确原因红(删注册表条目 / 弱来源配 Direct / Free 误当 Unknown)
- [ ] **AC-5** 0 回归:`[cep]` 全量绿 + 冻结面 41 产物零 diff + 3 个 CEP 闸全绿
- [ ] **AC-6** 遗留登记:architect 追加 SU 更新 D-1「不可机器判」
