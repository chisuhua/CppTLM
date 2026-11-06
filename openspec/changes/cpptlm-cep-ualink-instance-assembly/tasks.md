# tasks —— `ualink_4` 实例级装配

## T1 · 声明面 + 拓扑面

- [x] T1.1 `collective_planes.json`:`plane_binding` 加 `ualink_4` → `FabricCollectivePlane` + `current_state` / `reason` 同步
- [x] T1.2 `configs/cep_ualink_pe_minimal.json`(2 个 `CohIfProtocolEngine` + `cep.bridges` 挂 `ualink_4`)

## T2 · 判据面

- [x] T2.1 `include/tlm/cep/instance_assembly_gate.hh`(`[IAG-01]`~`[IAG-07]`)
- [x] T2.2 `test/test_cep_instance_assembly.cc`(正控 + 7 条负控 + 与 channel 闸一致性)
- [x] T2.3 存量计数修正(`plane_binding.size()` 3→4,两处)
- [x] T2.4 V6-c 的 M-1 夹具修正 + **新增反向断言**(规则有牙)

## T3 · 文档与台账

- [ ] T3.1 `07 号` §5.1.1 UALink 行改写 + `ADR-CEP-12` SU + `ADR-CEP-04` SU(M1 冻结面 ⇒ 两步授权)
- [ ] T3.2 台账 Entry #258(授权 + 实施回执)

## T4 · 验证与收口

- [ ] T4.1 `ctest --test-dir build -j4` 全绿 + 4 个 CEP 闸 + 冻结面授权通过
- [ ] T4.2 verifier 验收报告
- [ ] T4.3 归档 + KPI 回填 0/3
