# Tasks — cpptlm-cep-stub-cache-core

> **状态**: 🟡 Proposed(2026-10-25 建目录)· builder 阶段启动
> **形态**: **双 Phase 单 change**(2026-10-25 DRI 两问裁决)—— Phase 0 = M-4 行为侧决策落盘 + 契约化;Phase 1 = stub cache core 实现
> **权威**: 验收条款见 `proposal.md` 的 A-P0-1 ~ A-P0-8 / A-P1-1 ~ A-P1-7 / A-FIN-1 ~ A-FIN-4;决策依据见 `design.md` D1 ~ D11
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` Entry #58
> **红绿分离**: 「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为交付物登记。本文件的任务**不**因结论为红而失败

---

## 0. 落地与登记(planner → builder 交接)

- [x] **T0.1** 建 `openspec/changes/cpptlm-cep-stub-cache-core/` 4 件套(proposal / design / specs / tasks)✅ **本 commit 已完成**(planner 阶段,commit `d01a3cb8`)
- [x] **T0.2** `openspec validate cpptlm-cep-stub-cache-core --strict` rc=0 ✅ **planner 阶段已完成**
- [x] **T0.3** `obj-cep-platform §4.0` 状态列 📋 → 🟡;`§4.2` 承接列修正(S1-4 已消费 → 移除);`phase-10.md` 子任务行同步;`§11` 追加 Entry #58 ✅ **planner 阶段已完成**
- [x] **T0.4** `openspec list --json` 实测 active 数 == 1(本 change 占用 1 slot,≤ 3)✅ **planner 阶段已完成**
- [x] **T0.5** **冻结面授权**:2 个成员(`ADR-CEP-05` §14 / `ADR-CEP-00` Status Update 2026-10-25)已走两步授权(`--update --authorized-by "Entry #58"` → 32 成员不变 / 2 hash 更新)✅ **planner 阶段已完成**

---

## 1. Phase 0 —— M-4 传输层 VC 映射契约闭合

> **为什么 Phase 0 先于 Phase 1**:Entry #55 ③ 警示「M-4 改现状语义 → 越早定越好(否则期间新断言会固化现有索引语义)」;stub cache 是 CEP 第一个会产生此类新断言的实现型 change。

### 1.1 决策落盘(A-P0-1)

- [x] **T1.1** **A-P0-1** —— M-4 决策**可寻址**:`design.md` 含逐条编号裁定(协议层 / 传输层 / 语义正名 / 冲突输入 / 桶名 / 冻结面),每条带 `file:line` 或权威章节号 ✅ **planner 阶段已完成**(D2 ~ D9)
- [x] **T1.2** **A-P0-7** —— `ADR-CEP-05` 追加 `## Status Update`(声明 §10 的 `-base`/`-impl` 二分已被 `obj §4.0` 重排取代;**§10 正文不改**)✅ **planner 阶段已完成**
- [x] **T1.3** **A-P0-6** —— **零改动校验**:冻结面除 `ADR-CEP-05` + `ADR-CEP-00` 外全部 0 diff;`include/tlm/cep/fabric.schema.json` 的 `vc_count.maximum` 仍为 **4**

### 1.2 语义正名(A-P0-2 / A-P0-3)

- [x] **T1.4** **A-P0-2** —— `include/tlm/cluster/soc_internal_noc.hh`:`VC_DAT` 的描述从「`CATEGORY_CREDIT` —— 索引对齐的副产物」改为「**预留 DAT 桶** —— 当前无 `CATEGORY_*` 数据类」;`vc_dat_count()` 的描述从「实际统计的是 credit 流量」改为「**预留 DAT 桶,当前恒 0**」。**零逻辑改动、零 API 改动、零常量改名**
- [x] **T1.5** **A-P0-3** —— `test/test_soc_internal_noc_data_flow.cc:108` 的**陈旧注释**(「但 `vc_counts_[2]` (DAT VC) 仍增」)修正 —— 该用例实际写 `CATEGORY_REQUEST`(`:112`)且**只**断言 `flits_dispatched()` / `unknown_dst_count()`(`:116-117`),**本就不存在** `vc_counts_[2]` 断言 ⇒ 修正后**断言集合不变**

### 1.3 冲突输入规则显式化(A-P0-5)

- [x] **T1.6** **A-P0-5** —— `soc_internal_noc.hh` 显式声明 `vc_id` / `flit_category` 冲突时的归属规则:**计数归属 = `flit_category`** · **编码归属 = `vc_id`** · **传输层不仲裁**(不报错 / 不纠正 / 不丢弃)。理由须记录:仲裁需**一致性词表**知识 = adapter 层职责(队列 #6)

### 1.4 守门机器化(A-P0-8)

- [x] **T1.7** **A-P0-8 ①** —— lint 第 10 组断言 ①(`CATEGORY_*` 恰 3 个 + 值域连续 0..2)**不回退**
- [x] **T1.8** **A-P0-8 ②** —— lint 第 10 组断言 ②(幽灵类别防护)**不回退**
- [x] **T1.9** **A-P0-8 ③** —— lint 第 10 组断言 ③(`VC_COUNT=4` + 「4 独立 VC」字样)**不回退**
- [x] **T1.10** **A-P0-8 ④** —— lint 第 10 组**新增**断言 ④:`VC_DAT` / `vc_dat_count()` 的说明**不得**声称「统计 credit 流量」,且**必须**声明「预留」语义
- [x] **T1.11** **A-P0-8 ⑤** —— lint 第 10 组**新增**断言 ⑤:冲突输入规则**三个子项**(`计数归属` / `编码归属` / `不仲裁`)**结构齐全** + **代码等价性**(计数下标须源自 `flit_category`,不可只查注释)
- [x] **T1.12** **A-P0-8 ⑥** —— lint 第 10 组**新增**断言 ⑥:`test/test_soc_internal_noc_data_flow.cc` **不得**含陈旧的 `vc_counts_[2]` 表述
- [x] **T1.13** **A-P0-8 fixture** —— 新断言 ④⑤⑥ 各配**反向 fixture**(变异必红 + GREEN 样本)⇒ 扩展 `scripts/test/cep_gate_harness.py` 支持抽第 10 组 + `scripts/test/cep_gate_selftest.sh` 追加用例

### 1.5 正控用例(A-P0-4)

- [x] **T1.14** **A-P0-4** —— 新增 `test/test_cep_vc_mapping_contract.cc`:
  - **正控**(判据③「纯负向断言必须有正控」):注入一个 `CATEGORY_CREDIT` flit → `vc_dat_count()` **必须**从 **0 变为 1**
  - 三桶计数断言(`vc_req_count` / `vc_rsp_count` / `vc_dat_count` 随对应类别增加)
  - 冲突输入断言(`vc_id=VC_DAT` + `category=CATEGORY_REQUEST` → 计入 `vc_req_count()`、**不**计入 `vc_dat_count()`)
  - 「传输层不仲裁」断言(不报错 / 不纠正 / 不丢弃)
  - 值域连续性断言(3 类 → 0/1/2)
- [x] **T1.15** —— 注册 ctest 用例(`test/CMakeLists.txt` 新增 `cep_vc_mapping_contract`,标签 `[cep][vc-mapping-contract]`)

---

## 2. Phase 1 —— Stub Cache Core 实现(**已完成** 2026-10-25)

> **实际交付面**(较 8 周估时**显著小** —— 因 `ADR-CEP-05` 的定义足够收敛,6 接口 + 5 态
> + 6 条 `INV-SC*` 可在**单文件头**内实现,无需分层/时序/替换策略):
> `include/tlm/cep/stub_cache_core.hh`(header-only,**不注册 23 ABI**、**不进 `ModuleFactory`**)
> + `test/test_cep_stub_cache_core.cc`(14 cases / 204 assertions)
> + `test/test_cep_scenario_anchor_reconciliation.cc`(4 cases / 25 assertions,A-P1-7 对账)

- [x] **T2.1** **A-P1-1** —— 6 接口签名存在,命名逐字对齐 `ADR-CEP-05` 决策 1 的 6 行表
- [x] **T2.2** **A-P1-2** —— 5 态可达(`I`/`S`/`E`/`M`);接口面**不**出现 CHI UC/SC/UD/SD 字样
- [x] **T2.3** **A-P1-3** —— `INV-SC1` ~ `INV-SC6` **各**至少 1 条**行为**断言(断言行为,不断言「常量存在」)
- [x] **T2.4** **A-P1-4** —— 折叠映射**单向形态**:接口面**不**出现 `forward_data` / `back_invalidate` / `barrier` 字样
- [x] **T2.5** **A-P1-5** —— **零协议实例声明**:不引入任何 `status != "planned"` 的声明
- [x] **T2.6** **A-P1-6** —— **不可验证条款已降级登记**:`ADR-CEP-05 §3` 契约测试 #1(`forward_data` 不可分辨性)与 #4(barrier 不可见)登记「触发条件 = 队列 #6」,**不进**本 change 的 spec
- [x] **T2.7** **A-P1-7** —— **Scenario ↔ 锚点等值**:每个 Scenario 点名一个 ctest 用例或 lint 断言;对账测试断言「Scenario 数 == 锚点数」
- [x] **T2.8** —— `include/tlm/cep/stub_cache_core.hh` + 实现(Phase 1 产物;**不注册 23 ABI**、**不进 `ModuleFactory`**)

---

## 3. 验收

### 3.1 验收(Phase 0 + Phase 1)

- [x] **T3.1** **A-FIN-1** —— `openspec validate cpptlm-cep-stub-cache-core --strict` rc=0
- [x] **T3.2** **A-FIN-2** —— `cep_docs_sync_check.sh --strict` / `cep_doc_ref_check.sh --strict` / `cep_freeze_check.sh` 全 rc=0;`cep_gate_selftest` 全绿
- [x] **T3.3** **A-FIN-3** —— `ctest` 全绿;`[cep]` 标签的 case 数与断言数**逐项登记**(断言语义变化须**预登记**)
- [x] **T3.4** —— **反向验证自查**:新断言 ④⑤⑥ 各**故意破坏一次** → 对应 fixture **必须变红**(逐个验证并记录)
- [x] **T3.5** —— **零行为变更校验**:`git diff --stat` 显示生产代码改动**仅限注释**(Phase 0 不动任何逻辑);`cpptlm_core` 与 23 ABI 零改动
- [x] **T3.6** —— **Phase 1 反向验证自查**:`INV-SC1` ~ `INV-SC6` **各 1 条**实现变异 + **3 条**静态/集成面变异(接口面出现 `barrier` / 枚举出现 `SD` / 代码面出现 `REGISTER_MODULE`)→ **9 / 9 全部报红**,还原后全绿
- [x] **T3.7** —— **Phase 1 零集成面校验**:stub **不**注册 23 ABI、**不**进 `ModuleFactory`、**不**引入协议实例声明(3 条均有**正控**证明扫描机制本身有效)

### 3.2 收尾(归档时)

- [ ] **T3.8** **A-FIN-4** —— 归档时按 `obj §4.7.3` 的 **17 个落点**逐项回填;delta capability 合并进 main spec 后按 Entry #56 先例纳入冻结集(走 `--update --authorized-by`)
