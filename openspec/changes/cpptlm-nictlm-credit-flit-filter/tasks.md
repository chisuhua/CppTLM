# tasks: cpptlm-nictlm-credit-flit-filter

## T0 — 立项与判据

- [x] T0.1 improvement 5 段草稿（`.rddf/improvements/cpptlm-nictlm-credit-flit-filter.md` · per 台账 Entry #461）
- [x] T0.2 change 目录建立（proposal / design / tasks + 1 份 delta spec）
- [x] T0.3 metis 审查（22 条建议：8 P0 / 10 P1 / 4 NIT —— **全部处置**，见 proposal 与 design 的修订痕迹）

## T1 — 实现

- [ ] T1.1 `src/tlm/nic_tlm.cc` · `handle_net_resp`：在 `consume()` 之后、计数之前插入 credit 判定（`flit_category == CATEGORY_CREDIT` ⇒ `++stats_credits_dropped_` 并 **`return true;`** —— flit 已被消费，返回值须与既有 `:94` 语义一致）
- [ ] T1.2 `include/tlm/nic_tlm.hh`：新增 `stats_credits_dropped_`（标量名 `credits_dropped`，unit/description 与 `CohIfNICTLM` 侧逐字一致）+ 访问器 `credits_dropped()`；⛔ **不新增其它访问器**（`flits_received` / `packets_received` 继续经 `get_stats_group()` 标量读取）
- [ ] T1.3 ⛔ **不抽取共享判据**：⛔ 不在 `bundles::NoCFlitBundle` 增 `is_credit()` · ⛔ 不在 `framework/` 的 adapter 层做过滤 · ⛔ 不动 `include/bundles/**` · ⛔ 不动 `include/framework/**` · ⛔ 不动 `include/abi/**` · ⛔ 不动 `configs/**` · ⛔ 不动 `src/tlm/router_tlm.cc` / `include/tlm/router_tlm.hh` · ⛔ 不动 `include/tlm/cep/cohif_nic.hh`
- [ ] T1.4 丢弃路径**不**触碰 `flits_received` / `packets_received` / `reassemble` / `pe_resp_out_`（**由 T2.1 的断言验证**，⛔ 不接受「读码确认」式自查）

## T2 — 测试

- [ ] **T2.0 新建 `test/test_nictlm_credit_filter.cc`**（文件名与 delta spec 的 3 处锚点**逐字一致** —— 归档后 spec 被冻结，锚点不得指向不存在的路径）
- [ ] T2.1 AC-01：**带判别力的序列** —— 先注入真响应 flit（`FLIT_HEAD_TAIL` · `CATEGORY_RESPONSE` · `tid=T`）⇒ 断言 `flits_received==1` / `packets_received==1` / `pe_resp_out()` 有效且 `transaction_id==T` ⇒ `clear_valid()` ⇒ 再注入 credit（**同 `tid=T`** · `FLIT_HEAD_TAIL` · `CATEGORY_CREDIT`）⇒ 断言三面**保持** 1/1/无输出 且 `credits_dropped()==1`
- [ ] T2.1b AC-06：**同 `tid` 在途包不被劫持** —— 注入真 `FLIT_HEAD`（`tid=T`，多 flit 包）→ 注入 credit（`tid=T`）⇒ 断言 `pe_resp_out()` 无输出且在途包状态未被改写 → 注入同 `tid=T` 的 `FLIT_TAIL` ⇒ 断言**恰好一条**响应
- [ ] T2.1c 两种 flit 形状都测：`FLIT_HEAD_TAIL`（真实 credit 形状）与默认 `FLIT_HEAD` ⇒ **都必须**被丢弃
- [ ] T2.2 AC-02：端到端（**测试自建**最小「NIC → Router → NIC」带流量拓扑，⛔ 不用 `mesh_2x2_tlm`）⇒ 断言 `credits_dropped() > 0`（避免 `0==0` 空断言）且 `flits_received` == 测试自己注入的响应 flit 数
- [ ] T2.3 AC-04 / G-1：同一 credit flit 注入 `NICTLM` 与 `CohIfNICTLM` ⇒ 两侧丢弃行为**一致**；并断言 `get_stats_group()->stats().count("credits_dropped") > 0`（标量**名字**的机器锚点）
- [ ] T2.4 AC-03：既有 `NICTLM` 拓扑回归全绿（`test_complex_topologies_from_config` · `test_e2e_simulation` 的 nic 用例 · `test_nic_side_rules` · `test_noc_mesh_binding_semantics` · 触达标签 `[e2e][nic][router][noc]`）+ `ctest` 全量
- [ ] T2.5 D-3 纪律：任何 `flits_received` / `packets_received` 变化 ⇒ 在 `design.md` 列「旧值 → 新值 → 必然性」并附**命令输出**；⛔ 禁止就地改 `REQUIRE` 常量；⛔ 不接受「读码确认」式自查
- [ ] T2.6 AC-05 零 diff 面：`git diff --name-only <base>..HEAD` 的结果 ⊆ `{src/tlm/nic_tlm.cc, include/tlm/nic_tlm.hh, test/test_nictlm_credit_filter.cc, openspec/changes/cpptlm-nictlm-credit-flit-filter/**}`；**由 builder 在归档前（归档前 review）执行并把命令输出贴进 change**

## T3 — spec

- [ ] T3.1 delta spec · ADDED：`cep-nictlm-credit-filter`（1 条 Requirement / 3 个 Scenario：丢弃 credit flit · 同 `tid` 在途包不被劫持 · 观测面暴露丢弃计数）
- [ ] T3.2 ⛔ 确认未改 `port-type-system` spec（本 change 不改 `NICTLM` 的端口角色）
- [ ] T3.3 归档时 `openspec archive` 合并 delta + 新建 main spec（`openspec/specs/cep-nictlm-credit-filter/`）
- [ ] T3.4 归档时更新冻结清单：新增 main spec 落 `openspec/specs/cep-nictlm-credit-filter/` ⇒ 命中既有 glob `openspec/specs/cep-*/spec.md` ⇒ 须 `cep_freeze_check.sh --update --authorized-by "Entry #NNN"`；⚠️ `NICTLM` 源码**不**在冻结面 ⇒ **不得**因此新增冻结条目

## T4 — 验证

- [ ] T4.1 `cmake --build build` 无 error
- [ ] T4.2 新单测全绿 + `ctest -R cep` 全绿
- [ ] T4.3 `cep_docs_sync_check` / `cep_doc_ref_check` 全通过
- [ ] T4.4 遗留清扫：无 TODO / 临时诊断 / skip 测试 / 未跟踪残渣

## T5 — 收口

- [ ] T5.1 台账 Entry（实施记录 + KPI 回填条件）
- [ ] T5.2 KPI 回填 **6 处活口径 1/3 → 0/3**（归档时）—— 枚举：① `.rddf/roadmap/objectives/obj-cep-platform.md:474`（含「当前 1 个 active」）② `.rddf/roadmap/phases/phase-10.md:209` ③ `.rddf/improvements/cpptlm-cep-umbrella.md:306` ④ `.rddf/improvements/cpptlm-cep-nvlink-sharp.md:31` ⑤ 同文件 `:211` ⑥ `.rddf/roadmap.md:5`；⚠️ 另核 `AGENTS.md` 的 KPI 段（「OpenSpec Proposed 存量 ≤3」实测基线）是否需同步
- [ ] T5.3 归档（`openspec archive`，⛔ **不用** `--skip-specs` —— 实现已完成）
- [ ] T5.4 独立验收（切 verifier）
