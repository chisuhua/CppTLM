# tasks —— `ualink_4` 的记账级专用承载引擎

## T1 · 实现

- [x] T1.1 `include/tlm/cep/ualink_protocol_engine.hh`(三分法 + order-free 在途集合)
- [x] T1.2 `include/chstream_register.hh` 注册(短名 + 强度标签注释)
- [x] T1.3 `configs/cep_ualink_engine_pe_minimal.json`
- [x] T1.4 `test/test_cep_ualink_engine.cc`(UGE-01~06)

## T2 · 契约与文档

- [x] T2.1 delta spec → `openspec/specs/cep-ualink-coherence-engine/spec.md`
- [ ] T2.2 `07 号` §5.1.1 UALink 行精确化(architect 域 · 冻结面两步授权,per Entry #269 §5)
- [ ] T2.3 台账 Entry(实施回执)

## T3 · 验证与归档

- [ ] T3.1 `ctest` 全绿 + 4 闸 + 冻结面 41 产物零 diff
- [ ] T3.2 verifier 验收(报告 `.rddf/reviews/`)
- [ ] T3.3 归档 + KPI 回填 0/3
