# tasks: cpptlm-router-local-port-credit-exhaustion

## T0 — 立项与判据

- [x] T0.1 improvement 5 段草稿 + ADR-X.16 决策（Accepted）
- [x] T0.2 metis 审查修订（15 条全采纳；其中 **D4 锚点订正** 为实质修正）
- [x] T0.3 change 目录建立（proposal / design / tasks + 2 份 delta spec）

## T1 — 实现（方案 A）

- [ ] T1.1 `src/tlm/router_tlm.cc` · `has_credit`：`out_port == LOCAL` 恒真（**在 `vc` 边界检查之后**）
- [ ] T1.2 `src/tlm/router_tlm.cc` · `consume_credit`：`out_port == LOCAL` 不扣减
- [ ] T1.3 `src/tlm/router_tlm.cc` · `downstream_credits_[LOCAL][*]` 死状态处置（(a) 注释说明 / (b) 跳过记账循环，**二选一**）
- [ ] T1.4 `src/tlm/router_tlm.cc` · VA / SA 拒绝分支：stall 周期计数 +1（锚点 = 「存在待发 flit 且 `has_credit == false`」，**不是** `waiting_for_credit`）
- [ ] T1.5 `include/tlm/router_tlm.hh` · `stat_group_` 增标量 `credit_stall_cycles`
- [ ] T1.6 `include/tlm/router_tlm.hh` · `RouterPort::LOCAL` 注释显式声明终端语义（V-6 / G-2）
- [ ] T1.7 诊断日志：stall 达 16 cycles 打印一次（`DPRINTF`）
- [ ] T1.8 ⛔ 确认 `credit_timeout_` 默认值**未被改动**（仍为 0）
- [ ] T1.9 ⛔ 确认零 diff：`module_factory.cc` · `CohIfNICTLM` · `NICTLM` · JSON schema · `configs/*.json`

## T2 — 测试

- [ ] T2.1 AC-01：裸 `RouterTLM` 自环注入 40 flit ⇒ drain 计数 == 40（无 8 平台期；≥120 cycles）
- [ ] T2.2 AC-02：4 packet 同时在飞 + `in_vc` 0..3（手写 `vc_id`）⇒ drain 计数 == 80（≥200 cycles）
- [ ] T2.3 AC-03：裸 router（`adapter_=nullptr`）向 mesh 向注入 8+ flit ⇒ `credit_stall_cycles` ≥16 cycles 后递增；挂 adapter 后**停止增长**
- [ ] T2.4 AC-04a：具名回归集全绿（MESH-3a/3b/4/5/7/8/9 · `test_noc_mesh_binding_semantics.cc` · `test_e2e_simulation.cc` nic 用例 · `[cep]`/`[e2e]`/`[pcie]`）
- [ ] T2.5 AC-04b：任何数值变化 ⇒ `design.md` 列「旧值 → 新值 → 必然性」；⛔ 禁止就地改 `REQUIRE` 常量
- [ ] T2.6 AC-05：死状态处置可 grep 验证
- [ ] T2.7 AC-07：AC-01 / AC-02 场景下 `credit_stall_cycles` 保持 0
- [ ] T2.8 G-1 / G-2：结构性测试仍绿 + 端口注释检查

## T3 — spec

- [x] T3.1 delta spec · ADDED：`router-local-port-flow-control`（2 条 Requirement）
- [x] T3.2 delta spec · MODIFIED：`cep-cohif-network-adapter` 的「已知限制」句
- [ ] T3.3 归档时 `openspec archive` 合并 delta + 新建 main spec + 修订既有 main spec
- [ ] T3.4 归档时更新冻结清单（`cep_freeze_check --update --authorized-by "Entry #NNN"`）

## T4 — 验证

- [ ] T4.1 `cmake --build build` 无 error
- [ ] T4.2 新单测全绿 + `ctest -R cep` 全绿
- [ ] T4.3 `cep_docs_sync_check` / `cep_doc_ref_check` 全通过
- [ ] T4.4 遗留清扫：无 TODO / 临时诊断 / skip 测试 / 未跟踪残渣

## T5 — 收口

- [ ] T5.1 台账 Entry（实施记录 + KPI 回填条件）
- [ ] T5.2 KPI 回填 6 处活口径 **1/3 → 0/3**（归档时）
- [ ] T5.3 归档（`openspec archive`，⛔ **不用** `--skip-specs` —— 实现已完成）
- [ ] T5.4 独立验收（切 verifier）

## T6 — 待 architect（跨角色）

- [ ] T6.1 在 `ADR-X.16` 追加 Status Update：**澄清 D4 的 stall 计数锚点**（原文 `waiting_for_credit` 恒为 false ⇒ 实施口径改为 VA / SA 拒绝条件）
- [ ] T6.2 （可选）把 2 项范围外缺陷登记为 backlog improvement：`buffer_size` param 不生效 · `NICTLM` 缺 credit flit 过滤
