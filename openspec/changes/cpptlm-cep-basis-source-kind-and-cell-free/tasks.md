# tasks — 来源类型可机器判 + `CellKind::Free`

## T1 · 载体(先红后绿)

- [x] T1.1 新增 `include/tlm/cep/source_registry.hh`:`RefKind` / `SourceEntry` / `kSources[]` / `find_source` / `starts_with`
- [x] T1.2 新增 `include/tlm/cep/sources.json`(id / kind / title / locator / sha256 / retrieved / license)
- [x] T1.3 `tilelink_state_spec.hh`:`CellKind::Free` + `resolve_ref` + `basis_ref_kind_consistent`
- [x] T1.4 `cxl_state_spec.hh`:`CellKind::Free` + `resolve_ref` + `basis_ref_kind_consistent` + 四态计数自洽
- [x] T1.5 `mesif_state_spec.hh`:`resolve_ref` + `basis_ref_kind_consistent`(`Primary` ⇒ ref 非空)
- [x] T1.6 `ualink_inc_state_spec.hh`:`resolve_ref` + `basis_ref_kind_consistent`
- [x] T1.7 `tilelink_c_adapter.hh` / `cxl_cache_adapter.hh`:`transition_is_free` 显式分支

## T2 · 判据测试

- [x] T2.1 `test/test_cep_source_registry.cc`:4 载体逐格可解析(未登记 = 0)
- [x] T2.2 映射一致 0 违规(按载体,per design D-2)
- [x] T2.3 `sources.json` ↔ `kSources[]` 逐条一致
- [x] T2.4 四态互斥 + `Free` 计数 = 0 + `transition_is_free` 分支

## T3 · 决定性变异

- [x] T3.1 `scripts/test/cep_source_effect_check.sh`:clean / dropped / downgraded 三态
- [x] T3.2 三态签名钉死(非「任意失败即算」)+ 收尾真源 sha256 未变

## T4 · 验证

- [x] T4.1 `cmake --build build -j4 --target cpptlm_tests` 零新警告
- [x] T4.2 `./build/bin/cpptlm_tests "[cep]"` 全绿
- [x] T4.3 `ctest --test-dir build -j4` 0 回归
- [x] T4.4 冻结面零 diff + `cep_docs_sync_check.sh` / `cep_doc_ref_check.sh` / `cep_decl_alignment_check.sh` 全绿

## T5 · 收口

- [x] T5.1 台账 Entry(实施回执)+ 遗留登记(architect 追加 SU 更新 D-1 诚实声明)
- [x] T5.2 verifier 验收报告 `.rddf/reviews/cep-source-kind-verify.md`
