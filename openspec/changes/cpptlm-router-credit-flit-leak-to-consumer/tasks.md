# tasks: cpptlm-router-credit-flit-leak-to-consumer

## T0 — 立项与判据裁决

- [x] T0.1 improvement 5 段草稿(`.rddf/improvements/cpptlm-router-credit-flit-leak-to-consumer.md`)+ metis 审查修订(1 轮)
- [x] T0.2 planner 裁决等价判据形态(选项 A · 开环 + 闭环不变式)—— 台账 Entry #455,AC-05 / AC-06 修订
- [x] T0.3 change 目录建立(proposal / design / tasks + delta spec)

## T1 — 实现(消费者侧过滤)

- [x] T1.1 `include/tlm/cep/cohif_nic.hh`:`handle_net_ingress` 丢弃 `flit_category == CATEGORY_CREDIT`(不计数 / 不写 `last_recv_*` / 不投影 PE 侧)
- [x] T1.2 同文件:新增 `stats_credits_dropped_` + 访问器 `credits_dropped()`
- [x] T1.3 同文件:文件头「已知边界」补 2 条(credit 丢弃 / LOCAL credit 耗尽不属本模块)
- [x] T1.4 ⛔ 确认 `RouterTLM` / `module_factory.cc` / `CohIfBundle` **零 diff**

## T2 — 测试

- [x] T2.1 `test/test_cep_mesh_e2e.cc`:MESH-7(AC-01 —— 先真 flit 后 credit,三面零变化 + `credits_dropped` 递增)
- [x] T2.2 同文件:MESH-8(AC-02 / AC-03 —— 层次化与平铺各自 `credits_dropped > 0` + `nic_b.last_recv_dst_node == 3 && hops == 3`)
- [x] T2.3 同文件:MESH-3 拆为 **MESH-3a(开环等价 · 逐项严格相等)** 与 **MESH-3b(闭环不变式)**(per Entry #455)
- [x] T2.4 同文件:订正 `:17-21` 的「NIC 计数时序敏感」注释(过滤后只剩一半成立)

## T3 — spec

- [x] T3.1 delta spec:`specs/cep-cohif-network-adapter/spec.md` —— 增「网络 ingress SHALL 丢弃 credit flit」Scenario + 观测面增 `stats_credits_dropped_`
- [ ] T3.2 归档时 `openspec archive` 合并 delta 进 main spec + 更新冻结清单(`cep_freeze_check --update --authorized-by "Entry #NNN"`)—— **归档阶段执行**

## T4 — 验证

- [x] T4.1 `cmake --build build` 无 error
- [x] T4.2 `[credit]` 全绿(57 断言 / 3 用例)
- [x] T4.3 `[mesh]` 全绿(17 用例)
- [x] T4.4 `ctest -R cep` 全绿
- [x] T4.5 `cep_docs_sync_check` 全通过
- [x] T4.6 遗留清扫:无 TODO / 临时诊断 / skip 测试 / 未跟踪残渣

## T5 — 收口

- [x] T5.1 台账 Entry #456(实施记录 + KPI 回填条件)
- [x] T5.2 KPI 回填 6 处活口径 **0/3 → 1/3**(change 目录建立时)
- [ ] T5.3 归档(`openspec archive`,⛔ **不用** `--skip-specs` —— 实现已完成)
- [ ] T5.4 独立验收(切 verifier)
