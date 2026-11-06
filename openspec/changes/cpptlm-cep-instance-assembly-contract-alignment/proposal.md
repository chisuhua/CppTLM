# cpptlm-cep-instance-assembly-contract-alignment: 装配闸契约对齐(闭合 critic BLOCKER)+ 判据完备性加固

> **状态**: 🟡 **Proposed**(2026-11-06 立项 —— critic 复核 REJECT(1 BLOCKER)后的闭合 change;KPI 并发 **0/3 → 1/3**)
> **立项依据**:`.rddf/reviews/cep-instance-assembly-and-l4-e2e-verify.md` §三.3 的「reviewer 不可用」遗留 → 本轮 critic 复核 **REJECT:1 BLOCKER + 2 SHOULD_FIX + 3 NIT**
> **关联**:`cpptlm-cep-ualink-instance-assembly`(**已归档**)· `cpptlm-cep-moesi-gpu-l4-e2e`(**已归档**)
> **估时**: ~0.5d

## Why

P3 在 P2 **归档后**改判了装配闸的**平面层语义**(引入 `required_plane_instances`:null/空绑定 = 「不启用 collective」= 合法),但**冻结契约未同步**:

- `openspec/specs/cep-instance-assembly/spec.md` 的 Requirement ③ 与 IAG-01/02 仍是**无条件**判据(「若 `plane_binding` 含该实例键 ⇒ 绑定 SHALL 非空」)⇒ 按 spec 实现的第三方闸会把 `moesi_gpu` 判红,而仓内测试要求判绿 ⇒ **契约与实现/测试自相矛盾**;
- `ADR-CEP-12` SU(Entry #258)的 ① 三层证据表同样是无条件表述。

⇒ 本 change **只做契约对齐**(doc-only 主体)+ 判据完备性加固(3 条 NIT),⛔ 不改装配闸的**语义方向**(该方向已由 Entry #259 §⑤ 记录并被 critic 认可)。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | **契约对齐(BLOCKER)**:Requirement ③ + IAG-01/02 改写为「null/空 = 不启用 collective(合法);仅 **required** 实例的空/缺失绑定 ⇒ `[IAG-04]`」+ 钉死「required 清单由**调用方**显式传入,闸不自行推断」 | delta spec(MODIFIED `cep-instance-assembly`) |
| 2 | **ADR 追加 SU**(BLOCKER 的另一半) | `ADR-CEP-12` 追加 SU(注明 Entry #259 §⑤ 对 ③ 的修正,前段不回改) |
| 3 | **SHOULD_FIX-1**:rubric 增列 **L4-direct / L4-wire** 两档 + MOESI-GPU 行按两档标注 | `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md` |
| 4 | **NIT-1**:MGE-01 补**真跑 `ModuleFactory::instantiateAll`** | `test/test_cep_moesi_gpu_engine.cc`(+MGE-01b) |
| 5 | **NIT-2/3**:闸完备性(`family` 缺失 / `modules` 空或缺失 / `bridge_id` 缺失 ⇒ 红)+ 负控 +3 条 | `include/tlm/cep/instance_assembly_gate.hh` · `test/test_cep_instance_assembly.cc` |

**Out of Scope**: ⛔ 不改 TileLink-C 行的 L4 档位标注(**scope 外** —— 该行属更早归档的 Entry #246 ⇒ 登记 backlog,⛔ 不在本 change 回改历史记录);⛔ 不改装配闸的语义方向;⛔ 不新增 UALink 引擎。

## Acceptance

- [ ] **AC-1** `cep-instance-assembly` spec 的 ③ 与 IAG-01/02 与实现**逐条一致**(含 required 机制与「调用方传入」的权威声明)
- [ ] **AC-2** `ADR-CEP-12` 追加 SU 记录该修正(append-only,前段不回改)
- [ ] **AC-3** coverage rubric 有 L4 两档;MOESI-GPU 行按两档标注;TileLink-C 行**未**被回改(backlog 登记)
- [ ] **AC-4** 闸完备性加固的 3 条负控按 tag 变红;MGE-01b 真跑 `instantiateAll` 通过
- [ ] **AC-5** 0 回归:`ctest` 全绿 + 4 闸全绿 + 冻结面授权通过
- [ ] **AC-6** 台账 Entry #261 + 归档 + KPI 回填 0/3
