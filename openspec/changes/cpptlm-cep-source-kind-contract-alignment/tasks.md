# tasks —— 契约对齐 + 第四变异态

## T1 · 第四变异态

- [x] T1.1 probe 扩展:`Free` ⇔ `transition_is_free` 逐格一致性(签名 `FREE-KIND mismatch`)
- [x] T1.2 脚本:影子树改为复制整个 `include/tlm/cep/`;新增 ④ `misclassified` 态;真源 sha256 校验扩到 3 个文件

## T2 · 契约

- [x] T2.1 delta spec:`cep-ualink-inc-state-spec-self-containment` requirement + UIS-12 场景
- [x] T2.2 `openspec validate` 通过

## T3 · 判据

- [ ] T3.1 闸脚本 4 态全绿(8 PASS / 0 FAIL)
- [ ] T3.2 `ctest --test-dir build -j4` 0 回归
- [ ] T3.3 4 个 CEP 闸全绿 + M1 冻结面 41 产物零 diff

## T4 · 收口

- [ ] T4.1 台账 Entry(实施回执 + 归档回执)
- [ ] T4.2 verifier 验收报告 `.rddf/reviews/cep-source-contract-alignment-verify.md`
- [ ] T4.3 归档 + KPI 回填 0/3
