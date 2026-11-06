# Tasks · cpptlm-cep-tilelink-runtime-engine

> bite-sized(2~5 min 粒段)· per TDD 5 步纪律 · 2026-11-06(planner)

- [x] **P0-0**(F2 漏勾回填,per design D-6,**严格先于引擎改动**):台账 Entry #244 授权 → `07 号` ⑦ 补勾 4 框(L1/L2/L3/L9)+ dated 句 → `cep_freeze_check.sh --update --authorized-by "Entry #244"` → 清单单独 commit
- [ ] P0-1 `chstream_register.hh` 注册第 4 个引擎名 + `test_cep_registration_macro_contract.cc:167` 三元组 → **四元组**(名清单是不变的先例,per design D-1b)
- [ ] P0-2 `on_*` 签名契约清单(10 个虚函数与 `ProtocolEngineBase` 逐行对照,**零签名偏差**)
- [ ] P1-2 新建 `include/tlm/cep/tilelink_protocol_engine.hh`(header-only 骨架 + 零自有事件副本纪律)
- [ ] P1-3 7 空掩码意图的显式零事件断言(引擎层)
- [ ] P1-4 编译期锁(引擎侧 TLE-01/02 对应编译断言或 consteval 自检)
- [ ] P1-5 CMake/测试注册:新建 `test/test_cep_tilelink_engine.cc`,标签 `[cep][tilelink][engine]`
- [ ] P2-6 L4 E2E 骨架(装配切换:tilelink_c 引擎 Null→TileLink,测试侧 JSON)
- [ ] P2-7 5 流贯通断言(request/completion/snoop/snoop_resp/writeback 逐流)
- [ ] P2-8 `Cap`「源不定」显式测试
- [ ] P2-9 fail-closed 消费面反控(Pending ⇒ 零事件)
- [ ] P2-10 决定性变异取证(删被消费行 ⇒ 红,原因正确 ⇒ 复原 ⇒ 全绿)
- [ ] P3-11 07 号 SU 追加(TileLink-C 行 + B3b 达成度)+ 冻结面 `--update`(⚠️ 含 verifier F2 漏勾回填,一次授权)
- [ ] P3-12 台账 Entry #245(实施回执 + B3b 1/6→2/6 回填)+ verifier 验收
