# cpptlm-cep-moesi-gpu-l4-e2e: MOESI-GPU **L4 事务级 E2E** —— 闭合 coverage §3-A 缺口

> **状态**: 🟡 **Proposed**(2026-11-06 立项 —— 用户裁决「按建议顺序实施」链内第 4 项;KPI 并发 **1/3 → 2/3**)
> **立项依据**:`docs/validation/2026-11-04-cep-protocol-e2e-coverage.md` **§3-A**「MOESI-GPU:引擎存在,但测试只有 adapter 契约与 per-network 装配期校验,无 `instantiate → submit → tick → 抵达`」
> **关联 change**:`cpptlm-cep-ualink-instance-assembly`(装配闸,本 change 复用其 `instance_assembly_gate`)
> **估时**: ~2d(实际实现 < 3h)

## Why

`moesi_gpu` 是本仓**首个** `status=implemented` 实例(per `ADR-CEP-12 §13`),引擎 `moesi_gpu_protocol_engine.hh`(306 行)也已存在,但:

1. **引擎未注册** ⇒ `ModuleFactory` 无法实例化 ⇒ 「instantiate」这一步**根本走不通**;
2. 无任何 L4 驱动测试 ⇒ 10 条 canonical 流的记账、表驱动迁移、补偿拒绝路径**都没有运行期见证**;
3. coverage 文档的 §3-A 缺口因此长期挂账。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | **注册**引擎(短名 `MoesiGpuProtocolEngine`,N=2,`bundles::CohIfBundle`) | `include/chstream_register.hh` |
| 2 | **装配拓扑** | `configs/cep_moesi_gpu_pe_minimal.json`(2 引擎 + `cep.bridges[].protocol_instance = "moesi_gpu"`,`bridge_id = 4`) |
| 3 | **L4 判据**(MGE-01~05) | `test/test_cep_moesi_gpu_engine.cc`(新) |
| 4 | **缺口文档闭合** | `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md`(§3-A) |

**Out of Scope**: ⛔ **不**实现协议事件发射(引擎按决策 M-6「只记账」)⇒ 本 change 只闭合「实例化 + 记账 + 迁移 + 补偿」;⛔ 不改 `moesi_gpu_protocol_engine.hh` 的语义;⛔ 不改 `protocol_instances.json`。

## Impact

- **公共面**:ChStream 注册表 +1 项;
- **契约面**:新增 requirement `cep-moesi-gpu-l4-e2e`;
- **测试**:新增 1 个测试文件(5 cases / 89 assertions);
- **文档**:coverage §3-A 缺口闭合;
- **KPI**:占 1 slot(归档后回 0/3)。

## Acceptance

- [ ] **AC-1** `MoesiGpuProtocolEngine` 已注册 ⇒ `configs/cep_moesi_gpu_pe_minimal.json` 的拓扑可声明该类型
- [ ] **AC-2** MGE-02:10 条 canonical 流**逐条** `flow_events == 1`(5 条 CohIf + 5 条直调)
- [ ] **AC-3** MGE-03/04:表驱动迁移落状态机;`O` 态离开被拒(未登记 + `compensation_skipped` +1 + 迁移不增)
- [ ] **AC-4** MGE-05:装配闸 PASS + channel 闸对 `moesi_gpu` 有逐 VC verdict
- [ ] **AC-5** 0 回归:`ctest --test-dir build -j4` 全绿 + 4 个 CEP 闸全绿
- [ ] **AC-6** coverage §3-A 缺口闭合 + 台账 Entry + 归档
