# tasks —— MOESI-GPU L4 E2E

## T1 · 实现

- [x] T1.1 `chstream_register.hh` 注册 `MoesiGpuProtocolEngine`(N=2)
- [x] T1.2 `configs/cep_moesi_gpu_pe_minimal.json`
- [x] T1.3 `test/test_cep_moesi_gpu_engine.cc`(MGE-01~05)

## T2 · 文档与台账

- [ ] T2.1 coverage §3-A 缺口闭合
- [ ] T2.2 台账 Entry #259

## T3 · 验证与收口

- [ ] T3.1 `ctest --test-dir build -j4` 全绿 + 4 闸 + 冻结面零 diff
- [ ] T3.2 verifier 验收报告
- [ ] T3.3 归档 + KPI 回填 0/3
