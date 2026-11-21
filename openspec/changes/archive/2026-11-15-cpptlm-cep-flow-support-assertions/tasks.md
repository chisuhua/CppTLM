# tasks: cpptlm-cep-flow-support-assertions

> **范围**:repo_owned 6 实例 × 10 流 = **60 格** `flow_support` 期望值 + 逐格机器锁 + 脚本侧持久反向 fixture
> **AC 映射**:proposal AC-01 ~ AC-10 · design D-1 ~ D-7
> ⚠️ **零 src / 零声明源 / 零 schema 改动** —— 本 change 只动 `test/` + `scripts/test/` + delta spec

---

## Phase 0 · 立项态核对

- [x] **T0.1** `openspec validate cpptlm-cep-flow-support-assertions --strict` ⇒ **rc=0**(已做,planner 域)
- [x] **T0.2** `design.md` + `tasks.md` 落盘(builder 域)
- [x] **T0.3** 台账 **Entry #427** 落盘(planner 域)· KPI **0/3 → 1/3**(6 处活口径已回填)

## Phase 1 · W1 期望表(先写期望,后写判据 ⇒ TDD)

- [x] **T1.1** 新建 `test/cep_flow_support_expected.json` —— 6 个 repo_owned 实例 × 10 键 = **60 格**期望值(**逐格字面量**,⛔ 不从 JSON 推导)
- [x] **T1.2** 每实例 `source` 按 design **D-1.1** 三态填:`ualink_4` = **full**(`ADR-CEP-12` 13 键取值表行 2~5)· `chi_rnf` = partial(`ADR-CEP-19:551`,5 键)· `cxl_3_0_symmetric` / `cxl_2_0` = partial(`cep-cxl-dcoh-symmetric/spec.md:100`,各 1 键)· `tilelink_c` / `nvlink_4` = **none**
- [x] **T1.3** `coverage != full` 的实例 `note` 显式写「无逐值出处(冻结口径)」;⛔ **不伪造**出处
- [x] **T1.4** `schema` 字段 = `cep-flow-support-expected/v1` + `note` 写明本表是**变更检测器**、非独立 oracle

## Phase 2 · W2 判据

- [x] **T2.1** `test/test_cep_instance_declaration_contract.cc`:`load_json` helper 加 **2 个** env 覆盖(`CEP_FLOW_SUPPORT_DECL` / `CEP_FLOW_SUPPORT_EXPECTED`),⛔ **零外溢**(仅本文件)
- [x] **T2.2** 新增 `TEST_CASE("[cep][instance-decl] flow_support 60 格逐键锁 + 出处可核 + 反向控制")`
- [x] **T2.3** 判据 ⑤ 域检查:期望表 ids **双向集合相等** == 声明源 `repo_owned_instance_ids` 全集(⛔ 不按 `status` 过滤)
- [x] **T2.4** 判据 ② 形状联判:每实例 `flow_support` 键集 **恰好 10** 且与 `tlm::cep::kCanonicalFlowNames`(代码真源)逐名对齐
- [x] **T2.5** 判据 ③ 类型 + id 唯一性:`is_boolean()` + id 唯一断言
- [x] **T2.6** 判据 ① 逐格比对(per-instance `SECTION`)+ 失败消息含 `id` / `key` / 期望 / 实际
- [x] **T2.7** 判据 ⑥ 出处可核:按 design D-1.1 三态机器判据
- [x] **T2.8** 判据 ④ 反向控制:逐实例轮转翻转 1 格 ⇒ `REQUIRE(翻转值 != 原值)` + `REQUIRE_FALSE(比对通过)` + `REQUIRE(被变异实例数 == 6)`
- [x] **T2.9** ⚠️ **禁**「找到才断言」形态 ⇒ 以 id 查表 + 集合相等取代遍历时找

## Phase 3 · W3 + W4 持久反向 fixture

- [x] **T3.1** 新建 `scripts/test/cep_flow_support_reverse_verify.sh` —— **只写临时目录**(⛔ 仓内文件零改动)· `trap cleanup EXIT`
- [x] **T3.2** fixture 毒化**声明源副本** 1 格 ⇒ `build/bin/cpptlm_tests '[cep][instance-decl]'` ⇒ **必须红**(非零退出)
- [x] **T3.3** fixture 毒化**期望表副本** 1 格 ⇒ **必须红** ⇒ 两类毒化**分别单独跑**(归因不混计)
- [x] **T3.4** 未毒化态 ⇒ **必须绿**
- [x] **T3.5** ⚠️ 失败模式断言:红 ⇒ 必须**非零退出**;⛔ **不**接受「编译失败」冒充「判据失败」
- [x] **T3.6** `test/CMakeLists.txt` 注册 `cep_flow_support_reverse_verify` 进 ctest(形态 = `${CMAKE_SOURCE_DIR}/scripts/test/...` + `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`)⇒ **可并行安全**

## Phase 4 · 复验(0 回归)

- [x] **T4.1** `cmake --build build` ⇒ rc=0 且**零新增 warning**
- [x] **T4.2** `./build/bin/cpptlm_tests '[cep][instance-decl]'` ⇒ 绿
- [x] **T4.3** `./build/bin/cpptlm_tests '[cep]'` ⇒ **0 failures**;用例数 **+1**
- [x] **T4.4** `ctest --test-dir build -R cep` ⇒ **0 failures**(含新增 `cep_flow_support_reverse_verify`)
- [x] **T4.5** `bash scripts/test/cep_flow_support_reverse_verify.sh` ⇒ 红/绿/红 三态闭环 rc=0
- [x] **T4.6** `scripts/test/cep_*.sh` 逐个 rc=0 + `openspec validate --strict` rc=0 + `cep_docs_sync_check --strict` rc=0 + `agents_md_lint` 通过
- [x] **T4.7** **范围零 diff 复核**:`include/tlm/cep/**` · `protocol_instances.json` · `protocol_instance.schema.json` 全零 diff;新增 invariant = **0**

## Phase 5 · 归档 + 五步链 + KPI 回填

- [x] **T5.1** 台账 Entry(**归档期**那条,planner 域)= 归档授权记录
- [x] **T5.2** `openspec archive cpptlm-cep-flow-support-assertions --yes`(⛔ **禁** `--skip-specs`;delta **合并**进 `openspec/specs/cep-core-contracts/spec.md`)
- [x] **T5.3** 产物 commit
- [x] **T5.4** `cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ 清单 diff **只有** `cep-core-contracts/spec.md` 的 hash 变化 + `artifact_count` 不变(86)
- [x] **T5.5** 清单**单独** commit
- [x] **T5.6** 无参复验 `cep_freeze_check.sh` ⇒ rc=0 + provenance 一致
- [x] **T5.7** KPI **1/3 → 0/3** 回填(**6 处**活口径,先例 Entry #346)⇒ `cep_docs_sync_check --strict` rc=0
- [x] **T5.8** 台账归档回执 Entry(planner 域)+ 3 项残余状态更新

## Phase 6 · 验收

- [x] **T6.1** verifier 独立验收(proposal AC-01 ~ AC-10 逐条 + 工具证据)⇒ 报告落 `.rddf/reviews/cpptlm-cep-flow-support-assertions-verify.md`
- [x] **T6.2** 3 项残余状态确认:① 镜像 20 值 SKIP 洞(**待 architect**)② 3 族无逐值出处 ③ Scenario 标题债
