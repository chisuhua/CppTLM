# cpptlm-cep-ualink-instance-assembly: `ualink_4` 实例级装配 —— 闭合「声明 ≠ 装配」

> **状态**: 🟡 **Proposed**(2026-11-06 立项 —— 用户裁决「按建议顺序实施」,per 台账 Entry #258;KPI 并发 **0/3 → 1/3**)
> **立项依据**:`ADR-CEP-12 §5.1.1` + 台账 Entry #240 登记的 backlog —— `ualink_4` **实例级装配**(拓扑挂载 + collective plane 绑定)未闭合
> **关联 ADR**:`ADR-CEP-12`(实例声明 + 6 类消息词表)· `ADR-CEP-04`(`ualink_4` 取值裁决)· `ADR-CEP-01`(collective plane)
> **冻结面**:3 处追加/改写(`07 号` §5.1.1 UALink 行 · `ADR-CEP-12` SU · `ADR-CEP-04` SU)⇒ 两步授权
> **估时**: ~3d(本轮交付 = 装配层;coherence 流专用引擎留后续)

## Why

`ualink_4` 的**声明**已在 Entry #239 落地(13 键全部裁决),但 `ADR-CEP-12 §5.1.1` 与台账 Entry #240 明确记着:**声明 ≠ 装配** —— 实例级装配闭环仍需

1. 把 `ualink_4` **挂进拓扑**(可运行的 `cep.bridges[].protocol_instance`);
2. **绑定 collective plane**(否则 collective 能力无承载实现);
3. 「装配完整」有**可判**判据(**含反控**),否则「装了」与「没装」不可区分。

现状缺口:`collective_planes.json` 的 `current_state` 自陈「本仓尚无 `ualink_4` 实例 ⇒ 实例级装配尚不可达 ⇒ 登记为 backlog」;`nvlink_4` 虽有 plane 绑定但**无拓扑挂载** ⇒ 仓内**没有任何**「实例级装配」的判据。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | **平面绑定**:`ualink_4` → `FabricCollectivePlane`(承载真实 fabric 集合流量) | `include/tlm/cep/collective_planes.json`(`plane_binding` + `current_state` + `vc_reservation_declaration.reason` 同步) |
| 2 | **拓扑挂载**:2 个 `CohIfProtocolEngine` + `cep.bridges[].protocol_instance = "ualink_4"` | `configs/cep_ualink_pe_minimal.json`(新) |
| 3 | **装配闸**(新公共头,`GateReport` 同型):`[IAG-01]`~`[IAG-07]` | `include/tlm/cep/instance_assembly_gate.hh` |
| 4 | **判据**:正控 + 7 条负控(逐条按归因 tag 变红)+ 与既有 channel 闸的一致性 | `test/test_cep_instance_assembly.cc`(新) |
| 5 | **存量计数修正**:`plane_binding.size()` 3 → 4(两处)+ V6-c 的 M-1 夹具修正 + **新增反向断言**(规则有牙) | `test/test_cep_collective_plane_contract.cc` · `test/test_cep_nvlink_sharp.cc` |
| 6 | **文档同步**:`07 号` §5.1.1 UALink 行 + `ADR-CEP-12` SU + `ADR-CEP-04` SU | 3 处(M1 冻结面 ⇒ 两步授权) |

**Out of Scope**: ⛔ **不**新增 UALink 专用 coherence 引擎(承载 `request`/`completion` 的流语义)⇒ 本 change 只闭合「装配」,⛔ 不闭合「流语义」;⛔ 不改 `protocol_instances.json`;⛔ 不改 `module_factory.cc`(`cep.bridges` 路径通用);⛔ 不改 `07 号` §11 的 `7` vs `8` 既存漂移。

## Impact

- **公共 API**:新增 1 个头(`instance_assembly_gate.hh`);
- **声明面**:`collective_planes.json` +1 键;**拓扑面**:新增 1 个配置;
- **契约面**:新增 requirement `cep-instance-assembly`;
- **测试**:新增 1 个测试文件(3 cases / 43 assertions)+ 存量计数修正 2 处 + V6-c 反向断言 1 条;
- **冻结面**:3 处 ⇒ 两步授权;
- **KPI**:占 1 slot(归档后回 0/3)。

## Acceptance

- [ ] **AC-1** `ualink_4` 在 `plane_binding` 中绑**非空且已登记**的 plane(`FabricCollectivePlane`)
- [ ] **AC-2** `configs/cep_ualink_pe_minimal.json` 挂载 `ualink_4`(bridge + 模块在拓扑中 + `bridge_id` 唯一)
- [ ] **AC-3** 装配闸正控 PASS + 7 条负控按归因 tag 变红(`[IAG-01]`~`[IAG-07]`)
- [ ] **AC-4** V6-c 的 M-1 夹具修正后仍 PASS,且**新增反向断言**(M-1 + 真实 fabric 载体 ⇒ FAIL)
- [ ] **AC-5** 0 回归:`ctest --test-dir build -j4` 全绿 + 4 个 CEP 闸全绿 + M1 冻结面授权通过
- [ ] **AC-6** 文档同步:3 处(M1 冻结面)落定 + 台账 Entry + 归档
